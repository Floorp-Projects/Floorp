/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/* global XPCNativeWrapper */

const {Cu} = require("chrome");
const ChromeUtils = require("ChromeUtils");
// base-loader.js is a JSM without .jsm file extension, so it has to be loaded
// via ChromeUtils.import and not require() which would consider it as a CommonJS module
const {serializeStack, parseStack} = ChromeUtils.import("resource://devtools/shared/base-loader.js", {});

const {
  METHOD_FUNCTION,
  GETTER_FUNCTION,
  SETTER_FUNCTION,
} = require("devtools/shared/fronts/function-call");

const { FunctionCallActor } = require("devtools/server/actors/utils/function-call");

/**
 * This helper observes function calls on certain objects or globals.
 */
function CallWatcher(conn, targetActor) {
  this.targetActor = targetActor;
  this.conn = conn;
  this._onGlobalCreated = this._onGlobalCreated.bind(this);
  this._onGlobalDestroyed = this._onGlobalDestroyed.bind(this);
  this._onContentFunctionCall = this._onContentFunctionCall.bind(this);
  this.targetActor.on("window-ready", this._onGlobalCreated);
  this.targetActor.on("window-destroyed", this._onGlobalDestroyed);
}

exports.CallWatcher = CallWatcher;

CallWatcher.prototype = {
  /**
   * Lightweight listener invoked whenever an instrumented function is called
   * while recording. We're doing this to avoid the event emitter overhead,
   * since this is expected to be a very hot function.
   */
  onCall: null,

  /**
   * Starts waiting for the current target actor's document global to be
   * created, in order to instrument the specified objects and become
   * aware of everything the content does with them.
   */
  setup: function({
    tracedGlobals, tracedFunctions, startRecording, performReload, holdWeak, storeCalls,
  }) {
    if (this._initialized) {
      return;
    }
    this._initialized = true;
    this._timestampEpoch = 0;

    this._functionCalls = [];
    this._tracedGlobals = tracedGlobals || [];
    this._tracedFunctions = tracedFunctions || [];
    this._holdWeak = !!holdWeak;
    this._storeCalls = !!storeCalls;

    if (startRecording) {
      this.resumeRecording();
    }
    if (performReload) {
      this.targetActor.window.location.reload();
    }
  },

  /**
   * Stops listening for document global changes and puts this actor
   * to hibernation. This method is called automatically just before the
   * actor is destroyed.
   */
  finalize: function() {
    this.targetActor.off("window-ready", this._onGlobalCreated);
    this.targetActor.off("window-destroyed", this._onGlobalDestroyed);
    if (!this._initialized) {
      return;
    }
    this._initialized = false;
    this._finalized = true;

    this._tracedGlobals = null;
    this._tracedFunctions = null;
  },

  /**
   * Returns whether the instrumented function calls are currently recorded.
   */
  isRecording: function() {
    return this._recording;
  },

  /**
   * Initialize the timestamp epoch used to offset function call timestamps.
   */
  initTimestampEpoch: function() {
    this._timestampEpoch = this.targetActor.window.performance.now();
  },

  /**
   * Starts recording function calls.
   */
  resumeRecording: function() {
    this._recording = true;
  },

  /**
   * Stops recording function calls.
   */
  pauseRecording: function() {
    this._recording = false;
    return this._functionCalls;
  },

  /**
   * Erases all the recorded function calls.
   * Calling `resumeRecording` or `pauseRecording` does not erase history.
   */
  eraseRecording: function() {
    this._functionCalls = [];
  },

  /**
   * Invoked whenever the current target actor's document global is created.
   */
  _onGlobalCreated: function({window, id, isTopLevel}) {
    if (!this._initialized) {
      return;
    }

    // TODO: bug 981748, support more than just the top-level documents.
    if (!isTopLevel) {
      return;
    }

    const self = this;
    this._tracedWindowId = id;

    const unwrappedWindow = XPCNativeWrapper.unwrap(window);
    const callback = this._onContentFunctionCall;

    for (const global of this._tracedGlobals) {
      const prototype = unwrappedWindow[global].prototype;
      const properties = Object.keys(prototype);
      properties.forEach(name => overrideSymbol(global, prototype, name, callback));
    }

    for (const name of this._tracedFunctions) {
      overrideSymbol("window", unwrappedWindow, name, callback);
    }

    /**
     * Instruments a method, getter or setter on the specified target object to
     * invoke a callback whenever it is called.
     */
    function overrideSymbol(global, target, name, subcallback) {
      const propertyDescriptor = Object.getOwnPropertyDescriptor(target, name);

      if (propertyDescriptor.get || propertyDescriptor.set) {
        overrideAccessor(global, target, name, propertyDescriptor, subcallback);
        return;
      }
      if (propertyDescriptor.writable && typeof propertyDescriptor.value == "function") {
        overrideFunction(global, target, name, propertyDescriptor, subcallback);
      }
    }

    /**
     * Instruments a function on the specified target object.
     */
    function overrideFunction(global, target, name, descriptor, subcallback) {
      // Invoking .apply on an unxrayed content function doesn't work, because
      // the arguments array is inaccessible to it. Get Xrays back.
      const originalFunc = Cu.unwaiveXrays(target[name]);

      Cu.exportFunction(function(...args) {
        let result;
        try {
          result = Cu.waiveXrays(originalFunc.apply(this, args));
        } catch (e) {
          throw createContentError(e, unwrappedWindow);
        }

        if (self._recording) {
          const type = METHOD_FUNCTION;
          const stack = getStack(name);
          const now = self.targetActor.window.performance.now();
          const timestamp = now - self._timestampEpoch;
          subcallback(unwrappedWindow, global, this, type, name, stack, timestamp,
            args, result);
        }
        return result;
      }, target, { defineAs: name });

      Object.defineProperty(target, name, {
        configurable: descriptor.configurable,
        enumerable: descriptor.enumerable,
        writable: true,
      });
    }

    /**
     * Instruments a getter or setter on the specified target object.
     */
    function overrideAccessor(global, target, name, descriptor, subcallback) {
      // Invoking .apply on an unxrayed content function doesn't work, because
      // the arguments array is inaccessible to it. Get Xrays back.
      const originalGetter = Cu.unwaiveXrays(target.__lookupGetter__(name));
      const originalSetter = Cu.unwaiveXrays(target.__lookupSetter__(name));

      Object.defineProperty(target, name, {
        get: function(...args) {
          if (!originalGetter) {
            return undefined;
          }
          const result = Cu.waiveXrays(originalGetter.apply(this, args));

          if (self._recording) {
            const type = GETTER_FUNCTION;
            const stack = getStack(name);
            const now = self.targetActor.window.performance.now();
            const timestamp = now - self._timestampEpoch;
            subcallback(unwrappedWindow, global, this, type, name, stack, timestamp,
              args, result);
          }
          return result;
        },
        set: function(...args) {
          if (!originalSetter) {
            return;
          }
          originalSetter.apply(this, args);

          if (self._recording) {
            const type = SETTER_FUNCTION;
            const stack = getStack(name);
            const now = self.targetActor.window.performance.now();
            const timestamp = now - self._timestampEpoch;
            subcallback(unwrappedWindow, global, this, type, name, stack, timestamp,
              args, undefined);
          }
        },
        configurable: descriptor.configurable,
        enumerable: descriptor.enumerable,
      });
    }

    /**
     * Stores the relevant information about calls on the stack when
     * a function is called.
     */
    function getStack(caller) {
      let stack;
      try {
        // Using Components.stack wouldn't be a better idea, since it's
        // much slower because it attempts to retrieve the C++ stack as well.
        throw new Error();
      } catch (e) {
        stack = e.stack;
      }

      // Of course, using a simple regex like /(.*?)@(.*):(\d*):\d*/ would be
      // much prettier, but this is a very hot function, so let's sqeeze
      // every drop of performance out of it.
      const calls = [];
      let callIndex = 0;
      let currNewLinePivot = stack.indexOf("\n") + 1;
      let nextNewLinePivot = stack.indexOf("\n", currNewLinePivot);

      while (nextNewLinePivot > 0) {
        const nameDelimiterIndex = stack.indexOf("@", currNewLinePivot);
        const columnDelimiterIndex = stack.lastIndexOf(":", nextNewLinePivot - 1);
        const lineDelimiterIndex = stack.lastIndexOf(":", columnDelimiterIndex - 1);

        if (!calls[callIndex]) {
          calls[callIndex] = { name: "", file: "", line: 0 };
        }
        if (!calls[callIndex + 1]) {
          calls[callIndex + 1] = { name: "", file: "", line: 0 };
        }

        if (callIndex > 0) {
          const file = stack.substring(nameDelimiterIndex + 1, lineDelimiterIndex);
          const line = stack.substring(lineDelimiterIndex + 1, columnDelimiterIndex);
          const name = stack.substring(currNewLinePivot, nameDelimiterIndex);
          calls[callIndex].name = name;
          calls[callIndex - 1].file = file;
          calls[callIndex - 1].line = line;
        } else {
          // Since the topmost stack frame is actually our overwritten function,
          // it will not have the expected name.
          calls[0].name = caller;
        }

        currNewLinePivot = nextNewLinePivot + 1;
        nextNewLinePivot = stack.indexOf("\n", currNewLinePivot);
        callIndex++;
      }

      return calls;
    }
  },

  /**
   * Invoked whenever the current target actor's inner window is destroyed.
   */
  _onGlobalDestroyed: function({window, id, isTopLevel}) {
    if (this._tracedWindowId == id) {
      this.pauseRecording();
      this.eraseRecording();
      this._timestampEpoch = 0;
    }
  },

  /**
   * Invoked whenever an instrumented function is called.
   */
  _onContentFunctionCall: function(...details) {
    // If the consuming tool has finalized call-watcher, ignore the
    // still-instrumented calls.
    if (this._finalized) {
      return;
    }

    const functionCall = new FunctionCallActor(this.conn, details, this._holdWeak);

    if (this._storeCalls) {
      this._functionCalls.push(functionCall);
    }

    if (this.onCall) {
      this.onCall(functionCall);
    } else {
      this.emit("call", functionCall);
    }
  },
};

/**
 * Creates a new error from an error that originated from content but was called
 * from a wrapped overridden method. This is so we can make our own error
 * that does not look like it originated from the call watcher.
 *
 * We use toolkit/loader's parseStack and serializeStack rather than the
 * parsing done in the local `getStack` function, because it does not expose
 * column number, would have to change the protocol models `call-stack-items`
 * and `call-details` which hurts backwards compatibility, and the local `getStack`
 * is an optimized, hot function.
 */
function createContentError(e, win) {
  const { message, name, stack } = e;
  const parsedStack = parseStack(stack);
  const { fileName, lineNumber, columnNumber } = parsedStack[parsedStack.length - 1];
  let error;

  const isDOMException = ChromeUtils.getClassName(e) === "DOMException";
  const constructor = isDOMException ? win.DOMException : (win[e.name] || win.Error);

  if (isDOMException) {
    error = new constructor(message, name);
    Object.defineProperties(error, {
      code: { value: e.code },
      // columnNumber is always 0 for DOMExceptions?
      columnNumber: { value: 0 },
      // note the lowercase `filename`
      filename: { value: fileName },
      lineNumber: { value: lineNumber },
      result: { value: e.result },
      stack: { value: serializeStack(parsedStack) },
    });
  } else {
    // Constructing an error here retains all the stack information,
    // and we can add message, fileName and lineNumber via constructor, though
    // need to manually add columnNumber.
    error = new constructor(message, fileName, lineNumber);
    Object.defineProperty(error, "columnNumber", {
      value: columnNumber,
    });
  }
  return error;
}
