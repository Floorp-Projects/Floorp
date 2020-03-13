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
 * @property {import("../@types/perf").Presets} presets
 */

/**
 * @typedef {Object} ThunkDispatchProps
 * @property {typeof actions.changePreset} changePreset
 */

/**
 * @typedef {ResolveThunks<ThunkDispatchProps>} DispatchProps
 * @typedef {StateProps & DispatchProps} Props
 * @typedef {import("../@types/perf").State} StoreState
 */

"use strict";
const { PureComponent } = require("devtools/client/shared/vendor/react");
const {
  div,
  label,
  input,
} = require("devtools/client/shared/vendor/react-dom-factories");
const selectors = require("devtools/client/performance-new/store/selectors");
const actions = require("devtools/client/performance-new/store/actions");
const { connect } = require("devtools/client/shared/vendor/react-redux");

/**
 * Switch between various profiler presets, which will override the individualized
 * settings for the profiler.
 *
 * @extends {React.PureComponent<Props>}
 */
class Presets extends PureComponent {
  /** @param {Props} props */
  constructor(props) {
    super(props);
    this.onChange = this.onChange.bind(this);
  }

  /**
   * Handle the checkbox change.
   * @param {React.ChangeEvent<HTMLInputElement>} event
   */
  onChange(event) {
    const { presets } = this.props;
    this.props.changePreset(presets, event.target.value);
  }

  /**
   * @param {string} presetName
   * @returns {React.ReactNode}
   */
  renderPreset(presetName) {
    const preset = this.props.presets[presetName];
    let labelText, description;
    if (preset) {
      labelText = preset.label;
      description = preset.description;
    } else {
      labelText = "Custom";
    }
    return label(
      { className: "perf-presets-label" },
      div(
        { className: "perf-presets-input-container" },
        input({
          className: "perf-presets-input",
          type: "radio",
          name: "presets",
          value: presetName,
          checked: this.props.presetName === presetName,
          onChange: this.onChange,
        })
      ),
      div(
        { className: "perf-presets-text" },
        div({ className: "pref-preset-text-label" }, labelText),
        description
          ? div({ className: "perf-presets-description" }, description)
          : null
      )
    );
  }

  render() {
    return div(
      { className: "perf-presets" },
      this.renderPreset("web-developer"),
      this.renderPreset("firefox-platform"),
      this.renderPreset("firefox-front-end"),
      this.renderPreset("custom")
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
  };
}

/**
 * @type {ThunkDispatchProps}
 */
const mapDispatchToProps = {
  changePreset: actions.changePreset,
};

module.exports = connect(mapStateToProps, mapDispatchToProps)(Presets);
