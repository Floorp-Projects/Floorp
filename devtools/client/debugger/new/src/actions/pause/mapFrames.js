"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.updateFrameLocation = updateFrameLocation;
exports.mapDisplayNames = mapDisplayNames;
exports.mapFrames = mapFrames;

var _selectors = require("../../selectors/index");

var _assert = require("../../utils/assert");

var _assert2 = _interopRequireDefault(_assert);

var _ast = require("../../utils/ast");

var _devtoolsSourceMap = require("devtools/client/shared/source-map/index.js");

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function updateFrameLocation(frame, sourceMaps) {
  if (frame.isOriginal) {
    return Promise.resolve(frame);
  }

  return sourceMaps.getOriginalLocation(frame.location).then(loc => ({ ...frame,
    location: loc,
    generatedLocation: frame.generatedLocation || frame.location
  }));
}

function updateFrameLocations(frames, sourceMaps) {
  if (!frames || frames.length == 0) {
    return Promise.resolve(frames);
  }

  return Promise.all(frames.map(frame => updateFrameLocation(frame, sourceMaps)));
}

function mapDisplayNames(frames, getState) {
  return frames.map(frame => {
    if (frame.isOriginal) {
      return frame;
    }

    const source = (0, _selectors.getSource)(getState(), frame.location.sourceId);

    if (!source) {
      return frame;
    }

    const symbols = (0, _selectors.getSymbols)(getState(), source);

    if (!symbols || !symbols.functions) {
      return frame;
    }

    const originalFunction = (0, _ast.findClosestFunction)(symbols, frame.location);

    if (!originalFunction) {
      return frame;
    }

    const originalDisplayName = originalFunction.name;
    return { ...frame,
      originalDisplayName
    };
  });
}

function isWasmOriginalSourceFrame(frame, getState) {
  if ((0, _devtoolsSourceMap.isGeneratedId)(frame.location.sourceId)) {
    return false;
  }

  const generatedSource = (0, _selectors.getSource)(getState(), frame.generatedLocation.sourceId);
  return Boolean(generatedSource && generatedSource.isWasm);
}

async function expandFrames(frames, sourceMaps, getState) {
  const result = [];

  for (let i = 0; i < frames.length; ++i) {
    const frame = frames[i];

    if (frame.isOriginal || !isWasmOriginalSourceFrame(frame, getState)) {
      result.push(frame);
      continue;
    }

    const originalFrames = await sourceMaps.getOriginalStackFrames(frame.generatedLocation);

    if (!originalFrames) {
      result.push(frame);
      continue;
    }

    (0, _assert2.default)(originalFrames.length > 0, "Expected at least one original frame"); // First entry has not specific location -- use one from original frame.

    originalFrames[0] = { ...originalFrames[0],
      location: frame.location
    };
    originalFrames.forEach((originalFrame, j) => {
      // Keep outer most frame with true actor ID, and generate uniquie
      // one for the nested frames.
      const id = j == 0 ? frame.id : `${frame.id}-originalFrame${j}`;
      result.push({
        id,
        displayName: originalFrame.displayName,
        location: originalFrame.location,
        scope: frame.scope,
        this: frame.this,
        isOriginal: true,
        // More fields that will be added by the mapDisplayNames and
        // updateFrameLocation.
        generatedLocation: frame.generatedLocation,
        originalDisplayName: originalFrame.displayName
      });
    });
  }

  return result;
}
/**
 * Map call stack frame locations and display names to originals.
 * e.g.
 * 1. When the debuggee pauses
 * 2. When a source is pretty printed
 * 3. When symbols are loaded
 * @memberof actions/pause
 * @static
 */


function mapFrames() {
  return async function ({
    dispatch,
    getState,
    sourceMaps
  }) {
    const frames = (0, _selectors.getFrames)(getState());

    if (!frames) {
      return;
    }

    let mappedFrames = await updateFrameLocations(frames, sourceMaps);
    mappedFrames = await expandFrames(mappedFrames, sourceMaps, getState);
    mappedFrames = mapDisplayNames(mappedFrames, getState);
    dispatch({
      type: "MAP_FRAMES",
      frames: mappedFrames
    });
  };
}