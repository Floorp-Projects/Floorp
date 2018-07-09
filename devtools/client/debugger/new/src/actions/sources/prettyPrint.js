"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.createPrettySource = createPrettySource;
exports.togglePrettyPrint = togglePrettyPrint;

var _assert = require("../../utils/assert");

var _assert2 = _interopRequireDefault(_assert);

var _breakpoints = require("../breakpoints");

var _ast = require("../ast");

var _prettyPrint = require("../../workers/pretty-print/index");

var _parser = require("../../workers/parser/index");

var _source = require("../../utils/source");

var _loadSourceText = require("./loadSourceText");

var _sources = require("../sources/index");

var _pause = require("../pause/index");

var _selectors = require("../../selectors/index");

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function createPrettySource(sourceId) {
  return async ({
    dispatch,
    getState,
    sourceMaps
  }) => {
    const source = (0, _selectors.getSourceFromId)(getState(), sourceId);
    const url = (0, _source.getPrettySourceURL)(source.url);
    const id = await sourceMaps.generatedToOriginalId(sourceId, url);
    const prettySource = {
      url,
      id,
      isBlackBoxed: false,
      isPrettyPrinted: true,
      isWasm: false,
      contentType: "text/javascript",
      loadedState: "loading"
    };
    dispatch({
      type: "ADD_SOURCE",
      source: prettySource
    });
    const {
      code,
      mappings
    } = await (0, _prettyPrint.prettyPrint)({
      source,
      url
    });
    await sourceMaps.applySourceMap(source.id, url, code, mappings);
    const loadedPrettySource = { ...prettySource,
      text: code,
      loadedState: "loaded"
    };
    (0, _parser.setSource)(loadedPrettySource);
    dispatch({
      type: "UPDATE_SOURCE",
      source: loadedPrettySource
    });
    return prettySource;
  };
}
/**
 * Toggle the pretty printing of a source's text. All subsequent calls to
 * |getText| will return the pretty-toggled text. Nothing will happen for
 * non-javascript files.
 *
 * @memberof actions/sources
 * @static
 * @param string id The source form from the RDP.
 * @returns Promise
 *          A promise that resolves to [aSource, prettyText] or rejects to
 *          [aSource, error].
 */


function togglePrettyPrint(sourceId) {
  return async ({
    dispatch,
    getState,
    client,
    sourceMaps
  }) => {
    const source = (0, _selectors.getSource)(getState(), sourceId);

    if (!source) {
      return {};
    }

    if (!(0, _source.isLoaded)(source)) {
      await dispatch((0, _loadSourceText.loadSourceText)(source));
    }

    (0, _assert2.default)(sourceMaps.isGeneratedId(sourceId), "Pretty-printing only allowed on generated sources");
    const selectedLocation = (0, _selectors.getSelectedLocation)(getState());
    const url = (0, _source.getPrettySourceURL)(source.url);
    const prettySource = (0, _selectors.getSourceByURL)(getState(), url);
    const options = {};

    if (selectedLocation) {
      options.location = await sourceMaps.getOriginalLocation(selectedLocation);
    }

    if (prettySource) {
      const _sourceId = prettySource.id;
      return dispatch((0, _sources.selectLocation)({ ...options.location,
        sourceId: _sourceId
      }));
    }

    const newPrettySource = await dispatch(createPrettySource(sourceId));
    await dispatch((0, _breakpoints.remapBreakpoints)(sourceId));
    await dispatch((0, _pause.mapFrames)());
    await dispatch((0, _ast.setPausePoints)(newPrettySource.id));
    await dispatch((0, _ast.setSymbols)(newPrettySource.id));
    return dispatch((0, _sources.selectLocation)({ ...options.location,
      sourceId: newPrettySource.id
    }));
  };
}