/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Ci } = require("chrome");
const { isWindowIncluded } = require("devtools/shared/layout/utils");
const Services = require("Services");
const ChromeUtils = require("ChromeUtils");
const { WebConsoleUtils } = require("devtools/server/actors/webconsole/utils");

// The page errors listener

/**
 * The nsIConsoleService listener. This is used to send all of the console
 * messages (JavaScript, CSS and more) to the remote Web Console instance.
 *
 * @constructor
 * @param nsIDOMWindow [window]
 *        Optional - the window object for which we are created. This is used
 *        for filtering out messages that belong to other windows.
 * @param object listener
 *        The listener object must have one method:
 *        - onConsoleServiceMessage(). This method is invoked with one argument,
 *        the nsIConsoleMessage, whenever a relevant message is received.
 */
function ConsoleServiceListener(window, listener) {
  this.window = window;
  this.listener = listener;
}
exports.ConsoleServiceListener = ConsoleServiceListener;

ConsoleServiceListener.prototype = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIConsoleListener]),

  /**
   * The content window for which we listen to page errors.
   * @type nsIDOMWindow
   */
  window: null,

  /**
   * The listener object which is notified of messages from the console service.
   * @type object
   */
  listener: null,

  /**
   * Initialize the nsIConsoleService listener.
   */
  init: function() {
    Services.console.registerListener(this);
  },

  /**
   * The nsIConsoleService observer. This method takes all the script error
   * messages belonging to the current window and sends them to the remote Web
   * Console instance.
   *
   * @param nsIConsoleMessage message
   *        The message object coming from the nsIConsoleService.
   */
  observe: function(message) {
    if (!this.listener) {
      return;
    }

    if (this.window) {
      if (
        !(message instanceof Ci.nsIScriptError) ||
        !message.outerWindowID ||
        !this.isCategoryAllowed(message.category)
      ) {
        return;
      }

      const errorWindow = Services.wm.getOuterWindowWithId(
        message.outerWindowID
      );
      if (!errorWindow || !isWindowIncluded(this.window, errorWindow)) {
        return;
      }
    }

    this.listener.onConsoleServiceMessage(message);
  },

  /**
   * Check if the given message category is allowed to be tracked or not.
   * We ignore chrome-originating errors as we only care about content.
   *
   * @param string category
   *        The message category you want to check.
   * @return boolean
   *         True if the category is allowed to be logged, false otherwise.
   */
  isCategoryAllowed: function(category) {
    if (!category) {
      return false;
    }

    switch (category) {
      case "XPConnect JavaScript":
      case "component javascript":
      case "chrome javascript":
      case "chrome registration":
      case "XBL":
      case "XBL Prototype Handler":
      case "XBL Content Sink":
      case "xbl javascript":
        return false;
    }

    return true;
  },

  /**
   * Get the cached page errors for the current inner window and its (i)frames.
   *
   * @param boolean [includePrivate=false]
   *        Tells if you want to also retrieve messages coming from private
   *        windows. Defaults to false.
   * @return array
   *         The array of cached messages. Each element is an nsIScriptError or
   *         an nsIConsoleMessage
   */
  getCachedMessages: function(includePrivate = false) {
    const errors = Services.console.getMessageArray() || [];

    // if !this.window, we're in a browser console. Still need to filter
    // private messages.
    if (!this.window) {
      return errors.filter(error => {
        if (error instanceof Ci.nsIScriptError) {
          if (!includePrivate && error.isFromPrivateWindow) {
            return false;
          }
        }

        return true;
      });
    }

    const ids = WebConsoleUtils.getInnerWindowIDsForFrames(this.window);

    return errors.filter(error => {
      if (error instanceof Ci.nsIScriptError) {
        if (!includePrivate && error.isFromPrivateWindow) {
          return false;
        }
        if (
          ids &&
          (!ids.includes(error.innerWindowID) ||
            !this.isCategoryAllowed(error.category))
        ) {
          return false;
        }
      } else if (ids && ids[0]) {
        // If this is not an nsIScriptError and we need to do window-based
        // filtering we skip this message.
        return false;
      }

      return true;
    });
  },

  clearCachedMessages: function() {
    // if !this.window, we're in a browser console. Still need to filter
    // private messages.
    if (!this.window) {
      Services.console.reset();
    } else {
      WebConsoleUtils.getInnerWindowIDsForFrames(this.window).forEach(id =>
        Services.console.resetWindow(id)
      );
    }
  },

  /**
   * Remove the nsIConsoleService listener.
   */
  destroy: function() {
    Services.console.unregisterListener(this);
    this.listener = this.window = null;
  },
};
