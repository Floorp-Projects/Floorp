/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {Loader, Require} =
  Components.utils.import("resource://devtools/shared/base-loader.js", {});

const loader = new Loader({
  paths: {
    "devtools": "resource://devtools",
  },
  globals: {},
});
const require = Require(loader, { id: "undo-test" });

const {UndoStack} = require("devtools/client/shared/undo");

const MAX_SIZE = 5;

function run_test() {
  let str = "";
  let stack = new UndoStack(MAX_SIZE);

  function add(ch) {
    stack.do(function () {
      str += ch;
    }, function () {
      str = str.slice(0, -1);
    });
  }

  Assert.ok(!stack.canUndo());
  Assert.ok(!stack.canRedo());

  // Check adding up to the limit of the size
  add("a");
  Assert.ok(stack.canUndo());
  Assert.ok(!stack.canRedo());

  add("b");
  add("c");
  add("d");
  add("e");

  Assert.equal(str, "abcde");

  // Check a simple undo+redo
  stack.undo();

  Assert.equal(str, "abcd");
  Assert.ok(stack.canRedo());

  stack.redo();
  Assert.equal(str, "abcde");
  Assert.ok(!stack.canRedo());

  // Check an undo followed by a new action
  stack.undo();
  Assert.equal(str, "abcd");

  add("q");
  Assert.equal(str, "abcdq");
  Assert.ok(!stack.canRedo());

  stack.undo();
  Assert.equal(str, "abcd");
  stack.redo();
  Assert.equal(str, "abcdq");

  // Revert back to the beginning of the queue...
  while (stack.canUndo()) {
    stack.undo();
  }
  Assert.equal(str, "");

  // Now put it all back....
  while (stack.canRedo()) {
    stack.redo();
  }
  Assert.equal(str, "abcdq");

  // Now go over the undo limit...
  add("1");
  add("2");
  add("3");

  Assert.equal(str, "abcdq123");

  // And now undoing the whole stack should only undo 5 actions.
  while (stack.canUndo()) {
    stack.undo();
  }

  Assert.equal(str, "abc");
}
