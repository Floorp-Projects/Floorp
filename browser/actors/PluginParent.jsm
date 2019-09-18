/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["PluginParent", "PluginManager"];

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "gPluginHost",
  "@mozilla.org/plugin/host;1",
  "nsIPluginHost"
);

ChromeUtils.defineModuleGetter(
  this,
  "BrowserUtils",
  "resource://gre/modules/BrowserUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "CrashSubmit",
  "resource://gre/modules/CrashSubmit.jsm"
);

XPCOMUtils.defineLazyGetter(this, "gNavigatorBundle", function() {
  const url = "chrome://browser/locale/browser.properties";
  return Services.strings.createBundle(url);
});

const kNotificationId = "click-to-play-plugins";

const {
  PLUGIN_ACTIVE,
  PLUGIN_VULNERABLE_NO_UPDATE,
  PLUGIN_VULNERABLE_UPDATABLE,
  PLUGIN_CLICK_TO_PLAY_QUIET,
} = Ci.nsIObjectLoadingContent;

const PluginManager = {
  _initialized: false,

  pluginMap: new Map(),
  crashReports: new Map(),
  gmpCrashes: new Map(),
  _pendingCrashQueries: new Map(),

  // BrowserGlue.jsm ensures we catch all plugin crashes.
  // Barring crashes, we don't need to do anything until/unless an
  // actor gets instantiated, in which case we also care about the
  // plugin list changing.
  ensureInitialized() {
    if (this._initialized) {
      return;
    }
    this._initialized = true;
    this._updatePluginMap();
    Services.obs.addObserver(this, "plugins-list-updated");
    Services.obs.addObserver(this, "profile-after-change");
  },

  destroy() {
    if (!this._initialized) {
      return;
    }
    Services.obs.removeObserver(this, "plugins-list-updated");
    Services.obs.removeObserver(this, "profile-after-change");
    this.crashReports = new Map();
    this.gmpCrashes = new Map();
    this.pluginMap = new Map();
  },

  observe(subject, topic, data) {
    switch (topic) {
      case "plugins-list-updated":
        this._updatePluginMap();
        break;
      case "plugin-crashed":
        this.ensureInitialized();
        this._registerNPAPICrash(subject);
        break;
      case "gmp-plugin-crash":
        this.ensureInitialized();
        this._registerGMPCrash(subject);
        break;
      case "profile-after-change":
        this.destroy();
        break;
    }
  },

  getPluginTagById(id) {
    return this.pluginMap.get(id);
  },

  _updatePluginMap() {
    this.pluginMap = new Map();
    let plugins = gPluginHost.getPluginTags();
    for (let plugin of plugins) {
      this.pluginMap.set(plugin.id, plugin);
    }
  },

  // Crashed-plugin observer. Notified once per plugin crash, before events
  // are dispatched to individual plugin instances. However, because of IPC,
  // the event and the observer notification may still race.
  _registerNPAPICrash(subject) {
    let propertyBag = subject;
    if (
      !(propertyBag instanceof Ci.nsIWritablePropertyBag2) ||
      !propertyBag.hasKey("runID") ||
      !propertyBag.hasKey("pluginName")
    ) {
      Cu.reportError(
        "A NPAPI plugin crashed, but the notification is incomplete."
      );
      return;
    }

    let runID = propertyBag.getPropertyAsUint32("runID");
    let uglyPluginName = propertyBag.getPropertyAsAString("pluginName");
    let pluginName = BrowserUtils.makeNicePluginName(uglyPluginName);
    let pluginDumpID = propertyBag.getPropertyAsAString("pluginDumpID");
    let browserDumpID = propertyBag.getPropertyAsAString("browserDumpID");

    let state;
    let crashReporter = Services.appinfo.QueryInterface(Ci.nsICrashReporter);
    if (!AppConstants.MOZ_CRASHREPORTER || !crashReporter.enabled) {
      // This state tells the user that crash reporting is disabled, so we
      // cannot send a report.
      state = "noSubmit";
    } else if (!pluginDumpID) {
      // If we don't have a minidumpID, we can't submit anything.
      // This can happen if the plugin is killed from the task manager.
      // This state tells the user that this is the case.
      state = "noReport";
    } else {
      // This state asks the user to submit a crash report.
      state = "please";
    }

    let crashInfo = { runID, state, pluginName, pluginDumpID, browserDumpID };
    this.crashReports.set(runID, crashInfo);
    let listeners = this._pendingCrashQueries.get(runID) || [];
    for (let listener of listeners) {
      listener(crashInfo);
    }
    this._pendingCrashQueries.delete(runID);
  },

  _registerGMPCrash(subject) {
    let propertyBag = subject;
    if (
      !(propertyBag instanceof Ci.nsIWritablePropertyBag2) ||
      !propertyBag.hasKey("pluginID") ||
      !propertyBag.hasKey("pluginDumpID") ||
      !propertyBag.hasKey("pluginName")
    ) {
      Cu.reportError("PluginManager can not read plugin information.");
      return;
    }

    let pluginID = propertyBag.getPropertyAsUint32("pluginID");
    let pluginDumpID = propertyBag.getPropertyAsAString("pluginDumpID");
    if (pluginDumpID) {
      this.gmpCrashes.set(pluginID, { pluginDumpID, pluginID });
    }

    // Only the parent process gets the gmp-plugin-crash observer
    // notification, so we need to inform any content processes that
    // the GMP has crashed. This then fires PluginCrashed events in
    // all the relevant windows, which will trigger child actors being
    // created, which will contact us again, when we'll use the
    // gmpCrashes collection to respond.
    if (Services.ppmm) {
      let pluginName = propertyBag.getPropertyAsAString("pluginName");
      Services.ppmm.broadcastAsyncMessage("gmp-plugin-crash", {
        pluginName,
        pluginID,
      });
    }
  },

  /**
   * Submit a crash report for a crashed NPAPI plugin.
   *
   * @param pluginCrashID
   *        An object with either a runID (for NPAPI crashes) or a pluginID
   *        property (for GMP plugin crashes).
   *        A run ID is a unique identifier for a particular run of a plugin
   *        process - and is analogous to a process ID (though it is managed
   *        by Gecko instead of the operating system).
   * @param keyVals
   *        An object whose key-value pairs will be merged
   *        with the ".extra" file submitted with the report.
   *        The properties of htis object will override properties
   *        of the same name in the .extra file.
   */
  submitCrashReport(pluginCrashID, keyVals = {}) {
    let report = this.getCrashReport(pluginCrashID);
    if (!report) {
      Cu.reportError(
        `Could not find plugin dump IDs for ${JSON.stringify(pluginCrashID)}.` +
          `It is possible that a report was already submitted.`
      );
      return;
    }

    let { pluginDumpID, browserDumpID } = report;
    let submissionPromise = CrashSubmit.submit(pluginDumpID, {
      recordSubmission: true,
      extraExtraKeyVals: keyVals,
    });

    if (browserDumpID) {
      CrashSubmit.submit(browserDumpID).catch(Cu.reportError);
    }

    this.broadcastState(pluginCrashID, "submitting");

    submissionPromise.then(
      () => {
        this.broadcastState(pluginCrashID, "success");
      },
      () => {
        this.broadcastState(pluginCrashID, "failed");
      }
    );

    if (pluginCrashID.hasOwnProperty("runID")) {
      this.crashReports.delete(pluginCrashID.runID);
    } else {
      this.gmpCrashes.delete(pluginCrashID.pluginID);
    }
  },

  broadcastState(pluginCrashID, state) {
    if (!pluginCrashID.hasOwnProperty("runID")) {
      return;
    }
    let { runID } = pluginCrashID;
    Services.ppmm.broadcastAsyncMessage(
      "PluginParent:NPAPIPluginCrashReportSubmitted",
      { runID, state }
    );
  },

  getCrashReport(pluginCrashID) {
    if (pluginCrashID.hasOwnProperty("pluginID")) {
      return this.gmpCrashes.get(pluginCrashID.pluginID);
    }
    return this.crashReports.get(pluginCrashID.runID);
  },

  /**
   * Called by actors when they want crash info on behalf of the child.
   * Will either return such info immediately if we have it, or return
   * a promise, which resolves when we do have it. The promise resolution
   * function is kept around for when we get the `plugin-crashed` observer
   * notification.
   */
  awaitPluginCrashInfo(runID) {
    if (this.crashReports.has(runID)) {
      return this.crashReports.get(runID);
    }
    let listeners = this._pendingCrashQueries.get(runID);
    if (!listeners) {
      listeners = [];
      this._pendingCrashQueries.set(runID, listeners);
    }
    return new Promise(resolve => listeners.push(resolve));
  },

  /**
   * This allows dependency injection, where an automated test can
   * dictate how and when we respond to a child's inquiry about a crash.
   * This is helpful when testing different orderings for plugin crash
   * notifications (ie race conditions).
   *
   * Concretely, for the toplevel browsingContext of the `browser` we're
   * passed, call the passed `handler` function the next time the child
   * asks for crash data (using PluginContent:GetCrashData). We'll return
   * the result of the function to the child. The message in question
   * uses the actor query API, so promises and/or async functions will
   * Just Work.
   */
  mockResponse(browser, handler) {
    let { currentWindowGlobal } = browser.frameLoader.browsingContext;
    currentWindowGlobal.getActor("Plugin")._mockedResponder = handler;
  },
};

const PREF_SESSION_PERSIST_MINUTES =
  "plugin.sessionPermissionNow.intervalInMinutes";

class PluginParent extends JSWindowActorParent {
  constructor() {
    super();
    PluginManager.ensureInitialized();
  }

  receiveMessage(msg) {
    let browser = this.manager.rootFrameLoader.ownerElement;
    let win = browser.ownerGlobal;
    switch (msg.name) {
      case "PluginContent:ShowClickToPlayNotification":
        this.showClickToPlayNotification(
          browser,
          msg.data.plugin,
          msg.data.showNow
        );
        break;
      case "PluginContent:RemoveNotification":
        this.removeNotification(browser);
        break;
      case "PluginContent:ShowPluginCrashedNotification":
        this.showPluginCrashedNotification(browser, msg.data.pluginCrashID);
        break;
      case "PluginContent:SubmitReport":
        if (AppConstants.MOZ_CRASHREPORTER) {
          this.submitReport(
            msg.data.runID,
            msg.data.keyVals,
            msg.data.submitURLOptIn
          );
        }
        break;
      case "PluginContent:LinkClickCallback":
        switch (msg.data.name) {
          case "managePlugins":
          case "openHelpPage":
            this[msg.data.name](win);
            break;
          case "openPluginUpdatePage":
            this.openPluginUpdatePage(win, msg.data.pluginId);
            break;
        }
        break;
      case "PluginContent:GetCrashData":
        if (this._mockedResponder) {
          let rv = this._mockedResponder(msg.data);
          delete this._mockedResponder;
          return rv;
        }
        return PluginManager.awaitPluginCrashInfo(msg.data.runID);

      default:
        Cu.reportError(
          "PluginParent did not expect to handle message " + msg.name
        );
        break;
    }

    return null;
  }

  // Callback for user clicking on a disabled plugin
  managePlugins(window) {
    window.BrowserOpenAddonsMgr("addons://list/plugin");
  }

  // Callback for user clicking on the link in a click-to-play plugin
  // (where the plugin has an update)
  async openPluginUpdatePage(window, pluginId) {
    let pluginTag = PluginManager.getPluginTagById(pluginId);
    if (!pluginTag) {
      return;
    }
    let { Blocklist } = ChromeUtils.import(
      "resource://gre/modules/Blocklist.jsm"
    );
    let url = await Blocklist.getPluginBlockURL(pluginTag);
    window.openTrustedLinkIn(url, "tab");
  }

  submitReport(runID, keyVals, submitURLOptIn) {
    if (!AppConstants.MOZ_CRASHREPORTER) {
      return;
    }
    Services.prefs.setBoolPref(
      "dom.ipc.plugins.reportCrashURL",
      !!submitURLOptIn
    );
    PluginManager.submitCrashReport({ runID }, keyVals);
  }

  // Callback for user clicking a "reload page" link
  reloadPage(browser) {
    browser.reload();
  }

  // Callback for user clicking the help icon
  openHelpPage(window) {
    window.openHelpLink("plugin-crashed", false);
  }

  _clickToPlayNotificationEventCallback(event) {
    if (event == "showing") {
      Services.telemetry
        .getHistogramById("PLUGINS_NOTIFICATION_SHOWN")
        .add(!this.options.showNow);
    } else if (event == "dismissed") {
      // Once the popup is dismissed, clicking the icon should show the full
      // list again
      this.options.showNow = false;
    }
  }

  /**
   * Called from the plugin doorhanger to set the new permissions for a plugin
   * and activate plugins if necessary.
   * aNewState should be one of:
   * - "allownow"
   * - "block"
   * - "continue"
   * - "continueblocking"
   */
  _updatePluginPermission(aBrowser, aActivationInfo, aNewState) {
    let permission;
    let expireType;
    let expireTime;
    let histogram = Services.telemetry.getHistogramById(
      "PLUGINS_NOTIFICATION_USER_ACTION_2"
    );

    let window = aBrowser.ownerGlobal;
    let notification = window.PopupNotifications.getNotification(
      kNotificationId,
      aBrowser
    );

    // Update the permission manager.
    // Also update the current state of activationInfo.fallbackType so that
    // subsequent opening of the notification shows the current state.
    switch (aNewState) {
      case "allownow":
        permission = Ci.nsIPermissionManager.ALLOW_ACTION;
        expireType = Ci.nsIPermissionManager.EXPIRE_SESSION;
        expireTime =
          Date.now() +
          Services.prefs.getIntPref(PREF_SESSION_PERSIST_MINUTES) * 60 * 1000;
        histogram.add(0);
        aActivationInfo.fallbackType = PLUGIN_ACTIVE;
        notification.options.extraAttr = "active";
        break;

      case "block":
        permission = Ci.nsIPermissionManager.PROMPT_ACTION;
        expireType = Ci.nsIPermissionManager.EXPIRE_SESSION;
        expireTime = 0;
        histogram.add(2);
        let pluginTag = PluginManager.getPluginTagById(aActivationInfo.id);
        switch (pluginTag.blocklistState) {
          case Ci.nsIBlocklistService.STATE_VULNERABLE_UPDATE_AVAILABLE:
            aActivationInfo.fallbackType = PLUGIN_VULNERABLE_UPDATABLE;
            break;
          case Ci.nsIBlocklistService.STATE_VULNERABLE_NO_UPDATE:
            aActivationInfo.fallbackType = PLUGIN_VULNERABLE_NO_UPDATE;
            break;
          default:
            // PLUGIN_CLICK_TO_PLAY_QUIET will only last until they reload the page, at
            // which point it will be PLUGIN_CLICK_TO_PLAY (the overlays will appear)
            aActivationInfo.fallbackType = PLUGIN_CLICK_TO_PLAY_QUIET;
        }
        notification.options.extraAttr = "inactive";
        break;

      // In case a plugin has already been allowed/disallowed in another tab, the
      // buttons matching the existing block state shouldn't change any permissions
      // but should run the plugin-enablement code below.
      case "continue":
        aActivationInfo.fallbackType = PLUGIN_ACTIVE;
        notification.options.extraAttr = "active";
        break;

      case "continueblocking":
        aActivationInfo.fallbackType = PLUGIN_CLICK_TO_PLAY_QUIET;
        notification.options.extraAttr = "inactive";
        break;

      default:
        Cu.reportError(Error("Unexpected plugin state: " + aNewState));
        return;
    }

    if (aNewState != "continue" && aNewState != "continueblocking") {
      let { principal } = notification.options;
      Services.perms.addFromPrincipal(
        principal,
        aActivationInfo.permissionString,
        permission,
        expireType,
        expireTime
      );
    }

    this.sendAsyncMessage("PluginParent:ActivatePlugins", {
      activationInfo: aActivationInfo,
      newState: aNewState,
    });
  }

  showClickToPlayNotification(browser, plugin, showNow) {
    let window = browser.ownerGlobal;
    if (!window.PopupNotifications) {
      return;
    }
    let notification = window.PopupNotifications.getNotification(
      kNotificationId,
      browser
    );

    if (!plugin) {
      this.removeNotification(browser);
      return;
    }

    // We assume that we can only have 1 notification at a time anyway.
    if (notification) {
      if (showNow) {
        notification.options.showNow = true;
        notification.reshow();
      }
      return;
    }

    // Construct a notification for the plugin:
    let { id, fallbackType } = plugin;
    let pluginTag = PluginManager.getPluginTagById(id);
    if (!pluginTag) {
      return;
    }
    let permissionString = gPluginHost.getPermissionStringForTag(pluginTag);
    let active = fallbackType == PLUGIN_ACTIVE;

    let options = {
      dismissed: !showNow,
      hideClose: true,
      persistent: showNow,
      eventCallback: this._clickToPlayNotificationEventCallback,
      showNow,
      popupIconClass: "plugin-icon",
      extraAttr: active ? "active" : "inactive",
      principal: this.browsingContext.currentWindowGlobal.documentPrincipal,
    };

    let description;
    if (
      fallbackType == Ci.nsIObjectLoadingContent.PLUGIN_VULNERABLE_UPDATABLE
    ) {
      description = gNavigatorBundle.GetStringFromName(
        "flashActivate.outdated.message"
      );
    } else {
      description = gNavigatorBundle.GetStringFromName("flashActivate.message");
    }

    let badge = window.document.getElementById("plugin-icon-badge");
    badge.setAttribute("animate", "true");
    badge.addEventListener("animationend", function animListener(event) {
      if (
        event.animationName == "blink-badge" &&
        badge.hasAttribute("animate")
      ) {
        badge.removeAttribute("animate");
        badge.removeEventListener("animationend", animListener);
      }
    });

    let weakBrowser = Cu.getWeakReference(browser);

    let activationInfo = { id, fallbackType, permissionString };
    // Note: in both of these action callbacks, we check the fallbackType on the
    // activationInfo object, not the local variable. This is important because
    // the activationInfo object is effectively read/write - the notification
    // will stay up, and for blocking after allowing (or vice versa) to work, we
    // need to always read the updated value.
    let mainAction = {
      callback: () => {
        let browserRef = weakBrowser.get();
        if (!browserRef) {
          return;
        }
        let perm =
          activationInfo.fallbackType == PLUGIN_ACTIVE
            ? "continue"
            : "allownow";
        this._updatePluginPermission(browserRef, activationInfo, perm);
      },
      label: gNavigatorBundle.GetStringFromName("flashActivate.allow"),
      accessKey: gNavigatorBundle.GetStringFromName(
        "flashActivate.allow.accesskey"
      ),
      dismiss: true,
    };
    let secondaryActions = [
      {
        callback: () => {
          let browserRef = weakBrowser.get();
          if (!browserRef) {
            return;
          }
          let perm =
            activationInfo.fallbackType == PLUGIN_ACTIVE
              ? "block"
              : "continueblocking";
          this._updatePluginPermission(browserRef, activationInfo, perm);
        },
        label: gNavigatorBundle.GetStringFromName("flashActivate.noAllow"),
        accessKey: gNavigatorBundle.GetStringFromName(
          "flashActivate.noAllow.accesskey"
        ),
        dismiss: true,
      },
    ];

    window.PopupNotifications.show(
      browser,
      kNotificationId,
      description,
      "plugins-notification-icon",
      mainAction,
      secondaryActions,
      options
    );

    // Check if the plugin is insecure and update the notification icon accordingly.
    let haveInsecure = false;
    switch (fallbackType) {
      // haveInsecure will trigger the red flashing icon and the infobar
      // styling below
      case PLUGIN_VULNERABLE_UPDATABLE:
      case PLUGIN_VULNERABLE_NO_UPDATE:
        haveInsecure = true;
    }

    window.document
      .getElementById("plugins-notification-icon")
      .classList.toggle("plugin-blocked", haveInsecure);
  }

  removeNotification(browser) {
    let { PopupNotifications } = browser.ownerGlobal;
    let notification = PopupNotifications.getNotification(
      kNotificationId,
      browser
    );
    if (notification) {
      PopupNotifications.remove(notification);
    }
  }

  /**
   * Shows a plugin-crashed notification bar for a browser that has had an
   * invisible NPAPI plugin crash, or a GMP plugin crash.
   *
   * @param browser
   *        The browser to show the notification for.
   * @param pluginCrashID
   *        The unique-per-process identifier for the NPAPI plugin or GMP.
   *        This will have either a runID or pluginID property, identifying
   *        an npapi plugin or gmp plugin crash, respectively.
   */
  showPluginCrashedNotification(browser, pluginCrashID) {
    // If there's already an existing notification bar, don't do anything.
    let notificationBox = browser.getTabBrowser().getNotificationBox(browser);
    let notification = notificationBox.getNotificationWithValue(
      "plugin-crashed"
    );

    let report = PluginManager.getCrashReport(pluginCrashID);
    if (notification || !report) {
      return;
    }

    // Configure the notification bar
    let priority = notificationBox.PRIORITY_WARNING_MEDIUM;
    let iconURL = "chrome://global/skin/plugins/pluginGeneric.svg";
    let reloadLabel = gNavigatorBundle.GetStringFromName(
      "crashedpluginsMessage.reloadButton.label"
    );
    let reloadKey = gNavigatorBundle.GetStringFromName(
      "crashedpluginsMessage.reloadButton.accesskey"
    );

    let buttons = [
      {
        label: reloadLabel,
        accessKey: reloadKey,
        popup: null,
        callback() {
          browser.reload();
        },
      },
    ];

    if (AppConstants.MOZ_CRASHREPORTER) {
      let submitLabel = gNavigatorBundle.GetStringFromName(
        "crashedpluginsMessage.submitButton.label"
      );
      let submitKey = gNavigatorBundle.GetStringFromName(
        "crashedpluginsMessage.submitButton.accesskey"
      );
      let submitButton = {
        label: submitLabel,
        accessKey: submitKey,
        popup: null,
        callback: () => {
          PluginManager.submitCrashReport(pluginCrashID);
        },
      };

      buttons.push(submitButton);
    }

    let messageString = gNavigatorBundle.formatStringFromName(
      "crashedpluginsMessage.title",
      [report.pluginName]
    );
    notification = notificationBox.appendNotification(
      messageString,
      "plugin-crashed",
      iconURL,
      priority,
      buttons
    );

    // Add the "learn more" link.
    let link = notification.ownerDocument.createXULElement("label", {
      is: "text-link",
    });
    link.setAttribute(
      "value",
      gNavigatorBundle.GetStringFromName("crashedpluginsMessage.learnMore")
    );
    let crashurl = Services.urlFormatter.formatURLPref("app.support.baseURL");
    crashurl += "plugin-crashed-notificationbar";
    link.href = crashurl;
    notification.messageText.appendChild(link);
  }
}
