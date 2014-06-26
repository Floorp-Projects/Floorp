/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Class } = require("sdk/core/heritage");
const { registerPlugin, Plugin } = require("projecteditor/plugins/core");
const { getLocalizedString } = require("projecteditor/helpers/l10n");

// Handles the new command.
var NewFile = Class({
  extends: Plugin,

  init: function() {
    this.command = this.host.addCommand(this, {
      id: "cmd-new",
      key: getLocalizedString("projecteditor.new.commandkey"),
      modifiers: "accel"
    });
    this.host.createMenuItem({
      parent: this.host.fileMenuPopup,
      label: getLocalizedString("projecteditor.newLabel"),
      command: "cmd-new",
      key: "key_cmd-new"
    });
    this.host.createMenuItem({
      parent: this.host.contextMenuPopup,
      label: getLocalizedString("projecteditor.newLabel"),
      command: "cmd-new"
    });
  },

  onCommand: function(cmd) {
    if (cmd === "cmd-new") {
      let tree = this.host.projectTree;
      let resource = tree.getSelectedResource();
      parent = resource.isDir ? resource : resource.parent;
      sibling = resource.isDir ? null : resource;

      if (!("createChild" in parent)) {
        return;
      }

      let extension = sibling ? sibling.contentCategory : parent.store.defaultCategory;
      let template = "untitled{1}." + extension;
      let name = this.suggestName(parent, template);

      tree.promptNew(name, parent, sibling).then(name => {

        // XXX: sanitize bad file names.

        // If the name is already taken, just add/increment a number.
        if (this.hasChild(parent, name)) {
          let matches = name.match(/([^\d.]*)(\d*)([^.]*)(.*)/);
          template = matches[1] + "{1}" + matches[3] + matches[4];
          name = this.suggestName(parent, template, parseInt(matches[2]) || 2);
        }

        return parent.createChild(name);
      }).then(resource => {
        tree.selectResource(resource);
        this.host.currentEditor.focus();
      }).then(null, console.error);
    }
  },

  suggestName: function(parent, template, start=1) {
    let i = start;
    let name;
    do {
      name = template.replace("\{1\}", i === 1 ? "" : i);
      i++;
    } while (this.hasChild(parent, name));

    return name;
  },

  hasChild: function(resource, name) {
    for (let child of resource.children) {
      if (child.basename === name) {
        return true;
      }
    }
    return false;
  }
})
exports.NewFile = NewFile;
registerPlugin(NewFile);
