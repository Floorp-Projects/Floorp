/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var features = {
  trackingSizes: "mozMemory" in performance,
  showingGCs: "mozMemory" in performance,
};

var stroke = {
  gcslice: "rgb(255,100,0)",
  minor: "rgb(0,255,100)",
  initialMajor: "rgb(180,60,255)",
};

// Per-frame time sampling infra. Also GC'd: hopefully will not perturb things too badly.
var numSamples = 500;
var sampleIndex = 0;
var sampleTime = 16; // ms
var gHistogram = new Map(); // {ms: count}

var delays = new Array(numSamples);
var gcs = new Array(numSamples);
var minorGCs = new Array(numSamples);
var majorGCs = new Array(numSamples);
var slices = new Array(numSamples);
var gcBytes = new Array(numSamples);
var mallocBytes = new Array(numSamples);

var latencyGraph;
var memoryGraph;
var ctx;
var memoryCtx;

var loadState = "(init)"; // One of '(active)', '(inactive)', '(N/A)'
var testState = "idle"; // One of 'idle' or 'running'.

class FrameTimer {
  constructor() {
    // Start time of the current active test, adjusted for any time spent
    // stopped (so `now - this.start` is how long the current active test
    // has run for.)
    this.start = undefined;

    // Timestamp of callback following the previous frame.
    this.prev = undefined;

    // Timestamp when drawing was paused, or zero if drawing is active.
    this.stopped = 0;
  }

  is_stopped() {
    return this.stopped != 0;
  }

  start_recording(now = performance.now()) {
    this.start = this.prev = now;
  }

  on_frame_finished(now = performance.now()) {
    const delay = now - this.prev;
    this.prev = now;
    return delay;
  }

  pause(now = performance.now()) {
    this.stopped = now;
    // Abuse this.prev to store the time elapsed since the previous frame.
    // This will be used to adjust this.prev when we resume.
    this.prev = now - this.prev;
  }

  resume(now = performance.now()) {
    this.prev = now - this.prev;
    const stop_duration = now - this.stopped;
    this.start += stop_duration;
    this.stopped = 0;
  }
}

var gFrameTimer = new FrameTimer();
function parse_units(v) {
  if (!v.length) {
    return NaN;
  }
  var lastChar = v[v.length - 1].toLowerCase();
  if (!isNaN(parseFloat(lastChar))) {
    return parseFloat(v);
  }
  var units = parseFloat(v.substr(0, v.length - 1));
  if (lastChar == "k") {
    return units * 1e3;
  }
  if (lastChar == "m") {
    return units * 1e6;
  }
  if (lastChar == "g") {
    return units * 1e9;
  }
  return NaN;
}

function Graph(ctx) {
  this.ctx = ctx;

  var { height } = ctx.canvas;
  this.layout = {
    xAxisLabel_Y: height - 20,
  };
}

Graph.prototype.xpos = index => index * 2;

Graph.prototype.clear = function() {
  var { width, height } = this.ctx.canvas;
  this.ctx.clearRect(0, 0, width, height);
};

Graph.prototype.drawScale = function(delay) {
  this.drawHBar(delay, `${delay}ms`, "rgb(150,150,150)");
};

Graph.prototype.draw60fps = function() {
  this.drawHBar(1000 / 60, "60fps", "#00cf61", 25);
};

Graph.prototype.draw30fps = function() {
  this.drawHBar(1000 / 30, "30fps", "#cf0061", 25);
};

Graph.prototype.drawAxisLabels = function(x_label, y_label) {
  var ctx = this.ctx;
  var { width, height } = ctx.canvas;

  ctx.fillText(x_label, width / 2, this.layout.xAxisLabel_Y);

  ctx.save();
  ctx.rotate(Math.PI / 2);
  var start = height / 2 - ctx.measureText(y_label).width / 2;
  ctx.fillText(y_label, start, -width + 20);
  ctx.restore();
};

Graph.prototype.drawFrame = function() {
  var ctx = this.ctx;
  var { width, height } = ctx.canvas;

  // Draw frame to show size
  ctx.strokeStyle = "rgb(0,0,0)";
  ctx.fillStyle = "rgb(0,0,0)";
  ctx.beginPath();
  ctx.moveTo(0, 0);
  ctx.lineTo(width, 0);
  ctx.lineTo(width, height);
  ctx.lineTo(0, height);
  ctx.closePath();
  ctx.stroke();
};

function LatencyGraph(ctx) {
  Graph.call(this, ctx);
  console.log(this.ctx);
}

LatencyGraph.prototype = Object.create(Graph.prototype);

Object.defineProperty(LatencyGraph.prototype, "constructor", {
  enumerable: false,
  value: LatencyGraph,
});

LatencyGraph.prototype.ypos = function(delay) {
  var { height } = this.ctx.canvas;

  var r = height + 100 - Math.log(delay) * 64;
  if (r < 5) {
    return 5;
  }
  return r;
};

LatencyGraph.prototype.drawHBar = function(
  delay,
  label,
  color = "rgb(0,0,0)",
  label_offset = 0
) {
  var ctx = this.ctx;

  ctx.fillStyle = color;
  ctx.strokeStyle = color;
  ctx.fillText(
    label,
    this.xpos(numSamples) + 4 + label_offset,
    this.ypos(delay) + 3
  );

  ctx.beginPath();
  ctx.moveTo(this.xpos(0), this.ypos(delay));
  ctx.lineTo(this.xpos(numSamples) + label_offset, this.ypos(delay));
  ctx.stroke();
  ctx.strokeStyle = "rgb(0,0,0)";
  ctx.fillStyle = "rgb(0,0,0)";
};

LatencyGraph.prototype.draw = function() {
  let ctx = this.ctx;

  this.clear();
  this.drawFrame();

  for (const delay of [10, 20, 30, 50, 100, 200, 400, 800]) {
    this.drawScale(delay);
  }
  this.draw60fps();
  this.draw30fps();

  var worst = 0,
    worstpos = 0;
  ctx.beginPath();
  for (let i = 0; i < numSamples; i++) {
    ctx.lineTo(this.xpos(i), this.ypos(delays[i]));
    if (delays[i] >= worst) {
      worst = delays[i];
      worstpos = i;
    }
  }
  ctx.stroke();

  // Draw vertical lines marking minor and major GCs
  if (features.showingGCs) {
    ctx.strokeStyle = stroke.gcslice;
    let idx = sampleIndex % numSamples;
    const count = {
      major: majorGCs[idx],
      minor: 0,
      slice: slices[idx],
    };
    for (let i = 0; i < numSamples; i++) {
      idx = (sampleIndex + i) % numSamples;
      const isMajorStart = count.major < majorGCs[idx];
      if (count.slice < slices[idx]) {
        if (isMajorStart) {
          ctx.strokeStyle = stroke.initialMajor;
        }
        ctx.beginPath();
        ctx.moveTo(this.xpos(idx), 0);
        ctx.lineTo(this.xpos(idx), this.layout.xAxisLabel_Y);
        ctx.stroke();
        if (isMajorStart) {
          ctx.strokeStyle = stroke.gcslice;
        }
      }
      count.major = majorGCs[idx];
      count.slice = slices[idx];
    }

    ctx.strokeStyle = stroke.minor;
    idx = sampleIndex % numSamples;
    count.minor = gcs[idx];
    for (let i = 0; i < numSamples; i++) {
      idx = (sampleIndex + i) % numSamples;
      if (count.minor < minorGCs[idx]) {
        ctx.beginPath();
        ctx.moveTo(this.xpos(idx), 0);
        ctx.lineTo(this.xpos(idx), 20);
        ctx.stroke();
      }
      count.minor = minorGCs[idx];
    }
  }

  ctx.fillStyle = "rgb(255,0,0)";
  if (worst) {
    ctx.fillText(
      `${worst.toFixed(2)}ms`,
      this.xpos(worstpos) - 10,
      this.ypos(worst) - 14
    );
  }

  // Mark and label the slowest frame
  ctx.beginPath();
  var where = sampleIndex % numSamples;
  ctx.arc(this.xpos(where), this.ypos(delays[where]), 5, 0, Math.PI * 2, true);
  ctx.fill();
  ctx.fillStyle = "rgb(0,0,0)";

  this.drawAxisLabels("Time", "Pause between frames (log scale)");
};

function MemoryGraph(ctx) {
  Graph.call(this, ctx);
  this.worstEver = this.bestEver = performance.mozMemory.zone.gcBytes;
  this.limit = Math.max(
    this.worstEver,
    performance.mozMemory.zone.gcAllocTrigger
  );
}

MemoryGraph.prototype = Object.create(Graph.prototype);

Object.defineProperty(MemoryGraph.prototype, "constructor", {
  enumerable: false,
  value: MemoryGraph,
});

MemoryGraph.prototype.ypos = function(size) {
  var { height } = this.ctx.canvas;

  var range = this.limit - this.bestEver;
  var percent = (size - this.bestEver) / range;

  return (1 - percent) * height * 0.9 + 20;
};

MemoryGraph.prototype.drawHBar = function(
  size,
  label,
  color = "rgb(150,150,150)"
) {
  var ctx = this.ctx;

  var y = this.ypos(size);

  ctx.fillStyle = color;
  ctx.strokeStyle = color;
  ctx.fillText(label, this.xpos(numSamples) + 4, y + 3);

  ctx.beginPath();
  ctx.moveTo(this.xpos(0), y);
  ctx.lineTo(this.xpos(numSamples), y);
  ctx.stroke();
  ctx.strokeStyle = "rgb(0,0,0)";
  ctx.fillStyle = "rgb(0,0,0)";
};

MemoryGraph.prototype.draw = function() {
  var ctx = this.ctx;

  this.clear();
  this.drawFrame();

  var worst = 0,
    worstpos = 0;
  for (let i = 0; i < numSamples; i++) {
    if (gcBytes[i] >= worst) {
      worst = gcBytes[i];
      worstpos = i;
    }
    if (gcBytes[i] < this.bestEver) {
      this.bestEver = gcBytes[i];
    }
  }

  if (this.worstEver < worst) {
    this.worstEver = worst;
    this.limit = Math.max(
      this.worstEver,
      performance.mozMemory.zone.gcAllocTrigger
    );
  }

  this.drawHBar(
    this.bestEver,
    `${format_gcBytes(this.bestEver)} min`,
    "#00cf61"
  );
  this.drawHBar(
    this.worstEver,
    `${format_gcBytes(this.worstEver)} max`,
    "#cc1111"
  );
  this.drawHBar(
    performance.mozMemory.zone.gcAllocTrigger,
    `${format_gcBytes(performance.mozMemory.zone.gcAllocTrigger)} trigger`,
    "#cc11cc"
  );

  ctx.fillStyle = "rgb(255,0,0)";
  if (worst) {
    ctx.fillText(
      format_gcBytes(worst),
      this.xpos(worstpos) - 10,
      this.ypos(worst) - 14
    );
  }

  ctx.beginPath();
  var where = sampleIndex % numSamples;
  ctx.arc(this.xpos(where), this.ypos(gcBytes[where]), 5, 0, Math.PI * 2, true);
  ctx.fill();

  ctx.beginPath();
  for (var i = 0; i < numSamples; i++) {
    if (i == (sampleIndex + 1) % numSamples) {
      ctx.moveTo(this.xpos(i), this.ypos(gcBytes[i]));
    } else {
      ctx.lineTo(this.xpos(i), this.ypos(gcBytes[i]));
    }
    if (i == where) {
      ctx.stroke();
    }
  }
  ctx.stroke();

  this.drawAxisLabels("Time", "Heap Memory Usage");
};

function onUpdateDisplayChanged() {
  const do_graph = document.getElementById("do-graph");
  if (do_graph.checked) {
    window.requestAnimationFrame(handler);
    gFrameTimer.resume();
  } else {
    gFrameTimer.stop();
  }
  update_load_state_indicator();
}

function onDoLoadChange() {
  const do_load = document.getElementById("do-load");
  gLoadMgr.paused = !do_load.checked;
  console.log(`load paused: ${gLoadMgr.paused}`);
  update_load_state_indicator();
}

var previous = 0;
function handler(timestamp) {
  if (gFrameTimer.is_stopped()) {
    return;
  }

  const events = gLoadMgr.tick(timestamp);
  if (events & gLoadMgr.LOAD_ENDED) {
    end_test(timestamp, gLoadMgr.lastActive);
    if (!gLoadMgr.cycleStopped()) {
      start_test();
    }
  }

  if (testState == "running") {
    document.getElementById("test-progress").textContent =
      (gLoadMgr.cycleCurrentLoadRemaining(timestamp) / 1000).toFixed(1) +
      " sec";
  }

  const delay = gFrameTimer.on_frame_finished(timestamp);

  update_histogram(gHistogram, delay);

  // Total time elapsed while the active test has been running.
  var t = timestamp - gFrameTimer.start;
  var newIndex = Math.round(t / sampleTime);
  while (sampleIndex < newIndex) {
    sampleIndex++;
    var idx = sampleIndex % numSamples;
    delays[idx] = delay;
    if (features.trackingSizes) {
      gcBytes[idx] = performance.mozMemory.gcBytes;
    }
    if (features.showingGCs) {
      gcs[idx] = performance.mozMemory.gcNumber;
      minorGCs[idx] = performance.mozMemory.minorGCCount;
      majorGCs[idx] = performance.mozMemory.majorGCCount;

      // Previous versions lacking sliceCount will fall back to assuming
      // any GC activity was a major GC slice, even though that
      // incorrectly includes minor GCs. Although this file is versioned
      // with the code that implements the new sliceCount field, it is
      // common to load the gc-ubench index.html with different browser
      // versions.
      slices[idx] =
        performance.mozMemory.sliceCount || performance.mozMemory.gcNumber;
    }
  }

  latencyGraph.draw();
  if (memoryGraph) {
    memoryGraph.draw();
  }
  window.requestAnimationFrame(handler);
}

// For interactive debugging.
//
// ['a', 'b', 'b', 'b', 'c', 'c'] => ['a', 'b x 3', 'c x 2']
function summarize(arr) {
  if (!arr.length) {
    return [];
  }

  var result = [];
  var run_start = 0;
  var prev = arr[0];
  for (var i = 1; i <= arr.length; i++) {
    if (i == arr.length || arr[i] != prev) {
      if (i == run_start + 1) {
        result.push(arr[i]);
      } else {
        result.push(prev + " x " + (i - run_start));
      }
      run_start = i;
    }
    if (i != arr.length) {
      prev = arr[i];
    }
  }

  return result;
}

function reset_draw_state() {
  for (var i = 0; i < numSamples; i++) {
    delays[i] = 0;
  }
  sampleIndex = 0;
}

function onunload() {
  gLoadMgr.deactivateLoad();
}

function onload() {
  // The order of `tests` is currently based on their asynchronous load
  // order, rather than the listed order. Rearrange by extracting the test
  // names from their filenames, which is kind of gross.
  _tests = tests;
  tests = new Map();
  foreach_test_file(fn => {
    // "benchmarks/foo.js" => "foo"
    const name = fn.split(/\//)[1].split(/\./)[0];
    tests.set(name, _tests.get(name));
  });
  _tests = undefined;

  gLoadMgr = new AllocationLoadManager(tests);

  // Load initial test duration.
  duration_changed();

  // Load initial garbage size.
  garbage_piles_changed();
  garbage_per_frame_changed();

  // Populate the test selection dropdown.
  var select = document.getElementById("test-selection");
  for (var [name, test] of tests) {
    test.name = name;
    var option = document.createElement("option");
    option.id = name;
    option.text = name;
    option.title = test.description;
    select.add(option);
  }

  // Load the initial test.
  gLoadMgr.setActiveLoadByName("noAllocation");
  load_changed();
  document.getElementById("test-selection").value = "noAllocation";

  // Polyfill rAF.
  var requestAnimationFrame =
    window.requestAnimationFrame ||
    window.mozRequestAnimationFrame ||
    window.webkitRequestAnimationFrame ||
    window.msRequestAnimationFrame;
  window.requestAnimationFrame = requestAnimationFrame;

  // Acquire our canvas.
  var canvas = document.getElementById("graph");
  latencyGraph = new LatencyGraph(canvas.getContext("2d"));

  if (!performance.mozMemory) {
    document.getElementById("memgraph-disabled").style.display = "block";
    document.getElementById("track-sizes-div").style.display = "none";
  }

  trackHeapSizes(document.getElementById("track-sizes").checked);

  update_load_state_indicator();
  gFrameTimer.start_recording();

  // Start drawing.
  reset_draw_state();
  window.requestAnimationFrame(handler);
}

function run_one_test() {
  start_test_cycle([gLoadMgr.activeLoad().name]);
}

function run_all_tests() {
  start_test_cycle(tests.keys());
}

function start_test_cycle(tests_to_run) {
  // Convert from an iterable to an array for pop.
  gLoadMgr.startCycle(tests_to_run);
  testState = "running";
  gHistogram.clear();
  reset_draw_state();
}

function update_load_state_indicator() {
  if (!gLoadMgr.load_running()) {
    loadState = "(none)";
  } else if (gFrameTimer.is_stopped() || gLoadMgr.paused) {
    loadState = "(inactive)";
  } else {
    loadState = "(active)";
  }
  document.getElementById("load-running").textContent = loadState;
}

function start_test() {
  console.log(`Running test: ${gLoadMgr.activeLoad().name}`);
  document.getElementById("test-selection").value = gLoadMgr.activeLoad().name;
  update_load_state_indicator();
}

function end_test(timestamp, load) {
  document.getElementById("test-progress").textContent = "(not running)";
  report_test_result(load, gHistogram);
  gHistogram.clear();
  console.log(`Ending test ${load.name}`);
  if (gLoadMgr.cycleStopped()) {
    testState = "idle";
  }
  update_load_state_indicator();
  reset_draw_state();
}

function compute_test_spark_histogram(histogram) {
  const percents = compute_spark_histogram_percents(histogram);

  var sparks = "▁▂▃▄▅▆▇█";
  var colors = [
    "#aaaa00",
    "#007700",
    "#dd0000",
    "#ff0000",
    "#ff0000",
    "#ff0000",
    "#ff0000",
    "#ff0000",
  ];
  var line = "";
  for (let i = 0; i < percents.length; ++i) {
    var spark = sparks.charAt(parseInt(percents[i] * sparks.length));
    line += `<span style="color:${colors[i]}">${spark}</span>`;
  }
  return line;
}

function format_units(n) {
  n = String(n);
  if (n.length > 9 && n.substr(-9) == "000000000") {
    return n.substr(0, n.length - 9) + "G";
  } else if (n.length > 9 && n.substr(-6) == "000000") {
    return n.substr(0, n.length - 6) + "M";
  } else if (n.length > 3 && n.substr(-3) == "000") {
    return n.substr(0, n.length - 3) + "K";
  }
  return String(n);
}

function report_test_result(load, histogram) {
  var resultList = document.getElementById("results-display");
  var resultElem = document.createElement("div");
  var score = compute_test_score(histogram);
  var sparks = compute_test_spark_histogram(histogram);
  var params = `(${format_units(load.garbagePerFrame)},${format_units(
    load.garbagePiles
  )})`;
  resultElem.innerHTML = `${score.toFixed(3)} ms/s : ${sparks} : ${
    load.name
  }${params} - ${load.description}`;
  resultList.appendChild(resultElem);
}

function load_changed() {
  document.getElementById("garbage-per-frame").value = format_units(
    gLoadMgr.activeLoad().garbagePerFrame
  );
  document.getElementById("garbage-piles").value = format_units(
    gLoadMgr.activeLoad().garbagePiles
  );
  update_load_state_indicator();
}

function duration_changed() {
  var durationInput = document.getElementById("test-duration");
  gLoadMgr.testDurationMS = parseInt(durationInput.value) * 1000;
  console.log(
    `Updated test duration to: ${gLoadMgr.testDurationMS / 1000} seconds`
  );
}

function onLoadChange() {
  var select = document.getElementById("test-selection");
  console.log(`Switching to test: ${select.value}`);
  gLoadMgr.setActiveLoadByName(select.value);
  load_changed();
  gHistogram.clear();
  reset_draw_state();
}

function garbage_piles_changed() {
  var value = parse_units(document.getElementById("garbage-piles").value);
  if (isNaN(value)) {
    return;
  }

  if (gLoadMgr.load_running()) {
    gLoadMgr.change_garbagePiles(value);
    console.log(
      `Updated garbage-piles to ${gLoadMgr.activeLoad().garbagePiles} items`
    );
  }
  gHistogram.clear();
  reset_draw_state();
}

function garbage_per_frame_changed() {
  var value = parse_units(document.getElementById("garbage-per-frame").value);
  if (isNaN(value)) {
    return;
  }
  if (gLoadMgr.load_running()) {
    gLoadMgr.change_garbagePerFrame = value;
    console.log(
      `Updated garbage-per-frame to ${
        gLoadMgr.activeLoad().garbagePerFrame
      } items`
    );
  }
}

function trackHeapSizes(track) {
  features.trackingSizes = track;

  var canvas = document.getElementById("memgraph");

  if (features.trackingSizes) {
    canvas.style.display = "block";
    memoryGraph = new MemoryGraph(canvas.getContext("2d"));
  } else {
    canvas.style.display = "none";
    memoryGraph = null;
  }
}
