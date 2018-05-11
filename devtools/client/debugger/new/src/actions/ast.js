"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.setSourceMetaData = setSourceMetaData;
exports.setSymbols = setSymbols;
exports.setOutOfScopeLocations = setOutOfScopeLocations;
exports.setPausePoints = setPausePoints;

var _selectors = require("../selectors/index");

var _pause = require("./pause/index");

var _setInScopeLines = require("./ast/setInScopeLines");

var _parser = require("../workers/parser/index");

var _promise = require("./utils/middleware/promise");

var _devtoolsSourceMap = require("devtools/client/shared/source-map/index.js");

var _prefs = require("../utils/prefs");

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function setSourceMetaData(sourceId) {
  return async ({
    dispatch,
    getState
  }) => {
    const source = (0, _selectors.getSource)(getState(), sourceId);

    if (!source || !source.text || source.isWasm) {
      return;
    }

    const framework = await (0, _parser.getFramework)(source.id);
    dispatch({
      type: "SET_SOURCE_METADATA",
      sourceId: source.id,
      sourceMetaData: {
        framework
      }
    });
  };
}

function setSymbols(sourceId) {
  return async ({
    dispatch,
    getState
  }) => {
    const source = (0, _selectors.getSource)(getState(), sourceId);

    if (!source || !source.text || source.isWasm || (0, _selectors.hasSymbols)(getState(), source)) {
      return;
    }

    await dispatch({
      type: "SET_SYMBOLS",
      sourceId,
      [_promise.PROMISE]: (0, _parser.getSymbols)(sourceId)
    });

    if ((0, _selectors.isPaused)(getState())) {
      await dispatch((0, _pause.fetchExtra)());
      await dispatch((0, _pause.mapFrames)());
    }

    await dispatch(setPausePoints(sourceId));
    await dispatch(setSourceMetaData(sourceId));
  };
}

function setOutOfScopeLocations() {
  return async ({
    dispatch,
    getState
  }) => {
    const location = (0, _selectors.getSelectedLocation)(getState());

    if (!location) {
      return;
    }

    const source = (0, _selectors.getSource)(getState(), location.sourceId);
    let locations = null;

    if (location.line && source && (0, _selectors.isPaused)(getState())) {
      locations = await (0, _parser.findOutOfScopeLocations)(source.get("id"), location);
    }

    dispatch({
      type: "OUT_OF_SCOPE_LOCATIONS",
      locations
    });
    dispatch((0, _setInScopeLines.setInScopeLines)());
  };
}

function setPausePoints(sourceId) {
  return async ({
    dispatch,
    getState,
    client
  }) => {
    const source = (0, _selectors.getSource)(getState(), sourceId);

    if (!_prefs.features.pausePoints || !source || !source.text || source.isWasm) {
      return;
    }

    const pausePoints = await (0, _parser.getPausePoints)(sourceId);

    if ((0, _devtoolsSourceMap.isGeneratedId)(sourceId)) {
      await client.setPausePoints(sourceId, pausePoints);
    }

    dispatch({
      type: "SET_PAUSE_POINTS",
      sourceText: source.text,
      sourceId,
      pausePoints
    });
  };
}