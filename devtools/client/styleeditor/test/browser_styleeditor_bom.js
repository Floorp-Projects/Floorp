/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const BOM_CSS = TEST_BASE_HTTPS + "utf-16.css";
const DOCUMENT = "data:text/html;charset=UTF-8," +
        encodeURIComponent(
          ["<!DOCTYPE html>",
           "<html>",
           " <head>",
           "  <title>Bug 1301854</title>",
           '  <link rel="stylesheet" type="text/css" href="' + BOM_CSS + '">',
           " </head>",
           " <body>",
           " </body>",
           "</html>"
          ].join("\n"));

const CONTENTS = "// Note that this file must be utf-16 with a " +
      "BOM for the test to make sense.\n";

add_task(function* () {
  let {ui} = yield openStyleEditorForURL(DOCUMENT);

  is(ui.editors.length, 1, "correct number of editors");

  let editor = ui.editors[0];
  yield editor.getSourceEditor();

  let text = editor.sourceEditor.getText();
  is(text, CONTENTS, "editor contains expected text");
});
