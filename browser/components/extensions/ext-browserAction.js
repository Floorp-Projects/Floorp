/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

/* exported browserActionFor, sidebarActionFor, pageActionFor */
/* global browserActionFor:false, sidebarActionFor:false, pageActionFor:false */

// The ext-* files are imported into the same scopes.
/* import-globals-from ext-utils.js */

XPCOMUtils.defineLazyModuleGetter(this, "CustomizableUI",
                                  "resource:///modules/CustomizableUI.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "clearTimeout",
                                  "resource://gre/modules/Timer.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "setTimeout",
                                  "resource://gre/modules/Timer.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ViewPopup",
                                  "resource:///modules/ExtensionPopups.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "DOMUtils",
                                   "@mozilla.org/inspector/dom-utils;1",
                                   "inIDOMUtils");

Cu.import("resource://gre/modules/EventEmitter.jsm");

XPCOMUtils.defineLazyPreferenceGetter(this, "gPhotonStructure", "browser.photon.structure.enabled");

var {
  DefaultWeakMap,
} = ExtensionUtils;

Cu.import("resource://gre/modules/ExtensionParent.jsm");

var {
  IconDetails,
} = ExtensionParent;

const POPUP_PRELOAD_TIMEOUT_MS = 200;

var XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

function isAncestorOrSelf(target, node) {
  for (; node; node = node.parentNode) {
    if (node === target) {
      return true;
    }
  }
  return false;
}

// WeakMap[Extension -> BrowserAction]
const browserActionMap = new WeakMap();

XPCOMUtils.defineLazyGetter(this, "browserAreas", () => {
  return {
    "navbar": CustomizableUI.AREA_NAVBAR,
    "menupanel": gPhotonStructure ? CustomizableUI.AREA_FIXED_OVERFLOW_PANEL : CustomizableUI.AREA_PANEL,
    "tabstrip": CustomizableUI.AREA_TABSTRIP,
    "personaltoolbar": CustomizableUI.AREA_BOOKMARKS,
  };
});

this.browserAction = class extends ExtensionAPI {
  static for(extension) {
    return browserActionMap.get(extension);
  }

  onManifestEntry(entryName) {
    let {extension} = this;

    let options = extension.manifest.browser_action;

    this.iconData = new DefaultWeakMap(icons => this.getIconData(icons));

    let widgetId = makeWidgetId(extension.id);
    this.id = `${widgetId}-browser-action`;
    this.viewId = `PanelUI-webext-${widgetId}-browser-action-view`;
    this.widget = null;

    this.pendingPopup = null;
    this.pendingPopupTimeout = null;

    this.tabManager = extension.tabManager;

    this.defaults = {
      enabled: true,
      title: options.default_title || extension.name,
      badgeText: "",
      badgeBackgroundColor: null,
      icon: IconDetails.normalize({path: options.default_icon}, extension),
      popup: options.default_popup || "",
      area: browserAreas[options.default_area || "navbar"],
    };

    this.browserStyle = options.browser_style || false;
    if (options.browser_style === null) {
      this.extension.logger.warn("Please specify whether you want browser_style " +
                                 "or not in your browser_action options.");
    }

    this.tabContext = new TabContext(tab => Object.create(this.defaults),
                                     extension);

    EventEmitter.decorate(this);

    this.build();
    browserActionMap.set(extension, this);
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

      onBeforeCreated: document => {
        let view = document.createElementNS(XUL_NS, "panelview");
        view.id = this.viewId;
        view.setAttribute("flex", "1");
        view.setAttribute("extension", true);

        document.getElementById("PanelUI-multiView").appendChild(view);
        document.addEventListener("popupshowing", this);
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

        this.updateButton(node, this.defaults);
      },

      onViewShowing: event => {
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
            // FIXME: The line below needs to change eventually, but for now:
            // ensure the view is _always_ visible _before_ `popup.attach()` is
            // called. PanelMultiView.jsm dictates different behavior.
            event.target.setAttribute("current", true);
            let popup = this.getPopup(document.defaultView, popupURL);
            event.detail.addBlocker(popup.attach(event.target));
          } catch (e) {
            Cu.reportError(e);
            event.preventDefault();
          }
        } else {
          // This isn't not a hack, but it seems to provide the correct behavior
          // with the fewest complications.
          event.preventDefault();
          this.emit("click");
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
        if (gPhotonStructure) {
          await window.document.getElementById("nav-bar").overflowable.show();
        } else {
          await window.PanelUI.show();
        }
      }

      let event = new window.CustomEvent("command", {bubbles: true, cancelable: true});
      widget.node.dispatchEvent(event);
    } else {
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
          this.pendingPopup = this.getPopup(window, popupURL, true);
        }
        break;
      }

      case "mouseout":
        if (this.pendingPopup) {
          this.clearPopup();
        }
        break;


      case "popupshowing":
        if (!global.actionContextMenu) {
          break;
        }

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
  updateButton(node, tabData) {
    let title = tabData.title || this.extension.name;

    node.ownerGlobal.requestAnimationFrame(() => {
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

      let badgeNode = node.ownerDocument.getAnonymousElementByAttribute(node,
                                          "class", "toolbarbutton-badge");
      if (badgeNode) {
        let color = tabData.badgeBackgroundColor;
        if (color) {
          color = `rgba(${color[0]}, ${color[1]}, ${color[2]}, ${color[3] / 255})`;
        }
        badgeNode.style.backgroundColor = color || "";
      }

      let {style, legacy} = this.iconData.get(tabData.icon);
      const LEGACY_CLASS = "toolbarbutton-legacy-addon";
      if (legacy) {
        node.classList.add(LEGACY_CLASS);
      } else {
        node.classList.remove(LEGACY_CLASS);
      }

      node.setAttribute("style", style);
    });
  }

  getIconData(icons) {
    let baseSize = 16;
    let {icon, size} = IconDetails.getPreferredIcon(icons, this.extension, baseSize);

    let legacy = false;

    // If the best available icon size is not divisible by 16, check if we have
    // an 18px icon to fall back to, and trim off the padding instead.
    if (size % 16 && !icon.endsWith(".svg")) {
      let result = IconDetails.getPreferredIcon(icons, this.extension, 18);

      if (result.size % 18 == 0) {
        baseSize = 18;
        icon = result.icon;
        legacy = true;
      }
    }

    let getIcon = size => IconDetails.escapeUrl(
      IconDetails.getPreferredIcon(icons, this.extension, size).icon);

    let style = `
      --webextension-menupanel-image: url("${getIcon(32)}");
      --webextension-menupanel-image-2x: url("${getIcon(64)}");
      --webextension-toolbar-image: url("${IconDetails.escapeUrl(icon)}");
      --webextension-toolbar-image-2x: url("${getIcon(baseSize * 2)}");
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
    if (tab == null) {
      this.defaults[prop] = value;
    } else if (value != null) {
      this.tabContext.get(tab)[prop] = value;
    } else {
      delete this.tabContext.get(tab)[prop];
    }

    this.updateOnChange(tab);
  }

  // tab is allowed to be null.
  // prop should be one of "title", "badgeText", "popup", or "badgeBackgroundColor".
  getProperty(tab, prop) {
    if (tab == null) {
      return this.defaults[prop];
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
        onClicked: new SingletonEventManager(context, "browserAction.onClicked", fire => {
          let listener = () => {
            fire.async(tabManager.convert(tabTracker.activeTab));
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

        setTitle: function(details) {
          let tab = getTab(details.tabId);

          let title = details.title;
          // Clear the tab-specific title when given a null string.
          if (tab && title == "") {
            title = null;
          }
          browserAction.setProperty(tab, "title", title);
        },

        getTitle: function(details) {
          let tab = getTab(details.tabId);

          let title = browserAction.getProperty(tab, "title");
          return Promise.resolve(title);
        },

        setIcon: function(details) {
          let tab = getTab(details.tabId);

          let icon = IconDetails.normalize(details, extension, context);
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
            let col = DOMUtils.colorToRGBA(color);
            color = col && [col.r, col.g, col.b, Math.round(col.a * 255)];
          }
          browserAction.setProperty(tab, "badgeBackgroundColor", color);
        },

        getBadgeBackgroundColor: function(details, callback) {
          let tab = getTab(details.tabId);

          let color = browserAction.getProperty(tab, "badgeBackgroundColor");
          return Promise.resolve(color || [0xd9, 0, 0, 255]);
        },
      },
    };
  }
};

global.browserActionFor = this.browserAction.for;
