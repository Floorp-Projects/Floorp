/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";
const { PureComponent, createFactory } = require("devtools/client/shared/vendor/react");
const { div, details, summary, label, input, span, h2, section } = require("devtools/client/shared/vendor/react-dom-factories");
const Range = createFactory(require("devtools/client/performance-new/components/Range"));
const { makeExponentialScale, formatFileSize, calculateOverhead } = require("devtools/client/performance-new/utils");

// sizeof(double) + sizeof(char)
// http://searchfox.org/mozilla-central/rev/e8835f52eff29772a57dca7bcc86a9a312a23729/tools/profiler/core/ProfileEntry.h#73
const PROFILE_ENTRY_SIZE = 9;

const NOTCHES = Array(22).fill("discrete-level-notch");

const threadColumns = [
  [
    {
      name: "GeckoMain",
      title: "The main processes for both the parent process, and content processes"
    },
    {
      name: "Compositor",
      title: "Composites together different painted elements on the page."
    },
    {
      name: "DOM Worker",
      title: "This handle both web workers and service workers"
    },
    {
      name: "Renderer",
      title: "When WebRender is enabled, the thread that executes OpenGL calls"
    },
  ],
  [
    {
      name: "RenderBackend",
      title: "The WebRender RenderBackend thread"
    },
    {
      name: "PaintWorker",
      title: "When off-main-thread painting is enabled, the thread on which " +
        "painting happens"
    },
    {
      name: "StyleThread",
      title: "Style computation is split into multiple threads"
    },
    {
      name: "Socket Thread",
      title: "The thread where networking code runs any blocking socket calls"
    },
  ],
  [
    {
      name: "StreamTrans",
      title: "TODO"
    },
    {
      name: "ImgDecoder",
      title: "Image decoding threads"
    },
    {
      name: "DNS Resolver",
      title: "DNS resolution happens on this thread"
    },
  ]
];

const featureCheckboxes = [
  {
    name: "Native Stacks",
    value: "stackwalk",
    title: "Record native stacks (C++ and Rust). This is not available on all platforms.",
    recommended: true
  },
  {
    name: "JavaScript",
    value: "js",
    title: "Record JavaScript stack information, and interleave it with native stacks.",
    recommended: true
  },
  {
    name: "Java",
    value: "java",
    title: "Profile Java code (Android only)."
  },
  {
    name: "Native Leaf Stack",
    value: "leaf",
    title: "Record the native memory address of the leaf-most stack. This could be " +
      "useful on platforms that do not support stack walking."
  },
  {
    name: "Main Thread IO",
    value: "mainthreadio",
    title: "Record main thread I/O markers."
  },
  {
    name: "Memory",
    value: "memory",
    title: "Add memory measurements to the samples, this includes resident set size " +
      "(RSS) and unique set size (USS)."
  },
  {
    name: "Privacy",
    value: "privacy",
    title: "Remove some potentially user-identifiable information."
  },
  {
    name: "JIT Optimizations",
    value: "trackopts",
    title: "Track JIT optimizations in the JS engine."
  },
  {
    name: "TaskTracer",
    value: "tasktracer",
    title: "Enable TaskTracer (Experimental, requires custom build.)"
  },
];

/**
 * This component manages the settings for recording a performance profile. In addition
 * to rendering the UI, it also manages the state of the settings. In order to not
 * introduce the complexity of adding Redux to a relatively simple UI, this
 * component expects to be accessed via the `ref`, and then calling
 * `settings.getRecordingSettings()` to get out the settings. If the recording panel
 * takes on new responsibilities, then this decision should be revisited.
 */
class PerfSettings extends PureComponent {
  static get propTypes() {
    return {};
  }

  constructor(props) {
    super(props);
    // Right now the defaults are reset every time the panel is opened. These should
    // be persisted between sessions. See Bug 1453014.
    this.state = {
      interval: 1,
      entries: 10000000, // 90MB
      features: {
        js: true,
        stackwalk: true,
      },
      threads: "GeckoMain,Compositor",
      threadListFocused: false,
    };
    this._handleThreadCheckboxChange = this._handleThreadCheckboxChange.bind(this);
    this._handleFeaturesCheckboxChange = this._handleFeaturesCheckboxChange.bind(this);
    this._handleThreadTextChange = this._handleThreadTextChange.bind(this);
    this._handleThreadTextCleanup = this._handleThreadTextCleanup.bind(this);
    this._renderThreadsColumns = this._renderThreadsColumns.bind(this);
    this._onChangeInterval = this._onChangeInterval.bind(this);
    this._onChangeEntries = this._onChangeEntries.bind(this);
    this._intervalExponentialScale = makeExponentialScale(0.01, 100);
    this._entriesExponentialScale = makeExponentialScale(100000, 100000000);
  }

  getRecordingSettings() {
    const features = [];
    for (const [name, isSet] of Object.entries(this.state.features)) {
      if (isSet) {
        features.push(name);
      }
    }
    return {
      entries: this.state.entries,
      interval: this.state.interval,
      features,
      threads: _threadStringToList(this.state.threads)
    };
  }

  _renderNotches() {
    const { interval, entries, features } = this.state;
    const overhead = calculateOverhead(interval, entries, features);
    const notchCount = 22;
    const notches = [];
    for (let i = 0; i < notchCount; i++) {
      const active = i <= Math.round(overhead * (NOTCHES.length - 1))
        ? "active" : "inactive";

      let level = "normal";
      if (i > 16) {
        level = "critical";
      } else if (i > 10) {
        level = "warning";
      }
      notches.push(
        div({
          key: i,
          className:
          `perf-settings-notch perf-settings-notch-${level} ` +
            `perf-settings-notch-${active}`
        })
      );
    }
    return notches;
  }

  _handleThreadCheckboxChange(event) {
    const { checked, value }  = event.target;

    this.setState(state => {
      let threadsList = _threadStringToList(state.threads);
      if (checked) {
        if (!threadsList.includes(value)) {
          threadsList.push(value);
        }
      } else {
        threadsList = threadsList.filter(thread => thread !== value);
      }
      return { threads: threadsList.join(",") };
    });
  }

  _handleFeaturesCheckboxChange(event) {
    const { checked, value }  = event.target;

    this.setState(state => ({
      features: {...state.features, [value]: checked}
    }));
  }

  _handleThreadTextChange(event) {
    this.setState({ threads: event.target.value });
  }

  _handleThreadTextCleanup() {
    this.setState(state => {
      const threadsList = _threadStringToList(state.threads);
      return { threads: threadsList.join(",") };
    });
  }

  _renderThreadsColumns(threads, index) {
    return div(
      { className: "perf-settings-thread-column", key: index },
      threads.map(({name, title}) => label(
        {
          className: "perf-settings-checkbox-label",
          key: name,
          title
        },
        input({
          className: "perf-settings-checkbox",
          type: "checkbox",
          value: name,
          checked: this.state.threads.includes(name),
          onChange: this._handleThreadCheckboxChange
        }),
        name
      ))
    );
  }

  _renderThreads() {
    return details(
      { className: "perf-settings-details" },
      summary({ className: "perf-settings-summary" }, "Threads:"),
      // Contain the overflow of the slide down animation with the first div.
      div(
        { className: "perf-settings-details-contents" },
        // Provide a second <div> element for the contents of the slide down animation.
        div(
          { className: "perf-settings-details-contents-slider" },
          div(
            { className: "perf-settings-thread-columns" },
            threadColumns.map(this._renderThreadsColumns),
          ),
          div(
            { className: "perf-settings-row" },
            label(
              {
                className: "perf-settings-text-label",
                title: "These thread names are a comma separated list that is used to " +
                  "enable profiling of the threads in the profiler. The name needs to " +
                  "be only a partial match of the thread name to be included. It " +
                  "is whitespace sensitive."
              },
              div({}, "Add custom threads by name:"),
              input({
                className: "perf-settings-text-input",
                type: "text",
                value: this.state.threads,
                onChange: this._handleThreadTextChange,
                onBlur: this._handleThreadTextCleanup,
              })
            )
          )
        )
      )
    );
  }

  _renderFeatures() {
    return details(
      { className: "perf-settings-details" },
      summary({ className: "perf-settings-summary" }, "Features:"),
      div(
        { className: "perf-settings-details-contents" },
        div(
          { className: "perf-settings-details-contents-slider" },
          featureCheckboxes.map(({name, value, title, recommended}) => label(
            {
              className: "perf-settings-checkbox-label perf-settings-feature-label",
              key: value,
            },
            input({
              className: "perf-settings-checkbox",
              type: "checkbox",
              value,
              checked: this.state.features[value],
              onChange: this._handleFeaturesCheckboxChange
            }),
            div({ className: "perf-settings-feature-name" }, name),
            div(
              { className: "perf-settings-feature-title" },
              title,
              recommended
                ? span(
                  { className: "perf-settings-subtext" },
                  " (Recommended on by default.)"
                )
                : null
            )
          ))
        )
      )
    );
  }

  _onChangeInterval(interval) {
    this.setState({ interval });
  }

  _onChangeEntries(entries) {
    this.setState({ entries });
  }

  render() {
    return section(
      { className: "perf-settings" },
      h2({ className: "perf-settings-title" }, "Recording Settings"),
      div(
        { className: "perf-settings-row" },
        label({ className: "perf-settings-label" }, "Overhead:"),
        div(
          { className: "perf-settings-value perf-settings-notches" },
          this._renderNotches()
        )
      ),
      Range({
        label: "Sampling interval:",
        value: this.state.interval,
        id: "perf-range-interval",
        scale: this._intervalExponentialScale,
        display: _intervalTextDisplay,
        onChange: this._onChangeInterval
      }),
      Range({
        label: "Buffer size:",
        value: this.state.entries,
        id: "perf-range-entries",
        scale: this._entriesExponentialScale,
        display: _entriesTextDisplay,
        onChange: this._onChangeEntries
      }),
      this._renderThreads(),
      this._renderFeatures()
    );
  }
}

/**
 * Clean up the thread list string into a list of values.
 * @param string threads, comma separated values.
 * @return Array list of thread names
 */
function _threadStringToList(threads) {
  return threads
    // Split on commas
    .split(",")
    // Clean up any extraneous whitespace
    .map(string => string.trim())
    // Filter out any blank strings
    .filter(string => string);
}

/**
 * Format the interval number for display.
 * @param {number} value
 * @return {string}
 */
function _intervalTextDisplay(value) {
  return `${value} ms`;
}

/**
 * Format the entries number for display.
 * @param {number} value
 * @return {string}
 */
function _entriesTextDisplay(value) {
  return formatFileSize(value * PROFILE_ENTRY_SIZE);
}

module.exports = PerfSettings;
