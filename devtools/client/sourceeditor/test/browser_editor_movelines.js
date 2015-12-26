/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function test() {
  waitForExplicitFinish();
  setup((ed, win) => {
    let simpleProg = "function foo() {\n  let i = 1;\n  let j = 2;\n  " +
                     "return bar;\n}";
    ed.setText(simpleProg);

    // Move first line up
    ed.setCursor({ line: 0, ch: 0 });
    ed.moveLineUp();
    is(ed.getText(0), "function foo() {", "getText(num)");
    ch(ed.getCursor(), { line: 0, ch: 0 }, "getCursor");

    // Move last line down
    ed.setCursor({ line: 4, ch: 0 });
    ed.moveLineDown();
    is(ed.getText(4), "}", "getText(num)");
    ch(ed.getCursor(), { line: 4, ch: 0 }, "getCursor");

    // Move line 2 up
    ed.setCursor({ line: 1, ch: 5});
    ed.moveLineUp();
    is(ed.getText(0), "  let i = 1;", "getText(num)");
    is(ed.getText(1), "function foo() {", "getText(num)");
    ch(ed.getCursor(), { line: 0, ch: 5 }, "getCursor");

    // Undo previous move by moving line 1 down
    ed.moveLineDown();
    is(ed.getText(0), "function foo() {", "getText(num)");
    is(ed.getText(1), "  let i = 1;", "getText(num)");
    ch(ed.getCursor(), { line: 1, ch: 5 }, "getCursor");

    // Move line 2 and 3 up
    ed.setSelection({ line: 1, ch: 0 }, { line: 2, ch: 0 });
    ed.moveLineUp();
    is(ed.getText(0), "  let i = 1;", "getText(num)");
    is(ed.getText(1), "  let j = 2;", "getText(num)");
    is(ed.getText(2), "function foo() {", "getText(num)");
    ch(ed.getCursor("start"), { line: 0, ch: 0 }, "getCursor(string)");
    ch(ed.getCursor("end"), { line: 1, ch: 0 }, "getCursor(string)");

    // Move line 1 to 3 down twice
    ed.dropSelection();
    ed.setSelection({ line: 0, ch: 7 }, { line: 2, ch: 5 });
    ed.moveLineDown();
    ed.moveLineDown();
    is(ed.getText(0), "  return bar;", "getText(num)");
    is(ed.getText(1), "}", "getText(num)");
    is(ed.getText(2), "  let i = 1;", "getText(num)");
    is(ed.getText(3), "  let j = 2;", "getText(num)");
    is(ed.getText(4), "function foo() {", "getText(num)");
    ch(ed.getCursor("start"), { line: 2, ch: 7 }, "getCursor(string)");
    ch(ed.getCursor("end"), { line: 4, ch: 5 }, "getCursor(string)");

    teardown(ed, win);
  });
}
