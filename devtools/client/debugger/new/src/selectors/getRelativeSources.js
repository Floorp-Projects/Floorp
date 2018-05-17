"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.getRelativeSources = undefined;

var _selectors = require("../selectors/index");

var _sources = require("../reducers/sources");

var _source = require("../utils/source");

var _reselect = require("devtools/client/debugger/new/dist/vendors").vendored["reselect"];

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function getRelativeUrl(url, root) {
  if (!root) {
    return (0, _source.getSourcePath)(url);
  } // + 1 removes the leading "/"


  return url.slice(url.indexOf(root) + root.length + 1);
}

function formatSource(source, root) {
  return new _sources.RelativeSourceRecordClass(source).set("relativeUrl", getRelativeUrl(source.url, root));
}
/*
 * Gets the sources that are below a project root
 */


const getRelativeSources = exports.getRelativeSources = (0, _reselect.createSelector)(_selectors.getSources, _selectors.getProjectDirectoryRoot, (sources, root) => {
  return sources.valueSeq().filter(source => source.url && source.url.includes(root)).map(source => formatSource(source, root));
});