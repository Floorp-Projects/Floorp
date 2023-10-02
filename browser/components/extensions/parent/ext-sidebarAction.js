/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { ExtensionParent } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionParent.sys.mjs"
);

var { ExtensionError } = ExtensionUtils;

var { IconDetails } = ExtensionParent;

// WeakMap[Extension -> SidebarAction]
let sidebarActionMap = new WeakMap();

const sidebarURL = "chrome://browser/content/webext-panels.xhtml";

/**
 * Responsible for the sidebar_action section of the manifest as well
 * as the associated sidebar browser.
 */
this.sidebarAction = class extends ExtensionAPI {
  static for(extension) {
    return sidebarActionMap.get(extension);
  }

  onManifestEntry(entryName) {
    let { extension } = this;

    extension.once("ready", this.onReady.bind(this));

    let options = extension.manifest.sidebar_action;

    // Add the extension to the sidebar menu.  The sidebar widget will copy
    // from that when it is viewed, so we shouldn't need to update that.
    let widgetId = makeWidgetId(extension.id);
    this.id = `${widgetId}-sidebar-action`;
    this.menuId = `menubar_menu_${this.id}`;
    this.switcherMenuId = `sidebarswitcher_menu_${this.id}`;

    this.browserStyle = options.browser_style;

    this.defaults = {
      enabled: true,
      title: options.default_title || extension.name,
      icon: IconDetails.normalize({ path: options.default_icon }, extension),
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
    this.windowOpenListener = window => {
      this.createMenuItem(window, this.globals);
    };
    windowTracker.addOpenListener(this.windowOpenListener);

    this.updateHeader = event => {
      let window = event.target.ownerGlobal;
      let details = this.tabContext.get(window.gBrowser.selectedTab);
      let header = window.document.getElementById("sidebar-switcher-target");
      if (window.SidebarUI.currentID === this.id) {
        this.setMenuIcon(header, details);
      }
    };

    this.windowCloseListener = window => {
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

  onShutdown(isAppShutdown) {
    sidebarActionMap.delete(this.this);

    this.tabContext.shutdown();

    // Don't remove everything on app shutdown so session restore can handle
    // restoring open sidebars.
    if (isAppShutdown) {
      return;
    }

    for (let window of windowTracker.browserWindows()) {
      let { document, SidebarUI } = window;
      if (SidebarUI.currentID === this.id) {
        SidebarUI.hide();
      }
      document.getElementById(this.menuId)?.remove();
      document.getElementById(this.switcherMenuId)?.remove();
      let header = document.getElementById("sidebar-switcher-target");
      header.removeEventListener("SidebarShown", this.updateHeader);
      SidebarUI.sidebars.delete(this.id);
    }
    windowTracker.removeOpenListener(this.windowOpenListener);
    windowTracker.removeCloseListener(this.windowCloseListener);
  }

  static onUninstall(id) {
    const sidebarId = `${makeWidgetId(id)}-sidebar-action`;
    for (let window of windowTracker.browserWindows()) {
      let { SidebarUI } = window;
      if (SidebarUI.lastOpenedId === sidebarId) {
        SidebarUI.lastOpenedId = null;
      }
    }
  }

  build() {
    // eslint-disable-next-line mozilla/balanced-listeners
    this.tabContext.on("tab-select", (evt, tab) => {
      this.updateWindow(tab.ownerGlobal);
    });

    let install = this.extension.startupReason === "ADDON_INSTALL";
    for (let window of windowTracker.browserWindows()) {
      this.updateWindow(window);
      let { SidebarUI } = window;
      if (
        (install && this.extension.manifest.sidebar_action.open_at_install) ||
        SidebarUI.lastOpenedId == this.id
      ) {
        SidebarUI.show(this.id);
      }
    }
  }

  createMenuItem(window, details) {
    if (!this.extension.canAccessWindow(window)) {
      return;
    }
    let { document, SidebarUI } = window;
    let keyId = `ext-key-id-${this.id}`;

    SidebarUI.sidebars.set(this.id, {
      title: details.title,
      url: sidebarURL,
      menuId: this.menuId,
      switcherMenuId: this.switcherMenuId,
      // The following properties are specific to extensions
      extensionId: this.extension.id,
      panel: details.panel,
      browserStyle: this.browserStyle,
    });

    let header = document.getElementById("sidebar-switcher-target");
    header.addEventListener("SidebarShown", this.updateHeader);

    // Insert a menuitem for View->Show Sidebars.
    let menuitem = document.createXULElement("menuitem");
    menuitem.setAttribute("id", this.menuId);
    menuitem.setAttribute("type", "checkbox");
    menuitem.setAttribute("label", details.title);
    menuitem.setAttribute("oncommand", `SidebarUI.toggle("${this.id}");`);
    menuitem.setAttribute("class", "menuitem-iconic webextension-menuitem");
    menuitem.setAttribute("key", keyId);
    this.setMenuIcon(menuitem, details);

    // Insert a toolbarbutton for the sidebar dropdown selector.
    let switcherMenuitem = menuitem.cloneNode();
    switcherMenuitem.setAttribute("id", this.switcherMenuId);
    switcherMenuitem.removeAttribute("type");

    document.getElementById("viewSidebarMenu").appendChild(menuitem);
    let separator = document.getElementById("sidebar-extensions-separator");
    separator.parentNode.insertBefore(switcherMenuitem, separator);

    return menuitem;
  }

  setMenuIcon(menuitem, details) {
    let getIcon = size =>
      IconDetails.escapeUrl(
        IconDetails.getPreferredIcon(details.icon, this.extension, size).icon
      );

    menuitem.setAttribute(
      "style",
      `
      --webextension-menuitem-image: image-set(
        url("${getIcon(16)}"),
        url("${getIcon(32)}") 2x
      );
    `
    );
  }

  /**
   * Update the menu items with the tab context data in `tabData`.
   *
   * @param {ChromeWindow} window
   *        Browser chrome window.
   * @param {object} tabData
   *        Tab specific sidebar configuration.
   */
  updateButton(window, tabData) {
    let { document, SidebarUI } = window;
    let title = tabData.title || this.extension.name;
    let menu = document.getElementById(this.menuId);
    if (!menu) {
      menu = this.createMenuItem(window, tabData);
    }

    let urlChanged = tabData.panel !== SidebarUI.sidebars.get(this.id).panel;
    if (urlChanged) {
      SidebarUI.sidebars.get(this.id).panel = tabData.panel;
    }

    menu.setAttribute("label", title);
    this.setMenuIcon(menu, tabData);

    let button = document.getElementById(this.switcherMenuId);
    button.setAttribute("label", title);
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
   * Update the menu items for a given window.
   *
   * @param {ChromeWindow} window
   *        Browser chrome window.
   */
  updateWindow(window) {
    if (!this.extension.canAccessWindow(window)) {
      return;
    }
    let nativeTab = window.gBrowser.selectedTab;
    this.updateButton(window, this.tabContext.get(nativeTab));
  }

  /**
   * Update the menu items when the extension changes the icon,
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
   * Gets the target object corresponding to the `details` parameter of the various
   * get* and set* API methods.
   *
   * @param {object} details
   *        An object with optional `tabId` or `windowId` properties.
   * @param {number} [details.tabId]
   *        The target tab.
   * @param {number} [details.windowId]
   *        The target window.
   * @throws if both `tabId` and `windowId` are specified, or if they are invalid.
   * @returns {XULElement|ChromeWindow|null}
   *        If a `tabId` was specified, the corresponding XULElement tab.
   *        If a `windowId` was specified, the corresponding ChromeWindow.
   *        Otherwise, `null`.
   */
  getTargetFromDetails({ tabId, windowId }) {
    if (tabId != null && windowId != null) {
      throw new ExtensionError(
        "Only one of tabId and windowId can be specified."
      );
    }
    let target = null;
    if (tabId != null) {
      target = tabTracker.getTab(tabId);
      if (!this.extension.canAccessWindow(target.ownerGlobal)) {
        throw new ExtensionError(`Invalid tab ID: ${tabId}`);
      }
    } else if (windowId != null) {
      target = windowTracker.getWindow(windowId);
      if (!this.extension.canAccessWindow(target)) {
        throw new ExtensionError(`Invalid window ID: ${windowId}`);
      }
    }
    return target;
  }

  /**
   * Gets the data associated with a tab, window, or the global one.
   *
   * @param {XULElement|ChromeWindow|null} target
   *        A XULElement tab, a ChromeWindow, or null for the global data.
   * @returns {object}
   *        The icon, title, panel, etc. associated with the target.
   */
  getContextData(target) {
    if (target) {
      return this.tabContext.get(target);
    }
    return this.globals;
  }

  /**
   * Set a global, window specific or tab specific property.
   *
   * @param {XULElement|ChromeWindow|null} target
   *        A XULElement tab, a ChromeWindow, or null for the global data.
   * @param {string} prop
   *        String property to set ["icon", "title", or "panel"].
   * @param {string} value
   *        Value for property.
   */
  setProperty(target, prop, value) {
    let values = this.getContextData(target);
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
   * @param {XULElement|ChromeWindow|null} target
   *        A XULElement tab, a ChromeWindow, or null for the global data.
   * @param {string} prop
   *        String property to retrieve ["icon", "title", or "panel"]
   * @returns {string} value
   *          Value of prop.
   */
  getProperty(target, prop) {
    return this.getContextData(target)[prop];
  }

  setPropertyFromDetails(details, prop, value) {
    return this.setProperty(this.getTargetFromDetails(details), prop, value);
  }

  getPropertyFromDetails(details, prop) {
    return this.getProperty(this.getTargetFromDetails(details), prop);
  }

  /**
   * Triggers this sidebar action for the given window, with the same effects as
   * if it were toggled via menu or toolbarbutton by a user.
   *
   * @param {ChromeWindow} window
   */
  triggerAction(window) {
    let { SidebarUI } = window;
    if (SidebarUI && this.extension.canAccessWindow(window)) {
      SidebarUI.toggle(this.id);
    }
  }

  /**
   * Opens this sidebar action for the given window.
   *
   * @param {ChromeWindow} window
   */
  open(window) {
    let { SidebarUI } = window;
    if (SidebarUI && this.extension.canAccessWindow(window)) {
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
   * Toogles this sidebar action for the given window
   *
   * @param {ChromeWindow} window
   */
  toggle(window) {
    let { SidebarUI } = window;
    if (!SidebarUI || !this.extension.canAccessWindow(window)) {
      return;
    }

    if (!this.isOpen(window)) {
      SidebarUI.show(this.id);
    } else {
      SidebarUI.hide();
    }
  }

  /**
   * Checks whether this sidebar action is open in the given window.
   *
   * @param {ChromeWindow} window
   * @returns {boolean}
   */
  isOpen(window) {
    let { SidebarUI } = window;
    return SidebarUI.isOpen && this.id == SidebarUI.currentID;
  }

  getAPI(context) {
    let { extension } = context;
    const sidebarAction = this;

    return {
      sidebarAction: {
        async setTitle(details) {
          sidebarAction.setPropertyFromDetails(details, "title", details.title);
        },

        getTitle(details) {
          return sidebarAction.getPropertyFromDetails(details, "title");
        },

        async setIcon(details) {
          let icon = IconDetails.normalize(details, extension, context);
          if (!Object.keys(icon).length) {
            icon = null;
          }
          sidebarAction.setPropertyFromDetails(details, "icon", icon);
        },

        async setPanel(details) {
          let url;
          // Clear the url when given null or empty string.
          if (!details.panel) {
            url = null;
          } else {
            url = context.uri.resolve(details.panel);
            if (!context.checkLoadURL(url)) {
              return Promise.reject({
                message: `Access denied for URL ${url}`,
              });
            }
          }

          sidebarAction.setPropertyFromDetails(details, "panel", url);
        },

        getPanel(details) {
          return sidebarAction.getPropertyFromDetails(details, "panel");
        },

        open() {
          let window = windowTracker.topWindow;
          if (context.canAccessWindow(window)) {
            sidebarAction.open(window);
          }
        },

        close() {
          let window = windowTracker.topWindow;
          if (context.canAccessWindow(window)) {
            sidebarAction.close(window);
          }
        },

        toggle() {
          let window = windowTracker.topWindow;
          if (context.canAccessWindow(window)) {
            sidebarAction.toggle(window);
          }
        },

        isOpen(details) {
          let { windowId } = details;
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
