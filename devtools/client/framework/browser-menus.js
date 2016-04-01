/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This module inject dynamically menu items and key shortcuts into browser UI.
 *
 * Menu and shortcut definitions are fetched from:
 * - devtools/client/menus for top level entires
 * - devtools/client/definitions for tool-specifics entries
 */

const Services = require("Services");
const MenuStrings = Services.strings.createBundle("chrome://devtools/locale/menus.properties");

loader.lazyRequireGetter(this, "gDevTools", "devtools/client/framework/devtools", true);

// Keep list of inserted DOM Elements in order to remove them on unload
// Maps browser xul document => list of DOM Elements
const FragmentsCache = new Map();

function l10n(key) {
  return MenuStrings.GetStringFromName(key);
}

/**
 * Create a xul:key element
 *
 * @param {XULDocument} doc
 *        The document to which keys are to be added.
 * @param {String} l10nKey
 *        Prefix of the properties entry to look for key shortcut in
 *        localization file. We will look for {property}.key and
 *        {property}.keytext for non-character shortcuts like F12.
 * @param {String} command
 *        Id of the xul:command to map to.
 * @param {Object} key definition dictionnary
 *        Definition with following attributes:
 *        - {String} id
 *          xul:key's id, automatically prefixed with "key_",
 *        - {String} modifiers
 *          Space separater list of modifier names,
 *        - {Boolean} keytext
 *          If true, consider the shortcut as a characther one,
 *          otherwise a non-character one like F12.
 *
 * @return XULKeyElement
 */
function createKey(doc, l10nKey, command, key) {
  let k = doc.createElement("key");
  k.id = "key_" + key.id;
  let shortcut = l10n(l10nKey + ".key");
  if (shortcut.startsWith("VK_")) {
    k.setAttribute("keycode", shortcut);
    k.setAttribute("keytext", l10n(l10nKey + ".keytext"));
  } else {
    k.setAttribute("key", shortcut);
  }
  if (command) {
    k.setAttribute("command", command);
  }
  if (key.modifiers) {
    k.setAttribute("modifiers", key.modifiers);
  }
  return k;
}

/**
 * Create a xul:menuitem element
 *
 * @param {XULDocument} doc
 *        The document to which keys are to be added.
 * @param {String} id
 *        Element id.
 * @param {String} label
 *        Menu label.
 * @param {String} broadcasterId (optional)
 *        Id of the xul:broadcaster to map to.
 * @param {String} accesskey (optional)
 *        Access key of the menuitem, used as shortcut while opening the menu.
 * @param {Boolean} isCheckbox
 *        If true, the menuitem will act as a checkbox and have an optional
 *        tick on its left.
 *
 * @return XULMenuItemElement
 */
function createMenuItem({ doc, id, label, broadcasterId, accesskey, isCheckbox }) {
  let menuitem = doc.createElement("menuitem");
  menuitem.id = id;
  if (label) {
    menuitem.setAttribute("label", label);
  }
  if (broadcasterId) {
    menuitem.setAttribute("observes", broadcasterId);
  }
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
 * Create a xul:broadcaster element
 *
 * @param {XULDocument} doc
 *        The document to which keys are to be added.
 * @param {String} id
 *        Element id.
 * @param {String} label
 *        Broadcaster label.
 * @param {Boolean} isCheckbox
 *        If true, the broadcaster is a checkbox one.
 *
 * @return XULMenuItemElement
 */
function createBroadcaster({ doc, id, label, isCheckbox }) {
  let broadcaster = doc.createElement("broadcaster");
  broadcaster.id = id;
  broadcaster.setAttribute("label", label);
  if (isCheckbox) {
    broadcaster.setAttribute("type", "checkbox");
    broadcaster.setAttribute("autocheck", "false");
  }
  return broadcaster;
}

/**
 * Create a xul:command element
 *
 * @param {XULDocument} doc
 *        The document to which keys are to be added.
 * @param {String} id
 *        Element id.
 * @param {String} oncommand
 *        JS String to run when the command is fired.
 * @param {Boolean} disabled
 *        If true, the command is disabled and hidden.
 *
 * @return XULCommandElement
 */
function createCommand({ doc, id, oncommand, disabled }) {
  let command = doc.createElement("command");
  command.id = id;
  command.setAttribute("oncommand", oncommand);
  if (disabled) {
    command.setAttribute("disabled", "true");
    command.setAttribute("hidden", "true");
  }
  return command;
}

/**
 * Add a <key> to <keyset id="devtoolsKeyset">.
 * Appending a <key> element is not always enough. The <keyset> needs
 * to be detached and reattached to make sure the <key> is taken into
 * account (see bug 832984).
 *
 * @param {XULDocument} doc
 *        The document to which keys are to be added
 * @param {XULElement} or {DocumentFragment} keys
 *        Keys to add
 */
function attachKeybindingsToBrowser(doc, keys) {
  let devtoolsKeyset = doc.getElementById("devtoolsKeyset");

  if (!devtoolsKeyset) {
    devtoolsKeyset = doc.createElement("keyset");
    devtoolsKeyset.setAttribute("id", "devtoolsKeyset");
  }
  devtoolsKeyset.appendChild(keys);
  let mainKeyset = doc.getElementById("mainKeyset");
  mainKeyset.parentNode.insertBefore(devtoolsKeyset, mainKeyset);
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
  let id = toolDefinition.id;

  // Prevent multiple entries for the same tool.
  if (doc.getElementById("Tools:" + id)) {
    return;
  }

  let cmd = createCommand({
    doc,
    id: "Tools:" + id,
    oncommand: 'gDevToolsBrowser.selectToolCommand(gBrowser, "' + id + '");',
  });

  let key = null;
  if (toolDefinition.key) {
    key = doc.createElement("key");
    key.id = "key_" + id;

    if (toolDefinition.key.startsWith("VK_")) {
      key.setAttribute("keycode", toolDefinition.key);
    } else {
      key.setAttribute("key", toolDefinition.key);
    }

    key.setAttribute("command", cmd.id);
    key.setAttribute("modifiers", toolDefinition.modifiers);
  }

  let bc = createBroadcaster({
    doc,
    id: "devtoolsMenuBroadcaster_" + id,
    label: toolDefinition.menuLabel || toolDefinition.label
  });
  bc.setAttribute("command", cmd.id);

  if (key) {
    bc.setAttribute("key", "key_" + id);
  }

  let menuitem = createMenuItem({
    doc,
    id: "menuitem_" + id,
    broadcasterId: "devtoolsMenuBroadcaster_" + id,
    accesskey: toolDefinition.accesskey
  });

  return {
    cmd: cmd,
    key: key,
    bc: bc,
    menuitem: menuitem
  };
}

/**
 * Create xul menuitem, command, broadcaster and key elements for a given tool.
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
  let elements = createToolMenuElements(toolDefinition, doc);

  doc.getElementById("mainCommandSet").appendChild(elements.cmd);

  if (elements.key) {
    attachKeybindingsToBrowser(doc, elements.key);
  }

  doc.getElementById("mainBroadcasterSet").appendChild(elements.bc);

  let ref;
  if (prevDef) {
    let menuitem = doc.getElementById("menuitem_" + prevDef.id);
    ref = menuitem && menuitem.nextSibling ? menuitem.nextSibling : null;
  } else {
    ref = doc.getElementById("menu_devtools_separator");
  }

  if (ref) {
    ref.parentNode.insertBefore(elements.menuitem, ref);
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
  let command = doc.getElementById("Tools:" + toolId);
  if (command) {
    command.parentNode.removeChild(command);
  }

  let key = doc.getElementById("key_" + toolId);
  if (key) {
    key.parentNode.removeChild(key);
  }

  let bc = doc.getElementById("devtoolsMenuBroadcaster_" + toolId);
  if (bc) {
    bc.parentNode.removeChild(bc);
  }

  let menuitem = doc.getElementById("menuitem_" + toolId);
  if (menuitem) {
    menuitem.parentNode.removeChild(menuitem);
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
  let fragCommands = doc.createDocumentFragment();
  let fragKeys = doc.createDocumentFragment();
  let fragBroadcasters = doc.createDocumentFragment();
  let fragMenuItems = doc.createDocumentFragment();

  for (let toolDefinition of gDevTools.getToolDefinitionArray()) {
    if (!toolDefinition.inMenu) {
      continue;
    }

    let elements = createToolMenuElements(toolDefinition, doc);

    if (!elements) {
      continue;
    }

    fragCommands.appendChild(elements.cmd);
    if (elements.key) {
      fragKeys.appendChild(elements.key);
    }
    fragBroadcasters.appendChild(elements.bc);
    fragMenuItems.appendChild(elements.menuitem);
  }

  let mcs = doc.getElementById("mainCommandSet");
  mcs.appendChild(fragCommands);

  attachKeybindingsToBrowser(doc, fragKeys);

  let mbs = doc.getElementById("mainBroadcasterSet");
  mbs.appendChild(fragBroadcasters);

  let mps = doc.getElementById("menu_devtools_separator");
  if (mps) {
    mps.parentNode.insertBefore(fragMenuItems, mps);
  }
}

/**
 * Add global menus and shortcuts that are not panel specific.
 *
 * @param {XULDocument} doc
 *        The document to which keys and menus are to be added.
 */
function addTopLevelItems(doc) {
  let keys = doc.createDocumentFragment();
  let menuItems = doc.createDocumentFragment();

  let { menuitems } = require("../menus");
  for (let item of menuitems) {
    if (item.separator) {
      let separator = doc.createElement("menuseparator");
      separator.id = item.id;
      menuItems.appendChild(separator);
    } else {
      let { id, l10nKey } = item;

      // Create a <menuitem>
      let menuitem = createMenuItem({
        doc,
        id,
        label: l10n(l10nKey + ".label"),
        accesskey: l10n(l10nKey + ".accesskey"),
        isCheckbox: item.checkbox
      });
      menuitem.addEventListener("command", item.oncommand);
      menuItems.appendChild(menuitem);

      if (item.key && l10nKey) {
        // Create a <key>
        let key = createKey(doc, l10nKey, null, item.key);
        // Bug 371900: command event is fired only if "oncommand" attribute is set.
        key.setAttribute("oncommand", ";");
        key.addEventListener("command", item.oncommand);
        // Refer to the key in order to display the key shortcut at menu ends
        menuitem.setAttribute("key", key.id);
        keys.appendChild(key);
      }
      if (item.additionalKeys) {
        // Create additional <key>
        for (let key of item.additionalKeys) {
          let node = createKey(doc, key.l10nKey, null, key);
          // Bug 371900: command event is fired only if "oncommand" attribute is set.
          node.setAttribute("oncommand", ";");
          node.addEventListener("command", item.oncommand);
          keys.appendChild(node);
        }
      }
    }
  }

  // Cache all nodes before insertion to be able to remove them on unload
  let nodes = [];
  for(let node of keys.children) {
    nodes.push(node);
  }
  for(let node of menuItems.children) {
    nodes.push(node);
  }
  FragmentsCache.set(doc, nodes);

  attachKeybindingsToBrowser(doc, keys);

  let menu = doc.getElementById("menuWebDeveloperPopup");
  menu.appendChild(menuItems);

  // There is still "Page Source" menuitem hardcoded into browser.xul. Instead
  // of manually inserting everything around it, move it to the expected
  // position.
  let pageSource = doc.getElementById("menu_pageSource");
  let endSeparator = doc.getElementById("devToolsEndSeparator");
  menu.insertBefore(pageSource, endSeparator);
}

/**
 * Remove global menus and shortcuts that are not panel specific.
 *
 * @param {XULDocument} doc
 *        The document to which keys and menus are to be added.
 */
function removeTopLevelItems(doc) {
  let nodes = FragmentsCache.get(doc);
  if (!nodes) {
    return;
  }
  FragmentsCache.delete(doc);
  for (let node of nodes) {
    node.remove();
  }
}

/**
 * Add menus and shortcuts to a browser document
 *
 * @param {XULDocument} doc
 *        The document to which keys and menus are to be added.
 */
exports.addMenus = function (doc) {
  addTopLevelItems(doc);

  addAllToolsToMenu(doc);
};

/**
 * Remove menus and shortcuts from a browser document
 *
 * @param {XULDocument} doc
 *        The document to which keys and menus are to be removed.
 */
exports.removeMenus = function (doc) {
  // We only remove top level entries. Per-tool entries are removed while
  // unregistering each tool.
  removeTopLevelItems(doc);
};
