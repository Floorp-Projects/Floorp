var FPS = 60;
var gNumSamples = 500;

var waited = 0;

////////////// Host functionality /////////////////

function start_turn() {}

function end_turn() {
  clearKeptObjects();
}

function suspend(duration) {
  sleep(duration);
}

///////////////// Main /////////////////

loadRelativeToScript("harness.js");
loadRelativeToScript("perf.js");
loadRelativeToScript("test_list.js");

var tests = new Map();
foreach_test_file(f => loadRelativeToScript(f));
for (const [name, info] of tests.entries()) {
  if ("enabled" in info && !info.enabled) {
    tests.delete(name);
  }
}

function tick(loadMgr, timestamp) {
  start_turn();
  const events = loadMgr.tick(timestamp);
  end_turn();
  return events;
}

function now() {
  return performance.now();
}

function wait_for_next_frame(t0, t1) {
  const elapsed = (t1 - t0) / 1000;
  const period = 1 / FPS;
  const used = elapsed % period;
  const delay = period - used;
  waited += delay;
  suspend(delay);
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
  const loadMgr = new AllocationLoadManager(tests);
  const perf = new FrameHistory(gNumSamples);

  loads = loads.length ? loads : tests.keys();
  loadMgr.startCycle(loads);
  perf.start();

  const t0 = now();

  let possible = 0;
  let frames = 0;
  while (loadMgr.load_running()) {
    const timestamp = now();
    perf.on_frame(timestamp);
    const events = tick(loadMgr, timestamp);
    frames++;
    if (report_events(events, loadMgr)) {
      possible += (loadMgr.testDurationMS / 1000) * FPS;
      print(
        `  observed ${frames} / ${possible} frames in ${(now() - t0) /
          1000} seconds`
      );
    }
    wait_for_next_frame(t0, now());
  }
}

run(scriptArgs);
print(`Waited total of ${waited} seconds`);
