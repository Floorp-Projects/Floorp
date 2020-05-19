/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var FPS = 60;
var gNumSamples = 500;
var waited = 0;

// This requires a gHost to have been created that provides host-specific
// facilities. See eg spidermonkey.js.

loadRelativeToScript("harness.js");
loadRelativeToScript("perf.js");
loadRelativeToScript("test_list.js");

var gPerf = new PerfTracker();

var tests = new Map();
foreach_test_file(f => loadRelativeToScript(f));
for (const [name, info] of tests.entries()) {
  if ("enabled" in info && !info.enabled) {
    tests.delete(name);
  }
}

function tick(loadMgr, timestamp) {
  gPerf.before_mutator(timestamp);
  gHost.start_turn();
  const events = loadMgr.tick(timestamp);
  gHost.end_turn();
  gPerf.after_mutator(timestamp);
  return events;
}

function wait_for_next_frame(t0, t1) {
  const elapsed = (t1 - t0) / 1000;
  const period = 1 / FPS;
  const used = elapsed % period;
  const delay = period - used;
  waited += delay;
  gHost.suspend(delay);
}

function report_events(events, loadMgr) {
  let ended = false;
  if (events & loadMgr.LOAD_ENDED) {
    print(`${loadMgr.lastActive.name} ended`);
    ended = true;
  }
  if (events & loadMgr.LOAD_STARTED) {
    print(`${loadMgr.activeLoad().name} starting`);
  }
  return ended;
}

function run(loads) {
  const sequence = [];
  for (const mut of loads) {
    if (tests.has(mut)) {
      sequence.push(mut);
    } else if (mut === "all") {
      sequence.push(...tests.keys());
    } else {
      sequence.push(...[...tests.keys()].filter(t => t.includes(mut)));
    }
  }
  if (loads.length === 0) {
    sequence.push(...tests.keys());
  }

  const loadMgr = new AllocationLoadManager(tests);
  const perf = new FrameHistory(gNumSamples);

  loadMgr.startCycle(sequence);
  perf.start();

  const t0 = gHost.now();

  let possible = 0;
  let frames = 0;
  while (loadMgr.load_running()) {
    const timestamp = gHost.now();
    perf.on_frame(timestamp);
    const events = tick(loadMgr, timestamp);
    frames++;
    gPerf.handle_tick_events(events, loadMgr, timestamp, gHost.now());
    if (report_events(events, loadMgr)) {
      possible += (loadMgr.testDurationMS / 1000) * FPS;
      print(
        `  observed ${frames} / ${possible} frames in ${(gHost.now() - t0) /
          1000} seconds`
      );
    }
    wait_for_next_frame(t0, gHost.now());
  }
}

function report_results() {
  for (const result of gPerf.results) {
    const {
      load,
      elapsed_time,
      mutating,
      mutating_and_gc_fraction,
      suspended,
      full_time,
      frames,
      dropped_60fps_frames,
      dropped_60fps_fraction,
      minorGCs,
      majorGCs,
    } = result;

    const drop_pct = percent(dropped_60fps_fraction);
    const mut_pct = percent(mutating_and_gc_fraction);
    const mut_sec = mutating.toFixed(2);
    const full_sec = full_time.toFixed(2);
    const susp_sec = suspended.toFixed(2);
    print(`${load.name}:
  ${frames} (60fps) frames seen out of expected ${Math.floor(full_time * 60)}
  ${dropped_60fps_frames} = ${drop_pct} 60fps frames dropped
  ${mut_pct} of run spent mutating and GCing (${mut_sec}sec out of ${full_sec}sec vs ${susp_sec} sec waiting)
  ${minorGCs} minor GCs, ${majorGCs} major GCs
`);
  }
}
