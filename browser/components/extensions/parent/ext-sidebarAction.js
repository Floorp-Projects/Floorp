/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

ChromeUtils.import("resource://gre/modules/ExtensionParent.jsm");

var {
  ExtensionError,
} = ExtensionUtils;

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

    this.browserStyle = options.browser_style;

    this.defaults = {
      enabled: true,
      title: options.default_title || extension.name,
      icon: IconDetails.normalize({path: options.default_icon}, extension),
      panel: options.default_panel || "",
    };
    this.globals = Object.create(this.defaults);

    this.tabContext = new TabContext(target => {
      let window = target.ownerGlobal;
      if (target === window) {
        return this.globals;
      }
      return this.tabContext.get(window);
    });

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
      if (SidebarUI.lastOpenedId === this.id &&
          reason === "ADDON_UNINSTALL") {
        SidebarUI.lastOpenedId = null;
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
      if ((install && this.extension.manifest.sidebar_action.open_at_install) ||
          SidebarUI.lastOpenedId == this.id) {
        SidebarUI.show(this.id);
      }
    }
  }

  createMenuItem(window, details) {
    let {document, SidebarUI} = window;

    // Use of the broadcaster allows browser-sidebar.js to properly manage the
    // checkmarks in the menus.
    let broadcaster = document.createElementNS(XUL_NS, "broadcaster");
    broadcaster.setAttribute("id", this.id);
    broadcaster.setAttribute("autoCheck", "false");
    broadcaster.setAttribute("type", "checkbox");
    broadcaster.setAttribute("group", "sidebar");
    broadcaster.setAttribute("label", details.title);
    broadcaster.setAttribute("sidebarurl", sidebarURL);
    broadcaster.setAttribute("panel", details.panel);
    if (this.browserStyle) {
      broadcaster.setAttribute("browserStyle", "true");
    }
    broadcaster.setAttribute("extensionId", this.extension.id);
    let id = `ext-key-id-${this.id}`;
    broadcaster.setAttribute("key", id);

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
    SidebarUI.updateShortcut({button: toolbarbutton});

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

    let urlChanged = tabData.panel !== broadcaster.getAttribute("panel");
    if (urlChanged) {
      broadcaster.setAttribute("panel", tabData.panel);
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
   * title, url, etc. If it only changes a parameter for a single tab, `target`
   * will be that tab. If it only changes a parameter for a single window,
   * `target` will be that window. Otherwise `target` will be null.
   *
   * @param {XULElement|ChromeWindow|null} target
   *        Browser tab or browser chrome window, may be null.
   */
  updateOnChange(target) {
    if (target) {
      let window = target.ownerGlobal;
      if (target === window || target.selected) {
        this.updateWindow(window);
      }
    } else {
      for (let window of windowTracker.browserWindows()) {
        this.updateWindow(window);
      }
    }
  }

  /**
   * Gets the target object and its associated values corresponding to
   * the `details` parameter of the various get* and set* API methods.
   *
   * @param {Object} details
   *        An object with optional `tabId` or `windowId` properties.
   * @throws if both `tabId` and `windowId` are specified, or if they are invalid.
   * @returns {Object}
   *        An object with two properties: `target` and `values`.
   *        - If a `tabId` was specified, `target` will be the corresponding
   *          XULElement tab. If a `windowId` was specified, `target` will be
   *          the corresponding ChromeWindow. Otherwise it will be `null`.
   *        - `values` will contain the icon, title and panel associated with
   *          the target.
   */
  getContextData({tabId, windowId}) {
    if (tabId != null && windowId != null) {
      throw new ExtensionError("Only one of tabId and windowId can be specified.");
    }
    let target, values;
    if (tabId != null) {
      target = tabTracker.getTab(tabId);
      values = this.tabContext.get(target);
    } else if (windowId != null) {
      target = windowTracker.getWindow(windowId);
      values = this.tabContext.get(target);
    } else {
      target = null;
      values = this.globals;
    }
    return {target, values};
  }

  /**
   * Set a global, window specific or tab specific property.
   *
   * @param {Object} details
   *        An object with optional `tabId` or `windowId` properties.
   * @param {string} prop
   *        String property to set ["icon", "title", or "panel"].
   * @param {string} value
   *        Value for property.
   */
  setProperty(details, prop, value) {
    let {target, values} = this.getContextData(details);
    if (value === null) {
      delete values[prop];
    } else {
      values[prop] = value;
    }

    this.updateOnChange(target);
  }

  /**
   * Retrieve the value of a global, window specific or tab specific property.
   *
   * @param {Object} details
   *        An object with optional `tabId` or `windowId` properties.
   * @param {string} prop
   *        String property to retrieve ["icon", "title", or "panel"]
   * @returns {string} value
   *          Value of prop.
   */
  getProperty(details, prop) {
    return this.getContextData(details).values[prop];
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

    return {
      sidebarAction: {
        async setTitle(details) {
          sidebarAction.setProperty(details, "title", details.title);
        },

        getTitle(details) {
          return sidebarAction.getProperty(details, "title");
        },

        async setIcon(details) {
          let icon = IconDetails.normalize(details, extension, context);
          if (!Object.keys(icon).length) {
            icon = null;
          }
          sidebarAction.setProperty(details, "icon", icon);
        },

        async setPanel(details) {
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

          sidebarAction.setProperty(details, "panel", url);
        },

        getPanel(details) {
          return sidebarAction.getProperty(details, "panel");
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
            windowId = Window.WINDOW_ID_CURRENT;
          }
          let window = windowTracker.getWindow(windowId, context);
          return sidebarAction.isOpen(window);
        },
      },
    };
  }
};

global.sidebarActionFor = this.sidebarAction.for;
