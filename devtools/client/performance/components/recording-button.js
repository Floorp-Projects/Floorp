/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {L10N} = require("devtools/client/performance/modules/global");
const {DOM, createClass} = require("devtools/client/shared/vendor/react");
const {button} = DOM;

module.exports = createClass({
  displayName: "Recording Button",

  render() {
    let {
      onRecordButtonClick,
      isRecording
    } = this.props;

    return button(
      {
        className: "devtools-toolbarbutton record-button",
        onClick: onRecordButtonClick,
        "data-standalone": "true",
        "data-text-only": "true",
      },
      isRecording ? L10N.getStr("recordings.stop") : L10N.getStr("recordings.start")
    );
  }
});
