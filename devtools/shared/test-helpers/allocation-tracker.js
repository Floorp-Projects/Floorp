/* this source code form is subject to the terms of the mozilla public
 * license, v. 2.0. if a copy of the mpl was not distributed with this
 * file, you can obtain one at http://mozilla.org/mpl/2.0/. */

/*
 * This file helps tracking Javascript object allocations.
 * It is only included in local builds as a debugging helper.
 *
 * It is typicaly used when running DevTools tests (either mochitests or DAMP).
 * To use it, you need to set the following environment variable:
 *   DEBUG_DEVTOOLS_ALLOCATIONS="normal"
 *     This will only print the number of JS objects created during your test.
 *   DEBUG_DEVTOOLS_ALLOCATIONS="verbose"
 *     This will print the allocation sites of all the JS objects created during your
 *     test. i.e. from which files and lines the objects have been created.
 * In both cases, look for "DEVTOOLS ALLOCATION" in your terminal to see tracker's
 * output.
 *
 * But you can also import it from your test script if you want to focus on one
 * particular piece of code:
 *   const { allocationTracker } =
 *     require("devtools/shared/test-helpers/allocation-tracker");
 *   // Calling `allocationTracker` will immediately start recording allocations
 *   let tracker = allocationTracker();
 *
 *   // Do something
 *
 *   // If you want to log all the allocation sites, call this method:
 *   tracker.logAllocationSites();
 *   // Or, if you want to only print the number of objects being allocated, call this:
 *   tracker.logCount();
 *   // Once you are done, stop the tracker as it slow down execution a lot.
 *   tracker.stop();
 */

"use strict";

const { Cu, Cc, Ci } = require("chrome");
const ChromeUtils = require("ChromeUtils");

const MemoryReporter = Cc["@mozilla.org/memory-reporter-manager;1"].getService(
  Ci.nsIMemoryReporterManager
);

const global = Cu.getGlobalForObject(this);
const { addDebuggerToGlobal } = ChromeUtils.import(
  "resource://gre/modules/jsdebugger.jsm"
);
addDebuggerToGlobal(global);

/**
 * Start recording JS object allocations.
 *
 * @param Object watchGlobal
 *        One global object to observe. Only allocation made from this global
 *        will be recorded.
 * @param Boolean watchAllGlobals
 *        If true, allocations from everywhere are going to be recorded.
 * @param Boolean watchAllGlobals
 *        If true, only allocations made from DevTools contexts are going to be recorded.
 */
exports.allocationTracker = function({
  watchGlobal,
  watchAllGlobals,
  watchDevToolsGlobals,
} = {}) {
  dump("DEVTOOLS ALLOCATION: Start logging allocations\n");
  let dbg = new global.Debugger();

  // Enable allocation site tracking, to have the stack for each allocation
  dbg.memory.trackingAllocationSites = true;
  // Force saving *all* the allocation sites
  dbg.memory.allocationSamplingProbability = 1.0;
  // Bumps the default buffer size, which may prevent recording all the test allocations
  dbg.memory.maxAllocationsLogLength = 5000000;

  let acceptGlobal;
  if (watchGlobal) {
    acceptGlobal = () => false;
    dbg.addDebuggee(watchGlobal);
  } else if (watchAllGlobals) {
    acceptGlobal = () => true;
  } else if (watchDevToolsGlobals) {
    // Only accept globals related to DevTools
    const builtinGlobal = require("devtools/shared/builtin-modules");
    acceptGlobal = g => {
      // self-hosting-global crashes when trying to call unsafeDereference
      if (g.class == "self-hosting-global") {
        dump("TRACKER NEW GLOBAL: - : " + g.class + "\n");
        return false;
      }
      const ref = g.unsafeDereference();
      const location = Cu.getRealmLocation(ref);
      let accept = !!location.match(/devtools/i);

      // Also ignore the dedicated Sandbox used to spawn builtin-modules,
      // as well as its internal Sandbox used to fetch various platform globals.
      // We ignore the global used by the dedicated loader used to load
      // the allocation-tracker module.
      if (
        ref == Cu.getGlobalForObject(builtinGlobal) ||
        ref == builtinGlobal.internalSandbox
      ) {
        accept = false;
      }

      dump(
        "TRACKER NEW GLOBAL: " + (accept ? "+" : "-") + " : " + location + "\n"
      );
      return accept;
    };
  }

  // Watch all globals
  if (watchAllGlobals || watchDevToolsGlobals) {
    dbg.addAllGlobalsAsDebuggees();

    for (const g of dbg.getDebuggees()) {
      if (!acceptGlobal(g)) {
        dbg.removeDebuggee(g);
      }
    }
  }

  // Remove this global to ignore all its object/JS
  dbg.removeDebuggee(global);

  // addAllGlobalsAsDebuggees won't automatically track new ones,
  // so ensure tracking all new globals
  dbg.onNewGlobalObject = function(g) {
    if (acceptGlobal(g)) {
      dbg.addDebuggee(g);
    }
  };

  return {
    get overflowed() {
      return dbg.memory.allocationsLogOverflowed;
    },

    /**
     * Print to stdout data about all recorded allocations
     *
     * It prints an array of allocations per file, sorted by files allocating the most
     * objects. And get detail of allocation per line.
     *
     *   [{ src: "chrome://devtools/content/framework/toolbox.js",
     *      count: 210, // Total # of allocs for toolbox.js
     *      lines: [
     *       "10: 200", // toolbox.js allocation 200 objects on line 10
     *       "124: 10
     *      ]
     *    },
     *    { src: "chrome://devtools/content/inspector/inspector.js",
     *      count: 12,
     *      lines: [
     *       "20: 12",
     *      ]
     *    }]
     *
     * @param first Number
     *        Retrieve only the top $first script allocation the most
     *        objects
     */
    logAllocationSites({ first = 5 } = {}) {
      // Fetch all allocation sites from Debugger API
      const allocations = dbg.memory.drainAllocationsLog();

      // Process Debugger API data to store allocations by file
      // sources = {
      //   "chrome://devtools/content/framework/toolbox.js": {
      //     count: 10, // total # of allocs for toolbox.js
      //     lines: {
      //       10: 200, // total # of allocs for toolbox.js line 10
      //       124: 10, // same, for line 124
      //       ..
      //     }
      //   }
      // }
      const sources = {};
      for (const alloc of allocations) {
        const { frame } = alloc;
        let src = "UNKNOWN";
        let line = -1;
        try {
          if (frame) {
            src = frame.source || "UNKNOWN";
            line = frame.line || -1;
          }
        } catch (e) {
          // For some frames accessing source throws
        }

        let item = sources[src];
        if (!item) {
          item = sources[src] = { count: 0, lines: {} };
        }
        item.count++;
        if (line != -1) {
          if (!item.lines[line]) {
            item.lines[line] = 0;
          }
          item.lines[line]++;
        }
      }

      const allocationList = Object.entries(sources)
        // Sort by number of total object
        .sort(([srcA, itemA], [srcB, itemB]) => itemA.count < itemB.count)
        // Keep only the first 5 sources, with the most allocations
        .filter((_, i) => i < first)
        .map(([src, item]) => {
          const lines = [];
          Object.entries(item.lines)
            .filter(([line, count]) => count > 5)
            .sort(([lineA, countA], [lineB, countB]) => {
              if (countA != countB) {
                return countA < countB;
              }
              return lineA < lineB;
            })
            .forEach(([line, count]) => {
              // Compress the data to make it readable on stdout
              lines.push(line + ": " + count);
            });
          return { src, count: item.count, lines };
        });
      dump(
        "DEVTOOLS ALLOCATION: Javascript object allocations: " +
          allocations.length +
          "\n" +
          JSON.stringify(allocationList, null, 2) +
          "\n"
      );
    },

    logCount() {
      dump(
        "DEVTOOLS ALLOCATION: Javascript object allocations: " +
          this.countAllocations() +
          "\n"
      );
    },

    countAllocations() {
      // Fetch all allocation sites from Debugger API
      const allocations = dbg.memory.drainAllocationsLog();
      return allocations.length;
    },

    flushAllocations() {
      dbg.memory.drainAllocationsLog();
    },

    /**
     * Compute the live count of object currently allocated.
     *
     * `objects` attribute will count all the objects,
     * while `objectsWithNoStack` will report how many are missing allocation site/stack.
     */
    stillAllocatedObjects() {
      const sensus = dbg.memory.takeCensus({
        breakdown: { by: "allocationStack" },
      });
      let objectsWithStack = 0;
      let objectsWithoutStack = 0;
      for (const [k, v] of sensus.entries()) {
        // Objects with missing stack will all be keyed under "noStack" string,
        // while all others will have a stack object as key.
        if (k === "noStack") {
          objectsWithoutStack += v.count;
        } else {
          objectsWithStack += v.count;
        }
      }
      return { objectsWithStack, objectsWithoutStack };
    },

    /**
     * Reports the amount of OS memory used by the current process.
     */
    getAllocatedMemory() {
      return MemoryReporter.residentUnique;
    },

    async doGC() {
      // In order to get stable results, we really have to do 3 GC attempts
      // *and* do wait for 1s between each GC.
      const numCycles = 3;
      for (let i = 0; i < numCycles; i++) {
        Cu.forceGC();
        Cu.forceCC();
        await new Promise(resolve => Cu.schedulePreciseShrinkingGC(resolve));

        // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
        await new Promise(resolve => setTimeout(resolve, 1000));
      }
    },

    stop() {
      dump("DEVTOOLS ALLOCATION: Stop logging allocations\n");
      dbg.onNewGlobalObject = undefined;
      dbg.removeAllDebuggees();
      dbg = null;
    },
  };
};
