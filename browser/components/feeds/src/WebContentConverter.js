# -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is the Web Content Converter System.
#
# The Initial Developer of the Original Code is Google Inc.
# Portions created by the Initial Developer are Copyright (C) 2006
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Ben Goodger <beng@google.com>
#   Asaf Romano <mano@mozilla.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK ***** */

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

function LOG(str) {
  dump("*** " + str + "\n");
}

const WCCR_CONTRACTID = "@mozilla.org/embeddor.implemented/web-content-handler-registrar;1";
const WCCR_CLASSID = Components.ID("{792a7e82-06a0-437c-af63-b2d12e808acc}");
const WCCR_CLASSNAME = "Web Content Handler Registrar";

const WCC_CLASSID = Components.ID("{db7ebf28-cc40-415f-8a51-1b111851df1e}");
const WCC_CLASSNAME = "Web Service Handler";

const TYPE_MAYBE_FEED = "application/vnd.mozilla.maybe.feed";
const TYPE_ANY = "*/*";

const PREF_CONTENTHANDLERS_AUTO = "browser.contentHandlers.auto.";
const PREF_CONTENTHANDLERS_BRANCH = "browser.contentHandlers.types.";
const PREF_SELECTED_WEB = "browser.feeds.handlers.webservice";
const PREF_SELECTED_ACTION = "browser.feeds.handler";
const PREF_SELECTED_READER = "browser.feeds.handler.default";

const STRING_BUNDLE_URI = "chrome://browser/locale/feeds/subscribe.properties";

const NS_ERROR_MODULE_DOM = 2152923136;
const NS_ERROR_DOM_SYNTAX_ERR = NS_ERROR_MODULE_DOM + 12;

function WebContentConverter() {
}
WebContentConverter.prototype = {
  convert: function WCC_convert() { },
  asyncConvertData: function WCC_asyncConvertData() { },
  onDataAvailable: function WCC_onDataAvailable() { },
  onStopRequest: function WCC_onStopRequest() { },
  
  onStartRequest: function WCC_onStartRequest(request, context) {
    var wccr = 
        Cc[WCCR_CONTRACTID].
        getService(Ci.nsIWebContentConverterService);
    wccr.loadPreferredHandler(request);
  },
  
  QueryInterface: function WCC_QueryInterface(iid) {
    if (iid.equals(Ci.nsIStreamConverter) ||
        iid.equals(Ci.nsIStreamListener) ||
        iid.equals(Ci.nsISupports))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  }
};

var WebContentConverterFactory = {
  createInstance: function WCCF_createInstance(outer, iid) {
    if (outer != null)
      throw Cr.NS_ERROR_NO_AGGREGATION;
    return new WebContentConverter().QueryInterface(iid);
  },
    
  QueryInterface: function WCC_QueryInterface(iid) {
    if (iid.equals(Ci.nsIFactory) ||
        iid.equals(Ci.nsISupports))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  }
};

function ServiceInfo(contentType, uri, name) {
  this._contentType = contentType;
  this._uri = uri;
  this._name = name;
}
ServiceInfo.prototype = {
  /**
   * See nsIHandlerApp
   */
  get name() {
    return this._name;
  },
  
  /**
   * See nsIHandlerApp
   */
  equals: function SI_equals(aHandlerApp) {
    if (!aHandlerApp)
      throw Cr.NS_ERROR_NULL_POINTER;

    if (aHandlerApp instanceof Ci.nsIWebContentHandlerInfo &&
        aHandlerApp.contentType == this.contentType &&
        aHandlerApp.uri == this.uri)
      return true;

    return false;
  },

  /**
   * See nsIWebContentHandlerInfo
   */
  get contentType() {
    return this._contentType;
  },

  /**
   * See nsIWebContentHandlerInfo
   */
  get uri() {
    return this._uri;
  },

  /**
   * See nsIWebContentHandlerInfo
   */
  getHandlerURI: function SI_getHandlerURI(uri) {
    return this._uri.replace(/%s/gi, encodeURIComponent(uri));
  },
  
  QueryInterface: function SI_QueryInterface(iid) {
    if (iid.equals(Ci.nsIWebContentHandlerInfo) ||
        iid.equals(Ci.nsISupports))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  }
};

function WebContentConverterRegistrar() {}

WebContentConverterRegistrar.prototype = {
  _stringBundle: null,

  get stringBundle() {
    if (!this._stringBundle) {
      this._stringBundle = Cc["@mozilla.org/intl/stringbundle;1"].
                            getService(Ci.nsIStringBundleService).
                            createBundle(STRING_BUNDLE_URI);
    }

    return this._stringBundle;
  },

  _getFormattedString: function WCCR__getFormattedString(key, params) {
    return this.stringBundle.formatStringFromName(key, params, params.length);
  },
  
  _getString: function WCCR_getString(key) {
    return this.stringBundle.GetStringFromName(key);
  },

  _contentTypes: { },

  /**
   * Track auto handlers for various content types using a content-type to 
   * handler map.
   */
  _autoHandleContentTypes: { },

  /**
   * See nsIWebContentConverterService
   */
  getAutoHandler: 
  function WCCR_getAutoHandler(contentType) {
    contentType = this._resolveContentType(contentType);
    if (contentType in this._autoHandleContentTypes)
      return this._autoHandleContentTypes[contentType];
    return null;
  },
  
  /**
   * See nsIWebContentConverterService
   */
  setAutoHandler:
  function WCCR_setAutoHandler(contentType, handler) {
    if (handler && !this._typeIsRegistered(contentType, handler.uri))
      throw Cr.NS_ERROR_NOT_AVAILABLE;
      
    contentType = this._resolveContentType(contentType);
    this._setAutoHandler(contentType, handler);
    
    var ps = 
        Cc["@mozilla.org/preferences-service;1"].
        getService(Ci.nsIPrefService);
    var autoBranch = ps.getBranch(PREF_CONTENTHANDLERS_AUTO);
    if (handler)
      autoBranch.setCharPref(contentType, handler.uri);
    else if (autoBranch.prefHasUserValue(contentType))
      autoBranch.clearUserPref(contentType);
     
    ps.savePrefFile(null);
  },
  
  /**
   * Update the internal data structure (not persistent)
   */
  _setAutoHandler:
  function WCCR__setAutoHandler(contentType, handler) {
    if (handler) 
      this._autoHandleContentTypes[contentType] = handler;
    else if (contentType in this._autoHandleContentTypes)
      delete this._autoHandleContentTypes[contentType];
  },
  
  /**
   * See nsIWebContentConverterService
   */
  getWebContentHandlerByURI:
  function WCCR_getWebContentHandlerByURI(contentType, uri) {
    var handlers = this.getContentHandlers(contentType, { });
    for (var i = 0; i < handlers.length; ++i) {
      if (handlers[i].uri == uri) 
        return handlers[i];
    }
    return null;
  },
  
  /**
   * See nsIWebContentConverterService
   */
  loadPreferredHandler: 
  function WCCR_loadPreferredHandler(request) {
    var channel = request.QueryInterface(Ci.nsIChannel);
    var contentType = this._resolveContentType(channel.contentType);
    var handler = this.getAutoHandler(contentType);
    if (handler) {
      request.cancel(Cr.NS_ERROR_FAILURE);
      
      var webNavigation = 
          channel.notificationCallbacks.getInterface(Ci.nsIWebNavigation);
      webNavigation.loadURI(handler.getHandlerURI(channel.URI.spec), 
                            Ci.nsIWebNavigation.LOAD_FLAGS_NONE, 
                            null, null, null);
    }      
  },
  
  /**
   * See nsIWebContentConverterService
   */
  removeProtocolHandler: 
  function WCCR_removeProtocolHandler(aProtocol, aURITemplate) {
    var eps = Cc["@mozilla.org/uriloader/external-protocol-service;1"].
              getService(Ci.nsIExternalProtocolService);
    var handlerInfo = eps.getProtocolHandlerInfo(aProtocol);
    var handlers =  handlerInfo.possibleApplicationHandlers;
    for (let i = 0; i < handlers.length; i++) {
      try { // We only want to test web handlers
        let handler = handlers.queryElementAt(i, Ci.nsIWebHandlerApp);
        if (handler.uriTemplate == aURITemplate) {
          handlers.removeElementAt(i);
          var hs = Cc["@mozilla.org/uriloader/handler-service;1"].
                   getService(Ci.nsIHandlerService);
          hs.store(handlerInfo);
          return;
        }
      } catch (e) { /* it wasn't a web handler */ }
    }
  },
  
  /**
   * See nsIWebContentConverterService
   */
  removeContentHandler: 
  function WCCR_removeContentHandler(contentType, uri) {
    function notURI(serviceInfo) {
      return serviceInfo.uri != uri;
    }
  
    if (contentType in this._contentTypes) {
      this._contentTypes[contentType] = 
        this._contentTypes[contentType].filter(notURI);
    }
  },
  
  /**
   *
   */
  _mappings: { 
    "application/rss+xml": TYPE_MAYBE_FEED,
    "application/atom+xml": TYPE_MAYBE_FEED,
  },
  
  /**
   * These are types for which there is a separate content converter aside 
   * from our built in generic one. We should not automatically register
   * a factory for creating a converter for these types.
   */
  _blockedTypes: {
    "application/vnd.mozilla.maybe.feed": true,
  },
  
  /**
   * Determines the "internal" content type based on the _mappings.
   * @param   contentType
   * @returns The resolved contentType value. 
   */
  _resolveContentType: 
  function WCCR__resolveContentType(contentType) {
    if (contentType in this._mappings)
      return this._mappings[contentType];
    return contentType;
  },

  _makeURI: function(aURL, aOriginCharset, aBaseURI) {
    var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                              .getService(Components.interfaces.nsIIOService);
    return ioService.newURI(aURL, aOriginCharset, aBaseURI);
  },

  _checkAndGetURI:
  function WCCR_checkAndGetURI(aURIString)
  {
    try {
      var uri = this._makeURI(aURIString);
    } catch (ex) {
      // not supposed to throw according to spec
      return; 
    }
 
    // If the uri doesn't contain '%s', it won't be a good handler
    if (uri.spec.indexOf("%s") < 0)
      throw NS_ERROR_DOM_SYNTAX_ERR; 

    return uri;
  },

  /**
   * Determines if a web handler is already registered.
   *
   * @param aProtocol
   *        The scheme of the web handler we are checking for.
   * @param aURITemplate
   *        The URI template that the handler uses to handle the protocol.
   * @return true if it is already registered, false otherwise.
   */
  _protocolHandlerRegistered:
  function WCCR_protocolHandlerRegistered(aProtocol, aURITemplate) {
    var eps = Cc["@mozilla.org/uriloader/external-protocol-service;1"].
              getService(Ci.nsIExternalProtocolService);
    var handlerInfo = eps.getProtocolHandlerInfo(aProtocol);
    var handlers =  handlerInfo.possibleApplicationHandlers;
    for (let i = 0; i < handlers.length; i++) {
      try { // We only want to test web handlers
        let handler = handlers.queryElementAt(i, Ci.nsIWebHandlerApp);
        if (handler.uriTemplate == aURITemplate)
          return true;
      } catch (e) { /* it wasn't a web handler */ }
    }
    return false;
  },

  /**
   * See nsIWebContentHandlerRegistrar
   */
  registerProtocolHandler: 
  function WCCR_registerProtocolHandler(aProtocol, aURIString, aTitle, aContentWindow) {
    LOG("registerProtocolHandler(" + aProtocol + "," + aURIString + "," + aTitle + ")");
    
    // First, check to make sure this isn't already handled internally (we don't
    // want to let them take over, say "chrome").
    var ios = Cc["@mozilla.org/network/io-service;1"].
              getService(Ci.nsIIOService);
    var handler = ios.getProtocolHandler(aProtocol);
    if (!(handler instanceof Ci.nsIExternalProtocolHandler)) {
      // This is handled internally, so we don't want them to register
      // XXX this should be a "security exception" according to spec, but that
      // isn't defined yet.
      throw("Permission denied to add " + aURIString + "as a protocol handler");
    }
 
    var uri = this._checkAndGetURI(aURIString);

    var buttons, message;
    if (this._protocolHandlerRegistered(aProtocol, uri.spec))
      message = this._getFormattedString("protocolHandlerRegistered",
                                         [aTitle, aProtocol]);
    else {
      // Now Ask the user and provide the proper callback
      message = this._getFormattedString("addProtocolHandler",
                                         [aTitle, uri.host, aProtocol]);
      var fis = Cc["@mozilla.org/browser/favicon-service;1"].
                getService(Ci.nsIFaviconService);
      var notificationIcon = fis.getFaviconLinkForIcon(uri);
      var notificationValue = "Protocol Registration: " + aProtocol;
      var addButton = {
        label: this._getString("addProtocolHandlerAddButton"),
        accessKey: this._getString("addHandlerAddButtonAccesskey"),
        protocolInfo: { protocol: aProtocol, uri: uri.spec, name: aTitle },

        callback:
        function WCCR_addProtocolHandlerButtonCallback(aNotification, aButtonInfo) {
          var protocol = aButtonInfo.protocolInfo.protocol;
          var uri      = aButtonInfo.protocolInfo.uri;
          var name     = aButtonInfo.protocolInfo.name;

          var handler = Cc["@mozilla.org/uriloader/web-handler-app;1"].
                        createInstance(Ci.nsIWebHandlerApp);
          handler.name = name;
          handler.uriTemplate = uri;

          var eps = Cc["@mozilla.org/uriloader/external-protocol-service;1"].
                    getService(Ci.nsIExternalProtocolService);
          var handlerInfo = eps.getProtocolHandlerInfo(protocol);
          handlerInfo.possibleApplicationHandlers.appendElement(handler, false);

          var hs = Cc["@mozilla.org/uriloader/handler-service;1"].
                   getService(Ci.nsIHandlerService);
          hs.store(handlerInfo);
        }
      };
      buttons = [addButton];
    }

    var browserWindow = this._getBrowserWindowForContentWindow(aContentWindow);
    var browserElement = this._getBrowserForContentWindow(browserWindow, aContentWindow);
    var notificationBox = browserWindow.getBrowser().getNotificationBox(browserElement);
    notificationBox.appendNotification(message,
                                       notificationValue,
                                       notificationIcon,
                                       notificationBox.PRIORITY_INFO_LOW,
                                       buttons);
  },

  /**
   * See nsIWebContentHandlerRegistrar
   * This is the web front end into the registration system, so a prompt to 
   * confirm the registration is provided, and the result is saved to 
   * preferences.
   */
  registerContentHandler: 
  function WCCR_registerContentHandler(aContentType, aURIString, aTitle, aContentWindow) {
    LOG("registerContentHandler(" + aContentType + "," + aURIString + "," + aTitle + ")");

    // We only support feed types at present.
    // XXX this should be a "security exception" according to spec, but that
    // isn't defined yet.
    var contentType = this._resolveContentType(aContentType);
    if (contentType != TYPE_MAYBE_FEED)
      return;

    var uri = this._checkAndGetURI(aURIString);
            
    // For security reasons we reject non-http(s) urls (see bug Bug 354316),
    // we may need to revise this once we support more content types
    // XXX this should be a "security exception" according to spec, but that
    // isn't defined yet.
    if (uri.scheme != "http" &&  uri.scheme != "https")
      throw("Permission denied to add " + uri.spec + "as a content handler");

    var browserWindow = this._getBrowserWindowForContentWindow(aContentWindow);
    var browserElement = this._getBrowserForContentWindow(browserWindow, aContentWindow);
    var notificationBox = browserWindow.getBrowser().getNotificationBox(browserElement);
    this._appendFeedReaderNotification(uri, aTitle, notificationBox);
  },

  /**
   * Returns the browser chrome window in which the content window is in
   */
  _getBrowserWindowForContentWindow:
  function WCCR__getBrowserWindowForContentWindow(aContentWindow) {
    return aContentWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                         .getInterface(Ci.nsIWebNavigation)
                         .QueryInterface(Ci.nsIDocShellTreeItem)
                         .rootTreeItem
                         .QueryInterface(Ci.nsIInterfaceRequestor)
                         .getInterface(Ci.nsIDOMWindow)
                         .wrappedJSObject;
  },

  /**
   * Returns the <xul:browser> element associated with the given content
   * window.
   *
   * @param aBrowserWindow
   *        The browser window in which the content window is in.
   * @param aContentWindow
   *        The content window. It's possible to pass a child content window
   *        (i.e. the content window of a frame/iframe).
   */
  _getBrowserForContentWindow:
  function WCCR__getBrowserForContentWindow(aBrowserWindow, aContentWindow) {
    // This depends on pseudo APIs of browser.js and tabbrowser.xml
    aContentWindow = aContentWindow.top;
    var browsers = aBrowserWindow.getBrowser().browsers;
    for (var i = 0; i < browsers.length; ++i) {
      if (browsers[i].contentWindow == aContentWindow)
        return browsers[i];
    }
  },

  /**
   * Appends a notifcation for the given feed reader details.
   *
   * The notification could be either a pseudo-dialog which lets
   * the user to add the feed reader:
   * [ [icon] Add %feed-reader-name% (%feed-reader-host%) as a Feed Reader?  (Add) [x] ]
   *
   * or a simple message for the case where the feed reader is already registered:
   * [ [icon] %feed-reader-name% is already registered as a Feed Reader             [x] ]
   *
   * A new notification isn't appended if the given notificationbox has a
   * notification for the same feed reader.
   *
   * @param aURI
   *        The url of the feed reader as a nsIURI object
   * @param aName
   *        The feed reader name as it was passed to registerContentHandler
   * @param aNotificationBox
   *        The notification box to which a notification might be appended
   * @return true if a notification has been appended, false otherwise.
   */
  _appendFeedReaderNotification:
  function WCCR__appendFeedReaderNotification(aURI, aName, aNotificationBox) {
    var uriSpec = aURI.spec;
    var notificationValue = "feed reader notification: " + uriSpec;
    var notificationIcon = aURI.prePath + "/favicon.ico";

    // Don't append a new notification if the notificationbox
    // has a notification for the given feed reader already
    if (aNotificationBox.getNotificationWithValue(notificationValue))
      return false;

    var buttons, message;
    if (this.getWebContentHandlerByURI(TYPE_MAYBE_FEED, uriSpec))
      message = this._getFormattedString("handlerRegistered", [aName]);
    else {
      message = this._getFormattedString("addHandler", [aName, aURI.host]);
      var self = this;
      var addButton = {
        _outer: self,
        label: self._getString("addHandlerAddButton"),
        accessKey: self._getString("addHandlerAddButtonAccesskey"),
        feedReaderInfo: { uri: uriSpec, name: aName },

        /* static */
        callback:
        function WCCR__addFeedReaderButtonCallback(aNotification, aButtonInfo) {
          var uri = aButtonInfo.feedReaderInfo.uri;
          var name = aButtonInfo.feedReaderInfo.name;
          var outer = aButtonInfo._outer;

          // The reader could have been added from another window mean while
          if (!outer.getWebContentHandlerByURI(TYPE_MAYBE_FEED, uri)) {
            outer._registerContentHandler(TYPE_MAYBE_FEED, uri, name);
            outer._saveContentHandlerToPrefs(TYPE_MAYBE_FEED, uri, name);

            // Make the new handler the last-selected reader in the preview page
            // and make sure the preview page is shown the next time a feed is visited
            var pb = Cc["@mozilla.org/preferences-service;1"].
                     getService(Ci.nsIPrefService).getBranch(null);
            pb.setCharPref(PREF_SELECTED_READER, "web");

            var supportsString = 
              Cc["@mozilla.org/supports-string;1"].
              createInstance(Ci.nsISupportsString);
              supportsString.data = uri;
            pb.setComplexValue(PREF_SELECTED_WEB, Ci.nsISupportsString,
                               supportsString);
            pb.setCharPref(PREF_SELECTED_ACTION, "ask");
            outer._setAutoHandler(TYPE_MAYBE_FEED, null);
          }

          // avoid reference cycles
          aButtonInfo._outer = null;

          return false;
        }
      };
      buttons = [addButton];
    }

    aNotificationBox.appendNotification(message,
                                        notificationValue,
                                        notificationIcon,
                                        aNotificationBox.PRIORITY_INFO_LOW,
                                        buttons);
    return true;
  },

  /**
   * Save Web Content Handler metadata to persistent preferences. 
   * @param   contentType
   *          The content Type being handled
   * @param   uri
   *          The uri of the web service
   * @param   title
   *          The human readable name of the web service
   *
   * This data is stored under:
   * 
   *    browser.contentHandlers.type0 = content/type
   *    browser.contentHandlers.uri0 = http://www.foo.com/q=%s
   *    browser.contentHandlers.title0 = Foo 2.0alphr
   */
  _saveContentHandlerToPrefs: 
  function WCCR__saveContentHandlerToPrefs(contentType, uri, title) {
    var ps = 
        Cc["@mozilla.org/preferences-service;1"].
        getService(Ci.nsIPrefService);
    var i = 0;
    var typeBranch = null;
    while (true) {
      typeBranch = 
        ps.getBranch(PREF_CONTENTHANDLERS_BRANCH + i + ".");
      try {
        typeBranch.getCharPref("type");
        ++i;
      }
      catch (e) {
        // No more handlers
        break;
      }
    }
    if (typeBranch) {
      typeBranch.setCharPref("type", contentType);
      var pls = 
          Cc["@mozilla.org/pref-localizedstring;1"].
          createInstance(Ci.nsIPrefLocalizedString);
      pls.data = uri;
      typeBranch.setComplexValue("uri", Ci.nsIPrefLocalizedString, pls);
      pls.data = title;
      typeBranch.setComplexValue("title", Ci.nsIPrefLocalizedString, pls);
    
      ps.savePrefFile(null);
    }
  },
  
  /**
   * Determines if there is a type with a particular uri registered for the 
   * specified content type already.
   * @param   contentType
   *          The content type that the uri handles
   * @param   uri
   *          The uri of the 
   */
  _typeIsRegistered: function WCCR__typeIsRegistered(contentType, uri) {
    if (!(contentType in this._contentTypes))
      return false;
      
    var services = this._contentTypes[contentType];
    for (var i = 0; i < services.length; ++i) {
      // This uri has already been registered
      if (services[i].uri == uri)
        return true;
    }
    return false;
  },
  
  /**
   * Gets a stream converter contract id for the specified content type.
   * @param   contentType
   *          The source content type for the conversion.
   * @returns A contract id to construct a converter to convert between the 
   *          contentType and *\/*.
   */
  _getConverterContractID: function WCCR__getConverterContractID(contentType) {
    const template = "@mozilla.org/streamconv;1?from=%s&to=*/*";
    return template.replace(/%s/, contentType);
  },
  
  /**
   * Update the content type -> handler map. This mapping is not persisted, use
   * registerContentHandler or _saveContentHandlerToPrefs for that purpose.
   * @param   contentType
   *          The content Type being handled
   * @param   uri
   *          The uri of the web service
   * @param   title
   *          The human readable name of the web service
   */
  _registerContentHandler: 
  function WCCR__registerContentHandler(contentType, uri, title) {
    if (!(contentType in this._contentTypes))
      this._contentTypes[contentType] = [];

    // Avoid adding duplicates
    if (this._typeIsRegistered(contentType, uri)) 
      return;
    
    this._contentTypes[contentType].push(new ServiceInfo(contentType, uri, title));
    
    if (!(contentType in this._blockedTypes)) {
      var converterContractID = this._getConverterContractID(contentType);
      var cr = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
      cr.registerFactory(WCC_CLASSID, WCC_CLASSNAME, converterContractID, 
                         WebContentConverterFactory);
    }
  },
  
  /**
   * See nsIWebContentConverterService
   */
  getContentHandlers: 
  function WCCR_getContentHandlers(contentType, countRef) {
    countRef.value = 0;
    if (!(contentType in this._contentTypes))
      return [];
    
    var handlers = this._contentTypes[contentType];
    countRef.value = handlers.length;
    return handlers;
  },
  
  /**
   * See nsIWebContentConverterService
   */
  resetHandlersForType: 
  function WCCR_resetHandlersForType(contentType) {
    // currently unused within the tree, so only useful for extensions; previous
    // impl. was buggy (and even infinite-looped!), so I argue that this is a
    // definite improvement
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },
  
  /**
   * Registers a handler from the settings on a preferences branch.
   *
   * @param branch
   *        an nsIPrefBranch containing "type", "uri", and "title" preferences
   *        corresponding to the content handler to be registered
   */
  _registerContentHandlerWithBranch: function(branch) {
    /**
     * Since we support up to six predefined readers, we need to handle gaps 
     * better, since the first branch with user-added values will be .6
     * 
     * How we deal with that is to check to see if there's no prefs in the 
     * branch and stop cycling once that's true.  This doesn't fix the case
     * where a user manually removes a reader, but that's not supported yet!
     */
    var vals = branch.getChildList("", {});
    if (vals.length == 0)
      return;

    try {
      var type = branch.getCharPref("type");
      var uri = branch.getComplexValue("uri", Ci.nsIPrefLocalizedString).data;
      var title = branch.getComplexValue("title",
                                         Ci.nsIPrefLocalizedString).data;
      this._registerContentHandler(type, uri, title);
    }
    catch(ex) {
      // do nothing, the next branch might have values
    }
  },

  /**
   * Load the auto handler, content handler and protocol tables from 
   * preferences.
   */
  _init: function WCCR__init() {
    var ps = 
        Cc["@mozilla.org/preferences-service;1"].
        getService(Ci.nsIPrefService);

    var kids = ps.getBranch(PREF_CONTENTHANDLERS_BRANCH)
                 .getChildList("", {});

    // first get the numbers of the providers by getting all ###.uri prefs
    var nums = [];
    for (var i = 0; i < kids.length; i++) {
      var match = /^(\d+)\.uri$/.exec(kids[i]);
      if (!match)
        continue;
      else
        nums.push(match[1]);
    }

    // now register them
    for (var i = 0; i < nums.length; i++) {
      var branch = ps.getBranch(PREF_CONTENTHANDLERS_BRANCH + nums[i] + ".");
      this._registerContentHandlerWithBranch(branch);
    }

    // We need to do this _after_ registering all of the available handlers, 
    // so that getWebContentHandlerByURI can return successfully.
    try {
      var autoBranch = ps.getBranch(PREF_CONTENTHANDLERS_AUTO);
      var childPrefs = autoBranch.getChildList("", { });
      for (var i = 0; i < childPrefs.length; ++i) {
        var type = childPrefs[i];
        var uri = autoBranch.getCharPref(type);
        if (uri) {
          var handler = this.getWebContentHandlerByURI(type, uri);
          this._setAutoHandler(type, handler);
        }
      }
    }
    catch (e) {
      // No auto branch yet, that's fine
      //LOG("WCCR.init: There is no auto branch, benign");
    }
  },

  /**
   * See nsIObserver
   */
  observe: function WCCR_observe(subject, topic, data) {
    var os = 
        Cc["@mozilla.org/observer-service;1"].
        getService(Ci.nsIObserverService);
    switch (topic) {
    case "app-startup":
      os.addObserver(this, "profile-after-change", false);
      break;
    case "profile-after-change":
      os.removeObserver(this, "profile-after-change");
      this._init();
      break;
    }
  },
  
  /**
   * See nsIFactory
   */
  createInstance: function WCCR_createInstance(outer, iid) {
    if (outer != null)
      throw Cr.NS_ERROR_NO_AGGREGATION;
    return this.QueryInterface(iid);
  },

  /**
   * See nsIClassInfo
   */
  getInterfaces: function WCCR_getInterfaces(countRef) {
    var interfaces = 
        [Ci.nsIWebContentConverterService, Ci.nsIWebContentHandlerRegistrar,
         Ci.nsIObserver, Ci.nsIClassInfo, Ci.nsIFactory, Ci.nsISupports];
    countRef.value = interfaces.length;
    return interfaces;
  },
  getHelperForLanguage: function WCCR_getHelperForLanguage(language) {
    return null;
  },
  contractID: WCCR_CONTRACTID,
  classDescription: WCCR_CLASSNAME,
  classID: WCCR_CLASSID,
  implementationLanguage: Ci.nsIProgrammingLanguage.JAVASCRIPT,
  flags: Ci.nsIClassInfo.DOM_OBJECT,
  
  /**
   * See nsISupports
   */
  QueryInterface: XPCOMUtils.generateQI(
     [Ci.nsIWebContentConverterService, 
      Ci.nsIWebContentHandlerRegistrar,
      Ci.nsIObserver,
      Ci.nsIClassInfo,
      Ci.nsIFactory,
      Ci.nsISupports]),

  _xpcom_categories: [{
    category: "app-startup",
    service: true
  }]
};

function NSGetModule(cm, file) {
  return XPCOMUtils.generateModule([WebContentConverterRegistrar]);
}

#include ../../../../toolkit/content/debug.js

