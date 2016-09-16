/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EventEmitter = require("devtools/shared/event-emitter");
var Telemetry = require("devtools/client/shared/telemetry");
var { Task } = require("devtools/shared/task");

/**
 * This object represents replacement for ToolSidebar
 * implemented in devtools/client/framework/sidebar.js module
 *
 * This new component is part of devtools.html aimed at
 * removing XUL and use HTML for entire DevTools UI.
 * There are currently two implementation of the side bar since
 * the `sidebar.js` module (mentioned above) is still used by
 * other panels.
 * As soon as all panels are using this HTML based
 * implementation it can be removed.
 */
function ToolSidebar(tabbox, panel, uid, options = {}) {
  EventEmitter.decorate(this);

  this._tabbox = tabbox;
  this._uid = uid;
  this._panelDoc = this._tabbox.ownerDocument;
  this._toolPanel = panel;
  this._options = options;

  if (!options.disableTelemetry) {
    this._telemetry = new Telemetry();
  }

  this._tabs = [];

  if (this._options.hideTabstripe) {
    this._tabbox.setAttribute("hidetabs", "true");
  }

  this.render();

  this._toolPanel.emit("sidebar-created", this);
}

exports.ToolSidebar = ToolSidebar;

ToolSidebar.prototype = {
  TABPANEL_ID_PREFIX: "sidebar-panel-",

  // React

  get React() {
    return this._toolPanel.React;
  },

  get ReactDOM() {
    return this._toolPanel.ReactDOM;
  },

  get browserRequire() {
    return this._toolPanel.browserRequire;
  },

  get InspectorTabPanel() {
    return this._toolPanel.InspectorTabPanel;
  },

  // Rendering

  render: function () {
    let Tabbar = this.React.createFactory(this.browserRequire(
      "devtools/client/shared/components/tabs/tabbar"));

    let sidebar = Tabbar({
      toolbox: this._toolPanel._toolbox,
      showAllTabsMenu: true,
      onSelect: this.handleSelectionChange.bind(this),
    });

    this._tabbar = this.ReactDOM.render(sidebar, this._tabbox);
  },

  addExistingTab: function (id, title, selected) {
    let panel = this.InspectorTabPanel({
      id: id,
      idPrefix: this.TABPANEL_ID_PREFIX,
      key: id,
      title: title,
    });

    this._tabbar.addTab(id, title, selected, panel);

    this.emit("new-tab-registered", id);
  },

  /**
   * Register a tab. A tab is a document.
   * The document must have a title, which will be used as the name of the tab.
   *
   * @param {string} tab uniq id
   * @param {string} url
   */
  addFrameTab: function (id, title, url, selected) {
    let panel = this.InspectorTabPanel({
      id: id,
      idPrefix: this.TABPANEL_ID_PREFIX,
      key: id,
      title: title,
      url: url,
      onMount: this.onSidePanelMounted.bind(this),
    });

    this._tabbar.addTab(id, title, selected, panel);

    this.emit("new-tab-registered", id);
  },

  onSidePanelMounted: function (content, props) {
    let iframe = content.querySelector("iframe");
    if (!iframe || iframe.getAttribute("src")) {
      return;
    }

    let onIFrameLoaded = (event) => {
      iframe.removeEventListener("load", onIFrameLoaded, true);

      let doc = event.target;
      let win = doc.defaultView;
      if ("setPanel" in win) {
        win.setPanel(this._toolPanel, iframe);
      }
      this.emit(props.id + "-ready");
    };

    iframe.addEventListener("load", onIFrameLoaded, true);
    iframe.setAttribute("src", props.url);
  },

  /**
   * Remove an existing tab.
   * @param {String} tabId The ID of the tab that was used to register it, or
   * the tab id attribute value if the tab existed before the sidebar
   * got created.
   * @param {String} tabPanelId Optional. If provided, this ID will be used
   * instead of the tabId to retrieve and remove the corresponding <tabpanel>
   */
  removeTab: Task.async(function* (tabId, tabPanelId) {
    this._tabbar.removeTab(tabId);

    let win = this.getWindowForTab(tabId);
    if (win && ("destroy" in win)) {
      yield win.destroy();
    }

    this.emit("tab-unregistered", tabId);
  }),

  /**
   * Show or hide a specific tab.
   * @param {Boolean} isVisible True to show the tab/tabpanel, False to hide it.
   * @param {String} id The ID of the tab to be hidden.
   */
  toggleTab: function (isVisible, id) {
    this._tabbar.toggleTab(id, isVisible);
  },

  /**
   * Select a specific tab.
   */
  select: function (id) {
    this._tabbar.select(id);
  },

  /**
   * Return the id of the selected tab.
   */
  getCurrentTabID: function () {
    return this._currentTool;
  },

  /**
   * Returns the requested tab panel based on the id.
   * @param {String} id
   * @return {DOMNode}
   */
  getTabPanel: function (id) {
    // Search with and without the ID prefix as there might have been existing
    // tabpanels by the time the sidebar got created
    return this._panelDoc.querySelector("#" +
      this.TABPANEL_ID_PREFIX + id + ", #" + id);
  },

  /**
   * Event handler.
   */
  handleSelectionChange: function (id) {
    if (this._destroyed) {
      return;
    }

    let previousTool = this._currentTool;
    if (previousTool) {
      if (this._telemetry) {
        this._telemetry.toolClosed(previousTool);
      }
      this.emit(previousTool + "-unselected");
    }

    this._currentTool = id;

    if (this._telemetry) {
      this._telemetry.toolOpened(this._currentTool);
    }

    this.emit(this._currentTool + "-selected");
    this.emit("select", this._currentTool);
  },

  /**
   * Show the sidebar.
   *
   * @param  {String} id
   *         The sidebar tab id to select.
   */
  show: function (id) {
    this._tabbox.removeAttribute("hidden");

    // If an id is given, select the corresponding sidebar tab and record the
    // tool opened.
    if (id) {
      this._currentTool = id;

      if (this._telemetry) {
        this._telemetry.toolOpened(this._currentTool);
      }
    }

    this.emit("show");
  },

  /**
   * Show the sidebar.
   */
  hide: function () {
    this._tabbox.setAttribute("hidden", "true");

    this.emit("hide");
  },

  /**
   * Return the window containing the tab content.
   */
  getWindowForTab: function (id) {
    // Get the tabpanel and make sure it contains an iframe
    let panel = this.getTabPanel(id);
    if (!panel || !panel.firstElementChild || !panel.firstElementChild.contentWindow) {
      return null;
    }

    return panel.firstElementChild.contentWindow;
  },

  /**
   * Clean-up.
   */
  destroy: Task.async(function* () {
    if (this._destroyed) {
      return;
    }
    this._destroyed = true;

    this.emit("destroy");

    // Note that we check for the existence of this._tabbox.tabpanels at each
    // step as the container window may have been closed by the time one of the
    // panel's destroy promise resolves.
    let tabpanels = [...this._tabbox.querySelectorAll(".tab-panel-box")];
    for (let panel of tabpanels) {
      let iframe = panel.querySelector("iframe");
      if (!iframe) {
        continue;
      }
      let win = iframe.contentWindow;
      if (win && ("destroy" in win)) {
        yield win.destroy();
      }
      panel.remove();
    }

    if (this._currentTool && this._telemetry) {
      this._telemetry.toolClosed(this._currentTool);
    }

    this._toolPanel.emit("sidebar-destroyed", this);

    this._tabs = null;
    this._tabbox = null;
    this._panelDoc = null;
    this._toolPanel = null;
  })
};
