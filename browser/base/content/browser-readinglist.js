/*
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

XPCOMUtils.defineLazyModuleGetter(this, "ReadingList",
  "resource:///modules/readinglist/ReadingList.jsm");

const READINGLIST_COMMAND_ID = "readingListSidebar";

let ReadingListUI = {
  MESSAGES: [
    "ReadingList:GetVisibility",
    "ReadingList:ToggleVisibility",
  ],

  /**
   * Initialize the ReadingList UI.
   */
  init() {
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
    if (!enabled) {
      this.hideSidebar();
    }

    document.getElementById(READINGLIST_COMMAND_ID).setAttribute("hidden", !enabled);
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

  onReadingListPopupShowing(target) {
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

    ReadingList.getItems().then(items => {
      for (let item of items) {
        let menuitem = document.createElement("menuitem");
        menuitem.setAttribute("label", item.title || item.url.spec);
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
          uri: item.url.spec,

          // Used by the tooltip.
          title: item.title
        };

        Favicons.getFaviconURLForPage(item.url, uri => {
          if (uri) {
            menuitem.setAttribute("image",
                                  Favicons.getFaviconLinkForIcon(uri).spec);
          }
        });

        target.insertBefore(menuitem, insertPoint);
      }

      if (!items.length) {
        let menuitem = document.createElement("menuitem");
        let bundle =
          Services.strings.createBundle("chrome://browser/locale/places/places.properties");
        menuitem.setAttribute("label", bundle.GetStringFromName("bookmarksMenuEmptyFolder"));
        menuitem.setAttribute("class", "bookmark-item");
        menuitem.setAttribute("disabled", true);
        target.insertBefore(menuitem, insertPoint);
      }
    });
  },

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
    }
  },
};
