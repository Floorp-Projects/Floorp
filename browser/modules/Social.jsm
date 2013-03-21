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

// Add a pref observer for the enabled state
function prefObserver(subject, topic, data) {
  let enable = Services.prefs.getBoolPref("social.enabled");
  if (enable && !Social.provider) {
    Social.provider = Social.defaultProvider;
  } else if (!enable && Social.provider) {
    Social.provider = null;
  }
}

Services.prefs.addObserver("social.enabled", prefObserver, false);
Services.obs.addObserver(function xpcomShutdown() {
  Services.obs.removeObserver(xpcomShutdown, "xpcom-shutdown");
  Services.prefs.removeObserver("social.enabled", prefObserver);
}, "xpcom-shutdown", false);

this.Social = {
  initialized: false,
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
    this._setProvider(val);
  },

  // Sets the current provider and enables it. Also disables the
  // previously set provider, and notifies observers of the change.
  _setProvider: function (provider) {
    if (this._provider == provider)
      return;

    // Disable the previous provider, if any, since we want only one provider to
    // be enabled at once.
    if (this._provider)
      this._provider.enabled = false;

    this._provider = provider;

    if (this._provider) {
      this._provider.enabled = true;
      this._currentProviderPref = this._provider.origin;
    }
    let enabled = !!provider;
    if (enabled != SocialService.enabled) {
      SocialService.enabled = enabled;
      Services.prefs.setBoolPref("social.enabled", enabled);
    }

    let origin = this._provider && this._provider.origin;
    Services.obs.notifyObservers(null, "social:provider-set", origin);
  },

  get defaultProvider() {
    if (this.providers.length == 0)
      return null;
    let provider = this._getProviderFromOrigin(this._currentProviderPref);
    return provider || this.providers[0];
  },

  init: function Social_init() {
    this._disabledForSafeMode = Services.appinfo.inSafeMode && this.enabled;

    if (this.initialized) {
      return;
    }
    this.initialized = true;

    // Retrieve the current set of providers, and set the current provider.
    SocialService.getProviderList(function (providers) {
      this._updateProviderCache(providers);
    }.bind(this));

    // Register an observer for changes to the provider list
    SocialService.registerProviderListener(function providerListener(topic, data) {
      // An engine change caused by adding/removing a provider should notify
      if (topic == "provider-added" || topic == "provider-removed") {
        this._updateProviderCache(data);
        Services.obs.notifyObservers(null, "social:providers-changed", null);
      }
    }.bind(this));
  },

  // Called to update our cache of providers and set the current provider
  _updateProviderCache: function (providers) {
    this.providers = providers;

    // If social is currently disabled there's nothing else to do other than
    // to notify about the lack of a provider.
    if (!SocialService.enabled) {
      Services.obs.notifyObservers(null, "social:provider-set", null);
      return;
    }
    // Otherwise set the provider.
    this._setProvider(this.defaultProvider);
  },

  set enabled(val) {
    // Setting .enabled is just a shortcut for setting the provider to either
    // the default provider or null...
    if (val) {
      if (!this.provider)
        this.provider = this.defaultProvider;
    } else {
      this.provider = null;
    }
  },
  get enabled() {
    return this.provider != null;
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

  installProvider: function(doc, data, installCallback) {
    SocialService.installProvider(doc, data, installCallback);
  },

  uninstallProvider: function(origin) {
    SocialService.uninstallProvider(origin);
  },

  // Activation functionality
  activateFromOrigin: function (origin, callback) {
    // For now only "builtin" providers can be activated.  It's OK if the
    // provider has already been activated - we still get called back with it.
    SocialService.addBuiltinProvider(origin, function(provider) {
      if (provider) {
        // No need to activate again if we're already active
        if (provider == this.provider)
          return;
        this.provider = provider;
      }
      if (callback)
        callback(provider);
    }.bind(this));
  },

  deactivateFromOrigin: function (origin, oldOrigin) {
    // if we have the old provider, always set that before trying removal
    let provider = this._getProviderFromOrigin(origin);
    let oldProvider = this._getProviderFromOrigin(oldOrigin);
    if (!oldProvider && this.providers.length)
      oldProvider = this.providers[0];
    this.provider = oldProvider;
    if (provider)
      SocialService.removeProvider(origin);
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
