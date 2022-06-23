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
 * @property {string[]} supportedFeatures
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
  h1,
  h2,
  h3,
  section,
  p,
  span,
} = require("devtools/client/shared/vendor/react-dom-factories");
const Range = createFactory(
  require("devtools/client/performance-new/components/Range")
);
const DirectoryPicker = createFactory(
  require("devtools/client/performance-new/components/DirectoryPicker")
);
const {
  makeLinear10Scale,
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
const Localized = createFactory(
  require("devtools/client/shared/vendor/fluent-react").Localized
);

// The Gecko Profiler interprets the "entries" setting as 8 bytes per entry.
const PROFILE_ENTRY_SIZE = 8;

/**
 * @typedef {{ name: string, id: string, l10nId: string }} ThreadColumn
 */

/** @type {Array<ThreadColumn[]>} */
const threadColumns = [
  [
    {
      name: "GeckoMain",
      id: "gecko-main",
      // The l10nId take the form `perf-thread-${id}`, but isn't done programmatically
      // so that it is easy to search in the codebase.
      l10nId: "perftools-thread-gecko-main",
    },
    {
      name: "Compositor",
      id: "compositor",
      l10nId: "perftools-thread-compositor",
    },
    {
      name: "DOM Worker",
      id: "dom-worker",
      l10nId: "perftools-thread-dom-worker",
    },
    {
      name: "Renderer",
      id: "renderer",
      l10nId: "perftools-thread-renderer",
    },
  ],
  [
    {
      name: "RenderBackend",
      id: "render-backend",
      l10nId: "perftools-thread-render-backend",
    },
    {
      name: "Timer",
      id: "timer",
      l10nId: "perftools-thread-timer",
    },
    {
      name: "StyleThread",
      id: "style-thread",
      l10nId: "perftools-thread-style-thread",
    },
    {
      name: "Socket Thread",
      id: "socket-thread",
      l10nId: "perftools-thread-socket-thread",
    },
  ],
  [
    {
      name: "StreamTrans",
      id: "stream-trans",
      l10nId: "pref-thread-stream-trans",
    },
    {
      name: "ImgDecoder",
      id: "img-decoder",
      l10nId: "perftools-thread-img-decoder",
    },
    {
      name: "DNS Resolver",
      id: "dns-resolver",
      l10nId: "perftools-thread-dns-resolver",
    },
    {
      // Threads that are part of XPCOM's TaskController thread pool.
      name: "TaskController",
      id: "task-controller",
      l10nId: "perftools-thread-task-controller",
    },
  ],
];

/** @type {Array<ThreadColumn[]>} */
const jvmThreadColumns = [
  [
    {
      name: "Gecko",
      id: "gecko",
      l10nId: "perftools-thread-jvm-gecko",
    },
    {
      name: "Nimbus",
      id: "nimbus",
      l10nId: "perftools-thread-jvm-nimbus",
    },
  ],
  [
    {
      name: "DefaultDispatcher",
      id: "default-dispatcher",
      l10nId: "perftools-thread-jvm-default-dispatcher",
    },
    {
      name: "Glean",
      id: "glean",
      l10nId: "perftools-thread-jvm-glean",
    },
  ],
  [
    {
      name: "arch_disk_io",
      id: "arch-disk-io",
      l10nId: "perftools-thread-jvm-arch-disk-io",
    },
    {
      name: "pool-",
      id: "pool",
      l10nId: "perftools-thread-jvm-pool",
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

    this._intervalExponentialScale = makeLinear10Scale(0.01, 100);
    this._entriesExponentialScale = makePowerOf2Scale(
      128 * 1024,
      256 * 1024 * 1024
    );
  }

  /**
   * Handle the checkbox change.
   * @param {React.ChangeEvent<HTMLInputElement>} event
   */
  _handleThreadCheckboxChange = event => {
    const { threads, changeThreads } = this.props;
    const { checked, value } = event.target;

    if (checked) {
      if (!threads.includes(value)) {
        changeThreads([...threads, value]);
      }
    } else {
      changeThreads(threads.filter(thread => thread !== value));
    }
  };

  /**
   * Handle the checkbox change.
   * @param {React.ChangeEvent<HTMLInputElement>} event
   */
  _handleFeaturesCheckboxChange = event => {
    const { features, changeFeatures } = this.props;
    const { checked, value } = event.target;

    if (checked) {
      if (!features.includes(value)) {
        changeFeatures([value, ...features]);
      }
    } else {
      changeFeatures(features.filter(feature => feature !== value));
    }
  };

  _handleAddObjdir = () => {
    const { objdirs, changeObjdirs } = this.props;
    openFilePickerForObjdir(window, objdirs, changeObjdirs);
  };

  /**
   * @param {number} index
   * @return {void}
   */
  _handleRemoveObjdir = index => {
    const { objdirs, changeObjdirs } = this.props;
    const newObjdirs = [...objdirs];
    newObjdirs.splice(index, 1);
    changeObjdirs(newObjdirs);
  };

  /**
   * @param {React.ChangeEvent<HTMLInputElement>} event
   */
  _setThreadTextFromInput = event => {
    this.setState({ temporaryThreadText: event.target.value });
  };

  /**
   * @param {React.ChangeEvent<HTMLInputElement>} event
   */
  _handleThreadTextCleanup = event => {
    this.setState({ temporaryThreadText: null });
    this.props.changeThreads(_threadTextToList(event.target.value));
  };

  /**
   * @param {ThreadColumn[]} threadDisplay
   * @param {number} index
   * @return {React.ReactNode}
   */
  _renderThreadsColumns(threadDisplay, index) {
    const { threads } = this.props;
    const areAllThreadsIncluded = threads.includes("*");
    return div(
      { className: "perf-settings-thread-column", key: index },
      threadDisplay.map(({ name, id, l10nId }) =>
        Localized(
          // The title is localized with a description of the thread.
          { id: l10nId, attrs: { title: true }, key: name },
          label(
            {
              className: `perf-settings-checkbox-label perf-settings-thread-label toggle-container-with-text ${
                areAllThreadsIncluded
                  ? "perf-settings-checkbox-label-disabled"
                  : ""
              }`,
            },
            input({
              className: "perf-settings-checkbox",
              id: `perf-settings-thread-checkbox-${id}`,
              type: "checkbox",
              // Do not localize the value, this is used internally by the profiler.
              value: name,
              checked: threads.includes(name),
              disabled: areAllThreadsIncluded,
              onChange: this._handleThreadCheckboxChange,
            }),
            span(null, name)
          )
        )
      )
    );
  }
  _renderThreads() {
    const { temporaryThreadText } = this.state;
    const { threads } = this.props;

    return renderSection(
      "perf-settings-threads-summary",
      Localized({ id: "perftools-heading-threads" }, "Threads"),
      div(
        null,
        div(
          { className: "perf-settings-thread-columns" },
          threadColumns.map((threadDisplay, index) =>
            this._renderThreadsColumns(threadDisplay, index)
          )
        ),
        this._renderJvmThreads(),
        div(
          {
            className: "perf-settings-checkbox-label perf-settings-all-threads",
          },
          label(
            {
              className: "toggle-container-with-text",
            },
            input({
              id: "perf-settings-thread-checkbox-all-threads",
              type: "checkbox",
              value: "*",
              checked: threads.includes("*"),
              onChange: this._handleThreadCheckboxChange,
            }),
            Localized({ id: "perftools-record-all-registered-threads" })
          )
        ),
        div(
          { className: "perf-settings-row" },
          Localized(
            { id: "perftools-tools-threads-input-label" },
            label(
              { className: "perf-settings-text-label" },
              div(
                null,
                Localized(
                  { id: "perftools-custom-threads-label" },
                  "Add custom threads by name:"
                )
              ),
              input({
                className: "perf-settings-text-input",
                id: "perftools-settings-thread-text",
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
      )
    );
  }

  _renderJvmThreads() {
    if (!this.props.supportedFeatures.includes("java")) {
      return null;
    }

    return [
      h2(
        null,
        Localized({ id: "perftools-heading-threads-jvm" }, "JVM Threads")
      ),
      div(
        { className: "perf-settings-thread-columns" },
        jvmThreadColumns.map((threadDisplay, index) =>
          this._renderThreadsColumns(threadDisplay, index)
        )
      ),
    ];
  }

  /**
   * @param {React.ReactNode} sectionTitle
   * @param {FeatureDescription[]} features
   * @param {boolean} isSupported
   */
  _renderFeatureSection(sectionTitle, features, isSupported) {
    if (features.length === 0) {
      return null;
    }

    // Note: This area is not localized. This area is pretty deep in the UI, and is mostly
    // geared towards Firefox engineers. It may not be worth localizing. This decision
    // can be tracked in Bug 1682333.

    return div(
      null,
      h3(null, sectionTitle),
      features.map(featureDescription => {
        const { name, value, title, disabledReason } = featureDescription;
        const extraClassName = isSupported
          ? ""
          : "perf-settings-checkbox-label-disabled";
        return label(
          {
            className: `perf-settings-checkbox-label perf-toggle-label ${extraClassName}`,
            key: value,
          },
          input({
            id: `perf-settings-feature-checkbox-${value}`,
            type: "checkbox",
            value,
            checked: isSupported && this.props.features.includes(value),
            onChange: this._handleFeaturesCheckboxChange,
            disabled: !isSupported,
          }),
          div(
            { className: "perf-toggle-text-label" },
            !isSupported && featureDescription.experimental
              ? // Note when unsupported features are experimental.
                `${name} (Experimental)`
              : name
          ),
          div(
            { className: "perf-toggle-description" },
            title,
            !isSupported && disabledReason
              ? div(
                  { className: "perf-settings-feature-disabled-reason" },
                  disabledReason
                )
              : null
          )
        );
      })
    );
  }

  _renderFeatures() {
    const { supportedFeatures } = this.props;

    // Divvy up the features into their respective groups.
    const recommended = [];
    const supported = [];
    const unsupported = [];
    const experimental = [];

    for (const feature of featureDescriptions) {
      if (supportedFeatures.includes(feature.value)) {
        if (feature.experimental) {
          experimental.push(feature);
        } else if (feature.recommended) {
          recommended.push(feature);
        } else {
          supported.push(feature);
        }
      } else {
        unsupported.push(feature);
      }
    }

    return div(
      { className: "perf-settings-sections" },
      div(
        null,
        this._renderFeatureSection(
          Localized(
            { id: "perftools-heading-features-default" },
            "Features (Recommended on by default)"
          ),
          recommended,
          true
        ),
        this._renderFeatureSection(
          Localized({ id: "perftools-heading-features" }, "Features"),
          supported,
          true
        ),
        this._renderFeatureSection(
          Localized(
            { id: "perftools-heading-features-experimental" },
            "Experimental"
          ),
          experimental,
          true
        ),
        this._renderFeatureSection(
          Localized(
            { id: "perftools-heading-features-disabled" },
            "Disabled Features"
          ),
          unsupported,
          false
        )
      )
    );
  }

  _renderLocalBuildSection() {
    const { objdirs } = this.props;
    return renderSection(
      "perf-settings-local-build-summary",
      Localized({ id: "perftools-heading-local-build" }),
      div(
        null,
        p(null, Localized({ id: "perftools-description-local-build" })),
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
      h1(null, Localized({ id: "perftools-heading-settings" })),
      h2(
        { className: "perf-settings-title" },
        Localized({ id: "perftools-heading-buffer" })
      ),
      Range({
        label: Localized({ id: "perftools-range-interval-label" }),
        value: this.props.interval,
        id: "perf-range-interval",
        scale: this._intervalExponentialScale,
        display: _intervalTextDisplay,
        onChange: this.props.changeInterval,
      }),
      Range({
        label: Localized({ id: "perftools-range-entries-label" }),
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
 * @return {React.ReactNode}
 */
function _intervalTextDisplay(value) {
  return Localized({
    id: "perftools-range-interval-milliseconds",
    $interval: value,
  });
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
 * Renders a section for about:profiling.
 *
 * @param {string} id Unused.
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
