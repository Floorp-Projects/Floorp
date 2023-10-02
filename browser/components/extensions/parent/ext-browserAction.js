/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  BrowserUsageTelemetry: "resource:///modules/BrowserUsageTelemetry.sys.mjs",
  CustomizableUI: "resource:///modules/CustomizableUI.sys.mjs",
  ExtensionTelemetry: "resource://gre/modules/ExtensionTelemetry.sys.mjs",
  OriginControls: "resource://gre/modules/ExtensionPermissions.sys.mjs",
  ViewPopup: "resource:///modules/ExtensionPopups.sys.mjs",
  clearTimeout: "resource://gre/modules/Timer.sys.mjs",
  setTimeout: "resource://gre/modules/Timer.sys.mjs",
});

var { DefaultWeakMap, ExtensionError } = ExtensionUtils;

var { ExtensionParent } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionParent.sys.mjs"
);
var { BrowserActionBase } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionActions.sys.mjs"
);

var { IconDetails, StartupCache } = ExtensionParent;

const POPUP_PRELOAD_TIMEOUT_MS = 200;

// WeakMap[Extension -> BrowserAction]
const browserActionMap = new WeakMap();

ChromeUtils.defineLazyGetter(this, "browserAreas", () => {
  return {
    navbar: CustomizableUI.AREA_NAVBAR,
    menupanel: CustomizableUI.AREA_ADDONS,
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
    this.viewId = `PanelUI-webext-${widgetId}-BAV`;
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
    let { extension } = this;
    let widgetId = makeWidgetId(extension.id);
    let widget = CustomizableUI.createWidget({
      id: this.id,
      viewId: this.viewId,
      type: "custom",
      webExtension: true,
      removable: true,
      label: this.action.getProperty(null, "title"),
      tooltiptext: this.action.getProperty(null, "title"),
      defaultArea: browserAreas[this.action.getDefaultArea()],
      showInPrivateBrowsing: extension.privateBrowsingAllowed,
      disallowSubView: true,

      // Don't attempt to load properties from the built-in widget string
      // bundle.
      localized: false,

      // Build a custom widget that looks like a `unified-extensions-item`
      // custom element.
      onBuild(document) {
        let viewId = widgetId + "-BAP";
        let button = document.createXULElement("toolbarbutton");
        button.setAttribute("id", viewId);
        // Ensure the extension context menuitems are available by setting this
        // on all button children and the item.
        button.setAttribute("data-extensionid", extension.id);
        button.classList.add("unified-extensions-item-action-button");

        let contents = document.createXULElement("vbox");
        contents.classList.add("unified-extensions-item-contents");
        contents.setAttribute("move-after-stack", "true");

        let name = document.createXULElement("label");
        name.classList.add("unified-extensions-item-name");
        contents.appendChild(name);

        // This deck (and its labels) should be kept in sync with
        // `browser/base/content/unified-extensions-viewcache.inc.xhtml`.
        let deck = document.createXULElement("deck");
        deck.classList.add("unified-extensions-item-message-deck");

        let messageDefault = document.createXULElement("label");
        messageDefault.classList.add(
          "unified-extensions-item-message",
          "unified-extensions-item-message-default"
        );
        deck.appendChild(messageDefault);

        let messageHover = document.createXULElement("label");
        messageHover.classList.add(
          "unified-extensions-item-message",
          "unified-extensions-item-message-hover"
        );
        deck.appendChild(messageHover);

        let messageHoverForMenuButton = document.createXULElement("label");
        messageHoverForMenuButton.classList.add(
          "unified-extensions-item-message",
          "unified-extensions-item-message-hover-menu-button"
        );
        document.l10n.setAttributes(
          messageHoverForMenuButton,
          "unified-extensions-item-message-manage"
        );
        deck.appendChild(messageHoverForMenuButton);

        contents.appendChild(deck);

        button.appendChild(contents);

        let menuButton = document.createXULElement("toolbarbutton");
        menuButton.classList.add(
          "toolbarbutton-1",
          "unified-extensions-item-menu-button"
        );

        document.l10n.setAttributes(
          menuButton,
          "unified-extensions-item-open-menu"
        );
        // Allow the users to quickly move between extension items using
        // the arrow keys, see: `PanelMultiView._isNavigableWithTabOnly()`.
        menuButton.setAttribute("data-navigable-with-tab-only", true);

        menuButton.setAttribute("data-extensionid", extension.id);
        menuButton.setAttribute("closemenu", "none");

        let node = document.createXULElement("toolbaritem");
        node.classList.add(
          "toolbaritem-combined-buttons",
          "unified-extensions-item"
        );
        node.setAttribute("view-button-id", viewId);
        node.setAttribute("data-extensionid", extension.id);
        node.append(button, menuButton);
        node.viewButton = button;

        return node;
      },

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
        let actionButton = node.querySelector(
          ".unified-extensions-item-action-button"
        );
        actionButton.classList.add("panel-no-padding");
        actionButton.classList.add("webextension-browser-action");
        actionButton.setAttribute("badged", "true");
        actionButton.setAttribute("constrain-size", "true");
        actionButton.setAttribute("data-extensionid", this.extension.id);

        actionButton.onmousedown = event => this.handleEvent(event);
        actionButton.onmouseover = event => this.handleEvent(event);
        actionButton.onmouseout = event => this.handleEvent(event);
        actionButton.onauxclick = event => this.handleEvent(event);

        const menuButton = node.querySelector(
          ".unified-extensions-item-menu-button"
        );
        node.ownerDocument.l10n.setAttributes(
          menuButton,
          "unified-extensions-item-open-menu",
          { extensionName: this.extension.name }
        );

        menuButton.onblur = event => this.handleMenuButtonEvent(event);
        menuButton.onfocus = event => this.handleMenuButtonEvent(event);
        menuButton.onmouseout = event => this.handleMenuButtonEvent(event);
        menuButton.onmouseover = event => this.handleMenuButtonEvent(event);

        actionButton.onblur = event => this.handleEvent(event);
        actionButton.onfocus = event => this.handleEvent(event);

        this.updateButton(
          node,
          this.action.getContextData(null),
          /* sync */ true
        );
      },

      onBeforeCommand: (event, node) => {
        this.lastClickInfo = {
          button: event.button || 0,
          modifiers: clickModifiersFromEvent(event),
        };

        // The openPopupWithoutUserInteraction flag may be set by openPopup.
        this.openPopupWithoutUserInteraction =
          event.detail?.openPopupWithoutUserInteraction === true;

        if (
          event.target.classList.contains(
            "unified-extensions-item-action-button"
          )
        ) {
          return "view";
        } else if (
          event.target.classList.contains("unified-extensions-item-menu-button")
        ) {
          return "command";
        }
      },

      onCommand: event => {
        const { target } = event;

        if (event.button !== 0) {
          return;
        }

        // Open the unified extensions context menu.
        const popup = target.ownerDocument.getElementById(
          "unified-extensions-context-menu"
        );
        // Anchor to the visible part of the button.
        const anchor = target.firstElementChild;
        popup.openPopup(
          anchor,
          "after_end",
          0,
          0,
          true /* isContextMenu */,
          false /* attributesOverride */,
          event
        );
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

        let popupURL = !this.openPopupWithoutUserInteraction
          ? this.action.triggerClickOrPopup(tab, this.lastClickInfo)
          : this.action.getPopupUrl(tab);

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
   * Shows the popup. The caller is expected to check if a popup is set before
   * this is called.
   *
   * @param {Window} window Window to show the popup for
   * @param {boolean} openPopupWithoutUserInteraction
   *        If the popup was opened without user interaction
   */
  async openPopup(window, openPopupWithoutUserInteraction = false) {
    const widgetForWindow = this.widget.forWindow(window);

    if (!widgetForWindow.node) {
      return;
    }

    // We want to focus hidden or minimized windows (both for the API, and to
    // avoid an issue where showing the popup in a non-focused window
    // immediately triggers a popuphidden event)
    window.focus();

    if (widgetForWindow.node.firstElementChild.open) {
      return;
    }

    if (this.widget.areaType == CustomizableUI.TYPE_PANEL) {
      await window.gUnifiedExtensions.togglePanel();
    }

    // This should already have been checked by callers, but acts as an
    // an additional safeguard. It also makes sure we don't dispatch a click
    // if the URL is removed while waiting for the overflow to show above.
    if (!this.action.getPopupUrl(window.gBrowser.selectedTab)) {
      return;
    }

    const event = new window.CustomEvent("command", {
      bubbles: true,
      cancelable: true,
      detail: {
        openPopupWithoutUserInteraction,
      },
    });
    widgetForWindow.node.firstElementChild.dispatchEvent(event);
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
  triggerAction(window) {
    let popup = ViewPopup.for(this.extension, window);
    if (!this.pendingPopup && popup) {
      popup.closePopup();
      return;
    }

    let tab = window.gBrowser.selectedTab;

    let popupUrl = this.action.triggerClickOrPopup(tab, {
      button: 0,
      modifiers: [],
    });
    if (popupUrl) {
      this.openPopup(window);
    }
  }

  /**
   * Handles events on the (secondary) menu/cog button in an extension widget.
   *
   * @param {Event} event
   */
  handleMenuButtonEvent(event) {
    let window = event.target.ownerGlobal;
    let { node } = window.gBrowser && this.widget.forWindow(window);
    let messageDeck = node?.querySelector(
      ".unified-extensions-item-message-deck"
    );

    switch (event.type) {
      case "focus":
      case "mouseover": {
        if (messageDeck) {
          messageDeck.selectedIndex =
            window.gUnifiedExtensions.MESSAGE_DECK_INDEX_MENU_HOVER;
        }
        break;
      }

      case "blur":
      case "mouseout": {
        if (messageDeck) {
          messageDeck.selectedIndex =
            window.gUnifiedExtensions.MESSAGE_DECK_INDEX_DEFAULT;
        }
        break;
      }
    }
  }

  handleEvent(event) {
    // This button is the action/primary button in the custom widget.
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

      case "focus":
      case "mouseover": {
        let tab = window.gBrowser.selectedTab;
        let popupURL = this.action.getPopupUrl(tab);

        let { node } = window.gBrowser && this.widget.forWindow(window);
        if (node) {
          node.querySelector(
            ".unified-extensions-item-message-deck"
          ).selectedIndex = window.gUnifiedExtensions.MESSAGE_DECK_INDEX_HOVER;
        }

        // We don't want to preload the popup on focus (for now).
        if (event.type === "focus") {
          break;
        }

        // Begin pre-loading the browser for the popup, so it's more likely to
        // be ready by the time we get a complete click.
        if (
          popupURL &&
          (this.pendingPopup || !ViewPopup.for(this.extension, window))
        ) {
          this.eventQueue.push("Hover");
          this.pendingPopup = this.getPopup(window, popupURL, true);
        }
        break;
      }

      case "blur":
      case "mouseout": {
        let { node } = window.gBrowser && this.widget.forWindow(window);
        if (node) {
          node.querySelector(
            ".unified-extensions-item-message-deck"
          ).selectedIndex =
            window.gUnifiedExtensions.MESSAGE_DECK_INDEX_DEFAULT;
        }

        // We don't want to clear the popup on blur for now.
        if (event.type === "blur") {
          break;
        }

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
      }

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
  updateButton(
    node,
    tabData,
    sync = false,
    attention = false,
    quarantined = false
  ) {
    // This is the primary/action button in the custom widget.
    let button = node.querySelector(".unified-extensions-item-action-button");
    let extensionTitle = tabData.title || this.extension.name;

    let policy = WebExtensionPolicy.getByID(this.extension.id);
    let messages = OriginControls.getStateMessageIDs({
      policy,
      tab: node.ownerGlobal.gBrowser.selectedTab,
      isAction: true,
      hasPopup: !!tabData.popup,
    });

    let callback = () => {
      // This is set on the node so that it looks good in the toolbar.
      node.toggleAttribute("attention", attention);

      let msgId = "origin-controls-toolbar-button";
      if (attention) {
        msgId = quarantined
          ? "origin-controls-toolbar-button-quarantined"
          : "origin-controls-toolbar-button-permission-needed";
      }
      node.ownerDocument.l10n.setAttributes(button, msgId, { extensionTitle });

      button.querySelector(".unified-extensions-item-name").textContent =
        this.extension?.name;

      if (messages) {
        const messageDefaultElement = button.querySelector(
          ".unified-extensions-item-message-default"
        );
        node.ownerDocument.l10n.setAttributes(
          messageDefaultElement,
          messages.default
        );

        const messageHoverElement = button.querySelector(
          ".unified-extensions-item-message-hover"
        );
        node.ownerDocument.l10n.setAttributes(
          messageHoverElement,
          messages.onHover || messages.default
        );
      }

      if (tabData.badgeText) {
        button.setAttribute("badge", tabData.badgeText);
      } else {
        button.removeAttribute("badge");
      }

      if (tabData.enabled) {
        button.removeAttribute("disabled");
      } else {
        button.setAttribute("disabled", "true");
      }

      let serializeColor = ([r, g, b, a]) =>
        `rgba(${r}, ${g}, ${b}, ${a / 255})`;
      button.setAttribute(
        "badgeStyle",
        [
          `background-color: ${serializeColor(tabData.badgeBackgroundColor)}`,
          `color: ${serializeColor(this.action.getTextColor(tabData))}`,
        ].join("; ")
      );

      let style = this.iconData.get(tabData.icon);
      button.setAttribute("style", style);
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

    let getStyle = (name, icon1x, icon2x) => {
      return `
        --webextension-${name}: image-set(
          url("${getIcon(icon1x, "default")}"),
          url("${getIcon(icon2x, "default")}") 2x
        );
        --webextension-${name}-light: image-set(
          url("${getIcon(icon1x, "light")}"),
          url("${getIcon(icon2x, "light")}") 2x
        );
        --webextension-${name}-dark: image-set(
          url("${getIcon(icon1x, "dark")}"),
          url("${getIcon(icon2x, "dark")}") 2x
        );
      `;
    };

    let icon16 = IconDetails.getPreferredIcon(icons, this.extension, 16).icon;
    let icon32 = IconDetails.getPreferredIcon(icons, this.extension, 32).icon;
    let icon64 = IconDetails.getPreferredIcon(icons, this.extension, 64).icon;

    return `
        ${getStyle("menupanel-image", icon32, icon64)}
        ${getStyle("toolbar-image", icon16, icon32)}
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
      let { attention, quarantined } = OriginControls.getAttentionState(
        this.extension.policy,
        window
      );

      this.updateButton(
        node,
        this.action.getContextData(tab),
        /* sync */ false,
        attention,
        quarantined
      );
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

        getUserSettings: () => {
          let { area } = CustomizableUI.getPlacementOfWidget(
            action.buttonDelegate.id
          );
          return { isOnToolbar: area !== CustomizableUI.AREA_ADDONS };
        },
        openPopup: async options => {
          const isHandlingUserInput =
            context.callContextData?.isHandlingUserInput;

          if (
            !Services.prefs.getBoolPref(
              "extensions.openPopupWithoutUserGesture.enabled"
            ) &&
            !isHandlingUserInput
          ) {
            throw new ExtensionError("openPopup requires a user gesture");
          }

          const window =
            typeof options?.windowId === "number"
              ? windowTracker.getWindow(options.windowId, context)
              : windowTracker.getTopNormalWindow(context);

          if (this.action.getPopupUrl(window.gBrowser.selectedTab, true)) {
            await this.openPopup(window, !isHandlingUserInput);
          }
        },
      },
    };
  }
};

global.browserActionFor = this.browserAction.for;
