/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import generate from "@babel/generator";
import * as t from "@babel/types";

import { parseConsoleScript, hasNode } from "./utils/ast";
import { isTopLevel } from "./utils/helpers";

function hasTopLevelAwait(expression: string) {
  const ast = parseConsoleScript(expression);
  const hasAwait = hasNode(
    ast,
    (node, ancestors, b) => t.isAwaitExpression(node) && isTopLevel(ancestors)
  );

  return hasAwait && ast;
}

function wrapExpression(ast) {
  const statements = ast.program.body;
  const lastStatement = statements[statements.length - 1];
  const body = statements
    .slice(0, -1)
    .concat(t.returnStatement(lastStatement.expression));

  const newAst = t.expressionStatement(
    t.callExpression(
      t.arrowFunctionExpression([], t.blockStatement(body), true),
      []
    )
  );

  return generate(newAst).code;
}

export default function mapTopLevelAwait(expression: string) {
  const ast = hasTopLevelAwait(expression);
  if (ast) {
    return wrapExpression(ast);
  }

  return expression;
}
