/* This Source Code Form is subject to the terms of the Mozilla Public
  * License, v. 2.0. If a copy of the MPL was not distributed with this
  * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(function*() {
  info("Check new window button existence and functionality");
  yield PanelUI.show();
  info("Menu panel was opened");

  let windowWasHandled = false;
  let newWindow = null;

  let observerWindowOpened = {
    observe: function(aSubject, aTopic, aData) {
      if (aTopic == "domwindowopened") {
        newWindow = aSubject.QueryInterface(Components.interfaces.nsIDOMWindow);
        newWindow.addEventListener("load", function newWindowHandler() {
          newWindow.removeEventListener("load", newWindowHandler, false);
          is(newWindow.location.href, "chrome://browser/content/browser.xul",
             "A new browser window was opened");
          ok(!PrivateBrowsingUtils.isWindowPrivate(newWindow), "Window is not private");
          windowWasHandled = true;
        }, false);
      }
    }
  }

  Services.ww.registerNotification(observerWindowOpened);

  let newWindowButton = document.getElementById("new-window-button");
  ok(newWindowButton, "New Window button exists in Panel Menu");
  newWindowButton.click();

  try {
    yield waitForCondition(() => windowWasHandled);
    yield promiseWindowClosed(newWindow);
    info("The new window was closed");
  }
  catch (e) {
    ok(false, "The new browser window was not properly handled");
  }
  finally {
    Services.ww.unregisterNotification(observerWindowOpened);
  }
});
