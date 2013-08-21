/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["Social", "OpenGraphBuilder"];

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "SocialService",
  "resource://gre/modules/SocialService.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
  "resource://gre/modules/PlacesUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Promise",
  "resource://gre/modules/commonjs/sdk/core/promise.js");

XPCOMUtils.defineLazyServiceGetter(this, "unescapeService",
                                   "@mozilla.org/feed-unescapehtml;1",
                                   "nsIScriptableUnescapeHTML");

// Add a pref observer for the enabled state
function prefObserver(subject, topic, data) {
  let enable = Services.prefs.getBoolPref("social.enabled");
  if (enable && !Social.provider) {
    // this will result in setting Social.provider
    SocialService.getOrderedProviderList(function(providers) {
      Social._updateProviderCache(providers);
      Social.enabled = true;
      Services.obs.notifyObservers(null, "social:providers-changed", null);
    });
  } else if (!enable && Social.provider) {
    Social.provider = null;
  }
}

Services.prefs.addObserver("social.enabled", prefObserver, false);
Services.obs.addObserver(function xpcomShutdown() {
  Services.obs.removeObserver(xpcomShutdown, "xpcom-shutdown");
  Services.prefs.removeObserver("social.enabled", prefObserver);
}, "xpcom-shutdown", false);

function promiseSetAnnotation(aURI, providerList) {
  let deferred = Promise.defer();

  // Delaying to catch issues with asynchronous behavior while waiting
  // to implement asynchronous annotations in bug 699844.
  Services.tm.mainThread.dispatch(function() {
    try {
      if (providerList && providerList.length > 0) {
        PlacesUtils.annotations.setPageAnnotation(
          aURI, "social/mark", JSON.stringify(providerList), 0,
          PlacesUtils.annotations.EXPIRE_WITH_HISTORY);
      } else {
        PlacesUtils.annotations.removePageAnnotation(aURI, "social/mark");
      }
    } catch(e) {
      Cu.reportError("SocialAnnotation failed: " + e);
    }
    deferred.resolve();
  }, Ci.nsIThread.DISPATCH_NORMAL);

  return deferred.promise;
}

function promiseGetAnnotation(aURI) {
  let deferred = Promise.defer();

  // Delaying to catch issues with asynchronous behavior while waiting
  // to implement asynchronous annotations in bug 699844.
  Services.tm.mainThread.dispatch(function() {
    let val = null;
    try {
      val = PlacesUtils.annotations.getPageAnnotation(aURI, "social/mark");
    } catch (ex) { }

    deferred.resolve(val);
  }, Ci.nsIThread.DISPATCH_NORMAL);

  return deferred.promise;
}

this.Social = {
  initialized: false,
  lastEventReceived: 0,
  providers: [],
  _disabledForSafeMode: false,

  get allowMultipleWorkers() {
    return Services.prefs.prefHasUserValue("social.allowMultipleWorkers") &&
           Services.prefs.getBoolPref("social.allowMultipleWorkers");
  },

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

  // Sets the current provider and notifies observers of the change.
  _setProvider: function (provider) {
    if (this._provider == provider)
      return;

    // Disable the previous provider, if we are not allowing multiple workers,
    // since we want only one provider to be enabled at once.
    if (this._provider && !Social.allowMultipleWorkers)
      this._provider.enabled = false;

    this._provider = provider;

    if (this._provider) {
      this._provider.enabled = true;
      this._currentProviderPref = this._provider.origin;
    }
    let enabled = !!provider;
    if (enabled != SocialService.enabled) {
      SocialService.enabled = enabled;
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

    if (SocialService.enabled) {
      // Retrieve the current set of providers, and set the current provider.
      SocialService.getOrderedProviderList(function (providers) {
        Social._updateProviderCache(providers);
        Social._updateWorkerState(true);
      });
    }

    // Register an observer for changes to the provider list
    SocialService.registerProviderListener(function providerListener(topic, data) {
      // An engine change caused by adding/removing a provider should notify.
      // any providers we receive are enabled in the AddonsManager
      if (topic == "provider-added" || topic == "provider-removed") {
        Social._updateProviderCache(data);
        Social._updateWorkerState(true);
        Services.obs.notifyObservers(null, "social:providers-changed", null);
        return;
      }
      if (topic == "provider-update") {
        // a provider has self-updated its manifest, we need to update our cache
        // and reload the provider.
        let provider = data;
        SocialService.getOrderedProviderList(function(providers) {
          Social._updateProviderCache(providers);
          provider.reload();
          Services.obs.notifyObservers(null, "social:providers-changed", null);
        });
      }
    });
  },

  _updateWorkerState: function(enable) {
    // ensure that our providers are all disabled, and enabled if we allow
    // multiple workers
    if (enable && !Social.allowMultipleWorkers)
      return;
    [p.enabled = enable for (p of Social.providers) if (p.enabled != enable)];
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

    this._updateWorkerState(val);

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

  // Page Marking functionality
  _getMarkablePageUrl: function Social_getMarkablePageUrl(aURI) {
    let uri = aURI.clone();
    try {
      // Setting userPass on about:config throws.
      uri.userPass = "";
    } catch (e) {}
    return uri.spec;
  },

  isURIMarked: function(aURI, aCallback) {
    promiseGetAnnotation(aURI).then(function(val) {
      if (val) {
        let providerList = JSON.parse(val);
        val = providerList.indexOf(this.provider.origin) >= 0;
      }
      aCallback(!!val);
    }.bind(this));
  },

  markURI: function(aURI, aCallback) {
    // this should not be called if this.provider or the port is null
    if (!this.provider) {
      Cu.reportError("Can't mark a page when no provider is current");
      return;
    }
    let port = this.provider.getWorkerPort();
    if (!port) {
      Cu.reportError("Can't mark page as no provider port is available");
      return;
    }

    // update or set our annotation
    promiseGetAnnotation(aURI).then(function(val) {

      let providerList = val ? JSON.parse(val) : [];
      let marked = providerList.indexOf(this.provider.origin) >= 0;
      if (marked)
        return;
      providerList.push(this.provider.origin);
      // we allow marking links in a page that may not have been visited yet.
      // make sure there is a history entry for the uri, then annotate it.
      let place = {
        uri: aURI,
        visits: [{
          visitDate: Date.now() + 1000,
          transitionType: Ci.nsINavHistoryService.TRANSITION_LINK
        }]
      };
      PlacesUtils.asyncHistory.updatePlaces(place, {
        handleError: function () Cu.reportError("couldn't update history for socialmark annotation"),
        handleResult: function () {},
        handleCompletion: function () {
          promiseSetAnnotation(aURI, providerList).then();
          // post to the provider
          let url = this._getMarkablePageUrl(aURI);
          port.postMessage({
            topic: "social.page-mark",
            data: { url: url, 'marked': true }
          });
          port.close();
          if (aCallback)
            schedule(function() { aCallback(true); } );
        }.bind(this)
      });
    }.bind(this));
  },
  
  unmarkURI: function(aURI, aCallback) {
    // this should not be called if this.provider or the port is null
    if (!this.provider) {
      Cu.reportError("Can't mark a page when no provider is current");
      return;
    }
    let port = this.provider.getWorkerPort();
    if (!port) {
      Cu.reportError("Can't mark page as no provider port is available");
      return;
    }

    // set our annotation
    promiseGetAnnotation(aURI).then(function(val) {
      let providerList = val ? JSON.parse(val) : [];
      let marked = providerList.indexOf(this.provider.origin) >= 0;
      if (marked) {
        // remove the annotation
        providerList.splice(providerList.indexOf(this.provider.origin), 1);
        promiseSetAnnotation(aURI, providerList).then();
      }
      // post to the provider regardless
      let url = this._getMarkablePageUrl(aURI);
      port.postMessage({
        topic: "social.page-mark",
        data: { url: url, 'marked': false }
      });
      port.close();
      if (aCallback)
        schedule(function() { aCallback(false); } );
    }.bind(this));
  },

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


this.OpenGraphBuilder = {
  getData: function(browser) {
    let res = {
      url: this._validateURL(browser, browser.currentURI.spec),
      title: browser.contentDocument.title,
      previews: []
    };
    this._getMetaData(browser, res);
    this._getLinkData(browser, res);
    this._getPageData(browser, res);
    return res;
  },

  _getMetaData: function(browser, o) {
    // query for standardized meta data
    let els = browser.contentDocument
                  .querySelectorAll("head > meta[property], head > meta[name]");
    if (els.length < 1)
      return;
    let url;
    for (let el of els) {
      let value = el.getAttribute("content")
      if (!value)
        continue;
      value = unescapeService.unescape(value.trim());
      switch (el.getAttribute("property") || el.getAttribute("name")) {
        case "title":
        case "og:title":
          o.title = value;
          break;
        case "description":
        case "og:description":
          o.description = value;
          break;
        case "og:site_name":
          o.siteName = value;
          break;
        case "medium":
        case "og:type":
          o.medium = value;
          break;
        case "og:video":
          url = this._validateURL(browser, value);
          if (url)
            o.source = url;
          break;
        case "og:url":
          url = this._validateURL(browser, value);
          if (url)
            o.url = url;
          break;
        case "og:image":
          url = this._validateURL(browser, value);
          if (url)
            o.previews.push(url);
          break;
      }
    }
  },

  _getLinkData: function(browser, o) {
    let els = browser.contentDocument
                  .querySelectorAll("head > link[rel], head > link[id]");
    for (let el of els) {
      let url = el.getAttribute("href");
      if (!url)
        continue;
      url = this._validateURL(browser, unescapeService.unescape(url.trim()));
      switch (el.getAttribute("rel") || el.getAttribute("id")) {
        case "shorturl":
        case "shortlink":
          o.shortUrl = url;
          break;
        case "canonicalurl":
        case "canonical":
          o.url = url;
          break;
        case "image_src":
          o.previews.push(url);
          break;
      }
    }
  },

  // scrape through the page for data we want
  _getPageData: function(browser, o) {
    if (o.previews.length < 1)
      o.previews = this._getImageUrls(browser);
  },

  _validateURL: function(browser, url) {
    let uri = Services.io.newURI(browser.currentURI.resolve(url), null, null);
    if (["http", "https", "ftp", "ftps"].indexOf(uri.scheme) < 0)
      return null;
    uri.userPass = "";
    return uri.spec;
  },

  _getImageUrls: function(browser) {
    let l = [];
    let els = browser.contentDocument.querySelectorAll("img");
    for (let el of els) {
      let content = el.getAttribute("src");
      if (content) {
        l.push(this._validateURL(browser, unescapeService.unescape(content)));
        // we don't want a billion images
        if (l.length > 5)
          break;
      }
    }
    return l;
  }
};
