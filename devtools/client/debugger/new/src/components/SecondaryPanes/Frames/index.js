/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import React, { Component } from "react";
import { connect } from "react-redux";
import PropTypes from "prop-types";

import type { Frame, Why } from "../../../types";

import FrameComponent from "./Frame";
import Group from "./Group";

import renderWhyPaused from "./WhyPaused";

import actions from "../../../actions";
import { collapseFrames, formatCopyName } from "../../../utils/pause/frames";
import { copyToTheClipboard } from "../../../utils/clipboard";

import {
  getFrameworkGroupingState,
  getSelectedFrame,
  isPaused as getIsPaused,
  getCallStackFrames,
  getPauseReason
} from "../../../selectors";

import type { LocalFrame } from "./types";

import "./Frames.css";

const NUM_FRAMES_SHOWN = 7;

type Props = {
  frames: Array<Frame>,
  frameworkGroupingOn: boolean,
  selectedFrame: Object,
  why: Why,
  selectFrame: Function,
  toggleBlackBox: Function,
  toggleFrameworkGrouping: Function,
  disableFrameTruncate: boolean,
  disableContextMenu: boolean,
  displayFullUrl: boolean,
  getFrameTitle?: string => string
};

type State = {
  showAllFrames: boolean
};

class Frames extends Component<Props, State> {
  renderFrames: Function;
  toggleFramesDisplay: Function;
  truncateFrames: Function;
  copyStackTrace: Function;
  toggleFrameworkGrouping: Function;
  renderToggleButton: Function;

  constructor(props: Props) {
    super(props);

    this.state = {
      showAllFrames: !!props.disableFrameTruncate
    };
  }

  shouldComponentUpdate(nextProps: Props, nextState: State): boolean {
    const { frames, selectedFrame, frameworkGroupingOn } = this.props;
    const { showAllFrames } = this.state;
    return (
      frames !== nextProps.frames ||
      selectedFrame !== nextProps.selectedFrame ||
      showAllFrames !== nextState.showAllFrames ||
      frameworkGroupingOn !== nextProps.frameworkGroupingOn
    );
  }

  toggleFramesDisplay = (): void => {
    this.setState(prevState => ({
      showAllFrames: !prevState.showAllFrames
    }));
  };

  collapseFrames(frames: Array<Frame>) {
    const { frameworkGroupingOn } = this.props;
    if (!frameworkGroupingOn) {
      return frames;
    }

    return collapseFrames(frames);
  }

  truncateFrames(frames: Array<Frame>): Array<Frame> {
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

  renderFrames(frames: LocalFrame[]) {
    const {
      selectFrame,
      selectedFrame,
      toggleBlackBox,
      frameworkGroupingOn,
      displayFullUrl,
      getFrameTitle,
      disableContextMenu
    } = this.props;

    const framesOrGroups = this.truncateFrames(this.collapseFrames(frames));
    type FrameOrGroup = LocalFrame | LocalFrame[];

    return (
      <ul>
        {framesOrGroups.map(
          (frameOrGroup: FrameOrGroup) =>
            frameOrGroup.id ? (
              <FrameComponent
                frame={frameOrGroup}
                toggleFrameworkGrouping={this.toggleFrameworkGrouping}
                copyStackTrace={this.copyStackTrace}
                frameworkGroupingOn={frameworkGroupingOn}
                selectFrame={selectFrame}
                selectedFrame={selectedFrame}
                toggleBlackBox={toggleBlackBox}
                key={String(frameOrGroup.id)}
                displayFullUrl={displayFullUrl}
                getFrameTitle={getFrameTitle}
                disableContextMenu={disableContextMenu}
              />
            ) : (
              <Group
                group={frameOrGroup}
                toggleFrameworkGrouping={this.toggleFrameworkGrouping}
                copyStackTrace={this.copyStackTrace}
                frameworkGroupingOn={frameworkGroupingOn}
                selectFrame={selectFrame}
                selectedFrame={selectedFrame}
                toggleBlackBox={toggleBlackBox}
                key={frameOrGroup[0].id}
                displayFullUrl={displayFullUrl}
                getFrameTitle={getFrameTitle}
                disableContextMenu={disableContextMenu}
              />
            )
        )}
      </ul>
    );
  }

  renderToggleButton(frames: LocalFrame[]) {
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
    const { frames, disableFrameTruncate, why } = this.props;

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
        {renderWhyPaused(why)}
        {disableFrameTruncate ? null : this.renderToggleButton(frames)}
      </div>
    );
  }
}

Frames.contextTypes = { l10n: PropTypes.object };

const mapStateToProps = state => ({
  frames: getCallStackFrames(state),
  why: getPauseReason(state),
  frameworkGroupingOn: getFrameworkGroupingState(state),
  selectedFrame: getSelectedFrame(state),
  pause: getIsPaused(state)
});

export default connect(
  mapStateToProps,
  {
    selectFrame: actions.selectFrame,
    toggleBlackBox: actions.toggleBlackBox,
    toggleFrameworkGrouping: actions.toggleFrameworkGrouping,
    disableFrameTruncate: false,
    disableContextMenu: false,
    displayFullUrl: false
  }
)(Frames);

// Export the non-connected component in order to use it outside of the debugger
// panel (e.g. console, netmonitor, …).
export { Frames };
