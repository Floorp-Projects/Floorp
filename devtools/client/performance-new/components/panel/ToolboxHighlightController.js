/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// @ts-check

/**
 * @typedef {Object} StateProps
 * @property {RecordingState} recordingState
 */

/**
 * @typedef {Object} OwnProps
 * @property {any} toolbox
 */

/**
 * @typedef {StateProps & OwnProps} Props
 * @typedef {import("../../@types/perf").State} StoreState
 * @typedef {import("../../@types/perf").RecordingState} RecordingState
 */

"use strict";

const {
  PureComponent,
} = require("resource://devtools/client/shared/vendor/react.js");
const {
  connect,
} = require("resource://devtools/client/shared/vendor/react-redux.js");
const selectors = require("resource://devtools/client/performance-new/store/selectors.js");

/**
 * @extends {React.PureComponent<Props>}
 */
class ToolboxHighlightController extends PureComponent {
  /** @param {Props} prevProps */
  componentDidUpdate(prevProps) {
    const { recordingState, toolbox } = this.props;
    if (recordingState === "recording") {
      toolbox.highlightTool("performance");
    } else if (prevProps.recordingState === "recording") {
      toolbox.unhighlightTool("performance");
    }
  }

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
    recordingState: selectors.getRecordingState(state),
  };
}

module.exports = connect(mapStateToProps)(ToolboxHighlightController);
