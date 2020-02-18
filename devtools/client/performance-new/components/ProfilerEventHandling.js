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
 * @property {PerfFront} perfFront
 * @property {RecordingState} recordingState
 * @property {boolean?} isSupportedPlatform
 * @property {PageContext} pageContext
 * @property {string | null} promptEnvRestart
 */

/**
 * @typedef {Object} ThunkDispatchProps
 * @property {typeof actions.changeRecordingState} changeRecordingState
 * @property {typeof actions.reportProfilerReady} reportProfilerReady
 */

/**
 * @typedef {ResolveThunks<ThunkDispatchProps>} DispatchProps
 * @typedef {StateProps & DispatchProps} Props
 * @typedef {import("../@types/perf").PerfFront} PerfFront
 * @typedef {import("../@types/perf").RecordingState} RecordingState
 * @typedef {import("../@types/perf").State} StoreState
 * @typedef {import("../@types/perf").PageContext} PageContext
 */

/**
 * @typedef {import("../@types/perf").PanelWindow} PanelWindow
 */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const { connect } = require("devtools/client/shared/vendor/react-redux");
const actions = require("devtools/client/performance-new/store/actions");
const selectors = require("devtools/client/performance-new/store/selectors");

/**
 * This component state changes for the performance recording. e.g. If the profiler
 * suddenly becomes unavailable, it needs to react to those changes, and update the
 * recordingState in the store.
 *
 * @extends {React.PureComponent<Props>}
 */
class ProfilerEventHandling extends PureComponent {
  /** @param {Props} props */
  constructor(props) {
    super(props);
    this.handleProfilerStarting = this.handleProfilerStarting.bind(this);
    this.handleProfilerStopping = this.handleProfilerStopping.bind(this);
    this.handlePrivateBrowsingStarting = this.handlePrivateBrowsingStarting.bind(
      this
    );
    this.handlePrivateBrowsingEnding = this.handlePrivateBrowsingEnding.bind(
      this
    );
  }

  componentDidMount() {
    const { perfFront, reportProfilerReady } = this.props;

    // Ask for the initial state of the profiler.
    Promise.all([
      perfFront.isActive(),
      perfFront.isSupportedPlatform(),
      perfFront.isLockedForPrivateBrowsing(),
    ]).then(results => {
      const [
        isActive,
        isSupportedPlatform,
        isLockedForPrivateBrowsing,
      ] = results;

      let recordingState = this.props.recordingState;
      // It's theoretically possible we got an event that already let us know about
      // the current state of the profiler.
      if (recordingState === "not-yet-known" && isSupportedPlatform) {
        if (isLockedForPrivateBrowsing) {
          recordingState = "locked-by-private-browsing";
        } else if (isActive) {
          recordingState = "recording";
        } else {
          recordingState = "available-to-record";
        }
      }
      reportProfilerReady(isSupportedPlatform, recordingState);

      // If this component is inside the popup, then report it being ready so that
      // it will show. This defers the initial visibility of the popup until the
      // React components have fully rendered, and thus there is no annoying "blip"
      // to the screen when the page goes from fully blank, to showing the content.
      /** @type {any} */
      const anyWindow = window;
      /** @type {PanelWindow} - Coerce the window into the PanelWindow. */
      const { gReportReady } = anyWindow;
      if (gReportReady) {
        gReportReady();
      }
    });

    // Handle when the profiler changes state. It might be us, it might be someone else.
    this.props.perfFront.on("profiler-started", this.handleProfilerStarting);
    this.props.perfFront.on("profiler-stopped", this.handleProfilerStopping);
    this.props.perfFront.on(
      "profile-locked-by-private-browsing",
      this.handlePrivateBrowsingStarting
    );
    this.props.perfFront.on(
      "profile-unlocked-from-private-browsing",
      this.handlePrivateBrowsingEnding
    );
  }

  componentWillUnmount() {
    switch (this.props.recordingState) {
      case "not-yet-known":
      case "available-to-record":
      case "request-to-stop-profiler":
      case "request-to-get-profile-and-stop-profiler":
      case "locked-by-private-browsing":
        // Do nothing for these states.
        break;

      case "recording":
      case "request-to-start-recording":
        this.props.perfFront.stopProfilerAndDiscardProfile();
        break;

      default:
        throw new Error("Unhandled recording state.");
    }
  }

  handleProfilerStarting() {
    const { changeRecordingState, recordingState } = this.props;
    switch (recordingState) {
      case "not-yet-known":
      // We couldn't have started it yet, so it must have been someone
      // else. (fallthrough)
      case "available-to-record":
      // We aren't recording, someone else started it up. (fallthrough)
      case "request-to-stop-profiler":
      // We requested to stop the profiler, but someone else already started
      // it up. (fallthrough)
      case "request-to-get-profile-and-stop-profiler":
        changeRecordingState("recording");
        break;

      case "request-to-start-recording":
        // Wait for the profiler to tell us that it has started.
        changeRecordingState("recording");
        break;

      case "locked-by-private-browsing":
      case "recording":
        // These state cases don't make sense to happen, and means we have a logical
        // fallacy somewhere.
        throw new Error(
          "The profiler started recording, when it shouldn't have " +
            `been able to. Current state: "${recordingState}"`
        );
      default:
        throw new Error("Unhandled recording state");
    }
  }

  handleProfilerStopping() {
    const { changeRecordingState, recordingState } = this.props;
    switch (recordingState) {
      case "not-yet-known":
      case "request-to-get-profile-and-stop-profiler":
      case "request-to-stop-profiler":
        changeRecordingState("available-to-record");
        break;

      case "request-to-start-recording":
      // Highly unlikely, but someone stopped the recorder, this is fine.
      // Do nothing (fallthrough).
      case "locked-by-private-browsing":
        // The profiler is already locked, so we know about this already.
        break;

      case "recording":
        changeRecordingState("available-to-record", {
          didRecordingUnexpectedlyStopped: true,
        });
        break;

      case "available-to-record":
        throw new Error(
          "The profiler stopped recording, when it shouldn't have been able to."
        );
      default:
        throw new Error("Unhandled recording state");
    }
  }

  handlePrivateBrowsingStarting() {
    const { recordingState, changeRecordingState } = this.props;

    switch (recordingState) {
      case "request-to-get-profile-and-stop-profiler":
      // This one is a tricky case. Go ahead and act like nothing went wrong, maybe
      // it will resolve correctly? (fallthrough)
      case "request-to-stop-profiler":
      case "available-to-record":
      case "not-yet-known":
        changeRecordingState("locked-by-private-browsing");
        break;

      case "request-to-start-recording":
      case "recording":
        changeRecordingState("locked-by-private-browsing", {
          didRecordingUnexpectedlyStopped: false,
        });
        break;

      case "locked-by-private-browsing":
        // Do nothing
        break;

      default:
        throw new Error("Unhandled recording state");
    }
  }

  handlePrivateBrowsingEnding() {
    // No matter the state, go ahead and set this as ready to record. This should
    // be the only logical state to go into.
    this.props.changeRecordingState("available-to-record");
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
    perfFront: selectors.getPerfFront(state),
    recordingState: selectors.getRecordingState(state),
    isSupportedPlatform: selectors.getIsSupportedPlatform(state),
    pageContext: selectors.getPageContext(state),
    promptEnvRestart: selectors.getPromptEnvRestart(state),
  };
}

/** @type {ThunkDispatchProps} */
const mapDispatchToProps = {
  changeRecordingState: actions.changeRecordingState,
  reportProfilerReady: actions.reportProfilerReady,
};

module.exports = connect(
  mapStateToProps,
  mapDispatchToProps
)(ProfilerEventHandling);
