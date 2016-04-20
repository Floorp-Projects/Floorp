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
  browser: null,

  _box: null,
  _title: null,
  _splitter: null,

  init() {
    this._box = document.getElementById("sidebar-box");
    this.browser = document.getElementById("sidebar");
    this._title = document.getElementById("sidebar-title");
    this._splitter = document.getElementById("sidebar-splitter");

    if (!this.adoptFromWindow(window.opener)) {
      let commandID = this._box.getAttribute("sidebarcommand");
      if (commandID) {
        let command = document.getElementById(commandID);
        if (command) {
          this._delayedLoad = true;
          this._box.hidden = false;
          this._splitter.hidden = false;
          command.setAttribute("checked", "true");
        } else {
          // Remove the |sidebarcommand| attribute, because the element it
          // refers to no longer exists, so we should assume this sidebar
          // panel has been uninstalled. (249883)
          this._box.removeAttribute("sidebarcommand");
        }
      }
    }
  },

  uninit() {
    let enumerator = Services.wm.getEnumerator(null);
    enumerator.getNext();
    if (!enumerator.hasMoreElements()) {
      document.persist("sidebar-box", "sidebarcommand");
      document.persist("sidebar-box", "width");
      document.persist("sidebar-box", "src");
      document.persist("sidebar-title", "value");
    }
  },

  /**
   * Try and adopt the status of the sidebar from another window.
   * @param {Window} sourceWindow - Window to use as a source for sidebar status.
   * @return true if we adopted the state, or false if the caller should
   * initialize the state itself.
   */
  adoptFromWindow(sourceWindow) {
    // No source window, or it being closed, or not chrome, or in a different
    // private-browsing context means we can't adopt.
    if (!sourceWindow || sourceWindow.closed ||
        !sourceWindow.document.documentURIObject.schemeIs("chrome") ||
        PrivateBrowsingUtils.isWindowPrivate(window) != PrivateBrowsingUtils.isWindowPrivate(sourceWindow)) {
      return false;
    }

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
    let commandElem = document.getElementById(commandID);

    // dynamically generated sidebars will fail this check, but we still
    // consider it adopted.
    if (!commandElem) {
      return true;
    }

    this._title.setAttribute("value",
                             sourceUI._title.getAttribute("value"));
    this._box.setAttribute("width", sourceUI._box.boxObject.width);

    this._box.setAttribute("sidebarcommand", commandID);
    // Note: we're setting 'src' on this._box, which is a <vbox>, not on
    // the <browser id="sidebar">. This lets us delay the actual load until
    // delayedStartup().
    this._box.setAttribute("src", sourceUI.browser.getAttribute("src"));
    this._delayedLoad = true;

    this._box.hidden = false;
    this._splitter.hidden = false;
    commandElem.setAttribute("checked", "true");
    return true;
  },

  /**
   * If loading a sidebar was delayed on startup, start the load now.
   */
  startDelayedLoad() {
    if (!this._delayedLoad) {
      return;
    }

    this.browser.setAttribute("src", this._box.getAttribute("src"));
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

  /**
   * Toggle the visibility of the sidebar. If the sidebar is hidden or is open
   * with a different commandID, then the sidebar will be opened using the
   * specified commandID. Otherwise the sidebar will be hidden.
   *
   * @param {string} commandID ID of the xul:broadcaster element to use.
   * @return {Promise}
   */
  toggle(commandID = this.currentID) {
    if (this.isOpen && commandID == this.currentID) {
      this.hide();
      return Promise.resolve();
    } else {
      return this.show(commandID);
    }
  },

  /**
   * Show the sidebar, using the parameters from the specified broadcaster.
   * @see SidebarUI note.
   *
   * @param {string} commandID ID of the xul:broadcaster element to use.
   */
  show(commandID) {
    return new Promise((resolve, reject) => {
      let sidebarBroadcaster = document.getElementById(commandID);
      if (!sidebarBroadcaster || sidebarBroadcaster.localName != "broadcaster") {
        reject(new Error("Invalid sidebar broadcaster specified: " + commandID));
        return;
      }

      if (this.isOpen && commandID != this.currentID) {
        BrowserUITelemetry.countSidebarEvent(this.currentID, "hide");
      }

      let broadcasters = document.getElementsByAttribute("group", "sidebar");
      for (let broadcaster of broadcasters) {
        // skip elements that observe sidebar broadcasters and random
        // other elements
        if (broadcaster.localName != "broadcaster") {
          continue;
        }

        if (broadcaster != sidebarBroadcaster) {
          broadcaster.removeAttribute("checked");
        } else {
          sidebarBroadcaster.setAttribute("checked", "true");
        }
      }

      this._box.hidden = false;
      this._splitter.hidden = false;

      this._box.setAttribute("sidebarcommand", sidebarBroadcaster.id);

      let title = sidebarBroadcaster.getAttribute("sidebartitle");
      if (!title) {
        title = sidebarBroadcaster.getAttribute("label");
      }
      this._title.value = title;

      let url = sidebarBroadcaster.getAttribute("sidebarurl");
      this.browser.setAttribute("src", url); // kick off async load

      // We set this attribute here in addition to setting it on the <browser>
      // element itself, because the code in SidebarUI.uninit() persists this
      // attribute, not the "src" of the <browser id="sidebar">. The reason it
      // does that is that we want to delay sidebar load a bit when a browser
      // window opens. See delayedStartup() and SidebarUI.startDelayedLoad().
      this._box.setAttribute("src", url);

      if (this.browser.contentDocument.location.href != url) {
        let onLoad = event => {
          this.browser.removeEventListener("load", onLoad, true);

          // We're handling the 'load' event before it bubbles up to the usual
          // (non-capturing) event handlers. Let it bubble up before firing the
          // SidebarFocused event.
          setTimeout(() => this._fireFocusedEvent(), 0);

          // Run the original function for backwards compatibility.
          sidebarOnLoad(event);

          resolve();
        };

        this.browser.addEventListener("load", onLoad, true);
      } else {
        // Older code handled this case, so we do it too.
        this._fireFocusedEvent();
        resolve();
      }

      let selBrowser = gBrowser.selectedBrowser;
      selBrowser.messageManager.sendAsyncMessage("Sidebar:VisibilityChange",
        {commandID: commandID, isOpen: true}
      );
      BrowserUITelemetry.countSidebarEvent(commandID, "show");
    });
  },

  /**
   * Hide the sidebar.
   */
  hide() {
    if (!this.isOpen) {
      return;
    }

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
    this._title.value = "";
    this._box.hidden = true;
    this._splitter.hidden = true;

    let selBrowser = gBrowser.selectedBrowser;
    selBrowser.focus();
    selBrowser.messageManager.sendAsyncMessage("Sidebar:VisibilityChange",
      {commandID: commandID, isOpen: false}
    );
    BrowserUITelemetry.countSidebarEvent(commandID, "hide");
  },
};

/**
 * This exists for backards compatibility - it will be called once a sidebar is
 * ready, following any request to show it.
 *
 * @deprecated
 */
function fireSidebarFocusedEvent() {}

/**
 * This exists for backards compatibility - it gets called when a sidebar has
 * been loaded.
 *
 * @deprecated
 */
function sidebarOnLoad(event) {}

/**
 * This exists for backards compatibility, and is equivilent to
 * SidebarUI.toggle() without the forceOpen param. With forceOpen set to true,
 * it is equalivent to SidebarUI.show().
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
