/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import createSimplePath from "./simple-path";
import { traverseAst } from "./ast";
import { nodeContainsPosition } from "./contains";

export function getClosestPath(sourceId, location) {
  let closestPath = null;

  traverseAst(sourceId, {
    enter(node, ancestors) {
      if (nodeContainsPosition(node, location)) {
        const path = createSimplePath(ancestors);

        if (path && (!closestPath || path.depth > closestPath.depth)) {
          closestPath = path;
        }
      }
    },
  });

  if (!closestPath) {
    throw new Error("Assertion failure - This should always fine a path");
  }

  return closestPath;
}
