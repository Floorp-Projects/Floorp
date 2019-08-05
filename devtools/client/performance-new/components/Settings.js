/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";
const {
  PureComponent,
  createFactory,
} = require("devtools/client/shared/vendor/react");
const {
  div,
  details,
  summary,
  label,
  input,
  span,
  h2,
  section,
  p,
} = require("devtools/client/shared/vendor/react-dom-factories");
const Range = createFactory(
  require("devtools/client/performance-new/components/Range")
);
const DirectoryPicker = createFactory(
  require("devtools/client/performance-new/components/DirectoryPicker")
);
const {
  makeExponentialScale,
  formatFileSize,
  calculateOverhead,
} = require("devtools/client/performance-new/utils");
const { connect } = require("devtools/client/shared/vendor/react-redux");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const actions = require("devtools/client/performance-new/store/actions");
const selectors = require("devtools/client/performance-new/store/selectors");
const { XPCOMUtils } = require("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyServiceGetter(
  this,
  "FilePicker",
  "@mozilla.org/filepicker;1",
  "nsIFilePicker"
);

// sizeof(double) + sizeof(char)
// http://searchfox.org/mozilla-central/rev/e8835f52eff29772a57dca7bcc86a9a312a23729/tools/profiler/core/ProfileEntry.h#73
const PROFILE_ENTRY_SIZE = 9;

const NOTCHES = Array(22).fill("discrete-level-notch");

const threadColumns = [
  [
    {
      name: "GeckoMain",
      id: "gecko-main",
      title:
        "The main processes for both the parent process, and content processes",
    },
    {
      name: "Compositor",
      id: "compositor",
      title: "Composites together different painted elements on the page.",
    },
    {
      name: "DOM Worker",
      id: "dom-worker",
      title: "This handle both web workers and service workers",
    },
    {
      name: "Renderer",
      id: "renderer",
      title: "When WebRender is enabled, the thread that executes OpenGL calls",
    },
  ],
  [
    {
      name: "RenderBackend",
      id: "render-backend",
      title: "The WebRender RenderBackend thread",
    },
    {
      name: "PaintWorker",
      id: "paint-worker",
      title:
        "When off-main-thread painting is enabled, the thread on which " +
        "painting happens",
    },
    {
      name: "StyleThread",
      id: "style-thread",
      title: "Style computation is split into multiple threads",
    },
    {
      name: "Socket Thread",
      id: "socket-thread",
      title: "The thread where networking code runs any blocking socket calls",
    },
  ],
  [
    {
      name: "StreamTrans",
      id: "stream-trans",
      title: "TODO",
    },
    {
      name: "ImgDecoder",
      id: "img-decoder",
      title: "Image decoding threads",
    },
    {
      name: "DNS Resolver",
      id: "dns-resolver",
      title: "DNS resolution happens on this thread",
    },
  ],
];

const featureCheckboxes = [
  {
    name: "Native Stacks",
    value: "stackwalk",
    title:
      "Record native stacks (C++ and Rust). This is not available on all platforms.",
    recommended: true,
  },
  {
    name: "JavaScript",
    value: "js",
    title:
      "Record JavaScript stack information, and interleave it with native stacks.",
    recommended: true,
  },
  {
    name: "Responsiveness",
    value: "responsiveness",
    title: "Collect thread responsiveness information.",
    recommended: true,
  },
  {
    name: "Java",
    value: "java",
    title: "Profile Java code (Android only).",
  },
  {
    name: "Native Leaf Stack",
    value: "leaf",
    title:
      "Record the native memory address of the leaf-most stack. This could be " +
      "useful on platforms that do not support stack walking.",
  },
  {
    name: "Main Thread IO",
    value: "mainthreadio",
    title: "Record main thread I/O markers.",
  },
  {
    name: "Privacy",
    value: "privacy",
    title: "Remove some potentially user-identifiable information.",
  },
  {
    name: "Sequential Styling",
    value: "seqstyle",
    title: "Disable parallel traversal in styling.",
  },
  {
    name: "JIT Optimizations",
    value: "trackopts",
    title: "Track JIT optimizations in the JS engine.",
  },
  {
    name: "TaskTracer",
    value: "tasktracer",
    title: "Enable TaskTracer (Experimental, requires custom build.)",
  },
  {
    name: "Screenshots",
    value: "screenshots",
    title: "Record screenshots of all browser windows.",
  },
  {
    name: "JSTracer",
    value: "jstracer",
    title: "Trace JS engine (Experimental, requires custom build.)",
  },
];

/**
 * This component manages the settings for recording a performance profile.
 */
class Settings extends PureComponent {
  static get propTypes() {
    return {
      // StateProps
      interval: PropTypes.number.isRequired,
      entries: PropTypes.number.isRequired,
      features: PropTypes.array.isRequired,
      threads: PropTypes.array.isRequired,
      threadsString: PropTypes.string.isRequired,
      objdirs: PropTypes.array.isRequired,

      // DispatchProps
      changeInterval: PropTypes.func.isRequired,
      changeEntries: PropTypes.func.isRequired,
      changeFeatures: PropTypes.func.isRequired,
      changeThreads: PropTypes.func.isRequired,
      changeObjdirs: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);
    this.state = {
      // Allow the textbox to have a temporary tracked value.
      temporaryThreadText: null,
    };

    this._handleThreadCheckboxChange = this._handleThreadCheckboxChange.bind(
      this
    );
    this._handleFeaturesCheckboxChange = this._handleFeaturesCheckboxChange.bind(
      this
    );
    this._handleAddObjdir = this._handleAddObjdir.bind(this);
    this._handleRemoveObjdir = this._handleRemoveObjdir.bind(this);
    this._setThreadTextFromInput = this._setThreadTextFromInput.bind(this);
    this._handleThreadTextCleanup = this._handleThreadTextCleanup.bind(this);
    this._renderThreadsColumns = this._renderThreadsColumns.bind(this);

    this._intervalExponentialScale = makeExponentialScale(0.01, 100);
    this._entriesExponentialScale = makeExponentialScale(100000, 100000000);
  }

  _renderNotches() {
    const { interval, entries, features } = this.props;
    const overhead = calculateOverhead(interval, entries, features);
    const notchCount = 22;
    const notches = [];
    for (let i = 0; i < notchCount; i++) {
      const active =
        i <= Math.round(overhead * (NOTCHES.length - 1))
          ? "active"
          : "inactive";

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
            `perf-settings-notch-${active}`,
        })
      );
    }
    return notches;
  }

  _handleThreadCheckboxChange(event) {
    const { threads, changeThreads } = this.props;
    const { checked, value } = event.target;

    if (checked) {
      if (!threads.includes(value)) {
        changeThreads([...threads, value]);
      }
    } else {
      changeThreads(threads.filter(thread => thread !== value));
    }
  }

  _handleFeaturesCheckboxChange(event) {
    const { features, changeFeatures } = this.props;
    const { checked, value } = event.target;

    if (checked) {
      if (!features.includes(value)) {
        changeFeatures([value, ...features]);
      }
    } else {
      changeFeatures(features.filter(feature => feature !== value));
    }
  }

  _handleAddObjdir() {
    const { objdirs, changeObjdirs } = this.props;
    FilePicker.init(window, "Pick build directory", FilePicker.modeGetFolder);
    FilePicker.open(rv => {
      if (rv == FilePicker.returnOK) {
        const path = FilePicker.file.path;
        if (path && !objdirs.includes(path)) {
          const newObjdirs = [...objdirs, path];
          changeObjdirs(newObjdirs);
        }
      }
    });
  }

  _handleRemoveObjdir(index) {
    const { objdirs, changeObjdirs } = this.props;
    const newObjdirs = [...objdirs];
    newObjdirs.splice(index, 1);
    changeObjdirs(newObjdirs);
  }

  _setThreadTextFromInput(event) {
    this.setState({ temporaryThreadText: event.target.value });
  }

  _handleThreadTextCleanup(event) {
    this.setState({ temporaryThreadText: null });
    this.props.changeThreads(_threadTextToList(event.target.value));
  }

  _renderThreadsColumns(threadDisplay, index) {
    const { threads } = this.props;
    return div(
      { className: "perf-settings-thread-column", key: index },
      threadDisplay.map(({ name, title, id }) =>
        label(
          {
            className: "perf-settings-checkbox-label",
            key: name,
            title,
          },
          input({
            className: "perf-settings-checkbox",
            id: `perf-settings-thread-checkbox-${id}`,
            type: "checkbox",
            value: name,
            checked: threads.includes(name),
            onChange: this._handleThreadCheckboxChange,
          }),
          name
        )
      )
    );
  }

  _renderThreads() {
    return details(
      { className: "perf-settings-details" },
      summary(
        {
          className: "perf-settings-summary",
          id: "perf-settings-threads-summary",
        },
        "Threads:"
      ),
      // Contain the overflow of the slide down animation with the first div.
      div(
        { className: "perf-settings-details-contents" },
        // Provide a second <div> element for the contents of the slide down animation.
        div(
          { className: "perf-settings-details-contents-slider" },
          div(
            { className: "perf-settings-thread-columns" },
            threadColumns.map(this._renderThreadsColumns)
          ),
          div(
            { className: "perf-settings-row" },
            label(
              {
                className: "perf-settings-text-label",
                title:
                  "These thread names are a comma separated list that is used to " +
                  "enable profiling of the threads in the profiler. The name needs to " +
                  "be only a partial match of the thread name to be included. It " +
                  "is whitespace sensitive.",
              },
              div({}, "Add custom threads by name:"),
              input({
                className: "perf-settings-text-input",
                id: "perf-settings-thread-text",
                type: "text",
                value:
                  this.state.temporaryThreadText === null
                    ? this.props.threads
                    : this.state.temporaryThreadText,
                onBlur: this._handleThreadTextCleanup,
                onFocus: this._setThreadTextFromInput,
                onChange: this._setThreadTextFromInput,
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
      summary(
        {
          className: "perf-settings-summary",
          id: "perf-settings-features-summary",
        },
        "Features:"
      ),
      div(
        { className: "perf-settings-details-contents" },
        div(
          { className: "perf-settings-details-contents-slider" },
          featureCheckboxes.map(({ name, value, title, recommended }) =>
            label(
              {
                className:
                  "perf-settings-checkbox-label perf-settings-feature-label",
                key: value,
              },
              input({
                className: "perf-settings-checkbox",
                id: `perf-settings-feature-checkbox-${value}`,
                type: "checkbox",
                value,
                checked: this.props.features.includes(value),
                onChange: this._handleFeaturesCheckboxChange,
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
            )
          )
        )
      )
    );
  }

  _renderLocalBuildSection() {
    const { objdirs } = this.props;
    return details(
      { className: "perf-settings-details" },
      summary(
        {
          className: "perf-settings-summary",
          id: "perf-settings-local-build-summary",
        },
        "Local build:"
      ),
      div(
        { className: "perf-settings-details-contents" },
        div(
          { className: "perf-settings-details-contents-slider" },
          p(
            null,
            `If you're profiling a build that you have compiled yourself, on this
            machine, please add your build's objdir to the list below so that
            it can be used to look up symbol information.`
          ),
          DirectoryPicker({
            dirs: objdirs,
            onAdd: this._handleAddObjdir,
            onRemove: this._handleRemoveObjdir,
          })
        )
      )
    );
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
        value: this.props.interval,
        id: "perf-range-interval",
        scale: this._intervalExponentialScale,
        display: _intervalTextDisplay,
        onChange: this.props.changeInterval,
      }),
      Range({
        label: "Buffer size:",
        value: this.props.entries,
        id: "perf-range-entries",
        scale: this._entriesExponentialScale,
        display: _entriesTextDisplay,
        onChange: this.props.changeEntries,
      }),
      this._renderThreads(),
      this._renderFeatures(),
      this._renderLocalBuildSection()
    );
  }
}

/**
 * Clean up the thread list string into a list of values.
 * @param string threads, comma separated values.
 * @return Array list of thread names
 */
function _threadTextToList(threads) {
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

function mapStateToProps(state) {
  return {
    interval: selectors.getInterval(state),
    entries: selectors.getEntries(state),
    features: selectors.getFeatures(state),
    threads: selectors.getThreads(state),
    threadsString: selectors.getThreadsString(state),
    objdirs: selectors.getObjdirs(state),
  };
}

const mapDispatchToProps = {
  changeInterval: actions.changeInterval,
  changeEntries: actions.changeEntries,
  changeFeatures: actions.changeFeatures,
  changeThreads: actions.changeThreads,
  changeObjdirs: actions.changeObjdirs,
};

module.exports = connect(
  mapStateToProps,
  mapDispatchToProps
)(Settings);
