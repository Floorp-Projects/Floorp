"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.getFilenameFromPath = getFilenameFromPath;
exports.getURL = getURL;

var _url = require("devtools/client/debugger/new/dist/vendors").vendored["url"];

var _devtoolsModules = require("devtools/client/debugger/new/dist/vendors").vendored["devtools-modules"];

function _objectSpread(target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i] != null ? arguments[i] : {}; var ownKeys = Object.keys(source); if (typeof Object.getOwnPropertySymbols === 'function') { ownKeys = ownKeys.concat(Object.getOwnPropertySymbols(source).filter(function (sym) { return Object.getOwnPropertyDescriptor(source, sym).enumerable; })); } ownKeys.forEach(function (key) { _defineProperty(target, key, source[key]); }); } return target; }

function _defineProperty(obj, key, value) { if (key in obj) { Object.defineProperty(obj, key, { value: value, enumerable: true, configurable: true, writable: true }); } else { obj[key] = value; } return obj; }

const urlMap = new WeakMap();

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
const def = {
  path: "",
  group: "",
  filename: ""
};

function _getURL(source, debuggeeUrl) {
  const {
    url
  } = source;

  if (!url) {
    return def;
  }

  const {
    pathname,
    protocol,
    host,
    path
  } = (0, _url.parse)(url);
  const filename = (0, _devtoolsModules.getUnicodeUrlPath)(getFilenameFromPath(pathname));

  switch (protocol) {
    case "javascript:":
      // Ignore `javascript:` URLs for now
      return def;

    case "moz-extension:":
    case "resource:":
      return _objectSpread({}, def, {
        path,
        filename,
        group: `${protocol}//${host || ""}`
      });

    case "webpack:":
    case "ng:":
      return _objectSpread({}, def, {
        path: path,
        filename,
        group: `${protocol}//`
      });

    case "about:":
      // An about page is a special case
      return _objectSpread({}, def, {
        path: "/",
        filename,
        group: url
      });

    case "data:":
      return _objectSpread({}, def, {
        path: "/",
        group: NoDomain,
        filename: url
      });

    case null:
      if (pathname && pathname.startsWith("/")) {
        // use file protocol for a URL like "/foo/bar.js"
        return _objectSpread({}, def, {
          path: path,
          filename,
          group: "file://"
        });
      } else if (host === null) {
        // use anonymous group for weird URLs
        const defaultDomain = (0, _url.parse)(debuggeeUrl).host;
        return _objectSpread({}, def, {
          path: url,
          group: defaultDomain,
          filename
        });
      }

      break;

    case "http:":
    case "https:":
      return _objectSpread({}, def, {
        path: pathname,
        filename,
        group: (0, _devtoolsModules.getUnicodeHostname)(host)
      });
  }

  return _objectSpread({}, def, {
    path: path,
    group: protocol ? `${protocol}//` : "",
    filename
  });
}

function getURL(source, debuggeeUrl = "") {
  if (urlMap.has(source)) {
    return urlMap.get(source) || def;
  }

  const url = _getURL(source, debuggeeUrl);

  urlMap.set(source, url);
  return url;
}