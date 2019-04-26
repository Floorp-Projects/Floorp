/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * SidebarUI controls showing and hiding the browser sidebar.
 */
var SidebarUI = {
  get sidebars() {
    if (this._sidebars) {
      return this._sidebars;
    }
    return this._sidebars = new Map([
      ["viewBookmarksSidebar", {
        title: document.getElementById("sidebar-switcher-bookmarks")
                       .getAttribute("label"),
        url: "chrome://browser/content/places/bookmarksSidebar.xul",
        menuId: "menu_bookmarksSidebar",
        buttonId: "sidebar-switcher-bookmarks",
      }],
      ["viewHistorySidebar", {
        title: document.getElementById("sidebar-switcher-history")
                       .getAttribute("label"),
        url: "chrome://browser/content/places/historySidebar.xul",
        menuId: "menu_historySidebar",
        buttonId: "sidebar-switcher-history",
        triggerButtonId: "appMenuViewHistorySidebar",
      }],
      ["viewTabsSidebar", {
        title: document.getElementById("sidebar-switcher-tabs")
                       .getAttribute("label"),
        url: "chrome://browser/content/syncedtabs/sidebar.xhtml",
        menuId: "menu_tabsSidebar",
        buttonId: "sidebar-switcher-tabs",
        triggerButtonId: "PanelUI-remotetabs-view-sidebar",
      }],
    ]);
  },

  // Avoid getting the browser element from init() to avoid triggering the
  // <browser> constructor during startup if the sidebar is hidden.
  get browser() {
    if (this._browser)
      return this._browser;
    return this._browser = document.getElementById("sidebar");
  },
  POSITION_START_PREF: "sidebar.position_start",
  DEFAULT_SIDEBAR_ID: "viewBookmarksSidebar",

  // lastOpenedId is set in show() but unlike currentID it's not cleared out on hide
  // and isn't persisted across windows
  lastOpenedId: null,

  _box: null,
  // The constructor of this label accesses the browser element due to the
  // control="sidebar" attribute, so avoid getting this label during startup.
  get _title() {
    if (this.__title)
      return this.__title;
    return this.__title = document.getElementById("sidebar-title");
  },
  _splitter: null,
  _icon: null,
  _reversePositionButton: null,
  _switcherPanel: null,
  _switcherTarget: null,
  _switcherArrow: null,
  _inited: false,

  _initDeferred: PromiseUtils.defer(),

  get promiseInitialized() {
    return this._initDeferred.promise;
  },

  get initialized() {
    return this._inited;
  },

  init() {
    this._box = document.getElementById("sidebar-box");
    this._splitter = document.getElementById("sidebar-splitter");
    this._icon = document.getElementById("sidebar-icon");
    this._reversePositionButton = document.getElementById("sidebar-reverse-position");
    this._switcherPanel = document.getElementById("sidebarMenu-popup");
    this._switcherTarget = document.getElementById("sidebar-switcher-target");
    this._switcherArrow = document.getElementById("sidebar-switcher-arrow");

    this._switcherTarget.addEventListener("command", () => {
      this.toggleSwitcherPanel();
    });

    this._inited = true;

    this._initDeferred.resolve();
  },

  uninit() {
    // If this is the last browser window, persist various values that should be
    // remembered for after a restart / reopening a browser window.
    let enumerator = Services.wm.getEnumerator("navigator:browser");
    if (!enumerator.hasMoreElements()) {
      let xulStore = Services.xulStore;
      xulStore.persist(this._box, "sidebarcommand");

      if (this._box.hasAttribute("positionend")) {
        xulStore.persist(this._box, "positionend");
      } else {
        xulStore.removeValue(document.documentURI, "sidebar-box", "positionend");
      }
      if (this._box.hasAttribute("checked")) {
        xulStore.persist(this._box, "checked");
      } else {
        xulStore.removeValue(document.documentURI, "sidebar-box", "checked");
      }

      xulStore.persist(this._box, "width");
      xulStore.persist(this._title, "value");
    }
  },

  /**
   * Opens the switcher panel if it's closed, or closes it if it's open.
   */
  toggleSwitcherPanel() {
    if (this._switcherPanel.state == "open" || this._switcherPanel.state == "showing") {
      this.hideSwitcherPanel();
    } else if (this._switcherPanel.state == "closed") {
      this.showSwitcherPanel();
    }
  },

  hideSwitcherPanel() {
    this._switcherPanel.hidePopup();
  },

  showSwitcherPanel() {
    this._ensureShortcutsShown();
    this._switcherPanel.addEventListener("popuphiding", () => {
      this._switcherTarget.classList.remove("active");
    }, {once: true});

    // Combine start/end position with ltr/rtl to set the label in the popup appropriately.
    let label = this._positionStart == RTL_UI ?
                  gNavigatorBundle.getString("sidebar.moveToLeft") :
                  gNavigatorBundle.getString("sidebar.moveToRight");
    this._reversePositionButton.setAttribute("label", label);

    this._switcherPanel.hidden = false;
    this._switcherPanel.openPopup(this._icon);
    this._switcherTarget.classList.add("active");
  },

  updateShortcut({button, key}) {
    // If the shortcuts haven't been rendered yet then it will be set correctly
    // on the first render so there's nothing to do now.
    if (!this._addedShortcuts) {
      return;
    }
    if (key) {
      let keyId = key.getAttribute("id");
      button = this._switcherPanel.querySelector(`[key="${keyId}"]`);
    } else if (button) {
      let keyId = button.getAttribute("key");
      key = document.getElementById(keyId);
    }
    if (!button || !key) {
      return;
    }
    button.setAttribute("shortcut", ShortcutUtils.prettifyShortcut(key));
  },

  _addedShortcuts: false,
  _ensureShortcutsShown() {
    if (this._addedShortcuts) {
      return;
    }
    this._addedShortcuts = true;
    for (let button of this._switcherPanel.querySelectorAll("toolbarbutton[key]")) {
      this.updateShortcut({button});
    }
  },

  /**
   * Change the pref that will trigger a call to setPosition
   */
  reversePosition() {
    Services.prefs.setBoolPref(this.POSITION_START_PREF, !this._positionStart);
  },

  /**
   * Read the positioning pref and position the sidebar and the splitter
   * appropriately within the browser container.
   */
  setPosition() {
    // First reset all ordinals to match DOM ordering.
    let browser = document.getElementById("browser");
    [...browser.children].forEach((node, i) => {
      node.ordinal = i + 1;
    });

    if (!this._positionStart) {
      // DOM ordering is:     |  sidebar-box  | splitter |   appcontent  |
      // Want to display as:  |   appcontent  | splitter |  sidebar-box  |
      // So we just swap box and appcontent ordering
      let appcontent = document.getElementById("appcontent");
      let boxOrdinal = this._box.ordinal;
      this._box.ordinal = appcontent.ordinal;
      appcontent.ordinal = boxOrdinal;
      // Indicate we've switched ordering to the box
      this._box.setAttribute("positionend", true);
    } else {
      this._box.removeAttribute("positionend");
    }

    this.hideSwitcherPanel();

    let content = SidebarUI.browser.contentWindow;
    if (content && content.updatePosition) {
      content.updatePosition();
    }
  },

  /**
   * Try and adopt the status of the sidebar from another window.
   * @param {Window} sourceWindow - Window to use as a source for sidebar status.
   * @return true if we adopted the state, or false if the caller should
   * initialize the state itself.
   */
  adoptFromWindow(sourceWindow) {
    // If the opener had a sidebar, open the same sidebar in our window.
    // The opener can be the hidden window too, if we're coming from the state
    // where no windows are open, and the hidden window has no sidebar box.
    let sourceUI = sourceWindow.SidebarUI;
    if (!sourceUI || !sourceUI._box) {
      // no source UI or no _box means we also can't adopt the state.
      return false;
    }

    // Set sidebar command even if hidden, so that we keep the same sidebar
    // even if it's currently closed.
    let commandID = sourceUI._box.getAttribute("sidebarcommand");
    if (commandID) {
      this._box.setAttribute("sidebarcommand", commandID);
    }

    if (sourceUI._box.hidden) {
      // just hidden means we have adopted the hidden state.
      return true;
    }

    // dynamically generated sidebars will fail this check, but we still
    // consider it adopted.
    if (!this.sidebars.has(commandID)) {
      return true;
    }

    this._box.setAttribute("width", sourceUI._box.getBoundingClientRect().width);
    this.showInitially(commandID);

    return true;
  },

  windowPrivacyMatches(w1, w2) {
    return PrivateBrowsingUtils.isWindowPrivate(w1) === PrivateBrowsingUtils.isWindowPrivate(w2);
  },

  /**
   * If loading a sidebar was delayed on startup, start the load now.
   */
  startDelayedLoad() {
    let sourceWindow = window.opener;
    // No source window means this is the initial window.  If we're being
    // opened from another window, check that it is one we might open a sidebar
    // for.
    if (sourceWindow) {
      if (sourceWindow.closed || sourceWindow.location.protocol != "chrome:" ||
        !this.windowPrivacyMatches(sourceWindow, window)) {
        return;
      }
      // Try to adopt the sidebar state from the source window
      if (this.adoptFromWindow(sourceWindow)) {
        return;
      }
    }

    // If we're not adopting settings from a parent window, set them now.
    let wasOpen = this._box.getAttribute("checked");
    if (!wasOpen) {
      return;
    }

    let commandID = this._box.getAttribute("sidebarcommand");
    if (commandID && this.sidebars.has(commandID)) {
      this.showInitially(commandID);
    } else {
      this._box.removeAttribute("checked");
      // Remove the |sidebarcommand| attribute, because the element it
      // refers to no longer exists, so we should assume this sidebar
      // panel has been uninstalled. (249883)
      // We use setAttribute rather than removeAttribute so it persists
      // correctly.
      this._box.setAttribute("sidebarcommand", "");
      // On a startup in which the startup cache was invalidated (e.g. app update)
      // extensions will not be started prior to delayedLoad, thus the
      // sidebarcommand element will not exist yet.  Store the commandID so
      // extensions may reopen if necessary.  A startup cache invalidation
      // can be forced (for testing) by deleting compatibility.ini from the
      // profile.
      this.lastOpenedId = commandID;
    }
  },

  /**
   * Fire a "SidebarShown" event on the sidebar to give any interested parties
   * a chance to update the button or whatever.
   */
  _fireShowEvent() {
    let event = new CustomEvent("SidebarShown", {bubbles: true});
    this._switcherTarget.dispatchEvent(event);
  },

  /**
   * Fire a "SidebarFocused" event on the sidebar's |window| to give the sidebar
   * a chance to adjust focus as needed. An additional event is needed, because
   * we don't want to focus the sidebar when it's opened on startup or in a new
   * window, only when the user opens the sidebar.
   */
  _fireFocusedEvent() {
    let event = new CustomEvent("SidebarFocused", {bubbles: true});
    this.browser.contentWindow.dispatchEvent(event);
  },

  /**
   * True if the sidebar is currently open.
   */
  get isOpen() {
    return !this._box.hidden;
  },

  /**
   * The ID of the current sidebar.
   */
  get currentID() {
    return this.isOpen ? this._box.getAttribute("sidebarcommand") : "";
  },

  get title() {
    return this._title.value;
  },

  set title(value) {
    this._title.value = value;
  },

  /**
   * Toggle the visibility of the sidebar. If the sidebar is hidden or is open
   * with a different commandID, then the sidebar will be opened using the
   * specified commandID. Otherwise the sidebar will be hidden.
   *
   * @param  {string}  commandID     ID of the sidebar.
   * @param  {DOMNode} [triggerNode] Node, usually a button, that triggered the
   *                                 visibility toggling of the sidebar.
   * @return {Promise}
   */
  toggle(commandID = this.lastOpenedId, triggerNode) {
    // First priority for a default value is this.lastOpenedId which is set during show()
    // and not reset in hide(), unlike currentID. If show() hasn't been called and we don't
    // have a persisted command either, or the command doesn't exist anymore, then
    // fallback to a default sidebar.
    if (!commandID) {
      commandID = this._box.getAttribute("sidebarcommand");
    }
    if (!commandID || !this.sidebars.has(commandID)) {
      commandID = this.DEFAULT_SIDEBAR_ID;
    }

    if (this.isOpen && commandID == this.currentID) {
      this.hide(triggerNode);
      return Promise.resolve();
    }
    return this.show(commandID, triggerNode);
  },

  _loadSidebarExtension(commandID) {
    let sidebar = this.sidebars.get(commandID);
    let {extensionId} = sidebar;
    if (extensionId) {
      SidebarUI.browser.contentWindow.loadPanel(extensionId, sidebar.panel,
                                                sidebar.browserStyle);
    }
  },

  /**
   * Show the sidebar.
   *
   * This wraps the internal method, including a ping to telemetry.
   *
   * @param {string}  commandID     ID of the sidebar to use.
   * @param {DOMNode} [triggerNode] Node, usually a button, that triggered the
   *                                showing of the sidebar.
   * @return {Promise<boolean>}
   */
  async show(commandID, triggerNode) {
    // Extensions without private window access wont be in the
    // sidebars map.
    if (!this.sidebars.has(commandID)) {
      return false;
    }
    return this._show(commandID).then(() => {
      this._loadSidebarExtension(commandID);

      if (triggerNode) {
        updateToggleControlLabel(triggerNode);
      }

      this._fireFocusedEvent();
      return true;
    });
  },

  /**
   * Show the sidebar, without firing the focused event or logging telemetry.
   * This is intended to be used when the sidebar is opened automatically
   * when a window opens (not triggered by user interaction).
   *
   * @param {string} commandID ID of the sidebar.
   * @return {Promise<boolean>}
   */
  async showInitially(commandID) {
    // Extensions without private window access wont be in the
    // sidebars map.
    if (!this.sidebars.has(commandID)) {
      return false;
    }
    return this._show(commandID).then(() => {
      this._loadSidebarExtension(commandID);
      return true;
    });
  },

  /**
   * Implementation for show. Also used internally for sidebars that are shown
   * when a window is opened and we don't want to ping telemetry.
   *
   * @param {string} commandID ID of the sidebar.
   * @return {Promise<void>}
   */
  _show(commandID) {
    return new Promise(resolve => {
      this.selectMenuItem(commandID);

      this._box.hidden = this._splitter.hidden = false;
      this.setPosition();

      this.hideSwitcherPanel();

      this._box.setAttribute("checked", "true");
      this._box.setAttribute("sidebarcommand", commandID);
      this.lastOpenedId = commandID;

      let {url, title} = this.sidebars.get(commandID);
      this.title = title;
      this.browser.setAttribute("src", url); // kick off async load

      if (this.browser.contentDocument.location.href != url) {
        this.browser.addEventListener("load", event => {
          // We're handling the 'load' event before it bubbles up to the usual
          // (non-capturing) event handlers. Let it bubble up before resolving.
          setTimeout(() => {
            resolve();

            // Now that the currentId is updated, fire a show event.
            this._fireShowEvent();
          }, 0);
        }, {capture: true, once: true});
      } else {
        resolve();

        // Now that the currentId is updated, fire a show event.
        this._fireShowEvent();
      }

      let selBrowser = gBrowser.selectedBrowser;
      selBrowser.messageManager.sendAsyncMessage("Sidebar:VisibilityChange",
        {commandID, isOpen: true}
      );
    });
  },

  /**
   * Hide the sidebar.
   *
   * @param {DOMNode} [triggerNode] Node, usually a button, that triggered the
   *                                hiding of the sidebar.
   */
  hide(triggerNode) {
    if (!this.isOpen) {
      return;
    }

    this.hideSwitcherPanel();

    let commandID = this._box.getAttribute("sidebarcommand");
    this.selectMenuItem("");

    // Replace the document currently displayed in the sidebar with about:blank
    // so that we can free memory by unloading the page. We need to explicitly
    // create a new content viewer because the old one doesn't get destroyed
    // until about:blank has loaded (which does not happen as long as the
    // element is hidden).
    this.browser.setAttribute("src", "about:blank");
    this.browser.docShell.createAboutBlankContentViewer(null);

    this._box.removeAttribute("checked");
    this._box.hidden = this._splitter.hidden = true;

    let selBrowser = gBrowser.selectedBrowser;
    selBrowser.focus();
    selBrowser.messageManager.sendAsyncMessage("Sidebar:VisibilityChange",
      {commandID, isOpen: false}
    );
    if (triggerNode) {
      updateToggleControlLabel(triggerNode);
    }
  },

  /**
   * Sets the checked state only on the menu items of the specified sidebar, or
   * none if the argument is an empty string.
   */
  selectMenuItem(commandID) {
    for (let [id, {menuId, buttonId, triggerButtonId}] of this.sidebars) {
      let menu = document.getElementById(menuId);
      let button = document.getElementById(buttonId);
      let triggerbutton = triggerButtonId && document.getElementById(triggerButtonId);
      if (id == commandID) {
        menu.setAttribute("checked", "true");
        button.setAttribute("checked", "true");
        if (triggerbutton) {
          triggerbutton.setAttribute("checked", "true");
          updateToggleControlLabel(triggerbutton);
        }
      } else {
        menu.removeAttribute("checked");
        button.removeAttribute("checked");
        if (triggerbutton) {
          triggerbutton.removeAttribute("checked");
          updateToggleControlLabel(triggerbutton);
        }
      }
    }
  },
};

// Add getters related to the position here, since we will want them
// available for both startDelayedLoad and init.
XPCOMUtils.defineLazyPreferenceGetter(SidebarUI, "_positionStart",
  SidebarUI.POSITION_START_PREF, true, SidebarUI.setPosition.bind(SidebarUI));
