/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// The test loads a web page with mixed active and display content
// on it while the "block mixed content" settings are _off_.
// It then checks that the loading mixed content warning messages
// are logged to the console and have the correct "Learn More"
// url appended to them.
// Bug 875456 - Log mixed content messages from the Mixed Content
// Blocker to the Security Pane in the Web Console

"use strict";

const TEST_URI = "https://example.com/browser/browser/devtools/webconsole/" +
                 "test/test-mixedcontent-securityerrors.html";
const LEARN_MORE_URI = "https://developer.mozilla.org/docs/Security/" +
                       "MixedContent";

var test = asyncTest(function* () {
  yield pushPrefEnv();

  yield loadTab(TEST_URI);

  let hud = yield openConsole();

  let results = yield waitForMessages({
    webconsole: hud,
    messages: [
      {
        name: "Logged mixed active content",
        text: "Loading mixed (insecure) active content " +
              "\"http://example.com/\" on a secure page",
        category: CATEGORY_SECURITY,
        severity: SEVERITY_WARNING,
        objects: true,
      },
      {
        name: "Logged mixed passive content - image",
        text: "Loading mixed (insecure) display content " +
              "\"http://example.com/tests/image/test/mochitest/blue.png\" " +
              "on a secure page",
        category: CATEGORY_SECURITY,
        severity: SEVERITY_WARNING,
        objects: true,
      },
    ],
  });

  yield testClickOpenNewTab(hud, results);
});

function pushPrefEnv() {
  let deferred = promise.defer();
  let options = {"set":
      [["security.mixed_content.block_active_content", false],
       ["security.mixed_content.block_display_content", false]
  ]};
  SpecialPowers.pushPrefEnv(options, deferred.resolve);
  return deferred.promise;
}

function testClickOpenNewTab(hud, results) {
  let warningNode = results[0].clickableElements[0];
  ok(warningNode, "link element");
  ok(warningNode.classList.contains("learn-more-link"), "link class name");
  return simulateMessageLinkClick(warningNode, LEARN_MORE_URI);
}
