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

XPCOMUtils.defineLazyModuleGetter(this, "LoopContacts",
                                        "resource:///modules/loop/LoopContacts.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "LoopStorage",
                                        "resource:///modules/loop/LoopStorage.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "hookWindowCloseForPanelClose",
                                        "resource://gre/modules/MozSocialAPI.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PluralForm",
                                        "resource://gre/modules/PluralForm.jsm");
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
 * @param {Error}        error        Error object to copy
 * @param {nsIDOMWindow} targetWindow The content window to attach the API
 */
const cloneErrorObject = function(error, targetWindow) {
  let obj = new targetWindow.Error();
  for (let prop of Object.getOwnPropertyNames(error)) {
    let value = error[prop];
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

  // Inspect for an error this way, because the Error object is special.
  if (value.constructor.name == "Error") {
    return cloneErrorObject(value, targetWindow);
  }

  return Cu.cloneInto(value, targetWindow);
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
      let callback = params.pop();
      api[func](...params, function(...results) {
        callback(...[cloneValueInto(r, targetWindow) for (r of results)]);
      });
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
          errors[type] = Cu.cloneInto(error, targetWindow);

        }
        return Cu.cloneInto(errors, targetWindow);
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
     * Returns the callData for a specific callDataId
     *
     * The data was retrieved from the LoopServer via a GET/calls/<version> request
     * triggered by an incoming message from the LoopPushServer.
     *
     * @param {int} loopCallId
     * @returns {callData} The callData or undefined if error.
     */
    getCallData: {
      enumerable: true,
      writable: true,
      value: function(loopCallId) {
        return Cu.cloneInto(LoopCalls.getCallData(loopCallId), targetWindow);
      }
    },

    /**
     * Releases the callData for a specific loopCallId
     *
     * The result of this call will be a free call session slot.
     *
     * @param {int} loopCallId
     */
    releaseCallData: {
      enumerable: true,
      writable: true,
      value: function(loopCallId) {
        LoopCalls.releaseCallData(loopCallId);
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
     * Callback parameters:
     * - err null on success, non-null on unexpected failure to show the prompt.
     * - {Boolean} True if the user chose the OK button.
     */
    confirm: {
      enumerable: true,
      writable: true,
      value: function(bodyMessage, okButtonMessage, cancelButtonMessage, callback) {
        try {
          let buttonFlags =
            (Ci.nsIPrompt.BUTTON_POS_0 * Ci.nsIPrompt.BUTTON_TITLE_IS_STRING) +
            (Ci.nsIPrompt.BUTTON_POS_1 * Ci.nsIPrompt.BUTTON_TITLE_IS_STRING);

          let chosenButton = Services.prompt.confirmEx(null, "",
            bodyMessage, buttonFlags, okButtonMessage, cancelButtonMessage,
            null, null, {});

          callback(null, chosenButton == 0);
        } catch (ex) {
          callback(cloneValueInto(ex, targetWindow));
        }
      }
    },

    /**
     * Call to ensure that any necessary registrations for the Loop Service
     * have taken place.
     *
     * Callback parameters:
     * - err null on successful registration, non-null otherwise.
     *
     * @param {Function} callback Will be called once registration is complete,
     *                            or straight away if registration has already
     *                            happened.
     */
    ensureRegistered: {
      enumerable: true,
      writable: true,
      value: function(callback) {
        // We translate from a promise to a callback, as we can't pass promises from
        // Promise.jsm across the priv versus unpriv boundary.
        MozLoopService.register().then(() => {
          callback(null);
        }, err => {
          callback(cloneValueInto(err, targetWindow));
        }).catch(Cu.reportError);
      }
    },

    /**
     * Used to note a call url expiry time. If the time is later than the current
     * latest expiry time, then the stored expiry time is increased. For times
     * sooner, this function is a no-op; this ensures we always have the latest
     * expiry time for a url.
     *
     * This is used to determine whether or not we should be registering with the
     * push server on start.
     *
     * @param {Integer} expiryTimeSeconds The seconds since epoch of the expiry time
     *                                    of the url.
     */
    noteCallUrlExpiry: {
      enumerable: true,
      writable: true,
      value: function(expiryTimeSeconds) {
        MozLoopService.noteCallUrlExpiry(expiryTimeSeconds);
      }
    },

    /**
     * Set any character preference under "loop."
     *
     * @param {String} prefName The name of the pref without the preceding "loop."
     * @param {String} stringValue The value to set.
     *
     * Any errors thrown by the Mozilla pref API are logged to the console
     * and cause false to be returned.
     */
    setLoopCharPref: {
      enumerable: true,
      writable: true,
      value: function(prefName, value) {
        MozLoopService.setLoopCharPref(prefName, value);
      }
    },

    /**
     * Return any preference under "loop." that's coercible to a character
     * preference.
     *
     * @param {String} prefName The name of the pref without the preceding
     * "loop."
     *
     * Any errors thrown by the Mozilla pref API are logged to the console
     * and cause null to be returned. This includes the case of the preference
     * not being found.
     *
     * @return {String} on success, null on error
     */
    getLoopCharPref: {
      enumerable: true,
      writable: true,
      value: function(prefName) {
        return MozLoopService.getLoopCharPref(prefName);
      }
    },

    /**
     * Return any preference under "loop." that's coercible to a boolean
     * preference.
     *
     * @param {String} prefName The name of the pref without the preceding
     * "loop."
     *
     * Any errors thrown by the Mozilla pref API are logged to the console
     * and cause null to be returned. This includes the case of the preference
     * not being found.
     *
     * @return {String} on success, null on error
     */
    getLoopBoolPref: {
      enumerable: true,
      writable: true,
      value: function(prefName) {
        return MozLoopService.getLoopBoolPref(prefName);
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
        MozLoopService.hawkRequest(sessionType, path, method, payloadObj).then((response) => {
          callback(null, response.body);
        }, hawkError => {
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

    fxAEnabled: {
      enumerable: true,
      get: function() {
        return MozLoopService.fxAEnabled;
      },
    },

    logInToFxA: {
      enumerable: true,
      writable: true,
      value: function() {
        return MozLoopService.logInToFxA();
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
          let defaults = Services.prefs.getDefaultBranch(null);

          // If the lazy getter explodes, we're probably loaded in xpcshell,
          // which doesn't have what we need, so log an error.
          try {
            appVersionInfo = Cu.cloneInto({
              channel: defaults.getCharPref("app.update.channel"),
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
     * @param  {string}  histogramId Name of the telemetry histogram to update.
     * @param  {integer} value       Value to add to the histogram.
     */
    telemetryAdd: {
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

    /**
     * Starts a direct call to the contact addresses.
     *
     * @param {Object} contact The contact to call
     * @param {String} callType The type of call, e.g. "audio-video" or "audio-only"
     * @return true if the call is opened, false if it is not opened (i.e. busy)
     */
    startDirectCall: {
      enumerable: true,
      writable: true,
      value: function(contact, callType) {
        LoopCalls.startDirectCall(contact, callType);
      }
    },
  };

  function onStatusChanged(aSubject, aTopic, aData) {
    let event = new targetWindow.CustomEvent("LoopStatusChanged");
    targetWindow.dispatchEvent(event);
  };

  function onDOMWindowDestroyed(aSubject, aTopic, aData) {
    if (targetWindow && aSubject != targetWindow)
      return;
    Services.obs.removeObserver(onDOMWindowDestroyed, "dom-window-destroyed");
    Services.obs.removeObserver(onStatusChanged, "loop-status-changed");
  };

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
