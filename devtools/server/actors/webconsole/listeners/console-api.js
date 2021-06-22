/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cc, Ci } = require("chrome");
const Services = require("Services");
const ChromeUtils = require("ChromeUtils");
const {
  CONSOLE_WORKER_IDS,
  WebConsoleUtils,
} = require("devtools/server/actors/webconsole/utils");

// The window.console API observer

/**
 * The window.console API observer. This allows the window.console API messages
 * to be sent to the remote Web Console instance.
 *
 * @constructor
 * @param nsIDOMWindow window
 *        Optional - the window object for which we are created. This is used
 *        for filtering out messages that belong to other windows.
 * @param Function handler
 *        This function is invoked with one argument, the Console API message that comes
 *        from the observer service, whenever a relevant console API call is received.
 * @param object filteringOptions
 *        Optional - The filteringOptions that this listener should listen to:
 *        - addonId: filter console messages based on the addonId.
 *        - excludeMessagesBoundToWindow: Set to true to filter out messages that
 *          are bound to a specific window.
 */
class ConsoleAPIListener {
  constructor(window, handler, { addonId, excludeMessagesBoundToWindow } = {}) {
    this.window = window;
    this.handler = handler;
    this.addonId = addonId;
    this.excludeMessagesBoundToWindow = excludeMessagesBoundToWindow;
  }

  QueryInterface = ChromeUtils.generateQI([Ci.nsIObserver]);

  /**
   * The content window for which we listen to window.console API calls.
   * @type nsIDOMWindow
   */
  window = null;

  /**
   * The function which is notified of window.console API calls. It is invoked with one
   * argument: the console API call object that comes from the observer service.
   *
   * @type function
   */
  handler = null;

  /**
   * The addonId that we listen for. If not null then only messages from this
   * console will be returned.
   */
  addonId = null;

  /**
   * Initialize the window.console API observer.
   */
  init() {
    // Note that the observer is process-wide. We will filter the messages as
    // needed, see CAL_observe().
    Services.obs.addObserver(this, "console-api-log-event");
  }

  /**
   * The console API message observer. When messages are received from the
   * observer service we forward them to the remote Web Console instance.
   *
   * @param object message
   *        The message object receives from the observer service.
   * @param string topic
   *        The message topic received from the observer service.
   */
  observe(message, topic) {
    if (!this.handler) {
      return;
    }

    // Here, wrappedJSObject is not a security wrapper but a property defined
    // by the XPCOM component which allows us to unwrap the XPCOM interface and
    // access the underlying JSObject.
    const apiMessage = message.wrappedJSObject;

    if (!this.isMessageRelevant(apiMessage)) {
      return;
    }

    this.handler(apiMessage);
  }

  /**
   * Given a message, return true if this window should show it and false
   * if it should be ignored.
   *
   * @param message
   *        The message from the Storage Service
   * @return bool
   *         Do we care about this message?
   */
  isMessageRelevant(message) {
    const workerType = WebConsoleUtils.getWorkerType(message);

    if (this.window && workerType === "ServiceWorker") {
      // For messages from Service Workers, message.ID is the
      // scope, which can be used to determine whether it's controlling
      // a window.
      const scope = message.ID;

      if (!this.window.shouldReportForServiceWorkerScope(scope)) {
        return false;
      }
    }

    // innerID can be of different type:
    // - a number if the message is bound to a specific window
    // - a worker type ([Shared|Service]Worker) if the message comes from a worker
    // - a JSM filename
    // if we want to filter on a specific window, ignore all non-worker messages that
    // don't have a proper window id (for now, we receive the worker messages from the
    // main process so we still want to get them, although their innerID isn't a number).
    if (!workerType && typeof message.innerID !== "number" && this.window) {
      return false;
    }

    if (typeof message.innerID == "number") {
      if (
        this.excludeMessagesBoundToWindow &&
        // If innerID is 0, the message isn't actually bound to a window.
        message.innerID
      ) {
        return false;
      }

      if (
        this.window &&
        !WebConsoleUtils.getInnerWindowIDsForFrames(this.window).includes(
          message.innerID
        )
      ) {
        // Not the same window!
        return false;
      }
    }

    if (this.addonId) {
      // ConsoleAPI.jsm messages contains a consoleID, (and it is currently
      // used in Addon SDK add-ons), the standard 'console' object
      // (which is used in regular webpages and in WebExtensions pages)
      // contains the originAttributes of the source document principal.

      // Filtering based on the originAttributes used by
      // the Console API object.
      if (message.addonId == this.addonId) {
        return true;
      }

      // Filtering based on the old-style consoleID property used by
      // the legacy Console JSM module.
      if (message.consoleID && message.consoleID == `addon/${this.addonId}`) {
        return true;
      }

      return false;
    }

    return true;
  }

  /**
   * Get the cached messages for the current inner window and its (i)frames.
   *
   * @param boolean [includePrivate=false]
   *        Tells if you want to also retrieve messages coming from private
   *        windows. Defaults to false.
   * @return array
   *         The array of cached messages.
   */
  getCachedMessages(includePrivate = false) {
    let messages = [];
    const ConsoleAPIStorage = Cc[
      "@mozilla.org/consoleAPI-storage;1"
    ].getService(Ci.nsIConsoleAPIStorage);

    // if !this.window, we're in a browser console. Retrieve all events
    // for filtering based on privacy.
    if (!this.window) {
      messages = ConsoleAPIStorage.getEvents();
    } else {
      const ids = WebConsoleUtils.getInnerWindowIDsForFrames(this.window);
      ids.forEach(id => {
        messages = messages.concat(ConsoleAPIStorage.getEvents(id));
      });
    }

    CONSOLE_WORKER_IDS.forEach(id => {
      messages = messages.concat(ConsoleAPIStorage.getEvents(id));
    });

    messages = messages.filter(msg => {
      return this.isMessageRelevant(msg);
    });

    if (includePrivate) {
      return messages;
    }

    return messages.filter(m => !m.private);
  }

  /**
   * Destroy the console API listener.
   */
  destroy() {
    Services.obs.removeObserver(this, "console-api-log-event");
    this.window = this.handler = null;
  }
}
exports.ConsoleAPIListener = ConsoleAPIListener;
