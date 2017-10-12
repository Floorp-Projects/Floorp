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

const EventEmitter = require("devtools/shared/old-event-emitter");
const ConsoleOutput = React.createFactory(require("devtools/client/webconsole/new-console-output/components/ConsoleOutput"));
const FilterBar = React.createFactory(require("devtools/client/webconsole/new-console-output/components/FilterBar"));

let store = null;

function NewConsoleOutputWrapper(parentNode, jsterm, toolbox, owner, document) {
  EventEmitter.decorate(this);

  this.parentNode = parentNode;
  this.jsterm = jsterm;
  this.toolbox = toolbox;
  this.owner = owner;
  this.document = document;

  this.init = this.init.bind(this);

  this.queuedMessageAdds = [];
  this.queuedMessageUpdates = [];
  this.queuedRequestUpdates = [];
  this.throttledDispatchTimeout = false;

  store = configureStore(this.jsterm.hud);
}
NewConsoleOutputWrapper.prototype = {
  init: function () {
    const attachRefToHud = (id, node) => {
      this.jsterm.hud[id] = node;
    };
    // Focus the input line whenever the output area is clicked.
    this.parentNode.addEventListener("click", (event) => {
      // Do not focus on middle/right-click or 2+ clicks.
      if (event.detail !== 1 || event.button !== 0) {
        return;
      }

      // Do not focus if a link was clicked
      let target = event.originalTarget || event.target;
      if (target.closest("a")) {
        return;
      }

      // Do not focus if an input field was clicked
      if (target.closest("input")) {
        return;
      }

      // Do not focus if something other than the output region was clicked
      // (including e.g. the clear messages button in toolbar)
      if (!target.closest(".webconsole-output-wrapper")) {
        return;
      }

      // Do not focus if something is selected
      let selection = this.document.defaultView.getSelection();
      if (selection && !selection.isCollapsed) {
        return;
      }

      this.jsterm.focus();
    });

    const serviceContainer = {
      attachRefToHud,
      emitNewMessage: (node, messageId, timeStamp) => {
        this.jsterm.hud.emit("new-messages", new Set([{
          node,
          messageId,
          timeStamp,
        }]));
      },
      hudProxy: this.jsterm.hud.proxy,
      openLink: url => {
        this.jsterm.hud.owner.openLink(url);
      },
      createElement: nodename => {
        return this.document.createElement(nodename);
      },
    };

    // Set `openContextMenu` this way so, `serviceContainer` variable
    // is available in the current scope and we can pass it into
    // `createContextMenu` method.
    serviceContainer.openContextMenu = (e, message) => {
      let { screenX, screenY, target } = e;

      let messageEl = target.closest(".message");
      let clipboardText = messageEl ? messageEl.textContent : null;

      let messageVariable = target.closest(".objectBox");
      // Ensure that console.group and console.groupCollapsed commands are not captured
      let variableText = (messageVariable
        && !(messageEl.classList.contains("startGroup"))
        && !(messageEl.classList.contains("startGroupCollapsed")))
          ? messageVariable.textContent : null;

      // Retrieve closes actor id from the DOM.
      let actorEl = target.closest("[data-link-actor-id]");
      let actor = actorEl ? actorEl.dataset.linkActorId : null;

      let menu = createContextMenu(this.jsterm, this.parentNode,
        { actor, clipboardText, variableText, message, serviceContainer });

      // Emit the "menu-open" event for testing.
      menu.once("open", () => this.emit("menu-open"));
      menu.popup(screenX, screenY, this.toolbox);

      return menu;
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

    this.jsterm.focus();
  },

  dispatchMessageAdd: function (message, waitForResponse) {
    // Wait for the message to render to resolve with the DOM node.
    // This is just for backwards compatibility with old tests, and should
    // be removed once it's not needed anymore.
    // Can only wait for response if the action contains a valid message.
    let promise;
    if (waitForResponse) {
      promise = new Promise(resolve => {
        let jsterm = this.jsterm;
        jsterm.hud.on("new-messages", function onThisMessage(e, messages) {
          for (let m of messages) {
            if (m.timeStamp === message.timestamp) {
              resolve(m.node);
              jsterm.hud.off("new-messages", onThisMessage);
              return;
            }
          }
        });
      });
    } else {
      promise = Promise.resolve();
    }

    this.batchedMessagesAdd(message);
    return promise;
  },

  dispatchMessagesAdd: function (messages) {
    store.dispatch(actions.messagesAdd(messages));
  },

  dispatchMessagesClear: function () {
    store.dispatch(actions.messagesClear());
  },

  dispatchTimestampsToggle: function (enabled) {
    store.dispatch(actions.timestampsToggle(enabled));
  },

  dispatchMessageUpdate: function (message, res) {
    // network-message-updated will emit when all the update message arrives.
    // Since we can't ensure the order of the network update, we check
    // that networkInfo.updates has all we need.
    const NUMBER_OF_NETWORK_UPDATE = 8;
    if (res.networkInfo.updates.length === NUMBER_OF_NETWORK_UPDATE) {
      this.batchedMessageUpdates({ res, message });
    }
  },

  dispatchRequestUpdate: function (id, data) {
    this.batchedRequestUpdates({ id, data });
  },

  batchedMessageUpdates: function (info) {
    this.queuedMessageUpdates.push(info);
    this.setTimeoutIfNeeded();
  },

  batchedRequestUpdates: function (message) {
    this.queuedRequestUpdates.push(message);
    this.setTimeoutIfNeeded();
  },

  batchedMessagesAdd: function (message) {
    this.queuedMessageAdds.push(message);
    this.setTimeoutIfNeeded();
  },

  setTimeoutIfNeeded: function () {
    if (this.throttledDispatchTimeout) {
      return;
    }

    this.throttledDispatchTimeout = setTimeout(() => {
      this.throttledDispatchTimeout = null;

      store.dispatch(actions.messagesAdd(this.queuedMessageAdds));
      this.queuedMessageAdds = [];

      if (this.queuedMessageUpdates.length > 0) {
        this.queuedMessageUpdates.forEach(({ message, res }) => {
          store.dispatch(actions.networkMessageUpdate(message, null, res));
          this.jsterm.hud.emit("network-message-updated", res);
        });
        this.queuedMessageUpdates = [];
      }
      if (this.queuedRequestUpdates.length > 0) {
        this.queuedRequestUpdates.forEach(({ id, data}) => {
          store.dispatch(actions.networkUpdateRequest(id, data));
        });
        this.queuedRequestUpdates = [];

        // Fire an event indicating that all data fetched from
        // the backend has been received. This is based on
        // 'FirefoxDataProvider.isQueuePayloadReady', see more
        // comments in that method.
        // (netmonitor/src/connector/firefox-data-provider).
        // This event might be utilized in tests to find the right
        // time when to finish.
        this.jsterm.hud.emit("network-request-payload-ready");
      }
    }, 50);
  },

  // Should be used for test purpose only.
  getStore: function () {
    return store;
  }
};

// Exports from this module
module.exports = NewConsoleOutputWrapper;
