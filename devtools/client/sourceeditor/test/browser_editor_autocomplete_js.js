/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test to make sure that JS autocompletion is opening popups.
function test() {
  waitForExplicitFinish();
  setup((ed, win) => {
    let edWin = ed.container.contentWindow.wrappedJSObject;
    testJS(ed, edWin).then(() => {
      teardown(ed, win);
    });
  });
}

function testJS(ed, win) {
  ok (!ed.getOption("autocomplete"), "Autocompletion is not set");
  ok (!win.tern, "Tern is not defined on the window");

  ed.setMode(Editor.modes.js);
  ed.setOption("autocomplete", true);

  ok (ed.getOption("autocomplete"), "Autocompletion is set");
  ok (win.tern, "Tern is defined on the window");

  ed.focus();
  ed.setText("document.");
  ed.setCursor({line: 0, ch: 9});

  let waitForSuggestion = promise.defer();

  ed.on("before-suggest", () => {
    info("before-suggest has been triggered");
    EventUtils.synthesizeKey("VK_ESCAPE", { }, win);
    waitForSuggestion.resolve();
  });

  let autocompleteKey = Editor.keyFor("autocompletion", { noaccel: true }).toUpperCase();
  EventUtils.synthesizeKey("VK_" + autocompleteKey, { ctrlKey: true }, win);

  return waitForSuggestion.promise;
}
