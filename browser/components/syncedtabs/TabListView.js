/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Services.jsm");

let { getChromeWindow } = Cu.import("resource:///modules/syncedtabs/util.js", {});

let log = Cu.import("resource://gre/modules/Log.jsm", {})
            .Log.repository.getLogger("Sync.RemoteTabs");

this.EXPORTED_SYMBOLS = [
  "TabListView"
];

function getContextMenu(window) {
  return getChromeWindow(window).document.getElementById("SyncedTabsSidebarContext");
}

function getTabsFilterContextMenu(window) {
  return getChromeWindow(window).document.getElementById("SyncedTabsSidebarTabsFilterContext");
}

/*
 * TabListView
 *
 * Given a state, this object will render the corresponding DOM.
 * It maintains no state of it's own. It listens for DOM events
 * and triggers actions that may cause the state to change and
 * ultimately the view to rerender.
 */
function TabListView(window, props) {
  this.props = props;

  this._window = window;
  this._doc = this._window.document;

  this._tabsContainerTemplate = this._doc.getElementById("tabs-container-template");
  this._clientTemplate = this._doc.getElementById("client-template");
  this._emptyClientTemplate = this._doc.getElementById("empty-client-template");
  this._tabTemplate = this._doc.getElementById("tab-template");
  this.tabsFilter = this._doc.querySelector(".tabsFilter");
  this.clearFilter = this._doc.querySelector(".textbox-search-clear");
  this.searchBox = this._doc.querySelector(".search-box");
  this.searchIcon = this._doc.querySelector(".textbox-search-icon");

  this.container = this._doc.createElement("div");

  this._attachFixedListeners();

  this._setupContextMenu();
}

TabListView.prototype = {
  render(state) {
    // Don't rerender anything; just update attributes, e.g. selection
    if (state.canUpdateAll) {
      this._update(state);
      return;
    }
    // Rerender the tab list
    if (state.canUpdateInput) {
      this._updateSearchBox(state);
      this._createList(state);
      return;
    }
    // Create the world anew
    this._create(state);
  },

  // Create the initial DOM from templates
  _create(state) {
    let wrapper = this._doc.importNode(this._tabsContainerTemplate.content, true).firstElementChild;
    this._clearChilden();
    this.container.appendChild(wrapper);

    this.list = this.container.querySelector(".list");

    this._createList(state);
    this._updateSearchBox(state);

    this._attachListListeners();
  },

  _createList(state) {
    this._clearChilden(this.list);
    for (let client of state.clients) {
      if (state.filter) {
        this._renderFilteredClient(client);
      } else {
        this._renderClient(client);
      }
    }
    if (this.list.firstChild) {
      const firstTab = this.list.firstChild.querySelector(".item.tab:first-child .item-title");
      if (firstTab) {
        firstTab.setAttribute("tabindex", 2);
      }
    }
  },

  destroy() {
    this._teardownContextMenu();
    this.container.remove();
  },

  _update(state) {
    this._updateSearchBox(state);
    for (let client of state.clients) {
      let clientNode = this._doc.getElementById("item-" + client.id);
      if (clientNode) {
        this._updateClient(client, clientNode);
      }

      client.tabs.forEach((tab, index) => {
        let tabNode = this._doc.getElementById('tab-' + client.id + '-' + index);
        this._updateTab(tab, tabNode, index);
      });
    }
  },

  // Client rows are hidden when the list is filtered
  _renderFilteredClient(client, filter) {
    client.tabs.forEach((tab, index) => {
      let node = this._renderTab(client, tab, index);
      this.list.appendChild(node);
    });
  },

  _renderClient(client) {
    let itemNode = client.tabs.length ?
                    this._createClient(client) :
                    this._createEmptyClient(client);

    this._updateClient(client, itemNode);

    let tabsList = itemNode.querySelector(".item-tabs-list");
    client.tabs.forEach((tab, index) => {
      let node = this._renderTab(client, tab, index);
      tabsList.appendChild(node);
    });

    this.list.appendChild(itemNode);
    return itemNode;
  },

  _renderTab(client, tab, index) {
    let itemNode = this._createTab(tab);
    this._updateTab(tab, itemNode, index);
    return itemNode;
  },

  _createClient(item) {
    return this._doc.importNode(this._clientTemplate.content, true).firstElementChild;
  },

  _createEmptyClient(item) {
    return this._doc.importNode(this._emptyClientTemplate.content, true).firstElementChild;
  },

  _createTab(item) {
    return this._doc.importNode(this._tabTemplate.content, true).firstElementChild;
  },

  _clearChilden(node) {
    let parent = node || this.container;
    while (parent.firstChild) {
      parent.removeChild(parent.firstChild);
    }
  },

  // These listeners are attached only once, when we initialize the view
  _attachFixedListeners() {
    this.tabsFilter.addEventListener("input", this.onFilter.bind(this));
    this.tabsFilter.addEventListener("focus", this.onFilterFocus.bind(this));
    this.tabsFilter.addEventListener("blur", this.onFilterBlur.bind(this));
    this.clearFilter.addEventListener("click", this.onClearFilter.bind(this));
    this.searchIcon.addEventListener("click", this.onFilterFocus.bind(this));
  },

  // These listeners have to be re-created every time since we re-create the list
  _attachListListeners() {
    this.list.addEventListener("click", this.onClick.bind(this));
    this.list.addEventListener("mouseup", this.onMouseUp.bind(this));
    this.list.addEventListener("keydown", this.onKeyDown.bind(this));
  },

  _updateSearchBox(state) {
    if (state.filter) {
      this.searchBox.classList.add("filtered");
    } else {
      this.searchBox.classList.remove("filtered");
    }
    this.tabsFilter.value = state.filter;
    if (state.inputFocused) {
      this.searchBox.setAttribute("focused", true);
      this.tabsFilter.focus();
    } else {
      this.searchBox.removeAttribute("focused");
    }
  },

  /**
   * Update the element representing an item, ensuring it's in sync with the
   * underlying data.
   * @param {client} item - Item to use as a source.
   * @param {Element} itemNode - Element to update.
   */
  _updateClient(item, itemNode) {
    itemNode.setAttribute("id", "item-" + item.id);
    let lastSync = new Date(item.lastModified);
    let lastSyncTitle = getChromeWindow(this._window).gSyncUI.formatLastSyncDate(lastSync);
    itemNode.setAttribute("title", lastSyncTitle);
    if (item.closed) {
      itemNode.classList.add("closed");
    } else {
      itemNode.classList.remove("closed");
    }
    if (item.selected) {
      itemNode.classList.add("selected");
    } else {
      itemNode.classList.remove("selected");
    }
    if (item.isMobile) {
      itemNode.classList.add("device-image-mobile");
    } else {
      itemNode.classList.add("device-image-desktop");
    }
    if (item.focused) {
      itemNode.focus();
    }
    itemNode.dataset.id = item.id;
    itemNode.querySelector(".item-title").textContent = item.name;
  },

  /**
   * Update the element representing a tab, ensuring it's in sync with the
   * underlying data.
   * @param {tab} item - Item to use as a source.
   * @param {Element} itemNode - Element to update.
   */
  _updateTab(item, itemNode, index) {
    itemNode.setAttribute("title", `${item.title}\n${item.url}`);
    itemNode.setAttribute("id", "tab-" + item.client + '-' + index);
    if (item.selected) {
      itemNode.classList.add("selected");
    } else {
      itemNode.classList.remove("selected");
    }
    if (item.focused) {
      itemNode.focus();
    }
    itemNode.dataset.url = item.url;

    itemNode.querySelector(".item-title").textContent = item.title;

    if (item.icon) {
      let icon = itemNode.querySelector(".item-icon-container");
      icon.style.backgroundImage = "url(" + item.icon + ")";
    }
  },

  onMouseUp(event) {
    if (event.which == 2) { // Middle click
      this.onClick(event);
    }
  },

  onClick(event) {
    let itemNode = this._findParentItemNode(event.target);
    if (!itemNode) {
      return;
    }

    if (itemNode.classList.contains("tab")) {
      let url = itemNode.dataset.url;
      if (url) {
        this.onOpenSelected(url, event);
      }
    }

    // Middle click on a client
    if (itemNode.classList.contains("client")) {
      let where = getChromeWindow(this._window).whereToOpenLink(event);
      if (where != "current") {
        this._openAllClientTabs(itemNode, where);
      }
    }

    if (event.target.classList.contains("item-twisty-container")
        && event.which != 2) {
      this.props.onToggleBranch(itemNode.dataset.id);
      return;
    }

    let position = this._getSelectionPosition(itemNode);
    this.props.onSelectRow(position);
  },

  /**
   * Handle a keydown event on the list box.
   * @param {Event} event - Triggering event.
   */
  onKeyDown(event) {
    if (event.keyCode == this._window.KeyEvent.DOM_VK_DOWN) {
      event.preventDefault();
      this.props.onMoveSelectionDown();
    } else if (event.keyCode == this._window.KeyEvent.DOM_VK_UP) {
      event.preventDefault();
      this.props.onMoveSelectionUp();
    } else if (event.keyCode == this._window.KeyEvent.DOM_VK_RETURN) {
      let selectedNode = this.container.querySelector('.item.selected');
      if (selectedNode.dataset.url) {
        this.onOpenSelected(selectedNode.dataset.url, event);
      } else if (selectedNode) {
        this.props.onToggleBranch(selectedNode.dataset.id);
      }
    }
  },

  onBookmarkTab() {
    let item = this._getSelectedTabNode();
    if (item) {
      let title = item.querySelector(".item-title").textContent;
      this.props.onBookmarkTab(item.dataset.url, title);
    }
  },

  onCopyTabLocation() {
    let item = this._getSelectedTabNode();
    if (item) {
      this.props.onCopyTabLocation(item.dataset.url);
    }
  },

  onOpenSelected(url, event) {
    let where = getChromeWindow(this._window).whereToOpenLink(event);
    this.props.onOpenTab(url, where, {});
  },

  onOpenSelectedFromContextMenu(event) {
    let item = this._getSelectedTabNode();
    if (item) {
      let where = event.target.getAttribute("where");
      let params = {
        private: event.target.hasAttribute("private"),
      };
      this.props.onOpenTab(item.dataset.url, where, params);
    }
  },

  onOpenAllInTabs() {
    let item = this._getSelectedClientNode();
    if (item) {
      this._openAllClientTabs(item, "tab");
    }
  },

  onFilter(event) {
    let query = event.target.value;
    if (query) {
      this.props.onFilter(query);
    } else {
      this.props.onClearFilter();
    }
  },

  onClearFilter() {
    this.props.onClearFilter();
  },

  onFilterFocus() {
    this.props.onFilterFocus();
  },
  onFilterBlur() {
    this.props.onFilterBlur();
  },

  _getSelectedTabNode() {
    let item = this.container.querySelector('.item.selected');
    if (this._isTab(item) && item.dataset.url) {
      return item;
    }
    return null;
  },

  _getSelectedClientNode() {
    let item = this.container.querySelector('.item.selected');
    if (this._isClient(item)) {
      return item;
    }
    return null;
  },

  // Set up the custom context menu
  _setupContextMenu() {
    Services.els.addSystemEventListener(this._window, "contextmenu", this, false);
    for (let getMenu of [getContextMenu, getTabsFilterContextMenu]) {
      let menu = getMenu(this._window);
      menu.addEventListener("popupshowing", this, true);
      menu.addEventListener("command", this, true);
    }
  },

  _teardownContextMenu() {
    // Tear down context menu
    Services.els.removeSystemEventListener(this._window, "contextmenu", this, false);
    for (let getMenu of [getContextMenu, getTabsFilterContextMenu]) {
      let menu = getMenu(this._window);
      menu.removeEventListener("popupshowing", this, true);
      menu.removeEventListener("command", this, true);
    }
  },

  handleEvent(event) {
    switch (event.type) {
      case "contextmenu":
        this.handleContextMenu(event);
        break;

      case "popupshowing": {
        if (event.target.getAttribute("id") == "SyncedTabsSidebarTabsFilterContext") {
          this.handleTabsFilterContextMenuShown(event);
        }
        break;
      }

      case "command": {
        let menu = event.target.closest("menupopup");
        switch (menu.getAttribute("id")) {
          case "SyncedTabsSidebarContext":
            this.handleContentContextMenuCommand(event);
            break;

          case "SyncedTabsSidebarTabsFilterContext":
            this.handleTabsFilterContextMenuCommand(event);
            break;
        }
        break;
      }
    }
  },

  handleTabsFilterContextMenuShown(event) {
    let document = event.target.ownerDocument;
    let focusedElement = document.commandDispatcher.focusedElement;
    if (focusedElement != this.tabsFilter) {
      this.tabsFilter.focus();
    }
    for (let item of event.target.children) {
      if (!item.hasAttribute("cmd")) {
        continue;
      }
      let command = item.getAttribute("cmd");
      let controller = document.commandDispatcher.getControllerForCommand(command);
      if (controller.isCommandEnabled(command)) {
        item.removeAttribute("disabled");
      } else {
        item.setAttribute("disabled", "true");
      }
    }
  },

  handleContentContextMenuCommand(event) {
    let id = event.target.getAttribute("id");
    switch (id) {
      case "syncedTabsOpenSelected":
      case "syncedTabsOpenSelectedInTab":
      case "syncedTabsOpenSelectedInWindow":
      case "syncedTabsOpenSelectedInPrivateWindow":
        this.onOpenSelectedFromContextMenu(event);
        break;
      case "syncedTabsOpenAllInTabs":
        this.onOpenAllInTabs();
        break;
      case "syncedTabsBookmarkSelected":
        this.onBookmarkTab();
        break;
      case "syncedTabsCopySelected":
        this.onCopyTabLocation();
        break;
      case "syncedTabsRefresh":
      case "syncedTabsRefreshFilter":
        this.props.onSyncRefresh();
        break;
    }
  },

  handleTabsFilterContextMenuCommand(event) {
    let command = event.target.getAttribute("cmd");
    let dispatcher = getChromeWindow(this._window).document.commandDispatcher;
    let controller = dispatcher.focusedElement.controllers.getControllerForCommand(command);
    controller.doCommand(command);
  },

  handleContextMenu(event) {
    let menu;

    if (event.target == this.tabsFilter) {
      menu = getTabsFilterContextMenu(this._window);
    } else {
      let itemNode = this._findParentItemNode(event.target);
      if (itemNode) {
        let position = this._getSelectionPosition(itemNode);
        this.props.onSelectRow(position);
      }
      menu = getContextMenu(this._window);
      this.adjustContextMenu(menu);
    }

    menu.openPopupAtScreen(event.screenX, event.screenY, true, event);
  },

  adjustContextMenu(menu) {
    let item = this.container.querySelector('.item.selected');
    let showTabOptions = this._isTab(item);

    let el = menu.firstChild;

    while (el) {
      let show = false;
      if (showTabOptions) {
        if (el.getAttribute("id") != "syncedTabsOpenAllInTabs") {
          show = true;
        }
      } else if (el.getAttribute("id") == "syncedTabsOpenAllInTabs") {
        const tabs = item.querySelectorAll(".item-tabs-list > .item.tab");
        show = tabs.length > 0;
      } else if (el.getAttribute("id") == "syncedTabsRefresh") {
        show = true;
      }
      el.hidden = !show;

      el = el.nextSibling;
    }
  },

  /**
   * Find the parent item element, from a given child element.
   * @param {Element} node - Child element.
   * @return {Element} Element for the item, or null if not found.
   */
  _findParentItemNode(node) {
    while (node && node !== this.list && node !== this._doc.documentElement &&
           !node.classList.contains("item")) {
      node = node.parentNode;
    }

    if (node !== this.list && node !== this._doc.documentElement) {
      return node;
    }

    return null;
  },

  _findParentBranchNode(node) {
    while (node && !node.classList.contains("list") && node !== this._doc.documentElement &&
           !node.parentNode.classList.contains("list")) {
      node = node.parentNode;
    }

    if (node !== this.list && node !== this._doc.documentElement) {
      return node;
    }

    return null;
  },

  _getSelectionPosition(itemNode) {
    let parent = this._findParentBranchNode(itemNode);
    let parentPosition = this._indexOfNode(parent.parentNode, parent);
    let childPosition = -1;
    // if the node is not a client, find its position within the parent
    if (parent !== itemNode) {
      childPosition = this._indexOfNode(itemNode.parentNode, itemNode);
    }
    return [parentPosition, childPosition];
  },

  _indexOfNode(parent, child) {
    return Array.prototype.indexOf.call(parent.childNodes, child);
  },

  _isTab(item) {
    return item && item.classList.contains("tab");
  },

  _isClient(item) {
    return item && item.classList.contains("client");
  },

  _openAllClientTabs(clientNode, where) {
    const tabs = clientNode.querySelector(".item-tabs-list").childNodes;
    const urls = [...tabs].map(tab => tab.dataset.url);
    this.props.onOpenTabs(urls, where);
  }
};
