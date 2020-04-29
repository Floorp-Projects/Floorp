/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// @ts-check

/**
 * @template P
 * @typedef {import("react-redux").ResolveThunks<P>} ResolveThunks<P>
 */

"use strict";
const {
  PureComponent,
  createElement,
} = require("devtools/client/shared/vendor/react");
const {
  div,
  label,
  input,
} = require("devtools/client/shared/vendor/react-dom-factories");
const selectors = require("devtools/client/performance-new/store/selectors");
const actions = require("devtools/client/performance-new/store/actions");
const { connect } = require("devtools/client/shared/vendor/react-redux");

/**
 * @typedef {Object} PresetProps
 * @property {string} presetName
 * @property {boolean} selected
 * @property {import("../@types/perf").PresetDefinition | null} preset
 * @property {(presetName: string) => void} onChange
 */

/**
 * Switch between various profiler presets, which will override the individualized
 * settings for the profiler.
 *
 * @extends {React.PureComponent<PresetProps>}
 */
class Preset extends PureComponent {
  /**
   * Handle the checkbox change.
   * @param {React.ChangeEvent<HTMLInputElement>} event
   */
  onChange = event => {
    this.props.onChange(event.target.value);
  };

  render() {
    const { preset, presetName, selected } = this.props;
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
          checked: selected,
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
}

/**
 * @typedef {Object} StateProps
 * @property {string} selectedPresetName
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
   * @param {string} presetName
   */
  onChange(presetName) {
    const { presets } = this.props;
    this.props.changePreset(presets, presetName);
  }

  render() {
    const { presets, selectedPresetName } = this.props;

    return div(
      { className: "perf-presets" },
      Object.entries(presets).map(([presetName, preset]) =>
        createElement(Preset, {
          key: presetName,
          presetName,
          preset,
          selected: presetName === selectedPresetName,
          onChange: this.onChange,
        })
      ),
      createElement(Preset, {
        key: "custom",
        presetName: "custom",
        selected: selectedPresetName == "custom",
        preset: null,
        onChange: this.onChange,
      })
    );
  }
}

/**
 * @param {StoreState} state
 * @returns {StateProps}
 */
function mapStateToProps(state) {
  return {
    selectedPresetName: selectors.getPresetName(state),
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
