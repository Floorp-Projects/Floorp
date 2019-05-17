/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import generate from "@babel/generator";
import * as t from "@babel/types";

import { hasNode, replaceNode } from "./utils/ast";
import { isTopLevel } from "./utils/helpers";

function hasTopLevelAwait(ast: Object): boolean {
  const hasAwait = hasNode(
    ast,
    (node, ancestors, b) => t.isAwaitExpression(node) && isTopLevel(ancestors)
  );

  return hasAwait;
}

// translates new bindings `var a = 3` into `a = 3`.
function translateDeclarationIntoAssignment(node: Object): Object[] {
  return node.declarations.reduce((acc, declaration) => {
    // Don't translate declaration without initial assignment (e.g. `var a;`)
    if (!declaration.init) {
      return acc;
    }
    acc.push(
      t.expressionStatement(
        t.assignmentExpression("=", declaration.id, declaration.init)
      )
    );
    return acc;
  }, []);
}

/**
 * Given an AST, compute its last statement and replace it with a
 * return statement.
 */
function addReturnNode(ast: Object): Object {
  const statements = ast.program.body;
  const lastStatement = statements[statements.length - 1];
  return statements
    .slice(0, -1)
    .concat(t.returnStatement(lastStatement.expression));
}

function getDeclarations(node: Object) {
  const { kind, declarations } = node;
  const declaratorNodes = declarations.reduce((acc, d) => {
    const declarators = getVariableDeclarators(d.id);
    return acc.concat(declarators);
  }, []);

  // We can't declare const variables outside of the async iife because we
  // wouldn't be able to re-assign them. As a workaround, we transform them
  // to `let` which should be good enough for those case.
  return t.variableDeclaration(
    kind === "const" ? "let" : kind,
    declaratorNodes
  );
}

function getVariableDeclarators(node: Object): Object[] | Object {
  if (t.isIdentifier(node)) {
    return t.variableDeclarator(t.identifier(node.name));
  }

  if (t.isObjectProperty(node)) {
    return getVariableDeclarators(node.value);
  }
  if (t.isRestElement(node)) {
    return getVariableDeclarators(node.argument);
  }

  if (t.isAssignmentPattern(node)) {
    return getVariableDeclarators(node.left);
  }

  if (t.isArrayPattern(node)) {
    return node.elements.reduce(
      (acc, element) => acc.concat(getVariableDeclarators(element)),
      []
    );
  }
  if (t.isObjectPattern(node)) {
    return node.properties.reduce(
      (acc, property) => acc.concat(getVariableDeclarators(property)),
      []
    );
  }
  return [];
}

/**
 * Given an AST and an array of variableDeclaration nodes, return a new AST with
 * all the declarations at the top of the AST.
 */
function addTopDeclarationNodes(ast: Object, declarationNodes: Object[]) {
  const statements = [];
  declarationNodes.forEach(declarationNode => {
    statements.push(getDeclarations(declarationNode));
  });
  statements.push(ast);
  return t.program(statements);
}

/**
 * Given an AST, return an object of the following shape:
 *   - newAst: {AST} the AST where variable declarations were transformed into
 *             variable assignments
 *   - declarations: {Array<Node>} An array of all the declaration nodes needed
 *                   outside of the async iife.
 */
function translateDeclarationsIntoAssignment(
  ast: Object
): { newAst: Object, declarations: Node[] } {
  const declarations = [];
  t.traverse(ast, (node, ancestors) => {
    const parent = ancestors[ancestors.length - 1];

    if (
      t.isWithStatement(node) ||
      !isTopLevel(ancestors) ||
      t.isAssignmentExpression(node) ||
      !t.isVariableDeclaration(node) ||
      t.isForStatement(parent.node) ||
      !Array.isArray(node.declarations) ||
      node.declarations.length === 0
    ) {
      return;
    }

    const newNodes = translateDeclarationIntoAssignment(node);
    replaceNode(ancestors, newNodes);
    declarations.push(node);
  });

  return {
    newAst: ast,
    declarations,
  };
}

/**
 * Given an AST, wrap its body in an async iife, transform variable declarations
 * in assignments and move the variable declarations outside of the async iife.
 * Example: With the AST for the following expression: `let a = await 123`, the
 * function will return:
 * let a;
 * (async => {
 *   return a = await 123;
 * })();
 */
function wrapExpressionFromAst(ast: Object): string {
  // Transform let and var declarations into assignments, and get back an array
  // of variable declarations.
  let { newAst, declarations } = translateDeclarationsIntoAssignment(ast);
  const body = addReturnNode(newAst);

  // Create the async iife.
  newAst = t.expressionStatement(
    t.callExpression(
      t.arrowFunctionExpression([], t.blockStatement(body), true),
      []
    )
  );

  // Now let's put all the variable declarations at the top of the async iife.
  newAst = addTopDeclarationNodes(newAst, declarations);

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
