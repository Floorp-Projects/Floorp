/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { parse } from "../url";

const {
  getUnicodeHostname,
  getUnicodeUrlPath,
} = require("devtools/client/shared/unicode-url");

export function getFilenameFromPath(pathname) {
  let filename = "";
  if (pathname) {
    filename = pathname.substring(pathname.lastIndexOf("/") + 1);
    // This file does not have a name. Default should be (index).
    if (filename == "") {
      filename = "(index)";
    }
  }
  return filename;
}

const NoDomain = "(no domain)";
const def = { path: "", search: "", group: "", filename: "" };

/**
 * Compute the URL which may be displayed in the Source Tree.
 *
 * @param {String} url
 *        The source absolute URL as a string
 * @return URL Object
 *        A URL object to represent this source.
 */
export function getDisplayURL(url) {
  if (!url) {
    return def;
  }

  const { pathname, search, protocol, host } = parse(url);
  const filename = getUnicodeUrlPath(getFilenameFromPath(pathname));

  switch (protocol) {
    case "javascript:":
      // Ignore `javascript:` URLs for now
      return def;

    case "moz-extension:":
    case "resource:":
      return {
        ...def,
        path: pathname,
        search,
        filename,
        group: `${protocol}//${host || ""}`,
      };

    case "webpack:":
    case "ng:":
      return {
        ...def,
        path: pathname,
        search,
        filename,
        group: `${protocol}//`,
      };

    case "about:":
      // An about page is a special case
      return {
        ...def,
        path: "/",
        search,
        filename,
        group: url,
      };

    case "data:":
      return {
        ...def,
        path: "/",
        search,
        group: NoDomain,
        filename: url,
      };

    case "":
      if (pathname && pathname.startsWith("/")) {
        // use file protocol for a URL like "/foo/bar.js"
        return {
          ...def,
          path: pathname,
          search,
          filename,
          group: "file://",
        };
      } else if (!host) {
        return {
          ...def,
          path: pathname,
          search,
          group: "",
          filename,
        };
      }
      break;

    case "http:":
    case "https:":
      return {
        ...def,
        path: pathname,
        search,
        filename,
        group: getUnicodeHostname(host),
      };
  }

  return {
    ...def,
    path: pathname,
    search,
    group: protocol ? `${protocol}//` : "",
    filename,
  };
}
