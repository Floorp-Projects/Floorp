/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* import-globals-from ../debugger-controller.js */
/* import-globals-from ../debugger-view.js */
/* import-globals-from ../utils.js */
/* globals document */
"use strict";

function WorkersView() {
  this._onWorkerSelect = this._onWorkerSelect.bind(this);
}

WorkersView.prototype = extend(WidgetMethods, {
  initialize: function () {
    if (!Prefs.workersEnabled) {
      return;
    }

    document.getElementById("workers-pane").removeAttribute("hidden");
    document.getElementById("workers-splitter").removeAttribute("hidden");

    this.widget = new SideMenuWidget(document.getElementById("workers"), {
      showArrows: true,
    });
    this.emptyText = L10N.getStr("noWorkersText");
    this.widget.addEventListener("select", this._onWorkerSelect);
  },

  addWorker: function (workerForm) {
    let element = document.createElement("label");
    element.className = "plain dbg-worker-item";
    element.setAttribute("value", workerForm.url);
    element.setAttribute("flex", "1");

    this.push([element, workerForm.actor], {
      attachment: workerForm
    });
  },

  removeWorker: function (workerForm) {
    this.remove(this.getItemByValue(workerForm.actor));
  },

  _onWorkerSelect: function () {
    if (this.selectedItem !== null) {
      DebuggerController.Workers._onWorkerSelect(this.selectedItem.attachment);
      this.selectedItem = null;
    }
  }
});

DebuggerView.Workers = new WorkersView();
