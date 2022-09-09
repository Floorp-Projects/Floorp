/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cc } = require("chrome");

loader.lazyGetter(this, "ppmm", () => {
  return Cc["@mozilla.org/parentprocessmessagemanager;1"].getService();
});

function ProcessActorList() {
  this._actors = new Map();
  this._onListChanged = null;
  this._mustNotify = false;
  this._hasObserver = false;
}

ProcessActorList.prototype = {
  getList() {
    const processes = [];
    for (let i = 0; i < ppmm.childCount; i++) {
      const mm = ppmm.getChildAt(i);
      processes.push({
        // An ID of zero is always used for the parent. It would be nice to fix
        // this so that the pid is also used for the parent, see bug 1587443.
        id: mm.isInProcess ? 0 : mm.osPid,
        parent: mm.isInProcess,
        // TODO: exposes process message manager on frameloaders in order to compute this
        tabCount: undefined,
      });
    }
    this._mustNotify = true;
    this._checkListening();

    return processes;
  },

  get onListChanged() {
    return this._onListChanged;
  },

  set onListChanged(onListChanged) {
    if (typeof onListChanged !== "function" && onListChanged !== null) {
      throw new Error("onListChanged must be either a function or null.");
    }
    if (onListChanged === this._onListChanged) {
      return;
    }

    this._onListChanged = onListChanged;
    this._checkListening();
  },

  _checkListening() {
    if (this._onListChanged !== null && this._mustNotify) {
      if (!this._hasObserver) {
        Services.obs.addObserver(this, "ipc:content-created");
        Services.obs.addObserver(this, "ipc:content-shutdown");
        this._hasObserver = true;
      }
    } else if (this._hasObserver) {
      Services.obs.removeObserver(this, "ipc:content-created");
      Services.obs.removeObserver(this, "ipc:content-shutdown");
      this._hasObserver = false;
    }
  },

  observe() {
    if (this._mustNotify) {
      this._onListChanged();
      this._mustNotify = false;
    }
  },
};

exports.ProcessActorList = ProcessActorList;
