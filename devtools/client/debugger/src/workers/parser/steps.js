/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import * as t from "@babel/types";
import { getClosestPath } from "./utils/closest";
import { isAwaitExpression, isYieldExpression } from "./utils/helpers";

export function getNextStep(sourceId, pausedPosition) {
  const currentExpression = getSteppableExpression(sourceId, pausedPosition);
  if (!currentExpression) {
    return null;
  }

  const currentStatement = currentExpression.find(p => {
    return p.inList && t.isStatement(p.node);
  });

  if (!currentStatement) {
    throw new Error(
      "Assertion failure - this should always find at least Program"
    );
  }

  return _getNextStep(currentStatement, sourceId, pausedPosition);
}

function getSteppableExpression(sourceId, pausedPosition) {
  const closestPath = getClosestPath(sourceId, pausedPosition);

  if (!closestPath) {
    return null;
  }

  if (isAwaitExpression(closestPath) || isYieldExpression(closestPath)) {
    return closestPath;
  }

  return closestPath.find(
    p => t.isAwaitExpression(p.node) || t.isYieldExpression(p.node)
  );
}

function _getNextStep(statement, sourceId, position) {
  const nextStatement = statement.getSibling(1);
  if (nextStatement) {
    return {
      ...nextStatement.node.loc.start,
      sourceId,
    };
  }

  return null;
}
