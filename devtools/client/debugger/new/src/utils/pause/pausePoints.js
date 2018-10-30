/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
import { reverse } from "lodash";

import type { PausePoints } from "../../workers/parser";
import type { ColumnPosition } from "../../types";

type PausePoint = {
  location: ColumnPosition,
  types: { break: boolean, step: boolean }
};

function insertStrtAt(string, index, newString) {
  const start = string.slice(0, index);
  const end = string.slice(index);
  return `${start}${newString}${end}`;
}

export function convertToList(pausePoints: PausePoints): PausePoint[] {
  const list = [];
  for (const line in pausePoints) {
    for (const column in pausePoints[line]) {
      const point = pausePoints[line][column];
      list.push({
        location: { line: parseInt(line, 10), column: parseInt(column, 10) },
        types: point
      });
    }
  }
  return list;
}

export function formatPausePoints(text: string, pausePoints: PausePoints) {
  const nodes = reverse(convertToList(pausePoints));
  const lines = text.split("\n");
  nodes.forEach((node, index) => {
    const { line, column } = node.location;
    const { break: breakPoint, step } = node.types;
    const types = `${breakPoint ? "b" : ""}${step ? "s" : ""}`;
    lines[line - 1] = insertStrtAt(lines[line - 1], column, `/*${types}*/`);
  });

  return lines.join("\n");
}
