/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  createElement,
  createFactory,
} = require("devtools/client/shared/vendor/react");
const ReactDOM = require("devtools/client/shared/vendor/react-dom");
const { Provider } = require("devtools/client/shared/vendor/react-redux");
const ToolboxProvider = require("devtools/client/framework/store-provider");

const actions = require("devtools/client/webconsole/actions/index");
const { configureStore } = require("devtools/client/webconsole/store");

const {
  isPacketPrivate,
} = require("devtools/client/webconsole/utils/messages");
const {
  getAllMessagesById,
  getMessage,
} = require("devtools/client/webconsole/selectors/messages");
const Telemetry = require("devtools/client/shared/telemetry");

const EventEmitter = require("devtools/shared/event-emitter");
const App = createFactory(require("devtools/client/webconsole/components/App"));
const DataProvider = require("devtools/client/netmonitor/src/connector/firefox-data-provider");

const {
  setupServiceContainer,
} = require("devtools/client/webconsole/service-container");

loader.lazyRequireGetter(
  this,
  "Constants",
  "devtools/client/webconsole/constants"
);

function renderApp({ app, store, toolbox, root }) {
  return ReactDOM.render(
    createElement(
      Provider,
      { store },
      toolbox
        ? createElement(ToolboxProvider, { store: toolbox.store }, app)
        : app
    ),
    root
  );
}

let store = null;

class WebConsoleWrapper {
  /**
   *
   * @param {HTMLElement} parentNode
   * @param {WebConsoleUI} webConsoleUI
   * @param {Toolbox} toolbox
   * @param {Document} document
   */
  constructor(parentNode, webConsoleUI, toolbox, document) {
    EventEmitter.decorate(this);

    this.parentNode = parentNode;
    this.webConsoleUI = webConsoleUI;
    this.toolbox = toolbox;
    this.hud = this.webConsoleUI.hud;
    this.document = document;

    this.init = this.init.bind(this);

    this.queuedMessageAdds = [];
    this.queuedMessageUpdates = [];
    this.queuedRequestUpdates = [];
    this.throttledDispatchPromise = null;
    this.telemetry = new Telemetry();
  }

  async init() {
    const { webConsoleUI } = this;

    const webConsoleFront = await this.hud.currentTarget.getFront("console");

    this.networkDataProvider = new DataProvider({
      actions: {
        updateRequest: (id, data) => this.batchedRequestUpdates({ id, data }),
      },
      webConsoleFront,
    });

    return new Promise(resolve => {
      store = configureStore(this.webConsoleUI, {
        // We may not have access to the toolbox (e.g. in the browser console).
        telemetry: this.telemetry,
        thunkArgs: {
          webConsoleUI,
          hud: this.hud,
          toolbox: this.toolbox,
          client: this.webConsoleUI._commands,
        },
      });

      const serviceContainer = setupServiceContainer({
        webConsoleUI,
        toolbox: this.toolbox,
        hud: this.hud,
        webConsoleWrapper: this,
      });

      const app = App({
        serviceContainer,
        webConsoleUI,
        onFirstMeaningfulPaint: resolve,
        closeSplitConsole: this.closeSplitConsole.bind(this),
        hidePersistLogsCheckbox:
          webConsoleUI.isBrowserConsole || webConsoleUI.isBrowserToolboxConsole,
        hideShowContentMessagesCheckbox:
          !webConsoleUI.isBrowserConsole &&
          !webConsoleUI.isBrowserToolboxConsole,
      });

      // Render the root Application component.
      if (this.parentNode) {
        this.body = renderApp({
          app,
          store,
          root: this.parentNode,
          toolbox: this.toolbox,
        });
      } else {
        // If there's no parentNode, we are in a test. So we can resolve immediately.
        resolve();
      }
    });
  }

  dispatchMessageAdd(packet) {
    this.batchedMessagesAdd([packet]);
  }

  dispatchMessagesAdd(messages) {
    this.batchedMessagesAdd(messages);
  }

  dispatchMessagesClear() {
    // We might still have pending message additions and updates when the clear action is
    // triggered, so we need to flush them to make sure we don't have unexpected behavior
    // in the ConsoleOutput.
    this.queuedMessageAdds = [];
    this.queuedMessageUpdates = [];
    this.queuedRequestUpdates = [];
    store.dispatch(actions.messagesClear());
    this.webConsoleUI.emitForTests("messages-cleared");
  }

  dispatchPrivateMessagesClear() {
    // We might still have pending private message additions when the private messages
    // clear action is triggered. We need to remove any private-window-issued packets from
    // the queue so they won't appear in the output.

    // For (network) message updates, we need to check both messages queue and the state
    // since we can receive updates even if the message isn't rendered yet.
    const messages = [...getAllMessagesById(store.getState()).values()];
    this.queuedMessageUpdates = this.queuedMessageUpdates.filter(
      ({ networkInfo }) => {
        const { actor } = networkInfo;

        const queuedNetworkMessage = this.queuedMessageAdds.find(
          p => p.actor === actor
        );
        if (queuedNetworkMessage && isPacketPrivate(queuedNetworkMessage)) {
          return false;
        }

        const requestMessage = messages.find(
          message => actor === message.actor
        );
        if (requestMessage && requestMessage.private === true) {
          return false;
        }

        return true;
      }
    );

    // For (network) requests updates, we can check only the state, since there must be a
    // user interaction to get an update (i.e. the network message is displayed and thus
    // in the state).
    this.queuedRequestUpdates = this.queuedRequestUpdates.filter(({ id }) => {
      const requestMessage = getMessage(store.getState(), id);
      if (requestMessage && requestMessage.private === true) {
        return false;
      }

      return true;
    });

    // Finally we clear the messages queue. This needs to be done here since we use it to
    // clean the other queues.
    this.queuedMessageAdds = this.queuedMessageAdds.filter(
      p => !isPacketPrivate(p)
    );

    store.dispatch(actions.privateMessagesClear());
  }

  dispatchMessageUpdate(message, res) {
    // network-message-updated will emit when all the update message arrives.
    // Since we can't ensure the order of the network update, we check
    // that networkInfo.updates has all we need.
    // Note that 'requestPostData' is sent only for POST requests, so we need
    // to count with that.
    const NUMBER_OF_NETWORK_UPDATE = 8;

    let expectedLength = NUMBER_OF_NETWORK_UPDATE;
    if (res.networkInfo.updates.includes("responseCache")) {
      expectedLength++;
    }
    if (res.networkInfo.updates.includes("requestPostData")) {
      expectedLength++;
    }

    if (res.networkInfo.updates.length === expectedLength) {
      this.batchedMessageUpdates({ res, message });
    }
  }

  dispatchSidebarClose() {
    store.dispatch(actions.sidebarClose());
  }

  dispatchSplitConsoleCloseButtonToggle() {
    store.dispatch(
      actions.splitConsoleCloseButtonToggle(
        this.toolbox && this.toolbox.currentToolId !== "webconsole"
      )
    );
  }

  dispatchTabWillNavigate(packet) {
    const { ui } = store.getState();

    // For the browser console, we receive tab navigation
    // when the original top level window we attached to is closed,
    // but we don't want to reset console history and just switch to
    // the next available window.
    if (ui.persistLogs || this.webConsoleUI.isBrowserConsole) {
      // Add a type in order for this event packet to be identified by
      // utils/messages.js's `transformPacket`
      packet.type = "will-navigate";
      this.dispatchMessageAdd(packet);
    } else {
      this.webConsoleUI.clearNetworkRequests();
      this.dispatchMessagesClear();
      store.dispatch({
        type: Constants.WILL_NAVIGATE,
      });
    }
  }

  batchedMessageUpdates(info) {
    this.queuedMessageUpdates.push(info);
    this.setTimeoutIfNeeded();
  }

  batchedRequestUpdates(message) {
    this.queuedRequestUpdates.push(message);
    return this.setTimeoutIfNeeded();
  }

  batchedMessagesAdd(messages) {
    this.queuedMessageAdds = this.queuedMessageAdds.concat(messages);
    this.setTimeoutIfNeeded();
  }

  requestData(id, type) {
    this.networkDataProvider.requestData(id, type);
  }

  dispatchClearLogpointMessages(logpointId) {
    store.dispatch(actions.messagesClearLogpoint(logpointId));
  }

  dispatchClearHistory() {
    store.dispatch(actions.clearHistory());
  }

  /**
   *
   * @param {String} expression: The expression to evaluate
   */
  dispatchEvaluateExpression(expression) {
    store.dispatch(actions.evaluateExpression(expression));
  }

  /**
   * Returns a Promise that resolves once any async dispatch is finally dispatched.
   */
  waitAsyncDispatches() {
    if (!this.throttledDispatchPromise) {
      return Promise.resolve();
    }
    return this.throttledDispatchPromise;
  }

  setTimeoutIfNeeded() {
    if (this.throttledDispatchPromise) {
      return this.throttledDispatchPromise;
    }
    this.throttledDispatchPromise = new Promise(done => {
      setTimeout(async () => {
        this.throttledDispatchPromise = null;

        if (!store) {
          // The store is not initialized yet, we can call setTimeoutIfNeeded so the
          // messages will be handled in the next timeout when the store is ready.
          this.setTimeoutIfNeeded();
          return;
        }

        store.dispatch(actions.messagesAdd(this.queuedMessageAdds));

        const { length } = this.queuedMessageAdds;

        // This telemetry event is only useful when we have a toolbox so only
        // send it when we have one.
        if (this.toolbox) {
          this.telemetry.addEventProperty(
            this.toolbox,
            "enter",
            "webconsole",
            null,
            "message_count",
            length
          );
        }

        this.queuedMessageAdds = [];

        if (this.queuedMessageUpdates.length > 0) {
          for (const { message, res } of this.queuedMessageUpdates) {
            await store.dispatch(
              actions.networkMessageUpdate(message, null, res)
            );
            this.webConsoleUI.emitForTests("network-message-updated", res);
          }
          this.queuedMessageUpdates = [];
        }
        if (this.queuedRequestUpdates.length > 0) {
          for (const { id, data } of this.queuedRequestUpdates) {
            await store.dispatch(actions.networkUpdateRequest(id, data));
          }
          this.queuedRequestUpdates = [];

          // Fire an event indicating that all data fetched from
          // the backend has been received. This is based on
          // 'FirefoxDataProvider.isQueuePayloadReady', see more
          // comments in that method.
          // (netmonitor/src/connector/firefox-data-provider).
          // This event might be utilized in tests to find the right
          // time when to finish.
          this.webConsoleUI.emitForTests("network-request-payload-ready");
        }
        done();
      }, 50);
    });
    return this.throttledDispatchPromise;
  }

  getStore() {
    return store;
  }

  subscribeToStore(callback) {
    store.subscribe(() => callback(store.getState()));
  }

  createElement(nodename) {
    return this.document.createElement(nodename);
  }

  // Called by pushing close button.
  closeSplitConsole() {
    this.toolbox.closeSplitConsole();
  }
}

// Exports from this module
module.exports = WebConsoleWrapper;
