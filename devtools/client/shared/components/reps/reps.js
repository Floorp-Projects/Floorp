(function webpackUniversalModuleDefinition(root, factory) {
	if(typeof exports === 'object' && typeof module === 'object')
		module.exports = factory(require("devtools/client/shared/vendor/react-dom-factories"), require("devtools/client/shared/vendor/lodash"), require("devtools/client/shared/vendor/react-prop-types"), require("devtools/client/shared/vendor/react"));
	else if(typeof define === 'function' && define.amd)
		define(["devtools/client/shared/vendor/react-dom-factories", "devtools/client/shared/vendor/lodash", "devtools/client/shared/vendor/react-prop-types", "devtools/client/shared/vendor/react"], factory);
	else {
		var a = typeof exports === 'object' ? factory(require("devtools/client/shared/vendor/react-dom-factories"), require("devtools/client/shared/vendor/lodash"), require("devtools/client/shared/vendor/react-prop-types"), require("devtools/client/shared/vendor/react")) : factory(root["devtools/client/shared/vendor/react-dom-factories"], root["devtools/client/shared/vendor/lodash"], root["devtools/client/shared/vendor/react-prop-types"], root["devtools/client/shared/vendor/react"]);
		for(var i in a) (typeof exports === 'object' ? exports : root)[i] = a[i];
	}
})(this, function(__WEBPACK_EXTERNAL_MODULE_1__, __WEBPACK_EXTERNAL_MODULE_53__, __WEBPACK_EXTERNAL_MODULE_2__, __WEBPACK_EXTERNAL_MODULE_6__) {
return /******/ (function(modules) { // webpackBootstrap
/******/ 	// The module cache
/******/ 	var installedModules = {};
/******/
/******/ 	// The require function
/******/ 	function __webpack_require__(moduleId) {
/******/
/******/ 		// Check if module is in cache
/******/ 		if(installedModules[moduleId]) {
/******/ 			return installedModules[moduleId].exports;
/******/ 		}
/******/ 		// Create a new module (and put it into the cache)
/******/ 		var module = installedModules[moduleId] = {
/******/ 			i: moduleId,
/******/ 			l: false,
/******/ 			exports: {}
/******/ 		};
/******/
/******/ 		// Execute the module function
/******/ 		modules[moduleId].call(module.exports, module, module.exports, __webpack_require__);
/******/
/******/ 		// Flag the module as loaded
/******/ 		module.l = true;
/******/
/******/ 		// Return the exports of the module
/******/ 		return module.exports;
/******/ 	}
/******/
/******/
/******/ 	// expose the modules object (__webpack_modules__)
/******/ 	__webpack_require__.m = modules;
/******/
/******/ 	// expose the module cache
/******/ 	__webpack_require__.c = installedModules;
/******/
/******/ 	// define getter function for harmony exports
/******/ 	__webpack_require__.d = function(exports, name, getter) {
/******/ 		if(!__webpack_require__.o(exports, name)) {
/******/ 			Object.defineProperty(exports, name, {
/******/ 				configurable: false,
/******/ 				enumerable: true,
/******/ 				get: getter
/******/ 			});
/******/ 		}
/******/ 	};
/******/
/******/ 	// getDefaultExport function for compatibility with non-harmony modules
/******/ 	__webpack_require__.n = function(module) {
/******/ 		var getter = module && module.__esModule ?
/******/ 			function getDefault() { return module['default']; } :
/******/ 			function getModuleExports() { return module; };
/******/ 		__webpack_require__.d(getter, 'a', getter);
/******/ 		return getter;
/******/ 	};
/******/
/******/ 	// Object.prototype.hasOwnProperty.call
/******/ 	__webpack_require__.o = function(object, property) { return Object.prototype.hasOwnProperty.call(object, property); };
/******/
/******/ 	// __webpack_public_path__
/******/ 	__webpack_require__.p = "/assets/build";
/******/
/******/ 	// Load entry module and return exports
/******/ 	return __webpack_require__(__webpack_require__.s = 17);
/******/ })
/************************************************************************/
/******/ ([
/* 0 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Dependencies
const validProtocols = /^(http|https|ftp|data|javascript|resource|chrome):/i;
const tokenSplitRegex = /(\s|\'|\"|\\)+/;
const ELLIPSIS = "\u2026";
const dom = __webpack_require__(1);
const { span } = dom;

/**
 * Returns true if the given object is a grip (see RDP protocol)
 */
function isGrip(object) {
  return object && object.actor;
}

function escapeNewLines(value) {
  return value.replace(/\r/gm, "\\r").replace(/\n/gm, "\\n");
}

// Map from character code to the corresponding escape sequence.  \0
// isn't here because it would require special treatment in some
// situations.  \b, \f, and \v aren't here because they aren't very
// common.  \' isn't here because there's no need, we only
// double-quote strings.
const escapeMap = {
  // Tab.
  9: "\\t",
  // Newline.
  0xa: "\\n",
  // Carriage return.
  0xd: "\\r",
  // Quote.
  0x22: "\\\"",
  // Backslash.
  0x5c: "\\\\"
};

// Regexp that matches any character we might possibly want to escape.
// Note that we over-match here, because it's difficult to, say, match
// an unpaired surrogate with a regexp.  The details are worked out by
// the replacement function; see |escapeString|.
const escapeRegexp = new RegExp("[" +
// Quote and backslash.
"\"\\\\" +
// Controls.
"\x00-\x1f" +
// More controls.
"\x7f-\x9f" +
// BOM
"\ufeff" +
// Specials, except for the replacement character.
"\ufff0-\ufffc\ufffe\uffff" +
// Surrogates.
"\ud800-\udfff" +
// Mathematical invisibles.
"\u2061-\u2064" +
// Line and paragraph separators.
"\u2028-\u2029" +
// Private use area.
"\ue000-\uf8ff" + "]", "g");

/**
 * Escape a string so that the result is viewable and valid JS.
 * Control characters, other invisibles, invalid characters,
 * backslash, and double quotes are escaped.  The resulting string is
 * surrounded by double quotes.
 *
 * @param {String} str
 *        the input
 * @param {Boolean} escapeWhitespace
 *        if true, TAB, CR, and NL characters will be escaped
 * @return {String} the escaped string
 */
function escapeString(str, escapeWhitespace) {
  return "\"" + str.replace(escapeRegexp, (match, offset) => {
    let c = match.charCodeAt(0);
    if (c in escapeMap) {
      if (!escapeWhitespace && (c === 9 || c === 0xa || c === 0xd)) {
        return match[0];
      }
      return escapeMap[c];
    }
    if (c >= 0xd800 && c <= 0xdfff) {
      // Find the full code point containing the surrogate, with a
      // special case for a trailing surrogate at the start of the
      // string.
      if (c >= 0xdc00 && offset > 0) {
        --offset;
      }
      let codePoint = str.codePointAt(offset);
      if (codePoint >= 0xd800 && codePoint <= 0xdfff) {
        // Unpaired surrogate.
        return "\\u" + codePoint.toString(16);
      } else if (codePoint >= 0xf0000 && codePoint <= 0x10fffd) {
        // Private use area.  Because we visit each pair of a such a
        // character, return the empty string for one half and the
        // real result for the other, to avoid duplication.
        if (c <= 0xdbff) {
          return "\\u{" + codePoint.toString(16) + "}";
        }
        return "";
      }
      // Other surrogate characters are passed through.
      return match;
    }
    return "\\u" + ("0000" + c.toString(16)).substr(-4);
  }) + "\"";
}

/**
 * Escape a property name, if needed.  "Escaping" in this context
 * means surrounding the property name with quotes.
 *
 * @param {String}
 *        name the property name
 * @return {String} either the input, or the input surrounded by
 *                  quotes, properly quoted in JS syntax.
 */
function maybeEscapePropertyName(name) {
  // Quote the property name if it needs quoting.  This particular
  // test is an approximation; see
  // https://mathiasbynens.be/notes/javascript-properties.  However,
  // the full solution requires a fair amount of Unicode data, and so
  // let's defer that until either it's important, or the \p regexp
  // syntax lands, see
  // https://github.com/tc39/proposal-regexp-unicode-property-escapes.
  if (!/^\w+$/.test(name)) {
    name = escapeString(name);
  }
  return name;
}

function cropMultipleLines(text, limit) {
  return escapeNewLines(cropString(text, limit));
}

function rawCropString(text, limit, alternativeText = ELLIPSIS) {
  // Crop the string only if a limit is actually specified.
  if (!limit || limit <= 0) {
    return text;
  }

  // Set the limit at least to the length of the alternative text
  // plus one character of the original text.
  if (limit <= alternativeText.length) {
    limit = alternativeText.length + 1;
  }

  let halfLimit = (limit - alternativeText.length) / 2;

  if (text.length > limit) {
    return text.substr(0, Math.ceil(halfLimit)) + alternativeText + text.substr(text.length - Math.floor(halfLimit));
  }

  return text;
}

function cropString(text, limit, alternativeText) {
  return rawCropString(sanitizeString(text + ""), limit, alternativeText);
}

function sanitizeString(text) {
  // Replace all non-printable characters, except of
  // (horizontal) tab (HT: \x09) and newline (LF: \x0A, CR: \x0D),
  // with unicode replacement character (u+fffd).
  // eslint-disable-next-line no-control-regex
  let re = new RegExp("[\x00-\x08\x0B\x0C\x0E-\x1F\x7F-\x9F]", "g");
  return text.replace(re, "\ufffd");
}

function parseURLParams(url) {
  url = new URL(url);
  return parseURLEncodedText(url.searchParams);
}

function parseURLEncodedText(text) {
  let params = [];

  // In case the text is empty just return the empty parameters
  if (text == "") {
    return params;
  }

  let searchParams = new URLSearchParams(text);
  let entries = [...searchParams.entries()];
  return entries.map(entry => {
    return {
      name: entry[0],
      value: entry[1]
    };
  });
}

function getFileName(url) {
  let split = splitURLBase(url);
  return split.name;
}

function splitURLBase(url) {
  if (!isDataURL(url)) {
    return splitURLTrue(url);
  }
  return {};
}

function getURLDisplayString(url) {
  return cropString(url);
}

function isDataURL(url) {
  return url && url.substr(0, 5) == "data:";
}

function splitURLTrue(url) {
  const reSplitFile = /(.*?):\/{2,3}([^\/]*)(.*?)([^\/]*?)($|\?.*)/;
  let m = reSplitFile.exec(url);

  if (!m) {
    return {
      name: url,
      path: url
    };
  } else if (m[4] == "" && m[5] == "") {
    return {
      protocol: m[1],
      domain: m[2],
      path: m[3],
      name: m[3] != "/" ? m[3] : m[2]
    };
  }

  return {
    protocol: m[1],
    domain: m[2],
    path: m[2] + m[3],
    name: m[4] + m[5]
  };
}

/**
 * Wrap the provided render() method of a rep in a try/catch block that will render a
 * fallback rep if the render fails.
 */
function wrapRender(renderMethod) {
  const wrappedFunction = function (props) {
    try {
      return renderMethod.call(this, props);
    } catch (e) {
      console.error(e);
      return span({
        className: "objectBox objectBox-failure",
        title: "This object could not be rendered, " + "please file a bug on bugzilla.mozilla.org"
      },
      /* Labels have to be hardcoded for reps, see Bug 1317038. */
      "Invalid object");
    }
  };
  wrappedFunction.propTypes = renderMethod.propTypes;
  return wrappedFunction;
}

/**
 * Get preview items from a Grip.
 *
 * @param {Object} Grip from which we want the preview items
 * @return {Array} Array of the preview items of the grip, or an empty array
 *                 if the grip does not have preview items
 */
function getGripPreviewItems(grip) {
  if (!grip) {
    return [];
  }

  // Promise resolved value Grip
  if (grip.promiseState && grip.promiseState.value) {
    return [grip.promiseState.value];
  }

  // Array Grip
  if (grip.preview && grip.preview.items) {
    return grip.preview.items;
  }

  // Node Grip
  if (grip.preview && grip.preview.childNodes) {
    return grip.preview.childNodes;
  }

  // Set or Map Grip
  if (grip.preview && grip.preview.entries) {
    return grip.preview.entries.reduce((res, entry) => res.concat(entry), []);
  }

  // Event Grip
  if (grip.preview && grip.preview.target) {
    let keys = Object.keys(grip.preview.properties);
    let values = Object.values(grip.preview.properties);
    return [grip.preview.target, ...keys, ...values];
  }

  // RegEx Grip
  if (grip.displayString) {
    return [grip.displayString];
  }

  // Generic Grip
  if (grip.preview && grip.preview.ownProperties) {
    let propertiesValues = Object.values(grip.preview.ownProperties).map(property => property.value || property);

    let propertyKeys = Object.keys(grip.preview.ownProperties);
    propertiesValues = propertiesValues.concat(propertyKeys);

    // ArrayBuffer Grip
    if (grip.preview.safeGetterValues) {
      propertiesValues = propertiesValues.concat(Object.values(grip.preview.safeGetterValues).map(property => property.getterValue || property));
    }

    return propertiesValues;
  }

  return [];
}

/**
 * Get the type of an object.
 *
 * @param {Object} Grip from which we want the type.
 * @param {boolean} noGrip true if the object is not a grip.
 * @return {boolean}
 */
function getGripType(object, noGrip) {
  let type = typeof object;
  if (type == "object" && object instanceof String) {
    type = "string";
  } else if (object && type == "object" && object.type && noGrip !== true) {
    type = object.type;
  }

  if (isGrip(object)) {
    type = object.class;
  }

  return type;
}

/**
 * Determines whether a grip is a string containing a URL.
 *
 * @param string grip
 *        The grip, which may contain a URL.
 * @return boolean
 *         Whether the grip is a string containing a URL.
 */
function containsURL(grip) {
  if (typeof grip !== "string") {
    return false;
  }

  let tokens = grip.split(tokenSplitRegex);
  return tokens.some(isURL);
}

/**
 * Determines whether a string token is a valid URL.
 *
 * @param string token
 *        The token.
 * @return boolean
 *         Whenther the token is a URL.
 */
function isURL(token) {
  try {
    if (!validProtocols.test(token)) {
      return false;
    }
    new URL(token);
    return true;
  } catch (e) {
    return false;
  }
}

module.exports = {
  isGrip,
  isURL,
  cropString,
  containsURL,
  rawCropString,
  sanitizeString,
  escapeString,
  wrapRender,
  cropMultipleLines,
  parseURLParams,
  parseURLEncodedText,
  getFileName,
  getURLDisplayString,
  maybeEscapePropertyName,
  getGripPreviewItems,
  getGripType,
  tokenSplitRegex,
  ELLIPSIS
};

/***/ }),
/* 1 */
/***/ (function(module, exports) {

module.exports = __WEBPACK_EXTERNAL_MODULE_1__;

/***/ }),
/* 2 */
/***/ (function(module, exports) {

module.exports = __WEBPACK_EXTERNAL_MODULE_2__;

/***/ }),
/* 3 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

module.exports = {
  MODE: {
    TINY: Symbol("TINY"),
    SHORT: Symbol("SHORT"),
    LONG: Symbol("LONG")
  }
};

/***/ }),
/* 4 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

__webpack_require__(18);

// Load all existing rep templates
const Undefined = __webpack_require__(19);
const Null = __webpack_require__(20);
const StringRep = __webpack_require__(7);
const LongStringRep = __webpack_require__(21);
const Number = __webpack_require__(22);
const ArrayRep = __webpack_require__(10);
const Obj = __webpack_require__(23);
const SymbolRep = __webpack_require__(24);
const InfinityRep = __webpack_require__(25);
const NaNRep = __webpack_require__(26);
const Accessor = __webpack_require__(27);

// DOM types (grips)
const Attribute = __webpack_require__(28);
const DateTime = __webpack_require__(29);
const Document = __webpack_require__(30);
const Event = __webpack_require__(31);
const Func = __webpack_require__(32);
const PromiseRep = __webpack_require__(36);
const RegExp = __webpack_require__(37);
const StyleSheet = __webpack_require__(38);
const CommentNode = __webpack_require__(39);
const ElementNode = __webpack_require__(40);
const TextNode = __webpack_require__(41);
const ErrorRep = __webpack_require__(42);
const Window = __webpack_require__(43);
const ObjectWithText = __webpack_require__(44);
const ObjectWithURL = __webpack_require__(45);
const GripArray = __webpack_require__(13);
const GripMap = __webpack_require__(14);
const GripMapEntry = __webpack_require__(15);
const Grip = __webpack_require__(8);

// List of all registered template.
// XXX there should be a way for extensions to register a new
// or modify an existing rep.
let reps = [RegExp, StyleSheet, Event, DateTime, CommentNode, ElementNode, TextNode, Attribute, LongStringRep, Func, PromiseRep, ArrayRep, Document, Window, ObjectWithText, ObjectWithURL, ErrorRep, GripArray, GripMap, GripMapEntry, Grip, Undefined, Null, StringRep, Number, SymbolRep, InfinityRep, NaNRep, Accessor];

/**
 * Generic rep that is using for rendering native JS types or an object.
 * The right template used for rendering is picked automatically according
 * to the current value type. The value must be passed is as 'object'
 * property.
 */
const Rep = function (props) {
  let {
    object,
    defaultRep
  } = props;
  let rep = getRep(object, defaultRep, props.noGrip);
  return rep(props);
};

// Helpers

/**
 * Return a rep object that is responsible for rendering given
 * object.
 *
 * @param object {Object} Object to be rendered in the UI. This
 * can be generic JS object as well as a grip (handle to a remote
 * debuggee object).
 *
 * @param defaultObject {React.Component} The default template
 * that should be used to render given object if none is found.
 *
 * @param noGrip {Boolean} If true, will only check reps not made for remote objects.
 */
function getRep(object, defaultRep = Obj, noGrip = false) {
  for (let i = 0; i < reps.length; i++) {
    let rep = reps[i];
    try {
      // supportsObject could return weight (not only true/false
      // but a number), which would allow to priorities templates and
      // support better extensibility.
      if (rep.supportsObject(object, noGrip)) {
        return rep.rep;
      }
    } catch (err) {
      console.error(err);
    }
  }

  return defaultRep.rep;
}

module.exports = {
  Rep,
  REPS: {
    Accessor,
    ArrayRep,
    Attribute,
    CommentNode,
    DateTime,
    Document,
    ElementNode,
    ErrorRep,
    Event,
    Func,
    Grip,
    GripArray,
    GripMap,
    GripMapEntry,
    InfinityRep,
    LongStringRep,
    NaNRep,
    Null,
    Number,
    Obj,
    ObjectWithText,
    ObjectWithURL,
    PromiseRep,
    RegExp,
    Rep,
    StringRep,
    StyleSheet,
    SymbolRep,
    TextNode,
    Undefined,
    Window
  },
  // Exporting for tests
  getRep
};

/***/ }),
/* 5 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Dependencies
const PropTypes = __webpack_require__(2);
const {
  maybeEscapePropertyName,
  wrapRender
} = __webpack_require__(0);
const { MODE } = __webpack_require__(3);

const dom = __webpack_require__(1);
const { span } = dom;

/**
 * Property for Obj (local JS objects), Grip (remote JS objects)
 * and GripMap (remote JS maps and weakmaps) reps.
 * It's used to render object properties.
 */
PropRep.propTypes = {
  // Property name.
  name: PropTypes.oneOfType([PropTypes.string, PropTypes.object]).isRequired,
  // Equal character rendered between property name and value.
  equal: PropTypes.string,
  // @TODO Change this to Object.values once it's supported in Node's version of V8
  mode: PropTypes.oneOf(Object.keys(MODE).map(key => MODE[key])),
  onDOMNodeMouseOver: PropTypes.func,
  onDOMNodeMouseOut: PropTypes.func,
  onInspectIconClick: PropTypes.func,
  // Normally a PropRep will quote a property name that isn't valid
  // when unquoted; but this flag can be used to suppress the
  // quoting.
  suppressQuotes: PropTypes.bool
};

/**
 * Function that given a name, a delimiter and an object returns an array
 * of React elements representing an object property (e.g. `name: value`)
 *
 * @param {Object} props
 * @return {Array} Array of React elements.
 */
function PropRep(props) {
  const Grip = __webpack_require__(8);
  const { Rep } = __webpack_require__(4);

  let {
    name,
    mode,
    equal,
    suppressQuotes
  } = props;

  let key;
  // The key can be a simple string, for plain objects,
  // or another object for maps and weakmaps.
  if (typeof name === "string") {
    if (!suppressQuotes) {
      name = maybeEscapePropertyName(name);
    }
    key = span({ "className": "nodeName" }, name);
  } else {
    key = Rep(Object.assign({}, props, {
      className: "nodeName",
      object: name,
      mode: mode || MODE.TINY,
      defaultRep: Grip
    }));
  }

  return [key, span({
    "className": "objectEqual"
  }, equal), Rep(Object.assign({}, props))];
}

// Exports from this module
module.exports = wrapRender(PropRep);

/***/ }),
/* 6 */
/***/ (function(module, exports) {

module.exports = __WEBPACK_EXTERNAL_MODULE_6__;

/***/ }),
/* 7 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Dependencies
const PropTypes = __webpack_require__(2);

const {
  containsURL,
  isURL,
  escapeString,
  getGripType,
  rawCropString,
  sanitizeString,
  wrapRender,
  tokenSplitRegex,
  ELLIPSIS
} = __webpack_require__(0);

const dom = __webpack_require__(1);
const { a, span } = dom;

/**
 * Renders a string. String value is enclosed within quotes.
 */
StringRep.propTypes = {
  useQuotes: PropTypes.bool,
  escapeWhitespace: PropTypes.bool,
  style: PropTypes.object,
  object: PropTypes.string.isRequired,
  member: PropTypes.any,
  cropLimit: PropTypes.number,
  openLink: PropTypes.func,
  className: PropTypes.string,
  omitLinkHref: PropTypes.bool
};

function StringRep(props) {
  let {
    className,
    cropLimit,
    object: text,
    member,
    style,
    useQuotes = true,
    escapeWhitespace = true,
    openLink,
    omitLinkHref = true
  } = props;

  const classNames = ["objectBox", "objectBox-string"];
  if (className) {
    classNames.push(className);
  }
  let config = { className: classNames.join(" ") };
  if (style) {
    config.style = style;
  }

  if (useQuotes) {
    text = escapeString(text, escapeWhitespace);
  } else {
    text = sanitizeString(text);
  }

  const shouldCrop = (!member || !member.open) && cropLimit;
  if (!containsURL(text)) {
    if (shouldCrop) {
      text = rawCropString(text, cropLimit);
    }
    return span(config, text);
  }

  return span(config, ...getLinkifiedElements(text, shouldCrop && cropLimit, omitLinkHref, openLink));
}

/**
 * Get an array of the elements representing the string, cropped if needed,
 * with actual links.
 *
 * @param {String} text: The actual string to linkify.
 * @param {Integer | null} cropLimit
 * @param {Boolean} omitLinkHref: Do not create an href attribute if true.
 * @param {Function} openLink: Function handling the link opening.
 * @returns {Array<String|ReactElement>}
 */
function getLinkifiedElements(text, cropLimit, omitLinkHref, openLink) {
  const halfLimit = Math.ceil((cropLimit - ELLIPSIS.length) / 2);
  const startCropIndex = halfLimit;
  const endCropIndex = text.length - halfLimit;

  // As we walk through the tokens of the source string, we make sure to preserve
  // the original whitespace that separated the tokens.
  let currentIndex = 0;
  const items = [];
  for (let token of text.split(tokenSplitRegex)) {
    if (isURL(token)) {
      // Let's grab all the non-url strings before the link.
      const tokenStart = text.indexOf(token, currentIndex);
      let nonUrlText = text.slice(currentIndex, tokenStart);
      nonUrlText = getCroppedString(nonUrlText, currentIndex, startCropIndex, endCropIndex);
      if (nonUrlText) {
        items.push(nonUrlText);
      }

      // Update the index to match the beginning of the token.
      currentIndex = tokenStart;

      let linkText = getCroppedString(token, currentIndex, startCropIndex, endCropIndex);
      if (linkText) {
        items.push(a({
          className: "url",
          title: token,
          href: omitLinkHref === true ? null : token,
          draggable: false,
          onClick: openLink ? e => {
            e.preventDefault();
            openLink(token);
          } : null
        }, linkText));
      }

      currentIndex = tokenStart + token.length;
    }
  }

  // Clean up any non-URL text at the end of the source string,
  // i.e. not handled in the loop.
  if (currentIndex !== text.length) {
    let nonUrlText = text.slice(currentIndex, text.length);
    if (currentIndex < endCropIndex) {
      const cutIndex = endCropIndex - currentIndex;
      nonUrlText = nonUrlText.substring(cutIndex);
    }
    items.push(nonUrlText);
  }

  return items;
}

/**
 * Returns a cropped substring given an offset, start and end crop indices in a parent
 * string.
 *
 * @param {String} text: The substring to crop.
 * @param {Integer} offset: The offset corresponding to the index at which the substring
 *                          is in the parent string.
 * @param {Integer} startCropIndex: the index where the start of the crop should happen
 *                                  in the parent string
 * @param {Integer} endCropIndex: the index where the end of the crop should happen
 *                                  in the parent string
 * @returns {String|null} The cropped substring, or null if the text is completly cropped.
 */
function getCroppedString(text, offset = 0, startCropIndex, endCropIndex) {
  const start = offset;
  const end = offset + text.length;

  const shouldBeVisible = !(start >= startCropIndex && end <= endCropIndex);
  if (!shouldBeVisible) {
    return null;
  }

  const shouldCropEnd = start < startCropIndex && end > startCropIndex;
  const shouldCropStart = start < endCropIndex && end > endCropIndex;
  if (shouldCropEnd) {
    const cutIndex = startCropIndex - start;
    return text.substring(0, cutIndex) + ELLIPSIS + (shouldCropStart ? text.substring(endCropIndex - start) : "");
  }

  if (shouldCropStart) {
    // The string should be cropped at the beginning.
    const cutIndex = endCropIndex - start;
    return text.substring(cutIndex);
  }

  return text;
}

function supportsObject(object, noGrip = false) {
  return getGripType(object, noGrip) == "string";
}

// Exports from this module

module.exports = {
  rep: wrapRender(StringRep),
  supportsObject
};

/***/ }),
/* 8 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// ReactJS
const PropTypes = __webpack_require__(2);

// Dependencies
const {
  isGrip,
  wrapRender
} = __webpack_require__(0);
const PropRep = __webpack_require__(5);
const { MODE } = __webpack_require__(3);

const dom = __webpack_require__(1);
const { span } = dom;

/**
 * Renders generic grip. Grip is client representation
 * of remote JS object and is used as an input object
 * for this rep component.
 */
GripRep.propTypes = {
  object: PropTypes.object.isRequired,
  // @TODO Change this to Object.values once it's supported in Node's version of V8
  mode: PropTypes.oneOf(Object.keys(MODE).map(key => MODE[key])),
  isInterestingProp: PropTypes.func,
  title: PropTypes.string,
  onDOMNodeMouseOver: PropTypes.func,
  onDOMNodeMouseOut: PropTypes.func,
  onInspectIconClick: PropTypes.func,
  noGrip: PropTypes.bool
};

const DEFAULT_TITLE = "Object";

function GripRep(props) {
  let {
    mode = MODE.SHORT,
    object
  } = props;

  const config = {
    "data-link-actor-id": object.actor,
    className: "objectBox objectBox-object"
  };

  if (mode === MODE.TINY) {
    let propertiesLength = getPropertiesLength(object);

    const tinyModeItems = [];
    if (getTitle(props, object) !== DEFAULT_TITLE) {
      tinyModeItems.push(getTitleElement(props, object));
    } else {
      tinyModeItems.push(span({
        className: "objectLeftBrace"
      }, "{"), propertiesLength > 0 ? span({
        key: "more",
        className: "more-ellipsis",
        title: "more…"
      }, "…") : null, span({
        className: "objectRightBrace"
      }, "}"));
    }

    return span(config, ...tinyModeItems);
  }

  let propsArray = safePropIterator(props, object, maxLengthMap.get(mode));

  return span(config, getTitleElement(props, object), span({
    className: "objectLeftBrace"
  }, " { "), ...propsArray, span({
    className: "objectRightBrace"
  }, " }"));
}

function getTitleElement(props, object) {
  return span({
    className: "objectTitle"
  }, getTitle(props, object));
}

function getTitle(props, object) {
  return props.title || object.class || DEFAULT_TITLE;
}

function getPropertiesLength(object) {
  let propertiesLength = object.preview && object.preview.ownPropertiesLength ? object.preview.ownPropertiesLength : object.ownPropertyLength;

  if (object.preview && object.preview.safeGetterValues) {
    propertiesLength += Object.keys(object.preview.safeGetterValues).length;
  }

  if (object.preview && object.preview.ownSymbols) {
    propertiesLength += object.preview.ownSymbolsLength;
  }

  return propertiesLength;
}

function safePropIterator(props, object, max) {
  max = typeof max === "undefined" ? maxLengthMap.get(MODE.SHORT) : max;
  try {
    return propIterator(props, object, max);
  } catch (err) {
    console.error(err);
  }
  return [];
}

function propIterator(props, object, max) {
  if (object.preview && Object.keys(object.preview).includes("wrappedValue")) {
    const { Rep } = __webpack_require__(4);

    return [Rep({
      object: object.preview.wrappedValue,
      mode: props.mode || MODE.TINY,
      defaultRep: Grip
    })];
  }

  // Property filter. Show only interesting properties to the user.
  let isInterestingProp = props.isInterestingProp || ((type, value) => {
    return type == "boolean" || type == "number" || type == "string" && value.length != 0;
  });

  let properties = object.preview ? object.preview.ownProperties : {};
  let propertiesLength = getPropertiesLength(object);

  if (object.preview && object.preview.safeGetterValues) {
    properties = Object.assign({}, properties, object.preview.safeGetterValues);
  }

  let indexes = getPropIndexes(properties, max, isInterestingProp);
  if (indexes.length < max && indexes.length < propertiesLength) {
    // There are not enough props yet. Then add uninteresting props to display them.
    indexes = indexes.concat(getPropIndexes(properties, max - indexes.length, (t, value, name) => {
      return !isInterestingProp(t, value, name);
    }));
  }

  // The server synthesizes some property names for a Proxy, like
  // <target> and <handler>; we don't want to quote these because,
  // as synthetic properties, they appear more natural when
  // unquoted.
  const suppressQuotes = object.class === "Proxy";
  let propsArray = getProps(props, properties, indexes, suppressQuotes);

  // Show symbols.
  if (object.preview && object.preview.ownSymbols) {
    const { ownSymbols } = object.preview;
    const length = max - indexes.length;

    const symbolsProps = ownSymbols.slice(0, length).map(symbolItem => {
      return PropRep(Object.assign({}, props, {
        mode: MODE.TINY,
        name: symbolItem,
        object: symbolItem.descriptor.value,
        equal: ": ",
        defaultRep: Grip,
        title: null,
        suppressQuotes
      }));
    });

    propsArray.push(...symbolsProps);
  }

  if (Object.keys(properties).length > max || propertiesLength > max
  // When the object has non-enumerable properties, we don't have them in the packet,
  // but we might want to show there's something in the object.
  || propertiesLength > propsArray.length) {
    // There are some undisplayed props. Then display "more...".
    propsArray.push(span({
      key: "more",
      className: "more-ellipsis",
      title: "more…"
    }, "…"));
  }

  return unfoldProps(propsArray);
}

function unfoldProps(items) {
  return items.reduce((res, item, index) => {
    if (Array.isArray(item)) {
      res = res.concat(item);
    } else {
      res.push(item);
    }

    // Interleave commas between elements
    if (index !== items.length - 1) {
      res.push(", ");
    }
    return res;
  }, []);
}

/**
 * Get props ordered by index.
 *
 * @param {Object} componentProps Grip Component props.
 * @param {Object} properties Properties of the object the Grip describes.
 * @param {Array} indexes Indexes of properties.
 * @param {Boolean} suppressQuotes true if we should suppress quotes
 *                  on property names.
 * @return {Array} Props.
 */
function getProps(componentProps, properties, indexes, suppressQuotes) {
  // Make indexes ordered by ascending.
  indexes.sort(function (a, b) {
    return a - b;
  });

  const propertiesKeys = Object.keys(properties);
  return indexes.map(i => {
    let name = propertiesKeys[i];
    let value = getPropValue(properties[name]);

    return PropRep(Object.assign({}, componentProps, {
      mode: MODE.TINY,
      name,
      object: value,
      equal: ": ",
      defaultRep: Grip,
      title: null,
      suppressQuotes
    }));
  });
}

/**
 * Get the indexes of props in the object.
 *
 * @param {Object} properties Props object.
 * @param {Number} max The maximum length of indexes array.
 * @param {Function} filter Filter the props you want.
 * @return {Array} Indexes of interesting props in the object.
 */
function getPropIndexes(properties, max, filter) {
  let indexes = [];

  try {
    let i = 0;
    for (let name in properties) {
      if (indexes.length >= max) {
        return indexes;
      }

      // Type is specified in grip's "class" field and for primitive
      // values use typeof.
      let value = getPropValue(properties[name]);
      let type = value.class || typeof value;
      type = type.toLowerCase();

      if (filter(type, value, name)) {
        indexes.push(i);
      }
      i++;
    }
  } catch (err) {
    console.error(err);
  }
  return indexes;
}

/**
 * Get the actual value of a property.
 *
 * @param {Object} property
 * @return {Object} Value of the property.
 */
function getPropValue(property) {
  let value = property;
  if (typeof property === "object") {
    let keys = Object.keys(property);
    if (keys.includes("value")) {
      value = property.value;
    } else if (keys.includes("getterValue")) {
      value = property.getterValue;
    }
  }
  return value;
}

// Registration
function supportsObject(object, noGrip = false) {
  if (noGrip === true || !isGrip(object)) {
    return false;
  }

  return object.preview ? typeof object.preview.ownProperties !== "undefined" : typeof object.ownPropertyLength !== "undefined";
}

const maxLengthMap = new Map();
maxLengthMap.set(MODE.SHORT, 3);
maxLengthMap.set(MODE.LONG, 10);

// Grip is used in propIterator and has to be defined here.
let Grip = {
  rep: wrapRender(GripRep),
  supportsObject,
  maxLengthMap
};

// Exports from this module
module.exports = Grip;

/***/ }),
/* 9 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


var _svgInlineReact = __webpack_require__(11);

var _svgInlineReact2 = _interopRequireDefault(_svgInlineReact);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const React = __webpack_require__(6);
const PropTypes = __webpack_require__(2);


const svg = {
  "open-inspector": __webpack_require__(34),
  "jump-definition": __webpack_require__(35)
};

Svg.propTypes = {
  className: PropTypes.string
};

function Svg(name, props) {
  if (!svg[name]) {
    throw new Error("Unknown SVG: " + name);
  }
  let className = name;
  if (props && props.className) {
    className = `${name} ${props.className}`;
  }
  if (name === "subSettings") {
    className = "";
  }
  props = Object.assign({}, props, { className, src: svg[name] });
  return React.createElement(_svgInlineReact2.default, props);
}

module.exports = Svg;

/***/ }),
/* 10 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Dependencies
const dom = __webpack_require__(1);
const PropTypes = __webpack_require__(2);
const {
  wrapRender
} = __webpack_require__(0);
const { MODE } = __webpack_require__(3);
const { span } = dom;

const ModePropType = PropTypes.oneOf(
// @TODO Change this to Object.values once it's supported in Node's version of V8
Object.keys(MODE).map(key => MODE[key]));

/**
 * Renders an array. The array is enclosed by left and right bracket
 * and the max number of rendered items depends on the current mode.
 */
ArrayRep.propTypes = {
  mode: ModePropType,
  object: PropTypes.array.isRequired
};

function ArrayRep(props) {
  let {
    object,
    mode = MODE.SHORT
  } = props;

  let items;
  let brackets;
  let needSpace = function (space) {
    return space ? { left: "[ ", right: " ]" } : { left: "[", right: "]" };
  };

  if (mode === MODE.TINY) {
    let isEmpty = object.length === 0;
    if (isEmpty) {
      items = [];
    } else {
      items = [span({
        className: "more-ellipsis",
        title: "more…"
      }, "…")];
    }
    brackets = needSpace(false);
  } else {
    items = arrayIterator(props, object, maxLengthMap.get(mode));
    brackets = needSpace(items.length > 0);
  }

  return span({
    className: "objectBox objectBox-array" }, span({
    className: "arrayLeftBracket"
  }, brackets.left), ...items, span({
    className: "arrayRightBracket"
  }, brackets.right), span({
    className: "arrayProperties",
    role: "group" }));
}

function arrayIterator(props, array, max) {
  let items = [];

  for (let i = 0; i < array.length && i < max; i++) {
    let config = {
      mode: MODE.TINY,
      delim: i == array.length - 1 ? "" : ", "
    };
    let item;

    try {
      item = ItemRep(Object.assign({}, props, config, {
        object: array[i]
      }));
    } catch (exc) {
      item = ItemRep(Object.assign({}, props, config, {
        object: exc
      }));
    }
    items.push(item);
  }

  if (array.length > max) {
    items.push(span({
      className: "more-ellipsis",
      title: "more…"
    }, "…"));
  }

  return items;
}

/**
 * Renders array item. Individual values are separated by a comma.
 */
ItemRep.propTypes = {
  object: PropTypes.any.isRequired,
  delim: PropTypes.string.isRequired,
  mode: ModePropType
};

function ItemRep(props) {
  const { Rep } = __webpack_require__(4);

  let {
    object,
    delim,
    mode
  } = props;
  return span({}, Rep(Object.assign({}, props, {
    object: object,
    mode: mode
  })), delim);
}

function getLength(object) {
  return object.length;
}

function supportsObject(object) {
  return Array.isArray(object) || Object.prototype.toString.call(object) === "[object Arguments]";
}

const maxLengthMap = new Map();
maxLengthMap.set(MODE.SHORT, 3);
maxLengthMap.set(MODE.LONG, 10);

// Exports from this module
module.exports = {
  rep: wrapRender(ArrayRep),
  supportsObject,
  maxLengthMap,
  getLength
};

/***/ }),
/* 11 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


Object.defineProperty(exports, "__esModule", {
    value: true
});

var _extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; };

var _createClass = function () { function defineProperties(target, props) { for (var i = 0; i < props.length; i++) { var descriptor = props[i]; descriptor.enumerable = descriptor.enumerable || false; descriptor.configurable = true; if ("value" in descriptor) descriptor.writable = true; Object.defineProperty(target, descriptor.key, descriptor); } } return function (Constructor, protoProps, staticProps) { if (protoProps) defineProperties(Constructor.prototype, protoProps); if (staticProps) defineProperties(Constructor, staticProps); return Constructor; }; }();

var _react = __webpack_require__(6);

var _react2 = _interopRequireDefault(_react);

var _propTypes = __webpack_require__(2);

var _util = __webpack_require__(33);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function _objectWithoutProperties(obj, keys) { var target = {}; for (var i in obj) { if (keys.indexOf(i) >= 0) continue; if (!Object.prototype.hasOwnProperty.call(obj, i)) continue; target[i] = obj[i]; } return target; }

function _classCallCheck(instance, Constructor) { if (!(instance instanceof Constructor)) { throw new TypeError("Cannot call a class as a function"); } }

function _possibleConstructorReturn(self, call) { if (!self) { throw new ReferenceError("this hasn't been initialised - super() hasn't been called"); } return call && (typeof call === "object" || typeof call === "function") ? call : self; }

function _inherits(subClass, superClass) { if (typeof superClass !== "function" && superClass !== null) { throw new TypeError("Super expression must either be null or a function, not " + typeof superClass); } subClass.prototype = Object.create(superClass && superClass.prototype, { constructor: { value: subClass, enumerable: false, writable: true, configurable: true } }); if (superClass) Object.setPrototypeOf ? Object.setPrototypeOf(subClass, superClass) : subClass.__proto__ = superClass; }

var process = process || { env: {} };

var InlineSVG = function (_React$Component) {
    _inherits(InlineSVG, _React$Component);

    function InlineSVG() {
        _classCallCheck(this, InlineSVG);

        return _possibleConstructorReturn(this, (InlineSVG.__proto__ || Object.getPrototypeOf(InlineSVG)).apply(this, arguments));
    }

    _createClass(InlineSVG, [{
        key: 'componentWillReceiveProps',
        value: function componentWillReceiveProps(_ref) {
            var children = _ref.children;

            if ("production" !== process.env.NODE_ENV && children != null) {
                console.info('<InlineSVG />: `children` prop will be ignored.');
            }
        }
    }, {
        key: 'render',
        value: function render() {
            var Element = void 0,
                __html = void 0,
                svgProps = void 0;

            var _props = this.props,
                element = _props.element,
                raw = _props.raw,
                src = _props.src,
                otherProps = _objectWithoutProperties(_props, ['element', 'raw', 'src']);

            if (raw === true) {
                Element = 'svg';
                svgProps = (0, _util.extractSVGProps)(src);
                __html = (0, _util.getSVGFromSource)(src).innerHTML;
            }
            __html = __html || src;
            Element = Element || element;
            svgProps = svgProps || {};

            return _react2.default.createElement(Element, _extends({}, svgProps, otherProps, { src: null, children: null,
                dangerouslySetInnerHTML: { __html: __html } }));
        }
    }]);

    return InlineSVG;
}(_react2.default.Component);

exports.default = InlineSVG;


InlineSVG.defaultProps = {
    element: 'i',
    raw: false,
    src: ''
};

InlineSVG.propTypes = {
    src: _propTypes.string.isRequired,
    element: _propTypes.string,
    raw: _propTypes.bool
};

/***/ }),
/* 12 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

module.exports = {
  ELEMENT_NODE: 1,
  ATTRIBUTE_NODE: 2,
  TEXT_NODE: 3,
  CDATA_SECTION_NODE: 4,
  ENTITY_REFERENCE_NODE: 5,
  ENTITY_NODE: 6,
  PROCESSING_INSTRUCTION_NODE: 7,
  COMMENT_NODE: 8,
  DOCUMENT_NODE: 9,
  DOCUMENT_TYPE_NODE: 10,
  DOCUMENT_FRAGMENT_NODE: 11,
  NOTATION_NODE: 12,

  // DocumentPosition
  DOCUMENT_POSITION_DISCONNECTED: 0x01,
  DOCUMENT_POSITION_PRECEDING: 0x02,
  DOCUMENT_POSITION_FOLLOWING: 0x04,
  DOCUMENT_POSITION_CONTAINS: 0x08,
  DOCUMENT_POSITION_CONTAINED_BY: 0x10,
  DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC: 0x20
};

/***/ }),
/* 13 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Dependencies
const PropTypes = __webpack_require__(2);
const {
  getGripType,
  isGrip,
  wrapRender
} = __webpack_require__(0);
const { MODE } = __webpack_require__(3);

const dom = __webpack_require__(1);
const { span } = dom;

/**
 * Renders an array. The array is enclosed by left and right bracket
 * and the max number of rendered items depends on the current mode.
 */
GripArray.propTypes = {
  object: PropTypes.object.isRequired,
  // @TODO Change this to Object.values once it's supported in Node's version of V8
  mode: PropTypes.oneOf(Object.keys(MODE).map(key => MODE[key])),
  provider: PropTypes.object,
  onDOMNodeMouseOver: PropTypes.func,
  onDOMNodeMouseOut: PropTypes.func,
  onInspectIconClick: PropTypes.func
};

function GripArray(props) {
  let {
    object,
    mode = MODE.SHORT
  } = props;

  let items;
  let brackets;
  let needSpace = function (space) {
    return space ? { left: "[ ", right: " ]" } : { left: "[", right: "]" };
  };

  if (mode === MODE.TINY) {
    let objectLength = getLength(object);
    let isEmpty = objectLength === 0;
    if (isEmpty) {
      items = [];
    } else {
      items = [span({
        className: "more-ellipsis",
        title: "more…"
      }, "…")];
    }
    brackets = needSpace(false);
  } else {
    let max = maxLengthMap.get(mode);
    items = arrayIterator(props, object, max);
    brackets = needSpace(items.length > 0);
  }

  let title = getTitle(props, object);

  return span({
    "data-link-actor-id": object.actor,
    className: "objectBox objectBox-array" }, title, span({
    className: "arrayLeftBracket"
  }, brackets.left), ...interleaveCommas(items), span({
    className: "arrayRightBracket"
  }, brackets.right), span({
    className: "arrayProperties",
    role: "group" }));
}

function interleaveCommas(items) {
  return items.reduce((res, item, index) => {
    if (index !== items.length - 1) {
      return res.concat(item, ", ");
    }
    return res.concat(item);
  }, []);
}

function getLength(grip) {
  if (!grip.preview) {
    return 0;
  }

  return grip.preview.length || grip.preview.childNodesLength || 0;
}

function getTitle(props, object) {
  if (props.mode === MODE.TINY) {
    return "";
  }

  let title = props.title || object.class || "Array";
  return span({
    className: "objectTitle"
  }, title + " ");
}

function getPreviewItems(grip) {
  if (!grip.preview) {
    return null;
  }

  return grip.preview.items || grip.preview.childNodes || [];
}

function arrayIterator(props, grip, max) {
  let { Rep } = __webpack_require__(4);

  let items = [];
  const gripLength = getLength(grip);

  if (!gripLength) {
    return items;
  }

  const previewItems = getPreviewItems(grip);
  let provider = props.provider;

  let emptySlots = 0;
  let foldedEmptySlots = 0;
  items = previewItems.reduce((res, itemGrip) => {
    if (res.length >= max) {
      return res;
    }

    let object;
    try {
      if (!provider && itemGrip === null) {
        emptySlots++;
        return res;
      }

      object = provider ? provider.getValue(itemGrip) : itemGrip;
    } catch (exc) {
      object = exc;
    }

    if (emptySlots > 0) {
      res.push(getEmptySlotsElement(emptySlots));
      foldedEmptySlots = foldedEmptySlots + emptySlots - 1;
      emptySlots = 0;
    }

    if (res.length < max) {
      res.push(Rep(Object.assign({}, props, {
        object,
        mode: MODE.TINY,
        // Do not propagate title to array items reps
        title: undefined
      })));
    }

    return res;
  }, []);

  // Handle trailing empty slots if there are some.
  if (items.length < max && emptySlots > 0) {
    items.push(getEmptySlotsElement(emptySlots));
    foldedEmptySlots = foldedEmptySlots + emptySlots - 1;
  }

  const itemsShown = items.length + foldedEmptySlots;
  if (gripLength > itemsShown) {
    items.push(span({
      className: "more-ellipsis",
      title: "more…"
    }, "…"));
  }

  return items;
}

function getEmptySlotsElement(number) {
  // TODO: Use l10N - See https://github.com/devtools-html/reps/issues/141
  return `<${number} empty slot${number > 1 ? "s" : ""}>`;
}

function supportsObject(grip, noGrip = false) {
  if (noGrip === true || !isGrip(grip)) {
    return false;
  }

  return grip.preview && (grip.preview.kind == "ArrayLike" || getGripType(grip, noGrip) === "DocumentFragment");
}

const maxLengthMap = new Map();
maxLengthMap.set(MODE.SHORT, 3);
maxLengthMap.set(MODE.LONG, 10);

// Exports from this module
module.exports = {
  rep: wrapRender(GripArray),
  supportsObject,
  maxLengthMap,
  getLength
};

/***/ }),
/* 14 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Dependencies
const PropTypes = __webpack_require__(2);
const {
  isGrip,
  wrapRender
} = __webpack_require__(0);
const PropRep = __webpack_require__(5);
const { MODE } = __webpack_require__(3);

const dom = __webpack_require__(1);
const { span } = dom;

/**
 * Renders an map. A map is represented by a list of its
 * entries enclosed in curly brackets.
 */
GripMap.propTypes = {
  object: PropTypes.object,
  // @TODO Change this to Object.values once it's supported in Node's version of V8
  mode: PropTypes.oneOf(Object.keys(MODE).map(key => MODE[key])),
  isInterestingEntry: PropTypes.func,
  onDOMNodeMouseOver: PropTypes.func,
  onDOMNodeMouseOut: PropTypes.func,
  onInspectIconClick: PropTypes.func,
  title: PropTypes.string
};

function GripMap(props) {
  let {
    mode,
    object
  } = props;

  const config = {
    "data-link-actor-id": object.actor,
    className: "objectBox objectBox-object"
  };

  if (mode === MODE.TINY) {
    return span(config, getTitle(props, object));
  }

  let propsArray = safeEntriesIterator(props, object, maxLengthMap.get(mode));

  return span(config, getTitle(props, object), span({
    className: "objectLeftBrace"
  }, " { "), ...propsArray, span({
    className: "objectRightBrace"
  }, " }"));
}

function getTitle(props, object) {
  let title = props.title || (object && object.class ? object.class : "Map");
  return span({
    className: "objectTitle"
  }, title);
}

function safeEntriesIterator(props, object, max) {
  max = typeof max === "undefined" ? 3 : max;
  try {
    return entriesIterator(props, object, max);
  } catch (err) {
    console.error(err);
  }
  return [];
}

function entriesIterator(props, object, max) {
  // Entry filter. Show only interesting entries to the user.
  let isInterestingEntry = props.isInterestingEntry || ((type, value) => {
    return type == "boolean" || type == "number" || type == "string" && value.length != 0;
  });

  let mapEntries = object.preview && object.preview.entries ? object.preview.entries : [];

  let indexes = getEntriesIndexes(mapEntries, max, isInterestingEntry);
  if (indexes.length < max && indexes.length < mapEntries.length) {
    // There are not enough entries yet, so we add uninteresting entries.
    indexes = indexes.concat(getEntriesIndexes(mapEntries, max - indexes.length, (t, value, name) => {
      return !isInterestingEntry(t, value, name);
    }));
  }

  let entries = getEntries(props, mapEntries, indexes);
  if (entries.length < getLength(object)) {
    // There are some undisplayed entries. Then display "…".
    entries.push(span({
      key: "more",
      className: "more-ellipsis",
      title: "more…"
    }, "…"));
  }

  return unfoldEntries(entries);
}

function unfoldEntries(items) {
  return items.reduce((res, item, index) => {
    if (Array.isArray(item)) {
      res = res.concat(item);
    } else {
      res.push(item);
    }

    // Interleave commas between elements
    if (index !== items.length - 1) {
      res.push(", ");
    }
    return res;
  }, []);
}

/**
 * Get entries ordered by index.
 *
 * @param {Object} props Component props.
 * @param {Array} entries Entries array.
 * @param {Array} indexes Indexes of entries.
 * @return {Array} Array of PropRep.
 */
function getEntries(props, entries, indexes) {
  let {
    onDOMNodeMouseOver,
    onDOMNodeMouseOut,
    onInspectIconClick
  } = props;

  // Make indexes ordered by ascending.
  indexes.sort(function (a, b) {
    return a - b;
  });

  return indexes.map((index, i) => {
    let [key, entryValue] = entries[index];
    let value = entryValue.value !== undefined ? entryValue.value : entryValue;

    return PropRep({
      name: key,
      equal: " \u2192 ",
      object: value,
      mode: MODE.TINY,
      onDOMNodeMouseOver,
      onDOMNodeMouseOut,
      onInspectIconClick
    });
  });
}

/**
 * Get the indexes of entries in the map.
 *
 * @param {Array} entries Entries array.
 * @param {Number} max The maximum length of indexes array.
 * @param {Function} filter Filter the entry you want.
 * @return {Array} Indexes of filtered entries in the map.
 */
function getEntriesIndexes(entries, max, filter) {
  return entries.reduce((indexes, [key, entry], i) => {
    if (indexes.length < max) {
      let value = entry && entry.value !== undefined ? entry.value : entry;
      // Type is specified in grip's "class" field and for primitive
      // values use typeof.
      let type = (value && value.class ? value.class : typeof value).toLowerCase();

      if (filter(type, value, key)) {
        indexes.push(i);
      }
    }

    return indexes;
  }, []);
}

function getLength(grip) {
  return grip.preview.size || 0;
}

function supportsObject(grip, noGrip = false) {
  if (noGrip === true || !isGrip(grip)) {
    return false;
  }
  return grip.preview && grip.preview.kind == "MapLike";
}

const maxLengthMap = new Map();
maxLengthMap.set(MODE.SHORT, 3);
maxLengthMap.set(MODE.LONG, 10);

// Exports from this module
module.exports = {
  rep: wrapRender(GripMap),
  supportsObject,
  maxLengthMap,
  getLength
};

/***/ }),
/* 15 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Dependencies
const PropTypes = __webpack_require__(2);
// Shortcuts
const dom = __webpack_require__(1);
const { span } = dom;
const {
  wrapRender
} = __webpack_require__(0);
const PropRep = __webpack_require__(5);
const { MODE } = __webpack_require__(3);
/**
 * Renders an map entry. A map entry is represented by its key, a column and its value.
 */
GripMapEntry.propTypes = {
  object: PropTypes.object,
  // @TODO Change this to Object.values once it's supported in Node's version of V8
  mode: PropTypes.oneOf(Object.keys(MODE).map(key => MODE[key])),
  onDOMNodeMouseOver: PropTypes.func,
  onDOMNodeMouseOut: PropTypes.func,
  onInspectIconClick: PropTypes.func
};

function GripMapEntry(props) {
  const {
    object
  } = props;

  const {
    key,
    value
  } = object.preview;

  return span({
    className: "objectBox objectBox-map-entry"
  }, ...PropRep(Object.assign({}, props, {
    name: key,
    object: value,
    equal: " \u2192 ",
    title: null,
    suppressQuotes: false
  })));
}

function supportsObject(grip, noGrip = false) {
  if (noGrip === true) {
    return false;
  }
  return grip && grip.type === "mapEntry" && grip.preview;
}

function createGripMapEntry(key, value) {
  return {
    type: "mapEntry",
    preview: {
      key,
      value
    }
  };
}

// Exports from this module
module.exports = {
  rep: wrapRender(GripMapEntry),
  createGripMapEntry,
  supportsObject
};

/***/ }),
/* 16 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { get, has } = __webpack_require__(53);
const { maybeEscapePropertyName } = __webpack_require__(0);
const ArrayRep = __webpack_require__(10);
const GripArrayRep = __webpack_require__(13);
const GripMap = __webpack_require__(14);
const GripMapEntryRep = __webpack_require__(15);

const MAX_NUMERICAL_PROPERTIES = 100;

const NODE_TYPES = {
  BUCKET: Symbol("[n…n]"),
  DEFAULT_PROPERTIES: Symbol("[default properties]"),
  ENTRIES: Symbol("<entries>"),
  GET: Symbol("<get>"),
  GRIP: Symbol("GRIP"),
  MAP_ENTRY_KEY: Symbol("<key>"),
  MAP_ENTRY_VALUE: Symbol("<value>"),
  PROMISE_REASON: Symbol("<reason>"),
  PROMISE_STATE: Symbol("<state>"),
  PROMISE_VALUE: Symbol("<value>"),
  PROXY_HANDLER: Symbol("<handler>"),
  PROXY_TARGET: Symbol("<target>"),
  SET: Symbol("<set>"),
  PROTOTYPE: Symbol("__proto__")
};

let WINDOW_PROPERTIES = {};

if (typeof window === "object") {
  WINDOW_PROPERTIES = Object.getOwnPropertyNames(window);
}

const SAFE_PATH_PREFIX = "##-";

function getType(item) {
  return item.type;
}

function getValue(item) {
  if (has(item, "contents.value")) {
    return get(item, "contents.value");
  }

  if (has(item, "contents.getterValue")) {
    return get(item, "contents.getterValue", undefined);
  }

  if (nodeHasAccessors(item)) {
    return item.contents;
  }

  return undefined;
}

function nodeIsBucket(item) {
  return getType(item) === NODE_TYPES.BUCKET;
}

function nodeIsEntries(item) {
  return getType(item) === NODE_TYPES.ENTRIES;
}

function nodeIsMapEntry(item) {
  return GripMapEntryRep.supportsObject(getValue(item));
}

function nodeHasChildren(item) {
  return Array.isArray(item.contents);
}

function nodeIsObject(item) {
  const value = getValue(item);
  return value && value.type === "object";
}

function nodeIsArrayLike(item) {
  const value = getValue(item);
  return GripArrayRep.supportsObject(value) || ArrayRep.supportsObject(value);
}

function nodeIsFunction(item) {
  const value = getValue(item);
  return value && value.class === "Function";
}

function nodeIsOptimizedOut(item) {
  const value = getValue(item);
  return !nodeHasChildren(item) && value && value.optimizedOut;
}

function nodeIsMissingArguments(item) {
  const value = getValue(item);
  return !nodeHasChildren(item) && value && value.missingArguments;
}

function nodeHasProperties(item) {
  return !nodeHasChildren(item) && nodeIsObject(item);
}

function nodeIsPrimitive(item) {
  return !nodeHasChildren(item) && !nodeHasProperties(item) && !nodeIsEntries(item) && !nodeIsMapEntry(item) && !nodeHasAccessors(item) && !nodeIsBucket(item);
}

function nodeIsDefaultProperties(item) {
  return getType(item) === NODE_TYPES.DEFAULT_PROPERTIES;
}

function isDefaultWindowProperty(name) {
  return WINDOW_PROPERTIES.includes(name);
}

function nodeIsPromise(item) {
  const value = getValue(item);
  if (!value) {
    return false;
  }

  return value.class == "Promise";
}

function nodeIsProxy(item) {
  const value = getValue(item);
  if (!value) {
    return false;
  }

  return value.class == "Proxy";
}

function nodeIsPrototype(item) {
  return getType(item) === NODE_TYPES.PROTOTYPE;
}

function nodeIsWindow(item) {
  const value = getValue(item);
  if (!value) {
    return false;
  }

  return value.class == "Window";
}

function nodeIsGetter(item) {
  return getType(item) === NODE_TYPES.GET;
}

function nodeIsSetter(item) {
  return getType(item) === NODE_TYPES.SET;
}

function nodeHasAccessors(item) {
  return !!getNodeGetter(item) || !!getNodeSetter(item);
}

function nodeSupportsNumericalBucketing(item) {
  // We exclude elements with entries since it's the <entries> node
  // itself that can have buckets.
  return nodeIsArrayLike(item) && !nodeHasEntries(item) || nodeIsEntries(item) || nodeIsBucket(item);
}

function nodeHasEntries(item) {
  const value = getValue(item);
  if (!value) {
    return false;
  }

  return value.class === "Map" || value.class === "Set" || value.class === "WeakMap" || value.class === "WeakSet";
}

function nodeHasAllEntriesInPreview(item) {
  const { preview } = getValue(item) || {};
  if (!preview) {
    return false;
  }

  const {
    entries,
    items,
    length,
    size
  } = preview;

  if (!entries && !items) {
    return false;
  }

  return entries ? entries.length === size : items.length === length;
}

function nodeNeedsNumericalBuckets(item) {
  return nodeSupportsNumericalBucketing(item) && getNumericalPropertiesCount(item) > MAX_NUMERICAL_PROPERTIES;
}

function makeNodesForPromiseProperties(item) {
  const { promiseState: { reason, value, state } } = getValue(item);

  const properties = [];

  if (state) {
    properties.push(createNode(item, "<state>", `${item.path}/${SAFE_PATH_PREFIX}state`, { value: state }, NODE_TYPES.PROMISE_STATE));
  }

  if (reason) {
    properties.push(createNode(item, "<reason>", `${item.path}/${SAFE_PATH_PREFIX}reason`, { value: reason }, NODE_TYPES.PROMISE_REASON));
  }

  if (value) {
    properties.push(createNode(item, "<value>", `${item.path}/${SAFE_PATH_PREFIX}value`, { value: value }, NODE_TYPES.PROMISE_VALUE));
  }

  return properties;
}

function makeNodesForProxyProperties(item) {
  const {
    proxyHandler,
    proxyTarget
  } = getValue(item);

  return [createNode(item, "<target>", `${item.path}/${SAFE_PATH_PREFIX}target`, { value: proxyTarget }, NODE_TYPES.PROXY_TARGET), createNode(item, "<handler>", `${item.path}/${SAFE_PATH_PREFIX}handler`, { value: proxyHandler }, NODE_TYPES.PROXY_HANDLER)];
}

function makeNodesForEntries(item) {
  const { path } = item;
  const nodeName = "<entries>";
  const entriesPath = `${path}/${SAFE_PATH_PREFIX}entries`;

  if (nodeHasAllEntriesInPreview(item)) {
    let entriesNodes = [];
    const { preview } = getValue(item);
    if (preview.entries) {
      entriesNodes = preview.entries.map(([key, value], index) => {
        return createNode(item, index, `${entriesPath}/${index}`, {
          value: GripMapEntryRep.createGripMapEntry(key, value)
        });
      });
    } else if (preview.items) {
      entriesNodes = preview.items.map((value, index) => {
        return createNode(item, index, `${entriesPath}/${index}`, { value });
      });
    }
    return createNode(item, nodeName, entriesPath, entriesNodes, NODE_TYPES.ENTRIES);
  }
  return createNode(item, nodeName, entriesPath, null, NODE_TYPES.ENTRIES);
}

function makeNodesForMapEntry(item) {
  const nodeValue = getValue(item);
  if (!nodeValue || !nodeValue.preview) {
    return [];
  }

  const { key, value } = nodeValue.preview;
  const path = item.path;

  return [createNode(item, "<key>", `${path}/##key`, { value: key }, NODE_TYPES.MAP_ENTRY_KEY), createNode(item, "<value>", `${path}/##value`, { value }, NODE_TYPES.MAP_ENTRY_VALUE)];
}

function getNodeGetter(item) {
  return get(item, "contents.get", undefined);
}

function getNodeSetter(item) {
  return get(item, "contents.set", undefined);
}

function makeNodesForAccessors(item) {
  const accessors = [];

  const getter = getNodeGetter(item);
  if (getter && getter.type !== "undefined") {
    accessors.push(createNode(item, "<get>", `${item.path}/${SAFE_PATH_PREFIX}get`, { value: getter }, NODE_TYPES.GET));
  }

  const setter = getNodeSetter(item);
  if (setter && setter.type !== "undefined") {
    accessors.push(createNode(item, "<set>", `${item.path}/${SAFE_PATH_PREFIX}set`, { value: setter }, NODE_TYPES.SET));
  }

  return accessors;
}

function sortProperties(properties) {
  return properties.sort((a, b) => {
    // Sort numbers in ascending order and sort strings lexicographically
    const aInt = parseInt(a, 10);
    const bInt = parseInt(b, 10);

    if (isNaN(aInt) || isNaN(bInt)) {
      return a > b ? 1 : -1;
    }

    return aInt - bInt;
  });
}

function makeNumericalBuckets(parent) {
  const parentPath = parent.path;
  const numProperties = getNumericalPropertiesCount(parent);

  // We want to have at most a hundred slices.
  const bucketSize = 10 ** Math.max(2, Math.ceil(Math.log10(numProperties)) - 2);
  const numBuckets = Math.ceil(numProperties / bucketSize);

  let buckets = [];
  for (let i = 1; i <= numBuckets; i++) {
    const minKey = (i - 1) * bucketSize;
    const maxKey = Math.min(i * bucketSize - 1, numProperties - 1);
    const startIndex = nodeIsBucket(parent) ? parent.meta.startIndex : 0;
    const minIndex = startIndex + minKey;
    const maxIndex = startIndex + maxKey;
    const bucketKey = `${SAFE_PATH_PREFIX}bucket_${minIndex}-${maxIndex}`;
    const bucketName = `[${minIndex}…${maxIndex}]`;

    buckets.push(createNode(parent, bucketName, `${parentPath}/${bucketKey}`, null, NODE_TYPES.BUCKET, {
      startIndex: minIndex,
      endIndex: maxIndex
    }));
  }
  return buckets;
}

function makeDefaultPropsBucket(propertiesNames, parent, ownProperties) {
  const parentPath = parent.path;

  const userPropertiesNames = [];
  const defaultProperties = [];

  propertiesNames.forEach(name => {
    if (isDefaultWindowProperty(name)) {
      defaultProperties.push(name);
    } else {
      userPropertiesNames.push(name);
    }
  });

  let nodes = makeNodesForOwnProps(userPropertiesNames, parent, ownProperties);

  if (defaultProperties.length > 0) {
    const defaultPropertiesNode = createNode(parent, "[default properties]", `${parentPath}/${SAFE_PATH_PREFIX}default`, null, NODE_TYPES.DEFAULT_PROPERTIES);

    const defaultNodes = defaultProperties.map((name, index) => createNode(defaultPropertiesNode, maybeEscapePropertyName(name), `${parentPath}/${SAFE_PATH_PREFIX}bucket${index}/${name}`, ownProperties[name]));
    nodes.push(setNodeChildren(defaultPropertiesNode, defaultNodes));
  }
  return nodes;
}

function makeNodesForOwnProps(propertiesNames, parent, ownProperties) {
  const parentPath = parent.path;
  return propertiesNames.map(name => createNode(parent, maybeEscapePropertyName(name), `${parentPath}/${name}`, ownProperties[name]));
}

function makeNodesForProperties(objProps, parent) {
  const {
    ownProperties = {},
    ownSymbols,
    prototype,
    safeGetterValues
  } = objProps;

  const parentPath = parent.path;
  const parentValue = getValue(parent);

  let allProperties = Object.assign({}, ownProperties, safeGetterValues);

  // Ignore properties that are neither non-concrete nor getters/setters.
  const propertiesNames = sortProperties(Object.keys(allProperties)).filter(name => {
    if (!allProperties[name]) {
      return false;
    }

    const properties = Object.getOwnPropertyNames(allProperties[name]);
    return properties.some(property => ["value", "getterValue", "get", "set"].includes(property));
  });

  let nodes = [];
  if (parentValue && parentValue.class == "Window") {
    nodes = makeDefaultPropsBucket(propertiesNames, parent, allProperties);
  } else {
    nodes = makeNodesForOwnProps(propertiesNames, parent, allProperties);
  }

  if (Array.isArray(ownSymbols)) {
    ownSymbols.forEach((ownSymbol, index) => {
      nodes.push(createNode(parent, ownSymbol.name, `${parentPath}/${SAFE_PATH_PREFIX}symbol-${index}`, ownSymbol.descriptor || null));
    }, this);
  }

  if (nodeIsPromise(parent)) {
    nodes.push(...makeNodesForPromiseProperties(parent));
  }

  if (nodeHasEntries(parent)) {
    nodes.push(makeNodesForEntries(parent));
  }

  // Add the prototype if it exists and is not null
  if (prototype && prototype.type !== "null") {
    nodes.push(makeNodeForPrototype(objProps, parent));
  }

  return nodes;
}

function makeNodeForPrototype(objProps, parent) {
  const {
    prototype
  } = objProps || {};

  // Add the prototype if it exists and is not null
  if (prototype && prototype.type !== "null") {
    return createNode(parent, "__proto__", `${parent.path}/__proto__`, { value: prototype }, NODE_TYPES.PROTOTYPE);
  }

  return null;
}

function createNode(parent, name, path, contents, type = NODE_TYPES.GRIP, meta) {
  if (contents === undefined) {
    return null;
  }

  // The path is important to uniquely identify the item in the entire
  // tree. This helps debugging & optimizes React's rendering of large
  // lists. The path will be separated by property name,
  // i.e. `{ foo: { bar: { baz: 5 }}}` will have a path of `foo/bar/baz`
  // for the inner object.
  return {
    parent,
    name,
    path,
    contents,
    type,
    meta
  };
}

function setNodeChildren(node, children) {
  node.contents = children;
  return node;
}

function getChildren(options) {
  const {
    cachedNodes,
    loadedProperties = new Map(),
    item
  } = options;

  const key = item.path;
  if (cachedNodes && cachedNodes.has(key)) {
    return cachedNodes.get(key);
  }

  const loadedProps = loadedProperties.get(key);
  const {
    ownProperties,
    ownSymbols,
    safeGetterValues,
    prototype
  } = loadedProps || {};
  const hasLoadedProps = ownProperties || ownSymbols || safeGetterValues || prototype;

  // Because we are dynamically creating the tree as the user
  // expands it (not precalculated tree structure), we cache child
  // arrays. This not only helps performance, but is necessary
  // because the expanded state depends on instances of nodes
  // being the same across renders. If we didn't do this, each
  // node would be a new instance every render.
  // If the node needs properties, we only add children to
  // the cache if the properties are loaded.
  const addToCache = children => {
    if (cachedNodes) {
      cachedNodes.set(item.path, children);
    }
    return children;
  };

  // Nodes can either have children already, or be an object with
  // properties that we need to go and fetch.
  if (nodeHasChildren(item)) {
    return addToCache(item.contents);
  }

  if (nodeHasAccessors(item)) {
    return addToCache(makeNodesForAccessors(item));
  }

  if (nodeIsMapEntry(item)) {
    return addToCache(makeNodesForMapEntry(item));
  }

  if (nodeIsProxy(item)) {
    const nodes = makeNodesForProxyProperties(item);
    const protoNode = makeNodeForPrototype(loadedProps, item);
    if (protoNode) {
      return addToCache(nodes.concat(protoNode));
    }
    return nodes;
  }

  if (nodeNeedsNumericalBuckets(item)) {
    const bucketNodes = makeNumericalBuckets(item);
    // Even if we have numerical buckets, we might have loaded non indexed properties,
    // like length for example.
    if (hasLoadedProps) {
      return addToCache(bucketNodes.concat(makeNodesForProperties(loadedProps, item)));
    }

    // We don't cache the result here so we can have the prototype, properties and symbols
    // when they are loaded.
    return bucketNodes;
  }

  if (!nodeIsEntries(item) && !nodeIsBucket(item) && !nodeHasProperties(item)) {
    return [];
  }

  if (!hasLoadedProps) {
    return [];
  }

  return addToCache(makeNodesForProperties(loadedProps, item));
}

function getParent(item) {
  return item.parent;
}

function getNumericalPropertiesCount(item) {
  if (nodeIsBucket(item)) {
    return item.meta.endIndex - item.meta.startIndex + 1;
  }

  const value = getValue(getClosestGripNode(item));
  if (!value) {
    return 0;
  }

  if (GripArrayRep.supportsObject(value)) {
    return GripArrayRep.getLength(value);
  }

  if (GripMap.supportsObject(value)) {
    return GripMap.getLength(value);
  }

  // TODO: We can also have numerical properties on Objects, but at the
  // moment we don't have a way to distinguish them from non-indexed properties,
  // as they are all computed in a ownPropertiesLength property.

  return 0;
}

function getClosestGripNode(item) {
  const type = getType(item);
  if (type !== NODE_TYPES.BUCKET && type !== NODE_TYPES.DEFAULT_PROPERTIES && type !== NODE_TYPES.ENTRIES) {
    return item;
  }

  const parent = getParent(item);
  if (!parent) {
    return null;
  }

  return getClosestGripNode(parent);
}

function getClosestNonBucketNode(item) {
  const type = getType(item);

  if (type !== NODE_TYPES.BUCKET) {
    return item;
  }

  const parent = getParent(item);
  if (!parent) {
    return null;
  }

  return getClosestNonBucketNode(parent);
}

function shouldLoadItemIndexedProperties(item, loadedProperties = new Map()) {
  const gripItem = getClosestGripNode(item);
  const value = getValue(gripItem);

  return value && nodeHasProperties(gripItem) && !loadedProperties.has(item.path) && !nodeIsProxy(item) && !nodeNeedsNumericalBuckets(item) && !nodeIsEntries(getClosestNonBucketNode(item))
  // The data is loaded when expanding the window node.
  && !nodeIsDefaultProperties(item);
}

function shouldLoadItemNonIndexedProperties(item, loadedProperties = new Map()) {
  const gripItem = getClosestGripNode(item);
  const value = getValue(gripItem);

  return value && nodeHasProperties(gripItem) && !loadedProperties.has(item.path) && !nodeIsProxy(item) && !nodeIsEntries(getClosestNonBucketNode(item)) && !nodeIsBucket(item)
  // The data is loaded when expanding the window node.
  && !nodeIsDefaultProperties(item);
}

function shouldLoadItemEntries(item, loadedProperties = new Map()) {
  const gripItem = getClosestGripNode(item);
  const value = getValue(gripItem);

  return value && nodeIsEntries(getClosestNonBucketNode(item)) && !nodeHasAllEntriesInPreview(gripItem) && !loadedProperties.has(item.path) && !nodeNeedsNumericalBuckets(item);
}

function shouldLoadItemPrototype(item, loadedProperties = new Map()) {
  const value = getValue(item);

  return value && !loadedProperties.has(item.path) && !nodeIsBucket(item) && !nodeIsMapEntry(item) && !nodeIsEntries(item) && !nodeIsDefaultProperties(item) && !nodeHasAccessors(item) && !nodeIsPrimitive(item);
}

function shouldLoadItemSymbols(item, loadedProperties = new Map()) {
  const value = getValue(item);

  return value && !loadedProperties.has(item.path) && !nodeIsBucket(item) && !nodeIsMapEntry(item) && !nodeIsEntries(item) && !nodeIsDefaultProperties(item) && !nodeHasAccessors(item) && !nodeIsPrimitive(item) && !nodeIsProxy(item);
}

module.exports = {
  createNode,
  getChildren,
  getClosestGripNode,
  getClosestNonBucketNode,
  getParent,
  getNumericalPropertiesCount,
  getValue,
  makeNodesForEntries,
  makeNodesForPromiseProperties,
  makeNodesForProperties,
  makeNumericalBuckets,
  nodeHasAccessors,
  nodeHasAllEntriesInPreview,
  nodeHasChildren,
  nodeHasEntries,
  nodeHasProperties,
  nodeIsBucket,
  nodeIsDefaultProperties,
  nodeIsEntries,
  nodeIsFunction,
  nodeIsGetter,
  nodeIsMapEntry,
  nodeIsMissingArguments,
  nodeIsObject,
  nodeIsOptimizedOut,
  nodeIsPrimitive,
  nodeIsPromise,
  nodeIsPrototype,
  nodeIsProxy,
  nodeIsSetter,
  nodeIsWindow,
  nodeNeedsNumericalBuckets,
  nodeSupportsNumericalBucketing,
  setNodeChildren,
  shouldLoadItemEntries,
  shouldLoadItemIndexedProperties,
  shouldLoadItemNonIndexedProperties,
  shouldLoadItemPrototype,
  shouldLoadItemSymbols,
  sortProperties,
  NODE_TYPES,
  // Export for testing purpose.
  SAFE_PATH_PREFIX
};

/***/ }),
/* 17 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { MODE } = __webpack_require__(3);
const { REPS, getRep } = __webpack_require__(4);
const ObjectInspector = __webpack_require__(46);
const ObjectInspectorUtils = __webpack_require__(16);

const {
  parseURLEncodedText,
  parseURLParams,
  maybeEscapePropertyName,
  getGripPreviewItems
} = __webpack_require__(0);

module.exports = {
  REPS,
  getRep,
  MODE,
  maybeEscapePropertyName,
  parseURLEncodedText,
  parseURLParams,
  getGripPreviewItems,
  ObjectInspector,
  ObjectInspectorUtils
};

/***/ }),
/* 18 */
/***/ (function(module, exports) {

// removed by extract-text-webpack-plugin

/***/ }),
/* 19 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Dependencies
const {
  getGripType,
  wrapRender
} = __webpack_require__(0);

const dom = __webpack_require__(1);
const { span } = dom;

/**
 * Renders undefined value
 */
const Undefined = function () {
  return span({ className: "objectBox objectBox-undefined" }, "undefined");
};

function supportsObject(object, noGrip = false) {
  if (noGrip === true) {
    return object === undefined;
  }

  return object && object.type && object.type == "undefined" || getGripType(object, noGrip) == "undefined";
}

// Exports from this module

module.exports = {
  rep: wrapRender(Undefined),
  supportsObject
};

/***/ }),
/* 20 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Dependencies
const { wrapRender } = __webpack_require__(0);
const dom = __webpack_require__(1);
const { span } = dom;

/**
 * Renders null value
 */
function Null(props) {
  return span({ className: "objectBox objectBox-null" }, "null");
}

function supportsObject(object, noGrip = false) {
  if (noGrip === true) {
    return object === null;
  }

  if (object && object.type && object.type == "null") {
    return true;
  }

  return object == null;
}

// Exports from this module

module.exports = {
  rep: wrapRender(Null),
  supportsObject
};

/***/ }),
/* 21 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Dependencies
const PropTypes = __webpack_require__(2);
const {
  escapeString,
  sanitizeString,
  isGrip,
  wrapRender
} = __webpack_require__(0);

const dom = __webpack_require__(1);
const { span } = dom;

/**
 * Renders a long string grip.
 */
LongStringRep.propTypes = {
  useQuotes: PropTypes.bool,
  escapeWhitespace: PropTypes.bool,
  style: PropTypes.object,
  cropLimit: PropTypes.number.isRequired,
  member: PropTypes.string,
  object: PropTypes.object.isRequired
};

function LongStringRep(props) {
  let {
    cropLimit,
    member,
    object,
    style,
    useQuotes = true,
    escapeWhitespace = true
  } = props;
  let { fullText, initial, length } = object;

  let config = {
    "data-link-actor-id": object.actor,
    className: "objectBox objectBox-string"
  };

  if (style) {
    config.style = style;
  }

  let string = member && member.open ? fullText || initial : initial.substring(0, cropLimit);

  if (string.length < length) {
    string += "\u2026";
  }
  let formattedString = useQuotes ? escapeString(string, escapeWhitespace) : sanitizeString(string);
  return span(config, formattedString);
}

function supportsObject(object, noGrip = false) {
  if (noGrip === true || !isGrip(object)) {
    return false;
  }
  return object.type === "longString";
}

// Exports from this module
module.exports = {
  rep: wrapRender(LongStringRep),
  supportsObject
};

/***/ }),
/* 22 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Dependencies
const PropTypes = __webpack_require__(2);

const {
  getGripType,
  wrapRender
} = __webpack_require__(0);

const dom = __webpack_require__(1);
const { span } = dom;

/**
 * Renders a number
 */
Number.propTypes = {
  object: PropTypes.oneOfType([PropTypes.object, PropTypes.number, PropTypes.bool]).isRequired
};

function Number(props) {
  let value = props.object;

  return span({ className: "objectBox objectBox-number" }, stringify(value));
}

function stringify(object) {
  let isNegativeZero = Object.is(object, -0) || object.type && object.type == "-0";

  return isNegativeZero ? "-0" : String(object);
}

function supportsObject(object, noGrip = false) {
  return ["boolean", "number", "-0"].includes(getGripType(object, noGrip));
}

// Exports from this module

module.exports = {
  rep: wrapRender(Number),
  supportsObject
};

/***/ }),
/* 23 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Dependencies
const PropTypes = __webpack_require__(2);
const {
  wrapRender
} = __webpack_require__(0);
const PropRep = __webpack_require__(5);
const { MODE } = __webpack_require__(3);

const dom = __webpack_require__(1);
const { span } = dom;

const DEFAULT_TITLE = "Object";

/**
 * Renders an object. An object is represented by a list of its
 * properties enclosed in curly brackets.
 */
ObjectRep.propTypes = {
  object: PropTypes.object.isRequired,
  // @TODO Change this to Object.values once it's supported in Node's version of V8
  mode: PropTypes.oneOf(Object.keys(MODE).map(key => MODE[key])),
  title: PropTypes.string
};

function ObjectRep(props) {
  let object = props.object;
  let propsArray = safePropIterator(props, object);

  if (props.mode === MODE.TINY) {
    const tinyModeItems = [];
    if (getTitle(props, object) !== DEFAULT_TITLE) {
      tinyModeItems.push(getTitleElement(props, object));
    } else {
      tinyModeItems.push(span({
        className: "objectLeftBrace"
      }, "{"), propsArray.length > 0 ? span({
        key: "more",
        className: "more-ellipsis",
        title: "more…"
      }, "…") : null, span({
        className: "objectRightBrace"
      }, "}"));
    }

    return span({ className: "objectBox objectBox-object" }, ...tinyModeItems);
  }

  return span({ className: "objectBox objectBox-object" }, getTitleElement(props, object), span({
    className: "objectLeftBrace"
  }, " { "), ...propsArray, span({
    className: "objectRightBrace"
  }, " }"));
}

function getTitleElement(props, object) {
  return span({ className: "objectTitle" }, getTitle(props, object));
}

function getTitle(props, object) {
  return props.title || object.class || DEFAULT_TITLE;
}

function safePropIterator(props, object, max) {
  max = typeof max === "undefined" ? 3 : max;
  try {
    return propIterator(props, object, max);
  } catch (err) {
    console.error(err);
  }
  return [];
}

function propIterator(props, object, max) {
  // Work around https://bugzilla.mozilla.org/show_bug.cgi?id=945377
  if (Object.prototype.toString.call(object) === "[object Generator]") {
    object = Object.getPrototypeOf(object);
  }

  const elements = [];
  const unimportantProperties = [];
  let propertiesNumber = 0;
  const propertiesNames = Object.keys(object);

  const pushPropRep = (name, value) => {
    elements.push(PropRep(Object.assign({}, props, {
      key: name,
      mode: MODE.TINY,
      name,
      object: value,
      equal: ": "
    })));
    propertiesNumber++;

    if (propertiesNumber < propertiesNames.length) {
      elements.push(", ");
    }
  };

  try {
    for (let name of propertiesNames) {
      if (propertiesNumber >= max) {
        break;
      }

      let value;
      try {
        value = object[name];
      } catch (exc) {
        continue;
      }

      // Object members with non-empty values are preferred since it gives the
      // user a better overview of the object.
      if (isInterestingProp(value)) {
        pushPropRep(name, value);
      } else {
        // If the property is not important, put its name on an array for later use.
        unimportantProperties.push(name);
      }
    }
  } catch (err) {
    console.error(err);
  }

  if (propertiesNumber < max) {
    for (let name of unimportantProperties) {
      if (propertiesNumber >= max) {
        break;
      }

      let value;
      try {
        value = object[name];
      } catch (exc) {
        continue;
      }

      pushPropRep(name, value);
    }
  }

  if (propertiesNumber < propertiesNames.length) {
    elements.push(span({
      key: "more",
      className: "more-ellipsis",
      title: ", more…"
    }, "…"));
  }

  return elements;
}

function isInterestingProp(value) {
  const type = typeof value;
  return type == "boolean" || type == "number" || type == "string" && value;
}

function supportsObject(object) {
  return true;
}

// Exports from this module
module.exports = {
  rep: wrapRender(ObjectRep),
  supportsObject
};

/***/ }),
/* 24 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Dependencies
const PropTypes = __webpack_require__(2);

const {
  getGripType,
  wrapRender
} = __webpack_require__(0);

const dom = __webpack_require__(1);
const { span } = dom;

/**
 * Renders a symbol.
 */
SymbolRep.propTypes = {
  object: PropTypes.object.isRequired
};

function SymbolRep(props) {
  let {
    className = "objectBox objectBox-symbol",
    object
  } = props;
  let { name } = object;

  return span({ className }, `Symbol(${name || ""})`);
}

function supportsObject(object, noGrip = false) {
  return getGripType(object, noGrip) == "symbol";
}

// Exports from this module
module.exports = {
  rep: wrapRender(SymbolRep),
  supportsObject
};

/***/ }),
/* 25 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Dependencies
const PropTypes = __webpack_require__(2);

const {
  getGripType,
  wrapRender
} = __webpack_require__(0);

const dom = __webpack_require__(1);
const { span } = dom;

/**
 * Renders a Infinity object
 */
InfinityRep.propTypes = {
  object: PropTypes.object.isRequired
};

function InfinityRep(props) {
  const {
    object
  } = props;

  return span({ className: "objectBox objectBox-number" }, object.type);
}

function supportsObject(object, noGrip = false) {
  const type = getGripType(object, noGrip);
  return type == "Infinity" || type == "-Infinity";
}

// Exports from this module
module.exports = {
  rep: wrapRender(InfinityRep),
  supportsObject
};

/***/ }),
/* 26 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Dependencies
const {
  getGripType,
  wrapRender
} = __webpack_require__(0);

const dom = __webpack_require__(1);
const { span } = dom;

/**
 * Renders a NaN object
 */
function NaNRep(props) {
  return span({ className: "objectBox objectBox-nan" }, "NaN");
}

function supportsObject(object, noGrip = false) {
  return getGripType(object, noGrip) == "NaN";
}

// Exports from this module
module.exports = {
  rep: wrapRender(NaNRep),
  supportsObject
};

/***/ }),
/* 27 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Dependencies
const dom = __webpack_require__(1);
const PropTypes = __webpack_require__(2);
const {
  wrapRender
} = __webpack_require__(0);
const { MODE } = __webpack_require__(3);
const { span } = dom;

/**
 * Renders an object. An object is represented by a list of its
 * properties enclosed in curly brackets.
 */
Accessor.propTypes = {
  object: PropTypes.object.isRequired,
  mode: PropTypes.oneOf(Object.values(MODE))
};

function Accessor(props) {
  const {
    object
  } = props;

  const accessors = [];
  if (hasGetter(object)) {
    accessors.push("Getter");
  }
  if (hasSetter(object)) {
    accessors.push("Setter");
  }
  const title = accessors.join(" & ");

  return span({ className: "objectBox objectBox-accessor" }, span({
    className: "objectTitle"
  }, title));
}

function hasGetter(object) {
  return object && object.get && object.get.type !== "undefined";
}

function hasSetter(object) {
  return object && object.set && object.set.type !== "undefined";
}

function supportsObject(object, noGrip = false) {
  if (noGrip !== true && (hasGetter(object) || hasSetter(object))) {
    return true;
  }

  return false;
}

// Exports from this module
module.exports = {
  rep: wrapRender(Accessor),
  supportsObject
};

/***/ }),
/* 28 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// ReactJS
const PropTypes = __webpack_require__(2);
const dom = __webpack_require__(1);
const { span } = dom;

// Reps
const {
  getGripType,
  isGrip,
  wrapRender
} = __webpack_require__(0);
const { rep: StringRep } = __webpack_require__(7);

/**
 * Renders DOM attribute
 */
Attribute.propTypes = {
  object: PropTypes.object.isRequired
};

function Attribute(props) {
  let {
    object
  } = props;
  let value = object.preview.value;

  return span({
    "data-link-actor-id": object.actor,
    className: "objectBox-Attr"
  }, span({ className: "attrName" }, getTitle(object)), span({ className: "attrEqual" }, "="), StringRep({ className: "attrValue", object: value }));
}

function getTitle(grip) {
  return grip.preview.nodeName;
}

// Registration
function supportsObject(grip, noGrip = false) {
  if (noGrip === true || !isGrip(grip)) {
    return false;
  }

  return getGripType(grip, noGrip) == "Attr" && grip.preview;
}

module.exports = {
  rep: wrapRender(Attribute),
  supportsObject
};

/***/ }),
/* 29 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// ReactJS
const PropTypes = __webpack_require__(2);

// Reps
const {
  getGripType,
  isGrip,
  wrapRender
} = __webpack_require__(0);

const dom = __webpack_require__(1);
const { span } = dom;

/**
 * Used to render JS built-in Date() object.
 */
DateTime.propTypes = {
  object: PropTypes.object.isRequired
};

function DateTime(props) {
  let grip = props.object;
  let date;
  try {
    date = span({
      "data-link-actor-id": grip.actor,
      className: "objectBox"
    }, getTitle(grip), span({ className: "Date" }, new Date(grip.preview.timestamp).toISOString()));
  } catch (e) {
    date = span({ className: "objectBox" }, "Invalid Date");
  }

  return date;
}

function getTitle(grip) {
  return span({
    className: "objectTitle"
  }, grip.class + " ");
}

// Registration
function supportsObject(grip, noGrip = false) {
  if (noGrip === true || !isGrip(grip)) {
    return false;
  }

  return getGripType(grip, noGrip) == "Date" && grip.preview;
}

// Exports from this module
module.exports = {
  rep: wrapRender(DateTime),
  supportsObject
};

/***/ }),
/* 30 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// ReactJS
const PropTypes = __webpack_require__(2);

// Reps
const {
  getGripType,
  isGrip,
  getURLDisplayString,
  wrapRender
} = __webpack_require__(0);

const dom = __webpack_require__(1);
const { span } = dom;

/**
 * Renders DOM document object.
 */
Document.propTypes = {
  object: PropTypes.object.isRequired
};

function Document(props) {
  let grip = props.object;
  const location = getLocation(grip);
  return span({
    "data-link-actor-id": grip.actor,
    className: "objectBox objectBox-document"
  }, getTitle(grip), location ? " " : null, location ? span({ className: "location" }, location) : null);
}

function getLocation(grip) {
  let location = grip.preview.location;
  return location ? getURLDisplayString(location) : null;
}

function getTitle(grip) {
  return span({
    className: "objectTitle"
  }, grip.class);
}

// Registration
function supportsObject(object, noGrip = false) {
  if (noGrip === true || !isGrip(object)) {
    return false;
  }

  return object.preview && getGripType(object, noGrip) == "HTMLDocument";
}

// Exports from this module
module.exports = {
  rep: wrapRender(Document),
  supportsObject
};

/***/ }),
/* 31 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// ReactJS
const PropTypes = __webpack_require__(2);

// Reps
const {
  isGrip,
  wrapRender
} = __webpack_require__(0);

const { MODE } = __webpack_require__(3);
const { rep } = __webpack_require__(8);

/**
 * Renders DOM event objects.
 */
Event.propTypes = {
  object: PropTypes.object.isRequired,
  // @TODO Change this to Object.values once it's supported in Node's version of V8
  mode: PropTypes.oneOf(Object.keys(MODE).map(key => MODE[key])),
  onDOMNodeMouseOver: PropTypes.func,
  onDOMNodeMouseOut: PropTypes.func,
  onInspectIconClick: PropTypes.func
};

function Event(props) {
  // Use `Object.assign` to keep `props` without changes because:
  // 1. JSON.stringify/JSON.parse is slow.
  // 2. Immutable.js is planned for the future.
  let gripProps = Object.assign({}, props, {
    title: getTitle(props)
  });
  gripProps.object = Object.assign({}, props.object);
  gripProps.object.preview = Object.assign({}, props.object.preview);

  gripProps.object.preview.ownProperties = {};
  if (gripProps.object.preview.target) {
    Object.assign(gripProps.object.preview.ownProperties, {
      target: gripProps.object.preview.target
    });
  }
  Object.assign(gripProps.object.preview.ownProperties, gripProps.object.preview.properties);

  delete gripProps.object.preview.properties;
  gripProps.object.ownPropertyLength = Object.keys(gripProps.object.preview.ownProperties).length;

  switch (gripProps.object.class) {
    case "MouseEvent":
      gripProps.isInterestingProp = (type, value, name) => {
        return ["target", "clientX", "clientY", "layerX", "layerY"].includes(name);
      };
      break;
    case "KeyboardEvent":
      gripProps.isInterestingProp = (type, value, name) => {
        return ["target", "key", "charCode", "keyCode"].includes(name);
      };
      break;
    case "MessageEvent":
      gripProps.isInterestingProp = (type, value, name) => {
        return ["target", "isTrusted", "data"].includes(name);
      };
      break;
    default:
      gripProps.isInterestingProp = (type, value, name) => {
        // We want to show the properties in the order they are declared.
        return Object.keys(gripProps.object.preview.ownProperties).includes(name);
      };
  }

  return rep(gripProps);
}

function getTitle(props) {
  let preview = props.object.preview;
  let title = preview.type;

  if (preview.eventKind == "key" && preview.modifiers && preview.modifiers.length) {
    title = `${title} ${preview.modifiers.join("-")}`;
  }
  return title;
}

// Registration
function supportsObject(grip, noGrip = false) {
  if (noGrip === true || !isGrip(grip)) {
    return false;
  }

  return grip.preview && grip.preview.kind == "DOMEvent";
}

// Exports from this module
module.exports = {
  rep: wrapRender(Event),
  supportsObject
};

/***/ }),
/* 32 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// ReactJS
const PropTypes = __webpack_require__(2);

// Reps
const {
  getGripType,
  isGrip,
  cropString,
  wrapRender
} = __webpack_require__(0);
const { MODE } = __webpack_require__(3);
const Svg = __webpack_require__(9);

const dom = __webpack_require__(1);
const { span } = dom;

/**
 * This component represents a template for Function objects.
 */
FunctionRep.propTypes = {
  object: PropTypes.object.isRequired,
  parameterNames: PropTypes.array,
  onViewSourceInDebugger: PropTypes.func
};

function FunctionRep(props) {
  let {
    object: grip,
    onViewSourceInDebugger
  } = props;

  let jumpToDefinitionButton;
  if (onViewSourceInDebugger && grip.location && grip.location.url) {
    jumpToDefinitionButton = Svg("jump-definition", {
      element: "a",
      draggable: false,
      title: "Jump to definition",
      onClick: e => {
        // Stop the event propagation so we don't trigger ObjectInspector expand/collapse.
        e.stopPropagation();
        onViewSourceInDebugger(grip.location);
      }
    });
  }

  return span({
    "data-link-actor-id": grip.actor,
    className: "objectBox objectBox-function",
    // Set dir="ltr" to prevent function parentheses from
    // appearing in the wrong direction
    dir: "ltr"
  }, getTitle(grip, props), getFunctionName(grip, props), "(", ...renderParams(props), ")", jumpToDefinitionButton);
}

function getTitle(grip, props) {
  const {
    mode
  } = props;

  if (mode === MODE.TINY && !grip.isGenerator && !grip.isAsync) {
    return null;
  }

  let title = mode === MODE.TINY ? "" : "function ";

  if (grip.isGenerator) {
    title = mode === MODE.TINY ? "* " : "function* ";
  }

  if (grip.isAsync) {
    title = "async" + " " + title;
  }

  return span({
    className: "objectTitle"
  }, title);
}

// Decodes an anonymous naming scheme that
// spider monkey implements based on "Naming Anonymous JavaScript Functions"
// http://johnjbarton.github.io/nonymous/index.html
const objectProperty = /([\w\d]+)$/;
const arrayProperty = /\[(.*?)\]$/;
const functionProperty = /([\w\d]+)[\/\.<]*?$/;
const annonymousProperty = /([\w\d]+)\(\^\)$/;

function getFunctionName(grip, props) {
  let name = grip.userDisplayName || grip.displayName || grip.name || props.functionName || "";

  const scenarios = [objectProperty, arrayProperty, functionProperty, annonymousProperty];

  scenarios.some(reg => {
    const match = reg.exec(name);
    if (match) {
      name = match[1];
      return true;
    }
    return false;
  });

  return cropString(name, 100);
}

function renderParams(props) {
  const {
    parameterNames = []
  } = props;

  return parameterNames.filter(param => param).reduce((res, param, index, arr) => {
    res.push(span({ className: "param" }, param));
    if (index < arr.length - 1) {
      res.push(span({ className: "delimiter" }, ", "));
    }
    return res;
  }, []);
}

// Registration
function supportsObject(grip, noGrip = false) {
  const type = getGripType(grip, noGrip);
  if (noGrip === true || !isGrip(grip)) {
    return type == "function";
  }

  return type == "Function";
}

// Exports from this module

module.exports = {
  rep: wrapRender(FunctionRep),
  supportsObject,
  // exported for testing purpose.
  getFunctionName
};

/***/ }),
/* 33 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


Object.defineProperty(exports, "__esModule", {
    value: true
});
exports.convertReactSVGDOMProperty = convertReactSVGDOMProperty;
exports.startsWith = startsWith;
exports.serializeAttrs = serializeAttrs;
exports.getSVGFromSource = getSVGFromSource;
exports.extractSVGProps = extractSVGProps;
// Transform DOM prop/attr names applicable to `<svg>` element but react-limited

function convertReactSVGDOMProperty(str) {
    return str.replace(/[-|:]([a-z])/g, function (g) {
        return g[1].toUpperCase();
    });
}

function startsWith(str, substring) {
    return str.indexOf(substring) === 0;
}

var DataPropPrefix = 'data-';
// Serialize `Attr` objects in `NamedNodeMap`
function serializeAttrs(map) {
    var ret = {};
    for (var prop, i = 0; i < map.length; i++) {
        var key = map[i].name;
        if (!startsWith(key, DataPropPrefix)) {
            prop = convertReactSVGDOMProperty(key);
        }
        ret[prop] = map[i].value;
    }
    return ret;
}

function getSVGFromSource(src) {
    var svgContainer = document.createElement('div');
    svgContainer.innerHTML = src;
    var svg = svgContainer.firstElementChild;
    svg.remove(); // deref from parent element
    return svg;
}

// get <svg /> element props
function extractSVGProps(src) {
    var map = getSVGFromSource(src).attributes;
    return map.length > 0 ? serializeAttrs(map) : null;
}

/***/ }),
/* 34 */
/***/ (function(module, exports) {

module.exports = "<!-- This Source Code Form is subject to the terms of the Mozilla Public - License, v. 2.0. If a copy of the MPL was not distributed with this - file, You can obtain one at http://mozilla.org/MPL/2.0/. --><svg viewBox=\"0 0 16 16\" xmlns=\"http://www.w3.org/2000/svg\"><path d=\"M8,3L12,3L12,7L14,7L14,8L12,8L12,12L8,12L8,14L7,14L7,12L3,12L3,8L1,8L1,7L3,7L3,3L7,3L7,1L8,1L8,3ZM10,10L10,5L5,5L5,10L10,10Z\"></path></svg>"

/***/ }),
/* 35 */
/***/ (function(module, exports) {

module.exports = "<!-- This Source Code Form is subject to the terms of the Mozilla Public - License, v. 2.0. If a copy of the MPL was not distributed with this - file, You can obtain one at http://mozilla.org/MPL/2.0/. --><svg viewBox=\"0 0 16 16\" xmlns=\"http://www.w3.org/2000/svg\"><g stroke-width=\"1\" fill=\"none\" fill-rule=\"evenodd\" stroke-linecap=\"round\"><g id=\"arrow\" transform=\"translate(1.000000, 3.000000)\"><path d=\"M4.5,0.5 L6.5,2.5\"></path><path d=\"M4.5,2.5 L6.5,4.5\" transform=\"translate(5.500000, 3.500000) scale(1, -1) translate(-5.500000, -3.500000) \"></path><path d=\"M6.00090144,2.5 C4.67806937,2.5 3.67938478,2.5 3.00484766,2.5 C1.99304199,2.5 1.01049805,3.5168457 0.993840144,4.52403846 C0.988750751,4.54723808 0.988750751,5.87097168 0.993840144,8.49523926\" id=\"Path-2\" stroke-linejoin=\"round\"></path></g><g id=\"content-lines\" transform=\"translate(9.000000, 2.000000)\"><path d=\"M1.5,3.5 L5.5,3.5\"></path><path d=\"M0.5,1.5 L5.5,1.5\"></path><path d=\"M0.5,5.5 L5.5,5.5\"></path></g></g></svg>"

/***/ }),
/* 36 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// ReactJS
const PropTypes = __webpack_require__(2);
// Dependencies
const {
  getGripType,
  isGrip,
  wrapRender
} = __webpack_require__(0);

const PropRep = __webpack_require__(5);
const { MODE } = __webpack_require__(3);

const dom = __webpack_require__(1);
const { span } = dom;

/**
 * Renders a DOM Promise object.
 */
PromiseRep.propTypes = {
  object: PropTypes.object.isRequired,
  // @TODO Change this to Object.values once it's supported in Node's version of V8
  mode: PropTypes.oneOf(Object.keys(MODE).map(key => MODE[key])),
  onDOMNodeMouseOver: PropTypes.func,
  onDOMNodeMouseOut: PropTypes.func,
  onInspectIconClick: PropTypes.func
};

function PromiseRep(props) {
  const object = props.object;
  const { promiseState } = object;

  const config = {
    "data-link-actor-id": object.actor,
    className: "objectBox objectBox-object"
  };

  if (props.mode === MODE.TINY) {
    let { Rep } = __webpack_require__(4);

    return span(config, getTitle(object), span({
      className: "objectLeftBrace"
    }, " { "), Rep({ object: promiseState.state }), span({
      className: "objectRightBrace"
    }, " }"));
  }

  const propsArray = getProps(props, promiseState);
  return span(config, getTitle(object), span({
    className: "objectLeftBrace"
  }, " { "), ...propsArray, span({
    className: "objectRightBrace"
  }, " }"));
}

function getTitle(object) {
  return span({
    className: "objectTitle"
  }, object.class);
}

function getProps(props, promiseState) {
  const keys = ["state"];
  if (Object.keys(promiseState).includes("value")) {
    keys.push("value");
  }

  return keys.reduce((res, key, i) => {
    let object = promiseState[key];
    res = res.concat(PropRep(Object.assign({}, props, {
      mode: MODE.TINY,
      name: `<${key}>`,
      object,
      equal: ": ",
      suppressQuotes: true
    })));

    // Interleave commas between elements
    if (i !== keys.length - 1) {
      res.push(", ");
    }

    return res;
  }, []);
}

// Registration
function supportsObject(object, noGrip = false) {
  if (noGrip === true || !isGrip(object)) {
    return false;
  }
  return getGripType(object, noGrip) == "Promise";
}

// Exports from this module
module.exports = {
  rep: wrapRender(PromiseRep),
  supportsObject
};

/***/ }),
/* 37 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// ReactJS
const PropTypes = __webpack_require__(2);

// Reps
const {
  getGripType,
  isGrip,
  wrapRender
} = __webpack_require__(0);

const dom = __webpack_require__(1);
const { span } = dom;

/**
 * Renders a grip object with regular expression.
 */
RegExp.propTypes = {
  object: PropTypes.object.isRequired
};

function RegExp(props) {
  let { object } = props;

  return span({
    "data-link-actor-id": object.actor,
    className: "objectBox objectBox-regexp regexpSource"
  }, getSource(object));
}

function getSource(grip) {
  return grip.displayString;
}

// Registration
function supportsObject(object, noGrip = false) {
  if (noGrip === true || !isGrip(object)) {
    return false;
  }

  return getGripType(object, noGrip) == "RegExp";
}

// Exports from this module
module.exports = {
  rep: wrapRender(RegExp),
  supportsObject
};

/***/ }),
/* 38 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// ReactJS
const PropTypes = __webpack_require__(2);

// Reps
const {
  getGripType,
  isGrip,
  getURLDisplayString,
  wrapRender
} = __webpack_require__(0);

const dom = __webpack_require__(1);
const { span } = dom;

/**
 * Renders a grip representing CSSStyleSheet
 */
StyleSheet.propTypes = {
  object: PropTypes.object.isRequired
};

function StyleSheet(props) {
  let grip = props.object;

  return span({
    "data-link-actor-id": grip.actor,
    className: "objectBox objectBox-object"
  }, getTitle(grip), span({ className: "objectPropValue" }, getLocation(grip)));
}

function getTitle(grip) {
  let title = "StyleSheet ";
  return span({ className: "objectBoxTitle" }, title);
}

function getLocation(grip) {
  // Embedded stylesheets don't have URL and so, no preview.
  let url = grip.preview ? grip.preview.url : "";
  return url ? getURLDisplayString(url) : "";
}

// Registration
function supportsObject(object, noGrip = false) {
  if (noGrip === true || !isGrip(object)) {
    return false;
  }

  return getGripType(object, noGrip) == "CSSStyleSheet";
}

// Exports from this module

module.exports = {
  rep: wrapRender(StyleSheet),
  supportsObject
};

/***/ }),
/* 39 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Dependencies
const PropTypes = __webpack_require__(2);
const {
  isGrip,
  cropString,
  cropMultipleLines,
  wrapRender
} = __webpack_require__(0);
const { MODE } = __webpack_require__(3);
const nodeConstants = __webpack_require__(12);
const dom = __webpack_require__(1);
const { span } = dom;

/**
 * Renders DOM comment node.
 */
CommentNode.propTypes = {
  object: PropTypes.object.isRequired,
  // @TODO Change this to Object.values once it's supported in Node's version of V8
  mode: PropTypes.oneOf(Object.keys(MODE).map(key => MODE[key]))
};

function CommentNode(props) {
  let {
    object,
    mode = MODE.SHORT
  } = props;

  let { textContent } = object.preview;
  if (mode === MODE.TINY) {
    textContent = cropMultipleLines(textContent, 30);
  } else if (mode === MODE.SHORT) {
    textContent = cropString(textContent, 50);
  }

  return span({
    className: "objectBox theme-comment",
    "data-link-actor-id": object.actor
  }, `<!-- ${textContent} -->`);
}

// Registration
function supportsObject(object, noGrip = false) {
  if (noGrip === true || !isGrip(object)) {
    return false;
  }
  return object.preview && object.preview.nodeType === nodeConstants.COMMENT_NODE;
}

// Exports from this module
module.exports = {
  rep: wrapRender(CommentNode),
  supportsObject
};

/***/ }),
/* 40 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// ReactJS
const PropTypes = __webpack_require__(2);

// Utils
const {
  isGrip,
  wrapRender
} = __webpack_require__(0);
const { rep: StringRep } = __webpack_require__(7);
const { MODE } = __webpack_require__(3);
const nodeConstants = __webpack_require__(12);
const Svg = __webpack_require__(9);

const dom = __webpack_require__(1);
const { span } = dom;

/**
 * Renders DOM element node.
 */
ElementNode.propTypes = {
  object: PropTypes.object.isRequired,
  // @TODO Change this to Object.values once it's supported in Node's version of V8
  mode: PropTypes.oneOf(Object.keys(MODE).map(key => MODE[key])),
  onDOMNodeMouseOver: PropTypes.func,
  onDOMNodeMouseOut: PropTypes.func,
  onInspectIconClick: PropTypes.func
};

function ElementNode(props) {
  let {
    object,
    mode,
    onDOMNodeMouseOver,
    onDOMNodeMouseOut,
    onInspectIconClick
  } = props;
  let elements = getElements(object, mode);

  let isInTree = object.preview && object.preview.isConnected === true;

  let baseConfig = {
    "data-link-actor-id": object.actor,
    className: "objectBox objectBox-node"
  };
  let inspectIcon;
  if (isInTree) {
    if (onDOMNodeMouseOver) {
      Object.assign(baseConfig, {
        onMouseOver: _ => onDOMNodeMouseOver(object)
      });
    }

    if (onDOMNodeMouseOut) {
      Object.assign(baseConfig, {
        onMouseOut: onDOMNodeMouseOut
      });
    }

    if (onInspectIconClick) {
      inspectIcon = Svg("open-inspector", {
        element: "a",
        draggable: false,
        // TODO: Localize this with "openNodeInInspector" when Bug 1317038 lands
        title: "Click to select the node in the inspector",
        onClick: e => onInspectIconClick(object, e)
      });
    }
  }

  return span(baseConfig, ...elements, inspectIcon);
}

function getElements(grip, mode) {
  let { attributes, nodeName } = grip.preview;
  const nodeNameElement = span({
    className: "tag-name"
  }, nodeName);

  if (mode === MODE.TINY) {
    let elements = [nodeNameElement];
    if (attributes.id) {
      elements.push(span({ className: "attrName" }, `#${attributes.id}`));
    }
    if (attributes.class) {
      elements.push(span({ className: "attrName" }, attributes.class.trim().split(/\s+/).map(cls => `.${cls}`).join("")));
    }
    return elements;
  }
  let attributeKeys = Object.keys(attributes);
  if (attributeKeys.includes("class")) {
    attributeKeys.splice(attributeKeys.indexOf("class"), 1);
    attributeKeys.unshift("class");
  }
  if (attributeKeys.includes("id")) {
    attributeKeys.splice(attributeKeys.indexOf("id"), 1);
    attributeKeys.unshift("id");
  }
  const attributeElements = attributeKeys.reduce((arr, name, i, keys) => {
    let value = attributes[name];
    let attribute = span({}, span({ className: "attrName" }, name), span({ className: "attrEqual" }, "="), StringRep({ className: "attrValue", object: value }));

    return arr.concat([" ", attribute]);
  }, []);

  return [span({ className: "angleBracket" }, "<"), nodeNameElement, ...attributeElements, span({ className: "angleBracket" }, ">")];
}

// Registration
function supportsObject(object, noGrip = false) {
  if (noGrip === true || !isGrip(object)) {
    return false;
  }
  return object.preview && object.preview.nodeType === nodeConstants.ELEMENT_NODE;
}

// Exports from this module
module.exports = {
  rep: wrapRender(ElementNode),
  supportsObject
};

/***/ }),
/* 41 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// ReactJS
const PropTypes = __webpack_require__(2);

// Reps
const {
  isGrip,
  cropString,
  wrapRender
} = __webpack_require__(0);
const { MODE } = __webpack_require__(3);
const Svg = __webpack_require__(9);

const dom = __webpack_require__(1);
const { span } = dom;

/**
 * Renders DOM #text node.
 */
TextNode.propTypes = {
  object: PropTypes.object.isRequired,
  // @TODO Change this to Object.values once it's supported in Node's version of V8
  mode: PropTypes.oneOf(Object.keys(MODE).map(key => MODE[key])),
  onDOMNodeMouseOver: PropTypes.func,
  onDOMNodeMouseOut: PropTypes.func,
  onInspectIconClick: PropTypes.func
};

function TextNode(props) {
  let {
    object: grip,
    mode = MODE.SHORT,
    onDOMNodeMouseOver,
    onDOMNodeMouseOut,
    onInspectIconClick
  } = props;

  let baseConfig = {
    "data-link-actor-id": grip.actor,
    className: "objectBox objectBox-textNode"
  };
  let inspectIcon;
  let isInTree = grip.preview && grip.preview.isConnected === true;

  if (isInTree) {
    if (onDOMNodeMouseOver) {
      Object.assign(baseConfig, {
        onMouseOver: _ => onDOMNodeMouseOver(grip)
      });
    }

    if (onDOMNodeMouseOut) {
      Object.assign(baseConfig, {
        onMouseOut: onDOMNodeMouseOut
      });
    }

    if (onInspectIconClick) {
      inspectIcon = Svg("open-inspector", {
        element: "a",
        draggable: false,
        // TODO: Localize this with "openNodeInInspector" when Bug 1317038 lands
        title: "Click to select the node in the inspector",
        onClick: e => onInspectIconClick(grip, e)
      });
    }
  }

  if (mode === MODE.TINY) {
    return span(baseConfig, getTitle(grip), inspectIcon);
  }

  return span(baseConfig, getTitle(grip), span({ className: "nodeValue" }, " ", `"${getTextContent(grip)}"`), inspectIcon);
}

function getTextContent(grip) {
  return cropString(grip.preview.textContent);
}

function getTitle(grip) {
  const title = "#text";
  return span({}, title);
}

// Registration
function supportsObject(grip, noGrip = false) {
  if (noGrip === true || !isGrip(grip)) {
    return false;
  }

  return grip.preview && grip.class == "Text";
}

// Exports from this module
module.exports = {
  rep: wrapRender(TextNode),
  supportsObject
};

/***/ }),
/* 42 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// ReactJS
const PropTypes = __webpack_require__(2);
// Utils
const {
  getGripType,
  isGrip,
  wrapRender
} = __webpack_require__(0);
const { MODE } = __webpack_require__(3);

const dom = __webpack_require__(1);
const { span } = dom;

/**
 * Renders Error objects.
 */
ErrorRep.propTypes = {
  object: PropTypes.object.isRequired,
  // @TODO Change this to Object.values once it's supported in Node's version of V8
  mode: PropTypes.oneOf(Object.keys(MODE).map(key => MODE[key]))
};

function ErrorRep(props) {
  let object = props.object;
  let preview = object.preview;

  let name;
  if (preview && preview.name && preview.kind) {
    switch (preview.kind) {
      case "Error":
        name = preview.name;
        break;
      case "DOMException":
        name = preview.kind;
        break;
      default:
        throw new Error("Unknown preview kind for the Error rep.");
    }
  } else {
    name = "Error";
  }

  let content = props.mode === MODE.TINY ? name : `${name}: ${preview.message}`;

  if (preview.stack && props.mode !== MODE.TINY) {
    /*
      * Since Reps are used in the JSON Viewer, we can't localize
      * the "Stack trace" label (defined in debugger.properties as
      * "variablesViewErrorStacktrace" property), until Bug 1317038 lands.
      */
    content = `${content}\nStack trace:\n${preview.stack}`;
  }

  return span({
    "data-link-actor-id": object.actor,
    className: "objectBox-stackTrace"
  }, content);
}

// Registration
function supportsObject(object, noGrip = false) {
  if (noGrip === true || !isGrip(object)) {
    return false;
  }
  return object.preview && getGripType(object, noGrip) === "Error" || object.class === "DOMException";
}

// Exports from this module
module.exports = {
  rep: wrapRender(ErrorRep),
  supportsObject
};

/***/ }),
/* 43 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// ReactJS
const PropTypes = __webpack_require__(2);

// Reps
const {
  getGripType,
  isGrip,
  getURLDisplayString,
  wrapRender
} = __webpack_require__(0);

const { MODE } = __webpack_require__(3);

const dom = __webpack_require__(1);
const { span } = dom;

/**
 * Renders a grip representing a window.
 */
WindowRep.propTypes = {
  // @TODO Change this to Object.values once it's supported in Node's version of V8
  mode: PropTypes.oneOf(Object.keys(MODE).map(key => MODE[key])),
  object: PropTypes.object.isRequired
};

function WindowRep(props) {
  let {
    mode,
    object
  } = props;

  const config = {
    "data-link-actor-id": object.actor,
    className: "objectBox objectBox-Window"
  };

  if (mode === MODE.TINY) {
    return span(config, getTitle(object));
  }

  return span(config, getTitle(object), " ", span({ className: "location" }, getLocation(object)));
}

function getTitle(object) {
  let title = object.displayClass || object.class || "Window";
  return span({ className: "objectTitle" }, title);
}

function getLocation(object) {
  return getURLDisplayString(object.preview.url);
}

// Registration
function supportsObject(object, noGrip = false) {
  if (noGrip === true || !isGrip(object)) {
    return false;
  }

  return object.preview && getGripType(object, noGrip) == "Window";
}

// Exports from this module
module.exports = {
  rep: wrapRender(WindowRep),
  supportsObject
};

/***/ }),
/* 44 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// ReactJS
const PropTypes = __webpack_require__(2);

// Reps
const {
  isGrip,
  wrapRender
} = __webpack_require__(0);

const dom = __webpack_require__(1);
const { span } = dom;

/**
 * Renders a grip object with textual data.
 */
ObjectWithText.propTypes = {
  object: PropTypes.object.isRequired
};

function ObjectWithText(props) {
  let grip = props.object;
  return span({
    "data-link-actor-id": grip.actor,
    className: "objectBox objectBox-" + getType(grip)
  }, span({ className: "objectPropValue" }, getDescription(grip)));
}

function getType(grip) {
  return grip.class;
}

function getDescription(grip) {
  return "\"" + grip.preview.text + "\"";
}

// Registration
function supportsObject(grip, noGrip = false) {
  if (noGrip === true || !isGrip(grip)) {
    return false;
  }

  return grip.preview && grip.preview.kind == "ObjectWithText";
}

// Exports from this module
module.exports = {
  rep: wrapRender(ObjectWithText),
  supportsObject
};

/***/ }),
/* 45 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// ReactJS
const PropTypes = __webpack_require__(2);

// Reps
const {
  isGrip,
  getURLDisplayString,
  wrapRender
} = __webpack_require__(0);

const dom = __webpack_require__(1);
const { span } = dom;

/**
 * Renders a grip object with URL data.
 */
ObjectWithURL.propTypes = {
  object: PropTypes.object.isRequired
};

function ObjectWithURL(props) {
  let grip = props.object;
  return span({
    "data-link-actor-id": grip.actor,
    className: "objectBox objectBox-" + getType(grip)
  }, getTitle(grip), span({ className: "objectPropValue" }, getDescription(grip)));
}

function getTitle(grip) {
  return span({ className: "objectTitle" }, getType(grip) + " ");
}

function getType(grip) {
  return grip.class;
}

function getDescription(grip) {
  return getURLDisplayString(grip.preview.url);
}

// Registration
function supportsObject(grip, noGrip = false) {
  if (noGrip === true || !isGrip(grip)) {
    return false;
  }

  return grip.preview && grip.preview.kind == "ObjectWithURL";
}

// Exports from this module
module.exports = {
  rep: wrapRender(ObjectWithURL),
  supportsObject
};

/***/ }),
/* 46 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


var _devtoolsComponents = __webpack_require__(47);

var _devtoolsComponents2 = _interopRequireDefault(_devtoolsComponents);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Component, createFactory } = __webpack_require__(6);
const PropTypes = __webpack_require__(2);
const dom = __webpack_require__(1);

const Tree = createFactory(_devtoolsComponents2.default.Tree);
__webpack_require__(51);

const classnames = __webpack_require__(52);

const {
  REPS: {
    Rep,
    Grip
  }
} = __webpack_require__(4);
const {
  MODE
} = __webpack_require__(3);

const {
  getChildren,
  getClosestGripNode,
  getParent,
  getValue,
  nodeHasAccessors,
  nodeHasProperties,
  nodeIsDefaultProperties,
  nodeIsFunction,
  nodeIsGetter,
  nodeIsMapEntry,
  nodeIsMissingArguments,
  nodeIsOptimizedOut,
  nodeIsPrimitive,
  nodeIsPrototype,
  nodeIsSetter,
  nodeIsWindow,
  shouldLoadItemEntries,
  shouldLoadItemIndexedProperties,
  shouldLoadItemNonIndexedProperties,
  shouldLoadItemPrototype,
  shouldLoadItemSymbols
} = __webpack_require__(16);

const {
  enumEntries,
  enumIndexedProperties,
  enumNonIndexedProperties,
  getPrototype,
  enumSymbols
} = __webpack_require__(54);

// This implements a component that renders an interactive inspector
// for looking at JavaScript objects. It expects descriptions of
// objects from the protocol, and will dynamically fetch child
// properties as objects are expanded.
//
// If you want to inspect a single object, pass the name and the
// protocol descriptor of it:
//
//  ObjectInspector({
//    name: "foo",
//    desc: { writable: true, ..., { value: { actor: "1", ... }}},
//    ...
//  })
//
// If you want multiple top-level objects (like scopes), you can pass
// an array of manually constructed nodes as `roots`:
//
//  ObjectInspector({
//    roots: [{ name: ... }, ...],
//    ...
//  });

// There are 3 types of nodes: a simple node with a children array, an
// object that has properties that should be children when they are
// fetched, and a primitive value that should be displayed with no
// children.

class ObjectInspector extends Component {
  constructor(props) {
    super();
    this.cachedNodes = new Map();

    this.state = {
      actors: new Set(),
      expandedPaths: new Set(),
      focusedItem: null,
      loadedProperties: props.loadedProperties || new Map(),
      loading: new Map()
    };

    const self = this;

    self.getChildren = this.getChildren.bind(this);
    self.renderTreeItem = this.renderTreeItem.bind(this);
    self.setExpanded = this.setExpanded.bind(this);
    self.focusItem = this.focusItem.bind(this);
    self.getRoots = this.getRoots.bind(this);
  }

  shouldComponentUpdate(nextProps, nextState) {
    const {
      expandedPaths,
      loadedProperties
    } = this.state;
    return this.props.roots !== nextProps.roots || expandedPaths.size !== nextState.expandedPaths.size || loadedProperties.size !== nextState.loadedProperties.size || [...expandedPaths].some(key => !nextState.expandedPaths.has(key));
  }

  componentWillUnmount() {
    const { releaseActor } = this.props;
    if (typeof releaseActor !== "function") {
      return;
    }

    const { actors } = this.state;
    for (let actor of actors) {
      releaseActor(actor);
    }
  }

  getChildren(item) {
    const {
      loadedProperties
    } = this.state;
    const { cachedNodes } = this;

    return getChildren({
      loadedProperties,
      cachedNodes,
      item
    });
  }

  getRoots() {
    return this.props.roots;
  }

  getKey(item) {
    return item.path;
  }

  /**
   * This function is responsible for expanding/collapsing a given node,
   * which also means that it will check if we need to fetch properties,
   * entries, prototype and symbols for the said node. If we do, it will call
   * the appropriate ObjectClient functions, and change the state of the component
   * with the results it gets from those functions.
   */
  async setExpanded(item, expand) {
    if (nodeIsPrimitive(item)) {
      return;
    }

    const {
      loadedProperties
    } = this.state;

    const key = this.getKey(item);

    this.setState((prevState, props) => {
      const newPaths = new Set(prevState.expandedPaths);
      if (expand === true) {
        newPaths.add(key);
      } else {
        newPaths.delete(key);
      }
      return {
        expandedPaths: newPaths
      };
    });

    if (expand === true) {
      const gripItem = getClosestGripNode(item);
      const value = getValue(gripItem);

      const path = item.path;
      const [start, end] = item.meta ? [item.meta.startIndex, item.meta.endIndex] : [];

      let promises = [];
      let objectClient;
      const getObjectClient = () => {
        if (objectClient) {
          return objectClient;
        }
        return this.props.createObjectClient(value);
      };

      if (shouldLoadItemIndexedProperties(item, loadedProperties)) {
        promises.push(enumIndexedProperties(getObjectClient(), start, end));
      }

      if (shouldLoadItemNonIndexedProperties(item, loadedProperties)) {
        promises.push(enumNonIndexedProperties(getObjectClient(), start, end));
      }

      if (shouldLoadItemEntries(item, loadedProperties)) {
        promises.push(enumEntries(getObjectClient(), start, end));
      }

      if (shouldLoadItemPrototype(item, loadedProperties)) {
        promises.push(getPrototype(getObjectClient()));
      }

      if (shouldLoadItemSymbols(item, loadedProperties)) {
        promises.push(enumSymbols(getObjectClient(), start, end));
      }

      if (promises.length > 0) {
        // Set the loading state with the pending promises.
        this.setState((prevState, props) => {
          const nextLoading = new Map(prevState.loading);
          nextLoading.set(path, promises);
          return {
            loading: nextLoading
          };
        });

        const responses = await Promise.all(promises);

        // Let's loop through the responses to build a single response object.
        const response = responses.reduce((accumulator, res) => {
          Object.entries(res).forEach(([k, v]) => {
            if (accumulator.hasOwnProperty(k)) {
              if (Array.isArray(accumulator[k])) {
                accumulator[k].push(...v);
              } else if (typeof accumulator[k] === "object") {
                accumulator[k] = Object.assign({}, accumulator[k], v);
              }
            } else {
              accumulator[k] = v;
            }
          });
          return accumulator;
        }, {});

        this.setState((prevState, props) => {
          const nextLoading = new Map(prevState.loading);
          nextLoading.delete(path);

          const isRoot = this.props.roots.some(root => {
            const rootValue = getValue(root);
            return rootValue && rootValue.actor === value.actor;
          });

          return {
            actors: isRoot ? prevState.actors : new Set(prevState.actors).add(value.actor),
            loadedProperties: new Map(prevState.loadedProperties).set(path, response),
            loading: nextLoading
          };
        });
      }
    }
  }

  focusItem(item) {
    if (!this.props.disabledFocus && this.state.focusedItem !== item) {
      this.setState({
        focusedItem: item
      });

      if (this.props.onFocus) {
        this.props.onFocus(item);
      }
    }
  }

  renderTreeItem(item, depth, focused, arrow, expanded) {
    let objectValue;
    let label = item.name;
    let itemValue = getValue(item);

    const isPrimitive = nodeIsPrimitive(item);

    const unavailable = isPrimitive && itemValue && itemValue.hasOwnProperty && itemValue.hasOwnProperty("unavailable");

    if (nodeIsOptimizedOut(item)) {
      objectValue = dom.span({ className: "unavailable" }, "(optimized away)");
    } else if (nodeIsMissingArguments(item) || unavailable) {
      objectValue = dom.span({ className: "unavailable" }, "(unavailable)");
    } else if (nodeIsFunction(item) && !nodeIsGetter(item) && !nodeIsSetter(item) && (this.props.mode === MODE.TINY || !this.props.mode)) {
      objectValue = undefined;
      label = this.renderGrip(item, Object.assign({}, this.props, {
        functionName: label
      }));
    } else if (nodeHasProperties(item) || nodeHasAccessors(item) || nodeIsMapEntry(item) || isPrimitive) {
      let repsProp = Object.assign({}, this.props);
      if (depth > 0) {
        repsProp.mode = this.props.mode === MODE.LONG ? MODE.SHORT : MODE.TINY;
      }
      if (expanded) {
        repsProp.mode = MODE.TINY;
      }

      objectValue = this.renderGrip(item, repsProp);
    }

    const hasLabel = label !== null && typeof label !== "undefined";
    const hasValue = typeof objectValue !== "undefined";

    const {
      onDoubleClick,
      onLabelClick,
      dimTopLevelWindow
    } = this.props;

    return dom.div({
      className: classnames("node object-node", {
        focused,
        lessen: !expanded && (nodeIsDefaultProperties(item) || nodeIsPrototype(item) || dimTopLevelWindow === true && nodeIsWindow(item) && depth === 0)
      }),
      onClick: e => {
        e.stopPropagation();
        if (isPrimitive === false) {
          this.setExpanded(item, !expanded);
        }
      },
      onDoubleClick: onDoubleClick ? e => {
        e.stopPropagation();
        onDoubleClick(item, {
          depth,
          focused,
          expanded
        });
      } : null
    }, arrow, hasLabel ? dom.span({
      className: "object-label",
      onClick: onLabelClick ? event => {
        event.stopPropagation();
        onLabelClick(item, {
          depth,
          focused,
          expanded,
          setExpanded: this.setExpanded
        });
      } : null
    }, label) : null, hasLabel && hasValue ? dom.span({ className: "object-delimiter" }, ": ") : null, hasValue ? objectValue : null);
  }

  renderGrip(item, props) {
    const object = getValue(item);
    return Rep(Object.assign({}, props, {
      object,
      mode: props.mode || MODE.TINY,
      defaultRep: Grip
    }));
  }

  render() {
    const {
      autoExpandDepth = 1,
      autoExpandAll = true,
      disabledFocus,
      inline,
      itemHeight = 20,
      disableWrap = false
    } = this.props;

    const {
      expandedPaths,
      focusedItem
    } = this.state;

    let roots = this.getRoots();
    if (roots.length === 1) {
      const root = roots[0];
      const name = root && root.name;
      if (nodeIsPrimitive(root) && (name === null || typeof name === "undefined")) {
        return this.renderGrip(root, this.props);
      }
    }

    return Tree({
      className: classnames({
        inline,
        nowrap: disableWrap,
        "object-inspector": true
      }),
      autoExpandAll,
      autoExpandDepth,
      disabledFocus,
      itemHeight,

      isExpanded: item => expandedPaths.has(this.getKey(item)),
      isExpandable: item => nodeIsPrimitive(item) === false,
      focused: focusedItem,

      getRoots: this.getRoots,
      getParent,
      getChildren: this.getChildren,
      getKey: this.getKey,

      onExpand: item => this.setExpanded(item, true),
      onCollapse: item => this.setExpanded(item, false),
      onFocus: this.focusItem,

      renderItem: this.renderTreeItem
    });
  }
}

ObjectInspector.displayName = "ObjectInspector";

ObjectInspector.propTypes = {
  autoExpandAll: PropTypes.bool,
  autoExpandDepth: PropTypes.number,
  disabledFocus: PropTypes.bool,
  disableWrap: PropTypes.bool,
  inline: PropTypes.bool,
  roots: PropTypes.array,
  itemHeight: PropTypes.number,
  mode: PropTypes.oneOf(Object.values(MODE)),
  createObjectClient: PropTypes.func.isRequired,
  onFocus: PropTypes.func,
  onDoubleClick: PropTypes.func,
  onLabelClick: PropTypes.func
};

module.exports = ObjectInspector;

/***/ }),
/* 47 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


Object.defineProperty(exports, "__esModule", {
  value: true
});

var _tree = __webpack_require__(48);

var _tree2 = _interopRequireDefault(_tree);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

exports.default = {
  Tree: _tree2.default
}; /* This Source Code Form is subject to the terms of the Mozilla Public
    * License, v. 2.0. If a copy of the MPL was not distributed with this
    * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/***/ }),
/* 48 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


Object.defineProperty(exports, "__esModule", {
  value: true
});

var _react = __webpack_require__(6);

var _react2 = _interopRequireDefault(_react);

var _reactDomFactories = __webpack_require__(1);

var _reactDomFactories2 = _interopRequireDefault(_reactDomFactories);

var _propTypes = __webpack_require__(2);

var _propTypes2 = _interopRequireDefault(_propTypes);

var _svgInlineReact = __webpack_require__(11);

var _svgInlineReact2 = _interopRequireDefault(_svgInlineReact);

var _arrow = __webpack_require__(49);

var _arrow2 = _interopRequireDefault(_arrow);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

const { Component, createFactory, createElement } = _react2.default; /* This Source Code Form is subject to the terms of the Mozilla Public
                                                                      * License, v. 2.0. If a copy of the MPL was not distributed with this
                                                                      * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

__webpack_require__(50);

const AUTO_EXPAND_DEPTH = 0; // depth

/**
 * An arrow that displays whether its node is expanded (▼) or collapsed
 * (▶). When its node has no children, it is hidden.
 */
class ArrowExpander extends Component {
  static get propTypes() {
    return {
      expanded: _propTypes2.default.bool
    };
  }

  shouldComponentUpdate(nextProps, nextState) {
    return this.props.expanded !== nextProps.expanded;
  }

  render() {
    const {
      expanded
    } = this.props;

    const classNames = ["arrow"];
    if (expanded) {
      classNames.push("expanded");
    }
    return createElement(_svgInlineReact2.default, {
      className: classNames.join(" "),
      src: _arrow2.default
    });
  }
}

const treeIndent = _reactDomFactories2.default.span({ className: "tree-indent" }, "\u200B");

class TreeNode extends Component {
  static get propTypes() {
    return {
      id: _propTypes2.default.any.isRequired,
      index: _propTypes2.default.number.isRequired,
      depth: _propTypes2.default.number.isRequired,
      focused: _propTypes2.default.bool.isRequired,
      expanded: _propTypes2.default.bool.isRequired,
      item: _propTypes2.default.any.isRequired,
      isExpandable: _propTypes2.default.bool.isRequired,
      onClick: _propTypes2.default.func,
      renderItem: _propTypes2.default.func.isRequired
    };
  }

  shouldComponentUpdate(nextProps) {
    return this.props.item !== nextProps.item || this.props.focused !== nextProps.focused || this.props.expanded !== nextProps.expanded;
  }

  render() {
    const {
      depth,
      id,
      item,
      focused,
      expanded,
      renderItem,
      isExpandable
    } = this.props;

    const arrow = isExpandable ? ArrowExpanderFactory({
      item,
      expanded
    }) : null;

    let ariaExpanded;
    if (this.props.isExpandable) {
      ariaExpanded = false;
    }
    if (this.props.expanded) {
      ariaExpanded = true;
    }

    const indents = Array.from({ length: depth }).fill(treeIndent);
    let items = indents.concat(renderItem(item, depth, focused, arrow, expanded));

    return _reactDomFactories2.default.div({
      id,
      className: "tree-node" + (focused ? " focused" : ""),
      onClick: this.props.onClick,
      role: "treeitem",
      "aria-level": depth,
      "aria-expanded": ariaExpanded,
      "data-expandable": this.props.isExpandable
    }, ...items);
  }
}

const ArrowExpanderFactory = createFactory(ArrowExpander);
const TreeNodeFactory = createFactory(TreeNode);

/**
 * Create a function that calls the given function `fn` only once per animation
 * frame.
 *
 * @param {Function} fn
 * @returns {Function}
 */
function oncePerAnimationFrame(fn) {
  let animationId = null;
  let argsToPass = null;
  return function (...args) {
    argsToPass = args;
    if (animationId !== null) {
      return;
    }

    animationId = requestAnimationFrame(() => {
      fn.call(this, ...argsToPass);
      animationId = null;
      argsToPass = null;
    });
  };
}

/**
 * A generic tree component. See propTypes for the public API.
 *
 * This tree component doesn't make any assumptions about the structure of your
 * tree data. Whether children are computed on demand, or stored in an array in
 * the parent's `_children` property, it doesn't matter. We only require the
 * implementation of `getChildren`, `getRoots`, `getParent`, and `isExpanded`
 * functions.
 *
 * This tree component is well tested and reliable. See the tests in ./tests
 * and its usage in the performance and memory panels in mozilla-central.
 *
 * This tree component doesn't make any assumptions about how to render items in
 * the tree. You provide a `renderItem` function, and this component will ensure
 * that only those items whose parents are expanded and which are visible in the
 * viewport are rendered. The `renderItem` function could render the items as a
 * "traditional" tree or as rows in a table or anything else. It doesn't
 * restrict you to only one certain kind of tree.
 *
 * The tree comes with basic styling for the indent, the arrow, as well as hovered
 * and focused styles which can be override in CSS.
 *
 * ### Example Usage
 *
 * Suppose we have some tree data where each item has this form:
 *
 *     {
 *       id: Number,
 *       label: String,
 *       parent: Item or null,
 *       children: Array of child items,
 *       expanded: bool,
 *     }
 *
 * Here is how we could render that data with this component:
 *
 *     class MyTree extends Component {
 *       static get propTypes() {
 *         // The root item of the tree, with the form described above.
 *         return {
 *           root: PropTypes.object.isRequired
 *         };
 *       },
 *
 *       render() {
 *         return Tree({
 *           itemHeight: 20, // px
 *
 *           getRoots: () => [this.props.root],
 *
 *           getParent: item => item.parent,
 *           getChildren: item => item.children,
 *           getKey: item => item.id,
 *           isExpanded: item => item.expanded,
 *
 *           renderItem: (item, depth, isFocused, arrow, isExpanded) => {
 *             let className = "my-tree-item";
 *             if (isFocused) {
 *               className += " focused";
 *             }
 *             return dom.div({
 *               className,
 *             },
 *               arrow,
 *               // And here is the label for this item.
 *               dom.span({ className: "my-tree-item-label" }, item.label)
 *             );
 *           },
 *
 *           onExpand: item => dispatchExpandActionToRedux(item),
 *           onCollapse: item => dispatchCollapseActionToRedux(item),
 *         });
 *       }
 *     }
 */
class Tree extends Component {
  static get propTypes() {
    return {
      // Required props

      // A function to get an item's parent, or null if it is a root.
      //
      // Type: getParent(item: Item) -> Maybe<Item>
      //
      // Example:
      //
      //     // The parent of this item is stored in its `parent` property.
      //     getParent: item => item.parent
      getParent: _propTypes2.default.func.isRequired,

      // A function to get an item's children.
      //
      // Type: getChildren(item: Item) -> [Item]
      //
      // Example:
      //
      //     // This item's children are stored in its `children` property.
      //     getChildren: item => item.children
      getChildren: _propTypes2.default.func.isRequired,

      // A function which takes an item and ArrowExpander component instance and
      // returns a component, or text, or anything else that React considers
      // renderable.
      //
      // Type: renderItem(item: Item,
      //                  depth: Number,
      //                  isFocused: Boolean,
      //                  arrow: ReactComponent,
      //                  isExpanded: Boolean) -> ReactRenderable
      //
      // Example:
      //
      //     renderItem: (item, depth, isFocused, arrow, isExpanded) => {
      //       let className = "my-tree-item";
      //       if (isFocused) {
      //         className += " focused";
      //       }
      //       return dom.div(
      //         {
      //           className,
      //           style: { marginLeft: depth * 10 + "px" }
      //         },
      //         arrow,
      //         dom.span({ className: "my-tree-item-label" }, item.label)
      //       );
      //     },
      renderItem: _propTypes2.default.func.isRequired,

      // A function which returns the roots of the tree (forest).
      //
      // Type: getRoots() -> [Item]
      //
      // Example:
      //
      //     // In this case, we only have one top level, root item. You could
      //     // return multiple items if you have many top level items in your
      //     // tree.
      //     getRoots: () => [this.props.rootOfMyTree]
      getRoots: _propTypes2.default.func.isRequired,

      // A function to get a unique key for the given item. This helps speed up
      // React's rendering a *TON*.
      //
      // Type: getKey(item: Item) -> String
      //
      // Example:
      //
      //     getKey: item => `my-tree-item-${item.uniqueId}`
      getKey: _propTypes2.default.func.isRequired,

      // A function to get whether an item is expanded or not. If an item is not
      // expanded, then it must be collapsed.
      //
      // Type: isExpanded(item: Item) -> Boolean
      //
      // Example:
      //
      //     isExpanded: item => item.expanded,
      isExpanded: _propTypes2.default.func.isRequired,

      // Optional props

      // The currently focused item, if any such item exists.
      focused: _propTypes2.default.any,

      // Handle when a new item is focused.
      onFocus: _propTypes2.default.func,

      // The depth to which we should automatically expand new items.
      autoExpandDepth: _propTypes2.default.number,
      // Should auto expand all new items or just the new items under the first
      // root item.
      autoExpandAll: _propTypes2.default.bool,

      // Note: the two properties below are mutually exclusive. Only one of the
      // label properties is necessary.
      // ID of an element whose textual content serves as an accessible label for
      // a tree.
      labelledby: _propTypes2.default.string,
      // Accessibility label for a tree widget.
      label: _propTypes2.default.string,

      // Optional event handlers for when items are expanded or collapsed. Useful
      // for dispatching redux events and updating application state, maybe lazily
      // loading subtrees from a worker, etc.
      //
      // Type:
      //     onExpand(item: Item)
      //     onCollapse(item: Item)
      //
      // Example:
      //
      //     onExpand: item => dispatchExpandActionToRedux(item)
      onExpand: _propTypes2.default.func,
      onCollapse: _propTypes2.default.func,
      isExpandable: _propTypes2.default.func,
      // Additional classes to add to the root element.
      className: _propTypes2.default.string,
      // style object to be applied to the root element.
      style: _propTypes2.default.object
    };
  }

  static get defaultProps() {
    return {
      autoExpandDepth: AUTO_EXPAND_DEPTH,
      autoExpandAll: true
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      seen: new Set()
    };

    this._onExpand = oncePerAnimationFrame(this._onExpand).bind(this);
    this._onCollapse = oncePerAnimationFrame(this._onCollapse).bind(this);
    this._focusPrevNode = oncePerAnimationFrame(this._focusPrevNode).bind(this);
    this._focusNextNode = oncePerAnimationFrame(this._focusNextNode).bind(this);
    this._focusParentNode = oncePerAnimationFrame(this._focusParentNode).bind(this);

    this._autoExpand = this._autoExpand.bind(this);
    this._preventArrowKeyScrolling = this._preventArrowKeyScrolling.bind(this);
    this._dfs = this._dfs.bind(this);
    this._dfsFromRoots = this._dfsFromRoots.bind(this);
    this._focus = this._focus.bind(this);
    this._scrollNodeIntoView = this._scrollNodeIntoView.bind(this);
    this._onBlur = this._onBlur.bind(this);
    this._onKeyDown = this._onKeyDown.bind(this);
    this._nodeIsExpandable = this._nodeIsExpandable.bind(this);
  }

  componentDidMount() {
    this._autoExpand();
    if (this.props.focused) {
      this._scrollNodeIntoView(this.props.focused);
    }
  }

  componentWillReceiveProps(nextProps) {
    this._autoExpand();
  }

  componentDidUpdate(prevProps, prevState) {
    if (prevProps.focused !== this.props.focused) {
      this._scrollNodeIntoView(this.props.focused);
    }
  }

  _autoExpand() {
    if (!this.props.autoExpandDepth) {
      return;
    }

    // Automatically expand the first autoExpandDepth levels for new items. Do
    // not use the usual DFS infrastructure because we don't want to ignore
    // collapsed nodes.
    const autoExpand = (item, currentDepth) => {
      if (currentDepth >= this.props.autoExpandDepth || this.state.seen.has(item)) {
        return;
      }

      this.props.onExpand(item);
      this.state.seen.add(item);

      const children = this.props.getChildren(item);
      const length = children.length;
      for (let i = 0; i < length; i++) {
        autoExpand(children[i], currentDepth + 1);
      }
    };

    const roots = this.props.getRoots();
    const length = roots.length;
    if (this.props.autoExpandAll) {
      for (let i = 0; i < length; i++) {
        autoExpand(roots[i], 0);
      }
    } else if (length != 0) {
      autoExpand(roots[0], 0);
    }
  }

  _preventArrowKeyScrolling(e) {
    switch (e.key) {
      case "ArrowUp":
      case "ArrowDown":
      case "ArrowLeft":
      case "ArrowRight":
        e.preventDefault();
        e.stopPropagation();
        if (e.nativeEvent) {
          if (e.nativeEvent.preventDefault) {
            e.nativeEvent.preventDefault();
          }
          if (e.nativeEvent.stopPropagation) {
            e.nativeEvent.stopPropagation();
          }
        }
    }
  }

  /**
   * Perform a pre-order depth-first search from item.
   */
  _dfs(item, maxDepth = Infinity, traversal = [], _depth = 0) {
    traversal.push({ item, depth: _depth });

    if (!this.props.isExpanded(item)) {
      return traversal;
    }

    const nextDepth = _depth + 1;

    if (nextDepth > maxDepth) {
      return traversal;
    }

    const children = this.props.getChildren(item);
    const length = children.length;
    for (let i = 0; i < length; i++) {
      this._dfs(children[i], maxDepth, traversal, nextDepth);
    }

    return traversal;
  }

  /**
   * Perform a pre-order depth-first search over the whole forest.
   */
  _dfsFromRoots(maxDepth = Infinity) {
    const traversal = [];

    const roots = this.props.getRoots();
    const length = roots.length;
    for (let i = 0; i < length; i++) {
      this._dfs(roots[i], maxDepth, traversal);
    }

    return traversal;
  }

  /**
   * Expands current row.
   *
   * @param {Object} item
   * @param {Boolean} expandAllChildren
   */
  _onExpand(item, expandAllChildren) {
    if (this.props.onExpand) {
      this.props.onExpand(item);

      if (expandAllChildren) {
        const children = this._dfs(item);
        const length = children.length;
        for (let i = 0; i < length; i++) {
          this.props.onExpand(children[i].item);
        }
      }
    }
  }

  /**
   * Collapses current row.
   *
   * @param {Object} item
   */
  _onCollapse(item) {
    if (this.props.onCollapse) {
      this.props.onCollapse(item);
    }
  }

  /**
   * Sets the passed in item to be the focused item.
   *
   * @param {Object|undefined} item
   *        The item to be focused, or undefined to focus no item.
   *
   * @param {Object|undefined} options
   *        An options object which can contain:
   *          - dir: "up" or "down" to indicate if we should scroll the element to the
   *                 top or the bottom of the scrollable container when the element is
   *                 off canvas.
   */
  _focus(item, options) {
    this._scrollNodeIntoView(item, options);
    if (this.props.onFocus) {
      this.props.onFocus(item);
    }
  }

  /**
   * Sets the passed in item to be the focused item.
   *
   * @param {Object|undefined} item
   *        The item to be scrolled to.
   *
   * @param {Object|undefined} options
   *        An options object which can contain:
   *          - dir: "up" or "down" to indicate if we should scroll the element to the
   *                 top or the bottom of the scrollable container when the element is
   *                 off canvas.
   */
  _scrollNodeIntoView(item, options = {}) {
    if (item !== undefined) {
      const treeElement = this.treeRef;
      const element = document.getElementById(this.props.getKey(item));
      if (element) {
        const { top, bottom } = element.getBoundingClientRect();
        const closestScrolledParent = node => {
          if (node == null) {
            return null;
          }

          if (node.scrollHeight > node.clientHeight) {
            return node;
          }
          return closestScrolledParent(node.parentNode);
        };
        const scrolledParent = closestScrolledParent(treeElement);
        const isVisible = !scrolledParent || top >= 0 && bottom <= scrolledParent.clientHeight;

        if (!isVisible) {
          let scrollToTop = !options.alignTo && top < 0 || options.alignTo === "top";
          element.scrollIntoView(scrollToTop);
        }
      }
    }
  }

  /**
   * Sets the state to have no focused item.
   */
  _onBlur() {
    this._focus(undefined);
  }

  /**
   * Handles key down events in the tree's container.
   *
   * @param {Event} e
   */
  _onKeyDown(e) {
    if (this.props.focused == null) {
      return;
    }

    // Allow parent nodes to use navigation arrows with modifiers.
    if (e.altKey || e.ctrlKey || e.shiftKey || e.metaKey) {
      return;
    }

    this._preventArrowKeyScrolling(e);

    switch (e.key) {
      case "ArrowUp":
        this._focusPrevNode();
        return;

      case "ArrowDown":
        this._focusNextNode();
        return;

      case "ArrowLeft":
        if (this.props.isExpanded(this.props.focused) && this._nodeIsExpandable(this.props.focused)) {
          this._onCollapse(this.props.focused);
        } else {
          this._focusParentNode();
        }
        return;

      case "ArrowRight":
        if (this._nodeIsExpandable(this.props.focused) && !this.props.isExpanded(this.props.focused)) {
          this._onExpand(this.props.focused);
        } else {
          this._focusNextNode();
        }
    }
  }

  /**
   * Sets the previous node relative to the currently focused item, to focused.
   */
  _focusPrevNode() {
    // Start a depth first search and keep going until we reach the currently
    // focused node. Focus the previous node in the DFS, if it exists. If it
    // doesn't exist, we're at the first node already.

    let prev;

    const traversal = this._dfsFromRoots();
    const length = traversal.length;
    for (let i = 0; i < length; i++) {
      const item = traversal[i].item;
      if (item === this.props.focused) {
        break;
      }
      prev = item;
    }
    if (prev === undefined) {
      return;
    }

    this._focus(prev, { alignTo: "top" });
  }

  /**
   * Handles the down arrow key which will focus either the next child
   * or sibling row.
   */
  _focusNextNode() {
    // Start a depth first search and keep going until we reach the currently
    // focused node. Focus the next node in the DFS, if it exists. If it
    // doesn't exist, we're at the last node already.
    const traversal = this._dfsFromRoots();
    const length = traversal.length;
    let i = 0;

    while (i < length) {
      if (traversal[i].item === this.props.focused) {
        break;
      }
      i++;
    }

    if (i + 1 < traversal.length) {
      this._focus(traversal[i + 1].item, { alignTo: "bottom" });
    }
  }

  /**
   * Handles the left arrow key, going back up to the current rows'
   * parent row.
   */
  _focusParentNode() {
    const parent = this.props.getParent(this.props.focused);
    if (!parent) {
      this._focusPrevNode(this.props.focused);
      return;
    }

    const traversal = this._dfsFromRoots();
    const length = traversal.length;
    let parentIndex = 0;
    for (; parentIndex < length; parentIndex++) {
      if (traversal[parentIndex].item === parent) {
        break;
      }
    }

    this._focus(parent, { alignTo: "top" });
  }

  _nodeIsExpandable(item) {
    return this.props.isExpandable ? this.props.isExpandable(item) : !!this.props.getChildren(item).length;
  }

  render() {
    const traversal = this._dfsFromRoots();
    const {
      focused
    } = this.props;

    const nodes = traversal.map((v, i) => {
      const { item, depth } = traversal[i];
      const key = this.props.getKey(item, i);
      return TreeNodeFactory({
        key,
        id: key,
        index: i,
        item,
        depth,
        renderItem: this.props.renderItem,
        focused: focused === item,
        expanded: this.props.isExpanded(item),
        isExpandable: this._nodeIsExpandable(item),
        onExpand: this._onExpand,
        onCollapse: this._onCollapse,
        onClick: e => {
          this._focus(item);
          if (this.props.isExpanded(item)) {
            this.props.onCollapse(item);
          } else {
            this.props.onExpand(item, e.altKey);
          }
        }
      });
    });

    const style = Object.assign({}, this.props.style || {}, {
      padding: 0,
      margin: 0
    });

    return _reactDomFactories2.default.div({
      className: `tree ${this.props.className ? this.props.className : ""}`,
      ref: el => {
        this.treeRef = el;
      },
      role: "tree",
      tabIndex: "0",
      onKeyDown: this._onKeyDown,
      onKeyPress: this._preventArrowKeyScrolling,
      onKeyUp: this._preventArrowKeyScrolling,
      onFocus: ({ nativeEvent }) => {
        if (focused || !nativeEvent || !this.treeRef) {
          return;
        }

        let { explicitOriginalTarget } = nativeEvent;
        // Only set default focus to the first tree node if the focus came
        // from outside the tree (e.g. by tabbing to the tree from other
        // external elements).
        if (explicitOriginalTarget !== this.treeRef && !this.treeRef.contains(explicitOriginalTarget)) {
          this._focus(traversal[0].item);
        }
      },
      onBlur: this._onBlur,
      onClick: () => {
        // Focus should always remain on the tree container itself.
        this.treeRef.focus();
      },
      "aria-label": this.props.label,
      "aria-labelledby": this.props.labelledby,
      "aria-activedescendant": focused && this.props.getKey(focused),
      style
    }, nodes);
  }
}

exports.default = Tree;

/***/ }),
/* 49 */
/***/ (function(module, exports) {

module.exports = "<!-- This Source Code Form is subject to the terms of the Mozilla Public - License, v. 2.0. If a copy of the MPL was not distributed with this - file, You can obtain one at http://mozilla.org/MPL/2.0/. --><svg viewBox=\"0 0 16 16\" xmlns=\"http://www.w3.org/2000/svg\"><path d=\"M8 13.4c-.5 0-.9-.2-1.2-.6L.4 5.2C0 4.7-.1 4.3.2 3.7S1 3 1.6 3h12.8c.6 0 1.2.1 1.4.7.3.6.2 1.1-.2 1.6l-6.4 7.6c-.3.4-.7.5-1.2.5z\"></path></svg>"

/***/ }),
/* 50 */
/***/ (function(module, exports) {

// removed by extract-text-webpack-plugin

/***/ }),
/* 51 */
/***/ (function(module, exports) {

// removed by extract-text-webpack-plugin

/***/ }),
/* 52 */
/***/ (function(module, exports, __webpack_require__) {

var __WEBPACK_AMD_DEFINE_ARRAY__, __WEBPACK_AMD_DEFINE_RESULT__;/*!
  Copyright (c) 2016 Jed Watson.
  Licensed under the MIT License (MIT), see
  http://jedwatson.github.io/classnames
*/
/* global define */

(function () {
	'use strict';

	var hasOwn = {}.hasOwnProperty;

	function classNames () {
		var classes = [];

		for (var i = 0; i < arguments.length; i++) {
			var arg = arguments[i];
			if (!arg) continue;

			var argType = typeof arg;

			if (argType === 'string' || argType === 'number') {
				classes.push(arg);
			} else if (Array.isArray(arg)) {
				classes.push(classNames.apply(null, arg));
			} else if (argType === 'object') {
				for (var key in arg) {
					if (hasOwn.call(arg, key) && arg[key]) {
						classes.push(key);
					}
				}
			}
		}

		return classes.join(' ');
	}

	if (typeof module !== 'undefined' && module.exports) {
		module.exports = classNames;
	} else if (true) {
		// register as 'classnames', consistent with npm package name
		!(__WEBPACK_AMD_DEFINE_ARRAY__ = [], __WEBPACK_AMD_DEFINE_RESULT__ = function () {
			return classNames;
		}.apply(exports, __WEBPACK_AMD_DEFINE_ARRAY__),
				__WEBPACK_AMD_DEFINE_RESULT__ !== undefined && (module.exports = __WEBPACK_AMD_DEFINE_RESULT__));
	} else {
		window.classNames = classNames;
	}
}());


/***/ }),
/* 53 */
/***/ (function(module, exports) {

module.exports = __WEBPACK_EXTERNAL_MODULE_53__;

/***/ }),
/* 54 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


async function enumIndexedProperties(objectClient, start, end) {
  try {
    const { iterator } = await objectClient.enumProperties({ ignoreNonIndexedProperties: true });
    const response = await iteratorSlice(iterator, start, end);
    return response;
  } catch (e) {
    console.error("Error in enumIndexedProperties", e);
    return {};
  }
} /* This Source Code Form is subject to the terms of the Mozilla Public
   * License, v. 2.0. If a copy of the MPL was not distributed with this
   * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

async function enumNonIndexedProperties(objectClient, start, end) {
  try {
    const { iterator } = await objectClient.enumProperties({ ignoreIndexedProperties: true });
    const response = await iteratorSlice(iterator, start, end);
    return response;
  } catch (e) {
    console.error("Error in enumNonIndexedProperties", e);
    return {};
  }
}

async function enumEntries(objectClient, start, end) {
  try {
    const { iterator } = await objectClient.enumEntries();
    const response = await iteratorSlice(iterator, start, end);
    return response;
  } catch (e) {
    console.error("Error in enumEntries", e);
    return {};
  }
}

async function enumSymbols(objectClient, start, end) {
  try {
    const { iterator } = await objectClient.enumSymbols();
    const response = await iteratorSlice(iterator, start, end);
    return response;
  } catch (e) {
    console.error("Error in enumSymbols", e);
    return {};
  }
}

async function getPrototype(objectClient) {
  if (typeof objectClient.getPrototype !== "function") {
    console.error("objectClient.getPrototype is not a function");
    return Promise.resolve({});
  }
  return objectClient.getPrototype();
}

function iteratorSlice(iterator, start, end) {
  start = start || 0;
  const count = end ? end - start + 1 : iterator.count;
  return iterator.slice(start, count);
}

module.exports = {
  enumEntries,
  enumIndexedProperties,
  enumNonIndexedProperties,
  enumSymbols,
  getPrototype
};

/***/ })
/******/ ]);
});