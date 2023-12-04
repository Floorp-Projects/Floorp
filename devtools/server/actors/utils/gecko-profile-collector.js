/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// The fallback color for unexpected cases
const DEFAULT_COLOR = "grey";

// The default category for unexpected cases
const DEFAULT_CATEGORIES = [
  {
    name: "Mixed",
    color: DEFAULT_COLOR,
    subcategories: ["Other"],
  },
];

// Color for each type of category/frame's implementation
const PREDEFINED_COLORS = {
  interpreter: "yellow",
  baseline: "orange",
  ion: "blue",
  wasm: "purple",
};

/**
 * Utility class that collects the JS tracer data and converts it to a Gecko
 * profile object.
 */
class GeckoProfileCollector {
  #thread = null;
  #stackMap = new Map();
  #frameMap = new Map();
  #categories = DEFAULT_CATEGORIES;
  #currentStack = [];
  #time = 0;

  /**
   * Initialize the profiler and be ready to receive samples.
   */
  start() {
    this.#reset();
    this.#thread = this.#getEmptyThread();
  }

  /**
   * Stop the record and return the gecko profiler data.
   *
   * @return {Object}
   *         The Gecko profile object.
   */
  stop() {
    // Create the profile to return.
    const profile = this.#getEmptyProfile();
    profile.meta.categories = this.#categories;
    profile.threads.push(this.#thread);

    // Cleanup.
    this.#reset();

    return profile;
  }

  /**
   * Clear all the internal state of this class.
   */
  #reset() {
    this.#thread = null;
    this.#stackMap = new Map();
    this.#frameMap = new Map();
    this.#categories = DEFAULT_CATEGORIES;
    this.#currentStack = [];
    this.#time = 0;
  }

  /**
   * Initialize an empty Gecko profile object.
   *
   * @return {Object}
   *         Gecko profile object.
   */
  #getEmptyProfile() {
    const httpHandler = Cc[
      "@mozilla.org/network/protocol;1?name=http"
    ].getService(Ci.nsIHttpProtocolHandler);
    return {
      meta: {
        // Currently interval is 1, but we could change it to a lower number
        // when we have durations coming from js tracer.
        interval: 1,
        startTime: 0,
        product: Services.appinfo.name,
        importedFrom: "JS Tracer",
        version: 28,
        presymbolicated: true,
        abi: Services.appinfo.XPCOMABI,
        misc: httpHandler.misc,
        oscpu: httpHandler.oscpu,
        platform: httpHandler.platform,
        processType: Services.appinfo.processType,
        categories: [],
        stackwalk: 0,
        toolkit: Services.appinfo.widgetToolkit,
        appBuildID: Services.appinfo.appBuildID,
        sourceURL: Services.appinfo.sourceURL,
        physicalCPUs: 0,
        logicalCPUs: 0,
        CPUName: "",
        markerSchema: [],
      },
      libs: [],
      pages: [],
      threads: [],
      processes: [],
    };
  }

  /**
   * Generate a thread object to be stored in the Gecko profile object.
   */
  #getEmptyThread() {
    return {
      processType: "default",
      processStartupTime: 0,
      processShutdownTime: null,
      registerTime: 0,
      unregisterTime: null,
      pausedRanges: [],
      name: "GeckoMain",
      "eTLD+1": "JS Tracer",
      isMainThread: true,
      pid: Services.appinfo.processID,
      tid: 0,
      samples: {
        schema: {
          stack: 0,
          time: 1,
          eventDelay: 2,
        },
        data: [],
      },
      markers: {
        schema: {
          name: 0,
          startTime: 1,
          endTime: 2,
          phase: 3,
          category: 4,
          data: 5,
        },
        data: [],
      },
      stackTable: {
        schema: {
          prefix: 0,
          frame: 1,
        },
        data: [],
      },
      frameTable: {
        schema: {
          location: 0,
          relevantForJS: 1,
          innerWindowID: 2,
          implementation: 3,
          line: 4,
          column: 5,
          category: 6,
          subcategory: 7,
        },
        data: [],
      },
      stringTable: [],
    };
  }

  /**
   * Record a new sample to be stored in the Gecko profile object.
   *
   * @param {Object} frame
   *        Object describing a frame with following attributes:
   *        - {String} name
   *          Human readable name for this frame.
   *        - {String} url
   *          URL of the running script.
   *        - {Number} lineNumber
   *          Line currently executing for this script.
   *        - {Number} columnNumber
   *          Column currently executing for this script.
   *        - {String} category
   *          Which JS implementation is being used for this frame: interpreter, baseline, ion or wasm.
   *          See Debugger.frame.implementation.
   */
  addSample(frame, depth) {
    const currentDepth = this.#currentStack.length;
    if (currentDepth == depth) {
      // We are in the same depth and executing another frame. Replace the
      // current frame with the new one.
      this.#currentStack[currentDepth] = frame;
    } else if (currentDepth < depth) {
      // We are going deeper in the stack. Push the new frame.
      this.#currentStack.push(frame);
    } else {
      // We are going back in the stack. Pop frames until we reach the right depth.
      this.#currentStack.length = depth;
      this.#currentStack[depth] = frame;
    }

    const stack = this.#currentStack.reduce((prefix, stackFrame) => {
      const frameIndex = this.#getOrCreateFrame(stackFrame);
      return this.#getOrCreateStack(frameIndex, prefix);
    }, null);
    this.#thread.samples.data.push([
      stack,
      // We put simply 1 sample (1ms) for each frame. We can change it in the
      // future if we can get the duration of the frame.
      this.#time++,
      0, // eventDelay
    ]);
  }

  #getOrCreateFrame(frame) {
    const { frameTable, stringTable } = this.#thread;
    const frameString = `${frame.name}:${frame.url}:${frame.lineNumber}:${frame.columnNumber}:${frame.category}`;
    let frameIndex = this.#frameMap.get(frameString);

    if (frameIndex === undefined) {
      frameIndex = frameTable.data.length;
      const location = stringTable.length;
      // Profiler frontend except a particular string to match the source URL:
      // `functionName (http://script.url/:1234:1234)`
      // https://github.com/firefox-devtools/profiler/blob/dab645b2db7e1b21185b286f96dd03b77f68f5c3/src/profile-logic/process-profile.js#L518
      stringTable.push(
        `${frame.name} (${frame.url}:${frame.lineNumber}:${frame.columnNumber})`
      );

      const category = this.#getOrCreateCategory(frame.category);

      frameTable.data.push([
        location,
        true, // relevantForJS
        0, // innerWindowID
        null, // implementation
        frame.lineNumber, // line
        frame.columnNumber, // column
        category,
        0, // subcategory
      ]);
      this.#frameMap.set(frameString, frameIndex);
    }

    return frameIndex;
  }

  #getOrCreateStack(frameIndex, prefix) {
    const { stackTable } = this.#thread;
    const key = prefix === null ? `${frameIndex}` : `${frameIndex},${prefix}`;
    let stack = this.#stackMap.get(key);

    if (stack === undefined) {
      stack = stackTable.data.length;
      stackTable.data.push([prefix, frameIndex]);
      this.#stackMap.set(key, stack);
    }
    return stack;
  }

  #getOrCreateCategory(category) {
    const categories = this.#categories;
    let categoryIndex = categories.findIndex(c => c.name === category);

    if (categoryIndex === -1) {
      categoryIndex = categories.length;
      categories.push({
        name: category,
        color: PREDEFINED_COLORS[category] ?? DEFAULT_COLOR,
        subcategories: ["Other"],
      });
    }
    return categoryIndex;
  }
}

exports.GeckoProfileCollector = GeckoProfileCollector;
