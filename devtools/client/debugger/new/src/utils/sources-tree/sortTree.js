"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.sortEntireTree = sortEntireTree;
exports.sortTree = sortTree;

var _utils = require("./utils");

function _objectSpread(target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i] != null ? arguments[i] : {}; var ownKeys = Object.keys(source); if (typeof Object.getOwnPropertySymbols === 'function') { ownKeys = ownKeys.concat(Object.getOwnPropertySymbols(source).filter(function (sym) { return Object.getOwnPropertyDescriptor(source, sym).enumerable; })); } ownKeys.forEach(function (key) { _defineProperty(target, key, source[key]); }); } return target; }

function _defineProperty(obj, key, value) { if (key in obj) { Object.defineProperty(obj, key, { value: value, enumerable: true, configurable: true, writable: true }); } else { obj[key] = value; } return obj; }

/**
 * Look at the nodes in the source tree, and determine the index of where to
 * insert a new node. The ordering is index -> folder -> file.
 * @memberof utils/sources-tree
 * @static
 */
function sortEntireTree(tree, debuggeeUrl = "") {
  if ((0, _utils.nodeHasChildren)(tree)) {
    const contents = sortTree(tree, debuggeeUrl).map(subtree => sortEntireTree(subtree));
    return _objectSpread({}, tree, {
      contents
    });
  }

  return tree;
}
/**
 * Look at the nodes in the source tree, and determine the index of where to
 * insert a new node. The ordering is index -> folder -> file.
 * @memberof utils/sources-tree
 * @static
 */


function sortTree(tree, debuggeeUrl = "") {
  return tree.contents.sort((previousNode, currentNode) => {
    const currentNodeIsDir = (0, _utils.nodeHasChildren)(currentNode);
    const previousNodeIsDir = (0, _utils.nodeHasChildren)(previousNode);

    if (currentNode.name === "(index)") {
      return 1;
    } else if (previousNode.name === "(index)") {
      return -1;
    } else if ((0, _utils.isExactUrlMatch)(currentNode.name, debuggeeUrl)) {
      return 1;
    } else if ((0, _utils.isExactUrlMatch)(previousNode.name, debuggeeUrl)) {
      return -1; // If neither is the case, continue to compare alphabetically
    } else if (previousNodeIsDir && !currentNodeIsDir) {
      return -1;
    } else if (!previousNodeIsDir && currentNodeIsDir) {
      return 1;
    }

    return previousNode.name.localeCompare(currentNode.name);
  });
}