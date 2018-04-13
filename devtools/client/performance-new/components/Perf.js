/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { PureComponent, createFactory } = require("devtools/client/shared/vendor/react");
const { div, button, p, span, img } = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const PerfSettings = createFactory(require("devtools/client/performance-new/components/PerfSettings.js"));
const { openLink } = require("devtools/client/shared/link");

/**
 * The recordingState is one of the following:
 **/

// The initial state before we've queried the PerfActor
const NOT_YET_KNOWN = "not-yet-known";
// The profiler is available, we haven't started recording yet.
const AVAILABLE_TO_RECORD = "available-to-record";
// An async request has been sent to start the profiler.
const REQUEST_TO_START_RECORDING = "request-to-start-recording";
// An async request has been sent to get the profile and stop the profiler.
const REQUEST_TO_GET_PROFILE_AND_STOP_PROFILER =
  "request-to-get-profile-and-stop-profiler";
// An async request has been sent to stop the profiler.
const REQUEST_TO_STOP_PROFILER = "request-to-stop-profiler";
// The profiler notified us that our request to start it actually started it.
const RECORDING = "recording";
// Some other code with access to the profiler started it.
const OTHER_IS_RECORDING = "other-is-recording";
// Profiling is not available when in private browsing mode.
const LOCKED_BY_PRIVATE_BROWSING = "locked-by-private-browsing";

class Perf extends PureComponent {
  static get propTypes() {
    return {
      toolbox: PropTypes.object.isRequired,
      perfFront: PropTypes.object.isRequired,
      receiveProfile: PropTypes.func.isRequired
    };
  }

  constructor(props) {
    super(props);
    this.state = {
      recordingState: NOT_YET_KNOWN,
      recordingUnexpectedlyStopped: false,
      // The following is either "null" for unknown, or a boolean value.
      isSupportedPlatform: null
    };
    this.startRecording = this.startRecording.bind(this);
    this.getProfileAndStopProfiler = this.getProfileAndStopProfiler.bind(this);
    this.stopProfilerAndDiscardProfile = this.stopProfilerAndDiscardProfile.bind(this);
    this.handleProfilerStarting = this.handleProfilerStarting.bind(this);
    this.handleProfilerStopping = this.handleProfilerStopping.bind(this);
    this.handlePrivateBrowsingStarting = this.handlePrivateBrowsingStarting.bind(this);
    this.handlePrivateBrowsingEnding = this.handlePrivateBrowsingEnding.bind(this);
    this.settingsComponentCreated = this.settingsComponentCreated.bind(this);
    this.handleLinkClick = this.handleLinkClick.bind(this);
  }

  componentDidMount() {
    const { perfFront } = this.props;

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

      let recordingState = this.state.recordingState;
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
      this.setState({ isSupportedPlatform, recordingState });
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
    switch (this.state.recordingState) {
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

  /**
   * Store a reference to the settings component. This gives the <Perf> component
   * access to the `.getRecordingSettings()` method. At this time the recording panel
   * is not doing much state management, so this avoid the overhead of redux.
   */
  settingsComponentCreated(settings) {
    this.settings = settings;
  }

  getRecordingStateForTesting() {
    return this.state.recordingState;
  }

  handleProfilerStarting() {
    switch (this.state.recordingState) {
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

        this.setState({
          recordingState: OTHER_IS_RECORDING,
          recordingUnexpectedlyStopped: false
        });
        break;

      case REQUEST_TO_START_RECORDING:
        // Wait for the profiler to tell us that it has started.
        this.setState({
          recordingState: RECORDING,
          recordingUnexpectedlyStopped: false
        });
        break;

      case LOCKED_BY_PRIVATE_BROWSING:
      case OTHER_IS_RECORDING:
      case RECORDING:
        // These state cases don't make sense to happen, and means we have a logical
        // fallacy somewhere.
        throw new Error(
          "The profiler started recording, when it shouldn't have " +
          `been able to. Current state: "${this.state.recordingState}"`);
      default:
        throw new Error("Unhandled recording state");
    }
  }

  handleProfilerStopping() {
    switch (this.state.recordingState) {
      case NOT_YET_KNOWN:
      case REQUEST_TO_GET_PROFILE_AND_STOP_PROFILER:
      case REQUEST_TO_STOP_PROFILER:
      case OTHER_IS_RECORDING:
        this.setState({
          recordingState: AVAILABLE_TO_RECORD,
          recordingUnexpectedlyStopped: false
        });
        break;

      case REQUEST_TO_START_RECORDING:
        // Highly unlikely, but someone stopped the recorder, this is fine.
        // Do nothing (fallthrough).
      case LOCKED_BY_PRIVATE_BROWSING:
        // The profiler is already locked, so we know about this already.
        break;

      case RECORDING:
        this.setState({
          recordingState: AVAILABLE_TO_RECORD,
          recordingUnexpectedlyStopped: true
        });
        break;

      case AVAILABLE_TO_RECORD:
        throw new Error(
          "The profiler stopped recording, when it shouldn't have been able to.");
      default:
        throw new Error("Unhandled recording state");
    }
  }

  handlePrivateBrowsingStarting() {
    switch (this.state.recordingState) {
      case REQUEST_TO_GET_PROFILE_AND_STOP_PROFILER:
        // This one is a tricky case. Go ahead and act like nothing went wrong, maybe
        // it will resolve correctly? (fallthrough)
      case REQUEST_TO_STOP_PROFILER:
      case AVAILABLE_TO_RECORD:
      case OTHER_IS_RECORDING:
      case NOT_YET_KNOWN:
        this.setState({
          recordingState: LOCKED_BY_PRIVATE_BROWSING,
          recordingUnexpectedlyStopped: false
        });
        break;

      case REQUEST_TO_START_RECORDING:
      case RECORDING:
        this.setState({
          recordingState: LOCKED_BY_PRIVATE_BROWSING,
          recordingUnexpectedlyStopped: true
        });
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
    this.setState({
      recordingState: AVAILABLE_TO_RECORD,
      recordingUnexpectedlyStopped: false
    });
  }

  startRecording() {
    const settings = this.settings;
    if (!settings) {
      console.error("Expected the PerfSettings panel to be rendered and available.");
      return;
    }
    this.setState({
      recordingState: REQUEST_TO_START_RECORDING,
      // Reset this error state since it's no longer valid.
      recordingUnexpectedlyStopped: false,
    });
    this.props.perfFront.startProfiler(
      // Pull out the recording settings from the child component. This approach avoids
      // using Redux as a state manager.
      settings.getRecordingSettings()
    );
  }

  async getProfileAndStopProfiler() {
    this.setState({ recordingState: REQUEST_TO_GET_PROFILE_AND_STOP_PROFILER });
    const profile = await this.props.perfFront.getProfileAndStopProfiler();
    this.setState({ recordingState: AVAILABLE_TO_RECORD });
    console.log("getProfileAndStopProfiler");
    this.props.receiveProfile(profile);
  }

  stopProfilerAndDiscardProfile() {
    this.setState({ recordingState: REQUEST_TO_STOP_PROFILER });
    this.props.perfFront.stopProfilerAndDiscardProfile();
  }

  renderButton() {
    const { recordingState, isSupportedPlatform } = this.state;

    if (!isSupportedPlatform) {
      return renderButton({
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
        return renderButton({
          onClick: this.startRecording,
          label: span(
            null,
            img({
              className: "perf-button-image",
              src: "chrome://devtools/skin/images/tool-profiler.svg"
            }),
            "Start recording",
          ),
          additionalMessage: this.state.recordingUnexpectedlyStopped
            ? div(null, "The recording was stopped by another tool.")
            : null
        });

      case REQUEST_TO_STOP_PROFILER:
        return renderButton({
          label: "Stopping the recording",
          disabled: true
        });

      case REQUEST_TO_GET_PROFILE_AND_STOP_PROFILER:
        return renderButton({
          label: "Stopping the recording, and capturing the profile",
          disabled: true
        });

      case REQUEST_TO_START_RECORDING:
      case RECORDING:
        return renderButton({
          label: "Stop and grab the recording",
          onClick: this.getProfileAndStopProfiler,
          disabled: this.state.recordingState === REQUEST_TO_START_RECORDING
        });

      case OTHER_IS_RECORDING:
        return renderButton({
          label: "Stop and discard the other recording",
          onClick: this.stopProfilerAndDiscardProfile,
          additionalMessage: "Another tool is currently recording."
        });

      case LOCKED_BY_PRIVATE_BROWSING:
        return renderButton({
          label: "Start recording",
          disabled: true,
          additionalMessage: `The profiler is disabled when Private Browsing is enabled.
                              Close all Private Windows to re-enable the profiler`
        });

      default:
        throw new Error("Unhandled recording state");
    }
  }

  handleLinkClick(event) {
    openLink(event.target.value, this.props.toolbox);
  }

  render() {
    const { isSupportedPlatform } = this.state;

    if (isSupportedPlatform === null) {
      // We don't know yet if this is a supported platform, wait for a response.
      return null;
    }

    return div(
      { className: "perf" },
      this.renderButton(),
      PerfSettings({ ref: this.settingsComponentCreated }),
      div(
        { className: "perf-description" },
        p(null,
          "This new recording panel is a bit different from the existing " +
            "performance panel. It records the entire browser, and then opens up " +
            "and shares the profile with ",
          button(
            // Implement links as buttons to avoid any risk of loading the link in the
            // the panel.
            {
              className: "perf-external-link",
              value: "https://perf-html.io",
              onClick: this.handleLinkClick
            },
            "perf-html.io"
          ),
          ", a Mozilla performance analysis tool."
        ),
        p(null,
          "This is still a prototype. Join along or file bugs at: ",
          button(
            // Implement links as buttons to avoid any risk of loading the link in the
            // the panel.
            {
              className: "perf-external-link",
              value: "https://github.com/devtools-html/perf.html",
              onClick: this.handleLinkClick
            },
            "github.com/devtools-html/perf.html"
          ),
          "."
        )
      ),
    );
  }
}

module.exports = Perf;

function renderButton(props) {
  const { disabled, label, onClick, additionalMessage } = props;
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
