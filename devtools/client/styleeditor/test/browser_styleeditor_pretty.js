/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that minified sheets are automatically prettified but other are left
// untouched.

const TESTCASE_URI = TEST_BASE_HTTP + "minified.html";

const PRETTIFIED_CSS_TEXT = `
body {
  background:white;
}
div {
  font-size:4em;
  color:red
}
span {
  color:green;
  @media screen {
    background: blue;
    &>.myClass {
      padding: 1em
    }
  }
}
`.trimStart();

const NON_MINIFIED_CSS_TEXT = `
body { background: red; }
div {
font-size: 5em;
color: red
}`.trimStart();

add_task(async function () {
  // Use 2 spaces for indent
  await pushPref("devtools.editor.expandtab", true);
  await pushPref("devtools.editor.tabsize", 2);

  const { ui } = await openStyleEditorForURL(TESTCASE_URI);
  is(ui.editors.length, 2, "Two sheets present.");

  info("Testing minified style sheet.");
  let editor = await ui.editors[0].getSourceEditor();

  is(
    editor.sourceEditor.getText(),
    PRETTIFIED_CSS_TEXT,
    "minified source has been prettified automatically"
  );

  info("Selecting second, non-minified style sheet.");
  await ui.selectStyleSheet(ui.editors[1].styleSheet);

  editor = ui.editors[1];

  is(
    editor.sourceEditor.getText(),
    NON_MINIFIED_CSS_TEXT,
    "non-minified source has been left untouched"
  );
});
