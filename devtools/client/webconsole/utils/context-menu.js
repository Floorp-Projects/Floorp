/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Menu = require("devtools/client/framework/menu");
const MenuItem = require("devtools/client/framework/menu-item");

const { MESSAGE_SOURCE } = require("devtools/client/webconsole/constants");

const clipboardHelper = require("devtools/shared/platform/clipboard");
const { l10n } = require("devtools/client/webconsole/utils/messages");
const actions = require("devtools/client/webconsole/actions/index");

loader.lazyRequireGetter(this, "saveAs", "devtools/shared/DevToolsUtils", true);
loader.lazyRequireGetter(
  this,
  "openContentLink",
  "devtools/client/shared/link",
  true
);
loader.lazyRequireGetter(
  this,
  "getElementText",
  "devtools/client/webconsole/utils/clipboard",
  true
);

/**
 * Create a Menu instance for the webconsole.
 *
 * @param {Event} context menu event
 *        {Object} message (optional) message object containing metadata such as:
 *        - {String} source
 *        - {String} request
 * @param {Object} options
 *        - {Actions} bound actions
 *        - {WebConsoleWrapper} wrapper instance used for accessing properties like the store
 *          and window.
 */
function createContextMenu(event, message, webConsoleWrapper) {
  const { target } = event;
  const { parentNode, toolbox, hud } = webConsoleWrapper;
  const store = webConsoleWrapper.getStore();
  const { dispatch } = store;

  const messageEl = target.closest(".message");
  const clipboardText = getElementText(messageEl);

  const linkEl = target.closest("a[href]");
  const url = linkEl && linkEl.href;

  const messageVariable = target.closest(".objectBox");
  // Ensure that console.group and console.groupCollapsed commands are not captured
  const variableText =
    messageVariable &&
    !messageEl.classList.contains("startGroup") &&
    !messageEl.classList.contains("startGroupCollapsed")
      ? messageVariable.textContent
      : null;

  // Retrieve closes actor id from the DOM.
  const actorEl =
    target.closest("[data-link-actor-id]") ||
    target.querySelector("[data-link-actor-id]");
  const actor = actorEl ? actorEl.dataset.linkActorId : null;

  const rootObjectInspector = target.closest(".object-inspector");
  const rootActor = rootObjectInspector
    ? rootObjectInspector.querySelector("[data-link-actor-id]")
    : null;
  const rootActorId = rootActor ? rootActor.dataset.linkActorId : null;

  const elementNode =
    target.closest(".objectBox-node") || target.closest(".objectBox-textNode");
  const isConnectedElement =
    elementNode && elementNode.querySelector(".open-inspector") !== null;

  const win = parentNode.ownerDocument.defaultView;
  const selection = win.getSelection();

  const { source, request, messageId } = message || {};

  const menu = new Menu({ id: "webconsole-menu" });

  // Copy URL for a network request.
  menu.append(
    new MenuItem({
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
    })
  );

  if (toolbox && request) {
    // Open Network message in the Network panel.
    menu.append(
      new MenuItem({
        id: "console-menu-open-in-network-panel",
        label: l10n.getStr("webconsole.menu.openInNetworkPanel.label"),
        accesskey: l10n.getStr("webconsole.menu.openInNetworkPanel.accesskey"),
        visible: source === MESSAGE_SOURCE.NETWORK,
        click: () => dispatch(actions.openNetworkPanel(message.messageId)),
      })
    );
    // Resend Network message.
    menu.append(
      new MenuItem({
        id: "console-menu-resend-network-request",
        label: l10n.getStr("webconsole.menu.resendNetworkRequest.label"),
        accesskey: l10n.getStr(
          "webconsole.menu.resendNetworkRequest.accesskey"
        ),
        visible: source === MESSAGE_SOURCE.NETWORK,
        click: () => dispatch(actions.resendNetworkRequest(messageId)),
      })
    );
  }

  // Open URL in a new tab for a network request.
  menu.append(
    new MenuItem({
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
    })
  );

  // Open DOM node in the Inspector panel.
  if (isConnectedElement) {
    menu.append(
      new MenuItem({
        id: "console-menu-open-node",
        label: l10n.getStr("webconsole.menu.openNodeInInspector.label"),
        accesskey: l10n.getStr("webconsole.menu.openNodeInInspector.accesskey"),
        disabled: false,
        click: () => dispatch(actions.openNodeInInspector(actor)),
      })
    );
  }

  // Store as global variable.
  menu.append(
    new MenuItem({
      id: "console-menu-store",
      label: l10n.getStr("webconsole.menu.storeAsGlobalVar.label"),
      accesskey: l10n.getStr("webconsole.menu.storeAsGlobalVar.accesskey"),
      disabled: !actor,
      click: () => dispatch(actions.storeAsGlobal(actor)),
    })
  );

  // Copy message or grip.
  menu.append(
    new MenuItem({
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
    })
  );

  // Copy message object.
  menu.append(
    new MenuItem({
      id: "console-menu-copy-object",
      label: l10n.getStr("webconsole.menu.copyObject.label"),
      accesskey: l10n.getStr("webconsole.menu.copyObject.accesskey"),
      // Disabled if there is no actor and no variable text associated.
      disabled: !actor && !variableText,
      click: () => dispatch(actions.copyMessageObject(actor, variableText)),
    })
  );

  // Select all.
  menu.append(
    new MenuItem({
      id: "console-menu-select",
      label: l10n.getStr("webconsole.menu.selectAll.label"),
      accesskey: l10n.getStr("webconsole.menu.selectAll.accesskey"),
      disabled: false,
      click: () => {
        const webconsoleOutput = parentNode.querySelector(".webconsole-output");
        selection.selectAllChildren(webconsoleOutput);
      },
    })
  );

  const exportSubmenu = new Menu({
    id: "export-submenu",
  });

  // Export to clipboard
  exportSubmenu.append(
    new MenuItem({
      id: "console-menu-export-clipboard",
      label: l10n.getStr("webconsole.menu.exportSubmenu.exportCliboard.label"),
      disabled: false,
      click: () => {
        const webconsoleOutput = parentNode.querySelector(".webconsole-output");
        clipboardHelper.copyString(getElementText(webconsoleOutput));
      },
    })
  );

  // Export to file
  exportSubmenu.append(
    new MenuItem({
      id: "console-menu-export-file",
      label: l10n.getStr("webconsole.menu.exportSubmenu.exportFile.label"),
      disabled: false,
      // Note: not async, but returns a promise for the actual save.
      click: () => {
        const date = new Date();
        const suggestedName =
          `console-export-${date.getFullYear()}-` +
          `${date.getMonth() + 1}-${date.getDate()}_${date.getHours()}-` +
          `${date.getMinutes()}-${date.getSeconds()}.txt`;
        const webconsoleOutput = parentNode.querySelector(".webconsole-output");
        const data = new TextEncoder().encode(getElementText(webconsoleOutput));
        return saveAs(window, data, suggestedName);
      },
    })
  );

  menu.append(
    new MenuItem({
      id: "console-menu-export",
      label: l10n.getStr("webconsole.menu.exportSubmenu.label"),
      disabled: false,
      submenu: exportSubmenu,
    })
  );

  // Open object in sidebar.
  const shouldOpenSidebar = store.getState().prefs.sidebarToggle;
  if (shouldOpenSidebar) {
    menu.append(
      new MenuItem({
        id: "console-menu-open-sidebar",
        label: l10n.getStr("webconsole.menu.openInSidebar.label"),
        accesskey: l10n.getStr("webconsole.menu.openInSidebar.accesskey"),
        disabled: !rootActorId,
        click: () => dispatch(actions.openSidebar(messageId, rootActorId)),
      })
    );
  }

  if (url) {
    menu.append(
      new MenuItem({
        id: "console-menu-open-url",
        label: l10n.getStr("webconsole.menu.openURL.label"),
        accesskey: l10n.getStr("webconsole.menu.openURL.accesskey"),
        click: () =>
          openContentLink(url, {
            inBackground: true,
            relatedToCurrent: true,
          }),
      })
    );
    menu.append(
      new MenuItem({
        id: "console-menu-copy-url",
        label: l10n.getStr("webconsole.menu.copyURL.label"),
        accesskey: l10n.getStr("webconsole.menu.copyURL.accesskey"),
        click: () => clipboardHelper.copyString(url),
      })
    );
  }

  // Emit the "menu-open" event for testing.
  const { screenX, screenY } = event;
  menu.once("open", () => webConsoleWrapper.emitForTests("menu-open"));
  menu.popup(screenX, screenY, hud.chromeWindow.document);

  return menu;
}

exports.createContextMenu = createContextMenu;
