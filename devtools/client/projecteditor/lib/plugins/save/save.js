/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Class } = require("sdk/core/heritage");
const { registerPlugin, Plugin } = require("devtools/client/projecteditor/lib/plugins/core");
const picker = require("devtools/client/projecteditor/lib/helpers/file-picker");
const { getLocalizedString } = require("devtools/client/projecteditor/lib/helpers/l10n");

// Handles the save command.
var SavePlugin = Class({
  extends: Plugin,

  init: function (host) {

    this.host.addCommand(this, {
      id: "cmd-save",
      key: getLocalizedString("projecteditor.save.commandkey"),
      modifiers: "accel"
    });
    this.host.addCommand(this, {
      id: "cmd-saveas",
      key: getLocalizedString("projecteditor.save.commandkey"),
      modifiers: "accel shift"
    });
    this.host.createMenuItem({
      parent: this.host.fileMenuPopup,
      label: getLocalizedString("projecteditor.saveLabel"),
      command: "cmd-save",
      key: "key_cmd-save"
    });
    this.host.createMenuItem({
      parent: this.host.fileMenuPopup,
      label: getLocalizedString("projecteditor.saveAsLabel"),
      command: "cmd-saveas",
      key: "key_cmd-saveas"
    });
  },

  isCommandEnabled: function (cmd) {
    let currentEditor = this.host.currentEditor;
    return currentEditor.isEditable;
  },

  onCommand: function (cmd) {
    if (cmd === "cmd-save") {
      this.onEditorSaveRequested();
    } else if (cmd === "cmd-saveas") {
      this.saveAs();
    }
  },

  saveAs: function () {
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

  onEditorSaveRequested: function () {
    let editor = this.host.currentEditor;
    let resource = this.host.resourceFor(editor);
    if (!resource) {
      return this.saveAs();
    }

    return this.saveResource(editor, resource);
  },

  createResource: function (path) {
    return this.host.project.resourceFor(path, { create: true });
  },

  saveResource: function (editor, resource) {
    return editor.save(resource);
  }
});
exports.SavePlugin = SavePlugin;
registerPlugin(SavePlugin);
