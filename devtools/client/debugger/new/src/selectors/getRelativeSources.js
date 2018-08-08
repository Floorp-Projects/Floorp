"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.getRelativeSources = undefined;

var _selectors = require("../selectors/index");

var _lodash = require("devtools/client/shared/vendor/lodash");

var _sourcesTree = require("../utils/sources-tree/index");

var _reselect = require("devtools/client/debugger/new/dist/vendors").vendored["reselect"];

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function getRelativeUrl(source, root) {
  const {
    group,
    path
  } = (0, _sourcesTree.getURL)(source);

  if (!root) {
    return path;
  } // + 1 removes the leading "/"


  const url = group + path;
  return url.slice(url.indexOf(root) + root.length + 1);
}

function formatSource(source, root) {
  return { ...source,
    relativeUrl: getRelativeUrl(source, root)
  };
}

function underRoot(source, root) {
  return source.url && source.url.includes(root);
}
/*
 * Gets the sources that are below a project root
 */


const getRelativeSources = exports.getRelativeSources = (0, _reselect.createSelector)(_selectors.getSources, _selectors.getProjectDirectoryRoot, (sources, root) => {
  const relativeSources = (0, _lodash.chain)(sources).pickBy(source => underRoot(source, root)).mapValues(source => formatSource(source, root)).value();
  return relativeSources;
});