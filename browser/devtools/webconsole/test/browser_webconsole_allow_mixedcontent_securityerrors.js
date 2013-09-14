/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// The test loads a web page with mixed active and display content
// on it while the "block mixed content" settings are _off_.
// It then checks that the loading mixed content warning messages
// are logged to the console and have the correct "Learn More"
// url appended to them.
// Bug 875456 - Log mixed content messages from the Mixed Content
// Blocker to the Security Pane in the Web Console

const TEST_URI = "https://example.com/browser/browser/devtools/webconsole/test/test-mixedcontent-securityerrors.html";
const LEARN_MORE_URI = "https://developer.mozilla.org/docs/Security/MixedContent";

function test()
{
  SpecialPowers.pushPrefEnv({"set":
      [["security.mixed_content.block_active_content", false],
       ["security.mixed_content.block_display_content", false]
  ]}, loadingMixedContentTest);
}

function loadingMixedContentTest()
{
  addTab(TEST_URI);
  browser.addEventListener("load", function onLoad(aEvent) {
    browser.removeEventListener(aEvent.type, onLoad, true);
    openConsole(null, function testSecurityErrorLogged (hud) {
      waitForMessages({
        webconsole: hud,
        messages: [
          {
            name: "Logged mixed active content",
            text: "Loading mixed (insecure) active content on a secure page \"http://example.com/\"",
            category: CATEGORY_SECURITY,
            severity: SEVERITY_WARNING,
            objects: true,
          },
          {
            name: "Logged mixed passive content - image",
            text: "Loading mixed (insecure) display content on a secure page \"http://example.com/tests/image/test/mochitest/blue.png\"",
            category: CATEGORY_SECURITY,
            severity: SEVERITY_WARNING,
            objects: true,
          },
        ],
      }).then((results) => testClickOpenNewTab(hud, results));
    });
  }, true);
}

function testClickOpenNewTab(hud, results) {
  let warningNode = results[0].clickableElements[0];
  ok(warningNode, "link element");
  ok(warningNode.classList.contains("learn-more-link"), "link class name");

  // Invoke the click event and check if a new tab would open to the correct page.
  let linkOpened = false;
  let oldOpenUILinkIn = window.openUILinkIn;
  window.openUILinkIn = function(aLink) {
    if (aLink == LEARN_MORE_URI) {
      linkOpened = true;
    }
  }

  EventUtils.synthesizeMouse(warningNode, 2, 2, {},
                             warningNode.ownerDocument.defaultView);
  ok(linkOpened, "Clicking the Learn More Warning node opens the desired page");
  window.openUILinkIn = oldOpenUILinkIn;

  finishTest();
}
