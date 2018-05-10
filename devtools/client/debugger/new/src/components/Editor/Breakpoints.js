"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});

var _reactRedux = require("devtools/client/shared/vendor/react-redux");

var _react = require("devtools/client/shared/vendor/react");

var _react2 = _interopRequireDefault(_react);

var _Breakpoint = require("./Breakpoint");

var _Breakpoint2 = _interopRequireDefault(_Breakpoint);

var _selectors = require("../../selectors/index");

var _breakpoint = require("../../utils/breakpoint/index");

var _source = require("../../utils/source");

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
class Breakpoints extends _react.Component {
  shouldComponentUpdate(nextProps) {
    if (nextProps.selectedSource && !(0, _source.isLoaded)(nextProps.selectedSource)) {
      return false;
    }

    return true;
  }

  render() {
    const {
      breakpoints,
      selectedSource,
      editor
    } = this.props;

    if (!selectedSource || !breakpoints || selectedSource.get("isBlackBoxed")) {
      return null;
    }

    return _react2.default.createElement("div", null, breakpoints.valueSeq().map(bp => {
      return _react2.default.createElement(_Breakpoint2.default, {
        key: (0, _breakpoint.makeLocationId)(bp.location),
        breakpoint: bp,
        selectedSource: selectedSource,
        editor: editor
      });
    }));
  }

}

exports.default = (0, _reactRedux.connect)(state => ({
  breakpoints: (0, _selectors.getVisibleBreakpoints)(state),
  selectedSource: (0, _selectors.getSelectedSource)(state)
}))(Breakpoints);