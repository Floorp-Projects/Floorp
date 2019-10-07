/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  PureComponent,
  createFactory,
} = require("devtools/client/shared/vendor/react");
const { connect } = require("devtools/client/shared/vendor/react-redux");
const { div } = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const RecordingButton = createFactory(
  require("devtools/client/performance-new/components/RecordingButton.js")
);
const Settings = createFactory(
  require("devtools/client/performance-new/components/Settings.js")
);
const Description = createFactory(
  require("devtools/client/performance-new/components/Description.js")
);
const actions = require("devtools/client/performance-new/store/actions");
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
      isPopup: PropTypes.bool,

      // DispatchProps:
      changeRecordingState: PropTypes.func.isRequired,
      reportProfilerReady: PropTypes.func.isRequired,
    };
  }

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
    const { perfFront, reportProfilerReady, isPopup } = this.props;

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
          // The popup is a global control for the recording, so allow it to take
          // control of it.
          recordingState = isPopup ? "recording" : "other-is-recording";
        } else {
          recordingState = "available-to-record";
        }
      }
      reportProfilerReady(isSupportedPlatform, recordingState);

      // If this component is inside the popup, then report it being ready so that
      // it will show. This defers the initial visibility of the popup until the
      // React components have fully rendered, and thus there is no annoying "blip"
      // to the screen when the page goes from fully blank, to showing the content.
      if (window.gReportReady) {
        window.gReportReady();
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
      case "other-is-recording":
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
    const { changeRecordingState, recordingState, isPopup } = this.props;
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
        if (isPopup) {
          // The profiler popup doesn't care who is recording. It will take control
          // of it.
          changeRecordingState("recording");
        } else {
          // Someone re-started the profiler while we were asking for the completed
          // profile.
          changeRecordingState("other-is-recording");
        }
        break;

      case "request-to-start-recording":
        // Wait for the profiler to tell us that it has started.
        changeRecordingState("recording");
        break;

      case "locked-by-private-browsing":
      case "other-is-recording":
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
      case "other-is-recording":
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
      case "other-is-recording":
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
    const { isSupportedPlatform, isPopup } = this.props;

    if (isSupportedPlatform === null) {
      // We don't know yet if this is a supported platform, wait for a response.
      return null;
    }

    const additionalClassName = isPopup ? "perf-popup" : "perf-devtools";

    return div(
      { className: `perf ${additionalClassName}` },
      RecordingButton(),
      Settings(),
      isPopup ? null : Description()
    );
  }
}

function mapStateToProps(state) {
  return {
    perfFront: selectors.getPerfFront(state),
    recordingState: selectors.getRecordingState(state),
    isSupportedPlatform: selectors.getIsSupportedPlatform(state),
    isPopup: selectors.getIsPopup(state),
  };
}

const mapDispatchToProps = {
  changeRecordingState: actions.changeRecordingState,
  reportProfilerReady: actions.reportProfilerReady,
};

module.exports = connect(
  mapStateToProps,
  mapDispatchToProps
)(Perf);
