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

const {
  analyzeInputString,
  shouldInputBeAutocompleted,
} = require("devtools/shared/webconsole/analyze-input-string");

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
    const mappedVars = hud.getMappedVariables() ?? {};
    const allVars = (expressionVars ?? []).concat(Object.keys(mappedVars));
    const frameActorId = await hud.getSelectedFrameActorID();

    const cursor = webConsoleUI.getInputCursor();

    const state = getState().autocomplete;
    const { cache } = state;
    if (
      !force &&
      (!inputValue || /^[a-zA-Z0-9_$]/.test(inputValue.substring(cursor)))
    ) {
      return dispatch(autocompleteClear());
    }

    const rawInput = inputValue.substring(0, cursor);
    const retrieveFromCache =
      !force &&
      cache &&
      cache.input &&
      rawInput.startsWith(cache.input) &&
      /[a-zA-Z0-9]$/.test(rawInput) &&
      frameActorId === cache.frameActorId;

    if (retrieveFromCache) {
      return dispatch(autoCompleteDataRetrieveFromCache(rawInput));
    }

    const authorizedEvaluations = updateAuthorizedEvaluations(
      state.authorizedEvaluations,
      getterPath,
      mappedVars
    );

    const { input, originalExpression } = await getMappedInput(
      rawInput,
      mappedVars,
      hud
    );

    return dispatch(
      autocompleteDataFetch({
        input,
        frameActorId,
        authorizedEvaluations,
        force,
        allVars,
        mappedVars,
        originalExpression,
      })
    );
  };
}

/**
 * Combine or replace authorizedEvaluations with the newly authorized getter path, if any.
 * @param {Array<Array<String>>} authorizedEvaluations Existing authorized evaluations (may
 * be updated in place)
 * @param {Array<String>} getterPath The new getter path
 * @param {{[String]: String}} mappedVars Map of original to generated variable names.
 * @returns {Array<Array<String>>} The updated authorized evaluations (the original array,
 * if it was updated in place) */
function updateAuthorizedEvaluations(
  authorizedEvaluations,
  getterPath,
  mappedVars
) {
  if (!Array.isArray(authorizedEvaluations) || !authorizedEvaluations.length) {
    authorizedEvaluations = [];
  }

  if (Array.isArray(getterPath) && getterPath.length) {
    // We need to check for any previous authorizations. For example, here if getterPath
    // is ["a", "b", "c", "d"], we want to see if there was any other path that was
    // authorized in a previous request. For that, we only add the previous
    // authorizations if the last auth is contained in getterPath. (for the example, we
    // would keep if it is [["a", "b"]], not if [["b"]] nor [["f", "g"]])
    const last = authorizedEvaluations[authorizedEvaluations.length - 1];

    const generatedPath = mappedVars[getterPath[0]]?.split(".");
    if (generatedPath) {
      getterPath = generatedPath.concat(getterPath.slice(1));
    }

    const isMappedVariable =
      generatedPath && getterPath.length === generatedPath.length;
    const concat = !last || last.every((x, index) => x === getterPath[index]);
    if (isMappedVariable) {
      // If the path consists only of an original variable, add all the prefixes of its
      // mapping. For example, for myVar => a.b.c, authorize a, a.b, and a.b.c. This
      // ensures we'll only show a prompt for myVar once even if a.b and a.b.c are both
      // unsafe getters.
      authorizedEvaluations = generatedPath.map((_, i) =>
        generatedPath.slice(0, i + 1)
      );
    } else if (concat) {
      authorizedEvaluations.push(getterPath);
    } else {
      authorizedEvaluations = [getterPath];
    }
  }
  return authorizedEvaluations;
}

/**
 * Apply source mapping to the autocomplete input.
 * @param {String} rawInput The input to map.
 * @param {{[String]: String}} mappedVars Map of original to generated variable names.
 * @param {WebConsole} hud A reference to the webconsole hud.
 * @returns {String} The source-mapped expression to autocomplete.
 */
async function getMappedInput(rawInput, mappedVars, hud) {
  if (!mappedVars || !Object.keys(mappedVars).length) {
    return { input: rawInput, originalExpression: undefined };
  }

  const inputAnalysis = analyzeInputString(rawInput, 500);
  if (!shouldInputBeAutocompleted(inputAnalysis)) {
    return { input: rawInput, originalExpression: undefined };
  }

  const {
    mainExpression: originalExpression,
    isPropertyAccess,
    isElementAccess,
    lastStatement,
  } = inputAnalysis;

  // If we're autocompleting a variable name, pass it through unchanged so that we
  // show original variable names rather than generated ones.
  // For example, if we have the mapping `myVariable` => `x`, show variables starting
  // with myVariable rather than x.
  if (!isPropertyAccess && !isElementAccess) {
    return { input: lastStatement, originalExpression };
  }

  let generated =
    (await hud.getMappedExpression(originalExpression))?.expression ??
    originalExpression;
  // Strip off the semicolon if the expression was converted to a statement
  const trailingSemicolon = /;\s*$/;
  if (
    trailingSemicolon.test(generated) &&
    !trailingSemicolon.test(originalExpression)
  ) {
    generated = generated.slice(0, generated.lastIndexOf(";"));
  }

  const suffix = lastStatement.slice(originalExpression.length);
  return { input: generated + suffix, originalExpression };
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
 *        - {Array} authorizedEvaluations: Array of the properties access which can be
 *                  executed by the engine.
 *                   Example: [["x", "myGetter"], ["x", "myGetter", "y", "glitter"]]
 *                  to retrieve properties of `x.myGetter.` and `x.myGetter.y.glitter`.
 */
function autocompleteDataFetch({
  input,
  frameActorId,
  force,
  authorizedEvaluations,
  allVars,
  mappedVars,
  originalExpression,
}) {
  return async ({ dispatch, commands, webConsoleUI, hud }) => {
    // Retrieve the right WebConsole front that relates either to (by order of priority):
    // - the currently selected target in the context selector
    //   (contextSelectedTargetFront),
    // - the currently selected Node in the inspector (selectedNodeActor),
    // - the currently selected frame in the debugger (when paused) (frameActor),
    // - the currently selected target in the iframe dropdown
    //   (selectedTargetFront from the TargetCommand)
    const selectedNodeActorId = webConsoleUI.getSelectedNodeActorID();

    let targetFront = commands.targetCommand.selectedTargetFront;
    // Note that getSelectedTargetFront will return null if we default to the top level target.
    // For now, in the browser console (without a toolbox), we don't support the context selector.
    const contextSelectorTargetFront = hud.toolbox
      ? hud.toolbox.getSelectedTargetFront()
      : null;
    const selectedActorId = selectedNodeActorId || frameActorId;
    if (contextSelectorTargetFront) {
      targetFront = contextSelectorTargetFront;
    } else if (selectedActorId) {
      const selectedFront = commands.client.getFrontByID(selectedActorId);
      if (selectedFront) {
        targetFront = selectedFront.targetFront;
      }
    }

    const webconsoleFront = await targetFront.getFront("console");

    const id = generateRequestId();
    dispatch({ type: AUTOCOMPLETE_PENDING_REQUEST, id });

    webconsoleFront
      .autocomplete(
        input,
        undefined,
        frameActorId,
        selectedNodeActorId,
        authorizedEvaluations,
        allVars
      )
      .then(data => {
        if (data.isUnsafeGetter && originalExpression !== undefined) {
          data.getterPath = unmapGetterPath(
            data.getterPath,
            originalExpression,
            mappedVars
          );
        }
        return dispatch(
          autocompleteDataReceive({
            id,
            input,
            force,
            frameActorId,
            data,
            authorizedEvaluations,
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
 * Replace generated variable names in an unsafe getter path with their original
 * counterparts.
 * @param {Array<String>} getterPath Array of properties leading up to and including the
 * unsafe getter.
 * @param {String} originalExpression The expression that was evaluated, before mapping.
 * @param {{[String]: String}} mappedVars Map of original to generated variable names.
 * @returns {Array<String>} An updated getter path containing original variables.
 */
function unmapGetterPath(getterPath, originalExpression, mappedVars) {
  // We know that the original expression is a sequence of property accesses, that only
  // the first part can be a mapped variable, and that the getter path must start with
  // its generated path or be a prefix of it.

  // Suppose we have the expression `foo.bar`, which maps to `a.b.c.bar`.
  // Get the first part of the expression ("foo")
  const originalVariable = /^[^.[?]*/s.exec(originalExpression)[0].trim();
  const generatedVariable = mappedVars[originalVariable];
  if (generatedVariable) {
    // Get number of properties in "a.b.c"
    const generatedVariableParts = generatedVariable.split(".");
    // Replace ["a", "b", "c"] with "foo" in the getter path.
    // Note that this will also work if the getter path ends inside of the mapped
    // variable, like ["a", "b"].
    return [
      originalVariable,
      ...getterPath.slice(generatedVariableParts.length),
    ];
  }
  return getterPath;
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
