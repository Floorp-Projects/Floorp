/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Bug 1827382, as this module can be used from the worker thread,
// the following JSM may be loaded by the worker loader until
// we have proper support for ESM from workers.
const {
  startTracing,
  stopTracing,
  addTracingListener,
  removeTracingListener,
} = require("resource://devtools/server/tracer/tracer.jsm");

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
      this.#startTracing(options);
      return true;
    }
    this.stopTracing();
    return false;
  }

  /**
   * Start tracing.
   *
   * @param {String} logMethod
   *        The output method used by the tracer.
   *        See `LOG_METHODS` for potential values.
   * @param {Object} options
   *        Options used to configure JavaScriptTracer.
   *        See `JavaScriptTracer.startTracing`.
   */
  startTracing(logMethod = LOG_METHODS.STDOUT, options = {}) {
    this.#startTracing({ ...options, logMethod });
  }

  #startTracing(options) {
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
    this.logMethod = options.logMethod || LOG_METHODS.STDOUT;

    if (this.logMethod == LOG_METHODS.PROFILER) {
      this.geckoProfileCollector.start();
    }

    this.tracingListener = {
      onTracingFrame: this.onTracingFrame.bind(this),
      onTracingInfiniteLoop: this.onTracingInfiniteLoop.bind(this),
      onTracingToggled: this.onTracingToggled.bind(this),
    };
    addTracingListener(this.tracingListener);
    this.traceValues = !!options.traceValues;
    startTracing({
      global: this.targetActor.window || this.targetActor.workerGlobal,
      prefix: options.prefix || "",
      // Enable receiving the `currentDOMEvent` being passed to `onTracingFrame`
      traceDOMEvents: true,
      // Enable tracing function arguments as well as returned values
      traceValues: !!options.traceValues,
      // Enable tracing only on next user interaction
      traceOnNextInteraction: !!options.traceOnNextInteraction,
      // Ignore frames beyond the given depth
      maxDepth: options.maxDepth,
      // Stop the tracing after a number of top level frames
      maxRecords: options.maxRecords,
    });
  }

  stopTracing() {
    if (!this.tracingListener) {
      return;
    }
    stopTracing();
    removeTracingListener(this.tracingListener);
    this.logMethod = null;
    this.tracingListener = null;
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
   * @param {String} reason
   *        Optional string to justify why the tracer stopped.
   * @return {Boolean}
   *         Return true, if the JavaScriptTracer should log a message to stdout.
   */
  onTracingToggled(enabled, reason) {
    // stopTracing will clear `logMethod`, so compute this before calling it.
    const shouldLogToStdout = this.logMethod == LOG_METHODS.STDOUT;

    if (!enabled) {
      this.stopTracing();
    }
    return shouldLogToStdout;
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
        level: "error",
        chromeContext: false,
        timeStamp: ChromeUtils.dateNow(),
      },
    ]);

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
      if (this.traceValues) {
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
