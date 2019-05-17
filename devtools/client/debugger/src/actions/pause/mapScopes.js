/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import {
  getSource,
  getSourceContent,
  isMapScopesEnabled,
  getSelectedFrame,
  getSelectedGeneratedScope,
  getSelectedOriginalScope,
  getThreadContext,
} from "../../selectors";
import { loadSourceText } from "../sources/loadSourceText";
import { PROMISE } from "../utils/middleware/promise";
import assert from "../../utils/assert";

import { log } from "../../utils/log";
import { isGenerated, isOriginal } from "../../utils/source";
import type { Frame, Scope, ThreadContext } from "../../types";

import type { ThunkArgs } from "../types";

import { buildMappedScopes } from "../../utils/pause/mapScopes";
import { isFulfilled } from "../../utils/async-value";

export function toggleMapScopes() {
  return async function({ dispatch, getState, client, sourceMaps }: ThunkArgs) {
    if (isMapScopesEnabled(getState())) {
      return dispatch({ type: "TOGGLE_MAP_SCOPES", mapScopes: false });
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

export function mapScopes(
  cx: ThreadContext,
  scopes: Promise<Scope>,
  frame: Frame
) {
  return async function(thunkArgs: ThunkArgs) {
    const { dispatch, getState } = thunkArgs;
    assert(cx.thread == frame.thread, "Thread mismatch");

    const generatedSource = getSource(
      getState(),
      frame.generatedLocation.sourceId
    );

    const source = getSource(getState(), frame.location.sourceId);

    await dispatch({
      type: "MAP_SCOPES",
      cx,
      thread: cx.thread,
      frame,
      [PROMISE]: (async function() {
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

        await dispatch(loadSourceText({ cx, source }));
        if (isOriginal(source)) {
          await dispatch(loadSourceText({ cx, source: generatedSource }));
        }

        try {
          const content =
            getSource(getState(), source.id) &&
            getSourceContent(getState(), source.id);

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
      })(),
    });
  };
}
