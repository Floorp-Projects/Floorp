/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { getMessage } = require("devtools/client/webconsole/selectors/messages");

const {
  createContextMenu,
} = require("devtools/client/webconsole/utils/context-menu");

const {
  createEditContextMenu,
} = require("devtools/client/framework/toolbox-context-menu");

const ObjectClient = require("devtools/shared/client/object-client");
const LongStringClient = require("devtools/shared/client/long-string-client");

loader.lazyRequireGetter(
  this,
  "getElementText",
  "devtools/client/webconsole/utils/clipboard",
  true
);

// webConsoleUI

function setupServiceContainer({
  webConsoleUI,
  actions,
  debuggerClient,
  hud,
  toolbox,
  webconsoleWrapper,
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
    openLink: (url, e) => {
      webConsoleUI.hud.openLink(url, e);
    },
    canRewind: () => {
      const target = webConsoleUI.hud && webConsoleUI.hud.currentTarget;
      const traits = target && target.traits;
      return traits && traits.canRewind;
    },
    createElement: nodename => {
      return webconsoleWrapper.document.createElement(nodename);
    },
    fetchObjectProperties: async (grip, ignoreNonIndexedProperties) => {
      const client = new ObjectClient(hud.currentTarget.client, grip);
      const { iterator } = await client.enumProperties({
        ignoreNonIndexedProperties,
      });
      const { ownProperties } = await iterator.slice(0, iterator.count);
      return ownProperties;
    },
    fetchObjectEntries: async grip => {
      const client = new ObjectClient(hud.currentTarget.client, grip);
      const { iterator } = await client.enumEntries();
      const { ownProperties } = await iterator.slice(0, iterator.count);
      return ownProperties;
    },
    getLongString: grip => {
      const proxy = webConsoleUI.getProxy();
      return proxy.webConsoleClient.getString(grip);
    },
    requestData: (id, type) => {
      return webconsoleWrapper.networkDataProvider.requestData(id, type);
    },
    onViewSource(frame) {
      if (webConsoleUI && webConsoleUI.hud && webConsoleUI.hud.viewSource) {
        webConsoleUI.hud.viewSource(frame.url, frame.line);
      }
    },
    recordTelemetryEvent: (eventName, extra = {}) => {
      webconsoleWrapper.telemetry.recordEvent(eventName, "webconsole", null, {
        ...extra,
        session_id: (toolbox && toolbox.sessionId) || -1,
      });
    },
    createObjectClient: object => {
      return new ObjectClient(debuggerClient, object);
    },

    createLongStringClient: object => {
      return new LongStringClient(debuggerClient, object);
    },

    releaseActor: actor => {
      if (!actor) {
        return null;
      }

      return debuggerClient.release(actor);
    },

    /**
     * Retrieve the FrameActor ID given a frame depth, or the selected one if no
     * frame depth given.
     *
     * @return { frameActor: String|null, client: Object }:
     *         frameActor is the FrameActor ID for the given frame depth
     *         (or the selected frame if it exists), null if no frame was found.
     *         client is the WebConsole client for the thread the frame is
     *         associated with.
     */
    getFrameActor: () => {
      const state = hud.getDebuggerFrames();
      if (!state) {
        return { frameActor: null, client: webConsoleUI.webConsoleClient };
      }

      const grip = state.frames[state.selected];

      if (!grip) {
        return { frameActor: null, client: webConsoleUI.webConsoleClient };
      }

      return {
        frameActor: grip.actor,
        client: state.target.activeConsole,
      };
    },

    inputHasSelection: () => {
      const { editor } = webConsoleUI.jsterm || {};
      return editor && !!editor.getSelection();
    },

    getInputValue: () => {
      return hud.getInputValue();
    },

    getInputSelection: () => {
      if (!webConsoleUI.jsterm || !webConsoleUI.jsterm.editor) {
        return null;
      }
      return webConsoleUI.jsterm.editor.getSelection();
    },

    setInputValue: value => {
      hud.setInputValue(value);
    },

    focusInput: () => {
      return webConsoleUI.jsterm && webConsoleUI.jsterm.focus();
    },

    requestEvaluation: (string, options) => {
      return webConsoleUI.evaluateJSAsync(string, options);
    },

    getInputCursor: () => {
      return webConsoleUI.jsterm && webConsoleUI.jsterm.getSelectionStart();
    },

    getSelectedNodeActor: () => {
      const inspectorSelection = hud.getInspectorSelection();
      if (inspectorSelection && inspectorSelection.nodeFront) {
        return inspectorSelection.nodeFront.actorID;
      }
      return null;
    },

    getJsTermTooltipAnchor: () => {
      return webConsoleUI.outputNode.querySelector(".CodeMirror-cursor");
    },
    getMappedExpression: hud.getMappedExpression.bind(hud),
    getPanelWindow: () => webConsoleUI.window,
    inspectObjectActor: objectActor => {
      if (toolbox) {
        toolbox.inspectObjectActor(objectActor);
      } else {
        webConsoleUI.inspectObjectActor(objectActor);
      }
    },
  };

  // Set `openContextMenu` this way so, `serviceContainer` variable
  // is available in the current scope and we can pass it into
  // `createContextMenu` method.
  serviceContainer.openContextMenu = (e, message) => {
    const { screenX, screenY, target } = e;

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

    const sidebarTogglePref = webconsoleWrapper.getStore().getState().prefs
      .sidebarToggle;
    const openSidebar = sidebarTogglePref
      ? messageId => {
          webconsoleWrapper
            .getStore()
            .dispatch(
              actions.showMessageObjectInSidebar(rootActorId, messageId)
            );
        }
      : null;

    const messageData = getMessage(
      webconsoleWrapper.getStore().getState(),
      message.messageId
    );
    const executionPoint = messageData && messageData.executionPoint;

    const menu = createContextMenu(
      webconsoleWrapper.webConsoleUI,
      webconsoleWrapper.parentNode,
      {
        actor,
        clipboardText,
        variableText,
        message,
        serviceContainer,
        openSidebar,
        rootActorId,
        executionPoint,
        toolbox: toolbox,
        url,
      }
    );

    // Emit the "menu-open" event for testing.
    menu.once("open", () => webconsoleWrapper.emit("menu-open"));
    menu.popup(screenX, screenY, hud.chromeWindow.document);

    return menu;
  };

  serviceContainer.openEditContextMenu = e => {
    const { screenX, screenY } = e;
    const menu = createEditContextMenu(window, "webconsole-menu");
    // Emit the "menu-open" event for testing.
    menu.once("open", () => webconsoleWrapper.emit("menu-open"));
    menu.popup(screenX, screenY, hud.chromeWindow.document);

    return menu;
  };

  if (toolbox) {
    const { highlight, unhighlight } = toolbox.getHighlighter(true);

    Object.assign(serviceContainer, {
      onViewSourceInDebugger: frame => {
        toolbox
          .viewSourceInDebugger(
            frame.url,
            frame.line,
            frame.column,
            frame.sourceId
          )
          .then(() => {
            webconsoleWrapper.telemetry.recordEvent(
              "jump_to_source",
              "webconsole",
              null,
              {
                session_id: toolbox.sessionId,
              }
            );
            webconsoleWrapper.webConsoleUI.emit("source-in-debugger-opened");
          });
      },
      onViewSourceInScratchpad: frame =>
        toolbox.viewSourceInScratchpad(frame.url, frame.line).then(() => {
          webconsoleWrapper.telemetry.recordEvent(
            "jump_to_source",
            "webconsole",
            null,
            {
              session_id: toolbox.sessionId,
            }
          );
        }),
      onViewSourceInStyleEditor: frame =>
        toolbox
          .viewSourceInStyleEditor(frame.url, frame.line, frame.column)
          .then(() => {
            webconsoleWrapper.telemetry.recordEvent(
              "jump_to_source",
              "webconsole",
              null,
              {
                session_id: toolbox.sessionId,
              }
            );
          }),
      openNetworkPanel: requestId => {
        return toolbox.selectTool("netmonitor").then(panel => {
          return panel.panelWin.Netmonitor.inspectRequest(requestId);
        });
      },
      resendNetworkRequest: requestId => {
        return toolbox.getNetMonitorAPI().then(api => {
          return api.resendRequest(requestId);
        });
      },
      sourceMapService: toolbox ? toolbox.sourceMapURLService : null,
      highlightDomElement: highlight,
      unHighlightDomElement: unhighlight,
      openNodeInInspector: async grip => {
        const onSelectInspector = toolbox.selectTool(
          "inspector",
          "inspect_dom"
        );
        // TODO: Bug1574506 - Use the contextual WalkerFront for gripToNodeFront.
        const walkerFront = (await toolbox.target.getFront("inspector")).walker;
        const onGripNodeToFront = walkerFront.gripToNodeFront(grip);
        const [front, inspector] = await Promise.all([
          onGripNodeToFront,
          onSelectInspector,
        ]);

        const onInspectorUpdated = inspector.once("inspector-updated");
        const onNodeFrontSet = toolbox.selection.setNodeFront(front, {
          reason: "console",
        });

        return Promise.all([onNodeFrontSet, onInspectorUpdated]);
      },
      jumpToExecutionPoint: executionPoint =>
        toolbox.threadFront.timeWarp(executionPoint),

      onMessageHover: (type, messageId) => {
        const message = getMessage(
          webconsoleWrapper.getStore().getState(),
          messageId
        );
        webconsoleWrapper.webConsoleUI.emit("message-hover", type, message);
      },
    });
  }

  return serviceContainer;
}

module.exports.setupServiceContainer = setupServiceContainer;
