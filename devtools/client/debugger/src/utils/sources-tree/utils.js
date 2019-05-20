/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { parse } from "../../utils/url";

import type { TreeNode, TreeSource, TreeDirectory, ParentMap } from "./types";
import type { Source } from "../../types";
import { isPretty } from "../source";
import { getURL } from "./getURL";
const IGNORED_URLS = ["debugger eval code", "XStringBundle"];

export function nodeHasChildren(item: TreeNode): boolean {
  return Array.isArray(item.contents) && item.type == "directory";
}

export function isExactUrlMatch(pathPart: string, debuggeeUrl: string) {
  // compare to hostname with an optional 'www.' prefix
  const { host } = parse(debuggeeUrl);
  if (!host) {
    return false;
  }
  return host.replace(/^www\./, "") === pathPart.replace(/^www\./, "");
}

export function isPathDirectory(path: string) {
  // Assume that all urls point to files except when they end with '/'
  // Or directory node has children
  const parts = path.split("/").filter(p => p !== "");
  return parts.length === 0 || path.slice(-1) === "/";
}

export function isDirectory(item: TreeNode) {
  return (
    (isPathDirectory(item.path) || item.type === "directory") &&
    item.name != "(index)"
  );
}

export function getSourceFromNode(item: TreeNode): ?Source {
  const { contents } = item;
  if (!isDirectory(item) && !Array.isArray(contents)) {
    return contents;
  }
}

export function isSource(item: TreeNode) {
  return item.type === "source";
}

export function getFileExtension(source: Source): string {
  const parsedUrl = getURL(source).path;
  if (!parsedUrl) {
    return "";
  }
  return parsedUrl.split(".").pop();
}

export function isNotJavaScript(source: Source): boolean {
  return ["css", "svg", "png"].includes(getFileExtension(source));
}

export function isInvalidUrl(url: Object, source: Source) {
  return (
    IGNORED_URLS.includes(url) ||
    !source.url ||
    !url.group ||
    isPretty(source) ||
    isNotJavaScript(source)
  );
}

export function partIsFile(index: number, parts: Array<string>, url: Object) {
  const isLastPart = index === parts.length - 1;
  return !isDirectory(url) && isLastPart;
}

export function createDirectoryNode(
  name: string,
  path: string,
  contents: TreeNode[]
): TreeDirectory {
  return {
    type: "directory",
    name,
    path,
    contents,
  };
}

export function createSourceNode(
  name: string,
  path: string,
  contents: Source
): TreeSource {
  return {
    type: "source",
    name,
    path,
    contents,
  };
}

export function createParentMap(tree: TreeNode): ParentMap {
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

export function getRelativePath(url: string) {
  const { pathname } = parse(url);
  if (!pathname) {
    return url;
  }
  const path = pathname.split("/");
  path.shift();
  return path.join("/");
}
