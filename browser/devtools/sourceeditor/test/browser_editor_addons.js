/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function test() {
  waitForExplicitFinish();

  setup((ed, win) => {
    let doc = win.document.querySelector("iframe").contentWindow.document;

    // trailingspace.js
    ed.setText("Hello   ");
    ed.setOption("showTrailingSpace", false);
    ok(!doc.querySelector(".cm-trailingspace"));
    ed.setOption("showTrailingSpace", true);
    ok(doc.querySelector(".cm-trailingspace"));

    // foldcode.js and foldgutter.js
    ed.setMode(Editor.modes.js);
    ed.setText("function main() {\nreturn 'Hello, World!';\n}");
    executeSoon(() => testFold(doc, ed, win));
  });
}

function testFold(doc, ed, win) {
  // Wait until folding arrow is there.
  if (!doc.querySelector(".CodeMirror-foldgutter-open")) {
    executeSoon(() => testFold(doc, ed, win));
    return;
  }

  teardown(ed, win);
}