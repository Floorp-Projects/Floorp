"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.stop = exports.start = undefined;
exports.prettyPrint = prettyPrint;

var _devtoolsUtils = require("devtools/client/debugger/new/dist/vendors").vendored["devtools-utils"];

var _source = require("../../utils/source");

var _assert = require("../../utils/assert");

var _assert2 = _interopRequireDefault(_assert);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
const {
  WorkerDispatcher
} = _devtoolsUtils.workerUtils;
const dispatcher = new WorkerDispatcher();
const start = exports.start = dispatcher.start.bind(dispatcher);
const stop = exports.stop = dispatcher.stop.bind(dispatcher);

const _prettyPrint = dispatcher.task("prettyPrint");

async function prettyPrint({
  source,
  url
}) {
  const indent = 2;
  (0, _assert2.default)((0, _source.isJavaScript)(source), "Can't prettify non-javascript files.");
  return await _prettyPrint({
    url,
    indent,
    sourceText: source.text
  });
}