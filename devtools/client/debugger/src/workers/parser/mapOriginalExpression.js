/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import type { Position } from "../../types";
import { parseScript } from "./utils/ast";
import { buildScopeList } from "./getScopes";
import generate from "@babel/generator";
import * as t from "@babel/types";

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

function locationKey(start: Position): string {
  return `${start.line}:${start.column}`;
}

export default function mapOriginalExpression(
  expression: string,
  ast: ?Object,
  mappings: {
    [string]: string | null,
  }
): string {
  const scopes = buildScopeList(ast, "");
  let shouldUpdate = false;

  const nodes = new Map();
  const replacements = new Map();

  // The ref-only global bindings are the ones that are accessed, but not
  // declared anywhere in the parsed code, meaning they are either global,
  // or declared somewhere in a scope outside the parsed code, so we
  // rewrite all of those specifically to avoid rewritting declarations that
  // shadow outer mappings.
  for (const name of Object.keys(scopes[0].bindings)) {
    const { refs } = scopes[0].bindings[name];
    const mapping = mappings[name];

    if (
      !refs.every(ref => ref.type === "ref") ||
      !mapping ||
      mapping === name
    ) {
      continue;
    }

    let node = nodes.get(name);
    if (!node) {
      node = getFirstExpression(parseScript(mapping));
      nodes.set(name, node);
    }

    for (const ref of refs) {
      let { line, column } = ref.start;

      // This shouldn't happen, just keeping Flow happy.
      if (typeof column !== "number") {
        column = 0;
      }

      replacements.set(locationKey({ line, column }), node);
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

    const ancestor = ancestors[ancestors.length - 1];
    // Shorthand properties can have a key and value with `node.loc.start` value
    // and we only want to replace the value.
    if (t.isObjectProperty(ancestor.node) && ancestor.key !== "value") {
      return;
    }

    const replacement = replacements.get(locationKey(node.loc.start));
    if (replacement) {
      replaceNode(ancestors, t.cloneNode(replacement));
      shouldUpdate = true;
    }
  });

  if (shouldUpdate) {
    return generate(ast).code;
  }

  return expression;
}
