/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "PanelPopup",
                                  "resource:///modules/ExtensionPopups.jsm");

Cu.import("resource://gre/modules/Task.jsm");

var {
  IconDetails,
} = ExtensionUtils;

// WeakMap[Extension -> PageAction]
let pageActionMap = new WeakMap();

this.pageAction = class extends ExtensionAPI {
  static for(extension) {
    return pageActionMap.get(extension);
  }

  onManifestEntry(entryName) {
    let {extension} = this;
    let options = extension.manifest.page_action;

    this.id = makeWidgetId(extension.id) + "-page-action";

    this.tabManager = extension.tabManager;

    this.defaults = {
      show: false,
      title: options.default_title || extension.name,
      icon: IconDetails.normalize({path: options.default_icon}, extension),
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

    // WeakMap[ChromeWindow -> <xul:image>]
    this.buttons = new WeakMap();

    EventEmitter.decorate(this);

    pageActionMap.set(extension, this);
  }

  onShutdown(reason) {
    pageActionMap.delete(this.extension);

    this.tabContext.shutdown();

    for (let window of windowTracker.browserWindows()) {
      if (this.buttons.has(window)) {
        this.buttons.get(window).remove();
        window.document.removeEventListener("popupshowing", this);
      }
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
  // Shows or hides the icon, based on the "show" property.
  updateButton(window) {
    let tabData = this.tabContext.get(window.gBrowser.selectedTab);

    if (!(tabData.show || this.buttons.has(window))) {
      // Don't bother creating a button for a window until it actually
      // needs to be shown.
      return;
    }

    let button = this.getButton(window);

    if (tabData.show) {
      // Update the title and icon only if the button is visible.

      let title = tabData.title || this.extension.name;
      button.setAttribute("tooltiptext", title);
      button.setAttribute("aria-label", title);

      // These URLs should already be properly escaped, but make doubly sure CSS
      // string escape characters are escaped here, since they could lead to a
      // sandbox break.
      let escape = str => str.replace(/[\\\s"]/g, encodeURIComponent);

      let getIcon = size => escape(IconDetails.getPreferredIcon(tabData.icon, this.extension, size).icon);

      button.setAttribute("style", `
        --webextension-urlbar-image: url("${getIcon(16)}");
        --webextension-urlbar-image-2x: url("${getIcon(32)}");
      `);

      button.classList.add("webextension-page-action");
    }

    button.hidden = !tabData.show;
  }

  // Create an |image| node and add it to the |urlbar-icons|
  // container in the given window.
  addButton(window) {
    let document = window.document;

    let button = document.createElement("image");
    button.id = this.id;
    button.setAttribute("class", "urlbar-icon");

    button.addEventListener("click", this); // eslint-disable-line mozilla/balanced-listeners
    document.addEventListener("popupshowing", this);

    document.getElementById("urlbar-icons").appendChild(button);

    return button;
  }

  // Returns the page action button for the given window, creating it if
  // it doesn't already exist.
  getButton(window) {
    if (!this.buttons.has(window)) {
      let button = this.addButton(window);
      this.buttons.set(window, button);
    }

    return this.buttons.get(window);
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
    const window = event.target.ownerGlobal;

    switch (event.type) {
      case "click":
        if (event.button === 0) {
          this.handleClick(window);
        }
        break;

      case "popupshowing":
        const menu = event.target;
        const trigger = menu.triggerNode;

        if (menu.id === "toolbar-context-menu" && trigger && trigger.id === this.id) {
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
  handleClick(window) {
    let tab = window.gBrowser.selectedTab;
    let popupURL = this.tabContext.get(tab).popup;

    this.tabManager.addActiveTabPermission(tab);

    // If the widget has a popup URL defined, we open a popup, but do not
    // dispatch a click event to the extension.
    // If it has no popup URL defined, we dispatch a click event, but do not
    // open a popup.
    if (popupURL) {
      new PanelPopup(this.extension, this.getButton(window), popupURL,
                     this.browserStyle);
    } else {
      this.emit("click", tab);
    }
  }

  handleLocationChange(eventType, tab, fromBrowse) {
    if (fromBrowse) {
      this.tabContext.clear(tab);
    }
    this.updateButton(tab.ownerGlobal);
  }

  getAPI(context) {
    let {extension} = context;

    const {tabManager} = extension;
    const pageAction = this;

    return {
      pageAction: {
        onClicked: new SingletonEventManager(context, "pageAction.onClicked", fire => {
          let listener = (evt, tab) => {
            fire.async(tabManager.convert(tab));
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
          pageAction.setProperty(tab, "popup", url);
        },

        getPopup(details) {
          let tab = tabTracker.getTab(details.tabId);

          let popup = pageAction.getProperty(tab, "popup");
          return Promise.resolve(popup);
        },
      },
    };
  }
};

global.pageActionFor = this.pageAction.for;
