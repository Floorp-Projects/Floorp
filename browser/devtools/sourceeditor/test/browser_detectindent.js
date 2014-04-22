/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";


const TWO_SPACES_CODE = [
"/*",
" * tricky comment block",
" */",
"div {",
"  color: red;",
"  background: blue;",
"}",
"     ",
"span {",
"  padding-left: 10px;",
"}"
].join("\n");

const FOUR_SPACES_CODE = [
"var obj = {",
"    addNumbers: function() {",
"        var x = 5;",
"        var y = 18;",
"        return x + y;",
"    },",
"   ",
"    /*",
"     * Do some stuff to two numbers",
"     * ",
"     * @param x",
"     * @param y",
"     * ",
"     * @return the result of doing stuff",
"     */",
"    subtractNumbers: function(x, y) {",
"        var x += 7;",
"        var y += 18;",
"        var result = x - y;",
"        result %= 2;",
"    }",
"}"
].join("\n");

const TABS_CODE = [
"/*",
" * tricky comment block",
" */",
"div {",
"\tcolor: red;",
"\tbackground: blue;",
"}",
"",
"span {",
"\tpadding-left: 10px;",
"}"
].join("\n");


function test() {
  waitForExplicitFinish();

  setup((ed, win) => {
    is(ed.getOption("indentUnit"), 2,
       "2 spaces before code added");
    is(ed.getOption("indentWithTabs"), false,
       "spaces is default");

    ed.setText(FOUR_SPACES_CODE);
    is(ed.getOption("indentUnit"), 4,
       "4 spaces detected in 4 space code");
    is(ed.getOption("indentWithTabs"), false,
       "spaces detected in 4 space code");

    ed.setText(TWO_SPACES_CODE);
    is(ed.getOption("indentUnit"), 2,
       "2 spaces detected in 2 space code");
    is(ed.getOption("indentWithTabs"), false,
       "spaces detected in 2 space code");

    ed.setText(TABS_CODE);
    is(ed.getOption("indentUnit"), 2,
       "2 space indentation unit");
    is(ed.getOption("indentWithTabs"), true,
       "tabs detected in majority tabs code");

    teardown(ed, win);
  });
}
