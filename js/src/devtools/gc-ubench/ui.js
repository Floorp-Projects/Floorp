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

var numSamples = 500;

var gHistogram = new Map(); // {ms: count}
var gPerf = new FrameHistory(numSamples);

var latencyGraph;
var memoryGraph;
var ctx;
var memoryCtx;

var loadState = "(init)"; // One of '(active)', '(inactive)', '(N/A)'
var testState = "idle"; // One of 'idle' or 'running'.

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
    ctx.lineTo(this.xpos(i), this.ypos(gPerf.delays[i]));
    if (gPerf.delays[i] >= worst) {
      worst = gPerf.delays[i];
      worstpos = i;
    }
  }
  ctx.stroke();

  // Draw vertical lines marking minor and major GCs
  if (features.showingGCs) {
    ctx.strokeStyle = stroke.gcslice;
    let idx = sampleIndex % numSamples;
    const count = {
      major: gPerf.majorGCs[idx],
      minor: 0,
      slice: gPerf.slices[idx],
    };
    for (let i = 0; i < numSamples; i++) {
      idx = (sampleIndex + i) % numSamples;
      const isMajorStart = count.major < gPerf.majorGCs[idx];
      if (count.slice < gPerf.slices[idx]) {
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
      count.major = gPerf.majorGCs[idx];
      count.slice = gPerf.slices[idx];
    }

    ctx.strokeStyle = stroke.minor;
    idx = sampleIndex % numSamples;
    count.minor = gPerf.gcs[idx];
    for (let i = 0; i < numSamples; i++) {
      idx = (sampleIndex + i) % numSamples;
      if (count.minor < gPerf.minorGCs[idx]) {
        ctx.beginPath();
        ctx.moveTo(this.xpos(idx), 0);
        ctx.lineTo(this.xpos(idx), 20);
        ctx.stroke();
      }
      count.minor = gPerf.minorGCs[idx];
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
  ctx.arc(
    this.xpos(where),
    this.ypos(gPerf.delays[where]),
    5,
    0,
    Math.PI * 2,
    true
  );
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
    if (gPerf.gcBytes[i] >= worst) {
      worst = gPerf.gcBytes[i];
      worstpos = i;
    }
    if (gPerf.gcBytes[i] < this.bestEver) {
      this.bestEver = gPerf.gcBytes[i];
    }
  }

  if (this.worstEver < worst) {
    this.worstEver = worst;
    this.limit = Math.max(
      this.worstEver,
      performance.mozMemory.zone.gcAllocTrigger
    );
  }

  this.drawHBar(this.bestEver, `${format_bytes(this.bestEver)} min`, "#00cf61");
  this.drawHBar(
    this.worstEver,
    `${format_bytes(this.worstEver)} max`,
    "#cc1111"
  );
  this.drawHBar(
    performance.mozMemory.zone.gcAllocTrigger,
    `${format_bytes(performance.mozMemory.zone.gcAllocTrigger)} trigger`,
    "#cc11cc"
  );

  ctx.fillStyle = "rgb(255,0,0)";
  if (worst) {
    ctx.fillText(
      format_bytes(worst),
      this.xpos(worstpos) - 10,
      this.ypos(worst) - 14
    );
  }

  ctx.beginPath();
  var where = sampleIndex % numSamples;
  ctx.arc(
    this.xpos(where),
    this.ypos(gPerf.gcBytes[where]),
    5,
    0,
    Math.PI * 2,
    true
  );
  ctx.fill();

  ctx.beginPath();
  for (var i = 0; i < numSamples; i++) {
    if (i == (sampleIndex + 1) % numSamples) {
      ctx.moveTo(this.xpos(i), this.ypos(gPerf.gcBytes[i]));
    } else {
      ctx.lineTo(this.xpos(i), this.ypos(gPerf.gcBytes[i]));
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
    gPerf.resume();
  } else {
    gPerf.pause();
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
  if (gPerf.is_stopped()) {
    return;
  }

  const events = gLoadMgr.tick(timestamp);
  if (events & gLoadMgr.LOAD_ENDED) {
    end_test(timestamp, gLoadMgr.lastActive);
    if (!gLoadMgr.cycleStopped()) {
      start_test();
    }
    update_load_display();
  }

  if (testState == "running") {
    document.getElementById("test-progress").textContent =
      (gLoadMgr.cycleCurrentLoadRemaining(timestamp) / 1000).toFixed(1) +
      " sec";
  }

  const delay = gPerf.on_frame(timestamp);

  update_histogram(gHistogram, delay);

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
  gPerf.reset();
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
  update_load_display();
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
  gPerf.start();

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
  } else if (gPerf.is_stopped() || gLoadMgr.paused) {
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

function report_test_result(load, histogram) {
  var resultList = document.getElementById("results-display");
  var resultElem = document.createElement("div");
  var score = compute_test_score(histogram);
  var sparks = compute_test_spark_histogram(histogram);
  var params = `(${format_num(load.garbagePerFrame)},${format_num(
    load.garbagePiles
  )})`;
  resultElem.innerHTML = `${score.toFixed(3)} ms/s : ${sparks} : ${
    load.name
  }${params} - ${load.description}`;
  resultList.appendChild(resultElem);
}

function update_load_display() {
  const garbage = gLoadMgr.activeLoad()
    ? gLoadMgr.activeLoad().garbagePerFrame
    : parse_units(gDefaultGarbagePerFrame);
  document.getElementById("garbage-per-frame").value = format_num(garbage);
  const piles = gLoadMgr.activeLoad()
    ? gLoadMgr.activeLoad().garbagePiles
    : parse_units(gDefaultGarbagePiles);
  document.getElementById("garbage-piles").value = format_num(piles);
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
  update_load_display();
  gHistogram.clear();
  reset_draw_state();
}

function garbage_piles_changed() {
  const input = document.getElementById("garbage-piles");
  const value = parse_units(input.value);
  if (isNaN(value)) {
    update_load_display();
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
  const input = document.getElementById("garbage-per-frame");
  var value = parse_units(input.value);
  if (isNaN(value)) {
    update_load_display();
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
