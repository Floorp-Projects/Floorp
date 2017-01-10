/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const {gDevTools} = require("devtools/client/framework/devtools");

const Menu = require("devtools/client/framework/menu");
const MenuItem = require("devtools/client/framework/menu-item");

const { MESSAGE_SOURCE } = require("devtools/client/webconsole/new-console-output/constants");

const {openVariablesView} = require("devtools/client/webconsole/new-console-output/utils/variables-view");
const clipboardHelper = require("devtools/shared/platform/clipboard");
const { l10n } = require("devtools/client/webconsole/new-console-output/utils/messages");

/**
 * Create a Menu instance for the webconsole.
 *
 * @param {Object} jsterm
 *        The JSTerm instance used by the webconsole.
 * @param {Element} parentNode
 *        The container of the new console frontend output wrapper.
 * @param {Object} options
 *        - {String} actor (optional) actor id to use for context menu actions
 *        - {String} clipboardText (optional) text to "Copy" if no selection is available
 *        - {Object} message (optional) message object containing metadata such as:
 *          - {String} source
 *          - {String} request
 */
function createContextMenu(jsterm, parentNode, { actor, clipboardText, message }) {
  let win = parentNode.ownerDocument.defaultView;
  let selection = win.getSelection();

  let { source, request } = message || {};

  let menu = new Menu({
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
      let mainWindow = Services.wm.getMostRecentWindow(gDevTools.chromeWindowType);
      mainWindow.openUILinkIn(request.url, "tab");
    },
  }));

  // Open in variables view.
  menu.append(new MenuItem({
    id: "console-menu-open",
    label: l10n.getStr("webconsole.menu.openInVarView.label"),
    accesskey: l10n.getStr("webconsole.menu.openInVarView.accesskey"),
    disabled: !actor,
    click: () => {
      openVariablesView(actor);
    },
  }));

  // Store as global variable.
  menu.append(new MenuItem({
    id: "console-menu-store",
    label: l10n.getStr("webconsole.menu.storeAsGlobalVar.label"),
    accesskey: l10n.getStr("webconsole.menu.storeAsGlobalVar.accesskey"),
    disabled: !actor,
    click: () => {
      let evalString = `{ let i = 0;
        while (this.hasOwnProperty("temp" + i) && i < 1000) {
          i++;
        }
        this["temp" + i] = _self;
        "temp" + i;
      }`;
      let options = {
        selectedObjectActor: actor,
      };

      jsterm.requestEvaluation(evalString, options).then((res) => {
        jsterm.focus();
        jsterm.setInputValue(res.result);
      });
    },
  }));

  // Copy message or grip.
  menu.append(new MenuItem({
    id: "console-menu-copy",
    label: l10n.getStr("webconsole.menu.copy.label"),
    accesskey: l10n.getStr("webconsole.menu.copy.accesskey"),
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

  // Select all.
  menu.append(new MenuItem({
    id: "console-menu-select",
    label: l10n.getStr("webconsole.menu.selectAll.label"),
    accesskey: l10n.getStr("webconsole.menu.selectAll.accesskey"),
    disabled: false,
    click: () => {
      let webconsoleOutput = parentNode.querySelector(".webconsole-output");
      selection.selectAllChildren(webconsoleOutput);
    },
  }));

  return menu;
}

exports.createContextMenu = createContextMenu;
