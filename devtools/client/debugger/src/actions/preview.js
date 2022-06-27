/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { isConsole } from "../utils/preview";
import { findBestMatchExpression } from "../utils/ast";
import { getGrip, getFront } from "../utils/evaluation-result";
import { getExpressionFromCoords } from "../utils/editor/get-expression";
import { isNodeTest } from "../utils/environment";

import {
  getPreview,
  isLineInScope,
  isSelectedFrameVisible,
  getSelectedSource,
  getSelectedFrame,
  getSymbols,
  getCurrentThread,
  getPreviewCount,
  getSelectedException,
} from "../selectors";

import { getMappedExpression } from "./expressions";

function findExpressionMatch(state, codeMirror, tokenPos) {
  const source = getSelectedSource(state);
  if (!source) {
    return;
  }

  const symbols = getSymbols(state, source);

  let match;
  if (!symbols) {
    match = getExpressionFromCoords(codeMirror, tokenPos);
  } else {
    match = findBestMatchExpression(symbols, tokenPos);
  }
  return match;
}

export function updatePreview(cx, target, tokenPos, codeMirror) {
  return ({ dispatch, getState, client, sourceMaps }) => {
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
  cx,
  expression,
  location,
  tokenPos,
  cursorPos,
  target
) {
  return async ({ dispatch, getState, client, sourceMaps }) => {
    dispatch({ type: "START_PREVIEW" });
    const previewCount = getPreviewCount(getState());
    if (getPreview(getState())) {
      dispatch(clearPreview(cx));
    }

    const source = getSelectedSource(getState());
    if (!source) {
      return;
    }

    const thread = getCurrentThread(getState());
    const selectedFrame = getSelectedFrame(getState(), thread);

    if (location && source.isOriginal) {
      const mapResult = await dispatch(getMappedExpression(expression));
      if (mapResult) {
        expression = mapResult.expression;
      }
    }

    if (!selectedFrame) {
      return;
    }

    const { result } = await client.evaluate(expression, {
      frameId: selectedFrame.id,
    });

    const resultGrip = getGrip(result);

    // Error case occurs for a token that follows an errored evaluation
    // https://github.com/firefox-devtools/debugger/pull/8056
    // Accommodating for null allows us to show preview for falsy values
    // line "", false, null, Nan, and more
    if (resultGrip === null) {
      return;
    }

    // Handle cases where the result is invisible to the debugger
    // and not possible to preview. Bug 1548256
    if (
      resultGrip &&
      resultGrip.class &&
      typeof resultGrip.class === "string" &&
      resultGrip.class.includes("InvisibleToDebugger")
    ) {
      return;
    }

    const root = {
      name: expression,
      path: expression,
      contents: {
        value: resultGrip,
        front: getFront(result),
      },
    };
    const properties = await client.loadObjectProperties(root, thread);

    // The first time a popup is rendered, the mouse should be hovered
    // on the token. If it happens to be hovered on whitespace, it should
    // not render anything
    if (!target.matches(":hover") && !isNodeTest()) {
      return;
    }

    // Don't finish dispatching if another setPreview was started
    if (previewCount != getPreviewCount(getState())) {
      return;
    }

    dispatch({
      type: "SET_PREVIEW",
      cx,
      value: {
        expression,
        resultGrip,
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

export function clearPreview(cx) {
  return ({ dispatch, getState, client }) => {
    const currentSelection = getPreview(getState());
    if (!currentSelection) {
      return;
    }

    return dispatch({
      type: "CLEAR_PREVIEW",
      cx,
    });
  };
}

export function setExceptionPreview(cx, target, tokenPos, codeMirror) {
  return async ({ dispatch, getState }) => {
    const cursorPos = target.getBoundingClientRect();

    const match = findExpressionMatch(getState(), codeMirror, tokenPos);
    if (!match) {
      return;
    }

    const tokenColumnStart = match.location.start.column + 1;
    const exception = getSelectedException(
      getState(),
      tokenPos.line,
      tokenColumnStart
    );
    if (!exception) {
      return;
    }

    dispatch({
      type: "SET_PREVIEW",
      cx,
      value: {
        exception,
        location: match.location,
        tokenPos,
        cursorPos,
        target,
      },
    });
  };
}
