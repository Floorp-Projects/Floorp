/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import * as t from "@babel/types";
import generate from "@babel/generator";

export function isFunction(node) {
  return (
    t.isFunction(node) ||
    t.isArrowFunctionExpression(node) ||
    t.isObjectMethod(node) ||
    t.isClassMethod(node)
  );
}

export function isAwaitExpression(path) {
  const { node, parent } = path;
  return (
    t.isAwaitExpression(node) ||
    t.isAwaitExpression(parent.init) ||
    t.isAwaitExpression(parent)
  );
}

export function isYieldExpression(path) {
  const { node, parent } = path;
  return (
    t.isYieldExpression(node) ||
    t.isYieldExpression(parent.init) ||
    t.isYieldExpression(parent)
  );
}

export function isObjectShorthand(parent) {
  if (!t.isObjectProperty(parent)) {
    return false;
  }

  if (parent.value && parent.value.left) {
    return (
      parent.value.type === "AssignmentPattern" &&
      parent.value.left.type === "Identifier"
    );
  }

  return (
    parent.value &&
    parent.key.start == parent.value.start &&
    parent.key.loc.identifierName === parent.value.loc.identifierName
  );
}

export function getObjectExpressionValue(node) {
  const { value } = node;

  if (t.isIdentifier(value)) {
    return value.name;
  }

  if (t.isCallExpression(value) || t.isFunctionExpression(value)) {
    return "";
  }
  const code = generate(value).code;

  const shouldWrap = t.isObjectExpression(value);
  return shouldWrap ? `(${code})` : code;
}

export function getCode(node) {
  return generate(node).code;
}

export function getComments(ast) {
  if (!ast || !ast.comments) {
    return [];
  }
  return ast.comments.map(comment => ({
    name: comment.location,
    location: comment.loc,
  }));
}

export function getSpecifiers(specifiers) {
  if (!specifiers) {
    return [];
  }

  return specifiers.map(specifier => specifier.local?.name);
}

export function isComputedExpression(expression) {
  return /^\[/m.test(expression);
}

export function getMemberExpression(root) {
  function _getMemberExpression(node, expr) {
    if (t.isMemberExpression(node)) {
      expr = [node.property.name].concat(expr);
      return _getMemberExpression(node.object, expr);
    }

    if (t.isCallExpression(node)) {
      return [];
    }

    if (t.isThisExpression(node)) {
      return ["this"].concat(expr);
    }

    return [node.name].concat(expr);
  }

  const expr = _getMemberExpression(root, []);
  return expr.join(".");
}

export function getVariables(dec) {
  if (!dec.id) {
    return [];
  }

  if (t.isArrayPattern(dec.id)) {
    if (!dec.id.elements) {
      return [];
    }

    // NOTE: it's possible that an element is empty or has several variables
    // e.g. const [, a] = arr
    // e.g. const [{a, b }] = 2
    return dec.id.elements
      .filter(Boolean)
      .map(element => ({
        name: t.isAssignmentPattern(element)
          ? element.left.name
          : element.name || element.argument?.name,
        location: element.loc,
      }))
      .filter(({ name }) => name);
  }

  return [
    {
      name: dec.id.name,
      location: dec.loc,
    },
  ];
}

export function getPatternIdentifiers(pattern) {
  let items = [];
  if (t.isObjectPattern(pattern)) {
    items = pattern.properties.map(({ value }) => value);
  }

  if (t.isArrayPattern(pattern)) {
    items = pattern.elements;
  }

  return getIdentifiers(items);
}

function getIdentifiers(items) {
  let ids = [];
  items.forEach(function(item) {
    if (t.isObjectPattern(item) || t.isArrayPattern(item)) {
      ids = ids.concat(getPatternIdentifiers(item));
    } else if (t.isIdentifier(item)) {
      const { start, end } = item.loc;
      ids.push({
        name: item.name,
        expression: item.name,
        location: { start, end },
      });
    }
  });
  return ids;
}

// Top Level checks the number of "body" nodes in the ancestor chain
// if the node is top-level, then it shoul only have one body.
export function isTopLevel(ancestors) {
  return ancestors.filter(ancestor => ancestor.key == "body").length == 1;
}

export function nodeLocationKey(a) {
  const { start, end } = a.location;
  return `${start.line}:${start.column}:${end.line}:${end.column}`;
}

export function getFunctionParameterNames(path) {
  if (path.node.params != null) {
    return path.node.params.map(param => {
      if (param.type !== "AssignmentPattern") {
        return param.name;
      }

      // Parameter with default value
      if (
        param.left.type === "Identifier" &&
        param.right.type === "Identifier"
      ) {
        return `${param.left.name} = ${param.right.name}`;
      } else if (
        param.left.type === "Identifier" &&
        param.right.type === "StringLiteral"
      ) {
        return `${param.left.name} = ${param.right.value}`;
      } else if (
        param.left.type === "Identifier" &&
        param.right.type === "ObjectExpression"
      ) {
        return `${param.left.name} = {}`;
      } else if (
        param.left.type === "Identifier" &&
        param.right.type === "ArrayExpression"
      ) {
        return `${param.left.name} = []`;
      } else if (
        param.left.type === "Identifier" &&
        param.right.type === "NullLiteral"
      ) {
        return `${param.left.name} = null`;
      }

      return null;
    });
  }
  return [];
}
