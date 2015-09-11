/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/" +
                 "test/test-console.html";

let test = asyncTest(function* () {
  yield loadTab(TEST_URI);

  let hud = yield openConsole();

  BrowserReload();

  let results = yield waitForMessages({
    webconsole: hud,
    messages: [{
      text: "test-console.html",
      category: CATEGORY_NETWORK,
      severity: SEVERITY_LOG,
    }],
  });

  yield performTest(hud, results);
});

function performTest(HUD, results) {
  let deferred = promise.defer();

  let networkMessage = [...results[0].matched][0];
  ok(networkMessage, "network message element");

  let networkLink = networkMessage.querySelector(".url");
  ok(networkLink, "found network message link");

  let popupset = document.getElementById("mainPopupSet");
  ok(popupset, "found #mainPopupSet");

  let popupsShown = 0;
  let hiddenPopups = 0;

  let onpopupshown = function() {
    document.removeEventListener("popupshown", onpopupshown, false);
    popupsShown++;

    executeSoon(function() {
      let popups = popupset.querySelectorAll("panel[hudId=" + HUD.hudId + "]");
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
      let popups = popupset.querySelectorAll("panel[hudId=" + HUD.hudId + "]");
      is(popups.length, 0, "no popups found");

      executeSoon(deferred.resolve);
    });
  };

  document.addEventListener("popupshown", onpopupshown, false);

  registerCleanupFunction(function() {
    is(popupsShown, 1, "correct number of popups shown");
    if (popupsShown != 1) {
      document.removeEventListener("popupshown", onpopupshown, false);
    }
  });

  EventUtils.sendMouseEvent({ type: "mousedown" }, networkLink,
                              HUD.iframeWindow);
  EventUtils.sendMouseEvent({ type: "mouseup" }, networkLink, HUD.iframeWindow);
  EventUtils.sendMouseEvent({ type: "click" }, networkLink, HUD.iframeWindow);

  return deferred.promise;
}
