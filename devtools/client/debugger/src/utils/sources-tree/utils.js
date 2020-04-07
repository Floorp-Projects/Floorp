/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
import { parse } from "../../utils/url";

import type { TreeNode, TreeSource, TreeDirectory, ParentMap } from "./types";
import type { Source, Thread, URL } from "../../types";
import type { SourcesMapByThread } from "../../reducers/types";
import { isPretty } from "../source";
import { getURL } from "./getURL";
const IGNORED_URLS = ["debugger eval code", "XStringBundle"];

export function nodeHasChildren(item: TreeNode): boolean {
  return item.type == "directory" && Array.isArray(item.contents);
}

export function isExactUrlMatch(pathPart: string, debuggeeUrl: URL): boolean {
  // compare to hostname with an optional 'www.' prefix
  const { host } = parse(debuggeeUrl);
  if (!host) {
    return false;
  }
  return (
    host === pathPart ||
    host.replace(/^www\./, "") === pathPart.replace(/^www\./, "")
  );
}

export function isPathDirectory(path: string): boolean {
  // Assume that all urls point to files except when they end with '/'
  // Or directory node has children

  if (path.endsWith("/")) {
    return true;
  }

  let separators = 0;
  for (let i = 0; i < path.length - 1; ++i) {
    if (path[i] === "/") {
      if (path[i + i] !== "/") {
        return false;
      }

      ++separators;
    }
  }

  switch (separators) {
    case 0: {
      return false;
    }
    case 1: {
      return !path.startsWith("/");
    }
    default: {
      return true;
    }
  }
}

export function isDirectory(item: TreeNode): boolean {
  return (
    (item.type === "directory" || isPathDirectory(item.path)) &&
    item.name != "(index)"
  );
}

export function getSourceFromNode(item: TreeNode): ?Source {
  const { contents } = item;
  if (!isDirectory(item) && !Array.isArray(contents)) {
    return contents;
  }
}

export function isSource(item: TreeNode): boolean {
  return item.type === "source";
}

export function getFileExtension(source: Source): string {
  const { path } = getURL(source);
  if (!path) {
    return "";
  }

  const lastIndex = path.lastIndexOf(".");
  return lastIndex !== -1 ? path.slice(lastIndex + 1) : "";
}

export function isNotJavaScript(source: Source): boolean {
  return ["css", "svg", "png"].includes(getFileExtension(source));
}

export function isInvalidUrl(url: Object, source: Source): boolean {
  return (
    !source.url ||
    !url.group ||
    isNotJavaScript(source) ||
    IGNORED_URLS.includes(url) ||
    isPretty(source)
  );
}

export function partIsFile(
  index: number,
  parts: Array<string>,
  url: Object
): boolean {
  const isLastPart = index === parts.length - 1;
  return isLastPart && !isDirectory(url);
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

export function getRelativePath(url: URL): string {
  const { pathname } = parse(url);
  if (!pathname) {
    return url;
  }
  const index = pathname.indexOf("/");

  return index !== -1 ? pathname.slice(index + 1) : "";
}

export function getPathWithoutThread(path: string): string {
  const pathParts = path.split(/(context\d+?\/)/).splice(2);
  if (pathParts && pathParts.length > 0) {
    return pathParts.join("");
  }
  return "";
}

export function findSource(
  { threads, sources }: { threads: Thread[], sources: SourcesMapByThread },
  itemPath: string,
  source: ?Source
): ?Source {
  const targetThread = threads.find(thread => itemPath.includes(thread.actor));
  if (targetThread && source) {
    const { actor } = targetThread;
    if (sources[actor]) {
      return sources[actor][source.id];
    }
  }
  return source;
}

// NOTE: we get the source from sources because item.contents is cached
export function getSource(
  item: TreeNode,
  { threads, sources }: { threads: Thread[], sources: SourcesMapByThread }
): ?Source {
  const source = getSourceFromNode(item);
  return findSource({ threads, sources }, item.path, source);
}

export function getChildren(item: $Shape<TreeDirectory>) {
  return nodeHasChildren(item) ? item.contents : [];
}

export function getAllSources({
  threads,
  sources,
}: {
  threads: Thread[],
  sources: SourcesMapByThread,
}): Source[] {
  const sourcesAll = [];
  threads.forEach(thread => {
    const { actor } = thread;

    for (const source in sources[actor]) {
      sourcesAll.push(sources[actor][source]);
    }
  });
  return sourcesAll;
}

export function getSourcesInsideGroup(
  item: TreeNode,
  { threads, sources }: { threads: Thread[], sources: SourcesMapByThread }
): Source[] {
  const sourcesInsideDirectory = [];

  const findAllSourcesInsideDirectory = (directoryToSearch: TreeDirectory) => {
    const childrenItems = getChildren(directoryToSearch);

    childrenItems.forEach((itemChild: TreeNode) => {
      if (itemChild.type === "directory") {
        findAllSourcesInsideDirectory(itemChild);
      } else {
        const source = getSource(itemChild, { threads, sources });
        if (source) {
          sourcesInsideDirectory.push(source);
        }
      }
    });
  };
  if (item.type === "directory") {
    findAllSourcesInsideDirectory(item);
  }
  return sourcesInsideDirectory;
}
