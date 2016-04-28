/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that sheets inside iframes are shown in the editor.

add_task(function* () {
  function makeStylesheet(selector) {
    return ("data:text/css;charset=UTF-8," +
            encodeURIComponent(selector + " { }"));
  }

  function makeDocument(stylesheets, framedDocuments) {
    stylesheets = stylesheets || [];
    framedDocuments = framedDocuments || [];
    return "data:text/html;charset=UTF-8," + encodeURIComponent(
      Array.prototype.concat.call(
        ["<!DOCTYPE html>",
         "<html>",
         "<head>",
         "<title>Bug 740541</title>"],
        stylesheets.map(function (sheet) {
          return '<link rel="stylesheet" type="text/css" href="' + sheet + '">';
        }),
        ["</head>",
         "<body>"],
        framedDocuments.map(function (doc) {
          return '<iframe src="' + doc + '"></iframe>';
        }),
        ["</body>",
         "</html>"]
      ).join("\n"));
  }

  const DOCUMENT_WITH_INLINE_STYLE = "data:text/html;charset=UTF-8," +
          encodeURIComponent(
            ["<!DOCTYPE html>",
             "<html>",
             " <head>",
             "  <title>Bug 740541</title>",
             '  <style type="text/css">',
             "    .something {",
             "    }",
             "  </style>",
             " </head>",
             " <body>",
             " </body>",
             " </html>"
            ].join("\n"));

  const FOUR = TEST_BASE_HTTP + "four.html";

  const SIMPLE = TEST_BASE_HTTP + "simple.css";

  const SIMPLE_DOCUMENT = TEST_BASE_HTTP + "simple.html";

  const TESTCASE_URI = makeDocument(
    [makeStylesheet(".a")],
    [makeDocument([],
                  [FOUR,
                   DOCUMENT_WITH_INLINE_STYLE]),
     makeDocument([makeStylesheet(".b"),
                   SIMPLE],
                  [makeDocument([makeStylesheet(".c")],
                                [])]),
     makeDocument([SIMPLE], []),
     SIMPLE_DOCUMENT
    ]);

  const EXPECTED_STYLE_SHEET_COUNT = 12;

  let { ui } = yield openStyleEditorForURL(TESTCASE_URI);

  is(ui.editors.length, EXPECTED_STYLE_SHEET_COUNT,
    "Got the expected number of style sheets.");
});
