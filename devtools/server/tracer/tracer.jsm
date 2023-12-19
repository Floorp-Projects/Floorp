/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This module implements the JavaScript tracer.
 *
 * It is being used by:
 * - any code that want to manually toggle the tracer, typically when debugging code,
 * - the tracer actor to start and stop tracing from DevTools UI,
 * - the tracing state resource watcher in order to notify DevTools UI about the tracing state.
 *
 * It will default logging the tracers to the terminal/stdout.
 * But if DevTools are opened, it may delegate the logging to the tracer actor.
 * It will typically log the traces to the Web Console.
 *
 * `JavaScriptTracer.onEnterFrame` method is hot codepath and should be reviewed accordingly.
 */

"use strict";

const EXPORTED_SYMBOLS = [
  "startTracing",
  "stopTracing",
  "addTracingListener",
  "removeTracingListener",
];

const listeners = new Set();

// This module can be loaded from the worker thread, where we can't use ChromeUtils.
// So implement custom lazy getters (without XPCOMUtils ESM) from here.
// Worker codepath in DevTools will pass a custom Debugger instance.
const customLazy = {
  get Debugger() {
    // When this code runs in the worker thread, this module runs within
    // the WorkerDebuggerGlobalScope and have immediate access to Debugger class.
    // (while we can't use ChromeUtils.importESModule)
    if (globalThis.Debugger) {
      return globalThis.Debugger;
    }
    const { addDebuggerToGlobal } = ChromeUtils.importESModule(
      "resource://gre/modules/jsdebugger.sys.mjs"
    );
    // Avoid polluting all Modules global scope by using a Sandox as global.
    const systemPrincipal = Services.scriptSecurityManager.getSystemPrincipal();
    const debuggerSandbox = Cu.Sandbox(systemPrincipal);
    addDebuggerToGlobal(debuggerSandbox);
    delete customLazy.Debugger;
    customLazy.Debugger = debuggerSandbox.Debugger;
    return customLazy.Debugger;
  },

  get DistinctCompartmentDebugger() {
    const { addDebuggerToGlobal } = ChromeUtils.importESModule(
      "resource://gre/modules/jsdebugger.sys.mjs"
    );
    const systemPrincipal = Services.scriptSecurityManager.getSystemPrincipal();
    const debuggerSandbox = Cu.Sandbox(systemPrincipal, {
      // As we may debug the JSM/ESM shared global, we should be using a Debugger
      // from another system global.
      freshCompartment: true,
    });
    addDebuggerToGlobal(debuggerSandbox);
    delete customLazy.DistinctCompartmentDebugger;
    customLazy.DistinctCompartmentDebugger = debuggerSandbox.Debugger;
    return customLazy.DistinctCompartmentDebugger;
  },
};

/**
 * Start tracing against a given JS global.
 * Only code run from that global will be logged.
 *
 * @param {Object} options
 *        Object with configurations:
 * @param {Object} options.global
 *        The tracer only log traces related to the code executed within this global.
 *        When omitted, it will default to the options object's global.
 * @param {String} options.prefix
 *        Optional string logged as a prefix to all traces.
 * @param {Debugger} options.dbg
 *        Optional spidermonkey's Debugger instance.
 *        This allows devtools to pass a custom instance and ease worker support
 *        where we can't load jsdebugger.sys.mjs.
 * @param {Boolean} options.traceDOMEvents
 *        Optional setting to enable tracing all the DOM events being going through
 *        dom/events/EventListenerManager.cpp's `EventListenerManager`.
 */
class JavaScriptTracer {
  constructor(options) {
    this.onEnterFrame = this.onEnterFrame.bind(this);

    // By default, we would trace only JavaScript related to caller's global.
    // As there is no way to compute the caller's global default to the global of the
    // mandatory options argument.
    this.tracedGlobal = options.global || Cu.getGlobalForObject(options);

    // Instantiate a brand new Debugger API so that we can trace independently
    // of all other DevTools operations. i.e. we can pause while tracing without any interference.
    this.dbg = this.makeDebugger();

    this.depth = 0;
    this.prefix = options.prefix ? `${options.prefix}: ` : "";

    this.dbg.onEnterFrame = this.onEnterFrame;

    this.traceDOMEvents = !!options.traceDOMEvents;
    if (this.traceDOMEvents) {
      this.startTracingDOMEvents();
    }

    this.notifyToggle(true);
  }

  startTracingDOMEvents() {
    this.debuggerNotificationObserver = new DebuggerNotificationObserver();
    this.eventListener = this.eventListener.bind(this);
    this.debuggerNotificationObserver.addListener(this.eventListener);
    this.debuggerNotificationObserver.connect(this.tracedGlobal);

    this.currentDOMEvent = null;
  }

  stopTracingDOMEvents() {
    this.debuggerNotificationObserver.removeListener(this.eventListener);
    this.debuggerNotificationObserver.disconnect(this.tracedGlobal);
    this.debuggerNotificationObserver = null;
    this.currentDOMEvent = null;
  }

  /**
   * Called by DebuggerNotificationObserver interface when a DOM event start being notified
   * and after it has been notified.
   *
   * @param {DebuggerNotification} notification
   *        Info about the DOM event. See the related idl file.
   */
  eventListener(notification) {
    // For each event we get two notifications.
    // One just before firing the listeners and another one just after.
    //
    // Update `this.currentDOMEvent` to be refering to the event name
    // while the DOM event is being notified. It will be null the rest of the time.
    //
    // We don't need to maintain a stack of events as that's only consumed by onEnterFrame
    // which only cares about the very lastest event being currently trigerring some code.
    if (notification.phase == "pre") {
      // We get notified about "real" DOM event, but also when some particular callbacks are called like setTimeout.
      if (notification.type == "domEvent") {
        this.currentDOMEvent = `DOM(${notification.event.type})`;
      } else {
        this.currentDOMEvent = notification.type;
      }
    } else {
      this.currentDOMEvent = null;
    }
  }

  stopTracing() {
    if (!this.isTracing()) {
      return;
    }

    this.dbg.onEnterFrame = undefined;
    this.dbg.removeAllDebuggees();
    this.dbg.onNewGlobalObject = undefined;
    this.dbg = null;
    this.depth = 0;
    this.options = null;

    if (this.traceDOMEvents) {
      this.stopTracingDOMEvents();
    }

    this.tracedGlobal = null;

    this.notifyToggle(false);
  }

  isTracing() {
    return !!this.dbg;
  }

  /**
   * Instantiate a Debugger API instance dedicated to each Tracer instance.
   * It will notably be different from the instance used in DevTools.
   * This allows to implement tracing independently of DevTools.
   */
  makeDebugger() {
    // When this code runs in the worker thread, Cu isn't available
    // and we don't have system principal anyway in this context.
    const { isSystemPrincipal } =
      typeof Cu == "object" ? Cu.getObjectPrincipal(this.tracedGlobal) : {};

    // When debugging the system modules, we have to use a special instance
    // of Debugger loaded in a distinct system global.
    const dbg = isSystemPrincipal
      ? new customLazy.DistinctCompartmentDebugger()
      : new customLazy.Debugger();

    // For now, we only trace calls for one particular global at a time.
    // See the constructor for its definition.
    dbg.addDebuggee(this.tracedGlobal);

    return dbg;
  }

  /**
   * Notify DevTools and/or the user via stdout that tracing
   * has been enabled or disabled.
   *
   * @param {Boolean} state
   *        True if we just started tracing, false when it just stopped.
   */
  notifyToggle(state) {
    let shouldLogToStdout = listeners.size == 0;
    for (const listener of listeners) {
      if (typeof listener.onTracingToggled == "function") {
        shouldLogToStdout |= listener.onTracingToggled(state);
      }
    }
    if (shouldLogToStdout) {
      if (state) {
        dump(this.prefix + "Start tracing JavaScript\n");
      } else {
        dump(this.prefix + "Stop tracing JavaScript\n");
      }
    }
  }

  /**
   * Notify DevTools and/or the user via stdout that tracing
   * stopped because of an infinite loop.
   */
  notifyInfiniteLoop() {
    let shouldLogToStdout = listeners.size == 0;
    for (const listener of listeners) {
      if (typeof listener.onTracingInfiniteLoop == "function") {
        shouldLogToStdout |= listener.onTracingInfiniteLoop();
      }
    }
    if (shouldLogToStdout) {
      dump(
        this.prefix +
          "Looks like an infinite recursion? We stopped the JavaScript tracer, but code may still be running!\n"
      );
    }
  }

  /**
   * Called by the Debugger API (this.dbg) when a new frame is executed.
   *
   * @param {Debugger.Frame} frame
   *        A descriptor object for the JavaScript frame.
   */
  onEnterFrame(frame) {
    // Safe check, just in case we keep being notified, but the tracer has been stopped
    if (!this.dbg) {
      return;
    }
    try {
      if (this.depth == 100) {
        this.notifyInfiniteLoop();
        this.stopTracing();
        return;
      }

      const formatedDisplayName = formatDisplayName(frame);
      let shouldLogToStdout = true;

      // If there is at least one DevTools debugging this process,
      // delegate logging to DevTools actors.
      if (listeners.size > 0) {
        shouldLogToStdout = false;
        for (const listener of listeners) {
          // If any listener return true, also log to stdout
          if (typeof listener.onTracingFrame == "function") {
            shouldLogToStdout |= listener.onTracingFrame({
              frame,
              depth: this.depth,
              formatedDisplayName,
              prefix: this.prefix,
              currentDOMEvent: this.currentDOMEvent,
            });
          }
        }
      }

      // DevTools may delegate the work to log to stdout,
      // but if DevTools are closed, stdout is the only way to log the traces.
      if (shouldLogToStdout) {
        const { script } = frame;
        const { lineNumber, columnNumber } = script.getOffsetMetadata(
          frame.offset
        );
        const padding = "—".repeat(this.depth + 1);

        // If we are tracing DOM events and we are in middle of an event,
        // and are logging the topmost frame,
        // then log a preliminary dedicated line to mention that event type.
        if (this.currentDOMEvent && this.depth == 0) {
          dump(this.prefix + padding + this.currentDOMEvent + "\n");
        }

        // Use a special URL, including line and column numbers which Firefox
        // interprets as to be opened in the already opened DevTool's debugger
        const href = `${script.source.url}:${lineNumber}:${columnNumber}`;

        // Use special characters in order to print working hyperlinks right from the terminal
        // See https://gist.github.com/egmontkob/eb114294efbcd5adb1944c9f3cb5feda
        const urlLink = `\x1B]8;;${href}\x1B\\${href}\x1B]8;;\x1B\\`;

        const message = `${padding}[${
          frame.implementation
        }]—> ${urlLink} - ${formatDisplayName(frame)}`;

        dump(this.prefix + message + "\n");
      }

      this.depth++;
      frame.onPop = () => {
        this.depth--;
      };
    } catch (e) {
      console.error("Exception while tracing javascript", e);
    }
  }
}

/**
 * Try to describe the current frame we are tracing
 *
 * This will typically log the name of the method being called.
 *
 * @param {Debugger.Frame} frame
 *        The frame which is currently being executed.
 */
function formatDisplayName(frame) {
  if (frame.type === "call") {
    const callee = frame.callee;
    // Anonymous function will have undefined name and displayName.
    return "λ " + (callee.name || callee.displayName || "anonymous");
  }

  return `(${frame.type})`;
}

let activeTracer = null;

/**
 * Start tracing JavaScript.
 * i.e. log the name of any function being called in JS and its location in source code.
 *
 * @params {Object} options (mandatory)
 *        See JavaScriptTracer.startTracing jsdoc.
 */
function startTracing(options) {
  if (!options) {
    throw new Error("startTracing excepts an options object as first argument");
  }
  if (!activeTracer) {
    activeTracer = new JavaScriptTracer(options);
  } else {
    console.warn(
      "Can't start JavaScript tracing, another tracer is still active and we only support one tracer at a time."
    );
  }
}

/**
 * Stop tracing JavaScript.
 */
function stopTracing() {
  if (activeTracer) {
    activeTracer.stopTracing();
    activeTracer = null;
  } else {
    console.warn("Can't stop JavaScript Tracing as we were not tracing.");
  }
}

/**
 * Listen for tracing updates.
 *
 * The listener object may expose the following methods:
 * - onTracingToggled(state)
 *   Where state is a boolean to indicate if tracing has just been enabled of disabled.
 *   It may be immediatelly called if a tracer is already active.
 *
 * - onTracingInfiniteLoop()
 *   Called when the tracer stopped because of an infinite loop.
 *
 * - onTracingFrame({ frame, depth, formatedDisplayName, prefix })
 *   Called each time we enter a new JS frame.
 *   - frame is a Debugger.Frame object
 *   - depth is a number and represents the depth of the frame in the call stack
 *   - formatedDisplayName is a string and is a human readable name for the current frame
 *   - prefix is a string to display as a prefix of any logged frame
 *
 * @param {Object} listener
 */
function addTracingListener(listener) {
  listeners.add(listener);

  if (
    activeTracer?.isTracing() &&
    typeof listener.onTracingToggled == "function"
  ) {
    listener.onTracingToggled(true);
  }
}

/**
 * Unregister a listener previous registered via addTracingListener
 */
function removeTracingListener(listener) {
  listeners.delete(listener);
}

// This JSM may be execute as CommonJS when loaded in the worker thread
if (typeof module == "object") {
  module.exports = {
    startTracing,
    stopTracing,
    addTracingListener,
    removeTracingListener,
  };
}
