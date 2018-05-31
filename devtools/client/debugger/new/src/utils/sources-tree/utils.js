"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.nodeHasChildren = nodeHasChildren;
exports.isExactUrlMatch = isExactUrlMatch;
exports.isDirectory = isDirectory;
exports.getFileExtension = getFileExtension;
exports.isNotJavaScript = isNotJavaScript;
exports.isInvalidUrl = isInvalidUrl;
exports.partIsFile = partIsFile;
exports.createNode = createNode;
exports.createParentMap = createParentMap;
exports.getRelativePath = getRelativePath;

var _url = require("devtools/client/debugger/new/dist/vendors").vendored["url"];

var _source = require("../source");

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
const IGNORED_URLS = ["debugger eval code", "XStringBundle"];

function nodeHasChildren(item) {
  return Array.isArray(item.contents);
}

function isExactUrlMatch(pathPart, debuggeeUrl) {
  // compare to hostname with an optional 'www.' prefix
  const {
    host
  } = (0, _url.parse)(debuggeeUrl);

  if (!host) {
    return false;
  }

  return host.replace(/^www\./, "") === pathPart.replace(/^www\./, "");
}

function isDirectory(url) {
  const parts = url.path.split("/").filter(p => p !== ""); // Assume that all urls point to files except when they end with '/'
  // Or directory node has children

  return (parts.length === 0 || url.path.slice(-1) === "/" || nodeHasChildren(url)) && url.name != "(index)";
}

function getFileExtension(url = "") {
  const parsedUrl = (0, _url.parse)(url).pathname;

  if (!parsedUrl) {
    return "";
  }

  return parsedUrl.split(".").pop();
}

function isNotJavaScript(source) {
  return ["css", "svg", "png"].includes(getFileExtension(source.url));
}

function isInvalidUrl(url, source) {
  return IGNORED_URLS.indexOf(url) != -1 || !(source.get ? source.get("url") : source.url) || !url.group || (0, _source.isPretty)(source) || isNotJavaScript(source);
}

function partIsFile(index, parts, url) {
  const isLastPart = index === parts.length - 1;
  return !isDirectory(url) && isLastPart;
}

function createNode(name, path, contents) {
  return {
    name,
    path,
    contents
  };
}

function createParentMap(tree) {
  const map = new WeakMap();

  function _traverse(subtree) {
    if (nodeHasChildren(subtree)) {
      for (const child of subtree.contents) {
        map.set(child, subtree);

        _traverse(child);
      }
    }
  } // Don't link each top-level path to the "root" node because the
  // user never sees the root


  tree.contents.forEach(_traverse);
  return map;
}

function getRelativePath(url) {
  const {
    pathname
  } = (0, _url.parse)(url);

  if (!pathname) {
    return url;
  }

  const path = pathname.split("/");
  path.shift();
  return path.join("/");
}