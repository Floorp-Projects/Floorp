/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var Services = require("Services");
var {Task} = require("devtools/shared/task");
var EventEmitter = require("devtools/shared/event-emitter");
var Telemetry = require("devtools/client/shared/telemetry");

const {LocalizationHelper} = require("devtools/shared/l10n");
const L10N = new LocalizationHelper("devtools/client/locales/toolbox.properties");

const XULNS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

/**
 * ToolSidebar provides methods to register tabs in the sidebar.
 * It's assumed that the sidebar contains a xul:tabbox.
 * Typically, you'll want the tabbox parameter to be a XUL tabbox like this:
 *
 * <tabbox id="inspector-sidebar" handleCtrlTab="false" class="devtools-sidebar-tabs">
 *   <tabs/>
 *   <tabpanels flex="1"/>
 * </tabbox>
 *
 * The ToolSidebar API has a method to add new tabs, so the tabs and tabpanels
 * nodes can be empty. But they can also already contain items before the
 * ToolSidebar is created.
 *
 * Tabs added through the addTab method are only identified by an ID and a URL
 * which is used as the href of an iframe node that is inserted in the newly
 * created tabpanel.
 * Tabs already present before the ToolSidebar is created may contain anything.
 * However, these tabs must have ID attributes if it is required for the various
 * methods that accept an ID as argument to work here.
 *
 * @param {Node} tabbox
 *  <tabbox> node;
 * @param {ToolPanel} panel
 *  Related ToolPanel instance;
 * @param {String} uid
 *  Unique ID
 * @param {Object} options
 *  - hideTabstripe: Should the tabs be hidden. Defaults to false
 *  - showAllTabsMenu: Should a drop-down menu be displayed in case tabs
 *    become hidden. Defaults to false.
 *  - disableTelemetry: By default, switching tabs on and off in the sidebar
 *    will record tool usage in telemetry, pass this option to true to avoid it.
 *
 * Events raised:
 * - new-tab-registered : After a tab has been added via addTab. The tab ID
 *   is passed with the event. This however, is raised before the tab iframe
 *   is fully loaded.
 * - <tabid>-ready : After the tab iframe has been loaded
 * - <tabid>-selected : After tab <tabid> was selected
 * - select : Same as above, but for any tab, the ID is passed with the event
 * - <tabid>-unselected : After tab <tabid> is unselected
 */
function ToolSidebar(tabbox, panel, uid, options = {}) {
  EventEmitter.decorate(this);

  this._tabbox = tabbox;
  this._uid = uid;
  this._panelDoc = this._tabbox.ownerDocument;
  this._toolPanel = panel;
  this._options = options;

  this._onTabBoxOverflow = this._onTabBoxOverflow.bind(this);
  this._onTabBoxUnderflow = this._onTabBoxUnderflow.bind(this);

  try {
    this._width = Services.prefs.getIntPref("devtools.toolsidebar-width." + this._uid);
  } catch (e) {}

  if (!options.disableTelemetry) {
    this._telemetry = new Telemetry();
  }

  this._tabbox.tabpanels.addEventListener("select", this, true);

  this._tabs = new Map();

  // Check for existing tabs in the DOM and add them.
  this.addExistingTabs();

  if (this._options.hideTabstripe) {
    this._tabbox.setAttribute("hidetabs", "true");
  }

  if (this._options.showAllTabsMenu) {
    this.addAllTabsMenu();
  }

  this._toolPanel.emit("sidebar-created", this);
}

exports.ToolSidebar = ToolSidebar;

ToolSidebar.prototype = {
  TAB_ID_PREFIX: "sidebar-tab-",

  TABPANEL_ID_PREFIX: "sidebar-panel-",

  /**
   * Add a "…" button at the end of the tabstripe that toggles a dropdown menu
   * containing the list of all tabs if any become hidden due to lack of room.
   *
   * If the ToolSidebar was created with the "showAllTabsMenu" option set to
   * true, this is already done automatically. If not, you may call this
   * function at any time to add the menu.
   */
  addAllTabsMenu: function () {
    if (this._allTabsBtn) {
      return;
    }

    let tabs = this._tabbox.tabs;

    // Create a container and insert it first in the tabbox
    let allTabsContainer = this._panelDoc.createElementNS(XULNS, "stack");
    this._tabbox.insertBefore(allTabsContainer, tabs);

    // Move the tabs inside and make them flex
    allTabsContainer.appendChild(tabs);
    tabs.setAttribute("flex", "1");

    // Create the dropdown menu next to the tabs
    this._allTabsBtn = this._panelDoc.createElementNS(XULNS, "toolbarbutton");
    this._allTabsBtn.setAttribute("class", "devtools-sidebar-alltabs");
    this._allTabsBtn.setAttribute("end", "0");
    this._allTabsBtn.setAttribute("top", "0");
    this._allTabsBtn.setAttribute("width", "15");
    this._allTabsBtn.setAttribute("type", "menu");
    this._allTabsBtn.setAttribute("tooltiptext",
      L10N.getStr("sidebar.showAllTabs.tooltip"));
    this._allTabsBtn.setAttribute("hidden", "true");
    allTabsContainer.appendChild(this._allTabsBtn);

    let menuPopup = this._panelDoc.createElementNS(XULNS, "menupopup");
    this._allTabsBtn.appendChild(menuPopup);

    // Listening to tabs overflow event to toggle the alltabs button
    tabs.addEventListener("overflow", this._onTabBoxOverflow, false);
    tabs.addEventListener("underflow", this._onTabBoxUnderflow, false);

    // Add menuitems to the alltabs menu if there are already tabs in the
    // sidebar
    for (let [id, tab] of this._tabs) {
      let item = this._addItemToAllTabsMenu(id, tab, {
        selected: tab.hasAttribute("selected")
      });
      if (tab.hidden) {
        item.hidden = true;
      }
    }
  },

  removeAllTabsMenu: function () {
    if (!this._allTabsBtn) {
      return;
    }

    let tabs = this._tabbox.tabs;

    tabs.removeEventListener("overflow", this._onTabBoxOverflow, false);
    tabs.removeEventListener("underflow", this._onTabBoxUnderflow, false);

    // Moving back the tabs as a first child of the tabbox
    this._tabbox.insertBefore(tabs, this._tabbox.tabpanels);
    this._tabbox.querySelector("stack").remove();

    this._allTabsBtn = null;
  },

  _onTabBoxOverflow: function () {
    this._allTabsBtn.removeAttribute("hidden");
  },

  _onTabBoxUnderflow: function () {
    this._allTabsBtn.setAttribute("hidden", "true");
  },

  /**
   * Add an item in the allTabs menu for a given tab.
   */
  _addItemToAllTabsMenu: function (id, tab, options) {
    if (!this._allTabsBtn) {
      return;
    }

    let item = this._panelDoc.createElementNS(XULNS, "menuitem");
    let idPrefix = "sidebar-alltabs-item-";
    item.setAttribute("id", idPrefix + id);
    item.setAttribute("label", tab.getAttribute("label"));
    item.setAttribute("type", "checkbox");
    if (options.selected) {
      item.setAttribute("checked", true);
    }
    // The auto-checking of menuitems in this menu doesn't work, so let's do
    // it manually
    item.setAttribute("autocheck", false);

    let menu = this._allTabsBtn.querySelector("menupopup");
    if (options.insertBefore) {
      let referenceItem = menu.querySelector(`#${idPrefix}${options.insertBefore}`);
      menu.insertBefore(item, referenceItem);
    } else {
      menu.appendChild(item);
    }

    item.addEventListener("click", () => {
      this._tabbox.selectedTab = tab;
    }, false);

    tab.allTabsMenuItem = item;

    return item;
  },

  /**
   * Register a tab. A tab is a document.
   * The document must have a title, which will be used as the name of the tab.
   *
   * @param {string} id The unique id for this tab.
   * @param {string} url The URL of the document to load in this new tab.
   * @param {Object} options A set of options for this new tab:
   * - {Boolean} selected Set to true to make this new tab selected by default.
   * - {String} insertBefore By default, the new tab is appended at the end of the
   * tabbox, pass the ID of an existing tab to insert it before that tab instead.
   */
  addTab: function (id, url, options = {}) {
    let iframe = this._panelDoc.createElementNS(XULNS, "iframe");
    iframe.className = "iframe-" + id;
    iframe.setAttribute("flex", "1");
    iframe.setAttribute("src", url);
    iframe.tooltip = "aHTMLTooltip";

    // Creating the tab and adding it to the tabbox
    let tab = this._panelDoc.createElementNS(XULNS, "tab");

    tab.setAttribute("id", this.TAB_ID_PREFIX + id);
    tab.setAttribute("crop", "end");
    // Avoid showing "undefined" while the tab is loading
    tab.setAttribute("label", "");

    if (options.insertBefore) {
      let referenceTab = this.getTab(options.insertBefore);
      this._tabbox.tabs.insertBefore(tab, referenceTab);
    } else {
      this._tabbox.tabs.appendChild(tab);
    }

    // Add the tab to the allTabs menu if exists
    let allTabsItem = this._addItemToAllTabsMenu(id, tab, options);

    let onIFrameLoaded = (event) => {
      let doc = event.target;
      let win = doc.defaultView;
      tab.setAttribute("label", doc.title);

      if (allTabsItem) {
        allTabsItem.setAttribute("label", doc.title);
      }

      iframe.removeEventListener("load", onIFrameLoaded, true);
      if ("setPanel" in win) {
        win.setPanel(this._toolPanel, iframe);
      }
      this.emit(id + "-ready");
    };

    iframe.addEventListener("load", onIFrameLoaded, true);

    let tabpanel = this._panelDoc.createElementNS(XULNS, "tabpanel");
    tabpanel.setAttribute("id", this.TABPANEL_ID_PREFIX + id);
    tabpanel.appendChild(iframe);

    if (options.insertBefore) {
      let referenceTabpanel = this.getTabPanel(options.insertBefore);
      this._tabbox.tabpanels.insertBefore(tabpanel, referenceTabpanel);
    } else {
      this._tabbox.tabpanels.appendChild(tabpanel);
    }

    this._tooltip = this._panelDoc.createElementNS(XULNS, "tooltip");
    this._tooltip.id = "aHTMLTooltip";
    tabpanel.appendChild(this._tooltip);
    this._tooltip.page = true;

    tab.linkedPanel = this.TABPANEL_ID_PREFIX + id;

    // We store the index of this tab.
    this._tabs.set(id, tab);

    if (options.selected) {
      this._selectTabSoon(id);
    }

    this.emit("new-tab-registered", id);
  },

  untitledTabsIndex: 0,

  /**
   * Search for existing tabs in the markup that aren't know yet and add them.
   */
  addExistingTabs: function () {
    let knownTabs = [...this._tabs.values()];

    for (let tab of this._tabbox.tabs.querySelectorAll("tab")) {
      if (knownTabs.indexOf(tab) !== -1) {
        continue;
      }

      // Find an ID for this unknown tab
      let id = tab.getAttribute("id") || "untitled-tab-" + (this.untitledTabsIndex++);

      // If the existing tab contains the tab ID prefix, extract the ID of the
      // tab
      if (id.startsWith(this.TAB_ID_PREFIX)) {
        id = id.split(this.TAB_ID_PREFIX).pop();
      }

      // Register the tab
      this._tabs.set(id, tab);
      this.emit("new-tab-registered", id);
    }
  },

  /**
   * Remove an existing tab.
   * @param {String} tabId The ID of the tab that was used to register it, or
   * the tab id attribute value if the tab existed before the sidebar got created.
   * @param {String} tabPanelId Optional. If provided, this ID will be used
   * instead of the tabId to retrieve and remove the corresponding <tabpanel>
   */
  removeTab: Task.async(function* (tabId, tabPanelId) {
    // Remove the tab if it can be found
    let tab = this.getTab(tabId);
    if (!tab) {
      return;
    }

    let win = this.getWindowForTab(tabId);
    if (win && ("destroy" in win)) {
      yield win.destroy();
    }

    tab.remove();

    // Also remove the tabpanel
    let panel = this.getTabPanel(tabPanelId || tabId);
    if (panel) {
      panel.remove();
    }

    this._tabs.delete(tabId);
    this.emit("tab-unregistered", tabId);
  }),

  /**
   * Show or hide a specific tab.
   * @param {Boolean} isVisible True to show the tab/tabpanel, False to hide it.
   * @param {String} id The ID of the tab to be hidden.
   */
  toggleTab: function (isVisible, id) {
    // Toggle the tab.
    let tab = this.getTab(id);
    if (!tab) {
      return;
    }
    tab.hidden = !isVisible;

    // Toggle the item in the allTabs menu.
    if (this._allTabsBtn) {
      this._allTabsBtn.querySelector("#sidebar-alltabs-item-" + id).hidden = !isVisible;
    }
  },

  /**
   * Select a specific tab.
   */
  select: function (id) {
    let tab = this.getTab(id);
    if (tab) {
      this._tabbox.selectedTab = tab;
    }
  },

  /**
   * Hack required to select a tab right after it was created.
   *
   * @param  {String} id
   *         The sidebar tab id to select.
   */
  _selectTabSoon: function (id) {
    this._panelDoc.defaultView.setTimeout(() => {
      this.select(id);
    }, 0);
  },

  /**
   * Return the id of the selected tab.
   */
  getCurrentTabID: function () {
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
   * Returns the requested tab panel based on the id.
   * @param {String} id
   * @return {DOMNode}
   */
  getTabPanel: function (id) {
    // Search with and without the ID prefix as there might have been existing
    // tabpanels by the time the sidebar got created
    return this._tabbox.tabpanels.querySelector("#" + this.TABPANEL_ID_PREFIX + id + ", #" + id);
  },

  /**
   * Return the tab based on the provided id, if one was registered with this id.
   * @param {String} id
   * @return {DOMNode}
   */
  getTab: function (id) {
    // FIXME: A workaround for broken browser_net_raw_headers.js failure only in non-e10s mode
    return this._tabs && this._tabs.get(id);
  },

  /**
   * Event handler.
   */
  handleEvent: function (event) {
    if (event.type !== "select" || this._destroyed) {
      return;
    }

    if (this._currentTool == this.getCurrentTabID()) {
      // Tool hasn't changed.
      return;
    }

    let previousTool = this._currentTool;
    this._currentTool = this.getCurrentTabID();
    if (previousTool) {
      if (this._telemetry) {
        this._telemetry.toolClosed(previousTool);
      }
      this.emit(previousTool + "-unselected");
    }

    if (this._telemetry) {
      this._telemetry.toolOpened(this._currentTool);
    }

    this.emit(this._currentTool + "-selected");
    this.emit("select", this._currentTool);

    // Handlers for "select"/"...-selected"/"...-unselected" events might have
    // destroyed the sidebar in the meantime.
    if (this._destroyed) {
      return;
    }

    // Handle menuitem selection if the allTabsMenu is there by unchecking all
    // items except the selected one.
    let tab = this._tabbox.selectedTab;
    if (tab.allTabsMenuItem) {
      for (let otherItem of this._allTabsBtn.querySelectorAll("menuitem")) {
        otherItem.removeAttribute("checked");
      }
      tab.allTabsMenuItem.setAttribute("checked", true);
    }
  },

  /**
   * Toggle sidebar's visibility state.
   */
  toggle: function () {
    if (this._tabbox.hasAttribute("hidden")) {
      this.show();
    } else {
      this.hide();
    }
  },

  /**
   * Show the sidebar.
   *
   * @param  {String} id
   *         The sidebar tab id to select.
   */
  show: function (id) {
    if (this._width) {
      this._tabbox.width = this._width;
    }
    this._tabbox.removeAttribute("hidden");

    // If an id is given, select the corresponding sidebar tab and record the
    // tool opened.
    if (id) {
      this._currentTool = id;

      if (this._telemetry) {
        this._telemetry.toolOpened(this._currentTool);
      }

      this._selectTabSoon(id);
    }

    this.emit("show");
  },

  /**
   * Show the sidebar.
   */
  hide: function () {
    Services.prefs.setIntPref("devtools.toolsidebar-width." + this._uid, this._tabbox.width);
    this._tabbox.setAttribute("hidden", "true");
    this._panelDoc.activeElement.blur();

    this.emit("hide");
  },

  /**
   * Return the window containing the tab content.
   */
  getWindowForTab: function (id) {
    if (!this._tabs.has(id)) {
      return null;
    }

    // Get the tabpanel and make sure it contains an iframe
    let panel = this.getTabPanel(id);
    if (!panel || !panel.firstChild || !panel.firstChild.contentWindow) {
      return;
    }
    return panel.firstChild.contentWindow;
  },

  /**
   * Clean-up.
   */
  destroy: Task.async(function* () {
    if (this._destroyed) {
      return;
    }
    this._destroyed = true;

    Services.prefs.setIntPref("devtools.toolsidebar-width." + this._uid, this._tabbox.width);

    if (this._allTabsBtn) {
      this.removeAllTabsMenu();
    }

    this._tabbox.tabpanels.removeEventListener("select", this, true);

    // Note that we check for the existence of this._tabbox.tabpanels at each
    // step as the container window may have been closed by the time one of the
    // panel's destroy promise resolves.
    while (this._tabbox.tabpanels && this._tabbox.tabpanels.hasChildNodes()) {
      let panel = this._tabbox.tabpanels.firstChild;
      let win = panel.firstChild.contentWindow;
      if (win && ("destroy" in win)) {
        yield win.destroy();
      }
      panel.remove();
    }

    while (this._tabbox.tabs && this._tabbox.tabs.hasChildNodes()) {
      this._tabbox.tabs.removeChild(this._tabbox.tabs.firstChild);
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
