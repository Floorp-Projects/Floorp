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
ChromeUtils.defineModuleGetter(
  this,
  "BrowserUsageTelemetry",
  "resource:///modules/BrowserUsageTelemetry.jsm"
);

var { DefaultWeakMap } = ExtensionUtils;

var { ExtensionParent } = ChromeUtils.import(
  "resource://gre/modules/ExtensionParent.jsm"
);
var { BrowserActionBase } = ChromeUtils.import(
  "resource://gre/modules/ExtensionActions.jsm"
);

var { IconDetails, StartupCache } = ExtensionParent;

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

function actionWidgetId(widgetId) {
  return `${widgetId}-browser-action`;
}

class BrowserAction extends BrowserActionBase {
  constructor(extension, buttonDelegate) {
    let tabContext = new TabContext(target => {
      let window = target.ownerGlobal;
      if (target === window) {
        return this.getContextData(null);
      }
      return tabContext.get(window);
    });
    super(tabContext, extension);
    this.buttonDelegate = buttonDelegate;
  }

  updateOnChange(target) {
    if (target) {
      let window = target.ownerGlobal;
      if (target === window || target.selected) {
        this.buttonDelegate.updateWindow(window);
      }
    } else {
      for (let window of windowTracker.browserWindows()) {
        this.buttonDelegate.updateWindow(window);
      }
    }
  }

  getTab(tabId) {
    if (tabId !== null) {
      return tabTracker.getTab(tabId);
    }
    return null;
  }

  getWindow(windowId) {
    if (windowId !== null) {
      return windowTracker.getWindow(windowId);
    }
    return null;
  }

  dispatchClick(tab, clickInfo) {
    this.buttonDelegate.emit("click", tab, clickInfo);
  }
}

this.browserAction = class extends ExtensionAPIPersistent {
  static for(extension) {
    return browserActionMap.get(extension);
  }

  async onManifestEntry(entryName) {
    let { extension } = this;

    let options =
      extension.manifest.browser_action || extension.manifest.action;

    this.action = new BrowserAction(extension, this);
    await this.action.loadIconData();

    this.iconData = new DefaultWeakMap(icons => this.getIconData(icons));
    this.iconData.set(
      this.action.getIcon(),
      await StartupCache.get(
        extension,
        ["browserAction", "default_icon_data"],
        () => this.getIconData(this.action.getIcon())
      )
    );

    let widgetId = makeWidgetId(extension.id);
    this.id = actionWidgetId(widgetId);
    this.viewId = `PanelUI-webext-${widgetId}-browser-action-view`;
    this.widget = null;

    this.pendingPopup = null;
    this.pendingPopupTimeout = null;
    this.eventQueue = [];

    this.tabManager = extension.tabManager;
    this.browserStyle = options.browser_style;

    browserActionMap.set(extension, this);

    this.build();
  }

  static onUpdate(id, manifest) {
    if (!("browser_action" in manifest || "action" in manifest)) {
      // If the new version has no browser action then mark this widget as
      // hidden in the telemetry. If it is already marked hidden then this will
      // do nothing.
      BrowserUsageTelemetry.recordWidgetChange(
        actionWidgetId(makeWidgetId(id)),
        null,
        "addon"
      );
    }
  }

  static onDisable(id) {
    BrowserUsageTelemetry.recordWidgetChange(
      actionWidgetId(makeWidgetId(id)),
      null,
      "addon"
    );
  }

  static onUninstall(id) {
    // If the telemetry already has this widget as hidden then this will not
    // record anything.
    BrowserUsageTelemetry.recordWidgetChange(
      actionWidgetId(makeWidgetId(id)),
      null,
      "addon"
    );
  }

  onShutdown() {
    browserActionMap.delete(this.extension);
    this.action.onShutdown();

    CustomizableUI.destroyWidget(this.id);

    this.clearPopup();
  }

  build() {
    let widget = CustomizableUI.createWidget({
      id: this.id,
      viewId: this.viewId,
      type: "view",
      removable: true,
      label: this.action.getProperty(null, "title"),
      tooltiptext: this.action.getProperty(null, "title"),
      defaultArea: browserAreas[this.action.getDefaultArea()],
      showInPrivateBrowsing: this.extension.privateBrowsingAllowed,
      disallowSubView: true,

      // Don't attempt to load properties from the built-in widget string
      // bundle.
      localized: false,

      onBeforeCreated: document => {
        let view = document.createXULElement("panelview");
        view.id = this.viewId;
        view.setAttribute("flex", "1");
        view.setAttribute("extension", true);
        view.setAttribute("neverhidden", true);
        view.setAttribute("disallowSubView", true);

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
        node.onauxclick = event => this.handleEvent(event);

        this.updateButton(node, this.action.getContextData(null), true);
      },

      onBeforeCommand: event => {
        this.lastClickInfo = {
          button: event.button || 0,
          modifiers: clickModifiersFromEvent(event),
        };
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
        let popupURL = this.action.triggerClickOrPopup(tab, this.lastClickInfo);

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
          // Ensure we close any popups this node was in:
          CustomizableUI.hidePanelForNode(event.target);
        }
      },
    });

    if (this.extension.startupReason != "APP_STARTUP") {
      // Make sure the browser telemetry has the correct state for this widget.
      // Defer loading BrowserUsageTelemetry until after startup is complete.
      ExtensionParent.browserStartupPromise.then(() => {
        let placement = CustomizableUI.getPlacementOfWidget(widget.id);
        BrowserUsageTelemetry.recordWidgetChange(
          widget.id,
          placement?.area || null,
          "addon"
        );
      });
    }

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

    if (!widget.node) {
      return;
    }

    let popupUrl = this.action.triggerClickOrPopup(tab, {
      button: 0,
      modifiers: [],
    });
    if (popupUrl) {
      if (this.widget.areaType == CustomizableUI.TYPE_MENU_PANEL) {
        await window.document.getElementById("nav-bar").overflowable.show();
      }

      let event = new window.CustomEvent("command", {
        bubbles: true,
        cancelable: true,
      });
      widget.node.dispatchEvent(event);
    }
  }

  handleEvent(event) {
    let button = event.target;
    let window = button.ownerGlobal;

    switch (event.type) {
      case "mousedown":
        if (event.button == 0) {
          let tab = window.gBrowser.selectedTab;

          // Begin pre-loading the browser for the popup, so it's more likely to
          // be ready by the time we get a complete click.
          let popupURL = this.action.getPopupUrl(tab);
          if (
            popupURL &&
            (this.pendingPopup || !ViewPopup.for(this.extension, window))
          ) {
            // Add permission for the active tab so it will exist for the popup.
            this.action.setActiveTabForPreload(tab);
            this.eventQueue.push("Mousedown");
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
        let popupURL = this.action.getPopupUrl(tab);

        if (
          popupURL &&
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
          this.updateContextMenu(menu);
        }
        break;

      case "auxclick":
        if (event.button !== 1) {
          return;
        }

        let tab = window.gBrowser.selectedTab;
        if (this.action.getProperty(tab, "enabled")) {
          this.action.setActiveTabForPreload(null);
          this.tabManager.addActiveTabPermission(tab);
          this.action.dispatchClick(tab, {
            button: 1,
            modifiers: clickModifiersFromEvent(event),
          });
          // Ensure we close any popups this node was in:
          CustomizableUI.hidePanelForNode(event.target);
        }
        break;
    }
  }

  /**
   * Updates the given context menu with the extension's actions.
   *
   * @param {Element} menu
   *        The context menu element that should be updated.
   */
  updateContextMenu(menu) {
    const action =
      this.extension.manifestVersion < 3 ? "onBrowserAction" : "onAction";

    global.actionContextMenu({
      extension: this.extension,
      [action]: true,
      menu,
    });
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

    return new ViewPopup(
      this.extension,
      window,
      popupURL,
      this.browserStyle,
      false,
      blockParser
    );
  }

  /**
   * Clears any pending pre-loaded popup and related timeouts.
   */
  clearPopup() {
    this.clearPopupTimeout();
    this.action.setActiveTabForPreload(null);
    if (this.pendingPopup) {
      this.pendingPopup.destroy();
      this.pendingPopup = null;
    }
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
          `color: ${serializeColor(this.action.getTextColor(tabData))}`,
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
      this.updateButton(node, this.action.getContextData(tab));
    }
  }

  PERSISTENT_EVENTS = {
    onClicked({ context, fire }) {
      const { extension } = this;
      const { tabManager } = extension;
      async function listener(_event, tab, clickInfo) {
        if (fire.wakeup) {
          await fire.wakeup();
        }
        // TODO: we should double-check if the tab is already being closed by the time
        // the background script got started and we converted the primed listener.
        context?.withPendingBrowser(tab.linkedBrowser, () =>
          fire.sync(tabManager.convert(tab), clickInfo)
        );
      }
      this.on("click", listener);
      return {
        unregister: () => {
          this.off("click", listener);
        },
        convert(newFire, extContext) {
          fire = newFire;
          context = extContext;
        },
      };
    },
  };

  getAPI(context) {
    let { extension } = context;
    let { action } = this;
    let namespace = extension.manifestVersion < 3 ? "browserAction" : "action";

    return {
      [namespace]: {
        ...action.api(context),

        onClicked: new EventManager({
          context,
          // module name is "browserAction" because it the name used in the
          // ext-browser.json, indipendently from the manifest version.
          module: "browserAction",
          event: "onClicked",
          inputHandling: true,
          extensionApi: this,
        }).api(),

        openPopup: () => {
          let window = windowTracker.topWindow;
          this.triggerAction(window);
        },
      },
    };
  }
};

global.browserActionFor = this.browserAction.for;
