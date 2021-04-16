/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "Bookmarks",
  "resource://gre/modules/Bookmarks.jsm"
);

var { DefaultMap, ExtensionError, parseMatchPatterns } = ExtensionUtils;

var { ExtensionParent } = ChromeUtils.import(
  "resource://gre/modules/ExtensionParent.jsm"
);

var { IconDetails } = ExtensionParent;

const ACTION_MENU_TOP_LEVEL_LIMIT = 6;

// Map[Extension -> Map[ID -> MenuItem]]
// Note: we want to enumerate all the menu items so
// this cannot be a weak map.
var gMenuMap = new Map();

// Map[Extension -> MenuItem]
var gRootItems = new Map();

// Map[Extension -> ID[]]
// Menu IDs that were eligible for being shown in the current menu.
var gShownMenuItems = new DefaultMap(() => []);

// Map[Extension -> Set[Contexts]]
// A DefaultMap (keyed by extension) which keeps track of the
// contexts with a subscribed onShown event listener.
var gOnShownSubscribers = new DefaultMap(() => new Set());

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
    contextData = this.maybeOverrideContextData(contextData);
    let xulMenu = contextData.menu;
    xulMenu.addEventListener("popuphidden", this);
    this.xulMenu = xulMenu;
    for (let [, root] of gRootItems) {
      this.createAndInsertTopLevelElements(root, contextData, null);
    }
    this.afterBuildingMenu(contextData);

    if (
      contextData.webExtContextData &&
      !contextData.webExtContextData.showDefaults
    ) {
      // Wait until nsContextMenu.js has toggled the visibility of the default
      // menu items before hiding the default items.
      Promise.resolve().then(() => this.hideDefaultMenuItems());
    }
  },

  maybeOverrideContextData(contextData) {
    let { webExtContextData } = contextData;
    if (!webExtContextData || !webExtContextData.overrideContext) {
      return contextData;
    }
    let contextDataBase = {
      menu: contextData.menu,
      // eslint-disable-next-line no-use-before-define
      originalViewType: getContextViewType(contextData),
      originalViewUrl: contextData.inFrame
        ? contextData.frameUrl
        : contextData.pageUrl,
      webExtContextData,
    };
    if (webExtContextData.overrideContext === "bookmark") {
      return {
        ...contextDataBase,
        bookmarkId: webExtContextData.bookmarkId,
        onBookmark: true,
      };
    }
    if (webExtContextData.overrideContext === "tab") {
      // TODO: Handle invalid tabs more gracefully (instead of throwing).
      let tab = tabTracker.getTab(webExtContextData.tabId);
      return {
        ...contextDataBase,
        tab,
        pageUrl: tab.linkedBrowser.currentURI.spec,
        onTab: true,
      };
    }
    throw new Error(
      `Unexpected overrideContext: ${webExtContextData.overrideContext}`
    );
  },

  canAccessContext(extension, contextData) {
    if (!extension.privateBrowsingAllowed) {
      let nativeTab = contextData.tab;
      if (
        nativeTab &&
        PrivateBrowsingUtils.isBrowserPrivate(nativeTab.linkedBrowser)
      ) {
        return false;
      } else if (
        PrivateBrowsingUtils.isWindowPrivate(contextData.menu.ownerGlobal)
      ) {
        return false;
      }
    }
    return true;
  },

  createAndInsertTopLevelElements(root, contextData, nextSibling) {
    let rootElements;
    if (!this.canAccessContext(root.extension, contextData)) {
      return;
    }
    if (contextData.onBrowserAction || contextData.onPageAction) {
      if (contextData.extension.id !== root.extension.id) {
        return;
      }
      rootElements = this.buildTopLevelElements(
        root,
        contextData,
        ACTION_MENU_TOP_LEVEL_LIMIT,
        false
      );

      // Action menu items are prepended to the menu, followed by a separator.
      nextSibling = nextSibling || this.xulMenu.firstElementChild;
      if (rootElements.length && !this.itemsToCleanUp.has(nextSibling)) {
        rootElements.push(
          this.xulMenu.ownerDocument.createXULElement("menuseparator")
        );
      }
    } else if (contextData.webExtContextData) {
      let {
        extensionId,
        showDefaults,
        overrideContext,
      } = contextData.webExtContextData;
      if (extensionId === root.extension.id) {
        rootElements = this.buildTopLevelElements(
          root,
          contextData,
          Infinity,
          false
        );
        if (!nextSibling) {
          // The extension menu should be rendered at the top. If we use
          // a navigation group (on non-macOS), the extension menu should
          // come after that to avoid styling issues.
          if (AppConstants.platform == "macosx") {
            nextSibling = this.xulMenu.firstElementChild;
          } else {
            nextSibling = this.xulMenu.querySelector(
              ":scope > #context-sep-navigation + *"
            );
          }
        }
        if (
          rootElements.length &&
          showDefaults &&
          !this.itemsToCleanUp.has(nextSibling)
        ) {
          rootElements.push(
            this.xulMenu.ownerDocument.createXULElement("menuseparator")
          );
        }
      } else if (!showDefaults && !overrideContext) {
        // When the default menu items should be hidden, menu items from other
        // extensions should be hidden too.
        return;
      }
      // Fall through to show default extension menu items.
    }
    if (!rootElements) {
      rootElements = this.buildTopLevelElements(root, contextData, 1, true);
      if (
        rootElements.length &&
        !this.itemsToCleanUp.has(this.xulMenu.lastElementChild)
      ) {
        // All extension menu items are appended at the end.
        // Prepend separator if this is the first extension menu item.
        rootElements.unshift(
          this.xulMenu.ownerDocument.createXULElement("menuseparator")
        );
      }
    }

    if (!rootElements.length) {
      return;
    }

    if (nextSibling) {
      nextSibling.before(...rootElements);
    } else {
      this.xulMenu.append(...rootElements);
    }
    for (let item of rootElements) {
      this.itemsToCleanUp.add(item);
    }
  },

  buildElementWithChildren(item, contextData) {
    const element = this.buildSingleElement(item, contextData);
    const children = this.buildChildren(item, contextData);
    if (children.length) {
      element.firstElementChild.append(...children);
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

  buildTopLevelElements(root, contextData, maxCount, forceManifestIcons) {
    let children = this.buildChildren(root, contextData);

    // TODO: Fix bug 1492969 and remove this whole if block.
    if (
      children.length === 1 &&
      maxCount === 1 &&
      forceManifestIcons &&
      AppConstants.platform === "linux" &&
      children[0].getAttribute("type") === "checkbox"
    ) {
      // Keep single checkbox items in the submenu on Linux since
      // the extension icon overlaps the checkbox otherwise.
      maxCount = 0;
    }

    if (children.length > maxCount) {
      // Move excess items into submenu.
      let rootElement = this.buildSingleElement(root, contextData);
      rootElement.setAttribute("ext-type", "top-level-menu");
      rootElement.firstElementChild.append(...children.splice(maxCount - 1));
      children.push(rootElement);
    }

    if (forceManifestIcons) {
      for (let rootElement of children) {
        // Display the extension icon on the root element.
        if (
          root.extension.manifest.icons &&
          rootElement.getAttribute("type") !== "checkbox"
        ) {
          this.setMenuItemIcon(
            rootElement,
            root.extension,
            contextData,
            root.extension.manifest.icons
          );
        } else {
          this.removeMenuItemIcon(rootElement);
        }
      }
    }
    return children;
  },

  buildSingleElement(item, contextData) {
    let doc = contextData.menu.ownerDocument;
    let element;
    if (item.children.length) {
      element = this.createMenuElement(doc, item);
    } else if (item.type == "separator") {
      element = doc.createXULElement("menuseparator");
    } else {
      element = doc.createXULElement("menuitem");
    }

    return this.customizeElement(element, item, contextData);
  },

  createMenuElement(doc, item) {
    let element = doc.createXULElement("menu");
    // Menu elements need to have a menupopup child for its menu items.
    let menupopup = doc.createXULElement("menupopup");
    element.appendChild(menupopup);
    return element;
  },

  customizeElement(element, item, contextData) {
    let label = item.title;
    if (label) {
      let accessKey;
      label = label.replace(/&([\S\s]|$)/g, (_, nextChar, i) => {
        if (nextChar === "&") {
          return "&";
        }
        if (accessKey === undefined) {
          if (nextChar === "%" && label.charAt(i + 2) === "s") {
            accessKey = "";
          } else {
            accessKey = nextChar;
          }
        }
        return nextChar;
      });
      element.setAttribute("accesskey", accessKey || "");

      if (contextData.isTextSelected && label.indexOf("%s") > -1) {
        let selection = contextData.selectionText.trim();
        // The rendering engine will truncate the title if it's longer than 64 characters.
        // But if it makes sense let's try truncate selection text only, to handle cases like
        // 'look up "%s" in MyDictionary' more elegantly.

        let codePointsToRemove = 0;

        let selectionArray = Array.from(selection);

        let completeLabelLength = label.length - 2 + selectionArray.length;
        if (completeLabelLength > gMaxLabelLength) {
          codePointsToRemove = completeLabelLength - gMaxLabelLength;
        }

        if (codePointsToRemove) {
          let ellipsis = "\u2026";
          try {
            ellipsis = Services.prefs.getComplexValue(
              "intl.ellipsis",
              Ci.nsIPrefLocalizedString
            ).data;
          } catch (e) {}
          codePointsToRemove += 1;
          selection =
            selectionArray.slice(0, -codePointsToRemove).join("") + ellipsis;
        }

        label = label.replace(/%s/g, selection);
      }

      element.setAttribute("label", label);
    }

    element.setAttribute("id", item.elementId);

    if ("icons" in item) {
      if (item.icons) {
        this.setMenuItemIcon(element, item.extension, contextData, item.icons);
      } else {
        this.removeMenuItemIcon(element);
      }
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

    let button;

    element.addEventListener(
      "command",
      event => {
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

        let { webExtContextData } = contextData;
        if (
          contextData.tab &&
          // If the menu context was overridden by the extension, do not grant
          // activeTab since the extension also controls the tabId.
          (!webExtContextData ||
            webExtContextData.extensionId !== item.extension.id)
        ) {
          item.tabManager.addActiveTabPermission(contextData.tab);
        }

        let info = item.getClickInfo(contextData, wasChecked);
        info.modifiers = clickModifiersFromEvent(event);

        info.button = button;

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

        item.extension.emit(
          "webext-menu-menuitem-click",
          info,
          contextData.tab
        );
      },
      { once: true }
    );

    // eslint-disable-next-line mozilla/balanced-listeners
    element.addEventListener("click", event => {
      if (
        event.target !== event.currentTarget ||
        // Ignore menu items that are usually not clickeable,
        // such as separators and parents of submenus and disabled items.
        element.localName !== "menuitem" ||
        element.disabled
      ) {
        return;
      }

      button = event.button;
      if (event.button) {
        element.doCommand();
        contextData.menu.hidePopup();
      }
    });

    // Don't publish the ID of the root because the root element is
    // auto-generated.
    if (item.parent) {
      gShownMenuItems.get(item.extension).push(item.id);
    }

    return element;
  },

  setMenuItemIcon(element, extension, contextData, icons) {
    let parentWindow = contextData.menu.ownerGlobal;

    let { icon } = IconDetails.getPreferredIcon(
      icons,
      extension,
      16 * parentWindow.devicePixelRatio
    );

    // The extension icons in the manifest are not pre-resolved, since
    // they're sometimes used by the add-on manager when the extension is
    // not enabled, and its URLs are not resolvable.
    let resolvedURL = extension.baseURI.resolve(icon);

    if (element.localName == "menu") {
      element.setAttribute("class", "menu-iconic");
    } else if (element.localName == "menuitem") {
      element.setAttribute("class", "menuitem-iconic");
    }

    element.setAttribute("image", resolvedURL);
  },

  // Undo changes from setMenuItemIcon.
  removeMenuItemIcon(element) {
    element.removeAttribute("class");
    element.removeAttribute("image");
  },

  rebuildMenu(extension) {
    let { contextData } = this;
    if (!contextData) {
      // This happens if the menu is not visible.
      return;
    }

    // Find the group of existing top-level items (usually 0 or 1 items)
    // and remember its position for when the new items are inserted.
    let elementIdPrefix = `${makeWidgetId(extension.id)}-menuitem-`;
    let nextSibling = null;
    for (let item of this.itemsToCleanUp) {
      if (item.id && item.id.startsWith(elementIdPrefix)) {
        nextSibling = item.nextSibling;
        item.remove();
        this.itemsToCleanUp.delete(item);
      }
    }

    let root = gRootItems.get(extension);
    if (root) {
      this.createAndInsertTopLevelElements(root, contextData, nextSibling);
    }

    this.xulMenu.showHideSeparators?.();
  },

  // This should be called once, after constructing the top-level menus, if any.
  afterBuildingMenu(contextData) {
    let dispatchOnShownEvent = extension => {
      if (!this.canAccessContext(extension, contextData)) {
        return;
      }

      // Note: gShownMenuItems is a DefaultMap, so .get(extension) causes the
      // extension to be stored in the map even if there are currently no
      // shown menu items. This ensures that the onHidden event can be fired
      // when the menu is closed.
      let menuIds = gShownMenuItems.get(extension);
      extension.emit("webext-menu-shown", menuIds, contextData);
    };

    if (contextData.onBrowserAction || contextData.onPageAction) {
      dispatchOnShownEvent(contextData.extension);
    } else {
      for (const extension of gOnShownSubscribers.keys()) {
        dispatchOnShownEvent(extension);
      }
    }

    this.contextData = contextData;
  },

  hideDefaultMenuItems() {
    for (let item of this.xulMenu.children) {
      if (!this.itemsToCleanUp.has(item)) {
        item.hidden = true;
      }
    }

    if (this.xulMenu.showHideSeparators) {
      this.xulMenu.showHideSeparators();
    }
  },

  handleEvent(event) {
    if (this.xulMenu != event.target || event.type != "popuphidden") {
      return;
    }

    delete this.xulMenu;
    delete this.contextData;

    let target = event.target;
    target.removeEventListener("popuphidden", this);
    for (let item of this.itemsToCleanUp) {
      item.remove();
    }
    this.itemsToCleanUp.clear();
    for (let extension of gShownMenuItems.keys()) {
      extension.emit("webext-menu-hidden");
    }
    gShownMenuItems.clear();
  },

  itemsToCleanUp: new Set(),
};

// Called from pageAction or browserAction popup.
global.actionContextMenu = function(contextData) {
  contextData.tab = tabTracker.activeTab;
  contextData.pageUrl = contextData.tab.linkedBrowser.currentURI.spec;
  gMenuBuilder.build(contextData);
};

const contextsMap = {
  onAudio: "audio",
  onEditable: "editable",
  inFrame: "frame",
  onImage: "image",
  onLink: "link",
  onPassword: "password",
  isTextSelected: "selection",
  onVideo: "video",

  onBookmark: "bookmark",
  onBrowserAction: "browser_action",
  onPageAction: "page_action",
  onTab: "tab",
  inToolsMenu: "tools_menu",
};

const getMenuContexts = contextData => {
  let contexts = new Set();

  for (const [key, value] of Object.entries(contextsMap)) {
    if (contextData[key]) {
      contexts.add(value);
    }
  }

  if (contexts.size === 0) {
    contexts.add("page");
  }

  // New non-content contexts supported in Firefox are not part of "all".
  if (
    !contextData.onBookmark &&
    !contextData.onTab &&
    !contextData.inToolsMenu
  ) {
    contexts.add("all");
  }

  return contexts;
};

function getContextViewType(contextData) {
  if ("originalViewType" in contextData) {
    return contextData.originalViewType;
  }
  if (
    contextData.webExtBrowserType === "popup" ||
    contextData.webExtBrowserType === "sidebar"
  ) {
    return contextData.webExtBrowserType;
  }
  if (contextData.tab && contextData.menu.id === "contentAreaContextMenu") {
    return "tab";
  }
  return undefined;
}

function addMenuEventInfo(info, contextData, extension, includeSensitiveData) {
  info.viewType = getContextViewType(contextData);
  if (contextData.onVideo) {
    info.mediaType = "video";
  } else if (contextData.onAudio) {
    info.mediaType = "audio";
  } else if (contextData.onImage) {
    info.mediaType = "image";
  }
  if (contextData.frameId !== undefined) {
    info.frameId = contextData.frameId;
  }
  if (contextData.onBookmark) {
    info.bookmarkId = contextData.bookmarkId;
  }
  info.editable = contextData.onEditable || false;
  if (includeSensitiveData) {
    // menus.getTargetElement requires the "menus" permission, so do not set
    // targetElementId for extensions with only the "contextMenus" permission.
    if (contextData.timeStamp && extension.hasPermission("menus")) {
      // Convert to integer, in case the DOMHighResTimeStamp has a fractional part.
      info.targetElementId = Math.floor(contextData.timeStamp);
    }
    if (contextData.onLink) {
      info.linkText = contextData.linkText;
      info.linkUrl = contextData.linkUrl;
    }
    if (contextData.onAudio || contextData.onImage || contextData.onVideo) {
      info.srcUrl = contextData.srcUrl;
    }
    if (!contextData.onBookmark) {
      info.pageUrl = contextData.pageUrl;
    }
    if (contextData.inFrame) {
      info.frameUrl = contextData.frameUrl;
    }
    if (contextData.isTextSelected) {
      info.selectionText = contextData.selectionText;
    }
  }
  // If the context was overridden, then frameUrl should be the URL of the
  // document in which the menu was opened (instead of undefined, even if that
  // document is not in a frame).
  if (contextData.originalViewUrl) {
    info.frameUrl = contextData.originalViewUrl;
  }
}

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

    if ("icons" in createProperties && createProperties.icons === null) {
      this.icons = null;
    }

    if (createProperties.documentUrlPatterns != null) {
      this.documentUrlMatchPattern = parseMatchPatterns(
        this.documentUrlPatterns,
        {
          restrictSchemes: this.extension.restrictSchemes,
        }
      );
    }

    if (createProperties.targetUrlPatterns != null) {
      this.targetUrlMatchPattern = parseMatchPatterns(this.targetUrlPatterns, {
        // restrictSchemes default to false when matching links instead of pages
        // (see Bug 1280370 for a rationale).
        restrictSchemes: false,
      });
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
      visible: true,
    });
  },

  set id(id) {
    if (this.hasOwnProperty("_id")) {
      throw new ExtensionError("ID of a MenuItem cannot be changed");
    }
    let isIdUsed = gMenuMap.get(this.extension).has(id);
    if (isIdUsed) {
      throw new ExtensionError(`ID already exists: ${id}`);
    }
    this._id = id;
  },

  get id() {
    return this._id;
  },

  get elementId() {
    let id = this.id;
    // If the ID is an integer, it is auto-generated and globally unique.
    // If the ID is a string, it is only unique within one extension and the
    // ID needs to be concatenated with the extension ID.
    if (typeof id !== "number") {
      // To avoid collisions with numeric IDs, add a prefix to string IDs.
      id = `_${id}`;
    }
    return `${makeWidgetId(this.extension.id)}-menuitem-${id}`;
  },

  ensureValidParentId(parentId) {
    if (parentId === undefined) {
      return;
    }
    let menuMap = gMenuMap.get(this.extension);
    if (!menuMap.has(parentId)) {
      throw new ExtensionError(
        `Could not find any MenuItem with id: ${parentId}`
      );
    }
    for (let item = menuMap.get(parentId); item; item = item.parent) {
      if (item === this) {
        throw new ExtensionError(
          "MenuItem cannot be an ancestor (or self) of its new parent."
        );
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
      let root = new MenuItem(
        extension,
        { title: extension.name },
        /* isRoot = */ true
      );
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
    let info = {
      menuItemId: this.id,
    };
    if (this.parent) {
      info.parentMenuItemId = this.parentId;
    }

    addMenuEventInfo(info, contextData, this.extension, true);

    if (this.type === "checkbox" || this.type === "radio") {
      info.checked = this.checked;
      info.wasChecked = wasChecked;
    }

    return info;
  },

  enabledForContext(contextData) {
    if (!this.visible) {
      return false;
    }
    let contexts = getMenuContexts(contextData);
    if (!this.contexts.some(n => contexts.has(n))) {
      return false;
    }

    if (
      this.viewTypes &&
      !this.viewTypes.includes(getContextViewType(contextData))
    ) {
      return false;
    }

    let docPattern = this.documentUrlMatchPattern;
    // When viewTypes is specified, the menu item is expected to be restricted
    // to documents. So let documentUrlPatterns always apply to the URL of the
    // document in which the menu was opened. When maybeOverrideContextData
    // changes the context, contextData.pageUrl does not reflect that URL any
    // more, so use contextData.originalViewUrl instead.
    if (docPattern && this.viewTypes && contextData.originalViewUrl) {
      if (
        !docPattern.matches(Services.io.newURI(contextData.originalViewUrl))
      ) {
        return false;
      }
      docPattern = null; // Null it so that it won't be used with pageURI below.
    }

    if (contextData.onBookmark) {
      return this.extension.hasPermission("bookmarks");
    }

    let pageURI = Services.io.newURI(
      contextData[contextData.inFrame ? "frameUrl" : "pageUrl"]
    );
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
      if (
        !targetUrls.some(targetUrl =>
          targetPattern.matches(Services.io.newURI(targetUrl))
        )
      ) {
        return false;
      }
    }

    return true;
  },
};

// windowTracker only looks as browser windows, but we're also interested in
// the Library window.  Helper for menuTracker below.
const libraryTracker = {
  libraryWindowType: "Places:Organizer",

  isLibraryWindow(window) {
    let winType = window.document.documentElement.getAttribute("windowtype");
    return winType === this.libraryWindowType;
  },

  init(listener) {
    this._listener = listener;
    Services.ww.registerNotification(this);

    // See WindowTrackerBase#*browserWindows in ext-tabs-base.js for why we
    // can't use the enumerator's windowtype filter.
    for (let window of Services.wm.getEnumerator("")) {
      if (window.document.readyState === "complete") {
        if (this.isLibraryWindow(window)) {
          this.notify(window);
        }
      } else {
        window.addEventListener("load", this, { once: true });
      }
    }
  },

  // cleanupWindow is called on any library window that's open.
  uninit(cleanupWindow) {
    Services.ww.unregisterNotification(this);

    for (let window of Services.wm.getEnumerator("")) {
      window.removeEventListener("load", this);
      try {
        if (this.isLibraryWindow(window)) {
          cleanupWindow(window);
        }
      } catch (e) {
        Cu.reportError(e);
      }
    }
  },

  // Gets notifications from Services.ww.registerNotification.
  // Defer actually doing anything until the window's loaded, though.
  observe(window, topic) {
    if (topic === "domwindowopened") {
      window.addEventListener("load", this, { once: true });
    }
  },

  // Gets the load event for new windows(registered in observe()).
  handleEvent(event) {
    let window = event.target.defaultView;
    if (this.isLibraryWindow(window)) {
      this.notify(window);
    }
  },

  notify(window) {
    try {
      this._listener.call(null, window);
    } catch (e) {
      Cu.reportError(e);
    }
  },
};

// While any extensions are active, this Tracker registers to observe/listen
// for menu events from both Tools and context menus, both content and chrome.
const menuTracker = {
  menuIds: ["placesContext", "menu_ToolsPopup", "tabContextMenu"],

  register() {
    Services.obs.addObserver(this, "on-build-contextmenu");
    for (const window of windowTracker.browserWindows()) {
      this.onWindowOpen(window);
    }
    windowTracker.addOpenListener(this.onWindowOpen);
    libraryTracker.init(this.onLibraryOpen);
  },

  unregister() {
    Services.obs.removeObserver(this, "on-build-contextmenu");
    for (const window of windowTracker.browserWindows()) {
      this.cleanupWindow(window);
    }
    windowTracker.removeOpenListener(this.onWindowOpen);
    libraryTracker.uninit(this.cleanupLibrary);
  },

  observe(subject, topic, data) {
    subject = subject.wrappedJSObject;
    gMenuBuilder.build(subject);
  },

  async onWindowOpen(window) {
    for (const id of menuTracker.menuIds) {
      const menu = window.document.getElementById(id);
      menu.addEventListener("popupshowing", menuTracker);
    }

    const sidebarHeader = window.document.getElementById(
      "sidebar-switcher-target"
    );
    sidebarHeader.addEventListener("SidebarShown", menuTracker.onSidebarShown);

    await window.SidebarUI.promiseInitialized;

    if (
      !window.closed &&
      window.SidebarUI.currentID === "viewBookmarksSidebar"
    ) {
      menuTracker.onSidebarShown({ currentTarget: sidebarHeader });
    }
  },

  cleanupWindow(window) {
    for (const id of this.menuIds) {
      const menu = window.document.getElementById(id);
      menu.removeEventListener("popupshowing", this);
    }

    const sidebarHeader = window.document.getElementById(
      "sidebar-switcher-target"
    );
    sidebarHeader.removeEventListener("SidebarShown", this.onSidebarShown);

    if (window.SidebarUI.currentID === "viewBookmarksSidebar") {
      let sidebarBrowser = window.SidebarUI.browser;
      sidebarBrowser.removeEventListener("load", this.onSidebarShown);
      const menu = sidebarBrowser.contentDocument.getElementById(
        "placesContext"
      );
      menu.removeEventListener("popupshowing", this.onBookmarksContextMenu);
    }
  },

  onSidebarShown(event) {
    // The event target is an element in a browser window, so |window| will be
    // the browser window that contains the sidebar.
    const window = event.currentTarget.ownerGlobal;
    if (window.SidebarUI.currentID === "viewBookmarksSidebar") {
      let sidebarBrowser = window.SidebarUI.browser;
      if (sidebarBrowser.contentDocument.readyState !== "complete") {
        // SidebarUI.currentID may be updated before the bookmark sidebar's
        // document has finished loading. This sometimes happens when the
        // sidebar is automatically shown when a new window is opened.
        sidebarBrowser.addEventListener("load", menuTracker.onSidebarShown, {
          once: true,
        });
        return;
      }
      const menu = sidebarBrowser.contentDocument.getElementById(
        "placesContext"
      );
      menu.addEventListener("popupshowing", menuTracker.onBookmarksContextMenu);
    }
  },

  onLibraryOpen(window) {
    const menu = window.document.getElementById("placesContext");
    menu.addEventListener("popupshowing", menuTracker.onBookmarksContextMenu);
  },

  cleanupLibrary(window) {
    const menu = window.document.getElementById("placesContext");
    menu.removeEventListener(
      "popupshowing",
      menuTracker.onBookmarksContextMenu
    );
  },

  handleEvent(event) {
    const menu = event.target;

    if (menu.id === "placesContext") {
      const trigger = menu.triggerNode;
      if (!trigger._placesNode?.bookmarkGuid) {
        return;
      }

      gMenuBuilder.build({
        menu,
        bookmarkId: trigger._placesNode.bookmarkGuid,
        onBookmark: true,
      });
    }
    if (menu.id === "menu_ToolsPopup") {
      const tab = tabTracker.activeTab;
      const pageUrl = tab.linkedBrowser.currentURI.spec;
      gMenuBuilder.build({ menu, tab, pageUrl, inToolsMenu: true });
    }
    if (menu.id === "tabContextMenu") {
      const tab = menu.ownerGlobal.TabContextMenu.contextTab;
      const pageUrl = tab.linkedBrowser.currentURI.spec;
      gMenuBuilder.build({ menu, tab, pageUrl, onTab: true });
    }
  },

  onBookmarksContextMenu(event) {
    const menu = event.target;
    const tree = menu.triggerNode.parentElement;
    const cell = tree.getCellAt(event.x, event.y);
    const node = tree.view.nodeForTreeIndex(cell.row);

    if (!node.bookmarkGuid || Bookmarks.isVirtualRootItem(node.bookmarkGuid)) {
      return;
    }

    gMenuBuilder.build({
      menu,
      bookmarkId: node.bookmarkGuid,
      onBookmark: true,
    });
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

  onShutdown() {
    let { extension } = this;

    if (gMenuMap.has(extension)) {
      gMenuMap.delete(extension);
      gRootItems.delete(extension);
      gShownMenuItems.delete(extension);
      gOnShownSubscribers.delete(extension);
      if (!gMenuMap.size) {
        menuTracker.unregister();
      }
    }
  }

  getAPI(context) {
    let { extension } = context;

    const menus = {
      refresh() {
        gMenuBuilder.rebuildMenu(extension);
      },

      onShown: new EventManager({
        context,
        name: "menus.onShown",
        register: fire => {
          let listener = (event, menuIds, contextData) => {
            let info = {
              menuIds,
              contexts: Array.from(getMenuContexts(contextData)),
            };

            let nativeTab = contextData.tab;

            // The menus.onShown event is fired before the user has consciously
            // interacted with an extension, so we require permissions before
            // exposing sensitive contextual data.
            let contextUrl = contextData.inFrame
              ? contextData.frameUrl
              : contextData.pageUrl;
            let includeSensitiveData =
              (nativeTab &&
                extension.tabManager.hasActiveTabPermission(nativeTab)) ||
              (contextUrl && extension.allowedOrigins.matches(contextUrl));

            addMenuEventInfo(
              info,
              contextData,
              extension,
              includeSensitiveData
            );

            let tab = nativeTab && extension.tabManager.convert(nativeTab);
            fire.sync(info, tab);
          };
          gOnShownSubscribers.get(extension).add(context);
          extension.on("webext-menu-shown", listener);
          return () => {
            const contexts = gOnShownSubscribers.get(extension);
            contexts.delete(context);
            if (contexts.size === 0) {
              gOnShownSubscribers.delete(extension);
            }
            extension.off("webext-menu-shown", listener);
          };
        },
      }).api(),
      onHidden: new EventManager({
        context,
        name: "menus.onHidden",
        register: fire => {
          let listener = () => {
            fire.sync();
          };
          extension.on("webext-menu-hidden", listener);
          return () => {
            extension.off("webext-menu-hidden", listener);
          };
        },
      }).api(),
    };

    return {
      contextMenus: menus,
      menus,
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

        onClicked: new EventManager({
          context,
          name: "menusInternal.onClicked",
          register: fire => {
            let listener = (event, info, nativeTab) => {
              let { linkedBrowser } = nativeTab || tabTracker.activeTab;
              let tab = nativeTab && extension.tabManager.convert(nativeTab);
              context.withPendingBrowser(linkedBrowser, () =>
                fire.sync(info, tab)
              );
            };

            extension.on("webext-menu-menuitem-click", listener);
            return () => {
              extension.off("webext-menu-menuitem-click", listener);
            };
          },
        }).api(),
      },
    };
  }
};
