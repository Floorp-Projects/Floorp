/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const STRING_BUNDLE_URI = "chrome://browser/locale/feeds/subscribe.properties";

export function WebProtocolHandlerRegistrar() {}

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  NimbusFeatures: "resource://nimbus/ExperimentAPI.sys.mjs",
  PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
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
        lazy.log.debug("no protocolhandler registered, because: " + e.message);
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

  /* Private method to check if we are already the default protocolhandler
   * for `protocol`.
   *
   * @param {string} protocol name, e.g. mailto (without ://)
   * @returns {boolean}
   */
  _isOsDefault(protocol) {
    let shellService = Cc[
      "@mozilla.org/browser/shell-service;1"
    ].createInstance(Ci.nsIWindowsShellService);

    if (shellService.isDefaultHandlerFor(protocol)) {
      lazy.log.debug("_isOsDefault returns true.");
      return true;
    }

    lazy.log.debug("_isOsDefault returns false.");
    return false;
  },

  /**
   * Private method to determine if we can set a new OS default for a certain
   * protocol.
   *
   * @param {string} protocol name, e.g. mailto (without ://)
   * @returns {boolean}
   */
  _canSetOSDefault(protocol) {
    // an installHash is required for the association with a scheme handler,
    // also see _setOSDefault()
    if ("" == this._getInstallHash()) {
      lazy.log.debug("_canSetOSDefault returns false.");
      return false;
    }

    if (this._isOsDefault(protocol)) {
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
   * Private method, which returns true only if the OS default handler for
   * aProtocol is us and the configured mailto handler for us is aURI.spec.
   *
   * @param {string} aProtocol
   * @param {nsIURI} aURI
   * @returns {boolean}
   */
  _isOsAndLocalDefault(aProtocol, aURI, aTitle) {
    let eps = Cc[
      "@mozilla.org/uriloader/external-protocol-service;1"
    ].getService(Ci.nsIExternalProtocolService);

    let pah = eps.getProtocolHandlerInfo(aProtocol).preferredApplicationHandler;

    // that means that always ask is configure or at least no web handler, so
    // the answer if this is site/aURI.spec a local default is no/false.
    if (!pah) {
      return false;
    }

    let webHandlerApp = pah.QueryInterface(Ci.nsIWebHandlerApp);
    if (
      webHandlerApp.uriTemplate != aURI.spec ||
      webHandlerApp.name != aTitle
    ) {
      return false;
    }

    // If the protocol handler is already registered
    if (!this._protocolHandlerRegistered(aProtocol, aURI.spec)) {
      return false;
    }

    return true;
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

  /*
   * Function to store a value associated to a domain using the content pref
   * service.
   *
   * @param {string} domain: the domain for this setting
   * @param {string} setting: the name of the setting
   * @param {string} value: the actual setting to be stored
   * @param {string} context (optional): private window or not
   * @returns {string} the stored preference (see: nsIContentPrefService2.idl)
   */
  async _saveSiteSpecificSetting(domain, setting, value, context = null) {
    const gContentPrefs = Cc["@mozilla.org/content-pref/service;1"].getService(
      Ci.nsIContentPrefService2
    );

    return new Promise((resolve, reject) => {
      gContentPrefs.set(domain, setting, value, context, {
        handleResult(pref) {
          resolve(pref);
        },
        handleCompletion() {},
        handleError(err) {
          reject(err);
        },
      });
    });
  },

  /*
   * Function to return a stored value from the content pref service. Returns
   * a promise, so await can be used to synchonize the retrieval.
   *
   * @param {string} domain: the domain for this setting
   * @param {string} setting: the name of the setting
   * @param {string} context (optional): private window or not
   * @param {string} def (optional): the default value to return
   * @returns {string} either stored value or ""
   */
  async _getSiteSpecificSetting(domain, setting, context = null, def = null) {
    const gContentPrefs = Cc["@mozilla.org/content-pref/service;1"].getService(
      Ci.nsIContentPrefService2
    );

    return await new Promise((resolve, reject) => {
      gContentPrefs.getByDomainAndName(domain, setting, context, {
        _result: def,
        handleResult(pref) {
          this._result = pref.value;
        },
        handleCompletion(_) {
          resolve(this._result);
        },
        handleError(err) {
          reject(err);
        },
      });
    });
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
      primary: true,

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
   * Special implementation for mailto: A prompt (notificationbox.js) is only
   * shown if there is a realistic chance that we can really set the OS default,
   * e.g. if we have been properly installed and the current page is not already
   * the default and we have not asked users too often the same question.
   *
   * @param {string} browser
   * @param {string} aProtocol
   * @param {nsIURI} aURI
   * @param {string} aTitle
   */
  async _askUserToSetMailtoHandler(browser, aProtocol, aURI, aTitle) {
    let currentHandler = this._addLocal(aProtocol, aTitle, aURI.spec);
    let notificationId = "OS Protocol Registration: " + aProtocol;

    // guard: if we have shown the bar before and it was dismissed: do not show
    // it again. The pathHash is used to secure the URL by limiting this user
    // input to a well-defined character set with a fixed length before it gets
    // written to the underlaying sqlite database.
    const gMailtoSiteSpecificDismiss = "protocolhandler.mailto.pathHash";
    let pathHash = lazy.PlacesUtils.md5(aURI.spec, { format: "hex" });
    let lastHash = await this._getSiteSpecificSetting(
      aURI.host,
      gMailtoSiteSpecificDismiss
    );
    if (pathHash == lastHash) {
      lazy.log.debug(
        "prompt not shown, because a site with the pathHash " +
          pathHash +
          " was dismissed before."
      );
      return;
    }

    // guard: do not show the same bar twice on a single day after dismissed
    // with 'X'
    const gMailtoSiteSpecificXClick = "protocolhandler.mailto.xclickdate";
    let lastShown = await this._getSiteSpecificSetting(
      aURI.host,
      gMailtoSiteSpecificXClick,
      null,
      0
    );
    let currentTS = new Date().getTime();
    let timeRemaining = 24 * 60 * 60 * 1000 - (currentTS - lastShown);
    if (0 < timeRemaining) {
      lazy.log.debug(
        "prompt will only be shown again in " + timeRemaining + " ms."
      );
      return;
    }

    // guard: bail out if already configured as default...
    if (this._isOsAndLocalDefault(aProtocol, aURI, aTitle)) {
      lazy.log.debug(
        "prompt not shown, because " +
          aTitle +
          " is already configured" +
          " to handle " +
          aProtocol +
          "-links under " +
          aURI.spec +
          " and we are already configured to be the OS default handler."
      );
      return;
    }

    let osDefaultNotificationBox = browser
      .getTabBrowser()
      .getNotificationBox(browser);

    if (!osDefaultNotificationBox.getNotificationWithValue(notificationId)) {
      let win = browser.ownerGlobal;
      win.MozXULElement.insertFTLIfNeeded("branding/brand.ftl");
      win.MozXULElement.insertFTLIfNeeded("browser/webProtocolHandler.ftl");

      let notification = await osDefaultNotificationBox.appendNotification(
        notificationId,
        {
          label: {
            "l10n-id": "protocolhandler-mailto-handler-set-message",
            "l10n-args": { url: aURI.host },
          },
          priority: osDefaultNotificationBox.PRIORITY_INFO_LOW,
          eventCallback: eventType => {
            // after a click on 'X' save todays date, so that we can show the
            // bar again tomorrow...
            if (eventType === "dismissed") {
              this._saveSiteSpecificSetting(
                aURI.host,
                gMailtoSiteSpecificXClick,
                new Date().getTime()
              );
            }
          },
        },
        [
          {
            "l10n-id": "protocolhandler-mailto-os-handler-yes-button",
            primary: true,
            callback: newitem => {
              this._setLocalDefault(aProtocol, currentHandler);
              Glean.protocolhandlerMailto.promptClicked.set_local_default.add();

              if (this._canSetOSDefault(aProtocol)) {
                if (this._setOSDefault(aProtocol)) {
                  Glean.protocolhandlerMailto.promptClicked.set_os_default.add();
                  Services.telemetry.keyedScalarSet(
                    "os.environment.is_default_handler",
                    "mailto",
                    true
                  );
                  newitem.messageL10nId =
                    "protocolhandler-mailto-handler-confirm-message";
                  newitem.removeChild(newitem.buttonContainer);
                  newitem.setAttribute("type", "success"); // from moz-message-bar.css
                  newitem.eventCallback = null; // disable show only once per day for success
                  return true; // `true` does not hide the bar
                }

                // if anything goes wrong with setting the OS default, we want
                // to be informed so that we can fix it.
                Glean.protocolhandlerMailto.promptClicked.set_os_default_error.add();
                Services.telemetry.keyedScalarSet(
                  "os.environment.is_default_handler",
                  "mailto",
                  false
                );
                return false;
              }

              // if the installation does not have an install hash, we cannot
              // set the OS default, but mailto links from within the browser
              // should still work.
              Glean.protocolhandlerMailto.promptClicked.set_os_default_impossible.add();
              return false;
            },
          },
          {
            "l10n-id": "protocolhandler-mailto-os-handler-no-button",
            callback: () => {
              this._saveSiteSpecificSetting(
                aURI.host,
                gMailtoSiteSpecificDismiss,
                pathHash
              );
              return false;
            },
          },
        ]
      );

      // remove the icon from the infobar, which is automatically assigned
      // after its priority, because the priority is also an indicator which
      // type of bar it is, e.g. a warning or error:
      notification.setAttribute("type", "system");

      Glean.protocolhandlerMailto.handlerPromptShown.os_default.add();
    }
  },

  /**
   * See nsISupports
   */
  QueryInterface: ChromeUtils.generateQI(["nsIWebProtocolHandlerRegistrar"]),
};
