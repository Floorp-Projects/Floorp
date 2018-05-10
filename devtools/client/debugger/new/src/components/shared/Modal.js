"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.Modal = exports.transitionTimeout = undefined;
exports.default = Slide;

var _propTypes = require("devtools/client/shared/vendor/react-prop-types");

var _propTypes2 = _interopRequireDefault(_propTypes);

var _react = require("devtools/client/shared/vendor/react");

var _react2 = _interopRequireDefault(_react);

var _classnames = require("devtools/client/debugger/new/dist/vendors").vendored["classnames"];

var _classnames2 = _interopRequireDefault(_classnames);

var _Transition = require("devtools/client/debugger/new/dist/vendors").vendored["react-transition-group/Transition"];

var _Transition2 = _interopRequireDefault(_Transition);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
const transitionTimeout = exports.transitionTimeout = 175;

class Modal extends _react2.default.Component {
  constructor(...args) {
    var _temp;

    return _temp = super(...args), this.onClick = e => {
      e.stopPropagation();
    }, _temp;
  }

  render() {
    const {
      additionalClass,
      children,
      handleClose,
      status
    } = this.props;
    return _react2.default.createElement("div", {
      className: "modal-wrapper",
      onClick: handleClose
    }, _react2.default.createElement("div", {
      className: (0, _classnames2.default)("modal", additionalClass, status),
      onClick: this.onClick
    }, children));
  }

}

exports.Modal = Modal;
Modal.contextTypes = {
  shortcuts: _propTypes2.default.object
};

function Slide({
  in: inProp,
  children,
  additionalClass,
  handleClose
}) {
  return _react2.default.createElement(_Transition2.default, {
    "in": inProp,
    timeout: transitionTimeout,
    appear: true
  }, status => _react2.default.createElement(Modal, {
    status: status,
    additionalClass: additionalClass,
    handleClose: handleClose
  }, children));
}