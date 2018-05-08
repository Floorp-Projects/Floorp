/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */
ChromeUtils.import("resource://gre/modules/Services.jsm");

var AboutRestartRequired = {
  /* Only do autofocus if we're the toplevel frame; otherwise we
     don't want to call attention to ourselves!  The key part is
     that autofocus happens on insertion into the tree, so we
     can remove the button, add @autofocus, and reinsert the
     button.
  */
  addAutofocus() {
    if (window.top == window) {
      var button = document.getElementById("restart");
      var parent = button.parentNode;
      button.remove();
      button.setAttribute("autofocus", "true");
      parent.insertAdjacentElement("afterbegin", button);
    }
  },
  restart() {
    Services.startup.quit(Ci.nsIAppStartup.eRestart |
                          Ci.nsIAppStartup.eAttemptQuit);
  },
  init() {
    this.addAutofocus();
  },
};

AboutRestartRequired.init();
