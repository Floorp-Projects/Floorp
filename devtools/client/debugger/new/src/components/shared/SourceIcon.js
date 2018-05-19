"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});

var _react = require("devtools/client/shared/vendor/react");

var _react2 = _interopRequireDefault(_react);

var _reactRedux = require("devtools/client/shared/vendor/react-redux");

var _source = require("../../utils/source");

var _selectors = require("../../selectors/index");

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
class SourceIcon extends _react.PureComponent {
  render() {
    const {
      shouldHide,
      source,
      sourceMetaData
    } = this.props;
    const iconClass = (0, _source.getSourceClassnames)(source, sourceMetaData);

    if (shouldHide && shouldHide(iconClass)) {
      return null;
    }

    return _react2.default.createElement("img", {
      className: `source-icon ${iconClass}`
    });
  }

}

exports.default = (0, _reactRedux.connect)((state, props) => {
  return {
    sourceMetaData: (0, _selectors.getSourceMetaData)(state, props.source.id)
  };
})(SourceIcon);