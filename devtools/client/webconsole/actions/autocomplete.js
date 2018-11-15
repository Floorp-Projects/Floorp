/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  AUTOCOMPLETE_CLEAR,
  AUTOCOMPLETE_DATA_RECEIVE,
  AUTOCOMPLETE_PENDING_REQUEST,
  AUTOCOMPLETE_RETRIEVE_FROM_CACHE,
} = require("devtools/client/webconsole/constants");

/**
 * Update the data used for the autocomplete popup in the console input (JsTerm).
 *
 * @param {Object} Object of the following shape:
 *        - {String} inputValue: the expression to complete.
 *        - {Int} cursor: The position of the cursor in the inputValue.
 *        - {WebConsoleClient} client: The webconsole client.
 *        - {String} frameActorId: The id of the frame we want to autocomplete in.
 *        - {Boolean} force: True to force a call to the server (as opposed to retrieve
 *                           from the cache).
 *        - {String} selectedNodeActor: Actor id of the selected node in the inspector.
 */
function autocompleteUpdate({
  inputValue,
  cursor,
  client,
  frameActorId,
  force,
  selectedNodeActor,
}) {
  return ({dispatch, getState}) => {
    const {cache} = getState().autocomplete;

    if (!force && (
      !inputValue ||
      /^[a-zA-Z0-9_$]/.test(inputValue.substring(cursor))
    )) {
      return dispatch(autocompleteClear());
    }

    const input = inputValue.substring(0, cursor);
    const retrieveFromCache = !force &&
      cache &&
      cache.input &&
      input.startsWith(cache.input) &&
      /[a-zA-Z0-9]$/.test(input) &&
      frameActorId === cache.frameActorId;

    if (retrieveFromCache) {
      return dispatch(autoCompleteDataRetrieveFromCache(input));
    }

    return dispatch(autocompleteDataFetch({
      input,
      frameActorId,
      client,
      selectedNodeActor,
    }));
  };
}

/**
 * Called when the autocompletion data should be cleared.
 */
function autocompleteClear() {
  return {
    type: AUTOCOMPLETE_CLEAR,
  };
}

/**
 * Called when the autocompletion data should be retrieved from the cache (i.e.
 * client-side).
 *
 * @param {String} input: The input used to filter the cached data.
 */
function autoCompleteDataRetrieveFromCache(input) {
  return {
    type: AUTOCOMPLETE_RETRIEVE_FROM_CACHE,
    input,
  };
}

let currentRequestId = 0;
function generateRequestId() {
  return currentRequestId++;
}

/**
 * Action that fetch autocompletion data from the server.
 *
 * @param {Object} Object of the following shape:
 *        - {String} input: the expression that we want to complete.
 *        - {String} frameActorId: The id of the frame we want to autocomplete in.
 *        - {WebConsoleClient} client: The webconsole client.
 *        - {String} selectedNodeActor: Actor id of the selected node in the inspector.
 */
function autocompleteDataFetch({
  input,
  frameActorId,
  client,
  selectedNodeActor,
}) {
  return ({dispatch}) => {
    const id = generateRequestId();
    dispatch({type: AUTOCOMPLETE_PENDING_REQUEST, id});
    client.autocomplete(input, undefined, frameActorId, selectedNodeActor).then(res => {
      dispatch(autocompleteDataReceive(id, input, frameActorId, res));
    }).catch(e => {
      console.error("failed autocomplete", e);
      dispatch(autocompleteClear());
    });
  };
}

/**
 * Called when we receive the autocompletion data from the server.
 *
 * @param {Integer} id: The autocompletion request id. This will be used in the reducer to
 *                      check that we update the state with the last request results.
 * @param {String} input: The expression that was evaluated to get the data.
 *        - {String} frameActorId: The id of the frame the evaluation was made in.
 * @param {Object} data: The actual data sent from the server.
 */
function autocompleteDataReceive(id, input, frameActorId, data) {
  return {
    type: AUTOCOMPLETE_DATA_RECEIVE,
    id,
    input,
    frameActorId,
    data,
  };
}

module.exports = {
  autocompleteUpdate,
  autocompleteDataFetch,
  autocompleteDataReceive,
};
