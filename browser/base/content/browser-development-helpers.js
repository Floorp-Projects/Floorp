/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Extra features for local development. This file isn't loaded in
 * non-local builds.
 */

var DevelopmentHelpers = {
  init() {
    this.quickRestart = this.quickRestart.bind(this);
    this.addRestartShortcut();
  },

  quickRestart() {
    Services.obs.notifyObservers(null, "startupcache-invalidate");

    let env = Cc["@mozilla.org/process/environment;1"].getService(
      Ci.nsIEnvironment
    );
    env.set("MOZ_DISABLE_SAFE_MODE_KEY", "1");

    Services.startup.quit(
      Ci.nsIAppStartup.eAttemptQuit | Ci.nsIAppStartup.eRestart
    );
  },

  addRestartShortcut() {
    let command = document.createXULElement("command");
    command.setAttribute("id", "cmd_quickRestart");
    command.addEventListener("command", this.quickRestart, true);
    command.setAttribute("oncommand", "void 0;"); // Needed - bug 371900
    document.getElementById("mainCommandSet").prepend(command);

    let key = document.createXULElement("key");
    key.setAttribute("id", "key_quickRestart");
    key.setAttribute("key", "r");
    key.setAttribute("modifiers", "accel,alt");
    key.setAttribute("command", "cmd_quickRestart");
    key.setAttribute("oncommand", "void 0;"); // Needed - bug 371900
    document.getElementById("mainKeyset").prepend(key);

    let menuitem = document.createXULElement("menuitem");
    menuitem.setAttribute("id", "menu_FileRestartItem");
    menuitem.setAttribute("key", "key_quickRestart");
    menuitem.setAttribute("label", "Restart (Developer)");
    menuitem.addEventListener("command", this.quickRestart, true);
    document.getElementById("menu_FilePopup").appendChild(menuitem);
  },
};
