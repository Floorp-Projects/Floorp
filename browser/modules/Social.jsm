/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["Social", "CreateSocialStatusWidget",
                         "CreateSocialMarkWidget", "OpenGraphBuilder",
                         "DynamicResizeWatcher", "sizeSocialPanelToContent"];

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;

// The minimum sizes for the auto-resize panel code, minimum size necessary to
// properly show the error page in the panel.
const PANEL_MIN_HEIGHT = 200;
const PANEL_MIN_WIDTH = 330;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "CustomizableUI",
  "resource:///modules/CustomizableUI.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "SocialService",
  "resource://gre/modules/SocialService.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
  "resource://gre/modules/PlacesUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Promise",
  "resource://gre/modules/Promise.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "unescapeService",
                                   "@mozilla.org/feed-unescapehtml;1",
                                   "nsIScriptableUnescapeHTML");

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

  init: function Social_init() {
    this._disabledForSafeMode = Services.appinfo.inSafeMode && this.enabled;
    let deferred = Promise.defer();

    if (this.initialized) {
      deferred.resolve(true);
      return deferred.promise;
    }
    this.initialized = true;
    // if SocialService.hasEnabledProviders, retreive the providers so the
    // front-end can generate UI
    if (SocialService.hasEnabledProviders) {
      // Retrieve the current set of providers, and set the current provider.
      SocialService.getOrderedProviderList(function (providers) {
        Social._updateProviderCache(providers);
        Social._updateWorkerState(SocialService.enabled);
        deferred.resolve(false);
      });
    } else {
      deferred.resolve(false);
    }

    // Register an observer for changes to the provider list
    SocialService.registerProviderListener(function providerListener(topic, origin, providers) {
      // An engine change caused by adding/removing a provider should notify.
      // any providers we receive are enabled in the AddonsManager
      if (topic == "provider-installed" || topic == "provider-uninstalled") {
        // installed/uninstalled do not send the providers param
        Services.obs.notifyObservers(null, "social:" + topic, origin);
        return;
      }
      if (topic == "provider-enabled") {
        Social._updateProviderCache(providers);
        Social._updateWorkerState(true);
        Services.obs.notifyObservers(null, "social:" + topic, origin);
        return;
      }
      if (topic == "provider-disabled") {
        // a provider was removed from the list of providers, that does not
        // affect worker state for other providers
        Social._updateProviderCache(providers);
        Social._updateWorkerState(providers.length > 0);
        Services.obs.notifyObservers(null, "social:" + topic, origin);
        return;
      }
      if (topic == "provider-update") {
        // a provider has self-updated its manifest, we need to update our cache
        // and reload the provider.
        Social._updateProviderCache(providers);
        let provider = Social._getProviderFromOrigin(origin);
        provider.reload();
      }
    });
    return deferred.promise;
  },

  _updateWorkerState: function(enable) {
    [p.enabled = enable for (p of Social.providers) if (p.enabled != enable)];
  },

  // Called to update our cache of providers and set the current provider
  _updateProviderCache: function (providers) {
    this.providers = providers;
    Services.obs.notifyObservers(null, "social:providers-changed", null);
  },

  get enabled() {
    return !this._disabledForSafeMode && this.providers.length > 0;
  },

  toggleNotifications: function SocialNotifications_toggle() {
    let prefValue = Services.prefs.getBoolPref("social.toast-notifications.enabled");
    Services.prefs.setBoolPref("social.toast-notifications.enabled", !prefValue);
  },

  _getProviderFromOrigin: function (origin) {
    for (let p of this.providers) {
      if (p.origin == origin) {
        return p;
      }
    }
    return null;
  },

  getManifestByOrigin: function(origin) {
    return SocialService.getManifestByOrigin(origin);
  },

  installProvider: function(doc, data, installCallback, aBypassUserEnable=false) {
    SocialService.installProvider(doc, data, installCallback, aBypassUserEnable);
  },

  uninstallProvider: function(origin, aCallback) {
    SocialService.uninstallProvider(origin, aCallback);
  },

  // Activation functionality
  activateFromOrigin: function (origin, callback) {
    // It's OK if the provider has already been activated - we still get called
    // back with it.
    SocialService.enableProvider(origin, callback);
  },

  // Page Marking functionality
  isURIMarked: function(origin, aURI, aCallback) {
    promiseGetAnnotation(aURI).then(function(val) {
      if (val) {
        let providerList = JSON.parse(val);
        val = providerList.indexOf(origin) >= 0;
      }
      aCallback(!!val);
    }).then(null, Cu.reportError);
  },

  markURI: function(origin, aURI, aCallback) {
    // update or set our annotation
    promiseGetAnnotation(aURI).then(function(val) {

      let providerList = val ? JSON.parse(val) : [];
      let marked = providerList.indexOf(origin) >= 0;
      if (marked)
        return;
      providerList.push(origin);
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
          promiseSetAnnotation(aURI, providerList).then(function() {
            if (aCallback)
              schedule(function() { aCallback(true); } );
          }).then(null, Cu.reportError);
        }
      });
    }).then(null, Cu.reportError);
  },

  unmarkURI: function(origin, aURI, aCallback) {
    // this should not be called if this.provider or the port is null
    // set our annotation
    promiseGetAnnotation(aURI).then(function(val) {
      let providerList = val ? JSON.parse(val) : [];
      let marked = providerList.indexOf(origin) >= 0;
      if (marked) {
        // remove the annotation
        providerList.splice(providerList.indexOf(origin), 1);
        promiseSetAnnotation(aURI, providerList).then(function() {
          if (aCallback)
            schedule(function() { aCallback(false); } );
        }).then(null, Cu.reportError);
      }
    }).then(null, Cu.reportError);
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

function CreateSocialStatusWidget(aId, aProvider) {
  if (!aProvider.statusURL)
    return;
  let widget = CustomizableUI.getWidget(aId);
  // The widget is only null if we've created then destroyed the widget.
  // Once we've actually called createWidget the provider will be set to
  // PROVIDER_API.
  if (widget && widget.provider == CustomizableUI.PROVIDER_API)
    return;

  CustomizableUI.createWidget({
    id: aId,
    type: 'custom',
    removable: true,
    defaultArea: CustomizableUI.AREA_NAVBAR,
    onBuild: function(aDocument) {
      let node = aDocument.createElement('toolbarbutton');
      node.id = this.id;
      node.setAttribute('class', 'toolbarbutton-1 chromeclass-toolbar-additional social-status-button badged-button');
      node.style.listStyleImage = "url(" + (aProvider.icon32URL || aProvider.iconURL) + ")";
      node.setAttribute("origin", aProvider.origin);
      node.setAttribute("label", aProvider.name);
      node.setAttribute("tooltiptext", aProvider.name);
      node.setAttribute("oncommand", "SocialStatus.showPopup(this);");

      if (PrivateBrowsingUtils.isWindowPrivate(aDocument.defaultView))
        node.setAttribute("disabled", "true");

      return node;
    }
  });
};

function CreateSocialMarkWidget(aId, aProvider) {
  if (!aProvider.markURL)
    return;
  let widget = CustomizableUI.getWidget(aId);
  // The widget is only null if we've created then destroyed the widget.
  // Once we've actually called createWidget the provider will be set to
  // PROVIDER_API.
  if (widget && widget.provider == CustomizableUI.PROVIDER_API)
    return;

  CustomizableUI.createWidget({
    id: aId,
    type: 'custom',
    removable: true,
    defaultArea: CustomizableUI.AREA_NAVBAR,
    onBuild: function(aDocument) {
      let node = aDocument.createElement('toolbarbutton');
      node.id = this.id;
      node.setAttribute('class', 'toolbarbutton-1 chromeclass-toolbar-additional social-mark-button');
      node.setAttribute('type', "socialmark");
      node.style.listStyleImage = "url(" + (aProvider.unmarkedIcon || aProvider.icon32URL || aProvider.iconURL) + ")";
      node.setAttribute("origin", aProvider.origin);

      let window = aDocument.defaultView;
      let menuLabel = window.gNavigatorBundle.getFormattedString("social.markpageMenu.label", [aProvider.name]);
      node.setAttribute("label", menuLabel);
      node.setAttribute("tooltiptext", menuLabel);
      node.setAttribute("observes", "Social:PageShareOrMark");

      return node;
    }
  });
};

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
          // and 5xx (server errors).  responseStatus throws if it is not set.
          failure = aRequest.responseStatus >= 400 &&
                    aRequest.responseStatus < 600;
        } catch (e) {
          failure = aStatus == Components.results.NS_ERROR_CONNECTION_REFUSED;
        }
      }
    }

    // Calling cancel() will raise some OnStateChange notifications by itself,
    // so avoid doing that more than once
    if (failure && aStatus != Components.results.NS_BINDING_ABORTED) {
      aRequest.cancel(Components.results.NS_BINDING_ABORTED);
      let origin = this.iframe.getAttribute("origin");
      if (origin) {
        let provider = Social._getProviderFromOrigin(origin);
        provider.errorState = "content-error";
      }
      this.setErrorMessage(aWebProgress.QueryInterface(Ci.nsIDocShell)
                              .chromeEventHandler);
    }
  },

  onLocationChange: function SPL_onLocationChange(aWebProgress, aRequest, aLocation, aFlags) {
    if (aFlags & Ci.nsIWebProgressListener.LOCATION_CHANGE_ERROR_PAGE) {
      aRequest.cancel(Components.results.NS_BINDING_ABORTED);
      let origin = this.iframe.getAttribute("origin");
      if (origin) {
        let provider = Social._getProviderFromOrigin(origin);
        if (!provider.errorState)
          provider.errorState = "content-error";
      }
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


function sizeSocialPanelToContent(panel, iframe) {
  let doc = iframe.contentDocument;
  if (!doc || !doc.body) {
    return;
  }
  // We need an element to use for sizing our panel.  See if the body defines
  // an id for that element, otherwise use the body itself.
  let body = doc.body;
  let bodyId = body.getAttribute("contentid");
  if (bodyId) {
    body = doc.getElementById(bodyId) || doc.body;
  }
  // offsetHeight/Width don't include margins, so account for that.
  let cs = doc.defaultView.getComputedStyle(body);
  let width = PANEL_MIN_WIDTH;
  let height = PANEL_MIN_HEIGHT;
  // if the panel is preloaded prior to being shown, cs will be null.  in that
  // case use the minimum size for the panel until it is shown.
  if (cs) {
    let computedHeight = parseInt(cs.marginTop) + body.offsetHeight + parseInt(cs.marginBottom);
    height = Math.max(computedHeight, height);
    let computedWidth = parseInt(cs.marginLeft) + body.offsetWidth + parseInt(cs.marginRight);
    width = Math.max(computedWidth, width);
  }
  // add extra space the panel needs if any
  width += panel.boxObject.width - iframe.boxObject.width;
  height += panel.boxObject.height - iframe.boxObject.height;

  // when size is computed, we want to be sure changes are "significant" since
  // some sites will resize when the iframe is resized by a small amount, making
  // the panel slowely shrink to some minimum.
  if (Math.abs(panel.boxObject.width - width) > 2 || Math.abs(panel.boxObject.height - height) > 2) {
    panel.sizeTo(width, height);
  }
}

function DynamicResizeWatcher() {
  this._mutationObserver = null;
}

DynamicResizeWatcher.prototype = {
  start: function DynamicResizeWatcher_start(panel, iframe) {
    this.stop(); // just in case...
    let doc = iframe.contentDocument;
    this._mutationObserver = new iframe.contentWindow.MutationObserver(function(mutations) {
      sizeSocialPanelToContent(panel, iframe);
    });
    // Observe anything that causes the size to change.
    let config = {attributes: true, characterData: true, childList: true, subtree: true};
    this._mutationObserver.observe(doc, config);
    // and since this may be setup after the load event has fired we do an
    // initial resize now.
    sizeSocialPanelToContent(panel, iframe);
  },
  stop: function DynamicResizeWatcher_stop() {
    if (this._mutationObserver) {
      try {
        this._mutationObserver.disconnect();
      } catch (ex) {
        // may get "TypeError: can't access dead object" which seems strange,
        // but doesn't seem to indicate a real problem, so ignore it...
      }
      this._mutationObserver = null;
    }
  }
}


this.OpenGraphBuilder = {
  generateEndpointURL: function(URLTemplate, pageData) {
    // support for existing oexchange style endpoints by supporting their
    // querystring arguments. parse the query string template and do
    // replacements where necessary the query names may be different than ours,
    // so we could see u=%{url} or url=%{url}
    let [endpointURL, queryString] = URLTemplate.split("?");
    let query = {};
    if (queryString) {
      queryString.split('&').forEach(function (val) {
        let [name, value] = val.split('=');
        let p = /%\{(.+)\}/.exec(value);
        if (!p) {
          // preserve non-template query vars
          query[name] = value;
        } else if (pageData[p[1]]) {
          if (p[1] == "previews")
            query[name] = pageData[p[1]][0];
          else
            query[name] = pageData[p[1]];
        } else if (p[1] == "body") {
          // build a body for emailers
          let body = "";
          if (pageData.title)
            body += pageData.title + "\n\n";
          if (pageData.description)
            body += pageData.description + "\n\n";
          if (pageData.text)
            body += pageData.text + "\n\n";
          body += pageData.url;
          query["body"] = body;
        }
      });
    }
    var str = [];
    for (let p in query)
       str.push(p + "=" + encodeURIComponent(query[p]));
    if (str.length)
      endpointURL = endpointURL + "?" + str.join("&");
    return endpointURL;
  },

  getData: function(browser, target) {
    let res = {
      url: this._validateURL(browser, browser.currentURI.spec),
      title: browser.contentDocument.title,
      previews: []
    };
    this._getMetaData(browser, res);
    this._getLinkData(browser, res);
    this._getPageData(browser, res);
    res.microdata = this.getMicrodata(browser, target);
    return res;
  },

  getMicrodata: function (browser, target) {
    return getMicrodata(browser.contentDocument, target);
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
      let key = el.getAttribute("property") || el.getAttribute("name");
      if (!key)
        continue;
      // There are a wide array of possible meta tags, expressing articles,
      // products, etc. so all meta tags are passed through but we touch up the
      // most common attributes.
      o[key] = value;
      switch (key) {
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
        case "alternate":
          // expressly for oembed support but we're liberal here and will let
          // other alternate links through. oembed defines an href, supplied by
          // the site, where you can fetch additional meta data about a page.
          // We'll let the client fetch the oembed data themselves, but they
          // need the data from this link.
          if (!o.alternate)
            o.alternate = [];
          o.alternate.push({
            "type": el.getAttribute("type"),
            "href": el.getAttribute("href"),
            "title": el.getAttribute("title")
          })
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

// getMicrodata (and getObject) based on wg algorythm to convert microdata to json
// http://www.whatwg.org/specs/web-apps/current-work/multipage/microdata-2.html#json
function  getMicrodata(document, target) {

  function _getObject(item) {
    let result = {};
    if (item.itemType.length)
      result.types = [i for (i of item.itemType)];
    if (item.itemId)
      result.itemId = item.itemid;
    if (item.properties.length)
      result.properties = {};
    for (let elem of item.properties) {
      let value;
      if (elem.itemScope)
        value = _getObject(elem);
      else if (elem.itemValue)
        value = elem.itemValue;
      // handle mis-formatted microdata
      else if (elem.hasAttribute("content"))
        value = elem.getAttribute("content");

      for (let prop of elem.itemProp) {
        if (!result.properties[prop])
          result.properties[prop] = [];
        result.properties[prop].push(value);
      }
    }
    return result;
  }

  let result = { items: [] };
  let elms = target ? [target] : document.getItems();
  for (let el of elms) {
    if (el.itemScope)
      result.items.push(_getObject(el));
  }
  return result;
}
