"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.updateFrameLocation = updateFrameLocation;
exports.mapDisplayNames = mapDisplayNames;
exports.mapFrames = mapFrames;

var _selectors = require("../../selectors/index");

var _ast = require("../../utils/ast");

function _objectSpread(target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i] != null ? arguments[i] : {}; var ownKeys = Object.keys(source); if (typeof Object.getOwnPropertySymbols === 'function') { ownKeys = ownKeys.concat(Object.getOwnPropertySymbols(source).filter(function (sym) { return Object.getOwnPropertyDescriptor(source, sym).enumerable; })); } ownKeys.forEach(function (key) { _defineProperty(target, key, source[key]); }); } return target; }

function _defineProperty(obj, key, value) { if (key in obj) { Object.defineProperty(obj, key, { value: value, enumerable: true, configurable: true, writable: true }); } else { obj[key] = value; } return obj; }

function updateFrameLocation(frame, sourceMaps) {
  return sourceMaps.getOriginalLocation(frame.location).then(loc => _objectSpread({}, frame, {
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
    const source = (0, _selectors.getSource)(getState(), frame.location.sourceId);
    const symbols = (0, _selectors.getSymbols)(getState(), source);

    if (!symbols || !symbols.functions) {
      return frame;
    }

    const originalFunction = (0, _ast.findClosestFunction)(symbols, frame.location);

    if (!originalFunction) {
      return frame;
    }

    const originalDisplayName = originalFunction.name;
    return _objectSpread({}, frame, {
      originalDisplayName
    });
  });
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
    mappedFrames = mapDisplayNames(mappedFrames, getState);
    dispatch({
      type: "MAP_FRAMES",
      frames: mappedFrames
    });
  };
}