/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import typeof SourceMaps from "devtools-source-map";

import { locColumn } from "./locColumn";
import { positionCmp } from "./positionCmp";
import { filterSortedArray } from "./filtering";

import type { SourceScope } from "../../../workers/parser";
import type { PartialPosition, Frame, Source } from "../../../types";

type SourceOriginalRange = {
  line: number,
  columnStart: number,
  columnEnd: number,
};

// * match - Range contains a single identifier with matching start location
// * contains - Range contains a single identifier with non-matching start
// * multiple - Range contains multiple identifiers
// * empty - Range contains no identifiers
type MappedOriginalRangeType = "match" | "contains" | "multiple" | "empty";
export type MappedOriginalRange = {
  type: MappedOriginalRangeType,
  singleDeclaration: boolean,
  line: number,
  columnStart: number,
  columnEnd: number,
};

export async function loadRangeMetadata(
  source: Source,
  frame: Frame,
  originalAstScopes: Array<SourceScope>,
  sourceMaps: SourceMaps
): Promise<Array<MappedOriginalRange>> {
  const originalRanges: Array<SourceOriginalRange> = await sourceMaps.getOriginalRanges(
    frame.location.sourceId,
    source.url
  );

  const sortedOriginalAstBindings = [];
  for (const item of originalAstScopes) {
    for (const name of Object.keys(item.bindings)) {
      for (const ref of item.bindings[name].refs) {
        sortedOriginalAstBindings.push(ref);
      }
    }
  }
  sortedOriginalAstBindings.sort((a, b) => positionCmp(a.start, b.start));

  let i = 0;

  return originalRanges.map(range => {
    const bindings = [];

    while (
      i < sortedOriginalAstBindings.length &&
      (sortedOriginalAstBindings[i].start.line < range.line ||
        (sortedOriginalAstBindings[i].start.line === range.line &&
          locColumn(sortedOriginalAstBindings[i].start) < range.columnStart))
    ) {
      i++;
    }

    while (
      i < sortedOriginalAstBindings.length &&
      sortedOriginalAstBindings[i].start.line === range.line &&
      locColumn(sortedOriginalAstBindings[i].start) >= range.columnStart &&
      locColumn(sortedOriginalAstBindings[i].start) < range.columnEnd
    ) {
      const lastBinding = bindings[bindings.length - 1];
      // Only add bindings when they're in new positions
      if (
        !lastBinding ||
        positionCmp(lastBinding.start, sortedOriginalAstBindings[i].start) !== 0
      ) {
        bindings.push(sortedOriginalAstBindings[i]);
      }
      i++;
    }

    let type = "empty";
    let singleDeclaration = true;
    if (bindings.length === 1) {
      const binding = bindings[0];
      if (
        binding.start.line === range.line &&
        binding.start.column === range.columnStart
      ) {
        type = "match";
      } else {
        type = "contains";
      }
    } else if (bindings.length > 1) {
      type = "multiple";
      const binding = bindings[0];
      const declStart =
        binding.type !== "ref" ? binding.declaration.start : null;

      singleDeclaration = bindings.every(b => {
        return (
          declStart &&
          b.type !== "ref" &&
          positionCmp(declStart, b.declaration.start) === 0
        );
      });
    }

    return {
      type,
      singleDeclaration,
      ...range,
    };
  });
}

export function findMatchingRange(
  sortedOriginalRanges: Array<MappedOriginalRange>,
  bindingRange: { +end: PartialPosition, +start: PartialPosition }
): ?MappedOriginalRange {
  return filterSortedArray(sortedOriginalRanges, range => {
    if (range.line < bindingRange.start.line) {
      return -1;
    }
    if (range.line > bindingRange.start.line) {
      return 1;
    }

    if (range.columnEnd <= locColumn(bindingRange.start)) {
      return -1;
    }
    if (range.columnStart > locColumn(bindingRange.start)) {
      return 1;
    }

    return 0;
  }).pop();
}
