/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { Component } from "react";
import { connect } from "../../../utils/connect";
import PropTypes from "prop-types";

import FrameComponent from "./Frame";
import Group from "./Group";

import actions from "../../../actions";
import { collapseFrames } from "../../../utils/pause/frames";

import {
  getFrameworkGroupingState,
  getSelectedFrame,
  getCurrentThreadFrames,
  getCurrentThread,
  getShouldSelectOriginalLocation,
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
      disableContextMenu: PropTypes.bool.isRequired,
      disableFrameTruncate: PropTypes.bool.isRequired,
      displayFullUrl: PropTypes.bool.isRequired,
      frames: PropTypes.array.isRequired,
      frameworkGroupingOn: PropTypes.bool.isRequired,
      getFrameTitle: PropTypes.func,
      panel: PropTypes.oneOf(["debugger", "webconsole"]).isRequired,
      selectFrame: PropTypes.func.isRequired,
      selectLocation: PropTypes.func,
      selectedFrame: PropTypes.object,
      showFrameContextMenu: PropTypes.func,
      shouldDisplayOriginalLocation: PropTypes.bool,
    };
  }

  shouldComponentUpdate(nextProps, nextState) {
    const {
      frames,
      selectedFrame,
      frameworkGroupingOn,
      shouldDisplayOriginalLocation,
    } = this.props;
    const { showAllFrames } = this.state;
    return (
      frames !== nextProps.frames ||
      selectedFrame !== nextProps.selectedFrame ||
      showAllFrames !== nextState.showAllFrames ||
      frameworkGroupingOn !== nextProps.frameworkGroupingOn ||
      shouldDisplayOriginalLocation !== nextProps.shouldDisplayOriginalLocation
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

  renderFrames(frames) {
    const {
      selectFrame,
      selectLocation,
      selectedFrame,
      displayFullUrl,
      getFrameTitle,
      disableContextMenu,
      panel,
      shouldDisplayOriginalLocation,
      showFrameContextMenu,
    } = this.props;

    const framesOrGroups = this.truncateFrames(this.collapseFrames(frames));

    // We're not using a <ul> because it adds new lines before and after when
    // the user copies the trace. Needed for the console which has several
    // places where we don't want to have those new lines.
    return React.createElement(
      "div",
      {
        role: "list",
      },
      framesOrGroups.map(frameOrGroup =>
        frameOrGroup.id
          ? React.createElement(FrameComponent, {
              frame: frameOrGroup,
              showFrameContextMenu: showFrameContextMenu,
              selectFrame: selectFrame,
              selectLocation: selectLocation,
              selectedFrame: selectedFrame,
              shouldDisplayOriginalLocation: shouldDisplayOriginalLocation,
              key: String(frameOrGroup.id),
              displayFullUrl: displayFullUrl,
              getFrameTitle: getFrameTitle,
              disableContextMenu: disableContextMenu,
              panel: panel,
            })
          : React.createElement(Group, {
              group: frameOrGroup,
              showFrameContextMenu: showFrameContextMenu,
              selectFrame: selectFrame,
              selectLocation: selectLocation,
              selectedFrame: selectedFrame,
              key: frameOrGroup[0].id,
              displayFullUrl: displayFullUrl,
              getFrameTitle: getFrameTitle,
              disableContextMenu: disableContextMenu,
              panel: panel,
            })
      )
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
    return React.createElement(
      "div",
      {
        className: "show-more-container",
      },
      React.createElement(
        "button",
        {
          className: "show-more",
          onClick: this.toggleFramesDisplay,
        },
        buttonMessage
      )
    );
  }

  render() {
    const { frames, disableFrameTruncate } = this.props;

    if (!frames) {
      return React.createElement(
        "div",
        {
          className: "pane frames",
        },
        React.createElement(
          "div",
          {
            className: "pane-info empty",
          },
          L10N.getStr("callStack.notPaused")
        )
      );
    }
    return React.createElement(
      "div",
      {
        className: "pane frames",
      },
      this.renderFrames(frames),
      disableFrameTruncate ? null : this.renderToggleButton(frames)
    );
  }
}

Frames.contextTypes = { l10n: PropTypes.object };

const mapStateToProps = state => ({
  frames: getCurrentThreadFrames(state),
  frameworkGroupingOn: getFrameworkGroupingState(state),
  selectedFrame: getSelectedFrame(state, getCurrentThread(state)),
  shouldDisplayOriginalLocation: getShouldSelectOriginalLocation(state),
  disableFrameTruncate: false,
  disableContextMenu: false,
  displayFullUrl: false,
});

export default connect(mapStateToProps, {
  selectFrame: actions.selectFrame,
  selectLocation: actions.selectLocation,
  showFrameContextMenu: actions.showFrameContextMenu,
})(Frames);

// Export the non-connected component in order to use it outside of the debugger
// panel (e.g. console, netmonitor, …).
export { Frames };
