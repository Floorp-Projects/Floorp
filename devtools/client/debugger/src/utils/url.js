/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const defaultUrl = {
  hash: "",
  host: "",
  hostname: "",
  href: "",
  origin: "null",
  password: "",
  path: "",
  pathname: "",
  port: "",
  protocol: "",
  search: "",
  // This should be a "URLSearchParams" object
  searchParams: {},
  username: "",
};

const parseCache = new Map();
export function parse(url) {
  if (parseCache.has(url)) {
    return parseCache.get(url);
  }

  let urlObj;
  try {
    urlObj = new URL(url);
  } catch (err) {
    urlObj = { ...defaultUrl };
    // If we're given simply a filename...
    if (url) {
      const hashStart = url.indexOf("#");
      if (hashStart >= 0) {
        urlObj.hash = url.slice(hashStart);
        url = url.slice(0, hashStart);

        if (urlObj.hash === "#") {
          // The standard URL parser does not include the ? unless there are
          // parameters included in the search value.
          urlObj.hash = "";
        }
      }

      const queryStart = url.indexOf("?");
      if (queryStart >= 0) {
        urlObj.search = url.slice(queryStart);
        url = url.slice(0, queryStart);

        if (urlObj.search === "?") {
          // The standard URL parser does not include the ? unless there are
          // parameters included in the search value.
          urlObj.search = "";
        }
      }

      urlObj.pathname = url;
    }
  }
  // When provided a special URL like "webpack:///webpack/foo",
  // prevents passing the three slashes in the path, and pass only onea.
  // This will prevent displaying modules in empty-name sub folders.
  urlObj.pathname = urlObj.pathname.replace(/\/+/, "/");
  urlObj.path = urlObj.pathname + urlObj.search;

  // Cache the result
  parseCache.set(url, urlObj);
  return urlObj;
}

export function sameOrigin(firstUrl, secondUrl) {
  return parse(firstUrl).origin == parse(secondUrl).origin;
}
