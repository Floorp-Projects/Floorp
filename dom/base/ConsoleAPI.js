/* -*- Mode: js2; js2-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let Cu = Components.utils;
let Ci = Components.interfaces;
let Cc = Components.classes;

// The maximum allowed number of concurrent timers per page.
const MAX_PAGE_TIMERS = 10000;

// The regular expression used to parse %s/%d and other placeholders for
// variables in strings that need to be interpolated.
const ARGUMENT_PATTERN = /%\d*\.?\d*([osdif])\b/g;

// The maximum stacktrace depth when populating the stacktrace array used for
// console.trace().
const DEFAULT_MAX_STACKTRACE_DEPTH = 200;

// The console API methods are async and their action is executed later. This
// delay tells how much later.
const CALL_DELAY = 30; // milliseconds

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/ConsoleAPIStorage.jsm");

let nsITimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);

function ConsoleAPI() {}
ConsoleAPI.prototype = {

  classID: Components.ID("{b49c18f8-3379-4fc0-8c90-d7772c1a9ff3}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDOMGlobalPropertyInitializer]),

  _timerInitialized: false,
  _queuedCalls: null,
  _timerCallback: null,
  _destroyedWindows: null,

  // nsIDOMGlobalPropertyInitializer
  init: function CA_init(aWindow) {
    Services.obs.addObserver(this, "xpcom-shutdown", false);
    Services.obs.addObserver(this, "inner-window-destroyed", false);


    let outerID;
    let innerID;
    try {
      let windowUtils = aWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                          .getInterface(Ci.nsIDOMWindowUtils);

      outerID = windowUtils.outerWindowID;
      innerID = windowUtils.currentInnerWindowID;
    }
    catch (ex) {
      Cu.reportError(ex);
    }

    let meta = {
      outerID: outerID,
      innerID: innerID,
    };

    let self = this;
    let chromeObject = {
      // window.console API
      log: function CA_log() {
        self.queueCall("log", arguments, meta);
      },
      info: function CA_info() {
        self.queueCall("info", arguments, meta);
      },
      warn: function CA_warn() {
        self.queueCall("warn", arguments, meta);
      },
      error: function CA_error() {
        self.queueCall("error", arguments, meta);
      },
      debug: function CA_debug() {
        self.queueCall("debug", arguments, meta);
      },
      trace: function CA_trace() {
        self.queueCall("trace", arguments, meta);
      },
      // Displays an interactive listing of all the properties of an object.
      dir: function CA_dir() {
        self.queueCall("dir", arguments, meta);
      },
      group: function CA_group() {
        self.queueCall("group", arguments, meta);
      },
      groupCollapsed: function CA_groupCollapsed() {
        self.queueCall("groupCollapsed", arguments, meta);
      },
      groupEnd: function CA_groupEnd() {
        self.queueCall("groupEnd", arguments, meta);
      },
      time: function CA_time() {
        self.queueCall("time", arguments, meta);
      },
      timeEnd: function CA_timeEnd() {
        self.queueCall("timeEnd", arguments, meta);
      },
      __exposedProps__: {
        log: "r",
        info: "r",
        warn: "r",
        error: "r",
        debug: "r",
        trace: "r",
        dir: "r",
        group: "r",
        groupCollapsed: "r",
        groupEnd: "r",
        time: "r",
        timeEnd: "r"
      }
    };

    // We need to return an actual content object here, instead of a wrapped
    // chrome object. This allows things like console.log.bind() to work.
    let contentObj = Cu.createObjectIn(aWindow);
    function genPropDesc(fun) {
      return { enumerable: true, configurable: true, writable: true,
               value: chromeObject[fun].bind(chromeObject) };
    }
    const properties = {
      log: genPropDesc('log'),
      info: genPropDesc('info'),
      warn: genPropDesc('warn'),
      error: genPropDesc('error'),
      debug: genPropDesc('debug'),
      trace: genPropDesc('trace'),
      dir: genPropDesc('dir'),
      group: genPropDesc('group'),
      groupCollapsed: genPropDesc('groupCollapsed'),
      groupEnd: genPropDesc('groupEnd'),
      time: genPropDesc('time'),
      timeEnd: genPropDesc('timeEnd'),
      __noSuchMethod__: { enumerable: true, configurable: true, writable: true,
                          value: function() {} },
      __mozillaConsole__: { value: true }
    };

    Object.defineProperties(contentObj, properties);
    Cu.makeObjectPropsNormal(contentObj);

    this._queuedCalls = [];
    this._destroyedWindows = [];
    this._timerCallback = {
      notify: this._timerCallbackNotify.bind(this),
    };

    return contentObj;
  },

  observe: function CA_observe(aSubject, aTopic, aData)
  {
    if (aTopic == "xpcom-shutdown") {
      Services.obs.removeObserver(this, "xpcom-shutdown");
      Services.obs.removeObserver(this, "inner-window-destroyed");
      this._destroyedWindows = [];
      this._queuedCalls = [];
    }
    else if (aTopic == "inner-window-destroyed") {
      let innerWindowID = aSubject.QueryInterface(Ci.nsISupportsPRUint64).data;
      delete this.timerRegistry[innerWindowID + ""];
      this._destroyedWindows.push(innerWindowID);
    }
  },

  /**
   * Queue a call to a console method. See the CALL_DELAY constant.
   *
   * @param string aMethod
   *        The console method the code has invoked.
   * @param object aArguments
   *        The arguments passed to the console method.
   * @param object aMeta
   *        The associated call meta information. This needs to hold the inner
   *        and outer window IDs from where the console method was called.
   */
  queueCall: function CA_queueCall(aMethod, aArguments, aMeta)
  {
    let metaForCall = {
      outerID: aMeta.outerID,
      innerID: aMeta.innerID,
      timeStamp: Date.now(),
      stack: this.getStackTrace(aMethod != "trace" ? 1 : null),
    };

    this._queuedCalls.push([aMethod, aArguments, metaForCall]);

    if (!this._timerInitialized) {
      nsITimer.initWithCallback(this._timerCallback, CALL_DELAY,
                                Ci.nsITimer.TYPE_ONE_SHOT);
      this._timerInitialized = true;
    }
  },

  /**
   * Timer callback used to process each of the queued calls.
   * @private
   */
  _timerCallbackNotify: function CA__timerCallbackNotify()
  {
    this._timerInitialized = false;
    this._queuedCalls.splice(0).forEach(this._processQueuedCall, this);
    this._destroyedWindows = [];
  },

  /**
   * Process a queued call to a console method.
   *
   * @private
   * @param array aCall
   *        Array that holds information about the queued call.
   */
  _processQueuedCall: function CA__processQueuedItem(aCall)
  {
    let [method, args, meta] = aCall;

    let notifyMeta = {
      outerID: meta.outerID,
      innerID: meta.innerID,
      timeStamp: meta.timeStamp,
      frame: meta.stack[0],
    };

    let notifyArguments = null;

    switch (method) {
      case "log":
      case "info":
      case "warn":
      case "error":
      case "debug":
        notifyArguments = this.processArguments(args);
        break;
      case "trace":
        notifyArguments = meta.stack;
        break;
      case "group":
      case "groupCollapsed":
        notifyArguments = this.beginGroup(args);
        break;
      case "groupEnd":
      case "dir":
        notifyArguments = args;
        break;
      case "time":
        notifyArguments = this.startTimer(meta.innerID, args[0], meta.timeStamp);
        break;
      case "timeEnd":
        notifyArguments = this.stopTimer(meta.innerID, args[0], meta.timeStamp);
        break;
      default:
        // unknown console API method!
        return;
    }

    this.notifyObservers(method, notifyArguments, notifyMeta);
  },

  /**
   * Notify all observers of any console API call.
   *
   * @param string aLevel
   *        The message level.
   * @param mixed aArguments
   *        The arguments given to the console API call.
   * @param object aMeta
   *        Object that holds metadata about the console API call:
   *        - outerID - the outer ID of the window where the message came from.
   *        - innerID - the inner ID of the window where the message came from.
   *        - frame - the youngest content frame in the call stack.
   *        - timeStamp - when the console API call occurred.
   */
  notifyObservers: function CA_notifyObservers(aLevel, aArguments, aMeta) {
    let consoleEvent = {
      ID: aMeta.outerID,
      innerID: aMeta.innerID,
      level: aLevel,
      filename: aMeta.frame.filename,
      lineNumber: aMeta.frame.lineNumber,
      functionName: aMeta.frame.functionName,
      arguments: aArguments,
      timeStamp: aMeta.timeStamp,
    };

    consoleEvent.wrappedJSObject = consoleEvent;

    // Store messages for which the inner window was not destroyed.
    if (this._destroyedWindows.indexOf(aMeta.innerID) == -1) {
      ConsoleAPIStorage.recordEvent(aMeta.innerID, consoleEvent);
    }

    Services.obs.notifyObservers(consoleEvent, "console-api-log-event",
                                 aMeta.outerID);
  },

  /**
   * Process the console API call arguments in order to perform printf-like
   * string substitution.
   * TODO: object substitution should display an interactive property list (bug
   * 685815) and width and precision qualifiers should be taken into account
   * (bug 685813).
   *
   * @param mixed aArguments
   *        The arguments given to the console API call.
   **/
  processArguments: function CA_processArguments(aArguments) {
    if (aArguments.length < 2 || typeof aArguments[0] != "string") {
      return aArguments;
    }
    let args = Array.prototype.slice.call(aArguments);
    let format = args.shift();
    // Format specification regular expression.
    let processed = format.replace(ARGUMENT_PATTERN, function CA_PA_substitute(match, submatch) {
      switch (submatch) {
        case "o":
        case "s":
          return String(args.shift());
        case "d":
        case "i":
          return parseInt(args.shift());
        case "f":
          return parseFloat(args.shift());
        default:
          return submatch;
      };
    });
    args.unshift(processed);
    return args;
  },

  /**
   * Build the stacktrace array for the console.trace() call.
   *
   * @param number [aMaxDepth=DEFAULT_MAX_STACKTRACE_DEPTH]
   *        Optional maximum stacktrace depth.
   * @return array
   *         Each element is a stack frame that holds the following properties:
   *         filename, lineNumber, functionName and language.
   */
  getStackTrace: function CA_getStackTrace(aMaxDepth) {
    if (!aMaxDepth) {
      aMaxDepth = DEFAULT_MAX_STACKTRACE_DEPTH;
    }

    let stack = [];
    let frame = Components.stack.caller.caller;
    while (frame = frame.caller) {
      if (frame.language == Ci.nsIProgrammingLanguage.JAVASCRIPT ||
          frame.language == Ci.nsIProgrammingLanguage.JAVASCRIPT2) {
        stack.push({
          filename: frame.filename,
          lineNumber: frame.lineNumber,
          functionName: frame.name,
          language: frame.language,
        });
        if (stack.length == aMaxDepth) {
          break;
        }
      }
    }

    return stack;
  },

  /**
   * Begin a new group for logging output together.
   **/
  beginGroup: function CA_beginGroup() {
    return Array.prototype.join.call(arguments[0], " ");
  },

  /*
   * A registry of started timers. It contains a map of pages (defined by their
   * inner window IDs) to timer maps. Timer maps are key-value pairs of timer
   * names to timer start times, for all timers defined in that page. Timer
   * names are prepended with the inner window ID in order to avoid conflicts
   * with Object.prototype functions.
   */
  timerRegistry: {},

  /**
   * Create a new timer by recording the current time under the specified name.
   *
   * @param number aWindowId
   *        The inner ID of the window.
   * @param string aName
   *        The name of the timer.
   * @param number [aTimestamp=Date.now()]
   *        Optional timestamp that tells when the timer was originally started.
   * @return object
   *        The name property holds the timer name and the started property
   *        holds the time the timer was started. In case of error, it returns
   *        an object with the single property "error" that contains the key
   *        for retrieving the localized error message.
   **/
  startTimer: function CA_startTimer(aWindowId, aName, aTimestamp) {
    if (!aName) {
        return;
    }
    let innerID = aWindowId + "";
    if (!this.timerRegistry[innerID]) {
        this.timerRegistry[innerID] = {};
    }
    let pageTimers = this.timerRegistry[innerID];
    if (Object.keys(pageTimers).length > MAX_PAGE_TIMERS - 1) {
        return { error: "maxTimersExceeded" };
    }
    let key = aWindowId + "-" + aName.toString();
    if (!pageTimers[key]) {
        pageTimers[key] = aTimestamp || Date.now();
    }
    return { name: aName, started: pageTimers[key] };
  },

  /**
   * Stop the timer with the specified name and retrieve the elapsed time.
   *
   * @param number aWindowId
   *        The inner ID of the window.
   * @param string aName
   *        The name of the timer.
   * @param number [aTimestamp=Date.now()]
   *        Optional timestamp that tells when the timer was originally stopped.
   * @return object
   *        The name property holds the timer name and the duration property
   *        holds the number of milliseconds since the timer was started.
   **/
  stopTimer: function CA_stopTimer(aWindowId, aName, aTimestamp) {
    if (!aName) {
        return;
    }
    let innerID = aWindowId + "";
    let pageTimers = this.timerRegistry[innerID];
    if (!pageTimers) {
        return;
    }
    let key = aWindowId + "-" + aName.toString();
    if (!pageTimers[key]) {
        return;
    }
    let duration = (aTimestamp || Date.now()) - pageTimers[key];
    delete pageTimers[key];
    return { name: aName, duration: duration };
  }
};

let NSGetFactory = XPCOMUtils.generateNSGetFactory([ConsoleAPI]);
