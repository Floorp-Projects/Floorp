/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {FilterState} = require("devtools/client/webconsole/new-console-output/reducers/filters");
const {PrefState} = require("devtools/client/webconsole/new-console-output/reducers/prefs");
const {UiState} = require("devtools/client/webconsole/new-console-output/reducers/ui");
const {
  applyMiddleware,
  compose,
  createStore
} = require("devtools/client/shared/vendor/redux");
const { thunk } = require("devtools/client/shared/redux/middleware/thunk");
const {
  BATCH_ACTIONS
} = require("devtools/client/shared/redux/middleware/debounce");
const {
  MESSAGE_ADD,
  MESSAGE_OPEN,
  MESSAGES_ADD,
  MESSAGES_CLEAR,
  REMOVED_ACTORS_CLEAR,
  NETWORK_MESSAGE_UPDATE,
  PREFS,
} = require("devtools/client/webconsole/new-console-output/constants");
const { reducers } = require("./reducers/index");
const Services = require("Services");
const {
  getMessage,
  getAllMessagesUiById,
} = require("devtools/client/webconsole/new-console-output/selectors/messages");
const DataProvider = require("devtools/client/netmonitor/src/connector/firefox-data-provider");
const {
  getAllNetworkMessagesUpdateById,
} = require("devtools/client/webconsole/new-console-output/selectors/messages");

/**
 * Create and configure store for the Console panel. This is the place
 * where various enhancers and middleware can be registered.
 */
function configureStore(hud, options = {}) {
  const logLimit = options.logLimit
    || Math.max(Services.prefs.getIntPref("devtools.hud.loglimit"), 1);

  const sidebarToggle = Services.prefs.getBoolPref(PREFS.UI.SIDEBAR_TOGGLE);

  const initialState = {
    prefs: PrefState({ logLimit, sidebarToggle }),
    filters: new FilterState({
      error: Services.prefs.getBoolPref(PREFS.FILTER.ERROR),
      warn: Services.prefs.getBoolPref(PREFS.FILTER.WARN),
      info: Services.prefs.getBoolPref(PREFS.FILTER.INFO),
      debug: Services.prefs.getBoolPref(PREFS.FILTER.DEBUG),
      log: Services.prefs.getBoolPref(PREFS.FILTER.LOG),
      css: Services.prefs.getBoolPref(PREFS.FILTER.CSS),
      net: Services.prefs.getBoolPref(PREFS.FILTER.NET),
      netxhr: Services.prefs.getBoolPref(PREFS.FILTER.NETXHR),
    }),
    ui: UiState({
      filterBarVisible: Services.prefs.getBoolPref(PREFS.UI.FILTER_BAR),
      networkMessageActiveTabId: "headers",
      persistLogs: Services.prefs.getBoolPref(PREFS.UI.PERSIST),
    })
  };

  return createStore(
    createRootReducer(),
    initialState,
    compose(
      applyMiddleware(thunk),
      enableActorReleaser(hud),
      enableBatching(),
      enableNetProvider(hud)
    )
  );
}

function createRootReducer() {
  return function rootReducer(state, action) {
    // We want to compute the new state for all properties except "messages".
    const newState = [...Object.entries(reducers)].reduce((res, [key, reducer]) => {
      if (key !== "messages") {
        res[key] = reducer(state[key], action);
      }
      return res;
    }, {});

    return Object.assign(newState, {
      // specifically pass the updated filters and prefs state as additional arguments.
      messages: reducers.messages(
        state.messages,
        action,
        newState.filters,
        newState.prefs,
      ),
    });
  };
}

/**
 * A enhancer for the store to handle batched actions.
 */
function enableBatching() {
  return next => (reducer, initialState, enhancer) => {
    function batchingReducer(state, action) {
      switch (action.type) {
        case BATCH_ACTIONS:
          return action.actions.reduce(batchingReducer, state);
        default:
          return reducer(state, action);
      }
    }

    if (typeof initialState === "function" && typeof enhancer === "undefined") {
      enhancer = initialState;
      initialState = undefined;
    }

    return next(batchingReducer, initialState, enhancer);
  };
}

/**
 * This enhancer is responsible for releasing actors on the backend.
 * When messages with arguments are removed from the store we should also
 * clean up the backend.
 */
function enableActorReleaser(hud) {
  return next => (reducer, initialState, enhancer) => {
    function releaseActorsEnhancer(state, action) {
      state = reducer(state, action);

      let type = action.type;
      let proxy = hud ? hud.proxy : null;
      if (proxy && ([MESSAGE_ADD, MESSAGES_ADD, MESSAGES_CLEAR].includes(type))) {
        releaseActors(state.messages.removedActors, proxy);

        // Reset `removedActors` in message reducer.
        state = reducer(state, {
          type: REMOVED_ACTORS_CLEAR
        });
      }

      return state;
    }

    return next(releaseActorsEnhancer, initialState, enhancer);
  };
}

/**
 * This enhancer is responsible for fetching HTTP details data
 * collected by the backend. The fetch happens on-demand
 * when the user expands network log in order to inspect it.
 *
 * This way we don't slow down the Console logging by fetching.
 * unnecessary data over RDP.
 */
function enableNetProvider(hud) {
  let dataProvider;
  return next => (reducer, initialState, enhancer) => {
    function netProviderEnhancer(state, action) {
      let proxy = hud ? hud.proxy : null;
      if (!proxy) {
        return reducer(state, action);
      }

      let actions = {
        updateRequest: (id, data, batch) => {
          proxy.dispatchRequestUpdate(id, data);
        }
      };

      // Data provider implements async logic for fetching
      // data from the backend. It's created the first
      // time it's needed.
      if (!dataProvider) {
        dataProvider = new DataProvider({
          actions,
          webConsoleClient: proxy.webConsoleClient
        });

        // /!\ This is terrible, but it allows ResponsePanel to be able to call
        // `dataProvider.requestData` to fetch response content lazily.
        // `proxy.networkDataProvider` is put by NewConsoleOutputWrapper on
        // `serviceContainer` which allow NetworkEventMessage to expose requestData on
        // the fake `connector` object it hands over to ResponsePanel.
        proxy.networkDataProvider = dataProvider;
      }

      let type = action.type;
      let newState = reducer(state, action);

      // If network message has been opened, fetch all HTTP details
      // from the backend. It can happen (especially in test) that
      // the message is opened before all network event updates are
      // received. The rest of updates will be handled below, see:
      // NETWORK_MESSAGE_UPDATE action handler.
      if (type == MESSAGE_OPEN) {
        let message = getMessage(state, action.id);
        if (!message.openedOnce && message.source == "network") {
          let updates = getAllNetworkMessagesUpdateById(newState);

          // If there is no network request update received for this
          // request-log, it's likely that it comes from cache.
          // I.e. it's been executed before the console panel started
          // listening from network events. Let fix that by updating
          // the reducer now.
          // Executing the reducer means that the `networkMessagesUpdateById`
          // is updated (a actor key created). The key is needed for proper
          // handling NETWORK_UPDATE_REQUEST event (in the same reducer).
          if (!updates[action.id]) {
            newState = reducer(newState, {
              type: NETWORK_MESSAGE_UPDATE,
              message: message,
            });
          }

          dataProvider.onNetworkEvent(null, message);
          message.updates.forEach(updateType => {
            dataProvider.onNetworkEventUpdate(null, {
              packet: { updateType: updateType },
              networkInfo: message,
            });
          });
        }
      }

      // Process all incoming HTTP details packets.
      if (type == NETWORK_MESSAGE_UPDATE) {
        let actor = action.response.networkInfo.actor;
        let open = getAllMessagesUiById(state).includes(actor);
        if (open) {
          dataProvider.onNetworkEventUpdate(null, action.response);
        }
      }

      return newState;
    }

    return next(netProviderEnhancer, initialState, enhancer);
  };
}
/**
 * Helper function for releasing backend actors.
 */
function releaseActors(removedActors, proxy) {
  if (!proxy) {
    return;
  }

  removedActors.forEach(actor => proxy.releaseActor(actor));
}

// Provide the store factory for test code so that each test is working with
// its own instance.
module.exports.configureStore = configureStore;

