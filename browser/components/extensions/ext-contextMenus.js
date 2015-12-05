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

// Not really used yet, will be used for event pages.
var onClickedCallbacksMap = new WeakMap();

// If id is not specified for an item we use an integer.
var nextID = 0;

// When a new contextMenu is opened, this function is called and
// we populate the |xulMenu| with all the items from extensions
// to be displayed. We always clear all the items again when
// popuphidden fires. Since most of the info we need is already
// calculated in nsContextMenu.jsm we simple reuse its flags here.
// For remote processes there is a gContextMenuContentData where all
// the important info is stored from the child process. We get
// this data in |contextData|.
var menuBuilder = {
  build: function(contextData) {
    // TODO: icons should be set for items
    let xulMenu = contextData.menu;
    xulMenu.addEventListener("popuphidden", this);
    let doc = xulMenu.ownerDocument;
    for (let [ext, menuItemMap] of contextMenuMap) {
      let parentMap = new Map();
      let topLevelItems = new Set();
      for (let entry of menuItemMap) {
        // We need a closure over |item|, and we don't currently get a
        // fresh binding per loop if we declare it in the loop head.
        let [id, item] = entry;

        if (item.enabledForContext(contextData)) {
          let element;
          if (item.isMenu) {
            element = doc.createElement("menu");
            // Menu elements need to have a menupopup child for
            // its menu items.
            let menupopup = doc.createElement("menupopup");
            element.appendChild(menupopup);
            // Storing the menupopup in a map, so we can find it for its
            // menu item children later.
            parentMap.set(id, menupopup);
          } else {
            element =
             (item.type == "separator") ? doc.createElement("menuseparator")
                                        : doc.createElement("menuitem");
          }

          // FIXME: handle %s in the title
          element.setAttribute("label", item.title);

          if (!item.enabled) {
            element.setAttribute("disabled", true);
          }

          let parentId = item.parentId;
          if (parentId && parentMap.has(parentId)) {
            // If parentId is set we have to look up its parent
            // and appened to its menupopup.
            let parentElement = parentMap.get(parentId);
            parentElement.appendChild(element);
          } else {
            topLevelItems.add(element);
          }

          element.addEventListener("command", event => { // eslint-disable-line mozilla/balanced-listeners
            item.tabManager.addActiveTabPermission();
            if (item.onclick) {
              let clickData = item.getClickData(contextData, event);
              runSafe(item.extContext, item.onclick, clickData);
            }
          });
        }
      }
      if (topLevelItems.size > 1) {
        // If more than one top level items are visible, callopse them.
        let top = doc.createElement("menu");
        top.setAttribute("label", ext.name);
        top.setAttribute("ext-type", "top-level-menu");
        let menupopup = doc.createElement("menupopup");
        top.appendChild(menupopup);
        for (let i of topLevelItems) {
          menupopup.appendChild(i);
        }
        xulMenu.appendChild(top);
        this._itemsToCleanUp.add(top);
      } else if (topLevelItems.size == 1) {
        // If there is only one visible item, we can just append it.
        let singleItem = topLevelItems.values().next().value;
        xulMenu.appendChild(singleItem);
        this._itemsToCleanUp.add(singleItem);
      }
    }
  },

  handleEvent: function(event) {
    let target = event.target;

    target.removeEventListener("popuphidden", this);
    for (let item of this._itemsToCleanUp) {
      target.removeChild(item);
    }
    // Let's detach the top level items.
    this._itemsToCleanUp.clear();
  },

  _itemsToCleanUp: new Set(),
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

function MenuItem(extension, extContext, createProperties) {
  this.extension = extension;
  this.extContext = extContext;

  this.tabManager = TabManager.for(extension);

  this.init(createProperties);
}

MenuItem.prototype = {
  // init is called from the MenuItem ctor and from update. The |update|
  // flag is for the later case.
  init(createProperties, update = false) {
    let item = this;
    // return true if the prop was set on |this|
    function parseProp(propName, defaultValue = null) {
      if (propName in createProperties) {
        item[propName] = createProperties[propName];
        return true;
      } else if (!update && defaultValue !== null) {
        item[propName] = defaultValue;
        return true;
      }
      return false;
    }

    if (update && "id" in createProperties) {
      throw new Error("Id of a MenuItem cannot be changed");
    } else if (!update) {
      let isIdUsed = contextMenuMap.get(this.extension).has(createProperties.id);
      if (createProperties.id && isIdUsed) {
        throw new Error("Id already exists");
      }
      this.id = createProperties.id ? createProperties.id : nextID++;
    }

    parseProp("type", "normal");
    parseProp("title");
    parseProp("checked", false);
    parseProp("contexts", ["all"]);

    // It's a bit wacky... but we shouldn't be too scared to use wrappedJSObject here.
    // Later on we will do proper argument validation anyway.
    if ("onclick" in createProperties.wrappedJSObject) {
      this.onclick = createProperties.wrappedJSObject.onclick;
    }

    if (parseProp("parentId")) {
      let found = false;
      let menuMap = contextMenuMap.get(this.extension);
      if (menuMap.has(this.parentId)) {
        found = true;
        menuMap.get(this.parentId).isMenu = true;
      }
      if (!found) {
        throw new Error("MenuItem with this parentId not found");
      }
    } else {
      this.parentId = undefined;
    }

    if (parseProp("documentUrlPatterns")) {
      this.documentUrlMatchPattern = new MatchPattern(this.documentUrlPatterns);
    }

    if (parseProp("targetUrlPatterns")) {
      this.targetUrlPatterns = new MatchPattern(this.targetUrlPatterns);
    }

    parseProp("enabled", true);
  },

  remove() {
    let menuMap = contextMenuMap.get(this.extension);
    // We want to remove all the items that has |this| in its parent chain.
    // The |checked| map is only an optimisation to avoid checking any item
    // twice in the algorithm.
    let checked = new Map();
    function hasAncestorWithId(item, id) {
      if (checked.has(item)) {
        return checked.get(item);
      }
      if (item.parentId === undefined) {
        checked.set(item, false);
        return false;
      }
      let parent = menuMap.get(item.parentId);
      if (!parent) {
        checked.set(item, false);
        return false;
      }
      if (parent.id === id) {
        checked.set(item, true);
        return true;
      }
      let rv = hasAncestorWithId(parent, id);
      checked.set(item, rv);
      return rv;
    }

    let toRemove = new Set();
    toRemove.add(this.id);
    for (let [, item] of menuMap) {
      if (hasAncestorWithId(item, this.id)) {
        toRemove.add(item.id);
      }
    }
    for (let id of toRemove) {
      menuMap.delete(id);
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

    let tab = contextData.tab ? TabManager.convert(this.extension, contextData.tab) : undefined;

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
    let enabled = false;
    let contexts = getContexts(contextData);
    for (let c of this.contexts) {
      if (contexts.has(c)) {
        enabled = true;
        break;
      }
    }
    if (!enabled) {
      return false;
    }

    if (this.documentUrlMatchPattern &&
        !this.documentUrlMatchPattern.matches(contextData.documentURIObject)) {
      return false;
    }

    if (this.targetUrlPatterns &&
        (contextData.onImage || contextData.onAudio || contextData.onVideo) &&
        !this.targetUrlPatterns.matches(contextData.mediaURL)) {
      // TODO: double check if mediaURL is always set when we need it
      return false;
    }

    return true;
  },
};

var extCount = 0;
/* eslint-disable mozilla/balanced-listeners */
extensions.on("startup", (type, extension) => {
  contextMenuMap.set(extension, new Map());
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

extensions.registerPrivilegedAPI("contextMenus", (extension, context) => {
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
          menuItem.init(updateProperties, true);
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
        for (let [, menuItem] of contextMenuMap.get(extension)) {
          menuItem.remove();
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
