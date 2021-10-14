/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* globals ExtensionAPI, Services, XPCOMUtils, WebExtensionPolicy */

XPCOMUtils.defineLazyGlobalGetters(this, ["ChannelWrapper"]);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "ProxyService",
  "@mozilla.org/network/protocol-proxy-service;1",
  "nsIProtocolProxyService"
);
XPCOMUtils.defineLazyServiceGetter(
  this,
  "NSSErrorsService",
  "@mozilla.org/nss_errors_service;1",
  "nsINSSErrorsService"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  ExtensionParent: "resource://gre/modules/ExtensionParent.jsm",
  ExtensionPreferencesManager:
    "resource://gre/modules/ExtensionPreferencesManager.jsm",
});

XPCOMUtils.defineLazyGetter(
  this,
  "Management",
  () => ExtensionParent.apiManager
);

const PROXY_DIRECT = "direct";
const DISABLE_HOURS = 48;
const MAX_DISABLED_PI = 2;
const PREF_MONITOR_DATA = "extensions.proxyMonitor.state";
const PREF_MONITOR_LOGGING = "extensions.proxyMonitor.logging.enabled";
const PREF_PROXY_FAILOVER = "network.proxy.failover_direct";
const CHECK_EXTENSION_ONLY =
  Services.vc.compare(Services.appinfo.version, "92.0") >= 0;

const PROXY_CONFIG_TYPES = [
  "direct",
  "manual",
  "pac",
  "unused", // nsIProtocolProxyService.idl skips index 3.
  "wpad",
  "system",
];

function hoursSince(dt2, dt1 = Date.now()) {
  var diff = (dt2 - dt1) / 1000;
  diff /= 60 * 60;
  return Math.abs(Math.round(diff));
}

const DEBUG_LOG = Services.prefs.getBoolPref(PREF_MONITOR_LOGGING, true);
function log(msg) {
  if (DEBUG_LOG) {
    console.log(`proxy-monitor: ${msg}`);
  }
}

/**
 * ProxyMonitor monitors system and protected requests for failures due to bad
 * or unavailable proxy configurations.
 *
 * In a system with multiple layers of proxy configuration, if there is a
 * failing proxy we try to remove just that confuration from the chain.
 * However if we get too many failures, we'll make a direct connection the top
 * "proxy".
 *
 * 1. Any proxied system request without a direct failover will have one added.
 *
 * 2. If a proxied system request fails, the proxy configuration in use will be
 * disabled.  On later requests, disabled proxies are removed from the proxy
 * chain.  Disabled proxy configurations remain disabled for 48 hours to allow
 * any necessary requests to operate for a period of time. When disabled
 * proxies are used as a failover to a direct request (step 3 or 4 below), the
 * proxy can be detected as functional and be re-enabled despite not having
 * reached the 48 hours.
 *
 * 3. If too many proxy configurations got disabled, we make a direct config
 * first with failover to all other proxy configurations (essentially skipping
 * step 2).  This state remains for 48 hours before retrying without "direct".
 *
 * 4. If we've removed all proxies we make a direct config first and failover
 * to the other proxy configurations, similar to step 3.
 *
 * 5. Starting with Fx92, we will only disable proxy configurations provided by
 * extensions.  Prior to 92, we could not definitively identify extensions from
 * the proxyInfo instance.
 *
 * If we've disabled proxies, we continue to watch the requests for failures in
 * "direct" connection mode.  If we continue to fail with direct connections,
 * we fall back to allowing proxies again.
 */
const ProxyMonitor = {
  errors: new Map(),
  extensions: new Map(),
  disabledTime: 0,

  newDirectProxyInfo(failover = null) {
    return ProxyService.newProxyInfo(
      PROXY_DIRECT,
      "",
      0,
      "",
      "",
      0,
      0,
      failover
    );
  },

  async applyFilter(channel, defaultProxyInfo, proxyFilter) {
    let proxyInfo = defaultProxyInfo;
    // onProxyFilterResult must be called, so we wrap in a try/finally.
    try {
      if (!proxyInfo) {
        // If no proxy is in use, exit early.
        return;
      }
      // If this is not a system request we will allow existing
      // proxy behavior.
      if (!channel.loadInfo?.loadingPrincipal?.isSystemPrincipal) {
        return;
      }

      // We monitor for successful connections which in some cases may
      // re-enable a prior failed proxy configuration.
      let wrapper = ChannelWrapper.get(channel);
      wrapper.addEventListener("start", this);

      if (this.tooManyFailures()) {
        log(`too many proxy config failures, prepend direct rid ${wrapper.id}`);
        // A lot of failures are happening.  Try direct first, but failover to
        // any non-extension proxies "just in case".
        proxyInfo = this.newDirectProxyInfo(
          await this.pruneExtensions(defaultProxyInfo)
        );
        return;
      }
      this.dumpProxies(proxyInfo, `starting proxyInfo rid ${wrapper.id}`);

      proxyInfo = this.pruneProxyInfo(proxyInfo);
      if (!proxyInfo) {
        // All current proxies are disabled due to prior failures.  Try direct
        // first, but failover to any non-extension proxies "just in case".
        log(`all proxies disabled, prepend direct`);
        proxyInfo = this.newDirectProxyInfo(
          await this.pruneExtensions(defaultProxyInfo)
        );
        return;
      }

      // If we are not attempting a direct bypass we want to monitor for
      // non-connection errors such as invalid proxy servers.
      wrapper.addEventListener("error", this);

      // A little debug output
      this.dumpProxies(proxyInfo, `pruned proxyInfo rid ${wrapper.id}`);
    } finally {
      // This must be called.
      proxyFilter.onProxyFilterResult(proxyInfo);
    }
  },

  relinkProxyInfoChain(proxies) {
    if (!proxies.length) {
      return null;
    }
    // Re-link the proxy chain.
    // failoverProxy cannot be set to undefined or null, we fixup the last
    // failover with a direct failover if necessary.
    for (let i = 0; i < proxies.length - 2; i++) {
      proxies[i].failoverProxy = proxies[i + 1];
    }
    let top = proxies[0];
    let last = proxies.pop();
    // Ensure the last proxy is not linked to something we removed.  This
    // catches connection failures such as those to non-existant or non-http
    // ports.  The "error" handler added above catches http connections that
    // are not proxy servers.
    if (last.failoverProxy || last.type != PROXY_DIRECT) {
      last.failoverProxy = this.newDirectProxyInfo();
    }
    return top;
  },

  async pruneExtensions(proxyInfo) {
    // If an extension controls the settings, we must assume that all PIs
    // came from the extension.
    let extensionId = await this.getControllingExtension();
    if (extensionId) {
      return null;
    }
    let enabledProxies = [];
    let pi = proxyInfo;
    while (pi) {
      if (!pi.sourceId) {
        enabledProxies.push(pi);
      }
      pi = pi.failoverProxy;
    }
    return this.relinkProxyInfoChain(enabledProxies);
  },

  // Verify the entire proxy failover chain is clean.  There may be multiple
  // sources for proxyInfo in the chain, so we remove any disabled entries and
  // continue to use configurations that have not yet failed.
  pruneProxyInfo(proxyInfo) {
    let enabledProxies = [];
    let pi = proxyInfo;
    while (pi) {
      if (!this.proxyDisabled(pi)) {
        enabledProxies.push(pi);
      }
      pi = pi.failoverProxy;
    }
    return this.relinkProxyInfoChain(enabledProxies);
  },

  dumpProxies(proxyInfo, msg) {
    if (!DEBUG_LOG) {
      return;
    }
    log(msg);
    let pi = proxyInfo;
    while (pi) {
      log(`  ${pi.type}:${pi.host}:${pi.port}`);
      pi = pi.failoverProxy;
    }
  },

  tooManyFailures() {
    // If we have lots of PIs that are failing in a short period of time then
    // we back off proxy for a while.
    if (this.disabledTime && hoursSince(this.disabledTime) >= DISABLE_HOURS) {
      this.recordEvent("timeout", "proxyBypass", "global");
      this.reset();
    }
    return !!this.disabledTime;
  },

  proxyDisabled(proxyInfo) {
    let key = this.getProxyInfoKey(proxyInfo);
    if (!key) {
      return false;
    }

    // From 92 forward, if an extension has one disabled PI, we disable all PIs
    // from that extension for the DISABLE_HOURS perid.
    let extTime = proxyInfo.sourceId && this.extensions.get(proxyInfo.sourceId);
    if (extTime && hoursSince(extTime) <= DISABLE_HOURS) {
      return true;
    }

    let err = this.errors.get(key);
    if (!err) {
      return false;
    }

    // We keep a proxy config disabled for DISABLE_HOURS to give our daily
    // update checks time to complete again.
    if (hoursSince(err.time) >= DISABLE_HOURS) {
      this.errors.delete(key);
      this.logProxySource("timeout", proxyInfo);
      return false;
    }

    // This is harsh, but these requests are too important.
    return true;
  },

  getProxyInfoKey(proxyInfo) {
    if (!proxyInfo || proxyInfo.type == PROXY_DIRECT) {
      return;
    }
    let { type, host, port } = proxyInfo;
    // eslint-disable-next-line consistent-return
    return `${type}:${host}:${port}`;
  },

  // If proxy.settings is used to change the proxy, an extension will be "in
  // control".  This returns the id of that extension.
  async getControllingExtension() {
    // Is this proxied by an extension that set proxy prefs?
    let setting = await ExtensionPreferencesManager.getSetting(
      "proxy.settings"
    );
    return setting?.id;
  },

  async getProxySource(proxyInfo) {
    // sourceId is set when using proxy.onRequest
    if (proxyInfo.sourceId) {
      return {
        source: proxyInfo.sourceId,
        type: "api",
      };
    }
    let type = PROXY_CONFIG_TYPES[ProxyService.proxyConfigType] || "unknown";

    // If we have a policy it will have set the prefs.
    if (Services.policies.status === Services.policies.ACTIVE) {
      let policies = Services.policies
        .getActivePolicies()
        ?.filter(p => p.Proxy);
      if (policies?.length) {
        return {
          source: "policy",
          type,
        };
      }
    }

    let source = await this.getControllingExtension();
    return {
      source: source || "prefs",
      type,
    };
  },

  async logProxySource(state, proxyInfo) {
    let { source, type } = await this.getProxySource(proxyInfo);
    this.recordEvent(state, "proxyInfo", type, { source });
  },

  recordEvent(method, obj, type = null, source = {}) {
    try {
      Services.telemetry.recordEvent("proxyMonitor", method, obj, type, source);
      log(`event: ${method} ${obj} ${type} ${JSON.stringify(source)}`);
    } catch (err) {
      // If the telemetry throws just log the error so it doesn't break any
      // functionality.
      Cu.reportError(err);
    }
  },

  timeoutEntries() {
    // remove old entries
    for (let [k, err] of this.errors) {
      if (hoursSince(err.time) >= DISABLE_HOURS) {
        this.errors.delete(k);
        this.recordEvent("timeout", "proxyInfo");
      }
    }
    for (let [e, t] of this.extensions) {
      if (hoursSince(t) >= DISABLE_HOURS) {
        this.extensions.delete(e);
        // Not a full bypass, but an extension bypass
        this.recordEvent("timeout", "proxyBypass", "extension", { source: e });
      }
    }
  },

  async disableProxyInfo(proxyInfo) {
    this.dumpProxies(proxyInfo, "disableProxyInfo");
    let key = this.getProxyInfoKey(proxyInfo);
    if (!key) {
      log(`direct request failure`);
      return;
    }

    // From 92 forward, we disable all extension provided proxies if one fails
    let extensionId;
    if (CHECK_EXTENSION_ONLY) {
      extensionId =
        proxyInfo.sourceId || (await this.getControllingExtension());
    }

    this.timeoutEntries();

    let err = { time: Date.now(), extensionId };
    this.errors.set(key, err);
    if (extensionId) {
      this.extensions.set(extensionId, err.time);
      log(`all proxy configuration from extension ${extensionId} disabled`);
      this.recordEvent("start", "proxyBypass", "extension", {
        source: extensionId,
      });
    }
    this.logProxySource("disabled", proxyInfo);
    // If lots of proxies have failed, we
    // disable all proxies for a while to ensure system
    // requests have the best oportunity to get
    // through.
    if (!this.disabledTime && this.errors.size >= MAX_DISABLED_PI) {
      this.disabledTime = Date.now();
      this.recordEvent("start", "proxyBypass", "global");
    }
  },

  async enableProxyInfo(proxyInfo) {
    let key = this.getProxyInfoKey(proxyInfo);
    if (!key) {
      return;
    }
    if (this.errors.delete(key)) {
      this.logProxySource("enabled", proxyInfo);
    }
    // From 92 forward, we have tracked extensions.  If no keys are disabled,
    // remove the extension from the disabled list.
    if (!CHECK_EXTENSION_ONLY) {
      return;
    }
    let extensionId =
      proxyInfo.sourceId || (await this.getControllingExtension());
    if (!extensionId) {
      return;
    }
    // Only delete if no err entries with the id exists.
    // eslint-disable-next-line no-unused-vars
    for (let [k, err] of this.errors) {
      if (err.extensionId == extensionId) {
        return;
      }
    }
    this.extensions.delete(extensionId);
  },

  tlsCheck(channel) {
    let securityInfo = channel.securityInfo;
    if (!securityInfo) {
      return false;
    }
    securityInfo.QueryInterface(Ci.nsITransportSecurityInfo);
    if (NSSErrorsService.isNSSErrorCode(securityInfo.errorCode)) {
      return false;
    }

    const wpl = Ci.nsIWebProgressListener;
    const state = securityInfo.securityState;
    return !!(state & wpl.STATE_IS_SECURE);
  },

  handleEvent(event) {
    let wrapper = event.currentTarget; // channel wrapper
    let { channel } = wrapper;
    if (!(channel instanceof Ci.nsIProxiedChannel)) {
      log(`got ${event.type} event but not a proxied channel`);
      return;
    }
    // If this is an http request ignore it, it is too easily tampered with.
    // Fortunately its use is limited, potentially only captive portal.
    if (wrapper.finalURL.startsWith("http:")) {
      return;
    }

    // The tls handshake must succeed to re-enable a request.
    let tlsIsSecure = this.tlsCheck(channel);

    log(
      `request event ${event.type} rid ${wrapper.id} status ${wrapper.statusCode} tls ${tlsIsSecure} for ${channel.URI.spec}`
    );
    let status = wrapper.statusCode;
    switch (event.type) {
      case "error":
        if (!tlsIsSecure || status == 0) {
          this.disableProxyInfo(channel.proxyInfo);
        }
        break;
      case "start":
        if (tlsIsSecure && status >= 200 && status < 400) {
          this.enableProxyInfo(channel.proxyInfo);
        }
        break;
      default:
        break;
    }
  },

  reset() {
    this.disabledTime = 0;
    this.errors = new Map();
  },

  store() {
    if (!this.disabledTime && !this.errors.size) {
      Services.prefs.clearUserPref(PREF_MONITOR_DATA);
      return;
    }
    let data = JSON.stringify({
      disabledTime: this.disabledTime,
      errors: Array.from(this.errors),
    });
    Services.prefs.setStringPref(PREF_MONITOR_DATA, data);
  },

  restore() {
    let failovers = Services.prefs.getStringPref(PREF_MONITOR_DATA, null);
    if (failovers) {
      failovers = JSON.parse(failovers);
      this.disabledTime = failovers.disabledTime;
      this.errors = new Map(failovers.errors);
      this.extensions = new Map(
        failovers.errors
          .filter(e => e[1].extensionId)
          .sort((a, b) => a[1].time - b[1].time)
          .map(e => [e[1].extensionId, e[1].time])
      );
    } else {
      this.disabledTime = 0;
      this.errors = new Map();
      this.extensions = new Map();
    }
  },

  startup() {
    // Register filter with a very high position, this will sort to the last
    // filter called.
    ProxyService.registerChannelFilter(ProxyMonitor, Number.MAX_SAFE_INTEGER);
    this.restore();
    log("started");
  },

  shutdown() {
    ProxyService.unregisterFilter(ProxyMonitor);
    this.store();
    log("stopped");
  },
};

/**
 * Listen for changes in addons and pref to start or stop the ProxyMonitor.
 */
const monitor = {
  running: false,

  startup() {
    if (!this.failoverEnabled) {
      return;
    }

    Management.on("startup", this.handleEvent);
    Management.on("shutdown", this.handleEvent);
    Management.on("change-permissions", this.handleEvent);
    if (this.hasProxyExtension()) {
      monitor.startMonitors();
    }
  },

  shutdown() {
    Management.off("startup", this.handleEvent);
    Management.off("shutdown", this.handleEvent);
    Management.off("change-permissions", this.handleEvent);
    monitor.stopMonitors();
  },

  get failoverEnabled() {
    return Services.prefs.getBoolPref(PREF_PROXY_FAILOVER, true);
  },

  observe() {
    if (monitor.failoverEnabled) {
      monitor.startup();
    } else {
      monitor.shutdown();
    }
  },

  startMonitors() {
    if (!monitor.running) {
      ProxyMonitor.startup();
      monitor.running = true;
    }
  },

  stopMonitors() {
    if (monitor.running) {
      ProxyMonitor.shutdown();
      monitor.running = false;
    }
  },

  hasProxyExtension(ignore) {
    for (let policy of WebExtensionPolicy.getActiveExtensions()) {
      if (
        policy.id != ignore &&
        !policy.extension?.isAppProvided &&
        policy.hasPermission("proxy")
      ) {
        return true;
      }
    }
    return false;
  },

  handleEvent(kind, ...args) {
    switch (kind) {
      case "startup": {
        let [extension] = args;
        if (
          !monitor.running &&
          !extension.isAppProvided &&
          extension.hasPermission("proxy")
        ) {
          monitor.startMonitors();
        }
        break;
      }
      case "shutdown": {
        if (Services.startup.shuttingDown) {
          // Let normal shutdown handle things.
          break;
        }
        let [extension] = args;
        // WebExtensionPolicy is still active, pass the id to ignore it.
        if (
          monitor.running &&
          !extension.isAppProvided &&
          !monitor.hasProxyExtension(extension.id)
        ) {
          monitor.stopMonitors();
        }
        break;
      }
      case "change-permissions": {
        if (monitor.running) {
          break;
        }
        let { extensionId, added } = args[0];
        if (!added?.permissions.includes("proxy")) {
          return;
        }
        let extension = WebExtensionPolicy.getByID(extensionId)?.extension;
        if (extension && !extension.isAppProvided) {
          monitor.startMonitors();
        }
        break;
      }
    }
  },
};

this.failover = class extends ExtensionAPI {
  onStartup() {
    Services.telemetry.registerEvents("proxyMonitor", {
      proxyMonitor: {
        methods: ["enabled", "disabled", "start", "timeout"],
        objects: ["proxyInfo", "proxyBypass"],
        extra_keys: ["source"],
        record_on_release: true,
      },
    });

    monitor.startup();
    Services.prefs.addObserver(PREF_PROXY_FAILOVER, monitor);
  }

  onShutdown() {
    monitor.shutdown();
    Services.prefs.removeObserver(PREF_PROXY_FAILOVER, monitor);
  }
};
