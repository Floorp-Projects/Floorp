/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

// The ext-* files are imported into the same scopes.
/* import-globals-from ext-browser.js */
/* globals WINDOW_ID_CURRENT */

Cu.import("resource://gre/modules/ExtensionParent.jsm");

var {
  IconDetails,
} = ExtensionParent;

var XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

// WeakMap[Extension -> SidebarAction]
let sidebarActionMap = new WeakMap();

const sidebarURL = "chrome://browser/content/webext-panels.xul";

/**
 * Responsible for the sidebar_action section of the manifest as well
 * as the associated sidebar browser.
 */
this.sidebarAction = class extends ExtensionAPI {
  static for(extension) {
    return sidebarActionMap.get(extension);
  }

  onManifestEntry(entryName) {
    let {extension} = this;

    extension.once("ready", this.onReady.bind(this));

    let options = extension.manifest.sidebar_action;

    // Add the extension to the sidebar menu.  The sidebar widget will copy
    // from that when it is viewed, so we shouldn't need to update that.
    let widgetId = makeWidgetId(extension.id);
    this.id = `${widgetId}-sidebar-action`;
    this.menuId = `menu_${this.id}`;
    this.buttonId = `button_${this.id}`;

    // We default browser_style to true because this is a new API and
    // we therefore don't need to worry about breaking existing add-ons.
    this.browserStyle = options.browser_style || options.browser_style === null;

    this.defaults = {
      enabled: true,
      title: options.default_title || extension.name,
      icon: IconDetails.normalize({path: options.default_icon}, extension),
      panel: options.default_panel || "",
    };
    this.globals = Object.create(this.defaults);

    this.tabContext = new TabContext(tab => Object.create(this.globals),
                                     extension);

    // We need to ensure our elements are available before session restore.
    this.windowOpenListener = (window) => {
      this.createMenuItem(window, this.globals);
    };
    windowTracker.addOpenListener(this.windowOpenListener);

    this.updateHeader = (event) => {
      let window = event.target.ownerGlobal;
      let details = this.tabContext.get(window.gBrowser.selectedTab);
      let header = window.document.getElementById("sidebar-switcher-target");
      if (window.SidebarUI.currentID === this.id) {
        this.setMenuIcon(header, details);
      }
    };

    this.windowCloseListener = (window) => {
      let header = window.document.getElementById("sidebar-switcher-target");
      if (header) {
        header.removeEventListener("SidebarShown", this.updateHeader);
      }
    };
    windowTracker.addCloseListener(this.windowCloseListener);

    sidebarActionMap.set(extension, this);
  }

  onReady() {
    this.build();
  }

  onShutdown(reason) {
    sidebarActionMap.delete(this.this);

    this.tabContext.shutdown();

    // Don't remove everything on app shutdown so session restore can handle
    // restoring open sidebars.
    if (reason === "APP_SHUTDOWN") {
      return;
    }

    for (let window of windowTracker.browserWindows()) {
      let {document, SidebarUI} = window;
      if (SidebarUI.currentID === this.id) {
        SidebarUI.hide();
      }
      let menu = document.getElementById(this.menuId);
      if (menu) {
        menu.remove();
      }
      let button = document.getElementById(this.buttonId);
      if (button) {
        button.remove();
      }
      let broadcaster = document.getElementById(this.id);
      if (broadcaster) {
        broadcaster.remove();
      }
      let header = document.getElementById("sidebar-switcher-target");
      header.removeEventListener("SidebarShown", this.updateHeader);
    }
    windowTracker.removeOpenListener(this.windowOpenListener);
    windowTracker.removeCloseListener(this.windowCloseListener);
  }

  build() {
    this.tabContext.on("tab-select", // eslint-disable-line mozilla/balanced-listeners
                       (evt, tab) => { this.updateWindow(tab.ownerGlobal); });

    let install = this.extension.startupReason === "ADDON_INSTALL";
    for (let window of windowTracker.browserWindows()) {
      this.updateWindow(window);
      let {SidebarUI} = window;
      if (install || SidebarUI.lastOpenedId == this.id) {
        SidebarUI.show(this.id);
      }
    }
  }

  sidebarUrl(panel) {
    let url = `${sidebarURL}?panel=${encodeURIComponent(panel)}`;

    if (this.extension.remote) {
      url += "&remote=1";
    }

    if (this.browserStyle) {
      url += "&browser-style=1";
    }

    return url;
  }

  createMenuItem(window, details) {
    let {document} = window;

    // Use of the broadcaster allows browser-sidebar.js to properly manage the
    // checkmarks in the menus.
    let broadcaster = document.createElementNS(XUL_NS, "broadcaster");
    broadcaster.setAttribute("id", this.id);
    broadcaster.setAttribute("autoCheck", "false");
    broadcaster.setAttribute("type", "checkbox");
    broadcaster.setAttribute("group", "sidebar");
    broadcaster.setAttribute("label", details.title);
    broadcaster.setAttribute("sidebarurl", this.sidebarUrl(details.panel));

    // oncommand gets attached to menuitem, so we use the observes attribute to
    // get the command id we pass to SidebarUI.
    broadcaster.setAttribute("oncommand", "SidebarUI.toggle(this.getAttribute('observes'))");

    let header = document.getElementById("sidebar-switcher-target");
    header.addEventListener("SidebarShown", this.updateHeader);

    // Insert a menuitem for View->Show Sidebars.
    let menuitem = document.createElementNS(XUL_NS, "menuitem");
    menuitem.setAttribute("id", this.menuId);
    menuitem.setAttribute("observes", this.id);
    menuitem.setAttribute("class", "menuitem-iconic webextension-menuitem");
    this.setMenuIcon(menuitem, details);

    // Insert a toolbarbutton for the sidebar dropdown selector.
    let toolbarbutton = document.createElementNS(XUL_NS, "toolbarbutton");
    toolbarbutton.setAttribute("id", this.buttonId);
    toolbarbutton.setAttribute("observes", this.id);
    toolbarbutton.setAttribute("class", "subviewbutton subviewbutton-iconic webextension-menuitem");
    this.setMenuIcon(toolbarbutton, details);

    document.getElementById("mainBroadcasterSet").appendChild(broadcaster);
    document.getElementById("viewSidebarMenu").appendChild(menuitem);
    let separator = document.getElementById("sidebar-extensions-separator");
    separator.parentNode.insertBefore(toolbarbutton, separator);

    return menuitem;
  }

  setMenuIcon(menuitem, details) {
    let getIcon = size => IconDetails.escapeUrl(
      IconDetails.getPreferredIcon(details.icon, this.extension, size).icon);

    menuitem.setAttribute("style", `
      --webextension-menuitem-image: url("${getIcon(16)}");
      --webextension-menuitem-image-2x: url("${getIcon(32)}");
    `);
  }

  /**
   * Update the broadcaster and menuitem `node` with the tab context data
   * in `tabData`.
   *
   * @param {ChromeWindow} window
   *        Browser chrome window.
   * @param {object} tabData
   *        Tab specific sidebar configuration.
   */
  updateButton(window, tabData) {
    let {document, SidebarUI} = window;
    let title = tabData.title || this.extension.name;
    let menu = document.getElementById(this.menuId);
    if (!menu) {
      menu = this.createMenuItem(window, tabData);
    }

    // Update the broadcaster first, it will update both menus.
    let broadcaster = document.getElementById(this.id);
    broadcaster.setAttribute("tooltiptext", title);
    broadcaster.setAttribute("label", title);

    let url = this.sidebarUrl(tabData.panel);
    let urlChanged = url !== broadcaster.getAttribute("sidebarurl");
    if (urlChanged) {
      broadcaster.setAttribute("sidebarurl", url);
    }

    this.setMenuIcon(menu, tabData);

    let button = document.getElementById(this.buttonId);
    this.setMenuIcon(button, tabData);

    // Update the sidebar if this extension is the current sidebar.
    if (SidebarUI.currentID === this.id) {
      SidebarUI.title = title;
      let header = document.getElementById("sidebar-switcher-target");
      this.setMenuIcon(header, tabData);
      if (SidebarUI.isOpen && urlChanged) {
        SidebarUI.show(this.id);
      }
    }
  }

  /**
   * Update the broadcaster and menuitem for a given window.
   *
   * @param {ChromeWindow} window
   *        Browser chrome window.
   */
  updateWindow(window) {
    let nativeTab = window.gBrowser.selectedTab;
    this.updateButton(window, this.tabContext.get(nativeTab));
  }

  /**
   * Update the broadcaster and menuitem when the extension changes the icon,
   * title, url, etc. If it only changes a parameter for a single
   * tab, `tab` will be that tab. Otherwise it will be null.
   *
   * @param {XULElement|null} nativeTab
   *        Browser tab, may be null.
   */
  updateOnChange(nativeTab) {
    if (nativeTab) {
      if (nativeTab.selected) {
        this.updateWindow(nativeTab.ownerGlobal);
      }
    } else {
      for (let window of windowTracker.browserWindows()) {
        this.updateWindow(window);
      }
    }
  }

  /**
   * Set a default or tab specific property.
   *
   * @param {XULElement|null} nativeTab
   *        Webextension tab object, may be null.
   * @param {string} prop
   *        String property to retrieve ["icon", "title", or "panel"].
   * @param {string} value
   *        Value for property.
   */
  setProperty(nativeTab, prop, value) {
    let values;
    if (nativeTab === null) {
      values = this.globals;
    } else {
      values = this.tabContext.get(nativeTab);
    }
    if (value === null) {
      delete values[prop];
    } else {
      values[prop] = value;
    }

    this.updateOnChange(nativeTab);
  }

  /**
   * Retrieve a property from the tab or globals if tab is null.
   *
   * @param {XULElement|null} nativeTab
   *        Browser tab object, may be null.
   * @param {string} prop
   *        String property to retrieve ["icon", "title", or "panel"]
   * @returns {string} value
   *          Value for prop.
   */
  getProperty(nativeTab, prop) {
    if (nativeTab === null) {
      return this.globals[prop];
    }
    return this.tabContext.get(nativeTab)[prop];
  }

  /**
   * Triggers this sidebar action for the given window, with the same effects as
   * if it were toggled via menu or toolbarbutton by a user.
   *
   * @param {ChromeWindow} window
   */
  triggerAction(window) {
    let {SidebarUI} = window;
    if (SidebarUI) {
      SidebarUI.toggle(this.id);
    }
  }

  /**
   * Opens this sidebar action for the given window.
   *
   * @param {ChromeWindow} window
   */
  open(window) {
    let {SidebarUI} = window;
    if (SidebarUI) {
      SidebarUI.show(this.id);
    }
  }

  /**
   * Closes this sidebar action for the given window if this sidebar action is open.
   *
   * @param {ChromeWindow} window
   */
  close(window) {
    if (this.isOpen(window)) {
      window.SidebarUI.hide();
    }
  }

  /**
   * Checks whether this sidebar action is open in the given window.
   *
   * @param {ChromeWindow} window
   * @returns {boolean}
   */
  isOpen(window) {
    let {SidebarUI} = window;
    return SidebarUI.isOpen && this.id == SidebarUI.currentID;
  }

  getAPI(context) {
    let {extension} = context;
    const sidebarAction = this;

    function getTab(tabId) {
      if (tabId !== null) {
        return tabTracker.getTab(tabId);
      }
      return null;
    }

    return {
      sidebarAction: {
        async setTitle(details) {
          let nativeTab = getTab(details.tabId);
          sidebarAction.setProperty(nativeTab, "title", details.title);
        },

        getTitle(details) {
          let nativeTab = getTab(details.tabId);

          let title = sidebarAction.getProperty(nativeTab, "title");
          return Promise.resolve(title);
        },

        async setIcon(details) {
          let nativeTab = getTab(details.tabId);

          let icon = IconDetails.normalize(details, extension, context);
          if (!Object.keys(icon).length) {
            icon = null;
          }
          sidebarAction.setProperty(nativeTab, "icon", icon);
        },

        async setPanel(details) {
          let nativeTab = getTab(details.tabId);

          let url;
          // Clear the url when given null or empty string.
          if (!details.panel) {
            url = null;
          } else {
            url = context.uri.resolve(details.panel);
            if (!context.checkLoadURL(url)) {
              return Promise.reject({message: `Access denied for URL ${url}`});
            }
          }

          sidebarAction.setProperty(nativeTab, "panel", url);
        },

        getPanel(details) {
          let nativeTab = getTab(details.tabId);

          let panel = sidebarAction.getProperty(nativeTab, "panel");
          return Promise.resolve(panel);
        },

        open() {
          let window = windowTracker.topWindow;
          sidebarAction.open(window);
        },

        close() {
          let window = windowTracker.topWindow;
          sidebarAction.close(window);
        },

        isOpen(details) {
          let {windowId} = details;
          if (windowId == null) {
            windowId = WINDOW_ID_CURRENT;
          }
          let window = windowTracker.getWindow(windowId, context);
          return sidebarAction.isOpen(window);
        },
      },
    };
  }
};

global.sidebarActionFor = this.sidebarAction.for;
