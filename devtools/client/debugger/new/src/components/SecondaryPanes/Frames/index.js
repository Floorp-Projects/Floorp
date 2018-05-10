"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});

var _react = require("devtools/client/shared/vendor/react");

var _react2 = _interopRequireDefault(_react);

var _reactRedux = require("devtools/client/shared/vendor/react-redux");

var _Frame = require("./Frame");

var _Frame2 = _interopRequireDefault(_Frame);

var _Group = require("./Group");

var _Group2 = _interopRequireDefault(_Group);

var _WhyPaused = require("./WhyPaused");

var _WhyPaused2 = _interopRequireDefault(_WhyPaused);

var _actions = require("../../../actions/index");

var _actions2 = _interopRequireDefault(_actions);

var _frames = require("../../../utils/pause/frames/index");

var _clipboard = require("../../../utils/clipboard");

var _selectors = require("../../../selectors/index");

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
const NUM_FRAMES_SHOWN = 7;

class Frames extends _react.Component {
  constructor(props) {
    super(props);

    this.toggleFramesDisplay = () => {
      this.setState(prevState => ({
        showAllFrames: !prevState.showAllFrames
      }));
    };

    this.copyStackTrace = () => {
      const {
        frames
      } = this.props;
      const framesToCopy = frames.map(f => (0, _frames.formatCopyName)(f)).join("\n");
      (0, _clipboard.copyToTheClipboard)(framesToCopy);
    };

    this.toggleFrameworkGrouping = () => {
      const {
        toggleFrameworkGrouping,
        frameworkGroupingOn
      } = this.props;
      toggleFrameworkGrouping(!frameworkGroupingOn);
    };

    this.state = {
      showAllFrames: !!props.disableFrameTruncate
    };
  }

  shouldComponentUpdate(nextProps, nextState) {
    const {
      frames,
      selectedFrame,
      frameworkGroupingOn
    } = this.props;
    const {
      showAllFrames
    } = this.state;
    return frames !== nextProps.frames || selectedFrame !== nextProps.selectedFrame || showAllFrames !== nextState.showAllFrames || frameworkGroupingOn !== nextProps.frameworkGroupingOn;
  }

  collapseFrames(frames) {
    const {
      frameworkGroupingOn
    } = this.props;

    if (!frameworkGroupingOn) {
      return frames;
    }

    return (0, _frames.collapseFrames)(frames);
  }

  truncateFrames(frames) {
    const numFramesToShow = this.state.showAllFrames ? frames.length : NUM_FRAMES_SHOWN;
    return frames.slice(0, numFramesToShow);
  }

  renderFrames(frames) {
    const {
      selectFrame,
      selectedFrame,
      toggleBlackBox,
      frameworkGroupingOn
    } = this.props;
    const framesOrGroups = this.truncateFrames(this.collapseFrames(frames));
    return _react2.default.createElement("ul", null, framesOrGroups.map(frameOrGroup => frameOrGroup.id ? _react2.default.createElement(_Frame2.default, {
      frame: frameOrGroup,
      toggleFrameworkGrouping: this.toggleFrameworkGrouping,
      copyStackTrace: this.copyStackTrace,
      frameworkGroupingOn: frameworkGroupingOn,
      selectFrame: selectFrame,
      selectedFrame: selectedFrame,
      toggleBlackBox: toggleBlackBox,
      key: String(frameOrGroup.id)
    }) : _react2.default.createElement(_Group2.default, {
      group: frameOrGroup,
      toggleFrameworkGrouping: this.toggleFrameworkGrouping,
      copyStackTrace: this.copyStackTrace,
      frameworkGroupingOn: frameworkGroupingOn,
      selectFrame: selectFrame,
      selectedFrame: selectedFrame,
      toggleBlackBox: toggleBlackBox,
      key: frameOrGroup[0].id
    })));
  }

  renderToggleButton(frames) {
    const buttonMessage = this.state.showAllFrames ? L10N.getStr("callStack.collapse") : L10N.getStr("callStack.expand");
    frames = this.collapseFrames(frames);

    if (frames.length <= NUM_FRAMES_SHOWN) {
      return null;
    }

    return _react2.default.createElement("div", {
      className: "show-more-container"
    }, _react2.default.createElement("button", {
      className: "show-more",
      onClick: this.toggleFramesDisplay
    }, buttonMessage));
  }

  render() {
    const {
      frames,
      disableFrameTruncate,
      why
    } = this.props;

    if (!frames) {
      return _react2.default.createElement("div", {
        className: "pane frames"
      }, _react2.default.createElement("div", {
        className: "pane-info empty"
      }, L10N.getStr("callStack.notPaused")));
    }

    return _react2.default.createElement("div", {
      className: "pane frames"
    }, this.renderFrames(frames), (0, _WhyPaused2.default)(why), disableFrameTruncate ? null : this.renderToggleButton(frames));
  }

}

const mapStateToProps = state => ({
  frames: (0, _selectors.getCallStackFrames)(state),
  why: (0, _selectors.getPauseReason)(state),
  frameworkGroupingOn: (0, _selectors.getFrameworkGroupingState)(state),
  selectedFrame: (0, _selectors.getSelectedFrame)(state),
  pause: (0, _selectors.isPaused)(state)
});

exports.default = (0, _reactRedux.connect)(mapStateToProps, _actions2.default)(Frames);