/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Class } = require("sdk/core/heritage");
const { registerPlugin, Plugin } = require("projecteditor/plugins/core");
const { confirm } = require("projecteditor/helpers/prompts");
const { getLocalizedString } = require("projecteditor/helpers/l10n");

var DeletePlugin = Class({
  extends: Plugin,
  shouldConfirm: true,

  init: function(host) {
    this.host.addCommand(this, {
      id: "cmd-delete"
    });
    this.contextMenuItem = this.host.createMenuItem({
      parent: this.host.contextMenuPopup,
      label: getLocalizedString("projecteditor.deleteLabel"),
      command: "cmd-delete"
    });
  },

  confirmDelete: function(resource) {
    let deletePromptMessage = resource.isDir ?
      getLocalizedString("projecteditor.deleteFolderPromptMessage") :
      getLocalizedString("projecteditor.deleteFilePromptMessage");
    return !this.shouldConfirm || confirm(
      getLocalizedString("projecteditor.deletePromptTitle"),
      deletePromptMessage
    );
  },

  onContextMenuOpen: function(resource) {
    // Do not allow deletion of the top level items in the tree.  In the
    // case of the Web IDE in particular this can leave the UI in a weird
    // state. If we'd like to add ability to delete the project folder from
    // the tree in the future, then the UI could be cleaned up by listening
    // to the ProjectTree's "resource-removed" event.
    if (!resource.parent) {
      this.contextMenuItem.setAttribute("hidden", "true");
    } else {
      this.contextMenuItem.removeAttribute("hidden");
    }
  },

  onCommand: function(cmd) {
    if (cmd === "cmd-delete") {
      let tree = this.host.projectTree;
      let resource = tree.getSelectedResource();

      if (!this.confirmDelete(resource)) {
        return;
      }

      resource.delete().then(() => {
        this.host.project.refresh();
      });
    }
  }
});

exports.DeletePlugin = DeletePlugin;
registerPlugin(DeletePlugin);
