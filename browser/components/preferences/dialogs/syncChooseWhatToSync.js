/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from /toolkit/content/preferencesBindings.js */

Preferences.addAll([
  { id: "services.sync.engine.addons", type: "bool" },
  { id: "services.sync.engine.bookmarks", type: "bool" },
  { id: "services.sync.engine.history", type: "bool" },
  { id: "services.sync.engine.tabs", type: "bool" },
  { id: "services.sync.engine.prefs", type: "bool" },
  { id: "services.sync.engine.passwords", type: "bool" },
  { id: "services.sync.engine.addresses", type: "bool" },
  { id: "services.sync.engine.creditcards", type: "bool" },
]);

let gSyncChooseWhatToSync = {
  init() {
    this._adjustForPrefs();
    let options = window.arguments[0];
    if (options.disconnectFun) {
      // We offer 'disconnect'
      document.addEventListener("dialogextra2", function() {
        options.disconnectFun().then(disconnected => {
          if (disconnected) {
            window.close();
          }
        });
      });
    } else {
      // no "disconnect" - hide the button.
      document
        .getElementById("syncChooseOptions")
        .getButton("extra2").hidden = true;
    }
  },

  // make whatever tweaks we need based on preferences.
  _adjustForPrefs() {
    // These 2 engines are unique in that there are prefs that make the
    // entire engine unavailable (which is distinct from "disabled").
    let enginePrefs = [
      ["services.sync.engine.addresses", ".sync-engine-addresses"],
      ["services.sync.engine.creditcards", ".sync-engine-creditcards"],
    ];
    for (let [enabledPref, className] of enginePrefs) {
      let availablePref = enabledPref + ".available";
      // If the engine is enabled we force it to be available, otherwise we see
      // spooky things happen (like it magically re-appear later)
      if (Services.prefs.getBoolPref(enabledPref, false)) {
        Services.prefs.setBoolPref(availablePref, true);
      }
      if (!Services.prefs.getBoolPref(availablePref)) {
        let elt = document.querySelector(className);
        elt.hidden = true;
      }
    }
  },
};
