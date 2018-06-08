"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.getFilenameFromPath = getFilenameFromPath;
exports.getURL = getURL;

var _url = require("devtools/client/debugger/new/dist/vendors").vendored["url"];

var _lodash = require("devtools/client/shared/vendor/lodash");

var _devtoolsModules = require("devtools/client/debugger/new/dist/vendors").vendored["devtools-modules"];

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function getFilenameFromPath(pathname) {
  let filename = "";

  if (pathname) {
    filename = pathname.substring(pathname.lastIndexOf("/") + 1); // This file does not have a name. Default should be (index).

    if (filename == "" || !filename.includes(".")) {
      filename = "(index)";
    }
  }

  return filename;
}

const NoDomain = "(no domain)";

function getURL(sourceUrl, debuggeeUrl = "") {
  const url = sourceUrl;
  const def = {
    path: "",
    group: "",
    filename: ""
  };

  if (!url) {
    return def;
  }

  const {
    pathname,
    protocol,
    host,
    path
  } = (0, _url.parse)(url);
  const defaultDomain = (0, _url.parse)(debuggeeUrl).host;
  const filename = getFilenameFromPath(pathname);

  switch (protocol) {
    case "javascript:":
      // Ignore `javascript:` URLs for now
      return def;

    case "moz-extension:":
    case "resource:":
      return (0, _lodash.merge)(def, {
        path,
        group: `${protocol}//${host || ""}`,
        filename
      });

    case "webpack:":
    case "ng:":
      return (0, _lodash.merge)(def, {
        path: path,
        group: `${protocol}//`,
        filename: filename
      });

    case "about:":
      // An about page is a special case
      return (0, _lodash.merge)(def, {
        path: "/",
        group: url,
        filename: filename
      });

    case "data:":
      return (0, _lodash.merge)(def, {
        path: "/",
        group: NoDomain,
        filename: url
      });

    case null:
      if (pathname && pathname.startsWith("/")) {
        // use file protocol for a URL like "/foo/bar.js"
        return (0, _lodash.merge)(def, {
          path: path,
          group: "file://",
          filename: filename
        });
      } else if (host === null) {
        // use anonymous group for weird URLs
        return (0, _lodash.merge)(def, {
          path: url,
          group: defaultDomain,
          filename: filename
        });
      }

      break;

    case "http:":
    case "https:":
      return (0, _lodash.merge)(def, {
        path: pathname,
        group: (0, _devtoolsModules.getUnicodeHostname)(host),
        filename: filename
      });
  }

  return (0, _lodash.merge)(def, {
    path: path,
    group: protocol ? `${protocol}//` : "",
    filename: filename
  });
}