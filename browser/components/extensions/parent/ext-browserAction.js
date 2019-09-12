/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "CustomizableUI",
  "resource:///modules/CustomizableUI.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "clearTimeout",
  "resource://gre/modules/Timer.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "ExtensionTelemetry",
  "resource://gre/modules/ExtensionTelemetry.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "setTimeout",
  "resource://gre/modules/Timer.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "ViewPopup",
  "resource:///modules/ExtensionPopups.jsm"
);

var { DefaultWeakMap, ExtensionError } = ExtensionUtils;

var { ExtensionParent } = ChromeUtils.import(
  "resource://gre/modules/ExtensionParent.jsm"
);

var { IconDetails, StartupCache } = ExtensionParent;

XPCOMUtils.defineLazyGlobalGetters(this, ["InspectorUtils"]);

const POPUP_PRELOAD_TIMEOUT_MS = 200;

// WeakMap[Extension -> BrowserAction]
const browserActionMap = new WeakMap();

XPCOMUtils.defineLazyGetter(this, "browserAreas", () => {
  return {
    navbar: CustomizableUI.AREA_NAVBAR,
    menupanel: CustomizableUI.AREA_FIXED_OVERFLOW_PANEL,
    tabstrip: CustomizableUI.AREA_TABSTRIP,
    personaltoolbar: CustomizableUI.AREA_BOOKMARKS,
  };
});

this.browserAction = class extends ExtensionAPI {
  static for(extension) {
    return browserActionMap.get(extension);
  }

  async onManifestEntry(entryName) {
    let { extension } = this;

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
      badgeBackgroundColor: [0xd9, 0, 0, 255],
      badgeDefaultColor: [255, 255, 255, 255],
      badgeTextColor: null,
      popup: options.default_popup || "",
      area: browserAreas[options.default_area || "navbar"],
    };
    this.globals = Object.create(this.defaults);

    this.browserStyle = options.browser_style;

    browserActionMap.set(extension, this);

    this.defaults.icon = await StartupCache.get(
      extension,
      ["browserAction", "default_icon"],
      () =>
        IconDetails.normalize(
          {
            path: options.default_icon || extension.manifest.icons,
            iconType: "browserAction",
            themeIcons: options.theme_icons,
          },
          extension
        )
    );

    this.iconData.set(
      this.defaults.icon,
      await StartupCache.get(
        extension,
        ["browserAction", "default_icon_data"],
        () => this.getIconData(this.defaults.icon)
      )
    );

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

  onShutdown() {
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
      showInPrivateBrowsing: this.extension.privateBrowsingAllowed,

      // Don't attempt to load properties from the built-in widget string
      // bundle.
      localized: false,

      onBeforeCreated: document => {
        let view = document.createXULElement("panelview");
        view.id = this.viewId;
        view.setAttribute("flex", "1");
        view.setAttribute("extension", true);

        document.getElementById("appMenu-viewCache").appendChild(view);

        if (
          this.extension.hasPermission("menus") ||
          this.extension.hasPermission("contextMenus")
        ) {
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
        node.classList.add("panel-no-padding");
        node.classList.add("webextension-browser-action");
        node.setAttribute("badged", "true");
        node.setAttribute("constrain-size", "true");
        node.setAttribute("data-extensionid", this.extension.id);

        node.onmousedown = event => this.handleEvent(event);
        node.onmouseover = event => this.handleEvent(event);
        node.onmouseout = event => this.handleEvent(event);
        node.onkeypress = event => this.handleEvent(event);
        node.onmouseup = event => this.handleMouseUp(event);

        this.updateButton(node, this.globals, true);
      },

      onViewShowing: async event => {
        const { extension } = this;

        ExtensionTelemetry.browserActionPopupOpen.stopwatchStart(
          extension,
          this
        );
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
            ExtensionTelemetry.browserActionPopupOpen.stopwatchFinish(
              extension,
              this
            );
            if (this.eventQueue.length) {
              ExtensionTelemetry.browserActionPreloadResult.histogramAdd({
                category: "popupShown",
                extension,
              });
              this.eventQueue = [];
            }
          } catch (e) {
            ExtensionTelemetry.browserActionPopupOpen.stopwatchCancel(
              extension,
              this
            );
            Cu.reportError(e);
            event.preventDefault();
          }
        } else {
          ExtensionTelemetry.browserActionPopupOpen.stopwatchCancel(
            extension,
            this
          );
          // This isn't not a hack, but it seems to provide the correct behavior
          // with the fewest complications.
          event.preventDefault();
          this.emit("click", tabbrowser.selectedBrowser);
          // Ensure we close any popups this node was in:
          CustomizableUI.hidePanelForNode(event.target);
        }
      },
    });

    // eslint-disable-next-line mozilla/balanced-listeners
    this.tabContext.on("tab-select", (evt, tab) => {
      this.updateWindow(tab.ownerGlobal);
    });

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
    if (!this.pendingPopup && popup) {
      popup.closePopup();
      return;
    }

    let widget = this.widget.forWindow(window);
    let tab = window.gBrowser.selectedTab;

    if (!widget.node || !this.getProperty(tab, "enabled")) {
      return;
    }

    // Popups are shown only if a popup URL is defined; otherwise
    // a "click" event is dispatched. This is done for compatibility with the
    // Google Chrome onClicked extension API.
    if (this.getProperty(tab, "popup")) {
      if (this.widget.areaType == CustomizableUI.TYPE_MENU_PANEL) {
        await window.document.getElementById("nav-bar").overflowable.show();
      }

      let event = new window.CustomEvent("command", {
        bubbles: true,
        cancelable: true,
      });
      widget.node.dispatchEvent(event);
    } else {
      this.tabManager.addActiveTabPermission(tab);
      this.lastClickInfo = { button: 0, modifiers: [] };
      this.emit("click");
    }
  }

  handleMouseUp(event) {
    let window = event.target.ownerGlobal;

    this.lastClickInfo = {
      button: event.button,
      modifiers: clickModifiersFromEvent(event),
    };

    if (event.button === 1) {
      let { gBrowser } = window;
      if (this.getProperty(gBrowser.selectedTab, "enabled")) {
        this.emit("click", gBrowser.selectedBrowser);
      }
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

          if (
            popupURL &&
            enabled &&
            (this.pendingPopup || !ViewPopup.for(this.extension, window))
          ) {
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
              this.pendingPopupTimeout = setTimeout(
                () => this.clearPopup(),
                POPUP_PRELOAD_TIMEOUT_MS
              );
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

        if (
          popupURL &&
          enabled &&
          (this.pendingPopup || !ViewPopup.for(this.extension, window))
        ) {
          this.eventQueue.push("Hover");
          this.pendingPopup = this.getPopup(window, popupURL, true);
        }
        break;
      }

      case "mouseout":
        if (this.pendingPopup) {
          if (this.eventQueue.length) {
            ExtensionTelemetry.browserActionPreloadResult.histogramAdd({
              category: `clearAfter${this.eventQueue.pop()}`,
              extension: this.extension,
            });
            this.eventQueue = [];
          }
          this.clearPopup();
        }
        break;

      case "popupshowing":
        const menu = event.target;
        const trigger = menu.triggerNode;
        const node = window.document.getElementById(this.id);
        const contexts = [
          "toolbar-context-menu",
          "customizationPanelItemContextMenu",
        ];

        if (contexts.includes(menu.id) && node && node.contains(trigger)) {
          global.actionContextMenu({
            extension: this.extension,
            onBrowserAction: true,
            menu: menu,
          });
        }
        break;

      case "keypress":
        if (event.key === " " || event.key === "Enter") {
          this.lastClickInfo = {
            button: 0,
            modifiers: clickModifiersFromEvent(event),
          };
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
    let { pendingPopup } = this;
    this.pendingPopup = null;

    if (pendingPopup) {
      if (
        pendingPopup.window === window &&
        pendingPopup.popupURL === popupURL
      ) {
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
    return new ViewPopup(
      this.extension,
      window,
      popupURL,
      this.browserStyle,
      fixedWidth,
      blockParser
    );
  }

  /**
   * Clears any pending pre-loaded popup and related timeouts.
   */
  clearPopup() {
    this.clearPopupTimeout();
    if (this.pendingPopup) {
      if (this.tabToRevokeDuringClearPopup) {
        this.tabManager.revokeActiveTabPermission(
          this.tabToRevokeDuringClearPopup
        );
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

      let serializeColor = ([r, g, b, a]) =>
        `rgba(${r}, ${g}, ${b}, ${a / 255})`;
      node.setAttribute(
        "badgeStyle",
        [
          `background-color: ${serializeColor(tabData.badgeBackgroundColor)}`,
          `color: ${serializeColor(this.getTextColor(tabData))}`,
        ].join("; ")
      );

      let style = this.iconData.get(tabData.icon);
      node.setAttribute("style", style);
    };
    if (sync) {
      callback();
    } else {
      node.ownerGlobal.requestAnimationFrame(callback);
    }
  }

  getIconData(icons) {
    let getIcon = (icon, theme) => {
      if (typeof icon === "object") {
        return IconDetails.escapeUrl(icon[theme]);
      }
      return IconDetails.escapeUrl(icon);
    };

    let getStyle = (name, icon) => {
      return `
        --webextension-${name}: url("${getIcon(icon, "default")}");
        --webextension-${name}-light: url("${getIcon(icon, "light")}");
        --webextension-${name}-dark: url("${getIcon(icon, "dark")}");
      `;
    };

    let icon16 = IconDetails.getPreferredIcon(icons, this.extension, 16).icon;
    let icon32 = IconDetails.getPreferredIcon(icons, this.extension, 32).icon;
    return `
      ${getStyle("menupanel-image", icon16)}
      ${getStyle("menupanel-image-2x", icon32)}
      ${getStyle("toolbar-image", icon16)}
      ${getStyle("toolbar-image-2x", icon32)}
    `;
  }

  /**
   * Update the toolbar button for a given window.
   *
   * @param {ChromeWindow} window
   *        Browser chrome window.
   */
  updateWindow(window) {
    let node = this.widget.forWindow(window).node;
    if (node) {
      let tab = window.gBrowser.selectedTab;
      this.updateButton(node, this.tabContext.get(tab));
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
   * Gets the target object corresponding to the `details` parameter of the various
   * get* and set* API methods.
   *
   * @param {Object} details
   *        An object with optional `tabId` or `windowId` properties.
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
    if (tabId != null) {
      return tabTracker.getTab(tabId);
    } else if (windowId != null) {
      return windowTracker.getWindow(windowId);
    }
    return null;
  }

  /**
   * Gets the data associated with a tab, window, or the global one.
   *
   * @param {XULElement|ChromeWindow|null} target
   *        A XULElement tab, a ChromeWindow, or null for the global data.
   * @returns {Object}
   *        The icon, title, badge, etc. associated with the target.
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
   *        String property to set. Should should be one of "icon", "title", "badgeText",
   *        "popup", "badgeBackgroundColor", "badgeTextColor" or "enabled".
   * @param {string} value
   *        Value for prop.
   * @returns {Object}
   *        The object to which the property has been set.
   */
  setProperty(target, prop, value) {
    let values = this.getContextData(target);
    if (value === null) {
      delete values[prop];
    } else {
      values[prop] = value;
    }

    this.updateOnChange(target);
    return values;
  }

  /**
   * Retrieve the value of a global, window specific or tab specific property.
   *
   * @param {XULElement|ChromeWindow|null} target
   *        A XULElement tab, a ChromeWindow, or null for the global data.
   * @param {string} prop
   *        String property to retrieve. Should should be one of "icon", "title",
   *        "badgeText", "popup", "badgeBackgroundColor" or "enabled".
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
   * Determines the text badge color to be used in a tab, window, or globally.
   *
   * @param {Object} values
   *        The values associated with the tab or window, or global values.
   * @returns {ColorArray}
   */
  getTextColor(values) {
    // If a text color has been explicitly provided, use it.
    let { badgeTextColor } = values;
    if (badgeTextColor) {
      return badgeTextColor;
    }

    // Otherwise, check if the default color to be used has been cached previously.
    let { badgeDefaultColor } = values;
    if (badgeDefaultColor) {
      return badgeDefaultColor;
    }

    // Choose a color among white and black, maximizing contrast with background
    // according to https://www.w3.org/TR/WCAG20-TECHS/G18.html#G18-procedure
    let [r, g, b] = values.badgeBackgroundColor
      .slice(0, 3)
      .map(function(channel) {
        channel /= 255;
        if (channel <= 0.03928) {
          return channel / 12.92;
        }
        return ((channel + 0.055) / 1.055) ** 2.4;
      });
    let lum = 0.2126 * r + 0.7152 * g + 0.0722 * b;

    // The luminance is 0 for black, 1 for white, and `lum` for the background color.
    // Since `0 <= lum`, the contrast ratio for black is `c0 = (lum + 0.05) / 0.05`.
    // Since `lum <= 1`, the contrast ratio for white is `c1 = 1.05 / (lum + 0.05)`.
    // We want to maximize contrast, so black is chosen if `c1 < c0`, that is, if
    // `1.05 * 0.05 < (L + 0.05) ** 2`. Otherwise white is chosen.
    let channel = 1.05 * 0.05 < (lum + 0.05) ** 2 ? 0 : 255;
    let result = [channel, channel, channel, 255];

    // Cache the result as high as possible in the prototype chain
    while (!Object.getOwnPropertyDescriptor(values, "badgeDefaultColor")) {
      values = Object.getPrototypeOf(values);
    }
    values.badgeDefaultColor = result;
    return result;
  }

  getAPI(context) {
    let { extension } = context;
    let { tabManager } = extension;

    let browserAction = this;

    function parseColor(color, kind) {
      if (typeof color == "string") {
        let rgba = InspectorUtils.colorToRGBA(color);
        if (!rgba) {
          throw new ExtensionError(`Invalid badge ${kind} color: "${color}"`);
        }
        color = [rgba.r, rgba.g, rgba.b, Math.round(rgba.a * 255)];
      }
      return color;
    }

    return {
      browserAction: {
        onClicked: new EventManager({
          context,
          name: "browserAction.onClicked",
          inputHandling: true,
          register: fire => {
            let listener = (event, browser) => {
              context.withPendingBrowser(browser, () =>
                fire.sync(
                  tabManager.convert(tabTracker.activeTab),
                  browserAction.lastClickInfo
                )
              );
            };
            browserAction.on("click", listener);
            return () => {
              browserAction.off("click", listener);
            };
          },
        }).api(),

        enable: function(tabId) {
          browserAction.setPropertyFromDetails({ tabId }, "enabled", true);
        },

        disable: function(tabId) {
          browserAction.setPropertyFromDetails({ tabId }, "enabled", false);
        },

        isEnabled: function(details) {
          return browserAction.getPropertyFromDetails(details, "enabled");
        },

        setTitle: function(details) {
          browserAction.setPropertyFromDetails(details, "title", details.title);
        },

        getTitle: function(details) {
          return browserAction.getPropertyFromDetails(details, "title");
        },

        setIcon: function(details) {
          details.iconType = "browserAction";

          let icon = IconDetails.normalize(details, extension, context);
          if (!Object.keys(icon).length) {
            icon = null;
          }
          browserAction.setPropertyFromDetails(details, "icon", icon);
        },

        setBadgeText: function(details) {
          browserAction.setPropertyFromDetails(
            details,
            "badgeText",
            details.text
          );
        },

        getBadgeText: function(details) {
          return browserAction.getPropertyFromDetails(details, "badgeText");
        },

        setPopup: function(details) {
          // Note: Chrome resolves arguments to setIcon relative to the calling
          // context, but resolves arguments to setPopup relative to the extension
          // root.
          // For internal consistency, we currently resolve both relative to the
          // calling context.
          let url = details.popup && context.uri.resolve(details.popup);
          if (url && !context.checkLoadURL(url)) {
            return Promise.reject({ message: `Access denied for URL ${url}` });
          }
          browserAction.setPropertyFromDetails(details, "popup", url);
        },

        getPopup: function(details) {
          return browserAction.getPropertyFromDetails(details, "popup");
        },

        setBadgeBackgroundColor: function(details) {
          let color = parseColor(details.color, "background");
          let values = browserAction.setPropertyFromDetails(
            details,
            "badgeBackgroundColor",
            color
          );
          if (color === null) {
            // Let the default text color inherit after removing background color
            delete values.badgeDefaultColor;
          } else {
            // Invalidate a cached default color calculated with the old background
            values.badgeDefaultColor = null;
          }
        },

        getBadgeBackgroundColor: function(details, callback) {
          return browserAction.getPropertyFromDetails(
            details,
            "badgeBackgroundColor"
          );
        },

        setBadgeTextColor: function(details) {
          let color = parseColor(details.color, "text");
          browserAction.setPropertyFromDetails(
            details,
            "badgeTextColor",
            color
          );
        },

        getBadgeTextColor: function(details) {
          let target = browserAction.getTargetFromDetails(details);
          let values = browserAction.getContextData(target);
          return browserAction.getTextColor(values);
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
