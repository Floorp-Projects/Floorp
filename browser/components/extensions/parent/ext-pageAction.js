/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

ChromeUtils.defineModuleGetter(this, "ExtensionTelemetry",
                               "resource://gre/modules/ExtensionTelemetry.jsm");
ChromeUtils.defineModuleGetter(this, "PageActions",
                               "resource:///modules/PageActions.jsm");
ChromeUtils.defineModuleGetter(this, "PanelPopup",
                               "resource:///modules/ExtensionPopups.jsm");

var {ExtensionParent} = ChromeUtils.import("resource://gre/modules/ExtensionParent.jsm");

var {
  IconDetails,
  StartupCache,
} = ExtensionParent;

var {
  DefaultWeakMap,
} = ExtensionUtils;

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

    // `show` can have three different values:
    // - `false`. This means the page action is not shown.
    //   It's set as default if show_matches is empty. Can also be set in a tab via
    //   `pageAction.hide(tabId)`, e.g. in order to override show_matches.
    // - `true`. This means the page action is shown.
    //   It's never set as default because <all_urls> doesn't really match all URLs
    //   (e.g. "about:" URLs). But can be set in a tab via `pageAction.show(tabId)`.
    // - `undefined`.
    //   This is the default value when there are some patterns in show_matches.
    //   Can't be set as a tab-specific value.
    let show, showMatches, hideMatches;
    let show_matches = options.show_matches || [];
    let hide_matches = options.hide_matches || [];
    if (!show_matches.length) {
      // Always hide by default. No need to do any pattern matching.
      show = false;
    } else {
      // Might show or hide depending on the URL. Enable pattern matching.
      const {restrictSchemes} = extension;
      showMatches = new MatchPatternSet(show_matches, {restrictSchemes});
      hideMatches = new MatchPatternSet(hide_matches, {restrictSchemes});
    }

    this.defaults = {
      show,
      showMatches,
      hideMatches,
      title: options.default_title || extension.name,
      popup: options.default_popup || "",
      pinned: options.pinned,
    };

    this.browserStyle = options.browser_style;

    this.tabContext = new TabContext(tab => this.defaults);

    this.tabContext.on("location-change", this.handleLocationChange.bind(this)); // eslint-disable-line mozilla/balanced-listeners

    pageActionMap.set(extension, this);

    this.defaults.icon = await StartupCache.get(
      extension, ["pageAction", "default_icon"],
      () => this.normalize({path: options.default_icon || ""}));

    this.lastValues = new DefaultWeakMap(() => ({}));

    if (!this.browserPageAction) {
      this.browserPageAction = PageActions.addAction(new PageActions.Action({
        id: widgetId,
        extensionID: extension.id,
        title: this.defaults.title,
        iconURL: this.defaults.icon,
        pinnedToUrlbar: this.defaults.pinned,
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

      // If the page action is only enabled in some URLs, do pattern matching in
      // the active tabs and update the button if necessary.
      if (show === undefined) {
        for (let window of windowTracker.browserWindows()) {
          let tab = window.gBrowser.selectedTab;
          if (this.isShown(tab)) {
            this.updateButton(window);
          }
        }
      }
    }
  }

  onShutdown(isAppShutdown) {
    pageActionMap.delete(this.extension);

    this.tabContext.shutdown();

    // Removing the browser page action causes PageActions to forget about it
    // across app restarts, so don't remove it on app shutdown, but do remove
    // it on all other shutdowns since there's no guarantee the action will be
    // coming back.
    if (!isAppShutdown && this.browserPageAction) {
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

  normalize(details, context = null) {
    let icon = IconDetails.normalize(details, this.extension, context);
    if (!Object.keys(icon).length) {
      icon = null;
    }
    return icon;
  }

  // Updates the page action button in the given window to reflect the
  // properties of the currently selected tab:
  //
  // Updates "tooltiptext" and "aria-label" to match "title" property.
  // Updates "image" to match the "icon" property.
  // Enables or disables the icon, based on the "show" and "patternMatching" properties.
  updateButton(window) {
    let tab = window.gBrowser.selectedTab;
    let tabData = this.tabContext.get(tab);
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

      let show = tabData.show != null ? tabData.show : tabData.patternMatching;
      if (last.show !== show) {
        this.browserPageAction.setDisabled(!show, window);
        last.show = show;
      }

      let icon = tabData.icon;
      if (last.icon !== icon) {
        this.browserPageAction.setIconURL(icon, window);
        last.icon = icon;
      }
    });
  }

  // Checks whether the tab action is shown when the specified tab becomes active.
  // Does pattern matching if necessary, and caches the result as a tab-specific value.
  // @param {XULElement} tab
  //        The tab to be checked
  // @return boolean
  isShown(tab) {
    let tabData = this.tabContext.get(tab);

    // If there is a "show" value, return it. Can be due to show(), hide() or empty show_matches.
    if (tabData.show !== undefined) {
      return tabData.show;
    }

    // Otherwise pattern matching must have been configured. Do it, caching the result.
    if (tabData.patternMatching === undefined) {
      let uri = tab.linkedBrowser.currentURI;
      tabData.patternMatching = tabData.showMatches.matches(uri) && !tabData.hideMatches.matches(uri);
    }
    return tabData.patternMatching;
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
    if (this.isShown(window.gBrowser.selectedTab)) {
      this.handleClick(window);
    }
  }

  handleEvent(event) {
    switch (event.type) {
      case "popupshowing":
        const menu = event.target;
        const trigger = menu.triggerNode;

        if (menu.id === "pageActionContextMenu" &&
            trigger &&
            trigger.getAttribute("actionid") === this.browserPageAction.id &&
            !this.browserPageAction.getDisabled(trigger.ownerGlobal)) {
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
    const {extension} = this;

    ExtensionTelemetry.pageActionPopupOpen.stopwatchStart(extension, this);
    let tab = window.gBrowser.selectedTab;
    let popupURL = this.tabContext.get(tab).popup;

    this.tabManager.addActiveTabPermission(tab);

    // If the widget has a popup URL defined, we open a popup, but do not
    // dispatch a click event to the extension.
    // If it has no popup URL defined, we dispatch a click event, but do not
    // open a popup.
    if (popupURL) {
      if (this.popupNode && this.popupNode.panel.state !== "closed") {
        // The panel is being toggled closed.
        ExtensionTelemetry.pageActionPopupOpen.stopwatchCancel(extension, this);
        window.BrowserPageActions.togglePanelForAction(this.browserPageAction,
                                                       this.popupNode.panel);
        return;
      }

      this.popupNode = new PanelPopup(extension, window.document, popupURL,
                                      this.browserStyle);
      // Remove popupNode when it is closed.
      this.popupNode.panel.addEventListener("popuphiding", () => {
        this.popupNode = undefined;
      }, {once: true});
      await this.popupNode.contentReady;
      window.BrowserPageActions.togglePanelForAction(this.browserPageAction,
                                                     this.popupNode.panel);
      ExtensionTelemetry.pageActionPopupOpen.stopwatchFinish(extension, this);
    } else {
      ExtensionTelemetry.pageActionPopupOpen.stopwatchCancel(extension, this);
      this.emit("click", tab);
    }
  }

  /**
   * Updates the `tabData` for any location change, however it only updates the button
   * when the selected tab has a location change, or the selected tab has changed.
   *
   * @param {string} eventType
   *        The type of the event, should be "location-change".
   * @param {XULElement} tab
   *        The tab whose location changed, or which has become selected.
   * @param {boolean} [fromBrowse]
   *        - `true` if navigation occurred in `tab`.
   *        - `false` if the location changed but no navigation occurred, e.g. due to
               a hash change or `history.pushState`.
   *        - Omitted if TabSelect has occurred, tabData does not need to be updated.
   */
  handleLocationChange(eventType, tab, fromBrowse) {
    if (fromBrowse === true) {
      // Clear tab data on navigation.
      this.tabContext.clear(tab);
    } else if (fromBrowse === false) {
      // Clear pattern matching cache when URL changes.
      let tabData = this.tabContext.get(tab);
      if (tabData.patternMatching !== undefined) {
        tabData.patternMatching = undefined;
      }
    }

    if (tab.selected) {
      // isShown will do pattern matching (if necessary) and store the result
      // so that updateButton knows whether the page action should be shown.
      this.isShown(tab);
      this.updateButton(tab.ownerGlobal);
    }
  }

  getAPI(context) {
    let {extension} = context;

    const {tabManager} = extension;
    const pageAction = this;

    return {
      pageAction: {
        onClicked: new EventManager({
          context,
          name: "pageAction.onClicked",
          inputHandling: true,
          register: fire => {
            let listener = (evt, tab) => {
              context.withPendingBrowser(tab.linkedBrowser, () =>
                fire.sync(tabManager.convert(tab)));
            };

            pageAction.on("click", listener);
            return () => {
              pageAction.off("click", listener);
            };
          },
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
          return pageAction.isShown(tab);
        },

        setTitle(details) {
          let tab = tabTracker.getTab(details.tabId);
          pageAction.setProperty(tab, "title", details.title);
        },

        getTitle(details) {
          let tab = tabTracker.getTab(details.tabId);

          let title = pageAction.getProperty(tab, "title");
          return Promise.resolve(title);
        },

        setIcon(details) {
          let tab = tabTracker.getTab(details.tabId);

          let icon = pageAction.normalize(details, context);

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
