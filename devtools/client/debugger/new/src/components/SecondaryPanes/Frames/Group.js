"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});

var _react = require("devtools/client/shared/vendor/react");

var _react2 = _interopRequireDefault(_react);

var _classnames = require("devtools/client/debugger/new/dist/vendors").vendored["classnames"];

var _classnames2 = _interopRequireDefault(_classnames);

var _Svg = require("devtools/client/debugger/new/dist/vendors").vendored["Svg"];

var _Svg2 = _interopRequireDefault(_Svg);

var _frames = require("../../../utils/pause/frames/index");

var _FrameMenu = require("./FrameMenu");

var _FrameMenu2 = _interopRequireDefault(_FrameMenu);

var _Frame = require("./Frame");

var _Frame2 = _interopRequireDefault(_Frame);

var _Badge = require("../../shared/Badge");

var _Badge2 = _interopRequireDefault(_Badge);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function FrameLocation({
  frame
}) {
  const library = frame.library || (0, _frames.getLibraryFromUrl)(frame);

  if (!library) {
    return null;
  }

  return _react2.default.createElement("div", {
    className: "location"
  }, library, _react2.default.createElement(_Svg2.default, {
    name: library.toLowerCase(),
    className: "annotation-logo"
  }));
}

FrameLocation.displayName = "FrameLocation";

class Group extends _react.Component {
  constructor(...args) {
    super(...args);

    this.toggleFrames = () => {
      this.setState(prevState => ({
        expanded: !prevState.expanded
      }));
    };

    this.state = {
      expanded: false
    };
  }

  onContextMenu(event) {
    const {
      group,
      copyStackTrace,
      toggleFrameworkGrouping,
      toggleBlackBox,
      frameworkGroupingOn
    } = this.props;
    const frame = group[0];
    (0, _FrameMenu2.default)(frame, frameworkGroupingOn, {
      copyStackTrace,
      toggleFrameworkGrouping,
      toggleBlackBox
    }, event);
  }

  renderFrames() {
    const {
      group,
      selectFrame,
      selectedFrame,
      toggleFrameworkGrouping,
      frameworkGroupingOn,
      toggleBlackBox,
      copyStackTrace
    } = this.props;
    const {
      expanded
    } = this.state;

    if (!expanded) {
      return null;
    }

    return _react2.default.createElement("div", {
      className: "frames-list"
    }, group.map(frame => _react2.default.createElement(_Frame2.default, {
      copyStackTrace: copyStackTrace,
      frame: frame,
      frameworkGroupingOn: frameworkGroupingOn,
      hideLocation: true,
      key: frame.id,
      selectedFrame: selectedFrame,
      selectFrame: selectFrame,
      shouldMapDisplayName: false,
      toggleBlackBox: toggleBlackBox,
      toggleFrameworkGrouping: toggleFrameworkGrouping
    })));
  }

  renderDescription() {
    const frame = this.props.group[0];
    const displayName = (0, _frames.formatDisplayName)(frame);
    return _react2.default.createElement("li", {
      key: frame.id,
      className: (0, _classnames2.default)("group"),
      onClick: this.toggleFrames,
      tabIndex: 0
    }, _react2.default.createElement("div", {
      className: "d-flex align-items-center min-width-0"
    }, _react2.default.createElement("div", {
      className: "title"
    }, displayName), _react2.default.createElement(_Badge2.default, null, this.props.group.length)), _react2.default.createElement(FrameLocation, {
      frame: frame
    }));
  }

  render() {
    const {
      expanded
    } = this.state;
    return _react2.default.createElement("div", {
      className: (0, _classnames2.default)("frames-group", {
        expanded
      }),
      onContextMenu: e => this.onContextMenu(e)
    }, this.renderDescription(), this.renderFrames());
  }

}

exports.default = Group;
Group.displayName = "Group";