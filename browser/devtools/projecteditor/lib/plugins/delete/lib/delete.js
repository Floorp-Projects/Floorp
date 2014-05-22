/* -*- Mode: Javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Class } = require("sdk/core/heritage");
const { registerPlugin, Plugin } = require("projecteditor/plugins/core");
const { getLocalizedString } = require("projecteditor/helpers/l10n");

var DeletePlugin = Class({
  extends: Plugin,

  init: function(host) {
    this.host.addCommand({
      id: "cmd-delete"
    });
    this.host.createMenuItem({
      parent: "#directory-menu-popup",
      label: getLocalizedString("projecteditor.deleteLabel"),
      command: "cmd-delete"
    });
  },

  onCommand: function(cmd) {
    if (cmd === "cmd-delete") {
      let tree = this.host.projectTree;
      let resource = tree.getSelectedResource();
      let parent = resource.parent;
      tree.deleteResource(resource).then(() => {
        this.host.project.refresh();
      })
    }
  }
});

exports.DeletePlugin = DeletePlugin;
registerPlugin(DeletePlugin);
