/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that spread expressions work both in arrays and function calls.
 */

"use strict";

function test() {
  let { Parser, SyntaxTreeVisitor } =
    Cu.import("resource://devtools/shared/Parser.jsm", {});

  const SCRIPTS = ["[...a]", "foo(...a)"];

  for (let script of SCRIPTS) {
    info(`Testing spread expression in '${script}'`);
    let ast = Parser.reflectionAPI.parse(script);
    let nodes = SyntaxTreeVisitor.filter(ast,
      e => e.type == "SpreadExpression");
    ok(nodes && nodes.length === 1, "Found the SpreadExpression node");

    let expr = nodes[0].expression;
    ok(expr, "The SpreadExpression node has the sub-expression");
    is(expr.type, "Identifier", "The sub-expression is an Identifier");
    is(expr.name, "a", "The sub-expression identifier has a correct name");
  }

  finish();
}
