/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const CONSOLE_WORKER_IDS = (exports.CONSOLE_WORKER_IDS = new Set([
  "SharedWorker",
  "ServiceWorker",
  "Worker",
]));

var WebConsoleUtils = {
  /**
   * Given a message, return one of CONSOLE_WORKER_IDS if it matches
   * one of those.
   *
   * @return string
   */
  getWorkerType(message) {
    const innerID = message?.innerID;
    return CONSOLE_WORKER_IDS.has(innerID) ? innerID : null;
  },

  /**
   * Gets the ID of the inner window of this DOM window.
   *
   * @param nsIDOMWindow window
   * @return integer|null
   *         Inner ID for the given window, null if we can't access it.
   */
  getInnerWindowId(window) {
    // Might throw with SecurityError: Permission denied to access property
    // "windowGlobalChild" on cross-origin object.
    try {
      return window.windowGlobalChild.innerWindowId;
    } catch (e) {
      return null;
    }
  },

  /**
   * Recursively gather a list of inner window ids given a
   * top level window.
   *
   * @param nsIDOMWindow window
   * @return Array
   *         list of inner window ids.
   */
  getInnerWindowIDsForFrames(window) {
    const innerWindowID = this.getInnerWindowId(window);
    if (innerWindowID === null) {
      return [];
    }

    let ids = [innerWindowID];

    if (window.frames) {
      for (let i = 0; i < window.frames.length; i++) {
        const frame = window.frames[i];
        ids = ids.concat(this.getInnerWindowIDsForFrames(frame));
      }
    }

    return ids;
  },

  /**
   * Create a grip for the given value. If the value is an object,
   * an object wrapper will be created.
   *
   * @param mixed value
   *        The value you want to create a grip for, before sending it to the
   *        client.
   * @param function objectWrapper
   *        If the value is an object then the objectWrapper function is
   *        invoked to give us an object grip. See this.getObjectGrip().
   * @return mixed
   *         The value grip.
   */
  createValueGrip(value, objectWrapper) {
    switch (typeof value) {
      case "boolean":
        return value;
      case "string":
        return objectWrapper(value);
      case "number":
        if (value === Infinity) {
          return { type: "Infinity" };
        } else if (value === -Infinity) {
          return { type: "-Infinity" };
        } else if (Number.isNaN(value)) {
          return { type: "NaN" };
        } else if (!value && 1 / value === -Infinity) {
          return { type: "-0" };
        }
        return value;
      case "undefined":
        return { type: "undefined" };
      case "object":
        if (value === null) {
          return { type: "null" };
        }
      // Fall through.
      case "function":
      case "record":
      case "tuple":
        return objectWrapper(value);
      default:
        console.error(
          "Failed to provide a grip for value of " + typeof value + ": " + value
        );
        return null;
    }
  },

  /**
   * Remove any frames in a stack that are above a debugger-triggered evaluation
   * and will correspond with devtools server code, which we never want to show
   * to the user.
   *
   * @param array stack
   *        An array of frames, with the topmost first, and each of which has a
   *        'filename' property.
   * @return array
   *         An array of stack frames with any devtools server frames removed.
   *         The original array is not modified.
   */
  removeFramesAboveDebuggerEval(stack) {
    const debuggerEvalFilename = "debugger eval code";

    // Remove any frames for server code above the last debugger eval frame.
    const evalIndex = stack.findIndex(({ filename }, idx, arr) => {
      const nextFrame = arr[idx + 1];
      return (
        filename == debuggerEvalFilename &&
        (!nextFrame || nextFrame.filename !== debuggerEvalFilename)
      );
    });
    if (evalIndex != -1) {
      return stack.slice(0, evalIndex + 1);
    }

    // In some cases (e.g. evaluated expression with SyntaxError), we might not have a
    // "debugger eval code" frame but still have internal ones. If that's the case, we
    // return null as the end user shouldn't see those frames.
    if (
      stack.some(
        ({ filename }) =>
          filename && filename.startsWith("resource://devtools/")
      )
    ) {
      return null;
    }

    return stack;
  },
};

exports.WebConsoleUtils = WebConsoleUtils;
