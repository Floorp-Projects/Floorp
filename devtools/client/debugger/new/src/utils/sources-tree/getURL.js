"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.getFilenameFromPath = getFilenameFromPath;
exports.getURL = getURL;

var _url = require("devtools/client/debugger/new/dist/vendors").vendored["url"];

var _lodash = require("devtools/client/shared/vendor/lodash");

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

    case "webpack:":
      // A Webpack source is a special case
      return (0, _lodash.merge)(def, {
        path: path,
        group: "webpack://",
        filename: filename
      });

    case "ng:":
      // An Angular source is a special case
      return (0, _lodash.merge)(def, {
        path: path,
        group: "ng://",
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
        // If it's just a URL like "/foo/bar.js", resolve it to the file
        // protocol
        return (0, _lodash.merge)(def, {
          path: path,
          group: "file://",
          filename: filename
        });
      } else if (host === null) {
        // We don't know what group to put this under, and it's a script
        // with a weird URL. Just group them all under an anonymous group.
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
        group: host,
        filename: filename
      });
  }

  return (0, _lodash.merge)(def, {
    path: path,
    group: protocol ? `${protocol}//` : "",
    filename: filename
  });
}