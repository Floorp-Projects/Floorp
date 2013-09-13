/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *  Tanvi Vyas <tanvi@mozilla.com>
 *
 * ***** END LICENSE BLOCK ***** */

// Tests that the Web Console Mixed Content messages are displayed

const TEST_HTTPS_URI = "https://example.com/browser/browser/devtools/webconsole/test/test-bug-737873-mixedcontent.html";

function test() {
  addTab("data:text/html;charset=utf8,Web Console mixed content test");
  browser.addEventListener("load", onLoad, true);
}

function onLoad(aEvent) {
  browser.removeEventListener("load", onLoad, true);
  Services.prefs.setBoolPref("security.mixed_content.block_display_content", false);
  Services.prefs.setBoolPref("security.mixed_content.block_active_content", false);
  openConsole(null, testMixedContent);
}

function testMixedContent(hud) {
  content.location = TEST_HTTPS_URI;

  waitForMessages({
    webconsole: hud,
    messages: [{
      text: "example.com",
      category: CATEGORY_NETWORK,
      severity: SEVERITY_WARNING,
    }],
  }).then((results) => {
    let msg = [...results[0].matched][0];
    ok(msg, "page load logged");
    ok(msg.classList.contains("mixed-content"), ".mixed-content element");

    let link = msg.querySelector(".learn-more-link");
    ok(link, "mixed content link element");
    is(link.textContent, "[Mixed Content]", "link text is accurate");

    let oldOpenLink = hud.openLink;
    let linkOpened = false;
    hud.openLink = (url) => {
      is(url, "https://developer.mozilla.org/docs/Security/MixedContent",
         "url opened");
      linkOpened = true;
    };

    EventUtils.synthesizeMouse(link, 2, 2, {}, link.ownerDocument.defaultView);

    ok(linkOpened, "clicking the Mixed Content link opened a page");

    hud.openLink = oldOpenLink;

    ok(!msg.classList.contains("filtered-by-type"), "message is not filtered");

    hud.setFilterState("netwarn", false);

    ok(msg.classList.contains("filtered-by-type"), "message is filtered");

    hud.setFilterState("netwarn", true);

    Services.prefs.clearUserPref("security.mixed_content.block_display_content");
    Services.prefs.clearUserPref("security.mixed_content.block_active_content");

    finishTest();
  });
}
