/* - This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this file,
   - You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Components.utils.import("resource://gre/modules/Services.jsm");

if (!Services.prefs.getBoolPref("browser.search.showOneOffButtons")) {
  addEventListener("load", function onLoad() {
    removeEventListener("load", onLoad);
    let pane =
      document.getAnonymousElementByAttribute(document.documentElement,
                                              "pane", "paneSearch");
    pane.hidden = true;
    if (pane.selected)
      document.documentElement.showPane(document.getElementById("paneMain"));
  });
}
