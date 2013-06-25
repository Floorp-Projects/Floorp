/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-console.html";

function test() {
  addTab(TEST_URI);
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);

    openConsole(null, function(hud) {
      content.location.reload();

      waitForSuccess({
        name: "network message displayed",
        validatorFn: function()
        {
          return hud.outputNode.querySelector(".webconsole-msg-network");
        },
        successFn: performTest,
        failureFn: finishTest,
      });
    });
  }, true);
}

function performTest() {
  let hudId = HUDService.getHudIdByWindow(content);
  let HUD = HUDService.hudReferences[hudId];

  let networkMessage = HUD.outputNode.querySelector(".webconsole-msg-network");
  ok(networkMessage, "found network message");

  let networkLink = networkMessage.querySelector(".webconsole-msg-link");
  ok(networkLink, "found network message link");

  let popupset = document.getElementById("mainPopupSet");
  ok(popupset, "found #mainPopupSet");

  let popupsShown = 0;
  let hiddenPopups = 0;

  let onpopupshown = function() {
    document.removeEventListener("popupshown", onpopupshown, false);
    popupsShown++;

    executeSoon(function() {
      let popups = popupset.querySelectorAll("panel[hudId=" + hudId + "]");
      is(popups.length, 1, "found one popup");

      document.addEventListener("popuphidden", onpopuphidden, false);

      registerCleanupFunction(function() {
        is(hiddenPopups, 1, "correct number of popups hidden");
        if (hiddenPopups != 1) {
          document.removeEventListener("popuphidden", onpopuphidden, false);
        }
      });

      executeSoon(closeConsole);
    });
  };

  let onpopuphidden = function() {
    document.removeEventListener("popuphidden", onpopuphidden, false);
    hiddenPopups++;

    executeSoon(function() {
      let popups = popupset.querySelectorAll("panel[hudId=" + hudId + "]");
      is(popups.length, 0, "no popups found");

      executeSoon(finishTest);
    });
  };

  document.addEventListener("popupshown", onpopupshown, false);

  registerCleanupFunction(function() {
    is(popupsShown, 1, "correct number of popups shown");
    if (popupsShown != 1) {
      document.removeEventListener("popupshown", onpopupshown, false);
    }
  });

  EventUtils.sendMouseEvent({ type: "mousedown" }, networkLink, HUD.iframeWindow);
  EventUtils.sendMouseEvent({ type: "mouseup" }, networkLink, HUD.iframeWindow);
  EventUtils.sendMouseEvent({ type: "click" }, networkLink, HUD.iframeWindow);
}
