"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = mapOriginalExpression;

var _ast = require("./utils/ast");

var _getScopes = require("./getScopes/index");

var _generator = require("@babel/generator/index");

var _generator2 = _interopRequireDefault(_generator);

var _types = require("@babel/types/index");

var t = _interopRequireWildcard(_types);

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) { var desc = Object.defineProperty && Object.getOwnPropertyDescriptor ? Object.getOwnPropertyDescriptor(obj, key) : {}; if (desc.get || desc.set) { Object.defineProperty(newObj, key, desc); } else { newObj[key] = obj[key]; } } } } newObj.default = obj; return newObj; } }

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
// NOTE: this will only work if we are replacing an original identifier
function replaceNode(ancestors, node) {
  const ancestor = ancestors[ancestors.length - 1];

  if (typeof ancestor.index === "number") {
    ancestor.node[ancestor.key][ancestor.index] = node;
  } else {
    ancestor.node[ancestor.key] = node;
  }
}

function getFirstExpression(ast) {
  const statements = ast.program.body;

  if (statements.length == 0) {
    return null;
  }

  return statements[0].expression;
}

function locationKey(start) {
  return `${start.line}:${start.column}`;
}

function mapOriginalExpression(expression, mappings) {
  const ast = (0, _ast.parseScript)(expression);
  const scopes = (0, _getScopes.buildScopeList)(ast, "");
  const nodes = new Map();
  const replacements = new Map(); // The ref-only global bindings are the ones that are accessed, but not
  // declared anywhere in the parsed code, meaning they are either global,
  // or declared somewhere in a scope outside the parsed code, so we
  // rewrite all of those specifically to avoid rewritting declarations that
  // shadow outer mappings.

  for (const name of Object.keys(scopes[0].bindings)) {
    const {
      refs
    } = scopes[0].bindings[name];
    const mapping = mappings[name];

    if (!refs.every(ref => ref.type === "ref") || !mapping || mapping === name) {
      continue;
    }

    let node = nodes.get(name);

    if (!node) {
      node = getFirstExpression((0, _ast.parseScript)(mapping));
      nodes.set(name, node);
    }

    for (const ref of refs) {
      let {
        line,
        column
      } = ref.start; // This shouldn't happen, just keeping Flow happy.

      if (typeof column !== "number") {
        column = 0;
      }

      replacements.set(locationKey({
        line,
        column
      }), node);
    }
  }

  if (replacements.size === 0) {
    // Avoid the extra code generation work and also avoid potentially
    // reformatting the user's code unnecessarily.
    return expression;
  }

  t.traverse(ast, (node, ancestors) => {
    if (!t.isIdentifier(node) && !t.isThisExpression(node)) {
      return;
    }

    const replacement = replacements.get(locationKey(node.loc.start));

    if (replacement) {
      replaceNode(ancestors, t.cloneNode(replacement));
    }
  });
  return (0, _generator2.default)(ast).code;
}