"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.getRelativeSources = undefined;

var _selectors = require("../selectors/index");

var _lodash = require("devtools/client/shared/vendor/lodash");

var _source = require("../utils/source");

var _reselect = require("devtools/client/debugger/new/dist/vendors").vendored["reselect"];

function _objectSpread(target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i] != null ? arguments[i] : {}; var ownKeys = Object.keys(source); if (typeof Object.getOwnPropertySymbols === 'function') { ownKeys = ownKeys.concat(Object.getOwnPropertySymbols(source).filter(function (sym) { return Object.getOwnPropertyDescriptor(source, sym).enumerable; })); } ownKeys.forEach(function (key) { _defineProperty(target, key, source[key]); }); } return target; }

function _defineProperty(obj, key, value) { if (key in obj) { Object.defineProperty(obj, key, { value: value, enumerable: true, configurable: true, writable: true }); } else { obj[key] = value; } return obj; }

function getRelativeUrl(url, root) {
  if (!root) {
    return (0, _source.getSourcePath)(url);
  } // + 1 removes the leading "/"


  return url.slice(url.indexOf(root) + root.length + 1);
}

function formatSource(source, root) {
  return _objectSpread({}, source, {
    relativeUrl: getRelativeUrl(source.url, root)
  });
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