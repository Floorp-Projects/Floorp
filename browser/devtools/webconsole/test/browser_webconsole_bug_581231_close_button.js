/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *  Patrick Walton <pcwalton@mozilla.com>
 *
 * ***** END LICENSE BLOCK ***** */

// Tests that the Web Console close button functions.

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-console.html";

function test() {
  addTab(TEST_URI);
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    openConsole(null, testCloseButton);
  }, true);
}

function testCloseButton(hud) {
  let hudId = hud.hudId;
  HUDService.disableAnimation(hudId);
  waitForFocus(function() {
    let closeButton = hud.ui.closeButton;
    ok(closeButton != null, "we have the close button");

    EventUtils.synthesizeMouse(closeButton, 2, 2, {}, hud.iframeWindow);

    ok(!(hudId in HUDService.hudReferences), "the console is closed when " +
       "the close button is pressed");

    finishTest();
  }, hud.iframeWindow);
}
