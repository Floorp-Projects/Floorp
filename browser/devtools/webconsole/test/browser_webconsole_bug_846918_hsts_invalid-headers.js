 /* Any copyright is dedicated to the Public Domain.
  * http://creativecommons.org/publicdomain/zero/1.0/ */
/* Tests that errors about invalid HSTS security headers are logged
 *  to the web console */

"use strict";

const TEST_URI = "https://example.com/browser/browser/devtools/webconsole/" +
                 "test/test-bug-846918-hsts-invalid-headers.html";
const HSTS_INVALID_HEADER_MSG = "The site specified an invalid " +
                                "Strict-Transport-Security header.";
const LEARN_MORE_URI = "https://developer.mozilla.org/docs/Security/HTTP_Strict_Transport_Security";

let test = asyncTest(function* () {
  yield loadTab(TEST_URI);

  let hud = yield openConsole();

  let results = yield waitForMessages({
    webconsole: hud,
    messages: [
      {
        name: "Invalid HSTS header error displayed successfully",
        text: HSTS_INVALID_HEADER_MSG,
        category: CATEGORY_SECURITY,
        severity: SEVERITY_WARNING,
        objects: true,
      },
    ],
  });

  yield testClickOpenNewTab(hud, results);
});

function testClickOpenNewTab(hud, results) {
  let warningNode = results[0].clickableElements[0];
  ok(warningNode, "link element");
  ok(warningNode.classList.contains("learn-more-link"), "link class name");
  return simulateMessageLinkClick(warningNode, LEARN_MORE_URI);
}
