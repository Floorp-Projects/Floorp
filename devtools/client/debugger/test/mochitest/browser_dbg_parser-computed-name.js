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

  let ast = Parser.reflectionAPI.parse("({ [i]: 1 })");
  let nodes = SyntaxTreeVisitor.filter(ast, e => e.type == "ComputedName");
  ok(nodes && nodes.length === 1, "Found the ComputedName node");

  let name = nodes[0].name;
  ok(name, "The ComputedName node has a name property");
  is(name.type, "Identifier", "The name has a correct type");
  is(name.name, "i", "The name has a correct name");

  let identNodes = SyntaxTreeVisitor.filter(ast, e => e.type == "Identifier");
  ok(identNodes && identNodes.length === 1, "Found the Identifier node");

  is(identNodes[0].type, "Identifier", "The identifier has a correct type");
  is(identNodes[0].name, "i", "The identifier has a correct name");

  finish();
}
