"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.debugBtn = debugBtn;

var _classnames = require("devtools/client/debugger/new/dist/vendors").vendored["classnames"];

var _classnames2 = _interopRequireDefault(_classnames);

var _react = require("devtools/client/shared/vendor/react");

var _react2 = _interopRequireDefault(_react);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function _extends() { _extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; }; return _extends.apply(this, arguments); }

function _objectWithoutProperties(source, excluded) { if (source == null) return {}; var target = {}; var sourceKeys = Object.keys(source); var key, i; for (i = 0; i < sourceKeys.length; i++) { key = sourceKeys[i]; if (excluded.indexOf(key) >= 0) continue; target[key] = source[key]; } if (Object.getOwnPropertySymbols) { var sourceSymbolKeys = Object.getOwnPropertySymbols(source); for (i = 0; i < sourceSymbolKeys.length; i++) { key = sourceSymbolKeys[i]; if (excluded.indexOf(key) >= 0) continue; if (!Object.prototype.propertyIsEnumerable.call(source, key)) continue; target[key] = source[key]; } } return target; }

function debugBtn(onClick, type, className, tooltip, disabled = false, ariaPressed = false) {
  return _react2.default.createElement(CommandBarButton, {
    className: (0, _classnames2.default)(type, className),
    disabled: disabled,
    key: type,
    onClick: onClick,
    pressed: ariaPressed,
    title: tooltip
  }, _react2.default.createElement("img", {
    className: type
  }));
}

const CommandBarButton = props => {
  const {
    children,
    className,
    pressed = false
  } = props,
        rest = _objectWithoutProperties(props, ["children", "className", "pressed"]);

  return _react2.default.createElement("button", _extends({
    "aria-pressed": pressed,
    className: (0, _classnames2.default)("command-bar-button", className)
  }, rest), children);
};

exports.default = CommandBarButton;