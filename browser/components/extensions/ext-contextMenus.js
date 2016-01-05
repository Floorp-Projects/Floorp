/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

Cu.import("resource://gre/modules/ExtensionUtils.jsm");
Cu.import("resource://gre/modules/MatchPattern.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

var {
  EventManager,
  runSafe,
} = ExtensionUtils;

// Map[Extension -> Map[ID -> MenuItem]]
// Note: we want to enumerate all the menu items so
// this cannot be a weak map.
var contextMenuMap = new Map();

// Map[Extension -> MenuItem]
var rootItems = new Map();

// Not really used yet, will be used for event pages.
var onClickedCallbacksMap = new WeakMap();

// If id is not specified for an item we use an integer.
var nextID = 0;

var gMaxLabelLength = 64;

// When a new contextMenu is opened, this function is called and
// we populate the |xulMenu| with all the items from extensions
// to be displayed. We always clear all the items again when
// popuphidden fires.
var menuBuilder = {
  build: function(contextData) {
    let xulMenu = contextData.menu;
    xulMenu.addEventListener("popuphidden", this);
    this.xulMenu = xulMenu;
    for (let [extension, root] of rootItems) {
      let rootElement = this.buildElementWithChildren(root, contextData);
      if (!rootElement.childNodes.length) {
        // If the root has no visible children, there is no reason to show
        // the root menu item itself either.
        continue;
      }
      rootElement.setAttribute("ext-type", "top-level-menu");
      rootElement = this.removeTopLevelMenuIfNeeded(rootElement);
      xulMenu.appendChild(rootElement);
      this.itemsToCleanUp.add(rootElement);
    }
  },

  buildElementWithChildren(item, contextData) {
    let doc = contextData.menu.ownerDocument;
    let element = this.buildSingleElement(item, contextData);
    for (let child of item.children) {
      if (child.enabledForContext(contextData)) {
        let childElement = this.buildElementWithChildren(child, contextData);
        // Here element must be a menu element and its first child
        // is a menupopup, we have to append its children to this
        // menupopup.
        element.firstChild.appendChild(childElement);
      }
    }

    return element;
  },

  removeTopLevelMenuIfNeeded(element) {
    // If there is only one visible top level element we don't need the
    // root menu element for the extension.
    let menuPopup = element.firstChild;
    if (menuPopup && menuPopup.childNodes.length == 1) {
      let onlyChild = menuPopup.firstChild;
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
    // Menu elements need to have a menupopup child for
    // its menu items.
    let menupopup = doc.createElement("menupopup");
    element.appendChild(menupopup);
    return element;
  },

  customizeElement(element, item, contextData) {
    let label = item.title;
    if (label) {
      if (contextData.isTextSelected && label.indexOf('%s') > -1) {
        let selection = contextData.selectionText;
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

    if (!item.enabled) {
      element.setAttribute("disabled", true);
    }

    element.addEventListener("command", event => {
      item.tabManager.addActiveTabPermission();
      if (item.onclick) {
        let clickData = item.getClickData(contextData, event);
        runSafe(item.extContext, item.onclick, clickData);
       }
    });

    return element;
  },

  handleEvent: function(event) {
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

function contextMenuObserver(subject, topic, data) {
  subject = subject.wrappedJSObject;
  menuBuilder.build(subject);
}

function getContexts(contextData) {
  let contexts = new Set(["all"]);

  contexts.add("page");

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

  if (contextData.onImage) {
    contexts.add("image");
  }

  if (contextData.onVideo) {
    contexts.add("video");
  }

  if (contextData.onAudio) {
    contexts.add("audio");
  }

  return contexts;
}

function MenuItem(extension, extContext, createProperties, isRoot = false) {
  this.extension = extension;
  this.extContext = extContext;
  this.children = [];
  this.parent = null;
  this.tabManager = TabManager.for(extension);

  this.setDefaults();
  this.setProps(createProperties);
  if (!this.hasOwnProperty("_id")) {
    this.id = nextID++;
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
      this.documentUrlMatchPattern = new MatchPattern(this.documentUrlPatterns);
    }

    if (createProperties.targetUrlPatterns != null) {
      this.targetUrlMatchPattern = new MatchPattern(this.targetUrlPatterns);
    }
  },

  setDefaults() {
    this.setProps({
      type: "normal",
      checked: "false",
      contexts: ["all"],
      enabled: "true"
    });
  },

  set id(id) {
    if (this.hasOwnProperty("_id")) {
      throw new Error("Id of a MenuItem cannot be changed");
    }
    let isIdUsed = contextMenuMap.get(this.extension).has(id);
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
    let menuMap = contextMenuMap.get(this.extension);
    if (!menuMap.has(parentId)) {
      throw new Error("Could not find any MenuItem with id: " + parentId);
    }
    for (let item = menuMap.get(parentId); item; item = item.parent) {
      if (item === this) {
        throw new Error("MenuItem cannot be an ancestor (or self) of its new parent.");
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
      let menuMap = contextMenuMap.get(this.extension);
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
    if (!rootItems.has(extension)) {
      let root = new MenuItem(extension, this.context,
                              { title: extension.name },
                              /* isRoot = */ true);
      rootItems.set(extension, root);
    }

    return rootItems.get(extension);
  },

  remove() {
    if (this.parent) {
      this.parent.detachChild(this);
    }
    let children = this.children.slice(0);
    for (let child of children) {
      child.remove();
    }

    let menuMap = contextMenuMap.get(this.extension);
    menuMap.delete(this.id);
    if (this.root == this) {
      rootItems.delete(this.extension);
    }
  },

  getClickData(contextData, event) {
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

    let clickData = {
      menuItemId: this.id,
    };

    function setIfDefined(argName, value) {
      if (value) {
        clickData[argName] = value;
      }
    }

    let tab = contextData.tab ? TabManager.convert(this.extension, contextData.tab)
                              : undefined;

    setIfDefined("parentMenuItemId", this.parentId);
    setIfDefined("mediaType", mediaType);
    setIfDefined("linkUrl", contextData.linkUrl);
    setIfDefined("srcUrl", contextData.srcUrl);
    setIfDefined("pageUrl", contextData.pageUrl);
    setIfDefined("frameUrl", contextData.frameUrl);
    setIfDefined("selectionText", contextData.selectionText);
    setIfDefined("editable", contextData.onEditableArea);
    setIfDefined("tab", tab);

    return clickData;
  },

  enabledForContext(contextData) {
    let contexts = getContexts(contextData);
    if (!this.contexts.some(n => contexts.has(n))) {
      return false;
    }

    let docPattern = this.documentUrlMatchPattern;
    if (docPattern && !docPattern.matches(contextData.pageUrl)) {
      return false;
    }

    let isMedia = contextData.onImage || contextData.onAudio || contextData.onVideo;
    let targetPattern = this.targetUrlMatchPattern;
    if (isMedia && targetPattern && !targetPattern.matches(contextData.srcURL)) {
      // TODO: double check if mediaURL is always set when we need it
      return false;
    }

    return true;
  }
};

var extCount = 0;
/* eslint-disable mozilla/balanced-listeners */
extensions.on("startup", (type, extension) => {
  contextMenuMap.set(extension, new Map());
  rootItems.delete(extension);
  if (++extCount == 1) {
    Services.obs.addObserver(contextMenuObserver,
                             "on-build-contextmenu",
                             false);
  }
});

extensions.on("shutdown", (type, extension) => {
  contextMenuMap.delete(extension);
  if (--extCount == 0) {
    Services.obs.removeObserver(contextMenuObserver,
                                "on-build-contextmenu");
  }
});
/* eslint-enable mozilla/balanced-listeners */

extensions.registerSchemaAPI("contextMenus", "contextMenus", (extension, context) => {
  return {
    contextMenus: {
      create: function(createProperties, callback) {
        let menuItem = new MenuItem(extension, context, createProperties);
        contextMenuMap.get(extension).set(menuItem.id, menuItem);
        if (callback) {
          runSafe(context, callback);
        }
        return menuItem.id;
      },

      update: function(id, updateProperties, callback) {
        let menuItem = contextMenuMap.get(extension).get(id);
        if (menuItem) {
          menuItem.setProps(updateProperties);
        }
        if (callback) {
          runSafe(context, callback);
        }
      },

      remove: function(id, callback) {
        let menuItem = contextMenuMap.get(extension).get(id);
        if (menuItem) {
          menuItem.remove();
        }
        if (callback) {
          runSafe(context, callback);
        }
      },

      removeAll: function(callback) {
        let root = rootItems.get(extension);
        if (root) {
          root.remove();
        }
        if (callback) {
          runSafe(context, callback);
        }
      },

      // TODO: implement this once event pages are ready.
      onClicked: new EventManager(context, "contextMenus.onClicked", fire => {
        let callback = menuItem => {
          fire(menuItem.data);
        };

        onClickedCallbacksMap.set(extension, callback);
        return () => {
          onClickedCallbacksMap.delete(extension);
        };
      }).api(),
    },
  };
});
