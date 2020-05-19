/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// SpiderMonkey JS shell benchmark script
//
// Usage: run $JS spidermonkey.js --help

loadRelativeToScript("shell-bench.js");

var SpiderMonkey = class extends Host {
  start_turn() {}

  end_turn() {
    clearKeptObjects();
    maybegc();
    drainJobQueue();
  }

  suspend(duration) {
    sleep(duration);
  }

  get minorGCCount() {
    return performance.mozMemory.gc.minorGCCount;
  }
  get majorGCCount() {
    return performance.mozMemory.gc.majorGCCount;
  }
  get GCSliceCount() {
    return performance.mozMemory.gc.sliceCount;
  }
  get gcBytes() {
    return performance.mozMemory.gc.zone.gcBytes;
  }
  get gcAllocTrigger() {
    return performance.mozMemory.gc.zone.gcAllocTrigger;
  }

  features = {
    haveMemorySizes: true,
    haveGCCounts: true,
  };
};

var gHost = new SpiderMonkey();

run(scriptArgs);
print(`Waited total of ${waited} seconds`);
print("\nTest results:\n");
report_results();

var outfile = "spidermonkey-results.json";
var origOut = redirect(outfile);
print(JSON.stringify(gPerf.results));
redirect(origOut);
print(`Wrote detailed results to ${outfile}`);
