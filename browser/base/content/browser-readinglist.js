/*
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

let ReadingListUI = {
  /**
   * Initialize the ReadingList UI.
   */
  init() {
    Preferences.observe("browser.readinglist.enabled", this.updateUI, this);
    this.updateUI();
  },

  /**
   * Un-initialize the ReadingList UI.
   */
  uninit() {
    Preferences.ignore("browser.readinglist.enabled", this.updateUI, this);
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
    return SidebarUI.isOpen && SidebarUI.currentID == "readingListSidebar";
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

    document.getElementById("readingListSidebar").setAttribute("hidden", !enabled);
  },

  /**
   * Show the ReadingList sidebar.
   * @return {Promise}
   */
  showSidebar() {
    return SidebarUI.show("readingListSidebar");
  },

  /**
   * Hide the ReadingList sidebar, if it is currently shown.
   */
  hideSidebar() {
    if (this.isSidebarOpen) {
      SidebarUI.hide();
    }
  },
};
