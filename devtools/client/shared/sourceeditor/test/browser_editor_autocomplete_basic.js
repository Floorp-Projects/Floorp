/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const AUTOCOMPLETION_PREF = "devtools.editor.autocomplete";

// Test to make sure that different autocompletion modes can be created,
// switched, and destroyed.  This doesn't test the actual autocompletion
// popups, only their integration with the editor.
function test() {
  waitForExplicitFinish();
  setup((ed, win) => {
    const edWin = ed.container.contentWindow.wrappedJSObject;
    testJS(ed, edWin);
    testCSS(ed, edWin);
    testPref(ed, edWin);
    teardown(ed, win);
  });
}

function testJS(ed, win) {
  ok(!ed.getOption("autocomplete"), "Autocompletion is not set");
  ok(!win.tern, "Tern is not defined on the window");

  ed.setMode(Editor.modes.js);
  ed.setOption("autocomplete", true);

  ok(ed.getOption("autocomplete"), "Autocompletion is set");
  ok(win.tern, "Tern is defined on the window");
}

function testCSS(ed, win) {
  ok(ed.getOption("autocomplete"), "Autocompletion is set");
  ok(win.tern, "Tern is currently defined on the window");

  ed.setMode(Editor.modes.css);
  ed.setOption("autocomplete", true);

  ok(ed.getOption("autocomplete"), "Autocompletion is still set");
  ok(!win.tern, "Tern is no longer defined on the window");
}

function testPref(ed, win) {
  ed.setMode(Editor.modes.js);
  ed.setOption("autocomplete", true);

  ok(ed.getOption("autocomplete"), "Autocompletion is set");
  ok(win.tern, "Tern is defined on the window");

  info("Preffing autocompletion off");
  Services.prefs.setBoolPref(AUTOCOMPLETION_PREF, false);

  ok(ed.getOption("autocomplete"), "Autocompletion is still set");
  ok(!win.tern, "Tern is no longer defined on the window");

  Services.prefs.clearUserPref(AUTOCOMPLETION_PREF);
}
