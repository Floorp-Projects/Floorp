/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that style editor shows sheets loaded with @import rules.

// http rather than chrome to improve coverage
const TESTCASE_URI = TEST_BASE_HTTP + "import.html";

add_task(function* () {
  let { ui } = yield openStyleEditorForURL(TESTCASE_URI);

  is(ui.editors.length, 3,
    "there are 3 stylesheets after loading @imports");

  is(ui.editors[0].styleSheet.href, TEST_BASE_HTTP + "simple.css",
    "stylesheet 1 is simple.css");

  is(ui.editors[1].styleSheet.href, TEST_BASE_HTTP + "import.css",
    "stylesheet 2 is import.css");

  is(ui.editors[2].styleSheet.href, TEST_BASE_HTTP + "import2.css",
    "stylesheet 3 is import2.css");
});
