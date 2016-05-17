/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the Web Console Mixed Content messages are displayed

"use strict";

const TEST_URI = "data:text/html;charset=utf8,Web Console mixed content test";
const TEST_HTTPS_URI = "https://example.com/browser/devtools/client/" +
                       "webconsole/test/test-bug-737873-mixedcontent.html";
const LEARN_MORE_URI = "https://developer.mozilla.org/docs/Security/" +
                       "MixedContent";

add_task(function* () {
  Services.prefs.setBoolPref("security.mixed_content.block_display_content",
                             false);
  Services.prefs.setBoolPref("security.mixed_content.block_active_content",
                             false);

  yield loadTab(TEST_URI);

  let hud = yield openConsole();

  yield testMixedContent(hud);

  Services.prefs.clearUserPref("security.mixed_content.block_display_content");
  Services.prefs.clearUserPref("security.mixed_content.block_active_content");
});

var testMixedContent = Task.async(function* (hud) {
  content.location = TEST_HTTPS_URI;

  let results = yield waitForMessages({
    webconsole: hud,
    messages: [{
      text: "example.com",
      category: CATEGORY_NETWORK,
      severity: SEVERITY_WARNING,
    }],
  });

  let msg = [...results[0].matched][0];
  ok(msg, "page load logged");
  ok(msg.classList.contains("mixed-content"), ".mixed-content element");

  let link = msg.querySelector(".learn-more-link");
  ok(link, "mixed content link element");
  is(link.textContent, "[Mixed Content]", "link text is accurate");

  yield simulateMessageLinkClick(link, LEARN_MORE_URI);

  ok(!msg.classList.contains("filtered-by-type"), "message is not filtered");

  hud.setFilterState("netwarn", false);

  ok(msg.classList.contains("filtered-by-type"), "message is filtered");

  hud.setFilterState("netwarn", true);
});
