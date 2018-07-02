"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.nodeHasChildren = nodeHasChildren;
exports.isExactUrlMatch = isExactUrlMatch;
exports.isPathDirectory = isPathDirectory;
exports.isDirectory = isDirectory;
exports.getSourceFromNode = getSourceFromNode;
exports.isSource = isSource;
exports.getFileExtension = getFileExtension;
exports.isNotJavaScript = isNotJavaScript;
exports.isInvalidUrl = isInvalidUrl;
exports.partIsFile = partIsFile;
exports.createDirectoryNode = createDirectoryNode;
exports.createSourceNode = createSourceNode;
exports.createParentMap = createParentMap;
exports.getRelativePath = getRelativePath;

var _url = require("devtools/client/debugger/new/dist/vendors").vendored["url"];

var _source = require("../source");

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
const IGNORED_URLS = ["debugger eval code", "XStringBundle"];

function nodeHasChildren(item) {
  return Array.isArray(item.contents) && item.type === "directory";
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

function isPathDirectory(path) {
  // Assume that all urls point to files except when they end with '/'
  // Or directory node has children
  const parts = path.split("/").filter(p => p !== "");
  return parts.length === 0 || path.slice(-1) === "/";
}

function isDirectory(item) {
  return (isPathDirectory(item.path) || item.type === "directory") && item.name != "(index)";
}

function getSourceFromNode(item) {
  const {
    contents
  } = item;

  if (!isDirectory(item) && !Array.isArray(contents)) {
    return contents;
  }
}

function isSource(item) {
  return item.type === "source";
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
  return IGNORED_URLS.indexOf(url) != -1 || !source.url || !url.group || (0, _source.isPretty)(source) || isNotJavaScript(source);
}

function partIsFile(index, parts, url) {
  const isLastPart = index === parts.length - 1;
  return !isDirectory(url) && isLastPart;
}

function createDirectoryNode(name, path, contents) {
  return {
    type: "directory",
    name,
    path,
    contents
  };
}

function createSourceNode(name, path, contents) {
  return {
    type: "source",
    name,
    path,
    contents
  };
}

function createParentMap(tree) {
  const map = new WeakMap();

  function _traverse(subtree) {
    if (subtree.type === "directory") {
      for (const child of subtree.contents) {
        map.set(child, subtree);

        _traverse(child);
      }
    }
  }

  if (tree.type === "directory") {
    // Don't link each top-level path to the "root" node because the
    // user never sees the root
    tree.contents.forEach(_traverse);
  }

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