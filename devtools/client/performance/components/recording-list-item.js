/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {DOM, createClass, PropTypes} = require("devtools/client/shared/vendor/react");
const {div, li, span, button} = DOM;
const {L10N} = require("devtools/client/performance/modules/global");

module.exports = createClass({
  displayName: "Recording List Item",

  propTypes: {
    label: PropTypes.string.isRequired,
    duration: PropTypes.string,
    onSelect: PropTypes.func.isRequired,
    onSave: PropTypes.func.isRequired,
    isLoading: PropTypes.bool,
    isSelected: PropTypes.bool,
    isRecording: PropTypes.bool
  },

  render() {
    const {
      label,
      duration,
      onSelect,
      onSave,
      isLoading,
      isSelected,
      isRecording
    } = this.props;

    const className = `recording-list-item ${isSelected ? "selected" : ""}`;

    let durationText;
    if (isLoading) {
      durationText = L10N.getStr("recordingsList.loadingLabel");
    } else if (isRecording) {
      durationText = L10N.getStr("recordingsList.recordingLabel");
    } else {
      durationText = L10N.getFormatStr("recordingsList.durationLabel", duration);
    }

    return (
      li({ className, onClick: onSelect },
        div({ className: "recording-list-item-label" },
          label
        ),
        div({ className: "recording-list-item-footer" },
          span({ className: "recording-list-item-duration" }, durationText),
          button({ className: "recording-list-item-save", onClick: onSave },
            L10N.getStr("recordingsList.saveLabel")
          )
        )
      )
    );
  }
});
