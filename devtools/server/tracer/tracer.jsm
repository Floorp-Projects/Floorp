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
  "NEXT_INTERACTION_MESSAGE",
  "DOM_MUTATIONS",
];

const NEXT_INTERACTION_MESSAGE =
  "Waiting for next user interaction before tracing (next mousedown or keydown event)";

const FRAME_EXIT_REASONS = {
  // The function has been early terminated by the Debugger API
  TERMINATED: "terminated",
  // The function simply ends by returning a value
  RETURN: "return",
  // The function yields a new value
  YIELD: "yield",
  // The function await on a promise
  AWAIT: "await",
  // The function throws an exception
  THROW: "throw",
};

const DOM_MUTATIONS = {
  // Track all DOM Node being added
  ADD: "add",
  // Track all attributes being modified
  ATTRIBUTES: "attributes",
  // Track all DOM Node being removed
  REMOVE: "remove",
};

const listeners = new Set();

// Detecting worker is different if this file is loaded via Common JS loader (isWorker global)
// or as a JSM (constructor name)
const isWorker =
  globalThis.isWorker ||
  globalThis.constructor.name == "WorkerDebuggerGlobalScope";

// This module can be loaded from the worker thread, where we can't use ChromeUtils.
// So implement custom lazy getters (without XPCOMUtils ESM) from here.
// Worker codepath in DevTools will pass a custom Debugger instance.
const customLazy = {
  get Debugger() {
    // When this code runs in the worker thread, loaded via `loadSubScript`
    // (ex: browser_worker_tracer.js and WorkerDebugger.tracer.js),
    // this module runs within the WorkerDebuggerGlobalScope and have immediate access to Debugger class.
    if (globalThis.Debugger) {
      return globalThis.Debugger;
    }
    // When this code runs in the worker thread, loaded via `require`
    // (ex: from tracer actor module),
    // this module no longer has WorkerDebuggerGlobalScope as global,
    // but has to use require() to pull Debugger.
    if (isWorker) {
      return require("Debugger");
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
 * @param {Boolean} options.loggingMethod
 *        Optional setting to use something else than `dump()` to log traces to stdout.
 *        This is mostly used by tests.
 * @param {Boolean} options.traceDOMEvents
 *        Optional setting to enable tracing all the DOM events being going through
 *        dom/events/EventListenerManager.cpp's `EventListenerManager`.
 * @param {Array<string>} options.traceDOMMutations
 *        Optional setting to enable tracing all the DOM mutations.
 *        This array may contains three strings:
 *          - "add": trace all new DOM Node being added,
 *          - "attributes": trace all DOM attribute modifications,
 *          - "delete": trace all DOM Node being removed.
 * @param {Boolean} options.traceValues
 *        Optional setting to enable tracing all function call values as well,
 *        as returned values (when we do log returned frames).
 * @param {Boolean} options.traceOnNextInteraction
 *        Optional setting to enable when the tracing should only start when the
 *        use starts interacting with the page. i.e. on next keydown or mousedown.
 * @param {Boolean} options.traceSteps
 *        Optional setting to enable tracing each frame within a function execution.
 *        (i.e. not only function call and function returns [when traceFunctionReturn is true])
 * @param {Boolean} options.traceFunctionReturn
 *        Optional setting to enable when the tracing should notify about frame exit.
 *        i.e. when a function call returns or throws.
 * @param {String} options.filterFrameSourceUrl
 *        Optional setting to restrict all traces to only a given source URL.
 *        This is a loose check, so any source whose URL includes the passed string will be traced.
 * @param {Number} options.maxDepth
 *        Optional setting to ignore frames when depth is greater than the passed number.
 * @param {Number} options.maxRecords
 *        Optional setting to stop the tracer after having recorded at least
 *        the passed number of top level frames.
 * @param {Number} options.pauseOnStep
 *        Optional setting to delay each frame execution for a given amount of time in ms.
 */
class JavaScriptTracer {
  constructor(options) {
    this.onEnterFrame = this.onEnterFrame.bind(this);

    // DevTools CommonJS Workers modules don't have access to AbortController
    if (!isWorker) {
      this.abortController = new AbortController();
    }

    // By default, we would trace only JavaScript related to caller's global.
    // As there is no way to compute the caller's global default to the global of the
    // mandatory options argument.
    this.tracedGlobal = options.global || Cu.getGlobalForObject(options);

    // Instantiate a brand new Debugger API so that we can trace independently
    // of all other DevTools operations. i.e. we can pause while tracing without any interference.
    this.dbg = this.makeDebugger();

    this.prefix = options.prefix ? `${options.prefix}: ` : "";

    // List of all async frame which are poped per Spidermonkey API
    // but are actually waiting for async operation.
    // We should later enter them again when the async task they are being waiting for is completed.
    this.pendingAwaitFrames = new Set();

    this.loggingMethod = options.loggingMethod;
    if (!this.loggingMethod) {
      // On workers, `dump` can't be called with JavaScript on another object,
      // so bind it.
      this.loggingMethod = isWorker ? dump.bind(null) : dump;
    }

    this.traceDOMEvents = !!options.traceDOMEvents;

    if (options.traceDOMMutations) {
      if (!Array.isArray(options.traceDOMMutations)) {
        throw new Error("'traceDOMMutations' attribute should be an array");
      }
      const acceptedValues = Object.values(DOM_MUTATIONS);
      if (!options.traceDOMMutations.every(e => acceptedValues.includes(e))) {
        throw new Error(
          `'traceDOMMutations' only accept array of strings whose values can be: ${acceptedValues}`
        );
      }
      this.traceDOMMutations = options.traceDOMMutations;
    }
    this.traceSteps = !!options.traceSteps;
    this.traceValues = !!options.traceValues;
    this.traceFunctionReturn = !!options.traceFunctionReturn;
    this.maxDepth = options.maxDepth;
    this.maxRecords = options.maxRecords;
    this.records = 0;
    if ("pauseOnStep" in options) {
      if (typeof options.pauseOnStep != "number") {
        throw new Error("'pauseOnStep' attribute should be a number");
      }
      this.pauseOnStep = options.pauseOnStep;
    }
    if ("filterFrameSourceUrl" in options) {
      if (typeof options.filterFrameSourceUrl != "string") {
        throw new Error("'filterFrameSourceUrl' attribute should be a string");
      }
      this.filterFrameSourceUrl = options.filterFrameSourceUrl;
    }

    // An increment used to identify function calls and their returned/exit frames
    this.frameId = 0;

    // This feature isn't supported on Workers as they aren't involving user events
    if (options.traceOnNextInteraction && !isWorker) {
      this.#waitForNextInteraction();
    } else {
      this.#startTracing();
    }
  }

  // Is actively tracing?
  // We typically start tracing from the constructor, unless the "trace on next user interaction" feature is used.
  isTracing = false;

  /**
   * In case `traceOnNextInteraction` option is used, delay the actual start of tracing until a first user interaction.
   */
  #waitForNextInteraction() {
    // Use a dedicated Abort Controller as we are going to stop it as soon as we get the first user interaction,
    // whereas other listeners would typically wait for tracer stop.
    this.nextInteractionAbortController = new AbortController();

    const listener = () => {
      this.nextInteractionAbortController.abort();
      // Avoid tracing if the users asked to stop tracing while we were waiting for the user interaction.
      if (this.dbg) {
        this.#startTracing();
      }
    };
    const eventOptions = {
      signal: this.nextInteractionAbortController.signal,
      capture: true,
    };
    // Register the event listener on the Chrome Event Handler in order to receive the event first.
    // When used for the parent process target, `tracedGlobal` is browser.xhtml's window, which doesn't have a chromeEventHandler.
    const eventHandler =
      this.tracedGlobal.docShell.chromeEventHandler || this.tracedGlobal;
    eventHandler.addEventListener("mousedown", listener, eventOptions);
    eventHandler.addEventListener("keydown", listener, eventOptions);

    // Significate to the user that the tracer is registered, but not tracing just yet.
    let shouldLogToStdout = listeners.size == 0;
    for (const l of listeners) {
      if (typeof l.onTracingPending == "function") {
        shouldLogToStdout |= l.onTracingPending();
      }
    }
    if (shouldLogToStdout) {
      this.loggingMethod(this.prefix + NEXT_INTERACTION_MESSAGE + "\n");
    }
  }

  /**
   * Actually really start watching for executions.
   *
   * This may be delayed when traceOnNextInteraction options is used.
   * Otherwise we start tracing as soon as the class instantiates.
   */
  #startTracing() {
    this.isTracing = true;

    this.dbg.onEnterFrame = this.onEnterFrame;

    if (this.traceDOMEvents) {
      this.startTracingDOMEvents();
    }
    // This feature isn't supported on Workers as they aren't interacting with the DOM Tree
    if (this.traceDOMMutations?.length > 0 && !isWorker) {
      this.startTracingDOMMutations();
    }

    // In any case, we consider the tracing as started
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
    if (this.debuggerNotificationObserver) {
      this.debuggerNotificationObserver.removeListener(this.eventListener);
      this.debuggerNotificationObserver.disconnect(this.tracedGlobal);
      this.debuggerNotificationObserver = null;
    }
    this.currentDOMEvent = null;
  }

  startTracingDOMMutations() {
    this.tracedGlobal.document.devToolsWatchingDOMMutations = true;

    const eventOptions = {
      signal: this.abortController.signal,
      capture: true,
    };
    if (this.traceDOMMutations.includes(DOM_MUTATIONS.ADD)) {
      this.tracedGlobal.docShell.chromeEventHandler.addEventListener(
        "devtoolschildinserted",
        this.#onDOMMutation,
        eventOptions
      );
    }
    if (this.traceDOMMutations.includes(DOM_MUTATIONS.ATTRIBUTES)) {
      this.tracedGlobal.docShell.chromeEventHandler.addEventListener(
        "devtoolsattrmodified",
        this.#onDOMMutation,
        eventOptions
      );
    }
    if (this.traceDOMMutations.includes(DOM_MUTATIONS.REMOVE)) {
      this.tracedGlobal.docShell.chromeEventHandler.addEventListener(
        "devtoolschildremoved",
        this.#onDOMMutation,
        eventOptions
      );
    }
  }

  stopTracingDOMMutations() {
    this.tracedGlobal.document.devToolsWatchingDOMMutations = false;
    // Note that the event listeners are all going to be unregistered via the AbortController.
  }

  /**
   * Called for any DOM Mutation done in the traced document.
   *
   * @param {DOM Event} event
   */
  #onDOMMutation = event => {
    // Ignore elements inserted by DevTools, like the inspector's highlighters
    if (event.target.isNativeAnonymous) {
      return;
    }

    let type = "";
    switch (event.type) {
      case "devtoolschildinserted":
        type = DOM_MUTATIONS.ADD;
        break;
      case "devtoolsattrmodified":
        type = DOM_MUTATIONS.ATTRIBUTES;
        break;
      case "devtoolschildremoved":
        type = DOM_MUTATIONS.REMOVE;
        break;
      default:
        throw new Error("Unexpected DOM Mutation event type: " + event.type);
    }

    let shouldLogToStdout = true;
    if (listeners.size > 0) {
      shouldLogToStdout = false;
      for (const listener of listeners) {
        // If any listener return true, also log to stdout
        if (typeof listener.onTracingDOMMutation == "function") {
          shouldLogToStdout |= listener.onTracingDOMMutation({
            depth: this.depth,
            prefix: this.prefix,

            type,
            element: event.target,
            caller: Components.stack.caller,
          });
        }
      }
    }

    if (shouldLogToStdout) {
      const padding = "—".repeat(this.depth + 1);
      this.loggingMethod(
        this.prefix +
          padding +
          `[DOM Mutation | ${type}] ` +
          objectToString(event.target) +
          "\n"
      );
    }
  };

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
        let { type } = notification.event;
        if (!type) {
          // In the Worker thread, `notification.event` is an opaque wrapper.
          // In other threads it is a Xray wrapper.
          // Because of this difference, we have to fallback to use the Debugger.Object API.
          type = this.dbg
            .makeGlobalObjectReference(notification.global)
            .makeDebuggeeValue(notification.event)
            .getProperty("type").return;
        }
        this.currentDOMEvent = `DOM | ${type}`;
      } else {
        this.currentDOMEvent = notification.type;
      }
    } else {
      this.currentDOMEvent = null;
    }
  }

  /**
   * Stop observing execution.
   *
   * @param {String} reason
   *        Optional string to justify why the tracer stopped.
   */
  stopTracing(reason = "") {
    // Note that this may be called before `#startTracing()`, but still want to completely shut it down.
    if (!this.dbg) {
      return;
    }

    this.dbg.onEnterFrame = undefined;
    this.dbg.removeAllDebuggees();
    this.dbg.onNewGlobalObject = undefined;
    this.dbg = null;

    this.depth = 0;

    // Cancel the traceOnNextInteraction event listeners.
    if (this.nextInteractionAbortController) {
      this.nextInteractionAbortController.abort();
      this.nextInteractionAbortController = null;
    }

    if (this.traceDOMEvents) {
      this.stopTracingDOMEvents();
    }
    if (this.traceDOMMutations?.length > 0 && !isWorker) {
      this.stopTracingDOMMutations();
    }

    // Unregister all event listeners
    if (this.abortController) {
      this.abortController.abort();
    }

    this.tracedGlobal = null;
    this.isTracing = false;

    this.notifyToggle(false, reason);
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
   * @param {String} reason
   *        Optional string to justify why the tracer stopped.
   */
  notifyToggle(state, reason) {
    let shouldLogToStdout = listeners.size == 0;
    for (const listener of listeners) {
      if (typeof listener.onTracingToggled == "function") {
        shouldLogToStdout |= listener.onTracingToggled(state, reason);
      }
    }
    if (shouldLogToStdout) {
      if (state) {
        this.loggingMethod(this.prefix + "Start tracing JavaScript\n");
      } else {
        if (reason) {
          reason = ` (reason: ${reason})`;
        }
        this.loggingMethod(
          this.prefix + "Stop tracing JavaScript" + reason + "\n"
        );
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
      this.loggingMethod(
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
      // If an optional filter is passed, ignore frames which aren't matching the filter string
      if (
        this.filterFrameSourceUrl &&
        !frame.script.source.url?.includes(this.filterFrameSourceUrl)
      ) {
        return;
      }

      // Because of async frame which are popped and entered again on completion of the awaited async task,
      // we have to compute the depth from the frame. (and can't use a simple increment on enter/decrement on pop).
      const depth = getFrameDepth(frame);

      // Save the current depth for the DOM Mutation handler
      this.depth = depth;

      // Ignore the frame if we reached the depth limit (if one is provided)
      if (this.maxDepth && depth >= this.maxDepth) {
        return;
      }

      // When we encounter a frame which was previously popped because of pending on an async task,
      // ignore it and only log the following ones.
      if (this.pendingAwaitFrames.has(frame)) {
        this.pendingAwaitFrames.delete(frame);
        return;
      }

      // Auto-stop the tracer if we reached the number of max recorded top level frames
      if (depth === 0 && this.maxRecords) {
        if (this.records >= this.maxRecords) {
          this.stopTracing("max-records");
          return;
        }
        this.records++;
      }

      // Consider depth > 100 as an infinite recursive loop and stop the tracer.
      if (depth == 100) {
        this.notifyInfiniteLoop();
        this.stopTracing("infinite-loop");
        return;
      }

      const frameId = this.frameId++;
      let shouldLogToStdout = true;

      // If there is at least one DevTools debugging this process,
      // delegate logging to DevTools actors.
      if (listeners.size > 0) {
        shouldLogToStdout = false;
        const formatedDisplayName = formatDisplayName(frame);
        for (const listener of listeners) {
          // If any listener return true, also log to stdout
          if (typeof listener.onTracingFrame == "function") {
            shouldLogToStdout |= listener.onTracingFrame({
              frameId,
              frame,
              depth,
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
        this.logFrameEnteredToStdout(frame, depth);
      }

      if (this.traceSteps) {
        frame.onStep = () => {
          // Spidermonkey steps on many intermediate positions which don't make sense to the user.
          // `isStepStart` is close to each statement start, which is meaningful to the user.
          const { isStepStart } = frame.script.getOffsetMetadata(frame.offset);
          if (!isStepStart) {
            return;
          }

          shouldLogToStdout = true;
          if (listeners.size > 0) {
            shouldLogToStdout = false;
            for (const listener of listeners) {
              // If any listener return true, also log to stdout
              if (typeof listener.onTracingFrameStep == "function") {
                shouldLogToStdout |= listener.onTracingFrameStep({
                  frame,
                  depth,
                  prefix: this.prefix,
                });
              }
            }
          }
          if (shouldLogToStdout) {
            this.logFrameStepToStdout(frame, depth);
          }
          // Optionaly pause the frame execution by letting the other event loop to run in between.
          if (typeof this.pauseOnStep == "number") {
            syncPause(this.pauseOnStep);
          }
        };
      }

      frame.onPop = completion => {
        // Special case async frames. We are exiting the current frame because of waiting for an async task.
        // (this is typically a `await foo()` from an async function)
        // This frame should later be "entered" again.
        if (completion?.await) {
          this.pendingAwaitFrames.add(frame);
          return;
        }

        if (!this.traceFunctionReturn) {
          return;
        }

        let why = "";
        let rv = undefined;
        if (!completion) {
          why = FRAME_EXIT_REASONS.TERMINATED;
        } else if ("return" in completion) {
          why = FRAME_EXIT_REASONS.RETURN;
          rv = completion.return;
        } else if ("yield" in completion) {
          why = FRAME_EXIT_REASONS.YIELD;
          rv = completion.yield;
        } else if ("await" in completion) {
          why = FRAME_EXIT_REASONS.AWAIT;
        } else {
          why = FRAME_EXIT_REASONS.THROW;
          rv = completion.throw;
        }

        shouldLogToStdout = true;
        if (listeners.size > 0) {
          shouldLogToStdout = false;
          const formatedDisplayName = formatDisplayName(frame);
          for (const listener of listeners) {
            // If any listener return true, also log to stdout
            if (typeof listener.onTracingFrameExit == "function") {
              shouldLogToStdout |= listener.onTracingFrameExit({
                frameId,
                frame,
                depth,
                formatedDisplayName,
                prefix: this.prefix,
                why,
                rv,
              });
            }
          }
        }
        if (shouldLogToStdout) {
          this.logFrameExitedToStdout(frame, depth, why, rv);
        }
      };

      // Optionaly pause the frame execution by letting the other event loop to run in between.
      if (typeof this.pauseOnStep == "number") {
        syncPause(this.pauseOnStep);
      }
    } catch (e) {
      console.error("Exception while tracing javascript", e);
    }
  }

  /**
   * Display to stdout one given frame execution, which represents a function call.
   *
   * @param {Debugger.Frame} frame
   * @param {Number} depth
   */
  logFrameEnteredToStdout(frame, depth) {
    const padding = "—".repeat(depth + 1);

    // If we are tracing DOM events and we are in middle of an event,
    // and are logging the topmost frame,
    // then log a preliminary dedicated line to mention that event type.
    if (this.currentDOMEvent && depth == 0) {
      this.loggingMethod(this.prefix + padding + this.currentDOMEvent + "\n");
    }

    let message = `${padding}[${frame.implementation}]—> ${getTerminalHyperLink(
      frame
    )} - ${formatDisplayName(frame)}`;

    // Log arguments, but only when this feature is enabled as it introduces
    // some significant performance and visual overhead.
    // Also prevent trying to log function call arguments if we aren't logging a frame
    // with arguments (e.g. Debugger evaluation frames, when executing from the console)
    if (this.traceValues && frame.arguments) {
      message += "(";
      for (let i = 0, l = frame.arguments.length; i < l; i++) {
        const arg = frame.arguments[i];
        // Debugger.Frame.arguments contains either a Debugger.Object or primitive object
        if (arg?.unsafeDereference) {
          // Special case classes as they can't be easily differentiated in pure JavaScript
          if (arg.isClassConstructor) {
            message += "class " + arg.name;
          } else {
            message += objectToString(arg.unsafeDereference());
          }
        } else {
          message += primitiveToString(arg);
        }

        if (i < l - 1) {
          message += ", ";
        }
      }
      message += ")";
    }

    this.loggingMethod(this.prefix + message + "\n");
  }

  /**
   * Display to stdout one given frame execution, which represents a step within a function execution.
   *
   * @param {Debugger.Frame} frame
   * @param {Number} depth
   */
  logFrameStepToStdout(frame, depth) {
    const padding = "—".repeat(depth + 1);

    const message = `${padding}— ${getTerminalHyperLink(frame)}`;

    this.loggingMethod(this.prefix + message + "\n");
  }

  /**
   * Display to stdout the exit of a given frame execution, which represents a function return.
   *
   * @param {Debugger.Frame} frame
   * @param {String} why
   * @param {Number} depth
   */
  logFrameExitedToStdout(frame, depth, why, rv) {
    const padding = "—".repeat(depth + 1);

    let message = `${padding}[${frame.implementation}]<— ${getTerminalHyperLink(
      frame
    )} - ${formatDisplayName(frame)} ${why}`;

    // Log returned values, but only when this feature is enabled as it introduces
    // some significant performance and visual overhead.
    if (this.traceValues) {
      message += " ";
      // Debugger.Frame.arguments contains either a Debugger.Object or primitive object
      if (rv?.unsafeDereference) {
        // Special case classes as they can't be easily differentiated in pure JavaScript
        if (rv.isClassConstructor) {
          message += "class " + rv.name;
        } else {
          message += objectToString(rv.unsafeDereference());
        }
      } else {
        message += primitiveToString(rv);
      }
    }

    this.loggingMethod(this.prefix + message + "\n");
  }
}

/**
 * Return a string description for any arbitrary JS value.
 * Used when logging to stdout.
 *
 * @param {Object} obj
 *        Any JavaScript object to describe.
 * @return String
 *         User meaningful descriptor for the object.
 */
function objectToString(obj) {
  if (Element.isInstance(obj)) {
    let message = `<${obj.tagName}`;
    if (obj.id) {
      message += ` #${obj.id}`;
    }
    if (obj.className) {
      message += ` .${obj.className}`;
    }
    message += ">";
    return message;
  } else if (Array.isArray(obj)) {
    return `Array(${obj.length})`;
  } else if (Event.isInstance(obj)) {
    return `Event(${obj.type}) target=${objectToString(obj.target)}`;
  } else if (typeof obj === "function") {
    return `function ${obj.name || "anonymous"}()`;
  }
  return obj;
}

function primitiveToString(value) {
  const type = typeof value;
  if (type === "string") {
    // Use stringify to escape special characters and display in enclosing quotes.
    return JSON.stringify(value);
  } else if (value === 0 && 1 / value === -Infinity) {
    // -0 is very special and need special threatment.
    return "-0";
  } else if (type === "bigint") {
    return `BigInt(${value})`;
  } else if (value && typeof value.toString === "function") {
    // Use toString as it allows to stringify Symbols. Converting them to string throws.
    return value.toString();
  }

  // For all other types/cases, rely on native convertion to string
  return value;
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
    activeTracer?.isTracing &&
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

function getFrameDepth(frame) {
  if (typeof frame.depth !== "number") {
    let depth = 0;
    let f = frame;
    while ((f = f.older)) {
      depth++;
    }
    frame.depth = depth;
  }

  return frame.depth;
}

/**
 * Generate a magic string that will be rendered in smart terminals as a URL
 * for the given Frame object. This URL is special as it includes a line and column.
 * This URL can be clicked and Firefox will automatically open the source matching
 * the frame's URL in the currently opened Debugger.
 * Firefox will interpret differently the URLs ending with `/:?\d*:\d+/`.
 *
 * @param {Debugger.Frame} frame
 *        The frame being traced.
 * @return {String}
 *        The URL's magic string.
 */
function getTerminalHyperLink(frame) {
  const { script } = frame;
  const { lineNumber, columnNumber } = script.getOffsetMetadata(frame.offset);

  // Use a special URL, including line and column numbers which Firefox
  // interprets as to be opened in the already opened DevTool's debugger
  const href = `${script.source.url}:${lineNumber}:${columnNumber}`;

  // Use special characters in order to print working hyperlinks right from the terminal
  // See https://gist.github.com/egmontkob/eb114294efbcd5adb1944c9f3cb5feda
  return `\x1B]8;;${href}\x1B\\${href}\x1B]8;;\x1B\\`;
}

/**
 * Helper function to synchronously pause the current frame execution
 * for a given duration in ms.
 *
 * @param {Number} duration
 */
function syncPause(duration) {
  let freeze = true;
  const timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  timer.initWithCallback(
    () => {
      freeze = false;
    },
    duration,
    Ci.nsITimer.TYPE_ONE_SHOT
  );
  Services.tm.spinEventLoopUntil("debugger-slow-motion", function () {
    return !freeze;
  });
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
