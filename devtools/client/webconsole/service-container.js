/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createContextMenu,
} = require("devtools/client/webconsole/utils/context-menu");

const {
  createEditContextMenu,
} = require("devtools/client/framework/toolbox-context-menu");

function setupServiceContainer({
  webConsoleUI,
  hud,
  toolbox,
  webConsoleWrapper,
}) {
  const serviceContainer = {
    openContextMenu: (event, message) =>
      createContextMenu(event, message, webConsoleWrapper),

    openEditContextMenu: event => {
      const { screenX, screenY } = event;
      const menu = createEditContextMenu(window, "webconsole-menu");
      // Emit the "menu-open" event for testing.
      menu.once("open", () => webConsoleWrapper.emitForTests("menu-open"));
      menu.popup(screenX, screenY, hud.chromeWindow.document);
    },

    // NOTE these methods are proxied currently because the
    // service container is passed down the tree. These methods should eventually
    // be moved to redux actions.
    recordTelemetryEvent: (event, extra = {}) => hud.recordEvent(event, extra),
    openLink: (url, e) => hud.openLink(url, e),
    openNodeInInspector: grip => hud.openNodeInInspector(grip),
    getInputSelection: () => hud.getInputSelection(),
    onViewSource: location => hud.viewSource(location.url, location.line),
    resendNetworkRequest: requestId => hud.resendNetworkRequest(requestId),
    focusInput: () => hud.focusInput(),
    setInputValue: value => hud.setInputValue(value),
    getLongString: grip => webConsoleUI.getLongString(grip),
    getJsTermTooltipAnchor: () => webConsoleUI.getJsTermTooltipAnchor(),
    emitForTests: (event, value) => webConsoleUI.emitForTests(event, value),
    attachRefToWebConsoleUI: (id, node) => webConsoleUI.attachRef(id, node),
    requestData: (id, type) =>
      webConsoleUI.networkDataProvider.requestData(id, type),
    createElement: nodename => webConsoleWrapper.createElement(nodename),
  };

  if (toolbox) {
    const { highlight, unhighlight } = toolbox.getHighlighter();

    Object.assign(serviceContainer, {
      sourceMapURLService: toolbox.sourceMapURLService,
      highlightDomElement: highlight,
      unHighlightDomElement: unhighlight,
      onViewSourceInDebugger: location => hud.onViewSourceInDebugger(location),
      onViewSourceInStyleEditor: location =>
        hud.onViewSourceInStyleEditor(location),
    });
  }

  return serviceContainer;
}

module.exports.setupServiceContainer = setupServiceContainer;
