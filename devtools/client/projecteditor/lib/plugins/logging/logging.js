/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var { Class } = require("sdk/core/heritage");
var { registerPlugin, Plugin } = require("devtools/client/projecteditor/lib/plugins/core");

var LoggingPlugin = Class({
  extends: Plugin,

  // Editor state lifetime...
  onEditorCreated: function(editor) { console.log("editor created: " + editor) },
  onEditorDestroyed: function(editor) { console.log("editor destroyed: " + editor )},

  onEditorSave: function(editor) { console.log("editor saved: " + editor) },
  onEditorLoad: function(editor) { console.log("editor loaded: " + editor) },

  onEditorActivated: function(editor) { console.log("editor activated: " + editor )},
  onEditorDeactivated: function(editor) { console.log("editor deactivated: " + editor )},

  onEditorChange: function(editor) { console.log("editor changed: " + editor )},

  onCommand: function(cmd) { console.log("Command: " + cmd); }
});
exports.LoggingPlugin = LoggingPlugin;

registerPlugin(LoggingPlugin);
