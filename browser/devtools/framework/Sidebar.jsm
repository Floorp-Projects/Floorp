/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

this.EXPORTED_SYMBOLS = ["ToolSidebar"];

Cu.import("resource://gre/modules/commonjs/promise/core.js");
Cu.import("resource:///modules/devtools/EventEmitter.jsm");

const XULNS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

/**
 * ToolSidebar provides methods to register tabs in the sidebar.
 * It's assumed that the sidebar contains a xul:tabbox.
 *
 * @param {Node} tabbox
 *  <tabbox> node;
 * @param {ToolPanel} panel
 *  Related ToolPanel instance;
 * @param {Boolean} showTabstripe
 *  Show the tabs.
 */
this.ToolSidebar = function ToolSidebar(tabbox, panel, showTabstripe=true)
{
  EventEmitter.decorate(this);

  this._tabbox = tabbox;
  this._panelDoc = this._tabbox.ownerDocument;
  this._toolPanel = panel;

  this._tabbox.tabpanels.addEventListener("select", this, true);

  this._tabs = new Map();

  if (!showTabstripe) {
    this._tabbox.setAttribute("hidetabs", "true");
  }
}

ToolSidebar.prototype = {
  /**
   * Register a tab. A tab is a document.
   * The document must have a title, which will be used as the name of the tab.
   *
   * @param {string} tab uniq id
   * @param {string} url
   */
  addTab: function ToolSidebar_addTab(id, url, selected=false) {
    let iframe = this._panelDoc.createElementNS(XULNS, "iframe");
    iframe.className = "iframe-" + id;
    iframe.setAttribute("flex", "1");
    iframe.setAttribute("src", url);

    let tab = this._tabbox.tabs.appendItem();

    let onIFrameLoaded = function() {
      tab.setAttribute("label", iframe.contentDocument.title);
      iframe.removeEventListener("DOMContentLoaded", onIFrameLoaded, true);
      if ("setPanel" in iframe.contentWindow) {
        iframe.contentWindow.setPanel(this._toolPanel, iframe);
      }
      this.emit(id + "-ready");
    }.bind(this);

    iframe.addEventListener("DOMContentLoaded", onIFrameLoaded, true);

    let tabpanel = this._panelDoc.createElementNS(XULNS, "tabpanel");
    tabpanel.setAttribute("id", "sidebar-panel-" + id);
    tabpanel.appendChild(iframe);
    this._tabbox.tabpanels.appendChild(tabpanel);

    tab.linkedPanel = "sidebar-panel-" + id;

    // We store the index of this tab.
    this._tabs.set(id, tab);

    if (selected) {
      // For some reason I don't understand, if we call this.select in this
      // event loop (after inserting the tab), the tab will never get the
      // the "selected" attribute set to true.
      this._panelDoc.defaultView.setTimeout(function() {
        this.select(id);
      }.bind(this), 0);
    }

    this.emit("new-tab-registered", id);
  },

  /**
   * Select a specific tab.
   */
  select: function ToolSidebar_select(id) {
    let tab = this._tabs.get(id);
    if (tab) {
      this._tabbox.selectedTab = tab;
    }
  },

  /**
   * Return the id of the selected tab.
   */
  getCurrentTabID: function ToolSidebar_getCurrentTabID() {
    let currentID = null;
    for (let [id, tab] of this._tabs) {
      if (this._tabbox.tabs.selectedItem == tab) {
        currentID = id;
        break;
      }
    }
    return currentID;
  },

  /**
   * Returns the requested tab based on the id.
   *
   * @param String id
   *        unique id of the requested tab.
   */
  getTab: function ToolSidebar_getTab(id) {
    return this._tabbox.tabpanels.querySelector("#sidebar-panel-" + id);
  },

  /**
   * Event handler.
   */
  handleEvent: function ToolSidebar_eventHandler(event) {
    if (event.type == "select") {
      let previousTool = this._currentTool;
      this._currentTool = this.getCurrentTabID();
      if (previousTool) {
        this.emit(previousTool + "-unselected");
      }

      this.emit(this._currentTool + "-selected");
      this.emit("select", this._currentTool);
    }
  },

  /**
   * Toggle sidebar's visibility state.
   */
  toggle: function ToolSidebar_toggle() {
    if (this._tabbox.hasAttribute("hidden")) {
      this.show();
    } else {
      this.hide();
    }
  },

  /**
   * Show the sidebar.
   */
  show: function ToolSidebar_show() {
    this._tabbox.removeAttribute("hidden");
  },

  /**
   * Show the sidebar.
   */
  hide: function ToolSidebar_hide() {
    this._tabbox.setAttribute("hidden", "true");
  },

  /**
   * Return the window containing the tab content.
   */
  getWindowForTab: function ToolSidebar_getWindowForTab(id) {
    if (!this._tabs.has(id)) {
      return null;
    }

    let panel = this._panelDoc.getElementById(this._tabs.get(id).linkedPanel);
    return panel.firstChild.contentWindow;
  },

  /**
   * Clean-up.
   */
  destroy: function ToolSidebar_destroy() {
    if (this._destroyed) {
      return Promise.resolve(null);
    }
    this._destroyed = true;

    this._tabbox.tabpanels.removeEventListener("select", this, true);

    while (this._tabbox.tabpanels.hasChildNodes()) {
      this._tabbox.tabpanels.removeChild(this._tabbox.tabpanels.firstChild);
    }

    while (this._tabbox.tabs.hasChildNodes()) {
      this._tabbox.tabs.removeChild(this._tabbox.tabs.firstChild);
    }

    this._tabs = null;
    this._tabbox = null;
    this._panelDoc = null;
    this._toolPanel = null;

    return Promise.resolve(null);
  },
}
