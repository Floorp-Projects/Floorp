"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.loadSourceText = loadSourceText;

var _devtoolsSourceMap = require("devtools/client/shared/source-map/index.js");

var _promise = require("../utils/middleware/promise");

var _selectors = require("../../selectors/index");

var _parser = require("../../workers/parser/index");

var parser = _interopRequireWildcard(_parser);

var _source = require("../../utils/source");

var _devtoolsModules = require("devtools/client/debugger/new/dist/vendors").vendored["devtools-modules"];

var _defer = require("../../utils/defer");

var _defer2 = _interopRequireDefault(_defer);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) { var desc = Object.defineProperty && Object.getOwnPropertyDescriptor ? Object.getOwnPropertyDescriptor(obj, key) : {}; if (desc.get || desc.set) { Object.defineProperty(newObj, key, desc); } else { newObj[key] = obj[key]; } } } } newObj.default = obj; return newObj; } }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
const requests = new Map(); // Measures the time it takes for a source to load

const loadSourceHistogram = "DEVTOOLS_DEBUGGER_LOAD_SOURCE_MS";
const telemetry = new _devtoolsModules.Telemetry();

async function loadSource(source, {
  sourceMaps,
  client
}) {
  const id = source.id;

  if ((0, _devtoolsSourceMap.isOriginalId)(id)) {
    return sourceMaps.getOriginalSourceText(source);
  }

  const response = await client.sourceContents(id);
  telemetry.finish(loadSourceHistogram, source);
  return {
    id,
    text: response.source,
    contentType: response.contentType || "text/javascript"
  };
}
/**
 * @memberof actions/sources
 * @static
 */


function loadSourceText(source) {
  return async ({
    dispatch,
    getState,
    client,
    sourceMaps
  }) => {
    const id = source.id; // Fetch the source text only once.

    if (requests.has(id)) {
      return requests.get(id);
    }

    if ((0, _source.isLoaded)(source)) {
      return Promise.resolve();
    }

    const deferred = (0, _defer2.default)();
    requests.set(id, deferred.promise);
    telemetry.start(loadSourceHistogram, source);

    try {
      await dispatch({
        type: "LOAD_SOURCE_TEXT",
        sourceId: id,
        [_promise.PROMISE]: loadSource(source, {
          sourceMaps,
          client
        })
      });
    } catch (e) {
      deferred.resolve();
      requests.delete(id);
      return;
    }

    const newSource = (0, _selectors.getSourceFromId)(getState(), source.id);

    if ((0, _devtoolsSourceMap.isOriginalId)(newSource.id) && !newSource.isWasm) {
      const generatedSource = (0, _selectors.getGeneratedSource)(getState(), source);
      await dispatch(loadSourceText(generatedSource));
    }

    if (!newSource.isWasm) {
      await parser.setSource(newSource);
    } // signal that the action is finished


    deferred.resolve();
    requests.delete(id);
    return source;
  };
}