"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.updatePreview = updatePreview;
exports.setPreview = setPreview;
exports.clearPreview = clearPreview;

var _preview = require("../utils/preview");

var _ast = require("../utils/ast");

var _devtoolsSourceMap = require("devtools/client/shared/source-map/index.js");

var _promise = require("./utils/middleware/promise");

var _getExpression = require("../utils/editor/get-expression");

var _selectors = require("../selectors/index");

var _expressions = require("./expressions");

var _pause = require("./pause/index");

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function findExpressionMatch(state, codeMirror, tokenPos) {
  const source = (0, _selectors.getSelectedSource)(state);
  const symbols = (0, _selectors.getSymbols)(state, source);
  let match;

  if (!symbols || symbols.loading) {
    match = (0, _getExpression.getExpressionFromCoords)(codeMirror, tokenPos);
  } else {
    match = (0, _ast.findBestMatchExpression)(symbols, tokenPos);
  }

  return match;
}

function updatePreview(target, tokenPos, codeMirror) {
  return ({
    dispatch,
    getState,
    client,
    sourceMaps
  }) => {
    const cursorPos = target.getBoundingClientRect();

    if ((0, _selectors.getCanRewind)(getState()) || !(0, _selectors.isSelectedFrameVisible)(getState()) || !(0, _selectors.isLineInScope)(getState(), tokenPos.line)) {
      return;
    }

    const match = findExpressionMatch(getState(), codeMirror, tokenPos);

    if (!match) {
      return;
    }

    const {
      expression,
      location
    } = match;

    if ((0, _preview.isConsole)(expression)) {
      return;
    }

    dispatch(setPreview(expression, location, tokenPos, cursorPos));
  };
}

function setPreview(expression, location, tokenPos, cursorPos) {
  return async ({
    dispatch,
    getState,
    client,
    sourceMaps
  }) => {
    await dispatch({
      type: "SET_PREVIEW",
      [_promise.PROMISE]: async function () {
        const source = (0, _selectors.getSelectedSource)(getState());
        const sourceId = source.id;
        const selectedFrame = (0, _selectors.getSelectedFrame)(getState());

        if (location && !(0, _devtoolsSourceMap.isGeneratedId)(sourceId)) {
          expression = await dispatch((0, _expressions.getMappedExpression)(expression));
        }

        if (!selectedFrame) {
          return;
        }

        const {
          result
        } = await client.evaluateInFrame(expression, selectedFrame.id);

        if (result === undefined) {
          return;
        }

        const extra = await dispatch((0, _pause.getExtra)(expression, result));
        return {
          expression,
          result,
          location,
          tokenPos,
          cursorPos,
          extra
        };
      }()
    });
  };
}

function clearPreview() {
  return ({
    dispatch,
    getState,
    client
  }) => {
    const currentSelection = (0, _selectors.getPreview)(getState());

    if (!currentSelection) {
      return;
    }

    return dispatch({
      type: "CLEAR_SELECTION"
    });
  };
}