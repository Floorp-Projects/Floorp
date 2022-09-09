/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Ci } = require("chrome");
const { isWindowIncluded } = require("devtools/shared/layout/utils");
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
 * @param Function handler
 *        This function is invoked with one argument, the nsIConsoleMessage, whenever a
 *        relevant message is received.
 * @param object filteringOptions
 *        Optional - The filteringOptions that this listener should listen to:
 *        - matchExactWindow: Set to true to match the messages on a specific window (when
 *          `window` is defined) and not on the whole window tree.
 */
class ConsoleServiceListener {
  constructor(window, handler, { matchExactWindow } = {}) {
    this.window = window;
    this.handler = handler;
    this.matchExactWindow = matchExactWindow;
  }

  QueryInterface = ChromeUtils.generateQI([Ci.nsIConsoleListener]);

  /**
   * The content window for which we listen to page errors.
   * @type nsIDOMWindow
   */
  window = null;

  /**
   * The function which is notified of messages from the console service.
   * @type function
   */
  handler = null;

  /**
   * Initialize the nsIConsoleService listener.
   */
  init() {
    Services.console.registerListener(this);
  }

  /**
   * The nsIConsoleService observer. This method takes all the script error
   * messages belonging to the current window and sends them to the remote Web
   * Console instance.
   *
   * @param nsIConsoleMessage message
   *        The message object coming from the nsIConsoleService.
   */
  observe(message) {
    if (!this.handler) {
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

      if (!errorWindow) {
        return;
      }

      if (this.matchExactWindow && this.window !== errorWindow) {
        return;
      }

      if (!isWindowIncluded(this.window, errorWindow)) {
        return;
      }
    }

    // Don't display messages triggered by eager evaluation.
    if (message.sourceName === "debugger eager eval code") {
      return;
    }
    this.handler(message);
  }

  /**
   * Check if the given message category is allowed to be tracked or not.
   * We ignore chrome-originating errors as we only care about content.
   *
   * @param string category
   *        The message category you want to check.
   * @return boolean
   *         True if the category is allowed to be logged, false otherwise.
   */
  isCategoryAllowed(category) {
    if (!category) {
      return false;
    }

    switch (category) {
      case "XPConnect JavaScript":
      case "component javascript":
      case "chrome javascript":
      case "chrome registration":
        return false;
    }

    return true;
  }

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
  getCachedMessages(includePrivate = false) {
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

    const ids = this.matchExactWindow
      ? [WebConsoleUtils.getInnerWindowId(this.window)]
      : WebConsoleUtils.getInnerWindowIDsForFrames(this.window);

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
      } else if (ids?.[0]) {
        // If this is not an nsIScriptError and we need to do window-based
        // filtering we skip this message.
        return false;
      }

      return true;
    });
  }

  /**
   * Remove the nsIConsoleService listener.
   */
  destroy() {
    Services.console.unregisterListener(this);
    this.handler = this.window = null;
  }
}

exports.ConsoleServiceListener = ConsoleServiceListener;
