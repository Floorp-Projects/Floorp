/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import {
  getSettledSourceTextContent,
  isMapScopesEnabled,
  getSelectedFrame,
  getGeneratedFrameScope,
  getOriginalFrameScope,
  getFirstSourceActorForGeneratedSource,
  getCurrentThread,
} from "../../selectors/index";
import {
  loadOriginalSourceText,
  loadGeneratedSourceText,
} from "../sources/loadSourceText";
import { validateSelectedFrame } from "../../utils/context";
import { PROMISE } from "../utils/middleware/promise";

import { log } from "../../utils/log";

import { buildMappedScopes } from "../../utils/pause/mapScopes/index";
import { isFulfilled } from "../../utils/async-value";

import { getMappedLocation } from "../../utils/source-maps";

const expressionRegex = /\bfp\(\)/g;

export async function buildOriginalScopes(
  selectedFrame,
  client,
  generatedScopes
) {
  if (!selectedFrame.originalVariables) {
    throw new TypeError("(frame.originalVariables: XScopeVariables)");
  }
  const originalVariables = selectedFrame.originalVariables;
  const frameBase = originalVariables.frameBase || "";

  const inputs = [];
  for (let i = 0; i < originalVariables.vars.length; i++) {
    const { expr } = originalVariables.vars[i];
    const expression = expr
      ? expr.replace(expressionRegex, frameBase)
      : "void 0";

    inputs[i] = expression;
  }

  const results = await client.evaluateExpressions(inputs, {
    frameId: selectedFrame.id,
  });

  const variables = {};
  for (let i = 0; i < originalVariables.vars.length; i++) {
    const { name } = originalVariables.vars[i];
    variables[name] = { value: results[i].result };
  }

  const bindings = {
    arguments: [],
    variables,
  };

  const { actor } = await generatedScopes;
  const scope = {
    type: "function",
    scopeKind: "",
    actor,
    bindings,
    parent: null,
    function: null,
    block: null,
  };
  return {
    mappings: {},
    scope,
  };
}

export function toggleMapScopes() {
  return async function ({ dispatch, getState }) {
    if (isMapScopesEnabled(getState())) {
      dispatch({ type: "TOGGLE_MAP_SCOPES", mapScopes: false });
      return;
    }

    dispatch({ type: "TOGGLE_MAP_SCOPES", mapScopes: true });

    // Ignore the call if there is no selected frame (we are not paused?)
    const state = getState();
    const selectedFrame = getSelectedFrame(state, getCurrentThread(state));
    if (!selectedFrame) {
      return;
    }

    if (getOriginalFrameScope(getState(), selectedFrame)) {
      return;
    }

    // Also ignore the call if we didn't fetch the scopes for the selected frame
    const scopes = getGeneratedFrameScope(getState(), selectedFrame);
    if (!scopes) {
      return;
    }

    dispatch(mapScopes(selectedFrame, Promise.resolve(scopes.scope)));
  };
}

export function mapScopes(selectedFrame, scopes) {
  return async function (thunkArgs) {
    const { getState, dispatch, client } = thunkArgs;

    await dispatch({
      type: "MAP_SCOPES",
      selectedFrame,
      [PROMISE]: (async function () {
        if (selectedFrame.isOriginal && selectedFrame.originalVariables) {
          return buildOriginalScopes(selectedFrame, client, scopes);
        }

        // getMappedScopes is only specific to the sources where we map the variables
        // in scope and so only need a thread context. Assert that we are on the same thread
        // before retrieving a thread context.
        validateSelectedFrame(getState(), selectedFrame);

        return dispatch(getMappedScopes(scopes, selectedFrame));
      })(),
    });
  };
}

/**
 * Get scopes mapped for a precise location.
 *
 * @param {Promise} scopes
 *        Can be null. Result of Commands.js's client.getFrameScopes
 * @param {Objects locations
 *        Frame object, or custom object with 'location' and 'generatedLocation' attributes.
 */
export function getMappedScopes(scopes, locations) {
  return async function (thunkArgs) {
    const { getState, dispatch } = thunkArgs;
    const generatedSource = locations.generatedLocation.source;
    const source = locations.location.source;

    if (
      !isMapScopesEnabled(getState()) ||
      !source ||
      !generatedSource ||
      generatedSource.isWasm ||
      source.isPrettyPrinted ||
      !source.isOriginal
    ) {
      return null;
    }

    // Load source text for the original source
    await dispatch(loadOriginalSourceText(source));

    const generatedSourceActor = getFirstSourceActorForGeneratedSource(
      getState(),
      generatedSource.id
    );

    // Also load source text for its corresponding generated source
    await dispatch(loadGeneratedSourceText(generatedSourceActor));

    try {
      const content =
        // load original source text content
        getSettledSourceTextContent(getState(), locations.location);

      return await buildMappedScopes(
        source,
        content && isFulfilled(content)
          ? content.value
          : { type: "text", value: "", contentType: undefined },
        locations,
        await scopes,
        thunkArgs
      );
    } catch (e) {
      log(e);
      return null;
    }
  };
}

/**
 * Used to map variables used within conditional and log breakpoints.
 */
export function getMappedScopesForLocation(location) {
  return async function (thunkArgs) {
    const { dispatch } = thunkArgs;
    const mappedLocation = await getMappedLocation(location, thunkArgs);
    return dispatch(getMappedScopes(null, mappedLocation));
  };
}
