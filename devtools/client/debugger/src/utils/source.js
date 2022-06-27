/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Utils for working with Source URLs
 * @module utils/source
 */

const { getUnicodeUrl } = require("devtools/client/shared/unicode-url");

import { endTruncateStr } from "./utils";
import { truncateMiddleText } from "../utils/text";
import { parse as parseURL } from "../utils/url";
import { memoizeLast } from "../utils/memoizeLast";
import { renderWasmText } from "./wasm";
import { toEditorLine } from "./editor";
export { isMinified } from "./isMinified";
import { getURL, getFileExtension } from "./sources-tree";

import { isFulfilled } from "./async-value";

export const sourceTypes = {
  coffee: "coffeescript",
  js: "javascript",
  jsx: "react",
  ts: "typescript",
  tsx: "typescript",
  vue: "vue",
};

const javascriptLikeExtensions = ["marko", "es6", "vue", "jsm"];

function getPath(source) {
  const { path } = getURL(source);
  let lastIndex = path.lastIndexOf("/");
  let nextToLastIndex = path.lastIndexOf("/", lastIndex - 1);

  const result = [];
  do {
    result.push(path.slice(nextToLastIndex + 1, lastIndex));
    lastIndex = nextToLastIndex;
    nextToLastIndex = path.lastIndexOf("/", lastIndex - 1);
  } while (lastIndex !== nextToLastIndex);

  result.push("");

  return result;
}

export function shouldBlackbox(source) {
  if (!source) {
    return false;
  }

  if (!source.url) {
    return false;
  }

  return true;
}

/**
 * Checks if the frame is within a line ranges which are blackboxed
 * in the source.
 *
 * @param {Object}  frame
 *                  The current frame
 * @param {Object}  source
 *                  The source related to the frame
 * @param {Object}  blackboxedRanges
 *                  The currently blackboxedRanges for all the sources.
 * @param {Boolean} isFrameBlackBoxed
 *                  If the frame is within the blackboxed range
 *                  or not.
 */
export function isFrameBlackBoxed(frame, source, blackboxedRanges) {
  return (
    !!source?.isBlackBoxed &&
    (!blackboxedRanges[source.url].length ||
      !!findBlackBoxRange(source, blackboxedRanges, {
        start: frame.location.line,
        end: frame.location.line,
      }))
  );
}

/**
 * Checks if a blackbox range exist for the line range.
 * That is if any start and end lines overlap any of the
 * blackbox ranges
 *
 * @param {Object}  source
 *                  The current selected source
 * @param {Object}  blackboxedRanges
 *                  The store of blackboxedRanges
 * @param {Object}  lineRange
 *                  The start/end line range `{ start: <Number>, end: <Number> }`
 * @return {Object} blackboxRange
 *                  The first matching blackbox range that all or part of the
 *                  specified lineRange sits within.
 */
export function findBlackBoxRange(source, blackboxedRanges, lineRange) {
  const ranges = blackboxedRanges[source.url];
  if (!ranges || !ranges.length) {
    return null;
  }

  return ranges.find(
    range =>
      (lineRange.start >= range.start.line &&
        lineRange.start <= range.end.line) ||
      (lineRange.end >= range.start.line && lineRange.end <= range.end.line)
  );
}

/**
 * Returns true if the specified url and/or content type are specific to
 * javascript files.
 *
 * @return boolean
 *         True if the source is likely javascript.
 *
 * @memberof utils/source
 * @static
 */
export function isJavaScript(source, content) {
  const extension = getFileExtension(source).toLowerCase();
  const contentType = content.type === "wasm" ? null : content.contentType;
  return (
    javascriptLikeExtensions.includes(extension) ||
    !!(contentType && contentType.includes("javascript"))
  );
}

/**
 * @memberof utils/source
 * @static
 */
export function isPretty(source) {
  return isPrettyURL(source.url);
}

export function isPrettyURL(url) {
  return url ? url.endsWith(":formatted") : false;
}

export function isThirdParty(source) {
  const { url } = source;
  if (!source || !url) {
    return false;
  }

  return url.includes("node_modules") || url.includes("bower_components");
}

/**
 * @memberof utils/source
 * @static
 */
export function getPrettySourceURL(url) {
  if (!url) {
    url = "";
  }
  return `${url}:formatted`;
}

/**
 * @memberof utils/source
 * @static
 */
export function getRawSourceURL(url) {
  return url && url.endsWith(":formatted")
    ? url.slice(0, -":formatted".length)
    : url;
}

function resolveFileURL(
  url,
  transformUrl = initialUrl => initialUrl,
  truncate = true
) {
  url = getRawSourceURL(url || "");
  const name = transformUrl(url);
  if (!truncate) {
    return name;
  }
  return endTruncateStr(name, 50);
}

export function getFormattedSourceId(id) {
  const firstIndex = id.indexOf("/");
  const secondIndex = id.indexOf("/", firstIndex);
  return `SOURCE${id.slice(firstIndex, secondIndex)}`;
}

/**
 * Gets a readable filename from a source URL for display purposes.
 * If the source does not have a URL, the source ID will be returned instead.
 *
 * @memberof utils/source
 * @static
 */
export function getFilename(
  source,
  rawSourceURL = getRawSourceURL(source.url)
) {
  const { id } = source;
  if (!rawSourceURL) {
    return getFormattedSourceId(id);
  }

  const { filename } = getURL(source);
  return getRawSourceURL(filename);
}

/**
 * Provides a middle-trunated filename
 *
 * @memberof utils/source
 * @static
 */
export function getTruncatedFileName(source, querystring = "", length = 30) {
  return truncateMiddleText(`${getFilename(source)}${querystring}`, length);
}

/* Gets path for files with same filename for editor tabs, breakpoints, etc.
 * Pass the source, and list of other sources
 *
 * @memberof utils/source
 * @static
 */

export function getDisplayPath(mySource, sources) {
  const rawSourceURL = getRawSourceURL(mySource.url);
  const filename = getFilename(mySource, rawSourceURL);

  // Find sources that have the same filename, but different paths
  // as the original source
  const similarSources = sources.filter(source => {
    const rawSource = getRawSourceURL(source.url);
    return (
      rawSourceURL != rawSource && filename == getFilename(source, rawSource)
    );
  });

  if (similarSources.length == 0) {
    return undefined;
  }

  // get an array of source path directories e.g. ['a/b/c.html'] => [['b', 'a']]
  const paths = new Array(similarSources.length + 1);

  paths[0] = getPath(mySource);
  for (let i = 0; i < similarSources.length; ++i) {
    paths[i + 1] = getPath(similarSources[i]);
  }

  // create an array of similar path directories and one dis-similar directory
  // for example [`a/b/c.html`, `a1/b/c.html`] => ['b', 'a']
  // where 'b' is the similar directory and 'a' is the dis-similar directory.
  let displayPath = "";
  for (let i = 0; i < paths[0].length; i++) {
    let similar = false;
    for (let k = 1; k < paths.length; ++k) {
      if (paths[k][i] === paths[0][i]) {
        similar = true;
        break;
      }
    }

    displayPath = paths[0][i] + (i !== 0 ? "/" : "") + displayPath;

    if (!similar) {
      break;
    }
  }

  return displayPath;
}

/**
 * Gets a readable source URL for display purposes.
 * If the source does not have a URL, the source ID will be returned instead.
 *
 * @memberof utils/source
 * @static
 */
export function getFileURL(source, truncate = true) {
  const { url, id } = source;
  if (!url) {
    return getFormattedSourceId(id);
  }

  return resolveFileURL(url, getUnicodeUrl, truncate);
}

const contentTypeModeMap = {
  "text/javascript": { name: "javascript" },
  "text/typescript": { name: "javascript", typescript: true },
  "text/coffeescript": { name: "coffeescript" },
  "text/typescript-jsx": {
    name: "jsx",
    base: { name: "javascript", typescript: true },
  },
  "text/jsx": { name: "jsx" },
  "text/x-elm": { name: "elm" },
  "text/x-clojure": { name: "clojure" },
  "text/x-clojurescript": { name: "clojure" },
  "text/wasm": { name: "text" },
  "text/html": { name: "htmlmixed" },
};

export function getSourcePath(url) {
  if (!url) {
    return "";
  }

  const { path, href } = parseURL(url);
  // for URLs like "about:home" the path is null so we pass the full href
  return path || href;
}

/**
 * Returns amount of lines in the source. If source is a WebAssembly binary,
 * the function returns amount of bytes.
 */
export function getSourceLineCount(content) {
  if (content.type === "wasm") {
    const { binary } = content.value;
    return binary.length;
  }

  let count = 0;

  for (let i = 0; i < content.value.length; ++i) {
    if (content.value[i] === "\n") {
      ++count;
    }
  }

  return count + 1;
}

/**
 *
 * Checks if a source is minified based on some heuristics
 * @param key
 * @param text
 * @return boolean
 * @memberof utils/source
 * @static
 */

/**
 *
 * Returns Code Mirror mode for source content type
 * @param contentType
 * @return String
 * @memberof utils/source
 * @static
 */
// eslint-disable-next-line complexity
export function getMode(source, content, symbols) {
  const extension = getFileExtension(source);

  if (content.type !== "text") {
    return { name: "text" };
  }

  const { contentType, value: text } = content;

  if (extension === "jsx" || (symbols && symbols.hasJsx)) {
    if (symbols && symbols.hasTypes) {
      return { name: "text/typescript-jsx" };
    }
    return { name: "jsx" };
  }

  if (symbols && symbols.hasTypes) {
    if (symbols.hasJsx) {
      return { name: "text/typescript-jsx" };
    }

    return { name: "text/typescript" };
  }

  const languageMimeMap = [
    { ext: "c", mode: "text/x-csrc" },
    { ext: "kt", mode: "text/x-kotlin" },
    { ext: "cpp", mode: "text/x-c++src" },
    { ext: "m", mode: "text/x-objectivec" },
    { ext: "rs", mode: "text/x-rustsrc" },
    { ext: "hx", mode: "text/x-haxe" },
  ];

  // check for C and other non JS languages
  const result = languageMimeMap.find(({ ext }) => extension === ext);
  if (result !== undefined) {
    return { name: result.mode };
  }

  // if the url ends with a known Javascript-like URL, provide JavaScript mode.
  // uses the first part of the URL to ignore query string
  if (javascriptLikeExtensions.find(ext => ext === extension)) {
    return { name: "javascript" };
  }

  // Use HTML mode for files in which the first non whitespace
  // character is `<` regardless of extension.
  const isHTMLLike = text.match(/^\s*</);
  if (!contentType) {
    if (isHTMLLike) {
      return { name: "htmlmixed" };
    }
    return { name: "text" };
  }

  // // @flow or /* @flow */
  if (text.match(/^\s*(\/\/ @flow|\/\* @flow \*\/)/)) {
    return contentTypeModeMap["text/typescript"];
  }

  if (/script|elm|jsx|clojure|wasm|html/.test(contentType)) {
    if (contentType in contentTypeModeMap) {
      return contentTypeModeMap[contentType];
    }

    return contentTypeModeMap["text/javascript"];
  }

  if (isHTMLLike) {
    return { name: "htmlmixed" };
  }

  return { name: "text" };
}

export function isInlineScript(source) {
  return source.introductionType === "scriptElement";
}

function getNthLine(str, lineNum) {
  let startIndex = -1;

  let newLinesFound = 0;
  while (newLinesFound < lineNum) {
    const nextIndex = str.indexOf("\n", startIndex + 1);
    if (nextIndex === -1) {
      return null;
    }
    startIndex = nextIndex;
    newLinesFound++;
  }
  const endIndex = str.indexOf("\n", startIndex + 1);
  if (endIndex === -1) {
    return str.slice(startIndex + 1);
  }

  return str.slice(startIndex + 1, endIndex);
}

export const getLineText = memoizeLast((sourceId, asyncContent, line) => {
  if (!asyncContent || !isFulfilled(asyncContent)) {
    return "";
  }

  const content = asyncContent.value;

  if (content.type === "wasm") {
    const editorLine = toEditorLine(sourceId, line);
    const lines = renderWasmText(sourceId, content);
    return lines[editorLine] || "";
  }

  const lineText = getNthLine(content.value, line - 1);
  return lineText || "";
});

export function getTextAtPosition(sourceId, asyncContent, location) {
  const { column, line = 0 } = location;

  const lineText = getLineText(sourceId, asyncContent, line);
  return lineText.slice(column, column + 100).trim();
}

export function getSourceClassnames(source, symbols) {
  // Conditionals should be ordered by priority of icon!
  const defaultClassName = "file";

  if (!source || !source.url) {
    return defaultClassName;
  }

  if (isPretty(source)) {
    return "prettyPrint";
  }

  if (source.isBlackBoxed) {
    return "blackBox";
  }

  if (symbols && symbols.framework) {
    return symbols.framework.toLowerCase();
  }

  if (isUrlExtension(source.url)) {
    return "extension";
  }

  return sourceTypes[getFileExtension(source)] || defaultClassName;
}

export function getRelativeUrl(source, root) {
  const { group, path } = getURL(source);
  if (!root) {
    return path;
  }

  // + 1 removes the leading "/"
  const url = group + path;
  return url.slice(url.indexOf(root) + root.length + 1);
}

/**
 * source.url doesn't include thread actor ID, so before calling underRoot(), the thread actor ID
 * must be removed from the root, which this function handles.
 * @param {string} root The root url to be cleaned
 * @param {Set<Thread>} threads The list of threads
 * @returns {string} The root url with thread actor IDs removed
 */
export function removeThreadActorId(root, threads) {
  threads.forEach(thread => {
    if (root.includes(thread.actor)) {
      root = root.slice(thread.actor.length + 1);
    }
  });
  return root;
}

/**
 * Checks if the source is descendant of the root identified by the
 * root url specified. The root might likely be projectDirectoryRoot which
 * is a defined by a pref that allows users restrict the source tree to
 * a subset of sources.
 *
 * @param {Object} source
 *                  The source object
 * @param {String} rootUrlWithoutThreadActor
 *                 The url for the root node, without the thread actor ID. This can be obtained
 *                 by calling removeThreadActorId()
 */
export function isDescendantOfRoot(source, rootUrlWithoutThreadActor) {
  if (source.url && source.url.includes("chrome://")) {
    const { group, path } = getURL(source);
    return (group + path).includes(rootUrlWithoutThreadActor);
  }

  return !!source.url && source.url.includes(rootUrlWithoutThreadActor);
}

export function isGenerated(source) {
  return !source.isOriginal;
}

export function getSourceQueryString(source) {
  if (!source) {
    return;
  }

  return parseURL(getRawSourceURL(source.url)).search;
}

export function isUrlExtension(url) {
  return url.includes("moz-extension:") || url.includes("chrome-extension");
}

export function isExtensionDirectoryPath(url) {
  if (isUrlExtension(url)) {
    const urlArr = url.replace(/\/+/g, "/").split("/");
    let extensionIndex = urlArr.indexOf("moz-extension:");
    if (extensionIndex === -1) {
      extensionIndex = urlArr.indexOf("chrome-extension:");
    }
    return !urlArr[extensionIndex + 2];
  }
}
