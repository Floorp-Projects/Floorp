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
 *   tracker.logAllocationLog();
 *   // Or, if you want to only print the number of objects being allocated, call this:
 *   tracker.logCount();
 *   // Once you are done, stop the tracker as it slow down execution a lot.
 *   tracker.stop();
 */

"use strict";

const { Cu, Cc, Ci } = require("chrome");

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
    const builtinGlobal = require("devtools/shared/loader/builtin-modules");
    acceptGlobal = g => {
      // self-hosting-global crashes when trying to call unsafeDereference
      if (g.class == "self-hosting-global") {
        dump("TRACKER NEW GLOBAL: - : " + g.class + "\n");
        return false;
      }
      let ref = g.unsafeDereference();
      // If we are on a toolbox's iframe, typically each panel's iframe
      // retrieve the toolbox iframe via window.top
      if (g.class == "Window" && ref.top) {
        ref = ref.top;
      }
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

    async startRecordingAllocations(debug_allocations) {
      // Do a first pass of GC, to ensure all to-be-freed objects from the first run
      // are really freed.
      // We have to temporarily disable allocation-site recording in order to ensure
      // freeing everything and especially avoid retaining objects in the allocation-log
      // related to `drainAllocationLog` feature.
      dbg.memory.allocationSamplingProbability = 0.0;
      // Also force clearing the allocation log in order to prevent holding alive globals
      // which have been destroyed before we start recording
      this.flushAllocations();
      await this.doGC();
      dbg.memory.allocationSamplingProbability = 1.0;

      // Measure the current process memory usage
      const memory = this.getAllocatedMemory();

      // Then, record how many objects were already allocated, which should not be declared
      // as potential leaks. For ex, there is all the modules already loaded
      // in the main DevTools loader.
      const objects = this.stillAllocatedObjects();

      // Flush the allocations so that the next call to logAllocationLog
      // ignore allocations which happened before this call.
      if (debug_allocations == "allocations") {
        this.flushAllocations();
      }

      // Retrieve all allocation sites of all the objects already allocated.
      // So that we can ignore them when we stop the record.
      const allocations =
        debug_allocations == "leaks" ? this.getAllAllocations() : null;

      this.data = { memory, objects, allocations };
      return this.data;
    },

    async stopRecordingAllocations(debug_allocations) {
      // We have to flush the allocation log in order to prevent leaking some objects
      // being hold in memory solely by their allocation-site (i.e. `SavedFrame` in `Debugger::allocationsLog`)
      if (debug_allocations != "allocations") {
        this.flushAllocations();
      }

      // In the content process we watch for all globals.
      // Disable allocation record immediately, as we get some allocation reported by the allocation-tracker itself.
      if (watchAllGlobals) {
        dbg.memory.allocationSamplingProbability = 0.0;
      }

      // Before computing allocations, re-do some GCs in order to free all what is to-be-freed.
      await this.doGC();

      // If we are in the parent process, we watch only for devtools globals.
      // So we can more safely assert that no allocation occured while doing the GCs.
      // If means that the test we are recording is having pending operation which aren't properly recorded.
      if (!watchAllGlobals) {
        const allocations = dbg.memory.drainAllocationsLog();
        if (allocations.length) {
          this.logAllocationLog(
            allocations,
            "Allocation that happened during the GC"
          );
          console.error(
            "Allocation happened during the GC. Are you waiting correctly before calling stopRecordingAllocations?"
          );
        }
      }

      const memory = this.getAllocatedMemory();
      const objects = this.stillAllocatedObjects();

      let leaks;
      if (debug_allocations == "allocations") {
        this.logAllocationLog();
      } else if (debug_allocations == "leaks") {
        leaks = this.logAllocationSitesDiff(this.data.allocations);
      }

      return {
        objectsWithoutStack:
          objects.objectsWithoutStack - this.data.objects.objectsWithoutStack,
        objectsWithStack:
          objects.objectsWithStack - this.data.objects.objectsWithStack,
        memory: memory - this.data.memory,
        leaks,
      };
    },

    /**
     * Return the collection of currently allocated JS Objects.
     *
     * This returns an object whose structure is documented in logAllocationSites.
     */
    getAllAllocations() {
      const sensus = dbg.memory.takeCensus({
        breakdown: { by: "allocationStack" },
      });
      const sources = {};
      for (const [k, v] of sensus.entries()) {
        const src = k.source || "UNKNOWN";
        const line = k.line || "?";
        const count = v.count;

        let item = sources[src];
        if (!item) {
          item = sources[src] = { count: 0, lines: {} };
        }
        item.count += count;
        if (line != -1) {
          if (!item.lines[line]) {
            item.lines[line] = 0;
          }
          item.lines[line] += count;
        }
      }
      return sources;
    },

    /**
     * Substract count of `previousSources` from `newSources`.
     * This help know which allocations where done between `previousSources` and `newSources` records,
     * and, are still allocated.
     *
     * The structure of source objects is documented in logAllocationSites.
     */
    sourcesDiff(previousSources, newSources) {
      for (const src in previousSources) {
        const previousItem = previousSources[src];
        const item = newSources[src];
        if (!item) {
          continue;
        }
        item.count -= previousItem.count;

        for (const line in previousItem.lines) {
          const count = previousItem.lines[line];
          if (line != -1) {
            if (!item.lines[line]) {
              continue;
            }
            item.lines[line] -= count;
          }
        }
      }
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
    logAllocationSites(message, sources, { first = 1000 } = {}) {
      const allocationList = Object.entries(sources)
        // Sort by number of total object
        .sort(([srcA, itemA], [srcB, itemB]) => itemB.count - itemA.count)
        // Keep only the first n-th sources, with the most allocations
        .filter((_, i) => i < first)
        .map(([src, item]) => {
          const lines = [];
          Object.entries(item.lines)
            // Filter out lines where we only freed objects
            .filter(([line, count]) => count > 0)
            .sort(([lineA, countA], [lineB, countB]) => {
              if (countA != countB) {
                return countB - countA;
              }
              return lineB - lineA;
            })
            .forEach(([line, count]) => {
              // Compress the data to make it readable on stdout
              lines.push(line + ": " + count);
            });
          return { src, count: item.count, lines };
        })
        // Filter out modules where we only freed objects
        .filter(({ count }) => count > 0);
      dump(
        "DEVTOOLS ALLOCATION: " +
          message +
          ":\n" +
          JSON.stringify(allocationList, null, 2) +
          "\n"
      );
      return allocationList;
    },

    /**
     * This method requires a previous call to getAllAllocations
     * and will print only the allocation sites which are still allocated.
     * Usage:
     *   const previousSources = this.getAllAllocations();
     *     ... exercice something, which may leak ...
     *   this.logAllocationSitesDiff(previousSources);
     */
    logAllocationSitesDiff(previousSources) {
      const newSources = this.getAllAllocations();
      this.sourcesDiff(previousSources, newSources);
      return this.logAllocationSites("allocations which leaked", newSources);
    },

    /**
     * Convert allocation structure coming out from Memory API's `drainAllocationsLog()`
     * to source structure documented in logAllocationSites.
     */
    allocationsToSources(allocations) {
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
      return sources;
    },

    /**
     * This method will log all the allocations that happened since the last call
     * to this method -or- to `flushAllocations`.
     * Reported allocations may have been freed.
     * Use `logAllocationSitesDiff` to know what hasn't been freed.
     */
    logAllocationLog(allocations, msg = "") {
      if (!allocations) {
        allocations = dbg.memory.drainAllocationsLog();
      }
      const sources = this.allocationsToSources(allocations);
      return this.logAllocationSites(
        msg
          ? msg
          : "all allocations (which may be freed or are still allocated)",
        sources
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

    /**
     * Reset the allocation log, so that the next call to logAllocationLog/drainAllocationsLog
     * will report all allocations which happened after this call to flushAllocations.
     */
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

      // Also call minimizeMemoryUsage as that's the only way to purge JIT cache.
      // CachedIR objects (JIT related objects) are ultimately leading to keep
      // all transient globals in memory. For some reason, when enabling trackingAllocationSites=true
      // we compute stack traces (SavedFrame) for each object being allocated.
      // This either create new CachedIR -or- force holding alive existing CachedIR
      // and CachedIR itself hold strong references to the transient globals.
      // See bug 1733480.
      await new Promise(resolve => MemoryReporter.minimizeMemoryUsage(resolve));
    },

    /**
     * Return the absolute file path to a memory snapshot.
     * This is used to compute dominator trees in `traceObjects`.
     */
    getSnapshotFile() {
      return ChromeUtils.saveHeapSnapshot({ debugger: dbg });
    },

    /**
     * Print information about why a list of objects are being held in memory.
     *
     * @param Array<NodeId> objects
     *        List of NodeId's of objects to debug. NodeIds can be retrieved
     *        via ChromeUtils.getObjectNodeId.
     * @param String snapshotFile
     *        Absolute path to a Heap snapshot file retrieved via this.getSnapshotFile.
     *        This is used to trace content process objects. We have to record the snapshot
     *        from the content process, but can only read it from the parent process because
     *        of I/O restrictions in content processes.
     */
    traceObjects(objects, snapshotFile) {
      // There is no API to get the heap snapshot at runtime,
      // the only way is to save it to disk and then load it from disk
      if (!snapshotFile) {
        snapshotFile = this.getSnapshotFile();
      }
      const snapshot = ChromeUtils.readHeapSnapshot(snapshotFile);

      function getObjectClass(id) {
        if (!id) {
          return "<null>";
        }
        try {
          let stack = [...snapshot.describeNode({ by: "allocationStack" }, id)];
          let line;
          if (stack) {
            stack = stack.find(([src]) => src != "noStack");
            if (stack) {
              line = stack[0].line;
              stack = stack[0].source;
              if (stack) {
                const pstack = stack;
                stack = stack.match(/\/([^\/]+)$/);
                if (stack) {
                  stack = stack[1];
                } else {
                  stack = pstack;
                }
              } else {
                stack = "no-source";
              }
            } else {
              stack = "no-stack";
            }
          } else {
            stack = "no-desc";
          }
          return (
            Object.entries(
              snapshot.describeNode({ by: "objectClass" }, id)
            )[0][0] + (stack ? "@" + stack + ":" + line : "")
          );
        } catch (e) {
          if (e.name == "NS_ERROR_ILLEGAL_VALUE") {
            return "<not-in-memory-snapshot:is-from-untracked-global?>";
          }
          return "<invalid:" + id + ":" + e + ">";
        }
      }
      function printPath(src, dst) {
        let paths;
        try {
          paths = snapshot.computeShortestPaths(src, [dst], 10);
        } catch (e) {}
        if (paths && paths.has(dst)) {
          let pathLength = Infinity;
          for (const path of paths.get(dst)) {
            // Only print the smaller paths.
            // The longer ones will only repeat the smaller ones, with some extra edges.
            if (path.length > pathLength) {
              continue;
            }
            pathLength = path.length;
            dump(
              "- " +
                path
                  .map(
                    ({ predecessor, edge }) =>
                      getObjectClass(predecessor) + "." + edge
                  )
                  .join("\n \\--> ") +
                "\n \\--> " +
                getObjectClass(dst) +
                "\n"
            );
          }
        } else {
          dump("NO-PATH\n");
        }
      }

      const tree = snapshot.computeDominatorTree();
      for (const objectNodeId of objects) {
        dump(" # Tracing: " + getObjectClass(objectNodeId) + "\n");

        // Print the path from the global object down to leaked object.
        // This print the allocation site of each object which has a reference
        // to another object, ultimately leading to our leaked object.
        dump("### Path(s) from root:\n");
        printPath(tree.root, objectNodeId);

        /**
         * This happens to be redundant with printPath, but printed the other way around.
         *
        // Print the dominators.
        // i.e. from the leaked object, print all parent objects whichs
        // keeps a reference to the previous object, up to a global object.
        dump("### Dominators:\n");
        let node = objectNodeId,
        dump(" " + getObjectClass(node) + "\n");
        while ((node = tree.getImmediateDominator(node))) {
          dump(" ^-- " + getObjectClass(node) + "\n");
        }
        */

        /**
         * In case you are not able to figure out what the object is.
         * This will print all what it keeps allocated,
         * kinds of list of attributes
         *
        dump("### Dominateds:\n");
        node = objectNodeId,
        dump(" " + getObjectClass(node) + "\n");
        for (const n of tree.getImmediatelyDominated(objectNodeId)) {
          dump(" --> " + getObjectClass(n) + "\n");
        }
        */
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
