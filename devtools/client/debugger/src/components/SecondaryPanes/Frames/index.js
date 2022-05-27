/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { Component } from "react";
import { connect } from "../../../utils/connect";
import PropTypes from "prop-types";

import FrameComponent from "./Frame";
import Group from "./Group";

import actions from "../../../actions";
import { collapseFrames, formatCopyName } from "../../../utils/pause/frames";
import { copyToTheClipboard } from "../../../utils/clipboard";

import {
  getFrameworkGroupingState,
  getSelectedFrame,
  getCallStackFrames,
  getCurrentThread,
  getThreadContext,
} from "../../../selectors";

import "./Frames.css";

const NUM_FRAMES_SHOWN = 7;

class Frames extends Component {
  constructor(props) {
    super(props);

    this.state = {
      showAllFrames: !!props.disableFrameTruncate,
    };
  }

  static get propTypes() {
    return {
      cx: PropTypes.object,
      disableContextMenu: PropTypes.bool.isRequired,
      disableFrameTruncate: PropTypes.bool.isRequired,
      displayFullUrl: PropTypes.bool.isRequired,
      frames: PropTypes.array.isRequired,
      frameworkGroupingOn: PropTypes.bool.isRequired,
      getFrameTitle: PropTypes.func,
      panel: PropTypes.oneOf(["debugger", "webconsole"]).isRequired,
      restart: PropTypes.func,
      selectFrame: PropTypes.func.isRequired,
      selectLocation: PropTypes.func,
      selectedFrame: PropTypes.object,
      toggleBlackBox: PropTypes.func,
      toggleFrameworkGrouping: PropTypes.func,
    };
  }

  shouldComponentUpdate(nextProps, nextState) {
    const { frames, selectedFrame, frameworkGroupingOn } = this.props;
    const { showAllFrames } = this.state;
    return (
      frames !== nextProps.frames ||
      selectedFrame !== nextProps.selectedFrame ||
      showAllFrames !== nextState.showAllFrames ||
      frameworkGroupingOn !== nextProps.frameworkGroupingOn
    );
  }

  toggleFramesDisplay = () => {
    this.setState(prevState => ({
      showAllFrames: !prevState.showAllFrames,
    }));
  };

  collapseFrames(frames) {
    const { frameworkGroupingOn } = this.props;
    if (!frameworkGroupingOn) {
      return frames;
    }

    return collapseFrames(frames);
  }

  truncateFrames(frames) {
    const numFramesToShow = this.state.showAllFrames
      ? frames.length
      : NUM_FRAMES_SHOWN;

    return frames.slice(0, numFramesToShow);
  }

  copyStackTrace = () => {
    const { frames } = this.props;
    const { l10n } = this.context;
    const framesToCopy = frames.map(f => formatCopyName(f, l10n)).join("\n");
    copyToTheClipboard(framesToCopy);
  };

  toggleFrameworkGrouping = () => {
    const { toggleFrameworkGrouping, frameworkGroupingOn } = this.props;
    toggleFrameworkGrouping(!frameworkGroupingOn);
  };

  renderFrames(frames) {
    const {
      cx,
      selectFrame,
      selectLocation,
      selectedFrame,
      toggleBlackBox,
      frameworkGroupingOn,
      displayFullUrl,
      getFrameTitle,
      disableContextMenu,
      panel,
      restart,
    } = this.props;

    const framesOrGroups = this.truncateFrames(this.collapseFrames(frames));

    // We're not using a <ul> because it adds new lines before and after when
    // the user copies the trace. Needed for the console which has several
    // places where we don't want to have those new lines.
    return (
      <div role="list">
        {framesOrGroups.map(frameOrGroup =>
          frameOrGroup.id ? (
            <FrameComponent
              cx={cx}
              frame={frameOrGroup}
              toggleFrameworkGrouping={this.toggleFrameworkGrouping}
              copyStackTrace={this.copyStackTrace}
              frameworkGroupingOn={frameworkGroupingOn}
              selectFrame={selectFrame}
              selectLocation={selectLocation}
              selectedFrame={selectedFrame}
              toggleBlackBox={toggleBlackBox}
              key={String(frameOrGroup.id)}
              displayFullUrl={displayFullUrl}
              getFrameTitle={getFrameTitle}
              disableContextMenu={disableContextMenu}
              panel={panel}
              restart={restart}
            />
          ) : (
            <Group
              cx={cx}
              group={frameOrGroup}
              toggleFrameworkGrouping={this.toggleFrameworkGrouping}
              copyStackTrace={this.copyStackTrace}
              frameworkGroupingOn={frameworkGroupingOn}
              selectFrame={selectFrame}
              selectLocation={selectLocation}
              selectedFrame={selectedFrame}
              toggleBlackBox={toggleBlackBox}
              key={frameOrGroup[0].id}
              displayFullUrl={displayFullUrl}
              getFrameTitle={getFrameTitle}
              disableContextMenu={disableContextMenu}
              panel={panel}
              restart={restart}
            />
          )
        )}
      </div>
    );
  }

  renderToggleButton(frames) {
    const { l10n } = this.context;
    const buttonMessage = this.state.showAllFrames
      ? l10n.getStr("callStack.collapse")
      : l10n.getStr("callStack.expand");

    frames = this.collapseFrames(frames);
    if (frames.length <= NUM_FRAMES_SHOWN) {
      return null;
    }

    return (
      <div className="show-more-container">
        <button className="show-more" onClick={this.toggleFramesDisplay}>
          {buttonMessage}
        </button>
      </div>
    );
  }

  render() {
    const { frames, disableFrameTruncate } = this.props;

    if (!frames) {
      return (
        <div className="pane frames">
          <div className="pane-info empty">
            {L10N.getStr("callStack.notPaused")}
          </div>
        </div>
      );
    }

    return (
      <div className="pane frames">
        {this.renderFrames(frames)}
        {disableFrameTruncate ? null : this.renderToggleButton(frames)}
      </div>
    );
  }
}

Frames.contextTypes = { l10n: PropTypes.object };

const mapStateToProps = state => ({
  cx: getThreadContext(state),
  frames: getCallStackFrames(state),
  frameworkGroupingOn: getFrameworkGroupingState(state),
  selectedFrame: getSelectedFrame(state, getCurrentThread(state)),
  disableFrameTruncate: false,
  disableContextMenu: false,
  displayFullUrl: false,
});

export default connect(mapStateToProps, {
  selectFrame: actions.selectFrame,
  selectLocation: actions.selectLocation,
  toggleBlackBox: actions.toggleBlackBox,
  toggleFrameworkGrouping: actions.toggleFrameworkGrouping,
  restart: actions.restart,
})(Frames);

// Export the non-connected component in order to use it outside of the debugger
// panel (e.g. console, netmonitor, …).
export { Frames };
