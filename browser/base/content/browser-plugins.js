/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var gPluginHandler = {
  PREF_SESSION_PERSIST_MINUTES: "plugin.sessionPermissionNow.intervalInMinutes",
  PREF_PERSISTENT_DAYS: "plugin.persistentPermissionAlways.intervalInDays",
  PREF_SHOW_INFOBAR: "plugins.show_infobar",
  PREF_INFOBAR_DISMISSAL_PERMANENT: "plugins.remember_infobar_dismissal",

  MESSAGES: [
    "PluginContent:ShowClickToPlayNotification",
    "PluginContent:RemoveNotification",
    "PluginContent:UpdateHiddenPluginUI",
    "PluginContent:HideNotificationBar",
    "PluginContent:InstallSinglePlugin",
    "PluginContent:ShowPluginCrashedNotification",
    "PluginContent:SubmitReport",
    "PluginContent:LinkClickCallback",
  ],

  init() {
    const mm = window.messageManager;
    for (let msg of this.MESSAGES) {
      mm.addMessageListener(msg, this);
    }
    window.addEventListener("unload", this);
  },

  uninit() {
    const mm = window.messageManager;
    for (let msg of this.MESSAGES) {
      mm.removeMessageListener(msg, this);
    }
    window.removeEventListener("unload", this);
  },

  handleEvent(event) {
    if (event.type == "unload") {
      this.uninit();
    }
  },

  receiveMessage(msg) {
    switch (msg.name) {
      case "PluginContent:ShowClickToPlayNotification":
        this.showClickToPlayNotification(msg.target, msg.data.plugins, msg.data.showNow,
                                         msg.principal, msg.data.location);
        break;
      case "PluginContent:RemoveNotification":
        this.removeNotification(msg.target, msg.data.name);
        break;
      case "PluginContent:UpdateHiddenPluginUI":
        this.updateHiddenPluginUI(msg.target, msg.data.haveInsecure, msg.data.actions,
                                  msg.principal, msg.data.location)
          .catch(Cu.reportError);
        break;
      case "PluginContent:HideNotificationBar":
        this.hideNotificationBar(msg.target, msg.data.name);
        break;
      case "PluginContent:InstallSinglePlugin":
        this.installSinglePlugin(msg.data.pluginInfo);
        break;
      case "PluginContent:ShowPluginCrashedNotification":
        this.showPluginCrashedNotification(msg.target, msg.data.messageString,
                                           msg.data.pluginID);
        break;
      case "PluginContent:SubmitReport":
        if (AppConstants.MOZ_CRASHREPORTER) {
          this.submitReport(msg.data.runID, msg.data.keyVals, msg.data.submitURLOptIn);
        }
        break;
      case "PluginContent:LinkClickCallback":
        switch (msg.data.name) {
          case "managePlugins":
          case "openHelpPage":
          case "openPluginUpdatePage":
            this[msg.data.name](msg.data.pluginTag);
            break;
        }
        break;
      default:
        Cu.reportError("gPluginHandler did not expect to handle message " + msg.name);
        break;
    }
  },

  // Callback for user clicking on a disabled plugin
  managePlugins() {
    BrowserOpenAddonsMgr("addons://list/plugin");
  },

  // Callback for user clicking on the link in a click-to-play plugin
  // (where the plugin has an update)
  openPluginUpdatePage(pluginTag) {
    let url = Services.blocklist.getPluginInfoURL(pluginTag);
    if (!url) {
      url = Services.blocklist.getPluginBlocklistURL(pluginTag);
    }
    openUILinkIn(url, "tab");
  },

  submitReport: function submitReport(runID, keyVals, submitURLOptIn) {
    if (!AppConstants.MOZ_CRASHREPORTER) {
      return;
    }
    Services.prefs.setBoolPref("dom.ipc.plugins.reportCrashURL", submitURLOptIn);
    PluginCrashReporter.submitCrashReport(runID, keyVals);
  },

  // Callback for user clicking a "reload page" link
  reloadPage(browser) {
    browser.reload();
  },

  // Callback for user clicking the help icon
  openHelpPage() {
    openHelpLink("plugin-crashed", false);
  },

  _clickToPlayNotificationEventCallback: function PH_ctpEventCallback(event) {
    if (event == "showing") {
      Services.telemetry.getHistogramById("PLUGINS_NOTIFICATION_SHOWN")
        .add(!this.options.primaryPlugin);
      // Histograms always start at 0, even though our data starts at 1
      let histogramCount = this.options.pluginData.size - 1;
      if (histogramCount > 4) {
        histogramCount = 4;
      }
      Services.telemetry.getHistogramById("PLUGINS_NOTIFICATION_PLUGIN_COUNT")
        .add(histogramCount);
    } else if (event == "dismissed") {
      // Once the popup is dismissed, clicking the icon should show the full
      // list again
      this.options.primaryPlugin = null;
    }
  },

  /**
   * Called from the plugin doorhanger to set the new permissions for a plugin
   * and activate plugins if necessary.
   * aNewState should be either "allownow" "allowalways" or "block"
   */
  _updatePluginPermission(aNotification, aPluginInfo, aNewState) {
    let permission;
    let expireType;
    let expireTime;
    let histogram =
      Services.telemetry.getHistogramById("PLUGINS_NOTIFICATION_USER_ACTION");

    // Update the permission manager.
    // Also update the current state of pluginInfo.fallbackType so that
    // subsequent opening of the notification shows the current state.
    switch (aNewState) {
      case "allownow":
        permission = Ci.nsIPermissionManager.ALLOW_ACTION;
        expireType = Ci.nsIPermissionManager.EXPIRE_SESSION;
        expireTime = Date.now() + Services.prefs.getIntPref(this.PREF_SESSION_PERSIST_MINUTES) * 60 * 1000;
        histogram.add(0);
        aPluginInfo.fallbackType = Ci.nsIObjectLoadingContent.PLUGIN_ACTIVE;
        aNotification.options.extraAttr = "active";
        break;

      case "allowalways":
        permission = Ci.nsIPermissionManager.ALLOW_ACTION;
        expireType = Ci.nsIPermissionManager.EXPIRE_TIME;
        expireTime = Date.now() +
          Services.prefs.getIntPref(this.PREF_PERSISTENT_DAYS) * 24 * 60 * 60 * 1000;
        histogram.add(1);
        aPluginInfo.fallbackType = Ci.nsIObjectLoadingContent.PLUGIN_ACTIVE;
        aNotification.options.extraAttr = "active";
        break;

      case "block":
        permission = Ci.nsIPermissionManager.PROMPT_ACTION;
        expireType = Ci.nsIPermissionManager.EXPIRE_NEVER;
        expireTime = 0;
        histogram.add(2);
        switch (aPluginInfo.blocklistState) {
          case Ci.nsIBlocklistService.STATE_VULNERABLE_UPDATE_AVAILABLE:
            aPluginInfo.fallbackType = Ci.nsIObjectLoadingContent.PLUGIN_VULNERABLE_UPDATABLE;
            break;
          case Ci.nsIBlocklistService.STATE_VULNERABLE_NO_UPDATE:
            aPluginInfo.fallbackType = Ci.nsIObjectLoadingContent.PLUGIN_VULNERABLE_NO_UPDATE;
            break;
          default:
            aPluginInfo.fallbackType = Ci.nsIObjectLoadingContent.PLUGIN_CLICK_TO_PLAY;
        }
        aNotification.options.extraAttr = "inactive";
        break;

      // In case a plugin has already been allowed in another tab, the "continue allowing" button
      // shouldn't change any permissions but should run the plugin-enablement code below.
      case "continue":
        aPluginInfo.fallbackType = Ci.nsIObjectLoadingContent.PLUGIN_ACTIVE;
        aNotification.options.extraAttr = "active";
        break;
      default:
        Cu.reportError(Error("Unexpected plugin state: " + aNewState));
        return;
    }

    let browser = aNotification.browser;
    if (aNewState != "continue") {
      let principal = aNotification.options.principal;
      Services.perms.addFromPrincipal(principal, aPluginInfo.permissionString,
                                      permission, expireType, expireTime);
      aPluginInfo.pluginPermissionType = expireType;
    }

    browser.messageManager.sendAsyncMessage("BrowserPlugins:ActivatePlugins", {
      pluginInfo: aPluginInfo,
      newState: aNewState,
    });
  },

  showClickToPlayNotification(browser, plugins, showNow,
                                        principal, location) {
    // It is possible that we've received a message from the frame script to show
    // a click to play notification for a principal that no longer matches the one
    // that the browser's content now has assigned (ie, the browser has browsed away
    // after the message was sent, but before the message was received). In that case,
    // we should just ignore the message.
    if (!principal.equals(browser.contentPrincipal)) {
      return;
    }

    // Data URIs, when linked to from some page, inherit the principal of that
    // page. That means that we also need to compare the actual locations to
    // ensure we aren't getting a message from a Data URI that we're no longer
    // looking at.
    let receivedURI = Services.io.newURI(location);
    if (!browser.documentURI.equalsExceptRef(receivedURI)) {
      return;
    }

    let notification = PopupNotifications.getNotification("click-to-play-plugins", browser);

    // If this is a new notification, create a pluginData map, otherwise append
    let pluginData;
    if (notification) {
      pluginData = notification.options.pluginData;
    } else {
      pluginData = new Map();
    }

    let hasInactivePlugins = true;
    for (var pluginInfo of plugins) {
      if (pluginData.has(pluginInfo.permissionString)) {
        continue;
      }

      if (pluginInfo.fallbackType == Ci.nsIObjectLoadingContent.PLUGIN_ACTIVE) {
        hasInactivePlugins = false;
      }

      // If a block contains an infoURL, we should always prefer that to the default
      // URL that we construct in-product, even for other blocklist types.
      let url = Services.blocklist.getPluginInfoURL(pluginInfo.pluginTag);

      if (pluginInfo.blocklistState != Ci.nsIBlocklistService.STATE_NOT_BLOCKED) {
        if (!url) {
          url = Services.blocklist.getPluginBlocklistURL(pluginInfo.pluginTag);
        }
      } else {
        url = Services.urlFormatter.formatURLPref("app.support.baseURL") + "clicktoplay";
      }
      pluginInfo.detailsLink = url;

      pluginData.set(pluginInfo.permissionString, pluginInfo);
    }

    let primaryPluginPermission = null;
    if (showNow) {
      primaryPluginPermission = plugins[0].permissionString;
    }

    if (notification) {
      // Don't modify the notification UI while it's on the screen, that would be
      // jumpy and might allow clickjacking.
      if (showNow) {
        notification.options.primaryPlugin = primaryPluginPermission;
        notification.reshow();
        browser.messageManager.sendAsyncMessage("BrowserPlugins:NotificationShown");
      }
      return;
    }

    let options = {
      dismissed: !showNow,
      persistent: showNow,
      eventCallback: this._clickToPlayNotificationEventCallback,
      primaryPlugin: primaryPluginPermission,
      pluginData,
      principal,
      extraAttr: hasInactivePlugins ? "inactive" : "active",
    };

    let badge = document.getElementById("plugin-icon-badge");
    badge.setAttribute("animate", "true");
    badge.addEventListener("animationend", function animListener(event) {
      if (event.animationName == "blink-badge" &&
          badge.hasAttribute("animate")) {
        badge.removeAttribute("animate");
        badge.removeEventListener("animationend", animListener);
      }
    });

    PopupNotifications.show(browser, "click-to-play-plugins",
                            "", "plugins-notification-icon",
                            null, null, options);
    browser.messageManager.sendAsyncMessage("BrowserPlugins:NotificationShown");
  },

  removeNotification(browser, name) {
    let notification = PopupNotifications.getNotification(name, browser);
    if (notification)
      PopupNotifications.remove(notification);
  },

  hideNotificationBar(browser, name) {
    let notificationBox = gBrowser.getNotificationBox(browser);
    let notification = notificationBox.getNotificationWithValue(name);
    if (notification)
      notificationBox.removeNotification(notification, true);
  },

  infobarBlockedForURI(uri) {
    return new Promise((resolve, reject) => {
      let tableName = Services.prefs.getStringPref("urlclassifier.flashInfobarTable", "");
      if (!tableName) {
        resolve(false);
      }
      let classifier = Cc["@mozilla.org/url-classifier/dbservice;1"]
        .getService(Ci.nsIURIClassifier);
      classifier.asyncClassifyLocalWithTables(uri, tableName, (c, list) => {
        resolve(list.length > 0);
      });
    });
  },

  async updateHiddenPluginUI(browser, haveInsecure, actions,
                                 principal, location) {
    let origin = principal.originNoSuffix;

    let shouldShowNotification = !(await this.infobarBlockedForURI(browser.documentURI));

    // It is possible that we've received a message from the frame script to show
    // the hidden plugin notification for a principal that no longer matches the one
    // that the browser's content now has assigned (ie, the browser has browsed away
    // after the message was sent, but before the message was received). In that case,
    // we should just ignore the message.
    if (!principal.equals(browser.contentPrincipal)) {
      return;
    }

    // Data URIs, when linked to from some page, inherit the principal of that
    // page. That means that we also need to compare the actual locations to
    // ensure we aren't getting a message from a Data URI that we're no longer
    // looking at.
    let receivedURI = Services.io.newURI(location);
    if (!browser.documentURI.equalsExceptRef(receivedURI)) {
      return;
    }

    // Set up the icon
    document.getElementById("plugins-notification-icon").classList.
      toggle("plugin-blocked", haveInsecure);

    // Now configure the notification bar
    let notificationBox = gBrowser.getNotificationBox(browser);

    function hideNotification() {
      let n = notificationBox.getNotificationWithValue("plugin-hidden");
      if (n) {
        notificationBox.removeNotification(n, true);
      }
    }

    // There are three different cases when showing an infobar:
    // 1.  A single type of plugin is hidden on the page. Show the UI for that
    //     plugin.
    // 2a. Multiple types of plugins are hidden on the page. Show the multi-UI
    //     with the vulnerable styling.
    // 2b. Multiple types of plugins are hidden on the page, but none are
    //     vulnerable. Show the nonvulnerable multi-UI.
    function showNotification() {
      if (!Services.prefs.getBoolPref(gPluginHandler.PREF_SHOW_INFOBAR, true)) {
        return;
      }

      let n = notificationBox.getNotificationWithValue("plugin-hidden");
      if (n) {
        // If something is already shown, just keep it
        return;
      }

      Services.telemetry.getHistogramById("PLUGINS_INFOBAR_SHOWN").
        add(true);

      let message;
      // Icons set directly cannot be manipulated using moz-image-region, so
      // we use CSS classes instead.
      let brand = document.getElementById("bundle_brand").getString("brandShortName");

      if (actions.length == 1) {
        let pluginInfo = actions[0];
        let pluginName = pluginInfo.pluginName;

        switch (pluginInfo.fallbackType) {
          case Ci.nsIObjectLoadingContent.PLUGIN_CLICK_TO_PLAY:
            message = gNavigatorBundle.getFormattedString(
              "pluginActivationWarning.message",
              [brand]);
            break;
          case Ci.nsIObjectLoadingContent.PLUGIN_VULNERABLE_UPDATABLE:
            message = gNavigatorBundle.getFormattedString(
              "pluginActivateOutdated.message",
              [pluginName, origin, brand]);
            break;
          case Ci.nsIObjectLoadingContent.PLUGIN_VULNERABLE_NO_UPDATE:
            message = gNavigatorBundle.getFormattedString(
              "pluginActivateVulnerable.message",
              [pluginName, origin, brand]);
        }
      } else {
        // Multi-plugin
        message = gNavigatorBundle.getFormattedString(
          "pluginActivateMultiple.message", [origin]);
      }

      let buttons = [
        {
          label: gNavigatorBundle.getString("pluginContinueBlocking.label"),
          accessKey: gNavigatorBundle.getString("pluginContinueBlocking.accesskey"),
          callback() {
            Services.telemetry.getHistogramById("PLUGINS_INFOBAR_BLOCK").
              add(true);

            Services.perms.addFromPrincipal(principal,
                                            "plugin-hidden-notification",
                                            Services.perms.DENY_ACTION);
          }
        },
        {
          label: gNavigatorBundle.getString("pluginActivateTrigger.label"),
          accessKey: gNavigatorBundle.getString("pluginActivateTrigger.accesskey"),
          callback() {
            Services.telemetry.getHistogramById("PLUGINS_INFOBAR_ALLOW").
              add(true);

            let curNotification =
              PopupNotifications.getNotification("click-to-play-plugins",
                                                 browser);
            if (curNotification) {
              curNotification.reshow();
            }
          }
        }
      ];
      function notificationCallback(type) {
        if (type == "dismissed") {
          Services.telemetry.getHistogramById("PLUGINS_INFOBAR_DISMISSED").
            add(true);
          if (Services.prefs.getBoolPref(gPluginHandler.PREF_INFOBAR_DISMISSAL_PERMANENT, false)) {
            Services.perms.addFromPrincipal(principal,
                                            "plugin-hidden-notification",
                                            Services.perms.DENY_ACTION);
          }
        }
      }
      n = notificationBox.
        appendNotification(message, "plugin-hidden", null,
                           notificationBox.PRIORITY_INFO_HIGH, buttons,
                           notificationCallback);
      if (haveInsecure) {
        n.classList.add("pluginVulnerable");
      }
    }

    if (actions.length == 0) {
      shouldShowNotification = false;
    }
    if (shouldShowNotification &&
        Services.perms.testPermissionFromPrincipal(principal, "plugin-hidden-notification") ==
        Ci.nsIPermissionManager.DENY_ACTION) {
      shouldShowNotification = false;
    }
    if (shouldShowNotification) {
      showNotification();
    } else {
      hideNotification();
    }
  },

  contextMenuCommand(browser, plugin, command) {
    browser.messageManager.sendAsyncMessage("BrowserPlugins:ContextMenuCommand",
      { command }, { plugin });
  },

  // Crashed-plugin observer. Notified once per plugin crash, before events
  // are dispatched to individual plugin instances.
  NPAPIPluginCrashed(subject, topic, data) {
    let propertyBag = subject;
    if (!(propertyBag instanceof Ci.nsIPropertyBag2) ||
        !(propertyBag instanceof Ci.nsIWritablePropertyBag2) ||
        !propertyBag.hasKey("runID") ||
        !propertyBag.hasKey("pluginName")) {
      Cu.reportError("A NPAPI plugin crashed, but the properties of this plugin " +
                     "cannot be read.");
      return;
    }

    let runID = propertyBag.getPropertyAsUint32("runID");
    let uglyPluginName = propertyBag.getPropertyAsAString("pluginName");
    let pluginName = BrowserUtils.makeNicePluginName(uglyPluginName);
    let pluginDumpID = propertyBag.getPropertyAsAString("pluginDumpID");

    // If we don't have a minidumpID, we can't (or didn't) submit anything.
    // This can happen if the plugin is killed from the task manager.
    let state;
    if (!AppConstants.MOZ_CRASHREPORTER || !gCrashReporter.enabled) {
      // This state tells the user that crash reporting is disabled, so we
      // cannot send a report.
      state = "noSubmit";
    } else if (!pluginDumpID) {
      // This state tells the user that there is no crash report available.
      state = "noReport";
    } else {
      // This state asks the user to submit a crash report.
      state = "please";
    }

    let mm = window.getGroupMessageManager("browsers");
    mm.broadcastAsyncMessage("BrowserPlugins:NPAPIPluginProcessCrashed",
                             { pluginName, runID, state });
  },

  /**
   * Shows a plugin-crashed notification bar for a browser that has had an
   * invisiable NPAPI plugin crash, or a GMP plugin crash.
   *
   * @param browser
   *        The browser to show the notification for.
   * @param messageString
   *        The string to put in the notification bar
   * @param pluginID
   *        The unique-per-process identifier for the NPAPI plugin or GMP.
   *        For a GMP, this is the pluginID. For NPAPI plugins (where "pluginID"
   *        means something different), this is the runID.
   */
  showPluginCrashedNotification(browser, messageString, pluginID) {
    // If there's already an existing notification bar, don't do anything.
    let notificationBox = gBrowser.getNotificationBox(browser);
    let notification = notificationBox.getNotificationWithValue("plugin-crashed");
    if (notification) {
      return;
    }

    // Configure the notification bar
    let priority = notificationBox.PRIORITY_WARNING_MEDIUM;
    let iconURL = "chrome://mozapps/skin/plugins/notifyPluginCrashed.png";
    let reloadLabel = gNavigatorBundle.getString("crashedpluginsMessage.reloadButton.label");
    let reloadKey   = gNavigatorBundle.getString("crashedpluginsMessage.reloadButton.accesskey");

    let buttons = [{
      label: reloadLabel,
      accessKey: reloadKey,
      popup: null,
      callback() { browser.reload(); },
    }];

    if (AppConstants.MOZ_CRASHREPORTER &&
        PluginCrashReporter.hasCrashReport(pluginID)) {
      let submitLabel = gNavigatorBundle.getString("crashedpluginsMessage.submitButton.label");
      let submitKey   = gNavigatorBundle.getString("crashedpluginsMessage.submitButton.accesskey");
      let submitButton = {
        label: submitLabel,
        accessKey: submitKey,
        popup: null,
        callback: () => {
          PluginCrashReporter.submitCrashReport(pluginID);
        },
      };

      buttons.push(submitButton);
    }

    notification = notificationBox.appendNotification(messageString, "plugin-crashed",
                                                      iconURL, priority, buttons);

    // Add the "learn more" link.
    let XULNS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
    let link = notification.ownerDocument.createElementNS(XULNS, "label");
    link.className = "text-link";
    link.setAttribute("value", gNavigatorBundle.getString("crashedpluginsMessage.learnMore"));
    let crashurl = formatURL("app.support.baseURL", true);
    crashurl += "plugin-crashed-notificationbar";
    link.href = crashurl;
    let description = notification.ownerDocument.getAnonymousElementByAttribute(notification, "anonid", "messageText");
    description.appendChild(link);
  },
};

gPluginHandler.init();
