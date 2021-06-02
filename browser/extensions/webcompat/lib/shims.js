/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* globals browser, module, onMessageFromTab */

// To grant shims access to bundled logo images without risking
// exposing our moz-extension URL, we have the shim request them via
// nonsense URLs which we then redirect to the actual files (but only
// on tabs where a shim using a given logo happens to be active).
const LogosBaseURL = "https://smartblock.firefox.etp/";

const releaseBranchPromise = browser.appConstants.getReleaseBranch();

const platformPromise = browser.runtime.getPlatformInfo().then(info => {
  return info.os === "android" ? "android" : "desktop";
});

let debug = async function() {
  if ((await releaseBranchPromise) !== "release_or_beta") {
    console.debug.apply(this, arguments);
  }
};
let error = async function() {
  if ((await releaseBranchPromise) !== "release_or_beta") {
    console.error.apply(this, arguments);
  }
};
let warn = async function() {
  if ((await releaseBranchPromise) !== "release_or_beta") {
    console.warn.apply(this, arguments);
  }
};

class Shim {
  constructor(opts) {
    const { matches, unblocksOnOptIn } = opts;

    this.branches = opts.branches;
    this.bug = opts.bug;
    this.file = opts.file;
    this.hosts = opts.hosts;
    this.id = opts.id;
    this.logos = opts.logos || [];
    this.matches = [];
    this.name = opts.name;
    this.notHosts = opts.notHosts;
    this.onlyIfBlockedByETP = opts.onlyIfBlockedByETP;
    this._options = opts.options || {};
    this.needsShimHelpers = opts.needsShimHelpers;
    this.platform = opts.platform || "all";
    this.unblocksOnOptIn = unblocksOnOptIn;

    this._hostOptIns = new Set();

    this._disabledByConfig = opts.disabled;
    this._disabledGlobally = false;
    this._disabledByPlatform = false;
    this._disabledByReleaseBranch = false;

    this._activeOnTabs = new Set();
    this._showedOptInOnTabs = new Set();

    const pref = `disabled_shims.${this.id}`;

    for (const match of matches) {
      if (!match.types) {
        this.matches.push({ patterns: [match], types: ["script"] });
      } else {
        this.matches.push(match);
      }
    }

    browser.aboutConfigPrefs.onPrefChange.addListener(async () => {
      const value = await browser.aboutConfigPrefs.getPref(pref);
      this._disabledPrefValue = value;
      this._onEnabledStateChanged();
    }, pref);

    this.ready = Promise.all([
      browser.aboutConfigPrefs.getPref(pref),
      platformPromise,
      releaseBranchPromise,
    ]).then(([disabledPrefValue, platform, branch]) => {
      this._disabledPrefValue = disabledPrefValue;

      this._disabledByPlatform =
        this.platform !== "all" && this.platform !== platform;

      this._disabledByReleaseBranch = false;
      for (const supportedBranchAndPlatform of this.branches || []) {
        const [
          supportedBranch,
          supportedPlatform,
        ] = supportedBranchAndPlatform.split(":");
        if (
          (!supportedPlatform || supportedPlatform == platform) &&
          supportedBranch != branch
        ) {
          this._disabledByReleaseBranch = true;
        }
      }

      this._preprocessOptions(platform, branch);
      this._onEnabledStateChanged();
    });
  }

  _preprocessOptions(platform, branch) {
    // options may be any value, but can optionally be gated for specified
    // platform/branches, if in the format `{value, branches, platform}`
    this.options = {};
    for (const [k, v] of Object.entries(this._options)) {
      if (v?.value) {
        if (
          (!v.platform || v.platform === platform) &&
          (!v.branches || v.branches.includes(branch))
        ) {
          this.options[k] = v.value;
        }
      } else {
        this.options[k] = v;
      }
    }
  }

  get enabled() {
    if (this._disabledGlobally) {
      return false;
    }

    if (this._disabledPrefValue !== undefined) {
      return !this._disabledPrefValue;
    }

    return (
      !this._disabledByConfig &&
      !this._disabledByPlatform &&
      !this._disabledByReleaseBranch
    );
  }

  enable() {
    this._disabledGlobally = false;
    this._onEnabledStateChanged();
  }

  disable() {
    this._disabledGlobally = true;
    this._onEnabledStateChanged();
  }

  _onEnabledStateChanged() {
    if (!this.enabled) {
      return this._revokeRequestsInETP();
    }
    return this._allowRequestsInETP();
  }

  async _allowRequestsInETP() {
    const matches = this.matches.map(m => m.patterns).flat();
    if (matches.length) {
      await browser.trackingProtection.shim(this.id, matches);
    }

    if (this._hostOptIns.size) {
      const optIns = this.getApplicableOptIns();
      if (optIns.length) {
        await browser.trackingProtection.allow(
          this.id,
          this._optInPatterns,
          Array.from(this._hostOptIns)
        );
      }
    }
  }

  _revokeRequestsInETP() {
    return browser.trackingProtection.revoke(this.id);
  }

  setActiveOnTab(tabId, active = true) {
    if (active) {
      this._activeOnTabs.add(tabId);
    } else {
      this._activeOnTabs.delete(tabId);
      this._showedOptInOnTabs.delete(tabId);
    }
  }

  isActiveOnTab(tabId) {
    return this._activeOnTabs.has(tabId);
  }

  meantForHost(host) {
    const { hosts, notHosts } = this;
    if (hosts || notHosts) {
      if (
        (notHosts && notHosts.includes(host)) ||
        (hosts && !hosts.includes(host))
      ) {
        return false;
      }
    }
    return true;
  }

  async unblocksURLOnOptIn(url) {
    if (!this._optInPatterns) {
      this._optInPatterns = await this.getApplicableOptIns();
    }

    if (!this._optInMatcher) {
      this._optInMatcher = browser.matchPatterns.getMatcher(
        Array.from(this._optInPatterns)
      );
    }

    return this._optInMatcher.matches(url);
  }

  isTriggeredByURLAndType(url, type) {
    for (const entry of this.matches || []) {
      if (!entry.types.includes(type)) {
        continue;
      }
      if (!entry.matcher) {
        entry.matcher = browser.matchPatterns.getMatcher(
          Array.from(entry.patterns)
        );
      }
      if (entry.matcher.matches(url)) {
        return entry;
      }
    }

    return undefined;
  }

  async getApplicableOptIns() {
    if (this._applicableOptIns) {
      return this._applicableOptIns;
    }
    const optins = [];
    for (const unblock of this.unblocksOnOptIn || []) {
      if (typeof unblock === "string") {
        optins.push(unblock);
        continue;
      }
      const { branches, patterns, platforms } = unblock;
      if (platforms?.length) {
        const platform = await platformPromise;
        if (platform !== "all" && !platforms.includes(platform)) {
          continue;
        }
      }
      if (branches?.length) {
        const branch = await releaseBranchPromise;
        if (!branches.includes(branch)) {
          continue;
        }
      }
      optins.push.apply(optins, patterns);
    }
    this._applicableOptIns = optins;
    return optins;
  }

  async onUserOptIn(host) {
    const optins = await this.getApplicableOptIns();
    if (optins.length) {
      this.userHasOptedIn = true;
      this._hostOptIns.add(host);
      await browser.trackingProtection.allow(
        this.id,
        optins,
        Array.from(this._hostOptIns)
      );
    }
  }

  hasUserOptedInAlready(host) {
    return this._hostOptIns.has(host);
  }

  showOptInWarningOnce(tabId, origin) {
    if (this._showedOptInOnTabs.has(tabId)) {
      return Promise.resolve();
    }
    this._showedOptInOnTabs.add(tabId);

    const { bug, name } = this;
    const warning = `${name} is allowed on ${origin} for this browsing session due to user opt-in. See https://bugzilla.mozilla.org/show_bug.cgi?id=${bug} for details.`;
    return browser.tabs
      .executeScript(tabId, {
        code: `console.warn(${JSON.stringify(warning)})`,
        runAt: "document_start",
      })
      .catch(() => {});
  }
}

class Shims {
  constructor(availableShims) {
    if (!browser.trackingProtection) {
      console.error("Required experimental add-on APIs for shims unavailable");
      return;
    }

    this._registerShims(availableShims);

    onMessageFromTab(this._onMessageFromShim.bind(this));

    this.ENABLED_PREF = "enable_shims";
    browser.aboutConfigPrefs.onPrefChange.addListener(() => {
      this._checkEnabledPref();
    }, this.ENABLED_PREF);
    this._haveCheckedEnabledPref = this._checkEnabledPref();
  }

  _registerShims(shims) {
    if (this.shims) {
      throw new Error("_registerShims has already been called");
    }

    this.shims = new Map();
    for (const shimOpts of shims) {
      const { id } = shimOpts;
      if (!this.shims.has(id)) {
        this.shims.set(shimOpts.id, new Shim(shimOpts));
      }
    }

    const allMatchTypePatterns = new Map();
    const allLogos = [];
    for (const shim of this.shims.values()) {
      const { logos, matches } = shim;
      allLogos.push(...logos);
      for (const { patterns, types } of matches || []) {
        for (const type of types) {
          if (!allMatchTypePatterns.has(type)) {
            allMatchTypePatterns.set(type, { patterns: new Set() });
          }
          const allSet = allMatchTypePatterns.get(type).patterns;
          for (const pattern of patterns) {
            allSet.add(pattern);
          }
        }
      }
    }

    if (allLogos.length) {
      const urls = Array.from(new Set(allLogos)).map(l => {
        return `${LogosBaseURL}${l}`;
      });
      debug("Allowing access to these logos:", urls);
      const unmarkShimsActive = tabId => {
        for (const shim of this.shims.values()) {
          shim.setActiveOnTab(tabId, false);
        }
      };
      browser.tabs.onRemoved.addListener(unmarkShimsActive);
      browser.tabs.onUpdated.addListener(unmarkShimsActive, {
        properties: ["discarded", "url"],
      });
      browser.webRequest.onBeforeRequest.addListener(
        this._redirectLogos.bind(this),
        { urls, types: ["image"] },
        ["blocking"]
      );
    }

    if (!allMatchTypePatterns.size) {
      debug("Skipping shims; none enabled");
      return;
    }

    for (const [type, { patterns }] of allMatchTypePatterns.entries()) {
      const urls = Array.from(patterns);
      debug("Shimming these", type, "URLs:", urls);

      browser.webRequest.onBeforeRequest.addListener(
        this._ensureShimForRequestOnTab.bind(this),
        { urls, types: [type] },
        ["blocking"]
      );
    }
  }

  async _checkEnabledPref() {
    await browser.aboutConfigPrefs.getPref(this.ENABLED_PREF).then(value => {
      if (value === undefined) {
        browser.aboutConfigPrefs.setPref(this.ENABLED_PREF, true);
      } else if (value === false) {
        this.enabled = false;
      } else {
        this.enabled = true;
      }
    });
  }

  get enabled() {
    return this._enabled;
  }

  set enabled(enabled) {
    if (enabled === this._enabled) {
      return;
    }

    this._enabled = enabled;

    for (const shim of this.shims.values()) {
      if (enabled) {
        shim.enable();
      } else {
        shim.disable();
      }
    }
  }

  async _onMessageFromShim(payload, sender, sendResponse) {
    const { tab, frameId } = sender;
    const { id, url } = tab;
    const { shimId, message } = payload;

    // Ignore unknown messages (for instance, from about:compat).
    if (message !== "getOptions" && message !== "optIn") {
      return undefined;
    }

    if (sender.id !== browser.runtime.id || id === -1) {
      throw new Error("not allowed");
    }

    // Important! It is entirely possible for sites to spoof
    // these messages, due to shims allowing web pages to
    // communicate with the extension.

    const shim = this.shims.get(shimId);
    if (!shim?.needsShimHelpers?.includes(message)) {
      throw new Error("not allowed");
    }

    if (message === "getOptions") {
      return Object.assign(
        {
          platform: await platformPromise,
          releaseBranch: await releaseBranchPromise,
        },
        shim.options
      );
    } else if (message === "optIn") {
      try {
        await shim.onUserOptIn(new URL(url).hostname);
        const origin = new URL(tab.url).origin;
        warn(
          "** User opted in for",
          shim.name,
          "shim on",
          origin,
          "on tab",
          id,
          "frame",
          frameId
        );
        await shim.showOptInWarningOnce(id, origin);
      } catch (err) {
        console.error(err);
        throw new Error("error");
      }
    }

    return undefined;
  }

  async _redirectLogos(details) {
    await this._haveCheckedEnabledPref;

    if (!this.enabled) {
      return { cancel: true };
    }

    const { tabId, url } = details;
    const logo = new URL(url).pathname.slice(1);

    for (const shim of this.shims.values()) {
      await shim.ready;

      if (!shim.enabled) {
        continue;
      }

      if (!shim.logos.includes(logo)) {
        continue;
      }

      if (shim.isActiveOnTab(tabId)) {
        return { redirectUrl: browser.runtime.getURL(`shims/${logo}`) };
      }
    }

    return { cancel: true };
  }

  async _ensureShimForRequestOnTab(details) {
    await this._haveCheckedEnabledPref;

    if (!this.enabled) {
      return undefined;
    }

    // We only ever reach this point if a request is for a URL which ought to
    // be shimmed. We never get here if a request is blocked, and we only
    // unblock requests if at least one shim matches it.

    const { frameId, originUrl, requestId, tabId, type, url } = details;

    // Ignore requests unrelated to tabs
    if (tabId < 0) {
      return undefined;
    }

    // We need to base our checks not on the frame's host, but the tab's.
    const topHost = new URL((await browser.tabs.get(tabId)).url).hostname;
    const unblocked = await browser.trackingProtection.wasRequestUnblocked(
      requestId
    );

    let match;
    let shimToApply;
    for (const shim of this.shims.values()) {
      await shim.ready;

      if (!shim.enabled) {
        continue;
      }

      // Do not apply the shim if it is only meant to apply when strict mode ETP
      // (content blocking) was going to block the request.
      if (!unblocked && shim.onlyIfBlockedByETP) {
        continue;
      }

      if (!shim.meantForHost(topHost)) {
        continue;
      }

      // If this URL and content type isn't meant for this shim, don't apply it.
      match = shim.isTriggeredByURLAndType(url, type);
      if (match) {
        // If the user has already opted in for this shim, all requests it covers
        // should be allowed; no need for a shim anymore.
        if (shim.hasUserOptedInAlready(topHost)) {
          warn(
            `Allowing tracking ${type} ${url} on tab ${tabId} frame ${frameId} due to opt-in`
          );
          shim.showOptInWarningOnce(tabId, new URL(originUrl).origin);
          return undefined;
        }
        shimToApply = shim;
        break;
      }
    }

    if (shimToApply) {
      // Note that sites may request the same shim twice, but because the requests
      // may differ enough for some to fail (CSP/CORS/etc), we always let the request
      // complete via local redirect. Shims should gracefully handle this as well.

      const { target } = match;
      const { bug, file, id, name, needsShimHelpers } = shimToApply;

      const redirect = target || file;

      warn(
        `Shimming tracking ${type} ${url} on tab ${tabId} frame ${frameId} with ${redirect}`
      );

      const warning = `${name} is being shimmed by Firefox. See https://bugzilla.mozilla.org/show_bug.cgi?id=${bug} for details.`;

      try {
        // For scripts, we also set up any needed shim helpers.
        if (type === "script" && needsShimHelpers?.length) {
          await browser.tabs.executeScript(tabId, {
            file: "/lib/shim_messaging_helper.js",
            frameId,
            runAt: "document_start",
          });
          const origin = new URL(originUrl).origin;
          await browser.tabs.sendMessage(
            tabId,
            { origin, shimId: id, needsShimHelpers, warning },
            { frameId }
          );
          shimToApply.setActiveOnTab(tabId);
        } else {
          await browser.tabs.executeScript(tabId, {
            code: `console.warn(${JSON.stringify(warning)})`,
            runAt: "document_start",
          });
        }
      } catch (_) {}

      // If any shims matched the request to replace it, then redirect to the local
      // file bundled with SmartBlock, so the request never hits the network.
      return { redirectUrl: browser.runtime.getURL(`shims/${redirect}`) };
    }

    // Sanity check: if no shims end up handling this request,
    // yet it was meant to be blocked by ETP, then block it now.
    if (unblocked) {
      error(`unexpected: ${url} not shimmed on tab ${tabId} frame ${frameId}`);
      return { cancel: true };
    }

    debug(`ignoring ${url} on tab ${tabId} frame ${frameId}`);
    return undefined;
  }
}

module.exports = Shims;
