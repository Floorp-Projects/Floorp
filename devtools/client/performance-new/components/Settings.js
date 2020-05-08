/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// @ts-check

/**
 * @typedef {Object} StateProps
 * @property {number} interval
 * @property {number} entries
 * @property {string[]} features
 * @property {string[]} threads
 * @property {string} threadsString
 * @property {string[]} objdirs
 * @property {string[] | null} supportedFeatures
 */

/**
 * @typedef {Object} ThunkDispatchProps
 * @property {typeof actions.changeInterval} changeInterval
 * @property {typeof actions.changeEntries} changeEntries
 * @property {typeof actions.changeFeatures} changeFeatures
 * @property {typeof actions.changeThreads} changeThreads
 * @property {typeof actions.changeObjdirs} changeObjdirs
 */

/**
 * @typedef {ResolveThunks<ThunkDispatchProps>} DispatchProps
 */

/**
 * @typedef {Object} State
 * @property {null | string} temporaryThreadText
 */

/**
 * @typedef {import("../@types/perf").PopupWindow} PopupWindow
 * @typedef {import("../@types/perf").State} StoreState
 * @typedef {import("../@types/perf").FeatureDescription} FeatureDescription
 *
 * @typedef {StateProps & DispatchProps} Props
 */

/**
 * @template P
 * @typedef {import("react-redux").ResolveThunks<P>} ResolveThunks<P>
 */

/**
 * @template InjectedProps
 * @template NeededProps
 * @typedef {import("react-redux")
 *    .InferableComponentEnhancerWithProps<InjectedProps, NeededProps>
 * } InferableComponentEnhancerWithProps<InjectedProps, NeededProps>
 */
"use strict";

const {
  PureComponent,
  createFactory,
} = require("devtools/client/shared/vendor/react");
const {
  div,
  label,
  input,
  span,
  h1,
  h2,
  h3,
  section,
  p,
  em,
} = require("devtools/client/shared/vendor/react-dom-factories");
const Range = createFactory(
  require("devtools/client/performance-new/components/Range")
);
const DirectoryPicker = createFactory(
  require("devtools/client/performance-new/components/DirectoryPicker")
);
const {
  makeExponentialScale,
  makePowerOf2Scale,
  formatFileSize,
  featureDescriptions,
} = require("devtools/client/performance-new/utils");
const { connect } = require("devtools/client/shared/vendor/react-redux");
const actions = require("devtools/client/performance-new/store/actions");
const selectors = require("devtools/client/performance-new/store/selectors");
const {
  openFilePickerForObjdir,
} = require("devtools/client/performance-new/browser");

// The Gecko Profiler interprets the "entries" setting as 8 bytes per entry.
const PROFILE_ENTRY_SIZE = 8;

/**
 * @typedef {{ name: string, id: string, title: string }} ThreadColumn
 */

/** @type {Array<ThreadColumn[]>} */
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

/**
 * This component manages the settings for recording a performance profile.
 * @extends {React.PureComponent<Props, State>}
 */
class Settings extends PureComponent {
  /**
   * @param {Props} props
   */
  constructor(props) {
    super(props);
    /** @type {State} */
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
    this._entriesExponentialScale = makePowerOf2Scale(
      128 * 1024,
      256 * 1024 * 1024
    );
  }

  /**
   * Handle the checkbox change.
   * @param {React.ChangeEvent<HTMLInputElement>} event
   */
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

  /**
   * Handle the checkbox change.
   * @param {React.ChangeEvent<HTMLInputElement>} event
   */
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
    openFilePickerForObjdir(window, objdirs, changeObjdirs);
  }

  /**
   * @param {number} index
   * @return {void}
   */
  _handleRemoveObjdir(index) {
    const { objdirs, changeObjdirs } = this.props;
    const newObjdirs = [...objdirs];
    newObjdirs.splice(index, 1);
    changeObjdirs(newObjdirs);
  }

  /**
   * @param {React.ChangeEvent<HTMLInputElement>} event
   */
  _setThreadTextFromInput(event) {
    this.setState({ temporaryThreadText: event.target.value });
  }

  /**
   * @param {React.ChangeEvent<HTMLInputElement>} event
   */
  _handleThreadTextCleanup(event) {
    this.setState({ temporaryThreadText: null });
    this.props.changeThreads(_threadTextToList(event.target.value));
  }

  /**
   * @param {ThreadColumn[]} threadDisplay
   * @param {number} index
   * @return {React.ReactNode}
   */
  _renderThreadsColumns(threadDisplay, index) {
    const { threads } = this.props;
    return div(
      { className: "perf-settings-thread-column", key: index },
      threadDisplay.map(({ name, title, id }) =>
        label(
          {
            className:
              "perf-settings-checkbox-label perf-settings-thread-label",
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
    const { temporaryThreadText } = this.state;
    const { threads } = this.props;

    return renderSection(
      "perf-settings-threads-summary",
      "Threads",
      div(
        null,
        div(
          { className: "perf-settings-thread-columns" },
          threadColumns.map(this._renderThreadsColumns)
        ),
        div(
          { className: "perf-settings-all-threads" },
          label(
            {
              className: "perf-settings-checkbox-label",
            },
            input({
              className: "perf-settings-checkbox",
              id: "perf-settings-thread-checkbox-all-threads",
              type: "checkbox",
              value: "*",
              checked: threads.includes("*"),
              onChange: this._handleThreadCheckboxChange,
            }),
            "Bypass selections above and record ",
            em(null, "all"),
            " registered threads"
          )
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
            div(null, "Add custom threads by name:"),
            input({
              className: "perf-settings-text-input",
              id: "perf-settings-thread-text",
              type: "text",
              value:
                temporaryThreadText === null
                  ? threads.join(",")
                  : temporaryThreadText,
              onBlur: this._handleThreadTextCleanup,
              onFocus: this._setThreadTextFromInput,
              onChange: this._setThreadTextFromInput,
            })
          )
        )
      )
    );
  }

  /**
   * @param {FeatureDescription} featureDescription
   * @param {boolean} showUnsupportedFeatures
   */
  _renderFeatureCheckbox(featureDescription, showUnsupportedFeatures) {
    const { supportedFeatures } = this.props;
    const {
      name,
      value,
      title,
      recommended,
      disabledReason,
    } = featureDescription;
    let isSupported = true;
    if (supportedFeatures !== null && !supportedFeatures.includes(value)) {
      isSupported = false;
    }
    if (showUnsupportedFeatures === isSupported) {
      // This method gets called twice, once for supported featured, and once for
      // unsupported features. Only render the appropriate features for each section.
      return null;
    }

    const extraClassName = isSupported
      ? ""
      : "perf-settings-checkbox-label-disabled";

    return label(
      {
        className: `perf-settings-checkbox-label perf-settings-feature-label ${extraClassName}`,
        key: value,
      },
      div(
        { className: "perf-settings-checkbox-and-name" },
        input({
          className: "perf-settings-checkbox",
          id: `perf-settings-feature-checkbox-${value}`,
          type: "checkbox",
          value,
          checked: isSupported && this.props.features.includes(value),
          onChange: this._handleFeaturesCheckboxChange,
          disabled: !isSupported,
        }),
        div({ className: "perf-settings-feature-name" }, name)
      ),
      div(
        { className: "perf-settings-feature-title" },
        title,
        !isSupported && disabledReason
          ? div(
              { className: "perf-settings-feature-disabled-reason" },
              disabledReason
            )
          : null,
        recommended
          ? span(
              { className: "perf-settings-subtext" },
              " (Recommended on by default.)"
            )
          : null
      )
    );
  }

  _renderFeatures() {
    return renderSection(
      "perf-settings-features-summary",
      "Features",
      div(
        null,
        // Render the supported features first.
        featureDescriptions.map(featureDescription =>
          this._renderFeatureCheckbox(featureDescription, false)
        ),
        h3(
          { className: "perf-settings-features-disabled-title" },
          "The following features are currently unavailable:"
        ),
        // Render the unsupported features second.
        featureDescriptions.map(featureDescription =>
          this._renderFeatureCheckbox(featureDescription, true)
        )
      )
    );
  }

  _renderLocalBuildSection() {
    const { objdirs } = this.props;
    return renderSection(
      "perf-settings-local-build-summary",
      "Local build",
      div(
        null,
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
    );
  }

  render() {
    return section(
      { className: "perf-settings" },
      h1(null, "Full Settings"),
      h2({ className: "perf-settings-title" }, "Buffer Settings"),
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
 * @param {string} threads - Comma separated values.
 * @return {string[]}
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

/**
 * about:profiling doesn't need to collapse the children into details/summary,
 * but the popup and devtools do (for now).
 *
 * @param {string} id
 * @param {React.ReactNode} title
 * @param {React.ReactNode} children
 * @returns React.ReactNode
 */
function renderSection(id, title, children) {
  return div(
    { className: "perf-settings-sections" },
    div(null, h2(null, title), children)
  );
}

/**
 * @param {StoreState} state
 * @returns {StateProps}
 */
function mapStateToProps(state) {
  return {
    interval: selectors.getInterval(state),
    entries: selectors.getEntries(state),
    features: selectors.getFeatures(state),
    threads: selectors.getThreads(state),
    threadsString: selectors.getThreadsString(state),
    objdirs: selectors.getObjdirs(state),
    supportedFeatures: selectors.getSupportedFeatures(state),
  };
}

/** @type {ThunkDispatchProps} */
const mapDispatchToProps = {
  changeInterval: actions.changeInterval,
  changeEntries: actions.changeEntries,
  changeFeatures: actions.changeFeatures,
  changeThreads: actions.changeThreads,
  changeObjdirs: actions.changeObjdirs,
};

const SettingsConnected = connect(
  mapStateToProps,
  mapDispatchToProps
)(Settings);

module.exports = SettingsConnected;
