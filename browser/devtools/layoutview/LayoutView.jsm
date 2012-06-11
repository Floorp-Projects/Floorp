/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cu = Components.utils;
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource:///modules/inspector.jsm");
Cu.import("resource:///modules/devtools/LayoutHelpers.jsm");
Cu.import("resource:///modules/devtools/CssLogic.jsm");

var EXPORTED_SYMBOLS = ["LayoutView"];

function LayoutView(aOptions)
{
  this.chromeDoc = aOptions.document;
  this.inspector = aOptions.inspector;
  this.browser = this.inspector.chromeWindow.gBrowser;

  this.init();
}

LayoutView.prototype = {
  init: function LV_init() {
    this.cssLogic = new CssLogic();

    this.update = this.update.bind(this);
    this.onMessage = this.onMessage.bind(this);

    this.isOpen = false;
    this.documentReady = false;

    // Is the layout view was open before?
    if (!("_layoutViewIsOpen" in this.inspector)) {
      this.inspector._layoutViewIsOpen =
        Services.prefs.getBoolPref("devtools.layoutview.open");
    }

    // We update the values when:
    //  a node is locked
    //  we get the MozAfterPaint event and the node is locked
    function onSelect() {
      if (this.inspector.locked) {
        this.cssLogic.highlight(this.inspector.selection);
        this.undim();
        this.update();
        // We make sure we never add 2 listeners.
        if (!this.trackingPaint) {
          this.browser.addEventListener("MozAfterPaint", this.update, true);
          this.trackingPaint = true;
        }
      }
    }

    function onUnlock() {
      this.browser.removeEventListener("MozAfterPaint", this.update, true);
      this.trackingPaint = false;
      this.dim();
    }

    this.onSelect= onSelect.bind(this);
    this.onUnlock = onUnlock.bind(this);
    this.inspector.on("select", this.onSelect);
    this.inspector.on("unlocked", this.onUnlock);

    // Build the layout view in the sidebar.
    this.buildView();

    this.bound_handleKeypress = this.handleKeypress.bind(this);
    this.iframe.addEventListener("keypress", this.bound_handleKeypress, true);

    // Get messages from the iframe.
    this.inspector.chromeWindow.addEventListener("message", this.onMessage, true);

    // Store for the different dimensions of the node.
    // 'selector' refers to the element that holds the value in view.xhtml;
    // 'property' is what we are measuring;
    // 'value' is the computed dimension, computed in update().
    this.map = {
      marginTop: {selector: ".margin.top > span",
                  property: "margin-top",
                  value: undefined},
      marginBottom: {selector: ".margin.bottom > span",
                  property: "margin-bottom",
                  value: undefined},
      marginLeft: {selector: ".margin.left > span",
                  property: "margin-left",
                  value: undefined},
      marginRight: {selector: ".margin.right > span",
                  property: "margin-right",
                  value: undefined},
      paddingTop: {selector: ".padding.top > span",
                  property: "padding-top",
                  value: undefined},
      paddingBottom: {selector: ".padding.bottom > span",
                  property: "padding-bottom",
                  value: undefined},
      paddingLeft: {selector: ".padding.left > span",
                  property: "padding-left",
                  value: undefined},
      paddingRight: {selector: ".padding.right > span",
                  property: "padding-right",
                  value: undefined},
      borderTop: {selector: ".border.top > span",
                  property: "border-top-width",
                  value: undefined},
      borderBottom: {selector: ".border.bottom > span",
                  property: "border-bottom-width",
                  value: undefined},
      borderLeft: {selector: ".border.left > span",
                  property: "border-left-width",
                  value: undefined},
      borderRight: {selector: ".border.right > span",
                  property: "border-right-width",
                  value: undefined},
    };
  },

  /**
   * Destroy the nodes. Remove listeners.
   */
  destroy: function LV_destroy() {
    this.inspector.removeListener("select", this.onSelect);
    this.inspector.removeListener("unlocked", this.onUnlock);
    this.browser.removeEventListener("MozAfterPaint", this.update, true);
    this.iframe.removeEventListener("keypress", this.bound_handleKeypress, true);
    this.inspector.chromeWindow.removeEventListener("message", this.onMessage, true);
    this.close();
    this.iframe = null;
    this.view.parentNode.removeChild(this.view);
  },

  /**
   * Build the Layout container:
   *
   * <vbox id="inspector-layoutview-container">
   *  <iframe src="chrome://browser/content/devtools/layoutview/view.xhtml"/>
   * </vbox>
   */
  buildView: function LV_buildPanel() {
    this.iframe = this.chromeDoc.createElement("iframe");
    this.iframe.setAttribute("src", "chrome://browser/content/devtools/layoutview/view.xhtml");

    this.view = this.chromeDoc.createElement("vbox");
    this.view.id = "inspector-layoutview-container";
    this.view.appendChild(this.iframe);

    let sidebar = this.chromeDoc.getElementById("devtools-sidebar-box");
    sidebar.appendChild(this.view);
  },

  /**
   * Called when the iframe is loaded.
   */
  onDocumentReady: function LV_onDocumentReady() {
    this.documentReady = true;
    this.doc = this.iframe.contentDocument;

    // We can't do that earlier because open() and close() need to do stuff
    // inside the iframe.

    if (this.inspector.locked)
      this.onSelect();
    else
      this.onUnlock();

    if (this.inspector._layoutViewIsOpen) {
      this.open();
    } else {
      this.close();
    }

  },

  /**
   * This is where we get messages from the layout view iframe.
   */
  onMessage: function LV_onMessage(e) {
    switch (e.data) {
      case "layoutview-toggle-view":
        this.toggle(true);
        break;
      case "layoutview-ready":
        this.onDocumentReady();
        break;
      default:
        break;
    }
  },

  /**
   * Handle keypress.
   */
   handleKeypress: function LV_handleKeypress(event) {
     let win = this.inspector.chromeWindow;

     // avoid scroll
     if (event.keyCode == win.KeyEvent.DOM_VK_LEFT ||
         event.keyCode == win.KeyEvent.DOM_VK_RIGHT ||
         event.keyCode == win.KeyEvent.DOM_VK_UP ||
         event.keyCode == win.KeyEvent.DOM_VK_DOWN ||
         event.keyCode == win.KeyEvent.DOM_VK_PAGE_UP ||
         event.keyCode == win.KeyEvent.DOM_VK_PAGE_DOWN) {

        event.preventDefault();
     }

     if (event.charCode == win.KeyEvent.DOM_VK_SPACE) {
       this.toggle(true);
     }
   },

  /**
   * Open the view container.
   *
   * @param aUserAction Is the action triggered by the user (click on the
   * open/close button in the view)
   */
  open: function LV_open(aUserAction) {
    this.isOpen = true;
    if (this.documentReady)
      this.doc.body.classList.add("open");
    if (aUserAction) {
      this.inspector._layoutViewIsOpen = true;
      Services.prefs.setBoolPref("devtools.layoutview.open", true);
      this.view.removeAttribute("disable-transitions");
    } else {
      this.view.setAttribute("disable-transitions", "true");
    }
    this.iframe.setAttribute("open", "true");
    this.update();
  },

  /**
   * Close the view container.
   *
   * @param aUserAction Is the action triggered by the user (click on the
   * open/close button in the view)
   */
  close: function LV_close(aUserAction) {
    this.isOpen = false;
    if (this.documentReady)
      this.doc.body.classList.remove("open");
    if (aUserAction) {
      this.inspector._layoutViewIsOpen = false;
      Services.prefs.setBoolPref("devtools.layoutview.open", false);
      this.view.removeAttribute("disable-transitions");
    } else {
      this.view.setAttribute("disable-transitions", "true");
    }
    this.iframe.removeAttribute("open");
  },

  /**
   * Toggle view container state (open/close).
   *
   * @param aUserAction Is the action triggered by the user (click on the
   * open/close button in the view)
   */
  toggle: function LV_toggle(aUserAction) {
    this.isOpen ? this.close(aUserAction):this.open(aUserAction);
  },

  /**
   * Hide the layout boxes. No node are selected.
   */
  dim: function LV_dim() {
    if (!this.documentReady) return;
    this.doc.body.classList.add("dim");
  },

  /**
   * Show the layout boxes. A node is selected.
   */
  undim: function LV_dim() {
    if (!this.documentReady) return;
    this.doc.body.classList.remove("dim");
  },

  /**
   * Compute the dimensions of the node and update the values in
   * the layoutview/view.xhtml document.
   */
  update: function LV_update() {
    let node = this.inspector.selection;
    if (!node || !this.documentReady) return;

    // First, we update the first part of the layout view, with
    // the size of the element.

    let clientRect = node.getBoundingClientRect();
    let width = Math.round(clientRect.width);
    let height = Math.round(clientRect.height);
    this.doc.querySelector("#element-size").textContent =  width + "x" + height;

    // If the view is closed, no need to do anything more.
    if (!this.isOpen) return;

    // We compute and update the values of margins & co.
    let style = this.browser.contentWindow.getComputedStyle(node);;

    for (let i in this.map) {
      let selector = this.map[i].selector;
      let property = this.map[i].property;
      this.map[i].value = parseInt(style.getPropertyValue(property));
    }

    let margins = this.processMargins(node);
    if ("top" in margins) this.map.marginTop.value = "auto";
    if ("right" in margins) this.map.marginRight.value = "auto";
    if ("bottom" in margins) this.map.marginBottom.value = "auto";
    if ("left" in margins) this.map.marginLeft.value = "auto";

    for (let i in this.map) {
      let selector = this.map[i].selector;
      let span = this.doc.querySelector(selector);
      span.textContent = this.map[i].value;
    }

    width -= this.map.borderLeft.value + this.map.borderRight.value +
             this.map.paddingLeft.value + this.map.paddingRight.value;

    height -= this.map.borderTop.value + this.map.borderBottom.value +
              this.map.paddingTop.value + this.map.paddingBottom.value;

    this.doc.querySelector(".size > span").textContent = width + "x" + height;
  },

  /**
   * Find margins declared 'auto'
   */
  processMargins: function LV_processMargins(node) {
    let margins = {};

    for each (let prop in ["top", "bottom", "left", "right"]) {
      let info = this.cssLogic.getPropertyInfo("margin-" + prop);
      let selectors = info.matchedSelectors;
      if (selectors && selectors.length > 0 && selectors[0].value == "auto") {
        margins[prop] = "auto";
      }
    }

    return margins;
  },
}
