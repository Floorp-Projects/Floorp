/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// The test loads a web page with mixed active and display content
// on it while the "block mixed content" settings are _on_.
// It then checks that the blocked mixed content warning messages
// are logged to the console and have the correct "Learn More"
// url appended to them. After the first test finishes, it invokes
// a second test that overrides the mixed content blocker settings
// by clicking on the doorhanger shield and validates that the
// appropriate messages are logged to console.
// Bug 875456 - Log mixed content messages from the Mixed Content
// Blocker to the Security Pane in the Web Console

const TEST_URI = "https://example.com/browser/browser/devtools/webconsole/test/test-mixedcontent-securityerrors.html";
const LEARN_MORE_URI = "https://developer.mozilla.org/en/Security/MixedContent";

function test()
{
  SpecialPowers.pushPrefEnv({"set": [["security.mixed_content.block_active_content", true],
                            ["security.mixed_content.block_display_content", true]]}, blockMixedContentTest1);
}

function blockMixedContentTest1()
{
  addTab(TEST_URI);
  browser.addEventListener("load", function onLoad(aEvent) {
    browser.removeEventListener(aEvent.type, onLoad, true);
    openConsole(null, function testSecurityErrorLogged (hud) {
      waitForMessages({
        webconsole: hud,
        messages: [
          {
            name: "Logged blocking mixed active content",
            text: "Blocked loading mixed active content \"http://example.com/\"",
            category: CATEGORY_SECURITY,
            severity: SEVERITY_ERROR
          },
          {
            name: "Logged blocking mixed passive content - image",
            text: "Blocked loading mixed active content \"http://example.com/\"",
            category: CATEGORY_SECURITY,
            severity: SEVERITY_ERROR
          },
        ],
      }).then(() => {
        testClickOpenNewTab(hud);
        // Call the second (MCB override) test.
        mixedContentOverrideTest2(hud);
      });
    });
  }, true);
}

function mixedContentOverrideTest2(hud)
{
  var notification = PopupNotifications.getNotification("mixed-content-blocked", browser);
  ok(notification, "Mixed Content Doorhanger didn't appear");
  // Click on the doorhanger.
  notification.secondaryActions[0].callback();

  waitForMessages({
    webconsole: hud,
    messages: [
      {
      name: "Logged blocking mixed active content",
      text: "Loading mixed (insecure) active content on a secure"+
        " page \"http://example.com/\"",
      category: CATEGORY_SECURITY,
      severity: SEVERITY_WARNING
    },
    {
      name: "Logged blocking mixed passive content - image",
      text: "Loading mixed (insecure) display content on a secure page"+
        " \"http://example.com/tests/image/test/mochitest/blue.png\"",
      category: CATEGORY_SECURITY,
      severity: SEVERITY_WARNING
    },
    ],
  }).then(() => {
    testClickOpenNewTab(hud);
    finishTest();
  });
}

function testClickOpenNewTab(hud) {
  let warningNode = hud.outputNode.querySelector(".webconsole-learn-more-link");

    // Invoke the click event and check if a new tab would
    // open to the correct page.
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

}
