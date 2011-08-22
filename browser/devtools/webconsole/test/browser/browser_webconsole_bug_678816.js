/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/browser/test-console.html";
const FRAME_SCRIPT_URI ="chrome://mochitests/content/browser/browser/devtools/webconsole/test/browser/test-bug-678816-content.js";

let HUD;
let outputItem;

function tabLoad1(aEvent) {
  browser.removeEventListener(aEvent.type, arguments.callee, true);

  openConsole();
  HUD = HUDService.getHudByWindow(content);

  browser.addEventListener("load", tabLoad2, true);

  // Reload so we get some output in the console.
  browser.contentWindow.location.reload();
}

function tabLoad2(aEvent) {
  browser.removeEventListener(aEvent.type, tabLoad2, true);

  outputItem = HUD.outputNode.querySelector(".hud-networkinfo .hud-clickable");
  ok(outputItem, "found a network message");
  document.addEventListener("popupshown", networkPanelShown, false);

  // Click the network message to open the network panel.
  EventUtils.synthesizeMouseAtCenter(outputItem, {});
}

function networkPanelShown(aEvent) {
  document.removeEventListener(aEvent.type, networkPanelShown, false);

  executeSoon(function() {
    aEvent.target.addEventListener("popuphidden", networkPanelHidden, false);
    aEvent.target.hidePopup();
  });
}

function networkPanelHidden(aEvent) {
  this.removeEventListener(aEvent.type, networkPanelHidden, false);

  is(HUD.contentWindow, browser.contentWindow,
    "console has not been re-attached to the wrong window");

  finishTest();
}

function test() {
  messageManager.loadFrameScript(FRAME_SCRIPT_URI, true);

  registerCleanupFunction(function () {
    // There's no way to unload a frameScript so send a kill signal to
    // unregister the frame script's webProgressListener
    messageManager.sendAsyncMessage("bug-678816-kill-webProgressListener");
  });

  addTab(TEST_URI);
  browser.addEventListener("load", tabLoad1, true);
}
