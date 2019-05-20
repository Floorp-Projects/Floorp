/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { isConsole } from "../utils/preview";
import { findBestMatchExpression } from "../utils/ast";
import { getExpressionFromCoords } from "../utils/editor/get-expression";
import { isOriginal } from "../utils/source";
import { isTesting } from "devtools-environment";

import {
  getPreview,
  isLineInScope,
  isSelectedFrameVisible,
  getSelectedSource,
  getSelectedFrame,
  getSymbols,
  getCurrentThread,
} from "../selectors";

import { getMappedExpression } from "./expressions";

import type { Action, ThunkArgs } from "./types";
import type { Position, Context } from "../types";
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
  cx: Context,
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

    dispatch(setPreview(cx, expression, location, tokenPos, cursorPos, target));
  };
}

export function setPreview(
  cx: Context,
  expression: string,
  location: AstLocation,
  tokenPos: Position,
  cursorPos: ClientRect,
  target: HTMLElement
) {
  return async ({ dispatch, getState, client, sourceMaps }: ThunkArgs) => {
    if (getPreview(getState())) {
      dispatch(clearPreview(cx));
    }

    const source = getSelectedSource(getState());
    if (!source) {
      return;
    }

    const thread = getCurrentThread(getState());
    const selectedFrame = getSelectedFrame(getState(), thread);

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
      thread,
    });

    // Error case occurs for a token that follows an errored evaluation
    // https://github.com/firefox-devtools/debugger/pull/8056
    // Accommodating for null allows us to show preview for falsy values
    // line "", false, null, Nan, and more
    if (result === null) {
      return;
    }

    // Handle cases where the result is invisible to the debugger
    // and not possible to preview. Bug 1548256
    if (result.class && result.class.includes("InvisibleToDebugger")) {
      return;
    }

    const root = {
      name: expression,
      path: expression,
      contents: { value: result },
    };
    const properties = await client.loadObjectProperties(root);

    // The first time a popup is rendered, the mouse should be hovered
    // on the token. If it happens to be hovered on whitespace, it should
    // not render anything
    if (!target.matches(":hover") && !isTesting()) {
      return;
    }

    dispatch({
      type: "SET_PREVIEW",
      cx,
      value: {
        expression,
        result,
        properties,
        root,
        location,
        tokenPos,
        cursorPos,
        target,
      },
    });
  };
}

export function clearPreview(cx: Context) {
  return ({ dispatch, getState, client }: ThunkArgs) => {
    const currentSelection = getPreview(getState());
    if (!currentSelection) {
      return;
    }

    return dispatch(
      ({
        type: "CLEAR_PREVIEW",
        cx,
      }: Action)
    );
  };
}
