/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";
import { BrowserUtils } from "resource://gre/modules/BrowserUtils.sys.mjs";
import { PrivateBrowsingUtils } from "resource://gre/modules/PrivateBrowsingUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  AboutNewTab: "resource:///modules/AboutNewTab.sys.mjs",
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "ReferrerInfo", () =>
  Components.Constructor(
    "@mozilla.org/referrer-info;1",
    "nsIReferrerInfo",
    "init"
  )
);

function saveLink(window, url, params) {
  if ("isContentWindowPrivate" in params) {
    window.saveURL(
      url,
      null,
      null,
      null,
      true,
      true,
      params.referrerInfo,
      null,
      null,
      params.isContentWindowPrivate,
      params.originPrincipal
    );
  } else {
    if (!params.initiatingDoc) {
      console.error(
        "openUILink/openLinkIn was called with " +
          "where == 'save' but without initiatingDoc.  See bug 814264."
      );
      return;
    }
    window.saveURL(
      url,
      null,
      null,
      null,
      true,
      true,
      params.referrerInfo,
      null,
      params.initiatingDoc
    );
  }
}

function openInWindow(url, params, sourceWindow) {
  let {
    referrerInfo,
    forceNonPrivate,
    triggeringRemoteType,
    forceAllowDataURI,
    globalHistoryOptions,
    allowThirdPartyFixup,
    userContextId,
    postData,
    originPrincipal,
    originStoragePrincipal,
    triggeringPrincipal,
    csp,
    resolveOnContentBrowserCreated,
  } = params;
  let features = "chrome,dialog=no,all";
  if (params.private) {
    features += ",private";
    // To prevent regular browsing data from leaking to private browsing sites,
    // strip the referrer when opening a new private window. (See Bug: 1409226)
    referrerInfo = new lazy.ReferrerInfo(
      referrerInfo.referrerPolicy,
      false,
      referrerInfo.originalReferrer
    );
  } else if (forceNonPrivate) {
    features += ",non-private";
  }

  // This propagates to window.arguments.
  var sa = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);

  var wuri = Cc["@mozilla.org/supports-string;1"].createInstance(
    Ci.nsISupportsString
  );
  wuri.data = url;

  let extraOptions = Cc["@mozilla.org/hash-property-bag;1"].createInstance(
    Ci.nsIWritablePropertyBag2
  );
  if (triggeringRemoteType) {
    extraOptions.setPropertyAsACString(
      "triggeringRemoteType",
      triggeringRemoteType
    );
  }
  if (params.hasValidUserGestureActivation !== undefined) {
    extraOptions.setPropertyAsBool(
      "hasValidUserGestureActivation",
      params.hasValidUserGestureActivation
    );
  }
  if (forceAllowDataURI) {
    extraOptions.setPropertyAsBool("forceAllowDataURI", true);
  }
  if (params.fromExternal !== undefined) {
    extraOptions.setPropertyAsBool("fromExternal", params.fromExternal);
  }
  if (globalHistoryOptions?.triggeringSponsoredURL) {
    extraOptions.setPropertyAsACString(
      "triggeringSponsoredURL",
      globalHistoryOptions.triggeringSponsoredURL
    );
    if (globalHistoryOptions.triggeringSponsoredURLVisitTimeMS) {
      extraOptions.setPropertyAsUint64(
        "triggeringSponsoredURLVisitTimeMS",
        globalHistoryOptions.triggeringSponsoredURLVisitTimeMS
      );
    }
  }
  if (params.wasSchemelessInput !== undefined) {
    extraOptions.setPropertyAsBool(
      "wasSchemelessInput",
      params.wasSchemelessInput
    );
  }

  var allowThirdPartyFixupSupports = Cc[
    "@mozilla.org/supports-PRBool;1"
  ].createInstance(Ci.nsISupportsPRBool);
  allowThirdPartyFixupSupports.data = allowThirdPartyFixup;

  var userContextIdSupports = Cc[
    "@mozilla.org/supports-PRUint32;1"
  ].createInstance(Ci.nsISupportsPRUint32);
  userContextIdSupports.data = userContextId;

  sa.appendElement(wuri);
  sa.appendElement(extraOptions);
  sa.appendElement(referrerInfo);
  sa.appendElement(postData);
  sa.appendElement(allowThirdPartyFixupSupports);
  sa.appendElement(userContextIdSupports);
  sa.appendElement(originPrincipal);
  sa.appendElement(originStoragePrincipal);
  sa.appendElement(triggeringPrincipal);
  sa.appendElement(null); // allowInheritPrincipal
  sa.appendElement(csp);

  let win;

  // Returns a promise that will be resolved when the new window's startup is finished.
  function waitForWindowStartup() {
    return new Promise(resolve => {
      const delayedStartupObserver = aSubject => {
        if (aSubject == win) {
          Services.obs.removeObserver(
            delayedStartupObserver,
            "browser-delayed-startup-finished"
          );
          resolve();
        }
      };
      Services.obs.addObserver(
        delayedStartupObserver,
        "browser-delayed-startup-finished"
      );
    });
  }

  if (params.frameID != undefined && sourceWindow) {
    // Only notify it as a WebExtensions' webNavigation.onCreatedNavigationTarget
    // event if it contains the expected frameID params.
    // (e.g. we should not notify it as a onCreatedNavigationTarget if the user is
    // opening a new window using the keyboard shortcut).
    const sourceTabBrowser = sourceWindow.gBrowser.selectedBrowser;
    waitForWindowStartup().then(() => {
      Services.obs.notifyObservers(
        {
          wrappedJSObject: {
            url,
            createdTabBrowser: win.gBrowser.selectedBrowser,
            sourceTabBrowser,
            sourceFrameID: params.frameID,
          },
        },
        "webNavigation-createdNavigationTarget"
      );
    });
  }

  if (resolveOnContentBrowserCreated) {
    waitForWindowStartup().then(() =>
      resolveOnContentBrowserCreated(win.gBrowser.selectedBrowser)
    );
  }

  win = Services.ww.openWindow(
    sourceWindow,
    AppConstants.BROWSER_CHROME_URL,
    null,
    features,
    sa
  );
}

function openInCurrentTab(targetBrowser, url, uriObj, params) {
  let flags = Ci.nsIWebNavigation.LOAD_FLAGS_NONE;

  if (params.allowThirdPartyFixup) {
    flags |= Ci.nsIWebNavigation.LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP;
    flags |= Ci.nsIWebNavigation.LOAD_FLAGS_FIXUP_SCHEME_TYPOS;
  }
  // LOAD_FLAGS_DISALLOW_INHERIT_PRINCIPAL isn't supported for javascript URIs,
  // i.e. it causes them not to load at all. Callers should strip
  // "javascript:" from pasted strings to prevent blank tabs
  if (!params.allowInheritPrincipal) {
    flags |= Ci.nsIWebNavigation.LOAD_FLAGS_DISALLOW_INHERIT_PRINCIPAL;
  }

  if (params.allowPopups) {
    flags |= Ci.nsIWebNavigation.LOAD_FLAGS_ALLOW_POPUPS;
  }
  if (params.indicateErrorPageLoad) {
    flags |= Ci.nsIWebNavigation.LOAD_FLAGS_ERROR_LOAD_CHANGES_RV;
  }
  if (params.forceAllowDataURI) {
    flags |= Ci.nsIWebNavigation.LOAD_FLAGS_FORCE_ALLOW_DATA_URI;
  }
  if (params.fromExternal) {
    flags |= Ci.nsIWebNavigation.LOAD_FLAGS_FROM_EXTERNAL;
  }

  let { URI_INHERITS_SECURITY_CONTEXT } = Ci.nsIProtocolHandler;
  if (
    params.forceAboutBlankViewerInCurrent &&
    (!uriObj ||
      Services.io.getDynamicProtocolFlags(uriObj) &
        URI_INHERITS_SECURITY_CONTEXT)
  ) {
    // Unless we know for sure we're not inheriting principals,
    // force the about:blank viewer to have the right principal:
    targetBrowser.createAboutBlankDocumentViewer(
      params.originPrincipal,
      params.originStoragePrincipal
    );
  }

  let {
    triggeringPrincipal,
    csp,
    referrerInfo,
    postData,
    userContextId,
    hasValidUserGestureActivation,
    globalHistoryOptions,
    triggeringRemoteType,
    wasSchemelessInput,
  } = params;

  targetBrowser.fixupAndLoadURIString(url, {
    triggeringPrincipal,
    csp,
    flags,
    referrerInfo,
    postData,
    userContextId,
    hasValidUserGestureActivation,
    globalHistoryOptions,
    triggeringRemoteType,
    wasSchemelessInput,
  });
  params.resolveOnContentBrowserCreated?.(targetBrowser);
}

function updatePrincipals(window, params) {
  let { userContextId } = params;
  // Teach the principal about the right OA to use, e.g. in case when
  // opening a link in a new private window, or in a new container tab.
  // Please note we do not have to do that for SystemPrincipals and we
  // can not do it for NullPrincipals since NullPrincipals are only
  // identical if they actually are the same object (See Bug: 1346759)
  function useOAForPrincipal(principal) {
    if (principal && principal.isContentPrincipal) {
      let privateBrowsingId =
        params.private ||
        (window && PrivateBrowsingUtils.isWindowPrivate(window));
      let attrs = {
        userContextId,
        privateBrowsingId,
        firstPartyDomain: principal.originAttributes.firstPartyDomain,
      };
      return Services.scriptSecurityManager.principalWithOA(principal, attrs);
    }
    return principal;
  }
  params.originPrincipal = useOAForPrincipal(params.originPrincipal);
  params.originStoragePrincipal = useOAForPrincipal(
    params.originStoragePrincipal
  );
  params.triggeringPrincipal = useOAForPrincipal(params.triggeringPrincipal);
}

export const URILoadingHelper = {
  /* openLinkIn opens a URL in a place specified by the parameter |where|.
   *
   * The params object is the same as for `openLinkIn` and documented below.
   *
   * @param {String}  where
   *   |where| can be:
   *    "current"     current tab            (if there aren't any browser windows, then in a new window instead)
   *    "tab"         new tab                (if there aren't any browser windows, then in a new window instead)
   *    "tabshifted"  same as "tab" but in background if default is to select new tabs, and vice versa
   *    "window"      new window
   *    "save"        save to disk (with no filename hint!)
   *
   * @param {Object}  params
   *
   * Options relating to what tab/window to use and how to open it:
   *
   * @param {boolean} params.private
   *                  Load the URL in a private window.
   * @param {boolean} params.forceNonPrivate
   *                  Force the load to happen in non-private windows.
   * @param {boolean} params.relatedToCurrent
   *                  Whether new tabs should go immediately next to the current tab.
   * @param {Element} params.targetBrowser
   *                  The browser to use for the load. Only used if where == "current".
   * @param {boolean} params.inBackground
   *                  If explicitly true or false, whether to switch to the tab immediately.
   *                  If null, will switch to the tab if `forceForeground` was true. If
   *                  neither is passed, will defer to the user preference browser.tabs.loadInBackground.
   * @param {boolean} params.forceForeground
   *                  Ignore the user preference and load in the foreground.
   * @param {boolean} params.allowPinnedTabHostChange
   *                  Allow even a pinned tab to change hosts.
   * @param {boolean} params.allowPopups
   *                  whether the link is allowed to open in a popup window (ie one with no browser
   *                  chrome)
   * @param {boolean} params.skipTabAnimation
   *                  Skip the tab opening animation.
   * @param {Element} params.openerBrowser
   *                  The browser that started the load.
   * @param {boolean} params.avoidBrowserFocus
   *                  Don't focus the browser element immediately after starting
   *                  the load. Used by the URL bar to avoid leaking user input
   *                  into web content, see bug 1641287.
   *
   * Options relating to the load itself:
   *
   * @param {boolean} params.allowThirdPartyFixup
   *                  Allow transforming the 'url' into a search query.
   * @param {nsIInputStream} params.postData
   *                  Data to post as part of the request.
   * @param {nsIReferrerInfo} params.referrerInfo
   *                  Referrer info for the request.
   * @param {boolean} params.indicateErrorPageLoad
   *                  Whether docshell should throw an exception (i.e. return non-NS_OK)
   *                  if the load fails.
   * @param {string}  params.charset
   *                  Character set to use for the load. Only honoured for tabs.
   *                  Legacy argument - do not use.
   * @param {string}  params.wasSchemelessInput
   *                  Whether the search/URL term was without an explicit scheme.
   *
   * Options relating to security, whether the load is allowed to happen,
   * and what cookie container to use for the load:
   *
   * @param {boolean} params.forceAllowDataURI
   *                  Force allow a data URI to load as a toplevel load.
   * @param {number}  params.userContextId
   *                  The userContextId (container identifier) to use for the load.
   * @param {boolean} params.allowInheritPrincipal
   *                  Allow the load to inherit the triggering principal.
   * @param {boolean} params.forceAboutBlankViewerInCurrent
   *                  Force load an about:blank page first. Only used if
   *                  allowInheritPrincipal is passed or no URL was provided.
   * @param {nsIPrincipal} params.triggeringPrincipal
   *                  Triggering principal to pass to docshell for the load.
   * @param {nsIPrincipal} params.originPrincipal
   *                  Origin principal to pass to docshell for the load.
   * @param {nsIPrincipal} params.originStoragePrincipal
   *                  Storage principal to pass to docshell for the load.
   * @param {string}  params.triggeringRemoteType
   *                  The remoteType triggering this load.
   * @param {nsIContentSecurityPolicy} params.csp
   *                  The CSP that should apply to the load.
   * @param {boolean} params.hasValidUserGestureActivation
   *                  Indicates if a valid user gesture caused this load. This
   *                  informs e.g. popup blocker decisions.
   * @param {boolean} params.fromExternal
   *                  Indicates the load was started outside of the browser,
   *                  e.g. passed on the commandline or through OS mechanisms.
   *
   * Options used to track the load elsewhere
   *
   * @param {function} params.resolveOnNewTabCreated
   *                   This callback will be called when a new tab is created.
   * @param {function} params.resolveOnContentBrowserCreated
   *                   This callback will be called with the content browser once it's created.
   * @param {Object}   params.globalHistoryOptions
   *                   Used by places to keep track of search related metadata for loads.
   * @param {Number}   params.frameID
   *                   Used by webextensions for their loads.
   *
   * Options used for where="save" only:
   *
   * @param {boolean}  params.isContentWindowPrivate
   *                   Save content as coming from a private window.
   * @param {Document} params.initiatingDoc
   *                   Used to determine where to prompt for a filename.
   */
  openLinkIn(window, url, where, params) {
    if (!where || !url) {
      return;
    }

    let {
      allowThirdPartyFixup,
      postData,
      charset,
      relatedToCurrent,
      allowInheritPrincipal,
      forceAllowDataURI,
      forceNonPrivate,
      skipTabAnimation,
      allowPinnedTabHostChange,
      userContextId,
      triggeringPrincipal,
      originPrincipal,
      originStoragePrincipal,
      triggeringRemoteType,
      csp,
      resolveOnNewTabCreated,
      resolveOnContentBrowserCreated,
      globalHistoryOptions,
    } = params;

    // We want to overwrite some things for convenience when passing it to other
    // methods. To avoid impacting callers, copy the params.
    params = Object.assign({}, params);

    if (!params.referrerInfo) {
      params.referrerInfo = new lazy.ReferrerInfo(
        Ci.nsIReferrerInfo.EMPTY,
        true,
        null
      );
    }

    if (!triggeringPrincipal) {
      throw new Error("Must load with a triggering Principal");
    }

    if (where == "save") {
      saveLink(window, url, params);
      return;
    }

    // Establish which window we'll load the link in.
    let w;
    if (where == "current" && params.targetBrowser) {
      w = params.targetBrowser.ownerGlobal;
    } else {
      w = this.getTargetWindow(window, { forceNonPrivate });
    }
    // We don't want to open tabs in popups, so try to find a non-popup window in
    // that case.
    if ((where == "tab" || where == "tabshifted") && w && !w.toolbar.visible) {
      w = this.getTargetWindow(window, {
        skipPopups: true,
        forceNonPrivate,
      });
      relatedToCurrent = false;
    }

    updatePrincipals(w, params);

    if (!w || where == "window") {
      openInWindow(url, params, w || window);
      return;
    }

    // We're now committed to loading the link in an existing browser window.

    // Raise the target window before loading the URI, since loading it may
    // result in a new frontmost window (e.g. "javascript:window.open('');").
    w.focus();

    let targetBrowser;
    let loadInBackground;
    let uriObj;

    if (where == "current") {
      targetBrowser = params.targetBrowser || w.gBrowser.selectedBrowser;
      loadInBackground = false;
      try {
        uriObj = Services.io.newURI(url);
      } catch (e) {}

      // In certain tabs, we restrict what if anything may replace the loaded
      // page. If a load request bounces off for the currently selected tab,
      // we'll open a new tab instead.
      let tab = w.gBrowser.getTabForBrowser(targetBrowser);
      if (tab == w.FirefoxViewHandler.tab) {
        where = "tab";
        targetBrowser = null;
      } else if (
        !allowPinnedTabHostChange &&
        tab.pinned &&
        url != "about:crashcontent"
      ) {
        try {
          // nsIURI.host can throw for non-nsStandardURL nsIURIs.
          if (
            !uriObj ||
            (!uriObj.schemeIs("javascript") &&
              targetBrowser.currentURI.host != uriObj.host)
          ) {
            where = "tab";
            targetBrowser = null;
          }
        } catch (err) {
          where = "tab";
          targetBrowser = null;
        }
      }
    } else {
      // `where` is "tab" or "tabshifted", so we'll load the link in a new tab.
      loadInBackground = params.inBackground;
      if (loadInBackground == null) {
        loadInBackground = params.forceForeground
          ? false
          : Services.prefs.getBoolPref("browser.tabs.loadInBackground");
      }
    }

    let focusUrlBar = false;

    switch (where) {
      case "current":
        openInCurrentTab(targetBrowser, url, uriObj, params);

        // Don't focus the content area if focus is in the address bar and we're
        // loading the New Tab page.
        focusUrlBar =
          w.document.activeElement == w.gURLBar.inputField &&
          w.isBlankPageURL(url);
        break;
      case "tabshifted":
        loadInBackground = !loadInBackground;
      // fall through
      case "tab":
        focusUrlBar =
          !loadInBackground &&
          w.isBlankPageURL(url) &&
          !lazy.AboutNewTab.willNotifyUser;

        let tabUsedForLoad = w.gBrowser.addTab(url, {
          referrerInfo: params.referrerInfo,
          charset,
          postData,
          inBackground: loadInBackground,
          allowThirdPartyFixup,
          relatedToCurrent,
          skipAnimation: skipTabAnimation,
          userContextId,
          originPrincipal,
          originStoragePrincipal,
          triggeringPrincipal,
          allowInheritPrincipal,
          triggeringRemoteType,
          csp,
          forceAllowDataURI,
          focusUrlBar,
          openerBrowser: params.openerBrowser,
          fromExternal: params.fromExternal,
          globalHistoryOptions,
          wasSchemelessInput: params.wasSchemelessInput,
        });
        targetBrowser = tabUsedForLoad.linkedBrowser;

        resolveOnNewTabCreated?.(targetBrowser);
        resolveOnContentBrowserCreated?.(targetBrowser);

        if (params.frameID != undefined && w) {
          // Only notify it as a WebExtensions' webNavigation.onCreatedNavigationTarget
          // event if it contains the expected frameID params.
          // (e.g. we should not notify it as a onCreatedNavigationTarget if the user is
          // opening a new tab using the keyboard shortcut).
          Services.obs.notifyObservers(
            {
              wrappedJSObject: {
                url,
                createdTabBrowser: targetBrowser,
                sourceTabBrowser: w.gBrowser.selectedBrowser,
                sourceFrameID: params.frameID,
              },
            },
            "webNavigation-createdNavigationTarget"
          );
        }
        break;
    }

    if (
      !params.avoidBrowserFocus &&
      !focusUrlBar &&
      targetBrowser == w.gBrowser.selectedBrowser
    ) {
      // Focus the content, but only if the browser used for the load is selected.
      targetBrowser.focus();
    }
  },

  /**
   * Finds a browser window suitable for opening a link matching the
   * requirements given in the `params` argument. If the current window matches
   * the requirements then it is returned otherwise the top-most window that
   * matches will be returned.
   *
   * @param {Window} window - The current window.
   * @param {Object} params - Parameters for selecting the window.
   * @param {boolean} params.skipPopups - Require a non-popup window.
   * @param {boolean} params.forceNonPrivate - Require a non-private window.
   * @returns {Window | null} A matching browser window or null if none matched.
   */
  getTargetWindow(window, { skipPopups, forceNonPrivate } = {}) {
    let { top } = window;
    // If this is called in a browser window, use that window regardless of
    // whether it's the frontmost window, since commands can be executed in
    // background windows (bug 626148).
    if (
      top.document.documentElement.getAttribute("windowtype") ==
        "navigator:browser" &&
      (!skipPopups || top.toolbar.visible) &&
      (!forceNonPrivate || !PrivateBrowsingUtils.isWindowPrivate(top))
    ) {
      return top;
    }

    return lazy.BrowserWindowTracker.getTopWindow({
      private: !forceNonPrivate && PrivateBrowsingUtils.isWindowPrivate(window),
      allowPopups: !skipPopups,
    });
  },

  /**
   * openUILink handles clicks on UI elements that cause URLs to load.
   *
   * @param {string} url
   * @param {Event | Object} event Event or JSON object representing an Event
   * @param {Boolean | Object} aIgnoreButton
   *                           Boolean or object with the same properties as
   *                           accepted by openLinkIn, plus "ignoreButton"
   *                           and "ignoreAlt".
   * @param {Boolean} aIgnoreAlt
   * @param {Boolean} aAllowThirdPartyFixup
   * @param {Object} aPostData
   * @param {Object} aReferrerInfo
   */
  openUILink(
    window,
    url,
    event,
    aIgnoreButton,
    aIgnoreAlt,
    aAllowThirdPartyFixup,
    aPostData,
    aReferrerInfo
  ) {
    event = BrowserUtils.getRootEvent(event);
    let params;

    if (aIgnoreButton && typeof aIgnoreButton == "object") {
      params = aIgnoreButton;

      // don't forward "ignoreButton" and "ignoreAlt" to openLinkIn
      aIgnoreButton = params.ignoreButton;
      aIgnoreAlt = params.ignoreAlt;
      delete params.ignoreButton;
      delete params.ignoreAlt;
    } else {
      params = {
        allowThirdPartyFixup: aAllowThirdPartyFixup,
        postData: aPostData,
        referrerInfo: aReferrerInfo,
        initiatingDoc: event ? event.target.ownerDocument : null,
      };
    }

    if (!params.triggeringPrincipal) {
      throw new Error(
        "Required argument triggeringPrincipal missing within openUILink"
      );
    }

    let where = BrowserUtils.whereToOpenLink(event, aIgnoreButton, aIgnoreAlt);
    params.forceForeground ??= true;
    this.openLinkIn(window, url, where, params);
  },

  /* openTrustedLinkIn will attempt to open the given URI using the SystemPrincipal
   * as the trigeringPrincipal, unless a more specific Principal is provided.
   *
   * Otherwise, parameters are the same as openLinkIn, but we will set `forceForeground`
   * to true.
   */
  openTrustedLinkIn(window, url, where, params = {}) {
    if (!params.triggeringPrincipal) {
      params.triggeringPrincipal =
        Services.scriptSecurityManager.getSystemPrincipal();
    }

    params.forceForeground ??= true;
    this.openLinkIn(window, url, where, params);
  },

  /* openWebLinkIn will attempt to open the given URI using the NullPrincipal
   * as the triggeringPrincipal, unless a more specific Principal is provided.
   *
   * Otherwise, parameters are the same as openLinkIn, but we will set `forceForeground`
   * to true.
   */
  openWebLinkIn(window, url, where, params = {}) {
    if (!params.triggeringPrincipal) {
      params.triggeringPrincipal =
        Services.scriptSecurityManager.createNullPrincipal({});
    }
    if (params.triggeringPrincipal.isSystemPrincipal) {
      throw new Error(
        "System principal should never be passed into openWebLinkIn()"
      );
    }
    params.forceForeground ??= true;
    this.openLinkIn(window, url, where, params);
  },

  /**
   * Given a URI, guess which container to use to open it. This is used for external
   * openers as a quality of life improvement (e.g. to open a document into the container
   * where you are logged in to the service that hosts it).
   * matches will be returned.
   * For now this can only use currently-open tabs, until history is tagged with the
   * container id (https://bugzilla.mozilla.org/show_bug.cgi?id=1283320).
   *
   * @param {nsIURI} aURI - The URI being opened.
   * @returns {number | null} The guessed userContextId, or null if none.
   */
  guessUserContextId(aURI) {
    let host;
    try {
      host = aURI.host;
    } catch (e) {}
    if (!host) {
      return null;
    }
    const containerScores = new Map();
    let guessedUserContextId = null;
    let maxCount = 0;
    for (let win of lazy.BrowserWindowTracker.orderedWindows) {
      for (let tab of win.gBrowser.visibleTabs) {
        let { userContextId } = tab;
        let currentURIHost = null;
        try {
          currentURIHost = tab.linkedBrowser.currentURI.host;
        } catch (e) {}

        if (currentURIHost == host) {
          let count = (containerScores.get(userContextId) ?? 0) + 1;
          containerScores.set(userContextId, count);
          if (count > maxCount) {
            guessedUserContextId = userContextId;
            maxCount = count;
          }
        }
      }
    }

    return guessedUserContextId;
  },
};
