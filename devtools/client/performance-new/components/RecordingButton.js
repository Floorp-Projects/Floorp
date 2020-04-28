/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// @ts-check

/**
 * @template P
 * @typedef {import("react-redux").ResolveThunks<P>} ResolveThunks<P>
 */

/**
 * @typedef {Object} StateProps
 * @property {RecordingState} recordingState
 * @property {boolean} isSupportedPlatform
 * @property {boolean} recordingUnexpectedlyStopped
 * @property {PageContext} pageContext
 */

/**
 * @typedef {Object} ThunkDispatchProps
 * @property {typeof actions.startRecording} startRecording
 * @property {typeof actions.getProfileAndStopProfiler} getProfileAndStopProfiler
 * @property {typeof actions.stopProfilerAndDiscardProfile} stopProfilerAndDiscardProfile

 */

/**
 * @typedef {ResolveThunks<ThunkDispatchProps>} DispatchProps
 * @typedef {StateProps & DispatchProps} Props
 * @typedef {import("../@types/perf").RecordingState} RecordingState
 * @typedef {import("../@types/perf").State} StoreState
 * @typedef {import("../@types/perf").PageContext} PageContext
 */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const {
  div,
  button,
  span,
  img,
} = require("devtools/client/shared/vendor/react-dom-factories");
const { connect } = require("devtools/client/shared/vendor/react-redux");
const actions = require("devtools/client/performance-new/store/actions");
const selectors = require("devtools/client/performance-new/store/selectors");

/**
 * This component is not responsible for the full life cycle of recording a profile. It
 * is only responsible for the actual act of stopping and starting recordings. It
 * also reacts to the changes of the recording state from external changes.
 *
 * @extends {React.PureComponent<Props>}
 */
class RecordingButton extends PureComponent {
  render() {
    const {
      startRecording,
      stopProfilerAndDiscardProfile,
      recordingState,
      isSupportedPlatform,
      recordingUnexpectedlyStopped,
      getProfileAndStopProfiler,
    } = this.props;

    if (!isSupportedPlatform) {
      return renderButton({
        label: startRecordingLabel(),
        isPrimary: true,
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
        return renderButton({
          onClick: startRecording,
          isPrimary: true,
          label: startRecordingLabel(),
          additionalMessage: recordingUnexpectedlyStopped
            ? div(null, "The recording was stopped by another tool.")
            : null,
        });

      case "request-to-stop-profiler":
        return renderButton({
          label: "Stopping recording",
          disabled: true,
        });

      case "request-to-get-profile-and-stop-profiler":
        return renderButton({
          label: "Capturing profile",
          disabled: true,
        });

      case "request-to-start-recording":
      case "recording":
        return renderButton({
          label: span(
            null,
            "Capture recording",
            img({
              className: "perf-button-image",
              alt: "",
              /* This icon is actually the "open in new page" icon. */
              src: "chrome://devtools/skin/images/dock-undock.svg",
            })
          ),
          isPrimary: true,
          onClick: getProfileAndStopProfiler,
          disabled: recordingState === "request-to-start-recording",
          additionalButton: {
            label: "Cancel recording",
            onClick: stopProfilerAndDiscardProfile,
          },
        });

      case "locked-by-private-browsing":
        return renderButton({
          label: startRecordingLabel(),
          isPrimary: true,
          disabled: true,
          additionalMessage: `The profiler is disabled when Private Browsing is enabled.
                              Close all Private Windows to re-enable the profiler`,
        });

      default:
        throw new Error("Unhandled recording state");
    }
  }
}

/**
 * @param {{
 *   disabled?: boolean,
 *   label?: React.ReactNode,
 *   onClick?: any,
 *   additionalMessage?: React.ReactNode,
 *   isPrimary?: boolean,
 *   pageContext?: PageContext,
 *   additionalButton?: {
 *     label: string,
 *     onClick: any,
 *   },
 * }} buttonSettings
 */
function renderButton(buttonSettings) {
  const {
    disabled,
    label,
    onClick,
    additionalMessage,
    isPrimary,
    // pageContext,
    additionalButton,
  } = buttonSettings;

  const buttonClass = isPrimary ? "primary" : "default";

  return div(
    { className: "perf-button-container" },
    div(
      null,
      button(
        {
          className: `perf-photon-button perf-photon-button-${buttonClass} perf-button`,
          disabled,
          onClick,
        },
        label
      ),
      additionalButton
        ? button(
            {
              className: `perf-photon-button perf-photon-button-default perf-button`,
              onClick: additionalButton.onClick,
              disabled,
            },
            additionalButton.label
          )
        : null
    ),
    additionalMessage
      ? div({ className: "perf-additional-message" }, additionalMessage)
      : null
  );
}

function startRecordingLabel() {
  return span(
    null,
    "Start recording",
    img({
      className: "perf-button-image",
      alt: "",
      /* This icon is actually the "open in new page" icon. */
      src: "chrome://devtools/skin/images/dock-undock.svg",
    })
  );
}

/**
 * @param {StoreState} state
 * @returns {StateProps}
 */
function mapStateToProps(state) {
  return {
    recordingState: selectors.getRecordingState(state),
    isSupportedPlatform: selectors.getIsSupportedPlatform(state),
    recordingUnexpectedlyStopped: selectors.getRecordingUnexpectedlyStopped(
      state
    ),
    pageContext: selectors.getPageContext(state),
  };
}

/** @type {ThunkDispatchProps} */
const mapDispatchToProps = {
  startRecording: actions.startRecording,
  stopProfilerAndDiscardProfile: actions.stopProfilerAndDiscardProfile,
  getProfileAndStopProfiler: actions.getProfileAndStopProfiler,
};

module.exports = connect(mapStateToProps, mapDispatchToProps)(RecordingButton);
