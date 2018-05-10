"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = remapLocations;

function _objectSpread(target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i] != null ? arguments[i] : {}; var ownKeys = Object.keys(source); if (typeof Object.getOwnPropertySymbols === 'function') { ownKeys = ownKeys.concat(Object.getOwnPropertySymbols(source).filter(function (sym) { return Object.getOwnPropertyDescriptor(source, sym).enumerable; })); } ownKeys.forEach(function (key) { _defineProperty(target, key, source[key]); }); } return target; }

function _defineProperty(obj, key, value) { if (key in obj) { Object.defineProperty(obj, key, { value: value, enumerable: true, configurable: true, writable: true }); } else { obj[key] = value; } return obj; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function remapLocations(breakpoints, sourceId, sourceMaps) {
  const sourceBreakpoints = breakpoints.map(async breakpoint => {
    if (breakpoint.location.sourceId !== sourceId) {
      return breakpoint;
    }

    const location = await sourceMaps.getOriginalLocation(breakpoint.location);
    return _objectSpread({}, breakpoint, {
      location
    });
  });
  return Promise.all(sourceBreakpoints.valueSeq());
}