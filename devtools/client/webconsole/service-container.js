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
  actions,
  store,
  hud,
  toolbox,
  webConsoleWrapper,
}) {
  const attachRefToWebConsoleUI = (id, node) => {
    webConsoleUI[id] = node;
  };

  const serviceContainer = {
    attachRefToWebConsoleUI,
    emitNewMessage: (node, messageId, timeStamp) => {
      webConsoleUI.emit(
        "new-messages",
        new Set([
          {
            node,
            messageId,
            timeStamp,
          },
        ])
      );
    },

    canRewind: () => {
      const target = webConsoleUI.hud && webConsoleUI.hud.currentTarget;
      const traits = target && target.traits;
      return traits && traits.canRewind;
    },

    createElement: nodename => {
      return webConsoleWrapper.document.createElement(nodename);
    },

    getLongString: grip => {
      const proxy = webConsoleUI.getProxy();
      return proxy.webConsoleClient.getString(grip);
    },

    requestData: (id, type) => {
      return webConsoleWrapper.networkDataProvider.requestData(id, type);
    },

    onViewSource(frame) {
      if (webConsoleUI && webConsoleUI.hud && webConsoleUI.hud.viewSource) {
        webConsoleUI.hud.viewSource(frame.url, frame.line);
      }
    },

    getJsTermTooltipAnchor: () => {
      return webConsoleUI.outputNode.querySelector(".CodeMirror-cursor");
    },

    openContextMenu: (event, message) => {
      const menu = createContextMenu(event, message, webConsoleWrapper);

      // Emit the "menu-open" event for testing.
      const { screenX, screenY } = event;
      menu.once("open", () => webConsoleWrapper.emit("menu-open"));
      menu.popup(screenX, screenY, hud.chromeWindow.document);

      return menu;
    },

    openEditContextMenu: e => {
      const { screenX, screenY } = e;
      const menu = createEditContextMenu(window, "webconsole-menu");
      // Emit the "menu-open" event for testing.
      menu.once("open", () => webConsoleWrapper.emit("menu-open"));
      menu.popup(screenX, screenY, hud.chromeWindow.document);

      return menu;
    },

    // NOTE these methods are proxied currently because the
    // service container is passed down the tree. These methods should eventually
    // be moved to redux actions.
    recordTelemetryEvent: (event, extra = {}) => hud.recordEvent(event, extra),
    openLink: (url, e) => hud.openLink(url, e),
    openNodeInInspector: grip => hud.openNodeInInspector(grip),
    onMessageHover: (type, message) => webConsoleUI.onMessageHover(message),
  };

  if (toolbox) {
    const { highlight, unhighlight } = toolbox.getHighlighter(true);

    Object.assign(serviceContainer, {
      sourceMapService: toolbox.sourceMapURLService,
      highlightDomElement: highlight,
      unHighlightDomElement: unhighlight,
      jumpToExecutionPoint: executionPoint =>
        toolbox.threadFront.timeWarp(executionPoint),
      onViewSourceInDebugger: frame => hud.onViewSourceInDebugger(frame),
      onViewSourceInStyleEditor: frame => hud.onViewSourceInStyleEditor(frame),
    });
  }

  return serviceContainer;
}

module.exports.setupServiceContainer = setupServiceContainer;
