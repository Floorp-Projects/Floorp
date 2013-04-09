/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *  Mihai Șucan <mihai.sucan@gmail.com>
 *
 * ***** END LICENSE BLOCK ***** */

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-console.html";
let HUD;

let outputItem;

function consoleOpened(aHud) {
  HUD = aHud;

  outputNode = HUD.outputNode;

  browser.addEventListener("load", tabLoad2, true);

  // Reload so we get some output in the console.
  browser.contentWindow.location.reload();
}

function tabLoad2(aEvent) {
  browser.removeEventListener(aEvent.type, tabLoad2, true);

  waitForSuccess({
    name: "network message displayed",
    validatorFn: function()
    {
      return outputNode.querySelector(".hud-networkinfo .hud-clickable");
    },
    successFn: function() {
      outputItem = outputNode.querySelector(".hud-networkinfo .hud-clickable");
      ok(outputItem, "found a network message");
      document.addEventListener("popupshown", networkPanelShown, false);

      // Send the mousedown and click events such that the network panel opens.
      EventUtils.sendMouseEvent({type: "mousedown"}, outputItem);
      EventUtils.sendMouseEvent({type: "click"}, outputItem);
    },
    failureFn: finishTest,
  });
}

function networkPanelShown(aEvent) {
  document.removeEventListener(aEvent.type, networkPanelShown, false);

  info("networkPanelShown");

  document.addEventListener("popupshown", networkPanelShowFailure, false);

  // The network panel should not open for the second time.
  EventUtils.sendMouseEvent({type: "mousedown"}, outputItem);
  EventUtils.sendMouseEvent({type: "click"}, outputItem);

  executeSoon(function() {
    aEvent.target.addEventListener("popuphidden", networkPanelHidden, false);
    aEvent.target.hidePopup();
  });
}

function networkPanelShowFailure(aEvent) {
  document.removeEventListener(aEvent.type, networkPanelShowFailure, false);

  ok(false, "the network panel should not show");
}

function networkPanelHidden(aEvent) {
  this.removeEventListener(aEvent.type, networkPanelHidden, false);

  info("networkPanelHidden");

  // The network panel should not show because this is a mouse event that starts
  // in a position and ends in another.
  EventUtils.sendMouseEvent({type: "mousedown", clientX: 3, clientY: 4},
    outputItem);
  EventUtils.sendMouseEvent({type: "click", clientX: 5, clientY: 6},
    outputItem);

  // The network panel should not show because this is a middle-click.
  EventUtils.sendMouseEvent({type: "mousedown", button: 1},
    outputItem);
  EventUtils.sendMouseEvent({type: "click", button: 1},
    outputItem);

  // The network panel should not show because this is a right-click.
  EventUtils.sendMouseEvent({type: "mousedown", button: 2},
    outputItem);
  EventUtils.sendMouseEvent({type: "click", button: 2},
    outputItem);

  executeSoon(function() {
    document.removeEventListener("popupshown", networkPanelShowFailure, false);

    // Done with the network output. Now test the jsterm output and the property
    // panel.
    HUD.jsterm.execute("document", () => {
      info("jsterm execute 'document' callback");

      HUD.jsterm.once("variablesview-open", onVariablesViewOpen);
      let outputItem = outputNode
                       .querySelector(".webconsole-msg-output .hud-clickable");
      ok(outputItem, "jsterm output message found");

      // Send the mousedown and click events such that the property panel opens.
      EventUtils.sendMouseEvent({type: "mousedown"}, outputItem);
      EventUtils.sendMouseEvent({type: "click"}, outputItem);
    });
  });
}

function onVariablesViewOpen() {
  info("onVariablesViewOpen");

  executeSoon(function() {
    HUD = outputItem = null;
    executeSoon(finishTest);
  });
}

function test() {
  addTab(TEST_URI);
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    openConsole(null, consoleOpened);
  }, true);
}

