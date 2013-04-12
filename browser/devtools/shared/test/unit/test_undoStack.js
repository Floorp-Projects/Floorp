/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const Cu = Components.utils;
let {Loader} = Cu.import("resource://gre/modules/commonjs/toolkit/loader.js", {});

let loader = new Loader.Loader({
  paths: {
    "": "resource://gre/modules/commonjs/",
    "devtools": "resource:///modules/devtools",
  },
  globals: {},
});
let require = Loader.Require(loader, { id: "undo-test" })

let {UndoStack} = require("devtools/shared/undo");

const MAX_SIZE = 5;

function run_test()
{
  let str = "";
  let stack = new UndoStack(MAX_SIZE);

  function add(ch) {
    stack.do(function() {
      str += ch;
    }, function() {
      str = str.slice(0, -1);
    });
  }

  do_check_false(stack.canUndo());
  do_check_false(stack.canRedo());

  // Check adding up to the limit of the size
  add("a");
  do_check_true(stack.canUndo());
  do_check_false(stack.canRedo());

  add("b");
  add("c");
  add("d");
  add("e");

  do_check_eq(str, "abcde");

  // Check a simple undo+redo
  stack.undo();

  do_check_eq(str, "abcd");
  do_check_true(stack.canRedo());

  stack.redo();
  do_check_eq(str, "abcde")
  do_check_false(stack.canRedo());

  // Check an undo followed by a new action
  stack.undo();
  do_check_eq(str, "abcd");

  add("q");
  do_check_eq(str, "abcdq");
  do_check_false(stack.canRedo());

  stack.undo();
  do_check_eq(str, "abcd");
  stack.redo();
  do_check_eq(str, "abcdq");

  // Revert back to the beginning of the queue...
  while (stack.canUndo()) {
    stack.undo();
  }
  do_check_eq(str, "");

  // Now put it all back....
  while (stack.canRedo()) {
    stack.redo();
  }
  do_check_eq(str, "abcdq");

  // Now go over the undo limit...
  add("1");
  add("2");
  add("3");

  do_check_eq(str, "abcdq123");

  // And now undoing the whole stack should only undo 5 actions.
  while (stack.canUndo()) {
    stack.undo();
  }

  do_check_eq(str, "abc");
}
