"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.willNavigate = willNavigate;
exports.navigate = navigate;
exports.connect = connect;
exports.navigated = navigated;

var _editor = require("../utils/editor/index");

var _sourceQueue = require("../utils/source-queue");

var _sourceQueue2 = _interopRequireDefault(_sourceQueue);

var _sources = require("../reducers/sources");

var _utils = require("../utils/utils");

var _sources2 = require("./sources/index");

var _debuggee = require("./debuggee");

var _parser = require("../workers/parser/index");

var _wasm = require("../utils/wasm");

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Redux actions for the navigation state
 * @module actions/navigation
 */

/**
 * @memberof actions/navigation
 * @static
 */
function willNavigate(event) {
  return async function ({
    dispatch,
    getState,
    client,
    sourceMaps
  }) {
    await sourceMaps.clearSourceMaps();
    (0, _wasm.clearWasmStates)();
    (0, _editor.clearDocuments)();
    (0, _parser.clearSymbols)();
    (0, _parser.clearASTs)();
    (0, _parser.clearScopes)();
    (0, _parser.clearSources)();
    dispatch(navigate(event.url));
  };
}

function navigate(url) {
  _sourceQueue2.default.clear();

  return {
    type: "NAVIGATE",
    url
  };
}

function connect(url, canRewind) {
  return async function ({
    dispatch
  }) {
    await dispatch((0, _debuggee.updateWorkers)());
    dispatch({
      type: "CONNECT",
      url,
      canRewind
    });
  };
}
/**
 * @memberof actions/navigation
 * @static
 */


function navigated() {
  return async function ({
    dispatch,
    getState,
    client
  }) {
    await (0, _utils.waitForMs)(100);

    if ((0, _sources.getSources)(getState()).size == 0) {
      const sources = await client.fetchSources();
      dispatch((0, _sources2.newSources)(sources));
    }
  };
}