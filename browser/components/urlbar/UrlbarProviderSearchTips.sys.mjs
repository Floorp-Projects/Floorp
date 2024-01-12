/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This module exports a provider that might show a tip when the user opens
 * the newtab or starts an organic search with their default search engine.
 */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

import {
  UrlbarProvider,
  UrlbarUtils,
} from "resource:///modules/UrlbarUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  AppMenuNotifications: "resource://gre/modules/AppMenuNotifications.sys.mjs",
  DefaultBrowserCheck: "resource:///modules/BrowserGlue.sys.mjs",
  LaterRun: "resource:///modules/LaterRun.sys.mjs",
  SearchStaticData: "resource://gre/modules/SearchStaticData.sys.mjs",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
  UrlbarProviderTopSites: "resource:///modules/UrlbarProviderTopSites.sys.mjs",
  UrlbarResult: "resource:///modules/UrlbarResult.sys.mjs",
  UrlbarSearchUtils: "resource:///modules/UrlbarSearchUtils.sys.mjs",
  setTimeout: "resource://gre/modules/Timer.sys.mjs",
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "cfrFeaturesUserPref",
  "browser.newtabpage.activity-stream.asrouter.userprefs.cfr.features",
  true
);

// The possible tips to show.  These names (except NONE) are used in the names
// of keys in the `urlbar.tips` keyed scalar telemetry (see telemetry.rst).
// Don't modify them unless you've considered that.  If you do modify them or
// add new tips, then you are also adding new `urlbar.tips` keys and therefore
// need an expanded data collection review.
const TIPS = {
  NONE: "",
  ONBOARD: "searchTip_onboard",
  PERSIST: "searchTip_persist",
  REDIRECT: "searchTip_redirect",
};

ChromeUtils.defineLazyGetter(lazy, "SUPPORTED_ENGINES", () => {
  // Converts a list of Google domains to a pipe separated string of escaped TLDs.
  // [www.google.com, ..., www.google.co.uk] => "com|...|co\.uk"
  const googleTLDs = lazy.SearchStaticData.getAlternateDomains("www.google.com")
    .map(str => str.slice("www.google.".length).replaceAll(".", "\\."))
    .join("|");

  // This maps engine names to regexes matching their homepages. We show the
  // redirect tip on these pages.
  return new Map([
    ["Bing", { domainPath: /^www\.bing\.com\/$/ }],
    [
      "DuckDuckGo",
      {
        domainPath: /^(start\.)?duckduckgo\.com\/$/,
        prohibitedSearchParams: ["q"],
      },
    ],
    [
      "Google",
      {
        domainPath: new RegExp(`^www\.google\.(?:${googleTLDs})\/(webhp)?$`),
      },
    ],
  ]);
});

// The maximum number of times we'll show a tip across all sessions.
const MAX_SHOWN_COUNT = 4;

// Amount of time to wait before showing a tip after selecting a tab or
// navigating to a page where we should show a tip.
const SHOW_TIP_DELAY_MS = 200;

// Amount of time to wait before showing the persist tip after the
// onLocationChange event during the process of loading
// a default search engine results page.
const SHOW_PERSIST_TIP_DELAY_MS = 1500;

// We won't show a tip if the browser has been updated in the past
// LAST_UPDATE_THRESHOLD_HOURS.
const LAST_UPDATE_THRESHOLD_HOURS = 24;

/**
 * A provider that sometimes returns a tip result when the user visits the
 * newtab page or their default search engine's homepage.
 */
class ProviderSearchTips extends UrlbarProvider {
  constructor() {
    super();

    // Whether we should disable tips for the current browser session, for
    // example because a tip was already shown.
    this.disableTipsForCurrentSession = true;
    for (let tip of Object.values(TIPS)) {
      if (
        tip &&
        lazy.UrlbarPrefs.get(`tipShownCount.${tip}`) < MAX_SHOWN_COUNT
      ) {
        this.disableTipsForCurrentSession = false;
        break;
      }
    }

    // Whether and what kind of tip we've shown in the current engagement.
    this.showedTipTypeInCurrentEngagement = TIPS.NONE;

    // Used to track browser windows we've seen.
    this._seenWindows = new WeakSet();
  }

  /**
   * Enum of the types of search tips.
   *
   * @returns {{ NONE: string; ONBOARD: string; PERSIST: string; REDIRECT: string; }}
   */
  get TIP_TYPE() {
    return TIPS;
  }

  get PRIORITY() {
    // Search tips are prioritized over the Places and top sites providers.
    return lazy.UrlbarProviderTopSites.PRIORITY + 1;
  }

  get SHOW_PERSIST_TIP_DELAY_MS() {
    return SHOW_PERSIST_TIP_DELAY_MS;
  }

  /**
   * Unique name for the provider, used by the context to filter on providers.
   * Not using a unique name will cause the newest registration to win.
   *
   * @returns {string}
   */
  get name() {
    return "UrlbarProviderSearchTips";
  }

  /**
   * The type of the provider.
   *
   * @returns {UrlbarUtils.PROVIDER_TYPE}
   */
  get type() {
    return UrlbarUtils.PROVIDER_TYPE.PROFILE;
  }

  /**
   * Whether this provider should be invoked for the given context.
   * If this method returns false, the providers manager won't start a query
   * with this provider, to save on resources.
   *
   * @param {UrlbarQueryContext} queryContext The query context object
   * @returns {boolean} Whether this provider should be invoked for the search.
   */
  isActive(queryContext) {
    return this.currentTip && lazy.cfrFeaturesUserPref;
  }

  /**
   * Gets the provider's priority.
   *
   * @param {UrlbarQueryContext} queryContext The query context object
   * @returns {number} The provider's priority for the given query.
   */
  getPriority(queryContext) {
    return this.PRIORITY;
  }

  /**
   * Starts querying. Extended classes should return a Promise resolved when the
   * provider is done searching AND returning results.
   *
   * @param {UrlbarQueryContext} queryContext The query context object
   * @param {Function} addCallback Callback invoked by the provider to add a new
   *        result. A UrlbarResult should be passed to it.
   * @returns {Promise}
   */
  async startQuery(queryContext, addCallback) {
    let instance = this.queryInstance;

    let tip = this.currentTip;
    this.showedTipTypeInCurrentEngagement = this.currentTip;
    this.currentTip = TIPS.NONE;

    let defaultEngine = await Services.search.getDefault();
    if (instance != this.queryInstance) {
      return;
    }

    let result = new lazy.UrlbarResult(
      UrlbarUtils.RESULT_TYPE.TIP,
      UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
      {
        type: tip,
        buttons: [{ l10n: { id: "urlbar-search-tips-confirm" } }],
        icon: defaultEngine.getIconURL(),
      }
    );

    switch (tip) {
      case TIPS.ONBOARD:
        result.heuristic = true;
        result.payload.titleL10n = {
          id: "urlbar-search-tips-onboard",
          args: {
            engineName: defaultEngine.name,
          },
        };
        break;
      case TIPS.REDIRECT:
        result.heuristic = false;
        result.payload.titleL10n = {
          id: "urlbar-search-tips-redirect-2",
          args: {
            engineName: defaultEngine.name,
          },
        };
        break;
      case TIPS.PERSIST:
        result.heuristic = false;
        result.payload.titleL10n = {
          id: "urlbar-search-tips-persist",
        };
        result.payload.icon = UrlbarUtils.ICON.TIP;
        result.payload.buttons = [
          { l10n: { id: "urlbar-search-tips-confirm-short" } },
        ];
        break;
    }

    Services.telemetry.keyedScalarAdd("urlbar.tips", `${tip}-shown`, 1);

    addCallback(this, result);
  }

  /**
   * Called when the tip is selected.
   *
   * @param {UrlbarResult} result
   *   The result that was picked.
   * @param {window} window
   *   The browser window in which the tip is being displayed.
   */
  #pickResult(result, window) {
    let tip = result.payload.type;
    switch (tip) {
      case TIPS.PERSIST:
        window.gURLBar.removeAttribute("suppress-focus-border");
        window.gURLBar.select();
        break;
      default:
        window.gURLBar.value = "";
        window.gURLBar.setPageProxyState("invalid");
        window.gURLBar.removeAttribute("suppress-focus-border");
        window.gURLBar.focus();
        break;
    }

    // The user either clicked the tip's "Okay, Got It" button, or they clicked
    // in the urlbar while the tip was showing. We treat both as the user's
    // acknowledgment of the tip, and we don't show tips again in any session.
    // Set the shown count to the max.
    lazy.UrlbarPrefs.set(`tipShownCount.${tip}`, MAX_SHOWN_COUNT);
  }

  onEngagement(state, queryContext, details, controller) {
    // Ignore engagements on other results that didn't end the session.
    let { result } = details;
    if (result?.providerName != this.name && details.isSessionOngoing) {
      return;
    }

    if (result?.providerName == this.name) {
      this.#pickResult(result, controller.browserWindow);
    }

    this.showedTipTypeInCurrentEngagement = TIPS.NONE;
  }

  /**
   * Called from `onLocationChange` in browser.js.
   *
   * @param {window} window
   *  The browser window where the location change happened.
   * @param {nsIURI} uri
   *  The URI being navigated to.
   * @param {nsIURI | null} originalUri
   *  The original URI being navigated to.
   * @param {nsIWebProgress} webProgress
   *   The progress object, which can have event listeners added to it.
   * @param {number} flags
   *   Load flags. See nsIWebProgressListener.idl for possible values.
   */
  async onLocationChange(window, uri, originalUri, webProgress, flags) {
    let instance = (this._onLocationChangeInstance = {});

    // If this is the first time we've seen this browser window, we take some
    // precautions to avoid impacting ts_paint.
    if (!this._seenWindows.has(window)) {
      this._seenWindows.add(window);

      // First, wait until MozAfterPaint is fired in the current content window.
      await window.gBrowserInit.firstContentWindowPaintPromise;
      if (instance != this._onLocationChangeInstance) {
        return;
      }

      // Second, wait 500ms.  ts_paint waits at most 500ms after MozAfterPaint
      // before ending.  We use XPCOM directly instead of Timer.sys.mjs to avoid the
      // perf impact of loading Timer.sys.mjs, in case it's not already loaded.
      await new Promise(resolve => {
        let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
        timer.initWithCallback(resolve, 500, Ci.nsITimer.TYPE_ONE_SHOT);
      });
      if (instance != this._onLocationChangeInstance) {
        return;
      }
    }

    // Ignore events that don't change the document. Google is known to do this.
    // Also ignore changes in sub-frames. See bug 1623978.
    if (
      flags & Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT ||
      !webProgress.isTopLevel
    ) {
      return;
    }

    // The UrlbarView is usually closed on location change when the input is
    // blurred. Since we open the view to show the redirect tip without focusing
    // the input, the view won't close in that case. We need to close it
    // manually.
    if (this.showedTipTypeInCurrentEngagement != TIPS.NONE) {
      window.gURLBar.view.close();
    }

    // Check if we are supposed to show a tip for the current session.
    if (
      !lazy.cfrFeaturesUserPref ||
      (this.disableTipsForCurrentSession &&
        !lazy.UrlbarPrefs.get("searchTips.test.ignoreShowLimits"))
    ) {
      return;
    }

    this._maybeShowTipForUrl(uri.spec, originalUri, window).catch(ex =>
      this.logger.error(ex)
    );
  }

  /**
   * Determines whether we should show a tip for the current tab, sets
   * this.currentTip, and starts a search on an empty string.
   *
   * @param {string} urlStr
   *   The URL of the page being loaded, in string form.
   * @param {nsIURI | null} originalUri
   *   The original URI of the page being loaded.
   * @param {window} window
   *   The browser window in which the tip is being displayed.
   */
  async _maybeShowTipForUrl(urlStr, originalUri, window) {
    let instance = {};
    this._maybeShowTipForUrlInstance = instance;

    let ignoreShowLimits = lazy.UrlbarPrefs.get(
      "searchTips.test.ignoreShowLimits"
    );

    // Determine which tip we should show for the tab.  Do this check first
    // before the others below.  It has less of a performance impact than the
    // others, so in the common case where the URL is not one we're interested
    // in, we can return immediately.
    let tip;
    let isNewtab = ["about:newtab", "about:home"].includes(urlStr);
    let isSearchHomepage = !isNewtab && (await isDefaultEngineHomepage(urlStr));

    // Only show the persist tip if: the feature is enabled,
    // it's been shown fewer than the maximum number of times
    // a specific tip can be shown to the user, and the
    // the url is a default SERP.
    let shouldShowPersistTip =
      lazy.UrlbarPrefs.isPersistedSearchTermsEnabled() &&
      (lazy.UrlbarPrefs.get(`tipShownCount.${TIPS.PERSIST}`) <
        MAX_SHOWN_COUNT ||
        ignoreShowLimits) &&
      !!lazy.UrlbarSearchUtils.getSearchTermIfDefaultSerpUri(
        originalUri ?? urlStr
      );

    if (isNewtab) {
      tip = TIPS.ONBOARD;
    } else if (isSearchHomepage) {
      tip = TIPS.REDIRECT;
    } else if (shouldShowPersistTip) {
      tip = TIPS.PERSIST;
    } else {
      // No tip.
      return;
    }

    // If we've shown this type of tip the maximum number of times over all
    // sessions, don't show it again.
    let shownCount = lazy.UrlbarPrefs.get(`tipShownCount.${tip}`);
    if (shownCount >= MAX_SHOWN_COUNT && !ignoreShowLimits) {
      return;
    }

    // Don't show a tip if the browser has been updated recently.
    // Exception: TIPS.PERSIST should show immediately
    // after the feature is enabled for users.
    let hoursSinceUpdate = Math.min(
      lazy.LaterRun.hoursSinceInstall,
      lazy.LaterRun.hoursSinceUpdate
    );
    if (
      tip != TIPS.PERSIST &&
      hoursSinceUpdate < LAST_UPDATE_THRESHOLD_HOURS &&
      !ignoreShowLimits
    ) {
      return;
    }

    let tipDelay =
      tip == TIPS.PERSIST ? SHOW_PERSIST_TIP_DELAY_MS : SHOW_TIP_DELAY_MS;

    // Start a search.
    lazy.setTimeout(async () => {
      if (this._maybeShowTipForUrlInstance != instance) {
        return;
      }

      // We don't want to interrupt a user's typed query with a Search Tip.
      // See bugs 1613662 and 1619547. The persist search tip is an
      // exception because the query is not erased.
      if (
        window.gURLBar.getAttribute("pageproxystate") == "invalid" &&
        window.gURLBar.value != ""
      ) {
        return;
      }

      // The tab that initiated the tip might not be in the same window
      // as the one that is currently at the top. Only apply this search
      // tip to a tab showing a search term.
      if (tip == TIPS.PERSIST && !window.gBrowser.selectedBrowser.searchTerms) {
        return;
      }

      // Don't show a tip if the browser is already showing some other
      // notification.
      if (
        (!ignoreShowLimits && (await isBrowserShowingNotification(window))) ||
        this._maybeShowTipForUrlInstance != instance
      ) {
        return;
      }

      // Don't show a tip if a request is in progress, and the URI associated
      // with the request differs from the URI that triggered the search tip.
      // One contraint with this approach is related to Bug 1797748: SERPs
      // that use the History API to navigate between views will call
      // onLocationChange without a request, and thus, no originalUri is
      // available to check against, so the search tip and search terms may
      // show on search pages outside of the default SERP.
      let { documentRequest } = window.gBrowser.selectedBrowser.webProgress;
      if (
        documentRequest instanceof Ci.nsIChannel &&
        documentRequest.originalURI?.spec != originalUri?.spec &&
        (!isNewtab || originalUri)
      ) {
        return;
      }

      // At this point, we're showing a tip.
      this.disableTipsForCurrentSession = true;

      // Store the new shown count.
      lazy.UrlbarPrefs.set(`tipShownCount.${tip}`, shownCount + 1);

      this.currentTip = tip;

      let value =
        tip == TIPS.PERSIST ? window.gBrowser.selectedBrowser.searchTerms : "";
      window.gURLBar.search(value, { focus: tip == TIPS.ONBOARD });
    }, tipDelay);
  }
}

async function isBrowserShowingNotification(window) {
  // urlbar view and notification box (info bar)
  if (
    window.gURLBar.view.isOpen ||
    window.gNotificationBox.currentNotification ||
    window.gBrowser.getNotificationBox().currentNotification
  ) {
    return true;
  }

  // app menu notification doorhanger
  if (
    lazy.AppMenuNotifications.activeNotification &&
    !lazy.AppMenuNotifications.activeNotification.dismissed &&
    !lazy.AppMenuNotifications.activeNotification.options.badgeOnly
  ) {
    return true;
  }

  // PopupNotifications (e.g. Tracking Protection, Identity Box Doorhangers)
  if (window.PopupNotifications.isPanelOpen) {
    return true;
  }

  // page action button panels
  let pageActions = window.document.getElementById("page-action-buttons");
  if (pageActions) {
    for (let child of pageActions.childNodes) {
      if (child.getAttribute("open") == "true") {
        return true;
      }
    }
  }

  // toolbar button panels
  let navbar = window.document.getElementById("nav-bar-customization-target");
  for (let node of navbar.querySelectorAll("toolbarbutton")) {
    if (node.getAttribute("open") == "true") {
      return true;
    }
  }

  // Other modals like spotlight messages or default browser prompt
  // can be shown at startup
  if (window.gDialogBox.isOpen) {
    return true;
  }

  // On startup, the default browser check normally opens after the Search Tip.
  // As a result, we can't check for the prompt's presence, but we can check if
  // it plans on opening.
  const willPrompt = await lazy.DefaultBrowserCheck.willCheckDefaultBrowser(
    /* isStartupCheck */ false
  );
  if (willPrompt) {
    return true;
  }

  return false;
}

/**
 * Checks if the given URL is the homepage of the current default search engine.
 * Returns false if the default engine is not listed in SUPPORTED_ENGINES.
 *
 * @param {string} urlStr
 *   The URL to check, in string form.
 *
 * @returns {boolean}
 */
async function isDefaultEngineHomepage(urlStr) {
  let defaultEngine = await Services.search.getDefault();
  if (!defaultEngine) {
    return false;
  }

  let homepageMatches = lazy.SUPPORTED_ENGINES.get(defaultEngine.name);
  if (!homepageMatches) {
    return false;
  }

  // The URL object throws if the string isn't a valid URL.
  let url;
  try {
    url = new URL(urlStr);
  } catch (e) {
    return false;
  }

  if (url.searchParams.has(homepageMatches.prohibitedSearchParams)) {
    return false;
  }

  // Strip protocol and query params.
  urlStr = url.hostname.concat(url.pathname);

  return homepageMatches.domainPath.test(urlStr);
}

export var UrlbarProviderSearchTips = new ProviderSearchTips();
