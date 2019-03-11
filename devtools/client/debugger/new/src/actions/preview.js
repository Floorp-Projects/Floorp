/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
import { isConsole } from "../utils/preview";
import { findBestMatchExpression } from "../utils/ast";
import { PROMISE } from "./utils/middleware/promise";
import { getExpressionFromCoords } from "../utils/editor/get-expression";
import { isOriginal } from "../utils/source";

import {
  getPreview,
  isLineInScope,
  isSelectedFrameVisible,
  getSelectedSource,
  getSelectedFrame,
  getSymbols
} from "../selectors";

import { getMappedExpression } from "./expressions";

import type { Action, ThunkArgs } from "./types";
import type { Position } from "../types";
import type { AstLocation } from "../workers/parser";

function findExpressionMatch(state, codeMirror, tokenPos) {
  const source = getSelectedSource(state);
  if (!source) {
    return;
  }

  const symbols = getSymbols(state, source);

  let match;
  if (!symbols || symbols.loading) {
    match = getExpressionFromCoords(codeMirror, tokenPos);
  } else {
    match = findBestMatchExpression(symbols, tokenPos);
  }
  return match;
}

export function updatePreview(
  target: HTMLElement,
  tokenPos: Object,
  codeMirror: any
) {
  return ({ dispatch, getState, client, sourceMaps }: ThunkArgs) => {
    const cursorPos = target.getBoundingClientRect();

    if (
      !isSelectedFrameVisible(getState()) ||
      !isLineInScope(getState(), tokenPos.line)
    ) {
      return;
    }

    const match = findExpressionMatch(getState(), codeMirror, tokenPos);
    if (!match) {
      return;
    }

    const { expression, location } = match;

    if (isConsole(expression)) {
      return;
    }

    dispatch(setPreview(expression, location, tokenPos, cursorPos));
  };
}

export function setPreview(
  expression: string,
  location: AstLocation,
  tokenPos: Position,
  cursorPos: ClientRect
) {
  return async ({ dispatch, getState, client, sourceMaps }: ThunkArgs) => {
    await dispatch({
      type: "SET_PREVIEW",
      [PROMISE]: (async function() {
        const source = getSelectedSource(getState());
        if (!source) {
          return;
        }

        const selectedFrame = getSelectedFrame(getState());

        if (location && isOriginal(source)) {
          const mapResult = await dispatch(getMappedExpression(expression));
          if (mapResult) {
            expression = mapResult.expression;
          }
        }

        if (!selectedFrame) {
          return;
        }

        const { result } = await client.evaluateInFrame(expression, {
          frameId: selectedFrame.id,
          thread: selectedFrame.thread
        });

        // Error case occurs for a token that follows an errored evaluation
        // https://github.com/firefox-devtools/debugger/pull/8056
        // Accommodating for null allows us to show preview for falsy values
        // line "", false, null, Nan, and more
        if (result === null) {
          return;
        }

        return {
          expression,
          result,
          location,
          tokenPos,
          cursorPos
        };
      })()
    });
  };
}

export function clearPreview() {
  return ({ dispatch, getState, client }: ThunkArgs) => {
    const currentSelection = getPreview(getState());
    if (!currentSelection) {
      return;
    }

    return dispatch(
      ({
        type: "CLEAR_SELECTION"
      }: Action)
    );
  };
}
