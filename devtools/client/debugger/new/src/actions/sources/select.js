"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
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

function _objectSpread(target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i] != null ? arguments[i] : {}; var ownKeys = Object.keys(source); if (typeof Object.getOwnPropertySymbols === 'function') { ownKeys = ownKeys.concat(Object.getOwnPropertySymbols(source).filter(function (sym) { return Object.getOwnPropertyDescriptor(source, sym).enumerable; })); } ownKeys.forEach(function (key) { _defineProperty(target, key, source[key]); }); } return target; }

function _defineProperty(obj, key, value) { if (key in obj) { Object.defineProperty(obj, key, { value: value, enumerable: true, configurable: true, writable: true }); } else { obj[key] = value; } return obj; }

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
      const location = (0, _location.createLocation)(_objectSpread({}, options.location, {
        sourceId
      }));
      await dispatch(selectLocation(location));
    } else {
      dispatch({
        type: "SELECT_SOURCE_URL",
        url: url,
        line: options.location ? options.location.line : null
      });
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
    if (!client) {
      // No connection, do nothing. This happens when the debugger is
      // shut down too fast and it tries to display a default source.
      return;
    }

    const sourceRecord = (0, _selectors.getSource)(getState(), location.sourceId);

    if (!sourceRecord) {
      // If there is no source we deselect the current selected source
      return dispatch({
        type: "CLEAR_SELECTED_SOURCE"
      });
    }

    const activeSearch = (0, _selectors.getActiveSearch)(getState());

    if (activeSearch !== "file") {
      dispatch((0, _ui.closeActiveSearch)());
    }

    const source = sourceRecord.toJS();
    dispatch((0, _tabs.addTab)(source.url, 0));
    dispatch({
      type: "SELECT_SOURCE",
      source,
      location
    });
    await dispatch((0, _loadSourceText.loadSourceText)(sourceRecord));
    const selectedSource = (0, _selectors.getSelectedSource)(getState());

    if (!selectedSource) {
      return;
    }

    const sourceId = selectedSource.id;

    if (_prefs.prefs.autoPrettyPrint && !(0, _selectors.getPrettySource)(getState(), sourceId) && (0, _source.shouldPrettyPrint)(selectedSource) && (0, _source.isMinified)(selectedSource)) {
      await dispatch((0, _prettyPrint.togglePrettyPrint)(sourceId));
      dispatch((0, _tabs.closeTab)(source.url));
    }

    dispatch((0, _ast.setSymbols)(sourceId));
    dispatch((0, _ast.setOutOfScopeLocations)());
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
    if (!client) {
      // No connection, do nothing. This happens when the debugger is
      // shut down too fast and it tries to display a default source.
      return;
    }

    const sourceRecord = (0, _selectors.getSource)(getState(), location.sourceId);

    if (!sourceRecord) {
      // If there is no source we deselect the current selected source
      return dispatch({
        type: "CLEAR_SELECTED_SOURCE"
      });
    }

    const activeSearch = (0, _selectors.getActiveSearch)(getState());

    if (activeSearch !== "file") {
      dispatch((0, _ui.closeActiveSearch)());
    }

    const source = sourceRecord.toJS();
    dispatch((0, _tabs.addTab)(source, 0));
    dispatch({
      type: "SELECT_SOURCE",
      source,
      location
    });
    await dispatch((0, _loadSourceText.loadSourceText)(sourceRecord));
    const selectedSource = (0, _selectors.getSelectedSource)(getState());

    if (!selectedSource) {
      return;
    }

    const sourceId = selectedSource.id;
    dispatch((0, _ast.setSymbols)(sourceId));
    dispatch((0, _ast.setOutOfScopeLocations)());
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
      pairedLocation = await sourceMaps.getOriginalLocation(location, source.toJS());
    }

    return dispatch(selectLocation(_objectSpread({}, pairedLocation)));
  };
}

function jumpToMappedSelectedLocation() {
  return async function ({
    dispatch,
    getState
  }) {
    const location = (0, _selectors.getSelectedLocation)(getState());
    await dispatch(jumpToMappedLocation(location));
  };
}