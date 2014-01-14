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

    teardown(ed, win);
  });
}