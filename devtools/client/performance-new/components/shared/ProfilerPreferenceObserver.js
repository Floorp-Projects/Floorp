/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// @ts-check

/**
 * @template P
 * @typedef {import("react-redux").ResolveThunks<P>} ResolveThunks<P>
 */

/**
 * @typedef {import("../../@types/perf").State} StoreState
 */

/**
 * @typedef {Object} StateProps
 * @property {import("../../@types/perf").RecordingSettings} recordingSettingsFromRedux
 * @property {import("../../@types/perf").PageContext} pageContext
 * @property {string[]} supportedFeatures
 */

/**
 * @typedef {Object} ThunkDispatchProps
 * @property {typeof actions.updateSettingsFromPreferences} updateSettingsFromPreferences
 */

/**
 * @typedef {ResolveThunks<ThunkDispatchProps>} DispatchProps
 * @typedef {StateProps & DispatchProps} Props
 */

"use strict";

// These functions live in a JSM file so that this can be used both by this
// CommonJS DevTools environment and the popup which isn't in such a context.
const {
  getRecordingSettings,
  setRecordingSettings,
  addPrefObserver,
  removePrefObserver,
} = ChromeUtils.importESModule(
  "resource://devtools/client/performance-new/shared/background.sys.mjs"
);
const {
  PureComponent,
} = require("resource://devtools/client/shared/vendor/react.js");
const {
  connect,
} = require("resource://devtools/client/shared/vendor/react-redux.js");

const selectors = require("resource://devtools/client/performance-new/store/selectors.js");
const actions = require("resource://devtools/client/performance-new/store/actions.js");

/**
 * This component mirrors the settings in the redux store and the preferences in
 * Firefox.
 *
 * @extends {React.PureComponent<Props>}
 */
class ProfilerPreferenceObserver extends PureComponent {
  componentDidMount() {
    this._updateSettingsFromPreferences();
    addPrefObserver(this._updateSettingsFromPreferences);
  }

  componentDidUpdate() {
    const { recordingSettingsFromRedux, pageContext } = this.props;
    setRecordingSettings(pageContext, recordingSettingsFromRedux);
  }

  componentWillUnmount() {
    removePrefObserver(this._updateSettingsFromPreferences);
  }

  _updateSettingsFromPreferences = () => {
    const { updateSettingsFromPreferences, pageContext, supportedFeatures } =
      this.props;

    const recordingSettingsFromPrefs = getRecordingSettings(
      pageContext,
      supportedFeatures
    );
    updateSettingsFromPreferences(recordingSettingsFromPrefs);
  };

  render() {
    return null;
  }
}

/**
 * @param {StoreState} state
 * @returns {StateProps}
 */
function mapStateToProps(state) {
  return {
    recordingSettingsFromRedux: selectors.getRecordingSettings(state),
    pageContext: selectors.getPageContext(state),
    supportedFeatures: selectors.getSupportedFeatures(state),
  };
}

const mapDispatchToProps = {
  updateSettingsFromPreferences: actions.updateSettingsFromPreferences,
};

module.exports = connect(
  mapStateToProps,
  mapDispatchToProps
)(ProfilerPreferenceObserver);
