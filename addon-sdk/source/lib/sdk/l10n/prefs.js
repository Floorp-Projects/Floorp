/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { on } = require("../system/events");
const core = require("./core");
const { id: jetpackId } = require('../self');

const OPTIONS_DISPLAYED = "addon-options-displayed";

function enable() {
  on(OPTIONS_DISPLAYED, onOptionsDisplayed);  
}
exports.enable = enable;

function onOptionsDisplayed({ subject: document, data: addonId }) {
  if (addonId !== jetpackId)
    return;
  localizeInlineOptions(document);
}

function localizeInlineOptions(document) {
  let query = 'setting[data-jetpack-id="' + jetpackId + '"][pref-name], ' +
              'button[data-jetpack-id="' + jetpackId + '"][pref-name]';
  let nodes = document.querySelectorAll(query);
  for (let node of nodes) {
    let name = node.getAttribute("pref-name");
    if (node.tagName == "setting") {
      let desc = core.get(name + "_description");
      if (desc)
        node.setAttribute("desc", desc);
      let title = core.get(name + "_title");
      if (title)
        node.setAttribute("title", title);

      for (let item of node.querySelectorAll("menuitem, radio")) {
          let key = name + "_options." + item.getAttribute("label");
          let label = core.get(key);
          if (label)
            item.setAttribute("label", label);
      }
    }
    else if (node.tagName == "button") {
      let label = core.get(name + "_label");
      if (label)
        node.setAttribute("label", label);
    }
  }
}
exports.localizeInlineOptions = localizeInlineOptions;
