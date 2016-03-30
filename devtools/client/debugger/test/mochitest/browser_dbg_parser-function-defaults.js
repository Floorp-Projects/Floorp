/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that function default arguments are correctly processed.
 */

"use strict";

function test() {
  let { Parser, ParserHelpers, SyntaxTreeVisitor } =
    Cu.import("resource://devtools/shared/Parser.jsm", {});

  function verify(source, predicate, string) {
    let ast = Parser.reflectionAPI.parse(source);
    let node = SyntaxTreeVisitor.filter(ast, predicate).pop();
    let info = ParserHelpers.getIdentifierEvalString(node);
    is(info, string, "The identifier evaluation string is correct.");
  }

  // FunctionDeclaration
  verify("function foo(a, b='b') {}", e => e.type == "Literal", "\"b\"");
  // FunctionExpression
  verify("let foo=function(a, b='b') {}", e => e.type == "Literal", "\"b\"");
  // ArrowFunctionExpression
  verify("let foo=(a, b='b')=> {}", e => e.type == "Literal", "\"b\"");

  finish();
}
