/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import {
  getSelectedFrameId,
  getSettledSourceTextContent,
  isMapScopesEnabled,
  getSelectedFrame,
  getSelectedGeneratedScope,
  getSelectedOriginalScope,
  getThreadContext,
  getFirstSourceActorForGeneratedSource,
} from "../../selectors";
import {
  loadOriginalSourceText,
  loadGeneratedSourceText,
} from "../sources/loadSourceText";
import { PROMISE } from "../utils/middleware/promise";
import assert from "../../utils/assert";

import { log } from "../../utils/log";
import { isGenerated } from "../../utils/source";

import { buildMappedScopes } from "../../utils/pause/mapScopes";
import { isFulfilled } from "../../utils/async-value";

import { getMappedLocation } from "../../utils/source-maps";

const expressionRegex = /\bfp\(\)/g;

export async function buildOriginalScopes(
  frame,
  client,
  cx,
  frameId,
  generatedScopes
) {
  if (!frame.originalVariables) {
    throw new TypeError("(frame.originalVariables: XScopeVariables)");
  }
  const originalVariables = frame.originalVariables;
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
    frameId,
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

    const cx = getThreadContext(getState());

    if (getSelectedOriginalScope(getState(), cx.thread)) {
      return;
    }

    const scopes = getSelectedGeneratedScope(getState(), cx.thread);
    const frame = getSelectedFrame(getState(), cx.thread);
    if (!scopes || !frame) {
      return;
    }

    dispatch(mapScopes(cx, Promise.resolve(scopes.scope), frame));
  };
}

export function mapScopes(cx, scopes, frame) {
  return async function (thunkArgs) {
    const { dispatch, client, getState } = thunkArgs;
    assert(cx.thread == frame.thread, "Thread mismatch");

    await dispatch({
      type: "MAP_SCOPES",
      cx,
      thread: cx.thread,
      frame,
      [PROMISE]: (async function () {
        if (frame.isOriginal && frame.originalVariables) {
          const frameId = getSelectedFrameId(getState(), cx.thread);
          return buildOriginalScopes(frame, client, cx, frameId, scopes);
        }

        return dispatch(getMappedScopes(cx, scopes, frame));
      })(),
    });
  };
}

export function getMappedScopes(cx, scopes, frame) {
  return async function (thunkArgs) {
    const { getState, dispatch } = thunkArgs;
    const generatedSource = frame.generatedLocation.source;

    const source = frame.location.source;

    if (
      !isMapScopesEnabled(getState()) ||
      !source ||
      !generatedSource ||
      generatedSource.isWasm ||
      source.isPrettyPrinted ||
      isGenerated(source)
    ) {
      return null;
    }

    // Load source text for the original source
    await dispatch(loadOriginalSourceText({ cx, source }));

    const generatedSourceActor = getFirstSourceActorForGeneratedSource(
      getState(),
      generatedSource.id
    );

    // Also load source text for its corresponding generated source
    await dispatch(
      loadGeneratedSourceText({
        cx,
        sourceActor: generatedSourceActor,
      })
    );

    try {
      // load original source text content
      const content = getSettledSourceTextContent(getState(), frame.location);

      return await buildMappedScopes(
        source,
        content && isFulfilled(content)
          ? content.value
          : { type: "text", value: "", contentType: undefined },
        frame,
        await scopes,
        thunkArgs
      );
    } catch (e) {
      log(e);
      return null;
    }
  };
}

export function getMappedScopesForLocation(location) {
  return async function (thunkArgs) {
    const { dispatch, getState } = thunkArgs;
    const cx = getThreadContext(getState());
    const mappedLocation = await getMappedLocation(location, thunkArgs);
    return dispatch(getMappedScopes(cx, null, mappedLocation));
  };
}
