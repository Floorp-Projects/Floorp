/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * Registers about: pages provided by Shield, and listens for a shutdown event
 * from the add-on before un-registering them.
 *
 * This file is loaded as a process script. It is executed once for each
 * process, including the parent one.
 */

const { utils: Cu } = Components;
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://shield-recipe-client-content/AboutPages.jsm");

class ShieldChildListener {
  onStartup() {
    Services.cpmm.addMessageListener("Shield:ShuttingDown", this, true);
    AboutPages.aboutStudies.register();
  }

  onShutdown() {
    AboutPages.aboutStudies.unregister();
    Services.cpmm.removeMessageListener("Shield:ShuttingDown", this);

    // Unload AboutPages.jsm in case the add-on is reinstalled and we need to
    // load a new version of it.
    Cu.unload("resource://shield-recipe-client-content/AboutPages.jsm");
  }

  receiveMessage(message) {
    switch (message.name) {
      case "Shield:ShuttingDown":
        this.onShutdown();
        break;
    }
  }
}

// Only register in content processes; the parent process handles registration
// separately.
if (Services.appinfo.processType === Services.appinfo.PROCESS_TYPE_CONTENT) {
  const listener = new ShieldChildListener();
  listener.onStartup();
}
