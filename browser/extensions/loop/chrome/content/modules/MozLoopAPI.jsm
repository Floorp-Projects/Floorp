/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://services-common/utils.js");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("chrome://loop/content/modules/MozLoopService.jsm");
Cu.import("chrome://loop/content/modules/LoopRooms.jsm");
Cu.importGlobalProperties(["Blob"]);

XPCOMUtils.defineLazyModuleGetter(this, "NewTabURL",
                                        "resource:///modules/NewTabURL.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PageMetadata",
                                        "resource://gre/modules/PageMetadata.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PluralForm",
                                        "resource://gre/modules/PluralForm.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "UpdateUtils",
                                        "resource://gre/modules/UpdateUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "UITour",
                                        "resource:///modules/UITour.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Social",
                                        "resource:///modules/Social.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Promise",
                                        "resource://gre/modules/Promise.jsm");
XPCOMUtils.defineLazyGetter(this, "appInfo", function() {
  return Cc["@mozilla.org/xre/app-info;1"]
           .getService(Ci.nsIXULAppInfo)
           .QueryInterface(Ci.nsIXULRuntime);
});
XPCOMUtils.defineLazyServiceGetter(this, "clipboardHelper",
                                         "@mozilla.org/widget/clipboardhelper;1",
                                         "nsIClipboardHelper");
XPCOMUtils.defineLazyServiceGetter(this, "extProtocolSvc",
                                         "@mozilla.org/uriloader/external-protocol-service;1",
                                         "nsIExternalProtocolService");
this.EXPORTED_SYMBOLS = ["LoopAPI"];

const cloneableError = function(source) {
  // Simple Object that can be cloned over.
  let error = {};
  if (typeof source == "string") {
    source = new Error(source);
  }

  let props = Object.getOwnPropertyNames(source);
  // nsIException properties are not enumerable, so we'll try to copy the most
  // common and useful ones.
  if (!props.length) {
    props.push("message", "filename", "lineNumber", "columnNumber", "stack");
  }
  for (let prop of props) {
    let value = source[prop];
    let type = typeof value;

    // Functions can't be cloned. Period.
    // For nsIException objects, the property may not be defined.
    if (type == "function" || type == "undefined") {
      continue;
    }
    // Don't do anything to members that are already cloneable.
    if (/boolean|number|string/.test(type)) {
      error[prop] = value;
    } else {
      // Convert non-compatible types to a String.
      error[prop] = "" + value;
    }
  }

  // Mark the object as an Error, otherwise it won't be discernable from other,
  // regular objects.
  error.isError = true;

  return error;
};

const getObjectAPIFunctionName = function(action) {
  let funcName = action.split(":").pop();
  return funcName.charAt(0).toLowerCase() + funcName.substr(1);
};

/**
 * Retrieves a list of Social Providers from the Social API that are explicitly
 * capable of sharing URLs.
 * It also adds a listener that is fired whenever a new Provider is added or
 * removed.
 *
 * @return {Array} Sorted list of share-capable Social Providers.
 */
const updateSocialProvidersCache = function() {
  let providers = [];

  for (let provider of Social.providers) {
    if (!provider.shareURL) {
      continue;
    }

    // Only pass the relevant data on to content.
    providers.push({
      iconURL: provider.iconURL,
      name: provider.name,
      origin: provider.origin
    });
  }

  let providersWasSet = !!gSocialProviders;
  // Replace old with new.
  gSocialProviders = providers.sort((a, b) =>
    a.name.toLowerCase().localeCompare(b.name.toLowerCase()));

  // Start listening for changes in the social provider list, if we're not
  // doing that yet.
  if (!providersWasSet) {
    Services.obs.addObserver(updateSocialProvidersCache, "social:providers-changed", false);
  } else {
    // Dispatch an event to content to let stores freshen-up.
    LoopAPIInternal.broadcastPushMessage("SocialProvidersChanged");
  }

  return gSocialProviders;
};

/**
 *  Checks that [browser.js]'s global variable `gMultiProcessBrowser` is active,
 *  instead of checking on first available browser element.
 *  :see bug 1257243 comment 5:
 */
const isMultiProcessActive = function() {
  let win = Services.wm.getMostRecentWindow("navigator:browser");
  return !!win.gMultiProcessBrowser;
};

var gAppVersionInfo = null;
var gBrowserSharingListeners = new Set();
var gBrowserSharingWindows = new Set();
var gPageListeners = null;
var gOriginalPageListeners = null;
var gSocialProviders = null;
var gStringBundle = null;
var gStubbedMessageHandlers = null;
const kBatchMessage = "Batch";
const kMaxLoopCount = 10;
const kMessageName = "Loop:Message";
const kPushMessageName = "Loop:Message:Push";
const kPushSubscription = "pushSubscription";
const kRoomsPushPrefix = "Rooms:";
const kMauPrefMap = new Map(
  Object.getOwnPropertyNames(LOOP_MAU_TYPE).map(name => {
    let parts = name.toLowerCase().split("_");
    return [LOOP_MAU_TYPE[name], parts[0] + parts[1].charAt(0).toUpperCase() + parts[1].substr(1)];
  })
);

/**
 * WARNING: Every function in kMessageHandlers must call the reply() function,
 * as otherwise the content requesters can be left hanging.
 *
 * Ideally, we should rewrite them to handle failure/long times better, at which
 * point this could be relaxed slightly.
 */
const kMessageHandlers = {
  /**
   * Start browser sharing, which basically means to start listening for tab
   * switches and passing the new window ID to the sender whenever that happens.
   *
   * @param {Object}   message Message meant for the handler function, containing
   *                           the following parameters in its `data` property:
   *                           [
   *                             {String} roomToken The room ID to start browser sharing and listeners.
   *                           ]
   * @param {Function} reply   Callback function, invoked with the result of this
   *                           message handler. The result will be sent back to
   *                           the senders' channel.
   */
  AddBrowserSharingListener: function(message, reply) {
    let win = Services.wm.getMostRecentWindow("navigator:browser");
    let browser = win && win.gBrowser.selectedBrowser;
    if (!win || !browser) {
      // This may happen when an undocked conversation window is the only
      // window left.
      let err = new Error("No tabs available to share.");
      MozLoopService.log.error(err);
      reply(cloneableError(err));
      return;
    }

    let autoStart = MozLoopService.getLoopPref("remote.autostart");
    if (!autoStart && browser.getAttribute("remote") == "true") {
      // Tab sharing might not be supported yet for e10s-enabled browsers.
      let err = new Error("Tab sharing is not supported for e10s-enabled browsers");
      MozLoopService.log.error(err);
      reply(cloneableError(err));
      return;
    }

    // get room token from message
    let [windowId] = message.data;
    // For rooms, the windowId === roomToken. If we change the type of place we're
    // sharing from in the future, we may need to change this.
    win.LoopUI.startBrowserSharing(windowId);

    // Point new tab to load about:home to avoid accidentally sharing top sites.
    NewTabURL.override("about:home");

    gBrowserSharingWindows.add(Cu.getWeakReference(win));
    gBrowserSharingListeners.add(windowId);
    reply();
  },

  /**
   * Creates a layout for the remote cursor on the browser chrome,
   * and positions it on the received coordinates.
   *
   * @param {Object}  message Message meant for the handler function, contains
   *                          the following parameters in its 'data' property:
   *                          {
   *                            ratioX: cursor's X position (between 0-1)
   *                            ratioY: cursor's Y position (between 0-1)
   *                          }
   *
   * @param {Function} reply  Callback function, invoked with the result of the
   *                          message handler. The result will be sent back to
   *                          the senders' channel.
   */
  AddRemoteCursorOverlay: function(message, reply) {
    let win = Services.wm.getMostRecentWindow("navigator:browser");
    if (win) {
      win.LoopUI.addRemoteCursor(message.data[0]);
    }

    reply();
  },

  /**
   * Shows the click event on the remote cursor.
   *
   * @param {Object}  message Message meant for the handler function, contains
   *                          a boolean for the click event in its 'data' prop.
   *
   * @param {Function} reply  Callback function, invoked with the result of the
   *                          message handler. The result will be sent back to
   *                          the senders' channel.
   */
  ClickRemoteCursor: function(message, reply) {
    let win = Services.wm.getMostRecentWindow("navigator:browser");
    if (win) {
      win.LoopUI.clickRemoteCursor(message.data[0]);
    }

    reply();
  },

  /**
   * Associates a session-id and a call-id with a window for debugging.
   *
   * @param {Object}   message Message meant for the handler function, containing
   *                           the following parameters in its `data` property:
   *                           [
   *                             {String} windowId  The window id.
   *                             {String} sessionId OT session id.
   *                             {String} callId    The callId on the server.
   *                           ]
   * @param {Function} reply   Callback function, invoked with the result of this
   *                           message handler. The result will be sent back to
   *                           the senders' channel.
   */
  AddConversationContext: function(message, reply) {
    let [windowId, sessionId, callid] = message.data;
    MozLoopService.addConversationContext(windowId, {
      sessionId: sessionId,
      callId: callid
    });
    reply();
  },

  /**
   * Activates the Social Share panel with the Social Provider panel opened
   * when the popup open.
   *
   * @param {Object}   message Message meant for the handler function, containing
   *                           the following parameters in its `data` property:
   *                           [ ]
   * @param {Function} reply   Callback function, invoked with the result of this
   *                           message handler. The result will be sent back to
   *                           the senders' channel.
   */
  AddSocialShareProvider: function(message, reply) {
    let win = Services.wm.getMostRecentWindow("navigator:browser");
    if (!win || !win.SocialShare) {
      reply();
      return;
    }
    win.SocialShare.showDirectory(win.LoopUI.toolbarButton.anchor);
    reply();
  },

  /**
   * Composes an email via the external protocol service.
   *
   * @param {Object}   message Message meant for the handler function, containing
   *                           the following parameters in its `data` property:
   *                           [
   *                             {String} subject   Subject of the email to send
   *                             {String} body      Body message of the email to send
   *                             {String} recipient Recipient email address (optional)
   *                           ]
   * @param {Function} reply   Callback function, invoked with the result of this
   *                           message handler. The result will be sent back to
   *                           the senders' channel.
   */
  ComposeEmail: function(message, reply) {
    let [subject, body, recipient] = message.data;
    recipient = recipient || "";
    let mailtoURL = "mailto:" + encodeURIComponent(recipient) +
                    "?subject=" + encodeURIComponent(subject) +
                    "&body=" + encodeURIComponent(body);
    extProtocolSvc.loadURI(CommonUtils.makeURI(mailtoURL));
    reply();
  },

  /**
   * Show a confirmation dialog with the standard - localized - 'Yes'/ 'No'
   * buttons or custom labels.
   *
   * @param {Object}   message Message meant for the handler function, containing
   *                           the following parameters in its `data` property:
   *                           [
   *                             {Object} options Options for the confirm dialog:
   *                               - {String} message        Message body for the dialog
   *                               - {String} [okButton]     Label for the OK button
   *                               - {String} [cancelButton] Label for the Cancel button
   *                           ]
   * @param {Function} reply   Callback function, invoked with the result of this
   *                           message handler. The result will be sent back to
   *                           the senders' channel.
   */
  Confirm: function(message, reply) {
    let options = message.data[0];
    let buttonFlags;
    if (options.okButton && options.cancelButton) {
      buttonFlags =
        (Ci.nsIPrompt.BUTTON_POS_0 * Ci.nsIPrompt.BUTTON_TITLE_IS_STRING) +
        (Ci.nsIPrompt.BUTTON_POS_1 * Ci.nsIPrompt.BUTTON_TITLE_IS_STRING);
    } else if (!options.okButton && !options.cancelButton) {
      buttonFlags = Services.prompt.STD_YES_NO_BUTTONS;
    } else {
      reply(cloneableError("confirm: missing button options"));
      return;
    }

    try {
      let chosenButton = Services.prompt.confirmEx(null, "",
        options.message, buttonFlags, options.okButton, options.cancelButton,
        null, null, {});

      reply(chosenButton == 0);
    } catch (ex) {
      reply(ex);
    }
  },

  /**
   * Copies passed string onto the system clipboard.
   *
   * @param {Object}   message Message meant for the handler function, containing
   *                           the following parameters in its `data` property:
   *                           [
   *                             {String} str The string to copy
   *                           ]
   * @param {Function} reply   Callback function, invoked with the result of this
   *                           message handler. The result will be sent back to
   *                           the senders' channel.
   */
  CopyString: function(message, reply) {
    let str = message.data[0];
    clipboardHelper.copyString(str);
    reply();
  },

  /**
   * Returns a new GUID (UUID) in curly braces format.
   *
   * @param {Object}   message Message meant for the handler function, containing
   *                           the following parameters in its `data` property:
   *                           [ ]
   * @param {Function} reply   Callback function, invoked with the result of this
   *                           message handler. The result will be sent back to
   *                           the senders' channel.
   */
  GenerateUUID: function(message, reply) {
    reply(MozLoopService.generateUUID());
  },

  /**
   * Fetch the JSON blob of localized strings from the loop.properties bundle.
   * @see MozLoopService#getStrings
   *
   * @param {Object}   message Message meant for the handler function, containing
   *                           the following parameters in its `data` property:
   *                           [ ]
   * @param {Function} reply   Callback function, invoked with the result of this
   *                           message handler. The result will be sent back to
   *                           the senders' channel.
   */
  GetAllStrings: function(message, reply) {
    if (gStringBundle) {
      reply(gStringBundle);
      return;
    }

    // Get the map of strings.
    let strings = MozLoopService.getStrings();
    // Convert it to an object.
    gStringBundle = {};
    for (let [key, value] of strings.entries()) {
      gStringBundle[key] = value;
    }
    reply(gStringBundle);
  },

  /**
   * Fetch all constants that are used both on the client and the chrome-side.
   *
   * @param {Object}   message Message meant for the handler function, containing
   *                           the following parameters in its `data` property:
   *                           [ ]
   * @param {Function} reply   Callback function, invoked with the result of this
   *                           message handler. The result will be sent back to
   *                           the senders' channel.
   */
  GetAllConstants: function(message, reply) {
    reply({
      COPY_PANEL: COPY_PANEL,
      LOOP_SESSION_TYPE: LOOP_SESSION_TYPE,
      LOOP_MAU_TYPE: LOOP_MAU_TYPE,
      ROOM_CREATE: ROOM_CREATE,
      ROOM_DELETE: ROOM_DELETE,
      SHARING_ROOM_URL: SHARING_ROOM_URL,
      SHARING_SCREEN: SHARING_SCREEN,
      TWO_WAY_MEDIA_CONN_LENGTH: TWO_WAY_MEDIA_CONN_LENGTH
    });
  },

  /**
   * Returns the app version information for use during feedback.
   *
   * @param {Object}   message Message meant for the handler function, containing
   *                           the following parameters in its `data` property:
   *                           [ ]
   * @param {Function} reply   Callback function, invoked with the result of this
   *                           message handler. The result will be sent back to
   *                           the senders' channel.
   * @return {Object} An object containing:
   *   - channel: The update channel the application is on
   *   - version: The application version
   *   - OS: The operating system the application is running on
   */
  GetAppVersionInfo: function(message, reply) {
    if (!gAppVersionInfo) {
      // If the lazy getter explodes, we're probably loaded in xpcshell,
      // which doesn't have what we need, so log an error.
      try {
        gAppVersionInfo = {
          channel: UpdateUtils.UpdateChannel,
          version: appInfo.version,
          OS: appInfo.OS
        };
      } catch (ex) {
        // Do nothing
      }
    }
    reply(gAppVersionInfo);
  },

  /**
   * Fetch the contents of a specific audio file and return it as a Blob object.
   *
   * @param {Object}   message Message meant for the handler function, containing
   *                           the following parameters in its `data` property:
   *                           [
   *                             {String} name Name of the sound to fetch
   *                           ]
   * @param {Function} reply   Callback function, invoked with the result of this
   *                           message handler. The result will be sent back to
   *                           the senders' channel.
   */
  GetAudioBlob: function(message, reply) {
    let name = message.data[0];
    let request = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"]
                        .createInstance(Ci.nsIXMLHttpRequest);
    let url = `chrome://loop/content/shared/sounds/${name}.ogg`;

    request.open("GET", url, true);
    request.responseType = "arraybuffer";
    request.onload = () => {
      if (request.status < 200 || request.status >= 300) {
        reply(cloneableError(request.status + " " + request.statusText));
        return;
      }

      let blob = new Blob([request.response], { type: "audio/ogg" });
      reply(blob);
    };

    request.send();
  },

  /**
   * Returns the window data for a specific conversation window id.
   *
   * This data will be relevant to the type of window, e.g. rooms or calls.
   * See LoopRooms for more information.
   *
   * @param {Object}   message Message meant for the handler function, containing
   *                           the following parameters in its `data` property:
   *                           [
   *                             {String} conversationWindowId
   *                           ]
   * @param {Function} reply   Callback function, invoked with the result of this
   *                           message handler. The result will be sent back to
   *                           the senders' channel.
   * @returns {Object} The window data or null if error.
   */
  GetConversationWindowData: function(message, reply) {
    reply(MozLoopService.getConversationWindowData(message.data[0]));
  },

  /**
   * Gets the "do not disturb" mode activation flag.
   *
   * @param {Object}   message Message meant for the handler function, containing
   *                           the following parameters in its `data` property:
   *                           [ ]
   * @param {Function} reply   Callback function, invoked with the result of this
   *                           message handler. The result will be sent back to
   *                           the senders' channel.
   */
  GetDoNotDisturb: function(message, reply) {
    reply(MozLoopService.doNotDisturb);
  },

  /**
   * Retrieve the list of errors that are currently pending on the MozLoopService
   * class.
   *
   * @param {Object}   message Message meant for the handler function, containing
   *                           the following parameters in its `data` property:
   *                           [ ]
   * @param {Function} reply   Callback function, invoked with the result of this
   *                           message handler. The result will be sent back to
   *                           the senders' channel.
   */
  GetErrors: function(message, reply) {
    let errors = {};
    for (let [type, error] of MozLoopService.errors) {
      // if error.error is an nsIException, just delete it since it's hard
      // to clone across the boundary.
      if (error.error instanceof Ci.nsIException) {
        MozLoopService.log.debug("Warning: Some errors were omitted from MozLoopAPI.errors " +
                                 "due to issues copying nsIException across boundaries.",
                                 error.error);
        delete error.error;
      }

      errors[type] = cloneableError(error);
    }
    return reply(errors);
  },

  /**
   * Returns true if this profile has an encryption key.
   *
   * @param {Object}   message Message meant for the handler function, containing
   *                           the following parameters in its `data` property:
   *                           [ ]
   * @param {Function} reply   Callback function, invoked with the result of this
   *                           message handler. The result will be sent back to
   *                           the senders' channel.
   * @return {Boolean} True if the profile has an encryption key.
   */
  GetHasEncryptionKey: function(message, reply) {
    reply(MozLoopService.hasEncryptionKey);
  },

  /**
   * Returns the current locale of the browser.
   *
   * @param {Object}   message Message meant for the handler function, containing
   *                           the following parameters in its `data` property:
   *                           [ ]
   * @param {Function} reply   Callback function, invoked with the result of this
   *                           message handler. The result will be sent back to
   *                           the senders' channel.
   * @returns {String} The locale string
   */
  GetLocale: function(message, reply) {
    reply(MozLoopService.locale);
  },

  /**
   * Returns the version number for the addon.
   *
   * @param {Object}   message Message meant for the handler function, containing
   *                           the following parameters in its `data` property:
   *                           [ ]
   * @param {Function} reply   Callback function, invoked with the result of this
   *                           message handler. The result will be sent back to
   *                           the senders' channel.
   * @returns {String} Addon Version string.
   */
  GetAddonVersion: function(message, reply) {
    reply(MozLoopService.addonVersion);
  },

  /**
   * Return any preference under "loop.".
   * Any errors thrown by the Mozilla pref API are logged to the console
   * and cause null to be returned. This includes the case of the preference
   * not being found.
   *
   * @param {Object}   message Message meant for the handler function, containing
   *                           the following parameters in its `data` property:
   *                           [
   *                             {String} prefName The name of the pref without
   *                                               the preceding "loop."
   *                             {Enum}   prefType Type of preference, defined
   *                                               at Ci.nsIPrefBranch. Optional.
   *                           ]
   * @param {Function} reply   Callback function, invoked with the result of this
   *                           message handler. The result will be sent back to
   *                           the senders' channel.
   * @return {*} on success, null on error
   */
  GetLoopPref: function(message, reply) {
    let [prefName, prefType] = message.data;
    reply(MozLoopService.getLoopPref(prefName, prefType));
  },

  /**
   * Retrieve the plural rule number of the active locale.
   *
   * @param {Object}   message Message meant for the handler function, containing
   *                           the following parameters in its `data` property:
   *                           [ ]
   * @param {Function} reply   Callback function, invoked with the result of this
   *                           message handler. The result will be sent back to
   *                           the senders' channel.
   */
  GetPluralRule: function(message, reply) {
    reply(PluralForm.ruleNum);
  },

  /**
   * Gets the metadata related to the currently selected tab in
   * the most recent window.
   *
   * @param {Object}   message Message meant for the handler function, containing
   *                           the following parameters in its `data` property:
   *                           [ ]
   * @param {Function} reply   Callback function, invoked with the result of this
   *                           message handler. The result will be sent back to
   *                           the senders' channel.
   */
  GetSelectedTabMetadata: function(message, reply) {
    let win = Services.wm.getMostRecentWindow("navigator:browser");
    let browser = win && win.gBrowser.selectedBrowser;
    if (!win || !browser) {
      MozLoopService.log.error("Error occurred whilst fetching page metadata");
      reply();
      return;
    }

    // non-remote pages have no metadata
    if (!browser.getAttribute("remote") === "true") {
      reply(null);
    }

    win.messageManager.addMessageListener("PageMetadata:PageDataResult",
                                          function onPageDataResult(msg) {

      win.messageManager.removeMessageListener("PageMetadata:PageDataResult",
                                               onPageDataResult);
      let pageData = msg.json;
      win.LoopUI.getFavicon(function(err, favicon) {
        if (err && err !== "favicon not found for uri") {
          MozLoopService.log.error("Error occurred whilst fetching favicon", err);
          // We don't return here intentionally to make sure the callback is
          // invoked at all times. We just report the error here.
        }
        pageData.favicon = favicon || null;

        reply(pageData);
      });
    });
    win.gBrowser.selectedBrowser.messageManager.sendAsyncMessage("PageMetadata:GetPageData");
  },

  /**
   * Returns a sorted list of Social Providers that can share URLs. See
   * `updateSocialProvidersCache()` for more information.
   *
   * @param {Object}   message Message meant for the handler function, containing
   *                           the following parameters in its `data` property:
   *                           [ ]
   * @param {Function} reply   Callback function, invoked with the result of this
   *                           message handler. The result will be sent back to
   *                           the senders' channel.
   * @return {Array} Sorted list of share-capable Social Providers.
   */
  GetSocialShareProviders: function(message, reply) {
    if (!gSocialProviders) {
      updateSocialProvidersCache();
    }
    reply(gSocialProviders);
  },

  /**
   * Gets an object with data that represents the currently
   * authenticated user's identity.
   *
   * @param {Object}   message Message meant for the handler function, containing
   *                           the following parameters in its `data` property:
   *                           [ ]
   * @param {Function} reply   Callback function, invoked with the result of this
   *                           message handler. The result will be sent back to
   *                           the senders' channel.
   * @return null if user not logged in; profile object otherwise
   */
  GetUserProfile: function(message, reply) {
    if (!MozLoopService.userProfile) {
      reply(null);
      return;
    }

    reply({
      email: MozLoopService.userProfile.email,
      uid: MozLoopService.userProfile.uid
    });
  },

  /**
   * Hangup and close all chat windows that are open.
   *
   * @param {Object}   message Message meant for the handler function, containing
   *                           the following parameters in its `data` property:
   *                           [ ]
   * @param {Function} reply   Callback function, invoked with the result of this
   *                           message handler. The result will be sent back to
   *                           the senders' channel.
   */
  HangupAllChatWindows: function(message, reply) {
    MozLoopService.hangupAllChatWindows();
    reply();
  },

  /**
   * Hangup a specific chay window or room, by leaving a room, resetting the
   * screensharing state and removing any active browser switch listeners.
   *
   * @param {Object}   message Message meant for the handler function, containing
   *                           the following parameters in its `data` property:
   *                           [
   *                             {String} roomToken The token of the room to leave
   *                             {Number} windowId  The window ID of the chat window
   *                           ]
   * @param {Function} reply   Callback function, invoked with the result of this
   *                           message handler. The result will be sent back to
   *                           the senders' channel.
   */
  HangupNow: function(message, reply) {
    let [roomToken, sessionToken, windowId] = message.data;
    if (!windowId) {
      windowId = sessionToken;
    }

    LoopRooms.logDomains(roomToken);
    LoopRooms.leave(roomToken);
    MozLoopService.setScreenShareState(windowId, false);
    LoopAPI.sendMessageToHandler({
      name: "RemoveBrowserSharingListener",
      data: [windowId]
    });
    reply();
  },

  /**
   * Check if the current browser has e10s enabled or not
   *
   * @param {Object}   message Message meant for the handler function, containing
   *                           the following parameters in its `data` property:
   *                           []
   * @param {Function} reply   Callback function, invoked with the result of this
   *                           message handler. The result will be sent back to
   *                           the senders' channel.
   */
  IsMultiProcessActive: function(message, reply) {
    reply(isMultiProcessActive());
  },

  /**
   *  Checks that the current tab can be shared.
   *  Non-shareable tabs are the non-remote ones when e10s is enabled.
   *
   *  @param {Object}   message Message meant for the handler function,
   *                            with no data attached.
   *  @param {Function} reply   Callback function, invoked with the result of
   *                            the check. The result will be sent back to
   *                            the senders' channel.
   */
  IsTabShareable: function(message, reply) {
    let win = Services.wm.getMostRecentWindow("navigator:browser");
    let browser = win && win.gBrowser.selectedBrowser;
    if (!win || !browser) {
      reply(false);
      return;
    }

    let e10sActive = isMultiProcessActive();
    let tabRemote = browser.getAttribute("remote") === "true";

    reply(!e10sActive || (e10sActive && tabRemote));
  },

  /**
   * Start the FxA login flow using the OAuth client and params from the Loop
   * server.
   *
   * @param {Object}   message Message meant for the handler function, containing
   *                           the following parameters in its `data` property:
   *                           [
   *                             {Boolean} forceReAuth Set to true to force FxA
   *                                                   into a re-auth even if the
   *                                                   user is already logged in.
   *                           ]
   * @param {Function} reply   Callback function, invoked with the result of this
   *                           message handler. The result will be sent back to
   *                           the senders' channel.
   * @return {Promise} Returns a promise that is resolved on successful
   *                   completion, or rejected otherwise.
   */
  LoginToFxA: function(message, reply) {
    let forceReAuth = message.data[0];
    MozLoopService.logInToFxA(forceReAuth);
    reply();
  },

  /**
   * Logout completely from FxA.
   * @see MozLoopService#logOutFromFxA
   *
   * @param {Object}   message Message meant for the handler function, containing
   *                           the following parameters in its `data` property:
   *                           [ ]
   * @param {Function} reply   Callback function, invoked with the result of this
   *                           message handler. The result will be sent back to
   *                           the senders' channel.
   */
  LogoutFromFxA: function(message, reply) {
    MozLoopService.logOutFromFxA();
    reply();
  },

  /**
   * Notifies the UITour module that an event occurred that it might be
   * interested in.
   *
   * @param {Object}   message Message meant for the handler function, containing
   *                           the following parameters in its `data` property:
   *                           [
   *                             {String} subject  Subject of the notification
   *                             {mixed}  [params] Optional parameters, providing
   *                                               more details to the notification
   *                                               subject
   *                           ]
   * @param {Function} reply   Callback function, invoked with the result of this
   *                           message handler. The result will be sent back to
   *                           the senders' channel.
   */
  NotifyUITour: function(message, reply) {
    let [subject, params] = message.data;
    UITour.notify(subject, params);
    reply();
  },

  /**
   * Opens the Getting Started tour in the browser.
   *
   * @param {Object}   message Message meant for the handler function, containing
   *                           the following parameters in its `data` property:
   *                           []
   * @param {Function} reply   Callback function, invoked with the result of this
   *                           message handler. The result will be sent back to
   *                           the senders' channel.
   */
  OpenGettingStartedTour: function(message, reply) {
    MozLoopService.openGettingStartedTour();
    reply();
  },

  /**
   * Retrieves the Getting Started tour url.
   *
   * @param {Object}   message Message meant for the handler function, containing
   *                           the following parameters in its `data` property:
   *                           [aSrc, aAdditionalParams]
   * @param {Function} reply   Callback function, invoked with the result of this
   *                           message handler. The result will be sent back to
   *                           the senders' channel.
   */
  GettingStartedURL: function(message, reply) {
    let aSrc = message.data[0] || null;
    let aAdditionalParams = message.data[1] || {};
    reply(MozLoopService.getTourURL(aSrc, aAdditionalParams).href);
  },

  /**
   * Open the FxA profile/ settings page.
   *
   * @param {Object}   message Message meant for the handler function, containing
   *                           the following parameters in its `data` property:
   *                           [ ]
   * @param {Function} reply   Callback function, invoked with the result of this
   *                           message handler. The result will be sent back to
   *                           the senders' channel.
   */
  OpenFxASettings: function(message, reply) {
    MozLoopService.openFxASettings();
    reply();
  },

  /**
   * Opens a non e10s window
   *
   * @param {Object}   message Message meant for the handler function, containing
   *                           the following parameters in its `data` property:
   *                           [url]
   * @param {Function} reply   Callback function, invoked with the result of this
   *                           message handler. The result will be sent back to
   *                           the senders' channel.
   */
  OpenNonE10sWindow: function(message, reply) {
    let win = Services.wm.getMostRecentWindow("navigator:browser");
    let url = message.data[0] ? message.data[0] : "about:home";
    win.openDialog("chrome://browser/content/", "_blank", "chrome,all,dialog=no,non-remote", url);
    reply();
  },

  /**
   * Opens a URL in a new tab in the browser.
   *
   * @param {Object}   message Message meant for the handler function, containing
   *                           the following parameters in its `data` property:
   *                           [
   *                             {String} url The new url to open
   *                           ]
   * @param {Function} reply   Callback function, invoked with the result of this
   *                           message handler. The result will be sent back to
   *                           the senders' channel.
   */
  OpenURL: function(message, reply) {
    let url = message.data[0];
    MozLoopService.openURL(url);
    reply();
  },

  /**
   * Removes a listener that was previously added.
   *
   * @param {Object}   message Message meant for the handler function, containing
   *                           the following parameters in its `data` property:
   *                           [
   *                             {Number} windowId The window ID of the chat
   *                           ]
   * @param {Function} reply   Callback function, invoked with the result of this
   *                           message handler. The result will be sent back to
   *                           the senders' channel.
   */
  RemoveBrowserSharingListener: function(message, reply) {
    if (!gBrowserSharingListeners.size) {
      reply();
      return;
    }

    let [windowId] = message.data;
    gBrowserSharingListeners.delete(windowId);
    if (gBrowserSharingListeners.size > 0) {
      // There are still clients listening in, so keep on listening...
      reply();
      return;
    }

    for (let win of gBrowserSharingWindows) {
      win = win.get();
      if (!win) {
        continue;
      }
      win.LoopUI.stopBrowserSharing();
    }

    NewTabURL.reset();

    gBrowserSharingWindows.clear();
    reply();
  },

  "Rooms:*": function(action, message, reply) {
    LoopAPIInternal.handleObjectAPIMessage(LoopRooms, kRoomsPushPrefix,
      action, message, reply);
  },

  /**
   * Sets the "do not disturb" mode activation flag.
   *
   * @param {Object}   message Message meant for the handler function, containing
   *                           the following parameters in its `data` property:
   *                           [ ]
   * @param {Function} reply   Callback function, invoked with the result of this
   *                           message handler. The result will be sent back to
   *                           the senders' channel.
   */
  SetDoNotDisturb: function(message, reply) {
    MozLoopService.doNotDisturb = message.data[0];
    reply();
  },

  /**
   * Set any preference under "loop."
   * Any errors thrown by the Mozilla pref API are logged to the console
   * and cause false to be returned.
   *
   * @param {Object}   message Message meant for the handler function, containing
   *                           the following parameters in its `data` property:
   *                           [
   *                             {String} prefName The name of the pref without
   *                                               the preceding "loop."
   *                             {*}      value    The value to set.
   *                             {Enum}   prefType Type of preference, defined at
   *                                               Ci.nsIPrefBranch. Optional.
   *                           ]
   * @param {Function} reply   Callback function, invoked with the result of this
   *                           message handler. The result will be sent back to
   *                           the senders' channel.
   */
  SetLoopPref: function(message, reply) {
    let [prefName, value, prefType] = message.data;
    MozLoopService.setLoopPref(prefName, value, prefType);
    reply();
  },

  /**
   * Called when a closing room has just been created, so user can change
   * the name of the room to be stored.
   *
   * @param {Object}   message Message meant for the handler function, shouldn't
                               contain any data.
   * @param {Function} reply   Callback function, invoked with the result of this
   *                           message handler. The result will be sent back to
   *                           the senders' channel.
   */
  SetNameNewRoom: function(message, reply) {
    let win = Services.wm.getMostRecentWindow("navigator:browser");
    win && win.LoopUI.renameRoom();

    reply();
  },

  /**
   * Used to record the screen sharing state for a window so that it can
   * be reflected on the toolbar button.
   *
   * @param {Object}   message Message meant for the handler function, containing
   *                           the following parameters in its `data` property:
   *                           [
   *                             {String} windowId The id of the conversation window
   *                                               the state is being changed for.
   *                             {Boolean} active  Whether or not screen sharing
   *                                               is now active.
   *                           ]
   * @param {Function} reply   Callback function, invoked with the result of this
   *                           message handler. The result will be sent back to
   *                           the senders' channel.
   */
  SetScreenShareState: function(message, reply) {
    let [windowId, active] = message.data;
    MozLoopService.setScreenShareState(windowId, active);
    reply();
  },

  /**
   * Share a room URL with the Social API.
   *
   * @param {Object}   message Message meant for the handler function, containing
   *                           the following parameters in its `data` property:
   *                           [
   *                             {String} providerOrigin URL fragment that identifies
   *                                                     a social provider
   *                             {String} roomURL        URL of a room
   *                             {String} title          Title of the sharing message
   *                             {String} body           Body of the sharing message
   *                           ]
   * @param {Function} reply   Callback function, invoked with the result of this
   *                           message handler. The result will be sent back to
   *                           the senders' channel.
   */
  SocialShareRoom: function(message, reply) {
    let win = Services.wm.getMostRecentWindow("navigator:browser");
    if (!win || !win.SocialShare) {
      reply();
      return;
    }

    let [providerOrigin, roomURL, title, body] = message.data;
    let graphData = {
      url: roomURL,
      title: title
    };
    if (body) {
      graphData.body = body;
    }
    win.SocialShare.sharePage(providerOrigin, graphData, null,
      win.LoopUI.toolbarButton.anchor);
    reply();
  },

  /**
   * Adds a value to a telemetry histogram.
   *
   * @param {Object}   message Message meant for the handler function, containing
   *                           the following parameters in its `data` property:
   *                           [
   *                             {String} histogramId Name of the telemetry histogram
   *                                                  to update.
   *                             {String} value       Label of bucket to increment
   *                                                   in the histogram.
   *                           ]
   * @param {Function} reply   Callback function, invoked with the result of this
   *                           message handler. The result will be sent back to
   *                           the senders' channel.
   */
  TelemetryAddValue: function(message, reply) {
    let [histogramId, value] = message.data;

    if (histogramId === "LOOP_ACTIVITY_COUNTER") {
      let pref = "mau." + kMauPrefMap.get(value);
      let prefDate = MozLoopService.getLoopPref(pref) * 1000;
      let delta = Date.now() - prefDate;

      // Send telemetry event if period (30 days) passed.
      // 0 is default value for pref.
      // 2592000 seconds in 30 days
      if (pref === 0 || delta >= 2592000 * 1000) {
        try {
          Services.telemetry.getHistogramById(histogramId).add(value);
        } catch (ex) {
          MozLoopService.log.error("TelemetryAddValue failed for histogram '" + histogramId + "'", ex);
        }
        MozLoopService.setLoopPref(pref, Math.floor(Date.now() / 1000));
      }
    } else {
      try {
        Services.telemetry.getHistogramById(histogramId).add(value);
      } catch (ex) {
        MozLoopService.log.error("TelemetryAddValue failed for histogram '" + histogramId + "'", ex);
      }
    }
    reply();
  }
};

const LoopAPIInternal = {
  /**
   * Initialize the Loop API, which means:
   * 1) setup RemotePageManager to hook into loop documents as channels and
   *    start listening for messages therein.
   * 2) start listening for other events that may be interesting.
   */
  initialize: function() {
    if (gPageListeners) {
      return;
    }

    Cu.import("resource://gre/modules/RemotePageManager.jsm");

    gPageListeners = [new RemotePages("about:looppanel"),
      new RemotePages("about:loopconversation"),
      // Slideshow added here to expose the loop api to make L10n work.
      // XXX Can remove once slideshow is made remote.
      new RemotePages("chrome://loop/content/panels/slideshow.html")];
    for (let page of gPageListeners) {
      page.addMessageListener(kMessageName, this.handleMessage.bind(this));
    }

    // Subscribe to global events:
    Services.obs.addObserver(this.handleStatusChanged, "loop-status-changed", false);
  },

  /**
   * Handles incoming messages from RemotePageManager that are sent from Loop
   * content pages.
   *
   * @param {Object} message Object containing the following fields:
   *                         - {MessageManager} target Where the message came from
   *                         - {String}         name   Name of the message
   *                         - {Array}          data   Payload of the message
   * @param {Function} [reply]
   */
  handleMessage: function(message, reply) {
    let seq = message.data.shift();
    let action = message.data.shift();

    let actionParts = action.split(":");

    // The name that is supposed to match with a handler function is tucked inside
    // the second part of the message name. If all is well.
    let handlerName = actionParts.shift();

    if (!reply) {
      reply = result => {
        try {
          message.target.sendAsyncMessage(message.name, [seq, result]);
        } catch (ex) {
          MozLoopService.log.error("Failed to send reply back to content:", ex);
        }
      };
    }

    // First, check if this is a batch call.
    if (handlerName == kBatchMessage) {
      this.handleBatchMessage(seq, message, reply);
      return;
    }

    // Second, check if the message is meant for one of our Object APIs.
    // If so, a wildcard entry should exist for the message name in the
    // `kMessageHandlers` dictionary.
    let wildcardName = handlerName + ":*";
    if (kMessageHandlers[wildcardName]) {
      // A unit test might've stubbed the handler.
      if (gStubbedMessageHandlers && gStubbedMessageHandlers[wildcardName]) {
        gStubbedMessageHandlers[wildcardName](action, message, reply);
      } else {
        // Alright, pass the message forward.
        kMessageHandlers[wildcardName](action, message, reply);
      }
      // Aaaaand we're done.
      return;
    }

    // A unit test might've stubbed the handler.
    if (gStubbedMessageHandlers && gStubbedMessageHandlers[handlerName]) {
      gStubbedMessageHandlers[handlerName](message, reply);
      return;
    }

    if (!kMessageHandlers[handlerName]) {
      let msg = "Ouch, no message handler available for '" + handlerName + "'";
      MozLoopService.log.error(msg);
      reply(cloneableError(msg));
      return;
    }

    kMessageHandlers[handlerName](message, reply);
  },

  /**
   * If `sendMessage` above detects that the incoming message consists of a whole
   * set of messages, this function is tasked with handling them.
   * It iterates over all the messages, sends each to their appropriate handler
   * and collects their results. The results will be sent back in one go as response
   * to the batch message.
   *
   * @param {Number} seq       Sequence ID of this message
   * @param {Object} message   Message containing the following parameters in
   *                           its `data` property:
   *                           [
   *                             {Array} requests Sequence of messages
   *                           ]
   * @param {Function} reply   Callback function, invoked with the result of this
   *                           message handler. The result will be sent back to
   *                           the senders' channel.
   */
  handleBatchMessage: function(seq, message, reply) {
    let requests = message.data[0];
    if (!requests.length) {
      MozLoopService.log.error("Ough, a batch call with no requests is not much " +
        "of a batch, now is it?");
      return;
    }

    // Since `handleBatchMessage` can be called recursively, but the replies are
    // collected and sent only once, we'll make sure only one exists for the
    // entire tail.
    // We count the amount of recursive calls, because we don't want any consumer
    // to cause an infinite loop, now do we?
    if (!("loopCount" in reply)) {
      reply.loopCount = 0;
    } else if (++reply.loopCount > kMaxLoopCount) {
      reply(cloneableError("Too many nested calls"));
      return;
    }

    let resultSet = {};
    Promise.all(requests.map(requestSet => {
      let requestSeq = requestSet[0];
      return new Promise(resolve => this.handleMessage({ data: requestSet }, result => {
        resultSet[requestSeq] = result;
        resolve();
      }));
    })).then(() => reply(resultSet));
  },

  /**
   * Separate handler that is specialized in dealing with messages meant for sub-APIs,
   * like LoopRooms.
   *
   * @param {Object}   api               Pointer to the sub-API.
   * @param {String}   pushMessagePrefix
   * @param {String}   action            Action name that translates to a function
   *                                     name present on the sub-API.
   * @param {Object}   message           Message containing parameters required to
   *                                     perform the action on the sub-API  in its
   *                                     `data` property.
   * @param {Function} reply             Callback function, invoked with the result
   *                                     of this message handler. The result will
   *                                     be sent back to the senders' channel.
   */
  handleObjectAPIMessage: function(api, pushMessagePrefix, action, message, reply) {
    let funcName = getObjectAPIFunctionName(action);

    if (funcName == kPushSubscription) {
      // Incoming event listener request!
      let events = message.data[0];
      if (!events || !events.length) {
        let msg = "Oops, don't forget to pass in event names when you try to " +
          "subscribe to them!";
        MozLoopService.log.error(msg);
        reply(cloneableError(msg));
        return;
      }

      let handlerFunc = (e, ...data) => {
        let prettyEventName = e.charAt(0).toUpperCase() + e.substr(1);
        try {
          message.target.sendAsyncMessage(kPushMessageName, [pushMessagePrefix +
            prettyEventName, data]);
        } catch (ex) {
          MozLoopService.log.debug("Unable to send event through to target: " +
            ex.message);
          // Unregister event handlers when the message port is unreachable.
          for (let eventName of events) {
            api.off(eventName, handlerFunc);
          }
        }
      };

      for (let eventName of events) {
        api.on(eventName, handlerFunc);
      }
      reply();
      return;
    }

    if (typeof api[funcName] != "function") {
      reply(cloneableError("Sorry, function '" + funcName + "' does not exist!"));
      return;
    }
    api[funcName](...message.data, (err, result) => {
      reply(err ? cloneableError(err) : result);
    });
  },

  /**
   * Observer function for the 'loop-status-changed' event.
   */
  handleStatusChanged: function() {
    LoopAPIInternal.broadcastPushMessage("LoopStatusChanged");
  },

  /**
   * Send an event to the content window to indicate that the state on the chrome
   * side was updated.
   *
   * @param {name} name Name of the event
   */
  broadcastPushMessage: function(name, data) {
    if (!gPageListeners) {
      return;
    }
    for (let page of gPageListeners) {
      try {
        page.sendAsyncMessage(kPushMessageName, [name, data]);
      } catch (ex) {
        // Only make noise when the Remote Page Manager needs more time to
        // initialize.
        if (ex.result != Components.results.NS_ERROR_NOT_INITIALIZED) {
          throw ex;
        }
      }
    }
  },

  /**
   * De the reverse of `initialize` above; unhook page and event listeners.
   */
  destroy: function() {
    if (!gPageListeners) {
      return;
    }
    for (let listener of gPageListeners) {
      listener.destroy();
    }
    gPageListeners = null;

    // Unsubscribe from global events.
    Services.obs.removeObserver(this.handleStatusChanged, "loop-status-changed");
    // Stop listening for changes in the social provider list, if necessary.
    if (gSocialProviders) {
      Services.obs.removeObserver(updateSocialProvidersCache, "social:providers-changed");
    }
  }
};

this.LoopAPI = Object.freeze({
  /* @see LoopAPIInternal#initialize */
  initialize: function() {
    LoopAPIInternal.initialize();
  },
  /* @see LoopAPIInternal#broadcastPushMessage */
  broadcastPushMessage: function(name, data) {
    LoopAPIInternal.broadcastPushMessage(name, data);
  },
  /* @see LoopAPIInternal#destroy */
  destroy: function() {
    LoopAPIInternal.destroy();
  },
  /**
   * Gateway for chrome scripts to send a message to a message handler, when
   * using the RemotePageManager module is not an option.
   *
   * @param {Object}   message Message meant for the handler function, containing
   *                           the following properties:
   *                           - {String} name     Name of handler to send this
   *                                               message to. See `kMessageHandlers`
   *                                               for the available names.
   *                           - {String} [action] Optional action name of the
   *                                               function to call on a sub-API.
   *                           - {Array}  data     List of arguments that the
   *                                               handler can use.
   * @param {Function} [reply] Callback function, invoked with the result of this
   *                           message handler. Optional.
   */
  sendMessageToHandler: function(message, reply) {
    reply = reply || function() {};
    let handlerName = message.name;
    let handler = kMessageHandlers[handlerName];
    if (gStubbedMessageHandlers && gStubbedMessageHandlers[handlerName]) {
      handler = gStubbedMessageHandlers[handlerName];
    }
    if (!handler) {
      let msg = "Ouch, no message handler available for '" + handlerName + "'";
      MozLoopService.log.error(msg);
      reply(cloneableError(msg));
      return;
    }

    if (!message.data) {
      message.data = [];
    }

    if (handlerName.endsWith("*")) {
      handler(message.action, message, reply);
    } else {
      handler(message, reply);
    }
  },
  // The following functions are only used in unit tests.
  inspect: function() {
    return [Object.create(LoopAPIInternal), Object.create(kMessageHandlers),
      gPageListeners ? [...gPageListeners] : null];
  },
  stub: function(pageListeners) {
    if (!gOriginalPageListeners) {
      gOriginalPageListeners = gPageListeners;
    }
    gPageListeners = pageListeners;
  },
  stubMessageHandlers: function(handlers) {
    gStubbedMessageHandlers = handlers;
  },
  restore: function() {
    if (gOriginalPageListeners) {
      gPageListeners = gOriginalPageListeners;
    }
    gStubbedMessageHandlers = null;
  }
});
