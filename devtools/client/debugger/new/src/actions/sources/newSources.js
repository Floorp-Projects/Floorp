"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.newSource = newSource;
exports.newSources = newSources;

var _devtoolsSourceMap = require("devtools/client/shared/source-map/index.js");

var _lodash = require("devtools/client/shared/vendor/lodash");

var _blackbox = require("./blackbox");

var _breakpoints = require("../breakpoints");

var _loadSourceText = require("./loadSourceText");

var _prettyPrint = require("./prettyPrint");

var _sources = require("../sources/index");

var _source = require("../../utils/source");

var _selectors = require("../../selectors/index");

function _objectSpread(target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i] != null ? arguments[i] : {}; var ownKeys = Object.keys(source); if (typeof Object.getOwnPropertySymbols === 'function') { ownKeys = ownKeys.concat(Object.getOwnPropertySymbols(source).filter(function (sym) { return Object.getOwnPropertyDescriptor(source, sym).enumerable; })); } ownKeys.forEach(function (key) { _defineProperty(target, key, source[key]); }); } return target; }

function _defineProperty(obj, key, value) { if (key in obj) { Object.defineProperty(obj, key, { value: value, enumerable: true, configurable: true, writable: true }); } else { obj[key] = value; } return obj; }

function createOriginalSource(originalUrl, generatedSource, sourceMaps) {
  return {
    url: originalUrl,
    id: sourceMaps.generatedToOriginalId(generatedSource.id, originalUrl),
    isPrettyPrinted: false,
    isWasm: false,
    isBlackBoxed: false,
    loadedState: "unloaded"
  };
}

function loadSourceMaps(sources) {
  return async function ({
    dispatch,
    sourceMaps
  }) {
    if (!sourceMaps) {
      return;
    }

    const originalSources = await Promise.all(sources.map(source => dispatch(loadSourceMap(source.id))));
    await dispatch(newSources((0, _lodash.flatten)(originalSources)));
  };
}
/**
 * @memberof actions/sources
 * @static
 */


function loadSourceMap(sourceId) {
  return async function ({
    dispatch,
    getState,
    sourceMaps
  }) {
    const source = (0, _selectors.getSource)(getState(), sourceId).toJS();

    if (!sourceMaps || !(0, _devtoolsSourceMap.isGeneratedId)(sourceId) || !source.sourceMapURL) {
      return;
    }

    let urls = null;

    try {
      urls = await sourceMaps.getOriginalURLs(source);
    } catch (e) {
      console.error(e);
    }

    if (!urls) {
      // If this source doesn't have a sourcemap, enable it for pretty printing
      dispatch({
        type: "UPDATE_SOURCE",
        source: _objectSpread({}, source, {
          sourceMapURL: ""
        })
      });
      return;
    }

    return urls.map(url => createOriginalSource(url, source, sourceMaps));
  };
} // If a request has been made to show this source, go ahead and
// select it.


function checkSelectedSource(sourceId) {
  return async ({
    dispatch,
    getState
  }) => {
    const source = (0, _selectors.getSource)(getState(), sourceId);
    const pendingLocation = (0, _selectors.getPendingSelectedLocation)(getState());

    if (!pendingLocation || !pendingLocation.url || !source.url) {
      return;
    }

    const pendingUrl = pendingLocation.url;
    const rawPendingUrl = (0, _source.getRawSourceURL)(pendingUrl);

    if (rawPendingUrl === source.url) {
      if ((0, _source.isPrettyURL)(pendingUrl)) {
        return await dispatch((0, _prettyPrint.togglePrettyPrint)(source.id));
      }

      await dispatch((0, _sources.selectLocation)(_objectSpread({}, pendingLocation, {
        sourceId: source.id
      })));
    }
  };
}

function checkPendingBreakpoints(sourceId) {
  return async ({
    dispatch,
    getState
  }) => {
    // source may have been modified by selectLocation
    const source = (0, _selectors.getSource)(getState(), sourceId);
    const pendingBreakpoints = (0, _selectors.getPendingBreakpointsForSource)(getState(), source.get("url"));

    if (!pendingBreakpoints.size) {
      return;
    } // load the source text if there is a pending breakpoint for it


    await dispatch((0, _loadSourceText.loadSourceText)(source));
    const pendingBreakpointsArray = pendingBreakpoints.valueSeq().toJS();

    for (const pendingBreakpoint of pendingBreakpointsArray) {
      await dispatch((0, _breakpoints.syncBreakpoint)(sourceId, pendingBreakpoint));
    }
  };
}

function restoreBlackBoxedSources(sources) {
  return async ({
    dispatch
  }) => {
    const tabs = (0, _selectors.getBlackBoxList)();

    if (tabs.length == 0) {
      return;
    }

    for (const source of sources) {
      if (tabs.includes(source.url) && !source.isBlackBoxed) {
        dispatch((0, _blackbox.toggleBlackBox)(source));
      }
    }
  };
}
/**
 * Handler for the debugger client's unsolicited newSource notification.
 * @memberof actions/sources
 * @static
 */


function newSource(source) {
  return async ({
    dispatch
  }) => {
    await dispatch(newSources([source]));
  };
}

function newSources(sources) {
  return async ({
    dispatch,
    getState
  }) => {
    const filteredSources = sources.filter(source => source && !(0, _selectors.getSource)(getState(), source.id));

    if (filteredSources.length == 0) {
      return;
    }

    dispatch({
      type: "ADD_SOURCES",
      sources: filteredSources
    });

    for (const source of filteredSources) {
      dispatch(checkSelectedSource(source.id));
      dispatch(checkPendingBreakpoints(source.id));
    }

    await dispatch(loadSourceMaps(filteredSources)); // We would like to restore the blackboxed state
    // after loading all states to make sure the correctness.

    await dispatch(restoreBlackBoxedSources(filteredSources));
  };
}