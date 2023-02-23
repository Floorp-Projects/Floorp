/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";
import { BrowserUtils } from "resource://gre/modules/BrowserUtils.sys.mjs";
import { PrivateBrowsingUtils } from "resource://gre/modules/PrivateBrowsingUtils.sys.mjs";
import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  AboutNewTab: "resource:///modules/AboutNewTab.jsm",
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.jsm",
});

XPCOMUtils.defineLazyGetter(lazy, "ReferrerInfo", () =>
  Components.Constructor(
    "@mozilla.org/referrer-info;1",
    "nsIReferrerInfo",
    "init"
  )
);

export const URILoadingHelper = {
  /* eslint-disable complexity */
  openLinkIn(window, url, where, params) {
    if (!where || !url) {
      return;
    }

    var aFromChrome = params.fromChrome;
    var aAllowThirdPartyFixup = params.allowThirdPartyFixup;
    var aPostData = params.postData;
    var aCharset = params.charset;
    var aReferrerInfo = params.referrerInfo
      ? params.referrerInfo
      : new lazy.ReferrerInfo(Ci.nsIReferrerInfo.EMPTY, true, null);
    var aRelatedToCurrent = params.relatedToCurrent;
    var aAllowInheritPrincipal = !!params.allowInheritPrincipal;
    var aForceAllowDataURI = params.forceAllowDataURI;
    var aInBackground = params.inBackground;
    var aInitiatingDoc = params.initiatingDoc;
    var aIsPrivate = params.private;
    var aForceNonPrivate = params.forceNonPrivate;
    var aSkipTabAnimation = params.skipTabAnimation;
    var aAllowPinnedTabHostChange = !!params.allowPinnedTabHostChange;
    var aAllowPopups = !!params.allowPopups;
    var aUserContextId = params.userContextId;
    var aIndicateErrorPageLoad = params.indicateErrorPageLoad;
    var aPrincipal = params.originPrincipal;
    var aStoragePrincipal = params.originStoragePrincipal;
    var aTriggeringPrincipal = params.triggeringPrincipal;
    var aTriggeringRemoteType = params.triggeringRemoteType;
    var aCsp = params.csp;
    var aForceAboutBlankViewerInCurrent = params.forceAboutBlankViewerInCurrent;
    var aResolveOnNewTabCreated = params.resolveOnNewTabCreated;
    // This callback will be called with the content browser once it's created.
    var aResolveOnContentBrowserReady = params.resolveOnContentBrowserCreated;
    var aGlobalHistoryOptions = params.globalHistoryOptions;

    if (!aTriggeringPrincipal) {
      throw new Error("Must load with a triggering Principal");
    }

    if (where == "save") {
      if ("isContentWindowPrivate" in params) {
        window.saveURL(
          url,
          null,
          null,
          null,
          true,
          true,
          aReferrerInfo,
          null,
          null,
          params.isContentWindowPrivate,
          aPrincipal
        );
      } else {
        if (!aInitiatingDoc) {
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
          aReferrerInfo,
          null,
          aInitiatingDoc
        );
      }
      return;
    }

    // Establish which window we'll load the link in.
    let w;
    if (where == "current" && params.targetBrowser) {
      w = params.targetBrowser.ownerGlobal;
    } else {
      w = this.getTargetWindow(window, { forceNonPrivate: aForceNonPrivate });
    }
    // We don't want to open tabs in popups, so try to find a non-popup window in
    // that case.
    if ((where == "tab" || where == "tabshifted") && w && !w.toolbar.visible) {
      w = this.getTargetWindow(window, {
        skipPopups: true,
        forceNonPrivate: aForceNonPrivate,
      });
      aRelatedToCurrent = false;
    }

    // Teach the principal about the right OA to use, e.g. in case when
    // opening a link in a new private window, or in a new container tab.
    // Please note we do not have to do that for SystemPrincipals and we
    // can not do it for NullPrincipals since NullPrincipals are only
    // identical if they actually are the same object (See Bug: 1346759)
    function useOAForPrincipal(principal) {
      if (principal && principal.isContentPrincipal) {
        let attrs = {
          userContextId: aUserContextId,
          privateBrowsingId:
            aIsPrivate || (w && PrivateBrowsingUtils.isWindowPrivate(w)),
          firstPartyDomain: principal.originAttributes.firstPartyDomain,
        };
        return Services.scriptSecurityManager.principalWithOA(principal, attrs);
      }
      return principal;
    }
    aPrincipal = useOAForPrincipal(aPrincipal);
    aStoragePrincipal = useOAForPrincipal(aStoragePrincipal);
    aTriggeringPrincipal = useOAForPrincipal(aTriggeringPrincipal);

    if (!w || where == "window") {
      let features = "chrome,dialog=no,all";
      if (aIsPrivate) {
        features += ",private";
        // To prevent regular browsing data from leaking to private browsing sites,
        // strip the referrer when opening a new private window. (See Bug: 1409226)
        aReferrerInfo = new lazy.ReferrerInfo(
          aReferrerInfo.referrerPolicy,
          false,
          aReferrerInfo.originalReferrer
        );
      } else if (aForceNonPrivate) {
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
      if (aTriggeringRemoteType) {
        extraOptions.setPropertyAsACString(
          "triggeringRemoteType",
          aTriggeringRemoteType
        );
      }
      if (params.hasValidUserGestureActivation !== undefined) {
        extraOptions.setPropertyAsBool(
          "hasValidUserGestureActivation",
          params.hasValidUserGestureActivation
        );
      }
      if (aForceAllowDataURI) {
        extraOptions.setPropertyAsBool("forceAllowDataURI", true);
      }
      if (params.fromExternal !== undefined) {
        extraOptions.setPropertyAsBool("fromExternal", params.fromExternal);
      }
      if (aGlobalHistoryOptions?.triggeringSponsoredURL) {
        extraOptions.setPropertyAsACString(
          "triggeringSponsoredURL",
          aGlobalHistoryOptions.triggeringSponsoredURL
        );
        if (aGlobalHistoryOptions.triggeringSponsoredURLVisitTimeMS) {
          extraOptions.setPropertyAsUint64(
            "triggeringSponsoredURLVisitTimeMS",
            aGlobalHistoryOptions.triggeringSponsoredURLVisitTimeMS
          );
        }
      }

      var allowThirdPartyFixupSupports = Cc[
        "@mozilla.org/supports-PRBool;1"
      ].createInstance(Ci.nsISupportsPRBool);
      allowThirdPartyFixupSupports.data = aAllowThirdPartyFixup;

      var userContextIdSupports = Cc[
        "@mozilla.org/supports-PRUint32;1"
      ].createInstance(Ci.nsISupportsPRUint32);
      userContextIdSupports.data = aUserContextId;

      sa.appendElement(wuri);
      sa.appendElement(extraOptions);
      sa.appendElement(aReferrerInfo);
      sa.appendElement(aPostData);
      sa.appendElement(allowThirdPartyFixupSupports);
      sa.appendElement(userContextIdSupports);
      sa.appendElement(aPrincipal);
      sa.appendElement(aStoragePrincipal);
      sa.appendElement(aTriggeringPrincipal);
      sa.appendElement(null); // allowInheritPrincipal
      sa.appendElement(aCsp);

      const sourceWindow = w || window;
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

      if (aResolveOnContentBrowserReady) {
        waitForWindowStartup().then(() =>
          aResolveOnContentBrowserReady(win.gBrowser.selectedBrowser)
        );
      }

      win = Services.ww.openWindow(
        sourceWindow,
        AppConstants.BROWSER_CHROME_URL,
        null,
        features,
        sa
      );

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
        !aAllowPinnedTabHostChange &&
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
      loadInBackground = aInBackground;
      if (loadInBackground == null) {
        loadInBackground = aFromChrome
          ? false
          : Services.prefs.getBoolPref("browser.tabs.loadInBackground");
      }
    }

    let focusUrlBar = false;

    switch (where) {
      case "current":
        let flags = Ci.nsIWebNavigation.LOAD_FLAGS_NONE;

        if (aAllowThirdPartyFixup) {
          flags |= Ci.nsIWebNavigation.LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP;
          flags |= Ci.nsIWebNavigation.LOAD_FLAGS_FIXUP_SCHEME_TYPOS;
        }
        // LOAD_FLAGS_DISALLOW_INHERIT_PRINCIPAL isn't supported for javascript URIs,
        // i.e. it causes them not to load at all. Callers should strip
        // "javascript:" from pasted strings to prevent blank tabs
        if (!aAllowInheritPrincipal) {
          flags |= Ci.nsIWebNavigation.LOAD_FLAGS_DISALLOW_INHERIT_PRINCIPAL;
        }

        if (aAllowPopups) {
          flags |= Ci.nsIWebNavigation.LOAD_FLAGS_ALLOW_POPUPS;
        }
        if (aIndicateErrorPageLoad) {
          flags |= Ci.nsIWebNavigation.LOAD_FLAGS_ERROR_LOAD_CHANGES_RV;
        }
        if (aForceAllowDataURI) {
          flags |= Ci.nsIWebNavigation.LOAD_FLAGS_FORCE_ALLOW_DATA_URI;
        }
        if (params.fromExternal) {
          flags |= Ci.nsIWebNavigation.LOAD_FLAGS_FROM_EXTERNAL;
        }

        let { URI_INHERITS_SECURITY_CONTEXT } = Ci.nsIProtocolHandler;
        if (
          aForceAboutBlankViewerInCurrent &&
          (!uriObj ||
            Services.io.getDynamicProtocolFlags(uriObj) &
              URI_INHERITS_SECURITY_CONTEXT)
        ) {
          // Unless we know for sure we're not inheriting principals,
          // force the about:blank viewer to have the right principal:
          targetBrowser.createAboutBlankContentViewer(
            aPrincipal,
            aStoragePrincipal
          );
        }

        targetBrowser.fixupAndLoadURIString(url, {
          triggeringPrincipal: aTriggeringPrincipal,
          csp: aCsp,
          flags,
          referrerInfo: aReferrerInfo,
          postData: aPostData,
          userContextId: aUserContextId,
          hasValidUserGestureActivation: params.hasValidUserGestureActivation,
          globalHistoryOptions: aGlobalHistoryOptions,
          triggeringRemoteType: aTriggeringRemoteType,
        });
        if (aResolveOnContentBrowserReady) {
          aResolveOnContentBrowserReady(targetBrowser);
        }

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
          referrerInfo: aReferrerInfo,
          charset: aCharset,
          postData: aPostData,
          inBackground: loadInBackground,
          allowThirdPartyFixup: aAllowThirdPartyFixup,
          relatedToCurrent: aRelatedToCurrent,
          skipAnimation: aSkipTabAnimation,
          userContextId: aUserContextId,
          originPrincipal: aPrincipal,
          originStoragePrincipal: aStoragePrincipal,
          triggeringPrincipal: aTriggeringPrincipal,
          allowInheritPrincipal: aAllowInheritPrincipal,
          triggeringRemoteType: aTriggeringRemoteType,
          csp: aCsp,
          forceAllowDataURI: aForceAllowDataURI,
          focusUrlBar,
          openerBrowser: params.openerBrowser,
          fromExternal: params.fromExternal,
          globalHistoryOptions: aGlobalHistoryOptions,
        });
        targetBrowser = tabUsedForLoad.linkedBrowser;

        if (aResolveOnNewTabCreated) {
          aResolveOnNewTabCreated(targetBrowser);
        }
        if (aResolveOnContentBrowserReady) {
          aResolveOnContentBrowserReady(targetBrowser);
        }

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
   *                           accepted by openUILinkIn, plus "ignoreButton"
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

      // don't forward "ignoreButton" and "ignoreAlt" to openUILinkIn
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
    this.openUILinkIn(window, url, where, params);
  },

  /* openUILinkIn opens a URL in a place specified by the parameter |where|.
   *
   * |where| can be:
   *  "current"     current tab            (if there aren't any browser windows, then in a new window instead)
   *  "tab"         new tab                (if there aren't any browser windows, then in a new window instead)
   *  "tabshifted"  same as "tab" but in background if default is to select new tabs, and vice versa
   *  "window"      new window
   *  "save"        save to disk (with no filename hint!)
   *
   * DEPRECATION WARNING:
   * USE        -> openTrustedLinkIn(url, where, aParams) if the source is always
   *                     a user event on a user- or product-specified URL (as
   *                     opposed to URLs provided by a webpage)
   * USE        -> openWebLinkIn(url, where, aParams) if the URI should be loaded
   *                     with a specific triggeringPrincipal, for instance, if
   *                     the url was supplied by web content.
   * DEPRECATED -> openUILinkIn(url, where, AllowThirdPartyFixup, aPostData, ...)
   *
   *
   * allowThirdPartyFixup controls whether third party services such as Google's
   * I Feel Lucky are allowed to interpret this URL. This parameter may be
   * undefined, which is treated as false.
   *
   * Instead of aAllowThirdPartyFixup, you may also pass an object with any of
   * these properties:
   *   allowThirdPartyFixup (boolean)
   *   fromChrome           (boolean)
   *   postData             (nsIInputStream)
   *   referrerInfo         (nsIReferrerInfo)
   *   relatedToCurrent     (boolean)
   *   skipTabAnimation     (boolean)
   *   allowPinnedTabHostChange (boolean)
   *   allowPopups          (boolean)
   *   userContextId        (unsigned int)
   *   targetBrowser        (XUL browser)
   */
  openUILinkIn(
    window,
    url,
    where,
    aAllowThirdPartyFixup,
    aPostData,
    aReferrerInfo
  ) {
    var params;

    if (typeof aAllowThirdPartyFixup == "object") {
      params = aAllowThirdPartyFixup;
    }
    if (!params || !params.triggeringPrincipal) {
      throw new Error(
        "Required argument triggeringPrincipal missing within openUILinkIn"
      );
    }

    params.fromChrome = params.fromChrome ?? true;

    this.openLinkIn(window, url, where, params);
  },
};
