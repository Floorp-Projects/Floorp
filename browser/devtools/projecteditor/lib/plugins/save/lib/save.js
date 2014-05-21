/* -*- Mode: Javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Class } = require("sdk/core/heritage");
const { registerPlugin, Plugin } = require("projecteditor/plugins/core");
const picker = require("projecteditor/helpers/file-picker");
const { getLocalizedString } = require("projecteditor/helpers/l10n");

// Handles the save command.
var SavePlugin = Class({
  extends: Plugin,

  init: function(host) {

    this.host.addCommand({
      id: "cmd-saveas",
      key: getLocalizedString("projecteditor.save.commandkey"),
      modifiers: "accel shift"
    });
    this.host.addCommand({
      id: "cmd-save",
      key: getLocalizedString("projecteditor.save.commandkey"),
      modifiers: "accel"
    });

    // Wait until we can add things into the app manager menu
    // this.host.createMenuItem({
    //   parent: "#file-menu-popup",
    //   label: "Save",
    //   command: "cmd-save",
    //   key: "key-save"
    // });
    // this.host.createMenuItem({
    //   parent: "#file-menu-popup",
    //   label: "Save As",
    //   command: "cmd-saveas",
    // });
  },

  onCommand: function(cmd) {
    if (cmd === "cmd-save") {
      this.save();
    } else if (cmd === "cmd-saveas") {
      this.saveAs();
    }
  },

  saveAs: function() {
    let editor = this.host.currentEditor;
    let project = this.host.resourceFor(editor);

    let resource;
    picker.showSave({
      window: this.host.window,
      directory: project && project.parent ? project.parent.path : null,
      defaultName: project ? project.basename : null,
    }).then(path => {
      return this.createResource(path);
    }).then(res => {
      resource = res;
      return this.saveResource(editor, resource);
    }).then(() => {
      this.host.openResource(resource);
    }).then(null, console.error);
  },

  save: function() {
    let editor = this.host.currentEditor;
    let resource = this.host.resourceFor(editor);
    if (!resource) {
      return this.saveAs();
    }

    return this.saveResource(editor, resource);
  },

  createResource: function(path) {
    return this.host.project.resourceFor(path, { create: true })
  },

  saveResource: function(editor, resource) {
    return editor.save(resource);
  }
})
exports.SavePlugin = SavePlugin;
registerPlugin(SavePlugin);
