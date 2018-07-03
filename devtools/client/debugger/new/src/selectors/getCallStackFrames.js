"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.getCallStackFrames = undefined;
exports.formatCallStackFrames = formatCallStackFrames;

var _sources = require("../reducers/sources");

var _pause = require("../reducers/pause");

var _frames = require("../utils/pause/frames/index");

var _devtoolsSourceMap = require("devtools/client/shared/source-map/index.js");

var _lodash = require("devtools/client/shared/vendor/lodash");

var _reselect = require("devtools/client/debugger/new/dist/vendors").vendored["reselect"];

function _objectSpread(target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i] != null ? arguments[i] : {}; var ownKeys = Object.keys(source); if (typeof Object.getOwnPropertySymbols === 'function') { ownKeys = ownKeys.concat(Object.getOwnPropertySymbols(source).filter(function (sym) { return Object.getOwnPropertyDescriptor(source, sym).enumerable; })); } ownKeys.forEach(function (key) { _defineProperty(target, key, source[key]); }); } return target; }

function _defineProperty(obj, key, value) { if (key in obj) { Object.defineProperty(obj, key, { value: value, enumerable: true, configurable: true, writable: true }); } else { obj[key] = value; } return obj; }

function getLocation(frame, isGeneratedSource) {
  return isGeneratedSource ? frame.generatedLocation || frame.location : frame.location;
}

function getSourceForFrame(sources, frame, isGeneratedSource) {
  const sourceId = getLocation(frame, isGeneratedSource).sourceId;
  return (0, _sources.getSourceInSources)(sources, sourceId);
}

function appendSource(sources, frame, selectedSource) {
  const isGeneratedSource = selectedSource && !(0, _devtoolsSourceMap.isOriginalId)(selectedSource.id);
  return _objectSpread({}, frame, {
    location: getLocation(frame, isGeneratedSource),
    source: getSourceForFrame(sources, frame, isGeneratedSource)
  });
}

function formatCallStackFrames(frames, sources, selectedSource) {
  if (!frames) {
    return null;
  }

  const formattedFrames = frames.filter(frame => getSourceForFrame(sources, frame)).map(frame => appendSource(sources, frame, selectedSource)).filter(frame => !(0, _lodash.get)(frame, "source.isBlackBoxed"));
  return (0, _frames.annotateFrames)(formattedFrames);
}

const getCallStackFrames = exports.getCallStackFrames = (0, _reselect.createSelector)(_pause.getFrames, _sources.getSources, _sources.getSelectedSource, formatCallStackFrames);