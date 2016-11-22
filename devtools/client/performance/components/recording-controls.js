/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {L10N} = require("devtools/client/performance/modules/global");
const {DOM, createClass, PropTypes} = require("devtools/client/shared/vendor/react");
const {div, button} = DOM;

module.exports = createClass({
  displayName: "Recording Controls",

  propTypes: {
    onClearButtonClick: PropTypes.func.isRequired,
    onRecordButtonClick: PropTypes.func.isRequired,
    onImportButtonClick: PropTypes.func.isRequired,
    isRecording: PropTypes.bool,
    isLocked: PropTypes.bool
  },

  render() {
    let {
      onClearButtonClick,
      onRecordButtonClick,
      onImportButtonClick,
      isRecording,
      isLocked
    } = this.props;

    let recordButtonClassList = ["devtools-button", "record-button"];

    if (isRecording) {
      recordButtonClassList.push("checked");
    }

    return (
      div({ className: "devtools-toolbar" },
        div({ className: "toolbar-group" },
          button({
            id: "clear-button",
            className: "devtools-button",
            title: L10N.getStr("recordings.clear.tooltip"),
            onClick: onClearButtonClick
          }),
          button({
            id: "main-record-button",
            className: recordButtonClassList.join(" "),
            disabled: isLocked,
            title: L10N.getStr("recordings.start.tooltip"),
            onClick: onRecordButtonClick
          }),
          button({
            id: "import-button",
            className: "devtools-button",
            title: L10N.getStr("recordings.import.tooltip"),
            onClick: onImportButtonClick
          })
        )
      )
    );
  }
});
