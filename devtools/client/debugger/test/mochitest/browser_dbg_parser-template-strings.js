/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that template strings are correctly processed.
 */

"use strict";

function test() {
  let { Parser, SyntaxTreeVisitor } =
    Cu.import("resource://devtools/shared/Parser.jsm", {});

  let ast = Parser.reflectionAPI.parse("`foo${i}bar`");
  let nodes = SyntaxTreeVisitor.filter(ast, e => e.type == "TemplateLiteral");
  ok(nodes && nodes.length === 1, "Found the TemplateLiteral node");

  let elements = nodes[0].elements;
  ok(elements, "The TemplateLiteral node has elements");
  is(elements.length, 3, "There are 3 elements in the literal");

  ["Literal", "Identifier", "Literal"].forEach((type, i) => {
    is(elements[i].type, type, `Element at index ${i} is '${type}'`);
  });

  finish();
}
