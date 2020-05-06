/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Per-frame time sampling infra. Also GC'd: hopefully will not perturb things too badly.
var numSamples = 500;
var delays = new Array(numSamples);
var gcs = new Array(numSamples);
var minorGCs = new Array(numSamples);
var majorGCs = new Array(numSamples);
var slices = new Array(numSamples);
var gcBytes = new Array(numSamples);
var mallocBytes = new Array(numSamples);
var sampleIndex = 0;
var sampleTime = 16; // ms
var gHistogram = new Map(); // {ms: count}

var stroke = {
  gcslice: "rgb(255,100,0)",
  minor: "rgb(0,255,100)",
  initialMajor: "rgb(180,60,255)",
};

var features = {
  trackingSizes: "mozMemory" in performance,
  showingGCs: "mozMemory" in performance,
};

var load_paused = false;

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

  start_recording() {
    this.start = this.prev = performance.now();
  }

  record_frame_callback(timestamp) {
    const delay = timestamp - this.prev;
    this.prev = timestamp;
    return delay;
  }

  stop() {
    const now = performance.now();
    this.stopped = now;
  }

  resume() {
    const now = performance.now();
    this.prev += now - this.stopped;
    const stop_duration = now - this.stopped;
    this.start += stop_duration;
    this.stopped = 0;
  }
}

var gFrameTimer = new FrameTimer();

var latencyGraph;
var memoryGraph;
var ctx;
var memoryCtx;

// Current test state.
var activeTest = undefined;
var testDuration = undefined; // ms
var loadState = "(init)"; // One of '(active)', '(inactive)', '(N/A)'
var testState = "idle"; // One of 'idle' or 'running'.
var testStart = undefined; // ms
var testQueue = [];

// Global defaults
var globalDefaultGarbageTotal = "8M";
var globalDefaultGarbagePerFrame = "8K";

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
  var ctx = this.ctx;

  this.clear();
  this.drawFrame();

  for (var delay of [10, 20, 30, 50, 100, 200, 400, 800]) {
    this.drawScale(delay);
  }
  this.draw60fps();
  this.draw30fps();

  var worst = 0,
    worstpos = 0;
  ctx.beginPath();
  for (var i = 0; i < numSamples; i++) {
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
    var idx = sampleIndex % numSamples;
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

function format_gcBytes(bytes) {
  if (bytes < 4000) {
    return `${bytes} bytes`;
  } else if (bytes < 4e6) {
    return `${(bytes / 1024).toFixed(2)} KB`;
  } else if (bytes < 4e9) {
    return `${(bytes / 1024 / 1024).toFixed(2)} MB`;
  }
  return `${(bytes / 1024 / 1024 / 1024).toFixed(2)} GB`;
}

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
  for (let i = 0; i < numSamples; i++) {
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

function stopstart() {
  const do_graph = document.getElementById("do-graph");
  if (do_graph.checked) {
    window.requestAnimationFrame(handler);
    gFrameTimer.resume();
  } else {
    gFrameTimer.stop();
  }
  update_load_state();
}

function do_load() {
  const do_load = document.getElementById("do-load");
  load_paused = !do_load.checked;
  console.log(`load paused: ${load_paused}`);
  update_load_state();
}

var previous = 0;
function handler(timestamp) {
  if (gFrameTimer.is_stopped()) {
    return;
  }

  if (testState === "running" && timestamp - testStart > testDuration) {
    end_test(timestamp);
  }

  if (testState == "running") {
    document.getElementById("test-progress").textContent =
      ((testDuration - (timestamp - testStart)) / 1000).toFixed(1) + " sec";
  }

  if (!load_paused) {
    activeTest.makeGarbage(activeTest.garbagePerFrame);
  }

  const delay = gFrameTimer.record_frame_callback(timestamp);

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

function update_histogram(histogram, delay) {
  // Round to a whole number of 10us intervals to provide enough resolution to
  // capture a 16ms target with adequate accuracy.
  delay = Math.round(delay * 100) / 100;
  var current = histogram.has(delay) ? histogram.get(delay) : 0;
  histogram.set(delay, ++current);
}

function reset_draw_state() {
  for (var i = 0; i < numSamples; i++) {
    delays[i] = 0;
  }
  gFrameTimer.start_recording();
  sampleIndex = 0;
}

function onunload() {
  if (activeTest) {
    activeTest.unload();
  }
  activeTest = undefined;
}

function onload() {
  // Load initial test duration.
  duration_changed();

  // Load initial garbage size.
  garbage_total_changed();
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
  change_load("noAllocation");

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

  update_load_state();

  // Start drawing.
  reset_draw_state();
  window.requestAnimationFrame(handler);
}

function run_one_test() {
  start_test_cycle([activeTest.name]);
}

function run_all_tests() {
  start_test_cycle(tests.keys());
}

function start_test_cycle(tests_to_run) {
  // Convert from an iterable to an array for pop.
  testQueue = [];
  for (var key of tests_to_run) {
    testQueue.push(key);
  }
  testState = "running";
  testStart = performance.now();
  gHistogram.clear();

  start_test(testQueue.shift());
  reset_draw_state();
}

function update_load_state() {
  if (activeTest.name == "noAllocation") {
    loadState = "(none)";
  } else if (gFrameTimer.is_stopped() || load_paused) {
    loadState = "(inactive)";
  } else {
    loadState = "(active)";
  }
  document.getElementById("load-running").textContent = loadState;
}

function start_test(testName) {
  change_load(testName);
  console.log(`Running test: ${testName}`);
  document.getElementById("test-selection").value = testName;
  update_load_state();
}

function end_test(timestamp) {
  document.getElementById("test-progress").textContent = "(not running)";
  report_test_result(activeTest, gHistogram);
  gHistogram.clear();
  console.log(`Ending test ${activeTest.name}`);
  if (testQueue.length) {
    start_test(testQueue.shift());
    testStart = timestamp;
  } else {
    testState = "idle";
    testStart = 0;
  }
  reset_draw_state();
}

function report_test_result(test, histogram) {
  var resultList = document.getElementById("results-display");
  var resultElem = document.createElement("div");
  var score = compute_test_score(histogram);
  var sparks = compute_test_spark_histogram(histogram);
  var params = `(${format_units(test.garbagePerFrame)},${format_units(
    test.garbageTotal
  )})`;
  resultElem.innerHTML = `${score.toFixed(3)} ms/s : ${sparks} : ${
    test.name
  }${params} - ${test.description}`;
  resultList.appendChild(resultElem);
}

// Compute a score based on the total ms we missed frames by per second.
function compute_test_score(histogram) {
  var score = 0;
  for (let [delay, count] of histogram) {
    score += Math.abs((delay - 1000/60) * count);
  }
  score = score / (testDuration / 1000);
  return Math.round(score * 1000) / 1000;
}

// Build a spark-lines histogram for the test results to show with the aggregate score.
function compute_test_spark_histogram(histogram) {
  var ranges = [
    [-99999999, 16.6],
    [16.6, 16.8],
    [16.8, 25],
    [25, 33.4],
    [33.4, 60],
    [60, 100],
    [100, 300],
    [300, 99999999],
  ];
  var rescaled = new Map();
  for (let [delay,] of histogram) {
    for (var i = 0; i < ranges.length; ++i) {
      var low = ranges[i][0];
      var high = ranges[i][1];
      if (low <= delay && delay < high) {
        update_histogram(rescaled, i);
        break;
      }
    }
  }
  var total = 0;
  for (const [, count] of rescaled) {
    total += count;
  }
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
  for (let i = 0; i < ranges.length; ++i) {
    const amt = rescaled.has(i) ? rescaled.get(i) : 0;
    var spark = sparks.charAt(parseInt((amt / total) * 8));
    line += `<span style="color:${colors[i]}">${spark}</span>`;
  }
  return line;
}

function reload_active_test() {
  activeTest.unload();
  activeTest.load(activeTest.garbageTotal);
}

function change_load(new_test_name) {
  if (activeTest) {
    activeTest.unload();
  }
  activeTest = tests.get(new_test_name);

  if (!activeTest.garbagePerFrame) {
    activeTest.garbagePerFrame = parse_units(
      activeTest.defaultGarbagePerFrame || globalDefaultGarbagePerFrame
    );
  }
  if (!activeTest.garbageTotal) {
    activeTest.garbageTotal = parse_units(
      activeTest.defaultGarbageTotal || globalDefaultGarbageTotal
    );
  }

  document.getElementById("garbage-per-frame").value = format_units(
    activeTest.garbagePerFrame
  );
  document.getElementById("garbage-total").value = format_units(
    activeTest.garbageTotal
  );

  activeTest.load(activeTest.garbageTotal);
  update_load_state();
}

function duration_changed() {
  var durationInput = document.getElementById("test-duration");
  testDuration = parseInt(durationInput.value) * 1000;
  console.log(`Updated test duration to: ${testDuration / 1000} seconds`);
}

function test_changed() {
  var select = document.getElementById("test-selection");
  console.log(`Switching to test: ${select.value}`);
  change_load(select.value);
  gHistogram.clear();
  reset_draw_state();
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

function garbage_total_changed() {
  var value = parse_units(document.getElementById("garbage-total").value);
  if (isNaN(value)) {
    return;
  }
  if (activeTest) {
    activeTest.garbageTotal = value;
    console.log(`Updated garbage-total to ${activeTest.garbageTotal} items`);
    reload_active_test();
  }
  gHistogram.clear();
  reset_draw_state();
}

function garbage_per_frame_changed() {
  var value = parse_units(document.getElementById("garbage-per-frame").value);
  if (isNaN(value)) {
    return;
  }
  if (activeTest) {
    activeTest.garbagePerFrame = value;
    console.log(
      `Updated garbage-per-frame to ${activeTest.garbagePerFrame} items`
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
