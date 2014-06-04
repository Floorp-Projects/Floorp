/* -*- Mode: Javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Cu } = require("chrome");
const { Class } = require("sdk/core/heritage");
const promise = require("projecteditor/helpers/promise");
const { ItchEditor } = require("projecteditor/editors");

var AppProjectEditor = Class({
  extends: ItchEditor,

  hidesToolbar: true,

  initialize: function(document, host) {
    ItchEditor.prototype.initialize.apply(this, arguments);
    this.appended = promise.resolve();
    this.host = host;
    this.label = "app-manager";
  },

  destroy: function() {
    this.elt.remove();
    this.elt = null;
  },

  load: function(resource) {
    this.elt.textContent = "";
    let {appManagerOpts} = this.host.project;
    let iframe = this.iframe = this.elt.ownerDocument.createElement("iframe");
    iframe.setAttribute("flex", "1");
    iframe.setAttribute("src", appManagerOpts.projectOverviewURL);
    this.elt.appendChild(iframe);

    // Wait for other `appended` listeners before emitting load.
    this.appended.then(() => {
      this.emit("load");
    });
  }
});

exports.AppProjectEditor = AppProjectEditor;
