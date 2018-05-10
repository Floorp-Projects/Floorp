"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.getGeneratedLocationRanges = getGeneratedLocationRanges;

var _positionCmp = require("./positionCmp");

var _filtering = require("./filtering");

var _mappingContains = require("./mappingContains");

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
async function getGeneratedLocationRanges(generatedAstBindings, source, {
  start,
  end
}, bindingType, locationType, sourceMaps) {
  const endPosition = await sourceMaps.getGeneratedLocation(end, source);
  const startPosition = await sourceMaps.getGeneratedLocation(start, source); // If the start and end positions collapse into eachother, it means that
  // the range in the original content didn't _start_ at the start position.
  // Since this likely means that the range doesn't logically apply to this
  // binding location, we skip it.

  if ((0, _positionCmp.positionCmp)(startPosition, endPosition) === 0) {
    return [];
  }

  const ranges = await sourceMaps.getGeneratedRanges(start, source);
  const resultRanges = ranges.reduce((acc, mapRange) => {
    // Some tooling creates ranges that map a line as a whole, which is useful
    // for step-debugging, but can easily lead to finding the wrong binding.
    // To avoid these false-positives, we entirely ignore ranges that cover
    // full lines.
    if (locationType === "ref" && mapRange.columnStart === 0 && mapRange.columnEnd === Infinity) {
      return acc;
    }

    const range = {
      start: {
        line: mapRange.line,
        column: mapRange.columnStart
      },
      end: {
        line: mapRange.line,
        // SourceMapConsumer's 'lastColumn' is inclusive, so we add 1 to make
        // it exclusive like all other locations.
        column: mapRange.columnEnd + 1
      }
    };
    const previous = acc[acc.length - 1];

    if (previous && (previous.end.line === range.start.line && previous.end.column === range.start.column || previous.end.line + 1 === range.start.line && previous.end.column === Infinity && range.start.column === 0)) {
      previous.end.line = range.end.line;
      previous.end.column = range.end.column;
    } else {
      acc.push(range);
    }

    return acc;
  }, []); // When searching for imports, we expand the range to up to the next available
  // mapping to allow for import declarations that are composed of multiple
  // variable statements, where the later ones are entirely unmapped.
  // Babel 6 produces imports in this style, e.g.
  //
  // var _mod = require("mod"); // mapped from import statement
  // var _mod2 = interop(_mod); // entirely unmapped

  if (bindingType === "import" && locationType !== "ref") {
    for (const range of resultRanges) {
      if ((0, _mappingContains.mappingContains)(range, {
        start: startPosition,
        end: startPosition
      }) && (0, _positionCmp.positionCmp)(range.end, endPosition) < 0) {
        range.end.line = endPosition.line;
        range.end.column = endPosition.column;
        break;
      }
    }
  }

  return filterApplicableBindings(generatedAstBindings, resultRanges);
}

function filterApplicableBindings(bindings, ranges) {
  const result = [];

  for (const range of ranges) {
    // Any binding overlapping a part of the mapping range.
    const filteredBindings = (0, _filtering.filterSortedArray)(bindings, binding => {
      if ((0, _positionCmp.positionCmp)(binding.loc.end, range.start) <= 0) {
        return -1;
      }

      if ((0, _positionCmp.positionCmp)(binding.loc.start, range.end) >= 0) {
        return 1;
      }

      return 0;
    });
    let firstInRange = true;
    let firstOnLine = true;
    let line = -1;

    for (const binding of filteredBindings) {
      if (binding.loc.start.line === line) {
        firstOnLine = false;
      } else {
        line = binding.loc.start.line;
        firstOnLine = true;
      }

      result.push({
        binding,
        range,
        firstOnLine,
        firstInRange
      });
      firstInRange = false;
    }
  }

  return result;
}