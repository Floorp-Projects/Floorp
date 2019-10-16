/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

const md5 = require("md5");

function originalToGeneratedId(sourceId: string) {
  if (isGeneratedId(sourceId)) {
    return sourceId;
  }

  const lastIndex = sourceId.lastIndexOf("/originalSource");

  return lastIndex !== -1 ? sourceId.slice(0, lastIndex) : "";
}

const getMd5 = memoize((url: string) => md5(url));

function generatedToOriginalId(generatedId: string, url: string) {
  return `${generatedId}/originalSource-${getMd5(url)}`;
}

function isOriginalId(id: string) {
  return id.includes("/originalSource");
}

function isGeneratedId(id: string) {
  return !isOriginalId(id);
}

/**
 * Trims the query part or reference identifier of a URL string, if necessary.
 */
function trimUrlQuery(url: string): string {
  const length = url.length;

  for (let i = 0; i < length; ++i) {
    if (url[i] === "?" || url[i] === "&" || url[i] === "#") {
      return url.slice(0, i);
    }
  }

  return url;
}

// Map suffix to content type.
const contentMap = {
  js: "text/javascript",
  jsm: "text/javascript",
  mjs: "text/javascript",
  ts: "text/typescript",
  tsx: "text/typescript-jsx",
  jsx: "text/jsx",
  vue: "text/vue",
  coffee: "text/coffeescript",
  elm: "text/elm",
  cljc: "text/x-clojure",
  cljs: "text/x-clojurescript",
};

/**
 * Returns the content type for the specified URL.  If no specific
 * content type can be determined, "text/plain" is returned.
 *
 * @return String
 *         The content type.
 */
function getContentType(url: string): string {
  url = trimUrlQuery(url);
  const dot = url.lastIndexOf(".");
  if (dot >= 0) {
    const name = url.substring(dot + 1);
    if (name in contentMap) {
      return contentMap[name];
    }
  }
  return "text/plain";
}

function memoize<T: Function, A, R>(func: T): A => R {
  const map = new Map();

  return (arg: A) => {
    if (map.has(arg)) {
      return ((map.get(arg): any): R);
    }

    const result: R = func(arg);
    map.set(arg, result);
    return result;
  };
}

module.exports = {
  originalToGeneratedId,
  generatedToOriginalId,
  isOriginalId,
  isGeneratedId,
  getContentType,
  contentMapForTesting: contentMap,
};
