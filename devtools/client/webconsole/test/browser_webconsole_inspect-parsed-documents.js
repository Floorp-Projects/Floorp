/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
