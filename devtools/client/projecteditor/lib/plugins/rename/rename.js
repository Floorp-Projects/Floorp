/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Class } = require("sdk/core/heritage");
const { registerPlugin, Plugin } = require("devtools/client/projecteditor/lib/plugins/core");
const { getLocalizedString } = require("devtools/client/projecteditor/lib/helpers/l10n");

var RenamePlugin = Class({
  extends: Plugin,

  init: function (host) {
    this.host.addCommand(this, {
      id: "cmd-rename"
    });
    this.contextMenuItem = this.host.createMenuItem({
      parent: this.host.contextMenuPopup,
      label: getLocalizedString("projecteditor.renameLabel"),
      command: "cmd-rename"
    });
  },

  onContextMenuOpen: function (resource) {
    if (resource.isRoot) {
      this.contextMenuItem.setAttribute("hidden", "true");
    } else {
      this.contextMenuItem.removeAttribute("hidden");
    }
  },

  onCommand: function (cmd) {
    if (cmd === "cmd-rename") {
      let tree = this.host.projectTree;
      let resource = tree.getSelectedResource();
      let parent = resource.parent;
      let oldName = resource.basename;

      tree.promptEdit(oldName, resource).then(name => {
        if (name === oldName) {
          return resource;
        }
        if (parent.hasChild(name)) {
          let matches = name.match(/([^\d.]*)(\d*)([^.]*)(.*)/);
          let template = matches[1] + "{1}" + matches[3] + matches[4];
          name = this.suggestName(resource, template, parseInt(matches[2]) || 2);
        }
        return parent.rename(oldName, name);
      }).then(resource => {
        this.host.project.refresh();
        tree.selectResource(resource);
        if (!resource.isDir) {
          this.host.currentEditor.focus();
        }
      }).then(null, console.error);
    }
  },

  suggestName: function (resource, template, start = 1) {
    let i = start;
    let name;
    let parent = resource.parent;
    do {
      name = template.replace("\{1\}", i === 1 ? "" : i);
      i++;
    } while (parent.hasChild(name));

    return name;
  }
});

exports.RenamePlugin = RenamePlugin;
registerPlugin(RenamePlugin);
