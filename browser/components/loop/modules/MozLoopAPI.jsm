/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://services-common/utils.js");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource:///modules/loop/LoopCalls.jsm");
Cu.import("resource:///modules/loop/MozLoopService.jsm");
Cu.import("resource:///modules/loop/LoopRooms.jsm");
Cu.import("resource:///modules/loop/LoopContacts.jsm");
Cu.importGlobalProperties(["Blob"]);

XPCOMUtils.defineLazyModuleGetter(this, "LoopContacts",
                                        "resource:///modules/loop/LoopContacts.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "LoopStorage",
                                        "resource:///modules/loop/LoopStorage.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "hookWindowCloseForPanelClose",
                                        "resource://gre/modules/MozSocialAPI.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PageMetadata",
                                        "resource://gre/modules/PageMetadata.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PluralForm",
                                        "resource://gre/modules/PluralForm.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "UpdateChannel",
                                        "resource://gre/modules/UpdateChannel.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "UITour",
                                        "resource:///modules/UITour.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Social",
                                        "resource:///modules/Social.jsm");
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
this.EXPORTED_SYMBOLS = ["injectLoopAPI"];

/**
 * Trying to clone an Error object into a different container will yield an error.
 * We can work around this by copying the properties we care about onto a regular
 * object.
 *
 * @param {Error|nsIException} error        Error object to copy
 * @param {nsIDOMWindow}       targetWindow The content window to clone into
 */
const cloneErrorObject = function(error, targetWindow) {
  let obj = new targetWindow.Error();
  let props = Object.getOwnPropertyNames(error);
  // nsIException properties are not enumerable, so we'll try to copy the most
  // common and useful ones.
  if (!props.length) {
    props.push("message", "filename", "lineNumber", "columnNumber", "stack");
  }
  for (let prop of props) {
    let value = error[prop];
    // for nsIException objects, the property may not be defined.
    if (typeof value == "undefined") {
      continue;
    }
    if (typeof value != "string" && typeof value != "number") {
      value = String(value);
    }

    Object.defineProperty(Cu.waiveXrays(obj), prop, {
      configurable: false,
      enumerable: true,
      value: value,
      writable: false
    });
  }
  return obj;
};

/**
 * Makes an object or value available to an unprivileged target window.
 *
 * Primitives are returned as they are, while objects are cloned into the
 * specified target.  Error objects are also handled correctly.
 *
 * @param {any}          value        Value or object to copy
 * @param {nsIDOMWindow} targetWindow The content window to copy to
 */
const cloneValueInto = function(value, targetWindow) {
  if (!value || typeof value != "object") {
    return value;
  }

  // HAWK request errors contain an nsIException object inside `value`.
  if (("error" in value) && (value.error instanceof Ci.nsIException)) {
    value = value.error;
  }

  // Strip Function properties, since they can not be cloned across boundaries
  // like this.
  for (let prop of Object.getOwnPropertyNames(value)) {
    if (typeof value[prop] == "function") {
      delete value[prop];
    }
  }

  // Inspect for an error this way, because the Error object is special.
  if (value.constructor.name == "Error" || value instanceof Ci.nsIException) {
    return cloneErrorObject(value, targetWindow);
  }

  let clone;
  try {
    clone = Cu.cloneInto(value, targetWindow);
  } catch (ex) {
    MozLoopService.log.debug("Failed to clone value:", value);
    throw ex;
  }

  return clone;
};

/**
 * Get the two-digit hexadecimal code for a byte
 *
 * @param {byte} charCode
 */
const toHexString = function(charCode) {
  return ("0" + charCode.toString(16)).slice(-2);
};

/**
 * Inject any API containing _only_ function properties into the given window.
 *
 * @param {Object}       api          Object containing functions that need to
 *                                    be exposed to content
 * @param {nsIDOMWindow} targetWindow The content window to attach the API
 */
const injectObjectAPI = function(api, targetWindow) {
  let injectedAPI = {};
  // Wrap all the methods in `api` to help results passed to callbacks get
  // through the priv => unpriv barrier with `Cu.cloneInto()`.
  Object.keys(api).forEach(func => {
    injectedAPI[func] = function(...params) {
      let lastParam = params.pop();
      let callbackIsFunction = (typeof lastParam == "function");
      // Clone params coming from content to the current scope.
      params = [cloneValueInto(p, api) for (p of params)];

      // If the last parameter is a function, assume its a callback
      // and wrap it differently.
      if (callbackIsFunction) {
        api[func](...params, function callback(...results) {
          // When the function was garbage collected due to async events, like
          // closing a window, we want to circumvent a JS error.
          if (callbackIsFunction && typeof lastParam != "function") {
            MozLoopService.log.debug(func + ": callback function was lost.");
            // Clean up event listeners.
            if (func == "on" && api.off) {
              api.off(results[0], callback);
              return;
            }
            // Assume the presence of a first result argument to be an error.
            if (results[0]) {
              MozLoopService.log.error(func + " error:", results[0]);
            }
            return;
          }
          lastParam(...[cloneValueInto(r, targetWindow) for (r of results)]);
        });
      } else {
        try {
          lastParam = cloneValueInto(lastParam, api);
          return cloneValueInto(api[func](...params, lastParam), targetWindow);
        } catch (ex) {
          MozLoopService.log.error(func + " error: ", ex, params, lastParam);
          return cloneValueInto(ex, targetWindow);
        }
      }
    };
  });

  let contentObj = Cu.cloneInto(injectedAPI, targetWindow, {cloneFunctions: true});
  // Since we deny preventExtensions on XrayWrappers, because Xray semantics make
  // it difficult to act like an object has actually been frozen, we try to seal
  // the `contentObj` without Xrays.
  try {
    Object.seal(Cu.waiveXrays(contentObj));
  } catch (ex) {}
  return contentObj;
};

/**
 * Inject the loop API into the given window.  The caller must be sure the
 * window is a loop content window (eg, a panel, chatwindow, or similar).
 *
 * See the documentation on the individual functions for details of the API.
 *
 * @param {nsIDOMWindow} targetWindow The content window to attach the API.
 */
function injectLoopAPI(targetWindow) {
  let ringer;
  let ringerStopper;
  let appVersionInfo;
  let contactsAPI;
  let roomsAPI;
  let callsAPI;
  let savedWindowListeners = new Map();
  let socialProviders;
  const kShareWidgetId = "social-share-button";
  let socialShareButtonListenersAdded = false;


  let api = {
    /**
     * Gets an object with data that represents the currently
     * authenticated user's identity.
     *
     * @return null if user not logged in; profile object otherwise
     */
    userProfile: {
      enumerable: true,
      get: function() {
        if (!MozLoopService.userProfile)
          return null;
        let userProfile = Cu.cloneInto({
          email: MozLoopService.userProfile.email,
          uid: MozLoopService.userProfile.uid
        }, targetWindow);
        return userProfile;
      }
    },

    /**
     * Sets and gets the "do not disturb" mode activation flag.
     */
    doNotDisturb: {
      enumerable: true,
      get: function() {
        return MozLoopService.doNotDisturb;
      },
      set: function(aFlag) {
        MozLoopService.doNotDisturb = aFlag;
      }
    },

    errors: {
      enumerable: true,
      get: function() {
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

          // We have to clone the error property since it may be an Error object.
          if (error.hasOwnProperty("toString")) {
            delete error.toString;
          }
          errors[type] = Cu.waiveXrays(Cu.cloneInto(error, targetWindow, { cloneFunctions: true }));
        }
        return Cu.cloneInto(errors, targetWindow, { cloneFunctions: true });
      },
    },

    /**
     * Returns the current locale of the browser.
     *
     * @returns {String} The locale string
     */
    locale: {
      enumerable: true,
      get: function() {
        return MozLoopService.locale;
      }
    },

    /**
     * Adds a listener to the most recent window for browser/tab sharing. The
     * listener will be notified straight away of the current tab id, then every
     * time there is a change of tab.
     *
     * Listener parameters:
     * - {Object}  err      If there is a error this will be defined, null otherwise.
     * - {Number} windowId The new windowId after a change of tab.
     *
     * @param {Function} listener The listener to handle the windowId changes.
     */
    addBrowserSharingListener: {
      enumerable: true,
      writable: true,
      value: function(listener) {
        let win = Services.wm.getMostRecentWindow("navigator:browser");
        let browser = win && win.gBrowser.selectedBrowser;
        if (!win || !browser) {
          // This may happen when an undocked conversation window is the only
          // window left.
          let err = new Error("No tabs available to share.");
          MozLoopService.log.error(err);
          listener(cloneValueInto(err, targetWindow));
          return;
        }
        if (browser.getAttribute("remote") == "true") {
          // Tab sharing is not supported yet for e10s-enabled browsers. This will
          // be fixed in bug 1137634.
          let err = new Error("Tab sharing is not supported for e10s-enabled browsers");
          MozLoopService.log.error(err);
          listener(cloneValueInto(err, targetWindow));
          return;
        }

        win.LoopUI.addBrowserSharingListener(listener);

        savedWindowListeners.set(listener, Cu.getWeakReference(win));
      }
    },

    /**
     * Removes a listener that was previously added.
     *
     * @param {Function} listener The listener to handle the windowId changes.
     */
    removeBrowserSharingListener: {
      enumerable: true,
      writable: true,
      value: function(listener) {
        if (!savedWindowListeners.has(listener)) {
          return;
        }

        let win = savedWindowListeners.get(listener).get();

        // Remove the element, regardless of if the window exists or not so
        // that we clean the map.
        savedWindowListeners.delete(listener);

        if (!win) {
          return;
        }

        win.LoopUI.removeBrowserSharingListener(listener);
      }
    },

    /**
     * Returns the window data for a specific conversation window id.
     *
     * This data will be relevant to the type of window, e.g. rooms or calls.
     * See LoopRooms or LoopCalls for more information.
     *
     * @param {String} conversationWindowId
     * @returns {Object} The window data or null if error.
     */
    getConversationWindowData: {
      enumerable: true,
      writable: true,
      value: function(conversationWindowId) {
        return cloneValueInto(MozLoopService.getConversationWindowData(conversationWindowId),
          targetWindow);
      }
    },

    /**
     * Returns the contacts API.
     *
     * @returns {Object} The contacts API object
     */
    contacts: {
      enumerable: true,
      get: function() {
        if (contactsAPI) {
          return contactsAPI;
        }

        // Make a database switch when a userProfile is active already.
        let profile = MozLoopService.userProfile;
        if (profile) {
          LoopStorage.switchDatabase(profile.uid);
        }
        return contactsAPI = injectObjectAPI(LoopContacts, targetWindow);
      }
    },

    /**
     * Returns the rooms API.
     *
     * @returns {Object} The rooms API object
     */
    rooms: {
      enumerable: true,
      get: function() {
        if (roomsAPI) {
          return roomsAPI;
        }
        return roomsAPI = injectObjectAPI(LoopRooms, targetWindow);
      }
    },

    /**
     * Returns the calls API.
     *
     * @returns {Object} The rooms API object
     */
    calls: {
      enumerable: true,
      get: function() {
        if (callsAPI) {
          return callsAPI;
        }

        return callsAPI = injectObjectAPI(LoopCalls, targetWindow);
      }
    },

    /**
     * Import a list of (new) contacts from an external data source.
     *
     * @param {Object}   options  Property bag of options for the importer
     * @param {Function} callback Function that will be invoked once the operation
     *                            finished. The first argument passed will be an
     *                            `Error` object or `null`. The second argument will
     *                            be the result of the operation, if successfull.
     */
    startImport: {
      enumerable: true,
      writable: true,
      value: function(options, callback) {
        LoopContacts.startImport(options, getChromeWindow(targetWindow), function(...results) {
          callback(...[cloneValueInto(r, targetWindow) for (r of results)]);
        });
      }
    },

    /**
     * Returns translated strings associated with an element. Designed
     * for use with l10n.js
     *
     * @param {String} key The element id
     * @returns {Object} A JSON string containing the localized
     *                   attribute/value pairs for the element.
     */
    getStrings: {
      enumerable: true,
      writable: true,
      value: function(key) {
        return MozLoopService.getStrings(key);
      }
    },

    /**
     * Returns the correct form of a semi-colon separated string
     * based on the value of the `num` argument and the current locale.
     *
     * @param {Integer} num The number used to find the plural form.
     * @param {String} str The semi-colon separated string of word forms.
     * @returns {String} The correct word form based on the value of the number
     *                   and the current locale.
     */
    getPluralForm: {
      enumerable: true,
      writable: true,
      value: function(num, str) {
        return PluralForm.get(num, str);
      }
    },

    /**
     * Displays a confirmation dialog using the specified strings.
     *
     * @param {Object}   options  Confirm dialog options
     * @param {Function} callback Function that will be invoked once the operation
     *                            finished. The first argument passed will be an
     *                            `Error` object or `null`. The second argument
     *                            will be the result of the operation, TRUE if
     *                            the user chose the OK button.
     */
    confirm: {
      enumerable: true,
      writable: true,
      value: function(options, callback) {
        let buttonFlags;
        if (options.okButton && options.cancelButton) {
          buttonFlags =
            (Ci.nsIPrompt.BUTTON_POS_0 * Ci.nsIPrompt.BUTTON_TITLE_IS_STRING) +
            (Ci.nsIPrompt.BUTTON_POS_1 * Ci.nsIPrompt.BUTTON_TITLE_IS_STRING);
        } else if (!options.okButton && !options.cancelButton) {
          buttonFlags = Services.prompt.STD_YES_NO_BUTTONS;
        } else {
          callback(cloneValueInto(new Error("confirm: missing button options"), targetWindow));
        }

        try {
          let chosenButton = Services.prompt.confirmEx(null, "",
            options.message, buttonFlags, options.okButton, options.cancelButton,
            null, null, {});

          callback(null, chosenButton == 0);
        } catch (ex) {
          callback(cloneValueInto(ex, targetWindow));
        }
      }
    },

    /**
     * Set any preference under "loop."
     *
     * @param {String} prefName The name of the pref without the preceding "loop."
     * @param {*} value The value to set.
     * @param {Enum} prefType Type of preference, defined at Ci.nsIPrefBranch. Optional.
     *
     * Any errors thrown by the Mozilla pref API are logged to the console
     * and cause false to be returned.
     */
    setLoopPref: {
      enumerable: true,
      writable: true,
      value: function(prefName, value, prefType) {
        MozLoopService.setLoopPref(prefName, value, prefType);
      }
    },

    /**
     * Return any preference under "loop.".
     *
     * @param {String} prefName The name of the pref without the preceding
     * "loop."
     * @param {Enum} prefType Type of preference, defined at Ci.nsIPrefBranch. Optional.
     *
     * Any errors thrown by the Mozilla pref API are logged to the console
     * and cause null to be returned. This includes the case of the preference
     * not being found.
     *
     * @return {*} on success, null on error
     */
    getLoopPref: {
      enumerable: true,
      writable: true,
      value: function(prefName, prefType) {
        return MozLoopService.getLoopPref(prefName);
      }
    },

    /**
     * Starts alerting the user about an incoming call
     */
    startAlerting: {
      enumerable: true,
      writable: true,
      value: function() {
        let chromeWindow = getChromeWindow(targetWindow);
        chromeWindow.getAttention();
        ringer = new chromeWindow.Audio();
        ringer.src = Services.prefs.getCharPref("loop.ringtone");
        ringer.loop = true;
        ringer.load();
        ringer.play();
        targetWindow.document.addEventListener("visibilitychange",
          ringerStopper = function(event) {
            if (event.currentTarget.hidden) {
              api.stopAlerting.value();
            }
          });
      }
    },

    /**
     * Stops alerting the user about an incoming call
     */
    stopAlerting: {
      enumerable: true,
      writable: true,
      value: function() {
        if (ringerStopper) {
          targetWindow.document.removeEventListener("visibilitychange",
                                                    ringerStopper);
          ringerStopper = null;
        }
        if (ringer) {
          ringer.pause();
          ringer = null;
        }
      }
    },

    /**
     * Performs a hawk based request to the loop server.
     *
     * Callback parameters:
     *  - {Object|null} null if success. Otherwise an object:
     *    {
     *      code: 401,
     *      errno: 401,
     *      error: "Request failed",
     *      message: "invalid token"
     *    }
     *  - {String} The body of the response.
     *
     * @param {LOOP_SESSION_TYPE} sessionType The type of session to use for
     *                                        the request.  This is one of the
     *                                        LOOP_SESSION_TYPE members
     * @param {String} path The path to make the request to.
     * @param {String} method The request method, e.g. 'POST', 'GET'.
     * @param {Object} payloadObj An object which is converted to JSON and
     *                            transmitted with the request.
     * @param {Function} callback Called when the request completes.
     */
    hawkRequest: {
      enumerable: true,
      writable: true,
      value: function(sessionType, path, method, payloadObj, callback) {
        // XXX Should really return a DOM promise here.
        let callbackIsFunction = (typeof callback == "function");
        MozLoopService.hawkRequest(sessionType, path, method, payloadObj).then((response) => {
          callback(null, response.body);
        }, hawkError => {
          // When the function was garbage collected due to async events, like
          // closing a window, we want to circumvent a JS error.
          if (callbackIsFunction && typeof callback != "function") {
            MozLoopService.log.error("hawkRequest: callback function was lost.", hawkError);
            return;
          }
          // The hawkError.error property, while usually a string representing
          // an HTTP response status message, may also incorrectly be a native
          // error object that will cause the cloning function to fail.
          callback(Cu.cloneInto({
            error: (hawkError.error && typeof hawkError.error == "string")
                   ? hawkError.error : "Unexpected exception",
            message: hawkError.message,
            code: hawkError.code,
            errno: hawkError.errno,
          }, targetWindow));
        }).catch(Cu.reportError);
      }
    },

    LOOP_SESSION_TYPE: {
      enumerable: true,
      get: function() {
        return Cu.cloneInto(LOOP_SESSION_TYPE, targetWindow);
      }
    },

    TWO_WAY_MEDIA_CONN_LENGTH: {
      enumerable: true,
      get: function() {
        return Cu.cloneInto(TWO_WAY_MEDIA_CONN_LENGTH, targetWindow);
      }
    },

    SHARING_STATE_CHANGE: {
      enumerable: true,
      get: function() {
        return Cu.cloneInto(SHARING_STATE_CHANGE, targetWindow);
      }
    },

    fxAEnabled: {
      enumerable: true,
      get: function() {
        return MozLoopService.fxAEnabled;
      },
    },

    /**
     * Start the FxA login flow using the OAuth client and params from the Loop
     * server.
     *
     * @param {Boolean} forceReAuth Set to true to force FxA into a re-auth even
     *                              if the user is already logged in.
     * @return {Promise} Returns a promise that is resolved on successful
     *                   completion, or rejected otherwise.
     */
    logInToFxA: {
      enumerable: true,
      writable: true,
      value: function(forceReAuth) {
        return MozLoopService.logInToFxA(forceReAuth);
      }
    },

    logOutFromFxA: {
      enumerable: true,
      writable: true,
      value: function() {
        return MozLoopService.logOutFromFxA();
      }
    },

    openFxASettings: {
      enumerable: true,
      writable: true,
      value: function() {
        return MozLoopService.openFxASettings();
      },
    },

    /**
     * Returns true if this profile has an encryption key.
     *
     * @return {Boolean} True if the profile has an encryption key.
     */
    hasEncryptionKey: {
      enumerable: true,
      get: function() {
        return MozLoopService.hasEncryptionKey;
      }
    },

    /**
     * Opens the Getting Started tour in the browser.
     *
     * @param {String} aSrc
     *   - The UI element that the user used to begin the tour, optional.
     */
    openGettingStartedTour: {
      enumerable: true,
      writable: true,
      value: function(aSrc) {
        return MozLoopService.openGettingStartedTour(aSrc);
      },
    },

    /**
     * Opens a URL in a new tab in the browser.
     *
     * @param {String} url The new url to open
     */
    openURL: {
      enumerable: true,
      writable: true,
      value: function(url) {
        return MozLoopService.openURL(url);
      }
    },

    /**
     * Copies passed string onto the system clipboard.
     *
     * @param {String} str The string to copy
     */
    copyString: {
      enumerable: true,
      writable: true,
      value: function(str) {
        clipboardHelper.copyString(str);
      }
    },

    /**
     * Returns the app version information for use during feedback.
     *
     * @return {Object} An object containing:
     *   - channel: The update channel the application is on
     *   - version: The application version
     *   - OS: The operating system the application is running on
     */
    appVersionInfo: {
      enumerable: true,
      get: function() {
        if (!appVersionInfo) {
          // If the lazy getter explodes, we're probably loaded in xpcshell,
          // which doesn't have what we need, so log an error.
          try {
            appVersionInfo = Cu.cloneInto({
              channel: UpdateChannel.get(),
              version: appInfo.version,
              OS: appInfo.OS
            }, targetWindow);
          } catch (ex) {
            // only log outside of xpcshell to avoid extra message noise
            if (typeof targetWindow !== 'undefined' && "console" in targetWindow) {
              MozLoopService.log.error("Failed to construct appVersionInfo; if this isn't " +
                                       "an xpcshell unit test, something is wrong", ex);
            }
          }
        }
        return appVersionInfo;
      }
    },

    /**
     * Composes an email via the external protocol service.
     *
     * @param {String} subject   Subject of the email to send
     * @param {String} body      Body message of the email to send
     * @param {String} recipient Recipient email address (optional)
     */
    composeEmail: {
      enumerable: true,
      writable: true,
      value: function(subject, body, recipient) {
        recipient = recipient || "";
        let mailtoURL = "mailto:" + encodeURIComponent(recipient) +
                        "?subject=" + encodeURIComponent(subject) +
                        "&body=" + encodeURIComponent(body);
        extProtocolSvc.loadURI(CommonUtils.makeURI(mailtoURL));
      }
    },

    /**
     * Adds a value to a telemetry histogram.
     *
     * @param  {String}  histogramId Name of the telemetry histogram to update.
     * @param  {String}  value       Label of bucket to increment in the histogram.
     */
    telemetryAddValue: {
      enumerable: true,
      writable: true,
      value: function(histogramId, value) {
        Services.telemetry.getHistogramById(histogramId).add(value);
      }
    },

    /**
     * Returns a new GUID (UUID) in curly braces format.
     */
    generateUUID: {
      enumerable: true,
      writable: true,
      value: function() {
        return MozLoopService.generateUUID();
      }
    },

    getAudioBlob: {
      enumerable: true,
      writable: true,
      value: function(name, callback) {
        let request = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"]
                        .createInstance(Ci.nsIXMLHttpRequest);
        let url = `chrome://browser/content/loop/shared/sounds/${name}.ogg`;

        request.open("GET", url, true);
        request.responseType = "arraybuffer";
        request.onload = () => {
          if (request.status < 200 || request.status >= 300) {
            let error = new Error(request.status + " " + request.statusText);
            callback(cloneValueInto(error, targetWindow));
            return;
          }

          let blob = new Blob([request.response], {type: "audio/ogg"});
          callback(null, cloneValueInto(blob, targetWindow));
        };

        request.send();
      }
    },

    /**
     * Compose a URL pointing to the location of an avatar by email address.
     * At the moment we use the Gravatar service to match email addresses with
     * avatars. This might change in the future as avatars might come from another
     * source.
     *
     * @param {String} emailAddress Users' email address
     * @param {Number} size         Size of the avatar image to return in pixels.
     *                              Optional. Default value: 40.
     * @return the URL pointing to an avatar matching the provided email address.
     */
    getUserAvatar: {
      enumerable: true,
      writable: true,
      value: function(emailAddress, size = 40) {
        const kEmptyGif = "data:image/gif;base64,R0lGODlhAQABAIAAAAAAAP///yH5BAEAAAAALAAAAAABAAEAAAIBRAA7";
        if (!emailAddress || !MozLoopService.getLoopPref("contacts.gravatars.show")) {
          return kEmptyGif;
        }

        // Do the MD5 dance.
        let hasher = Cc["@mozilla.org/security/hash;1"]
                       .createInstance(Ci.nsICryptoHash);
        hasher.init(Ci.nsICryptoHash.MD5);
        let stringStream = Cc["@mozilla.org/io/string-input-stream;1"]
                             .createInstance(Ci.nsIStringInputStream);
        stringStream.data = emailAddress.trim().toLowerCase();
        hasher.updateFromStream(stringStream, -1);
        let hash = hasher.finish(false);
        // Convert the binary hash data to a hex string.
        let md5Email = [toHexString(hash.charCodeAt(i)) for (i in hash)].join("");

        // Compose the Gravatar URL.
        return "https://www.gravatar.com/avatar/" + md5Email + ".jpg?default=blank&s=" + size;
      }
    },

    /**
     * Gets the metadata related to the currently selected tab in
     * the most recent window.
     *
     * @param {Function} A callback that is passed the metadata.
     */
    getSelectedTabMetadata: {
      value: function(callback) {
        let win = Services.wm.getMostRecentWindow("navigator:browser");
        win.messageManager.addMessageListener("PageMetadata:PageDataResult", function onPageDataResult(msg) {
          win.messageManager.removeMessageListener("PageMetadata:PageDataResult", onPageDataResult);
          let pageData = msg.json;
          callback(cloneValueInto(pageData, targetWindow));
        });
        win.gBrowser.selectedBrowser.messageManager.sendAsyncMessage("PageMetadata:GetPageData");
      }
    },

    /**
     * Associates a session-id and a call-id with a window for debugging.
     *
     * @param  {string}  windowId  The window id.
     * @param  {string}  sessionId OT session id.
     * @param  {string}  callId    The callId on the server.
     */
    addConversationContext: {
      enumerable: true,
      writable: true,
      value: function(windowId, sessionId, callid) {
        MozLoopService.addConversationContext(windowId, {
          sessionId: sessionId,
          callId: callid
        });
      }
    },

    /**
     * Notifies the UITour module that an event occurred that it might be
     * interested in.
     *
     * @param {String} subject  Subject of the notification
     * @param {mixed}  [params] Optional parameters, providing more details to
     *                          the notification subject
     */
    notifyUITour: {
      enumerable: true,
      writable: true,
      value: function(subject, params) {
        UITour.notify(subject, params);
      }
    },

    /**
     * Used to record the screen sharing state for a window so that it can
     * be reflected on the toolbar button.
     *
     * @param {String} windowId The id of the conversation window the state
     *                          is being changed for.
     * @param {Boolean} active  Whether or not screen sharing is now active.
     */
    setScreenShareState: {
      enumerable: true,
      writable: true,
      value: function(windowId, active) {
        MozLoopService.setScreenShareState(windowId, active);
      }
    },

    /**
     * Checks if the Social Share widget is available in any of the registered
     * widget areas (navbar, MenuPanel, etc).
     *
     * @return {Boolean} `true` if the widget is available and `false` when it's
     *                   still in the Customization palette.
     */
    isSocialShareButtonAvailable: {
      enumerable: true,
      writable: true,
      value: function() {
        let win = Services.wm.getMostRecentWindow("navigator:browser");
        if (!win || !win.CustomizableUI) {
          return false;
        }

        let widget = win.CustomizableUI.getWidget(kShareWidgetId);
        if (widget) {
          if (!socialShareButtonListenersAdded) {
            let eventName = "social:" + kShareWidgetId;
            Services.obs.addObserver(onShareWidgetChanged, eventName + "-added", false);
            Services.obs.addObserver(onShareWidgetChanged, eventName + "-removed", false);
            socialShareButtonListenersAdded = true;
          }
          return !!widget.areaType;
        }

        return false;
      }
    },

    /**
     * Add the Social Share widget to the navbar area, but only when it's not
     * located anywhere else than the Customization palette.
     */
    addSocialShareButton: {
      enumerable: true,
      writable: true,
      value: function() {
        // Don't do anything if the button is already available.
        if (api.isSocialShareButtonAvailable.value()) {
          return;
        }

        let win = Services.wm.getMostRecentWindow("navigator:browser");
        if (!win || !win.CustomizableUI) {
          return;
        }
        win.CustomizableUI.addWidgetToArea(kShareWidgetId, win.CustomizableUI.AREA_NAVBAR);
      }
    },

    /**
     * Activates the Social Share panel with the Social Provider panel opened
     * when the popup open.
     */
    addSocialShareProvider: {
      enumerable: true,
      writable: true,
      value: function() {
        // Don't do anything if the button is _not_ available.
        if (!api.isSocialShareButtonAvailable.value()) {
          return;
        }

        let win = Services.wm.getMostRecentWindow("navigator:browser");
        if (!win || !win.SocialShare) {
          return;
        }
        win.SocialShare.showDirectory();
      }
    },

    /**
     * Returns a sorted list of Social Providers that can share URLs. See
     * `updateSocialProvidersCache()` for more information.
     * 
     * @return {Array} Sorted list of share-capable Social Providers.
     */
    getSocialShareProviders: {
      enumerable: true,
      writable: true,
      value: function() {
        if (socialProviders) {
          return socialProviders;
        }
        return updateSocialProvidersCache();
      }
    },

    /**
     * Share a room URL through a Social Provider with the provided title message.
     * This action will open the share panel, which is anchored to the Social
     * Share widget.
     *
     * @param {String} providerOrigin Identifier of the targeted Social Provider
     * @param {String} roomURL        URL that points to the standalone client
     * @param {String} title          Message that augments the URL inside the
     *                                share message
     * @param {String} [body]         Optional longer message to be displayed
     *                                similar to the body of an email 
     */
    socialShareRoom: {
      enumerable: true,
      writable: true,
      value: function(providerOrigin, roomURL, title, body = null) {
        let win = Services.wm.getMostRecentWindow("navigator:browser");
        if (!win || !win.SocialShare) {
          return;
        }

        let graphData = {
          url: roomURL,
          title: title
        };
        if (body) {
          graphData.body = body;
        }
        win.SocialShare.sharePage(providerOrigin, graphData);
      }
    }
  };

  /**
   * Send an event to the content window to indicate that the state on the chrome
   * side was updated.
   *
   * @param  {name} name Name of the event, defaults to 'LoopStatusChanged'
   */
  function sendEvent(name = "LoopStatusChanged") {
    if (typeof targetWindow.CustomEvent != "function") {
      MozLoopService.log.debug("Could not send event to content document, " +
        "because it's being destroyed or we're in a unit test where " +
        "`targetWindow` is mocked.");
      return;
    }

    let event = new targetWindow.CustomEvent(name);
    targetWindow.dispatchEvent(event);
  }

  function onStatusChanged(aSubject, aTopic, aData) {
    sendEvent();
  }

  function onDOMWindowDestroyed(aSubject, aTopic, aData) {
    if (targetWindow && aSubject != targetWindow)
      return;
    Services.obs.removeObserver(onDOMWindowDestroyed, "dom-window-destroyed");
    Services.obs.removeObserver(onStatusChanged, "loop-status-changed");
    // Stop listening for changes in the social provider list, if necessary.
    if (socialProviders)
      Services.obs.removeObserver(updateSocialProvidersCache, "social:providers-changed");
    if (socialShareButtonListenersAdded) {
      let eventName = "social:" + kShareWidgetId;
      Services.obs.removeObserver(onShareWidgetChanged, eventName + "-added");
      Services.obs.removeObserver(onShareWidgetChanged, eventName + "-removed");
    }
  }

  function onShareWidgetChanged(aSubject, aTopic, aData) {
    sendEvent("LoopShareWidgetChanged");
  }

  /**
   * Retrieves a list of Social Providers from the Social API that are explicitly
   * capable of sharing URLs.
   * It also adds a listener that is fired whenever a new Provider is added or
   * removed.
   *
   * @return {Array} Sorted list of share-capable Social Providers.
   */
  function updateSocialProvidersCache() {
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

    let providersWasSet = !!socialProviders;
    // Replace old with new.
    socialProviders = cloneValueInto(providers.sort((a, b) =>
      a.name.toLowerCase().localeCompare(b.name.toLowerCase())), targetWindow);

    // Start listening for changes in the social provider list, if we're not
    // doing that yet.
    if (!providersWasSet) {
      Services.obs.addObserver(updateSocialProvidersCache, "social:providers-changed", false);
    } else {
      // Dispatch an event to content to let stores freshen-up.
      sendEvent("LoopSocialProvidersChanged");
    }

    return socialProviders;
  }

  let contentObj = Cu.createObjectIn(targetWindow);
  Object.defineProperties(contentObj, api);
  Object.seal(contentObj);
  Cu.makeObjectPropsNormal(contentObj);
  Services.obs.addObserver(onStatusChanged, "loop-status-changed", false);
  Services.obs.addObserver(onDOMWindowDestroyed, "dom-window-destroyed", false);

  if ("navigator" in targetWindow) {
    targetWindow.navigator.wrappedJSObject.__defineGetter__("mozLoop", function () {
      // We do this in a getter, so that we create these objects
      // only on demand (this is a potential concern, since
      // otherwise we might add one per iframe, and keep them
      // alive for as long as the window is alive).
      delete targetWindow.navigator.wrappedJSObject.mozLoop;
      return targetWindow.navigator.wrappedJSObject.mozLoop = contentObj;
    });

    // Handle window.close correctly on the panel and chatbox.
    hookWindowCloseForPanelClose(targetWindow);
  } else {
    // This isn't a window; but it should be a JS scope; used for testing
    return targetWindow.mozLoop = contentObj;
  }

}

function getChromeWindow(contentWin) {
  return contentWin.QueryInterface(Ci.nsIInterfaceRequestor)
                   .getInterface(Ci.nsIWebNavigation)
                   .QueryInterface(Ci.nsIDocShellTreeItem)
                   .rootTreeItem
                   .QueryInterface(Ci.nsIInterfaceRequestor)
                   .getInterface(Ci.nsIDOMWindow);
}
