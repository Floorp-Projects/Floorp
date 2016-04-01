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

loader.lazyRequireGetter(this, "gDevTools", "devtools/client/framework/devtools", true);

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

  let cmd = doc.createElement("command");
  cmd.id = "Tools:" + id;
  cmd.setAttribute("oncommand",
      'gDevToolsBrowser.selectToolCommand(gBrowser, "' + id + '");');

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

  let bc = doc.createElement("broadcaster");
  bc.id = "devtoolsMenuBroadcaster_" + id;
  bc.setAttribute("label", toolDefinition.menuLabel || toolDefinition.label);
  bc.setAttribute("command", cmd.id);

  if (key) {
    bc.setAttribute("key", "key_" + id);
  }

  let menuitem = doc.createElement("menuitem");
  menuitem.id = "menuitem_" + id;
  menuitem.setAttribute("observes", "devtoolsMenuBroadcaster_" + id);

  if (toolDefinition.accesskey) {
    menuitem.setAttribute("accesskey", toolDefinition.accesskey);
  }

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
 * Detect the presence of a Firebug.
 */
function isFirebugInstalled() {
  let bootstrappedAddons = Services.prefs.getCharPref("extensions.bootstrappedAddons");
  return bootstrappedAddons.indexOf("firebug@software.joehewitt.com") != -1;
}

/**
 * Add menus and shortcuts to a browser document
 *
 * @param {XULDocument} doc
 *        The document to which keys and menus are to be added.
 */
exports.addMenus = function (doc) {
  addAllToolsToMenu(doc);

  if (isFirebugInstalled()) {
    let broadcaster = doc.getElementById("devtoolsMenuBroadcaster_DevToolbox");
    broadcaster.removeAttribute("key");
  }
};
