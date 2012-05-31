/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-console.html";

function test() {
  addTab(TEST_URI);
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);

    openConsole(null, function() {
      content.location.reload();
      browser.addEventListener("load", tabLoaded, true);
    });
  }, true);
}

function tabLoaded() {
  browser.removeEventListener("load", tabLoaded, true);

  let hudId = HUDService.getHudIdByWindow(content);
  let HUD = HUDService.hudReferences[hudId];

  HUD.jsterm.execute("document");

  let networkMessage = HUD.outputNode.querySelector(".webconsole-msg-network");
  ok(networkMessage, "found network message");

  let networkLink = networkMessage.querySelector(".webconsole-msg-link");
  ok(networkLink, "found network message link");

  let popupset = document.getElementById("mainPopupSet");
  ok(popupset, "found #mainPopupSet");

  let popupsShown = 0;
  let hiddenPopups = 0;

  let onpopupshown = function() {
    popupsShown++;
    if (popupsShown == 2) {
      document.removeEventListener("popupshown", onpopupshown, false);

      executeSoon(function() {
        let popups = popupset.querySelectorAll("panel[hudId=" + hudId + "]");
        is(popups.length, 2, "found two popups");

        document.addEventListener("popuphidden", onpopuphidden, false);

        registerCleanupFunction(function() {
          is(hiddenPopups, 2, "correct number of popups hidden");
          if (hiddenPopups != 2) {
            document.removeEventListener("popuphidden", onpopuphidden, false);
          }
        });

        executeSoon(closeConsole);
      });
    }
  };

  let onpopuphidden = function() {
    hiddenPopups++;
    if (hiddenPopups == 2) {
      document.removeEventListener("popuphidden", onpopuphidden, false);

      executeSoon(function() {
        let popups = popupset.querySelectorAll("panel[hudId=" + hudId + "]");
        is(popups.length, 0, "no popups found");

        executeSoon(finishTest);
      });
    }
  };

  document.addEventListener("popupshown", onpopupshown, false);

  registerCleanupFunction(function() {
    is(popupsShown, 2, "correct number of popups shown");
    if (popupsShown != 2) {
      document.removeEventListener("popupshown", onpopupshown, false);
    }
  });

  waitForSuccess({
    name: "jsterm output message",
    validatorFn: function()
    {
      return HUD.outputNode.querySelector(".webconsole-msg-output");
    },
    successFn: function()
    {
      let jstermMessage = HUD.outputNode.querySelector(".webconsole-msg-output");
      EventUtils.synthesizeMouse(jstermMessage, 2, 2, {});
      EventUtils.synthesizeMouse(networkLink, 2, 2, {});
    },
    failureFn: finishTest,
  });
}
