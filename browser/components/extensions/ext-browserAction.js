/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "CustomizableUI",
                                  "resource:///modules/CustomizableUI.jsm");

Cu.import("resource://devtools/shared/event-emitter.js");

Cu.import("resource://gre/modules/ExtensionUtils.jsm");
var {
  EventManager,
  DefaultWeakMap,
  runSafe,
} = ExtensionUtils;

// WeakMap[Extension -> BrowserAction]
var browserActionMap = new WeakMap();

function browserActionOf(extension)
{
  return browserActionMap.get(extension);
}

var nextActionId = 0;

// Responsible for the browser_action section of the manifest as well
// as the associated popup.
function BrowserAction(options, extension)
{
  this.extension = extension;
  this.id = makeWidgetId(extension.id) + "-browser-action";
  this.widget = null;

  this.tabManager = TabManager.for(extension);

  let title = extension.localize(options.default_title || "");
  let popup = extension.localize(options.default_popup || "");
  if (popup) {
    popup = extension.baseURI.resolve(popup);
  }

  this.defaults = {
    enabled: true,
    title: title,
    badgeText: "",
    badgeBackgroundColor: null,
    icon: IconDetails.normalize({ path: options.default_icon }, extension,
                                null, true),
    popup: popup,
  };

  this.tabContext = new TabContext(tab => Object.create(this.defaults),
                                   extension);

  EventEmitter.decorate(this);
}

BrowserAction.prototype = {
  build() {
    let widget = CustomizableUI.createWidget({
      id: this.id,
      type: "custom",
      removable: true,
      defaultArea: CustomizableUI.AREA_NAVBAR,
      onBuild: document => {
        let node = document.createElement("toolbarbutton");
        node.id = this.id;
        node.setAttribute("class", "toolbarbutton-1 chromeclass-toolbar-additional badged-button");
        node.setAttribute("constrain-size", "true");

        this.updateButton(node, this.defaults);

        let tabbrowser = document.defaultView.gBrowser;

        node.addEventListener("command", event => {
          let tab = tabbrowser.selectedTab;
          let popup = this.getProperty(tab, "popup");
          this.tabManager.addActiveTabPermission(tab);
          if (popup) {
            this.togglePopup(node, popup);
          } else {
            this.emit("click");
          }
        });

        return node;
      },
    });

    this.tabContext.on("tab-select",
                       (evt, tab) => { this.updateWindow(tab.ownerDocument.defaultView); })

    this.widget = widget;
  },

  togglePopup(node, popupResource) {
    openPanel(node, popupResource, this.extension);
  },

  // Update the toolbar button |node| with the tab context data
  // in |tabData|.
  updateButton(node, tabData) {
    if (tabData.title) {
      node.setAttribute("tooltiptext", tabData.title);
      node.setAttribute("label", tabData.title);
      node.setAttribute("aria-label", tabData.title);
    } else {
      node.removeAttribute("tooltiptext");
      node.removeAttribute("label");
      node.removeAttribute("aria-label");
    }

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
                                        'class', 'toolbarbutton-badge');
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
    } else {
      this.tabContext.get(tab)[prop] = value;
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

extensions.registerAPI((extension, context) => {
  return {
    browserAction: {
      onClicked: new EventManager(context, "browserAction.onClicked", fire => {
        let listener = () => {
          let tab = TabManager.activeTab;
          fire(TabManager.convert(extension, tab));
        };
        browserActionOf(extension).on("click", listener);
        return () => {
          browserActionOf(extension).off("click", listener);
        };
      }).api(),

      enable: function(tabId) {
        let tab = tabId ? TabManager.getTab(tabId) : null;
        browserActionOf(extension).setProperty(tab, "enabled", true);
      },

      disable: function(tabId) {
        let tab = tabId ? TabManager.getTab(tabId) : null;
        browserActionOf(extension).setProperty(tab, "enabled", false);
      },

      setTitle: function(details) {
        let tab = details.tabId ? TabManager.getTab(details.tabId) : null;
        browserActionOf(extension).setProperty(tab, "title", details.title);
      },

      getTitle: function(details, callback) {
        let tab = details.tabId ? TabManager.getTab(details.tabId) : null;
        let title = browserActionOf(extension).getProperty(tab, "title");
        runSafe(context, callback, title);
      },

      setIcon: function(details, callback) {
        let tab = details.tabId ? TabManager.getTab(details.tabId) : null;
        let icon = IconDetails.normalize(details, extension, context);
        browserActionOf(extension).setProperty(tab, "icon", icon);
      },

      setBadgeText: function(details) {
        let tab = details.tabId ? TabManager.getTab(details.tabId) : null;
        browserActionOf(extension).setProperty(tab, "badgeText", details.text);
      },

      getBadgeText: function(details, callback) {
        let tab = details.tabId ? TabManager.getTab(details.tabId) : null;
        let text = browserActionOf(extension).getProperty(tab, "badgeText");
        runSafe(context, callback, text);
      },

      setPopup: function(details) {
        let tab = details.tabId ? TabManager.getTab(details.tabId) : null;
        // Note: Chrome resolves arguments to setIcon relative to the calling
        // context, but resolves arguments to setPopup relative to the extension
        // root.
        // For internal consistency, we currently resolve both relative to the
        // calling context.
        let url = details.popup && context.uri.resolve(details.popup);
        browserActionOf(extension).setProperty(tab, "popup", url);
      },

      getPopup: function(details, callback) {
        let tab = details.tabId ? TabManager.getTab(details.tabId) : null;
        let popup = browserActionOf(extension).getProperty(tab, "popup");
        runSafe(context, callback, popup);
      },

      setBadgeBackgroundColor: function(details) {
        let color = details.color;
        let tab = details.tabId ? TabManager.getTab(details.tabId) : null;
        browserActionOf(extension).setProperty(tab, "badgeBackgroundColor", details.color);
      },

      getBadgeBackgroundColor: function(details, callback) {
        let tab = details.tabId ? TabManager.getTab(details.tabId) : null;
        let color = browserActionOf(extension).getProperty(tab, "badgeBackgroundColor");
        runSafe(context, callback, color);
      },
    }
  };
});
