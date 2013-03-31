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

var origBlockDisplay;
var origBlockActive;

function test() {
  addTab("data:text/html;charset=utf8,Web Console mixed content test");
  browser.addEventListener("load", onLoad, true);
}

function onLoad(aEvent) {
  browser.removeEventListener("load", onLoad, true);
  origBlockDisplay = Services.prefs.getBoolPref("security.mixed_content.block_display_content");
  origBlockActive = Services.prefs.getBoolPref("security.mixed_content.block_active_content")
  Services.prefs.setBoolPref("security.mixed_content.block_display_content", false);
  Services.prefs.setBoolPref("security.mixed_content.block_active_content", false);
  openConsole(null, testMixedContent);
}

function testMixedContent(hud) {
  content.location = TEST_HTTPS_URI;
  var aOutputNode = hud.outputNode;

  waitForSuccess(
    {
      name: "mixed content warning displayed successfully",
      timeout: 20000,
      validatorFn: function() {
        return ( aOutputNode.querySelector(".webconsole-mixed-content") );
      },

      successFn: function() {

        //tests on the urlnode
        let node = aOutputNode.querySelector(".webconsole-mixed-content");
        ok(testSeverity(node), "Severity type is SEVERITY_WARNING.");

        //tests on the warningNode
        let warningNode = aOutputNode.querySelector(".webconsole-mixed-content-link");
        is(warningNode.value, "[Mixed Content]", "Message text is accurate." );
        testClickOpenNewTab(warningNode);

        endTest();
      },

      failureFn: endTest,
    }
  );

}

function testSeverity(node) {
  let linkNode = node.parentNode;
  let msgNode = linkNode.parentNode;
  let bodyNode = msgNode.parentNode;
  let finalNode = bodyNode.parentNode;

  return finalNode.classList.contains("webconsole-msg-warn");
}

function testClickOpenNewTab(warningNode) {
  /* Invoke the click event and check if a new tab would open to the correct page */
  let linkOpened = false;
  let oldOpenUILinkIn = window.openUILinkIn;

  window.openUILinkIn = function(aLink) {
    if (aLink == "https://developer.mozilla.org/en/Security/MixedContent") {
      linkOpened = true;
    }
  }

  EventUtils.synthesizeMouse(warningNode, 2, 2, {},
                             warningNode.ownerDocument.defaultView);

  ok(linkOpened, "Clicking the Mixed Content Warning node opens the desired page");

  window.openUILinkIn = oldOpenUILinkIn;
}

function endTest() {
  Services.prefs.setBoolPref("security.mixed_content.block_display_content", origBlockDisplay);
  Services.prefs.setBoolPref("security.mixed_content.block_active_content", origBlockActive);
  finishTest();
}
