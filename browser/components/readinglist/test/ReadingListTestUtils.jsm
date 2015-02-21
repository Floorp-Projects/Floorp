"use strict";

this.EXPORTED_SYMBOLS = [
  "ReadingListTestUtils",
];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource:///modules/readinglist/ReadingList.jsm");


/** Preference name controlling whether the ReadingList feature is enabled/disabled. */
const PREF_RL_ENABLED = "browser.readinglist.enabled";


/**
 * Utilities for testing the ReadingList sidebar.
 */
function SidebarUtils(window, assert) {
  this.window = window;
  this.Assert = assert;
}

SidebarUtils.prototype = {
  /**
   * Reference to the RLSidebar object controlling the ReadingList sidebar UI.
   * @type {object}
   */
  get RLSidebar() {
    return this.window.SidebarUI.browser.contentWindow.RLSidebar;
  },

  /**
   * Reference to the list container element in the sidebar.
   * @type {Element}
   */
  get list() {
    return this.RLSidebar.list;
  },

  /**
   * Check that the number of elements in the list matches the expected count.
   * @param {number} count - Expected number of items.
   */
  expectNumItems(count) {
    this.Assert.equal(this.list.childElementCount, count,
                      "Should have expected number of items in the sidebar list");
  },

  /**
   * Check all items in the sidebar list, ensuring the DOM matches the data.
   */
  checkAllItems() {
    for (let itemNode of this.list.children) {
      this.checkSidebarItem(itemNode);
    }
  },

  /**
   * Run a series of sanity checks for an element in the list associated with
   * an Item, ensuring the DOM matches the data.
   */
  checkItem(node) {
    let item = this.RLSidebar.getItemFromNode(node);

    this.Assert.ok(node.classList.contains("item"),
                   "Node should have .item class");
    this.Assert.equal(node.id, "item-" + item.id,
                      "Node should have correct ID");
    this.Assert.equal(node.getAttribute("title"), item.title + "\n" + item.url.spec,
                      "Node should have correct title attribute");
    this.Assert.equal(node.querySelector(".item-title").textContent, item.title,
                      "Node's title element's text should match item title");
    this.Assert.equal(node.querySelector(".item-domain").textContent, item.domain,
                      "Node's domain element's text should match item title");
  },

  expectSelectedId(itemId) {
    let selectedItem = this.RLSidebar.selectedItem;
    if (itemId == null) {
      this.Assert.equal(selectedItem, null, "Should have no selected item");
    } else {
      this.Assert.notEqual(selectedItem, null, "selectedItem should not be null");
      let selectedId = this.RLSidebar.getItemIdFromNode(selectedItem);
      this.Assert.equal(itemId, selectedId, "Should have currect item selected");
    }
  },

  expectActiveId(itemId) {
    let activeItem = this.RLSidebar.activeItem;
    if (itemId == null) {
      this.Assert.equal(activeItem, null, "Should have no active item");
    } else {
      this.Assert.notEqual(activeItem, null, "activeItem should not be null");
      let activeId = this.RLSidebar.getItemIdFromNode(activeItem);
      this.Assert.equal(itemId, activeId, "Should have correct item active");
    }
  },
};


/**
 * Utilities for testing the ReadingList.
 */
this.ReadingListTestUtils = {
  /**
   * Whether the ReadingList feature is enabled or not.
   * @type {boolean}
   */
  get enabled() {
    return Preferences.get(PREF_RL_ENABLED, false);
  },
  set enabled(value) {
    Preferences.set(PREF_RL_ENABLED, !!value);
  },

  /**
   * Utilities for testing the ReadingList sidebar.
   */
  SidebarUtils: SidebarUtils,

  /**
   * Synthetically add an item to the ReadingList.
   * @param {object|[object]} data - Object or array of objects to pass to the
   *                                 Item constructor.
   * @return {Promise} Promise that gets fulfilled with the item or items added.
   */
  addItem(data) {
    if (Array.isArray(data)) {
      let promises = [];
      for (let itemData of data) {
        promises.push(this.addItem(itemData));
      }
      return Promise.all(promises);
    }

    return new Promise(resolve => {
      let item = new ReadingList.Item(data);
      ReadingList._items.push(item);
      ReadingList._notifyListeners("onItemAdded", item);
      resolve(item);
    });
  },

  /**
   * Cleanup all data, resetting to a blank state.
   */
  cleanup() {
    return new Promise(resolve => {
      ReadingList._items = [];
      ReadingList._listeners.clear();
      Preferences.reset(PREF_RL_ENABLED);
      resolve();
    });
  },
};
