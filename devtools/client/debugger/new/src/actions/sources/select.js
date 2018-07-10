"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.clearSelectedLocation = exports.setPendingSelectedLocation = exports.setSelectedLocation = undefined;
exports.selectSourceURL = selectSourceURL;
exports.selectSource = selectSource;
exports.selectLocation = selectLocation;
exports.selectSpecificLocation = selectSpecificLocation;
exports.selectSpecificSource = selectSpecificSource;
exports.jumpToMappedLocation = jumpToMappedLocation;
exports.jumpToMappedSelectedLocation = jumpToMappedSelectedLocation;

var _devtoolsSourceMap = require("devtools/client/shared/source-map/index.js");

var _ast = require("../ast");

var _ui = require("../ui");

var _prettyPrint = require("./prettyPrint");

var _tabs = require("./tabs");

var _loadSourceText = require("./loadSourceText");

var _prefs = require("../../utils/prefs");

var _source = require("../../utils/source");

var _location = require("../../utils/location");

var _sourceMaps = require("../../utils/source-maps");

var _selectors = require("../../selectors/index");

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Redux actions for the sources state
 * @module actions/sources
 */
const setSelectedLocation = exports.setSelectedLocation = (source, location) => ({
  type: "SET_SELECTED_LOCATION",
  source,
  location
});

const setPendingSelectedLocation = exports.setPendingSelectedLocation = (url, options) => ({
  type: "SET_PENDING_SELECTED_LOCATION",
  url: url,
  line: options.location ? options.location.line : null
});

const clearSelectedLocation = exports.clearSelectedLocation = () => ({
  type: "CLEAR_SELECTED_LOCATION"
});
/**
 * Deterministically select a source that has a given URL. This will
 * work regardless of the connection status or if the source exists
 * yet. This exists mostly for external things to interact with the
 * debugger.
 *
 * @memberof actions/sources
 * @static
 */


function selectSourceURL(url, options = {}) {
  return async ({
    dispatch,
    getState
  }) => {
    const source = (0, _selectors.getSourceByURL)(getState(), url);

    if (source) {
      const sourceId = source.id;
      const location = (0, _location.createLocation)({ ...options.location,
        sourceId
      });
      await dispatch(selectLocation(location));
    } else {
      dispatch(setPendingSelectedLocation(url, options));
    }
  };
}
/**
 * @memberof actions/sources
 * @static
 */


function selectSource(sourceId) {
  return async ({
    dispatch
  }) => {
    const location = (0, _location.createLocation)({
      sourceId
    });
    return await dispatch(selectLocation(location));
  };
}
/**
 * @memberof actions/sources
 * @static
 */


function selectLocation(location) {
  return async ({
    dispatch,
    getState,
    client
  }) => {
    const currentSource = (0, _selectors.getSelectedSource)(getState());

    if (!client) {
      // No connection, do nothing. This happens when the debugger is
      // shut down too fast and it tries to display a default source.
      return;
    }

    const source = (0, _selectors.getSource)(getState(), location.sourceId);

    if (!source) {
      // If there is no source we deselect the current selected source
      return dispatch(clearSelectedLocation());
    }

    const activeSearch = (0, _selectors.getActiveSearch)(getState());

    if (activeSearch !== "file") {
      dispatch((0, _ui.closeActiveSearch)());
    }

    dispatch((0, _tabs.addTab)(source.url, 0));
    dispatch(setSelectedLocation(source, location));
    await dispatch((0, _loadSourceText.loadSourceText)(source));
    const loadedSource = (0, _selectors.getSource)(getState(), source.id);

    if (!loadedSource) {
      // If there was a navigation while we were loading the loadedSource
      return;
    }

    if (_prefs.prefs.autoPrettyPrint && !(0, _selectors.getPrettySource)(getState(), loadedSource.id) && (0, _source.shouldPrettyPrint)(loadedSource) && (0, _source.isMinified)(loadedSource)) {
      await dispatch((0, _prettyPrint.togglePrettyPrint)(loadedSource.id));
      dispatch((0, _tabs.closeTab)(loadedSource.url));
    }

    dispatch((0, _ast.setSymbols)(loadedSource.id));
    dispatch((0, _ast.setOutOfScopeLocations)()); // If a new source is selected update the file search results

    const newSource = (0, _selectors.getSelectedSource)(getState());

    if (currentSource && currentSource !== newSource) {
      dispatch((0, _ui.updateActiveFileSearch)());
    }
  };
}
/**
 * @memberof actions/sources
 * @static
 */


function selectSpecificLocation(location) {
  return async ({
    dispatch,
    getState,
    client
  }) => {
    const currentSource = (0, _selectors.getSelectedSource)(getState());

    if (!client) {
      // No connection, do nothing. This happens when the debugger is
      // shut down too fast and it tries to display a default source.
      return;
    }

    const source = (0, _selectors.getSource)(getState(), location.sourceId);

    if (!source) {
      // If there is no source we deselect the current selected source
      return dispatch(clearSelectedLocation());
    }

    const activeSearch = (0, _selectors.getActiveSearch)(getState());

    if (activeSearch !== "file") {
      dispatch((0, _ui.closeActiveSearch)());
    }

    dispatch((0, _tabs.addTab)(source.url, 0));
    dispatch(setSelectedLocation(source, location));
    await dispatch((0, _loadSourceText.loadSourceText)(source));
    const loadedSource = (0, _selectors.getSource)(getState(), source.id);

    if (!loadedSource) {
      return;
    }

    const sourceId = loadedSource.id;
    dispatch((0, _ast.setSymbols)(sourceId));
    dispatch((0, _ast.setOutOfScopeLocations)()); // If a new source is selected update the file search results

    const newSource = (0, _selectors.getSelectedSource)(getState());

    if (currentSource && currentSource !== newSource) {
      dispatch((0, _ui.updateActiveFileSearch)());
    }
  };
}
/**
 * @memberof actions/sources
 * @static
 */


function selectSpecificSource(sourceId) {
  return async ({
    dispatch
  }) => {
    const location = (0, _location.createLocation)({
      sourceId
    });
    return await dispatch(selectSpecificLocation(location));
  };
}
/**
 * @memberof actions/sources
 * @static
 */


function jumpToMappedLocation(location) {
  return async function ({
    dispatch,
    getState,
    client,
    sourceMaps
  }) {
    if (!client) {
      return;
    }

    const source = (0, _selectors.getSource)(getState(), location.sourceId);
    let pairedLocation;

    if ((0, _devtoolsSourceMap.isOriginalId)(location.sourceId)) {
      pairedLocation = await (0, _sourceMaps.getGeneratedLocation)(getState(), source, location, sourceMaps);
    } else {
      pairedLocation = await sourceMaps.getOriginalLocation(location, source);
    }

    return dispatch(selectLocation({ ...pairedLocation
    }));
  };
}

function jumpToMappedSelectedLocation() {
  return async function ({
    dispatch,
    getState
  }) {
    const location = (0, _selectors.getSelectedLocation)(getState());

    if (!location) {
      return;
    }

    await dispatch(jumpToMappedLocation(location));
  };
}