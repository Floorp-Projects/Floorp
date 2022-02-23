/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// @ts-check

/**
 * @typedef {import("../@types/perf").PerfFront} PerfFront
 * @typedef {import("../@types/perf").RecordingState} RecordingState
 * @typedef {import("../@types/perf").State} StoreState
 * @typedef {import("../@types/perf").RootTraits} RootTraits
 * @typedef {import("../@types/perf").PanelWindow} PanelWindow
 */

/**
 * @template P
 * @typedef {import("react-redux").ResolveThunks<P>} ResolveThunks<P>
 */

/**
 * @typedef {Object} StateProps
 * @property {RecordingState} recordingState
 * @property {boolean?} isSupportedPlatform
 */

/**
 * @typedef {Object} ThunkDispatchProps
 * @property {typeof actions.reportProfilerReady} reportProfilerReady
 * @property {typeof actions.reportProfilerStarted} reportProfilerStarted
 * @property {typeof actions.reportProfilerStopped} reportProfilerStopped
 * @property {typeof actions.reportPrivateBrowsingStarted} reportPrivateBrowsingStarted
 * @property {typeof actions.reportPrivateBrowsingStopped} reportPrivateBrowsingStopped
 */

/**
 * @typedef {Object} OwnProps
 * @property {PerfFront} perfFront
 * @property {RootTraits} traits
 */

/**
 * @typedef {ResolveThunks<ThunkDispatchProps>} DispatchProps
 * @typedef {StateProps & DispatchProps & OwnProps} Props
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
  componentDidMount() {
    const {
      perfFront,
      isSupportedPlatform,
      reportProfilerReady,
      reportProfilerStarted,
      reportProfilerStopped,
      reportPrivateBrowsingStarted,
      reportPrivateBrowsingStopped,
      traits: { noDisablingOnPrivateBrowsing },
    } = this.props;

    if (!isSupportedPlatform) {
      return;
    }

    // Ask for the initial state of the profiler.
    Promise.all([
      perfFront.isActive(),
      noDisablingOnPrivateBrowsing
        ? false
        : perfFront.isLockedForPrivateBrowsing(),
    ]).then(([isActive, isLockedForPrivateBrowsing]) => {
      reportProfilerReady(isActive, isLockedForPrivateBrowsing);
    });

    // Handle when the profiler changes state. It might be us, it might be someone else.
    this.props.perfFront.on("profiler-started", reportProfilerStarted);
    this.props.perfFront.on("profiler-stopped", reportProfilerStopped);

    if (!noDisablingOnPrivateBrowsing) {
      // @backward-compat { version 98 }
      // These events are not used anymore in Firefox 98 and above. They can be
      // removed along with the rest of the functionality once 98 hits release.
      this.props.perfFront.on(
        "profile-locked-by-private-browsing",
        reportPrivateBrowsingStarted
      );
      this.props.perfFront.on(
        "profile-unlocked-from-private-browsing",
        reportPrivateBrowsingStopped
      );
    }
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
    recordingState: selectors.getRecordingState(state),
    isSupportedPlatform: selectors.getIsSupportedPlatform(state),
  };
}

/** @type {ThunkDispatchProps} */
const mapDispatchToProps = {
  reportProfilerReady: actions.reportProfilerReady,
  reportProfilerStarted: actions.reportProfilerStarted,
  reportProfilerStopped: actions.reportProfilerStopped,
  reportPrivateBrowsingStarted: actions.reportPrivateBrowsingStarted,
  reportPrivateBrowsingStopped: actions.reportPrivateBrowsingStopped,
};

module.exports = connect(
  mapStateToProps,
  mapDispatchToProps
)(ProfilerEventHandling);
