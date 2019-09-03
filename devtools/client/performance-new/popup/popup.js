/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
const background = ChromeUtils.import(
  "resource://devtools/client/performance-new/popup/background.jsm"
);

const intervalScale = makeExponentialScale(0.01, 100);
const buffersizeScale = makeExponentialScale(10000, 100000000);
// Window Length accepts a numerical value between 1-N. We also need to put an
// infinite number at the end of the window length slider. Therefore, the max
// value pretends like it's infinite in the slider.
// The maximum value of window length is 300 seconds. For that reason, we are
// treating 400 as infinity.
const infiniteWindowLength = 400;
const windowLengthScale = makeExponentialScale(1, infiniteWindowLength);

const PROFILE_ENTRY_SIZE = 9; // sizeof(double) + sizeof(char), http://searchfox.org/mozilla-central/rev/e8835f52eff29772a57dca7bcc86a9a312a23729/tools/profiler/core/ProfileEntry.h#73

const featurePrefix = "perf-settings-feature-checkbox-";
const features = [
  "java",
  "js",
  "leaf",
  "mainthreadio",
  "privacy",
  "responsiveness",
  "screenshots",
  "seqstyle",
  "stackwalk",
  "tasktracer",
  "jstracer",
  "jsallocations",
  "preferencereads",
  "trackopts",
];
const threadPrefix = "perf-settings-thread-checkbox-";
// A map of element ID suffixes to their corresponding profile thread name, for
// creating ID -> name mappings, e.g.`$threadPrefix}dom-worker` - DOM Worker.
const threadMap = {
  compositor: "Compositor",
  "dns-resolver": "DNS Resolver",
  "dom-worker": "DOM Worker",
  "gecko-main": "GeckoMain",
  "img-decoder": "ImgDecoder",
  "paint-worker": "PaintWorker",
  "render-backend": "RenderBackend",
  renderer: "Renderer",
  "socket-thread": "Socket Thread",
  "stream-trans": "StreamTrans",
  "style-thread": "StyleThread",
};

function initializePopup() {
  const updateIsRunning = () => {
    renderState(background.state);
  };
  background.isRunningObserver.addObserver(updateIsRunning);

  window.addEventListener("unload", () => {
    background.isRunningObserver.removeObserver(updateIsRunning);
  });

  for (const name of features) {
    setupFeatureCheckbox(name);
  }

  for (const name in threadMap) {
    setupThreadCheckbox(name);
  }

  document
    .querySelector("#perf-settings-thread-text")
    .addEventListener("change", async e => {
      background.adjustState({ threads: e.target.value });
      renderState(background.state);
    });

  document
    .querySelector(".settings-apply-button")
    .addEventListener("click", async () => {
      background.restartProfiler();
    });

  document
    .querySelector(".button-start")
    .addEventListener("click", async () => {
      background.startProfiler();
      background.adjustState({ isRunning: true });
      renderState(background.state);
    });

  document
    .querySelector(".button-cancel")
    .addEventListener("click", async () => {
      background.stopProfiler();
      background.adjustState({ isRunning: false });
      renderState(background.state);
    });

  document
    .querySelector("#button-capture")
    .addEventListener("click", async () => {
      if (document.documentElement.classList.contains("status-running")) {
        await background.captureProfile();
        window.gClosePopup();
      }
    });

  document
    .querySelector("#settings-label")
    .addEventListener("click", async () => {
      const { settingsOpen } = background.state;
      background.adjustState({
        settingsOpen: !settingsOpen,
      });
      renderState(background.state);
    });

  document
    .querySelector(".interval-range")
    .addEventListener("input", async e => {
      const frac = e.target.value / 100;
      background.adjustState({
        interval: intervalScale.fromFractionToSingleDigitValue(frac),
      });
      renderState(background.state);
    });

  document
    .querySelector(".buffersize-range")
    .addEventListener("input", async e => {
      const frac = e.target.value / 100;
      background.adjustState({
        buffersize: buffersizeScale.fromFractionToSingleDigitValue(frac),
      });
      renderState(background.state);
    });

  document
    .querySelector(".windowlength-range")
    .addEventListener("input", async e => {
      const frac = e.target.value / 100;
      background.adjustState({
        windowLength: windowLengthScale.fromFractionToSingleDigitValue(frac),
      });
      renderState(background.state);
    });

  window.onload = () => {
    // Letting the background script know how the infiniteWindowLength is represented.
    background.adjustState({
      infiniteWindowLength,
    });
  };

  renderState(background.state);
}

function renderState(state) {
  const { isRunning, settingsOpen, interval, buffersize, windowLength } = state;
  document.documentElement.classList.toggle("status-running", isRunning);
  document.documentElement.classList.toggle("status-stopped", !isRunning);
  document.querySelector(".settings").classList.toggle("open", settingsOpen);
  document.querySelector(".interval-value").textContent = `${interval} ms`;
  document.querySelector(".buffersize-value").textContent = prettyBytes(
    buffersize * PROFILE_ENTRY_SIZE
  );
  document.querySelector(".windowlength-value").textContent =
    windowLength === infiniteWindowLength ? `∞` : `${windowLength} sec`;
  const overhead = calculateOverhead(state);
  const overheadDiscreteContainer = document.querySelector(".discrete-level");
  for (let i = 0; i < overheadDiscreteContainer.children.length; i++) {
    const discreteLevelNotch = overheadDiscreteContainer.children[i];
    const isActive =
      i <=
      Math.round(overhead * (overheadDiscreteContainer.children.length - 1));
    discreteLevelNotch.classList.toggle("active", isActive);
    discreteLevelNotch.classList.toggle("inactive", !isActive);
  }

  renderControls(state);

  window.requestAnimationFrame(() => {
    if (window.gResizePopup) {
      window.gResizePopup(document.body.clientHeight);
    }
  });
}

function renderControls(state) {
  document.querySelector(".interval-range").value =
    intervalScale.fromValueToFraction(state.interval) * 100;
  document.querySelector(".buffersize-range").value =
    buffersizeScale.fromValueToFraction(state.buffersize) * 100;
  document.querySelector(".windowlength-range").value =
    windowLengthScale.fromValueToFraction(state.windowLength) * 100;

  for (const name of features) {
    document.getElementById(featurePrefix + name).value = state[name];
  }

  for (const name in threadMap) {
    document.getElementById(
      threadPrefix + name
    ).checked = state.threads.includes(threadMap[name]);
  }

  document.querySelector("#perf-settings-thread-text").value = state.threads;
}

function clamp(val, min, max) {
  return Math.max(min, Math.min(max, val));
}

function lerp(frac, rangeStart, rangeEnd) {
  return (1 - frac) * rangeStart + frac * rangeEnd;
}

function scaleRangeWithClamping(
  val,
  sourceRangeStart,
  sourceRangeEnd,
  destRangeStart,
  destRangeEnd
) {
  const frac = clamp(
    (val - sourceRangeStart) / (sourceRangeEnd - sourceRangeStart),
    0,
    1
  );
  return lerp(frac, destRangeStart, destRangeEnd);
}

function calculateOverhead(state) {
  const overheadFromSampling =
    scaleRangeWithClamping(
      Math.log(state.interval),
      Math.log(0.05),
      Math.log(1),
      1,
      0
    ) +
    scaleRangeWithClamping(
      Math.log(state.interval),
      Math.log(1),
      Math.log(100),
      0.1,
      0
    );
  const overheadFromBuffersize = scaleRangeWithClamping(
    Math.log(state.buffersize),
    Math.log(10),
    Math.log(1000000),
    0,
    0.1
  );
  const overheadFromStackwalk = state.stackwalk ? 0.05 : 0;
  const overheadFromResponsiveness = state.responsiveness ? 0.05 : 0;
  const overheadFromJavaScrpt = state.js ? 0.05 : 0;
  const overheadFromSeqStyle = state.seqstyle ? 0.05 : 0;
  const overheadFromTaskTracer = state.tasktracer ? 0.05 : 0;
  const overheadFromJSTracer = state.jstracer ? 0.05 : 0;
  const overheadFromJSAllocations = state.jsallocations ? 0.05 : 0;
  return clamp(
    overheadFromSampling +
      overheadFromBuffersize +
      overheadFromStackwalk +
      overheadFromResponsiveness +
      overheadFromJavaScrpt +
      overheadFromSeqStyle +
      overheadFromTaskTracer +
      overheadFromJSTracer +
      overheadFromJSAllocations,
    0,
    1
  );
}

/**
 * This helper initializes and adds listeners to the features checkboxes that
 * will adjust the profiler state when changed.
 */
async function setupFeatureCheckbox(name) {
  // Java profiling is only meaningful on android.
  if (name == "java") {
    if (background.platform !== "android") {
      document.querySelector("#java").style.display = "none";
      return;
    }
  }

  const checkbox = document.querySelector(`#${featurePrefix}${name}`);
  checkbox.checked = background.state.features[name];

  checkbox.addEventListener("change", async e => {
    const newFeatures = Object.assign({}, background.state.features);
    newFeatures[name] = e.target.checked;
    background.adjustState({ features: newFeatures });
    renderState(background.state);
  });
}

/**
 * This helper initializes and adds listeners to the threads checkboxes that
 * will adjust the profiler state when changed.
 */
async function setupThreadCheckbox(name) {
  const checkbox = document.querySelector(`#${threadPrefix}${name}`);
  checkbox.checked = background.state.threads.includes(threadMap[name]);

  checkbox.addEventListener("change", async e => {
    let threads = background.state.threads;
    if (e.target.checked) {
      threads += "," + e.target.value;
    } else {
      threads = threadTextToList(threads)
        .filter(thread => thread !== e.target.value)
        .join(",");
    }
    background.adjustState({ threads });
    renderState(background.state);
  });
}

/**
 * Clean up the thread list string into a list of values.
 * @param string threads, comma separated values.
 * @return Array list of thread names
 */
function threadTextToList(threads) {
  return (
    threads
      // Split on commas
      .split(",")
      // Clean up any extraneous whitespace
      .map(string => string.trim())
      // Filter out any blank strings
      .filter(string => string)
  );
}

function makeExponentialScale(rangeStart, rangeEnd) {
  const startExp = Math.log(rangeStart);
  const endExp = Math.log(rangeEnd);
  const fromFractionToValue = frac =>
    Math.exp((1 - frac) * startExp + frac * endExp);
  const fromValueToFraction = value =>
    (Math.log(value) - startExp) / (endExp - startExp);
  const fromFractionToSingleDigitValue = frac => {
    return +fromFractionToValue(frac).toPrecision(1);
  };
  return {
    fromFractionToValue,
    fromValueToFraction,
    fromFractionToSingleDigitValue,
  };
}

const UNITS = ["B", "kB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB"];

function prettyBytes(num) {
  if (!Number.isFinite(num)) {
    throw new TypeError(`Expected a finite number, got ${typeof num}: ${num}`);
  }

  const neg = num < 0;

  if (neg) {
    num = -num;
  }

  if (num < 1) {
    return (neg ? "-" : "") + num + " B";
  }

  const exponent = Math.min(
    Math.floor(Math.log(num) / Math.log(1000)),
    UNITS.length - 1
  );
  const numStr = Number((num / Math.pow(1000, exponent)).toPrecision(3));
  const unit = UNITS[exponent];

  return (neg ? "-" : "") + numStr + " " + unit;
}

module.exports = {
  initializePopup,
};
