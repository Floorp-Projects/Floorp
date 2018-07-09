"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.getNextStep = getNextStep;

var _types = require("@babel/types/index");

var t = _interopRequireWildcard(_types);

var _closest = require("./utils/closest");

var _helpers = require("./utils/helpers");

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) { var desc = Object.defineProperty && Object.getOwnPropertyDescriptor ? Object.getOwnPropertyDescriptor(obj, key) : {}; if (desc.get || desc.set) { Object.defineProperty(newObj, key, desc); } else { newObj[key] = obj[key]; } } } } newObj.default = obj; return newObj; } }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function getNextStep(sourceId, pausedPosition) {
  const currentExpression = getSteppableExpression(sourceId, pausedPosition);

  if (!currentExpression) {
    return null;
  }

  const currentStatement = currentExpression.find(p => {
    return p.inList && t.isStatement(p.node);
  });

  if (!currentStatement) {
    throw new Error("Assertion failure - this should always find at least Program");
  }

  return _getNextStep(currentStatement, sourceId, pausedPosition);
}

function getSteppableExpression(sourceId, pausedPosition) {
  const closestPath = (0, _closest.getClosestPath)(sourceId, pausedPosition);

  if (!closestPath) {
    return null;
  }

  if ((0, _helpers.isAwaitExpression)(closestPath) || (0, _helpers.isYieldExpression)(closestPath)) {
    return closestPath;
  }

  return closestPath.find(p => t.isAwaitExpression(p.node) || t.isYieldExpression(p.node));
}

function _getNextStep(statement, sourceId, position) {
  const nextStatement = statement.getSibling(1);

  if (nextStatement) {
    return { ...nextStatement.node.loc.start,
      sourceId: sourceId
    };
  }

  return null;
}