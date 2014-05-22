/* -*- Mode: Javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This is the core plugin API.

const { Class } = require("sdk/core/heritage");

var Plugin = Class({
  initialize: function(host) {
    this.host = host;
    this.init(host);
  },

  destroy: function(host) { },

  init: function(host) {},

  showForCategories: function(elt, categories) {
    this._showFor = this._showFor || [];
    let set = new Set(categories);
    this._showFor.push({
      elt: elt,
      categories: new Set(categories)
    });
    if (this.host.currentEditor) {
      this.onEditorActivated(this.host.currentEditor);
    } else {
      elt.classList.add("plugin-hidden");
    }
  },

  priv: function(item) {
    if (!this._privData) {
      this._privData = new WeakMap();
    }
    if (!this._privData.has(item)) {
       this._privData.set(item, {});
    }
    return this._privData.get(item);
  },
  onTreeSelected: function(resource) {},


  // Editor state lifetime...
  onEditorCreated: function(editor) {},
  onEditorDestroyed: function(editor) {},

  onEditorActivated: function(editor) {
    if (this._showFor) {
      let category = editor.category;
      for (let item of this._showFor) {
        if (item.categories.has(category)) {
          item.elt.classList.remove("plugin-hidden");
        } else {
          item.elt.classList.add("plugin-hidden");
        }
      }
    }
  },
  onEditorDeactivated: function(editor) {
    if (this._showFor) {
      for (let item of this._showFor) {
        item.elt.classList.add("plugin-hidden");
      }
    }
  },

  onEditorLoad: function(editor) {},
  onEditorSave: function(editor) {},
  onEditorChange: function(editor) {},
  onEditorCursorActivity: function(editor) {},
});
exports.Plugin = Plugin;

function registerPlugin(constr) {
  exports.registeredPlugins.push(constr);
}
exports.registerPlugin = registerPlugin;

exports.registeredPlugins = [];
