/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["WebProtocolHandlerRegistrar"];

const STRING_BUNDLE_URI = "chrome://browser/locale/feeds/subscribe.properties";

function WebProtocolHandlerRegistrar() {}

WebProtocolHandlerRegistrar.prototype = {
  get stringBundle() {
    let sb = Services.strings.createBundle(STRING_BUNDLE_URI);
    delete WebProtocolHandlerRegistrar.prototype.stringBundle;
    return (WebProtocolHandlerRegistrar.prototype.stringBundle = sb);
  },

  _getFormattedString(key, params) {
    return this.stringBundle.formatStringFromName(key, params);
  },

  _getString(key) {
    return this.stringBundle.GetStringFromName(key);
  },

  /**
   * See nsIWebProtocolHandlerRegistrar
   */
  removeProtocolHandler(aProtocol, aURITemplate) {
    let eps = Cc[
      "@mozilla.org/uriloader/external-protocol-service;1"
    ].getService(Ci.nsIExternalProtocolService);
    let handlerInfo = eps.getProtocolHandlerInfo(aProtocol);
    let handlers = handlerInfo.possibleApplicationHandlers;
    for (let i = 0; i < handlers.length; i++) {
      try {
        // We only want to test web handlers
        let handler = handlers.queryElementAt(i, Ci.nsIWebHandlerApp);
        if (handler.uriTemplate == aURITemplate) {
          handlers.removeElementAt(i);
          let hs = Cc["@mozilla.org/uriloader/handler-service;1"].getService(
            Ci.nsIHandlerService
          );
          hs.store(handlerInfo);
          return;
        }
      } catch (e) {
        /* it wasn't a web handler */
      }
    }
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
    let eps = Cc[
      "@mozilla.org/uriloader/external-protocol-service;1"
    ].getService(Ci.nsIExternalProtocolService);
    let handlerInfo = eps.getProtocolHandlerInfo(aProtocol);
    let handlers = handlerInfo.possibleApplicationHandlers;
    for (let i = 0; i < handlers.length; i++) {
      try {
        // We only want to test web handlers
        let handler = handlers.queryElementAt(i, Ci.nsIWebHandlerApp);
        if (handler.uriTemplate == aURITemplate) {
          return true;
        }
      } catch (e) {
        /* it wasn't a web handler */
      }
    }
    return false;
  },

  /**
   * See nsIWebProtocolHandlerRegistrar
   */
  registerProtocolHandler(
    aProtocol,
    aURI,
    aTitle,
    aDocumentURI,
    aBrowserOrWindow
  ) {
    aProtocol = (aProtocol || "").toLowerCase();
    if (!aURI || !aDocumentURI) {
      return;
    }

    let browser = aBrowserOrWindow; // This is the e10s case.
    if (aBrowserOrWindow instanceof Ci.nsIDOMWindow) {
      // In the non-e10s case, grab the browser off the same-process window.
      let rootDocShell = aBrowserOrWindow.docShell.sameTypeRootTreeItem;
      browser = rootDocShell.QueryInterface(Ci.nsIDocShell).chromeEventHandler;
    }

    let browserWindow = browser.ownerGlobal;
    try {
      browserWindow.navigator.checkProtocolHandlerAllowed(
        aProtocol,
        aURI,
        aDocumentURI
      );
    } catch (ex) {
      // We should have already shown the user an error.
      return;
    }

    // If the protocol handler is already registered, just return early.
    if (this._protocolHandlerRegistered(aProtocol, aURI.spec)) {
      return;
    }

    // Now Ask the user and provide the proper callback
    let message = this._getFormattedString("addProtocolHandlerMessage", [
      aURI.host,
      aProtocol,
    ]);

    let notificationIcon = aURI.prePath + "/favicon.ico";
    let notificationValue = "Protocol Registration: " + aProtocol;
    let addButton = {
      label: this._getString("addProtocolHandlerAddButton"),
      accessKey: this._getString("addProtocolHandlerAddButtonAccesskey"),
      protocolInfo: { protocol: aProtocol, uri: aURI.spec, name: aTitle },

      callback(aNotification, aButtonInfo) {
        let protocol = aButtonInfo.protocolInfo.protocol;
        let name = aButtonInfo.protocolInfo.name;

        let handler = Cc[
          "@mozilla.org/uriloader/web-handler-app;1"
        ].createInstance(Ci.nsIWebHandlerApp);
        handler.name = name;
        handler.uriTemplate = aButtonInfo.protocolInfo.uri;

        let eps = Cc[
          "@mozilla.org/uriloader/external-protocol-service;1"
        ].getService(Ci.nsIExternalProtocolService);
        let handlerInfo = eps.getProtocolHandlerInfo(protocol);
        handlerInfo.possibleApplicationHandlers.appendElement(handler);

        // Since the user has agreed to add a new handler, chances are good
        // that the next time they see a handler of this type, they're going
        // to want to use it.  Reset the handlerInfo to ask before the next
        // use.
        handlerInfo.alwaysAskBeforeHandling = true;

        let hs = Cc["@mozilla.org/uriloader/handler-service;1"].getService(
          Ci.nsIHandlerService
        );
        hs.store(handlerInfo);
      },
    };
    let notificationBox = browser.getTabBrowser().getNotificationBox(browser);
    notificationBox.appendNotification(
      notificationValue,
      {
        label: message,
        image: notificationIcon,
        priority: notificationBox.PRIORITY_INFO_LOW,
      },
      [addButton]
    );
  },

  /**
   * See nsISupports
   */
  QueryInterface: ChromeUtils.generateQI(["nsIWebProtocolHandlerRegistrar"]),
};
