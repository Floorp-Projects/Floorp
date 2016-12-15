/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/PrivateBrowsingUtils.jsm");

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

function LOG(str) {
  dump("*** " + str + "\n");
}

const WCCR_CONTRACTID = "@mozilla.org/embeddor.implemented/web-content-handler-registrar;1";
const WCCR_CLASSID = Components.ID("{792a7e82-06a0-437c-af63-b2d12e808acc}");

const WCC_CLASSID = Components.ID("{db7ebf28-cc40-415f-8a51-1b111851df1e}");
const WCC_CLASSNAME = "Web Service Handler";

const TYPE_MAYBE_FEED = "application/vnd.mozilla.maybe.feed";
const TYPE_ANY = "*/*";

const PREF_CONTENTHANDLERS_AUTO = "browser.contentHandlers.auto.";
const PREF_CONTENTHANDLERS_BRANCH = "browser.contentHandlers.types.";
const PREF_SELECTED_WEB = "browser.feeds.handlers.webservice";
const PREF_SELECTED_ACTION = "browser.feeds.handler";
const PREF_SELECTED_READER = "browser.feeds.handler.default";
const PREF_HANDLER_EXTERNAL_PREFIX = "network.protocol-handler.external";

const STRING_BUNDLE_URI = "chrome://browser/locale/feeds/subscribe.properties";

const NS_ERROR_MODULE_DOM = 2152923136;
const NS_ERROR_DOM_SYNTAX_ERR = NS_ERROR_MODULE_DOM + 12;

function WebContentConverter() {
}
WebContentConverter.prototype = {
  convert() { },
  asyncConvertData() { },
  onDataAvailable() { },
  onStopRequest() { },

  onStartRequest(request, context) {
    let wccr =
        Cc[WCCR_CONTRACTID].
        getService(Ci.nsIWebContentConverterService);
    wccr.loadPreferredHandler(request);
  },

  QueryInterface(iid) {
    if (iid.equals(Ci.nsIStreamConverter) ||
        iid.equals(Ci.nsIStreamListener) ||
        iid.equals(Ci.nsISupports))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  }
};

let WebContentConverterFactory = {
  createInstance(outer, iid) {
    if (outer != null)
      throw Cr.NS_ERROR_NO_AGGREGATION;
    return new WebContentConverter().QueryInterface(iid);
  },

  QueryInterface(iid) {
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
  equals(aHandlerApp) {
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
  getHandlerURI(uri) {
    return this._uri.replace(/%s/gi, encodeURIComponent(uri));
  },

  QueryInterface(iid) {
    if (iid.equals(Ci.nsIWebContentHandlerInfo) ||
        iid.equals(Ci.nsISupports))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  }
};

const Utils = {
  makeURI(aURL, aOriginCharset, aBaseURI) {
    return Services.io.newURI(aURL, aOriginCharset, aBaseURI);
  },

  checkAndGetURI(aURIString, aContentWindow) {
    let uri;
    try {
      let baseURI = aContentWindow.document.baseURIObject;
      uri = this.makeURI(aURIString, null, baseURI);
    } catch (ex) {
      throw NS_ERROR_DOM_SYNTAX_ERR;
    }

    // For security reasons we reject non-http(s) urls (see bug 354316),
    // we may need to revise this once we support more content types
    if (uri.scheme != "http" && uri.scheme != "https") {
      throw this.getSecurityError(
        "Permission denied to add " + uri.spec + " as a content or protocol handler",
        aContentWindow);
    }

    // We also reject handlers registered from a different host (see bug 402287)
    // The pref allows us to test the feature
    let pb = Services.prefs;
    if (!["http:", "https:"].includes(aContentWindow.location.protocol) ||
         aContentWindow.location.hostname != uri.host) {
      throw this.getSecurityError(
        "Permission denied to add " + uri.spec + " as a content or protocol handler",
        aContentWindow);
    }

    // If the uri doesn't contain '%s', it won't be a good handler
    if (uri.spec.indexOf("%s") < 0)
      throw NS_ERROR_DOM_SYNTAX_ERR;

    return uri;
  },

  // NB: Throws if aProtocol is not allowed.
  checkProtocolHandlerAllowed(aProtocol, aURIString, aWindowOrNull) {
    // First, check to make sure this isn't already handled internally (we don't
    // want to let them take over, say "chrome").
    let handler = Services.io.getProtocolHandler(aProtocol);
    if (!(handler instanceof Ci.nsIExternalProtocolHandler)) {
      // This is handled internally, so we don't want them to register
      throw this.getSecurityError(
        `Permission denied to add ${aURIString} as a protocol handler`,
        aWindowOrNull);
    }

    // check if it is in the black list
    let pb = Services.prefs;
    let allowed;
    try {
      allowed = pb.getBoolPref(PREF_HANDLER_EXTERNAL_PREFIX + "." + aProtocol);
    }
    catch (e) {
      allowed = pb.getBoolPref(PREF_HANDLER_EXTERNAL_PREFIX + "-default");
    }
    if (!allowed) {
      throw this.getSecurityError(
        `Not allowed to register a protocol handler for ${aProtocol}`,
        aWindowOrNull);
    }
  },

  // Return a SecurityError exception from the given Window if one is given.  If
  // none is given, just return the given error string, for lack of anything
  // better.
  getSecurityError(errorString, aWindowOrNull) {
    if (!aWindowOrNull) {
      return errorString;
    }

    return new aWindowOrNull.DOMException(errorString, "SecurityError");
  },

  /**
   * Mappings from known feed types to our internal content type.
   */
  _mappings: {
    "application/rss+xml": TYPE_MAYBE_FEED,
    "application/atom+xml": TYPE_MAYBE_FEED,
  },

  resolveContentType(aContentType) {
    if (aContentType in this._mappings)
      return this._mappings[aContentType];
    return aContentType;
  }
};

function WebContentConverterRegistrar() {
  this._contentTypes = {};
  this._autoHandleContentTypes = {};
}

WebContentConverterRegistrar.prototype = {
  get stringBundle() {
    let sb = Services.strings.createBundle(STRING_BUNDLE_URI);
    delete WebContentConverterRegistrar.prototype.stringBundle;
    return WebContentConverterRegistrar.prototype.stringBundle = sb;
  },

  _getFormattedString(key, params) {
    return this.stringBundle.formatStringFromName(key, params, params.length);
  },

  _getString(key) {
    return this.stringBundle.GetStringFromName(key);
  },

  /**
   * See nsIWebContentConverterService
   */
  getAutoHandler(contentType) {
    contentType = Utils.resolveContentType(contentType);
    if (contentType in this._autoHandleContentTypes)
      return this._autoHandleContentTypes[contentType];
    return null;
  },

  /**
   * See nsIWebContentConverterService
   */
  setAutoHandler(contentType, handler) {
    if (handler && !this._typeIsRegistered(contentType, handler.uri))
      throw Cr.NS_ERROR_NOT_AVAILABLE;

    contentType = Utils.resolveContentType(contentType);
    this._setAutoHandler(contentType, handler);

    let ps = Services.prefs;
    let autoBranch = ps.getBranch(PREF_CONTENTHANDLERS_AUTO);
    if (handler)
      autoBranch.setCharPref(contentType, handler.uri);
    else if (autoBranch.prefHasUserValue(contentType))
      autoBranch.clearUserPref(contentType);

    ps.savePrefFile(null);
  },

  /**
   * Update the internal data structure (not persistent)
   */
  _setAutoHandler(contentType, handler) {
    if (handler)
      this._autoHandleContentTypes[contentType] = handler;
    else if (contentType in this._autoHandleContentTypes)
      delete this._autoHandleContentTypes[contentType];
  },

  /**
   * See nsIWebContentConverterService
   */
  getWebContentHandlerByURI(contentType, uri) {
    return this.getContentHandlers(contentType)
               .find(e => e.uri == uri) || null;
  },

  /**
   * See nsIWebContentConverterService
   */
  loadPreferredHandler(request) {
    let channel = request.QueryInterface(Ci.nsIChannel);
    let contentType = Utils.resolveContentType(channel.contentType);
    let handler = this.getAutoHandler(contentType);
    if (handler) {
      request.cancel(Cr.NS_ERROR_FAILURE);

      let webNavigation =
          channel.notificationCallbacks.getInterface(Ci.nsIWebNavigation);
      webNavigation.loadURI(handler.getHandlerURI(channel.URI.spec),
                            Ci.nsIWebNavigation.LOAD_FLAGS_NONE,
                            null, null, null);
    }
  },

  /**
   * See nsIWebContentConverterService
   */
  removeProtocolHandler(aProtocol, aURITemplate) {
    let eps = Cc["@mozilla.org/uriloader/external-protocol-service;1"].
              getService(Ci.nsIExternalProtocolService);
    let handlerInfo = eps.getProtocolHandlerInfo(aProtocol);
    let handlers =  handlerInfo.possibleApplicationHandlers;
    for (let i = 0; i < handlers.length; i++) {
      try { // We only want to test web handlers
        let handler = handlers.queryElementAt(i, Ci.nsIWebHandlerApp);
        if (handler.uriTemplate == aURITemplate) {
          handlers.removeElementAt(i);
          let hs = Cc["@mozilla.org/uriloader/handler-service;1"].
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
  removeContentHandler(contentType, uri) {
    function notURI(serviceInfo) {
      return serviceInfo.uri != uri;
    }

    if (contentType in this._contentTypes) {
      this._contentTypes[contentType] =
        this._contentTypes[contentType].filter(notURI);
    }
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
   * Determines if a web handler is already registered.
   *
   * @param aProtocol
   *        The scheme of the web handler we are checking for.
   * @param aURITemplate
   *        The URI template that the handler uses to handle the protocol.
   * @return true if it is already registered, false otherwise.
   */
  _protocolHandlerRegistered(aProtocol, aURITemplate) {
    let eps = Cc["@mozilla.org/uriloader/external-protocol-service;1"].
              getService(Ci.nsIExternalProtocolService);
    let handlerInfo = eps.getProtocolHandlerInfo(aProtocol);
    let handlers =  handlerInfo.possibleApplicationHandlers;
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
  registerProtocolHandler(aProtocol, aURIString, aTitle, aBrowserOrWindow) {
    LOG("registerProtocolHandler(" + aProtocol + "," + aURIString + "," + aTitle + ")");
    let haveWindow = (aBrowserOrWindow instanceof Ci.nsIDOMWindow);
    let uri;
    if (haveWindow) {
      uri = Utils.checkAndGetURI(aURIString, aBrowserOrWindow);
    } else {
      // aURIString must not be a relative URI.
      uri = Utils.makeURI(aURIString, null);
    }

    // If the protocol handler is already registered, just return early.
    if (this._protocolHandlerRegistered(aProtocol, uri.spec)) {
      return;
    }

    let browser;
    if (haveWindow) {
      let browserWindow =
        this._getBrowserWindowForContentWindow(aBrowserOrWindow);
      browser = this._getBrowserForContentWindow(browserWindow,
                                                 aBrowserOrWindow);
    } else {
      browser = aBrowserOrWindow;
    }
    if (PrivateBrowsingUtils.isBrowserPrivate(browser)) {
      // Inside the private browsing mode, we don't want to alert the user to save
      // a protocol handler.  We log it to the error console so that web developers
      // would have some way to tell what's going wrong.
      Services.console.
      logStringMessage("Web page denied access to register a protocol handler inside private browsing mode");
      return;
    }

    Utils.checkProtocolHandlerAllowed(aProtocol, aURIString,
                                      haveWindow ? aBrowserOrWindow : null);

    // Now Ask the user and provide the proper callback
    let message = this._getFormattedString("addProtocolHandler",
                                           [aTitle, uri.host, aProtocol]);

    let notificationIcon = uri.prePath + "/favicon.ico";
    let notificationValue = "Protocol Registration: " + aProtocol;
    let addButton = {
      label: this._getString("addProtocolHandlerAddButton"),
      accessKey: this._getString("addProtocolHandlerAddButtonAccesskey"),
      protocolInfo: { protocol: aProtocol, uri: uri.spec, name: aTitle },

      callback(aNotification, aButtonInfo) {
          let protocol = aButtonInfo.protocolInfo.protocol;
          let name     = aButtonInfo.protocolInfo.name;

          let handler = Cc["@mozilla.org/uriloader/web-handler-app;1"].
                        createInstance(Ci.nsIWebHandlerApp);
          handler.name = name;
          handler.uriTemplate = aButtonInfo.protocolInfo.uri;

          let eps = Cc["@mozilla.org/uriloader/external-protocol-service;1"].
                    getService(Ci.nsIExternalProtocolService);
          let handlerInfo = eps.getProtocolHandlerInfo(protocol);
          handlerInfo.possibleApplicationHandlers.appendElement(handler, false);

          // Since the user has agreed to add a new handler, chances are good
          // that the next time they see a handler of this type, they're going
          // to want to use it.  Reset the handlerInfo to ask before the next
          // use.
          handlerInfo.alwaysAskBeforeHandling = true;

          let hs = Cc["@mozilla.org/uriloader/handler-service;1"].
                   getService(Ci.nsIHandlerService);
          hs.store(handlerInfo);
        }
    };
    let notificationBox = browser.getTabBrowser().getNotificationBox(browser);
    notificationBox.appendNotification(message,
                                       notificationValue,
                                       notificationIcon,
                                       notificationBox.PRIORITY_INFO_LOW,
                                       [addButton]);
  },

  /**
   * See nsIWebContentHandlerRegistrar
   * If a DOM window is provided, then the request came from content, so we
   * prompt the user to confirm the registration.
   */
  registerContentHandler(aContentType, aURIString, aTitle, aWindowOrBrowser) {
    LOG("registerContentHandler(" + aContentType + "," + aURIString + "," + aTitle + ")");

    // Make sure to do our URL checks up front, before our content type check,
    // just like the WebContentConverterRegistrarContent does.
    let haveWindow = aWindowOrBrowser &&
                     (aWindowOrBrowser instanceof Ci.nsIDOMWindow);
    let uri;
    if (haveWindow) {
      uri = Utils.checkAndGetURI(aURIString, aWindowOrBrowser);
    } else if (aWindowOrBrowser) {
      // uri was vetted in the content process.
      uri = Utils.makeURI(aURIString, null);
    }

    // We only support feed types at present.
    let contentType = Utils.resolveContentType(aContentType);
    // XXX We should be throwing a Utils.getSecurityError() here in at least
    // some cases.  See bug 1266492.
    if (contentType != TYPE_MAYBE_FEED) {
      return;
    }

    if (aWindowOrBrowser) {
      let notificationBox;
      if (haveWindow) {
        let browserWindow = this._getBrowserWindowForContentWindow(aWindowOrBrowser);
        let browserElement = this._getBrowserForContentWindow(browserWindow, aWindowOrBrowser);
        notificationBox = browserElement.getTabBrowser().getNotificationBox(browserElement);
      } else {
        notificationBox = aWindowOrBrowser.getTabBrowser()
                                          .getNotificationBox(aWindowOrBrowser);
      }

      this._appendFeedReaderNotification(uri, aTitle, notificationBox);
    }
    else {
      this._registerContentHandler(contentType, aURIString, aTitle);
    }
  },

  /**
   * Returns the browser chrome window in which the content window is in
   */
  _getBrowserWindowForContentWindow(aContentWindow) {
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
  _getBrowserForContentWindow(aBrowserWindow, aContentWindow) {
    // This depends on pseudo APIs of browser.js and tabbrowser.xml
    aContentWindow = aContentWindow.top;
    return aBrowserWindow.gBrowser.browsers.find((browser) =>
      browser.contentWindow == aContentWindow);
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
  _appendFeedReaderNotification(aURI, aName, aNotificationBox) {
    let uriSpec = aURI.spec;
    let notificationValue = "feed reader notification: " + uriSpec;
    let notificationIcon = aURI.prePath + "/favicon.ico";

    // Don't append a new notification if the notificationbox
    // has a notification for the given feed reader already
    if (aNotificationBox.getNotificationWithValue(notificationValue))
      return false;

    let buttons;
    let message;
    if (this.getWebContentHandlerByURI(TYPE_MAYBE_FEED, uriSpec))
      message = this._getFormattedString("handlerRegistered", [aName]);
    else {
      message = this._getFormattedString("addHandler", [aName, aURI.host]);
      let self = this;
      let addButton = {
        _outer: self,
        label: self._getString("addHandlerAddButton"),
        accessKey: self._getString("addHandlerAddButtonAccesskey"),
        feedReaderInfo: { uri: uriSpec, name: aName },

        /* static */
        callback(aNotification, aButtonInfo) {
          let uri = aButtonInfo.feedReaderInfo.uri;
          let name = aButtonInfo.feedReaderInfo.name;
          let outer = aButtonInfo._outer;

          // The reader could have been added from another window mean while
          if (!outer.getWebContentHandlerByURI(TYPE_MAYBE_FEED, uri))
            outer._registerContentHandler(TYPE_MAYBE_FEED, uri, name);

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
  _saveContentHandlerToPrefs(contentType, uri, title) {
    let ps = Services.prefs;
    let i = 0;
    let typeBranch = null;
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
      let pls =
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
   *          The uri of the content type
   */
  _typeIsRegistered(contentType, uri) {
    if (!(contentType in this._contentTypes))
      return false;

    return this._contentTypes[contentType]
               .some(t => t.uri == uri);
  },

  /**
   * Gets a stream converter contract id for the specified content type.
   * @param   contentType
   *          The source content type for the conversion.
   * @returns A contract id to construct a converter to convert between the
   *          contentType and *\/*.
   */
  _getConverterContractID(contentType) {
    const template = "@mozilla.org/streamconv;1?from=%s&to=*/*";
    return template.replace(/%s/, contentType);
  },

  /**
   * Register a web service handler for a content type.
   *
   * @param   contentType
   *          the content type being handled
   * @param   uri
   *          the URI of the web service
   * @param   title
   *          the human readable name of the web service
   */
  _registerContentHandler(contentType, uri, title) {
    this._updateContentTypeHandlerMap(contentType, uri, title);
    this._saveContentHandlerToPrefs(contentType, uri, title);

    if (contentType == TYPE_MAYBE_FEED) {
      // Make the new handler the last-selected reader in the preview page
      // and make sure the preview page is shown the next time a feed is visited
      let pb = Services.prefs.getBranch(null);
      pb.setCharPref(PREF_SELECTED_READER, "web");

      let supportsString =
        Cc["@mozilla.org/supports-string;1"].
        createInstance(Ci.nsISupportsString);
        supportsString.data = uri;
      pb.setComplexValue(PREF_SELECTED_WEB, Ci.nsISupportsString,
                         supportsString);
      pb.setCharPref(PREF_SELECTED_ACTION, "ask");
      this._setAutoHandler(TYPE_MAYBE_FEED, null);
    }
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
  _updateContentTypeHandlerMap(contentType, uri, title) {
    if (!(contentType in this._contentTypes))
      this._contentTypes[contentType] = [];

    // Avoid adding duplicates
    if (this._typeIsRegistered(contentType, uri))
      return;

    this._contentTypes[contentType].push(new ServiceInfo(contentType, uri, title));

    if (!(contentType in this._blockedTypes)) {
      let converterContractID = this._getConverterContractID(contentType);
      let cr = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
      cr.registerFactory(WCC_CLASSID, WCC_CLASSNAME, converterContractID,
                         WebContentConverterFactory);
    }
  },

  /**
   * See nsIWebContentConverterService
   */
  getContentHandlers(contentType, countRef) {
    if (countRef) {
      countRef.value = 0;
    }
    if (!(contentType in this._contentTypes))
      return [];

    let handlers = this._contentTypes[contentType];
    if (countRef) {
      countRef.value = handlers.length;
    }
    return handlers;
  },

  /**
   * See nsIWebContentConverterService
   */
  resetHandlersForType(contentType) {
    // currently unused within the tree, so only useful for extensions; previous
    // impl. was buggy (and even infinite-looped!), so I argue that this is a
    // definite improvement
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },

  /**
   * Registers a handler from the settings on a preferences branch.
   *
   * Since we support up to six predefined readers, we need to handle gaps
   * better, since the first branch with user-added values will be .6
   *
   * How we deal with that is to check to see if there's no prefs in the
   * branch and stop cycling once that's true.  This doesn't fix the case
   * where a user manually removes a reader, but that's not supported yet!
   *
   * @param branch
   *        an nsIPrefBranch containing "type", "uri", and "title" preferences
   *        corresponding to the content handler to be registered
   */
  _registerContentHandlerHavingBranch(branch) {
    let vals = branch.getChildList("");
    if (vals.length == 0)
      return;

    let type = branch.getCharPref("type");
    let uri = branch.getComplexValue("uri", Ci.nsIPrefLocalizedString).data;
    let title = branch.getComplexValue("title",
                                       Ci.nsIPrefLocalizedString).data;
    this._updateContentTypeHandlerMap(type, uri, title);
  },

  /**
   * Load the auto handler, content handler and protocol tables from
   * preferences.
   */
  _init() {
    let ps = Services.prefs;

    let children = ps.getBranch(PREF_CONTENTHANDLERS_BRANCH)
                     .getChildList("");

    // first get the numbers of the providers by getting all ###.uri prefs
    let nums = children.map((child) => {
      let match = /^(\d+)\.uri$/.exec(child);
      return match ? match[1] : "";
    }).filter(child => !!child)
      .sort();


    // now register them
    for (let num of nums) {
      let branch = ps.getBranch(PREF_CONTENTHANDLERS_BRANCH + num + ".");
      try {
        this._registerContentHandlerHavingBranch(branch);
      } catch (ex) {
        // do nothing, the next branch might have values
      }
    }

    // We need to do this _after_ registering all of the available handlers,
    // so that getWebContentHandlerByURI can return successfully.
    let autoBranch;
    try {
      autoBranch = ps.getBranch(PREF_CONTENTHANDLERS_AUTO);
    } catch (e) {
      // No auto branch yet, that's fine
      // LOG("WCCR.init: There is no auto branch, benign");
    }

    if (autoBranch) {
      for (let type of autoBranch.getChildList("")) {
        let uri = autoBranch.getCharPref(type);
        if (uri) {
          let handler = this.getWebContentHandlerByURI(type, uri);
          if (handler) {
            this._setAutoHandler(type, handler);
          }
        }
      }
    }
  },

  /**
   * See nsIObserver
   */
  observe(subject, topic, data) {
    let os = Services.obs;
    switch (topic) {
    case "app-startup":
      os.addObserver(this, "browser-ui-startup-complete", false);
      break;
    case "browser-ui-startup-complete":
      os.removeObserver(this, "browser-ui-startup-complete");
      this._init();
      break;
    }
  },

  /**
   * See nsIFactory
   */
  createInstance(outer, iid) {
    if (outer != null)
      throw Cr.NS_ERROR_NO_AGGREGATION;
    return this.QueryInterface(iid);
  },

  classID: WCCR_CLASSID,

  /**
   * See nsISupports
   */
  QueryInterface: XPCOMUtils.generateQI(
     [Ci.nsIWebContentConverterService,
      Ci.nsIWebContentHandlerRegistrar,
      Ci.nsIObserver,
      Ci.nsIFactory]),

  _xpcom_categories: [{
    category: "app-startup",
    service: true
  }]
};

function WebContentConverterRegistrarContent() {
  this._contentTypes = {};
}

WebContentConverterRegistrarContent.prototype = {

  /**
   * Load the auto handler, content handler and protocol tables from
   * preferences.
   */
  _init() {
    let ps = Services.prefs;

    let children = ps.getBranch(PREF_CONTENTHANDLERS_BRANCH)
                     .getChildList("");

    // first get the numbers of the providers by getting all ###.uri prefs
    let nums = children.map((child) => {
      let match = /^(\d+)\.uri$/.exec(child);
      return match ? match[1] : "";
    }).filter(child => !!child)
      .sort();

    // now register them
    for (num of nums) {
      let branch = ps.getBranch(PREF_CONTENTHANDLERS_BRANCH + num + ".");
      try {
        this._registerContentHandlerHavingBranch(branch);
      } catch (ex) {
        // do nothing, the next branch might have values
      }
    }
  },

  _typeIsRegistered(contentType, uri) {
    return this._contentTypes[contentType]
               .some(e => e.uri == uri);
  },

  /**
   * Since we support up to six predefined readers, we need to handle gaps
   * better, since the first branch with user-added values will be .6
   *
   * How we deal with that is to check to see if there's no prefs in the
   * branch and stop cycling once that's true.  This doesn't fix the case
   * where a user manually removes a reader, but that's not supported yet!
   *
   * @param   branch
   *          The pref branch to register the content handler under
   *
   */
  _registerContentHandlerHavingBranch(branch) {
    let vals = branch.getChildList("");
    if (vals.length == 0)
      return;

    let type = branch.getCharPref("type");
    let uri = branch.getComplexValue("uri", Ci.nsIPrefLocalizedString).data;
    let title = branch.getComplexValue("title",
                                       Ci.nsIPrefLocalizedString).data;
    this._updateContentTypeHandlerMap(type, uri, title);
  },

  _updateContentTypeHandlerMap(contentType, uri, title) {
    if (!(contentType in this._contentTypes))
      this._contentTypes[contentType] = [];

    // Avoid adding duplicates
    if (this._typeIsRegistered(contentType, uri))
      return;

    this._contentTypes[contentType].push(new ServiceInfo(contentType, uri, title));

    if (!(contentType in this._blockedTypes)) {
      let converterContractID = this._getConverterContractID(contentType);
      let cr = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
      cr.registerFactory(WCC_CLASSID, WCC_CLASSNAME, converterContractID,
                         WebContentConverterFactory);
    }
  },

  /**
   * See nsIWebContentConverterService
   */
  getContentHandlers(contentType, countRef) {
    this._init();
    if (countRef) {
      countRef.value = 0;
    }

    if (!(contentType in this._contentTypes))
      return [];

    let handlers = this._contentTypes[contentType];
    if (countRef) {
      countRef.value = handlers.length;
    }
    return handlers;
  },

  setAutoHandler(contentType, handler) {
    Services.cpmm.sendAsyncMessage("WCCR:setAutoHandler",
                                   { contentType, handler });
  },

  getWebContentHandlerByURI(contentType, uri) {
    return this.getContentHandlers(contentType)
               .find(e => e.uri == uri) || null;
  },

  /**
   * See nsIWebContentHandlerRegistrar
   */
  registerContentHandler(aContentType, aURIString, aTitle, aBrowserOrWindow) {
    // aBrowserOrWindow must be a window.
    let messageManager = aBrowserOrWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                                         .getInterface(Ci.nsIWebNavigation)
                                         .QueryInterface(Ci.nsIDocShell)
                                         .QueryInterface(Ci.nsIInterfaceRequestor)
                                         .getInterface(Ci.nsITabChild)
                                         .messageManager;

    let uri = Utils.checkAndGetURI(aURIString, aBrowserOrWindow);
    // XXX We should be throwing a Utils.getSecurityError() here in at least
    // some cases.  See bug 1266492.
    if (Utils.resolveContentType(aContentType) != TYPE_MAYBE_FEED) {
      return;
    }

    messageManager.sendAsyncMessage("WCCR:registerContentHandler",
                                    { contentType: aContentType,
                                      uri: uri.spec,
                                      title: aTitle });
  },

  registerProtocolHandler(aProtocol, aURIString, aTitle, aBrowserOrWindow) {
    // aBrowserOrWindow must be a window.
    let messageManager = aBrowserOrWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                                         .getInterface(Ci.nsIWebNavigation)
                                         .QueryInterface(Ci.nsIDocShell)
                                         .QueryInterface(Ci.nsIInterfaceRequestor)
                                         .getInterface(Ci.nsITabChild)
                                         .messageManager;

    let uri = Utils.checkAndGetURI(aURIString, aBrowserOrWindow);
    Utils.checkProtocolHandlerAllowed(aProtocol, aURIString, aBrowserOrWindow);

    messageManager.sendAsyncMessage("WCCR:registerProtocolHandler",
                                    { protocol: aProtocol,
                                      uri: uri.spec,
                                      title: aTitle });
  },

  /**
   * See nsIFactory
   */
  createInstance(outer, iid) {
    if (outer != null)
      throw Cr.NS_ERROR_NO_AGGREGATION;
    return this.QueryInterface(iid);
  },

  classID: WCCR_CLASSID,

  /**
   * See nsISupports
   */
  QueryInterface: XPCOMUtils.generateQI(
                     [Ci.nsIWebContentHandlerRegistrar,
                      Ci.nsIWebContentConverterService,
                      Ci.nsIFactory])
};

this.NSGetFactory =
  (Services.appinfo.processType === Services.appinfo.PROCESS_TYPE_CONTENT) ?
    XPCOMUtils.generateNSGetFactory([WebContentConverterRegistrarContent]) :
    XPCOMUtils.generateNSGetFactory([WebContentConverterRegistrar]);
