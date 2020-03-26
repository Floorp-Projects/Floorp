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
 * @param {Boolean} force: True to force a call to the server (as opposed to retrieve
 *                         from the cache).
 * @param {Array<String>} getterPath: Array representing the getter access (i.e.
 *                                    `a.b.c.d.` is described as ['a', 'b', 'c', 'd'] ).
 * @param {Array<String>} expressionVars: Array of the variables defined in the expression.
 */
function autocompleteUpdate(force, getterPath, expressionVars) {
  return async ({ dispatch, getState, webConsoleUI, hud }) => {
    if (hud.inputHasSelection()) {
      return dispatch(autocompleteClear());
    }

    const inputValue = hud.getInputValue();
    const frameActorId = await webConsoleUI.getFrameActor();
    const webconsoleFront = await webConsoleUI.getWebconsoleFront({
      frameActorId,
    });

    const cursor = webConsoleUI.getInputCursor();

    const state = getState().autocomplete;
    const { cache } = state;
    if (
      !force &&
      (!inputValue || /^[a-zA-Z0-9_$]/.test(inputValue.substring(cursor)))
    ) {
      return dispatch(autocompleteClear());
    }

    const input = inputValue.substring(0, cursor);
    const retrieveFromCache =
      !force &&
      cache &&
      cache.input &&
      input.startsWith(cache.input) &&
      /[a-zA-Z0-9]$/.test(input) &&
      frameActorId === cache.frameActorId;

    if (retrieveFromCache) {
      return dispatch(autoCompleteDataRetrieveFromCache(input));
    }

    let authorizedEvaluations =
      Array.isArray(state.authorizedEvaluations) &&
      state.authorizedEvaluations.length > 0
        ? state.authorizedEvaluations
        : [];

    if (Array.isArray(getterPath) && getterPath.length > 0) {
      // We need to check for any previous authorizations. For example, here if getterPath
      // is ["a", "b", "c", "d"], we want to see if there was any other path that was
      // authorized in a previous request. For that, we only add the previous
      // authorizations if the last auth is contained in getterPath. (for the example, we
      // would keep if it is [["a", "b"]], not if [["b"]] nor [["f", "g"]])
      const last = authorizedEvaluations[authorizedEvaluations.length - 1];
      const concat = !last || last.every((x, index) => x === getterPath[index]);
      if (concat) {
        authorizedEvaluations.push(getterPath);
      } else {
        authorizedEvaluations = [getterPath];
      }
    }

    return dispatch(
      autocompleteDataFetch({
        input,
        frameActorId,
        webconsoleFront,
        authorizedEvaluations,
        force,
        expressionVars,
      })
    );
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
 *        - {Boolean} force: true if the user forced an autocompletion (with Ctrl+Space).
 *        - {WebConsoleFront} client: The webconsole front.
 *        - {Array} authorizedEvaluations: Array of the properties access which can be
 *                  executed by the engine.
 *                   Example: [["x", "myGetter"], ["x", "myGetter", "y", "glitter"]]
 *                  to retrieve properties of `x.myGetter.` and `x.myGetter.y.glitter`.
 */
function autocompleteDataFetch({
  input,
  frameActorId,
  force,
  webconsoleFront,
  authorizedEvaluations,
  expressionVars,
}) {
  return async ({ dispatch, webConsoleUI }) => {
    const selectedNodeActor = webConsoleUI.getSelectedNodeActorID();
    const id = generateRequestId();
    dispatch({ type: AUTOCOMPLETE_PENDING_REQUEST, id });

    webconsoleFront
      .autocomplete(
        input,
        undefined,
        frameActorId,
        selectedNodeActor,
        authorizedEvaluations,
        expressionVars
      )
      .then(data => {
        dispatch(
          autocompleteDataReceive({
            id,
            input,
            force,
            frameActorId,
            data,
            authorizedEvaluations,
            expressionVars,
          })
        );
      })
      .catch(e => {
        console.error("failed autocomplete", e);
        dispatch(autocompleteClear());
      });
  };
}

/**
 * Called when we receive the autocompletion data from the server.
 *
 * @param {Object} Object of the following shape:
 *        - {Integer} id: The autocompletion request id. This will be used in the reducer
 *                        to check that we update the state with the last request results.
 *        - {String} input: the expression that we want to complete.
 *        - {String} frameActorId: The id of the frame we want to autocomplete in.
 *        - {Boolean} force: true if the user forced an autocompletion (with Ctrl+Space).
 *        - {Object} data: The actual data returned from the server.
 *        - {Array} authorizedEvaluations: Array of the properties access which can be
 *                  executed by the engine.
 *                   Example: [["x", "myGetter"], ["x", "myGetter", "y", "glitter"]]
 *                  to retrieve properties of `x.myGetter.` and `x.myGetter.y.glitter`.
 */
function autocompleteDataReceive({
  id,
  input,
  frameActorId,
  force,
  data,
  authorizedEvaluations,
}) {
  return {
    type: AUTOCOMPLETE_DATA_RECEIVE,
    id,
    input,
    force,
    frameActorId,
    data,
    authorizedEvaluations,
  };
}

module.exports = {
  autocompleteClear,
  autocompleteUpdate,
};
