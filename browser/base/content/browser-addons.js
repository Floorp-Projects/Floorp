# -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Removes a doorhanger notification if all of the installs it was notifying
// about have ended in some way.
function removeNotificationOnEnd(notification, installs) {
  let count = installs.length;

  function maybeRemove(install) {
    install.removeListener(this);

    if (--count == 0) {
      // Check that the notification is still showing
      let current = PopupNotifications.getNotification(notification.id, notification.browser);
      if (current === notification)
        notification.remove();
    }
  }

  for (let install of installs) {
    install.addListener({
      onDownloadCancelled: maybeRemove,
      onDownloadFailed: maybeRemove,
      onInstallFailed: maybeRemove,
      onInstallEnded: maybeRemove
    });
  }
}

const gXPInstallObserver = {
  _findChildShell: function (aDocShell, aSoughtShell)
  {
    if (aDocShell == aSoughtShell)
      return aDocShell;

    var node = aDocShell.QueryInterface(Components.interfaces.nsIDocShellTreeItem);
    for (var i = 0; i < node.childCount; ++i) {
      var docShell = node.getChildAt(i);
      docShell = this._findChildShell(docShell, aSoughtShell);
      if (docShell == aSoughtShell)
        return docShell;
    }
    return null;
  },

  _getBrowser: function (aDocShell)
  {
    for (let browser of gBrowser.browsers) {
      if (this._findChildShell(browser.docShell, aDocShell))
        return browser;
    }
    return null;
  },

  pendingInstalls: new WeakMap(),

  showInstallConfirmation: function(browser, installInfo, height = undefined) {
    // If the confirmation notification is already open cache the installInfo
    // and the new confirmation will be shown later
    if (PopupNotifications.getNotification("addon-install-confirmation", browser)) {
      let pending = this.pendingInstalls.get(browser);
      if (pending) {
        pending.push(installInfo);
      } else {
        this.pendingInstalls.set(browser, [installInfo]);
      }
      return;
    }

    let showNextConfirmation = () => {
      // Make sure the browser is still alive.
      if (gBrowser.browsers.indexOf(browser) == -1)
        return;

      let pending = this.pendingInstalls.get(browser);
      if (pending && pending.length)
        this.showInstallConfirmation(browser, pending.shift());
    }

    // If all installs have already been cancelled in some way then just show
    // the next confirmation
    if (installInfo.installs.every(i => i.state != AddonManager.STATE_DOWNLOADED)) {
      showNextConfirmation();
      return;
    }

    const anchorID = "addons-notification-icon";

    // Make notifications persist a minimum of 30 seconds
    var options = {
      displayURI: installInfo.originatingURI,
      timeout: Date.now() + 30000,
    };

    let cancelInstallation = () => {
      if (installInfo) {
        for (let install of installInfo.installs) {
          // The notification may have been closed because the add-ons got
          // cancelled elsewhere, only try to cancel those that are still
          // pending install.
          if (install.state != AddonManager.STATE_CANCELLED)
            install.cancel();
        }
      }

      this.acceptInstallation = null;

      showNextConfirmation();
    };

    let unsigned = installInfo.installs.filter(i => i.addon.signedState <= AddonManager.SIGNEDSTATE_MISSING);
    let someUnsigned = unsigned.length > 0 && unsigned.length < installInfo.installs.length;

    options.eventCallback = (aEvent) => {
      switch (aEvent) {
        case "removed":
          cancelInstallation();
          break;
        case "shown":
          let addonList = document.getElementById("addon-install-confirmation-content");
          while (addonList.firstChild)
            addonList.firstChild.remove();

          for (let install of installInfo.installs) {
            let container = document.createElement("hbox");

            let name = document.createElement("label");
            name.setAttribute("value", install.addon.name);
            name.setAttribute("class", "addon-install-confirmation-name");
            container.appendChild(name);

            if (someUnsigned && install.addon.signedState <= AddonManager.SIGNEDSTATE_MISSING) {
              let unsigned = document.createElement("label");
              unsigned.setAttribute("value", gNavigatorBundle.getString("addonInstall.unsigned"));
              unsigned.setAttribute("class", "addon-install-confirmation-unsigned");
              container.appendChild(unsigned);
            }

            addonList.appendChild(container);
          }

          this.acceptInstallation = () => {
            for (let install of installInfo.installs)
              install.install();
            installInfo = null;

            Services.telemetry
                    .getHistogramById("SECURITY_UI")
                    .add(Ci.nsISecurityUITelemetry.WARNING_CONFIRM_ADDON_INSTALL_CLICK_THROUGH);
          };
          break;
      }
    };

    options.learnMoreURL = Services.urlFormatter.formatURLPref("app.support.baseURL");

    let messageString;
    let notification = document.getElementById("addon-install-confirmation-notification");
    if (unsigned.length == installInfo.installs.length) {
      // None of the add-ons are verified
      messageString = gNavigatorBundle.getString("addonConfirmInstallUnsigned.message");
      notification.setAttribute("warning", "true");
      options.learnMoreURL += "unsigned-addons";
    }
    else if (unsigned.length == 0) {
      // All add-ons are verified or don't need to be verified
      messageString = gNavigatorBundle.getString("addonConfirmInstall.message");
      notification.removeAttribute("warning");
      options.learnMoreURL += "find-and-install-add-ons";
    }
    else {
      // Some of the add-ons are unverified, the list of names will indicate
      // which
      messageString = gNavigatorBundle.getString("addonConfirmInstallSomeUnsigned.message");
      notification.setAttribute("warning", "true");
      options.learnMoreURL += "unsigned-addons";
    }

    let brandBundle = document.getElementById("bundle_brand");
    let brandShortName = brandBundle.getString("brandShortName");

    messageString = PluralForm.get(installInfo.installs.length, messageString);
    messageString = messageString.replace("#1", brandShortName);
    messageString = messageString.replace("#2", installInfo.installs.length);

    let cancelButton = document.getElementById("addon-install-confirmation-cancel");
    cancelButton.label = gNavigatorBundle.getString("addonInstall.cancelButton.label");
    cancelButton.accessKey = gNavigatorBundle.getString("addonInstall.cancelButton.accesskey");

    let acceptButton = document.getElementById("addon-install-confirmation-accept");
    acceptButton.label = gNavigatorBundle.getString("addonInstall.acceptButton.label");
    acceptButton.accessKey = gNavigatorBundle.getString("addonInstall.acceptButton.accesskey");

    if (height) {
      let notification = document.getElementById("addon-install-confirmation-notification");
      notification.style.minHeight = height + "px";
    }

    let tab = gBrowser.getTabForBrowser(browser);
    if (tab) {
      gBrowser.selectedTab = tab;
    }

    let popup = PopupNotifications.show(browser, "addon-install-confirmation",
                                        messageString, anchorID, null, null,
                                        options);

    removeNotificationOnEnd(popup, installInfo.installs);

    Services.telemetry
            .getHistogramById("SECURITY_UI")
            .add(Ci.nsISecurityUITelemetry.WARNING_CONFIRM_ADDON_INSTALL);
  },

  observe: function (aSubject, aTopic, aData)
  {
    var brandBundle = document.getElementById("bundle_brand");
    var installInfo = aSubject.QueryInterface(Components.interfaces.amIWebInstallInfo);
    var browser = installInfo.browser;

    // Make sure the browser is still alive.
    if (!browser || gBrowser.browsers.indexOf(browser) == -1)
      return;

    const anchorID = "addons-notification-icon";
    var messageString, action;
    var brandShortName = brandBundle.getString("brandShortName");

    var notificationID = aTopic;
    // Make notifications persist a minimum of 30 seconds
    var options = {
      displayURI: installInfo.originatingURI,
      timeout: Date.now() + 30000,
    };

    switch (aTopic) {
    case "addon-install-disabled": {
      notificationID = "xpinstall-disabled";

      if (gPrefService.prefIsLocked("xpinstall.enabled")) {
        messageString = gNavigatorBundle.getString("xpinstallDisabledMessageLocked");
        buttons = [];
      }
      else {
        messageString = gNavigatorBundle.getString("xpinstallDisabledMessage");

        action = {
          label: gNavigatorBundle.getString("xpinstallDisabledButton"),
          accessKey: gNavigatorBundle.getString("xpinstallDisabledButton.accesskey"),
          callback: function editPrefs() {
            gPrefService.setBoolPref("xpinstall.enabled", true);
          }
        };
      }

      PopupNotifications.show(browser, notificationID, messageString, anchorID,
                              action, null, options);
      break; }
    case "addon-install-origin-blocked": {
      messageString = gNavigatorBundle.getFormattedString("xpinstallPromptMessage",
                        [brandShortName]);

      let secHistogram = Components.classes["@mozilla.org/base/telemetry;1"].getService(Ci.nsITelemetry).getHistogramById("SECURITY_UI");
      secHistogram.add(Ci.nsISecurityUITelemetry.WARNING_ADDON_ASKING_PREVENTED);
      let popup = PopupNotifications.show(browser, notificationID,
                                          messageString, anchorID,
                                          null, null, options);
      removeNotificationOnEnd(popup, installInfo.installs);
      break; }
    case "addon-install-blocked": {
      messageString = gNavigatorBundle.getFormattedString("xpinstallPromptMessage",
                        [brandShortName]);

      let secHistogram = Components.classes["@mozilla.org/base/telemetry;1"].getService(Ci.nsITelemetry).getHistogramById("SECURITY_UI");
      action = {
        label: gNavigatorBundle.getString("xpinstallPromptAllowButton"),
        accessKey: gNavigatorBundle.getString("xpinstallPromptAllowButton.accesskey"),
        callback: function() {
          secHistogram.add(Ci.nsISecurityUITelemetry.WARNING_ADDON_ASKING_PREVENTED_CLICK_THROUGH);
          installInfo.install();
        }
      };

      secHistogram.add(Ci.nsISecurityUITelemetry.WARNING_ADDON_ASKING_PREVENTED);
      let popup = PopupNotifications.show(browser, notificationID,
                                          messageString, anchorID,
                                          action, null, options);
      removeNotificationOnEnd(popup, installInfo.installs);
      break; }
    case "addon-install-started": {
      let needsDownload = function needsDownload(aInstall) {
        return aInstall.state != AddonManager.STATE_DOWNLOADED;
      }
      // If all installs have already been downloaded then there is no need to
      // show the download progress
      if (!installInfo.installs.some(needsDownload))
        return;
      notificationID = "addon-progress";
      messageString = gNavigatorBundle.getString("addonDownloadingAndVerifying");
      messageString = PluralForm.get(installInfo.installs.length, messageString);
      messageString = messageString.replace("#1", installInfo.installs.length);
      options.installs = installInfo.installs;
      options.contentWindow = browser.contentWindow;
      options.sourceURI = browser.currentURI;
      options.eventCallback = (aEvent) => {
        switch (aEvent) {
          case "removed":
            options.contentWindow = null;
            options.sourceURI = null;
            break;
        }
      };
      let notification = PopupNotifications.show(browser, notificationID, messageString,
                                                 anchorID, null, null, options);
      notification._startTime = Date.now();

      let cancelButton = document.getElementById("addon-progress-cancel");
      cancelButton.label = gNavigatorBundle.getString("addonInstall.cancelButton.label");
      cancelButton.accessKey = gNavigatorBundle.getString("addonInstall.cancelButton.accesskey");

      let acceptButton = document.getElementById("addon-progress-accept");
      if (Preferences.get("xpinstall.customConfirmationUI", false)) {
        acceptButton.label = gNavigatorBundle.getString("addonInstall.acceptButton.label");
        acceptButton.accessKey = gNavigatorBundle.getString("addonInstall.acceptButton.accesskey");
      } else {
        acceptButton.hidden = true;
      }
      break; }
    case "addon-install-failed": {
      // TODO This isn't terribly ideal for the multiple failure case
      for (let install of installInfo.installs) {
        let host;
        try {
          host  = options.displayURI.host;
        } catch (e) {
          // displayURI might be missing or 'host' might throw for non-nsStandardURL nsIURIs.
        }

        if (!host)
          host = (install.sourceURI instanceof Ci.nsIStandardURL) &&
                 install.sourceURI.host;

        let error = (host || install.error == 0) ? "addonInstallError" : "addonLocalInstallError";
        let args;
        if (install.error < 0) {
          error += install.error;
          args = [brandShortName, install.name];
        } else if (install.addon.blocklistState == Ci.nsIBlocklistService.STATE_BLOCKED) {
          error += "Blocklisted";
          args = [install.name];
        } else {
          error += "Incompatible";
          args = [brandShortName, Services.appinfo.version, install.name];
        }

        // Add Learn More link when refusing to install an unsigned add-on
        if (install.error == AddonManager.ERROR_SIGNEDSTATE_REQUIRED) {
          options.learnMoreURL = Services.urlFormatter.formatURLPref("app.support.baseURL") + "unsigned-addons";
        }

        messageString = gNavigatorBundle.getFormattedString(error, args);

        PopupNotifications.show(browser, notificationID, messageString, anchorID,
                                action, null, options);

        // Can't have multiple notifications with the same ID, so stop here.
        break;
      }
      this._removeProgressNotification(browser);
      break; }
    case "addon-install-confirmation": {
      let showNotification = () => {
        let height = undefined;

        if (PopupNotifications.isPanelOpen) {
          let rect = document.getElementById("addon-progress-notification").getBoundingClientRect();
          height = rect.height;
        }

        this._removeProgressNotification(browser);
        this.showInstallConfirmation(browser, installInfo, height);
      };

      let progressNotification = PopupNotifications.getNotification("addon-progress", browser);
      if (progressNotification) {
        let downloadDuration = Date.now() - progressNotification._startTime;
        let securityDelay = Services.prefs.getIntPref("security.dialog_enable_delay") - downloadDuration;
        if (securityDelay > 0) {
          setTimeout(() => {
            // The download may have been cancelled during the security delay
            if (PopupNotifications.getNotification("addon-progress", browser))
              showNotification();
          }, securityDelay);
          break;
        }
      }
      showNotification();
      break; }
    case "addon-install-complete": {
      let needsRestart = installInfo.installs.some(function(i) {
        return i.addon.pendingOperations != AddonManager.PENDING_NONE;
      });

      if (needsRestart) {
        notificationID = "addon-install-restart";
        messageString = gNavigatorBundle.getString("addonsInstalledNeedsRestart");
        action = {
          label: gNavigatorBundle.getString("addonInstallRestartButton"),
          accessKey: gNavigatorBundle.getString("addonInstallRestartButton.accesskey"),
          callback: function() {
            BrowserUtils.restartApplication();
          }
        };
      }
      else {
        messageString = gNavigatorBundle.getString("addonsInstalled");
        action = null;
      }

      messageString = PluralForm.get(installInfo.installs.length, messageString);
      messageString = messageString.replace("#1", installInfo.installs[0].name);
      messageString = messageString.replace("#2", installInfo.installs.length);
      messageString = messageString.replace("#3", brandShortName);

      // Remove notificaion on dismissal, since it's possible to cancel the
      // install through the addons manager UI, making the "restart" prompt
      // irrelevant.
      options.removeOnDismissal = true;

      PopupNotifications.show(browser, notificationID, messageString, anchorID,
                              action, null, options);
      break; }
    }
  },
  _removeProgressNotification(aBrowser) {
    let notification = PopupNotifications.getNotification("addon-progress", aBrowser);
    if (notification)
      notification.remove();
  }
};

var LightWeightThemeWebInstaller = {
  init: function () {
    let mm = window.messageManager;
    mm.addMessageListener("LightWeightThemeWebInstaller:Install", this);
    mm.addMessageListener("LightWeightThemeWebInstaller:Preview", this);
    mm.addMessageListener("LightWeightThemeWebInstaller:ResetPreview", this);
  },

  receiveMessage: function (message) {
    // ignore requests from background tabs
    if (message.target != gBrowser.selectedBrowser) {
      return;
    }

    let data = message.data;

    switch (message.name) {
      case "LightWeightThemeWebInstaller:Install": {
        this._installRequest(data.themeData, data.baseURI);
        break;
      }
      case "LightWeightThemeWebInstaller:Preview": {
        this._preview(data.themeData, data.baseURI);
        break;
      }
      case "LightWeightThemeWebInstaller:ResetPreview": {
        this._resetPreview(data && data.baseURI);
        break;
      }
    }
  },

  handleEvent: function (event) {
    switch (event.type) {
      case "TabSelect": {
        this._resetPreview();
        break;
      }
    }
  },

  get _manager () {
    let temp = {};
    Cu.import("resource://gre/modules/LightweightThemeManager.jsm", temp);
    delete this._manager;
    return this._manager = temp.LightweightThemeManager;
  },

  _installRequest: function (dataString, baseURI) {
    let data = this._manager.parseTheme(dataString, baseURI);

    if (!data) {
      return;
    }

    if (this._isAllowed(baseURI)) {
      this._install(data);
      return;
    }

    let allowButtonText =
      gNavigatorBundle.getString("lwthemeInstallRequest.allowButton");
    let allowButtonAccesskey =
      gNavigatorBundle.getString("lwthemeInstallRequest.allowButton.accesskey");
    let message =
      gNavigatorBundle.getFormattedString("lwthemeInstallRequest.message",
                                          [makeURI(baseURI).host]);
    let buttons = [{
      label: allowButtonText,
      accessKey: allowButtonAccesskey,
      callback: function () {
        LightWeightThemeWebInstaller._install(data);
      }
    }];

    this._removePreviousNotifications();

    let notificationBox = gBrowser.getNotificationBox();
    let notificationBar =
      notificationBox.appendNotification(message, "lwtheme-install-request", "",
                                         notificationBox.PRIORITY_INFO_MEDIUM,
                                         buttons);
    notificationBar.persistence = 1;
  },

  _install: function (newLWTheme) {
    let previousLWTheme = this._manager.currentTheme;

    let listener = {
      onEnabling: function(aAddon, aRequiresRestart) {
        if (!aRequiresRestart) {
          return;
        }

        let messageString = gNavigatorBundle.getFormattedString("lwthemeNeedsRestart.message",
          [aAddon.name], 1);

        let action = {
          label: gNavigatorBundle.getString("lwthemeNeedsRestart.button"),
          accessKey: gNavigatorBundle.getString("lwthemeNeedsRestart.accesskey"),
          callback: function () {
            BrowserUtils.restartApplication();
          }
        };

        let options = {
          timeout: Date.now() + 30000
        };

        PopupNotifications.show(gBrowser.selectedBrowser, "addon-theme-change",
                                messageString, "addons-notification-icon",
                                action, null, options);
      },

      onEnabled: function(aAddon) {
        LightWeightThemeWebInstaller._postInstallNotification(newLWTheme, previousLWTheme);
      }
    };

    AddonManager.addAddonListener(listener);
    this._manager.currentTheme = newLWTheme;
    AddonManager.removeAddonListener(listener);
  },

  _postInstallNotification: function (newTheme, previousTheme) {
    function text(id) {
      return gNavigatorBundle.getString("lwthemePostInstallNotification." + id);
    }

    let buttons = [{
      label: text("undoButton"),
      accessKey: text("undoButton.accesskey"),
      callback: function () {
        LightWeightThemeWebInstaller._manager.forgetUsedTheme(newTheme.id);
        LightWeightThemeWebInstaller._manager.currentTheme = previousTheme;
      }
    }, {
      label: text("manageButton"),
      accessKey: text("manageButton.accesskey"),
      callback: function () {
        BrowserOpenAddonsMgr("addons://list/theme");
      }
    }];

    this._removePreviousNotifications();

    let notificationBox = gBrowser.getNotificationBox();
    let notificationBar =
      notificationBox.appendNotification(text("message"),
                                         "lwtheme-install-notification", "",
                                         notificationBox.PRIORITY_INFO_MEDIUM,
                                         buttons);
    notificationBar.persistence = 1;
    notificationBar.timeout = Date.now() + 20000; // 20 seconds
  },

  _removePreviousNotifications: function () {
    let box = gBrowser.getNotificationBox();

    ["lwtheme-install-request",
     "lwtheme-install-notification"].forEach(function (value) {
        let notification = box.getNotificationWithValue(value);
        if (notification)
          box.removeNotification(notification);
      });
  },

  _preview: function (dataString, baseURI) {
    if (!this._isAllowed(baseURI))
      return;

    let data = this._manager.parseTheme(dataString, baseURI);
    if (!data)
      return;

    this._resetPreview();
    gBrowser.tabContainer.addEventListener("TabSelect", this, false);
    this._manager.previewTheme(data);
  },

  _resetPreview: function (baseURI) {
    if (baseURI && !this._isAllowed(baseURI))
      return;
    gBrowser.tabContainer.removeEventListener("TabSelect", this, false);
    this._manager.resetPreview();
  },

  _isAllowed: function (srcURIString) {
    let uri;
    try {
      uri = makeURI(srcURIString);
    }
    catch(e) {
      //makeURI fails if srcURIString is a nonsense URI
      return false;
    }

    if (!uri.schemeIs("https")) {
      return false;
    }

    let pm = Services.perms;
    return pm.testPermission(uri, "install") == pm.ALLOW_ACTION;
  }
};

/*
 * Listen for Lightweight Theme styling changes and update the browser's theme accordingly.
 */
let LightweightThemeListener = {
  _modifiedStyles: [],

  init: function () {
    XPCOMUtils.defineLazyGetter(this, "styleSheet", function() {
      for (let i = document.styleSheets.length - 1; i >= 0; i--) {
        let sheet = document.styleSheets[i];
        if (sheet.href == "chrome://browser/skin/browser-lightweightTheme.css")
          return sheet;
      }
    });

    Services.obs.addObserver(this, "lightweight-theme-styling-update", false);
    Services.obs.addObserver(this, "lightweight-theme-optimized", false);
    if (document.documentElement.hasAttribute("lwtheme"))
      this.updateStyleSheet(document.documentElement.style.backgroundImage);
  },

  uninit: function () {
    Services.obs.removeObserver(this, "lightweight-theme-styling-update");
    Services.obs.removeObserver(this, "lightweight-theme-optimized");
  },

  /**
   * Append the headerImage to the background-image property of all rulesets in
   * browser-lightweightTheme.css.
   *
   * @param headerImage - a string containing a CSS image for the lightweight theme header.
   */
  updateStyleSheet: function(headerImage) {
    if (!this.styleSheet)
      return;
    this.substituteRules(this.styleSheet.cssRules, headerImage);
  },

  substituteRules: function(ruleList, headerImage, existingStyleRulesModified = 0) {
    let styleRulesModified = 0;
    for (let i = 0; i < ruleList.length; i++) {
      let rule = ruleList[i];
      if (rule instanceof Ci.nsIDOMCSSGroupingRule) {
        // Add the number of modified sub-rules to the modified count
        styleRulesModified += this.substituteRules(rule.cssRules, headerImage, existingStyleRulesModified + styleRulesModified);
      } else if (rule instanceof Ci.nsIDOMCSSStyleRule) {
        if (!rule.style.backgroundImage)
          continue;
        let modifiedIndex = existingStyleRulesModified + styleRulesModified;
        if (!this._modifiedStyles[modifiedIndex])
          this._modifiedStyles[modifiedIndex] = { backgroundImage: rule.style.backgroundImage };

        rule.style.backgroundImage = this._modifiedStyles[modifiedIndex].backgroundImage + ", " + headerImage;
        styleRulesModified++;
      } else {
        Cu.reportError("Unsupported rule encountered");
      }
    }
    return styleRulesModified;
  },

  // nsIObserver
  observe: function (aSubject, aTopic, aData) {
    if ((aTopic != "lightweight-theme-styling-update" && aTopic != "lightweight-theme-optimized") ||
          !this.styleSheet)
      return;

    if (aTopic == "lightweight-theme-optimized" && aSubject != window)
      return;

    let themeData = JSON.parse(aData);
    if (!themeData)
      return;
    this.updateStyleSheet("url(" + themeData.headerURL + ")");
  },
};
