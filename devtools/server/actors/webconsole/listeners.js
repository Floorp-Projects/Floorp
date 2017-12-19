/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cc, Ci, components} = require("chrome");
const {isWindowIncluded} = require("devtools/shared/layout/utils");
const Services = require("Services");
const {XPCOMUtils} = require("resource://gre/modules/XPCOMUtils.jsm");
const {CONSOLE_WORKER_IDS, WebConsoleUtils} = require("devtools/server/actors/webconsole/utils");

XPCOMUtils.defineLazyServiceGetter(this,
                                   "swm",
                                   "@mozilla.org/serviceworkers/manager;1",
                                   "nsIServiceWorkerManager");

// Process script used to forward console calls from content processes to parent process
const CONTENT_PROCESS_SCRIPT = "resource://devtools/server/actors/webconsole/content-process-forward.js";

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

ConsoleServiceListener.prototype =
{
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIConsoleListener]),

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
  init: function () {
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
  observe: function (message) {
    if (!this.listener) {
      return;
    }

    if (this.window) {
      if (!(message instanceof Ci.nsIScriptError) ||
          !message.outerWindowID ||
          !this.isCategoryAllowed(message.category)) {
        return;
      }

      let errorWindow = Services.wm.getOuterWindowWithId(message.outerWindowID);
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
  isCategoryAllowed: function (category) {
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
  getCachedMessages: function (includePrivate = false) {
    let errors = Services.console.getMessageArray() || [];

    // if !this.window, we're in a browser console. Still need to filter
    // private messages.
    if (!this.window) {
      return errors.filter((error) => {
        if (error instanceof Ci.nsIScriptError) {
          if (!includePrivate && error.isFromPrivateWindow) {
            return false;
          }
        }

        return true;
      });
    }

    let ids = WebConsoleUtils.getInnerWindowIDsForFrames(this.window);

    return errors.filter((error) => {
      if (error instanceof Ci.nsIScriptError) {
        if (!includePrivate && error.isFromPrivateWindow) {
          return false;
        }
        if (ids &&
            (ids.indexOf(error.innerWindowID) == -1 ||
             !this.isCategoryAllowed(error.category))) {
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

  /**
   * Remove the nsIConsoleService listener.
   */
  destroy: function () {
    Services.console.unregisterListener(this);
    this.listener = this.window = null;
  },
};

// The window.console API observer

/**
 * The window.console API observer. This allows the window.console API messages
 * to be sent to the remote Web Console instance.
 *
 * @constructor
 * @param nsIDOMWindow window
 *        Optional - the window object for which we are created. This is used
 *        for filtering out messages that belong to other windows.
 * @param object owner
 *        The owner object must have the following methods:
 *        - onConsoleAPICall(). This method is invoked with one argument, the
 *        Console API message that comes from the observer service, whenever
 *        a relevant console API call is received.
 * @param object filteringOptions
 *        Optional - The filteringOptions that this listener should listen to:
 *        - addonId: filter console messages based on the addonId.
 */
function ConsoleAPIListener(window, owner, {addonId} = {}) {
  this.window = window;
  this.owner = owner;
  this.addonId = addonId;
}
exports.ConsoleAPIListener = ConsoleAPIListener;

ConsoleAPIListener.prototype =
{
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),

  /**
   * The content window for which we listen to window.console API calls.
   * @type nsIDOMWindow
   */
  window: null,

  /**
   * The owner object which is notified of window.console API calls. It must
   * have a onConsoleAPICall method which is invoked with one argument: the
   * console API call object that comes from the observer service.
   *
   * @type object
   * @see WebConsoleActor
   */
  owner: null,

  /**
   * The addonId that we listen for. If not null then only messages from this
   * console will be returned.
   */
  addonId: null,

  /**
   * Initialize the window.console API observer.
   */
  init: function () {
    // Note that the observer is process-wide. We will filter the messages as
    // needed, see CAL_observe().
    Services.obs.addObserver(this, "console-api-log-event");
  },

  /**
   * The console API message observer. When messages are received from the
   * observer service we forward them to the remote Web Console instance.
   *
   * @param object message
   *        The message object receives from the observer service.
   * @param string topic
   *        The message topic received from the observer service.
   */
  observe: function (message, topic) {
    if (!this.owner) {
      return;
    }

    // Here, wrappedJSObject is not a security wrapper but a property defined
    // by the XPCOM component which allows us to unwrap the XPCOM interface and
    // access the underlying JSObject.
    let apiMessage = message.wrappedJSObject;

    if (!this.isMessageRelevant(apiMessage)) {
      return;
    }

    this.owner.onConsoleAPICall(apiMessage);
  },

  /**
   * Given a message, return true if this window should show it and false
   * if it should be ignored.
   *
   * @param message
   *        The message from the Storage Service
   * @return bool
   *         Do we care about this message?
   */
  isMessageRelevant: function (message) {
    let workerType = WebConsoleUtils.getWorkerType(message);

    if (this.window && workerType === "ServiceWorker") {
      // For messages from Service Workers, message.ID is the
      // scope, which can be used to determine whether it's controlling
      // a window.
      let scope = message.ID;

      if (!this.window.shouldReportForServiceWorkerScope(scope)) {
        return false;
      }
    }

    if (this.window && !workerType) {
      let msgWindow = Services.wm.getCurrentInnerWindowWithId(message.innerID);
      if (!msgWindow || !isWindowIncluded(this.window, msgWindow)) {
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
  },

  /**
   * Get the cached messages for the current inner window and its (i)frames.
   *
   * @param boolean [includePrivate=false]
   *        Tells if you want to also retrieve messages coming from private
   *        windows. Defaults to false.
   * @return array
   *         The array of cached messages.
   */
  getCachedMessages: function (includePrivate = false) {
    let messages = [];
    let ConsoleAPIStorage = Cc["@mozilla.org/consoleAPI-storage;1"]
                              .getService(Ci.nsIConsoleAPIStorage);

    // if !this.window, we're in a browser console. Retrieve all events
    // for filtering based on privacy.
    if (!this.window) {
      messages = ConsoleAPIStorage.getEvents();
    } else {
      let ids = WebConsoleUtils.getInnerWindowIDsForFrames(this.window);
      ids.forEach((id) => {
        messages = messages.concat(ConsoleAPIStorage.getEvents(id));
      });
    }

    CONSOLE_WORKER_IDS.forEach((id) => {
      messages = messages.concat(ConsoleAPIStorage.getEvents(id));
    });

    messages = messages.filter(msg => {
      return this.isMessageRelevant(msg);
    });

    if (includePrivate) {
      return messages;
    }

    return messages.filter((m) => !m.private);
  },

  /**
   * Destroy the console API listener.
   */
  destroy: function () {
    Services.obs.removeObserver(this, "console-api-log-event");
    this.window = this.owner = null;
  },
};

/**
 * A ReflowObserver that listens for reflow events from the page.
 * Implements nsIReflowObserver.
 *
 * @constructor
 * @param object window
 *        The window for which we need to track reflow.
 * @param object owner
 *        The listener owner which needs to implement:
 *        - onReflowActivity(reflowInfo)
 */

function ConsoleReflowListener(window, listener) {
  this.docshell = window.QueryInterface(Ci.nsIInterfaceRequestor)
                         .getInterface(Ci.nsIWebNavigation)
                         .QueryInterface(Ci.nsIDocShell);
  this.listener = listener;
  this.docshell.addWeakReflowObserver(this);
}

exports.ConsoleReflowListener = ConsoleReflowListener;

ConsoleReflowListener.prototype =
{
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIReflowObserver,
                                         Ci.nsISupportsWeakReference]),
  docshell: null,
  listener: null,

  /**
   * Forward reflow event to listener.
   *
   * @param DOMHighResTimeStamp start
   * @param DOMHighResTimeStamp end
   * @param boolean interruptible
   */
  sendReflow: function (start, end, interruptible) {
    let frame = components.stack.caller.caller;

    let filename = frame ? frame.filename : null;

    if (filename) {
      // Because filename could be of the form "xxx.js -> xxx.js -> xxx.js",
      // we only take the last part.
      filename = filename.split(" ").pop();
    }

    this.listener.onReflowActivity({
      interruptible: interruptible,
      start: start,
      end: end,
      sourceURL: filename,
      sourceLine: frame ? frame.lineNumber : null,
      functionName: frame ? frame.name : null
    });
  },

  /**
   * On uninterruptible reflow
   *
   * @param DOMHighResTimeStamp start
   * @param DOMHighResTimeStamp end
   */
  reflow: function (start, end) {
    this.sendReflow(start, end, false);
  },

  /**
   * On interruptible reflow
   *
   * @param DOMHighResTimeStamp start
   * @param DOMHighResTimeStamp end
   */
  reflowInterruptible: function (start, end) {
    this.sendReflow(start, end, true);
  },

  /**
   * Unregister listener.
   */
  destroy: function () {
    this.docshell.removeWeakReflowObserver(this);
    this.listener = this.docshell = null;
  },
};

/**
 * Forward console message calls from content processes to the parent process.
 * Used by Browser console and toolbox to see messages from all processes.
 *
 * @constructor
 * @param object owner
 *        The listener owner which needs to implement:
 *        - onConsoleAPICall(message)
 */
function ContentProcessListener(listener) {
  this.listener = listener;

  Services.ppmm.addMessageListener("Console:Log", this);
  Services.ppmm.loadProcessScript(CONTENT_PROCESS_SCRIPT, true);
}

exports.ContentProcessListener = ContentProcessListener;

ContentProcessListener.prototype = {
  receiveMessage(message) {
    let logMsg = message.data;
    logMsg.wrappedJSObject = logMsg;
    this.listener.onConsoleAPICall(logMsg);
  },

  destroy() {
    // Tell the content processes to stop listening and forwarding messages
    Services.ppmm.broadcastAsyncMessage("DevTools:StopForwardingContentProcessMessage");

    Services.ppmm.removeMessageListener("Console:Log", this);
    Services.ppmm.removeDelayedProcessScript(CONTENT_PROCESS_SCRIPT);

    this.listener = null;
  }
};
