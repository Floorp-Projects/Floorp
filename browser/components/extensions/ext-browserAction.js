/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "CustomizableUI",
                                  "resource:///modules/CustomizableUI.jsm");

Cu.import("resource://devtools/shared/event-emitter.js");

Cu.import("resource://gre/modules/ExtensionUtils.jsm");
var {
  EventManager,
} = ExtensionUtils;

const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

// WeakMap[Extension -> BrowserAction]
var browserActionMap = new WeakMap();

// Responsible for the browser_action section of the manifest as well
// as the associated popup.
function BrowserAction(options, extension) {
  this.extension = extension;

  let widgetId = makeWidgetId(extension.id);
  this.id = `${widgetId}-browser-action`;
  this.viewId = `PanelUI-webext-${widgetId}-browser-action-view`;
  this.widget = null;

  this.tabManager = TabManager.for(extension);

  this.defaults = {
    enabled: true,
    title: options.default_title || extension.name,
    badgeText: "",
    badgeBackgroundColor: null,
    icon: IconDetails.normalize({path: options.default_icon}, extension),
    popup: options.default_popup || "",
  };

  this.browserStyle = options.browser_style || false;
  if (options.browser_style === null) {
    this.extension.logger.warn("Please specify whether you want browser_style " +
                               "or not in your browser_action options.");
  }

  this.tabContext = new TabContext(tab => Object.create(this.defaults),
                                   extension);

  EventEmitter.decorate(this);
}

BrowserAction.prototype = {
  build() {
    let widget = CustomizableUI.createWidget({
      id: this.id,
      viewId: this.viewId,
      type: "view",
      removable: true,
      label: this.defaults.title || this.extension.name,
      tooltiptext: this.defaults.title || "",
      defaultArea: CustomizableUI.AREA_NAVBAR,

      onBeforeCreated: document => {
        let view = document.createElementNS(XUL_NS, "panelview");
        view.id = this.viewId;
        view.setAttribute("flex", "1");

        document.getElementById("PanelUI-multiView").appendChild(view);
      },

      onDestroyed: document => {
        let view = document.getElementById(this.viewId);
        if (view) {
          view.remove();
        }
      },

      onCreated: node => {
        node.classList.add("badged-button");
        node.setAttribute("constrain-size", "true");

        this.updateButton(node, this.defaults);
      },

      onViewShowing: event => {
        let document = event.target.ownerDocument;
        let tabbrowser = document.defaultView.gBrowser;

        let tab = tabbrowser.selectedTab;
        let popupURL = this.getProperty(tab, "popup");
        this.tabManager.addActiveTabPermission(tab);

        // If the widget has a popup URL defined, we open a popup, but do not
        // dispatch a click event to the extension.
        // If it has no popup URL defined, we dispatch a click event, but do not
        // open a popup.
        if (popupURL) {
          try {
            new ViewPopup(this.extension, event.target, popupURL, this.browserStyle);
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
                       (evt, tab) => { this.updateWindow(tab.ownerDocument.defaultView); });

    this.widget = widget;
  },

  // Update the toolbar button |node| with the tab context data
  // in |tabData|.
  updateButton(node, tabData) {
    let title = tabData.title || this.extension.name;
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
      if (Array.isArray(color)) {
        color = `rgb(${color[0]}, ${color[1]}, ${color[2]})`;
      }
      badgeNode.style.backgroundColor = color || "";
    }

    let iconURL = IconDetails.getURL(
      tabData.icon, node.ownerDocument.defaultView, this.extension);
    node.setAttribute("image", iconURL);
  },

  // Update the toolbar button for a given window.
  updateWindow(window) {
    let widget = this.widget.forWindow(window);
    if (widget) {
      let tab = window.gBrowser.selectedTab;
      this.updateButton(widget.node, this.tabContext.get(tab));
    }
  },

  // Update the toolbar button when the extension changes the icon,
  // title, badge, etc. If it only changes a parameter for a single
  // tab, |tab| will be that tab. Otherwise it will be null.
  updateOnChange(tab) {
    if (tab) {
      if (tab.selected) {
        this.updateWindow(tab.ownerDocument.defaultView);
      }
    } else {
      for (let window of WindowListManager.browserWindows()) {
        this.updateWindow(window);
      }
    }
  },

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
  },

  // tab is allowed to be null.
  // prop should be one of "title", "badgeText", "popup", or "badgeBackgroundColor".
  getProperty(tab, prop) {
    if (tab == null) {
      return this.defaults[prop];
    } else {
      return this.tabContext.get(tab)[prop];
    }
  },

  shutdown() {
    this.tabContext.shutdown();
    CustomizableUI.destroyWidget(this.id);
  },
};

BrowserAction.for = (extension) => {
  return browserActionMap.get(extension);
};

global.browserActionFor = BrowserAction.for;

/* eslint-disable mozilla/balanced-listeners */
extensions.on("manifest_browser_action", (type, directive, extension, manifest) => {
  let browserAction = new BrowserAction(manifest.browser_action, extension);
  browserAction.build();
  browserActionMap.set(extension, browserAction);
});

extensions.on("shutdown", (type, extension) => {
  if (browserActionMap.has(extension)) {
    browserActionMap.get(extension).shutdown();
    browserActionMap.delete(extension);
  }
});
/* eslint-enable mozilla/balanced-listeners */

extensions.registerSchemaAPI("browserAction", null, (extension, context) => {
  return {
    browserAction: {
      onClicked: new EventManager(context, "browserAction.onClicked", fire => {
        let listener = () => {
          let tab = TabManager.activeTab;
          fire(TabManager.convert(extension, tab));
        };
        BrowserAction.for(extension).on("click", listener);
        return () => {
          BrowserAction.for(extension).off("click", listener);
        };
      }).api(),

      enable: function(tabId) {
        let tab = tabId !== null ? TabManager.getTab(tabId) : null;
        BrowserAction.for(extension).setProperty(tab, "enabled", true);
      },

      disable: function(tabId) {
        let tab = tabId !== null ? TabManager.getTab(tabId) : null;
        BrowserAction.for(extension).setProperty(tab, "enabled", false);
      },

      setTitle: function(details) {
        let tab = details.tabId !== null ? TabManager.getTab(details.tabId) : null;

        let title = details.title;
        // Clear the tab-specific title when given a null string.
        if (tab && title == "") {
          title = null;
        }
        BrowserAction.for(extension).setProperty(tab, "title", title);
      },

      getTitle: function(details) {
        let tab = details.tabId !== null ? TabManager.getTab(details.tabId) : null;
        let title = BrowserAction.for(extension).getProperty(tab, "title");
        return Promise.resolve(title);
      },

      setIcon: function(details) {
        let tab = details.tabId !== null ? TabManager.getTab(details.tabId) : null;
        let icon = IconDetails.normalize(details, extension, context);
        BrowserAction.for(extension).setProperty(tab, "icon", icon);
        return Promise.resolve();
      },

      setBadgeText: function(details) {
        let tab = details.tabId !== null ? TabManager.getTab(details.tabId) : null;
        BrowserAction.for(extension).setProperty(tab, "badgeText", details.text);
      },

      getBadgeText: function(details) {
        let tab = details.tabId !== null ? TabManager.getTab(details.tabId) : null;
        let text = BrowserAction.for(extension).getProperty(tab, "badgeText");
        return Promise.resolve(text);
      },

      setPopup: function(details) {
        let tab = details.tabId !== null ? TabManager.getTab(details.tabId) : null;
        // Note: Chrome resolves arguments to setIcon relative to the calling
        // context, but resolves arguments to setPopup relative to the extension
        // root.
        // For internal consistency, we currently resolve both relative to the
        // calling context.
        let url = details.popup && context.uri.resolve(details.popup);
        BrowserAction.for(extension).setProperty(tab, "popup", url);
      },

      getPopup: function(details) {
        let tab = details.tabId !== null ? TabManager.getTab(details.tabId) : null;
        let popup = BrowserAction.for(extension).getProperty(tab, "popup");
        return Promise.resolve(popup);
      },

      setBadgeBackgroundColor: function(details) {
        let tab = details.tabId !== null ? TabManager.getTab(details.tabId) : null;
        BrowserAction.for(extension).setProperty(tab, "badgeBackgroundColor", details.color);
      },

      getBadgeBackgroundColor: function(details, callback) {
        let tab = details.tabId !== null ? TabManager.getTab(details.tabId) : null;
        let color = BrowserAction.for(extension).getProperty(tab, "badgeBackgroundColor");
        return Promise.resolve(color);
      },
    },
  };
});
