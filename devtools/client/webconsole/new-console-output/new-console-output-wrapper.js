/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// React & Redux
const React = require("devtools/client/shared/vendor/react");
const ReactDOM = require("devtools/client/shared/vendor/react-dom");
const { Provider } = require("devtools/client/shared/vendor/react-redux");

const actions = require("devtools/client/webconsole/new-console-output/actions/index");
const { createContextMenu } = require("devtools/client/webconsole/new-console-output/utils/context-menu");
const { configureStore } = require("devtools/client/webconsole/new-console-output/store");

const EventEmitter = require("devtools/shared/event-emitter");
const ConsoleOutput = React.createFactory(require("devtools/client/webconsole/new-console-output/components/console-output"));
const FilterBar = React.createFactory(require("devtools/client/webconsole/new-console-output/components/filter-bar"));

let store = null;
let queuedActions = [];
let throttledDispatchTimeout = false;

function NewConsoleOutputWrapper(parentNode, jsterm, toolbox, owner, document) {
  EventEmitter.decorate(this);

  this.parentNode = parentNode;
  this.jsterm = jsterm;
  this.toolbox = toolbox;
  this.owner = owner;
  this.document = document;

  this.init = this.init.bind(this);

  store = configureStore(this.jsterm.hud);
}

NewConsoleOutputWrapper.prototype = {
  init: function () {
    const attachRefToHud = (id, node) => {
      this.jsterm.hud[id] = node;
    };

    const serviceContainer = {
      attachRefToHud,
      emitNewMessage: (node, messageId) => {
        this.jsterm.hud.emit("new-messages", new Set([{
          node,
          messageId,
        }]));
      },
      hudProxyClient: this.jsterm.hud.proxy.client,
      openContextMenu: (e, message) => {
        let { screenX, screenY, target } = e;

        let messageEl = target.closest(".message");
        let clipboardText = messageEl ? messageEl.textContent : null;

        // Retrieve closes actor id from the DOM.
        let actorEl = target.closest("[data-link-actor-id]");
        let actor = actorEl ? actorEl.dataset.linkActorId : null;

        let menu = createContextMenu(this.jsterm, this.parentNode,
          { actor, clipboardText, message });

        // Emit the "menu-open" event for testing.
        menu.once("open", () => this.emit("menu-open"));
        menu.popup(screenX, screenY, this.toolbox);

        return menu;
      },
      openLink: url => this.jsterm.hud.owner.openLink(url),
      createElement: nodename => {
        return this.document.createElementNS("http://www.w3.org/1999/xhtml", nodename);
      },
    };

    if (this.toolbox) {
      Object.assign(serviceContainer, {
        onViewSourceInDebugger: frame => {
          this.toolbox.viewSourceInDebugger(frame.url, frame.line).then(() =>
            this.jsterm.hud.emit("source-in-debugger-opened")
          );
        },
        onViewSourceInScratchpad: frame => this.toolbox.viewSourceInScratchpad(
          frame.url,
          frame.line
        ),
        onViewSourceInStyleEditor: frame => this.toolbox.viewSourceInStyleEditor(
          frame.url,
          frame.line
        ),
        openNetworkPanel: (requestId) => {
          return this.toolbox.selectTool("netmonitor").then((panel) => {
            let { inspectRequest } = panel.panelWin.windowRequire(
              "devtools/client/netmonitor/src/connector/index");
            return inspectRequest(requestId);
          });
        },
        sourceMapService: this.toolbox ? this.toolbox.sourceMapURLService : null,
        highlightDomElement: (grip, options = {}) => {
          return this.toolbox.highlighterUtils
            ? this.toolbox.highlighterUtils.highlightDomValueGrip(grip, options)
            : null;
        },
        unHighlightDomElement: (forceHide = false) => {
          return this.toolbox.highlighterUtils
            ? this.toolbox.highlighterUtils.unhighlight(forceHide)
            : null;
        },
        openNodeInInspector: async (grip) => {
          let onSelectInspector = this.toolbox.selectTool("inspector");
          let onGripNodeToFront = this.toolbox.highlighterUtils.gripToNodeFront(grip);
          let [
            front,
            inspector
          ] = await Promise.all([onGripNodeToFront, onSelectInspector]);

          let onInspectorUpdated = inspector.once("inspector-updated");
          let onNodeFrontSet = this.toolbox.selection.setNodeFront(front, "console");

          return Promise.all([onNodeFrontSet, onInspectorUpdated]);
        }
      });
    }

    let childComponent = ConsoleOutput({serviceContainer});

    let filterBar = FilterBar({
      serviceContainer: {
        attachRefToHud
      }
    });

    let provider = React.createElement(
      Provider,
      { store },
      React.DOM.div(
        {className: "webconsole-output-wrapper"},
        filterBar,
        childComponent
    ));

    this.body = ReactDOM.render(provider, this.parentNode);
  },

  dispatchMessageAdd: function (message, waitForResponse) {
    let action = actions.messageAdd(message);
    batchedMessageAdd(action);

    // Wait for the message to render to resolve with the DOM node.
    // This is just for backwards compatibility with old tests, and should
    // be removed once it's not needed anymore.
    // Can only wait for response if the action contains a valid message.
    if (waitForResponse && action.message) {
      let messageId = action.message.id;
      return new Promise(resolve => {
        let jsterm = this.jsterm;
        jsterm.hud.on("new-messages", function onThisMessage(e, messages) {
          for (let m of messages) {
            if (m.messageId === messageId) {
              resolve(m.node);
              jsterm.hud.off("new-messages", onThisMessage);
              return;
            }
          }
        });
      });
    }

    return Promise.resolve();
  },

  dispatchMessagesAdd: function (messages) {
    const batchedActions = messages.map(message => actions.messageAdd(message));
    store.dispatch(actions.batchActions(batchedActions));
  },

  dispatchMessagesClear: function () {
    store.dispatch(actions.messagesClear());
  },

  dispatchTimestampsToggle: function (enabled) {
    store.dispatch(actions.timestampsToggle(enabled));
  },

  dispatchMessageUpdate: function (message, res) {
    // network-message-updated will emit when eventTimings message arrives
    // which is the last one of 8 updates happening on network message update.
    if (res.packet.updateType === "eventTimings") {
      batchedMessageAdd(actions.networkMessageUpdate(message));
      this.jsterm.hud.emit("network-message-updated", res);
    }
  },

  // Should be used for test purpose only.
  getStore: function () {
    return store;
  }
};

function batchedMessageAdd(action) {
  queuedActions.push(action);
  if (!throttledDispatchTimeout) {
    throttledDispatchTimeout = setTimeout(() => {
      store.dispatch(actions.batchActions(queuedActions));
      queuedActions = [];
      throttledDispatchTimeout = null;
    }, 50);
  }
}

// Exports from this module
module.exports = NewConsoleOutputWrapper;
