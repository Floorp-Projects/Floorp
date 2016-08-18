/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// This file is loaded as a process script, it will be loaded in the parent
// process as well as all content processes.

const { utils: Cu } = Components;
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("chrome://pocket/content/AboutPocket.jsm");

function AboutPocketChildListener() {
}
AboutPocketChildListener.prototype = {
  onStartup: function onStartup() {

    // Only do this in content processes since, as the broadcaster of this
    // message, the parent process doesn't also receive it.  We handlers
    // the shutting down separately.
    if (Services.appinfo.processType ==
        Services.appinfo.PROCESS_TYPE_CONTENT) {

      Services.cpmm.addMessageListener("PocketShuttingDown", this, true);
    }

    AboutPocket.aboutSaved.register();
    AboutPocket.aboutSignup.register();
  },

  onShutdown: function onShutdown() {
    AboutPocket.aboutSignup.unregister();
    AboutPocket.aboutSaved.unregister();

    Services.cpmm.removeMessageListener("PocketShuttingDown", this);
    Cu.unload("chrome://pocket/content/AboutPocket.jsm");
  },

  receiveMessage: function receiveMessage(message) {
    switch (message.name) {
      case "PocketShuttingDown":
        this.onShutdown();
        break;
      default:
        break;
    }

    return;
  }
};

const listener = new AboutPocketChildListener();
listener.onStartup();
