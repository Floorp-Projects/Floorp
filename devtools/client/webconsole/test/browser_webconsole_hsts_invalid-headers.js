/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests that errors about invalid HSTS security headers are logged
//  to the web console.

"use strict";

const TEST_URI = "data:text/html;charset=utf-8,Web Console HSTS invalid " +
                 "header test";
const SJS_URL = "https://example.com/browser/devtools/client/webconsole/" +
                "test/test_hsts-invalid-headers.sjs";
const LEARN_MORE_URI = "https://developer.mozilla.org/docs/Security/" +
                       "HTTP_Strict_Transport_Security";

add_task(function* () {
  yield loadTab(TEST_URI);

  let hud = yield openConsole();

  yield* checkForMessage({
    url: SJS_URL + "?badSyntax",
    name: "Could not parse header error displayed successfully",
    text: "Strict-Transport-Security: The site specified a header that could " +
          "not be parsed successfully."
  }, hud);

  yield* checkForMessage({
    url: SJS_URL + "?noMaxAge",
    name: "No max-age error displayed successfully",
    text: "Strict-Transport-Security: The site specified a header that did " +
          "not include a 'max-age' directive."
  }, hud);

  yield* checkForMessage({
    url: SJS_URL + "?invalidIncludeSubDomains",
    name: "Invalid includeSubDomains error displayed successfully",
    text: "Strict-Transport-Security: The site specified a header that " +
          "included an invalid 'includeSubDomains' directive."
  }, hud);

  yield* checkForMessage({
    url: SJS_URL + "?invalidMaxAge",
    name: "Invalid max-age error displayed successfully",
    text: "Strict-Transport-Security: The site specified a header that " +
          "included an invalid 'max-age' directive."
  }, hud);

  yield* checkForMessage({
    url: SJS_URL + "?multipleIncludeSubDomains",
    name: "Multiple includeSubDomains error displayed successfully",
    text: "Strict-Transport-Security: The site specified a header that " +
          "included multiple 'includeSubDomains' directives."
  }, hud);

  yield* checkForMessage({
    url: SJS_URL + "?multipleMaxAge",
    name: "Multiple max-age error displayed successfully",
    text: "Strict-Transport-Security: The site specified a header that " +
          "included multiple 'max-age' directives."
  }, hud);
});

function* checkForMessage(curTest, hud) {
  hud.jsterm.clearOutput();

  content.location = curTest.url;

  let results = yield waitForMessages({
    webconsole: hud,
    messages: [
      {
        name: curTest.name,
        text: curTest.text,
        category: CATEGORY_SECURITY,
        severity: SEVERITY_WARNING,
        objects: true,
      },
    ],
  });

  yield testClickOpenNewTab(hud, results);
}

function testClickOpenNewTab(hud, results) {
  let warningNode = results[0].clickableElements[0];
  ok(warningNode, "link element");
  ok(warningNode.classList.contains("learn-more-link"), "link class name");
  return simulateMessageLinkClick(warningNode, LEARN_MORE_URI);
}
