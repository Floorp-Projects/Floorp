/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  createElement,
  createFactory,
} = require("resource://devtools/client/shared/vendor/react.js");
const ReactDOM = require("resource://devtools/client/shared/vendor/react-dom.js");
const {
  Provider,
  createProvider,
} = require("resource://devtools/client/shared/vendor/react-redux.js");

const actions = require("resource://devtools/client/webconsole/actions/index.js");
const {
  configureStore,
} = require("resource://devtools/client/webconsole/store.js");

const {
  isPacketPrivate,
} = require("resource://devtools/client/webconsole/utils/messages.js");
const {
  getMutableMessagesById,
  getMessage,
  getAllNetworkMessagesUpdateById,
} = require("resource://devtools/client/webconsole/selectors/messages.js");

const EventEmitter = require("resource://devtools/shared/event-emitter.js");
const App = createFactory(
  require("resource://devtools/client/webconsole/components/App.js")
);
const {
  getAllFilters,
} = require("resource://devtools/client/webconsole/selectors/filters.js");

loader.lazyGetter(this, "AppErrorBoundary", () =>
  createFactory(
    require("resource://devtools/client/shared/components/AppErrorBoundary.js")
  )
);

const {
  setupServiceContainer,
} = require("resource://devtools/client/webconsole/service-container.js");

loader.lazyRequireGetter(
  this,
  "Constants",
  "resource://devtools/client/webconsole/constants.js"
);

// Localized strings for (devtools/client/locales/en-US/startup.properties)
loader.lazyGetter(this, "L10N", function () {
  const { LocalizationHelper } = require("resource://devtools/shared/l10n.js");
  return new LocalizationHelper("devtools/client/locales/startup.properties");
});

// Only Browser Console needs Fluent bundles at the moment
loader.lazyRequireGetter(
  this,
  "FluentL10n",
  "resource://devtools/client/shared/fluent-l10n/fluent-l10n.js",
  true
);
loader.lazyRequireGetter(
  this,
  "LocalizationProvider",
  "resource://devtools/client/shared/vendor/fluent-react.js",
  true
);

let store = null;

class WebConsoleWrapper {
  /**
   *
   * @param {HTMLElement} parentNode
   * @param {WebConsoleUI} webConsoleUI
   * @param {Toolbox} toolbox
   * @param {Document} document
   *
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

    this.telemetry = this.hud.telemetry;
  }

  #serviceContainer;

  async init() {
    const { webConsoleUI } = this;

    let fluentBundles;
    if (webConsoleUI.isBrowserConsole) {
      const fluentL10n = new FluentL10n();
      await fluentL10n.init(["devtools/client/toolbox.ftl"]);
      fluentBundles = fluentL10n.getBundles();
    }

    return new Promise(resolve => {
      store = configureStore(this.webConsoleUI, {
        // We may not have access to the toolbox (e.g. in the browser console).
        telemetry: this.telemetry,
        thunkArgs: {
          webConsoleUI,
          hud: this.hud,
          toolbox: this.toolbox,
          commands: this.hud.commands,
        },
      });

      const app = AppErrorBoundary(
        {
          componentName: "Console",
          panel: L10N.getStr("ToolboxTabWebconsole.label"),
        },
        App({
          serviceContainer: this.getServiceContainer(),
          webConsoleUI,
          onFirstMeaningfulPaint: resolve,
          closeSplitConsole: this.closeSplitConsole.bind(this),
          inputEnabled:
            !webConsoleUI.isBrowserConsole ||
            Services.prefs.getBoolPref("devtools.chrome.enabled"),
        })
      );

      // Render the root Application component.
      if (this.parentNode) {
        const maybeLocalizedElement = fluentBundles
          ? createElement(LocalizationProvider, { bundles: fluentBundles }, app)
          : app;

        this.body = ReactDOM.render(
          createElement(
            Provider,
            { store },
            createElement(
              createProvider(this.hud.commands.targetCommand.storeId),
              { store: this.hud.commands.targetCommand.store },
              maybeLocalizedElement
            )
          ),
          this.parentNode
        );
      } else {
        // If there's no parentNode, we are in a test. So we can resolve immediately.
        resolve();
      }
    });
  }

  destroy() {
    // This component can be instantiated from jest test, in which case we don't have
    // a parentNode reference.
    if (this.parentNode) {
      ReactDOM.unmountComponentAtNode(this.parentNode);
    }
  }

  /**
   * Query the reducer store for the current state of filtering
   * a given type of message
   *
   * @param {String} filter
   *        Type of message to be filtered.
   * @return {Boolean}
   *         True if this type of message should be displayed.
   */
  getFilterState(filter) {
    return getAllFilters(this.getStore().getState())[filter];
  }

  dispatchMessageAdd(packet) {
    this.batchedMessagesAdd([packet]);
  }

  dispatchMessagesAdd(messages) {
    this.batchedMessagesAdd(messages);
  }

  dispatchNetworkMessagesDisable() {
    const networkMessageIds = Object.keys(
      getAllNetworkMessagesUpdateById(store.getState())
    );
    store.dispatch(actions.messagesDisable(networkMessageIds));
  }

  dispatchMessagesClear() {
    // We might still have pending message additions and updates when the clear action is
    // triggered, so we need to flush them to make sure we don't have unexpected behavior
    // in the ConsoleOutput. *But* we want to keep any pending navigation request,
    // as we want to keep displaying them even if we received a clear request.
    function filter(l) {
      return l.filter(update => update.isNavigationRequest);
    }
    this.queuedMessageAdds = filter(this.queuedMessageAdds);
    this.queuedMessageUpdates = filter(this.queuedMessageUpdates);
    this.queuedRequestUpdates = this.queuedRequestUpdates.filter(
      update => update.data.isNavigationRequest
    );

    store?.dispatch(actions.messagesClear());
    this.webConsoleUI.emitForTests("messages-cleared");
  }

  dispatchPrivateMessagesClear() {
    // We might still have pending private message additions when the private messages
    // clear action is triggered. We need to remove any private-window-issued packets from
    // the queue so they won't appear in the output.

    // For (network) message updates, we need to check both messages queue and the state
    // since we can receive updates even if the message isn't rendered yet.
    const messages = [...getMutableMessagesById(store.getState()).values()];
    this.queuedMessageUpdates = this.queuedMessageUpdates.filter(
      ({ actor }) => {
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

  dispatchTargetMessagesRemove(targetFront) {
    // We might still have pending packets in the queues from the target that we need to remove
    // to prevent messages appearing in the output.

    for (let i = this.queuedMessageUpdates.length - 1; i >= 0; i--) {
      const packet = this.queuedMessageUpdates[i];
      if (packet.targetFront == targetFront) {
        this.queuedMessageUpdates.splice(i, 1);
      }
    }

    for (let i = this.queuedRequestUpdates.length - 1; i >= 0; i--) {
      const packet = this.queuedRequestUpdates[i];
      if (packet.data.targetFront == targetFront) {
        this.queuedRequestUpdates.splice(i, 1);
      }
    }

    for (let i = this.queuedMessageAdds.length - 1; i >= 0; i--) {
      const packet = this.queuedMessageAdds[i];
      // Keep in sync with the check done in the reducer for the TARGET_MESSAGES_REMOVE action.
      if (
        packet.targetFront == targetFront &&
        packet.type !== Constants.MESSAGE_TYPE.COMMAND &&
        packet.type !== Constants.MESSAGE_TYPE.RESULT
      ) {
        this.queuedMessageAdds.splice(i, 1);
      }
    }

    store.dispatch(actions.targetMessagesRemove(targetFront));
  }

  dispatchMessagesUpdate(messages) {
    this.batchedMessagesUpdates(messages);
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
      this.dispatchMessagesClear();
      store.dispatch({
        type: Constants.WILL_NAVIGATE,
      });
    }
  }

  batchedMessagesUpdates(messages) {
    if (messages.length) {
      this.queuedMessageUpdates.push(...messages);
      this.setTimeoutIfNeeded();
    }
  }

  batchedRequestUpdates(message) {
    this.queuedRequestUpdates.push(message);
    return this.setTimeoutIfNeeded();
  }

  batchedMessagesAdd(messages) {
    if (messages.length) {
      this.queuedMessageAdds.push(...messages);
      this.setTimeoutIfNeeded();
    }
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

  dispatchUpdateInstantEvaluationResultForCurrentExpression() {
    store.dispatch(actions.updateInstantEvaluationResultForCurrentExpression());
  }

  /**
   * Returns a Promise that resolves once any async dispatch is finally dispatched.
   */
  waitAsyncDispatches() {
    if (!this.throttledDispatchPromise) {
      return Promise.resolve();
    }
    // When closing the console during initialization,
    // setTimeoutIfNeeded may never resolve its promise
    // as window.setTimeout will be disabled on document destruction.
    const onUnload = new Promise(r =>
      window.addEventListener("unload", r, { once: true })
    );
    return Promise.race([this.throttledDispatchPromise, onUnload]);
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
          done();
          return;
        }

        const { ui } = store.getState();
        store.dispatch(
          actions.messagesAdd(this.queuedMessageAdds, null, ui.persistLogs)
        );

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

        if (this.queuedMessageUpdates.length) {
          await store.dispatch(
            actions.networkMessageUpdates(this.queuedMessageUpdates, null)
          );
          this.webConsoleUI.emitForTests("network-messages-updated");
          this.queuedMessageUpdates = [];
        }
        if (this.queuedRequestUpdates.length) {
          await store.dispatch(
            actions.networkUpdateRequests(this.queuedRequestUpdates)
          );
          const updateCount = this.queuedRequestUpdates.length;
          this.queuedRequestUpdates = [];

          // Fire an event indicating that all data fetched from
          // the backend has been received. This is based on
          // 'FirefoxDataProvider.isQueuePayloadReady', see more
          // comments in that method.
          // (netmonitor/src/connector/firefox-data-provider).
          // This event might be utilized in tests to find the right
          // time when to finish.

          this.webConsoleUI.emitForTests(
            "network-request-payload-ready",
            updateCount
          );
        }
        done();
      }, 50);
    });
    return this.throttledDispatchPromise;
  }

  getStore() {
    return store;
  }

  getServiceContainer() {
    if (!this.#serviceContainer) {
      this.#serviceContainer = setupServiceContainer({
        webConsoleUI: this.webConsoleUI,
        toolbox: this.toolbox,
        hud: this.hud,
        webConsoleWrapper: this,
      });
    }
    return this.#serviceContainer;
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

  toggleOriginalVariableMappingEvaluationNotification(show) {
    store.dispatch(
      actions.showEvaluationNotification(
        show ? Constants.ORIGINAL_VARIABLE_MAPPING : ""
      )
    );
  }
}

// Exports from this module
module.exports = WebConsoleWrapper;
