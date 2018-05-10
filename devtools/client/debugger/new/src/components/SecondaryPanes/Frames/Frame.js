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

var _source = require("../../../utils/source");

var _FrameMenu = require("./FrameMenu");

var _FrameMenu2 = _interopRequireDefault(_FrameMenu);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function FrameTitle({
  frame,
  options
}) {
  const displayName = (0, _frames.formatDisplayName)(frame, options);
  return _react2.default.createElement("div", {
    className: "title"
  }, displayName);
}

function FrameLocation({
  frame
}) {
  if (!frame.source) {
    return;
  }

  if (frame.library) {
    return _react2.default.createElement("div", {
      className: "location"
    }, frame.library, _react2.default.createElement(_Svg2.default, {
      name: frame.library.toLowerCase(),
      className: "annotation-logo"
    }));
  }

  const filename = (0, _source.getFilename)(frame.source);
  return _react2.default.createElement("div", {
    className: "location"
  }, `${filename}: ${frame.location.line}`);
}

FrameLocation.displayName = "FrameLocation";

class FrameComponent extends _react.Component {
  onContextMenu(event) {
    const {
      frame,
      copyStackTrace,
      toggleFrameworkGrouping,
      toggleBlackBox,
      frameworkGroupingOn
    } = this.props;
    (0, _FrameMenu2.default)(frame, frameworkGroupingOn, {
      copyStackTrace,
      toggleFrameworkGrouping,
      toggleBlackBox
    }, event);
  }

  onMouseDown(e, frame, selectedFrame) {
    if (e.nativeEvent.which == 3) {
      return;
    }

    this.props.selectFrame(frame);
  }

  onKeyUp(event, frame, selectedFrame) {
    if (event.key != "Enter") {
      return;
    }

    this.props.selectFrame(frame);
  }

  render() {
    const {
      frame,
      selectedFrame,
      hideLocation,
      shouldMapDisplayName
    } = this.props;
    const className = (0, _classnames2.default)("frame", {
      selected: selectedFrame && selectedFrame.id === frame.id
    });
    return _react2.default.createElement("li", {
      key: frame.id,
      className: className,
      onMouseDown: e => this.onMouseDown(e, frame, selectedFrame),
      onKeyUp: e => this.onKeyUp(e, frame, selectedFrame),
      onContextMenu: e => this.onContextMenu(e),
      tabIndex: 0
    }, _react2.default.createElement(FrameTitle, {
      frame: frame,
      options: {
        shouldMapDisplayName
      }
    }), !hideLocation && _react2.default.createElement(FrameLocation, {
      frame: frame
    }));
  }

}

exports.default = FrameComponent;
FrameComponent.defaultProps = {
  hideLocation: false,
  shouldMapDisplayName: true
};
FrameComponent.displayName = "Frame";