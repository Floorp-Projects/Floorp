/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Menu = require("devtools/client/framework/menu");
const MenuItem = require("devtools/client/framework/menu-item");

const { MESSAGE_SOURCE } = require("devtools/client/webconsole/constants");

const clipboardHelper = require("devtools/shared/platform/clipboard");
const { l10n } = require("devtools/client/webconsole/utils/messages");

loader.lazyRequireGetter(this, "openContentLink", "devtools/client/shared/link", true);

/**
 * Create a Menu instance for the webconsole.
 *
 * @param {Object} hud
 *        The webConsoleFrame.
 * @param {Element} parentNode
 *        The container of the new console frontend output wrapper.
 * @param {Object} options
 *        - {String} actor (optional) actor id to use for context menu actions
 *        - {String} clipboardText (optional) text to "Copy" if no selection is available
 *        - {String} variableText (optional) which is the textual frontend
 *            representation of the variable
 *        - {Object} message (optional) message object containing metadata such as:
 *          - {String} source
 *          - {String} request
 *        - {Function} openSidebar (optional) function that will open the object
 *            inspector sidebar
 *        - {String} rootActorId (optional) actor id for the root object being clicked on
 */
function createContextMenu(hud, parentNode, {
  actor,
  clipboardText,
  variableText,
  message,
  serviceContainer,
  openSidebar,
  rootActorId,
}) {
  const win = parentNode.ownerDocument.defaultView;
  const selection = win.getSelection();

  const { source, request } = message || {};

  const menu = new Menu({
    id: "webconsole-menu"
  });

  // Copy URL for a network request.
  menu.append(new MenuItem({
    id: "console-menu-copy-url",
    label: l10n.getStr("webconsole.menu.copyURL.label"),
    accesskey: l10n.getStr("webconsole.menu.copyURL.accesskey"),
    visible: source === MESSAGE_SOURCE.NETWORK,
    click: () => {
      if (!request) {
        return;
      }
      clipboardHelper.copyString(request.url);
    },
  }));

  // Open Network message in the Network panel.
  if (serviceContainer.openNetworkPanel && request) {
    menu.append(new MenuItem({
      id: "console-menu-open-in-network-panel",
      label: l10n.getStr("webconsole.menu.openInNetworkPanel.label"),
      accesskey: l10n.getStr("webconsole.menu.openInNetworkPanel.accesskey"),
      visible: source === MESSAGE_SOURCE.NETWORK,
      click: () => serviceContainer.openNetworkPanel(message.messageId)
    }));
  }

  // Open URL in a new tab for a network request.
  menu.append(new MenuItem({
    id: "console-menu-open-url",
    label: l10n.getStr("webconsole.menu.openURL.label"),
    accesskey: l10n.getStr("webconsole.menu.openURL.accesskey"),
    visible: source === MESSAGE_SOURCE.NETWORK,
    click: () => {
      if (!request) {
        return;
      }
      openContentLink(request.url);
    },
  }));

  // Store as global variable.
  menu.append(new MenuItem({
    id: "console-menu-store",
    label: l10n.getStr("webconsole.menu.storeAsGlobalVar.label"),
    accesskey: l10n.getStr("webconsole.menu.storeAsGlobalVar.accesskey"),
    disabled: !actor,
    click: () => {
      const evalString = `{ let i = 0;
        while (this.hasOwnProperty("temp" + i) && i < 1000) {
          i++;
        }
        this["temp" + i] = _self;
        "temp" + i;
      }`;
      const options = {
        selectedObjectActor: actor,
      };

      hud.jsterm.requestEvaluation(evalString, options).then((res) => {
        hud.jsterm.focus();
        hud.jsterm.setInputValue(res.result);
      });
    },
  }));

  // Copy message or grip.
  menu.append(new MenuItem({
    id: "console-menu-copy",
    label: l10n.getStr("webconsole.menu.copyMessage.label"),
    accesskey: l10n.getStr("webconsole.menu.copyMessage.accesskey"),
    // Disabled if there is no selection and no message element available to copy.
    disabled: selection.isCollapsed && !clipboardText,
    click: () => {
      if (selection.isCollapsed) {
        // If the selection is empty/collapsed, copy the text content of the
        // message for which the context menu was opened.
        clipboardHelper.copyString(clipboardText);
      } else {
        clipboardHelper.copyString(selection.toString());
      }
    },
  }));

  // Copy message object.
  menu.append(new MenuItem({
    id: "console-menu-copy-object",
    label: l10n.getStr("webconsole.menu.copyObject.label"),
    accesskey: l10n.getStr("webconsole.menu.copyObject.accesskey"),
    // Disabled if there is no actor and no variable text associated.
    disabled: (!actor && !variableText),
    click: () => {
      if (actor) {
        // The Debugger.Object of the OA will be bound to |_self| during evaluation,
        hud.jsterm.copyObject(`_self`, { selectedObjectActor: actor }).then((res) => {
          clipboardHelper.copyString(res.helperResult.value);
        });
      } else {
        clipboardHelper.copyString(variableText);
      }
    },
  }));

  // Select all.
  menu.append(new MenuItem({
    id: "console-menu-select",
    label: l10n.getStr("webconsole.menu.selectAll.label"),
    accesskey: l10n.getStr("webconsole.menu.selectAll.accesskey"),
    disabled: false,
    click: () => {
      const webconsoleOutput = parentNode.querySelector(".webconsole-output");
      selection.selectAllChildren(webconsoleOutput);
    },
  }));

  // Open object in sidebar.
  if (openSidebar) {
    menu.append(new MenuItem({
      id: "console-menu-open-sidebar",
      label: l10n.getStr("webconsole.menu.openInSidebar.label"),
      accesskey: l10n.getStr("webconsole.menu.openInSidebar.accesskey"),
      disabled: !rootActorId,
      click: () => openSidebar(message.messageId),
    }));
  }

  return menu;
}

exports.createContextMenu = createContextMenu;
