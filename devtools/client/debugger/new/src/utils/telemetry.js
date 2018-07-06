"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.recordEvent = recordEvent;

var _telemetry = require("devtools/client/shared/telemetry");

var _telemetry2 = _interopRequireDefault(_telemetry);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function _objectSpread(target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i] != null ? arguments[i] : {}; var ownKeys = Object.keys(source); if (typeof Object.getOwnPropertySymbols === 'function') { ownKeys = ownKeys.concat(Object.getOwnPropertySymbols(source).filter(function (sym) { return Object.getOwnPropertyDescriptor(source, sym).enumerable; })); } ownKeys.forEach(function (key) { _defineProperty(target, key, source[key]); }); } return target; }

function _defineProperty(obj, key, value) { if (key in obj) { Object.defineProperty(obj, key, { value: value, enumerable: true, configurable: true, writable: true }); } else { obj[key] = value; } return obj; }

const telemetry = new _telemetry2.default();
/**
 * @memberof utils/telemetry
 * @static
 */

function recordEvent(eventName, fields = {}) {
  let sessionId = -1;

  if (typeof window === "object" && window.parent.frameElement) {
    sessionId = window.parent.frameElement.getAttribute("session_id");
  }
  /* eslint-disable camelcase */


  telemetry.recordEvent("devtools.main", eventName, "debugger", null, _objectSpread({
    session_id: sessionId
  }, fields));
  /* eslint-enable camelcase */
}