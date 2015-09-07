XPCOMUtils.defineLazyModuleGetter(this, "CustomizableUI",
                                  "resource:///modules/CustomizableUI.jsm");

Cu.import("resource://gre/modules/devtools/shared/event-emitter.js");

Cu.import("resource://gre/modules/ExtensionUtils.jsm");
var {
  EventManager,
  DefaultWeakMap,
  ignoreEvent,
  runSafe,
} = ExtensionUtils;

// WeakMap[Extension -> BrowserAction]
var browserActionMap = new WeakMap();

// WeakMap[Extension -> docshell]
// This map is a cache of the windowless browser that's used to render ImageData
// for the browser_action icon.
let imageRendererMap = new WeakMap();

function browserActionOf(extension)
{
  return browserActionMap.get(extension);
}

function makeWidgetId(id)
{
  id = id.toLowerCase();
  return id.replace(/[^a-z0-9_-]/g, "_");
}

var nextActionId = 0;

// Responsible for the browser_action section of the manifest as well
// as the associated popup.
function BrowserAction(options, extension)
{
  this.extension = extension;
  this.id = makeWidgetId(extension.id) + "-browser-action";
  this.widget = null;

  this.title = new DefaultWeakMap(extension.localize(options.default_title));
  this.badgeText = new DefaultWeakMap();
  this.badgeBackgroundColor = new DefaultWeakMap();
  this.icon = new DefaultWeakMap(options.default_icon);
  this.popup = new DefaultWeakMap(options.default_popup);

  this.context = null;
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

        this.updateTab(null, node);

        let tabbrowser = document.defaultView.gBrowser;
        tabbrowser.tabContainer.addEventListener("TabSelect", this);

        node.addEventListener("command", event => {
          let tab = tabbrowser.selectedTab;
          let popup = this.getProperty(tab, "popup");
          if (popup) {
            this.togglePopup(node, popup);
          } else {
            this.emit("click");
          }
        });

        return node;
      },
    });
    this.widget = widget;
  },

  handleEvent(event) {
    if (event.type == "TabSelect") {
      let window = event.target.ownerDocument.defaultView;
      let tabbrowser = window.gBrowser;
      let instance = CustomizableUI.getWidget(this.id).forWindow(window);
      if (instance) {
        this.updateTab(tabbrowser.selectedTab, instance.node);
      }
    }
  },

  togglePopup(node, popupResource) {
    let popupURL = this.extension.baseURI.resolve(popupResource);

    let document = node.ownerDocument;
    let panel = document.createElement("panel");
    panel.setAttribute("class", "browser-action-panel");
    panel.setAttribute("type", "arrow");
    panel.setAttribute("flip", "slide");
    node.appendChild(panel);

    panel.addEventListener("popuphidden", () => {
      this.context.unload();
      this.context = null;
      panel.remove();
    });

    const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
    let browser = document.createElementNS(XUL_NS, "browser");
    browser.setAttribute("type", "content");
    browser.setAttribute("disableglobalhistory", "true");
    panel.appendChild(browser);

    let loadListener = () => {
      panel.removeEventListener("load", loadListener);

      this.context = new ExtensionPage(this.extension, {
        type: "popup",
        contentWindow: browser.contentWindow,
        uri: Services.io.newURI(popupURL, null, null),
        docShell: browser.docShell,
      });
      GlobalManager.injectInDocShell(browser.docShell, this.extension, this.context);
      browser.setAttribute("src", popupURL);

      let contentLoadListener = () => {
        browser.removeEventListener("load", contentLoadListener);

        let contentViewer = browser.docShell.contentViewer;
        let width = {}, height = {};
        try {
          contentViewer.getContentSize(width, height);
          [width, height] = [width.value, height.value];
        } catch (e) {
          // getContentSize can throw
          [width, height] = [400, 400];
        }

        let window = document.defaultView;
        width /= window.devicePixelRatio;
        height /= window.devicePixelRatio;
        width = Math.min(width, 800);
        height = Math.min(height, 800);

        browser.setAttribute("width", width);
        browser.setAttribute("height", height);

        let anchor = document.getAnonymousElementByAttribute(node, "class", "toolbarbutton-icon");
        panel.openPopup(anchor, "bottomcenter topright", 0, 0, false, false);
      };
      browser.addEventListener("load", contentLoadListener, true);
    };
    panel.addEventListener("load", loadListener);
  },

  // Initialize the toolbar icon and popup given that |tab| is the
  // current tab and |node| is the CustomizableUI node. Note: |tab|
  // will be null if we don't know the current tab yet (during
  // initialization).
  updateTab(tab, node) {
    let window = node.ownerDocument.defaultView;

    let title = this.getProperty(tab, "title");
    if (title) {
      node.setAttribute("tooltiptext", title);
      node.setAttribute("label", title);
    } else {
      node.removeAttribute("tooltiptext");
      node.removeAttribute("label");
    }

    let badgeText = this.badgeText.get(tab);
    if (badgeText) {
      node.setAttribute("badge", badgeText);
    } else {
      node.removeAttribute("badge");
    }

    function toHex(n) {
      return Math.floor(n / 16).toString(16) + (n % 16).toString(16);
    }

    let badgeNode = node.ownerDocument.getAnonymousElementByAttribute(node,
                                        'class', 'toolbarbutton-badge');
    if (badgeNode) {
      let color = this.badgeBackgroundColor.get(tab);
      if (Array.isArray(color)) {
        color = `rgb(${color[0]}, ${color[1]}, ${color[2]})`;
      }
      badgeNode.style.backgroundColor = color;
    }

    let iconURL = this.getIcon(tab, node);
    node.setAttribute("image", iconURL);
  },

  // Note: tab is allowed to be null here.
  getIcon(tab, node) {
    let icon = this.icon.get(tab);

    let url;
    if (typeof(icon) != "object") {
      url = icon;
    } else {
      let window = node.ownerDocument.defaultView;
      let utils = window.QueryInterface(Ci.nsIInterfaceRequestor)
                        .getInterface(Components.interfaces.nsIDOMWindowUtils);
      let res = {value: 1}
      utils.getResolution(res);

      let size = res.value == 1 ? 19 : 38;
      url = icon[size];
    }

    if (url) {
      return this.extension.baseURI.resolve(url);
    } else {
      return "chrome://browser/content/extension.svg";
    }
  },

  // Update the toolbar button for a given window.
  updateWindow(window) {
    let tab = window.gBrowser ? window.gBrowser.selectedTab : null;
    let node = CustomizableUI.getWidget(this.id).forWindow(window).node;
    this.updateTab(tab, node);
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
      let e = Services.wm.getEnumerator("navigator:browser");
      while (e.hasMoreElements()) {
        let window = e.getNext();
        if (window.gBrowser) {
          this.updateWindow(window);
        }
      }
    }
  },

  // tab is allowed to be null.
  // prop should be one of "icon", "title", "badgeText", "popup", or "badgeBackgroundColor".
  setProperty(tab, prop, value) {
    this[prop].set(tab, value);
    this.updateOnChange(tab);
  },

  // tab is allowed to be null.
  // prop should be one of "title", "badgeText", "popup", or "badgeBackgroundColor".
  getProperty(tab, prop) {
    return this[prop].get(tab);
  },

  shutdown() {
    let widget = CustomizableUI.getWidget(this.id);
    for (let instance of widget.instances) {
      let window = instance.node.ownerDocument.defaultView;
      let tabbrowser = window.gBrowser;
      tabbrowser.tabContainer.removeEventListener("TabSelect", this);
    }

    CustomizableUI.destroyWidget(this.id);
  },
};

EventEmitter.decorate(BrowserAction.prototype);

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
  imageRendererMap.delete(extension);
});

function convertImageDataToPNG(extension, imageData)
{
  let webNav = imageRendererMap.get(extension);
  if (!webNav) {
    webNav = Services.appShell.createWindowlessBrowser(false);
    let principal = Services.scriptSecurityManager.createCodebasePrincipal(extension.baseURI,
                                                                           {addonId: extension.id});
    let interfaceRequestor = webNav.QueryInterface(Ci.nsIInterfaceRequestor);
    let docShell = interfaceRequestor.getInterface(Ci.nsIDocShell);

    GlobalManager.injectInDocShell(docShell, extension, null);

    docShell.createAboutBlankContentViewer(principal);
  }

  let document = webNav.document;
  let canvas = document.createElement("canvas");
  canvas.width = imageData.width;
  canvas.height = imageData.height;
  canvas.getContext("2d").putImageData(imageData, 0, 0);

  let url = canvas.toDataURL("image/png");

  canvas.remove();

  return url;
}

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
        if (details.imageData) {
          let url = convertImageDataToPNG(extension, details.imageData);
          browserActionOf(extension).setProperty(tab, "icon", url);
        } else {
          browserActionOf(extension).setProperty(tab, "icon", details.path);
        }
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
        browserActionOf(extension).setProperty(tab, "popup", details.popup);
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
