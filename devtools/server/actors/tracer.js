/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const lazy = {};
ChromeUtils.defineESModuleGetters(
  lazy,
  {
    JSTracer: "resource://devtools/server/tracer/tracer.sys.mjs",
  },
  { global: "contextual" }
);

const { Actor } = require("resource://devtools/shared/protocol.js");
const { tracerSpec } = require("resource://devtools/shared/specs/tracer.js");

const { throttle } = require("resource://devtools/shared/throttle.js");

const {
  makeDebuggeeValue,
  createValueGripForTarget,
} = require("devtools/server/actors/object/utils");

const {
  TYPES,
  getResourceWatcher,
} = require("resource://devtools/server/actors/resources/index.js");
const { JSTRACER_TRACE } = TYPES;

loader.lazyRequireGetter(
  this,
  "GeckoProfileCollector",
  "resource://devtools/server/actors/utils/gecko-profile-collector.js",
  true
);

const LOG_METHODS = {
  STDOUT: "stdout",
  CONSOLE: "console",
  PROFILER: "profiler",
};
exports.LOG_METHODS = LOG_METHODS;
const VALID_LOG_METHODS = Object.values(LOG_METHODS);

const CONSOLE_THROTTLING_DELAY = 250;

class TracerActor extends Actor {
  constructor(conn, targetActor) {
    super(conn, tracerSpec);
    this.targetActor = targetActor;
    this.sourcesManager = this.targetActor.sourcesManager;

    this.throttledTraces = [];
    // On workers, we don't have access to setTimeout and can't have throttling
    this.throttleEmitTraces = isWorker
      ? this.flushTraces.bind(this)
      : throttle(this.flushTraces.bind(this), CONSOLE_THROTTLING_DELAY);

    this.geckoProfileCollector = new GeckoProfileCollector();
  }

  destroy() {
    this.stopTracing();
  }

  getLogMethod() {
    return this.logMethod;
  }

  /**
   * Toggle tracing JavaScript.
   * Meant for the WebConsole command in order to pass advanced
   * configuration directly to JavaScriptTracer class.
   *
   * @param {Object} options
   *        Options used to configure JavaScriptTracer.
   *        See `JavaScriptTracer.startTracing`.
   * @return {Boolean}
   *         True if the tracer starts, or false if it was stopped.
   */
  toggleTracing(options) {
    if (!this.tracingListener) {
      this.startTracing(options);
      return true;
    }
    this.stopTracing();
    return false;
  }

  /**
   * Start tracing.
   *
   * @param {Object} options
   *        Options used to configure JavaScriptTracer.
   *        See `JavaScriptTracer.startTracing`.
   */
  startTracing(options = {}) {
    if (options.logMethod && !VALID_LOG_METHODS.includes(options.logMethod)) {
      throw new Error(
        `Invalid log method '${options.logMethod}'. Only supports: ${VALID_LOG_METHODS}`
      );
    }
    if (options.prefix && typeof options.prefix != "string") {
      throw new Error("Invalid prefix, only support string type");
    }
    if (options.maxDepth && typeof options.maxDepth != "number") {
      throw new Error("Invalid max-depth, only support numbers");
    }
    if (options.maxRecords && typeof options.maxRecords != "number") {
      throw new Error("Invalid max-records, only support numbers");
    }

    // When tracing on next user interaction is enabled,
    // disable logging from workers as this makes the tracer work
    // against visible documents and is actived per document thread.
    if (options.traceOnNextInteraction && isWorker) {
      return;
    }

    // Ignore WindowGlobal target actors for WindowGlobal of iframes running in the same process and thread as their parent document.
    // isProcessRoot will be true for each WindowGlobal being the top parent within a given process.
    // It will typically be true for WindowGlobal of iframe running in a distinct origin and process,
    // but only for the top iframe document. It will also be true for the top level tab document.
    if (
      this.targetActor.window &&
      !this.targetActor.window.windowGlobalChild?.isProcessRoot
    ) {
      return;
    }

    this.logMethod = options.logMethod || LOG_METHODS.STDOUT;

    if (this.logMethod == LOG_METHODS.PROFILER) {
      this.geckoProfileCollector.start();
    }

    this.tracingListener = {
      onTracingFrame: this.onTracingFrame.bind(this),
      onTracingFrameStep: this.onTracingFrameStep.bind(this),
      onTracingFrameExit: this.onTracingFrameExit.bind(this),
      onTracingInfiniteLoop: this.onTracingInfiniteLoop.bind(this),
      onTracingToggled: this.onTracingToggled.bind(this),
      onTracingPending: this.onTracingPending.bind(this),
      onTracingDOMMutation: this.onTracingDOMMutation.bind(this),
    };
    lazy.JSTracer.addTracingListener(this.tracingListener);
    this.traceValues = !!options.traceValues;
    try {
      lazy.JSTracer.startTracing({
        global: this.targetActor.window || this.targetActor.workerGlobal,
        prefix: options.prefix || "",
        // Enable receiving the `currentDOMEvent` being passed to `onTracingFrame`
        traceDOMEvents: true,
        // Enable tracing DOM Mutations
        traceDOMMutations: options.traceDOMMutations,
        // Enable tracing function arguments as well as returned values
        traceValues: !!options.traceValues,
        // Enable tracing only on next user interaction
        traceOnNextInteraction: !!options.traceOnNextInteraction,
        // Notify about frame exit / function call returning
        traceFunctionReturn: !!options.traceFunctionReturn,
        // Ignore frames beyond the given depth
        maxDepth: options.maxDepth,
        // Stop the tracing after a number of top level frames
        maxRecords: options.maxRecords,
      });
    } catch (e) {
      // If startTracing throws, it probably rejected one of its options and we should
      // unregister the tracing listener.
      this.stopTracing();
      throw e;
    }
  }

  stopTracing() {
    if (!this.tracingListener) {
      return;
    }
    // Remove before stopping to prevent receiving the stop notification
    lazy.JSTracer.removeTracingListener(this.tracingListener);
    this.tracingListener = null;

    lazy.JSTracer.stopTracing();
    this.logMethod = null;
  }

  /**
   * Queried by THREAD_STATE watcher to send the gecko profiler data
   * as part of THREAD STATE "stop" resource.
   *
   * @return {Object} Gecko profiler profile object.
   */
  getProfile() {
    const profile = this.geckoProfileCollector.stop();
    // We only open the profile if it contains samples, otherwise it can crash the frontend.
    if (profile.threads[0].samples.data.length) {
      return profile;
    }
    return null;
  }

  /**
   * Be notified by the underlying JavaScriptTracer class
   * in case it stops by itself, instead of being stopped when the Actor's stopTracing
   * method is called by the user.
   *
   * @param {Boolean} enabled
   *        True if the tracer starts tracing, false it it stops.
   * @return {Boolean}
   *         Return true, if the JavaScriptTracer should log a message to stdout.
   */
  onTracingToggled(enabled) {
    // stopTracing will clear `logMethod`, so compute this before calling it.
    const shouldLogToStdout = this.logMethod == LOG_METHODS.STDOUT;

    if (!enabled) {
      this.stopTracing();
    }
    return shouldLogToStdout;
  }

  /**
   * Called when "trace on next user interaction" is enabled, to notify the user
   * that the tracer is initialized but waiting for the user first input.
   */
  onTracingPending() {
    // Delegate to JavaScriptTracer to log to stdout
    if (this.logMethod == LOG_METHODS.STDOUT) {
      return true;
    }

    if (this.logMethod == LOG_METHODS.CONSOLE) {
      const consoleMessageWatcher = getResourceWatcher(
        this.targetActor,
        TYPES.CONSOLE_MESSAGE
      );
      if (consoleMessageWatcher) {
        consoleMessageWatcher.emitMessages([
          {
            arguments: [lazy.JSTracer.NEXT_INTERACTION_MESSAGE],
            styles: [],
            level: "jstracer",
            chromeContext: false,
            timeStamp: ChromeUtils.dateNow(),
          },
        ]);
      }
      return false;
    }
    return false;
  }

  onTracingInfiniteLoop() {
    if (this.logMethod == LOG_METHODS.STDOUT) {
      return true;
    }
    if (this.logMethod == LOG_METHODS.PROFILER) {
      this.geckoProfileCollector.stop();
      return true;
    }
    const consoleMessageWatcher = getResourceWatcher(
      this.targetActor,
      TYPES.CONSOLE_MESSAGE
    );
    if (!consoleMessageWatcher) {
      return true;
    }

    const message =
      "Looks like an infinite recursion? We stopped the JavaScript tracer, but code may still be running!";
    consoleMessageWatcher.emitMessages([
      {
        arguments: [message],
        styles: [],
        level: "jstracer",
        chromeContext: false,
        timeStamp: ChromeUtils.dateNow(),
      },
    ]);

    return false;
  }

  /**
   * Called by JavaScriptTracer class when a new mutation happened on any DOM Element.
   *
   * @param {Object} options
   * @param {Number} options.depth
   *        Represents the depth of the frame in the call stack.
   * @param {String} options.prefix
   *        A string to be displayed as a prefix of any logged frame.
   * @param {nsIStackFrame} options.caller
   *        The JS Callsite which caused this mutation.
   * @param {String} options.type
   *        Type of DOM Mutation:
   *        - "add": Node being added,
   *        - "attributes": Node whose attributes changed,
   *        - "remove": Node being removed,
   * @param {DOMNode} options.element
   *        The DOM Node related to the current mutation.
   */
  onTracingDOMMutation({ depth, prefix, type, caller, element }) {
    // Delegate to JavaScriptTracer to log to stdout
    if (this.logMethod == LOG_METHODS.STDOUT) {
      return true;
    }

    if (this.logMethod == LOG_METHODS.CONSOLE) {
      const dbgObj = makeDebuggeeValue(this.targetActor, element);
      this.throttledTraces.push({
        resourceType: JSTRACER_TRACE,
        prefix,
        timeStamp: ChromeUtils.dateNow(),

        filename: caller?.filename,
        lineNumber: caller?.lineNumber,
        columnNumber: caller?.columnNumber,
        sourceId: caller.sourceId,

        depth,
        mutationType: type,
        mutationElement: createValueGripForTarget(this.targetActor, dbgObj),
      });
      this.throttleEmitTraces();
      return false;
    }
    return false;
  }

  /**
   * Called by JavaScriptTracer class on each step of a function call.
   *
   * @param {Object} options
   * @param {Debugger.Frame} options.frame
   *        A descriptor object for the JavaScript frame.
   * @param {Number} options.depth
   *        Represents the depth of the frame in the call stack.
   * @param {String} options.prefix
   *        A string to be displayed as a prefix of any logged frame.
   * @return {Boolean}
   *         Return true, if the JavaScriptTracer should log the step to stdout.
   */
  onTracingFrameStep({ frame, depth, prefix }) {
    const { script } = frame;
    const { lineNumber, columnNumber } = script.getOffsetMetadata(frame.offset);
    const url = script.source.url;

    // NOTE: Debugger.Script.prototype.getOffsetMetadata returns
    //       columnNumber in 1-based.
    //       Convert to 0-based, while keeping the wasm's column (1) as is.
    //       (bug 1863878)
    const columnBase = script.format === "wasm" ? 0 : 1;

    // Ignore blackboxed sources
    if (
      this.sourcesManager.isBlackBoxed(
        url,
        lineNumber,
        columnNumber - columnBase
      )
    ) {
      return false;
    }

    if (this.logMethod == LOG_METHODS.STDOUT) {
      // By returning true, we let JavaScriptTracer class log the message to stdout.
      return true;
    }

    if (this.logMethod == LOG_METHODS.CONSOLE) {
      this.throttledTraces.push({
        resourceType: JSTRACER_TRACE,
        prefix,
        timeStamp: ChromeUtils.dateNow(),

        depth,
        filename: url,
        lineNumber,
        columnNumber: columnNumber - columnBase,
        sourceId: script.source.id,
      });
      this.throttleEmitTraces();
    }

    return false;
  }
  /**
   * Called by JavaScriptTracer class when a new JavaScript frame is executed.
   *
   * @param {Debugger.Frame} frame
   *        A descriptor object for the JavaScript frame.
   * @param {Number} depth
   *        Represents the depth of the frame in the call stack.
   * @param {String} formatedDisplayName
   *        A human readable name for the current frame.
   * @param {String} prefix
   *        A string to be displayed as a prefix of any logged frame.
   * @param {String} currentDOMEvent
   *        If this is a top level frame (depth==0), and we are currently processing
   *        a DOM Event, this will refer to the name of that DOM Event.
   *        Note that it may also refer to setTimeout and setTimeout callback calls.
   * @return {Boolean}
   *         Return true, if the JavaScriptTracer should log the frame to stdout.
   */
  onTracingFrame({
    frame,
    depth,
    formatedDisplayName,
    prefix,
    currentDOMEvent,
  }) {
    const { script } = frame;
    const { lineNumber, columnNumber } = script.getOffsetMetadata(frame.offset);
    const url = script.source.url;

    // NOTE: Debugger.Script.prototype.getOffsetMetadata returns
    //       columnNumber in 1-based.
    //       Convert to 0-based, while keeping the wasm's column (1) as is.
    //       (bug 1863878)
    const columnBase = script.format === "wasm" ? 0 : 1;

    // Ignore blackboxed sources
    if (
      this.sourcesManager.isBlackBoxed(
        url,
        lineNumber,
        columnNumber - columnBase
      )
    ) {
      return false;
    }

    if (this.logMethod == LOG_METHODS.STDOUT) {
      // By returning true, we let JavaScriptTracer class log the message to stdout.
      return true;
    }

    if (this.logMethod == LOG_METHODS.CONSOLE) {
      // We may receive the currently processed DOM event (if this relates to one).
      // In this case, log a preliminary message, which looks different to highlight it.
      if (currentDOMEvent && depth == 0) {
        // Create a JSTRACER_TRACE resource with a slightly different shape
        this.throttledTraces.push({
          resourceType: JSTRACER_TRACE,
          prefix,
          timeStamp: ChromeUtils.dateNow(),

          eventName: currentDOMEvent,
        });
      }

      let args = undefined;
      // Log arguments, but only when this feature is enabled as it introduce
      // some significant overhead in perf as well as memory as it may hold the objects in memory.
      // Also prevent trying to log function call arguments if we aren't logging a frame
      // with arguments (e.g. Debugger evaluation frames, when executing from the console)
      if (this.traceValues && frame.arguments) {
        args = [];
        for (let arg of frame.arguments) {
          // Debugger.Frame.arguments contains either a Debugger.Object or primitive object
          if (arg?.unsafeDereference) {
            arg = arg.unsafeDereference();
          }
          // Instantiate a object actor so that the tools can easily inspect these objects
          const dbgObj = makeDebuggeeValue(this.targetActor, arg);
          args.push(createValueGripForTarget(this.targetActor, dbgObj));
        }
      }

      // Create a message object that fits Console Message Watcher expectations
      this.throttledTraces.push({
        resourceType: JSTRACER_TRACE,
        prefix,
        timeStamp: ChromeUtils.dateNow(),

        depth,
        implementation: frame.implementation,
        displayName: formatedDisplayName,
        filename: url,
        lineNumber,
        columnNumber: columnNumber - columnBase,
        sourceId: script.source.id,
        args,
      });
      this.throttleEmitTraces();
    } else if (this.logMethod == LOG_METHODS.PROFILER) {
      this.geckoProfileCollector.addSample(
        {
          // formatedDisplayName has a lambda at the beginning, remove it.
          name: formatedDisplayName.replace("λ ", ""),
          url,
          lineNumber,
          columnNumber,
          category: frame.implementation,
        },
        depth
      );
    }

    return false;
  }

  /**
   * Called by JavaScriptTracer class when a JavaScript frame exits (i.e. a function returns or throw).
   *
   * @param {Object} options
   * @param {Number} options.frameId
   *        Unique identifier for the current frame.
   *        This should match a frame notified via onTracingFrame.
   * @param {Debugger.Frame} options.frame
   *        A descriptor object for the JavaScript frame.
   * @param {Number} options.depth
   *        Represents the depth of the frame in the call stack.
   * @param {String} options.formatedDisplayName
   *        A human readable name for the current frame.
   * @param {String} options.prefix
   *        A string to be displayed as a prefix of any logged frame.
   * @param {String} options.why
   *        A string to explain why the function stopped.
   *        See tracer.sys.mjs's FRAME_EXIT_REASONS.
   * @param {Debugger.Object|primitive} options.rv
   *        The returned value. It can be the returned value, or the thrown exception.
   *        It is either a primitive object, otherwise it is a Debugger.Object for any other JS Object type.
   * @return {Boolean}
   *         Return true, if the JavaScriptTracer should log the frame to stdout.
   */
  onTracingFrameExit({
    frameId,
    frame,
    depth,
    formatedDisplayName,
    prefix,
    why,
    rv,
  }) {
    const { script } = frame;
    const { lineNumber, columnNumber } = script.getOffsetMetadata(frame.offset);
    const url = script.source.url;

    // NOTE: Debugger.Script.prototype.getOffsetMetadata returns
    //       columnNumber in 1-based.
    //       Convert to 0-based, while keeping the wasm's column (1) as is.
    //       (bug 1863878)
    const columnBase = script.format === "wasm" ? 0 : 1;

    // Ignore blackboxed sources
    if (
      this.sourcesManager.isBlackBoxed(
        url,
        lineNumber,
        columnNumber - columnBase
      )
    ) {
      return false;
    }

    if (this.logMethod == LOG_METHODS.STDOUT) {
      // By returning true, we let JavaScriptTracer class log the message to stdout.
      return true;
    }

    if (this.logMethod == LOG_METHODS.CONSOLE) {
      let returnedValue = undefined;
      // Log arguments, but only when this feature is enabled as it introduce
      // some significant overhead in perf as well as memory as it may hold the objects in memory.
      if (this.traceValues) {
        // Debugger.Frame.arguments contains either a Debugger.Object or primitive object
        if (rv?.unsafeDereference) {
          rv = rv.unsafeDereference();
        }
        // Instantiate a object actor so that the tools can easily inspect these objects
        const dbgObj = makeDebuggeeValue(this.targetActor, rv);
        returnedValue = createValueGripForTarget(this.targetActor, dbgObj);
      }

      // Create a message object that fits Console Message Watcher expectations
      this.throttledTraces.push({
        resourceType: JSTRACER_TRACE,
        prefix,
        timeStamp: ChromeUtils.dateNow(),

        depth,
        displayName: formatedDisplayName,
        filename: url,
        lineNumber,
        columnNumber: columnNumber - columnBase,
        sourceId: script.source.id,

        relatedTraceId: frameId,
        returnedValue,
        why,
      });
      this.throttleEmitTraces();
    } else if (this.logMethod == LOG_METHODS.PROFILER) {
      // For now, the profiler doesn't use this.
    }

    return false;
  }

  /**
   * This method is throttled and will notify all pending traces to be logged in the console
   * via the console message watcher.
   */
  flushTraces() {
    const traceWatcher = getResourceWatcher(this.targetActor, JSTRACER_TRACE);
    // Ignore the request if the frontend isn't listening to traces for that target.
    if (!traceWatcher) {
      return;
    }
    const traces = this.throttledTraces;
    this.throttledTraces = [];

    traceWatcher.emitTraces(traces);
  }
}
exports.TracerActor = TracerActor;
