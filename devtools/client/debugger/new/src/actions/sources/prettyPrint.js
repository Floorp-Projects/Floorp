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

function _objectSpread(target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i] != null ? arguments[i] : {}; var ownKeys = Object.keys(source); if (typeof Object.getOwnPropertySymbols === 'function') { ownKeys = ownKeys.concat(Object.getOwnPropertySymbols(source).filter(function (sym) { return Object.getOwnPropertyDescriptor(source, sym).enumerable; })); } ownKeys.forEach(function (key) { _defineProperty(target, key, source[key]); }); } return target; }

function _defineProperty(obj, key, value) { if (key in obj) { Object.defineProperty(obj, key, { value: value, enumerable: true, configurable: true, writable: true }); } else { obj[key] = value; } return obj; }

function createPrettySource(sourceId) {
  return async ({
    dispatch,
    getState,
    sourceMaps
  }) => {
    const source = (0, _selectors.getSource)(getState(), sourceId);
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

    const loadedPrettySource = _objectSpread({}, prettySource, {
      text: code,
      loadedState: "loaded"
    });

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
      const _sourceId = prettySource.get("id");

      return dispatch((0, _sources.selectLocation)(_objectSpread({}, options.location, {
        sourceId: _sourceId
      })));
    }

    const newPrettySource = await dispatch(createPrettySource(sourceId));
    await dispatch((0, _breakpoints.remapBreakpoints)(sourceId));
    await dispatch((0, _pause.mapFrames)());
    await dispatch((0, _ast.setPausePoints)(newPrettySource.id));
    await dispatch((0, _ast.setSymbols)(newPrettySource.id));
    return dispatch((0, _sources.selectLocation)(_objectSpread({}, options.location, {
      sourceId: newPrettySource.id
    })));
  };
}