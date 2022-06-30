/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { parse } from "../../utils/url";

export function getPathParts(url, thread, mainThreadHost) {
  const parts = url.path.split("/");
  if (parts.length > 1 && parts[parts.length - 1] === "") {
    parts.pop();
    if (url.search) {
      parts.push(url.search);
    }
  } else {
    parts[parts.length - 1] += url.search;
  }

  parts[0] = url.group;
  if (thread) {
    parts.unshift(thread);
  }

  let path = "";
  return parts.map((part, index) => {
    if (index == 0 && thread) {
      path = thread;
    } else {
      path = `${path}/${part}`;
    }

    const mainThreadHostIfRoot = index === 1 ? mainThreadHost : null;

    return {
      part,
      path,
      mainThreadHostIfRoot,
    };
  });
}

export function nodeHasChildren(item) {
  return item.type == "directory" && Array.isArray(item.contents);
}

export function isPathDirectory(path) {
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

export function isDirectory(item) {
  return (
    (item.type === "directory" || isPathDirectory(item.path)) &&
    item.name != "(index)"
  );
}

export function getSourceFromNode(item) {
  const { contents } = item;
  if (!isDirectory(item) && !Array.isArray(contents)) {
    return contents;
  }
}

export function isSource(item) {
  return item.type === "source";
}

export function getFileExtension(source) {
  const { path } = source.displayURL;
  if (!path) {
    return "";
  }

  const lastIndex = path.lastIndexOf(".");
  return lastIndex !== -1 ? path.slice(lastIndex + 1) : "";
}

export function partIsFile(index, parts, url) {
  const isLastPart = index === parts.length - 1;
  return isLastPart && !isDirectory(url);
}

export function createDirectoryNode(name, path, contents) {
  return {
    type: "directory",
    name,
    path,
    contents,
  };
}

export function createSourceNode(name, path, contents) {
  return {
    type: "source",
    name,
    path,
    contents,
  };
}

export function createParentMap(tree) {
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

/**
 * Get the relative path of the url
 * Does not include any query parameters or fragment parts
 *
 * @param string url
 * @returns string path
 */
export function getRelativePath(url) {
  const { pathname } = parse(url);
  if (!pathname) {
    return url;
  }
  const index = pathname.indexOf("/");
  if (index !== -1) {
    const path = pathname.slice(index + 1);
    // If the path is empty this is likely the index file.
    // e.g http://foo.com/
    if (path == "") {
      return "(index)";
    }
    return path;
  }
  return "";
}

export function getPathWithoutThread(path) {
  const pathParts = path.split(/(context\d+?\/)/).splice(2);
  if (pathParts && pathParts.length > 0) {
    return pathParts.join("");
  }
  return "";
}

export function findSource({ threads, sources }, itemPath, source) {
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
export function getSource(item, { threads, sources }) {
  const source = getSourceFromNode(item);
  return findSource({ threads, sources }, item.path, source);
}

export function getChildren(item) {
  return nodeHasChildren(item) ? item.contents : [];
}

export function getAllSources({ threads, sources }) {
  const sourcesAll = [];
  threads.forEach(thread => {
    const { actor } = thread;

    for (const source in sources[actor]) {
      sourcesAll.push(sources[actor][source]);
    }
  });
  return sourcesAll;
}

export function getSourcesInsideGroup(item, { threads, sources }) {
  const sourcesInsideDirectory = [];

  const findAllSourcesInsideDirectory = directoryToSearch => {
    const childrenItems = getChildren(directoryToSearch);

    childrenItems.forEach(itemChild => {
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
