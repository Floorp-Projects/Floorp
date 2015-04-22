/*
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

XPCOMUtils.defineLazyModuleGetter(this, "ReadingList",
  "resource:///modules/readinglist/ReadingList.jsm");

const READINGLIST_COMMAND_ID = "readingListSidebar";

let ReadingListUI = {
  /**
   * Frame-script messages we want to listen to.
   * @type {[string]}
   */
  MESSAGES: [
    "ReadingList:GetVisibility",
    "ReadingList:ToggleVisibility",
    "ReadingList:ShowIntro",
  ],

  /**
   * Add-to-ReadingList toolbar button in the URLbar.
   * @type {Element}
   */
  toolbarButton: null,

  /**
   * Whether this object is currently registered as a listener with ReadingList.
   * Used to avoid inadvertantly loading the ReadLingList.jsm module on startup.
   * @type {Boolean}
   */
  listenerRegistered: false,

  /**
   * Initialize the ReadingList UI.
   */
  init() {
    this.toolbarButton = document.getElementById("readinglist-addremove-button");

    Preferences.observe("browser.readinglist.enabled", this.updateUI, this);

    const mm = window.messageManager;
    for (let msg of this.MESSAGES) {
      mm.addMessageListener(msg, this);
    }

    this.updateUI();
  },

  /**
   * Un-initialize the ReadingList UI.
   */
  uninit() {
    Preferences.ignore("browser.readinglist.enabled", this.updateUI, this);

    const mm = window.messageManager;
    for (let msg of this.MESSAGES) {
      mm.removeMessageListener(msg, this);
    }

    if (this.listenerRegistered) {
      ReadingList.removeListener(this);
      this.listenerRegistered = false;
    }
  },

  /**
   * Whether the ReadingList feature is enabled or not.
   * @type {boolean}
   */
  get enabled() {
    return Preferences.get("browser.readinglist.enabled", false);
  },

  /**
   * Whether the ReadingList sidebar is currently open or not.
   * @type {boolean}
   */
  get isSidebarOpen() {
    return SidebarUI.isOpen && SidebarUI.currentID == READINGLIST_COMMAND_ID;
  },

  /**
   * Update the UI status, ensuring the UI is shown or hidden depending on
   * whether the feature is enabled or not.
   */
  updateUI() {
    let enabled = this.enabled;
    if (enabled) {
      // This is a no-op if we're already registered.
      ReadingList.addListener(this);
      this.listenerRegistered = true;
    } else {
      if (this.listenerRegistered) {
        // This is safe to call if we're not currently registered, but we don't
        // want to forcibly load the normally lazy-loaded module on startup.
        ReadingList.removeListener(this);
        this.listenerRegistered = false;
      }

      this.hideSidebar();
    }

    document.getElementById(READINGLIST_COMMAND_ID).setAttribute("hidden", !enabled);
    document.getElementById(READINGLIST_COMMAND_ID).setAttribute("disabled", !enabled);
  },

  /**
   * Show the ReadingList sidebar.
   * @return {Promise}
   */
  showSidebar() {
    if (this.enabled) {
      return SidebarUI.show(READINGLIST_COMMAND_ID);
    }
  },

  /**
   * Hide the ReadingList sidebar, if it is currently shown.
   */
  hideSidebar() {
    if (this.isSidebarOpen) {
      SidebarUI.hide();
    }
  },

  /**
   * Re-refresh the ReadingList bookmarks submenu when it opens.
   *
   * @param {Element} target - Menu element opening.
   */
  onReadingListPopupShowing: Task.async(function* (target) {
    if (target.id == "BMB_readingListPopup") {
      // Setting this class in the .xul file messes with the way
      // browser-places.js inserts bookmarks in the menu.
      document.getElementById("BMB_viewReadingListSidebar")
              .classList.add("panel-subview-footer");
    }

    while (!target.firstChild.id)
      target.firstChild.remove();

    let classList = "menuitem-iconic bookmark-item menuitem-with-favicon";
    let insertPoint = target.firstChild;
    if (insertPoint.classList.contains("subviewbutton"))
      classList += " subviewbutton";

    let hasItems = false;
    yield ReadingList.forEachItem(item => {
      hasItems = true;

      let menuitem = document.createElement("menuitem");
      menuitem.setAttribute("label", item.title || item.url);
      menuitem.setAttribute("class", classList);

      let node = menuitem._placesNode = {
        // Passing the PlacesUtils.nodeIsURI check is required for the
        // onCommand handler to load our URI.
        type: Ci.nsINavHistoryResultNode.RESULT_TYPE_URI,

        // makes PlacesUIUtils.canUserRemove return false.
        // The context menu is broken without this.
        parent: {type: Ci.nsINavHistoryResultNode.RESULT_TYPE_FOLDER},

        // A -1 id makes this item a non-bookmark, which avoids calling
        // PlacesUtils.annotations.itemHasAnnotation to check if the
        // bookmark should be opened in the sidebar (this call fails for
        // readinglist item, and breaks loading our URI).
        itemId: -1,

        // Used by the tooltip and onCommand handlers.
        uri: item.url,

        // Used by the tooltip.
        title: item.title
      };

      Favicons.getFaviconURLForPage(item.uri, uri => {
        if (uri) {
          menuitem.setAttribute("image",
                                Favicons.getFaviconLinkForIcon(uri).spec);
        }
      });

      target.insertBefore(menuitem, insertPoint);
    }, {sort: "addedOn", descending: true});

    if (!hasItems) {
      let menuitem = document.createElement("menuitem");
      let bundle =
        Services.strings.createBundle("chrome://browser/locale/places/places.properties");
      menuitem.setAttribute("label", bundle.GetStringFromName("bookmarksMenuEmptyFolder"));
      menuitem.setAttribute("class", "bookmark-item");
      menuitem.setAttribute("disabled", true);
      target.insertBefore(menuitem, insertPoint);
    }
  }),

  /**
   * Hide the ReadingList sidebar, if it is currently shown.
   */
  toggleSidebar() {
    if (this.enabled) {
      SidebarUI.toggle(READINGLIST_COMMAND_ID);
    }
  },

  /**
   * Respond to messages.
   */
  receiveMessage(message) {
    switch (message.name) {
      case "ReadingList:GetVisibility": {
        if (message.target.messageManager) {
          message.target.messageManager.sendAsyncMessage("ReadingList:VisibilityStatus",
            { isOpen: this.isSidebarOpen });
        }
        break;
      }

      case "ReadingList:ToggleVisibility": {
        this.toggleSidebar();
        break;
      }

      case "ReadingList:ShowIntro": {
        if (this.enabled && !Preferences.get("browser.readinglist.introShown", false)) {
          Preferences.set("browser.readinglist.introShown", true);
          this.showSidebar();
        }
        break;
      }
    }
  },

  /**
   * Handles toolbar button styling based on page proxy state changes.
   *
   * @see SetPageProxyState()
   *
   * @param {string} state - New state. Either "valid" or "invalid".
   */
  onPageProxyStateChanged: Task.async(function* (state) {
    if (!this.toolbarButton) {
      // nothing to do if we have no button.
      return;
    }

    let uri;
    if (this.enabled && state == "valid") {
      uri = gBrowser.currentURI;
      if (uri.schemeIs("about"))
        uri = ReaderMode.getOriginalUrl(uri.spec);
      else if (!uri.schemeIs("http") && !uri.schemeIs("https"))
        uri = null;
    }

    let msg = {topic: "UpdateActiveItem", url: null};
    if (!uri) {
      this.toolbarButton.setAttribute("hidden", true);
      if (this.isSidebarOpen)
        document.getElementById("sidebar").contentWindow.postMessage(msg, "*");
      return;
    }

    let isInList = yield ReadingList.hasItemForURL(uri);

    if (window.closed) {
      // Skip updating the UI if the window was closed since our hasItemForURL call.
      return;
    }

    if (this.isSidebarOpen) {
      if (isInList)
        msg.url = typeof uri == "string" ? uri : uri.spec;
      document.getElementById("sidebar").contentWindow.postMessage(msg, "*");
    }
    this.setToolbarButtonState(isInList);
  }),

  /**
   * Set the state of the ReadingList toolbar button in the urlbar.
   * If the current tab's page is in the ReadingList (active), sets the button
   * to allow removing the page. Otherwise, sets the button to allow adding the
   * page (not active).
   *
   * @param {boolean} active - True if the button should be active (page is
   *                           already in the list).
   */
  setToolbarButtonState(active) {
    this.toolbarButton.setAttribute("already-added", active);

    let type = (active ? "remove" : "add");
    let tooltip = gNavigatorBundle.getString(`readingList.urlbar.${type}`);
    this.toolbarButton.setAttribute("tooltiptext", tooltip);

    this.toolbarButton.removeAttribute("hidden");
  },

  /**
   * Toggle a page (from a browser) in the ReadingList, adding if it's not already added, or
   * removing otherwise.
   *
   * @param {<xul:browser>} browser - Browser with page to toggle.
   * @returns {Promise} Promise resolved when operation has completed.
   */
  togglePageByBrowser: Task.async(function* (browser) {
    let uri = browser.currentURI;
    if (uri.spec.startsWith("about:reader?"))
      uri = ReaderMode.getOriginalUrl(uri.spec);
    if (!uri)
      return;

    let item = yield ReadingList.itemForURL(uri);
    if (item) {
      yield item.delete();
    } else {
      yield ReadingList.addItemFromBrowser(browser, uri);
    }
  }),

  /**
   * Checks if a given item matches the current tab in this window.
   *
   * @param {ReadingListItem} item - Item to check
   * @returns True if match, false otherwise.
   */
  isItemForCurrentBrowser(item) {
    let currentURL = gBrowser.currentURI.spec;
    if (currentURL.startsWith("about:reader?"))
      currentURL = ReaderMode.getOriginalUrl(currentURL);

    if (item.url == currentURL || item.resolvedURL == currentURL) {
      return true;
    }
    return false;
  },

  /**
   * ReadingList event handler for when an item is added.
   *
   * @param {ReadingListItem} item - Item added.
   */
  onItemAdded(item) {
    if (!Services.prefs.getBoolPref("browser.readinglist.sidebarEverOpened")) {
      SidebarUI.show("readingListSidebar");
    }
    if (this.isItemForCurrentBrowser(item)) {
      this.setToolbarButtonState(true);
      if (this.isSidebarOpen) {
        let msg = {topic: "UpdateActiveItem", url: item.url};
        document.getElementById("sidebar").contentWindow.postMessage(msg, "*");
      }
    }
  },

  /**
   * ReadingList event handler for when an item is deleted.
   *
   * @param {ReadingListItem} item - Item deleted.
   */
  onItemDeleted(item) {
    if (this.isItemForCurrentBrowser(item)) {
      this.setToolbarButtonState(false);
    }
  },
};
