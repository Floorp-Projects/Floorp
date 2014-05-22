/* -*- Mode: Javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Cu } = require("chrome");
const { Class } = require("sdk/core/heritage");
const promise = require("projecteditor/helpers/promise");
const { ImageEditor } = require("./image-editor");
const { registerPlugin, Plugin } = require("projecteditor/plugins/core");

var ImageEditorPlugin = Class({
  extends: Plugin,

  editorForResource: function(node) {
    if (node.contentCategory === "image") {
      return ImageEditor;
    }
  },

  init: function(host) {

  }
});

exports.ImageEditorPlugin = ImageEditorPlugin;
registerPlugin(ImageEditorPlugin);
