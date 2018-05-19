"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.updatePreview = updatePreview;
exports.setPreview = setPreview;
exports.clearPreview = clearPreview;

var _preview = require("../utils/preview");

var _ast = require("../utils/ast");

var _editor = require("../utils/editor/index");

var _devtoolsSourceMap = require("devtools/client/shared/source-map/index.js");

var _promise = require("./utils/middleware/promise");

var _getExpression = require("../utils/editor/get-expression");

var _selectors = require("../selectors/index");

var _expressions = require("./expressions");

var _pause = require("./pause/index");

var _lodash = require("devtools/client/shared/vendor/lodash");

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function isInvalidTarget(target) {
  if (!target || !target.innerText) {
    return true;
  }

  const tokenText = target.innerText.trim();
  const cursorPos = target.getBoundingClientRect(); // exclude literal tokens where it does not make sense to show a preview

  const invalidType = ["cm-atom", ""].includes(target.className); // exclude syntax where the expression would be a syntax error

  const invalidToken = tokenText === "" || tokenText.match(/^[(){}\|&%,.;=<>\+-/\*\s](?=)/);
  const isPresentation = target.attributes.role && target.attributes.getNamedItem("role").value == "presentation"; // exclude codemirror elements that are not tokens

  const invalidTarget = target.parentElement && !target.parentElement.closest(".CodeMirror-line") || cursorPos.top == 0;
  return invalidTarget || invalidToken || invalidType || isPresentation;
}

function updatePreview(target, editor) {
  return ({
    dispatch,
    getState,
    client,
    sourceMaps
  }) => {
    const tokenPos = (0, _editor.getTokenLocation)(editor.codeMirror, target);
    const cursorPos = target.getBoundingClientRect();
    const preview = (0, _selectors.getPreview)(getState());

    if ((0, _selectors.getCanRewind)(getState())) {
      return;
    }

    if (preview) {
      // Return early if we are currently showing another preview or
      // if we are mousing over the same token as before
      if (preview.updating || (0, _lodash.isEqual)(preview.tokenPos, tokenPos)) {
        return;
      } // We are mousing over a new token that is not in the preview


      if (!target.classList.contains("debug-expression")) {
        dispatch(clearPreview());
      }
    }

    if (isInvalidTarget(target)) {
      dispatch(clearPreview());
      return;
    }

    if (!(0, _selectors.isSelectedFrameVisible)(getState()) || !(0, _selectors.isLineInScope)(getState(), tokenPos.line)) {
      return;
    }

    const source = (0, _selectors.getSelectedSource)(getState());
    const symbols = (0, _selectors.getSymbols)(getState(), source);
    let match;

    if (!symbols || symbols.loading) {
      match = (0, _getExpression.getExpressionFromCoords)(editor.codeMirror, tokenPos);
    } else {
      match = (0, _ast.findBestMatchExpression)(symbols, tokenPos);
    }

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