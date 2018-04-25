/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { createElement, createFactory } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const ReactDOM = require("devtools/client/shared/vendor/react-dom");
const { Provider } = require("devtools/client/shared/vendor/react-redux");

const actions = require("devtools/client/webconsole/actions/index");
const { createContextMenu } = require("devtools/client/webconsole/utils/context-menu");
const { configureStore } = require("devtools/client/webconsole/store");
const { isPacketPrivate } = require("devtools/client/webconsole/utils/messages");
const { getAllMessagesById, getMessage } = require("devtools/client/webconsole/selectors/messages");
const Telemetry = require("devtools/client/shared/telemetry");

const EventEmitter = require("devtools/shared/event-emitter");
const ConsoleOutput = createFactory(require("devtools/client/webconsole/components/ConsoleOutput"));
const FilterBar = createFactory(require("devtools/client/webconsole/components/FilterBar"));
const SideBar = createFactory(require("devtools/client/webconsole/components/SideBar"));
const JSTerm = createFactory(require("devtools/client/webconsole/components/JSTerm"));

let store = null;

function NewConsoleOutputWrapper(parentNode, hud, toolbox, owner, document) {
  EventEmitter.decorate(this);

  this.parentNode = parentNode;
  this.hud = hud;
  this.toolbox = toolbox;
  this.owner = owner;
  this.document = document;

  this.init = this.init.bind(this);

  this.queuedMessageAdds = [];
  this.queuedMessageUpdates = [];
  this.queuedRequestUpdates = [];
  this.throttledDispatchPromise = null;

  this._telemetry = new Telemetry();

  store = configureStore(this.hud);
}
NewConsoleOutputWrapper.prototype = {
  init: function() {
    return new Promise((resolve) => {
      const attachRefToHud = (id, node) => {
        this.hud[id] = node;
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

        if (this.hud && this.hud.jsterm) {
          this.hud.jsterm.focus();
        }
      });

      let { hud } = this;

      const serviceContainer = {
        attachRefToHud,
        emitNewMessage: (node, messageId, timeStamp) => {
          hud.emit("new-messages", new Set([{
            node,
            messageId,
            timeStamp,
          }]));
        },
        hudProxy: hud.proxy,
        openLink: (url, e) => {
          hud.owner.openLink(url, e);
        },
        createElement: nodename => {
          return this.document.createElement(nodename);
        },
        getLongString: (grip) => {
          return hud.proxy.webConsoleClient.getString(grip);
        },
        requestData(id, type) {
          return hud.proxy.networkDataProvider.requestData(id, type);
        },
        onViewSource(frame) {
          if (hud && hud.owner && hud.owner.viewSource) {
            hud.owner.viewSource(frame.url, frame.line);
          }
        }
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
        let actorEl = target.closest("[data-link-actor-id]") ||
                      target.querySelector("[data-link-actor-id]");
        let actor = actorEl ? actorEl.dataset.linkActorId : null;

        let rootObjectInspector = target.closest(".object-inspector");
        let rootActor = rootObjectInspector ?
                        rootObjectInspector.querySelector("[data-link-actor-id]") : null;
        let rootActorId = rootActor ? rootActor.dataset.linkActorId : null;

        let sidebarTogglePref = store.getState().prefs.sidebarToggle;
        let openSidebar = sidebarTogglePref ? (messageId) => {
          store.dispatch(actions.showObjectInSidebar(rootActorId, messageId));
        } : null;

        let menu = createContextMenu(this.hud, this.parentNode, {
          actor,
          clipboardText,
          variableText,
          message,
          serviceContainer,
          openSidebar,
          rootActorId
        });

        // Emit the "menu-open" event for testing.
        menu.once("open", () => this.emit("menu-open"));
        menu.popup(screenX, screenY, { doc: this.owner.chromeWindow.document });

        return menu;
      };

      if (this.toolbox) {
        Object.assign(serviceContainer, {
          onViewSourceInDebugger: frame => {
            this.toolbox.viewSourceInDebugger(frame.url, frame.line).then(() =>
              this.hud.emit("source-in-debugger-opened")
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
              return panel.panelWin.Netmonitor.inspectRequest(requestId);
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
            let onNodeFrontSet = this.toolbox.selection
              .setNodeFront(front, { reason: "console" });

            return Promise.all([onNodeFrontSet, onInspectorUpdated]);
          }
        });
      }

      let provider = createElement(
        Provider,
        { store },
        dom.div(
          {className: "webconsole-output-wrapper"},
          FilterBar({
            hidePersistLogsCheckbox: this.hud.isBrowserConsole,
            serviceContainer: {
              attachRefToHud
            }
          }),
          ConsoleOutput({
            serviceContainer,
            onFirstMeaningfulPaint: resolve
          }),
          SideBar({
            serviceContainer,
          }),
          JSTerm({
            hud: this.hud,
          }),
        ));
      this.body = ReactDOM.render(provider, this.parentNode);
    });
  },

  dispatchMessageAdd: function(packet, waitForResponse) {
    // Wait for the message to render to resolve with the DOM node.
    // This is just for backwards compatibility with old tests, and should
    // be removed once it's not needed anymore.
    // Can only wait for response if the action contains a valid message.
    let promise;
    // Also, do not expect any update while the panel is in background.
    if (waitForResponse && document.visibilityState === "visible") {
      const timeStampToMatch = packet.message
        ? packet.message.timeStamp
        : packet.timestamp;

      promise = new Promise(resolve => {
        this.hud.on("new-messages", function onThisMessage(messages) {
          for (let m of messages) {
            if (m.timeStamp === timeStampToMatch) {
              resolve(m.node);
              this.hud.off("new-messages", onThisMessage);
              return;
            }
          }
        }.bind(this));
      });
    } else {
      promise = Promise.resolve();
    }

    this.batchedMessagesAdd(packet);
    return promise;
  },

  dispatchMessagesAdd: function(messages) {
    store.dispatch(actions.messagesAdd(messages));
  },

  dispatchMessagesClear: function() {
    // We might still have pending message additions and updates when the clear action is
    // triggered, so we need to flush them to make sure we don't have unexpected behavior
    // in the ConsoleOutput.
    this.queuedMessageAdds = [];
    this.queuedMessageUpdates = [];
    this.queuedRequestUpdates = [];
    store.dispatch(actions.messagesClear());
  },

  dispatchPrivateMessagesClear: function() {
    // We might still have pending private message additions when the private messages
    // clear action is triggered. We need to remove any private-window-issued packets from
    // the queue so they won't appear in the output.

    // For (network) message updates, we need to check both messages queue and the state
    // since we can receive updates even if the message isn't rendered yet.
    const messages = [...getAllMessagesById(store.getState()).values()];
    this.queuedMessageUpdates = this.queuedMessageUpdates.filter(({networkInfo}) => {
      const { actor } = networkInfo;

      const queuedNetworkMessage = this.queuedMessageAdds.find(p => p.actor === actor);
      if (queuedNetworkMessage && isPacketPrivate(queuedNetworkMessage)) {
        return false;
      }

      const requestMessage = messages.find(message => actor === message.actor);
      if (requestMessage && requestMessage.private === true) {
        return false;
      }

      return true;
    });

    // For (network) requests updates, we can check only the state, since there must be a
    // user interaction to get an update (i.e. the network message is displayed and thus
    // in the state).
    this.queuedRequestUpdates = this.queuedRequestUpdates.filter(({id}) => {
      const requestMessage = getMessage(store.getState(), id);
      if (requestMessage && requestMessage.private === true) {
        return false;
      }

      return true;
    });

    // Finally we clear the messages queue. This needs to be done here since we use it to
    // clean the other queues.
    this.queuedMessageAdds = this.queuedMessageAdds.filter(p => !isPacketPrivate(p));

    store.dispatch(actions.privateMessagesClear());
  },

  dispatchTimestampsToggle: function(enabled) {
    store.dispatch(actions.timestampsToggle(enabled));
  },

  dispatchMessageUpdate: function(message, res) {
    // network-message-updated will emit when all the update message arrives.
    // Since we can't ensure the order of the network update, we check
    // that networkInfo.updates has all we need.
    // Note that 'requestPostData' is sent only for POST requests, so we need
    // to count with that.
    const NUMBER_OF_NETWORK_UPDATE = 8;
    let expectedLength = NUMBER_OF_NETWORK_UPDATE;
    if (res.networkInfo.updates.includes("requestPostData")) {
      expectedLength++;
    }

    if (res.networkInfo.updates.length === expectedLength) {
      this.batchedMessageUpdates({ res, message });
    }
  },

  dispatchRequestUpdate: function(id, data) {
    this.batchedRequestUpdates({ id, data });
  },

  dispatchSidebarClose: function() {
    store.dispatch(actions.sidebarClose());
  },

  batchedMessageUpdates: function(info) {
    this.queuedMessageUpdates.push(info);
    this.setTimeoutIfNeeded();
  },

  batchedRequestUpdates: function(message) {
    this.queuedRequestUpdates.push(message);
    this.setTimeoutIfNeeded();
  },

  batchedMessagesAdd: function(message) {
    this.queuedMessageAdds.push(message);
    this.setTimeoutIfNeeded();
  },

  /**
   * Returns a Promise that resolves once any async dispatch is finally dispatched.
   */
  waitAsyncDispatches: function() {
    if (!this.throttledDispatchPromise) {
      return Promise.resolve();
    }
    return this.throttledDispatchPromise;
  },

  setTimeoutIfNeeded: function() {
    if (this.throttledDispatchPromise) {
      return;
    }

    this.throttledDispatchPromise = new Promise(done => {
      setTimeout(() => {
        this.throttledDispatchPromise = null;

        store.dispatch(actions.messagesAdd(this.queuedMessageAdds));

        const length = this.queuedMessageAdds.length;
        this._telemetry.addEventProperty(
          "devtools.main", "enter", "webconsole", null, "message_count", length);

        this.queuedMessageAdds = [];

        if (this.queuedMessageUpdates.length > 0) {
          this.queuedMessageUpdates.forEach(({ message, res }) => {
            store.dispatch(actions.networkMessageUpdate(message, null, res));
            this.hud.emit("network-message-updated", res);
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
          this.hud.emit("network-request-payload-ready");
        }
        done();
      }, 50);
    });
  },

  // Should be used for test purpose only.
  getStore: function() {
    return store;
  }
};

// Exports from this module
module.exports = NewConsoleOutputWrapper;
