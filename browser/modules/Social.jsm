/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["Social"];

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "SocialService",
  "resource://gre/modules/SocialService.jsm");

this.Social = {
  lastEventReceived: 0,
  providers: null,
  _disabledForSafeMode: false,

  get _currentProviderPref() {
    try {
      return Services.prefs.getComplexValue("social.provider.current",
                                            Ci.nsISupportsString).data;
    } catch (ex) {}
    return null;
  },
  set _currentProviderPref(val) {
    let string = Cc["@mozilla.org/supports-string;1"].
                 createInstance(Ci.nsISupportsString);
    string.data = val;
    Services.prefs.setComplexValue("social.provider.current",
                                   Ci.nsISupportsString, string);
  },

  _provider: null,
  get provider() {
    return this._provider;
  },
  set provider(val) {
    // Changes triggered by the public setter should notify of an engine change.
    this._setProvider(val, true);
  },

  // Sets the current provider and enables and activates it. Also disables the
  // previously set provider, and optionally notifies observers of the change.
  _setProvider: function (provider, notify) {
    if (this._provider == provider)
      return;

    if (provider && !provider.active)
      throw new Error("Social.provider cannot be set to an inactive provider.");

    // Disable the previous provider, if any, since we want only one provider to
    // be enabled at once.
    if (this._provider)
      this._provider.enabled = false;

    this._provider = provider;

    if (this._provider) {
      if (this.enabled)
        this._provider.enabled = true;
      this._currentProviderPref = this._provider.origin;
    } else {
      Services.prefs.clearUserPref("social.provider.current");
    }

    if (notify) {
      let origin = this._provider && this._provider.origin;
      Services.obs.notifyObservers(null, "social:provider-set", origin);
    }
  },

  init: function Social_init(callback) {
    this._disabledForSafeMode = Services.appinfo.inSafeMode && this.enabled;

    if (this.providers) {
      schedule(callback);
      return;
    }

    if (!this._addedObservers) {
#ifndef MOZ_PER_WINDOW_PRIVATE_BROWSING
      Services.obs.addObserver(this, "private-browsing", false);
#endif
      Services.obs.addObserver(this, "social:pref-changed", false);
      this._addedObservers = true;
    }

    // Retrieve the current set of providers, and set the current provider.
    SocialService.getProviderList(function (providers) {
      // We don't want to notify about a provider change when we're setting
      // this.provider for the first time, so pass false here.
      this._updateProviderCache(providers, false);
      callback();
    }.bind(this));

    // Register an observer for changes to the provider list
    SocialService.registerProviderListener(function providerListener(topic, data) {
      // An engine change caused by adding/removing a provider should notify
      if (topic == "provider-added" || topic == "provider-removed")
        this._updateProviderCache(data, true);
    }.bind(this));
  },

  // Called to update our cache of providers and set the current provider
  _updateProviderCache: function (providers, notifyProviderChange) {
    this.providers = providers;

    // Set our current provider
    let currentProviderPref = this._currentProviderPref;
    let currentProvider;
    if (this._currentProviderPref) {
      currentProvider = this._getProviderFromOrigin(this._currentProviderPref);
    } else {
      // Migrate data from previous single-provider builds where we used
      // social.active to indicate that the first available provider should be
      // used.
      try {
        let active = Services.prefs.getBoolPref("social.active");
        if (active) {
          Services.prefs.clearUserPref("social.active");
          currentProvider = providers[0];
          currentProvider.active = true;
        }
      } catch(ex) {}
    }
    this._setProvider(currentProvider, notifyProviderChange);
  },

  observe: function(aSubject, aTopic, aData) {
#ifndef MOZ_PER_WINDOW_PRIVATE_BROWSING
    if (aTopic == "private-browsing") {
      if (aData == "enter") {
        this._enabledBeforePrivateBrowsing = this.enabled;
        this.enabled = false;
      } else if (aData == "exit") {
        // if the user has explicitly re-enabled social in PB mode, then upon
        // leaving we want to tear the world down then reenable to prevent
        // information leaks during this transition.
        // The next 2 lines rely on the fact that setting this.enabled to
        // its current value doesn't actually do anything...
        this.enabled = false;
        this.enabled = this._enabledBeforePrivateBrowsing;
      }
    } else
#endif
    if (aTopic == "social:pref-changed") {
      // Make sure our provider's enabled state matches the overall state of the
      // social components.
      if (this.provider)
        this.provider.enabled = this.enabled;
    }
  },

  set enabled(val) {
    SocialService.enabled = val;
  },
  get enabled() {
    return SocialService.enabled;
  },

  get active() {
    return this.provider && this.providers.some(function (p) p.active);
  },

  toggle: function Social_toggle() {
    this.enabled = this._disabledForSafeMode ? false : !this.enabled;
    this._disabledForSafeMode = false;
  },

  toggleSidebar: function SocialSidebar_toggle() {
    let prefValue = Services.prefs.getBoolPref("social.sidebar.open");
    Services.prefs.setBoolPref("social.sidebar.open", !prefValue);
  },

  toggleNotifications: function SocialNotifications_toggle() {
    let prefValue = Services.prefs.getBoolPref("social.toast-notifications.enabled");
    Services.prefs.setBoolPref("social.toast-notifications.enabled", !prefValue);
  },

  haveLoggedInUser: function () {
    return !!(this.provider && this.provider.profile && this.provider.profile.userName);
  },

  setProviderByOrigin: function (origin) {
    this.provider = this._getProviderFromOrigin(origin);
  },

  _getProviderFromOrigin: function (origin) {
    for (let p of this.providers) {
      if (p.origin == origin) {
        return p;
      }
    }
    return null;
  },

  // Activation functionality
  activateFromOrigin: function (origin) {
    let provider = this._getProviderFromOrigin(origin);
    if (provider) {
      // No need to activate again if we're already active
      if (provider == this.provider && provider.active)
        return null;

      provider.active = true;
      this.provider = provider;
      Social.enabled = true;
    }
    return provider;
  },

  deactivateFromOrigin: function (origin, oldOrigin) {
    let provider = this._getProviderFromOrigin(origin);
    if (provider && provider == this.provider) {
      this.provider.active = false;
      // Set the provider to the previously-selected provider (in case of undo),
      // or to the first available provider otherwise.
      this.provider = this._getProviderFromOrigin(oldOrigin);
      if (!this.provider)
        this.provider = this.providers.filter(function (p) p.active)[0];
      if (!this.provider) // Still no provider found, disable
        this.enabled = false;
    }
  },

  // Sharing functionality
  _getShareablePageUrl: function Social_getShareablePageUrl(aURI) {
    let uri = aURI.clone();
    try {
      // Setting userPass on about:config throws.
      uri.userPass = "";
    } catch (e) {}
    return uri.spec;
  },

  isPageShared: function Social_isPageShared(aURI) {
    let url = this._getShareablePageUrl(aURI);
    return this._sharedUrls.hasOwnProperty(url);
  },

  sharePage: function Social_sharePage(aURI) {
    // this should not be called if this.provider or the port is null
    if (!this.provider) {
      Cu.reportError("Can't share a page when no provider is current");
      return;
    }
    let port = this.provider.getWorkerPort();
    if (!port) {
      Cu.reportError("Can't share page as no provider port is available");
      return;
    }
    let url = this._getShareablePageUrl(aURI);
    this._sharedUrls[url] = true;
    port.postMessage({
      topic: "social.user-recommend",
      data: { url: url }
    });
    port.close();
  },

  unsharePage: function Social_unsharePage(aURI) {
    // this should not be called if this.provider or the port is null
    if (!this.provider) {
      Cu.reportError("Can't unshare a page when no provider is current");
      return;
    }
    let port = this.provider.getWorkerPort();
    if (!port) {
      Cu.reportError("Can't unshare page as no provider port is available");
      return;
    }
    let url = this._getShareablePageUrl(aURI);
    delete this._sharedUrls[url];
    port.postMessage({
      topic: "social.user-unrecommend",
      data: { url: url }
    });
    port.close();
  },

  _sharedUrls: {},

  setErrorListener: function(iframe, errorHandler) {
    if (iframe.socialErrorListener)
      return iframe.socialErrorListener;
    return new SocialErrorListener(iframe, errorHandler);
  }
};

function schedule(callback) {
  Services.tm.mainThread.dispatch(callback, Ci.nsIThread.DISPATCH_NORMAL);
}


// Error handling class used to listen for network errors in the social frames
// and replace them with a social-specific error page
function SocialErrorListener(iframe, errorHandler) {
  this.setErrorMessage = errorHandler;
  this.iframe = iframe;
  iframe.socialErrorListener = this;
  iframe.docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                                   .getInterface(Ci.nsIWebProgress)
                                   .addProgressListener(this,
                                                        Ci.nsIWebProgress.NOTIFY_STATE_REQUEST |
                                                        Ci.nsIWebProgress.NOTIFY_LOCATION);
}

SocialErrorListener.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebProgressListener,
                                         Ci.nsISupportsWeakReference,
                                         Ci.nsISupports]),

  remove: function() {
    this.iframe.docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                                     .getInterface(Ci.nsIWebProgress)
                                     .removeProgressListener(this);
    delete this.iframe.socialErrorListener;
  },

  onStateChange: function SPL_onStateChange(aWebProgress, aRequest, aState, aStatus) {
    let failure = false;
    if ((aState & Ci.nsIWebProgressListener.STATE_STOP)) {
      if (aRequest instanceof Ci.nsIHttpChannel) {
        try {
          // Change the frame to an error page on 4xx (client errors)
          // and 5xx (server errors)
          failure = aRequest.responseStatus >= 400 &&
                    aRequest.responseStatus < 600;
        } catch (e) {}
      }
    }

    // Calling cancel() will raise some OnStateChange notifications by itself,
    // so avoid doing that more than once
    if (failure && aStatus != Components.results.NS_BINDING_ABORTED) {
      aRequest.cancel(Components.results.NS_BINDING_ABORTED);
      Social.provider.errorState = "content-error";
      this.setErrorMessage(aWebProgress.QueryInterface(Ci.nsIDocShell)
                              .chromeEventHandler);
    }
  },

  onLocationChange: function SPL_onLocationChange(aWebProgress, aRequest, aLocation, aFlags) {
    let failure = aFlags & Ci.nsIWebProgressListener.LOCATION_CHANGE_ERROR_PAGE;
    if (failure && Social.provider.errorState != "frameworker-error") {
      aRequest.cancel(Components.results.NS_BINDING_ABORTED);
      Social.provider.errorState = "content-error";
      schedule(function() {
        this.setErrorMessage(aWebProgress.QueryInterface(Ci.nsIDocShell)
                              .chromeEventHandler);
      }.bind(this));
    }
  },

  onProgressChange: function SPL_onProgressChange() {},
  onStatusChange: function SPL_onStatusChange() {},
  onSecurityChange: function SPL_onSecurityChange() {},
};
