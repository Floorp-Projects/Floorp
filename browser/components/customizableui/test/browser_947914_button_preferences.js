/* This Source Code Form is subject to the terms of the Mozilla Public
  * License, v. 2.0. If a copy of the MPL was not distributed with this
  * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(function() {
  info("Check preferences button existence and functionality");
  yield PanelUI.show();

  var windowWasHandled = false;

  let observer = {
    observe: function(aSubject, aTopic, aData) {
      if (aTopic == "domwindowopened") {
        let win = aSubject.QueryInterface(Components.interfaces.nsIDOMWindow);
        win.addEventListener("load", function newWindowHandler() {
          win.removeEventListener("load", newWindowHandler, false);
          is(win.location.href, "chrome://browser/content/preferences/preferences.xul",
                                "A new preferences window was opened");

          win.close();
          windowWasHandled = true;
        }, false);
      }
    }
  }

  Services.ww.registerNotification(observer);

  let preferencesButton = document.getElementById("preferences-button");
  ok(preferencesButton, "Preferences button exists in Panel Menu");
  preferencesButton.click();

  try{
    yield waitForCondition(() => windowWasHandled);
  }
  catch(e) {
    ok(false, "The preferences window was not properly handled");
  }
  finally {
    Services.ww.unregisterNotification(observer);
  }
});
