/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Cu } = require("chrome");
const { Class } = require("sdk/core/heritage");
const promise = require("promise");
const { ItchEditor } = require("devtools/client/projecteditor/lib/editors");

var AppProjectEditor = Class({
  extends: ItchEditor,

  hidesToolbar: true,

  initialize: function(host) {
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
    let {appManagerOpts} = this.host.project;

    // Only load the frame the first time it is selected
    if (!this.iframe || this.iframe.getAttribute("src") !== appManagerOpts.projectOverviewURL) {

      this.elt.textContent = "";
      let iframe = this.iframe = this.elt.ownerDocument.createElement("iframe");
      let iframeLoaded = this.iframeLoaded = promise.defer();

      iframe.addEventListener("load", function onLoad() {
        iframe.removeEventListener("load", onLoad);
        iframeLoaded.resolve();
      });

      iframe.setAttribute("flex", "1");
      iframe.setAttribute("src", appManagerOpts.projectOverviewURL);
      this.elt.appendChild(iframe);

    }

    promise.all([this.iframeLoaded.promise, this.appended]).then(() => {
      this.emit("load");
    });
  }
});

exports.AppProjectEditor = AppProjectEditor;
