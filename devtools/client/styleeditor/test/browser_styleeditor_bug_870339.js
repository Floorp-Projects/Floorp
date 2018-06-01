/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const SIMPLE = TEST_BASE_HTTP + "simple.css";
const DOCUMENT_WITH_ONE_STYLESHEET = "data:text/html;charset=UTF-8," +
        encodeURIComponent(
          ["<!DOCTYPE html>",
           "<html>",
           " <head>",
           "  <title>Bug 870339</title>",
           '  <link rel="stylesheet" type="text/css" href="' + SIMPLE + '">',
           " </head>",
           " <body>",
           " </body>",
           "</html>"
          ].join("\n"));

add_task(async function() {
  const { ui } = await openStyleEditorForURL(DOCUMENT_WITH_ONE_STYLESHEET);

  // Spam the _onNewDocument callback multiple times before the
  // StyleEditorActor has a chance to respond to the first one.
  const SPAM_COUNT = 2;
  for (let i = 0; i < SPAM_COUNT; ++i) {
    ui._onNewDocument();
  }

  // Wait for the StyleEditorActor to respond to each "newDocument"
  // message.
  await new Promise(resolve => {
    let loadCount = 0;
    ui.on("stylesheets-reset", function onReset() {
      ++loadCount;
      if (loadCount == SPAM_COUNT) {
        ui.off("stylesheets-reset", onReset);
        // No matter how large SPAM_COUNT is, the number of style
        // sheets should never be more than the number of style sheets
        // in the document.
        is(ui.editors.length, 1, "correct style sheet count");
        resolve();
      }
    });
  });
});
