/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

// The ext-* files are imported into the same scopes.
/* import-globals-from ext-utils.js */

Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");

var {
  ExtensionError,
} = ExtensionUtils;

Cu.import("resource://gre/modules/ExtensionParent.jsm");

var {
  IconDetails,
} = ExtensionParent;

const ACTION_MENU_TOP_LEVEL_LIMIT = 6;

// Map[Extension -> Map[ID -> MenuItem]]
// Note: we want to enumerate all the menu items so
// this cannot be a weak map.
var gMenuMap = new Map();

// Map[Extension -> MenuItem]
var gRootItems = new Map();

// If id is not specified for an item we use an integer.
var gNextMenuItemID = 0;

// Used to assign unique names to radio groups.
var gNextRadioGroupID = 0;

// The max length of a menu item's label.
var gMaxLabelLength = 64;

var gMenuBuilder = {
  // When a new menu is opened, this function is called and
  // we populate the |xulMenu| with all the items from extensions
  // to be displayed. We always clear all the items again when
  // popuphidden fires.
  build(contextData) {
    let firstItem = true;
    let xulMenu = contextData.menu;
    xulMenu.addEventListener("popuphidden", this);
    this.xulMenu = xulMenu;
    for (let [, root] of gRootItems) {
      let rootElement = this.buildElementWithChildren(root, contextData);
      if (!rootElement.firstChild || !rootElement.firstChild.childNodes.length) {
        // If the root has no visible children, there is no reason to show
        // the root menu item itself either.
        continue;
      }
      rootElement.setAttribute("ext-type", "top-level-menu");
      rootElement = this.removeTopLevelMenuIfNeeded(rootElement);

      // Display the extension icon on the root element.
      if (root.extension.manifest.icons) {
        let parentWindow = contextData.menu.ownerGlobal;
        let extension = root.extension;

        let {icon} = IconDetails.getPreferredIcon(extension.manifest.icons, extension,
                                                  16 * parentWindow.devicePixelRatio);

        // The extension icons in the manifest are not pre-resolved, since
        // they're sometimes used by the add-on manager when the extension is
        // not enabled, and its URLs are not resolvable.
        let resolvedURL = root.extension.baseURI.resolve(icon);

        if (rootElement.localName == "menu") {
          rootElement.setAttribute("class", "menu-iconic");
        } else if (rootElement.localName == "menuitem") {
          rootElement.setAttribute("class", "menuitem-iconic");
        }
        rootElement.setAttribute("image", resolvedURL);
      }

      if (firstItem) {
        firstItem = false;
        const separator = xulMenu.ownerDocument.createElement("menuseparator");
        this.itemsToCleanUp.add(separator);
        xulMenu.append(separator);
      }

      xulMenu.appendChild(rootElement);
      this.itemsToCleanUp.add(rootElement);
    }
  },

  // Builds a context menu for browserAction and pageAction buttons.
  buildActionContextMenu(contextData) {
    const {menu} = contextData;

    contextData.tab = tabTracker.activeTab;
    contextData.pageUrl = contextData.tab.linkedBrowser.currentURI.spec;

    const root = gRootItems.get(contextData.extension);
    const children = this.buildChildren(root, contextData);
    const visible = children.slice(0, ACTION_MENU_TOP_LEVEL_LIMIT);

    if (visible.length) {
      this.xulMenu = menu;
      menu.addEventListener("popuphidden", this);

      const separator = menu.ownerDocument.createElement("menuseparator");
      menu.insertBefore(separator, menu.firstChild);
      this.itemsToCleanUp.add(separator);

      for (const child of visible) {
        this.itemsToCleanUp.add(child);
        menu.insertBefore(child, separator);
      }
    }
  },

  buildElementWithChildren(item, contextData) {
    const element = this.buildSingleElement(item, contextData);
    const children = this.buildChildren(item, contextData);
    if (children.length) {
      element.firstChild.append(...children);
    }
    return element;
  },

  buildChildren(item, contextData) {
    let groupName;
    let children = [];
    for (let child of item.children) {
      if (child.type == "radio" && !child.groupName) {
        if (!groupName) {
          groupName = `webext-radio-group-${gNextRadioGroupID++}`;
        }
        child.groupName = groupName;
      } else {
        groupName = null;
      }

      if (child.enabledForContext(contextData)) {
        children.push(this.buildElementWithChildren(child, contextData));
      }
    }
    return children;
  },

  removeTopLevelMenuIfNeeded(element) {
    // If there is only one visible top level element we don't need the
    // root menu element for the extension.
    let menuPopup = element.firstChild;
    if (menuPopup && menuPopup.childNodes.length == 1) {
      let onlyChild = menuPopup.firstChild;

      // Keep single checkbox items in the submenu on Linux since
      // the extension icon overlaps the checkbox otherwise.
      if (AppConstants.platform === "linux" && onlyChild.getAttribute("type") === "checkbox") {
        return element;
      }

      onlyChild.remove();
      return onlyChild;
    }

    return element;
  },

  buildSingleElement(item, contextData) {
    let doc = contextData.menu.ownerDocument;
    let element;
    if (item.children.length > 0) {
      element = this.createMenuElement(doc, item);
    } else if (item.type == "separator") {
      element = doc.createElement("menuseparator");
    } else {
      element = doc.createElement("menuitem");
    }

    return this.customizeElement(element, item, contextData);
  },

  createMenuElement(doc, item) {
    let element = doc.createElement("menu");
    // Menu elements need to have a menupopup child for its menu items.
    let menupopup = doc.createElement("menupopup");
    element.appendChild(menupopup);
    return element;
  },

  customizeElement(element, item, contextData) {
    let label = item.title;
    if (label) {
      if (contextData.isTextSelected && label.indexOf("%s") > -1) {
        let selection = contextData.selectionText.trim();
        // The rendering engine will truncate the title if it's longer than 64 characters.
        // But if it makes sense let's try truncate selection text only, to handle cases like
        // 'look up "%s" in MyDictionary' more elegantly.
        let maxSelectionLength = gMaxLabelLength - label.length + 2;
        if (maxSelectionLength > 4) {
          selection = selection.substring(0, maxSelectionLength - 3) + "...";
        }
        label = label.replace(/%s/g, selection);
      }

      element.setAttribute("label", label);
    }

    if (item.id && item.extension && item.extension.id) {
      element.setAttribute("id",
        `${makeWidgetId(item.extension.id)}_${item.id}`);
    }

    if (item.type == "checkbox") {
      element.setAttribute("type", "checkbox");
      if (item.checked) {
        element.setAttribute("checked", "true");
      }
    } else if (item.type == "radio") {
      element.setAttribute("type", "radio");
      element.setAttribute("name", item.groupName);
      if (item.checked) {
        element.setAttribute("checked", "true");
      }
    }

    if (!item.enabled) {
      element.setAttribute("disabled", "true");
    }

    element.addEventListener("command", event => {  // eslint-disable-line mozilla/balanced-listeners
      if (event.target !== event.currentTarget) {
        return;
      }
      const wasChecked = item.checked;
      if (item.type == "checkbox") {
        item.checked = !item.checked;
      } else if (item.type == "radio") {
        // Deselect all radio items in the current radio group.
        for (let child of item.parent.children) {
          if (child.type == "radio" && child.groupName == item.groupName) {
            child.checked = false;
          }
        }
        // Select the clicked radio item.
        item.checked = true;
      }

      item.tabManager.addActiveTabPermission();

      let tab = item.tabManager.convert(contextData.tab);
      let info = item.getClickInfo(contextData, wasChecked);

      const map = {shiftKey: "Shift", altKey: "Alt", metaKey: "Command", ctrlKey: "Ctrl"};
      info.modifiers = Object.keys(map).filter(key => event[key]).map(key => map[key]);
      if (event.ctrlKey && AppConstants.platform === "macosx") {
        info.modifiers.push("MacCtrl");
      }

      // Allow menus to open various actions supported in webext prior
      // to notifying onclicked.
      let actionFor = {
        _execute_page_action: global.pageActionFor,
        _execute_browser_action: global.browserActionFor,
        _execute_sidebar_action: global.sidebarActionFor,
      }[item.command];
      if (actionFor) {
        let win = event.target.ownerGlobal;
        actionFor(item.extension).triggerAction(win);
      }

      item.extension.emit("webext-menu-menuitem-click", info, tab);
    });

    return element;
  },

  handleEvent(event) {
    if (this.xulMenu != event.target || event.type != "popuphidden") {
      return;
    }

    delete this.xulMenu;
    let target = event.target;
    target.removeEventListener("popuphidden", this);
    for (let item of this.itemsToCleanUp) {
      item.remove();
    }
    this.itemsToCleanUp.clear();
  },

  itemsToCleanUp: new Set(),
};

// Called from pageAction or browserAction popup.
global.actionContextMenu = function(contextData) {
  gMenuBuilder.buildActionContextMenu(contextData);
};

const getMenuContexts = contextData => {
  let contexts = new Set();

  if (contextData.inFrame) {
    contexts.add("frame");
  }

  if (contextData.isTextSelected) {
    contexts.add("selection");
  }

  if (contextData.onLink) {
    contexts.add("link");
  }

  if (contextData.onEditableArea) {
    contexts.add("editable");
  }

  if (contextData.onPassword) {
    contexts.add("password");
  }

  if (contextData.onImage) {
    contexts.add("image");
  }

  if (contextData.onVideo) {
    contexts.add("video");
  }

  if (contextData.onAudio) {
    contexts.add("audio");
  }

  if (contextData.onPageAction) {
    contexts.add("page_action");
  }

  if (contextData.onBrowserAction) {
    contexts.add("browser_action");
  }

  if (contextData.onTab) {
    contexts.add("tab");
  }

  if (contexts.size === 0) {
    contexts.add("page");
  }

  // New non-content contexts supported in Firefox are not part of "all".
  if (!contextData.onTab) {
    contexts.add("all");
  }

  return contexts;
};

function MenuItem(extension, createProperties, isRoot = false) {
  this.extension = extension;
  this.children = [];
  this.parent = null;
  this.tabManager = extension.tabManager;

  this.setDefaults();
  this.setProps(createProperties);

  if (!this.hasOwnProperty("_id")) {
    this.id = gNextMenuItemID++;
  }
  // If the item is not the root and has no parent
  // it must be a child of the root.
  if (!isRoot && !this.parent) {
    this.root.addChild(this);
  }
}

MenuItem.prototype = {
  setProps(createProperties) {
    for (let propName in createProperties) {
      if (createProperties[propName] === null) {
        // Omitted optional argument.
        continue;
      }
      this[propName] = createProperties[propName];
    }

    if (createProperties.documentUrlPatterns != null) {
      this.documentUrlMatchPattern = new MatchPatternSet(this.documentUrlPatterns);
    }

    if (createProperties.targetUrlPatterns != null) {
      this.targetUrlMatchPattern = new MatchPatternSet(this.targetUrlPatterns);
    }

    // If a child MenuItem does not specify any contexts, then it should
    // inherit the contexts specified from its parent.
    if (createProperties.parentId && !createProperties.contexts) {
      this.contexts = this.parent.contexts;
    }
  },

  setDefaults() {
    this.setProps({
      type: "normal",
      checked: false,
      contexts: ["all"],
      enabled: true,
    });
  },

  set id(id) {
    if (this.hasOwnProperty("_id")) {
      throw new Error("Id of a MenuItem cannot be changed");
    }
    let isIdUsed = gMenuMap.get(this.extension).has(id);
    if (isIdUsed) {
      throw new Error("Id already exists");
    }
    this._id = id;
  },

  get id() {
    return this._id;
  },

  ensureValidParentId(parentId) {
    if (parentId === undefined) {
      return;
    }
    let menuMap = gMenuMap.get(this.extension);
    if (!menuMap.has(parentId)) {
      throw new Error("Could not find any MenuItem with id: " + parentId);
    }
    for (let item = menuMap.get(parentId); item; item = item.parent) {
      if (item === this) {
        throw new ExtensionError("MenuItem cannot be an ancestor (or self) of its new parent.");
      }
    }
  },

  set parentId(parentId) {
    this.ensureValidParentId(parentId);

    if (this.parent) {
      this.parent.detachChild(this);
    }

    if (parentId === undefined) {
      this.root.addChild(this);
    } else {
      let menuMap = gMenuMap.get(this.extension);
      menuMap.get(parentId).addChild(this);
    }
  },

  get parentId() {
    return this.parent ? this.parent.id : undefined;
  },

  addChild(child) {
    if (child.parent) {
      throw new Error("Child MenuItem already has a parent.");
    }
    this.children.push(child);
    child.parent = this;
  },

  detachChild(child) {
    let idx = this.children.indexOf(child);
    if (idx < 0) {
      throw new Error("Child MenuItem not found, it cannot be removed.");
    }
    this.children.splice(idx, 1);
    child.parent = null;
  },

  get root() {
    let extension = this.extension;
    if (!gRootItems.has(extension)) {
      let root = new MenuItem(extension,
                              {title: extension.name},
                              /* isRoot = */ true);
      gRootItems.set(extension, root);
    }

    return gRootItems.get(extension);
  },

  remove() {
    if (this.parent) {
      this.parent.detachChild(this);
    }
    let children = this.children.slice(0);
    for (let child of children) {
      child.remove();
    }

    let menuMap = gMenuMap.get(this.extension);
    menuMap.delete(this.id);
    if (this.root == this) {
      gRootItems.delete(this.extension);
    }
  },

  getClickInfo(contextData, wasChecked) {
    let mediaType;
    if (contextData.onVideo) {
      mediaType = "video";
    }
    if (contextData.onAudio) {
      mediaType = "audio";
    }
    if (contextData.onImage) {
      mediaType = "image";
    }

    let info = {
      menuItemId: this.id,
      editable: contextData.onEditableArea || contextData.onPassword,
    };

    function setIfDefined(argName, value) {
      if (value !== undefined) {
        info[argName] = value;
      }
    }

    setIfDefined("parentMenuItemId", this.parentId);
    setIfDefined("mediaType", mediaType);
    setIfDefined("linkText", contextData.linkText);
    setIfDefined("linkUrl", contextData.linkUrl);
    setIfDefined("srcUrl", contextData.srcUrl);
    setIfDefined("pageUrl", contextData.pageUrl);
    setIfDefined("frameUrl", contextData.frameUrl);
    setIfDefined("frameId", contextData.frameId);
    setIfDefined("selectionText", contextData.selectionText);

    if ((this.type === "checkbox") || (this.type === "radio")) {
      info.checked = this.checked;
      info.wasChecked = wasChecked;
    }

    return info;
  },

  enabledForContext(contextData) {
    let contexts = getMenuContexts(contextData);
    if (!this.contexts.some(n => contexts.has(n))) {
      return false;
    }

    let docPattern = this.documentUrlMatchPattern;
    let pageURI = Services.io.newURI(contextData[contextData.inFrame ? "frameUrl" : "pageUrl"]);
    if (docPattern && !docPattern.matches(pageURI)) {
      return false;
    }

    let targetPattern = this.targetUrlMatchPattern;
    if (targetPattern) {
      let targetUrls = [];
      if (contextData.onImage || contextData.onAudio || contextData.onVideo) {
        // TODO: double check if srcUrl is always set when we need it
        targetUrls.push(contextData.srcUrl);
      }
      if (contextData.onLink) {
        targetUrls.push(contextData.linkUrl);
      }
      if (!targetUrls.some(targetUrl => targetPattern.matches(NetUtil.newURI(targetUrl)))) {
        return false;
      }
    }

    return true;
  },
};

// While any extensions are active, this Tracker registers to observe/listen
// for contex-menu events from both content and chrome.
const menuTracker = {
  register() {
    Services.obs.addObserver(this, "on-build-contextmenu");
    for (const window of windowTracker.browserWindows()) {
      this.onWindowOpen(window);
    }
    windowTracker.addOpenListener(this.onWindowOpen);
  },

  unregister() {
    Services.obs.removeObserver(this, "on-build-contextmenu");
    for (const window of windowTracker.browserWindows()) {
      const menu = window.document.getElementById("tabContextMenu");
      menu.removeEventListener("popupshowing", this);
    }
    windowTracker.removeOpenListener(this.onWindowOpen);
  },

  observe(subject, topic, data) {
    subject = subject.wrappedJSObject;
    gMenuBuilder.build(subject);
  },

  onWindowOpen(window) {
    const menu = window.document.getElementById("tabContextMenu");
    menu.addEventListener("popupshowing", menuTracker);
  },

  handleEvent(event) {
    const menu = event.target;
    if (menu.id === "tabContextMenu") {
      const trigger = menu.triggerNode;
      const tab = trigger.localName === "tab" ? trigger : tabTracker.activeTab;
      const pageUrl = tab.linkedBrowser.currentURI.spec;
      gMenuBuilder.build({menu, tab, pageUrl, onTab: true});
    }
  },
};

this.menusInternal = class extends ExtensionAPI {
  constructor(extension) {
    super(extension);

    if (!gMenuMap.size) {
      menuTracker.register();
    }
    gMenuMap.set(extension, new Map());
  }

  onShutdown(reason) {
    let {extension} = this;

    if (gMenuMap.has(extension)) {
      gMenuMap.delete(extension);
      gRootItems.delete(extension);
      if (!gMenuMap.size) {
        menuTracker.unregister();
      }
    }
  }

  getAPI(context) {
    let {extension} = context;

    return {
      menusInternal: {
        create: function(createProperties) {
          // Note that the id is required by the schema. If the addon did not set
          // it, the implementation of menus.create in the child should
          // have added it.
          let menuItem = new MenuItem(extension, createProperties);
          gMenuMap.get(extension).set(menuItem.id, menuItem);
        },

        update: function(id, updateProperties) {
          let menuItem = gMenuMap.get(extension).get(id);
          if (menuItem) {
            menuItem.setProps(updateProperties);
          }
        },

        remove: function(id) {
          let menuItem = gMenuMap.get(extension).get(id);
          if (menuItem) {
            menuItem.remove();
          }
        },

        removeAll: function() {
          let root = gRootItems.get(extension);
          if (root) {
            root.remove();
          }
        },

        onClicked: new EventManager(context, "menusInternal.onClicked", fire => {
          let listener = (event, info, tab) => {
            context.withPendingBrowser(tab.linkedBrowser,
                                       () => fire.sync(info, tab));
          };

          extension.on("webext-menu-menuitem-click", listener);
          return () => {
            extension.off("webext-menu-menuitem-click", listener);
          };
        }).api(),
      },
    };
  }
};
