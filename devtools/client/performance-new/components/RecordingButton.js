/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const { div, button, span, img } = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { connect } = require("devtools/client/shared/vendor/react-redux");
const actions = require("devtools/client/performance-new/store/actions");
const { recordingState: {
  NOT_YET_KNOWN,
  AVAILABLE_TO_RECORD,
  REQUEST_TO_START_RECORDING,
  REQUEST_TO_GET_PROFILE_AND_STOP_PROFILER,
  REQUEST_TO_STOP_PROFILER,
  RECORDING,
  OTHER_IS_RECORDING,
  LOCKED_BY_PRIVATE_BROWSING,
}} = require("devtools/client/performance-new/utils");
const selectors = require("devtools/client/performance-new/store/selectors");

/**
 * This component is not responsible for the full life cycle of recording a profile. It
 * is only responsible for the actual act of stopping and starting recordings. It
 * also reacts to the changes of the recording state from external changes.
 */
class RecordingButton extends PureComponent {
  static get propTypes() {
    return {
      // StateProps
      recordingState: PropTypes.string.isRequired,
      isSupportedPlatform: PropTypes.bool,
      recordingUnexpectedlyStopped: PropTypes.bool.isRequired,

      // DispatchProps
      startRecording: PropTypes.func.isRequired,
      getProfileAndStopProfiler: PropTypes.func.isRequired,
      stopProfilerAndDiscardProfile: PropTypes.func.isRequired,
    };
  }

  renderButton(buttonSettings) {
    const { disabled, label, onClick, additionalMessage } = buttonSettings;
    const nbsp = "\u00A0";

    return div(
      { className: "perf-button-container" },
      div({ className: "perf-additional-message" }, additionalMessage || nbsp),
      div(
        null,
        button(
          {
            className: "devtools-button perf-button",
            "data-standalone": true,
            disabled,
            onClick
          },
          label
        )
      )
    );
  }

  render() {
    const {
      startRecording,
      getProfileAndStopProfiler,
      stopProfilerAndDiscardProfile,
      recordingState,
      isSupportedPlatform,
      recordingUnexpectedlyStopped
    } = this.props;

    if (!isSupportedPlatform) {
      return this.renderButton({
        label: "Start recording",
        disabled: true,
        additionalMessage: "Your platform is not supported. The Gecko Profiler only " +
                           "supports Tier-1 platforms."
      });
    }

    // TODO - L10N all of the messages. Bug 1418056
    switch (recordingState) {
      case NOT_YET_KNOWN:
        return null;

      case AVAILABLE_TO_RECORD:
        return this.renderButton({
          onClick: startRecording,
          label: span(
            null,
            img({
              className: "perf-button-image",
              src: "chrome://devtools/skin/images/tool-profiler.svg"
            }),
            "Start recording",
          ),
          additionalMessage: recordingUnexpectedlyStopped
            ? div(null, "The recording was stopped by another tool.")
            : null
        });

      case REQUEST_TO_STOP_PROFILER:
        return this.renderButton({
          label: "Stopping the recording",
          disabled: true
        });

      case REQUEST_TO_GET_PROFILE_AND_STOP_PROFILER:
        return this.renderButton({
          label: "Stopping the recording, and capturing the profile",
          disabled: true
        });

      case REQUEST_TO_START_RECORDING:
      case RECORDING:
        return this.renderButton({
          label: "Stop and grab the recording",
          onClick: getProfileAndStopProfiler,
          disabled: recordingState === REQUEST_TO_START_RECORDING
        });

      case OTHER_IS_RECORDING:
        return this.renderButton({
          label: "Stop and discard the other recording",
          onClick: stopProfilerAndDiscardProfile,
          additionalMessage: "Another tool is currently recording."
        });

      case LOCKED_BY_PRIVATE_BROWSING:
        return this.renderButton({
          label: span(
            null,
            img({
              className: "perf-button-image",
              src: "chrome://devtools/skin/images/tool-profiler.svg"
            }),
            "Start recording",
          ),
          disabled: true,
          additionalMessage: `The profiler is disabled when Private Browsing is enabled.
                              Close all Private Windows to re-enable the profiler`
        });

      default:
        throw new Error("Unhandled recording state");
    }
  }
}

function mapStateToProps(state) {
  return {
    recordingState: selectors.getRecordingState(state),
    isSupportedPlatform: selectors.getIsSupportedPlatform(state),
    recordingUnexpectedlyStopped: selectors.getRecordingUnexpectedlyStopped(state),
  };
}

const mapDispatchToProps = {
  startRecording: actions.startRecording,
  stopProfilerAndDiscardProfile: actions.stopProfilerAndDiscardProfile,
  getProfileAndStopProfiler: actions.getProfileAndStopProfiler,
};

module.exports = connect(mapStateToProps, mapDispatchToProps)(RecordingButton);
