/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const constants = require("../constants");
const { asPaused } = require("../utils");
const { reportException } = require("devtools/shared/DevToolsUtils");
const { setNamedTimeout } = require("devtools/client/shared/widgets/view-helpers");
const { Task } = require("devtools/shared/task");

const FETCH_EVENT_LISTENERS_DELAY = 200; // ms

function fetchEventListeners() {
  return (dispatch, getState) => {
    // Make sure we"re not sending a batch of closely repeated requests.
    // This can easily happen whenever new sources are fetched.
    setNamedTimeout("event-listeners-fetch", FETCH_EVENT_LISTENERS_DELAY, () => {
      // In case there is still a request of listeners going on (it
      // takes several RDP round trips right now), make sure we wait
      // on a currently running request
      if (getState().eventListeners.fetchingListeners) {
        dispatch({
          type: services.WAIT_UNTIL,
          predicate: action => (
            action.type === constants.FETCH_EVENT_LISTENERS &&
            action.status === "done"
          ),
          run: dispatch => dispatch(fetchEventListeners())
        });
        return;
      }

      dispatch({
        type: constants.FETCH_EVENT_LISTENERS,
        status: "begin"
      });

      asPaused(gThreadClient, _getListeners).then(listeners => {
        // Notify that event listeners were fetched and shown in the view,
        // and callback to resume the active thread if necessary.
        window.emit(EVENTS.EVENT_LISTENERS_FETCHED);

        dispatch({
          type: constants.FETCH_EVENT_LISTENERS,
          status: "done",
          listeners: listeners
        });
      });
    });
  };
}

const _getListeners = Task.async(function* () {
  const response = yield gThreadClient.eventListeners();

  // Make sure all the listeners are sorted by the event type, since
  // they"re not guaranteed to be clustered together.
  response.listeners.sort((a, b) => a.type > b.type ? 1 : -1);

  // Add all the listeners in the debugger view event linsteners container.
  let fetchedDefinitions = new Map();
  let listeners = [];
  for (let listener of response.listeners) {
    let definitionSite;
    if (fetchedDefinitions.has(listener.function.actor)) {
      definitionSite = fetchedDefinitions.get(listener.function.actor);
    } else if (listener.function.class == "Function") {
      definitionSite = yield _getDefinitionSite(listener.function);
      if (!definitionSite) {
        // We don"t know where this listener comes from so don"t show it in
        // the UI as breaking on it doesn"t work (bug 942899).
        continue;
      }

      fetchedDefinitions.set(listener.function.actor, definitionSite);
    }
    listener.function.url = definitionSite;
    listeners.push(listener);
  }
  fetchedDefinitions.clear();

  return listeners;
});

const _getDefinitionSite = Task.async(function* (aFunction) {
  const grip = gThreadClient.pauseGrip(aFunction);
  let response;

  try {
    response = yield grip.getDefinitionSite();
  }
  catch (e) {
    // Don't make this error fatal, because it would break the entire events pane.
    reportException("_getDefinitionSite", e);
    return null;
  }

  return response.source.url;
});

function updateEventBreakpoints(eventNames) {
  return dispatch => {
    setNamedTimeout("event-breakpoints-update", 0, () => {
      gThreadClient.pauseOnDOMEvents(eventNames, function () {
        // Notify that event breakpoints were added/removed on the server.
        window.emit(EVENTS.EVENT_BREAKPOINTS_UPDATED);

        dispatch({
          type: constants.UPDATE_EVENT_BREAKPOINTS,
          eventNames: eventNames
        });
      });
    });
  };
}

module.exports = { updateEventBreakpoints, fetchEventListeners };
