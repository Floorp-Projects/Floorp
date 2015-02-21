/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource:///modules/readinglist/ReadingList.jsm");

let log = Cu.import("resource://gre/modules/Log.jsm", {})
            .Log.repository.getLogger("readinglist.sidebar");


let RLSidebar = {
  /**
   * Container element for all list item elements.
   * @type {Element}
   */
  list: null,

  /**
   * <template> element used for constructing list item elements.
   * @type {Element}
   */
  itemTemplate: null,

  /**
   * Map of ReadingList Item objects, keyed by their ID.
   * @type {Map}
   */
  itemsById: new Map(),
  /**
   * Map of list item elements, keyed by their corresponding Item's ID.
   * @type {Map}
   */
  itemNodesById: new Map(),

  /**
   * Initialize the sidebar UI.
   */
  init() {
    log.debug("Initializing");

    addEventListener("unload", () => this.uninit());

    this.list = document.getElementById("list");
    this.itemTemplate = document.getElementById("item-template");

    this.list.addEventListener("click", event => this.onListClick(event));
    this.list.addEventListener("mousemove", event => this.onListMouseMove(event));
    this.list.addEventListener("keydown", event => this.onListKeyDown(event), true);

    this.ensureListItems();
    ReadingList.addListener(this);

    let initEvent = new CustomEvent("Initialized", {bubbles: true});
    document.documentElement.dispatchEvent(initEvent);
  },

  /**
   * Un-initialize the sidebar UI.
   */
  uninit() {
    log.debug("Shutting down");

    ReadingList.removeListener(this);
  },

  /**
   * Handle an item being added to the ReadingList.
   * TODO: We may not want to show this new item right now.
   * TODO: We should guard against the list growing here.
   *
   * @param {Readinglist.Item} item - Item that was added.
   */
  onItemAdded(item) {
    log.trace(`onItemAdded: ${item}`);

    let itemNode = document.importNode(this.itemTemplate.content, true).firstElementChild;
    this.updateItem(item, itemNode);
    this.list.appendChild(itemNode);
    this.itemNodesById.set(item.id, itemNode);
    this.itemsById.set(item.id, item);
  },

  /**
   * Handle an item being deleted from the ReadingList.
   * @param {ReadingList.Item} item - Item that was deleted.
   */
  onItemDeleted(item) {
    log.trace(`onItemDeleted: ${item}`);

    let itemNode = this.itemNodesById.get(item.id);
    itemNode.remove();
    this.itemNodesById.delete(item.id);
    this.itemsById.delete(item.id);
    // TODO: ensureListItems doesn't yet cope with needing to add one item.
    //this.ensureListItems();
  },

  /**
   * Handle an item in the ReadingList having any of its properties changed.
   * @param {ReadingList.Item} item - Item that was updated.
   */
  onItemUpdated(item) {
    log.trace(`onItemUpdated: ${item}`);

    let itemNode = this.itemNodesById.get(item.id);
    if (!itemNode)
      return;

    this.updateItem(item, itemNode);
  },

  /**
   * Update the element representing an item, ensuring it's in sync with the
   * underlying data.
   * @param {ReadingList.Item} item - Item to use as a source.
   * @param {Element} itemNode - Element to update.
   */
  updateItem(item, itemNode) {
    itemNode.setAttribute("id", "item-" + item.id);
    itemNode.setAttribute("title", `${item.title}\n${item.url.spec}`);

    itemNode.querySelector(".item-title").textContent = item.title;
    itemNode.querySelector(".item-domain").textContent = item.domain;
  },

  /**
   * Ensure that the list is populated with the correct items.
   */
  ensureListItems() {
    ReadingList.getItems().then(items => {
      for (let item of items) {
        // TODO: Should be batch inserting via DocumentFragment
        try {
          this.onItemAdded(item);
        } catch (e) {
          log.warn("Error adding item", e);
        }
      }
    });
  },

  /**
   * Get the number of items currently displayed in the list.
   * @type {number}
   */
  get numItems() {
    return this.list.childElementCount;
  },

  /**
   * The currently active element in the list.
   * @type {Element}
   */
  get activeItem() {
    return document.querySelector("#list > .item.active");
  },

  set activeItem(node) {
    if (node && node.parentNode != this.list) {
      log.error(`Unable to set activeItem to invalid node ${node}`);
      return;
    }

    log.debug(`Setting activeItem: ${node ? node.id : null}`);

    if (node) {
      if (!node.classList.contains("selected")) {
        this.selectedItem = node;
      }

      if (node.classList.contains("active")) {
        return;
      }
    }

    let prevItem = document.querySelector("#list > .item.active");
    if (prevItem) {
      prevItem.classList.remove("active");
    }

    if (node) {
      node.classList.add("active");
    }

    let event = new CustomEvent("ActiveItemChanged", {bubbles: true});
    this.list.dispatchEvent(event);
  },

  /**
   * The currently selected item in the list.
   * @type {Element}
   */
  get selectedItem() {
    return document.querySelector("#list > .item.selected");
  },

  set selectedItem(node) {
    if (node && node.parentNode != this.list) {
      log.error(`Unable to set selectedItem to invalid node ${node}`);
      return;
    }

    log.debug(`Setting activeItem: ${node ? node.id : null}`);

    let prevItem = document.querySelector("#list > .item.selected");
    if (prevItem) {
      prevItem.classList.remove("selected");
    }

    if (node) {
      node.classList.add("selected");
      let itemId = this.getItemIdFromNode(node);
      this.list.setAttribute("aria-activedescendant", "item-" + itemId);
    } else {
      this.list.removeAttribute("aria-activedescendant");
    }

    let event = new CustomEvent("SelectedItemChanged", {bubbles: true});
    this.list.dispatchEvent(event);
  },

  /**
   * The index of the currently selected item in the list.
   * @type {number}
   */
  get selectedIndex() {
    for (let i = 0; i < this.numItems; i++) {
      let item = this.list.children.item(i);
      if (!item) {
        break;
      }
      if (item.classList.contains("selected")) {
        return i;
      }
    }
    return -1;
  },

  set selectedIndex(index) {
    log.debug(`Setting selectedIndex: ${index}`);

    if (index == -1) {
      this.selectedItem = null;
      return;
    }

    let item = this.list.children.item(index);
    if (!item) {
      log.warn(`Unable to set selectedIndex to invalid index ${index}`);
      return;
    }
    this.selectedItem = item;
  },

  /**
   * Open a given URL. The event is used to determine where it should be opened
   * (current tab, new tab, new window).
   * @param {string} url - URL to open.
   * @param {Event} event - KeyEvent or MouseEvent that triggered this action.
   */
  openURL(url, event) {
    // TODO: Disabled while working on the listbox mechanics.
    log.debug(`Opening page ${url}`);
    return;

    let mainWindow = window.QueryInterface(Ci.nsIInterfaceRequestor)
                           .getInterface(Ci.nsIWebNavigation)
                           .QueryInterface(Ci.nsIDocShellTreeItem)
                           .rootTreeItem
                           .QueryInterface(Ci.nsIInterfaceRequestor)
                           .getInterface(Ci.nsIDOMWindow);
    mainWindow.openUILink(url, event);
  },

  /**
   * Get the ID of the Item associated with a given list item element.
   * @param {element} node - List item element to get an ID for.
   * @return {string} Assocated Item ID.
   */
  getItemIdFromNode(node) {
    let id = node.getAttribute("id");
    if (id && id.startsWith("item-")) {
      return id.slice(5);
    }

    return null;
  },

  /**
   * Get the Item associated with a given list item element.
   * @param {element} node - List item element to get an Item for.
   * @return {string} Associated Item.
   */
  getItemFromNode(node) {
    let itemId = this.getItemIdFromNode(node);
    if (!itemId) {
      return null;
    }

    return this.itemsById.get(itemId);
  },

  /**
   * Open the active item in the list.
   * @param {Event} event - Event triggering this.
   */
  openActiveItem(event) {
    let itemNode = this.activeItem;
    if (!itemNode) {
      return;
    }

    let item = this.getItemFromNode(itemNode);
    this.openURL(item.url.spec, event);
  },

  /**
   * Find the parent item element, from a given child element.
   * @param {Element} node - Child element.
   * @return {Element} Element for the item, or null if not found.
   */
  findParentItemNode(node) {
    while (node && node != this.list && node != document.documentElement &&
           !node.classList.contains("item")) {
      node = node.parentNode;
    }

    if (node != this.list && node != document.documentElement) {
      return node;
    }

    return null;
  },

  /**
   * Handle a click event on the list box.
   * @param {Event} event - Triggering event.
   */
  onListClick(event) {
    let itemNode = this.findParentItemNode(event.target);
    if (!itemNode)
      return;

    this.activeItem = itemNode;
    this.openActiveItem(event);
  },

  /**
   * Handle a mousemove event over the list box.
   * @param {Event} event - Triggering event.
   */
  onListMouseMove(event) {
    let itemNode = this.findParentItemNode(event.target);
    if (!itemNode)
      return;

    this.selectedItem = itemNode;
  },

  /**
   * Handle a keydown event on the list box.
   * @param {Event} event - Triggering event.
   */
  onListKeyDown(event) {
    if (event.keyCode == KeyEvent.DOM_VK_DOWN) {
      // TODO: Refactor this so we pass a direction to a generic method.
      // See autocomplete.xml's getNextIndex
      event.preventDefault();
      let index = this.selectedIndex + 1;
      if (index >= this.numItems) {
        index = 0;
      }

      this.selectedIndex = index;
      this.selectedItem.focus();
    } else if (event.keyCode == KeyEvent.DOM_VK_UP) {
      event.preventDefault();

      let index = this.selectedIndex - 1;
      if (index < 0) {
        index = this.numItems - 1;
      }

      this.selectedIndex = index;
      this.selectedItem.focus();
    } else if (event.keyCode == KeyEvent.DOM_VK_RETURN) {
      let selectedItem = this.selectedItem;
      if (selectedItem) {
        this.activeItem = this.selectedItem;
        this.openActiveItem(event);
      }
    }
  },
};


addEventListener("DOMContentLoaded", () => RLSidebar.init());
