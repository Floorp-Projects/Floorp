"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = getFunctionName;

var _types = require("@babel/types/index");

var t = _interopRequireWildcard(_types);

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) { var desc = Object.defineProperty && Object.getOwnPropertyDescriptor ? Object.getOwnPropertyDescriptor(obj, key) : {}; if (desc.get || desc.set) { Object.defineProperty(newObj, key, desc); } else { newObj[key] = obj[key]; } } } } newObj.default = obj; return newObj; } }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
// Perform ES6's anonymous function name inference for all
// locations where static analysis is possible.
// eslint-disable-next-line complexity
function getFunctionName(node, parent) {
  if (t.isIdentifier(node.id)) {
    return node.id.name;
  }

  if (t.isObjectMethod(node, {
    computed: false
  }) || t.isClassMethod(node, {
    computed: false
  })) {
    const key = node.key;

    if (t.isIdentifier(key)) {
      return key.name;
    }

    if (t.isStringLiteral(key)) {
      return key.value;
    }

    if (t.isNumericLiteral(key)) {
      return `${key.value}`;
    }
  }

  if (t.isObjectProperty(parent, {
    computed: false,
    value: node
  }) || // TODO: Babylon 6 doesn't support computed class props. It is included
  // here so that it is most flexible. Once Babylon 7 is used, this
  // can change to use computed: false like ObjectProperty.
  t.isClassProperty(parent, {
    value: node
  }) && !parent.computed) {
    const key = parent.key;

    if (t.isIdentifier(key)) {
      return key.name;
    }

    if (t.isStringLiteral(key)) {
      return key.value;
    }

    if (t.isNumericLiteral(key)) {
      return `${key.value}`;
    }
  }

  if (t.isAssignmentExpression(parent, {
    operator: "=",
    right: node
  })) {
    if (t.isIdentifier(parent.left)) {
      return parent.left.name;
    } // This case is not supported in standard ES6 name inference, but it
    // is included here since it is still a helpful case during debugging.


    if (t.isMemberExpression(parent.left, {
      computed: false
    })) {
      return parent.left.property.name;
    }
  }

  if (t.isAssignmentPattern(parent, {
    right: node
  }) && t.isIdentifier(parent.left)) {
    return parent.left.name;
  }

  if (t.isVariableDeclarator(parent, {
    init: node
  }) && t.isIdentifier(parent.id)) {
    return parent.id.name;
  }

  if (t.isExportDefaultDeclaration(parent, {
    declaration: node
  }) && t.isFunctionDeclaration(node)) {
    return "default";
  }

  return "anonymous";
}