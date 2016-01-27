/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that dynamically created (HTML|XML|SVG)Documents can be inspected by
// clicking on the object in console (bug 1035198).

"use strict";

const TEST_CASES = [
  {
    input: '(new DOMParser()).parseFromString("<a />", "text/html")',
    output: "HTMLDocument",
    inspectable: true,
  },
  {
    input: '(new DOMParser()).parseFromString("<a />", "application/xml")',
    output: "XMLDocument",
    inspectable: true,
  },
  {
    input: '(new DOMParser()).parseFromString("<svg></svg>", "image/svg+xml")',
    output: "SVGDocument",
    inspectable: true,
  },
];

const TEST_URI = "data:text/html;charset=utf8," +
  "browser_webconsole_inspect-parsed-documents.js";
add_task(function* () {
  let {tab} = yield loadTab(TEST_URI);
  let hud = yield openConsole(tab);
  yield checkOutputForInputs(hud, TEST_CASES);
});
