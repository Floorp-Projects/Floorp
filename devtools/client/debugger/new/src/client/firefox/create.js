"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.createFrame = createFrame;
exports.createSource = createSource;
exports.createPause = createPause;
exports.createBreakpointLocation = createBreakpointLocation;

function _objectSpread(target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i] != null ? arguments[i] : {}; var ownKeys = Object.keys(source); if (typeof Object.getOwnPropertySymbols === 'function') { ownKeys = ownKeys.concat(Object.getOwnPropertySymbols(source).filter(function (sym) { return Object.getOwnPropertyDescriptor(source, sym).enumerable; })); } ownKeys.forEach(function (key) { _defineProperty(target, key, source[key]); }); } return target; }

function _defineProperty(obj, key, value) { if (key in obj) { Object.defineProperty(obj, key, { value: value, enumerable: true, configurable: true, writable: true }); } else { obj[key] = value; } return obj; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
// This module converts Firefox specific types to the generic types
function createFrame(frame) {
  if (!frame) {
    return null;
  }

  let title;

  if (frame.type == "call") {
    const c = frame.callee;
    title = c.name || c.userDisplayName || c.displayName || L10N.getStr("anonymous");
  } else {
    title = `(${frame.type})`;
  }

  const location = {
    sourceId: frame.where.source.actor,
    line: frame.where.line,
    column: frame.where.column
  };
  return {
    id: frame.actor,
    displayName: title,
    location,
    generatedLocation: location,
    this: frame.this,
    scope: frame.environment
  };
}

function createSource(source, {
  supportsWasm
}) {
  return {
    id: source.actor,
    url: source.url,
    isPrettyPrinted: false,
    isWasm: supportsWasm && source.introductionType === "wasm",
    sourceMapURL: source.sourceMapURL,
    isBlackBoxed: false,
    loadedState: "unloaded"
  };
}

function createPause(packet, response) {
  // NOTE: useful when the debugger is already paused
  const frame = packet.frame || response.frames[0];
  return _objectSpread({}, packet, {
    frame: createFrame(frame),
    frames: response.frames.map(createFrame)
  });
} // Firefox only returns `actualLocation` if it actually changed,
// but we want it always to exist. Format `actualLocation` if it
// exists, otherwise use `location`.


function createBreakpointLocation(location, actualLocation) {
  if (!actualLocation) {
    return location;
  }

  return {
    sourceId: actualLocation.source.actor,
    sourceUrl: actualLocation.source.url,
    line: actualLocation.line,
    column: actualLocation.column
  };
}