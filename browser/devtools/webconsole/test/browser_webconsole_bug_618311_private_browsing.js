/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-console.html";

let pb = Cc["@mozilla.org/privatebrowsing;1"].
         getService(Ci.nsIPrivateBrowsingService);

function test() {
  addTab("data:text/html;charset=utf-8,Web Console test for bug 618311 (private browsing)");

  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);

    registerCleanupFunction(function() {
      pb.privateBrowsingEnabled = false;
      pb = null;
    });

    ok(!pb.privateBrowsingEnabled, "private browsing is not enabled");

    togglePBAndThen(function() {
      ok(pb.privateBrowsingEnabled, "private browsing is enabled");

      openConsole(gBrowser.selectedTab, function(hud) {
        content.location = TEST_URI;
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
    });
  }, true);
}

function performTest() {
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
        // Make sure the two panels are open.
        let popups = popupset.querySelectorAll("panel[hudId=" + hudId + "]");
        is(popups.length, 2, "found two popups");

        document.addEventListener("popuphidden", onpopuphidden, false);

        registerCleanupFunction(function() {
          is(hiddenPopups, 2, "correct number of popups hidden");
          if (hiddenPopups != 2) {
            document.removeEventListener("popuphidden", onpopuphidden, false);
          }
        });

        // Now toggle private browsing off. We expect the panels to close.
        pb.privateBrowsingEnabled = !pb.privateBrowsingEnabled;
      });
    }
  };

  let onpopuphidden = function(aEvent) {
    // Skip popups that are not opened by the Web Console.
    if (!aEvent.target.hasAttribute("hudId")) {
      return;
    }

    hiddenPopups++;
    if (hiddenPopups == 2) {
      document.removeEventListener("popuphidden", onpopuphidden, false);

      executeSoon(function() {
        let popups = popupset.querySelectorAll("panel[hudId=" + hudId + "]");
        is(popups.length, 0, "no popups found");

        ok(!pb.privateBrowsingEnabled, "private browsing is not enabled");

        gBrowser.removeCurrentTab();
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

  // Show the network and object inspector panels.
  waitForSuccess({
    name: "jsterm output message",
    validatorFn: function()
    {
      return HUD.outputNode.querySelector(".webconsole-msg-output");
    },
    successFn: function()
    {
      let jstermMessage = HUD.outputNode.querySelector(".webconsole-msg-output");
      EventUtils.sendMouseEvent({ type: "mousedown" }, jstermMessage);
      EventUtils.sendMouseEvent({ type: "mouseup" }, jstermMessage);
      EventUtils.sendMouseEvent({ type: "click" }, jstermMessage);
      EventUtils.sendMouseEvent({ type: "mousedown" }, networkLink);
      EventUtils.sendMouseEvent({ type: "mouseup" }, networkLink);
      EventUtils.sendMouseEvent({ type: "click" }, networkLink);
    },
    failureFn: finishTest,
  });
}

function togglePBAndThen(callback) {
  function pbObserver(aSubject, aTopic, aData) {
    if (aTopic != "private-browsing-transition-complete") {
      return;
    }

    Services.obs.removeObserver(pbObserver, "private-browsing-transition-complete");
    afterAllTabsLoaded(callback);
  }

  Services.obs.addObserver(pbObserver, "private-browsing-transition-complete", false);
  pb.privateBrowsingEnabled = !pb.privateBrowsingEnabled;
}
