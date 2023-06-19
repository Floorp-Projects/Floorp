/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  BrowserUsageTelemetry: "resource:///modules/BrowserUsageTelemetry.sys.mjs",
  ExtensionTelemetry: "resource://gre/modules/ExtensionTelemetry.sys.mjs",
  PageActions: "resource:///modules/PageActions.sys.mjs",
  PanelPopup: "resource:///modules/ExtensionPopups.sys.mjs",
});

var { DefaultWeakMap } = ExtensionUtils;

var { ExtensionParent } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionParent.sys.mjs"
);
var { PageActionBase } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionActions.sys.mjs"
);

// WeakMap[Extension -> PageAction]
let pageActionMap = new WeakMap();

class PageAction extends PageActionBase {
  constructor(extension, buttonDelegate) {
    let tabContext = new TabContext(tab => this.getContextData(null));
    super(tabContext, extension);
    this.buttonDelegate = buttonDelegate;
  }

  updateOnChange(target) {
    this.buttonDelegate.updateButton(target.ownerGlobal);
  }

  dispatchClick(tab, clickInfo) {
    this.buttonDelegate.emit("click", tab, clickInfo);
  }

  getTab(tabId) {
    if (tabId !== null) {
      return tabTracker.getTab(tabId);
    }
    return null;
  }
}

this.pageAction = class extends ExtensionAPIPersistent {
  static for(extension) {
    return pageActionMap.get(extension);
  }

  static onUpdate(id, manifest) {
    if (!("page_action" in manifest)) {
      // If the new version has no page action then mark this widget as hidden
      // in the telemetry. If it is already marked hidden then this will do
      // nothing.
      BrowserUsageTelemetry.recordWidgetChange(makeWidgetId(id), null, "addon");
    }
  }

  static onDisable(id) {
    BrowserUsageTelemetry.recordWidgetChange(makeWidgetId(id), null, "addon");
  }

  static onUninstall(id) {
    // If the telemetry already has this widget as hidden then this will not
    // record anything.
    BrowserUsageTelemetry.recordWidgetChange(makeWidgetId(id), null, "addon");
  }

  async onManifestEntry(entryName) {
    let { extension } = this;
    let options = extension.manifest.page_action;

    this.action = new PageAction(extension, this);
    await this.action.loadIconData();

    let widgetId = makeWidgetId(extension.id);
    this.id = widgetId + "-page-action";

    this.tabManager = extension.tabManager;

    this.browserStyle = options.browser_style;

    pageActionMap.set(extension, this);

    this.lastValues = new DefaultWeakMap(() => ({}));

    if (!this.browserPageAction) {
      let onPlacedHandler = (buttonNode, isPanel) => {
        // eslint-disable-next-line mozilla/balanced-listeners
        buttonNode.addEventListener("auxclick", event => {
          if (event.button !== 1 || event.target.disabled) {
            return;
          }

          // The panel is not automatically closed when middle-clicked.
          if (isPanel) {
            buttonNode.closest("#pageActionPanel").hidePopup();
          }
          let window = event.target.ownerGlobal;
          let tab = window.gBrowser.selectedTab;
          this.tabManager.addActiveTabPermission(tab);
          this.action.dispatchClick(tab, {
            button: event.button,
            modifiers: clickModifiersFromEvent(event),
          });
        });
      };

      this.browserPageAction = PageActions.addAction(
        new PageActions.Action({
          id: widgetId,
          extensionID: extension.id,
          title: this.action.getProperty(null, "title"),
          iconURL: this.action.getProperty(null, "icon"),
          pinnedToUrlbar: this.action.getPinned(),
          disabled: !this.action.getProperty(null, "enabled"),
          onCommand: (event, buttonNode) => {
            this.handleClick(event.target.ownerGlobal, {
              button: event.button || 0,
              modifiers: clickModifiersFromEvent(event),
            });
          },
          onBeforePlacedInWindow: browserWindow => {
            if (
              this.extension.hasPermission("menus") ||
              this.extension.hasPermission("contextMenus")
            ) {
              browserWindow.document.addEventListener("popupshowing", this);
            }
          },
          onPlacedInPanel: buttonNode => onPlacedHandler(buttonNode, true),
          onPlacedInUrlbar: buttonNode => onPlacedHandler(buttonNode, false),
          onRemovedFromWindow: browserWindow => {
            browserWindow.document.removeEventListener("popupshowing", this);
          },
        })
      );

      if (this.extension.startupReason != "APP_STARTUP") {
        // Make sure the browser telemetry has the correct state for this widget.
        // Defer loading BrowserUsageTelemetry until after startup is complete.
        ExtensionParent.browserStartupPromise.then(() => {
          BrowserUsageTelemetry.recordWidgetChange(
            widgetId,
            this.browserPageAction.pinnedToUrlbar
              ? "page-action-buttons"
              : null,
            "addon"
          );
        });
      }

      // If the page action is only enabled in some URLs, do pattern matching in
      // the active tabs and update the button if necessary.
      if (this.action.getProperty(null, "enabled") === undefined) {
        for (let window of windowTracker.browserWindows()) {
          let tab = window.gBrowser.selectedTab;
          if (this.action.isShownForTab(tab)) {
            this.updateButton(window);
          }
        }
      }
    }
  }

  onShutdown(isAppShutdown) {
    pageActionMap.delete(this.extension);
    this.action.onShutdown();

    // Removing the browser page action causes PageActions to forget about it
    // across app restarts, so don't remove it on app shutdown, but do remove
    // it on all other shutdowns since there's no guarantee the action will be
    // coming back.
    if (!isAppShutdown && this.browserPageAction) {
      this.browserPageAction.remove();
      this.browserPageAction = null;
    }
  }

  // Updates the page action button in the given window to reflect the
  // properties of the currently selected tab:
  //
  // Updates "tooltiptext" and "aria-label" to match "title" property.
  // Updates "image" to match the "icon" property.
  // Enables or disables the icon, based on the "enabled" and "patternMatching" properties.
  updateButton(window) {
    let tab = window.gBrowser.selectedTab;
    let tabData = this.action.getContextData(tab);
    let last = this.lastValues.get(window);

    window.requestAnimationFrame(() => {
      // If we get called just before shutdown, we might have been destroyed by
      // this point.
      if (!this.browserPageAction) {
        return;
      }

      let title = tabData.title || this.extension.name;
      if (last.title !== title) {
        this.browserPageAction.setTitle(title, window);
        last.title = title;
      }

      let enabled =
        tabData.enabled != null ? tabData.enabled : tabData.patternMatching;
      if (last.enabled !== enabled) {
        this.browserPageAction.setDisabled(!enabled, window);
        last.enabled = enabled;
      }

      let icon = tabData.icon;
      if (last.icon !== icon) {
        this.browserPageAction.setIconURL(icon, window);
        last.icon = icon;
      }
    });
  }

  /**
   * Triggers this page action for the given window, with the same effects as
   * if it were clicked by a user.
   *
   * This has no effect if the page action is hidden for the selected tab.
   *
   * @param {Window} window
   */
  triggerAction(window) {
    this.handleClick(window, { button: 0, modifiers: [] });
  }

  handleEvent(event) {
    switch (event.type) {
      case "popupshowing":
        const menu = event.target;
        const trigger = menu.triggerNode;
        const getActionId = () => {
          let actionId = trigger.getAttribute("actionid");
          if (actionId) {
            return actionId;
          }
          // When a page action is clicked, triggerNode will be an ancestor of
          // a node corresponding to an action. triggerNode will be the page
          // action node itself when a page action is selected with the
          // keyboard. That's because the semantic meaning of page action is on
          // an hbox that contains an <image>.
          for (let n = trigger; n && !actionId; n = n.parentElement) {
            if (n.id == "page-action-buttons" || n.localName == "panelview") {
              // We reached the page-action-buttons or panelview container.
              // Stop looking; no action was found.
              break;
            }
            actionId = n.getAttribute("actionid");
          }
          return actionId;
        };
        if (
          menu.id === "pageActionContextMenu" &&
          trigger &&
          getActionId() === this.browserPageAction.id &&
          !this.browserPageAction.getDisabled(trigger.ownerGlobal)
        ) {
          global.actionContextMenu({
            extension: this.extension,
            onPageAction: true,
            menu: menu,
          });
        }
        break;
    }
  }

  // Handles a click event on the page action button for the given
  // window.
  // If the page action has a |popup| property, a panel is opened to
  // that URL. Otherwise, a "click" event is emitted, and dispatched to
  // the any click listeners in the add-on.
  async handleClick(window, clickInfo) {
    const { extension } = this;

    ExtensionTelemetry.pageActionPopupOpen.stopwatchStart(extension, this);
    let tab = window.gBrowser.selectedTab;
    let popupURL = this.action.triggerClickOrPopup(tab, clickInfo);

    // If the widget has a popup URL defined, we open a popup, but do not
    // dispatch a click event to the extension.
    // If it has no popup URL defined, we dispatch a click event, but do not
    // open a popup.
    if (popupURL) {
      if (this.popupNode && this.popupNode.panel.state !== "closed") {
        // The panel is being toggled closed.
        ExtensionTelemetry.pageActionPopupOpen.stopwatchCancel(extension, this);
        window.BrowserPageActions.togglePanelForAction(
          this.browserPageAction,
          this.popupNode.panel
        );
        return;
      }

      this.popupNode = new PanelPopup(
        extension,
        window.document,
        popupURL,
        this.browserStyle
      );
      // Remove popupNode when it is closed.
      this.popupNode.panel.addEventListener(
        "popuphiding",
        () => {
          this.popupNode = undefined;
        },
        { once: true }
      );
      await this.popupNode.contentReady;
      window.BrowserPageActions.togglePanelForAction(
        this.browserPageAction,
        this.popupNode.panel
      );
      ExtensionTelemetry.pageActionPopupOpen.stopwatchFinish(extension, this);
    } else {
      ExtensionTelemetry.pageActionPopupOpen.stopwatchCancel(extension, this);
    }
  }

  PERSISTENT_EVENTS = {
    onClicked({ context, fire }) {
      const { extension } = this;
      const { tabManager } = extension;

      let listener = async (_event, tab, clickInfo) => {
        if (fire.wakeup) {
          await fire.wakeup();
        }
        // TODO: we should double-check if the tab is already being closed by the time
        // the background script got started and we converted the primed listener.
        context?.withPendingBrowser(tab.linkedBrowser, () =>
          fire.sync(tabManager.convert(tab), clickInfo)
        );
      };

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
    const { action } = this;

    return {
      pageAction: {
        ...action.api(context),

        onClicked: new EventManager({
          context,
          module: "pageAction",
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

global.pageActionFor = this.pageAction.for;
