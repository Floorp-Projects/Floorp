/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * SidebarUI controls showing and hiding the browser sidebar.
 *
 * @note
 * Some of these methods take a commandID argument - we expect to find a
 * xul:broadcaster element with the specified ID.
 * The following attributes on that element may be used and/or modified:
 *  - id           (required) the string to match commandID. The convention
 *                 is to use this naming scheme: 'view<sidebar-name>Sidebar'.
 *  - sidebarurl   (required) specifies the URL to load in this sidebar.
 *  - sidebartitle or label (in that order) specify the title to
 *                 display on the sidebar.
 *  - checked      indicates whether the sidebar is currently displayed.
 *                 Note that toggleSidebar updates this attribute when
 *                 it changes the sidebar's visibility.
 *  - group        this attribute must be set to "sidebar".
 */
var SidebarUI = {
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
  },

  uninit() {
    let enumerator = Services.wm.getEnumerator(null);
    enumerator.getNext();
    if (!enumerator.hasMoreElements()) {
      document.persist("sidebar-box", "sidebarcommand");
      document.persist("sidebar-box", "checked");
      document.persist("sidebar-box", "width");
      document.persist("sidebar-title", "value");
    }
  },

  /**
   * Opens the switcher panel if it's closed, or closes it if it's open.
   */
  toggleSwitcherPanel() {
    if (this._switcherPanel.state == "open" || this._switcherPanel.state == "showing") {
      this.hideSwitcherPanel();
    } else {
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
    let label = this._positionStart == this.isRTL ?
                  gNavigatorBundle.getString("sidebar.moveToLeft") :
                  gNavigatorBundle.getString("sidebar.moveToRight");
    this._reversePositionButton.setAttribute("label", label);

    this._switcherPanel.hidden = false;
    this._switcherPanel.openPopup(this._icon);
    this._switcherTarget.classList.add("active");
  },

  _addedShortcuts: false,
  _ensureShortcutsShown() {
    if (this._addedShortcuts) {
      return;
    }
    this._addedShortcuts = true;
    for (let button of this._switcherPanel.querySelectorAll("toolbarbutton[key]")) {
      let keyId = button.getAttribute("key");
      let key = document.getElementById(keyId);
      if (!key) {
        continue;
      }
      button.setAttribute("shortcut", ShortcutUtils.prettifyShortcut(key));
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
    [...browser.childNodes].forEach((node, i) => {
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
    }

    this.hideSwitcherPanel();
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
    if (sourceUI._box.hidden) {
      // just hidden means we have adopted the hidden state.
      return true;
    }

    let commandID = sourceUI._box.getAttribute("sidebarcommand");

    // dynamically generated sidebars will fail this check, but we still
    // consider it adopted.
    if (!document.getElementById(commandID)) {
      return true;
    }

    this._box.setAttribute("width", sourceUI._box.boxObject.width);
    this._show(commandID);

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
    let commandID = this._box.getAttribute("sidebarcommand");
    if (!commandID) {
      return;
    }

    if (document.getElementById(commandID)) {
      this._show(commandID);
    } else {
      // Remove the |sidebarcommand| attribute, because the element it
      // refers to no longer exists, so we should assume this sidebar
      // panel has been uninstalled. (249883)
      this._box.removeAttribute("sidebarcommand");
    }
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

    // Run the original function for backwards compatibility.
    fireSidebarFocusedEvent();
  },

  /**
   * True if the sidebar is currently open.
   */
  get isOpen() {
    return !this._box.hidden;
  },

  /**
   * The ID of the current sidebar (ie, the ID of the broadcaster being used).
   * This can be set even if the sidebar is hidden.
   */
  get currentID() {
    return this._box.getAttribute("sidebarcommand");
  },

  get title() {
    return this._title.value;
  },

  set title(value) {
    this._title.value = value;
  },

  getBroadcasterById(id) {
    let sidebarBroadcaster = document.getElementById(id);
    if (sidebarBroadcaster && sidebarBroadcaster.localName == "broadcaster") {
      return sidebarBroadcaster;
    }
    return null;
  },

  /**
   * Toggle the visibility of the sidebar. If the sidebar is hidden or is open
   * with a different commandID, then the sidebar will be opened using the
   * specified commandID. Otherwise the sidebar will be hidden.
   *
   * @param {string} commandID ID of the xul:broadcaster element to use.
   * @return {Promise}
   */
  toggle(commandID = this.lastOpenedId) {
    // First priority for a default value is this.lastOpenedId which is set during show()
    // and not reset in hide(), unlike currentID. If show() hasn't been called or the command
    // doesn't exist anymore, then fallback to a default sidebar.
    if (!commandID || !this.getBroadcasterById(commandID)) {
      commandID = this.DEFAULT_SIDEBAR_ID;
    }

    if (this.isOpen && commandID == this.currentID) {
      this.hide();
      return Promise.resolve();
    }
    return this.show(commandID);
  },

  /**
   * Show the sidebar, using the parameters from the specified broadcaster.
   * @see SidebarUI note.
   *
   * This wraps the internal method, including a ping to telemetry.
   *
   * @param {string} commandID ID of the xul:broadcaster element to use.
   */
  show(commandID) {
    return this._show(commandID).then(() => {
      BrowserUITelemetry.countSidebarEvent(commandID, "show");
    });
  },

  /**
   * Implementation for show. Also used internally for sidebars that are shown
   * when a window is opened and we don't want to ping telemetry.
   *
   * @param {string} commandID ID of the xul:broadcaster element to use.
   */
  _show(commandID) {
    return new Promise((resolve, reject) => {
      let sidebarBroadcaster = this.getBroadcasterById(commandID);
      if (!sidebarBroadcaster) {
        reject(new Error("Invalid sidebar broadcaster specified: " + commandID));
        return;
      }

      if (this.isOpen && commandID != this.currentID) {
        BrowserUITelemetry.countSidebarEvent(this.currentID, "hide");
      }

      let broadcasters = document.querySelectorAll("broadcaster[group=sidebar]");
      for (let broadcaster of broadcasters) {
        if (broadcaster != sidebarBroadcaster) {
          broadcaster.removeAttribute("checked");
        } else {
          sidebarBroadcaster.setAttribute("checked", "true");
        }
      }

      this._box.hidden = this._splitter.hidden = false;
      this.setPosition();

      this.hideSwitcherPanel();

      this._box.setAttribute("checked", "true");
      this._box.setAttribute("sidebarcommand", sidebarBroadcaster.id);
      this.lastOpenedId = sidebarBroadcaster.id;

      let title = sidebarBroadcaster.getAttribute("sidebartitle") ||
                  sidebarBroadcaster.getAttribute("label");

      // When loading a web page in the sidebar there is no title set on the
      // broadcaster, as it is instead set by openWebPanel. Don't clear out
      // the title in this case.
      if (title) {
        this.title = title;
      }

      let url = sidebarBroadcaster.getAttribute("sidebarurl");
      this.browser.setAttribute("src", url); // kick off async load

      if (this.browser.contentDocument.location.href != url) {
        this.browser.addEventListener("load", event => {

          // We're handling the 'load' event before it bubbles up to the usual
          // (non-capturing) event handlers. Let it bubble up before firing the
          // SidebarFocused event.
          setTimeout(() => this._fireFocusedEvent(), 0);

          // Run the original function for backwards compatibility.
          sidebarOnLoad(event);

          resolve();
        }, {capture: true, once: true});
      } else {
        // Older code handled this case, so we do it too.
        this._fireFocusedEvent();
        resolve();
      }

      let selBrowser = gBrowser.selectedBrowser;
      selBrowser.messageManager.sendAsyncMessage("Sidebar:VisibilityChange",
        {commandID, isOpen: true}
      );
    });
  },

  /**
   * Hide the sidebar.
   */
  hide() {
    if (!this.isOpen) {
      return;
    }

    this.hideSwitcherPanel();

    let commandID = this._box.getAttribute("sidebarcommand");
    let sidebarBroadcaster = document.getElementById(commandID);

    if (sidebarBroadcaster.getAttribute("checked") != "true") {
      return;
    }

    // Replace the document currently displayed in the sidebar with about:blank
    // so that we can free memory by unloading the page. We need to explicitly
    // create a new content viewer because the old one doesn't get destroyed
    // until about:blank has loaded (which does not happen as long as the
    // element is hidden).
    this.browser.setAttribute("src", "about:blank");
    this.browser.docShell.createAboutBlankContentViewer(null);

    sidebarBroadcaster.removeAttribute("checked");
    this._box.setAttribute("sidebarcommand", "");
    this._box.removeAttribute("checked");
    this.title = "";
    this._box.hidden = this._splitter.hidden = true;

    let selBrowser = gBrowser.selectedBrowser;
    selBrowser.focus();
    selBrowser.messageManager.sendAsyncMessage("Sidebar:VisibilityChange",
      {commandID, isOpen: false}
    );
    BrowserUITelemetry.countSidebarEvent(commandID, "hide");
  },
};

// Add getters related to the position here, since we will want them
// available for both startDelayedLoad and init.
XPCOMUtils.defineLazyPreferenceGetter(SidebarUI, "_positionStart",
  SidebarUI.POSITION_START_PREF, true, SidebarUI.setPosition.bind(SidebarUI));
XPCOMUtils.defineLazyGetter(SidebarUI, "isRTL", () => {
  return getComputedStyle(document.documentElement).direction == "rtl";
});

/**
 * This exists for backwards compatibility - it will be called once a sidebar is
 * ready, following any request to show it.
 *
 * @deprecated
 */
function fireSidebarFocusedEvent() {}

/**
 * This exists for backwards compatibility - it gets called when a sidebar has
 * been loaded.
 *
 * @deprecated
 */
function sidebarOnLoad(event) {}

/**
 * This exists for backwards compatibility, and is equivilent to
 * SidebarUI.toggle() without the forceOpen param. With forceOpen set to true,
 * it is equivalent to SidebarUI.show().
 *
 * @deprecated
 */
function toggleSidebar(commandID, forceOpen = false) {
  Deprecated.warning("toggleSidebar() is deprecated, please use SidebarUI.toggle() or SidebarUI.show() instead",
                     "https://developer.mozilla.org/en-US/Add-ons/Code_snippets/Sidebar");

  if (forceOpen) {
    SidebarUI.show(commandID);
  } else {
    SidebarUI.toggle(commandID);
  }
}
