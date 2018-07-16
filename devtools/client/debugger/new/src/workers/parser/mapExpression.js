"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = mapExpression;

var _mapOriginalExpression = require("./mapOriginalExpression");

var _mapOriginalExpression2 = _interopRequireDefault(_mapOriginalExpression);

var _mapBindings = require("./mapBindings");

var _mapBindings2 = _interopRequireDefault(_mapBindings);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function mapExpression(expression, mappings, bindings, shouldMapBindings = true) {
  let originalExpression = expression;

  if (mappings) {
    originalExpression = (0, _mapOriginalExpression2.default)(expression, mappings);
  }

  let safeExpression = originalExpression;

  if (shouldMapBindings) {
    safeExpression = (0, _mapBindings2.default)(originalExpression, bindings);
  }

  return safeExpression;
}