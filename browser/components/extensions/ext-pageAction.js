/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

// The ext-* files are imported into the same scopes.
/* import-globals-from ext-browserAction.js */
/* import-globals-from ext-browser.js */

XPCOMUtils.defineLazyModuleGetter(this, "PageActions",
                                  "resource:///modules/PageActions.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PanelPopup",
                                  "resource:///modules/ExtensionPopups.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "TelemetryStopwatch",
                                  "resource://gre/modules/TelemetryStopwatch.jsm");


Cu.import("resource://gre/modules/ExtensionParent.jsm");

var {
  IconDetails,
  StartupCache,
} = ExtensionParent;

const popupOpenTimingHistogram = "WEBEXT_PAGEACTION_POPUP_OPEN_MS";

// WeakMap[Extension -> PageAction]
let pageActionMap = new WeakMap();

this.pageAction = class extends ExtensionAPI {
  static for(extension) {
    return pageActionMap.get(extension);
  }

  async onManifestEntry(entryName) {
    let {extension} = this;
    let options = extension.manifest.page_action;

    let widgetId = makeWidgetId(extension.id);
    this.id = widgetId + "-page-action";

    this.tabManager = extension.tabManager;

    // If <all_urls> is present, the default is to show the page action.
    let show = !!options.show_matches && options.show_matches.includes("<all_urls>");
    let showMatches = new MatchPatternSet(options.show_matches || []);
    let hideMatches = new MatchPatternSet(options.hide_matches || []);

    this.defaults = {
      show,
      showMatches,
      hideMatches,
      title: options.default_title || extension.name,
      popup: options.default_popup || "",
    };

    this.browserStyle = options.browser_style || false;
    if (options.browser_style === null) {
      this.extension.logger.warn("Please specify whether you want browser_style " +
                                 "or not in your page_action options.");
    }

    this.tabContext = new TabContext(tab => Object.create(this.defaults),
                                     extension);

    this.tabContext.on("location-change", this.handleLocationChange.bind(this)); // eslint-disable-line mozilla/balanced-listeners

    pageActionMap.set(extension, this);

    this.defaults.icon = await StartupCache.get(
      extension, ["pageAction", "default_icon"],
      () => IconDetails.normalize({path: options.default_icon}, extension));

    if (!this.browserPageAction) {
      this.browserPageAction = PageActions.addAction(new PageActions.Action({
        id: widgetId,
        extensionID: extension.id,
        title: this.defaults.title,
        iconURL: this.getIconData(this.defaults.icon),
        pinnedToUrlbar: true,
        disabled: !this.defaults.show,
        onCommand: (event, buttonNode) => {
          this.handleClick(event.target.ownerGlobal);
        },
        onBeforePlacedInWindow: browserWindow => {
          if (this.extension.hasPermission("menus") ||
              this.extension.hasPermission("contextMenus")) {
            browserWindow.document.addEventListener("popupshowing", this);
          }
        },
        onRemovedFromWindow: browserWindow => {
          browserWindow.document.removeEventListener("popupshowing", this);
        },
      }));
    }
  }

  onShutdown(reason) {
    pageActionMap.delete(this.extension);

    this.tabContext.shutdown();

    // Removing the browser page action causes PageActions to forget about it
    // across app restarts, so don't remove it on app shutdown, but do remove
    // it on all other shutdowns since there's no guarantee the action will be
    // coming back.
    if (reason != "APP_SHUTDOWN" && this.browserPageAction) {
      this.browserPageAction.remove();
      this.browserPageAction = null;
    }
  }

  // Returns the value of the property |prop| for the given tab, where
  // |prop| is one of "show", "title", "icon", "popup".
  getProperty(tab, prop) {
    return this.tabContext.get(tab)[prop];
  }

  // Sets the value of the property |prop| for the given tab to the
  // given value, symmetrically to |getProperty|.
  //
  // If |tab| is currently selected, updates the page action button to
  // reflect the new value.
  setProperty(tab, prop, value) {
    if (value != null) {
      this.tabContext.get(tab)[prop] = value;
    } else {
      delete this.tabContext.get(tab)[prop];
    }

    if (tab.selected) {
      this.updateButton(tab.ownerGlobal);
    }
  }

  // Updates the page action button in the given window to reflect the
  // properties of the currently selected tab:
  //
  // Updates "tooltiptext" and "aria-label" to match "title" property.
  // Updates "image" to match the "icon" property.
  // Enables or disables the icon, based on the "show" property.
  updateButton(window) {
    let tabData = this.tabContext.get(window.gBrowser.selectedTab);
    let title = tabData.title || this.extension.name;
    this.browserPageAction.setTitle(title, window);
    this.browserPageAction.setTooltip(title, window);
    this.browserPageAction.setDisabled(!tabData.show, window);

    let iconURL;
    if (typeof(tabData.icon) == "string") {
      iconURL = IconDetails.escapeUrl(tabData.icon);
    } else {
      iconURL = this.getIconData(tabData.icon);
    }
    this.browserPageAction.setIconURL(iconURL, window);
  }

  getIconData(icons) {
    let getIcon = size => {
      let {icon} = IconDetails.getPreferredIcon(icons, this.extension, size);
      // TODO: implement theme based icon for pageAction (Bug 1398156)
      return IconDetails.escapeUrl(icon);
    };
    return {
      "16": getIcon(16),
      "32": getIcon(32),
    };
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
    let pageAction = pageActionMap.get(this.extension);
    if (pageAction.getProperty(window.gBrowser.selectedTab, "show")) {
      pageAction.handleClick(window);
    }
  }

  handleEvent(event) {
    switch (event.type) {
      case "popupshowing":
        const menu = event.target;
        const trigger = menu.triggerNode;

        if (menu.id === "pageActionContextMenu" &&
            trigger &&
            trigger.getAttribute("actionid") === this.browserPageAction.id) {
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
  async handleClick(window) {
    TelemetryStopwatch.start(popupOpenTimingHistogram, this);
    let tab = window.gBrowser.selectedTab;
    let popupURL = this.tabContext.get(tab).popup;

    this.tabManager.addActiveTabPermission(tab);

    // If the widget has a popup URL defined, we open a popup, but do not
    // dispatch a click event to the extension.
    // If it has no popup URL defined, we dispatch a click event, but do not
    // open a popup.
    if (popupURL) {
      let popup = new PanelPopup(this.extension, window.document, popupURL,
                                 this.browserStyle);
      await popup.contentReady;
      window.BrowserPageActions.togglePanelForAction(this.browserPageAction,
                                                     popup.panel);
      TelemetryStopwatch.finish(popupOpenTimingHistogram, this);
    } else {
      TelemetryStopwatch.cancel(popupOpenTimingHistogram, this);
      this.emit("click", tab);
    }
  }

  handleLocationChange(eventType, tab, fromBrowse) {
    if (fromBrowse) {
      this.tabContext.clear(tab);
    }

    // Set show via pattern matching.
    let context = this.tabContext.get(tab);
    let uri = tab.linkedBrowser.currentURI;
    context.show = (context.show || context.showMatches.matches(uri)) && !context.hideMatches.matches(uri);

    this.updateButton(tab.ownerGlobal);
  }

  getAPI(context) {
    let {extension} = context;

    const {tabManager} = extension;
    const pageAction = this;

    return {
      pageAction: {
        onClicked: new InputEventManager(context, "pageAction.onClicked", fire => {
          let listener = (evt, tab) => {
            context.withPendingBrowser(tab.linkedBrowser, () =>
              fire.sync(tabManager.convert(tab)));
          };

          pageAction.on("click", listener);
          return () => {
            pageAction.off("click", listener);
          };
        }).api(),

        show(tabId) {
          let tab = tabTracker.getTab(tabId);
          pageAction.setProperty(tab, "show", true);
        },

        hide(tabId) {
          let tab = tabTracker.getTab(tabId);
          pageAction.setProperty(tab, "show", false);
        },

        isShown(details) {
          let tab = tabTracker.getTab(details.tabId);
          return pageAction.getProperty(tab, "show");
        },

        setTitle(details) {
          let tab = tabTracker.getTab(details.tabId);

          // Clear the tab-specific title when given a null string.
          pageAction.setProperty(tab, "title", details.title || null);
        },

        getTitle(details) {
          let tab = tabTracker.getTab(details.tabId);

          let title = pageAction.getProperty(tab, "title");
          return Promise.resolve(title);
        },

        setIcon(details) {
          let tab = tabTracker.getTab(details.tabId);

          let icon = IconDetails.normalize(details, extension, context);
          pageAction.setProperty(tab, "icon", icon);
        },

        setPopup(details) {
          let tab = tabTracker.getTab(details.tabId);

          // Note: Chrome resolves arguments to setIcon relative to the calling
          // context, but resolves arguments to setPopup relative to the extension
          // root.
          // For internal consistency, we currently resolve both relative to the
          // calling context.
          let url = details.popup && context.uri.resolve(details.popup);
          if (url && !context.checkLoadURL(url)) {
            return Promise.reject({message: `Access denied for URL ${url}`});
          }
          pageAction.setProperty(tab, "popup", url);
        },

        getPopup(details) {
          let tab = tabTracker.getTab(details.tabId);

          let popup = pageAction.getProperty(tab, "popup");
          return Promise.resolve(popup);
        },

        openPopup: function() {
          let window = windowTracker.topWindow;
          pageAction.triggerAction(window);
        },
      },
    };
  }
};

global.pageActionFor = this.pageAction.for;
