"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.sourceTypes = exports.isMinified = undefined;

var _isMinified = require("./isMinified");

Object.defineProperty(exports, "isMinified", {
  enumerable: true,
  get: function () {
    return _isMinified.isMinified;
  }
});
exports.shouldPrettyPrint = shouldPrettyPrint;
exports.isJavaScript = isJavaScript;
exports.isPretty = isPretty;
exports.isPrettyURL = isPrettyURL;
exports.isThirdParty = isThirdParty;
exports.getPrettySourceURL = getPrettySourceURL;
exports.getRawSourceURL = getRawSourceURL;
exports.getFormattedSourceId = getFormattedSourceId;
exports.getFilename = getFilename;
exports.getTruncatedFileName = getTruncatedFileName;
exports.getFileURL = getFileURL;
exports.getSourcePath = getSourcePath;
exports.getSourceLineCount = getSourceLineCount;
exports.getMode = getMode;
exports.isLoaded = isLoaded;
exports.isLoading = isLoading;
exports.getTextAtPosition = getTextAtPosition;
exports.getSourceClassnames = getSourceClassnames;

var _devtoolsSourceMap = require("devtools/client/shared/source-map/index.js");

var _utils = require("./utils");

var _text = require("../utils/text");

var _url = require("devtools/client/debugger/new/dist/vendors").vendored["url"];

var _devtoolsModules = require("devtools/client/debugger/new/dist/vendors").vendored["devtools-modules"];

var _sourcesTree = require("./sources-tree/index");

const sourceTypes = exports.sourceTypes = {
  coffee: "coffeescript",
  js: "javascript",
  jsx: "react",
  ts: "typescript"
};
/**
 * Trims the query part or reference identifier of a url string, if necessary.
 *
 * @memberof utils/source
 * @static
 */

function trimUrlQuery(url) {
  const length = url.length;
  const q1 = url.indexOf("?");
  const q2 = url.indexOf("&");
  const q3 = url.indexOf("#");
  const q = Math.min(q1 != -1 ? q1 : length, q2 != -1 ? q2 : length, q3 != -1 ? q3 : length);
  return url.slice(0, q);
}

function shouldPrettyPrint(source) {
  if (!source) {
    return false;
  }

  const _isPretty = isPretty(source);

  const _isJavaScript = isJavaScript(source);

  const isOriginal = (0, _devtoolsSourceMap.isOriginalId)(source.id);
  const hasSourceMap = source.sourceMapURL;

  if (_isPretty || isOriginal || hasSourceMap || !_isJavaScript) {
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


function isJavaScript(source) {
  const url = source.url;
  const contentType = source.contentType;
  return url && /\.(jsm|js)?$/.test(trimUrlQuery(url)) || !!(contentType && contentType.includes("javascript"));
}
/**
 * @memberof utils/source
 * @static
 */


function isPretty(source) {
  const url = source.url;
  return isPrettyURL(url);
}

function isPrettyURL(url) {
  return url ? /formatted$/.test(url) : false;
}

function isThirdParty(source) {
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


function getPrettySourceURL(url) {
  if (!url) {
    url = "";
  }

  return `${url}:formatted`;
}
/**
 * @memberof utils/source
 * @static
 */


function getRawSourceURL(url) {
  return url ? url.replace(/:formatted$/, "") : url;
}

function resolveFileURL(url, transformUrl = initialUrl => initialUrl) {
  url = getRawSourceURL(url || "");
  const name = transformUrl(url);
  return (0, _utils.endTruncateStr)(name, 50);
}

function getFormattedSourceId(id) {
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


function getFilename(source) {
  const {
    url,
    id
  } = source;

  if (!url) {
    return getFormattedSourceId(id);
  }

  const {
    filename
  } = (0, _sourcesTree.getURL)(source);
  return filename;
}
/**
 * Provides a middle-trunated filename
 *
 * @memberof utils/source
 * @static
 */


function getTruncatedFileName(source, length = 30) {
  return (0, _text.truncateMiddleText)(getFilename(source), length);
}
/**
 * Gets a readable source URL for display purposes.
 * If the source does not have a URL, the source ID will be returned instead.
 *
 * @memberof utils/source
 * @static
 */


function getFileURL(source) {
  const {
    url,
    id
  } = source;

  if (!url) {
    return getFormattedSourceId(id);
  }

  return resolveFileURL(url, _devtoolsModules.getUnicodeUrl);
}

const contentTypeModeMap = {
  "text/javascript": {
    name: "javascript"
  },
  "text/typescript": {
    name: "javascript",
    typescript: true
  },
  "text/coffeescript": {
    name: "coffeescript"
  },
  "text/typescript-jsx": {
    name: "jsx",
    base: {
      name: "javascript",
      typescript: true
    }
  },
  "text/jsx": {
    name: "jsx"
  },
  "text/x-elm": {
    name: "elm"
  },
  "text/x-clojure": {
    name: "clojure"
  },
  "text/wasm": {
    name: "text"
  },
  "text/html": {
    name: "htmlmixed"
  }
};

function getSourcePath(url) {
  if (!url) {
    return "";
  }

  const {
    path,
    href
  } = (0, _url.parse)(url); // for URLs like "about:home" the path is null so we pass the full href

  return path || href;
}
/**
 * Returns amount of lines in the source. If source is a WebAssembly binary,
 * the function returns amount of bytes.
 */


function getSourceLineCount(source) {
  if (source.isWasm && !source.error) {
    const {
      binary
    } = source.text;
    return binary.length;
  }

  return source.text != undefined ? source.text.split("\n").length : 0;
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


function getMode(source, symbols) {
  const {
    contentType,
    text,
    isWasm,
    url
  } = source;

  if (!text || isWasm) {
    return {
      name: "text"
    };
  }

  if (url && url.match(/\.jsx$/i) || symbols && symbols.hasJsx) {
    if (symbols && symbols.hasTypes) {
      return {
        name: "text/typescript-jsx"
      };
    }

    return {
      name: "jsx"
    };
  }

  if (symbols && symbols.hasTypes) {
    if (symbols.hasJsx) {
      return {
        name: "text/typescript-jsx"
      };
    }

    return {
      name: "text/typescript"
    };
  }

  const languageMimeMap = [{
    ext: ".c",
    mode: "text/x-csrc"
  }, {
    ext: ".kt",
    mode: "text/x-kotlin"
  }, {
    ext: ".cpp",
    mode: "text/x-c++src"
  }, {
    ext: ".m",
    mode: "text/x-objectivec"
  }, {
    ext: ".rs",
    mode: "text/x-rustsrc"
  }, {
    ext: ".hx",
    mode: "text/x-haxe"
  }]; // check for C and other non JS languages

  if (url) {
    const result = languageMimeMap.find(({
      ext
    }) => url.endsWith(ext));

    if (result !== undefined) {
      return {
        name: result.mode
      };
    }
  } // if the url ends with .marko we set the name to Javascript so
  // syntax highlighting works for marko too


  if (url && url.match(/\.marko$/i)) {
    return {
      name: "javascript"
    };
  } // Use HTML mode for files in which the first non whitespace
  // character is `<` regardless of extension.


  const isHTMLLike = text.match(/^\s*</);

  if (!contentType) {
    if (isHTMLLike) {
      return {
        name: "htmlmixed"
      };
    }

    return {
      name: "text"
    };
  } // //  or /* @flow */


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
    return {
      name: "htmlmixed"
    };
  }

  return {
    name: "text"
  };
}

function isLoaded(source) {
  return source.loadedState === "loaded";
}

function isLoading(source) {
  return source.loadedState === "loading";
}

function getTextAtPosition(source, location) {
  if (!source || !source.text || source.isWasm) {
    return "";
  }

  const line = location.line;
  const column = location.column || 0;
  const lineText = source.text.split("\n")[line - 1];

  if (!lineText) {
    return "";
  }

  return lineText.slice(column, column + 100).trim();
}

function getSourceClassnames(source, sourceMetaData) {
  // Conditionals should be ordered by priority of icon!
  const defaultClassName = "file";

  if (!source || !source.url) {
    return defaultClassName;
  }

  if (sourceMetaData && sourceMetaData.framework) {
    return sourceMetaData.framework.toLowerCase();
  }

  if (isPretty(source)) {
    return "prettyPrint";
  }

  if (source.isBlackBoxed) {
    return "blackBox";
  }

  return sourceTypes[(0, _sourcesTree.getFileExtension)(source.url)] || defaultClassName;
}