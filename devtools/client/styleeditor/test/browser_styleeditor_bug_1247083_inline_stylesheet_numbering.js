/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that the displayed numbering of inline and user-created stylesheets are independent of their absolute index
// See bug 1247083.

const SIMPLE = TEST_BASE_HTTP + "simple.css";
const LONG = TEST_BASE_HTTP + "doc_long.css";
const DOCUMENT_WITH_LONG_SHEET =
  "data:text/html;charset=UTF-8," +
  encodeURIComponent(
    [
      "<!DOCTYPE html>",
      "<html>",
      " <head>",
      "  <title>Style editor numbering test page</title>",

      // first inline stylesheet
      "  <style>",
      "   #p-blue {",
      "    color: blue;",
      "   }",
      "  </style>",
      // first external stylesheet
      '  <link rel="stylesheet" type="text/css" href="' + SIMPLE + '">',
      // second external stylesheet
      '  <link rel="stylesheet" type="text/css" href="' + LONG + '">',
      // second inline stylesheet
      "  <style>",
      "   #p-green {",
      "    color: green;",
      "   }",
      "   #p-red {",
      "    color: red;",
      "   }",
      "  </style>",

      " </head>",
      " <body>",
      " </body>",
      "</html>",
    ].join("\n")
  );

add_task(async function () {
  info("Test that inline stylesheets are numbered correctly");
  const { ui } = await openStyleEditorForURL(DOCUMENT_WITH_LONG_SHEET);

  is(ui.editors.length, 4, "4 editors present.");

  const firstEditor = ui.editors[0];
  is(
    firstEditor.styleSheetFriendlyIndex,
    0,
    "1st inline stylesheet's index is 0"
  );

  is(
    firstEditor.styleSheet.styleSheetIndex,
    0,
    "1st inline stylesheet is also the first stylesheet declared"
  );

  is(firstEditor.styleSheet.ruleCount, 1, "1st inline stylesheet has 1 rule");

  const secondEditor = ui.editors[3];
  is(
    secondEditor.styleSheetFriendlyIndex,
    1,
    "2nd inline stylesheet's index is 1"
  );

  is(
    secondEditor.styleSheet.styleSheetIndex,
    3,
    "2nd inline stylesheet is the last stylesheet"
  );

  is(secondEditor.styleSheet.ruleCount, 2, "2nd inline stylesheet has 2 rules");
});

add_task(async function () {
  info("Test that user-created stylesheets are numbered correctly");
  const { panel, ui } = await openStyleEditorForURL(DOCUMENT_WITH_LONG_SHEET);
  await createNewStyleSheet(ui, panel.panelWindow);
  await createNewStyleSheet(ui, panel.panelWindow);

  is(ui.editors.length, 6, "6 editors present.");

  ok(ui.editors[4].isNew, "2nd to last editor is user-created");
  is(
    ui.editors[4].styleSheetFriendlyIndex,
    0,
    "2nd to last user created stylesheet's index is 0"
  );

  ok(ui.editors[5].isNew, "Last editor is user-created");
  is(
    ui.editors[5].styleSheetFriendlyIndex,
    1,
    "Last user created stylesheet's index is 1"
  );
});
