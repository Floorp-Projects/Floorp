/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource:///modules/loop/MozLoopService.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "hookWindowCloseForPanelClose",
  "resource://gre/modules/MozSocialAPI.jsm");

this.EXPORTED_SYMBOLS = ["injectLoopAPI"];

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

  let api = {
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
        return MozLoopService.register().then(() => {
          callback(null);
        }, err => {
          callback(err);
        });
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
     * @param {String} path The path to make the request to.
     * @param {String} method The request method, e.g. 'POST', 'GET'.
     * @param {Object} payloadObj An object which is converted to JSON and
     *                            transmitted with the request.
     * @param {Function} callback Called when the request completes.
     */
    hawkRequest: {
      enumerable: true,
      writable: true,
      value: function(path, method, payloadObj, callback) {
        // XXX Should really return a DOM promise here.
        return MozLoopService.hawkRequest(path, method, payloadObj).then((response) => {
          callback(null, response.body);
        }, (error) => {
          callback(Cu.cloneInto(error, targetWindow));
        });
      }
    },
  };

  let contentObj = Cu.createObjectIn(targetWindow);
  Object.defineProperties(contentObj, api);
  Object.seal(contentObj);
  Cu.makeObjectPropsNormal(contentObj);

  targetWindow.navigator.wrappedJSObject.__defineGetter__("mozLoop", function() {
    // We do this in a getter, so that we create these objects
    // only on demand (this is a potential concern, since
    // otherwise we might add one per iframe, and keep them
    // alive for as long as the window is alive).
    delete targetWindow.navigator.wrappedJSObject.mozLoop;
    return targetWindow.navigator.wrappedJSObject.mozLoop = contentObj;
  });

  // Handle window.close correctly on the panel and chatbox.
  hookWindowCloseForPanelClose(targetWindow);
}

function getChromeWindow(contentWin) {
  return contentWin.QueryInterface(Ci.nsIInterfaceRequestor)
                   .getInterface(Ci.nsIWebNavigation)
                   .QueryInterface(Ci.nsIDocShellTreeItem)
                   .rootTreeItem
                   .QueryInterface(Ci.nsIInterfaceRequestor)
                   .getInterface(Ci.nsIDOMWindow);
}
