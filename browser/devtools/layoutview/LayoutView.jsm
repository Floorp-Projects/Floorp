/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla Layout Module.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2012
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Paul Rouget <paul@mozilla.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

"use strict";

const Cu = Components.utils;
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource:///modules/inspector.jsm");
Cu.import("resource:///modules/devtools/LayoutHelpers.jsm");

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
    function onLock() {
      this.undim();
      this.update();
      // We make sure we never add 2 listeners.
      if (!this.trackingPaint) {
        this.browser.addEventListener("MozAfterPaint", this.update, true);
        this.trackingPaint = true;
      }
    }

    function onUnlock() {
      this.browser.removeEventListener("MozAfterPaint", this.update, true);
      this.trackingPaint = false;
      this.dim();
    }

    this.onLock = onLock.bind(this);
    this.onUnlock = onUnlock.bind(this);
    this.inspector.on("locked", this.onLock);
    this.inspector.on("unlocked", this.onUnlock);

    // Build the layout view in the sidebar.
    this.buildView();

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
    this.inspector.removeListener("locked", this.onLock);
    this.inspector.removeListener("unlocked", this.onUnlock);
    this.browser.removeEventListener("MozAfterPaint", this.update, true);
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
      this.onLock();
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
      let span = this.doc.querySelector(selector);
      span.textContent = this.map[i].value;
    }

    width -= this.map.borderLeft.value + this.map.borderRight.value +
             this.map.paddingLeft.value + this.map.paddingRight.value;

    height -= this.map.borderTop.value + this.map.borderBottom.value +
              this.map.paddingTop.value + this.map.paddingBottom.value;

    this.doc.querySelector(".size > span").textContent = width + "x" + height;
  },
}
