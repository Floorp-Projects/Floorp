/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
import * as t from "@babel/types";
import type { Node } from "@babel/types";

// Perform ES6's anonymous function name inference for all
// locations where static analysis is possible.
// eslint-disable-next-line complexity
export default function getFunctionName(node: Node, parent: Node): string {
  if (t.isIdentifier(node.id)) {
    return node.id.name;
  }

  if (
    t.isObjectMethod(node, { computed: false }) ||
    t.isClassMethod(node, { computed: false })
  ) {
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

  if (
    t.isObjectProperty(parent, { computed: false, value: node }) ||
    // TODO: Babylon 6 doesn't support computed class props. It is included
    // here so that it is most flexible. Once Babylon 7 is used, this
    // can change to use computed: false like ObjectProperty.
    (t.isClassProperty(parent, { value: node }) && !parent.computed)
  ) {
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

  if (t.isAssignmentExpression(parent, { operator: "=", right: node })) {
    if (t.isIdentifier(parent.left)) {
      return parent.left.name;
    }

    // This case is not supported in standard ES6 name inference, but it
    // is included here since it is still a helpful case during debugging.
    if (t.isMemberExpression(parent.left, { computed: false })) {
      return parent.left.property.name;
    }
  }

  if (
    t.isAssignmentPattern(parent, { right: node }) &&
    t.isIdentifier(parent.left)
  ) {
    return parent.left.name;
  }

  if (
    t.isVariableDeclarator(parent, { init: node }) &&
    t.isIdentifier(parent.id)
  ) {
    return parent.id.name;
  }

  if (
    t.isExportDefaultDeclaration(parent, { declaration: node }) &&
    t.isFunctionDeclaration(node)
  ) {
    return "default";
  }

  return "anonymous";
}
