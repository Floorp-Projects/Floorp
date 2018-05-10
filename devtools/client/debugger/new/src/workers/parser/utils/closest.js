"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.getClosestPath = getClosestPath;

var _simplePath = require("./simple-path");

var _simplePath2 = _interopRequireDefault(_simplePath);

var _ast = require("./ast");

var _contains = require("./contains");

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function getClosestPath(sourceId, location) {
  let closestPath = null;
  (0, _ast.traverseAst)(sourceId, {
    enter(node, ancestors) {
      if ((0, _contains.nodeContainsPosition)(node, location)) {
        const path = (0, _simplePath2.default)(ancestors);

        if (path && (!closestPath || path.depth > closestPath.depth)) {
          closestPath = path;
        }
      }
    }

  });

  if (!closestPath) {
    throw new Error("Assertion failure - This should always fine a path");
  }

  return closestPath;
}