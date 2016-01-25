/* This Source Code Form is subject to the terms of the Mozilla Public
  * License, v. 2.0. If a copy of the MPL was not distributed with this
  * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(function*() {
  info("Check private browsing button existence and functionality");

  yield PanelUI.show();
  info("Menu panel was opened");

  let windowWasHandled = false;
  let privateWindow = null;

  let observerWindowOpened = {
    observe: function(aSubject, aTopic, aData) {
      if (aTopic == "domwindowopened") {
        privateWindow = aSubject.QueryInterface(Components.interfaces.nsIDOMWindow);
        privateWindow.addEventListener("load", function newWindowHandler() {
          privateWindow.removeEventListener("load", newWindowHandler, false);
          is(privateWindow.location.href, "chrome://browser/content/browser.xul",
             "A new browser window was opened");
          ok(PrivateBrowsingUtils.isWindowPrivate(privateWindow), "Window is private");
          windowWasHandled = true;
        }, false);
      }
    }
  }

  Services.ww.registerNotification(observerWindowOpened);

  let privateBrowsingButton = document.getElementById("privatebrowsing-button");
  ok(privateBrowsingButton, "Private browsing button exists in Panel Menu");
  privateBrowsingButton.click();

  try {
    yield waitForCondition(() => windowWasHandled);
    yield promiseWindowClosed(privateWindow);
    info("The new private window was closed");
  }
  catch(e) {
    ok(false, "The new private browser window was not properly handled");
  }
  finally {
    Services.ww.unregisterNotification(observerWindowOpened);
  }
});
