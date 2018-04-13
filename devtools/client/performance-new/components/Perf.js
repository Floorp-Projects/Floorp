/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { PureComponent, createFactory } = require("devtools/client/shared/vendor/react");
const { connect } = require("devtools/client/shared/vendor/react-redux");
const { div } = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const RecordingButton = createFactory(require("devtools/client/performance-new/components/RecordingButton.js"));
const Settings = createFactory(require("devtools/client/performance-new/components/Settings.js"));
const Description = createFactory(require("devtools/client/performance-new/components/Description.js"));
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
 * This is the top level component for initializing the performance recording panel.
 * It has two jobs:
 *
 * 1) It manages state changes for the performance recording. e.g. If the profiler
 * suddenly becomes unavailable, it needs to react to those changes, and update the
 * recordingState in the store.
 *
 * 2) It mounts all of the sub components, but is itself very light on actual
 * markup for presentation.
 */
class Perf extends PureComponent {
  static get propTypes() {
    return {
      // StateProps:
      perfFront: PropTypes.object.isRequired,
      recordingState: PropTypes.string.isRequired,
      isSupportedPlatform: PropTypes.bool,

      // DispatchProps:
      changeRecordingState: PropTypes.func.isRequired,
      reportProfilerReady: PropTypes.func.isRequired
    };
  }

  constructor(props) {
    super(props);
    this.handleProfilerStarting = this.handleProfilerStarting.bind(this);
    this.handleProfilerStopping = this.handleProfilerStopping.bind(this);
    this.handlePrivateBrowsingStarting = this.handlePrivateBrowsingStarting.bind(this);
    this.handlePrivateBrowsingEnding = this.handlePrivateBrowsingEnding.bind(this);
  }

  componentDidMount() {
    const { perfFront, reportProfilerReady } = this.props;

    // Ask for the initial state of the profiler.
    Promise.all([
      perfFront.isActive(),
      perfFront.isSupportedPlatform(),
      perfFront.isLockedForPrivateBrowsing(),
    ]).then((results) => {
      const [
        isActive,
        isSupportedPlatform,
        isLockedForPrivateBrowsing
      ] = results;

      let recordingState = this.props.recordingState;
      // It's theoretically possible we got an event that already let us know about
      // the current state of the profiler.
      if (recordingState === NOT_YET_KNOWN && isSupportedPlatform) {
        if (isLockedForPrivateBrowsing) {
          recordingState = LOCKED_BY_PRIVATE_BROWSING;
        } else {
          recordingState = isActive
            ? OTHER_IS_RECORDING
            : AVAILABLE_TO_RECORD;
        }
      }
      reportProfilerReady(isSupportedPlatform, recordingState);
    });

    // Handle when the profiler changes state. It might be us, it might be someone else.
    this.props.perfFront.on("profiler-started", this.handleProfilerStarting);
    this.props.perfFront.on("profiler-stopped", this.handleProfilerStopping);
    this.props.perfFront.on("profile-locked-by-private-browsing",
      this.handlePrivateBrowsingStarting);
    this.props.perfFront.on("profile-unlocked-from-private-browsing",
      this.handlePrivateBrowsingEnding);
  }

  componentWillUnmount() {
    switch (this.props.recordingState) {
      case NOT_YET_KNOWN:
      case AVAILABLE_TO_RECORD:
      case REQUEST_TO_STOP_PROFILER:
      case REQUEST_TO_GET_PROFILE_AND_STOP_PROFILER:
      case LOCKED_BY_PRIVATE_BROWSING:
      case OTHER_IS_RECORDING:
        // Do nothing for these states.
        break;

      case RECORDING:
      case REQUEST_TO_START_RECORDING:
        this.props.perfFront.stopProfilerAndDiscardProfile();
        break;

      default:
        throw new Error("Unhandled recording state.");
    }
  }

  handleProfilerStarting() {
    const { changeRecordingState, recordingState } = this.props;
    switch (recordingState) {
      case NOT_YET_KNOWN:
        // We couldn't have started it yet, so it must have been someone
        // else. (fallthrough)
      case AVAILABLE_TO_RECORD:
        // We aren't recording, someone else started it up. (fallthrough)
      case REQUEST_TO_STOP_PROFILER:
        // We requested to stop the profiler, but someone else already started
        // it up. (fallthrough)
      case REQUEST_TO_GET_PROFILE_AND_STOP_PROFILER:
        // Someone re-started the profiler while we were asking for the completed
        // profile.

        changeRecordingState(OTHER_IS_RECORDING);
        break;

      case REQUEST_TO_START_RECORDING:
        // Wait for the profiler to tell us that it has started.
        changeRecordingState(RECORDING);
        break;

      case LOCKED_BY_PRIVATE_BROWSING:
      case OTHER_IS_RECORDING:
      case RECORDING:
        // These state cases don't make sense to happen, and means we have a logical
        // fallacy somewhere.
        throw new Error(
          "The profiler started recording, when it shouldn't have " +
          `been able to. Current state: "${recordingState}"`);
      default:
        throw new Error("Unhandled recording state");
    }
  }

  handleProfilerStopping() {
    const { changeRecordingState, recordingState } = this.props;
    switch (recordingState) {
      case NOT_YET_KNOWN:
      case REQUEST_TO_GET_PROFILE_AND_STOP_PROFILER:
      case REQUEST_TO_STOP_PROFILER:
      case OTHER_IS_RECORDING:
        changeRecordingState(AVAILABLE_TO_RECORD);
        break;

      case REQUEST_TO_START_RECORDING:
        // Highly unlikely, but someone stopped the recorder, this is fine.
        // Do nothing (fallthrough).
      case LOCKED_BY_PRIVATE_BROWSING:
        // The profiler is already locked, so we know about this already.
        break;

      case RECORDING:
        changeRecordingState(
          AVAILABLE_TO_RECORD,
          { didRecordingUnexpectedlyStopped: true }
        );
        break;

      case AVAILABLE_TO_RECORD:
        throw new Error(
          "The profiler stopped recording, when it shouldn't have been able to.");
      default:
        throw new Error("Unhandled recording state");
    }
  }

  handlePrivateBrowsingStarting() {
    const { recordingState, changeRecordingState } = this.props;

    switch (recordingState) {
      case REQUEST_TO_GET_PROFILE_AND_STOP_PROFILER:
        // This one is a tricky case. Go ahead and act like nothing went wrong, maybe
        // it will resolve correctly? (fallthrough)
      case REQUEST_TO_STOP_PROFILER:
      case AVAILABLE_TO_RECORD:
      case OTHER_IS_RECORDING:
      case NOT_YET_KNOWN:
        changeRecordingState(LOCKED_BY_PRIVATE_BROWSING);
        break;

      case REQUEST_TO_START_RECORDING:
      case RECORDING:
        changeRecordingState(
          LOCKED_BY_PRIVATE_BROWSING,
          { didRecordingUnexpectedlyStopped: false }
        );
        break;

      case LOCKED_BY_PRIVATE_BROWSING:
        // Do nothing
        break;

      default:
        throw new Error("Unhandled recording state");
    }
  }

  handlePrivateBrowsingEnding() {
    // No matter the state, go ahead and set this as ready to record. This should
    // be the only logical state to go into.
    this.props.changeRecordingState(AVAILABLE_TO_RECORD);
  }

  render() {
    const { isSupportedPlatform } = this.props;

    if (isSupportedPlatform === null) {
      // We don't know yet if this is a supported platform, wait for a response.
      return null;
    }

    return div(
      { className: "perf" },
      RecordingButton(),
      Settings(),
      Description(),
    );
  }
}

function mapStateToProps(state) {
  return {
    perfFront: selectors.getPerfFront(state),
    recordingState: selectors.getRecordingState(state),
    isSupportedPlatform: selectors.getIsSupportedPlatform(state),
  };
}

const mapDispatchToProps = {
  changeRecordingState: actions.changeRecordingState,
  reportProfilerReady: actions.reportProfilerReady,
};

module.exports = connect(mapStateToProps, mapDispatchToProps)(Perf);
