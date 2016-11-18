/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {L10N} = require("devtools/client/performance/modules/global");
const {DOM, createClass, PropTypes} = require("devtools/client/shared/vendor/react");
const {button} = DOM;

module.exports = createClass({
  displayName: "Recording Button",

  propTypes: {
    onRecordButtonClick: PropTypes.func.isRequired,
    isRecording: PropTypes.bool,
    isLocked: PropTypes.bool
  },

  render() {
    let {
      onRecordButtonClick,
      isRecording,
      isLocked
    } = this.props;

    let classList = ["devtools-button", "record-button"];

    if (isRecording) {
      classList.push("checked");
    }

    return button(
      {
        className: classList.join(" "),
        onClick: onRecordButtonClick,
        "data-standalone": "true",
        "data-text-only": "true",
        disabled: isLocked
      },
      isRecording ? L10N.getStr("recordings.stop") : L10N.getStr("recordings.start")
    );
  }
});
