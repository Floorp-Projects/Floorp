/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;
const DOMUtils = Cc["@mozilla.org/inspector/dom-utils;1"].getService(Ci.inIDOMUtils);

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
  "resource://gre/modules/Task.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "console",
  "resource://gre/modules/devtools/Console.jsm");

function FontInspector(inspector, window)
{
  this.inspector = inspector;
  this.pageStyle = this.inspector.pageStyle;
  this.chromeDoc = window.document;
  this.init();
}

FontInspector.prototype = {
  init: function FI_init() {
    this.update = this.update.bind(this);
    this.onNewNode = this.onNewNode.bind(this);
    this.inspector.selection.on("new-node", this.onNewNode);
    this.inspector.sidebar.on("fontinspector-selected", this.onNewNode);
    this.showAll = this.showAll.bind(this);
    this.showAllButton = this.chromeDoc.getElementById("showall");
    this.showAllButton.addEventListener("click", this.showAll);
    this.update();
  },

  /**
   * Is the fontinspector visible in the sidebar?
   */
  isActive: function FI_isActive() {
    return this.inspector.sidebar &&
           this.inspector.sidebar.getCurrentTabID() == "fontinspector";
  },

  /**
   * Remove listeners.
   */
  destroy: function FI_destroy() {
    this.chromeDoc = null;
    this.inspector.sidebar.off("fontinspector-selected", this.onNewNode);
    this.inspector.selection.off("new-node", this.onNewNode);
    this.showAllButton.removeEventListener("click", this.showAll);
  },

  /**
   * Selection 'new-node' event handler.
   */
  onNewNode: function FI_onNewNode() {
    if (this.isActive() &&
        this.inspector.selection.isConnected() &&
        this.inspector.selection.isElementNode()) {
      this.undim();
      this.update();
    } else {
      this.dim();
    }
  },

  /**
   * Hide the font list. No node are selected.
   */
  dim: function FI_dim() {
    this.chromeDoc.body.classList.add("dim");
    this.chromeDoc.querySelector("#all-fonts").innerHTML = "";
  },

  /**
   * Show the font list. A node is selected.
   */
  undim: function FI_undim() {
    this.chromeDoc.body.classList.remove("dim");
  },

 /**
  * Retrieve all the font info for the selected node and display it.
  */
  update: Task.async(function*() {
    let node = this.inspector.selection.nodeFront;

    if (!node ||
        !this.isActive() ||
        !this.inspector.selection.isConnected() ||
        !this.inspector.selection.isElementNode() ||
        this.chromeDoc.body.classList.contains("dim")) {
      return;
    }

    this.chromeDoc.querySelector("#all-fonts").innerHTML = "";

    let fillStyle = (Services.prefs.getCharPref("devtools.theme") == "light") ?
        "black" : "white";
    let options = {
      includePreviews: true,
      previewFillStyle: fillStyle
    }

    let fonts = yield this.pageStyle.getUsedFontFaces(node, options)
                      .then(null, console.error);
    if (!fonts) {
      return;
    }

    for (let font of fonts) {
      font.previewUrl = yield font.preview.data.string();
    }

    // in case we've been destroyed in the meantime
    if (!this.chromeDoc) {
      return;
    }

    // clear again in case an update got in right before us
    this.chromeDoc.querySelector("#all-fonts").innerHTML = "";

    for (let font of fonts) {
      this.render(font);
    }

    this.inspector.emit("fontinspector-updated");
  }),

  /**
   * Display the information of one font.
   */
  render: function FI_render(font) {
    let s = this.chromeDoc.querySelector("#template > section");
    s = s.cloneNode(true);

    s.querySelector(".font-name").textContent = font.name;
    s.querySelector(".font-css-name").textContent = font.CSSFamilyName;

    if (font.URI) {
      s.classList.add("is-remote");
    } else {
      s.classList.add("is-local");
    }

    let formatElem = s.querySelector(".font-format");
    if (font.format) {
      formatElem.textContent = font.format;
    } else {
      formatElem.hidden = true;
    }

    s.querySelector(".font-url").value = font.URI;

    if (font.rule) {
      // This is the @font-face{…} code.
      let cssText = font.ruleText;

      s.classList.add("has-code");
      s.querySelector(".font-css-code").textContent = cssText;
    }
    let preview = s.querySelector(".font-preview");
    preview.src = font.previewUrl;

    this.chromeDoc.querySelector("#all-fonts").appendChild(s);
  },

  /**
   * Select the <body> to show all the fonts included in the document.
   */
  showAll: function FI_showAll() {
    if (!this.isActive() ||
        !this.inspector.selection.isConnected() ||
        !this.inspector.selection.isElementNode()) {
      return;
    }

    // Select the body node to show all fonts
    let walker = this.inspector.walker;

    walker.getRootNode().then(root => walker.querySelector(root, "body")).then(body => {
      this.inspector.selection.setNodeFront(body, "fontinspector");
    });
  },
}

window.setPanel = function(panel) {
  window.fontInspector = new FontInspector(panel, window);
}

window.onunload = function() {
  if (window.fontInspector) {
    window.fontInspector.destroy();
  }
}
