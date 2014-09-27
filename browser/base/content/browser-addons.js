# -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

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

  observe: function (aSubject, aTopic, aData)
  {
    var brandBundle = document.getElementById("bundle_brand");
    var installInfo = aSubject.QueryInterface(Components.interfaces.amIWebInstallInfo);
    var winOrBrowser = installInfo.originator;

    var browser;
    try {
      var shell = winOrBrowser.QueryInterface(Components.interfaces.nsIDOMWindow)
                              .QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                              .getInterface(Components.interfaces.nsIWebNavigation)
                              .QueryInterface(Components.interfaces.nsIDocShell);
      browser = this._getBrowser(shell);
    } catch (e) {
      browser = winOrBrowser;
    }
    // Note that the above try/catch will pass through dead object proxies and
    // other degenerate objects. Make sure the browser is bonafide.
    if (!browser || !gBrowser.browsers.contains(browser))
      return;

    const anchorID = "addons-notification-icon";
    var messageString, action;
    var brandShortName = brandBundle.getString("brandShortName");

    var notificationID = aTopic;
    // Make notifications persist a minimum of 30 seconds
    var options = {
      timeout: Date.now() + 30000
    };

    switch (aTopic) {
    case "addon-install-disabled":
      notificationID = "xpinstall-disabled"

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
      break;
    case "addon-install-blocked":
      let originatingHost;
      try {
        originatingHost = installInfo.originatingURI.host;
      } catch (ex) {
        // Need to deal with missing originatingURI and with about:/data: URIs more gracefully,
        // see bug 1063418 - but for now, bail:
        return;
      }
      messageString = gNavigatorBundle.getFormattedString("xpinstallPromptWarning",
                        [brandShortName, originatingHost]);

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
      PopupNotifications.show(browser, notificationID, messageString, anchorID,
                              action, null, options);
      break;
    case "addon-install-started":
      var needsDownload = function needsDownload(aInstall) {
        return aInstall.state != AddonManager.STATE_DOWNLOADED;
      }
      // If all installs have already been downloaded then there is no need to
      // show the download progress
      if (!installInfo.installs.some(needsDownload))
        return;
      notificationID = "addon-progress";
      messageString = gNavigatorBundle.getString("addonDownloading");
      messageString = PluralForm.get(installInfo.installs.length, messageString);
      options.installs = installInfo.installs;
      options.contentWindow = browser.contentWindow;
      options.sourceURI = browser.currentURI;
      options.eventCallback = function(aEvent) {
        if (aEvent != "removed")
          return;
        options.contentWindow = null;
        options.sourceURI = null;
      };
      PopupNotifications.show(browser, notificationID, messageString, anchorID,
                              null, null, options);
      break;
    case "addon-install-failed":
      // TODO This isn't terribly ideal for the multiple failure case
      for (let install of installInfo.installs) {
        let host = (installInfo.originatingURI instanceof Ci.nsIStandardURL) &&
                   installInfo.originatingURI.host;
        if (!host)
          host = (install.sourceURI instanceof Ci.nsIStandardURL) &&
                 install.sourceURI.host;

        let error = (host || install.error == 0) ? "addonError" : "addonLocalError";
        if (install.error != 0)
          error += install.error;
        else if (install.addon.blocklistState == Ci.nsIBlocklistService.STATE_BLOCKED)
          error += "Blocklisted";
        else
          error += "Incompatible";

        messageString = gNavigatorBundle.getString(error);
        messageString = messageString.replace("#1", install.name);
        if (host)
          messageString = messageString.replace("#2", host);
        messageString = messageString.replace("#3", brandShortName);
        messageString = messageString.replace("#4", Services.appinfo.version);

        PopupNotifications.show(browser, notificationID, messageString, anchorID,
                                action, null, options);
      }
      break;
    case "addon-install-complete":
      var needsRestart = installInfo.installs.some(function(i) {
        return i.addon.pendingOperations != AddonManager.PENDING_NONE;
      });

      if (needsRestart) {
        messageString = gNavigatorBundle.getString("addonsInstalledNeedsRestart");
        action = {
          label: gNavigatorBundle.getString("addonInstallRestartButton"),
          accessKey: gNavigatorBundle.getString("addonInstallRestartButton.accesskey"),
          callback: function() {
            Application.restart();
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
      break;
    }
  }
};

var LightWeightThemeWebInstaller = {
  handleEvent: function (event) {
    switch (event.type) {
      case "InstallBrowserTheme":
      case "PreviewBrowserTheme":
      case "ResetBrowserThemePreview":
        // ignore requests from background tabs
        if (event.target.ownerDocument.defaultView.top != content)
          return;
    }
    switch (event.type) {
      case "InstallBrowserTheme":
        this._installRequest(event);
        break;
      case "PreviewBrowserTheme":
        this._preview(event);
        break;
      case "ResetBrowserThemePreview":
        this._resetPreview(event);
        break;
      case "pagehide":
      case "TabSelect":
        this._resetPreview();
        break;
    }
  },

  get _manager () {
    var temp = {};
    Cu.import("resource://gre/modules/LightweightThemeManager.jsm", temp);
    delete this._manager;
    return this._manager = temp.LightweightThemeManager;
  },

  _installRequest: function (event) {
    var node = event.target;
    var data = this._getThemeFromNode(node);
    if (!data)
      return;

    if (this._isAllowed(node)) {
      this._install(data);
      return;
    }

    var allowButtonText =
      gNavigatorBundle.getString("lwthemeInstallRequest.allowButton");
    var allowButtonAccesskey =
      gNavigatorBundle.getString("lwthemeInstallRequest.allowButton.accesskey");
    var message =
      gNavigatorBundle.getFormattedString("lwthemeInstallRequest.message",
                                          [node.ownerDocument.location.host]);
    var buttons = [{
      label: allowButtonText,
      accessKey: allowButtonAccesskey,
      callback: function () {
        LightWeightThemeWebInstaller._install(data);
      }
    }];

    this._removePreviousNotifications();

    var notificationBox = gBrowser.getNotificationBox();
    var notificationBar =
      notificationBox.appendNotification(message, "lwtheme-install-request", "",
                                         notificationBox.PRIORITY_INFO_MEDIUM,
                                         buttons);
    notificationBar.persistence = 1;
  },

  _install: function (newLWTheme) {
    var previousLWTheme = this._manager.currentTheme;

    var listener = {
      onEnabling: function(aAddon, aRequiresRestart) {
        if (!aRequiresRestart)
          return;

        let messageString = gNavigatorBundle.getFormattedString("lwthemeNeedsRestart.message",
          [aAddon.name], 1);

        let action = {
          label: gNavigatorBundle.getString("lwthemeNeedsRestart.button"),
          accessKey: gNavigatorBundle.getString("lwthemeNeedsRestart.accesskey"),
          callback: function () {
            Application.restart();
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

    var buttons = [{
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

    var notificationBox = gBrowser.getNotificationBox();
    var notificationBar =
      notificationBox.appendNotification(text("message"),
                                         "lwtheme-install-notification", "",
                                         notificationBox.PRIORITY_INFO_MEDIUM,
                                         buttons);
    notificationBar.persistence = 1;
    notificationBar.timeout = Date.now() + 20000; // 20 seconds
  },

  _removePreviousNotifications: function () {
    var box = gBrowser.getNotificationBox();

    ["lwtheme-install-request",
     "lwtheme-install-notification"].forEach(function (value) {
        var notification = box.getNotificationWithValue(value);
        if (notification)
          box.removeNotification(notification);
      });
  },

  _previewWindow: null,
  _preview: function (event) {
    if (!this._isAllowed(event.target))
      return;

    var data = this._getThemeFromNode(event.target);
    if (!data)
      return;

    this._resetPreview();

    this._previewWindow = event.target.ownerDocument.defaultView;
    this._previewWindow.addEventListener("pagehide", this, true);
    gBrowser.tabContainer.addEventListener("TabSelect", this, false);

    this._manager.previewTheme(data);
  },

  _resetPreview: function (event) {
    if (!this._previewWindow ||
        event && !this._isAllowed(event.target))
      return;

    this._previewWindow.removeEventListener("pagehide", this, true);
    this._previewWindow = null;
    gBrowser.tabContainer.removeEventListener("TabSelect", this, false);

    this._manager.resetPreview();
  },

  _isAllowed: function (node) {
    var pm = Services.perms;

    var uri = node.ownerDocument.documentURIObject;
    return pm.testPermission(uri, "install") == pm.ALLOW_ACTION;
  },

  _getThemeFromNode: function (node) {
    return this._manager.parseTheme(node.getAttribute("data-browsertheme"),
                                    node.baseURI);
  }
}

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
