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
  browser.addEventListener("DOMContentLoaded", testCloseButton, false);
}

function testCloseButton() {
  browser.removeEventListener("DOMContentLoaded", testCloseButton, false);

  openConsole();

  let hud = HUDService.getHudByWindow(content);
  let hudId = hud.hudId;

  HUDService.disableAnimation(hudId);
  executeSoon(function() {
    let closeButton = hud.HUDBox.querySelector(".webconsole-close-button");
    ok(closeButton != null, "we have the close button");

    // XXX: ASSERTION: ###!!! ASSERTION: XPConnect is being called on a scope without a 'Components' property!: 'Error', file /home/ddahl/code/moz/mozilla-central/mozilla-central/js/src/xpconnect/src/xpcwrappednativescope.cpp, line 795

    closeButton.addEventListener("command", function() {
      closeButton.removeEventListener("command", arguments.callee, false);

      ok(!(hudId in HUDService.hudReferences), "the console is closed when " +
         "the close button is pressed");
      closeButton = null;
      finishTest();
    }, false);

    EventUtils.synthesizeMouse(closeButton, 2, 2, {});
  });
}
