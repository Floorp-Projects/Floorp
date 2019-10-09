/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { Cc } = require("chrome");

loader.lazyGetter(this, "ppmm", () => {
  return Cc["@mozilla.org/parentprocessmessagemanager;1"].getService();
});

function ProcessActorList() {
  this._actors = new Map();
  this._onListChanged = null;
  this._mustNotify = false;

  this._onMessage = this._onMessage.bind(this);
  this._processScript =
    "resource://devtools/server/startup/debug-new-process.js";
}

ProcessActorList.prototype = {
  getList: function() {
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

  _checkListening: function() {
    if (this._onListChanged !== null && this._mustNotify) {
      this._knownProcesses = [];
      for (let i = 0; i < ppmm.childCount; i++) {
        this._knownProcesses.push(ppmm.getChildAt(i));
      }
      ppmm.addMessageListener("debug:new-process", this._onMessage);
      ppmm.loadProcessScript(this._processScript, true);
    } else {
      ppmm.removeMessageListener("debug:new-process", this._onMessage);
      ppmm.removeDelayedProcessScript(this._processScript);
    }
  },

  _notifyListChanged: function() {
    if (this._mustNotify) {
      this._onListChanged();
      this._mustNotify = false;
    }
  },

  _onMessage: function({ target }) {
    if (this._knownProcesses.includes(target)) {
      return;
    }
    this._notifyListChanged();
  },
};

exports.ProcessActorList = ProcessActorList;
