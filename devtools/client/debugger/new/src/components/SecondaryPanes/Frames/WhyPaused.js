"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = renderWhyPaused;

var _react = require("devtools/client/shared/vendor/react");

var _react2 = _interopRequireDefault(_react);

var _pause = require("../../../utils/pause/index");

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function renderExceptionSummary(exception) {
  if (typeof exception === "string") {
    return exception;
  }

  const preview = exception.preview;

  if (!preview) {
    return;
  }

  return `${preview.name}: ${preview.message}`;
}

function renderMessage(why) {
  if (why.type == "exception" && why.exception) {
    return _react2.default.createElement("div", {
      className: "message warning"
    }, renderExceptionSummary(why.exception));
  }

  if (typeof why.message == "string") {
    return _react2.default.createElement("div", {
      className: "message"
    }, why.message);
  }

  return null;
}

function renderWhyPaused(why) {
  const reason = (0, _pause.getPauseReason)(why);

  if (!reason) {
    return null;
  }

  return _react2.default.createElement("div", {
    className: "pane why-paused"
  }, _react2.default.createElement("div", null, L10N.getStr(reason)), renderMessage(why));
}

renderWhyPaused.displayName = "whyPaused";