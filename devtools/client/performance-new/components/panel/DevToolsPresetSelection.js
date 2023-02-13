/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// @ts-check

/**
 * @template P
 * @typedef {import("react-redux").ResolveThunks<P>} ResolveThunks<P>
 */

/**
 * @typedef {Object} StateProps
 * @property {string} presetName
 * @property {number} interval
 * @property {string[]} threads
 * @property {string[]} features
 * @property {import("../../@types/perf").Presets} presets
 */

/**
 * @typedef {Object} ThunkDispatchProps
 * @property {typeof actions.changePreset} changePreset
 */

/**
 * @typedef {Object} OwnProps
 * @property {() => void} onEditSettingsLinkClicked
 */

/**
 * @typedef {ResolveThunks<ThunkDispatchProps>} DispatchProps
 * @typedef {StateProps & DispatchProps & OwnProps} Props
 * @typedef {import("../../@types/perf").State} StoreState
 * @typedef {import("../../@types/perf").FeatureDescription} FeatureDescription
 */

"use strict";

const {
  PureComponent,
  createFactory,
} = require("resource://devtools/client/shared/vendor/react.js");
const {
  div,
  select,
  option,
  button,
  ul,
  li,
  span,
} = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const {
  connect,
} = require("resource://devtools/client/shared/vendor/react-redux.js");
const actions = require("resource://devtools/client/performance-new/store/actions.js");
const selectors = require("resource://devtools/client/performance-new/store/selectors.js");
const {
  featureDescriptions,
} = require("resource://devtools/client/performance-new/shared/utils.js");
const Localized = createFactory(
  require("resource://devtools/client/shared/vendor/fluent-react.js").Localized
);

/**
 * This component displays the preset selection for the DevTools panel. It should be
 * basically the same implementation as the popup, but done in React. The popup
 * is written using vanilla JS and browser chrome elements in order to be more
 * performant.
 *
 * @extends {React.PureComponent<Props>}
 */
class DevToolsPresetSelection extends PureComponent {
  /** @param {Props} props */
  constructor(props) {
    super(props);

    /**
     * Create an object map to easily look up feature description.
     * @type {{[key: string]: FeatureDescription}}
     */
    this.featureDescriptionMap = {};
    for (const feature of featureDescriptions) {
      this.featureDescriptionMap[feature.value] = feature;
    }
  }

  /**
   * Handle the select change.
   * @param {React.ChangeEvent<HTMLSelectElement>} event
   */
  onPresetChange = event => {
    const { presets } = this.props;
    this.props.changePreset(presets, event.target.value);
  };

  render() {
    const { presetName, presets, onEditSettingsLinkClicked } = this.props;

    let presetDescription;
    const currentPreset = presets[presetName];
    if (currentPreset) {
      // Display the current preset's description.
      presetDescription = Localized({
        id: currentPreset.l10nIds.devtools.description,
      });
    } else {
      // Build up a display of the details of the custom preset.
      const { interval, threads, features } = this.props;
      presetDescription = div(
        null,
        ul(
          { className: "perf-presets-custom" },
          li(
            null,
            Localized(
              { id: "perftools-devtools-interval-label" },
              span({ className: "perf-presets-custom-bold" })
            ),
            " ",
            Localized({
              id: "perftools-range-interval-milliseconds",
              $interval: interval,
            })
          ),
          li(
            null,
            Localized(
              { id: "perftools-devtools-threads-label" },
              span({ className: "perf-presets-custom-bold" })
            ),
            " ",
            threads.join(", ")
          ),
          features.map(feature => {
            const description = this.featureDescriptionMap[feature];
            if (!description) {
              throw new Error(
                "Could not find the feature description for " + feature
              );
            }
            return li(
              { key: feature },
              description ? description.name : feature
            );
          })
        )
      );
    }

    return div(
      { className: "perf-presets" },
      div(
        { className: "perf-presets-settings" },
        Localized({ id: "perftools-devtools-settings-label" })
      ),
      div(
        { className: "perf-presets-details" },
        div(
          { className: "perf-presets-details-row" },
          select(
            {
              className: "perf-presets-select",
              onChange: this.onPresetChange,
              value: presetName,
            },
            Object.entries(presets).map(([name, preset]) =>
              Localized(
                { id: preset.l10nIds.devtools.label },
                option({ key: name, value: name })
              )
            ),
            Localized(
              { id: "perftools-presets-custom-label" },
              option({ value: "custom" })
            )
          )
          // The overhead component will go here.
        ),
        div(
          { className: "perf-presets-details-row perf-presets-description" },
          presetDescription
        ),
        button(
          {
            className: "perf-external-link",
            onClick: onEditSettingsLinkClicked,
          },
          Localized({ id: "perftools-button-edit-settings" })
        )
      )
    );
  }
}

/**
 * @param {StoreState} state
 * @returns {StateProps}
 */
function mapStateToProps(state) {
  return {
    presetName: selectors.getPresetName(state),
    presets: selectors.getPresets(state),
    interval: selectors.getInterval(state),
    threads: selectors.getThreads(state),
    features: selectors.getFeatures(state),
  };
}

/**
 * @type {ThunkDispatchProps}
 */
const mapDispatchToProps = {
  changePreset: actions.changePreset,
};

module.exports = connect(
  mapStateToProps,
  mapDispatchToProps
)(DevToolsPresetSelection);
