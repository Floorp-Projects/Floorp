/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;
const DOMUtils = Cc["@mozilla.org/inspector/dom-utils;1"].getService(Ci.inIDOMUtils);

function FontInspector(inspector, window)
{
  this.inspector = inspector;
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
        this.inspector.selection.isLocal() &&
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
   * Retrieve all the font related info we have for the selected
   * node and display them.
   */
  update: function FI_update() {
    if (!this.isActive() ||
        !this.inspector.selection.isConnected() ||
        !this.inspector.selection.isElementNode() ||
        this.chromeDoc.body.classList.contains("dim")) {
      return;
    }

    let node = this.inspector.selection.node;
    let contentDocument = node.ownerDocument;

    // We don't get fonts for a node, but for a range
    let rng = contentDocument.createRange();
    rng.selectNodeContents(node);
    let fonts = DOMUtils.getUsedFontFaces(rng);
    let fontsArray = [];
    for (let i = 0; i < fonts.length; i++) {
      fontsArray.push(fonts.item(i));
    }
    fontsArray = fontsArray.sort(function(a, b) {
      return a.srcIndex < b.srcIndex;
    });
    this.chromeDoc.querySelector("#all-fonts").innerHTML = "";
    for (let f of fontsArray) {
      this.render(f, contentDocument);
    }
  },

  /**
   * Display the information of one font.
   */
  render: function FI_render(font, document) {
    let s = this.chromeDoc.querySelector("#template > section");
    s = s.cloneNode(true);

    s.querySelector(".font-name").textContent = font.name;
    s.querySelector(".font-css-name").textContent = font.CSSFamilyName;
    s.querySelector(".font-format").textContent = font.format;

    if (font.srcIndex == -1) {
      s.classList.add("is-local");
    } else {
      s.classList.add("is-remote");
    }

    s.querySelector(".font-url").value = font.URI;

    let iframe = s.querySelector(".font-preview");
    if (font.rule) {
      // This is the @font-face{…} code.
      let cssText = font.rule.style.parentRule.cssText;

      s.classList.add("has-code");
      s.querySelector(".font-css-code").textContent = cssText;

      // We guess the base URL of the stylesheet to make
      // sure the font will be accessible in the preview.
      // If the font-face is in an inline <style>, we get
      // the location of the page.
      let origin = font.rule.style.parentRule.parentStyleSheet.href;
      if (!origin) { // Inline stylesheet
        origin = document.location.href;
      }
      // We remove the last part of the URL to get a correct base.
      let base = origin.replace(/\/[^\/]*$/,"/")

      // From all this information, we build a preview.
      this.buildPreview(iframe, font.CSSFamilyName, cssText, base);
    } else {
      this.buildPreview(iframe, font.CSSFamilyName, "", "");
    }

    this.chromeDoc.querySelector("#all-fonts").appendChild(s);
  },

  /**
   * Show a preview of the font in an iframe.
   */
  buildPreview: function FI_buildPreview(iframe, name, cssCode, base) {
    /* The HTML code of the preview is:
     *   <!DOCTYPE HTML>
     *   <head>
     *    <base href="{base}"></base>
     *   </head>
     *   <style>
     *   p {font-family: {name};}
     *   * {font-size: 40px;line-height:60px;padding:0 10px;margin:0};
     *   </style>
     *   <p contenteditable spellcheck='false'>Abc</p>
     */
    let extraCSS = "* {padding:0;margin:0}";
    extraCSS += ".theme-dark {color: white}";
    extraCSS += "p {font-size: 40px;line-height:60px;padding:0 10px;margin:0;}";
    cssCode += extraCSS;
    let src = "data:text/html;charset=utf-8,<!DOCTYPE HTML><head><base></base></head><style></style><p contenteditable spellcheck='false'>Abc</p>";
    iframe.addEventListener("load", function onload() {
      iframe.removeEventListener("load", onload, true);
      let doc = iframe.contentWindow.document;
      // We could have done that earlier, but we want to avoid any URL-encoding
      // nightmare.
      doc.querySelector("base").href = base;
      doc.querySelector("style").textContent = cssCode;
      doc.querySelector("p").style.fontFamily = name;
      // Forward theme
      doc.documentElement.className = document.documentElement.className;
    }, true);
    iframe.src = src;
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
