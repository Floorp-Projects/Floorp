/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This file manages a MigrationWizard embedded in a dialog that runs
 * in either a TabDialogBox or a top-level dialog window. It's main
 * responsibility is to listen for dialog-specific events from the
 * embedded MigrationWizard and to respond appropriately to them.
 */

const MigrationDialog = {
  _wiz: null,

  init() {
    addEventListener("load", this);
  },

  onLoad() {
    this._wiz = document.getElementById("wizard");
    this._wiz.addEventListener("MigrationWizard:Close", this);
  },

  handleEvent(event) {
    switch (event.type) {
      case "load": {
        this.onLoad();
        break;
      }
      case "MigrationWizard:Close": {
        window.close();
        break;
      }
    }
  },
};

MigrationDialog.init();
