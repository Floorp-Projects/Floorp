/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This module inject dynamically menu items into browser UI.
 *
 * Menu definitions are fetched from:
 * - devtools/client/menus for top level entires
 * - devtools/client/definitions for tool-specifics entries
 */

const {Cu} = require("chrome");
const {LocalizationHelper} = require("devtools/shared/l10n");
const MENUS_L10N = new LocalizationHelper("devtools/client/locales/menus.properties");

loader.lazyRequireGetter(this, "gDevTools", "devtools/client/framework/devtools", true);
loader.lazyRequireGetter(this, "gDevToolsBrowser", "devtools/client/framework/devtools-browser", true);

// Keep list of inserted DOM Elements in order to remove them on unload
// Maps browser xul document => list of DOM Elements
const FragmentsCache = new Map();

function l10n(key) {
  return MENUS_L10N.getStr(key);
}

/**
 * Create a xul:menuitem element
 *
 * @param {XULDocument} doc
 *        The document to which menus are to be added.
 * @param {String} id
 *        Element id.
 * @param {String} label
 *        Menu label.
 * @param {String} accesskey (optional)
 *        Access key of the menuitem, used as shortcut while opening the menu.
 * @param {Boolean} isCheckbox (optional)
 *        If true, the menuitem will act as a checkbox and have an optional
 *        tick on its left.
 *
 * @return XULMenuItemElement
 */
function createMenuItem({ doc, id, label, accesskey, isCheckbox }) {
  const menuitem = doc.createElement("menuitem");
  menuitem.id = id;
  menuitem.setAttribute("label", label);
  if (accesskey) {
    menuitem.setAttribute("accesskey", accesskey);
  }
  if (isCheckbox) {
    menuitem.setAttribute("type", "checkbox");
    menuitem.setAttribute("autocheck", "false");
  }
  return menuitem;
}

/**
 * Add a menu entry for a tool definition
 *
 * @param {Object} toolDefinition
 *        Tool definition of the tool to add a menu entry.
 * @param {XULDocument} doc
 *        The document to which the tool menu item is to be added.
 */
function createToolMenuElements(toolDefinition, doc) {
  const id = toolDefinition.id;
  const menuId = "menuitem_" + id;

  // Prevent multiple entries for the same tool.
  if (doc.getElementById(menuId)) {
    return;
  }

  const oncommand = function(id, event) {
    const window = event.target.ownerDocument.defaultView;
    gDevToolsBrowser.selectToolCommand(window.gBrowser, id, Cu.now());
  }.bind(null, id);

  const menuitem = createMenuItem({
    doc,
    id: "menuitem_" + id,
    label: toolDefinition.menuLabel || toolDefinition.label,
    accesskey: toolDefinition.accesskey
  });
  // Refer to the key in order to display the key shortcut at menu ends
  // This <key> element is being created by devtools/client/devtools-startup.js
  menuitem.setAttribute("key", "key_" + id);
  menuitem.addEventListener("command", oncommand);

  return {
    menuitem
  };
}

/**
 * Create xul menuitem, key elements for a given tool.
 * And then insert them into browser DOM.
 *
 * @param {XULDocument} doc
 *        The document to which the tool is to be registered.
 * @param {Object} toolDefinition
 *        Tool definition of the tool to register.
 * @param {Object} prevDef
 *        The tool definition after which the tool menu item is to be added.
 */
function insertToolMenuElements(doc, toolDefinition, prevDef) {
  const { menuitem } = createToolMenuElements(toolDefinition, doc);

  let ref;
  if (prevDef) {
    const menuitem = doc.getElementById("menuitem_" + prevDef.id);
    ref = menuitem && menuitem.nextSibling ? menuitem.nextSibling : null;
  } else {
    ref = doc.getElementById("menu_devtools_separator");
  }

  if (ref) {
    ref.parentNode.insertBefore(menuitem, ref);
  }
}
exports.insertToolMenuElements = insertToolMenuElements;

/**
 * Remove a tool's menuitem from a window
 *
 * @param {string} toolId
 *        Id of the tool to add a menu entry for
 * @param {XULDocument} doc
 *        The document to which the tool menu item is to be removed from
 */
function removeToolFromMenu(toolId, doc) {
  const key = doc.getElementById("key_" + toolId);
  if (key) {
    key.remove();
  }

  const menuitem = doc.getElementById("menuitem_" + toolId);
  if (menuitem) {
    menuitem.remove();
  }
}
exports.removeToolFromMenu = removeToolFromMenu;

/**
 * Add all tools to the developer tools menu of a window.
 *
 * @param {XULDocument} doc
 *        The document to which the tool items are to be added.
 */
function addAllToolsToMenu(doc) {
  const fragKeys = doc.createDocumentFragment();
  const fragMenuItems = doc.createDocumentFragment();

  for (const toolDefinition of gDevTools.getToolDefinitionArray()) {
    if (!toolDefinition.inMenu) {
      continue;
    }

    const elements = createToolMenuElements(toolDefinition, doc);

    if (!elements) {
      continue;
    }

    if (elements.key) {
      fragKeys.appendChild(elements.key);
    }
    fragMenuItems.appendChild(elements.menuitem);
  }

  const mps = doc.getElementById("menu_devtools_separator");
  if (mps) {
    mps.parentNode.insertBefore(fragMenuItems, mps);
  }
}

/**
 * Add global menus that are not panel specific.
 *
 * @param {XULDocument} doc
 *        The document to which menus are to be added.
 */
function addTopLevelItems(doc) {
  const menuItems = doc.createDocumentFragment();

  const { menuitems } = require("../menus");
  for (const item of menuitems) {
    if (item.separator) {
      const separator = doc.createElement("menuseparator");
      separator.id = item.id;
      menuItems.appendChild(separator);
    } else {
      const { id, l10nKey } = item;

      // Create a <menuitem>
      const menuitem = createMenuItem({
        doc,
        id,
        label: l10n(l10nKey + ".label"),
        accesskey: l10n(l10nKey + ".accesskey"),
        isCheckbox: item.checkbox
      });
      menuitem.addEventListener("command", item.oncommand);
      menuItems.appendChild(menuitem);

      if (item.keyId) {
        menuitem.setAttribute("key", "key_" + item.keyId);
      }
    }
  }

  // Cache all nodes before insertion to be able to remove them on unload
  const nodes = [];
  for (const node of menuItems.children) {
    nodes.push(node);
  }
  FragmentsCache.set(doc, nodes);

  const menu = doc.getElementById("menuWebDeveloperPopup");
  menu.appendChild(menuItems);

  // There is still "Page Source" menuitem hardcoded into browser.xul. Instead
  // of manually inserting everything around it, move it to the expected
  // position.
  const pageSource = doc.getElementById("menu_pageSource");
  const endSeparator = doc.getElementById("devToolsEndSeparator");
  menu.insertBefore(pageSource, endSeparator);
}

/**
 * Remove global menus that are not panel specific.
 *
 * @param {XULDocument} doc
 *        The document to which menus are to be added.
 */
function removeTopLevelItems(doc) {
  const nodes = FragmentsCache.get(doc);
  if (!nodes) {
    return;
  }
  FragmentsCache.delete(doc);
  for (const node of nodes) {
    node.remove();
  }
}

/**
 * Add menus to a browser document
 *
 * @param {XULDocument} doc
 *        The document to which menus are to be added.
 */
exports.addMenus = function(doc) {
  addTopLevelItems(doc);

  addAllToolsToMenu(doc);
};

/**
 * Remove menus from a browser document
 *
 * @param {XULDocument} doc
 *        The document to which menus are to be removed.
 */
exports.removeMenus = function(doc) {
  // We only remove top level entries. Per-tool entries are removed while
  // unregistering each tool.
  removeTopLevelItems(doc);
};
