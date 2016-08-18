/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {L10N} = require("devtools/client/performance/modules/global");
const {DOM, createClass} = require("devtools/client/shared/vendor/react");
const {div, button} = DOM;

module.exports = createClass({
  displayName: "Recording Controls",

  /**
   * Manually handle the "checked" and "locked" attributes, as the DOM element won't
   * change by just by changing the checked attribute through React.
   */
  componentDidUpdate() {
    if (this.props.isRecording) {
      this._recordButton.setAttribute("checked", true);
    } else {
      this._recordButton.removeAttribute("checked");
    }

    if (this.props.isLocked) {
      this._recordButton.setAttribute("locked", true);
    } else {
      this._recordButton.removeAttribute("locked");
    }
  },

  render() {
    let {
      onClearButtonClick,
      onRecordButtonClick,
      onImportButtonClick,
      isRecording,
      isLocked
    } = this.props;

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
            className: "devtools-button record-button",
            title: L10N.getStr("recordings.start.tooltip"),
            onClick: onRecordButtonClick,
            checked: isRecording,
            ref: (el) => {
              this._recordButton = el;
            },
            locked: isLocked
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
