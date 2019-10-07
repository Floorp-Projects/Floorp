/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const {
  div,
  button,
  span,
  img,
} = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { connect } = require("devtools/client/shared/vendor/react-redux");
const actions = require("devtools/client/performance-new/store/actions");
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
      isPopup: PropTypes.bool.isRequired,

      // DispatchProps
      startRecording: PropTypes.func.isRequired,
      getProfileAndStopProfiler: PropTypes.func.isRequired,
      stopProfilerAndDiscardProfile: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);
    this._getProfileAndStopProfiler = () =>
      this.props.getProfileAndStopProfiler(window);
  }

  renderButton(buttonSettings) {
    const {
      disabled,
      label,
      onClick,
      additionalMessage,
      isPrimary,
      isPopup,
      additionalButton,
    } = buttonSettings;

    const nbsp = "\u00A0";
    const showAdditionalMessage = isPopup && additionalMessage;
    const buttonClass = isPrimary ? "primary" : "default";

    return div(
      { className: "perf-button-container" },
      showAdditionalMessage
        ? div(
            { className: "perf-additional-message" },
            additionalMessage || nbsp
          )
        : null,
      div(
        null,
        button(
          {
            className: `perf-photon-button perf-photon-button-${buttonClass} perf-button`,
            "data-standalone": true,
            disabled,
            onClick,
          },
          label
        ),
        additionalButton
          ? button(
              {
                className: `perf-photon-button perf-photon-button-default perf-button`,
                "data-standalone": true,
                onClick: additionalButton.onClick,
                disabled,
              },
              additionalButton.label
            )
          : null
      )
    );
  }

  render() {
    const {
      startRecording,
      stopProfilerAndDiscardProfile,
      recordingState,
      isSupportedPlatform,
      recordingUnexpectedlyStopped,
    } = this.props;

    if (!isSupportedPlatform) {
      return this.renderButton({
        label: "Start recording",
        disabled: true,
        additionalMessage:
          "Your platform is not supported. The Gecko Profiler only " +
          "supports Tier-1 platforms.",
      });
    }

    // TODO - L10N all of the messages. Bug 1418056
    switch (recordingState) {
      case "not-yet-known":
        return null;

      case "available-to-record":
        return this.renderButton({
          onClick: startRecording,
          label: span(
            null,
            img({
              className: "perf-button-image",
              src: "chrome://devtools/skin/images/tool-profiler.svg",
            }),
            "Start recording"
          ),
          additionalMessage: recordingUnexpectedlyStopped
            ? div(null, "The recording was stopped by another tool.")
            : null,
        });

      case "request-to-stop-profiler":
        return this.renderButton({
          label: "Stopping recording",
          disabled: true,
        });

      case "request-to-get-profile-and-stop-profiler":
        return this.renderButton({
          label: "Capturing profile",
          disabled: true,
        });

      case "request-to-start-recording":
      case "recording":
        return this.renderButton({
          label: "Capture recording",
          isPrimary: true,
          onClick: this._getProfileAndStopProfiler,
          disabled: recordingState === "request-to-start-recording",
          additionalButton: {
            label: "Cancel recording",
            onClick: stopProfilerAndDiscardProfile,
          },
        });

      case "other-is-recording":
        return this.renderButton({
          label: "Stop and discard the other recording",
          onClick: stopProfilerAndDiscardProfile,
          additionalMessage: "Another tool is currently recording.",
        });

      case "locked-by-private-browsing":
        return this.renderButton({
          label: span(
            null,
            img({
              className: "perf-button-image",
              src: "chrome://devtools/skin/images/tool-profiler.svg",
            }),
            "Start recording"
          ),
          disabled: true,
          additionalMessage: `The profiler is disabled when Private Browsing is enabled.
                              Close all Private Windows to re-enable the profiler`,
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
    recordingUnexpectedlyStopped: selectors.getRecordingUnexpectedlyStopped(
      state
    ),
    isPopup: selectors.getIsPopup(state),
  };
}

const mapDispatchToProps = {
  startRecording: actions.startRecording,
  stopProfilerAndDiscardProfile: actions.stopProfilerAndDiscardProfile,
  getProfileAndStopProfiler: actions.getProfileAndStopProfiler,
};

module.exports = connect(
  mapStateToProps,
  mapDispatchToProps
)(RecordingButton);
