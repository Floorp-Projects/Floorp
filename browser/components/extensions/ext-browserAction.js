/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

/* exported browserActionFor, sidebarActionFor, pageActionFor */
/* global browserActionFor:false, sidebarActionFor:false, pageActionFor:false */

// The ext-* files are imported into the same scopes.
/* import-globals-from ext-browser.js */

XPCOMUtils.defineLazyModuleGetter(this, "CustomizableUI",
                                  "resource:///modules/CustomizableUI.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "clearTimeout",
                                  "resource://gre/modules/Timer.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "setTimeout",
                                  "resource://gre/modules/Timer.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "TelemetryStopwatch",
                                  "resource://gre/modules/TelemetryStopwatch.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ViewPopup",
                                  "resource:///modules/ExtensionPopups.jsm");

var {
  DefaultWeakMap,
} = ExtensionUtils;

Cu.import("resource://gre/modules/ExtensionParent.jsm");

var {
  IconDetails,
  StartupCache,
} = ExtensionParent;

Cu.importGlobalProperties(["InspectorUtils"]);

const POPUP_PRELOAD_TIMEOUT_MS = 200;
const POPUP_OPEN_MS_HISTOGRAM = "WEBEXT_BROWSERACTION_POPUP_OPEN_MS";
const POPUP_RESULT_HISTOGRAM = "WEBEXT_BROWSERACTION_POPUP_PRELOAD_RESULT_COUNT";

var XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

const isAncestorOrSelf = (target, node) => {
  for (; node; node = node.parentNode) {
    if (node === target) {
      return true;
    }
  }
  return false;
};

// WeakMap[Extension -> BrowserAction]
const browserActionMap = new WeakMap();

XPCOMUtils.defineLazyGetter(this, "browserAreas", () => {
  return {
    "navbar": CustomizableUI.AREA_NAVBAR,
    "menupanel": CustomizableUI.AREA_FIXED_OVERFLOW_PANEL,
    "tabstrip": CustomizableUI.AREA_TABSTRIP,
    "personaltoolbar": CustomizableUI.AREA_BOOKMARKS,
  };
});

this.browserAction = class extends ExtensionAPI {
  static for(extension) {
    return browserActionMap.get(extension);
  }

  async onManifestEntry(entryName) {
    let {extension} = this;

    let options = extension.manifest.browser_action;

    this.iconData = new DefaultWeakMap(icons => this.getIconData(icons));

    let widgetId = makeWidgetId(extension.id);
    this.id = `${widgetId}-browser-action`;
    this.viewId = `PanelUI-webext-${widgetId}-browser-action-view`;
    this.widget = null;

    this.pendingPopup = null;
    this.pendingPopupTimeout = null;
    this.eventQueue = [];

    this.tabManager = extension.tabManager;

    this.defaults = {
      enabled: true,
      title: options.default_title || extension.name,
      badgeText: "",
      badgeBackgroundColor: null,
      popup: options.default_popup || "",
      area: browserAreas[options.default_area || "navbar"],
    };
    this.globals = Object.create(this.defaults);

    this.browserStyle = options.browser_style || false;
    if (options.browser_style === null) {
      this.extension.logger.warn("Please specify whether you want browser_style " +
                                 "or not in your browser_action options.");
    }

    browserActionMap.set(extension, this);

    this.defaults.icon = await StartupCache.get(
      extension, ["browserAction", "default_icon"],
      () => IconDetails.normalize({
        path: options.default_icon,
        iconType: "browserAction",
        themeIcons: options.theme_icons,
      }, extension));

    this.iconData.set(
      this.defaults.icon,
      await StartupCache.get(
        extension, ["browserAction", "default_icon_data"],
        () => this.getIconData(this.defaults.icon)));

    this.tabContext = new TabContext(tab => Object.create(this.globals),
                                     extension);

    // eslint-disable-next-line mozilla/balanced-listeners
    this.tabContext.on("location-change", this.handleLocationChange.bind(this));

    this.build();
  }

  handleLocationChange(eventType, tab, fromBrowse) {
    if (fromBrowse) {
      this.tabContext.clear(tab);
      this.updateOnChange(tab);
    }
  }

  onShutdown(reason) {
    browserActionMap.delete(this.extension);

    this.tabContext.shutdown();
    CustomizableUI.destroyWidget(this.id);

    this.clearPopup();
  }

  build() {
    let widget = CustomizableUI.createWidget({
      id: this.id,
      viewId: this.viewId,
      type: "view",
      removable: true,
      label: this.defaults.title || this.extension.name,
      tooltiptext: this.defaults.title || "",
      defaultArea: this.defaults.area,

      // Don't attempt to load properties from the built-in widget string
      // bundle.
      localized: false,

      onBeforeCreated: document => {
        let view = document.createElementNS(XUL_NS, "panelview");
        view.id = this.viewId;
        view.setAttribute("flex", "1");
        view.setAttribute("extension", true);

        document.getElementById("appMenu-viewCache").appendChild(view);

        if (this.extension.hasPermission("menus") ||
            this.extension.hasPermission("contextMenus")) {
          document.addEventListener("popupshowing", this);
        }
      },

      onDestroyed: document => {
        document.removeEventListener("popupshowing", this);

        let view = document.getElementById(this.viewId);
        if (view) {
          this.clearPopup();
          CustomizableUI.hidePanelForNode(view);
          view.remove();
        }
      },

      onCreated: node => {
        node.classList.add("badged-button");
        node.classList.add("webextension-browser-action");
        node.setAttribute("constrain-size", "true");

        node.onmousedown = event => this.handleEvent(event);
        node.onmouseover = event => this.handleEvent(event);
        node.onmouseout = event => this.handleEvent(event);

        this.updateButton(node, this.globals, true);
      },

      onViewShowing: async event => {
        TelemetryStopwatch.start(POPUP_OPEN_MS_HISTOGRAM, this);
        let document = event.target.ownerDocument;
        let tabbrowser = document.defaultView.gBrowser;

        let tab = tabbrowser.selectedTab;
        let popupURL = this.getProperty(tab, "popup");
        this.tabManager.addActiveTabPermission(tab);

        // Popups are shown only if a popup URL is defined; otherwise
        // a "click" event is dispatched. This is done for compatibility with the
        // Google Chrome onClicked extension API.
        if (popupURL) {
          try {
            let popup = this.getPopup(document.defaultView, popupURL);
            let attachPromise = popup.attach(event.target);
            event.detail.addBlocker(attachPromise);
            await attachPromise;
            TelemetryStopwatch.finish(POPUP_OPEN_MS_HISTOGRAM, this);
            if (this.eventQueue.length) {
              let histogram = Services.telemetry.getHistogramById(POPUP_RESULT_HISTOGRAM);
              histogram.add("popupShown");
              this.eventQueue = [];
            }
          } catch (e) {
            TelemetryStopwatch.cancel(POPUP_OPEN_MS_HISTOGRAM, this);
            Cu.reportError(e);
            event.preventDefault();
          }
        } else {
          TelemetryStopwatch.cancel(POPUP_OPEN_MS_HISTOGRAM, this);
          // This isn't not a hack, but it seems to provide the correct behavior
          // with the fewest complications.
          event.preventDefault();
          this.emit("click", tabbrowser.selectedBrowser);
          // Ensure we close any popups this node was in:
          CustomizableUI.hidePanelForNode(event.target);
        }
      },
    });

    this.tabContext.on("tab-select", // eslint-disable-line mozilla/balanced-listeners
                       (evt, tab) => { this.updateWindow(tab.ownerGlobal); });

    this.widget = widget;
  }

  /**
   * Triggers this browser action for the given window, with the same effects as
   * if it were clicked by a user.
   *
   * This has no effect if the browser action is disabled for, or not
   * present in, the given window.
   *
   * @param {Window} window
   */
  async triggerAction(window) {
    let popup = ViewPopup.for(this.extension, window);
    if (popup) {
      popup.closePopup();
      return;
    }

    let widget = this.widget.forWindow(window);
    let tab = window.gBrowser.selectedTab;

    if (!widget || !this.getProperty(tab, "enabled")) {
      return;
    }

    // Popups are shown only if a popup URL is defined; otherwise
    // a "click" event is dispatched. This is done for compatibility with the
    // Google Chrome onClicked extension API.
    if (this.getProperty(tab, "popup")) {
      if (this.widget.areaType == CustomizableUI.TYPE_MENU_PANEL) {
        await window.document.getElementById("nav-bar").overflowable.show();
      }

      let event = new window.CustomEvent("command", {bubbles: true, cancelable: true});
      widget.node.dispatchEvent(event);
    } else {
      this.tabManager.addActiveTabPermission(tab);
      this.emit("click");
    }
  }

  handleEvent(event) {
    let button = event.target;
    let window = button.ownerGlobal;

    switch (event.type) {
      case "mousedown":
        if (event.button == 0) {
          // Begin pre-loading the browser for the popup, so it's more likely to
          // be ready by the time we get a complete click.
          let tab = window.gBrowser.selectedTab;
          let popupURL = this.getProperty(tab, "popup");
          let enabled = this.getProperty(tab, "enabled");

          if (popupURL && enabled && (this.pendingPopup || !ViewPopup.for(this.extension, window))) {
            this.eventQueue.push("Mousedown");
            // Add permission for the active tab so it will exist for the popup.
            // Store the tab to revoke the permission during clearPopup.
            if (!this.tabManager.hasActiveTabPermission(tab)) {
              this.tabManager.addActiveTabPermission(tab);
              this.tabToRevokeDuringClearPopup = tab;
            }

            this.pendingPopup = this.getPopup(window, popupURL);
            window.addEventListener("mouseup", this, true);
          } else {
            this.clearPopup();
          }
        }
        break;

      case "mouseup":
        if (event.button == 0) {
          this.clearPopupTimeout();
          // If we have a pending pre-loaded popup, cancel it after we've waited
          // long enough that we can be relatively certain it won't be opening.
          if (this.pendingPopup) {
            let node = window.gBrowser && this.widget.forWindow(window).node;
            if (isAncestorOrSelf(node, event.originalTarget)) {
              this.pendingPopupTimeout = setTimeout(() => this.clearPopup(),
                                                    POPUP_PRELOAD_TIMEOUT_MS);
            } else {
              this.clearPopup();
            }
          }
        }
        break;

      case "mouseover": {
        // Begin pre-loading the browser for the popup, so it's more likely to
        // be ready by the time we get a complete click.
        let tab = window.gBrowser.selectedTab;
        let popupURL = this.getProperty(tab, "popup");
        let enabled = this.getProperty(tab, "enabled");

        if (popupURL && enabled && (this.pendingPopup || !ViewPopup.for(this.extension, window))) {
          this.eventQueue.push("Hover");
          this.pendingPopup = this.getPopup(window, popupURL, true);
        }
        break;
      }

      case "mouseout":
        if (this.pendingPopup) {
          if (this.eventQueue.length) {
            let histogram = Services.telemetry.getHistogramById(POPUP_RESULT_HISTOGRAM);
            histogram.add(`clearAfter${this.eventQueue.pop()}`);
            this.eventQueue = [];
          }
          this.clearPopup();
        }
        break;


      case "popupshowing":
        const menu = event.target;
        const trigger = menu.triggerNode;
        const node = window.document.getElementById(this.id);
        const contexts = ["toolbar-context-menu", "customizationPanelItemContextMenu"];

        if (contexts.includes(menu.id) && node && isAncestorOrSelf(node, trigger)) {
          global.actionContextMenu({
            extension: this.extension,
            onBrowserAction: true,
            menu: menu,
          });
        }
        break;
    }
  }

  /**
   * Returns a potentially pre-loaded popup for the given URL in the given
   * window. If a matching pre-load popup already exists, returns that.
   * Otherwise, initializes a new one.
   *
   * If a pre-load popup exists which does not match, it is destroyed before a
   * new one is created.
   *
   * @param {Window} window
   *        The browser window in which to create the popup.
   * @param {string} popupURL
   *        The URL to load into the popup.
   * @param {boolean} [blockParser = false]
   *        True if the HTML parser should initially be blocked.
   * @returns {ViewPopup}
   */
  getPopup(window, popupURL, blockParser = false) {
    this.clearPopupTimeout();
    let {pendingPopup} = this;
    this.pendingPopup = null;

    if (pendingPopup) {
      if (pendingPopup.window === window && pendingPopup.popupURL === popupURL) {
        if (!blockParser) {
          pendingPopup.unblockParser();
        }

        return pendingPopup;
      }
      pendingPopup.destroy();
    }

    let fixedWidth = this.widget.areaType == CustomizableUI.TYPE_MENU_PANEL;
    return new ViewPopup(this.extension, window, popupURL, this.browserStyle, fixedWidth, blockParser);
  }

  /**
   * Clears any pending pre-loaded popup and related timeouts.
   */
  clearPopup() {
    this.clearPopupTimeout();
    if (this.pendingPopup) {
      if (this.tabToRevokeDuringClearPopup) {
        this.tabManager.revokeActiveTabPermission(this.tabToRevokeDuringClearPopup);
      }
      this.pendingPopup.destroy();
      this.pendingPopup = null;
    }
    this.tabToRevokeDuringClearPopup = null;
  }

  /**
   * Clears any pending timeouts to clear stale, pre-loaded popups.
   */
  clearPopupTimeout() {
    if (this.pendingPopup) {
      this.pendingPopup.window.removeEventListener("mouseup", this, true);
    }

    if (this.pendingPopupTimeout) {
      clearTimeout(this.pendingPopupTimeout);
      this.pendingPopupTimeout = null;
    }
  }

  // Update the toolbar button |node| with the tab context data
  // in |tabData|.
  updateButton(node, tabData, sync = false) {
    let title = tabData.title || this.extension.name;
    let callback = () => {
      node.setAttribute("tooltiptext", title);
      node.setAttribute("label", title);

      if (tabData.badgeText) {
        node.setAttribute("badge", tabData.badgeText);
      } else {
        node.removeAttribute("badge");
      }

      if (tabData.enabled) {
        node.removeAttribute("disabled");
      } else {
        node.setAttribute("disabled", "true");
      }

      let color = tabData.badgeBackgroundColor;
      if (color) {
        color = `rgba(${color[0]}, ${color[1]}, ${color[2]}, ${color[3] / 255})`;
        node.setAttribute("badgeStyle", `background-color: ${color};`);
      } else {
        node.removeAttribute("badgeStyle");
      }

      let {style, legacy} = this.iconData.get(tabData.icon);
      const LEGACY_CLASS = "toolbarbutton-legacy-addon";
      if (legacy) {
        node.classList.add(LEGACY_CLASS);
      } else {
        node.classList.remove(LEGACY_CLASS);
      }

      node.setAttribute("style", style);
    };
    if (sync) {
      callback();
    } else {
      node.ownerGlobal.requestAnimationFrame(callback);
    }
  }

  getIconData(icons) {
    let baseSize = 16;
    let {icon, size} = IconDetails.getPreferredIcon(icons, this.extension, baseSize);

    let legacy = false;

    // If the best available icon size is not divisible by 16, check if we have
    // an 18px icon to fall back to, and trim off the padding instead.
    if (size % 16 && typeof icon === "string" && !icon.endsWith(".svg")) {
      let result = IconDetails.getPreferredIcon(icons, this.extension, 18);

      if (result.size % 18 == 0) {
        baseSize = 18;
        icon = result.icon;
        legacy = true;
      }
    }

    let getIcon = (size, theme) => {
      let {icon} = IconDetails.getPreferredIcon(icons, this.extension, size);
      if (typeof icon === "object") {
        return IconDetails.escapeUrl(icon[theme]);
      }
      return IconDetails.escapeUrl(icon);
    };

    let getStyle = (name, size) => {
      return `
        --webextension-${name}: url("${getIcon(size, "default")}");
        --webextension-${name}-light: url("${getIcon(size, "light")}");
        --webextension-${name}-dark: url("${getIcon(size, "dark")}");
      `;
    };

    let style = `
      ${getStyle("menupanel-image", 32)}
      ${getStyle("menupanel-image-2x", 64)}
      ${getStyle("toolbar-image", baseSize)}
      ${getStyle("toolbar-image-2x", baseSize * 2)}
    `;

    return {style, legacy};
  }

  // Update the toolbar button for a given window.
  updateWindow(window) {
    let widget = this.widget.forWindow(window);
    if (widget) {
      let tab = window.gBrowser.selectedTab;
      this.updateButton(widget.node, this.tabContext.get(tab));
    }
  }

  // Update the toolbar button when the extension changes the icon,
  // title, badge, etc. If it only changes a parameter for a single
  // tab, |tab| will be that tab. Otherwise it will be null.
  updateOnChange(tab) {
    if (tab) {
      if (tab.selected) {
        this.updateWindow(tab.ownerGlobal);
      }
    } else {
      for (let window of windowTracker.browserWindows()) {
        this.updateWindow(window);
      }
    }
  }

  // tab is allowed to be null.
  // prop should be one of "icon", "title", "badgeText", "popup", or "badgeBackgroundColor".
  setProperty(tab, prop, value) {
    let values;
    if (tab == null) {
      values = this.globals;
    } else {
      values = this.tabContext.get(tab);
    }
    if (value == null) {
      delete values[prop];
    } else {
      values[prop] = value;
    }

    this.updateOnChange(tab);
  }

  // tab is allowed to be null.
  // prop should be one of "title", "badgeText", "popup", or "badgeBackgroundColor".
  getProperty(tab, prop) {
    if (tab == null) {
      return this.globals[prop];
    }
    return this.tabContext.get(tab)[prop];
  }

  getAPI(context) {
    let {extension} = context;
    let {tabManager} = extension;

    let browserAction = this;

    function getTab(tabId) {
      if (tabId !== null) {
        return tabTracker.getTab(tabId);
      }
      return null;
    }

    return {
      browserAction: {
        onClicked: new InputEventManager(context, "browserAction.onClicked", fire => {
          let listener = (event, browser) => {
            context.withPendingBrowser(browser, () =>
              fire.sync(tabManager.convert(tabTracker.activeTab)));
          };
          browserAction.on("click", listener);
          return () => {
            browserAction.off("click", listener);
          };
        }).api(),

        enable: function(tabId) {
          let tab = getTab(tabId);
          browserAction.setProperty(tab, "enabled", true);
        },

        disable: function(tabId) {
          let tab = getTab(tabId);
          browserAction.setProperty(tab, "enabled", false);
        },

        isEnabled: function(details) {
          let tab = getTab(details.tabId);
          return browserAction.getProperty(tab, "enabled");
        },

        setTitle: function(details) {
          let tab = getTab(details.tabId);

          browserAction.setProperty(tab, "title", details.title);
        },

        getTitle: function(details) {
          let tab = getTab(details.tabId);

          let title = browserAction.getProperty(tab, "title");
          return Promise.resolve(title);
        },

        setIcon: function(details) {
          let tab = getTab(details.tabId);

          details.iconType = "browserAction";

          let icon = IconDetails.normalize(details, extension, context);
          if (!Object.keys(icon).length) {
            icon = null;
          }
          browserAction.setProperty(tab, "icon", icon);
        },

        setBadgeText: function(details) {
          let tab = getTab(details.tabId);

          browserAction.setProperty(tab, "badgeText", details.text);
        },

        getBadgeText: function(details) {
          let tab = getTab(details.tabId);

          let text = browserAction.getProperty(tab, "badgeText");
          return Promise.resolve(text);
        },

        setPopup: function(details) {
          let tab = getTab(details.tabId);

          // Note: Chrome resolves arguments to setIcon relative to the calling
          // context, but resolves arguments to setPopup relative to the extension
          // root.
          // For internal consistency, we currently resolve both relative to the
          // calling context.
          let url = details.popup && context.uri.resolve(details.popup);
          if (url && !context.checkLoadURL(url)) {
            return Promise.reject({message: `Access denied for URL ${url}`});
          }
          browserAction.setProperty(tab, "popup", url);
        },

        getPopup: function(details) {
          let tab = getTab(details.tabId);

          let popup = browserAction.getProperty(tab, "popup");
          return Promise.resolve(popup);
        },

        setBadgeBackgroundColor: function(details) {
          let tab = getTab(details.tabId);
          let color = details.color;
          if (!Array.isArray(color)) {
            let col = InspectorUtils.colorToRGBA(color);
            color = col && [col.r, col.g, col.b, Math.round(col.a * 255)];
          }
          browserAction.setProperty(tab, "badgeBackgroundColor", color);
        },

        getBadgeBackgroundColor: function(details, callback) {
          let tab = getTab(details.tabId);

          let color = browserAction.getProperty(tab, "badgeBackgroundColor");
          return Promise.resolve(color || [0xd9, 0, 0, 255]);
        },

        openPopup: function() {
          let window = windowTracker.topWindow;
          browserAction.triggerAction(window);
        },
      },
    };
  }
};

global.browserActionFor = this.browserAction.for;
