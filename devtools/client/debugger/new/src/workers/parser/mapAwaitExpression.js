/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import generate from "@babel/generator";
import * as t from "@babel/types";

import { hasNode } from "./utils/ast";
import { isTopLevel } from "./utils/helpers";

function hasTopLevelAwait(ast: Object): boolean {
  const hasAwait = hasNode(
    ast,
    (node, ancestors, b) => t.isAwaitExpression(node) && isTopLevel(ancestors)
  );

  return hasAwait;
}

function wrapExpressionFromAst(ast): string {
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

export default function mapTopLevelAwait(
  expression: string,
  ast?: Object
): string {
  if (!ast) {
    // If there's no ast this means the expression is malformed. And if the
    // expression contains the await keyword, we still want to wrap it in an
    // async iife in order to get a meaningful message (without this, the
    // engine will throw an Error stating that await keywords are only valid
    // in async functions and generators).
    if (expression.includes("await ")) {
      return `(async () => { ${expression} })();`;
    }

    return expression;
  }

  if (!hasTopLevelAwait(ast)) {
    return expression;
  }

  return wrapExpressionFromAst(ast);
}
