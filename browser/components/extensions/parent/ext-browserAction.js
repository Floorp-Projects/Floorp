/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

ChromeUtils.defineModuleGetter(this, "CustomizableUI",
                               "resource:///modules/CustomizableUI.jsm");
ChromeUtils.defineModuleGetter(this, "clearTimeout",
                               "resource://gre/modules/Timer.jsm");
ChromeUtils.defineModuleGetter(this, "setTimeout",
                               "resource://gre/modules/Timer.jsm");
ChromeUtils.defineModuleGetter(this, "TelemetryStopwatch",
                               "resource://gre/modules/TelemetryStopwatch.jsm");
ChromeUtils.defineModuleGetter(this, "ViewPopup",
                               "resource:///modules/ExtensionPopups.jsm");

var {
  DefaultWeakMap,
  ExtensionError,
} = ExtensionUtils;

ChromeUtils.import("resource://gre/modules/ExtensionParent.jsm");

var {
  IconDetails,
  StartupCache,
} = ExtensionParent;

Cu.importGlobalProperties(["InspectorUtils"]);

const POPUP_PRELOAD_TIMEOUT_MS = 200;
const POPUP_OPEN_MS_HISTOGRAM = "WEBEXT_BROWSERACTION_POPUP_OPEN_MS";
const POPUP_RESULT_HISTOGRAM = "WEBEXT_BROWSERACTION_POPUP_PRELOAD_RESULT_COUNT";

var XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

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

    this.tabContext = new TabContext(target => {
      let window = target.ownerGlobal;
      if (target === window) {
        return this.globals;
      }
      return this.tabContext.get(window);
    });

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
        node.setAttribute("data-extensionid", this.extension.id);

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
            if (node && node.contains(event.originalTarget)) {
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

        if (contexts.includes(menu.id) && node && node.contains(trigger)) {
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

    let fixedWidth =
      this.widget.areaType == CustomizableUI.TYPE_MENU_PANEL ||
      this.widget.forWindow(window).overflowed;
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

  /**
   * Update the toolbar button for a given window.
   *
   * @param {ChromeWindow} window
   *        Browser chrome window.
   */
  updateWindow(window) {
    let widget = this.widget.forWindow(window);
    if (widget) {
      let tab = window.gBrowser.selectedTab;
      this.updateButton(widget.node, this.tabContext.get(tab));
    }
  }

  /**
   * Update the toolbar button when the extension changes the icon, title, url, etc.
   * If it only changes a parameter for a single tab, `target` will be that tab.
   * If it only changes a parameter for a single window, `target` will be that window.
   * Otherwise `target` will be null.
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
   *        - `values` will contain the icon, title, badge, etc. associated with
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
   *        String property to set. Should should be one of "icon", "title",
   *        "badgeText", "popup", "badgeBackgroundColor" or "enabled".
   * @param {string} value
   *        Value for prop.
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
   *        String property to retrieve. Should should be one of "icon", "title",
   *        "badgeText", "popup", "badgeBackgroundColor" or "enabled".
   * @returns {string} value
   *          Value of prop.
   */
  getProperty(details, prop) {
    return this.getContextData(details).values[prop];
  }

  getAPI(context) {
    let {extension} = context;
    let {tabManager} = extension;

    let browserAction = this;

    return {
      browserAction: {
        onClicked: new EventManager({
          context,
          name: "browserAction.onClicked",
          inputHandling: true,
          register: fire => {
            let listener = (event, browser) => {
              context.withPendingBrowser(browser, () =>
                fire.sync(tabManager.convert(tabTracker.activeTab)));
            };
            browserAction.on("click", listener);
            return () => {
              browserAction.off("click", listener);
            };
          },
        }).api(),

        enable: function(tabId) {
          browserAction.setProperty({tabId}, "enabled", true);
        },

        disable: function(tabId) {
          browserAction.setProperty({tabId}, "enabled", false);
        },

        isEnabled: function(details) {
          return browserAction.getProperty(details, "enabled");
        },

        setTitle: function(details) {
          browserAction.setProperty(details, "title", details.title);
        },

        getTitle: function(details) {
          return browserAction.getProperty(details, "title");
        },

        setIcon: function(details) {
          details.iconType = "browserAction";

          let icon = IconDetails.normalize(details, extension, context);
          if (!Object.keys(icon).length) {
            icon = null;
          }
          browserAction.setProperty(details, "icon", icon);
        },

        setBadgeText: function(details) {
          browserAction.setProperty(details, "badgeText", details.text);
        },

        getBadgeText: function(details) {
          return browserAction.getProperty(details, "badgeText");
        },

        setPopup: function(details) {
          // Note: Chrome resolves arguments to setIcon relative to the calling
          // context, but resolves arguments to setPopup relative to the extension
          // root.
          // For internal consistency, we currently resolve both relative to the
          // calling context.
          let url = details.popup && context.uri.resolve(details.popup);
          if (url && !context.checkLoadURL(url)) {
            return Promise.reject({message: `Access denied for URL ${url}`});
          }
          browserAction.setProperty(details, "popup", url);
        },

        getPopup: function(details) {
          return browserAction.getProperty(details, "popup");
        },

        setBadgeBackgroundColor: function(details) {
          let color = details.color;
          if (typeof color == "string") {
            let col = InspectorUtils.colorToRGBA(color);
            if (!col) {
              throw new ExtensionError(`Invalid badge background color: "${color}"`);
            }
            color = col && [col.r, col.g, col.b, Math.round(col.a * 255)];
          }
          browserAction.setProperty(details, "badgeBackgroundColor", color);
        },

        getBadgeBackgroundColor: function(details, callback) {
          let color = browserAction.getProperty(details, "badgeBackgroundColor");
          return color || [0xd9, 0, 0, 255];
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
