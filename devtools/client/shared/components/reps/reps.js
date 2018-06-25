(function webpackUniversalModuleDefinition(root, factory) {
	if(typeof exports === 'object' && typeof module === 'object')
		module.exports = factory(require("devtools/client/shared/vendor/react"), require("devtools/client/shared/vendor/react-redux"), require("devtools/client/shared/vendor/redux"), require("devtools/client/shared/vendor/react-prop-types"), require("devtools/client/shared/vendor/react-dom-factories"));
	else if(typeof define === 'function' && define.amd)
		define(["devtools/client/shared/vendor/react", "devtools/client/shared/vendor/react-redux", "devtools/client/shared/vendor/redux", "devtools/client/shared/vendor/react-prop-types", "devtools/client/shared/vendor/react-dom-factories"], factory);
	else {
		var a = typeof exports === 'object' ? factory(require("devtools/client/shared/vendor/react"), require("devtools/client/shared/vendor/react-redux"), require("devtools/client/shared/vendor/redux"), require("devtools/client/shared/vendor/react-prop-types"), require("devtools/client/shared/vendor/react-dom-factories")) : factory(root["devtools/client/shared/vendor/react"], root["devtools/client/shared/vendor/react-redux"], root["devtools/client/shared/vendor/redux"], root["devtools/client/shared/vendor/react-prop-types"], root["devtools/client/shared/vendor/react-dom-factories"]);
		for(var i in a) (typeof exports === 'object' ? exports : root)[i] = a[i];
	}
})(typeof self !== 'undefined' ? self : this, function(__WEBPACK_EXTERNAL_MODULE_0__, __WEBPACK_EXTERNAL_MODULE_3592__, __WEBPACK_EXTERNAL_MODULE_3593__, __WEBPACK_EXTERNAL_MODULE_3642__, __WEBPACK_EXTERNAL_MODULE_3643__) {
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
/******/ 	return __webpack_require__(__webpack_require__.s = 3730);
/******/ })
/************************************************************************/
/******/ ({

/***/ 0:
/***/ (function(module, exports) {

module.exports = __WEBPACK_EXTERNAL_MODULE_0__;

/***/ }),

/***/ 175:
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
		!(__WEBPACK_AMD_DEFINE_ARRAY__ = [], __WEBPACK_AMD_DEFINE_RESULT__ = (function () {
			return classNames;
		}).apply(exports, __WEBPACK_AMD_DEFINE_ARRAY__),
				__WEBPACK_AMD_DEFINE_RESULT__ !== undefined && (module.exports = __WEBPACK_AMD_DEFINE_RESULT__));
	} else {
		window.classNames = classNames;
	}
}());


/***/ }),

/***/ 3592:
/***/ (function(module, exports) {

module.exports = __WEBPACK_EXTERNAL_MODULE_3592__;

/***/ }),

/***/ 3593:
/***/ (function(module, exports) {

module.exports = __WEBPACK_EXTERNAL_MODULE_3593__;

/***/ }),

/***/ 3642:
/***/ (function(module, exports) {

module.exports = __WEBPACK_EXTERNAL_MODULE_3642__;

/***/ }),

/***/ 3643:
/***/ (function(module, exports) {

module.exports = __WEBPACK_EXTERNAL_MODULE_3643__;

/***/ }),

/***/ 3644:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Dependencies
const validProtocols = /(http|https|ftp|data|resource|chrome):/i;
const tokenSplitRegex = /(\s|\'|\"|\\)+/;
const ELLIPSIS = "\u2026";
const dom = __webpack_require__(3643);
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
  0x22: '\\"',
  // Backslash.
  0x5c: "\\\\"
};

// Regexp that matches any character we might possibly want to escape.
// Note that we over-match here, because it's difficult to, say, match
// an unpaired surrogate with a regexp.  The details are worked out by
// the replacement function; see |escapeString|.
const escapeRegexp = new RegExp("[" +
// Quote and backslash.
'"\\\\' +
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
  return `"${str.replace(escapeRegexp, (match, offset) => {
    const c = match.charCodeAt(0);
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
      const codePoint = str.codePointAt(offset);
      if (codePoint >= 0xd800 && codePoint <= 0xdfff) {
        // Unpaired surrogate.
        return `\\u${codePoint.toString(16)}`;
      } else if (codePoint >= 0xf0000 && codePoint <= 0x10fffd) {
        // Private use area.  Because we visit each pair of a such a
        // character, return the empty string for one half and the
        // real result for the other, to avoid duplication.
        if (c <= 0xdbff) {
          return `\\u{${codePoint.toString(16)}}`;
        }
        return "";
      }
      // Other surrogate characters are passed through.
      return match;
    }
    return `\\u${`0000${c.toString(16)}`.substr(-4)}`;
  })}"`;
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

  const halfLimit = (limit - alternativeText.length) / 2;

  if (text.length > limit) {
    return text.substr(0, Math.ceil(halfLimit)) + alternativeText + text.substr(text.length - Math.floor(halfLimit));
  }

  return text;
}

function cropString(text, limit, alternativeText) {
  return rawCropString(sanitizeString(`${text}`), limit, alternativeText);
}

function sanitizeString(text) {
  // Replace all non-printable characters, except of
  // (horizontal) tab (HT: \x09) and newline (LF: \x0A, CR: \x0D),
  // with unicode replacement character (u+fffd).
  // eslint-disable-next-line no-control-regex
  const re = new RegExp("[\x00-\x08\x0B\x0C\x0E-\x1F\x7F-\x9F]", "g");
  return text.replace(re, "\ufffd");
}

function parseURLParams(url) {
  url = new URL(url);
  return parseURLEncodedText(url.searchParams);
}

function parseURLEncodedText(text) {
  const params = [];

  // In case the text is empty just return the empty parameters
  if (text == "") {
    return params;
  }

  const searchParams = new URLSearchParams(text);
  const entries = [...searchParams.entries()];
  return entries.map(entry => {
    return {
      name: entry[0],
      value: entry[1]
    };
  });
}

function getFileName(url) {
  const split = splitURLBase(url);
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
  const m = reSplitFile.exec(url);

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
 * Wrap the provided render() method of a rep in a try/catch block that will
 * render a fallback rep if the render fails.
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
    const keys = Object.keys(grip.preview.properties);
    const values = Object.values(grip.preview.properties);
    return [grip.preview.target, ...keys, ...values];
  }

  // RegEx Grip
  if (grip.displayString) {
    return [grip.displayString];
  }

  // Generic Grip
  if (grip.preview && grip.preview.ownProperties) {
    let propertiesValues = Object.values(grip.preview.ownProperties).map(property => property.value || property);

    const propertyKeys = Object.keys(grip.preview.ownProperties);
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
  if (noGrip || Object(object) !== object) {
    return typeof object;
  }
  if (object.type === "object") {
    return object.class;
  }
  return object.type;
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
  // An URL can't be shorter than 5 char (e.g. "ftp:").
  if (typeof grip !== "string" || grip.length < 5) {
    return false;
  }

  return validProtocols.test(grip);
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

/**
 * Returns new array in which `char` are interleaved between the original items.
 *
 * @param {Array} items
 * @param {String} char
 * @returns Array
 */
function interleave(items, char) {
  return items.reduce((res, item, index) => {
    if (index !== items.length - 1) {
      return res.concat(item, char);
    }
    return res.concat(item);
  }, []);
}

const ellipsisElement = span({
  key: "more",
  className: "more-ellipsis",
  title: `more${ELLIPSIS}`
}, ELLIPSIS);

module.exports = {
  interleave,
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
  ellipsisElement,
  ELLIPSIS
};

/***/ }),

/***/ 3645:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

module.exports = {
  MODE: {
    TINY: Symbol("TINY"),
    SHORT: Symbol("SHORT"),
    LONG: Symbol("LONG")
  }
};

/***/ }),

/***/ 3647:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

__webpack_require__(3672);

// Load all existing rep templates
const Undefined = __webpack_require__(3673);
const Null = __webpack_require__(3674);
const StringRep = __webpack_require__(3648);
const Number = __webpack_require__(3675);
const ArrayRep = __webpack_require__(3649);
const Obj = __webpack_require__(3676);
const SymbolRep = __webpack_require__(3677);
const InfinityRep = __webpack_require__(3678);
const NaNRep = __webpack_require__(3679);
const Accessor = __webpack_require__(3680);

// DOM types (grips)
const Attribute = __webpack_require__(3681);
const DateTime = __webpack_require__(3682);
const Document = __webpack_require__(3683);
const DocumentType = __webpack_require__(3684);
const Event = __webpack_require__(3685);
const Func = __webpack_require__(3658);
const PromiseRep = __webpack_require__(3686);
const RegExp = __webpack_require__(3687);
const StyleSheet = __webpack_require__(3688);
const CommentNode = __webpack_require__(3689);
const ElementNode = __webpack_require__(3690);
const TextNode = __webpack_require__(3691);
const ErrorRep = __webpack_require__(3660);
const Window = __webpack_require__(3692);
const ObjectWithText = __webpack_require__(3693);
const ObjectWithURL = __webpack_require__(3694);
const GripArray = __webpack_require__(3661);
const GripMap = __webpack_require__(3663);
const GripMapEntry = __webpack_require__(3664);
const Grip = __webpack_require__(3656);

// List of all registered template.
// XXX there should be a way for extensions to register a new
// or modify an existing rep.
const reps = [RegExp, StyleSheet, Event, DateTime, CommentNode, ElementNode, TextNode, Attribute, Func, PromiseRep, ArrayRep, Document, DocumentType, Window, ObjectWithText, ObjectWithURL, ErrorRep, GripArray, GripMap, GripMapEntry, Grip, Undefined, Null, StringRep, Number, SymbolRep, InfinityRep, NaNRep, Accessor, Obj];

/**
 * Generic rep that is used for rendering native JS types or an object.
 * The right template used for rendering is picked automatically according
 * to the current value type. The value must be passed in as the 'object'
 * property.
 */
const Rep = function (props) {
  const { object, defaultRep } = props;
  const rep = getRep(object, defaultRep, props.noGrip);
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
 * @param defaultRep {React.Component} The default template
 * that should be used to render given object if none is found.
 *
 * @param noGrip {Boolean} If true, will only check reps not made for remote
 *                         objects.
 */
function getRep(object, defaultRep = Grip, noGrip = false) {
  for (let i = 0; i < reps.length; i++) {
    const rep = reps[i];
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
    DocumentType,
    ElementNode,
    ErrorRep,
    Event,
    Func,
    Grip,
    GripArray,
    GripMap,
    GripMapEntry,
    InfinityRep,
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

/***/ 3648:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Dependencies
const PropTypes = __webpack_require__(3642);

const {
  containsURL,
  isURL,
  escapeString,
  getGripType,
  rawCropString,
  sanitizeString,
  wrapRender,
  isGrip,
  tokenSplitRegex,
  ELLIPSIS
} = __webpack_require__(3644);

const dom = __webpack_require__(3643);
const { a, span } = dom;

/**
 * Renders a string. String value is enclosed within quotes.
 */
StringRep.propTypes = {
  useQuotes: PropTypes.bool,
  escapeWhitespace: PropTypes.bool,
  style: PropTypes.object,
  cropLimit: PropTypes.number.isRequired,
  member: PropTypes.object,
  object: PropTypes.object.isRequired,
  openLink: PropTypes.func,
  className: PropTypes.string
};

function StringRep(props) {
  const {
    className,
    style,
    cropLimit,
    object,
    useQuotes = true,
    escapeWhitespace = true,
    member,
    openLink
  } = props;

  let text = object;

  const isLong = isLongString(object);
  const isOpen = member && member.open;
  const shouldCrop = !isOpen && cropLimit && text.length > cropLimit;

  if (isLong) {
    text = maybeCropLongString({
      shouldCrop,
      cropLimit
    }, text);

    const { fullText } = object;
    if (isOpen && fullText) {
      text = fullText;
    }
  }

  text = formatText({
    useQuotes,
    escapeWhitespace
  }, text);

  const config = getElementConfig({
    className,
    style,
    actor: object.actor
  });

  if (!isLong) {
    if (containsURL(text)) {
      return span(config, ...getLinkifiedElements(text, shouldCrop && cropLimit, openLink));
    }

    // Cropping of longString has been handled before formatting.
    text = maybeCropString({
      isLong,
      shouldCrop,
      cropLimit
    }, text);
  }

  return span(config, text);
}

function maybeCropLongString(opts, text) {
  const { shouldCrop, cropLimit } = opts;

  const { initial, length } = text;

  text = shouldCrop ? initial.substring(0, cropLimit) : initial;

  if (text.length < length) {
    text += ELLIPSIS;
  }

  return text;
}

function formatText(opts, text) {
  const { useQuotes, escapeWhitespace } = opts;

  return useQuotes ? escapeString(text, escapeWhitespace) : sanitizeString(text);
}

function getElementConfig(opts) {
  const { className, style, actor } = opts;

  const config = {};

  if (actor) {
    config["data-link-actor-id"] = actor;
  }

  const classNames = ["objectBox", "objectBox-string"];
  if (className) {
    classNames.push(className);
  }
  config.className = classNames.join(" ");

  if (style) {
    config.style = style;
  }

  return config;
}

function maybeCropString(opts, text) {
  const { shouldCrop, cropLimit } = opts;

  return shouldCrop ? rawCropString(text, cropLimit) : text;
}

/**
 * Get an array of the elements representing the string, cropped if needed,
 * with actual links.
 *
 * @param {String} text: The actual string to linkify.
 * @param {Integer | null} cropLimit
 * @param {Function} openLink: Function handling the link opening.
 * @returns {Array<String|ReactElement>}
 */
function getLinkifiedElements(text, cropLimit, openLink) {
  const halfLimit = Math.ceil((cropLimit - ELLIPSIS.length) / 2);
  const startCropIndex = cropLimit ? halfLimit : null;
  const endCropIndex = cropLimit ? text.length - halfLimit : null;

  // As we walk through the tokens of the source string, we make sure to
  // preserve the original whitespace that separated the tokens.
  let currentIndex = 0;
  const items = [];
  for (const token of text.split(tokenSplitRegex)) {
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

      const linkText = getCroppedString(token, currentIndex, startCropIndex, endCropIndex);
      if (linkText) {
        items.push(a({
          className: "url",
          title: token,
          draggable: false,
          onClick: openLink ? e => {
            e.preventDefault();
            openLink(token, e);
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
 * Returns a cropped substring given an offset, start and end crop indices in a
 * parent string.
 *
 * @param {String} text: The substring to crop.
 * @param {Integer} offset: The offset corresponding to the index at which
 *                          the substring is in the parent string.
 * @param {Integer|null} startCropIndex: the index where the start of the crop
 *                                       should happen in the parent string.
 * @param {Integer|null} endCropIndex: the index where the end of the crop
 *                                     should happen in the parent string
 * @returns {String|null} The cropped substring, or null if the text is
 *                        completly cropped.
 */
function getCroppedString(text, offset = 0, startCropIndex, endCropIndex) {
  if (!startCropIndex) {
    return text;
  }

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

function isLongString(object) {
  return object && object.type === "longString";
}

function supportsObject(object, noGrip = false) {
  if (noGrip === false && isGrip(object)) {
    return isLongString(object);
  }

  return getGripType(object, noGrip) == "string";
}

// Exports from this module

module.exports = {
  rep: wrapRender(StringRep),
  supportsObject,
  isLongString
};

/***/ }),

/***/ 3649:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


var _extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; };

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Dependencies
const dom = __webpack_require__(3643);
const PropTypes = __webpack_require__(3642);
const { wrapRender } = __webpack_require__(3644);
const { MODE } = __webpack_require__(3645);
const { span } = dom;

const ModePropType = PropTypes.oneOf(
// @TODO Change this to Object.values when supported in Node's version of V8
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
  const { object, mode = MODE.SHORT } = props;

  let items;
  let brackets;
  const needSpace = function (space) {
    return space ? { left: "[ ", right: " ]" } : { left: "[", right: "]" };
  };

  if (mode === MODE.TINY) {
    const isEmpty = object.length === 0;
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
    className: "objectBox objectBox-array"
  }, span({
    className: "arrayLeftBracket"
  }, brackets.left), ...items, span({
    className: "arrayRightBracket"
  }, brackets.right));
}

function arrayIterator(props, array, max) {
  const items = [];

  for (let i = 0; i < array.length && i < max; i++) {
    const config = {
      mode: MODE.TINY,
      delim: i == array.length - 1 ? "" : ", "
    };
    let item;

    try {
      item = ItemRep(_extends({}, props, config, {
        object: array[i]
      }));
    } catch (exc) {
      item = ItemRep(_extends({}, props, config, {
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
  const { Rep } = __webpack_require__(3647);

  const { object, delim, mode } = props;
  return span({}, Rep(_extends({}, props, {
    object: object,
    mode: mode
  })), delim);
}

function getLength(object) {
  return object.length;
}

function supportsObject(object, noGrip = false) {
  return noGrip && (Array.isArray(object) || Object.prototype.toString.call(object) === "[object Arguments]");
}

const maxLengthMap = new Map();
maxLengthMap.set(MODE.SHORT, 3);
maxLengthMap.set(MODE.LONG, 10);

// Exports from this module
module.exports = {
  rep: wrapRender(ArrayRep),
  supportsObject,
  maxLengthMap,
  getLength,
  ModePropType
};

/***/ }),

/***/ 3650:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


var _extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; };

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Dependencies
const PropTypes = __webpack_require__(3642);
const { maybeEscapePropertyName, wrapRender } = __webpack_require__(3644);
const { MODE } = __webpack_require__(3645);

const { span } = __webpack_require__(3643);

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
  // @TODO Change this to Object.values when supported in Node's version of V8
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
  const Grip = __webpack_require__(3656);
  const { Rep } = __webpack_require__(3647);

  let { name, mode, equal, suppressQuotes } = props;

  let key;
  // The key can be a simple string, for plain objects,
  // or another object for maps and weakmaps.
  if (typeof name === "string") {
    if (!suppressQuotes) {
      name = maybeEscapePropertyName(name);
    }
    key = span({ className: "nodeName" }, name);
  } else {
    key = Rep(_extends({}, props, {
      className: "nodeName",
      object: name,
      mode: mode || MODE.TINY,
      defaultRep: Grip
    }));
  }

  return [key, span({
    className: "objectEqual"
  }, equal), Rep(_extends({}, props))];
}

// Exports from this module
module.exports = wrapRender(PropRep);

/***/ }),

/***/ 3655:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const { MODE } = __webpack_require__(3645);
const { REPS, getRep } = __webpack_require__(3647);
const ObjectInspector = __webpack_require__(3695);
const ObjectInspectorUtils = __webpack_require__(3657);

const {
  parseURLEncodedText,
  parseURLParams,
  maybeEscapePropertyName,
  getGripPreviewItems
} = __webpack_require__(3644);

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

/***/ 3656:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


var _extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; };

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// ReactJS
const PropTypes = __webpack_require__(3642);

// Dependencies
const { interleave, isGrip, wrapRender } = __webpack_require__(3644);
const PropRep = __webpack_require__(3650);
const { MODE } = __webpack_require__(3645);

const dom = __webpack_require__(3643);
const { span } = dom;

/**
 * Renders generic grip. Grip is client representation
 * of remote JS object and is used as an input object
 * for this rep component.
 */
GripRep.propTypes = {
  object: PropTypes.object.isRequired,
  // @TODO Change this to Object.values when supported in Node's version of V8
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
  const { mode = MODE.SHORT, object } = props;

  const config = {
    "data-link-actor-id": object.actor,
    className: "objectBox objectBox-object"
  };

  if (mode === MODE.TINY) {
    const propertiesLength = getPropertiesLength(object);

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

  const propsArray = safePropIterator(props, object, maxLengthMap.get(mode));

  return span(config, getTitleElement(props, object), span({
    className: "objectLeftBrace"
  }, " { "), ...interleave(propsArray, ", "), span({
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
    const { Rep } = __webpack_require__(3647);

    return [Rep({
      object: object.preview.wrappedValue,
      mode: props.mode || MODE.TINY,
      defaultRep: Grip
    })];
  }

  // Property filter. Show only interesting properties to the user.
  const isInterestingProp = props.isInterestingProp || ((type, value) => {
    return type == "boolean" || type == "number" || type == "string" && value.length != 0;
  });

  let properties = object.preview ? object.preview.ownProperties || {} : {};

  const propertiesLength = getPropertiesLength(object);

  if (object.preview && object.preview.safeGetterValues) {
    properties = _extends({}, properties, object.preview.safeGetterValues);
  }

  let indexes = getPropIndexes(properties, max, isInterestingProp);
  if (indexes.length < max && indexes.length < propertiesLength) {
    // There are not enough props yet.
    // Then add uninteresting props to display them.
    indexes = indexes.concat(getPropIndexes(properties, max - indexes.length, (t, value, name) => {
      return !isInterestingProp(t, value, name);
    }));
  }

  // The server synthesizes some property names for a Proxy, like
  // <target> and <handler>; we don't want to quote these because,
  // as synthetic properties, they appear more natural when
  // unquoted.
  const suppressQuotes = object.class === "Proxy";
  const propsArray = getProps(props, properties, indexes, suppressQuotes);

  // Show symbols.
  if (object.preview && object.preview.ownSymbols) {
    const { ownSymbols } = object.preview;
    const length = max - indexes.length;

    const symbolsProps = ownSymbols.slice(0, length).map(symbolItem => {
      return PropRep(_extends({}, props, {
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

  if (Object.keys(properties).length > max || propertiesLength > max ||
  // When the object has non-enumerable properties, we don't have them in the
  // packet, but we might want to show there's something in the object.
  propertiesLength > propsArray.length) {
    // There are some undisplayed props. Then display "more...".
    propsArray.push(span({
      key: "more",
      className: "more-ellipsis",
      title: "more…"
    }, "…"));
  }

  return propsArray;
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
    const name = propertiesKeys[i];
    const value = getPropValue(properties[name]);

    return PropRep(_extends({}, componentProps, {
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
  const indexes = [];

  try {
    let i = 0;
    for (const name in properties) {
      if (indexes.length >= max) {
        return indexes;
      }

      // Type is specified in grip's "class" field and for primitive
      // values use typeof.
      const value = getPropValue(properties[name]);
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
    const keys = Object.keys(property);
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

  if (object.class === "DeadObject") {
    return true;
  }

  return object.preview ? typeof object.preview.ownProperties !== "undefined" : typeof object.ownPropertyLength !== "undefined";
}

const maxLengthMap = new Map();
maxLengthMap.set(MODE.SHORT, 3);
maxLengthMap.set(MODE.LONG, 10);

// Grip is used in propIterator and has to be defined here.
const Grip = {
  rep: wrapRender(GripRep),
  supportsObject,
  maxLengthMap
};

// Exports from this module
module.exports = Grip;

/***/ }),

/***/ 3657:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


var _extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; };

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const client = __webpack_require__(3665);
const loadProperties = __webpack_require__(3666);
const node = __webpack_require__(3667);
const { nodeIsError, nodeIsPrimitive } = node;
const selection = __webpack_require__(3698);

const { MODE } = __webpack_require__(3645);
const {
  REPS: { Rep, Grip }
} = __webpack_require__(3647);


function shouldRenderRootsInReps(roots) {
  if (roots.length > 1) {
    return false;
  }

  const root = roots[0];
  const name = root && root.name;
  return (name === null || typeof name === "undefined") && (nodeIsPrimitive(root) || nodeIsError(root));
}

function renderRep(item, props) {
  return Rep(_extends({}, props, {
    object: node.getValue(item),
    mode: props.mode || MODE.TINY,
    defaultRep: Grip
  }));
}

module.exports = {
  client,
  loadProperties,
  node,
  renderRep,
  selection,
  shouldRenderRootsInReps
};

/***/ }),

/***/ 3658:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// ReactJS
const PropTypes = __webpack_require__(3642);

// Reps
const { getGripType, isGrip, cropString, wrapRender } = __webpack_require__(3644);
const { MODE } = __webpack_require__(3645);

const dom = __webpack_require__(3643);
const { span } = dom;

const IGNORED_SOURCE_URLS = ["debugger eval code"];

/**
 * This component represents a template for Function objects.
 */
FunctionRep.propTypes = {
  object: PropTypes.object.isRequired,
  parameterNames: PropTypes.array,
  onViewSourceInDebugger: PropTypes.func
};

function FunctionRep(props) {
  const { object: grip, onViewSourceInDebugger } = props;

  let jumpToDefinitionButton;
  if (onViewSourceInDebugger && grip.location && grip.location.url && !IGNORED_SOURCE_URLS.includes(grip.location.url)) {
    jumpToDefinitionButton = dom.button({
      className: "jump-definition",
      draggable: false,
      title: "Jump to definition",
      onClick: e => {
        // Stop the event propagation so we don't trigger ObjectInspector
        // expand/collapse.
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
  const { mode } = props;

  if (mode === MODE.TINY && !grip.isGenerator && !grip.isAsync) {
    return null;
  }

  let title = mode === MODE.TINY ? "" : "function ";

  if (grip.isGenerator) {
    title = mode === MODE.TINY ? "* " : "function* ";
  }

  if (grip.isAsync) {
    title = `${"async" + " "}${title}`;
  }

  return span({
    className: "objectTitle"
  }, title);
}

/**
 * Returns a ReactElement representing the function name.
 *
 * @param {Object} grip : Function grip
 * @param {Object} props: Function rep props
 */
function getFunctionName(grip, props = {}) {
  let { functionName } = props;
  let name;

  if (functionName) {
    const end = functionName.length - 1;
    functionName = functionName.startsWith('"') && functionName.endsWith('"') ? functionName.substring(1, end) : functionName;
  }

  if (grip.displayName != undefined && functionName != undefined && grip.displayName != functionName) {
    name = `${functionName}:${grip.displayName}`;
  } else {
    name = cleanFunctionName(grip.userDisplayName || grip.displayName || grip.name || props.functionName || "");
  }

  return cropString(name, 100);
}

const objectProperty = /([\w\d]+)$/;
const arrayProperty = /\[(.*?)\]$/;
const functionProperty = /([\w\d]+)[\/\.<]*?$/;
const annonymousProperty = /([\w\d]+)\(\^\)$/;

/**
 * Decodes an anonymous naming scheme that
 * spider monkey implements based on "Naming Anonymous JavaScript Functions"
 * http://johnjbarton.github.io/nonymous/index.html
 *
 * @param {String} name : Function name to clean up
 * @returns String
 */
function cleanFunctionName(name) {
  for (const reg of [objectProperty, arrayProperty, functionProperty, annonymousProperty]) {
    const match = reg.exec(name);
    if (match) {
      return match[1];
    }
  }

  return name;
}

function renderParams(props) {
  const { parameterNames = [] } = props;

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
  cleanFunctionName,
  // exported for testing purpose.
  getFunctionName
};

/***/ }),

/***/ 3659:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

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

/***/ 3660:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// ReactJS
const PropTypes = __webpack_require__(3642);
// Utils
const { getGripType, isGrip, wrapRender } = __webpack_require__(3644);
const { cleanFunctionName } = __webpack_require__(3658);
const { isLongString } = __webpack_require__(3648);
const { MODE } = __webpack_require__(3645);

const dom = __webpack_require__(3643);
const { span } = dom;
const IGNORED_SOURCE_URLS = ["debugger eval code"];

/**
 * Renders Error objects.
 */
ErrorRep.propTypes = {
  object: PropTypes.object.isRequired,
  // @TODO Change this to Object.values when supported in Node's version of V8
  mode: PropTypes.oneOf(Object.keys(MODE).map(key => MODE[key]))
};

function ErrorRep(props) {
  const object = props.object;
  const preview = object.preview;

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

  const content = [];

  if (props.mode === MODE.TINY) {
    content.push(name);
  } else {
    content.push(`${name}: "${preview.message}"`);
  }

  if (preview.stack && props.mode !== MODE.TINY) {
    content.push("\n", getStacktraceElements(props, preview));
  }

  return span({
    "data-link-actor-id": object.actor,
    className: "objectBox-stackTrace"
  }, content);
}

/**
 * Returns a React element reprensenting the Error stacktrace, i.e.
 * transform error.stack from:
 *
 * semicolon@debugger eval code:1:109
 * jkl@debugger eval code:1:63
 * asdf@debugger eval code:1:28
 * @debugger eval code:1:227
 *
 * Into a column layout:
 *
 * semicolon  (<anonymous>:8:10)
 * jkl        (<anonymous>:5:10)
 * asdf       (<anonymous>:2:10)
 *            (<anonymous>:11:1)
 */
function getStacktraceElements(props, preview) {
  const stack = [];
  if (!preview.stack) {
    return stack;
  }

  const isStacktraceALongString = isLongString(preview.stack);
  const stackString = isStacktraceALongString ? preview.stack.initial : preview.stack;

  stackString.split("\n").forEach((frame, index) => {
    if (!frame) {
      // Skip any blank lines
      return;
    }

    let functionName;
    let location;

    // Given the input: "functionName@scriptLocation:2:100"
    // Result: [
    //   "functionName@scriptLocation:2:100",
    //   "functionName",
    //   "scriptLocation:2:100"
    // ]
    const result = frame.match(/^(.*)@(.*)$/);
    if (result && result.length === 3) {
      functionName = result[1];

      // If the resource was loaded by base-loader.js, the location looks like:
      // resource://devtools/shared/base-loader.js -> resource://path/to/file.js .
      // What's needed is only the last part after " -> ".
      location = result[2].split(" -> ").pop();
    }

    if (!functionName) {
      functionName = "<anonymous>";
    }

    let onLocationClick;
    // Given the input: "scriptLocation:2:100"
    // Result:
    // ["scriptLocation:2:100", "scriptLocation", "2", "100"]
    const locationParts = location.match(/^(.*):(\d+):(\d+)$/);
    if (props.onViewSourceInDebugger && location && !IGNORED_SOURCE_URLS.includes(locationParts[1]) && locationParts) {
      const [, url, line, column] = locationParts;
      onLocationClick = e => {
        // Don't trigger ObjectInspector expand/collapse.
        e.stopPropagation();
        props.onViewSourceInDebugger({
          url,
          line: Number(line),
          column: Number(column)
        });
      };
    }

    stack.push(span({
      key: `fn${index}`,
      className: "objectBox-stackTrace-fn"
    }, cleanFunctionName(functionName)), span({
      key: `location${index}`,
      className: "objectBox-stackTrace-location",
      onClick: onLocationClick,
      title: onLocationClick ? `View source in debugger → ${location}` : undefined
    }, location));
  });

  if (isStacktraceALongString) {
    // Remove the last frame (i.e. 2 last elements in the array, the function
    // name and the location) which is certainly incomplete.
    // Can be removed when https://bugzilla.mozilla.org/show_bug.cgi?id=1448833
    // is fixed.
    stack.splice(-2);
  }

  return span({
    key: "stack",
    className: "objectBox-stackTrace-grid"
  }, stack);
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

/***/ 3661:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


var _extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; };

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Dependencies
const PropTypes = __webpack_require__(3642);

const { lengthBubble } = __webpack_require__(3662);
const {
  interleave,
  getGripType,
  isGrip,
  wrapRender,
  ellipsisElement
} = __webpack_require__(3644);
const { MODE } = __webpack_require__(3645);

const dom = __webpack_require__(3643);
const { span } = dom;
const { ModePropType } = __webpack_require__(3649);
const DEFAULT_TITLE = "Array";

/**
 * Renders an array. The array is enclosed by left and right bracket
 * and the max number of rendered items depends on the current mode.
 */
GripArray.propTypes = {
  object: PropTypes.object.isRequired,
  // @TODO Change this to Object.values when supported in Node's version of V8
  mode: ModePropType,
  provider: PropTypes.object,
  onDOMNodeMouseOver: PropTypes.func,
  onDOMNodeMouseOut: PropTypes.func,
  onInspectIconClick: PropTypes.func
};

function GripArray(props) {
  const { object, mode = MODE.SHORT } = props;

  let brackets;
  const needSpace = function (space) {
    return space ? { left: "[ ", right: " ]" } : { left: "[", right: "]" };
  };

  const config = {
    "data-link-actor-id": object.actor,
    className: "objectBox objectBox-array"
  };

  const title = getTitle(props, object);

  if (mode === MODE.TINY) {
    const isEmpty = getLength(object) === 0;

    // Omit bracketed ellipsis for non-empty non-Array arraylikes (f.e: Sets).
    if (!isEmpty && object.class !== "Array") {
      return span(config, title);
    }

    brackets = needSpace(false);
    return span(config, title, span({
      className: "arrayLeftBracket"
    }, brackets.left), isEmpty ? null : ellipsisElement, span({
      className: "arrayRightBracket"
    }, brackets.right));
  }

  const max = maxLengthMap.get(mode);
  const items = arrayIterator(props, object, max);
  brackets = needSpace(items.length > 0);

  return span({
    "data-link-actor-id": object.actor,
    className: "objectBox objectBox-array"
  }, title, span({
    className: "arrayLeftBracket"
  }, brackets.left), ...interleave(items, ", "), span({
    className: "arrayRightBracket"
  }, brackets.right), span({
    className: "arrayProperties",
    role: "group"
  }));
}

function getLength(grip) {
  if (!grip.preview) {
    return 0;
  }

  return grip.preview.length || grip.preview.childNodesLength || 0;
}

function getTitle(props, object) {
  const objectLength = getLength(object);
  const isEmpty = objectLength === 0;

  let title = props.title || object.class || DEFAULT_TITLE;

  const length = lengthBubble({
    object,
    mode: props.mode,
    maxLengthMap,
    getLength
  });

  if (props.mode === MODE.TINY) {
    if (isEmpty) {
      if (object.class === DEFAULT_TITLE) {
        return null;
      }

      return span({ className: "objectTitle" }, `${title} `);
    }

    let trailingSpace;
    if (object.class === DEFAULT_TITLE) {
      title = null;
      trailingSpace = " ";
    }

    return span({ className: "objectTitle" }, title, length, trailingSpace);
  }

  return span({ className: "objectTitle" }, title, length, " ");
}

function getPreviewItems(grip) {
  if (!grip.preview) {
    return null;
  }

  return grip.preview.items || grip.preview.childNodes || [];
}

function arrayIterator(props, grip, max) {
  const { Rep } = __webpack_require__(3647);

  let items = [];
  const gripLength = getLength(grip);

  if (!gripLength) {
    return items;
  }

  const previewItems = getPreviewItems(grip);
  const provider = props.provider;

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
      res.push(Rep(_extends({}, props, {
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
    items.push(ellipsisElement);
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

/***/ 3662:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const PropTypes = __webpack_require__(3642);

const { wrapRender } = __webpack_require__(3644);
const { MODE } = __webpack_require__(3645);
const { ModePropType } = __webpack_require__(3649);

const dom = __webpack_require__(3643);
const { span } = dom;

GripLengthBubble.propTypes = {
  object: PropTypes.object.isRequired,
  maxLengthMap: PropTypes.instanceOf(Map).isRequired,
  getLength: PropTypes.func.isRequired,
  mode: ModePropType,
  visibilityThreshold: PropTypes.number
};

function GripLengthBubble(props) {
  const {
    object,
    mode = MODE.SHORT,
    visibilityThreshold = 2,
    maxLengthMap,
    getLength,
    showZeroLength = false
  } = props;

  const length = getLength(object);
  const isEmpty = length === 0;
  const isObvious = [MODE.SHORT, MODE.LONG].includes(mode) && length > 0 && length <= maxLengthMap.get(mode) && length <= visibilityThreshold;
  if (isEmpty && !showZeroLength || isObvious) {
    return "";
  }

  return span({
    className: "objectLengthBubble"
  }, `(${length})`);
}

module.exports = {
  lengthBubble: wrapRender(GripLengthBubble)
};

/***/ }),

/***/ 3663:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Dependencies

const { lengthBubble } = __webpack_require__(3662);
const PropTypes = __webpack_require__(3642);
const {
  interleave,
  isGrip,
  wrapRender,
  ellipsisElement
} = __webpack_require__(3644);
const PropRep = __webpack_require__(3650);
const { MODE } = __webpack_require__(3645);
const { ModePropType } = __webpack_require__(3649);

const { span } = __webpack_require__(3643);

/**
 * Renders an map. A map is represented by a list of its
 * entries enclosed in curly brackets.
 */
GripMap.propTypes = {
  object: PropTypes.object,
  // @TODO Change this to Object.values when supported in Node's version of V8
  mode: ModePropType,
  isInterestingEntry: PropTypes.func,
  onDOMNodeMouseOver: PropTypes.func,
  onDOMNodeMouseOut: PropTypes.func,
  onInspectIconClick: PropTypes.func,
  title: PropTypes.string
};

function GripMap(props) {
  const { mode, object } = props;

  const config = {
    "data-link-actor-id": object.actor,
    className: "objectBox objectBox-object"
  };

  const title = getTitle(props, object);
  const isEmpty = getLength(object) === 0;

  if (isEmpty || mode === MODE.TINY) {
    return span(config, title);
  }

  const propsArray = safeEntriesIterator(props, object, maxLengthMap.get(mode));

  return span(config, title, span({
    className: "objectLeftBrace"
  }, " { "), ...interleave(propsArray, ", "), span({
    className: "objectRightBrace"
  }, " }"));
}

function getTitle(props, object) {
  const title = props.title || (object && object.class ? object.class : "Map");
  return span({
    className: "objectTitle"
  }, title, lengthBubble({
    object,
    mode: props.mode,
    maxLengthMap,
    getLength,
    showZeroLength: true
  }));
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
  const isInterestingEntry = props.isInterestingEntry || ((type, value) => {
    return type == "boolean" || type == "number" || type == "string" && value.length != 0;
  });

  const mapEntries = object.preview && object.preview.entries ? object.preview.entries : [];

  let indexes = getEntriesIndexes(mapEntries, max, isInterestingEntry);
  if (indexes.length < max && indexes.length < mapEntries.length) {
    // There are not enough entries yet, so we add uninteresting entries.
    indexes = indexes.concat(getEntriesIndexes(mapEntries, max - indexes.length, (t, value, name) => {
      return !isInterestingEntry(t, value, name);
    }));
  }

  const entries = getEntries(props, mapEntries, indexes);
  if (entries.length < getLength(object)) {
    // There are some undisplayed entries. Then display "…".
    entries.push(ellipsisElement);
  }

  return entries;
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
  const { onDOMNodeMouseOver, onDOMNodeMouseOut, onInspectIconClick } = props;

  // Make indexes ordered by ascending.
  indexes.sort(function (a, b) {
    return a - b;
  });

  return indexes.map((index, i) => {
    const [key, entryValue] = entries[index];
    const value = entryValue.value !== undefined ? entryValue.value : entryValue;

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
      const value = entry && entry.value !== undefined ? entry.value : entry;
      // Type is specified in grip's "class" field and for primitive
      // values use typeof.
      const type = (value && value.class ? value.class : typeof value).toLowerCase();

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

/***/ 3664:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


var _extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; };

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Dependencies
const PropTypes = __webpack_require__(3642);
// Shortcuts
const dom = __webpack_require__(3643);
const { span } = dom;
const { wrapRender } = __webpack_require__(3644);
const PropRep = __webpack_require__(3650);
const { MODE } = __webpack_require__(3645);
/**
 * Renders an map entry. A map entry is represented by its key,
 * a column and its value.
 */
GripMapEntry.propTypes = {
  object: PropTypes.object,
  // @TODO Change this to Object.values when supported in Node's version of V8
  mode: PropTypes.oneOf(Object.keys(MODE).map(key => MODE[key])),
  onDOMNodeMouseOver: PropTypes.func,
  onDOMNodeMouseOut: PropTypes.func,
  onInspectIconClick: PropTypes.func
};

function GripMapEntry(props) {
  const { object } = props;

  const { key, value } = object.preview;

  return span({
    className: "objectBox objectBox-map-entry"
  }, PropRep(_extends({}, props, {
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
  return grip && (grip.type === "mapEntry" || grip.type === "storageEntry") && grip.preview;
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

/***/ 3665:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


const { getValue, nodeHasFullText } = __webpack_require__(3667); /* This Source Code Form is subject to the terms of the Mozilla Public
                                                                 * License, v. 2.0. If a copy of the MPL was not distributed with this
                                                                 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

async function enumIndexedProperties(objectClient, start, end) {
  try {
    const { iterator } = await objectClient.enumProperties({
      ignoreNonIndexedProperties: true
    });
    const response = await iteratorSlice(iterator, start, end);
    return response;
  } catch (e) {
    console.error("Error in enumIndexedProperties", e);
    return {};
  }
}

async function enumNonIndexedProperties(objectClient, start, end) {
  try {
    const { iterator } = await objectClient.enumProperties({
      ignoreIndexedProperties: true
    });
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

async function getFullText(longStringClient, item) {
  const { initial, fullText, length } = getValue(item);

  // Return fullText property if it exists so that it can be added to the
  // loadedProperties map.
  if (nodeHasFullText(item)) {
    return Promise.resolve({ fullText });
  }

  return new Promise((resolve, reject) => {
    longStringClient.substring(initial.length, length, response => {
      if (response.error) {
        console.error("LongStringClient.substring", `${response.error}: ${response.message}`);
        reject({});
        return;
      }

      resolve({
        fullText: initial + response.substring
      });
    });
  });
}

function iteratorSlice(iterator, start, end) {
  start = start || 0;
  const count = end ? end - start + 1 : iterator.count;

  if (count === 0) {
    return Promise.resolve({});
  }
  return iterator.slice(start, count);
}

module.exports = {
  enumEntries,
  enumIndexedProperties,
  enumNonIndexedProperties,
  enumSymbols,
  getPrototype,
  getFullText
};

/***/ }),

/***/ 3666:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


var _extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; };

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const {
  enumEntries,
  enumIndexedProperties,
  enumNonIndexedProperties,
  getPrototype,
  enumSymbols,
  getFullText
} = __webpack_require__(3665);

const {
  getClosestGripNode,
  getClosestNonBucketNode,
  getValue,
  nodeHasAccessors,
  nodeHasAllEntriesInPreview,
  nodeHasProperties,
  nodeIsBucket,
  nodeIsDefaultProperties,
  nodeIsEntries,
  nodeIsMapEntry,
  nodeIsPrimitive,
  nodeIsProxy,
  nodeNeedsNumericalBuckets,
  nodeIsLongString
} = __webpack_require__(3667);

function loadItemProperties(item, createObjectClient, createLongStringClient, loadedProperties) {
  const gripItem = getClosestGripNode(item);
  const value = getValue(gripItem);

  const [start, end] = item.meta ? [item.meta.startIndex, item.meta.endIndex] : [];

  const promises = [];
  let objectClient;
  const getObjectClient = () => objectClient || createObjectClient(value);

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

  if (shouldLoadItemFullText(item, loadedProperties)) {
    promises.push(getFullText(createLongStringClient(value), item));
  }

  return Promise.all(promises).then(mergeResponses);
}

function mergeResponses(responses) {
  const data = {};

  for (const response of responses) {
    if (response.hasOwnProperty("ownProperties")) {
      data.ownProperties = _extends({}, data.ownProperties, response.ownProperties);
    }

    if (response.ownSymbols && response.ownSymbols.length > 0) {
      data.ownSymbols = response.ownSymbols;
    }

    if (response.prototype) {
      data.prototype = response.prototype;
    }

    if (response.fullText) {
      data.fullText = response.fullText;
    }
  }

  return data;
}

function shouldLoadItemIndexedProperties(item, loadedProperties = new Map()) {
  const gripItem = getClosestGripNode(item);
  const value = getValue(gripItem);

  return value && nodeHasProperties(gripItem) && !loadedProperties.has(item.path) && !nodeIsProxy(item) && !nodeNeedsNumericalBuckets(item) && !nodeIsEntries(getClosestNonBucketNode(item)) &&
  // The data is loaded when expanding the window node.
  !nodeIsDefaultProperties(item);
}

function shouldLoadItemNonIndexedProperties(item, loadedProperties = new Map()) {
  const gripItem = getClosestGripNode(item);
  const value = getValue(gripItem);

  return value && nodeHasProperties(gripItem) && !loadedProperties.has(item.path) && !nodeIsProxy(item) && !nodeIsEntries(getClosestNonBucketNode(item)) && !nodeIsBucket(item) &&
  // The data is loaded when expanding the window node.
  !nodeIsDefaultProperties(item);
}

function shouldLoadItemEntries(item, loadedProperties = new Map()) {
  const gripItem = getClosestGripNode(item);
  const value = getValue(gripItem);

  return value && nodeIsEntries(getClosestNonBucketNode(item)) && !nodeHasAllEntriesInPreview(gripItem) && !loadedProperties.has(item.path) && !nodeNeedsNumericalBuckets(item);
}

function shouldLoadItemPrototype(item, loadedProperties = new Map()) {
  const value = getValue(item);

  return value && !loadedProperties.has(item.path) && !nodeIsBucket(item) && !nodeIsMapEntry(item) && !nodeIsEntries(item) && !nodeIsDefaultProperties(item) && !nodeHasAccessors(item) && !nodeIsPrimitive(item) && !nodeIsLongString(item);
}

function shouldLoadItemSymbols(item, loadedProperties = new Map()) {
  const value = getValue(item);

  return value && !loadedProperties.has(item.path) && !nodeIsBucket(item) && !nodeIsMapEntry(item) && !nodeIsEntries(item) && !nodeIsDefaultProperties(item) && !nodeHasAccessors(item) && !nodeIsPrimitive(item) && !nodeIsLongString(item) && !nodeIsProxy(item);
}

function shouldLoadItemFullText(item, loadedProperties = new Map()) {
  return !loadedProperties.has(item.path) && nodeIsLongString(item);
}

module.exports = {
  loadItemProperties,
  mergeResponses,
  shouldLoadItemEntries,
  shouldLoadItemIndexedProperties,
  shouldLoadItemNonIndexedProperties,
  shouldLoadItemPrototype,
  shouldLoadItemSymbols,
  shouldLoadItemFullText
};

/***/ }),

/***/ 3667:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


var _extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; };

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const { maybeEscapePropertyName } = __webpack_require__(3644);
const ArrayRep = __webpack_require__(3649);
const GripArrayRep = __webpack_require__(3661);
const GripMap = __webpack_require__(3663);
const GripMapEntryRep = __webpack_require__(3664);
const ErrorRep = __webpack_require__(3660);
const { isLongString } = __webpack_require__(3648);

const MAX_NUMERICAL_PROPERTIES = 100;

const NODE_TYPES = {
  BUCKET: Symbol("[n…m]"),
  DEFAULT_PROPERTIES: Symbol("<default properties>"),
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
  PROTOTYPE: Symbol("<prototype>"),
  BLOCK: Symbol("☲")
};

let WINDOW_PROPERTIES = {};

if (typeof window === "object") {
  WINDOW_PROPERTIES = Object.getOwnPropertyNames(window);
}

function getType(item) {
  return item.type;
}

function getValue(item) {
  if (item && item.contents && item.contents.hasOwnProperty("value")) {
    return item.contents.value;
  }

  if (item && item.contents && item.contents.hasOwnProperty("getterValue")) {
    return item.contents.getterValue;
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

function nodeIsUninitializedBinding(item) {
  const value = getValue(item);
  return value && value.uninitialized;
}

// Used to check if an item represents a binding that exists in a sourcemap's
// original file content, but does not match up with a binding found in the
// generated code.
function nodeIsUnmappedBinding(item) {
  const value = getValue(item);
  return value && value.unmapped;
}

// Used to check if an item represents a binding that exists in the debugger's
// parser result, but does not match up with a binding returned by the
// debugger server.
function nodeIsUnscopedBinding(item) {
  const value = getValue(item);
  return value && value.unscoped;
}

function nodeIsMissingArguments(item) {
  const value = getValue(item);
  return !nodeHasChildren(item) && value && value.missingArguments;
}

function nodeHasProperties(item) {
  return !nodeHasChildren(item) && nodeIsObject(item);
}

function nodeIsPrimitive(item) {
  return !nodeHasChildren(item) && !nodeHasProperties(item) && !nodeIsEntries(item) && !nodeIsMapEntry(item) && !nodeHasAccessors(item) && !nodeIsBucket(item) && !nodeIsLongString(item);
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

function nodeIsBlock(item) {
  return getType(item) === NODE_TYPES.BLOCK;
}

function nodeIsError(item) {
  return ErrorRep.supportsObject(getValue(item));
}

function nodeIsLongString(item) {
  return isLongString(getValue(item));
}

function nodeHasFullText(item) {
  const value = getValue(item);
  return nodeIsLongString(item) && value.hasOwnProperty("fullText");
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

  return value.class === "Map" || value.class === "Set" || value.class === "WeakMap" || value.class === "WeakSet" || value.class === "Storage";
}

function nodeHasAllEntriesInPreview(item) {
  const { preview } = getValue(item) || {};
  if (!preview) {
    return false;
  }

  const { entries, items, length, size } = preview;

  if (!entries && !items) {
    return false;
  }

  return entries ? entries.length === size : items.length === length;
}

function nodeNeedsNumericalBuckets(item) {
  return nodeSupportsNumericalBucketing(item) && getNumericalPropertiesCount(item) > MAX_NUMERICAL_PROPERTIES;
}

function makeNodesForPromiseProperties(item) {
  const {
    promiseState: { reason, value, state }
  } = getValue(item);

  const properties = [];

  if (state) {
    properties.push(createNode({
      parent: item,
      name: "<state>",
      contents: { value: state },
      type: NODE_TYPES.PROMISE_STATE
    }));
  }

  if (reason) {
    properties.push(createNode({
      parent: item,
      name: "<reason>",
      contents: { value: reason },
      type: NODE_TYPES.PROMISE_REASON
    }));
  }

  if (value) {
    properties.push(createNode({
      parent: item,
      name: "<value>",
      contents: { value: value },
      type: NODE_TYPES.PROMISE_VALUE
    }));
  }

  return properties;
}

function makeNodesForProxyProperties(item) {
  const { proxyHandler, proxyTarget } = getValue(item);

  return [createNode({
    parent: item,
    name: "<target>",
    contents: { value: proxyTarget },
    type: NODE_TYPES.PROXY_TARGET
  }), createNode({
    parent: item,
    name: "<handler>",
    contents: { value: proxyHandler },
    type: NODE_TYPES.PROXY_HANDLER
  })];
}

function makeNodesForEntries(item) {
  const nodeName = "<entries>";
  const entriesPath = "<entries>";

  if (nodeHasAllEntriesInPreview(item)) {
    let entriesNodes = [];
    const { preview } = getValue(item);
    if (preview.entries) {
      entriesNodes = preview.entries.map(([key, value], index) => {
        return createNode({
          parent: item,
          name: index,
          path: `${entriesPath}/${index}`,
          contents: { value: GripMapEntryRep.createGripMapEntry(key, value) }
        });
      });
    } else if (preview.items) {
      entriesNodes = preview.items.map((value, index) => {
        return createNode({
          parent: item,
          name: index,
          path: `${entriesPath}/${index}`,
          contents: { value }
        });
      });
    }
    return createNode({
      parent: item,
      name: nodeName,
      contents: entriesNodes,
      type: NODE_TYPES.ENTRIES
    });
  }
  return createNode({
    parent: item,
    name: nodeName,
    contents: null,
    type: NODE_TYPES.ENTRIES
  });
}

function makeNodesForMapEntry(item) {
  const nodeValue = getValue(item);
  if (!nodeValue || !nodeValue.preview) {
    return [];
  }

  const { key, value } = nodeValue.preview;

  return [createNode({
    parent: item,
    name: "<key>",
    contents: { value: key },
    type: NODE_TYPES.MAP_ENTRY_KEY
  }), createNode({
    parent: item,
    name: "<value>",
    contents: { value },
    type: NODE_TYPES.MAP_ENTRY_VALUE
  })];
}

function getNodeGetter(item) {
  return item && item.contents ? item.contents.get : undefined;
}

function getNodeSetter(item) {
  return item && item.contents ? item.contents.set : undefined;
}

function makeNodesForAccessors(item) {
  const accessors = [];

  const getter = getNodeGetter(item);
  if (getter && getter.type !== "undefined") {
    accessors.push(createNode({
      parent: item,
      name: "<get>",
      contents: { value: getter },
      type: NODE_TYPES.GET
    }));
  }

  const setter = getNodeSetter(item);
  if (setter && setter.type !== "undefined") {
    accessors.push(createNode({
      parent: item,
      name: "<set>",
      contents: { value: setter },
      type: NODE_TYPES.SET
    }));
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
  const numProperties = getNumericalPropertiesCount(parent);

  // We want to have at most a hundred slices.
  const bucketSize = 10 ** Math.max(2, Math.ceil(Math.log10(numProperties)) - 2);
  const numBuckets = Math.ceil(numProperties / bucketSize);

  const buckets = [];
  for (let i = 1; i <= numBuckets; i++) {
    const minKey = (i - 1) * bucketSize;
    const maxKey = Math.min(i * bucketSize - 1, numProperties - 1);
    const startIndex = nodeIsBucket(parent) ? parent.meta.startIndex : 0;
    const minIndex = startIndex + minKey;
    const maxIndex = startIndex + maxKey;
    const bucketName = `[${minIndex}…${maxIndex}]`;

    buckets.push(createNode({
      parent,
      name: bucketName,
      contents: null,
      type: NODE_TYPES.BUCKET,
      meta: {
        startIndex: minIndex,
        endIndex: maxIndex
      }
    }));
  }
  return buckets;
}

function makeDefaultPropsBucket(propertiesNames, parent, ownProperties) {
  const userPropertiesNames = [];
  const defaultProperties = [];

  propertiesNames.forEach(name => {
    if (isDefaultWindowProperty(name)) {
      defaultProperties.push(name);
    } else {
      userPropertiesNames.push(name);
    }
  });

  const nodes = makeNodesForOwnProps(userPropertiesNames, parent, ownProperties);

  if (defaultProperties.length > 0) {
    const defaultPropertiesNode = createNode({
      parent,
      name: "<default properties>",
      contents: null,
      type: NODE_TYPES.DEFAULT_PROPERTIES
    });

    const defaultNodes = defaultProperties.map((name, index) => createNode({
      parent: defaultPropertiesNode,
      name: maybeEscapePropertyName(name),
      path: `${index}/${name}`,
      contents: ownProperties[name]
    }));
    nodes.push(setNodeChildren(defaultPropertiesNode, defaultNodes));
  }
  return nodes;
}

function makeNodesForOwnProps(propertiesNames, parent, ownProperties) {
  return propertiesNames.map(name => createNode({
    parent,
    name: maybeEscapePropertyName(name),
    contents: ownProperties[name]
  }));
}

function makeNodesForProperties(objProps, parent) {
  const {
    ownProperties = {},
    ownSymbols,
    prototype,
    safeGetterValues
  } = objProps;

  const parentValue = getValue(parent);

  const allProperties = _extends({}, ownProperties, safeGetterValues);

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
      nodes.push(createNode({
        parent,
        name: ownSymbol.name,
        path: `symbol-${index}`,
        contents: ownSymbol.descriptor || null
      }));
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

function setNodeFullText(loadedProps, node) {
  if (nodeHasFullText(node)) {
    return node;
  }

  if (nodeIsLongString(node)) {
    node.contents.value.fullText = loadedProps.fullText;
  }

  return node;
}

function makeNodeForPrototype(objProps, parent) {
  const { prototype } = objProps || {};

  // Add the prototype if it exists and is not null
  if (prototype && prototype.type !== "null") {
    return createNode({
      parent,
      name: "<prototype>",
      contents: { value: prototype },
      type: NODE_TYPES.PROTOTYPE
    });
  }

  return null;
}

function createNode(options) {
  const {
    parent,
    name,
    path,
    contents,
    type = NODE_TYPES.GRIP,
    meta
  } = options;

  if (contents === undefined) {
    return null;
  }

  // The path is important to uniquely identify the item in the entire
  // tree. This helps debugging & optimizes React's rendering of large
  // lists. The path will be separated by property name, wrapped in a Symbol
  // to avoid name clashing,
  // i.e. `{ foo: { bar: { baz: 5 }}}` will have a path of Symbol(`foo/bar/baz`)
  // for the inner object.
  return {
    parent,
    name,
    path: parent ? Symbol(`${getSymbolDescriptor(parent.path)}/${path || name}`) : Symbol(path || name),
    contents,
    type,
    meta
  };
}

function getSymbolDescriptor(symbol) {
  return symbol.toString().replace(/^(Symbol\()(.*)(\))$/, "$2");
}

function setNodeChildren(node, children) {
  node.contents = children;
  return node;
}

function getChildren(options) {
  const { cachedNodes, loadedProperties = new Map(), item } = options;

  const key = item.path;
  if (cachedNodes && cachedNodes.has(key)) {
    return cachedNodes.get(key);
  }

  const loadedProps = loadedProperties.get(key);
  const hasLoadedProps = loadedProperties.has(key);

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
    return addToCache(makeNodesForProxyProperties(item));
  }

  if (nodeIsLongString(item) && hasLoadedProps) {
    // Set longString object's fullText to fetched one.
    return addToCache(setNodeFullText(loadedProps, item));
  }

  if (nodeNeedsNumericalBuckets(item) && hasLoadedProps) {
    // Even if we have numerical buckets, we should have loaded non indexed
    // properties.
    const bucketNodes = makeNumericalBuckets(item);
    return addToCache(bucketNodes.concat(makeNodesForProperties(loadedProps, item)));
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
  nodeIsBlock,
  nodeIsBucket,
  nodeIsDefaultProperties,
  nodeIsEntries,
  nodeIsError,
  nodeIsLongString,
  nodeHasFullText,
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
  nodeIsUninitializedBinding,
  nodeIsUnmappedBinding,
  nodeIsUnscopedBinding,
  nodeIsWindow,
  nodeNeedsNumericalBuckets,
  nodeSupportsNumericalBucketing,
  setNodeChildren,
  sortProperties,
  NODE_TYPES
};

/***/ }),

/***/ 3669:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


var _tree = __webpack_require__(3670);

var _tree2 = _interopRequireDefault(_tree);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

module.exports = {
  Tree: _tree2.default
}; /* This Source Code Form is subject to the terms of the Mozilla Public
    * License, v. 2.0. If a copy of the MPL was not distributed with this
    * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/***/ }),

/***/ 3670:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


Object.defineProperty(exports, "__esModule", {
  value: true
});

var _react = __webpack_require__(0);

var _react2 = _interopRequireDefault(_react);

var _reactDomFactories = __webpack_require__(3643);

var _reactDomFactories2 = _interopRequireDefault(_reactDomFactories);

var _propTypes = __webpack_require__(3642);

var _propTypes2 = _interopRequireDefault(_propTypes);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

const { Component, createFactory } = _react2.default; /* This Source Code Form is subject to the terms of the Mozilla Public
                                                       * License, v. 2.0. If a copy of the MPL was not distributed with this
                                                       * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

__webpack_require__(3671);

// depth
const AUTO_EXPAND_DEPTH = 0;

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
    const { expanded } = this.props;

    const classNames = ["arrow"];
    if (expanded) {
      classNames.push("expanded");
    }
    return _reactDomFactories2.default.img({
      className: classNames.join(" ")
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
    const items = indents.concat(renderItem(item, depth, focused, arrow, expanded));

    return _reactDomFactories2.default.div({
      id,
      className: `tree-node${focused ? " focused" : ""}`,
      onClick: this.props.onClick,
      role: "treeitem",
      "aria-level": depth + 1,
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
 * The tree comes with basic styling for the indent, the arrow, as well as
 * hovered and focused styles which can be override in CSS.
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
      // ID of an element whose textual content serves as an accessible label
      // for a tree.
      labelledby: _propTypes2.default.string,
      // Accessibility label for a tree widget.
      label: _propTypes2.default.string,

      // Optional event handlers for when items are expanded or collapsed.
      // Useful for dispatching redux events and updating application state,
      // maybe lazily loading subtrees from a worker, etc.
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
      // Optional event handler called with the current focused node when the
      // Enter key is pressed. Can be useful to allow further keyboard actions
      // within the tree node.
      onActivate: _propTypes2.default.func,
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
    this._focusFirstNode = oncePerAnimationFrame(this._focusFirstNode).bind(this);
    this._focusLastNode = oncePerAnimationFrame(this._focusLastNode).bind(this);

    this._autoExpand = this._autoExpand.bind(this);
    this._preventArrowKeyScrolling = this._preventArrowKeyScrolling.bind(this);
    this._dfs = this._dfs.bind(this);
    this._dfsFromRoots = this._dfsFromRoots.bind(this);
    this._focus = this._focus.bind(this);
    this._scrollNodeIntoView = this._scrollNodeIntoView.bind(this);
    this._onBlur = this._onBlur.bind(this);
    this._onKeyDown = this._onKeyDown.bind(this);
    this._nodeIsExpandable = this._nodeIsExpandable.bind(this);
    this._activateNode = oncePerAnimationFrame(this._activateNode).bind(this);
  }

  componentDidMount() {
    this._autoExpand();
    if (this.props.focused) {
      this._scrollNodeIntoView(this.props.focused);
      // Always keep the focus on the tree itself.
      this.treeRef.focus();
    }
  }

  componentWillReceiveProps(nextProps) {
    this._autoExpand();
  }

  componentDidUpdate(prevProps, prevState) {
    if (this.props.focused && prevProps.focused !== this.props.focused) {
      this._scrollNodeIntoView(this.props.focused);
      // Always keep the focus on the tree itself.
      this.treeRef.focus();
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
   *          - dir: "up" or "down" to indicate if we should scroll the element
   *                 to the top or the bottom of the scrollable container when
   *                 the element is off canvas.
   */
  _focus(item, options = {}) {
    const { preventAutoScroll } = options;
    if (item && !preventAutoScroll) {
      this._scrollNodeIntoView(item, options);
    }
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
   *          - dir: "up" or "down" to indicate if we should scroll the element
   *                 to the top or the bottom of the scrollable container when
   *                 the element is off canvas.
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
        const scrolledParentRect = scrolledParent ? scrolledParent.getBoundingClientRect() : null;
        const isVisible = !scrolledParent || top >= scrolledParentRect.top && bottom <= scrolledParentRect.bottom;

        if (!isVisible) {
          const { alignTo } = options;
          const scrollToTop = alignTo ? alignTo === "top" : !scrolledParentRect || top < scrolledParentRect.top;
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
        return;

      case "Home":
        this._focusFirstNode();
        return;

      case "End":
        this._focusLastNode();
        return;

      case "Enter":
        this._activateNode();
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

  _focusFirstNode() {
    const traversal = this._dfsFromRoots();
    this._focus(traversal[0].item, { alignTo: "top" });
  }

  _focusLastNode() {
    const traversal = this._dfsFromRoots();
    const lastIndex = traversal.length - 1;
    this._focus(traversal[lastIndex].item, { alignTo: "bottom" });
  }

  _activateNode() {
    if (this.props.onActivate) {
      this.props.onActivate(this.props.focused);
    }
  }

  _nodeIsExpandable(item) {
    return this.props.isExpandable ? this.props.isExpandable(item) : !!this.props.getChildren(item).length;
  }

  render() {
    const traversal = this._dfsFromRoots();
    const { focused } = this.props;

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
          // We can stop the propagation since click handler on the node can be
          // created in `renderItem`.
          e.stopPropagation();

          // Since the user just clicked the node, there's no need to check if
          // it should be scrolled into view.
          this._focus(item, { preventAutoScroll: true });
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

        const { explicitOriginalTarget } = nativeEvent;

        // Only set default focus to the first tree node if the focus came
        // from outside the tree (e.g. by tabbing to the tree from other
        // external elements).
        if (explicitOriginalTarget !== this.treeRef && !this.treeRef.contains(explicitOriginalTarget)) {
          this._focus(traversal[0].item);
        }
      },
      onBlur: this._onBlur,
      "aria-label": this.props.label,
      "aria-labelledby": this.props.labelledby,
      "aria-activedescendant": focused && this.props.getKey(focused),
      style
    }, nodes);
  }
}

exports.default = Tree;

/***/ }),

/***/ 3671:
/***/ (function(module, exports) {

// removed by extract-text-webpack-plugin

/***/ }),

/***/ 3672:
/***/ (function(module, exports) {

// removed by extract-text-webpack-plugin

/***/ }),

/***/ 3673:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Dependencies
const { getGripType, wrapRender } = __webpack_require__(3644);

const dom = __webpack_require__(3643);
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

/***/ 3674:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Dependencies
const { wrapRender } = __webpack_require__(3644);
const dom = __webpack_require__(3643);
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

/***/ 3675:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Dependencies
const PropTypes = __webpack_require__(3642);

const { getGripType, wrapRender } = __webpack_require__(3644);

const dom = __webpack_require__(3643);
const { span } = dom;

/**
 * Renders a number
 */
Number.propTypes = {
  object: PropTypes.oneOfType([PropTypes.object, PropTypes.number, PropTypes.bool]).isRequired
};

function Number(props) {
  const value = props.object;

  return span({ className: "objectBox objectBox-number" }, stringify(value));
}

function stringify(object) {
  const isNegativeZero = Object.is(object, -0) || object.type && object.type == "-0";

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

/***/ 3676:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


var _extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; };

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Dependencies
const PropTypes = __webpack_require__(3642);
const { wrapRender, ellipsisElement } = __webpack_require__(3644);
const PropRep = __webpack_require__(3650);
const { MODE } = __webpack_require__(3645);

const dom = __webpack_require__(3643);
const { span } = dom;

const DEFAULT_TITLE = "Object";

/**
 * Renders an object. An object is represented by a list of its
 * properties enclosed in curly brackets.
 */
ObjectRep.propTypes = {
  object: PropTypes.object.isRequired,
  // @TODO Change this to Object.values when supported in Node's version of V8
  mode: PropTypes.oneOf(Object.keys(MODE).map(key => MODE[key])),
  title: PropTypes.string
};

function ObjectRep(props) {
  const object = props.object;
  const propsArray = safePropIterator(props, object);

  if (props.mode === MODE.TINY) {
    const tinyModeItems = [];
    if (getTitle(props, object) !== DEFAULT_TITLE) {
      tinyModeItems.push(getTitleElement(props, object));
    } else {
      tinyModeItems.push(span({
        className: "objectLeftBrace"
      }, "{"), propsArray.length > 0 ? ellipsisElement : null, span({
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
  return props.title || DEFAULT_TITLE;
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
    elements.push(PropRep(_extends({}, props, {
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
    for (const name of propertiesNames) {
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
        // If the property is not important, put its name on an array for later
        // use.
        unimportantProperties.push(name);
      }
    }
  } catch (err) {
    console.error(err);
  }

  if (propertiesNumber < max) {
    for (const name of unimportantProperties) {
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
    elements.push(ellipsisElement);
  }

  return elements;
}

function isInterestingProp(value) {
  const type = typeof value;
  return type == "boolean" || type == "number" || type == "string" && value;
}

function supportsObject(object, noGrip = false) {
  return noGrip;
}

// Exports from this module
module.exports = {
  rep: wrapRender(ObjectRep),
  supportsObject
};

/***/ }),

/***/ 3677:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Dependencies
const PropTypes = __webpack_require__(3642);

const { getGripType, wrapRender } = __webpack_require__(3644);

const dom = __webpack_require__(3643);
const { span } = dom;

/**
 * Renders a symbol.
 */
SymbolRep.propTypes = {
  object: PropTypes.object.isRequired
};

function SymbolRep(props) {
  const { className = "objectBox objectBox-symbol", object } = props;
  const { name } = object;

  return span({
    className,
    "data-link-actor-id": object.actor
  }, `Symbol(${name || ""})`);
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

/***/ 3678:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Dependencies
const PropTypes = __webpack_require__(3642);

const { getGripType, wrapRender } = __webpack_require__(3644);

const dom = __webpack_require__(3643);
const { span } = dom;

/**
 * Renders a Infinity object
 */
InfinityRep.propTypes = {
  object: PropTypes.object.isRequired
};

function InfinityRep(props) {
  const { object } = props;

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

/***/ 3679:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Dependencies
const { getGripType, wrapRender } = __webpack_require__(3644);

const dom = __webpack_require__(3643);
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

/***/ 3680:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Dependencies
const dom = __webpack_require__(3643);
const PropTypes = __webpack_require__(3642);
const { wrapRender } = __webpack_require__(3644);
const { MODE } = __webpack_require__(3645);
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
  const { object } = props;

  const accessors = [];
  if (hasGetter(object)) {
    accessors.push("Getter");
  }
  if (hasSetter(object)) {
    accessors.push("Setter");
  }
  const title = accessors.join(" & ");

  return span({ className: "objectBox objectBox-accessor objectTitle" }, title);
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

/***/ 3681:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// ReactJS
const PropTypes = __webpack_require__(3642);
const dom = __webpack_require__(3643);
const { span } = dom;

// Reps
const { getGripType, isGrip, wrapRender } = __webpack_require__(3644);
const { rep: StringRep } = __webpack_require__(3648);

/**
 * Renders DOM attribute
 */
Attribute.propTypes = {
  object: PropTypes.object.isRequired
};

function Attribute(props) {
  const { object } = props;
  const value = object.preview.value;

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

/***/ 3682:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// ReactJS
const PropTypes = __webpack_require__(3642);

// Reps
const { getGripType, isGrip, wrapRender } = __webpack_require__(3644);

const dom = __webpack_require__(3643);
const { span } = dom;

/**
 * Used to render JS built-in Date() object.
 */
DateTime.propTypes = {
  object: PropTypes.object.isRequired
};

function DateTime(props) {
  const grip = props.object;
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
  }, `${grip.class} `);
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

/***/ 3683:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// ReactJS
const PropTypes = __webpack_require__(3642);

// Reps
const {
  getGripType,
  isGrip,
  getURLDisplayString,
  wrapRender
} = __webpack_require__(3644);

const dom = __webpack_require__(3643);
const { span } = dom;

/**
 * Renders DOM document object.
 */
Document.propTypes = {
  object: PropTypes.object.isRequired
};

function Document(props) {
  const grip = props.object;
  const location = getLocation(grip);
  return span({
    "data-link-actor-id": grip.actor,
    className: "objectBox objectBox-document"
  }, getTitle(grip), location ? span({ className: "location" }, ` ${location}`) : null);
}

function getLocation(grip) {
  const location = grip.preview.location;
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

  const type = getGripType(object, noGrip);
  return object.preview && (type === "HTMLDocument" || type === "XULDocument");
}

// Exports from this module
module.exports = {
  rep: wrapRender(Document),
  supportsObject
};

/***/ }),

/***/ 3684:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// ReactJS
const PropTypes = __webpack_require__(3642);

// Reps
const { getGripType, isGrip, wrapRender } = __webpack_require__(3644);
const dom = __webpack_require__(3643);
const { span } = dom;

/**
 * Renders DOM documentType object.
 */
DocumentType.propTypes = {
  object: PropTypes.object.isRequired
};

function DocumentType(props) {
  const { object } = props;
  const name = object && object.preview && object.preview.nodeName ? ` ${object.preview.nodeName}` : "";
  return span({
    "data-link-actor-id": props.object.actor,
    className: "objectBox objectBox-document"
  }, `<!DOCTYPE${name}>`);
}

// Registration
function supportsObject(object, noGrip = false) {
  if (noGrip === true || !isGrip(object)) {
    return false;
  }

  const type = getGripType(object, noGrip);
  return object.preview && type === "DocumentType";
}

// Exports from this module
module.exports = {
  rep: wrapRender(DocumentType),
  supportsObject
};

/***/ }),

/***/ 3685:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


var _extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; };

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// ReactJS
const PropTypes = __webpack_require__(3642);

// Reps
const { isGrip, wrapRender } = __webpack_require__(3644);

const { MODE } = __webpack_require__(3645);
const { rep } = __webpack_require__(3656);

/**
 * Renders DOM event objects.
 */
Event.propTypes = {
  object: PropTypes.object.isRequired,
  // @TODO Change this to Object.values when supported in Node's version of V8
  mode: PropTypes.oneOf(Object.keys(MODE).map(key => MODE[key])),
  onDOMNodeMouseOver: PropTypes.func,
  onDOMNodeMouseOut: PropTypes.func,
  onInspectIconClick: PropTypes.func
};

function Event(props) {
  const gripProps = _extends({}, props, {
    title: getTitle(props),
    object: _extends({}, props.object, {
      preview: _extends({}, props.object.preview, {
        ownProperties: {}
      })
    })
  });

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
  const preview = props.object.preview;
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

/***/ 3686:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


var _extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; };

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// ReactJS
const PropTypes = __webpack_require__(3642);
// Dependencies
const { getGripType, isGrip, wrapRender } = __webpack_require__(3644);

const PropRep = __webpack_require__(3650);
const { MODE } = __webpack_require__(3645);

const dom = __webpack_require__(3643);
const { span } = dom;

/**
 * Renders a DOM Promise object.
 */
PromiseRep.propTypes = {
  object: PropTypes.object.isRequired,
  // @TODO Change this to Object.values when supported in Node's version of V8
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
    const { Rep } = __webpack_require__(3647);

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
    const object = promiseState[key];
    res = res.concat(PropRep(_extends({}, props, {
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

/***/ 3687:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// ReactJS
const PropTypes = __webpack_require__(3642);

// Reps
const { getGripType, isGrip, wrapRender } = __webpack_require__(3644);

const dom = __webpack_require__(3643);
const { span } = dom;

/**
 * Renders a grip object with regular expression.
 */
RegExp.propTypes = {
  object: PropTypes.object.isRequired
};

function RegExp(props) {
  const { object } = props;

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

/***/ 3688:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// ReactJS
const PropTypes = __webpack_require__(3642);

// Reps
const {
  getGripType,
  isGrip,
  getURLDisplayString,
  wrapRender
} = __webpack_require__(3644);

const dom = __webpack_require__(3643);
const { span } = dom;

/**
 * Renders a grip representing CSSStyleSheet
 */
StyleSheet.propTypes = {
  object: PropTypes.object.isRequired
};

function StyleSheet(props) {
  const grip = props.object;

  return span({
    "data-link-actor-id": grip.actor,
    className: "objectBox objectBox-object"
  }, getTitle(grip), span({ className: "objectPropValue" }, getLocation(grip)));
}

function getTitle(grip) {
  const title = "StyleSheet ";
  return span({ className: "objectBoxTitle" }, title);
}

function getLocation(grip) {
  // Embedded stylesheets don't have URL and so, no preview.
  const url = grip.preview ? grip.preview.url : "";
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

/***/ 3689:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Dependencies
const PropTypes = __webpack_require__(3642);
const {
  isGrip,
  cropString,
  cropMultipleLines,
  wrapRender
} = __webpack_require__(3644);
const { MODE } = __webpack_require__(3645);
const nodeConstants = __webpack_require__(3659);
const dom = __webpack_require__(3643);
const { span } = dom;

/**
 * Renders DOM comment node.
 */
CommentNode.propTypes = {
  object: PropTypes.object.isRequired,
  // @TODO Change this to Object.values when supported in Node's version of V8
  mode: PropTypes.oneOf(Object.keys(MODE).map(key => MODE[key]))
};

function CommentNode(props) {
  const { object, mode = MODE.SHORT } = props;

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

/***/ 3690:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// ReactJS
const PropTypes = __webpack_require__(3642);

// Utils
const { isGrip, wrapRender } = __webpack_require__(3644);
const { rep: StringRep } = __webpack_require__(3648);
const { MODE } = __webpack_require__(3645);
const nodeConstants = __webpack_require__(3659);

const dom = __webpack_require__(3643);
const { span } = dom;

/**
 * Renders DOM element node.
 */
ElementNode.propTypes = {
  object: PropTypes.object.isRequired,
  inspectIconTitle: PropTypes.string,
  // @TODO Change this to Object.values when supported in Node's version of V8
  mode: PropTypes.oneOf(Object.keys(MODE).map(key => MODE[key])),
  onDOMNodeClick: PropTypes.func,
  onDOMNodeMouseOver: PropTypes.func,
  onDOMNodeMouseOut: PropTypes.func,
  onInspectIconClick: PropTypes.func
};

function ElementNode(props) {
  const {
    object,
    inspectIconTitle,
    mode,
    onDOMNodeClick,
    onDOMNodeMouseOver,
    onDOMNodeMouseOut,
    onInspectIconClick
  } = props;
  const elements = getElements(object, mode);

  const isInTree = object.preview && object.preview.isConnected === true;

  const baseConfig = {
    "data-link-actor-id": object.actor,
    className: "objectBox objectBox-node"
  };
  let inspectIcon;
  if (isInTree) {
    if (onDOMNodeClick) {
      Object.assign(baseConfig, {
        onClick: _ => onDOMNodeClick(object),
        className: `${baseConfig.className} clickable`
      });
    }

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
      inspectIcon = dom.button({
        className: "open-inspector",
        // TODO: Localize this with "openNodeInInspector" when Bug 1317038 lands
        title: inspectIconTitle || "Click to select the node in the inspector",
        onClick: e => {
          if (onDOMNodeClick) {
            e.stopPropagation();
          }

          onInspectIconClick(object, e);
        }
      });
    }
  }

  return span(baseConfig, ...elements, inspectIcon);
}

function getElements(grip, mode) {
  const {
    attributes,
    nodeName,
    isAfterPseudoElement,
    isBeforePseudoElement
  } = grip.preview;
  const nodeNameElement = span({
    className: "tag-name"
  }, nodeName);

  if (isAfterPseudoElement || isBeforePseudoElement) {
    return [span({ className: "attrName" }, `::${isAfterPseudoElement ? "after" : "before"}`)];
  }

  if (mode === MODE.TINY) {
    const elements = [nodeNameElement];
    if (attributes.id) {
      elements.push(span({ className: "attrName" }, `#${attributes.id}`));
    }
    if (attributes.class) {
      elements.push(span({ className: "attrName" }, attributes.class.trim().split(/\s+/).map(cls => `.${cls}`).join("")));
    }
    return elements;
  }

  const attributeKeys = Object.keys(attributes);
  if (attributeKeys.includes("class")) {
    attributeKeys.splice(attributeKeys.indexOf("class"), 1);
    attributeKeys.unshift("class");
  }
  if (attributeKeys.includes("id")) {
    attributeKeys.splice(attributeKeys.indexOf("id"), 1);
    attributeKeys.unshift("id");
  }
  const attributeElements = attributeKeys.reduce((arr, name, i, keys) => {
    const value = attributes[name];
    const attribute = span({}, span({ className: "attrName" }, name), span({ className: "attrEqual" }, "="), StringRep({ className: "attrValue", object: value }));

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

/***/ 3691:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// ReactJS
const PropTypes = __webpack_require__(3642);

// Reps
const { isGrip, cropString, wrapRender } = __webpack_require__(3644);
const { MODE } = __webpack_require__(3645);

const dom = __webpack_require__(3643);
const { span } = dom;

/**
 * Renders DOM #text node.
 */
TextNode.propTypes = {
  object: PropTypes.object.isRequired,
  // @TODO Change this to Object.values when supported in Node's version of V8
  mode: PropTypes.oneOf(Object.keys(MODE).map(key => MODE[key])),
  onDOMNodeMouseOver: PropTypes.func,
  onDOMNodeMouseOut: PropTypes.func,
  onInspectIconClick: PropTypes.func
};

function TextNode(props) {
  const {
    object: grip,
    mode = MODE.SHORT,
    onDOMNodeMouseOver,
    onDOMNodeMouseOut,
    onInspectIconClick
  } = props;

  const baseConfig = {
    "data-link-actor-id": grip.actor,
    className: "objectBox objectBox-textNode"
  };
  let inspectIcon;
  const isInTree = grip.preview && grip.preview.isConnected === true;

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
      inspectIcon = dom.button({
        className: "open-inspector",
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

/***/ 3692:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// ReactJS
const PropTypes = __webpack_require__(3642);

// Reps
const {
  getGripType,
  isGrip,
  getURLDisplayString,
  wrapRender
} = __webpack_require__(3644);

const { MODE } = __webpack_require__(3645);

const dom = __webpack_require__(3643);
const { span } = dom;

/**
 * Renders a grip representing a window.
 */
WindowRep.propTypes = {
  // @TODO Change this to Object.values when supported in Node's version of V8
  mode: PropTypes.oneOf(Object.keys(MODE).map(key => MODE[key])),
  object: PropTypes.object.isRequired
};

function WindowRep(props) {
  const { mode, object } = props;

  const config = {
    "data-link-actor-id": object.actor,
    className: "objectBox objectBox-Window"
  };

  if (mode === MODE.TINY) {
    return span(config, getTitle(object));
  }

  return span(config, getTitle(object, true), span({ className: "location" }, getLocation(object)));
}

function getTitle(object, trailingSpace) {
  let title = object.displayClass || object.class || "Window";
  if (trailingSpace === true) {
    title = `${title} `;
  }
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

/***/ 3693:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// ReactJS
const PropTypes = __webpack_require__(3642);

// Reps
const { isGrip, wrapRender } = __webpack_require__(3644);

const String = __webpack_require__(3648).rep;

const dom = __webpack_require__(3643);
const { span } = dom;

/**
 * Renders a grip object with textual data.
 */
ObjectWithText.propTypes = {
  object: PropTypes.object.isRequired
};

function ObjectWithText(props) {
  const grip = props.object;
  return span({
    "data-link-actor-id": grip.actor,
    className: `objectTitle objectBox objectBox-${getType(grip)}`
  }, `${getType(grip)} `, getDescription(grip));
}

function getType(grip) {
  return grip.class;
}

function getDescription(grip) {
  return String({
    object: grip.preview.text
  });
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

/***/ 3694:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// ReactJS
const PropTypes = __webpack_require__(3642);

// Reps
const { isGrip, getURLDisplayString, wrapRender } = __webpack_require__(3644);

const dom = __webpack_require__(3643);
const { span } = dom;

/**
 * Renders a grip object with URL data.
 */
ObjectWithURL.propTypes = {
  object: PropTypes.object.isRequired
};

function ObjectWithURL(props) {
  const grip = props.object;
  return span({
    "data-link-actor-id": grip.actor,
    className: `objectBox objectBox-${getType(grip)}`
  }, getTitle(grip), span({ className: "objectPropValue" }, getDescription(grip)));
}

function getTitle(grip) {
  return span({ className: "objectTitle" }, `${getType(grip)} `);
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

/***/ 3695:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const { createElement, createFactory, PureComponent } = __webpack_require__(0);
const { Provider } = __webpack_require__(3592);
const ObjectInspector = createFactory(__webpack_require__(3696));
const createStore = __webpack_require__(3700);
const Utils = __webpack_require__(3657);
const { renderRep, shouldRenderRootsInReps } = Utils;

class OI extends PureComponent {
  constructor(props) {
    super(props);
    this.store = createStore(props);
  }

  getStore() {
    return this.store;
  }

  render() {
    return createElement(Provider, { store: this.store }, ObjectInspector(this.props));
  }
}

module.exports = props => {
  const { roots } = props;
  if (shouldRenderRootsInReps(roots)) {
    return renderRep(roots[0], props);
  }
  return new OI(props);
};

/***/ }),

/***/ 3696:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


var _extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; };

var _devtoolsComponents = __webpack_require__(3669);

var _devtoolsComponents2 = _interopRequireDefault(_devtoolsComponents);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const { Component, createFactory } = __webpack_require__(0);
const dom = __webpack_require__(3643);
const { connect } = __webpack_require__(3592);
const { bindActionCreators } = __webpack_require__(3593);

const Tree = createFactory(_devtoolsComponents2.default.Tree);
__webpack_require__(3697);

const classnames = __webpack_require__(175);
const { MODE } = __webpack_require__(3645);

const Utils = __webpack_require__(3657);

const {
  getChildren,
  getClosestGripNode,
  getParent,
  getValue,
  nodeHasAccessors,
  nodeHasProperties,
  nodeIsBlock,
  nodeIsDefaultProperties,
  nodeIsFunction,
  nodeIsGetter,
  nodeIsMapEntry,
  nodeIsMissingArguments,
  nodeIsOptimizedOut,
  nodeIsPrimitive,
  nodeIsPrototype,
  nodeIsSetter,
  nodeIsUninitializedBinding,
  nodeIsUnmappedBinding,
  nodeIsUnscopedBinding,
  nodeIsWindow,
  nodeIsLongString,
  nodeHasFullText
} = Utils.node;

// This implements a component that renders an interactive inspector
// for looking at JavaScript objects. It expects descriptions of
// objects from the protocol, and will dynamically fetch children
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

    const self = this;

    self.getItemChildren = this.getItemChildren.bind(this);
    self.renderTreeItem = this.renderTreeItem.bind(this);
    self.setExpanded = this.setExpanded.bind(this);
    self.focusItem = this.focusItem.bind(this);
    self.getRoots = this.getRoots.bind(this);
  }

  shouldComponentUpdate(nextProps) {
    const { expandedPaths, focusedItem, loadedProperties, roots } = this.props;

    if (roots !== nextProps.roots) {
      // Since the roots changed, we assume the properties did as well,
      // so we need to cleanup the component internal state.

      // We can clear the cachedNodes to avoid bugs and memory leaks.
      this.cachedNodes.clear();
      // The rootsChanged action will be handled in a middleware to release the
      // actors of the old roots, as well as cleanup the state properties
      // (expandedPaths, loadedProperties, …).
      this.props.rootsChanged(nextProps);
      // We don't render right away since the state is going to be changed by
      // the rootsChanged action. The `state.forceUpdate` flag will be set
      // to `true` so we can execute a new render cycle with the cleaned state.
      return false;
    }

    if (nextProps.forceUpdate === true) {
      return true;
    }

    // We should update if:
    // - there are new loaded properties
    // - OR the expanded paths number changed, and all of them have properties
    //      loaded
    // - OR the expanded paths number did not changed, but old and new sets
    //      differ
    // - OR the focused node changed.
    return loadedProperties.size !== nextProps.loadedProperties.size || expandedPaths.size !== nextProps.expandedPaths.size && [...nextProps.expandedPaths].every(path => nextProps.loadedProperties.has(path)) || expandedPaths.size === nextProps.expandedPaths.size && [...nextProps.expandedPaths].some(key => !expandedPaths.has(key)) || focusedItem !== nextProps.focusedItem;
  }

  componentDidUpdate(prevProps) {
    if (this.props.forceUpdate) {
      // If the component was updated, we can then reset the forceUpdate flag.
      this.props.forceUpdated();
    }
  }

  componentWillUnmount() {
    const { releaseActor } = this.props;
    if (typeof releaseActor !== "function") {
      return;
    }

    const { actors } = this.props;
    for (const actor of actors) {
      releaseActor(actor);
    }
  }

  getItemChildren(item) {
    const { loadedProperties } = this.props;
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

  getNodeKey(item) {
    return item.path && typeof item.path.toString === "function" ? item.path.toString() : JSON.stringify(item);
  }

  setExpanded(item, expand) {
    if (nodeIsPrimitive(item)) {
      return;
    }

    const {
      createObjectClient,
      createLongStringClient,
      loadedProperties,
      nodeExpand,
      nodeCollapse,
      roots
    } = this.props;

    if (expand === true) {
      const gripItem = getClosestGripNode(item);
      const value = getValue(gripItem);
      const isRoot = value && roots.some(root => {
        const rootValue = getValue(root);
        return rootValue && rootValue.actor === value.actor;
      });
      const actor = isRoot || !value ? null : value.actor;
      nodeExpand(item, actor, loadedProperties, createObjectClient, createLongStringClient);
    } else {
      nodeCollapse(item);
    }
  }

  focusItem(item) {
    const { focusable = true, focusedItem, nodeFocus, onFocus } = this.props;

    if (focusable && focusedItem !== item) {
      nodeFocus(item);
      if (focusedItem !== item && onFocus) {
        onFocus(item);
      }
    }
  }

  // eslint-disable-next-line complexity
  getTreeItemLabelAndValue(item, depth, expanded) {
    const label = item.name;
    const isPrimitive = nodeIsPrimitive(item);

    if (nodeIsOptimizedOut(item)) {
      return {
        label,
        value: dom.span({ className: "unavailable" }, "(optimized away)")
      };
    }

    if (nodeIsUninitializedBinding(item)) {
      return {
        label,
        value: dom.span({ className: "unavailable" }, "(uninitialized)")
      };
    }

    if (nodeIsUnmappedBinding(item)) {
      return {
        label,
        value: dom.span({ className: "unavailable" }, "(unmapped)")
      };
    }

    if (nodeIsUnscopedBinding(item)) {
      return {
        label,
        value: dom.span({ className: "unavailable" }, "(unscoped)")
      };
    }

    const itemValue = getValue(item);
    const unavailable = isPrimitive && itemValue && itemValue.hasOwnProperty && itemValue.hasOwnProperty("unavailable");

    if (nodeIsMissingArguments(item) || unavailable) {
      return {
        label,
        value: dom.span({ className: "unavailable" }, "(unavailable)")
      };
    }

    if (nodeIsFunction(item) && !nodeIsGetter(item) && !nodeIsSetter(item) && (this.props.mode === MODE.TINY || !this.props.mode)) {
      return {
        label: Utils.renderRep(item, _extends({}, this.props, {
          functionName: label
        }))
      };
    }

    if (nodeHasProperties(item) || nodeHasAccessors(item) || nodeIsMapEntry(item) || nodeIsLongString(item) || isPrimitive) {
      const repProps = _extends({}, this.props);
      if (depth > 0) {
        repProps.mode = this.props.mode === MODE.LONG ? MODE.SHORT : MODE.TINY;
      }
      if (expanded) {
        repProps.mode = MODE.TINY;
      }

      if (nodeIsLongString(item)) {
        repProps.member = {
          open: nodeHasFullText(item) && expanded
        };
      }

      return {
        label,
        value: Utils.renderRep(item, repProps)
      };
    }

    return {
      label
    };
  }

  renderTreeItemLabel(label, item, depth, focused, expanded) {
    if (label === null || typeof label === "undefined") {
      return null;
    }

    const { onLabelClick } = this.props;

    return dom.span({
      className: "object-label",
      onClick: onLabelClick ? event => {
        event.stopPropagation();

        // If the user selected text, bail out.
        if (Utils.selection.documentHasSelection()) {
          return;
        }

        onLabelClick(item, {
          depth,
          focused,
          expanded,
          setExpanded: this.setExpanded
        });
      } : undefined
    }, label);
  }

  getTreeTopElementProps(item, depth, focused, expanded) {
    const { onCmdCtrlClick, onDoubleClick, dimTopLevelWindow } = this.props;

    const parentElementProps = {
      className: classnames("node object-node", {
        focused,
        lessen: !expanded && (nodeIsDefaultProperties(item) || nodeIsPrototype(item) || dimTopLevelWindow === true && nodeIsWindow(item) && depth === 0),
        block: nodeIsBlock(item)
      }),
      onClick: e => {
        if (onCmdCtrlClick && (isMacOS && e.metaKey || !isMacOS && e.ctrlKey)) {
          onCmdCtrlClick(item, {
            depth,
            event: e,
            focused,
            expanded
          });
          e.stopPropagation();
          return;
        }

        // If this click happened because the user selected some text, bail out.
        // Note that if the user selected some text before and then clicks here,
        // the previously selected text will be first unselected, unless the
        // user clicked on the arrow itself. Indeed because the arrow is an
        // image, clicking on it does not remove any existing text selection.
        // So we need to also check if the arrow was clicked.
        if (Utils.selection.documentHasSelection() && !(e.target && e.target.matches && e.target.matches(".arrow"))) {
          e.stopPropagation();
        }
      }
    };

    if (onDoubleClick) {
      parentElementProps.onDoubleClick = e => {
        e.stopPropagation();
        onDoubleClick(item, {
          depth,
          focused,
          expanded
        });
      };
    }

    return parentElementProps;
  }

  renderTreeItem(item, depth, focused, arrow, expanded) {
    const { label, value } = this.getTreeItemLabelAndValue(item, depth, expanded);
    const labelElement = this.renderTreeItemLabel(label, item, depth, focused, expanded);
    const delimiter = value && labelElement ? dom.span({ className: "object-delimiter" }, ": ") : null;

    return dom.div(this.getTreeTopElementProps(item, depth, focused, expanded), arrow, labelElement, delimiter, value);
  }

  render() {
    const {
      autoExpandAll = true,
      autoExpandDepth = 1,
      focusable = true,
      disableWrap = false,
      expandedPaths,
      focusedItem,
      inline
    } = this.props;

    return Tree({
      className: classnames({
        inline,
        nowrap: disableWrap,
        "object-inspector": true
      }),
      autoExpandAll,
      autoExpandDepth,

      isExpanded: item => expandedPaths && expandedPaths.has(item.path),
      isExpandable: item => nodeIsPrimitive(item) === false,
      focused: focusedItem,

      getRoots: this.getRoots,
      getParent,
      getChildren: this.getItemChildren,
      getKey: this.getNodeKey,

      onExpand: item => this.setExpanded(item, true),
      onCollapse: item => this.setExpanded(item, false),
      onFocus: focusable ? this.focusItem : null,

      renderItem: this.renderTreeItem
    });
  }
}

function mapStateToProps(state, props) {
  return {
    actors: state.actors,
    expandedPaths: state.expandedPaths,
    // If the root changes, we want to pass a possibly new focusedItem property
    focusedItem: state.roots !== props.roots ? props.focusedItem : state.focusedItem,
    loadedProperties: state.loadedProperties,
    forceUpdate: state.forceUpdate
  };
}

function mapDispatchToProps(dispatch) {
  return bindActionCreators(__webpack_require__(3699), dispatch);
}

module.exports = connect(mapStateToProps, mapDispatchToProps)(ObjectInspector);

/***/ }),

/***/ 3697:
/***/ (function(module, exports) {

// removed by extract-text-webpack-plugin

/***/ }),

/***/ 3698:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

function documentHasSelection() {
  const selection = getSelection();
  if (!selection) {
    return false;
  }

  return selection.type === "Range";
}

module.exports = {
  documentHasSelection
};

/***/ }),

/***/ 3699:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


const { loadItemProperties } = __webpack_require__(3666); /* This Source Code Form is subject to the terms of the Mozilla Public
                                                                    * License, v. 2.0. If a copy of the MPL was not distributed with this
                                                                    * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * This action is responsible for expanding a given node, which also means that
 * it will call the action responsible to fetch properties.
 */
function nodeExpand(node, actor, loadedProperties, createObjectClient, createLongStringClient) {
  return async ({ dispatch }) => {
    dispatch({
      type: "NODE_EXPAND",
      data: { node }
    });

    if (!loadedProperties.has(node.path)) {
      dispatch(nodeLoadProperties(node, actor, loadedProperties, createObjectClient, createLongStringClient));
    }
  };
}

function nodeCollapse(node) {
  return {
    type: "NODE_COLLAPSE",
    data: { node }
  };
}

function nodeFocus(node) {
  return {
    type: "NODE_FOCUS",
    data: { node }
  };
}
/*
 * This action checks if we need to fetch properties, entries, prototype and
 * symbols for a given node. If we do, it will call the appropriate ObjectClient
 * functions.
 */
function nodeLoadProperties(item, actor, loadedProperties, createObjectClient, createLongStringClient) {
  return async ({ dispatch }) => {
    try {
      const properties = await loadItemProperties(item, createObjectClient, createLongStringClient, loadedProperties);
      dispatch(nodePropertiesLoaded(item, actor, properties));
    } catch (e) {
      console.error(e);
    }
  };
}

function nodePropertiesLoaded(node, actor, properties) {
  return {
    type: "NODE_PROPERTIES_LOADED",
    data: { node, actor, properties }
  };
}

/*
 * This action is dispatched when the `roots` prop, provided by a consumer of
 * the ObjectInspector (inspector, console, …), is modified. It will clean the
 * internal state properties (expandedPaths, loadedProperties, …) and release
 * the actors consumed with the previous roots.
 * It takes a props argument which reflects what is passed by the upper-level
 * consumer.
 */
function rootsChanged(props) {
  return {
    type: "ROOTS_CHANGED",
    data: props
  };
}

/*
 * This action will reset the `forceUpdate` flag in the state.
 */
function forceUpdated() {
  return {
    type: "FORCE_UPDATED"
  };
}

module.exports = {
  forceUpdated,
  nodeExpand,
  nodeCollapse,
  nodeFocus,
  nodeLoadProperties,
  nodePropertiesLoaded,
  rootsChanged
};

/***/ }),

/***/ 3700:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


var _extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; };

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const { applyMiddleware, createStore, compose } = __webpack_require__(3593);
const { thunk } = __webpack_require__(3701);
const {
  waitUntilService
} = __webpack_require__(3702);
const reducer = __webpack_require__(3703);

function createInitialState(overrides) {
  return _extends({
    actors: new Set(),
    expandedPaths: new Set(),
    focusedItem: null,
    loadedProperties: new Map(),
    forceUpdated: false
  }, overrides);
}

function enableStateReinitializer(props) {
  return next => (innerReducer, initialState, enhancer) => {
    function reinitializerEnhancer(state, action) {
      if (action.type !== "ROOTS_CHANGED") {
        return innerReducer(state, action);
      }

      if (props.releaseActor && initialState.actors) {
        initialState.actors.forEach(props.releaseActor);
      }

      return _extends({}, action.data, {
        actors: new Set(),
        expandedPaths: new Set(),
        loadedProperties: new Map(),
        // Indicates to the component that we do want to render on the next
        // render cycle.
        forceUpdate: true
      });
    }
    return next(reinitializerEnhancer, initialState, enhancer);
  };
}

module.exports = props => {
  const middlewares = [thunk];

  if (props.injectWaitService) {
    middlewares.push(waitUntilService);
  }

  return createStore(reducer, createInitialState(props), compose(applyMiddleware(...middlewares), enableStateReinitializer(props)));
};

/***/ }),

/***/ 3701:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * A middleware that allows thunks (functions) to be dispatched.
 * If it's a thunk, it is called with `dispatch` and `getState`,
 * allowing the action to create multiple actions (most likely
 * asynchronously).
 */
function thunk({ dispatch, getState }) {
  return next => action => {
    return typeof action === "function" ? action({ dispatch, getState }) : next(action);
  };
}
exports.thunk = thunk;

/***/ }),

/***/ 3702:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const WAIT_UNTIL_TYPE = "@@service/waitUntil";
/**
 * A middleware which acts like a service, because it is stateful
 * and "long-running" in the background. It provides the ability
 * for actions to install a function to be run once when a specific
 * condition is met by an action coming through the system. Think of
 * it as a thunk that blocks until the condition is met. Example:
 *
 * ```js
 * const services = { WAIT_UNTIL: require('wait-service').NAME };
 *
 * { type: services.WAIT_UNTIL,
 *   predicate: action => action.type === "ADD_ITEM",
 *   run: (dispatch, getState, action) => {
 *     // Do anything here. You only need to accept the arguments
 *     // if you need them. `action` is the action that satisfied
 *     // the predicate.
 *   }
 * }
 * ```
 */
function waitUntilService({ dispatch, getState }) {
  let pending = [];

  function checkPending(action) {
    const readyRequests = [];
    const stillPending = [];

    // Find the pending requests whose predicates are satisfied with
    // this action. Wait to run the requests until after we update the
    // pending queue because the request handler may synchronously
    // dispatch again and run this service (that use case is
    // completely valid).
    for (const request of pending) {
      if (request.predicate(action)) {
        readyRequests.push(request);
      } else {
        stillPending.push(request);
      }
    }

    pending = stillPending;
    for (const request of readyRequests) {
      request.run(dispatch, getState, action);
    }
  }

  return next => action => {
    if (action.type === WAIT_UNTIL_TYPE) {
      pending.push(action);
      return null;
    }
    const result = next(action);
    checkPending(action);
    return result;
  };
}

module.exports = {
  WAIT_UNTIL_TYPE,
  waitUntilService
};

/***/ }),

/***/ 3703:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


var _extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; };

function reducer(state = {}, action) {
  const { type, data } = action;

  const cloneState = overrides => _extends({}, state, overrides);

  if (type === "NODE_EXPAND") {
    return cloneState({
      expandedPaths: new Set(state.expandedPaths).add(data.node.path)
    });
  }

  if (type === "NODE_COLLAPSE") {
    const expandedPaths = new Set(state.expandedPaths);
    expandedPaths.delete(data.node.path);
    return cloneState({ expandedPaths });
  }

  if (type === "NODE_PROPERTIES_LOADED") {
    return cloneState({
      actors: data.actor ? new Set(state.actors || []).add(data.actor) : state.actors,
      loadedProperties: new Map(state.loadedProperties).set(data.node.path, action.data.properties)
    });
  }

  if (type === "NODE_FOCUS") {
    if (state.focusedItem === data.node) {
      return state;
    }

    return cloneState({
      focusedItem: data.node
    });
  }

  if (type === "FORCE_UPDATED") {
    return cloneState({
      forceUpdate: false
    });
  }

  return state;
} /* This Source Code Form is subject to the terms of the Mozilla Public
   * License, v. 2.0. If a copy of the MPL was not distributed with this
   * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */


module.exports = reducer;

/***/ }),

/***/ 3730:
/***/ (function(module, exports, __webpack_require__) {

module.exports = __webpack_require__(3655);


/***/ })

/******/ });
});