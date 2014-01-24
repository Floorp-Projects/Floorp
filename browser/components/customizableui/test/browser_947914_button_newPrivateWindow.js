/* This Source Code Form is subject to the terms of the Mozilla Public
  * License, v. 2.0. If a copy of the MPL was not distributed with this
  * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(function() {
  info("Check private browsing button existence and functionality");
  yield PanelUI.show();

  var windowWasHandled = false;

  let observer = {
    observe: function(aSubject, aTopic, aData) {
      if (aTopic == "domwindowopened") {
        let win = aSubject.QueryInterface(Components.interfaces.nsIDOMWindow);
        win.addEventListener("load", function newWindowHandler() {
          win.removeEventListener("load", newWindowHandler, false);
          is(win.location.href, "chrome://browser/content/browser.xul",
                                "A new private browser window was opened");
          ok(PrivateBrowsingUtils.isWindowPrivate(win), "Window is private");

          win.close();
          windowWasHandled = true;
        }, false);
      }
    }
  }

  Services.ww.registerNotification(observer);

  let privateBrowsingButton = document.getElementById("privatebrowsing-button");
  ok(privateBrowsingButton, "Private browsing button exists in Panel Menu");
  privateBrowsingButton.click();

  try{
    yield waitForCondition(() => windowWasHandled);
  }
  catch(e) {
    ok(false, "The new private window was not properly handled");
  }
  finally {
    Services.ww.unregisterNotification(observer);
  }
});
