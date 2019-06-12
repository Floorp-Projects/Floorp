/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

/**
 * Utils for working with Source URLs
 * @module utils/source
 */

import { isOriginalId, isGeneratedId } from "devtools-source-map";
import { getUnicodeUrl } from "devtools-modules";

import { endTruncateStr } from "./utils";
import { truncateMiddleText } from "../utils/text";
import { parse as parseURL } from "../utils/url";
import { memoizeLast } from "../utils/memoizeLast";
import { renderWasmText } from "./wasm";
import { toEditorLine } from "./editor";
export { isMinified } from "./isMinified";
import { getURL, getFileExtension } from "./sources-tree";
import { prefs, features } from "./prefs";

import type {
  SourceId,
  Source,
  SourceActor,
  SourceContent,
  SourceLocation,
} from "../types";
import { isFulfilled, type AsyncValue } from "./async-value";
import type { Symbols } from "../reducers/types";

type transformUrlCallback = string => string;

export const sourceTypes = {
  coffee: "coffeescript",
  js: "javascript",
  jsx: "react",
  ts: "typescript",
  vue: "vue",
};

/**
 * Trims the query part or reference identifier of a url string, if necessary.
 *
 * @memberof utils/source
 * @static
 */
function trimUrlQuery(url: string): string {
  const length = url.length;
  const q1 = url.indexOf("?");
  const q2 = url.indexOf("&");
  const q3 = url.indexOf("#");
  const q = Math.min(
    q1 != -1 ? q1 : length,
    q2 != -1 ? q2 : length,
    q3 != -1 ? q3 : length
  );

  return url.slice(0, q);
}

export function shouldBlackbox(source: ?Source) {
  if (!source) {
    return false;
  }

  if (!source.url) {
    return false;
  }

  if (isOriginalId(source.id) && !features.originalBlackbox) {
    return false;
  }

  return true;
}

export function shouldPrettyPrint(
  source: Source,
  content: SourceContent
): boolean {
  if (
    !source ||
    isPretty(source) ||
    !isJavaScript(source, content) ||
    isOriginal(source) ||
    (prefs.clientSourceMapsEnabled && source.sourceMapURL)
  ) {
    return false;
  }

  return true;
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
export function isJavaScript(source: Source, content: SourceContent): boolean {
  const url = source.url;
  const contentType = content.type === "wasm" ? null : content.contentType;
  return (
    (url && /\.(jsm|js)?$/.test(trimUrlQuery(url))) ||
    !!(contentType && contentType.includes("javascript"))
  );
}

/**
 * @memberof utils/source
 * @static
 */
export function isPretty(source: Source): boolean {
  const url = source.url;
  return isPrettyURL(url);
}

export function isPrettyURL(url: string): boolean {
  return url ? /formatted$/.test(url) : false;
}

export function isThirdParty(source: Source) {
  const url = source.url;
  if (!source || !url) {
    return false;
  }

  return !!url.match(/(node_modules|bower_components)/);
}

/**
 * @memberof utils/source
 * @static
 */
export function getPrettySourceURL(url: ?string): string {
  if (!url) {
    url = "";
  }
  return `${url}:formatted`;
}

/**
 * @memberof utils/source
 * @static
 */
export function getRawSourceURL(url: string): string {
  return url ? url.replace(/:formatted$/, "") : url;
}

function resolveFileURL(
  url: string,
  transformUrl: transformUrlCallback = initialUrl => initialUrl,
  truncate: boolean = true
) {
  url = getRawSourceURL(url || "");
  const name = transformUrl(url);
  if (!truncate) {
    return name;
  }
  return endTruncateStr(name, 50);
}

export function getFormattedSourceId(id: string) {
  const sourceId = id.split("/")[1];
  return `SOURCE${sourceId}`;
}

/**
 * Gets a readable filename from a source URL for display purposes.
 * If the source does not have a URL, the source ID will be returned instead.
 *
 * @memberof utils/source
 * @static
 */
export function getFilename(source: Source) {
  const { url, id } = source;
  if (!getRawSourceURL(url)) {
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
export function getTruncatedFileName(
  source: Source,
  querystring: string = "",
  length: number = 30
) {
  return truncateMiddleText(`${getFilename(source)}${querystring}`, length);
}

/* Gets path for files with same filename for editor tabs, breakpoints, etc.
 * Pass the source, and list of other sources
 *
 * @memberof utils/source
 * @static
 */

export function getDisplayPath(mySource: Source, sources: Source[]) {
  const filename = getFilename(mySource);

  // Find sources that have the same filename, but different paths
  // as the original source
  const similarSources = sources.filter(
    source =>
      getRawSourceURL(mySource.url) != getRawSourceURL(source.url) &&
      filename == getFilename(source)
  );

  if (similarSources.length == 0) {
    return undefined;
  }

  // get an array of source path directories e.g. ['a/b/c.html'] => [['b', 'a']]
  const paths = [mySource, ...similarSources].map(source =>
    getURL(source)
      .path.split("/")
      .reverse()
      .slice(1)
  );

  // create an array of similar path directories and one dis-similar directory
  // for example [`a/b/c.html`, `a1/b/c.html`] => ['b', 'a']
  // where 'b' is the similar directory and 'a' is the dis-similar directory.
  let similar = true;
  const displayPath = [];
  for (let i = 0; similar && i < paths[0].length; i++) {
    const [dir, ...dirs] = paths.map(path => path[i]);
    displayPath.push(dir);
    similar = dirs.includes(dir);
  }

  return displayPath.reverse().join("/");
}

/**
 * Gets a readable source URL for display purposes.
 * If the source does not have a URL, the source ID will be returned instead.
 *
 * @memberof utils/source
 * @static
 */
export function getFileURL(source: Source, truncate: boolean = true) {
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

export function getSourcePath(url: string) {
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
export function getSourceLineCount(content: SourceContent): number {
  if (content.type === "wasm") {
    const { binary } = content.value;
    return binary.length;
  }

  return content.value.split("\n").length;
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
export function getMode(
  source: Source,
  content: SourceContent,
  symbols?: Symbols
): { name: string, base?: Object } {
  const { url } = source;

  if (content.type !== "text") {
    return { name: "text" };
  }

  const { contentType, value: text } = content;

  if ((url && url.match(/\.jsx$/i)) || (symbols && symbols.hasJsx)) {
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
    { ext: ".c", mode: "text/x-csrc" },
    { ext: ".kt", mode: "text/x-kotlin" },
    { ext: ".cpp", mode: "text/x-c++src" },
    { ext: ".m", mode: "text/x-objectivec" },
    { ext: ".rs", mode: "text/x-rustsrc" },
    { ext: ".hx", mode: "text/x-haxe" },
  ];

  // check for C and other non JS languages
  if (url) {
    const result = languageMimeMap.find(({ ext }) => url.endsWith(ext));

    if (result !== undefined) {
      return { name: result.mode };
    }
  }

  // if the url ends with .marko or .es6 we set the name to Javascript so
  // syntax highlighting works for these file extensions too
  if (url && url.match(/\.marko|\.es6$/i)) {
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

export function isInlineScript(source: SourceActor): boolean {
  return source.introductionType === "scriptElement";
}

export const getLineText = memoizeLast(
  (
    sourceId: SourceId,
    asyncContent: AsyncValue<SourceContent> | null,
    line: number
  ) => {
    if (!asyncContent || !isFulfilled(asyncContent)) {
      return "";
    }

    const content = asyncContent.value;

    if (content.type === "wasm") {
      const editorLine = toEditorLine(sourceId, line);
      const lines = renderWasmText(sourceId, content);
      return lines[editorLine] || "";
    }

    const lineText = content.value.split("\n")[line - 1];
    return lineText || "";
  }
);

export function getTextAtPosition(
  sourceId: SourceId,
  asyncContent: AsyncValue<SourceContent> | null,
  location: SourceLocation
) {
  const column = location.column || 0;
  const line = location.line;

  const lineText = getLineText(sourceId, asyncContent, line);
  return lineText.slice(column, column + 100).trim();
}

export function getSourceClassnames(source: Object, symbols?: Symbols) {
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

  if (symbols && !symbols.loading && symbols.framework) {
    return symbols.framework.toLowerCase();
  }

  return sourceTypes[getFileExtension(source)] || defaultClassName;
}

export function getRelativeUrl(source: Source, root: string) {
  const { group, path } = getURL(source);
  if (!root) {
    return path;
  }

  // + 1 removes the leading "/"
  const url = group + path;
  return url.slice(url.indexOf(root) + root.length + 1);
}

export function underRoot(source: Source, root: string) {
  return source.url && source.url.includes(root);
}

export function isOriginal(source: Source) {
  // Pretty-printed sources are given original IDs, so no need
  // for any additional check
  return isOriginalId(source.id);
}

export function isGenerated(source: Source) {
  return isGeneratedId(source.id);
}

export function getSourceQueryString(source: ?Source) {
  if (!source) {
    return;
  }

  return parseURL(getRawSourceURL(source.url)).search;
}

export function isUrlExtension(url: string) {
  return /\/?(chrome|moz)-extension:\//.test(url);
}

export function getPlainUrl(url: string): string {
  const queryStart = url.indexOf("?");
  return queryStart !== -1 ? url.slice(0, queryStart) : url;
}
