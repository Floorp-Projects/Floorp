/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Ensure Alt-B and Alt-F keyboard shortcuts work as expected in the source editor.
// See Bug 1481443.

add_task(async function () {
  const { ed, win } = await setup();
  const editorDoc = ed.container.contentDocument;
  await promiseWaitForFocus();
  const isMacOS = Services.appinfo.OS === "Darwin";

  ed.focus();

  const initialText = "a b c d e";
  ed.setText(initialText);

  ed.setCursor({ line: 1, ch: initialText.length });

  EventUtils.synthesizeKey("b", { altKey: true }, editorDoc.defaultView);

  // A character is added only on OSX.
  let expectedText = isMacOS ? initialText + "b" : initialText;
  is(
    ed.getCursor().ch,
    expectedText.length,
    "Cursor is at expected position after Alt-B"
  );
  is(ed.getText(), expectedText, "Editor has expected content after Alt-B");

  EventUtils.synthesizeKey("f", { altKey: true }, editorDoc.defaultView);

  // A character is added only on OSX.
  expectedText = isMacOS ? expectedText + "f" : initialText;
  is(
    ed.getCursor().ch,
    expectedText.length,
    "Cursor is at expected position after Alt-F"
  );
  is(ed.getText(), expectedText, "Editor has expected content after Alt-F");

  ed.destroy();
  win.close();
});
