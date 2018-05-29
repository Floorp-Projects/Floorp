"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.loadRangeMetadata = loadRangeMetadata;
exports.findMatchingRange = findMatchingRange;

var _locColumn = require("./locColumn");

var _positionCmp = require("./positionCmp");

var _filtering = require("./filtering");

function _objectSpread(target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i] != null ? arguments[i] : {}; var ownKeys = Object.keys(source); if (typeof Object.getOwnPropertySymbols === 'function') { ownKeys = ownKeys.concat(Object.getOwnPropertySymbols(source).filter(function (sym) { return Object.getOwnPropertyDescriptor(source, sym).enumerable; })); } ownKeys.forEach(function (key) { _defineProperty(target, key, source[key]); }); } return target; }

function _defineProperty(obj, key, value) { if (key in obj) { Object.defineProperty(obj, key, { value: value, enumerable: true, configurable: true, writable: true }); } else { obj[key] = value; } return obj; }

async function loadRangeMetadata(source, frame, originalAstScopes, sourceMaps) {
  const originalRanges = await sourceMaps.getOriginalRanges(frame.location.sourceId, source.url);
  const sortedOriginalAstBindings = [];

  for (const item of originalAstScopes) {
    for (const name of Object.keys(item.bindings)) {
      for (const ref of item.bindings[name].refs) {
        sortedOriginalAstBindings.push(ref);
      }
    }
  }

  sortedOriginalAstBindings.sort((a, b) => (0, _positionCmp.positionCmp)(a.start, b.start));
  let i = 0;
  return originalRanges.map(range => {
    const bindings = [];

    while (i < sortedOriginalAstBindings.length && (sortedOriginalAstBindings[i].start.line < range.line || sortedOriginalAstBindings[i].start.line === range.line && (0, _locColumn.locColumn)(sortedOriginalAstBindings[i].start) < range.columnStart)) {
      i++;
    }

    while (i < sortedOriginalAstBindings.length && sortedOriginalAstBindings[i].start.line === range.line && (0, _locColumn.locColumn)(sortedOriginalAstBindings[i].start) >= range.columnStart && (0, _locColumn.locColumn)(sortedOriginalAstBindings[i].start) < range.columnEnd) {
      bindings.push(sortedOriginalAstBindings[i]);
      i++;
    }

    let type = "empty";
    let singleDeclaration = true;

    if (bindings.length === 1) {
      const binding = bindings[0];

      if (binding.start.line === range.line && binding.start.column === range.columnStart) {
        type = "match";
      } else {
        type = "contains";
      }
    } else if (bindings.length > 1) {
      type = "multiple";
      const binding = bindings[0];
      const declStart = binding.type !== "ref" ? binding.declaration.start : null;
      singleDeclaration = bindings.every(b => {
        return declStart && b.type !== "ref" && (0, _positionCmp.positionCmp)(declStart, b.declaration.start) === 0;
      });
    }

    return _objectSpread({
      type,
      singleDeclaration
    }, range);
  });
}

function findMatchingRange(sortedOriginalRanges, bindingRange) {
  return (0, _filtering.filterSortedArray)(sortedOriginalRanges, range => {
    if (range.line < bindingRange.start.line) {
      return -1;
    }

    if (range.line > bindingRange.start.line) {
      return 1;
    }

    if (range.columnEnd <= (0, _locColumn.locColumn)(bindingRange.start)) {
      return -1;
    }

    if (range.columnStart > (0, _locColumn.locColumn)(bindingRange.start)) {
      return 1;
    }

    return 0;
  }).pop();
}