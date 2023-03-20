/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This file manages a MigrationWizard embedded in a dialog that runs
 * in a top-level dialog window. It's main responsibility is to listen
 * for dialog-specific events from the embedded MigrationWizard and to
 * respond appropriately to them.
 *
 * A single object argument is expected to be passed when opening
 * this dialog.
 *
 * @param {object} window.arguments.0
 * @param {Function} window.arguments.0.onResize
 *   A callback to resize the container of this document when the
 *   MigrationWizard resizes.
 * @param {object} window.arguments.0.options
 *   A series of options for configuring the dialog. See
 *   MigrationUtils.showMigrationWizard for a description of this
 *   object.
 */

const MigrationDialog = {
  _wiz: null,

  init() {
    addEventListener("load", this);
  },

  onLoad() {
    this._wiz = document.getElementById("wizard");
    this._wiz.addEventListener("MigrationWizard:Close", this);
    document.addEventListener("keypress", this);

    let args = window.arguments[0];
    // When opened via nsIWindowWatcher.openWindow, the arguments are
    // passed through C++, and they arrive to us wrapped as an XPCOM
    // object. We use wrappedJSObject to get at the underlying JS
    // object.
    if (args instanceof Ci.nsISupports) {
      args = args.wrappedJSObject;
    }

    // We have to inform the container of this document that the
    // MigrationWizard has changed size in order for it to update
    // its dimensions too.
    let observer = new ResizeObserver(() => {
      args.onResize();
    });
    observer.observe(this._wiz);

    let panelList = this._wiz.querySelector("panel-list");
    let panel = document.createXULElement("panel");
    panel.appendChild(panelList);
    this._wiz.appendChild(panel);
    this._wiz.requestState();
  },

  handleEvent(event) {
    switch (event.type) {
      case "load": {
        this.onLoad();
        break;
      }
      case "keypress": {
        if (event.keyCode == KeyEvent.DOM_VK_ESCAPE) {
          window.close();
        }
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
