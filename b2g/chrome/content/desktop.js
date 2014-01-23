/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

window.addEventListener("ContentStart", function(evt) {
  // Enable touch event shim on desktop that translates mouse events
  // into touch ones
  let require = Cu.import("resource://gre/modules/devtools/Loader.jsm", {})
                  .devtools.require;
  let { TouchEventHandler } = require("devtools/touch-events");
  let chromeEventHandler = window.QueryInterface(Ci.nsIInterfaceRequestor)
                                 .getInterface(Ci.nsIWebNavigation)
                                 .QueryInterface(Ci.nsIDocShell)
                                 .chromeEventHandler || window;
  let touchEventHandler = new TouchEventHandler(chromeEventHandler);
  touchEventHandler.start();
});
