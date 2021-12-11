/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that minified sheets are automatically prettified but other are left
// untouched.

const TESTCASE_URI = TEST_BASE_HTTP + "minified.html";

/*
  body {
    background:white;
  }
  div {
    font-size:4em;
    color:red
  }
  span {
    color:green;
  }
*/
const PRETTIFIED_SOURCE =
  "" +
  "body {\r?\n" +
  "\tbackground:white;\r?\n" +
  "}\r?\n" +
  "div {\r?\n" +
  "\tfont-size:4em;\r?\n" +
  "\tcolor:red\r?\n" +
  "}\r?\n" +
  "span {\r?\n" +
  "\tcolor:green;\r?\n" +
  "}\r?\n";

/*
  body { background: red; }
  div {
    font-size: 5em;
    color: red
  }
*/
const ORIGINAL_SOURCE =
  "" +
  "body { background: red; }\r?\n" +
  "div {\r?\n" +
  "font-size: 5em;\r?\n" +
  "color: red\r?\n" +
  "}";

const EXPAND_TAB = "devtools.editor.expandtab";

add_task(async function() {
  const oldExpandTabPref = SpecialPowers.getBoolPref(EXPAND_TAB);
  // The 'EXPAND_TAB' preference has to be set to false because
  // the constant 'PRETTIFIED_SOURCE' uses tabs for indentation.
  SpecialPowers.setBoolPref(EXPAND_TAB, false);

  const { ui } = await openStyleEditorForURL(TESTCASE_URI);
  is(ui.editors.length, 2, "Two sheets present.");

  info("Testing minified style sheet.");
  let editor = await ui.editors[0].getSourceEditor();

  const prettifiedSourceRE = new RegExp(PRETTIFIED_SOURCE);
  ok(
    prettifiedSourceRE.test(editor.sourceEditor.getText()),
    "minified source has been prettified automatically"
  );

  info("Selecting second, non-minified style sheet.");
  await ui.selectStyleSheet(ui.editors[1].styleSheet);

  editor = ui.editors[1];

  const originalSourceRE = new RegExp(ORIGINAL_SOURCE);
  ok(
    originalSourceRE.test(editor.sourceEditor.getText()),
    "non-minified source has been left untouched"
  );

  SpecialPowers.setBoolPref(EXPAND_TAB, oldExpandTabPref);
});
