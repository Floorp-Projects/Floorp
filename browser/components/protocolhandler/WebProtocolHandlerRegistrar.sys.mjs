/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const STRING_BUNDLE_URI = "chrome://browser/locale/feeds/subscribe.properties";

export function WebProtocolHandlerRegistrar() {}

const lazy = {};

XPCOMUtils.defineLazyServiceGetters(lazy, {
  ExternalProtocolService: [
    "@mozilla.org/uriloader/external-protocol-service;1",
    "nsIExternalProtocolService",
  ],
});

ChromeUtils.defineESModuleGetters(lazy, {
  NimbusFeatures: "resource://nimbus/ExperimentAPI.sys.mjs",
  PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
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

  /* Because we want to iterate over the known webmailers in the observe method
   * and with each site visited, we want to check as fast as possible if the
   * current site is already registered as a mailto handler. Using the sites
   * domain name as a key ensures that we can use Map.has(...) later to find
   * it.
   */
  _addedObservers: 0,
  _knownWebmailerCache: new Map(),
  _ensureWebmailerCache() {
    this._knownWebmailerCache = new Map();

    const handler =
      lazy.ExternalProtocolService.getProtocolHandlerInfo("mailto");

    for (const h of handler.possibleApplicationHandlers.enumerate()) {
      // Services.io.newURI could fail for broken handlers in which case we
      // simply leave them out, but write a debug message (just in case)
      try {
        if (h instanceof Ci.nsIWebHandlerApp && h.uriTemplate) {
          const mailerUri = Services.io.newURI(h.uriTemplate);
          if (mailerUri.scheme == "https") {
            this._knownWebmailerCache.set(
              Services.io.newURI(h.uriTemplate).host,
              {
                uriPath: Services.io.newURI(h.uriTemplate).resolve("."),
                uriTemplate: Services.io.newURI(h.uriTemplate),
                name: h.name,
              }
            );
          }
        }
      } catch (e) {
        lazy.log.debug(`Could not add ${h.uriTemplate} to cache: ${e.message}`);
      }
    }
  },

  /* This function can be called multiple times and (re-)initializes the cache
   * if the feature is toggled on. If called with the feature off it will also
   * unregister the observers.
   *
   * @param {boolean} firstInit
   *
   */
  init(firstInit = false) {
    if (firstInit) {
      lazy.NimbusFeatures.mailto.onUpdate(() =>
        // make firstInit explicitly false to avoid multiple registrations.
        this.init(false)
      );
    }

    const observers = ["mailto::onLocationChange", "mailto::onClearCache"];
    if (
      lazy.NimbusFeatures.mailto.getVariable("dualPrompt") &&
      lazy.NimbusFeatures.mailto.getVariable("dualPrompt.onLocationChange")
    ) {
      this._ensureWebmailerCache();
      observers.forEach(o => {
        this._addedObservers++;
        Services.obs.addObserver(this, o);
      });
      lazy.log.debug(`mailto observers activated: [${observers}]`);
    } else {
      // With `dualPrompt` and `dualPrompt.onLocationChange` toggled on we get
      // up to two notifications when we turn the feature off again, but we
      // only want to unregister the observers once.
      //
      // Using `hasObservers` would allow us to loop over all observers as long
      // as there are more, but hasObservers is not implemented hence why we
      // use `enumerateObservers` here to create the loop and `hasMoreElements`
      // to return true or false as `hasObservers` would if it existed.
      observers.forEach(o => {
        if (
          0 < this._addedObservers &&
          Services.obs.enumerateObservers(o).hasMoreElements()
        ) {
          Services.obs.removeObserver(this, o);
          this._addedObservers--;
          lazy.log.debug(`mailto observer "${o}" deactivated.`);
        }
      });
    }
  },

  async observe(browser, topic) {
    try {
      switch (topic) {
        case "mailto::onLocationChange": {
          // registerProtocolHandler only works for https
          const uri = browser.currentURI;
          if (!uri.schemeIs("https")) {
            return;
          }

          const host = browser.currentURI.host;
          if (this._knownWebmailerCache.has(host)) {
            // second: search the cache for an entry which starts with the path
            // of the current uri. If it exists we identified the current page as
            // webmailer (again).
            const value = this._knownWebmailerCache.get(host);
            await this._askUserToSetMailtoHandler(
              browser,
              "mailto",
              value.uriTemplate,
              value.name
            );
          }
          break; // the switch(topic) statement
        }
        case "mailto::onClearCache":
          // clear the cache for now. We could try to dynamically update the
          // cache, which is easy if a webmailer is added to the settings, but
          // becomes more complicated when webmailers are removed, because then
          // the store gets rewritten and we would require an event to deal with
          // that as well. So instead we recreate it entirely.
          this._ensureWebmailerCache();
          break;
        default:
          lazy.log.debug(`observe reached with unknown topic: ${topic}`);
      }
    } catch (e) {
      lazy.log.debug(`Problem in observer: ${e}`);
    }
  },

  /**
   * See nsIWebProtocolHandlerRegistrar
   */
  removeProtocolHandler(aProtocol, aURITemplate) {
    let handlerInfo =
      lazy.ExternalProtocolService.getProtocolHandlerInfo(aProtocol);
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
    let handlerInfo =
      lazy.ExternalProtocolService.getProtocolHandlerInfo(aProtocol);
    let handlers = handlerInfo.possibleApplicationHandlers;
    for (let handler of handlers.enumerate()) {
      // We only want to test web handlers
      if (
        handler instanceof Ci.nsIWebHandlerApp &&
        handler.uriTemplate == aURITemplate
      ) {
        return true;
      }
    }
    return false;
  },

  /**
   * Returns true if aURITemplate.spec points to the currently configured
   * handler for aProtocol links and the OS default is also readily configured.
   * Returns false if some of it can be made default.
   *
   * @param {string} aProtocol
   *        The scheme of the web handler we are checking for.
   * @param {string} aURITemplate
   *        The URI template that the handler uses to handle the protocol.
   */
  _isProtocolHandlerDefault(aProtocol, aURITemplate) {
    const handlerInfo =
      lazy.ExternalProtocolService.getProtocolHandlerInfo(aProtocol);

    if (
      handlerInfo.preferredAction == Ci.nsIHandlerInfo.useHelperApp &&
      handlerInfo.preferredApplicationHandler instanceof Ci.nsIWebHandlerApp
    ) {
      let webHandlerApp =
        handlerInfo.preferredApplicationHandler.QueryInterface(
          Ci.nsIWebHandlerApp
        );

      // If we are already configured as default, we cannot set a new default
      // and if the current site is already registered as default webmailer we
      // are fully set up as the default app for webmail.
      if (
        !this._canSetOSDefault(aProtocol) &&
        webHandlerApp.uriTemplate == aURITemplate.spec
      ) {
        return true;
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
   * Private method to set the default uri to handle a certain protocol. This
   * automates in a way what a user can do in settings under applications,
   * where different 'actions' can be chosen for different 'content types'.
   *
   * @param {string} protocol
   * @param {handler} handler
   */
  _setProtocolHandlerDefault(protocol, handler) {
    let handlerInfo =
      lazy.ExternalProtocolService.getProtocolHandlerInfo(protocol);
    handlerInfo.preferredAction = Ci.nsIHandlerInfo.useHelperApp;
    handlerInfo.preferredApplicationHandler = handler;
    handlerInfo.alwaysAskBeforeHandling = false;
    let hs = Cc["@mozilla.org/uriloader/handler-service;1"].getService(
      Ci.nsIHandlerService
    );
    hs.store(handlerInfo);
    return handlerInfo;
  },

  /**
   * Private method to add a ProtocolHandler of type nsIWebHandlerApp to the
   * list of possible handlers for a protocol.
   *
   * @param {string} protocol - e.g. 'mailto', so again without ://
   * @param {string} name - the protocol associated 'Action'
   * @param {string} uri - the uri (compare 'use other...' in the preferences)
   * @returns {handler} handler - either the existing one or a newly created
   */
  _addWebProtocolHandler(protocol, name, uri) {
    let phi = lazy.ExternalProtocolService.getProtocolHandlerInfo(protocol);
    // not adding duplicates and bail out with the existing entry
    for (let h of phi.possibleApplicationHandlers.enumerate()) {
      if (h instanceof Ci.nsIWebHandlerApp && h.uriTemplate === uri) {
        return h;
      }
    }

    let handler = Cc["@mozilla.org/uriloader/web-handler-app;1"].createInstance(
      Ci.nsIWebHandlerApp
    );
    handler.name = name;
    handler.uriTemplate = uri;

    let handlerInfo =
      lazy.ExternalProtocolService.getProtocolHandlerInfo(protocol);
    handlerInfo.possibleApplicationHandlers.appendElement(handler);

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

  /* function to look up for a specific stored (in site specific settings)
   * timestamp and check if it is now older as again_after_minutes.
   *
   * @param {string} group
   *        site specific setting group (e.g. domain)
   * @param {string} setting_name
   *        site specific settings name (e.g. `last_dimissed_ts`)
   * @param {number} again_after_minutes
   *        length in minutes of time difference allowed between setting and now
   * @returns {boolean}
   *        true: within again_after_minutes
   *        false: older than again_after_minutes
   */
  async _isStillDismissed(group, setting_name, again_after_minutes = 0) {
    let lastDismiss = await this._getSiteSpecificSetting(group, setting_name);

    // first: if we find such a value, then the user has already been
    // interactive with the page
    if (lastDismiss) {
      let timeNow = new Date().getTime();
      let minutesAgo = (timeNow - lastDismiss) / 1000 / 60;
      if (minutesAgo < again_after_minutes) {
        lazy.log.debug(
          `prompt not shown- a site with setting_name '${setting_name}'` +
            ` was last dismissed ${minutesAgo.toFixed(2)} minutes ago and` +
            ` will only be shown again after ${again_after_minutes} minutes.`
        );
        return true;
      }
    } else {
      lazy.log.debug(`no such setting: '${setting_name}'`);
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

        let handlerInfo =
          lazy.ExternalProtocolService.getProtocolHandlerInfo(protocol);
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
    let notificationId = "OS Protocol Registration: " + aProtocol;

    // guard: we do not want to reconfigure settings in private browsing mode
    if (lazy.PrivateBrowsingUtils.isWindowPrivate(browser.ownerGlobal)) {
      lazy.log.debug("prompt not shown, because this is a private window.");
      return;
    }

    // guard: check if everything has been configured to use the current site
    // as default webmailer and bail out if so.
    if (this._isProtocolHandlerDefault(aProtocol, aURI)) {
      lazy.log.debug(
        `prompt not shown, because ${aTitle} is already configured to` +
          ` handle ${aProtocol}-links under ${aURI.spec}.`
      );
      return;
    }

    // guard: bail out if this site has been dismissed before (either by
    // clicking the 'x' button or the 'not now' button.
    const pathHash = lazy.PlacesUtils.md5(aURI.spec, { format: "hex" });
    const aSettingGroup = aURI.host + `/${pathHash}`;

    const mailtoSiteSpecificNotNow = `dismissed`;
    const mailtoSiteSpecificXClick = `xclicked`;

    if (
      await this._isStillDismissed(
        aSettingGroup,
        mailtoSiteSpecificNotNow,
        lazy.NimbusFeatures.mailto.getVariable(
          "dualPrompt.dismissNotNowMinutes"
        )
      )
    ) {
      return;
    }

    if (
      await this._isStillDismissed(
        aSettingGroup,
        mailtoSiteSpecificXClick,
        lazy.NimbusFeatures.mailto.getVariable(
          "dualPrompt.dismissXClickMinutes"
        )
      )
    ) {
      return;
    }

    // Now show the prompt if there is not already one...
    let osDefaultNotificationBox = browser
      .getTabBrowser()
      .getNotificationBox(browser);

    if (!osDefaultNotificationBox.getNotificationWithValue(notificationId)) {
      let win = browser.ownerGlobal;
      win.MozXULElement.insertFTLIfNeeded("browser/webProtocolHandler.ftl");

      let notification = await osDefaultNotificationBox.appendNotification(
        notificationId,
        {
          label: {
            "l10n-id": "protocolhandler-mailto-handler-set",
            "l10n-args": { url: aURI.host },
          },
          priority: osDefaultNotificationBox.PRIORITY_INFO_LOW,
          eventCallback: eventType => {
            // after a click on 'X' save todays date, so that we can show the
            // bar again tomorrow...
            if (eventType === "dismissed") {
              this._saveSiteSpecificSetting(
                aSettingGroup,
                mailtoSiteSpecificXClick,
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
              let currentHandler = this._addWebProtocolHandler(
                aProtocol,
                aTitle,
                aURI.spec
              );
              this._setProtocolHandlerDefault(aProtocol, currentHandler);
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
                    "protocolhandler-mailto-handler-confirm";
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
                aSettingGroup,
                mailtoSiteSpecificNotNow,
                new Date().getTime()
              );
              return false;
            },
          },
        ]
      );

      Glean.protocolhandlerMailto.handlerPromptShown.os_default.add();
      // remove the icon from the infobar, which is automatically assigned
      // after its priority, because the priority is also an indicator which
      // type of bar it is, e.g. a warning or error:
      notification.setAttribute("type", "system");
    }
  },

  /**
   * See nsISupports
   */
  QueryInterface: ChromeUtils.generateQI(["nsIWebProtocolHandlerRegistrar"]),
};
