/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function test() {
  waitForExplicitFinish();
  setup((ed, win) => {
    ok(ed.isClean(), "default isClean");
    ok(!ed.canUndo(), "default canUndo");
    ok(!ed.canRedo(), "default canRedo");

    ed.setText("Hello, World!");
    ok(!ed.isClean(), "isClean");
    ok(ed.canUndo(), "canUndo");
    ok(!ed.canRedo(), "canRedo");

    ed.undo();
    ok(ed.isClean(), "isClean after undo");
    ok(!ed.canUndo(), "canUndo after undo");
    ok(ed.canRedo(), "canRedo after undo");

    ed.setText("What's up?");
    ed.setClean();
    ok(ed.isClean(), "isClean after setClean");
    ok(ed.canUndo(), "canUndo after setClean");
    ok(!ed.canRedo(), "canRedo after setClean");

    teardown(ed, win);
  });
}
