"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.inferClassName = inferClassName;

var _types = require("@babel/types/index");

var t = _interopRequireWildcard(_types);

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) { var desc = Object.defineProperty && Object.getOwnPropertyDescriptor ? Object.getOwnPropertyDescriptor(obj, key) : {}; if (desc.get || desc.set) { Object.defineProperty(newObj, key, desc); } else { newObj[key] = obj[key]; } } } } newObj.default = obj; return newObj; } }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
// the function class is inferred from a call like
// createClass or extend
function fromCallExpression(callExpression) {
  const whitelist = ["extend", "createClass"];
  const callee = callExpression.node.callee;

  if (!callee) {
    return null;
  }

  const name = t.isMemberExpression(callee) ? callee.property.name : callee.name;

  if (!whitelist.includes(name)) {
    return null;
  }

  const variable = callExpression.findParent(p => t.isVariableDeclarator(p.node));

  if (variable) {
    return variable.node.id.name;
  }

  const assignment = callExpression.findParent(p => t.isAssignmentExpression(p.node));

  if (!assignment) {
    return null;
  }

  const left = assignment.node.left;

  if (left.name) {
    return name;
  }

  if (t.isMemberExpression(left)) {
    return left.property.name;
  }

  return null;
} // the function class is inferred from a prototype assignment
// e.g. TodoClass.prototype.render = function() {}


function fromPrototype(assignment) {
  const left = assignment.node.left;

  if (!left) {
    return null;
  }

  if (t.isMemberExpression(left) && left.object && t.isMemberExpression(left.object) && left.object.property.identifier === "prototype") {
    return left.object.object.name;
  }

  return null;
} // infer class finds an appropriate class for functions
// that are defined inside of a class like thing.
// e.g. `class Foo`, `TodoClass.prototype.foo`,
//      `Todo = createClass({ foo: () => {}})`


function inferClassName(path) {
  const classDeclaration = path.findParent(p => t.isClassDeclaration(p.node));

  if (classDeclaration) {
    return classDeclaration.node.id.name;
  }

  const callExpression = path.findParent(p => t.isCallExpression(p.node));

  if (callExpression) {
    return fromCallExpression(callExpression);
  }

  const assignment = path.findParent(p => t.isAssignmentExpression(p.node));

  if (assignment) {
    return fromPrototype(assignment);
  }

  return null;
}