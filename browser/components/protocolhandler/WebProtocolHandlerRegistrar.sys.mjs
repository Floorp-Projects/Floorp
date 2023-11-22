/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const STRING_BUNDLE_URI = "chrome://browser/locale/feeds/subscribe.properties";

export function WebProtocolHandlerRegistrar() {}

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  NimbusFeatures: "resource://nimbus/ExperimentAPI.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "log", () => {
  let { ConsoleAPI } = ChromeUtils.importESModule(
    "resource://gre/modules/Console.sys.mjs"
  );
  let consoleOptions = {
    // tip: set maxLogLevel to "debug" and use lazy.log.debug() to create
    // detailed messages during development. See LOG_LEVELS in Console.sys.mjs
    // for details.
    maxLogLevel: "warning",
    maxLogLevelPref: "browser.protocolhandler.loglevel",
    prefix: "WebProtocolHandlerRegistrar.sys.mjs",
  };
  return new ConsoleAPI(consoleOptions);
});

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
   * @param {string} aProtocol
   *        The scheme of the web handler we are checking for.
   * @param {string} aURITemplate
   *        The URI template that the handler uses to handle the protocol.
   * @returns {boolean} true if it is already registered, false otherwise.
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
        lazy.log.debug("No protocolHandler registered, because: " + e.message);
      }
    }
    return false;
  },

  /**
   * Private method to return the installHash, which is important for app
   * registration on OS level. Without it apps cannot be default/handler apps
   * under Windows. Because this value is used to check if its possible to reset
   * the default and to actually set it as well, this function puts the
   * acquisition of the installHash in one place in the hopes that check and set
   * conditions will never deviate.
   *
   * @returns {string} installHash
   */
  _getInstallHash() {
    const xreDirProvider = Cc[
      "@mozilla.org/xre/directory-provider;1"
    ].getService(Ci.nsIXREDirProvider);
    return xreDirProvider.getInstallHash();
  },

  /**
   * Private method to determine if we can set a new OS default for a certain
   * protocol.
   *
   * @param {string} protocol name, e.g. mailto (without ://)
   * @returns {boolean}
   */
  _canSetOSDefault(protocol) {
    // can be toggled off individually if necessary...
    if (!lazy.NimbusFeatures.mailto.getVariable("dualPrompt.os")) {
      lazy.log.debug("_canSetOSDefault: false: mailto rollout deactivated.");
      return false;
    }

    // this preferences saves that the user has dismissed the bar before...
    if (!Services.prefs.getBoolPref("browser.mailto.prompt.os", true)) {
      lazy.log.debug("_canSetOSDefault: false: prompt dismissed before.");
      return false;
    }

    // an installHash is required for the association with a scheme handler
    if ("" == this._getInstallHash()) {
      lazy.log.debug("_canSetOSDefault: false: no installation hash.");
      return false;
    }

    // check if we are already the protocolhandler...
    let shellService = Cc[
      "@mozilla.org/browser/shell-service;1"
    ].createInstance(Ci.nsIWindowsShellService);

    if (shellService.isDefaultHandlerFor(protocol)) {
      lazy.log.debug("_canSetOSDefault: false: is already default handler.");
      return false;
    }

    return true;
  },

  /**
   * Private method to reset the OS default for a certain protocol/uri scheme.
   * We basically ignore, that setDefaultExtensionHandlersUserChoice can fail
   * when the installHash is wrong or cannot be determined.
   *
   * @param {string} protocol name, e.g. mailto (without ://)
   * @returns {boolean}
   */
  _setOSDefault(protocol) {
    try {
      let defaultAgent = Cc["@mozilla.org/default-agent;1"].createInstance(
        Ci.nsIDefaultAgent
      );
      defaultAgent.setDefaultExtensionHandlersUserChoice(
        this._getInstallHash(),
        [protocol, "FirefoxURL"]
      );
      return true;
    } catch (e) {
      // TODO: why could not we just add the installHash and promote the running
      // install to be a properly installed one?
      lazy.log.debug(
        "Could not set Firefox as default application for " +
          protocol +
          ", because: " +
          e.message
      );
    }
    return false;
  },

  /**
   * Private method to set the default uri to handle a certain protocol. This
   * automates in a way what a user can do in settings under applications,
   * where different 'actions' can be chosen for different 'content types'.
   *
   * @param {string} protocol
   * @param {handler} handler
   */
  _setLocalDefault(protocol, handler) {
    let eps = Cc[
      "@mozilla.org/uriloader/external-protocol-service;1"
    ].getService(Ci.nsIExternalProtocolService);

    let handlerInfo = eps.getProtocolHandlerInfo(protocol);
    handlerInfo.preferredAction = Ci.nsIHandlerInfo.useHelperApp; // this is IMPORTANT!
    handlerInfo.preferredApplicationHandler = handler;
    handlerInfo.alwaysAskBeforeHandling = false;
    let hs = Cc["@mozilla.org/uriloader/handler-service;1"].getService(
      Ci.nsIHandlerService
    );
    hs.store(handlerInfo);
  },

  /**
   * Private method to set the default uri to handle a certain protocol. This
   * automates in a way what a user can do in settings under applications,
   * where different 'actions' can be chosen for different 'content types'.
   *
   * @param {string} protocol - e.g. 'mailto', so again without ://
   * @param {string} name - the protocol associated 'Action'
   * @param {string} uri - the uri (compare 'use other...' in the preferences)
   * @returns {handler} handler - either the existing one or a newly created
   */
  _addLocal(protocol, name, uri) {
    let eps = Cc[
      "@mozilla.org/uriloader/external-protocol-service;1"
    ].getService(Ci.nsIExternalProtocolService);

    let phi = eps.getProtocolHandlerInfo(protocol);
    // not adding duplicates and bail out with the existing entry
    for (let h of phi.possibleApplicationHandlers.enumerate()) {
      if (h.uriTemplate == uri) {
        return h;
      }
    }

    let handler = Cc["@mozilla.org/uriloader/web-handler-app;1"].createInstance(
      Ci.nsIWebHandlerApp
    );
    handler.name = name;
    handler.uriTemplate = uri;

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

    return handler;
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
    // first mitigation: check if the API call comes from another domain
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
    if (lazy.NimbusFeatures.mailto.getVariable("dualPrompt")) {
      if ("mailto" === aProtocol) {
        this._askUserToSetMailtoHandler(browser, aProtocol, aURI, aTitle);
        return;
      }
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

    // check if the notification box is already shown
    if (notificationBox.getNotificationWithValue(notificationValue)) {
      return;
    }

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

  /*
   * Special implementation for mailto
   *
   * @param {string} browser
   * @param {string} aProtocol
   * @param {string} aURI
   * @param {string} aTitle
   */
  async _askUserToSetMailtoHandler(browser, aProtocol, aURI, aTitle) {
    // shortcut for Localization
    let l10n = new Localization([
      "branding/brand.ftl",
      "browser/webProtocolHandler.ftl",
    ]);
    let [
      msg_os_box,
      msg_os_yes_confirm,
      msg_os_yes,
      msg_os_no,
      msg_box,
      msg_yes_confirm,
      msg_yes,
      msg_no,
    ] = await l10n.formatValues([
      { id: "protocolhandler-mailto-os-handler-notificationbox" },
      { id: "protocolhandler-mailto-os-handler-yes-confirm" },
      { id: "protocolhandler-mailto-os-handler-yes-button" },
      { id: "protocolhandler-mailto-os-handler-no-button" },
      {
        id: "protocolhandler-mailto-handler-notificationbox-always",
        args: { url: aURI.prePath },
      },
      {
        id: "protocolhandler-mailto-handler-yes-confirm",
        args: { url: aURI.prePath },
      },
      { id: "protocolhandler-mailto-handler-yes-button" },
      { id: "protocolhandler-mailto-handler-no-button" },
    ]);

    // First prompt:
    // Only shown if there is a realistic chance that we can really set the OS
    // default and can also be disabled with a preference or experiement
    if (this._canSetOSDefault(aProtocol)) {
      // Only show if not already set and if we have been properly installed
      let notificationId = "OS Protocol Registration: " + aProtocol;
      let osDefaultNotificationBox = browser
        .getTabBrowser()
        .getNotificationBox(browser);
      if (!osDefaultNotificationBox.getNotificationWithValue(notificationId)) {
        osDefaultNotificationBox.appendNotification(
          notificationId,
          {
            label: msg_os_box,
            priority: osDefaultNotificationBox.PRIORITY_INFO_LOW,
          },
          [
            {
              label: msg_os_yes,
              callback: () => {
                this._setOSDefault(aProtocol);
                Glean.protocolhandlerMailto.promptClicked.set_os_default.add();
                osDefaultNotificationBox.appendNotification(
                  notificationId,
                  {
                    label: msg_os_yes_confirm,
                    priority: osDefaultNotificationBox.PRIORITY_INFO_LOW,
                  },
                  []
                );
                return false;
              },
            },
            {
              label: msg_os_no,
              callback: () => {
                Services.prefs.setBoolPref("browser.mailto.prompt.os", false);
                Glean.protocolhandlerMailto.promptClicked.dismiss_os_default.add();
                return false;
              },
            },
          ]
        );

        Glean.protocolhandlerMailto.handlerPromptShown.os_default.add();
      }
    }

    // Second prompt:
    // Only shown if the protocol handler is not already registered
    if (!this._protocolHandlerRegistered(aProtocol, aURI.spec)) {
      let notificationId = "Protocol Registration: " + aProtocol;
      let FxDefaultNotificationBox = browser
        .getTabBrowser()
        .getNotificationBox(browser);
      if (!FxDefaultNotificationBox.getNotificationWithValue(notificationId)) {
        FxDefaultNotificationBox.appendNotification(
          notificationId,
          {
            label: msg_box,
            priority: FxDefaultNotificationBox.PRIORITY_INFO_LOW,
          },
          [
            {
              label: msg_yes,
              callback: () => {
                this._setLocalDefault(
                  aProtocol,
                  this._addLocal(aProtocol, aTitle, aURI.spec)
                );
                Glean.protocolhandlerMailto.promptClicked.set_local_default.add();
                FxDefaultNotificationBox.appendNotification(
                  notificationId,
                  {
                    label: msg_yes_confirm,
                    priority: FxDefaultNotificationBox.PRIORITY_INFO_LOW,
                  },
                  []
                );
                return false;
              },
            },
            {
              label: msg_no,
              callback: () => {
                Glean.protocolhandlerMailto.promptClicked.dismiss_local_default.add();
                return false;
              },
            },
          ]
        );

        Glean.protocolhandlerMailto.handlerPromptShown.fx_default.add();
      }
    }
  },

  /**
   * See nsISupports
   */
  QueryInterface: ChromeUtils.generateQI(["nsIWebProtocolHandlerRegistrar"]),
};
