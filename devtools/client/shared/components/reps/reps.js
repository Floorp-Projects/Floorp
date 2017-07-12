(function webpackUniversalModuleDefinition(root, factory) {
	if(typeof exports === 'object' && typeof module === 'object')
		module.exports = factory(require("devtools/client/shared/vendor/react"));
	else if(typeof define === 'function' && define.amd)
		define(["devtools/client/shared/vendor/react"], factory);
	else {
		var a = typeof exports === 'object' ? factory(require("devtools/client/shared/vendor/react")) : factory(root["devtools/client/shared/vendor/react"]);
		for(var i in a) (typeof exports === 'object' ? exports : root)[i] = a[i];
	}
})(this, function(__WEBPACK_EXTERNAL_MODULE_8__) {
return /******/ (function(modules) { // webpackBootstrap
/******/ 	// The module cache
/******/ 	var installedModules = {};

/******/ 	// The require function
/******/ 	function __webpack_require__(moduleId) {

/******/ 		// Check if module is in cache
/******/ 		if(installedModules[moduleId])
/******/ 			return installedModules[moduleId].exports;

/******/ 		// Create a new module (and put it into the cache)
/******/ 		var module = installedModules[moduleId] = {
/******/ 			exports: {},
/******/ 			id: moduleId,
/******/ 			loaded: false
/******/ 		};

/******/ 		// Execute the module function
/******/ 		modules[moduleId].call(module.exports, module, module.exports, __webpack_require__);

/******/ 		// Flag the module as loaded
/******/ 		module.loaded = true;

/******/ 		// Return the exports of the module
/******/ 		return module.exports;
/******/ 	}


/******/ 	// expose the modules object (__webpack_modules__)
/******/ 	__webpack_require__.m = modules;

/******/ 	// expose the module cache
/******/ 	__webpack_require__.c = installedModules;

/******/ 	// __webpack_public_path__
/******/ 	__webpack_require__.p = "/assets/build";

/******/ 	// Load entry module and return exports
/******/ 	return __webpack_require__(0);
/******/ })
/************************************************************************/
/******/ ([
/* 0 */
/***/ function(module, exports, __webpack_require__) {

	/* This Source Code Form is subject to the terms of the Mozilla Public
	 * License, v. 2.0. If a copy of the MPL was not distributed with this
	 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

	const { MODE } = __webpack_require__(1);
	const { REPS, getRep } = __webpack_require__(2);
	const ObjectInspector = __webpack_require__(44);

	const {
	  parseURLEncodedText,
	  parseURLParams,
	  maybeEscapePropertyName,
	  getGripPreviewItems
	} = __webpack_require__(7);

	module.exports = {
	  REPS,
	  getRep,
	  MODE,
	  maybeEscapePropertyName,
	  parseURLEncodedText,
	  parseURLParams,
	  getGripPreviewItems,
	  ObjectInspector
	};

/***/ },
/* 1 */
/***/ function(module, exports) {

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

/***/ },
/* 2 */
/***/ function(module, exports, __webpack_require__) {

	/* This Source Code Form is subject to the terms of the Mozilla Public
	 * License, v. 2.0. If a copy of the MPL was not distributed with this
	 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

	__webpack_require__(3);
	const { isGrip } = __webpack_require__(7);

	// Load all existing rep templates
	const Undefined = __webpack_require__(9);
	const Null = __webpack_require__(10);
	const StringRep = __webpack_require__(11);
	const LongStringRep = __webpack_require__(12);
	const Number = __webpack_require__(13);
	const ArrayRep = __webpack_require__(14);
	const Obj = __webpack_require__(16);
	const SymbolRep = __webpack_require__(19);
	const InfinityRep = __webpack_require__(20);
	const NaNRep = __webpack_require__(21);

	// DOM types (grips)
	const Attribute = __webpack_require__(22);
	const DateTime = __webpack_require__(23);
	const Document = __webpack_require__(24);
	const Event = __webpack_require__(25);
	const Func = __webpack_require__(26);
	const PromiseRep = __webpack_require__(27);
	const RegExp = __webpack_require__(28);
	const StyleSheet = __webpack_require__(29);
	const CommentNode = __webpack_require__(30);
	const ElementNode = __webpack_require__(32);
	const TextNode = __webpack_require__(37);
	const ErrorRep = __webpack_require__(38);
	const Window = __webpack_require__(39);
	const ObjectWithText = __webpack_require__(40);
	const ObjectWithURL = __webpack_require__(41);
	const GripArray = __webpack_require__(42);
	const GripMap = __webpack_require__(43);
	const Grip = __webpack_require__(18);

	// List of all registered template.
	// XXX there should be a way for extensions to register a new
	// or modify an existing rep.
	let reps = [RegExp, StyleSheet, Event, DateTime, CommentNode, ElementNode, TextNode, Attribute, LongStringRep, Func, PromiseRep, ArrayRep, Document, Window, ObjectWithText, ObjectWithURL, ErrorRep, GripArray, GripMap, Grip, Undefined, Null, StringRep, Number, SymbolRep, InfinityRep, NaNRep];

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
	  let type = typeof object;
	  if (type == "object" && object instanceof String) {
	    type = "string";
	  } else if (object && type == "object" && object.type && noGrip !== true) {
	    type = object.type;
	  }

	  if (isGrip(object)) {
	    type = object.class;
	  }

	  for (let i = 0; i < reps.length; i++) {
	    let rep = reps[i];
	    try {
	      // supportsObject could return weight (not only true/false
	      // but a number), which would allow to priorities templates and
	      // support better extensibility.
	      if (rep.supportsObject(object, type, noGrip)) {
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

/***/ },
/* 3 */
/***/ function(module, exports) {

	// removed by extract-text-webpack-plugin

/***/ },
/* 4 */,
/* 5 */,
/* 6 */,
/* 7 */
/***/ function(module, exports, __webpack_require__) {

	/* This Source Code Form is subject to the terms of the Mozilla Public
	 * License, v. 2.0. If a copy of the MPL was not distributed with this
	 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

	// Dependencies
	const React = __webpack_require__(8);

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
	// Replacement characters and non-characters.
	"\ufffc-\uffff" +
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

	function rawCropString(text, limit, alternativeText) {
	  if (!alternativeText) {
	    alternativeText = "\u2026";
	  }

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
	      return React.DOM.span({
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

	module.exports = {
	  isGrip,
	  cropString,
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
	  getGripPreviewItems
	};

/***/ },
/* 8 */
/***/ function(module, exports) {

	module.exports = __WEBPACK_EXTERNAL_MODULE_8__;

/***/ },
/* 9 */
/***/ function(module, exports, __webpack_require__) {

	/* This Source Code Form is subject to the terms of the Mozilla Public
	 * License, v. 2.0. If a copy of the MPL was not distributed with this
	 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

	// Dependencies
	const React = __webpack_require__(8);

	const { wrapRender } = __webpack_require__(7);

	// Shortcuts
	const { span } = React.DOM;

	/**
	 * Renders undefined value
	 */
	const Undefined = function () {
	  return span({ className: "objectBox objectBox-undefined" }, "undefined");
	};

	function supportsObject(object, type, noGrip = false) {
	  if (noGrip === true) {
	    return false;
	  }

	  return object && object.type && object.type == "undefined" || type == "undefined";
	}

	// Exports from this module

	module.exports = {
	  rep: wrapRender(Undefined),
	  supportsObject
	};

/***/ },
/* 10 */
/***/ function(module, exports, __webpack_require__) {

	/* This Source Code Form is subject to the terms of the Mozilla Public
	 * License, v. 2.0. If a copy of the MPL was not distributed with this
	 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

	// Dependencies
	const React = __webpack_require__(8);

	const { wrapRender } = __webpack_require__(7);

	// Shortcuts
	const { span } = React.DOM;

	/**
	 * Renders null value
	 */
	function Null(props) {
	  return span({ className: "objectBox objectBox-null" }, "null");
	}

	function supportsObject(object, type, noGrip = false) {
	  if (noGrip === true) {
	    return false;
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

/***/ },
/* 11 */
/***/ function(module, exports, __webpack_require__) {

	/* This Source Code Form is subject to the terms of the Mozilla Public
	 * License, v. 2.0. If a copy of the MPL was not distributed with this
	 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

	// Dependencies
	const React = __webpack_require__(8);

	const {
	  escapeString,
	  rawCropString,
	  sanitizeString,
	  wrapRender
	} = __webpack_require__(7);

	// Shortcuts
	const { span } = React.DOM;

	/**
	 * Renders a string. String value is enclosed within quotes.
	 */
	StringRep.propTypes = {
	  useQuotes: React.PropTypes.bool,
	  escapeWhitespace: React.PropTypes.bool,
	  style: React.PropTypes.object,
	  object: React.PropTypes.string.isRequired,
	  member: React.PropTypes.any,
	  cropLimit: React.PropTypes.number
	};

	function StringRep(props) {
	  let {
	    cropLimit,
	    object: text,
	    member,
	    style,
	    useQuotes = true,
	    escapeWhitespace = true
	  } = props;

	  let config = { className: "objectBox objectBox-string" };
	  if (style) {
	    config.style = style;
	  }

	  if (useQuotes) {
	    text = escapeString(text, escapeWhitespace);
	  } else {
	    text = sanitizeString(text);
	  }

	  if ((!member || !member.open) && cropLimit) {
	    text = rawCropString(text, cropLimit);
	  }

	  return span(config, text);
	}

	function supportsObject(object, type) {
	  return type == "string";
	}

	// Exports from this module

	module.exports = {
	  rep: wrapRender(StringRep),
	  supportsObject
	};

/***/ },
/* 12 */
/***/ function(module, exports, __webpack_require__) {

	/* This Source Code Form is subject to the terms of the Mozilla Public
	 * License, v. 2.0. If a copy of the MPL was not distributed with this
	 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

	// Dependencies
	const React = __webpack_require__(8);
	const {
	  escapeString,
	  sanitizeString,
	  isGrip,
	  wrapRender
	} = __webpack_require__(7);
	// Shortcuts
	const { span } = React.DOM;

	/**
	 * Renders a long string grip.
	 */
	LongStringRep.propTypes = {
	  useQuotes: React.PropTypes.bool,
	  escapeWhitespace: React.PropTypes.bool,
	  style: React.PropTypes.object,
	  cropLimit: React.PropTypes.number.isRequired,
	  member: React.PropTypes.string,
	  object: React.PropTypes.object.isRequired
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

	function supportsObject(object, type, noGrip = false) {
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

/***/ },
/* 13 */
/***/ function(module, exports, __webpack_require__) {

	/* This Source Code Form is subject to the terms of the Mozilla Public
	 * License, v. 2.0. If a copy of the MPL was not distributed with this
	 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

	// Dependencies
	const React = __webpack_require__(8);

	const { wrapRender } = __webpack_require__(7);

	// Shortcuts
	const { span } = React.DOM;

	/**
	 * Renders a number
	 */
	Number.propTypes = {
	  object: React.PropTypes.oneOfType([React.PropTypes.object, React.PropTypes.number, React.PropTypes.bool]).isRequired
	};

	function Number(props) {
	  let value = props.object;

	  return span({ className: "objectBox objectBox-number" }, stringify(value));
	}

	function stringify(object) {
	  let isNegativeZero = Object.is(object, -0) || object.type && object.type == "-0";

	  return isNegativeZero ? "-0" : String(object);
	}

	function supportsObject(object, type) {
	  return ["boolean", "number", "-0"].includes(type);
	}

	// Exports from this module

	module.exports = {
	  rep: wrapRender(Number),
	  supportsObject
	};

/***/ },
/* 14 */
/***/ function(module, exports, __webpack_require__) {

	/* This Source Code Form is subject to the terms of the Mozilla Public
	 * License, v. 2.0. If a copy of the MPL was not distributed with this
	 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

	// Dependencies
	const React = __webpack_require__(8);
	const {
	  wrapRender
	} = __webpack_require__(7);
	const Caption = __webpack_require__(15);
	const { MODE } = __webpack_require__(1);

	const ModePropType = React.PropTypes.oneOf(
	// @TODO Change this to Object.values once it's supported in Node's version of V8
	Object.keys(MODE).map(key => MODE[key]));

	// Shortcuts
	const DOM = React.DOM;

	/**
	 * Renders an array. The array is enclosed by left and right bracket
	 * and the max number of rendered items depends on the current mode.
	 */
	ArrayRep.propTypes = {
	  mode: ModePropType,
	  object: React.PropTypes.array.isRequired
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
	    items = [DOM.span({ className: "length" }, isEmpty ? "" : object.length)];
	    brackets = needSpace(false);
	  } else {
	    items = arrayIterator(props, object, maxLengthMap.get(mode));
	    brackets = needSpace(items.length > 0);
	  }

	  return DOM.span({
	    className: "objectBox objectBox-array" }, DOM.span({
	    className: "arrayLeftBracket"
	  }, brackets.left), ...items, DOM.span({
	    className: "arrayRightBracket"
	  }, brackets.right), DOM.span({
	    className: "arrayProperties",
	    role: "group" }));
	}

	function arrayIterator(props, array, max) {
	  let items = [];
	  let delim;

	  for (let i = 0; i < array.length && i < max; i++) {
	    try {
	      let value = array[i];

	      delim = i == array.length - 1 ? "" : ", ";

	      items.push(ItemRep({
	        object: value,
	        // Hardcode tiny mode to avoid recursive handling.
	        mode: MODE.TINY,
	        delim: delim
	      }));
	    } catch (exc) {
	      items.push(ItemRep({
	        object: exc,
	        mode: MODE.TINY,
	        delim: delim
	      }));
	    }
	  }

	  if (array.length > max) {
	    items.push(Caption({
	      object: DOM.span({}, "more…")
	    }));
	  }

	  return items;
	}

	/**
	 * Renders array item. Individual values are separated by a comma.
	 */
	ItemRep.propTypes = {
	  object: React.PropTypes.any.isRequired,
	  delim: React.PropTypes.string.isRequired,
	  mode: ModePropType
	};

	function ItemRep(props) {
	  const { Rep } = __webpack_require__(2);

	  let {
	    object,
	    delim,
	    mode
	  } = props;
	  return DOM.span({}, Rep({ object: object, mode: mode }), delim);
	}

	function supportsObject(object, type) {
	  return Array.isArray(object) || Object.prototype.toString.call(object) === "[object Arguments]";
	}

	const maxLengthMap = new Map();
	maxLengthMap.set(MODE.SHORT, 3);
	maxLengthMap.set(MODE.LONG, 10);

	// Exports from this module
	module.exports = {
	  rep: wrapRender(ArrayRep),
	  supportsObject,
	  maxLengthMap
	};

/***/ },
/* 15 */
/***/ function(module, exports, __webpack_require__) {

	/* This Source Code Form is subject to the terms of the Mozilla Public
	 * License, v. 2.0. If a copy of the MPL was not distributed with this
	 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

	// Dependencies
	const React = __webpack_require__(8);
	const DOM = React.DOM;

	const { wrapRender } = __webpack_require__(7);

	/**
	 * Renders a caption. This template is used by other components
	 * that needs to distinguish between a simple text/value and a label.
	 */
	Caption.propTypes = {
	  object: React.PropTypes.oneOfType([React.PropTypes.number, React.PropTypes.string]).isRequired
	};

	function Caption(props) {
	  return DOM.span({ "className": "caption" }, props.object);
	}

	// Exports from this module
	module.exports = wrapRender(Caption);

/***/ },
/* 16 */
/***/ function(module, exports, __webpack_require__) {

	/* This Source Code Form is subject to the terms of the Mozilla Public
	 * License, v. 2.0. If a copy of the MPL was not distributed with this
	 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

	// Dependencies
	const React = __webpack_require__(8);
	const {
	  wrapRender
	} = __webpack_require__(7);
	const Caption = __webpack_require__(15);
	const PropRep = __webpack_require__(17);
	const { MODE } = __webpack_require__(1);
	// Shortcuts
	const { span } = React.DOM;
	/**
	 * Renders an object. An object is represented by a list of its
	 * properties enclosed in curly brackets.
	 */
	ObjectRep.propTypes = {
	  object: React.PropTypes.object.isRequired,
	  // @TODO Change this to Object.values once it's supported in Node's version of V8
	  mode: React.PropTypes.oneOf(Object.keys(MODE).map(key => MODE[key])),
	  title: React.PropTypes.string
	};

	function ObjectRep(props) {
	  let object = props.object;
	  let propsArray = safePropIterator(props, object);

	  if (props.mode === MODE.TINY || !propsArray.length) {
	    return span({ className: "objectBox objectBox-object" }, getTitle(props, object));
	  }

	  return span({ className: "objectBox objectBox-object" }, getTitle(props, object), span({
	    className: "objectLeftBrace"
	  }, " { "), ...propsArray, span({
	    className: "objectRightBrace"
	  }, " }"));
	}

	function getTitle(props, object) {
	  let title = props.title || object.class || "Object";
	  return span({ className: "objectTitle" }, title);
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
	  let isInterestingProp = (type, value) => {
	    // Do not pick objects, it could cause recursion.
	    return type == "boolean" || type == "number" || type == "string" && value;
	  };

	  // Work around https://bugzilla.mozilla.org/show_bug.cgi?id=945377
	  if (Object.prototype.toString.call(object) === "[object Generator]") {
	    object = Object.getPrototypeOf(object);
	  }

	  // Object members with non-empty values are preferred since it gives the
	  // user a better overview of the object.
	  let interestingObject = getFilteredObject(object, max, isInterestingProp);

	  if (Object.keys(interestingObject).length < max) {
	    // There are not enough props yet (or at least, not enough props to
	    // be able to know whether we should print "more…" or not).
	    // Let's display also empty members and functions.
	    interestingObject = Object.assign({}, interestingObject, getFilteredObject(object, max - Object.keys(interestingObject).length, (type, value) => !isInterestingProp(type, value)));
	  }

	  let propsArray = getPropsArray(interestingObject);
	  if (Object.keys(object).length > max) {
	    propsArray.push(Caption({
	      object: span({}, "more…")
	    }));
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
	 * Get an array of components representing the properties of the object
	 *
	 * @param {Object} object
	 * @return {Array} Array of PropRep.
	 */
	function getPropsArray(object) {
	  let propsArray = [];

	  if (!object) {
	    return propsArray;
	  }

	  // Hardcode tiny mode to avoid recursive handling.
	  let mode = MODE.TINY;
	  const objectKeys = Object.keys(object);
	  return objectKeys.map((name, i) => PropRep({
	    mode,
	    name,
	    object: object[name],
	    equal: ": "
	  }));
	}

	/**
	 * Get a copy of the object filtered by a given predicate.
	 *
	 * @param {Object} object.
	 * @param {Number} max The maximum length of keys array.
	 * @param {Function} filter Filter the props you want.
	 * @return {Object} the filtered object.
	 */
	function getFilteredObject(object, max, filter) {
	  let filteredObject = {};

	  try {
	    for (let name in object) {
	      if (Object.keys(filteredObject).length >= max) {
	        return filteredObject;
	      }

	      let value;
	      try {
	        value = object[name];
	      } catch (exc) {
	        continue;
	      }

	      let t = typeof value;
	      if (filter(t, value)) {
	        filteredObject[name] = value;
	      }
	    }
	  } catch (err) {
	    console.error(err);
	  }
	  return filteredObject;
	}

	function supportsObject(object, type) {
	  return true;
	}

	// Exports from this module
	module.exports = {
	  rep: wrapRender(ObjectRep),
	  supportsObject
	};

/***/ },
/* 17 */
/***/ function(module, exports, __webpack_require__) {

	/* This Source Code Form is subject to the terms of the Mozilla Public
	 * License, v. 2.0. If a copy of the MPL was not distributed with this
	 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

	// Dependencies
	const React = __webpack_require__(8);
	const {
	  maybeEscapePropertyName,
	  wrapRender
	} = __webpack_require__(7);
	const { MODE } = __webpack_require__(1);
	// Shortcuts
	const { span } = React.DOM;

	/**
	 * Property for Obj (local JS objects), Grip (remote JS objects)
	 * and GripMap (remote JS maps and weakmaps) reps.
	 * It's used to render object properties.
	 */
	PropRep.propTypes = {
	  // Property name.
	  name: React.PropTypes.oneOfType([React.PropTypes.string, React.PropTypes.object]).isRequired,
	  // Equal character rendered between property name and value.
	  equal: React.PropTypes.string,
	  // @TODO Change this to Object.values once it's supported in Node's version of V8
	  mode: React.PropTypes.oneOf(Object.keys(MODE).map(key => MODE[key])),
	  onDOMNodeMouseOver: React.PropTypes.func,
	  onDOMNodeMouseOut: React.PropTypes.func,
	  onInspectIconClick: React.PropTypes.func,
	  // Normally a PropRep will quote a property name that isn't valid
	  // when unquoted; but this flag can be used to suppress the
	  // quoting.
	  suppressQuotes: React.PropTypes.bool
	};

	/**
	 * Function that given a name, a delimiter and an object returns an array
	 * of React elements representing an object property (e.g. `name: value`)
	 *
	 * @param {Object} props
	 * @return {Array} Array of React elements.
	 */
	function PropRep(props) {
	  const Grip = __webpack_require__(18);
	  const { Rep } = __webpack_require__(2);

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

/***/ },
/* 18 */
/***/ function(module, exports, __webpack_require__) {

	/* This Source Code Form is subject to the terms of the Mozilla Public
	 * License, v. 2.0. If a copy of the MPL was not distributed with this
	 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

	// ReactJS
	const React = __webpack_require__(8);
	// Dependencies
	const {
	  isGrip,
	  wrapRender
	} = __webpack_require__(7);
	const Caption = __webpack_require__(15);
	const PropRep = __webpack_require__(17);
	const { MODE } = __webpack_require__(1);
	// Shortcuts
	const { span } = React.DOM;

	/**
	 * Renders generic grip. Grip is client representation
	 * of remote JS object and is used as an input object
	 * for this rep component.
	 */
	GripRep.propTypes = {
	  object: React.PropTypes.object.isRequired,
	  // @TODO Change this to Object.values once it's supported in Node's version of V8
	  mode: React.PropTypes.oneOf(Object.keys(MODE).map(key => MODE[key])),
	  isInterestingProp: React.PropTypes.func,
	  title: React.PropTypes.string,
	  onDOMNodeMouseOver: React.PropTypes.func,
	  onDOMNodeMouseOut: React.PropTypes.func,
	  onInspectIconClick: React.PropTypes.func,
	  noGrip: React.PropTypes.bool
	};

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
	    return span(config, getTitle(props, object));
	  }

	  let propsArray = safePropIterator(props, object, maxLengthMap.get(mode));

	  return span(config, getTitle(props, object), span({
	    className: "objectLeftBrace"
	  }, " { "), ...propsArray, span({
	    className: "objectRightBrace"
	  }, " }"));
	}

	function getTitle(props, object) {
	  let title = props.title || object.class || "Object";
	  return span({
	    className: "objectTitle"
	  }, title);
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
	    const { Rep } = __webpack_require__(2);

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
	  let propertiesLength = object.preview && object.preview.ownPropertiesLength ? object.preview.ownPropertiesLength : object.ownPropertyLength;

	  if (object.preview && object.preview.safeGetterValues) {
	    properties = Object.assign({}, properties, object.preview.safeGetterValues);
	    propertiesLength += Object.keys(object.preview.safeGetterValues).length;
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
	  if (Object.keys(properties).length > max || propertiesLength > max) {
	    // There are some undisplayed props. Then display "more...".
	    propsArray.push(Caption({
	      object: span({}, "more…")
	    }));
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
	function supportsObject(object, type, noGrip = false) {
	  if (noGrip === true || !isGrip(object)) {
	    return false;
	  }
	  return object.preview && object.preview.ownProperties;
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

/***/ },
/* 19 */
/***/ function(module, exports, __webpack_require__) {

	/* This Source Code Form is subject to the terms of the Mozilla Public
	 * License, v. 2.0. If a copy of the MPL was not distributed with this
	 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

	// Dependencies
	const React = __webpack_require__(8);

	const { wrapRender } = __webpack_require__(7);

	// Shortcuts
	const { span } = React.DOM;

	/**
	 * Renders a symbol.
	 */
	SymbolRep.propTypes = {
	  object: React.PropTypes.object.isRequired
	};

	function SymbolRep(props) {
	  let { object } = props;
	  let { name } = object;

	  return span({ className: "objectBox objectBox-symbol" }, `Symbol(${name || ""})`);
	}

	function supportsObject(object, type) {
	  return type == "symbol";
	}

	// Exports from this module
	module.exports = {
	  rep: wrapRender(SymbolRep),
	  supportsObject
	};

/***/ },
/* 20 */
/***/ function(module, exports, __webpack_require__) {

	/* This Source Code Form is subject to the terms of the Mozilla Public
	 * License, v. 2.0. If a copy of the MPL was not distributed with this
	 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

	// Dependencies
	const React = __webpack_require__(8);

	const { wrapRender } = __webpack_require__(7);

	// Shortcuts
	const { span } = React.DOM;

	/**
	 * Renders a Infinity object
	 */
	InfinityRep.propTypes = {
	  object: React.PropTypes.object.isRequired
	};

	function InfinityRep(props) {
	  const {
	    object
	  } = props;

	  return span({ className: "objectBox objectBox-number" }, object.type);
	}

	function supportsObject(object, type) {
	  return type == "Infinity" || type == "-Infinity";
	}

	// Exports from this module
	module.exports = {
	  rep: wrapRender(InfinityRep),
	  supportsObject
	};

/***/ },
/* 21 */
/***/ function(module, exports, __webpack_require__) {

	/* This Source Code Form is subject to the terms of the Mozilla Public
	 * License, v. 2.0. If a copy of the MPL was not distributed with this
	 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

	// Dependencies
	const React = __webpack_require__(8);

	const { wrapRender } = __webpack_require__(7);

	// Shortcuts
	const { span } = React.DOM;

	/**
	 * Renders a NaN object
	 */
	function NaNRep(props) {
	  return span({ className: "objectBox objectBox-nan" }, "NaN");
	}

	function supportsObject(object, type) {
	  return type == "NaN";
	}

	// Exports from this module
	module.exports = {
	  rep: wrapRender(NaNRep),
	  supportsObject
	};

/***/ },
/* 22 */
/***/ function(module, exports, __webpack_require__) {

	/* This Source Code Form is subject to the terms of the Mozilla Public
	 * License, v. 2.0. If a copy of the MPL was not distributed with this
	 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

	// ReactJS
	const React = __webpack_require__(8);

	// Reps
	const {
	  isGrip,
	  wrapRender
	} = __webpack_require__(7);
	const { rep: StringRep } = __webpack_require__(11);

	// Shortcuts
	const { span } = React.DOM;

	/**
	 * Renders DOM attribute
	 */
	Attribute.propTypes = {
	  object: React.PropTypes.object.isRequired
	};

	function Attribute(props) {
	  let {
	    object
	  } = props;
	  let value = object.preview.value;

	  return span({
	    "data-link-actor-id": object.actor,
	    className: "objectLink-Attr"
	  }, span({ className: "attrTitle" }, getTitle(object)), span({ className: "attrEqual" }, "="), StringRep({ object: value }));
	}

	function getTitle(grip) {
	  return grip.preview.nodeName;
	}

	// Registration
	function supportsObject(grip, type, noGrip = false) {
	  if (noGrip === true || !isGrip(grip)) {
	    return false;
	  }

	  return type == "Attr" && grip.preview;
	}

	module.exports = {
	  rep: wrapRender(Attribute),
	  supportsObject
	};

/***/ },
/* 23 */
/***/ function(module, exports, __webpack_require__) {

	/* This Source Code Form is subject to the terms of the Mozilla Public
	 * License, v. 2.0. If a copy of the MPL was not distributed with this
	 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

	// ReactJS
	const React = __webpack_require__(8);

	// Reps
	const {
	  isGrip,
	  wrapRender
	} = __webpack_require__(7);

	// Shortcuts
	const { span } = React.DOM;

	/**
	 * Used to render JS built-in Date() object.
	 */
	DateTime.propTypes = {
	  object: React.PropTypes.object.isRequired
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
	function supportsObject(grip, type, noGrip = false) {
	  if (noGrip === true || !isGrip(grip)) {
	    return false;
	  }

	  return type == "Date" && grip.preview;
	}

	// Exports from this module
	module.exports = {
	  rep: wrapRender(DateTime),
	  supportsObject
	};

/***/ },
/* 24 */
/***/ function(module, exports, __webpack_require__) {

	/* This Source Code Form is subject to the terms of the Mozilla Public
	 * License, v. 2.0. If a copy of the MPL was not distributed with this
	 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

	// ReactJS
	const React = __webpack_require__(8);

	// Reps
	const {
	  isGrip,
	  getURLDisplayString,
	  wrapRender
	} = __webpack_require__(7);

	// Shortcuts
	const { span } = React.DOM;

	/**
	 * Renders DOM document object.
	 */
	Document.propTypes = {
	  object: React.PropTypes.object.isRequired
	};

	function Document(props) {
	  let grip = props.object;

	  return span({
	    "data-link-actor-id": grip.actor,
	    className: "objectBox objectBox-object"
	  }, getTitle(grip), span({ className: "objectPropValue" }, getLocation(grip)));
	}

	function getLocation(grip) {
	  let location = grip.preview.location;
	  return location ? getURLDisplayString(location) : "";
	}

	function getTitle(grip) {
	  return span({
	    className: "objectTitle"
	  }, grip.class + " ");
	}

	// Registration
	function supportsObject(object, type, noGrip = false) {
	  if (noGrip === true || !isGrip(object)) {
	    return false;
	  }

	  return object.preview && type == "HTMLDocument";
	}

	// Exports from this module
	module.exports = {
	  rep: wrapRender(Document),
	  supportsObject
	};

/***/ },
/* 25 */
/***/ function(module, exports, __webpack_require__) {

	/* This Source Code Form is subject to the terms of the Mozilla Public
	 * License, v. 2.0. If a copy of the MPL was not distributed with this
	 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

	// ReactJS
	const React = __webpack_require__(8);

	// Reps
	const {
	  isGrip,
	  wrapRender
	} = __webpack_require__(7);

	const { MODE } = __webpack_require__(1);
	const { rep } = __webpack_require__(18);

	/**
	 * Renders DOM event objects.
	 */
	Event.propTypes = {
	  object: React.PropTypes.object.isRequired,
	  // @TODO Change this to Object.values once it's supported in Node's version of V8
	  mode: React.PropTypes.oneOf(Object.keys(MODE).map(key => MODE[key])),
	  onDOMNodeMouseOver: React.PropTypes.func,
	  onDOMNodeMouseOut: React.PropTypes.func,
	  onInspectIconClick: React.PropTypes.func
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
	function supportsObject(grip, type, noGrip = false) {
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

/***/ },
/* 26 */
/***/ function(module, exports, __webpack_require__) {

	/* This Source Code Form is subject to the terms of the Mozilla Public
	 * License, v. 2.0. If a copy of the MPL was not distributed with this
	 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

	// ReactJS
	const React = __webpack_require__(8);

	// Reps
	const {
	  isGrip,
	  cropString,
	  wrapRender
	} = __webpack_require__(7);

	// Shortcuts
	const { span } = React.DOM;

	/**
	 * This component represents a template for Function objects.
	 */
	FunctionRep.propTypes = {
	  object: React.PropTypes.object.isRequired,
	  parameterNames: React.PropTypes.array
	};

	function FunctionRep(props) {
	  let grip = props.object;

	  return span({
	    "data-link-actor-id": grip.actor,
	    className: "objectBox objectBox-function",
	    // Set dir="ltr" to prevent function parentheses from
	    // appearing in the wrong direction
	    dir: "ltr"
	  }, getTitle(props, grip), summarizeFunction(grip), "(", ...renderParams(props), ")");
	}

	function getTitle(props, grip) {
	  let title = "function ";
	  if (grip.isGenerator) {
	    title = "function* ";
	  }
	  if (grip.isAsync) {
	    title = "async " + title;
	  }

	  return span({
	    className: "objectTitle"
	  }, title);
	}

	function summarizeFunction(grip) {
	  let name = grip.userDisplayName || grip.displayName || grip.name || "";
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
	function supportsObject(grip, type, noGrip = false) {
	  if (noGrip === true || !isGrip(grip)) {
	    return type == "function";
	  }

	  return type == "Function";
	}

	// Exports from this module

	module.exports = {
	  rep: wrapRender(FunctionRep),
	  supportsObject
	};

/***/ },
/* 27 */
/***/ function(module, exports, __webpack_require__) {

	/* This Source Code Form is subject to the terms of the Mozilla Public
	 * License, v. 2.0. If a copy of the MPL was not distributed with this
	 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

	// ReactJS
	const React = __webpack_require__(8);
	// Dependencies
	const {
	  isGrip,
	  wrapRender
	} = __webpack_require__(7);

	const PropRep = __webpack_require__(17);
	const { MODE } = __webpack_require__(1);
	// Shortcuts
	const { span } = React.DOM;

	/**
	 * Renders a DOM Promise object.
	 */
	PromiseRep.propTypes = {
	  object: React.PropTypes.object.isRequired,
	  // @TODO Change this to Object.values once it's supported in Node's version of V8
	  mode: React.PropTypes.oneOf(Object.keys(MODE).map(key => MODE[key])),
	  onDOMNodeMouseOver: React.PropTypes.func,
	  onDOMNodeMouseOut: React.PropTypes.func,
	  onInspectIconClick: React.PropTypes.func
	};

	function PromiseRep(props) {
	  const object = props.object;
	  const { promiseState } = object;

	  const config = {
	    "data-link-actor-id": object.actor,
	    className: "objectBox objectBox-object"
	  };

	  if (props.mode === MODE.TINY) {
	    let { Rep } = __webpack_require__(2);

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
	function supportsObject(object, type, noGrip = false) {
	  if (noGrip === true || !isGrip(object)) {
	    return false;
	  }
	  return type === "Promise";
	}

	// Exports from this module
	module.exports = {
	  rep: wrapRender(PromiseRep),
	  supportsObject
	};

/***/ },
/* 28 */
/***/ function(module, exports, __webpack_require__) {

	/* This Source Code Form is subject to the terms of the Mozilla Public
	 * License, v. 2.0. If a copy of the MPL was not distributed with this
	 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

	// ReactJS
	const React = __webpack_require__(8);

	// Reps
	const {
	  isGrip,
	  wrapRender
	} = __webpack_require__(7);

	/**
	 * Renders a grip object with regular expression.
	 */
	RegExp.propTypes = {
	  object: React.PropTypes.object.isRequired
	};

	function RegExp(props) {
	  let { object } = props;

	  return React.DOM.span({
	    "data-link-actor-id": object.actor,
	    className: "objectBox objectBox-regexp regexpSource"
	  }, getSource(object));
	}

	function getSource(grip) {
	  return grip.displayString;
	}

	// Registration
	function supportsObject(object, type, noGrip = false) {
	  if (noGrip === true || !isGrip(object)) {
	    return false;
	  }

	  return type == "RegExp";
	}

	// Exports from this module
	module.exports = {
	  rep: wrapRender(RegExp),
	  supportsObject
	};

/***/ },
/* 29 */
/***/ function(module, exports, __webpack_require__) {

	/* This Source Code Form is subject to the terms of the Mozilla Public
	 * License, v. 2.0. If a copy of the MPL was not distributed with this
	 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

	// ReactJS
	const React = __webpack_require__(8);

	// Reps
	const {
	  isGrip,
	  getURLDisplayString,
	  wrapRender
	} = __webpack_require__(7);

	// Shortcuts
	const { span } = React.DOM;

	/**
	 * Renders a grip representing CSSStyleSheet
	 */
	StyleSheet.propTypes = {
	  object: React.PropTypes.object.isRequired
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
	function supportsObject(object, type, noGrip = false) {
	  if (noGrip === true || !isGrip(object)) {
	    return false;
	  }

	  return type == "CSSStyleSheet";
	}

	// Exports from this module

	module.exports = {
	  rep: wrapRender(StyleSheet),
	  supportsObject
	};

/***/ },
/* 30 */
/***/ function(module, exports, __webpack_require__) {

	/* This Source Code Form is subject to the terms of the Mozilla Public
	 * License, v. 2.0. If a copy of the MPL was not distributed with this
	 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

	// Dependencies
	const React = __webpack_require__(8);
	const {
	  isGrip,
	  cropString,
	  cropMultipleLines,
	  wrapRender
	} = __webpack_require__(7);
	const { MODE } = __webpack_require__(1);
	const nodeConstants = __webpack_require__(31);

	// Shortcuts
	const { span } = React.DOM;

	/**
	 * Renders DOM comment node.
	 */
	CommentNode.propTypes = {
	  object: React.PropTypes.object.isRequired,
	  // @TODO Change this to Object.values once it's supported in Node's version of V8
	  mode: React.PropTypes.oneOf(Object.keys(MODE).map(key => MODE[key]))
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
	function supportsObject(object, type, noGrip = false) {
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

/***/ },
/* 31 */
/***/ function(module, exports) {

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

/***/ },
/* 32 */
/***/ function(module, exports, __webpack_require__) {

	/* This Source Code Form is subject to the terms of the Mozilla Public
	 * License, v. 2.0. If a copy of the MPL was not distributed with this
	 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

	// ReactJS
	const React = __webpack_require__(8);

	// Utils
	const {
	  isGrip,
	  wrapRender
	} = __webpack_require__(7);
	const { MODE } = __webpack_require__(1);
	const nodeConstants = __webpack_require__(31);
	const Svg = __webpack_require__(33);

	// Shortcuts
	const { span } = React.DOM;

	/**
	 * Renders DOM element node.
	 */
	ElementNode.propTypes = {
	  object: React.PropTypes.object.isRequired,
	  // @TODO Change this to Object.values once it's supported in Node's version of V8
	  mode: React.PropTypes.oneOf(Object.keys(MODE).map(key => MODE[key])),
	  onDOMNodeMouseOver: React.PropTypes.func,
	  onDOMNodeMouseOut: React.PropTypes.func,
	  onInspectIconClick: React.PropTypes.func
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
	    className: "tag-name theme-fg-color3"
	  }, nodeName);

	  if (mode === MODE.TINY) {
	    let elements = [nodeNameElement];
	    if (attributes.id) {
	      elements.push(span({ className: "attr-name theme-fg-color2" }, `#${attributes.id}`));
	    }
	    if (attributes.class) {
	      elements.push(span({ className: "attr-name theme-fg-color2" }, attributes.class.replace(/(^\s+)|(\s+$)/g, "").split(" ").map(cls => `.${cls}`).join("")));
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
	    let attribute = span({}, span({ className: "attr-name theme-fg-color2" }, `${name}`), `="`, span({ className: "attr-value theme-fg-color6" }, `${value}`), `"`);

	    return arr.concat([" ", attribute]);
	  }, []);

	  return ["<", nodeNameElement, ...attributeElements, ">"];
	}

	// Registration
	function supportsObject(object, type, noGrip = false) {
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

/***/ },
/* 33 */
/***/ function(module, exports, __webpack_require__) {

	/* This Source Code Form is subject to the terms of the Mozilla Public
	 * License, v. 2.0. If a copy of the MPL was not distributed with this
	 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

	const React = __webpack_require__(8);
	const InlineSVG = __webpack_require__(34);

	const svg = {
	  "arrow": __webpack_require__(35),
	  "open-inspector": __webpack_require__(36)
	};

	Svg.propTypes = {
	  className: React.PropTypes.string
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
	  return React.createElement(InlineSVG, props);
	}

	module.exports = Svg;

/***/ },
/* 34 */
/***/ function(module, exports, __webpack_require__) {

	'use strict';

	Object.defineProperty(exports, '__esModule', {
	    value: true
	});

	var _extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; };

	var _createClass = (function () { function defineProperties(target, props) { for (var i = 0; i < props.length; i++) { var descriptor = props[i]; descriptor.enumerable = descriptor.enumerable || false; descriptor.configurable = true; if ('value' in descriptor) descriptor.writable = true; Object.defineProperty(target, descriptor.key, descriptor); } } return function (Constructor, protoProps, staticProps) { if (protoProps) defineProperties(Constructor.prototype, protoProps); if (staticProps) defineProperties(Constructor, staticProps); return Constructor; }; })();

	var _get = function get(_x, _x2, _x3) { var _again = true; _function: while (_again) { var object = _x, property = _x2, receiver = _x3; _again = false; if (object === null) object = Function.prototype; var desc = Object.getOwnPropertyDescriptor(object, property); if (desc === undefined) { var parent = Object.getPrototypeOf(object); if (parent === null) { return undefined; } else { _x = parent; _x2 = property; _x3 = receiver; _again = true; desc = parent = undefined; continue _function; } } else if ('value' in desc) { return desc.value; } else { var getter = desc.get; if (getter === undefined) { return undefined; } return getter.call(receiver); } } };

	function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { 'default': obj }; }

	function _objectWithoutProperties(obj, keys) { var target = {}; for (var i in obj) { if (keys.indexOf(i) >= 0) continue; if (!Object.prototype.hasOwnProperty.call(obj, i)) continue; target[i] = obj[i]; } return target; }

	function _classCallCheck(instance, Constructor) { if (!(instance instanceof Constructor)) { throw new TypeError('Cannot call a class as a function'); } }

	function _inherits(subClass, superClass) { if (typeof superClass !== 'function' && superClass !== null) { throw new TypeError('Super expression must either be null or a function, not ' + typeof superClass); } subClass.prototype = Object.create(superClass && superClass.prototype, { constructor: { value: subClass, enumerable: false, writable: true, configurable: true } }); if (superClass) Object.setPrototypeOf ? Object.setPrototypeOf(subClass, superClass) : subClass.__proto__ = superClass; }

	var _react = __webpack_require__(8);

	var _react2 = _interopRequireDefault(_react);

	var DOMParser = typeof window !== 'undefined' && window.DOMParser;
	var process = process || {};
	process.env = process.env || {};
	var parserAvailable = typeof DOMParser !== 'undefined' && DOMParser.prototype != null && DOMParser.prototype.parseFromString != null;

	function isParsable(src) {
	    // kinda naive but meh, ain't gonna use full-blown parser for this
	    return parserAvailable && typeof src === 'string' && src.trim().substr(0, 4) === '<svg';
	}

	// parse SVG string using `DOMParser`
	function parseFromSVGString(src) {
	    var parser = new DOMParser();
	    return parser.parseFromString(src, "image/svg+xml");
	}

	// Transform DOM prop/attr names applicable to `<svg>` element but react-limited
	function switchSVGAttrToReactProp(propName) {
	    switch (propName) {
	        case 'class':
	            return 'className';
	        default:
	            return propName;
	    }
	}

	var InlineSVG = (function (_React$Component) {
	    _inherits(InlineSVG, _React$Component);

	    _createClass(InlineSVG, null, [{
	        key: 'defaultProps',
	        value: {
	            element: 'i',
	            raw: false,
	            src: ''
	        },
	        enumerable: true
	    }, {
	        key: 'propTypes',
	        value: {
	            src: _react2['default'].PropTypes.string.isRequired,
	            element: _react2['default'].PropTypes.string,
	            raw: _react2['default'].PropTypes.bool
	        },
	        enumerable: true
	    }]);

	    function InlineSVG(props) {
	        _classCallCheck(this, InlineSVG);

	        _get(Object.getPrototypeOf(InlineSVG.prototype), 'constructor', this).call(this, props);
	        this._extractSVGProps = this._extractSVGProps.bind(this);
	    }

	    // Serialize `Attr` objects in `NamedNodeMap`

	    _createClass(InlineSVG, [{
	        key: '_serializeAttrs',
	        value: function _serializeAttrs(map) {
	            var ret = {};
	            var prop = undefined;
	            for (var i = 0; i < map.length; i++) {
	                prop = switchSVGAttrToReactProp(map[i].name);
	                ret[prop] = map[i].value;
	            }
	            return ret;
	        }

	        // get <svg /> element props
	    }, {
	        key: '_extractSVGProps',
	        value: function _extractSVGProps(src) {
	            var map = parseFromSVGString(src).documentElement.attributes;
	            return map.length > 0 ? this._serializeAttrs(map) : null;
	        }

	        // get content inside <svg> element.
	    }, {
	        key: '_stripSVG',
	        value: function _stripSVG(src) {
	            return parseFromSVGString(src).documentElement.innerHTML;
	        }
	    }, {
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
	            var Element = undefined,
	                __html = undefined,
	                svgProps = undefined;
	            var _props = this.props;
	            var element = _props.element;
	            var raw = _props.raw;
	            var src = _props.src;

	            var otherProps = _objectWithoutProperties(_props, ['element', 'raw', 'src']);

	            if (raw === true && isParsable(src)) {
	                Element = 'svg';
	                svgProps = this._extractSVGProps(src);
	                __html = this._stripSVG(src);
	            }
	            __html = __html || src;
	            Element = Element || element;
	            svgProps = svgProps || {};

	            return _react2['default'].createElement(Element, _extends({}, svgProps, otherProps, { src: null, children: null,
	                dangerouslySetInnerHTML: { __html: __html } }));
	        }
	    }]);

	    return InlineSVG;
	})(_react2['default'].Component);

	exports['default'] = InlineSVG;
	module.exports = exports['default'];

/***/ },
/* 35 */
/***/ function(module, exports) {

	module.exports = "<!-- This Source Code Form is subject to the terms of the Mozilla Public - License, v. 2.0. If a copy of the MPL was not distributed with this - file, You can obtain one at http://mozilla.org/MPL/2.0/. --><svg viewBox=\"0 0 16 16\" xmlns=\"http://www.w3.org/2000/svg\"><path d=\"M8 13.4c-.5 0-.9-.2-1.2-.6L.4 5.2C0 4.7-.1 4.3.2 3.7S1 3 1.6 3h12.8c.6 0 1.2.1 1.4.7.3.6.2 1.1-.2 1.6l-6.4 7.6c-.3.4-.7.5-1.2.5z\"></path></svg>"

/***/ },
/* 36 */
/***/ function(module, exports) {

	module.exports = "<!-- This Source Code Form is subject to the terms of the Mozilla Public - License, v. 2.0. If a copy of the MPL was not distributed with this - file, You can obtain one at http://mozilla.org/MPL/2.0/. --><svg viewBox=\"0 0 16 16\" xmlns=\"http://www.w3.org/2000/svg\"><path d=\"M8,3L12,3L12,7L14,7L14,8L12,8L12,12L8,12L8,14L7,14L7,12L3,12L3,8L1,8L1,7L3,7L3,3L7,3L7,1L8,1L8,3ZM10,10L10,5L5,5L5,10L10,10Z\"></path></svg>"

/***/ },
/* 37 */
/***/ function(module, exports, __webpack_require__) {

	/* This Source Code Form is subject to the terms of the Mozilla Public
	 * License, v. 2.0. If a copy of the MPL was not distributed with this
	 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

	// ReactJS
	const React = __webpack_require__(8);

	// Reps
	const {
	  isGrip,
	  cropString,
	  wrapRender
	} = __webpack_require__(7);
	const { MODE } = __webpack_require__(1);
	const Svg = __webpack_require__(33);

	// Shortcuts
	const DOM = React.DOM;

	/**
	 * Renders DOM #text node.
	 */
	TextNode.propTypes = {
	  object: React.PropTypes.object.isRequired,
	  // @TODO Change this to Object.values once it's supported in Node's version of V8
	  mode: React.PropTypes.oneOf(Object.keys(MODE).map(key => MODE[key])),
	  onDOMNodeMouseOver: React.PropTypes.func,
	  onDOMNodeMouseOut: React.PropTypes.func,
	  onInspectIconClick: React.PropTypes.func
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
	    return DOM.span(baseConfig, getTitle(grip), inspectIcon);
	  }

	  return DOM.span(baseConfig, getTitle(grip), DOM.span({ className: "nodeValue" }, " ", `"${getTextContent(grip)}"`), inspectIcon);
	}

	function getTextContent(grip) {
	  return cropString(grip.preview.textContent);
	}

	function getTitle(grip) {
	  const title = "#text";
	  return DOM.span({}, title);
	}

	// Registration
	function supportsObject(grip, type, noGrip = false) {
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

/***/ },
/* 38 */
/***/ function(module, exports, __webpack_require__) {

	/* This Source Code Form is subject to the terms of the Mozilla Public
	 * License, v. 2.0. If a copy of the MPL was not distributed with this
	 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

	// ReactJS
	const React = __webpack_require__(8);
	// Utils
	const {
	  isGrip,
	  wrapRender
	} = __webpack_require__(7);
	const { MODE } = __webpack_require__(1);

	// Shortcuts
	const { span } = React.DOM;

	/**
	 * Renders Error objects.
	 */
	ErrorRep.propTypes = {
	  object: React.PropTypes.object.isRequired,
	  // @TODO Change this to Object.values once it's supported in Node's version of V8
	  mode: React.PropTypes.oneOf(Object.keys(MODE).map(key => MODE[key]))
	};

	function ErrorRep(props) {
	  let object = props.object;
	  let preview = object.preview;
	  let name = preview && preview.name ? preview.name : "Error";

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
	function supportsObject(object, type, noGrip = false) {
	  if (noGrip === true || !isGrip(object)) {
	    return false;
	  }
	  return object.preview && type === "Error";
	}

	// Exports from this module
	module.exports = {
	  rep: wrapRender(ErrorRep),
	  supportsObject
	};

/***/ },
/* 39 */
/***/ function(module, exports, __webpack_require__) {

	/* This Source Code Form is subject to the terms of the Mozilla Public
	 * License, v. 2.0. If a copy of the MPL was not distributed with this
	 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

	// ReactJS
	const React = __webpack_require__(8);

	// Reps
	const {
	  isGrip,
	  getURLDisplayString,
	  wrapRender
	} = __webpack_require__(7);

	const { MODE } = __webpack_require__(1);

	// Shortcuts
	const { span } = React.DOM;

	/**
	 * Renders a grip representing a window.
	 */
	WindowRep.propTypes = {
	  // @TODO Change this to Object.values once it's supported in Node's version of V8
	  mode: React.PropTypes.oneOf(Object.keys(MODE).map(key => MODE[key])),
	  object: React.PropTypes.object.isRequired
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

	  return span(config, getTitle(object), " ", span({ className: "objectPropValue" }, getLocation(object)));
	}

	function getTitle(object) {
	  let title = object.displayClass || object.class || "Window";
	  return span({ className: "objectBoxTitle" }, title);
	}

	function getLocation(object) {
	  return getURLDisplayString(object.preview.url);
	}

	// Registration
	function supportsObject(object, type, noGrip = false) {
	  if (noGrip === true || !isGrip(object)) {
	    return false;
	  }

	  return object.preview && type == "Window";
	}

	// Exports from this module
	module.exports = {
	  rep: wrapRender(WindowRep),
	  supportsObject
	};

/***/ },
/* 40 */
/***/ function(module, exports, __webpack_require__) {

	/* This Source Code Form is subject to the terms of the Mozilla Public
	 * License, v. 2.0. If a copy of the MPL was not distributed with this
	 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

	// ReactJS
	const React = __webpack_require__(8);

	// Reps
	const {
	  isGrip,
	  wrapRender
	} = __webpack_require__(7);

	// Shortcuts
	const { span } = React.DOM;

	/**
	 * Renders a grip object with textual data.
	 */
	ObjectWithText.propTypes = {
	  object: React.PropTypes.object.isRequired
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
	function supportsObject(grip, type, noGrip = false) {
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

/***/ },
/* 41 */
/***/ function(module, exports, __webpack_require__) {

	/* This Source Code Form is subject to the terms of the Mozilla Public
	 * License, v. 2.0. If a copy of the MPL was not distributed with this
	 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

	// ReactJS
	const React = __webpack_require__(8);

	// Reps
	const {
	  isGrip,
	  getURLDisplayString,
	  wrapRender
	} = __webpack_require__(7);

	// Shortcuts
	const { span } = React.DOM;

	/**
	 * Renders a grip object with URL data.
	 */
	ObjectWithURL.propTypes = {
	  object: React.PropTypes.object.isRequired
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
	function supportsObject(grip, type, noGrip = false) {
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

/***/ },
/* 42 */
/***/ function(module, exports, __webpack_require__) {

	/* This Source Code Form is subject to the terms of the Mozilla Public
	 * License, v. 2.0. If a copy of the MPL was not distributed with this
	 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

	// Dependencies
	const React = __webpack_require__(8);
	const {
	  isGrip,
	  wrapRender
	} = __webpack_require__(7);
	const Caption = __webpack_require__(15);
	const { MODE } = __webpack_require__(1);

	// Shortcuts
	const { span } = React.DOM;

	/**
	 * Renders an array. The array is enclosed by left and right bracket
	 * and the max number of rendered items depends on the current mode.
	 */
	GripArray.propTypes = {
	  object: React.PropTypes.object.isRequired,
	  // @TODO Change this to Object.values once it's supported in Node's version of V8
	  mode: React.PropTypes.oneOf(Object.keys(MODE).map(key => MODE[key])),
	  provider: React.PropTypes.object,
	  onDOMNodeMouseOver: React.PropTypes.func,
	  onDOMNodeMouseOut: React.PropTypes.func,
	  onInspectIconClick: React.PropTypes.func
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
	    items = [span({
	      className: "length"
	    }, isEmpty ? "" : objectLength)];
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

	  return grip.preview.items || grip.preview.childNodes || null;
	}

	function arrayIterator(props, grip, max) {
	  let { Rep } = __webpack_require__(2);

	  let items = [];
	  const gripLength = getLength(grip);

	  if (!gripLength) {
	    return items;
	  }

	  const previewItems = getPreviewItems(grip);
	  if (!previewItems) {
	    return items;
	  }

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
	    items.push(Caption({
	      object: span({}, "more…")
	    }));
	  }

	  return items;
	}

	function getEmptySlotsElement(number) {
	  // TODO: Use l10N - See https://github.com/devtools-html/reps/issues/141
	  return `<${number} empty slot${number > 1 ? "s" : ""}>`;
	}

	function supportsObject(grip, type, noGrip = false) {
	  if (noGrip === true || !isGrip(grip)) {
	    return false;
	  }

	  return grip.preview && (grip.preview.kind == "ArrayLike" || type === "DocumentFragment");
	}

	const maxLengthMap = new Map();
	maxLengthMap.set(MODE.SHORT, 3);
	maxLengthMap.set(MODE.LONG, 10);

	// Exports from this module
	module.exports = {
	  rep: wrapRender(GripArray),
	  supportsObject,
	  maxLengthMap
	};

/***/ },
/* 43 */
/***/ function(module, exports, __webpack_require__) {

	/* This Source Code Form is subject to the terms of the Mozilla Public
	 * License, v. 2.0. If a copy of the MPL was not distributed with this
	 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

	// Dependencies
	const React = __webpack_require__(8);
	const {
	  isGrip,
	  wrapRender
	} = __webpack_require__(7);
	const Caption = __webpack_require__(15);
	const PropRep = __webpack_require__(17);
	const { MODE } = __webpack_require__(1);
	// Shortcuts
	const { span } = React.DOM;

	/**
	 * Renders an map. A map is represented by a list of its
	 * entries enclosed in curly brackets.
	 */
	GripMap.propTypes = {
	  object: React.PropTypes.object,
	  // @TODO Change this to Object.values once it's supported in Node's version of V8
	  mode: React.PropTypes.oneOf(Object.keys(MODE).map(key => MODE[key])),
	  isInterestingEntry: React.PropTypes.func,
	  onDOMNodeMouseOver: React.PropTypes.func,
	  onDOMNodeMouseOut: React.PropTypes.func,
	  onInspectIconClick: React.PropTypes.func,
	  title: React.PropTypes.string
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
	  if (entries.length < mapEntries.length) {
	    // There are some undisplayed entries. Then display "more…".
	    entries.push(Caption({
	      key: "more",
	      object: span({}, "more…")
	    }));
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
	      equal: ": ",
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

	function supportsObject(grip, type, noGrip = false) {
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
	  maxLengthMap
	};

/***/ },
/* 44 */
/***/ function(module, exports, __webpack_require__) {

	/* This Source Code Form is subject to the terms of the Mozilla Public
	 * License, v. 2.0. If a copy of the MPL was not distributed with this
	 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

	const {
	  Component,
	  createFactory,
	  DOM: dom,
	  PropTypes
	} = __webpack_require__(8);

	const Tree = createFactory(__webpack_require__(45).Tree);
	__webpack_require__(47);

	const classnames = __webpack_require__(49);
	const Svg = __webpack_require__(33);
	const {
	  REPS: { Rep }
	} = __webpack_require__(2);
	const {
	  MODE
	} = __webpack_require__(1);

	const {
	  getChildren,
	  getValue,
	  isDefault,
	  nodeHasProperties,
	  nodeIsMissingArguments,
	  nodeIsOptimizedOut,
	  nodeIsPrimitive
	} = __webpack_require__(50);

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
	  constructor() {
	    super();

	    this.actors = {};
	    this.state = {
	      expandedKeys: new Set(),
	      focusedItem: null
	    };

	    const self = this;

	    self.getChildren = this.getChildren.bind(this);
	    self.renderTreeItem = this.renderTreeItem.bind(this);
	    self.setExpanded = this.setExpanded.bind(this);
	    self.focusItem = this.focusItem.bind(this);
	    self.getRoots = this.getRoots.bind(this);
	  }

	  isDefaultProperty(item) {
	    const roots = this.props.roots;
	    return isDefault(item, roots);
	  }

	  getParent(item) {
	    return null;
	  }

	  getChildren(item) {
	    const { getObjectProperties } = this.props;
	    const { actors } = this;

	    return getChildren({
	      getObjectProperties,
	      actors,
	      item
	    });
	  }

	  getRoots() {
	    return this.props.roots;
	  }

	  getKey(item) {
	    return item.path;
	  }

	  setExpanded(item, expand) {
	    const { expandedKeys } = this.state;
	    const key = this.getKey(item);

	    if (expand === true) {
	      expandedKeys.add(key);
	    } else {
	      expandedKeys.delete(key);
	    }

	    this.setState({ expandedKeys });

	    if (expand === true) {
	      const {
	        getObjectProperties,
	        loadObjectProperties
	      } = this.props;

	      const value = getValue(item);
	      if (nodeHasProperties(item) && value && !getObjectProperties(value.actor)) {
	        loadObjectProperties(value);
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

	    const unavailable = nodeIsPrimitive(item) && itemValue && itemValue.hasOwnProperty && itemValue.hasOwnProperty("unavailable");

	    if (nodeIsOptimizedOut(item)) {
	      objectValue = dom.span({ className: "unavailable" }, "(optimized away)");
	    } else if (nodeIsMissingArguments(item) || unavailable) {
	      objectValue = dom.span({ className: "unavailable" }, "(unavailable)");
	    } else if (nodeHasProperties(item) || nodeIsPrimitive(item)) {
	      let mode;
	      if (depth === 0) {
	        mode = this.props.mode;
	      } else {
	        mode = this.props.mode === MODE.LONG ? MODE.SHORT : MODE.TINY;
	      }

	      objectValue = this.renderGrip(item, this.props, mode);
	    }

	    const SINGLE_INDENT_WIDTH = 15;
	    const indentWidth = (depth + (nodeIsPrimitive(item) ? 1 : 0)) * SINGLE_INDENT_WIDTH;

	    const {
	      onDoubleClick,
	      onLabelClick
	    } = this.props;

	    return dom.div({
	      className: classnames("node object-node", {
	        focused,
	        "default-property": this.isDefaultProperty(item)
	      }),
	      style: {
	        marginLeft: indentWidth
	      },
	      onClick: e => {
	        e.stopPropagation();
	        this.setExpanded(item, !expanded);
	      },
	      onDoubleClick: onDoubleClick ? e => {
	        e.stopPropagation();
	        onDoubleClick(item, {
	          depth,
	          focused,
	          expanded
	        });
	      } : null
	    }, nodeIsPrimitive(item) === false ? Svg("arrow", {
	      className: classnames({
	        expanded: expanded
	      })
	    }) : null, label ? dom.span({
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
	    }, label) : null, label && objectValue ? dom.span({ className: "object-delimiter" }, " : ") : null, objectValue);
	  }

	  renderGrip(item, props, mode = MODE.TINY) {
	    const object = getValue(item);
	    return Rep(Object.assign({}, props, {
	      object,
	      mode
	    }));
	  }

	  render() {
	    const {
	      autoExpandDepth = 1,
	      autoExpandAll = true,
	      disabledFocus,
	      itemHeight = 20,
	      disableWrap = false
	    } = this.props;

	    const {
	      expandedKeys,
	      focusedItem
	    } = this.state;

	    let roots = this.getRoots();
	    if (roots.length === 1 && nodeIsPrimitive(roots[0])) {
	      return this.renderGrip(roots[0], this.props, this.props.mode);
	    }

	    return Tree({
	      className: disableWrap ? "nowrap" : "",
	      autoExpandAll,
	      autoExpandDepth,
	      disabledFocus,
	      itemHeight,

	      isExpanded: item => expandedKeys.has(this.getKey(item)),
	      focused: focusedItem,

	      getRoots: this.getRoots,
	      getParent: this.getParent,
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
	  roots: PropTypes.array,
	  getObjectProperties: PropTypes.func.isRequired,
	  loadObjectProperties: PropTypes.func.isRequired,
	  itemHeight: PropTypes.number,
	  mode: PropTypes.oneOf(Object.values(MODE)),
	  onFocus: PropTypes.func,
	  onDoubleClick: PropTypes.func,
	  onLabelClick: PropTypes.func
	};

	module.exports = ObjectInspector;

/***/ },
/* 45 */
/***/ function(module, exports, __webpack_require__) {

	const Tree = __webpack_require__(46);

	module.exports = {
	  Tree
	};

/***/ },
/* 46 */
/***/ function(module, exports, __webpack_require__) {

	const { DOM: dom, createClass, createFactory, PropTypes } = __webpack_require__(8);

	const AUTO_EXPAND_DEPTH = 0; // depth

	/**
	 * An arrow that displays whether its node is expanded (▼) or collapsed
	 * (▶). When its node has no children, it is hidden.
	 */
	const ArrowExpander = createFactory(createClass({
	  displayName: "ArrowExpander",

	  shouldComponentUpdate(nextProps, nextState) {
	    return this.props.item !== nextProps.item || this.props.visible !== nextProps.visible || this.props.expanded !== nextProps.expanded;
	  },

	  render() {
	    const attrs = {
	      className: "arrow theme-twisty",
	      onClick: this.props.expanded ? () => this.props.onCollapse(this.props.item) : e => this.props.onExpand(this.props.item, e.altKey)
	    };

	    if (this.props.expanded) {
	      attrs.className += " open";
	    }

	    if (!this.props.visible) {
	      attrs.style = Object.assign({}, this.props.style || {}, {
	        visibility: "hidden"
	      });
	    }

	    return dom.div(attrs, this.props.children);
	  }
	}));

	const TreeNode = createFactory(createClass({
	  displayName: "TreeNode",

	  componentDidMount() {
	    if (this.props.focused) {
	      this.refs.button.focus();
	    }
	  },

	  componentDidUpdate() {
	    if (this.props.focused) {
	      this.refs.button.focus();
	    }
	  },

	  shouldComponentUpdate(nextProps) {
	    return this.props.item !== nextProps.item || this.props.focused !== nextProps.focused || this.props.expanded !== nextProps.expanded;
	  },

	  render() {
	    const arrow = ArrowExpander({
	      item: this.props.item,
	      expanded: this.props.expanded,
	      visible: this.props.hasChildren,
	      onExpand: this.props.onExpand,
	      onCollapse: this.props.onCollapse
	    });

	    let isOddRow = this.props.index % 2;
	    return dom.div({
	      className: `tree-node div ${isOddRow ? "tree-node-odd" : ""}`,
	      onFocus: this.props.onFocus,
	      onClick: this.props.onFocus,
	      onBlur: this.props.onBlur,
	      style: {
	        padding: 0,
	        margin: 0
	      }
	    }, this.props.renderItem(this.props.item, this.props.depth, this.props.focused, arrow, this.props.expanded),

	    // XXX: OSX won't focus/blur regular elements even if you set tabindex
	    // unless there is an input/button child.
	    dom.button(this._buttonAttrs));
	  },

	  _buttonAttrs: {
	    ref: "button",
	    style: {
	      opacity: 0,
	      width: "0 !important",
	      height: "0 !important",
	      padding: "0 !important",
	      outline: "none",
	      MozAppearance: "none",
	      // XXX: Despite resetting all of the above properties (and margin), the
	      // button still ends up with ~79px width, so we set a large negative
	      // margin to completely hide it.
	      MozMarginStart: "-1000px !important"
	    }
	  }
	}));

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

	const NUMBER_OF_OFFSCREEN_ITEMS = 1;

	/**
	 * A generic tree component. See propTypes for the public API.
	 *
	 * @see `devtools/client/memory/components/test/mochitest/head.js` for usage
	 * @see `devtools/client/memory/components/heap.js` for usage
	 */
	const Tree = module.exports = createClass({
	  displayName: "Tree",

	  propTypes: {
	    // Required props

	    // A function to get an item's parent, or null if it is a root.
	    getParent: PropTypes.func.isRequired,
	    // A function to get an item's children.
	    getChildren: PropTypes.func.isRequired,
	    // A function which takes an item and ArrowExpander and returns a
	    // component.
	    renderItem: PropTypes.func.isRequired,
	    // A function which returns the roots of the tree (forest).
	    getRoots: PropTypes.func.isRequired,
	    // A function to get a unique key for the given item.
	    getKey: PropTypes.func.isRequired,
	    // A function to get whether an item is expanded or not. If an item is not
	    // expanded, then it must be collapsed.
	    isExpanded: PropTypes.func.isRequired,
	    // The height of an item in the tree including margin and padding, in
	    // pixels.
	    itemHeight: PropTypes.number.isRequired,

	    // Optional props

	    // The currently focused item, if any such item exists.
	    focused: PropTypes.any,
	    // Handle when a new item is focused.
	    onFocus: PropTypes.func,
	    // The depth to which we should automatically expand new items.
	    autoExpandDepth: PropTypes.number,
	    // Should auto expand all new items or just the new items under the first
	    // root item.
	    autoExpandAll: PropTypes.bool,
	    // Optional event handlers for when items are expanded or collapsed.
	    onExpand: PropTypes.func,
	    onCollapse: PropTypes.func
	  },

	  getDefaultProps() {
	    return {
	      autoExpandDepth: AUTO_EXPAND_DEPTH,
	      autoExpandAll: true
	    };
	  },

	  getInitialState() {
	    return {
	      scroll: 0,
	      height: window.innerHeight,
	      seen: new Set()
	    };
	  },

	  componentDidMount() {
	    window.addEventListener("resize", this._updateHeight);
	    this._autoExpand(this.props);
	    this._updateHeight();
	  },

	  componentWillUnmount() {
	    window.removeEventListener("resize", this._updateHeight);
	  },

	  componentWillReceiveProps(nextProps) {
	    this._autoExpand(nextProps);
	    this._updateHeight();
	  },

	  _autoExpand(props) {
	    if (!props.autoExpandDepth) {
	      return;
	    }

	    // Automatically expand the first autoExpandDepth levels for new items. Do
	    // not use the usual DFS infrastructure because we don't want to ignore
	    // collapsed nodes.
	    const autoExpand = (item, currentDepth) => {
	      if (currentDepth >= props.autoExpandDepth || this.state.seen.has(item)) {
	        return;
	      }

	      props.onExpand(item);
	      this.state.seen.add(item);

	      const children = props.getChildren(item);
	      const length = children.length;
	      for (let i = 0; i < length; i++) {
	        autoExpand(children[i], currentDepth + 1);
	      }
	    };

	    const roots = props.getRoots();
	    const length = roots.length;
	    if (props.autoExpandAll) {
	      for (let i = 0; i < length; i++) {
	        autoExpand(roots[i], 0);
	      }
	    } else if (length != 0) {
	      autoExpand(roots[0], 0);
	    }
	  },

	  render() {
	    const traversal = this._dfsFromRoots();

	    const renderItem = i => {
	      let { item, depth } = traversal[i];
	      return TreeNode({
	        key: this.props.getKey(item, i),
	        index: i,
	        item: item,
	        depth: depth,
	        renderItem: this.props.renderItem,
	        focused: this.props.focused === item,
	        expanded: this.props.isExpanded(item),
	        hasChildren: !!this.props.getChildren(item).length,
	        onExpand: this._onExpand,
	        onCollapse: this._onCollapse,
	        onFocus: () => this._focus(i, item)
	      });
	    };

	    const style = Object.assign({}, this.props.style || {}, {
	      padding: 0,
	      margin: 0
	    });

	    return dom.div({
	      className: "tree",
	      ref: "tree",
	      onKeyDown: this._onKeyDown,
	      onKeyPress: this._preventArrowKeyScrolling,
	      onKeyUp: this._preventArrowKeyScrolling,
	      onScroll: this._onScroll,
	      style
	    }, traversal.map((v, i) => renderItem(i)));
	  },

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
	  },

	  /**
	   * Updates the state's height based on clientHeight.
	   */
	  _updateHeight() {
	    this.setState({
	      height: this.refs.tree.clientHeight
	    });
	  },

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
	  },

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
	  },

	  /**
	   * Expands current row.
	   *
	   * @param {Object} item
	   * @param {Boolean} expandAllChildren
	   */
	  _onExpand: oncePerAnimationFrame(function (item, expandAllChildren) {
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
	  }),

	  /**
	   * Collapses current row.
	   *
	   * @param {Object} item
	   */
	  _onCollapse: oncePerAnimationFrame(function (item) {
	    if (this.props.onCollapse) {
	      this.props.onCollapse(item);
	    }
	  }),

	  /**
	   * Sets the passed in item to be the focused item.
	   *
	   * @param {Number} index
	   *        The index of the item in a full DFS traversal (ignoring collapsed
	   *        nodes). Ignored if `item` is undefined.
	   *
	   * @param {Object|undefined} item
	   *        The item to be focused, or undefined to focus no item.
	   */
	  _focus(index, item) {
	    if (item !== undefined) {
	      const itemStartPosition = index * this.props.itemHeight;
	      const itemEndPosition = (index + 1) * this.props.itemHeight;

	      // Note that if the height of the viewport (this.state.height) is less than
	      // `this.props.itemHeight`, we could accidentally try and scroll both up and
	      // down in a futile attempt to make both the item's start and end positions
	      // visible. Instead, give priority to the start of the item by checking its
	      // position first, and then using an "else if", rather than a separate "if",
	      // for the end position.
	      if (this.state.scroll > itemStartPosition) {
	        this.refs.tree.scrollTop = itemStartPosition;
	      } else if (this.state.scroll + this.state.height < itemEndPosition) {
	        this.refs.tree.scrollTop = itemEndPosition - this.state.height;
	      }
	    }

	    if (this.props.onFocus) {
	      this.props.onFocus(item);
	    }
	  },

	  /**
	   * Sets the state to have no focused item.
	   */
	  _onBlur() {
	    this._focus(0, undefined);
	  },

	  /**
	   * Fired on a scroll within the tree's container, updates
	   * the stored position of the view port to handle virtual view rendering.
	   *
	   * @param {Event} e
	   */
	  _onScroll: oncePerAnimationFrame(function (e) {
	    this.setState({
	      scroll: Math.max(this.refs.tree.scrollTop, 0),
	      height: this.refs.tree.clientHeight
	    });
	  }),

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
	        if (this.props.isExpanded(this.props.focused) && this.props.getChildren(this.props.focused).length) {
	          this._onCollapse(this.props.focused);
	        } else {
	          this._focusParentNode();
	        }
	        return;

	      case "ArrowRight":
	        if (!this.props.isExpanded(this.props.focused)) {
	          this._onExpand(this.props.focused);
	        }
	        return;
	    }
	  },

	  /**
	   * Sets the previous node relative to the currently focused item, to focused.
	   */
	  _focusPrevNode: oncePerAnimationFrame(function () {
	    // Start a depth first search and keep going until we reach the currently
	    // focused node. Focus the previous node in the DFS, if it exists. If it
	    // doesn't exist, we're at the first node already.

	    let prev;
	    let prevIndex;

	    const traversal = this._dfsFromRoots();
	    const length = traversal.length;
	    for (let i = 0; i < length; i++) {
	      const item = traversal[i].item;
	      if (item === this.props.focused) {
	        break;
	      }
	      prev = item;
	      prevIndex = i;
	    }

	    if (prev === undefined) {
	      return;
	    }

	    this._focus(prevIndex, prev);
	  }),

	  /**
	   * Handles the down arrow key which will focus either the next child
	   * or sibling row.
	   */
	  _focusNextNode: oncePerAnimationFrame(function () {
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
	      this._focus(i + 1, traversal[i + 1].item);
	    }
	  }),

	  /**
	   * Handles the left arrow key, going back up to the current rows'
	   * parent row.
	   */
	  _focusParentNode: oncePerAnimationFrame(function () {
	    const parent = this.props.getParent(this.props.focused);
	    if (!parent) {
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

	    this._focus(parentIndex, parent);
	  })
	});

/***/ },
/* 47 */
/***/ function(module, exports) {

	// removed by extract-text-webpack-plugin

/***/ },
/* 48 */,
/* 49 */
/***/ function(module, exports, __webpack_require__) {

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
			}.apply(exports, __WEBPACK_AMD_DEFINE_ARRAY__), __WEBPACK_AMD_DEFINE_RESULT__ !== undefined && (module.exports = __WEBPACK_AMD_DEFINE_RESULT__));
		} else {
			window.classNames = classNames;
		}
	}());


/***/ },
/* 50 */
/***/ function(module, exports, __webpack_require__) {

	/* This Source Code Form is subject to the terms of the Mozilla Public
	 * License, v. 2.0. If a copy of the MPL was not distributed with this
	 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

	const get = __webpack_require__(51);
	const { maybeEscapePropertyName } = __webpack_require__(7);

	let WINDOW_PROPERTIES = {};

	if (typeof window === "object") {
	  WINDOW_PROPERTIES = Object.getOwnPropertyNames(window);
	}

	function getValue(item) {
	  return get(item, "contents.value", undefined);
	}

	function isBucket(item) {
	  return item.path && item.path.match(/bucket(\d+)$/);
	}

	function nodeHasChildren(item) {
	  return Array.isArray(item.contents) || isBucket(item);
	}

	function nodeIsObject(item) {
	  const value = getValue(item);
	  return value && value.type === "object";
	}

	function nodeIsArray(value) {
	  return value && value.class === "Array";
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
	  return !nodeHasChildren(item) && !nodeHasProperties(item);
	}

	function isPromise(item) {
	  const value = getValue(item);
	  return value.class == "Promise";
	}

	function getPromiseProperties(item) {
	  const { promiseState: { reason, value, state } } = getValue(item);

	  const properties = [];

	  if (state) {
	    properties.push(createNode("<state>", `${item.path}/state`, { value: state }));
	  }

	  if (reason) {
	    properties.push(createNode("<reason>", `${item.path}/reason`, { value: reason }));
	  }

	  if (value) {
	    properties.push(createNode("<value>", `${item.path}/value`, { value: value }));
	  }

	  return properties;
	}

	function isDefault(item, roots) {
	  if (roots && roots.length === 1) {
	    const value = getValue(roots[0]);
	    return value.class === "Window";
	  }
	  return WINDOW_PROPERTIES.includes(item.name);
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

	function makeNumericalBuckets(props, bucketSize, parentPath, ownProperties) {
	  const numProperties = props.length;
	  const numBuckets = Math.ceil(numProperties / bucketSize);
	  let buckets = [];
	  for (let i = 1; i <= numBuckets; i++) {
	    const bucketKey = `bucket${i}`;
	    const minKey = (i - 1) * bucketSize;
	    const maxKey = Math.min(i * bucketSize - 1, numProperties);
	    const bucketName = `[${minKey}..${maxKey}]`;
	    const bucketProperties = props.slice(minKey, maxKey);

	    const bucketNodes = bucketProperties.map(name => createNode(name, `${parentPath}/${bucketKey}/${name}`, ownProperties[name]));

	    buckets.push(createNode(bucketName, `${parentPath}/${bucketKey}`, bucketNodes));
	  }
	  return buckets;
	}

	function makeDefaultPropsBucket(props, parentPath, ownProperties) {
	  const userProps = props.filter(name => !isDefault({ name }));
	  const defaultProps = props.filter(name => isDefault({ name }));

	  let nodes = makeNodesForOwnProps(userProps, parentPath, ownProperties);

	  if (defaultProps.length > 0) {
	    const defaultNodes = defaultProps.map((name, index) => createNode(maybeEscapePropertyName(name), `${parentPath}/bucket${index}/${name}`, ownProperties[name]));
	    nodes.push(createNode("[default properties]", `${parentPath}/##-default`, defaultNodes));
	  }
	  return nodes;
	}

	function makeNodesForOwnProps(properties, parentPath, ownProperties) {
	  return properties.map(name => createNode(maybeEscapePropertyName(name), `${parentPath}/${name}`, ownProperties[name]));
	}

	/*
	 * Ignore non-concrete values like getters and setters
	 * for now by making sure we have a value.
	*/
	function makeNodesForProperties(objProps, parent, { bucketSize = 100 } = {}) {
	  const { ownProperties, prototype, ownSymbols } = objProps;
	  const parentPath = parent.path;
	  const parentValue = getValue(parent);
	  const properties = sortProperties(Object.keys(ownProperties)).filter(name => ownProperties[name].hasOwnProperty("value"));

	  const numProperties = properties.length;

	  let nodes = [];
	  if (nodeIsArray(prototype) && numProperties > bucketSize) {
	    nodes = makeNumericalBuckets(properties, bucketSize, parentPath, ownProperties);
	  } else if (parentValue.class == "Window") {
	    nodes = makeDefaultPropsBucket(properties, parentPath, ownProperties);
	  } else {
	    nodes = makeNodesForOwnProps(properties, parentPath, ownProperties);
	  }

	  for (let index in ownSymbols) {
	    nodes.push(createNode(ownSymbols[index].name, `${parentPath}/##symbol-${index}`, ownSymbols[index].descriptor));
	  }

	  if (isPromise(parent)) {
	    nodes.push(...getPromiseProperties(parent));
	  }

	  // Add the prototype if it exists and is not null
	  if (prototype && prototype.type !== "null") {
	    nodes.push(createNode("__proto__", `${parentPath}/__proto__`, { value: prototype }));
	  }

	  return nodes;
	}

	function createNode(name, path, contents) {
	  if (contents === undefined) {
	    return null;
	  }
	  // The path is important to uniquely identify the item in the entire
	  // tree. This helps debugging & optimizes React's rendering of large
	  // lists. The path will be separated by property name,
	  // i.e. `{ foo: { bar: { baz: 5 }}}` will have a path of `foo/bar/baz`
	  // for the inner object.
	  return { name, path, contents };
	}

	function getChildren({ getObjectProperties, actors, item }) {
	  const obj = item.contents;

	  // Nodes can either have children already, or be an object with
	  // properties that we need to go and fetch.
	  if (nodeHasChildren(item)) {
	    return item.contents;
	  }

	  if (!nodeHasProperties(item)) {
	    return [];
	  }

	  const actor = obj.value.actor;

	  // Because we are dynamically creating the tree as the user
	  // expands it (not precalculated tree structure), we cache child
	  // arrays. This not only helps performance, but is necessary
	  // because the expanded state depends on instances of nodes
	  // being the same across renders. If we didn't do this, each
	  // node would be a new instance every render.
	  const key = item.path;
	  if (actors && actors[key]) {
	    return actors[key];
	  }

	  if (isBucket(item)) {
	    return item.contents.children;
	  }

	  const loadedProps = getObjectProperties(actor);
	  const { ownProperties, prototype } = loadedProps || {};

	  if (!ownProperties && !prototype) {
	    return [];
	  }

	  let children = makeNodesForProperties(loadedProps, item);
	  actors[key] = children;
	  return children;
	}

	module.exports = {
	  createNode,
	  getChildren,
	  getPromiseProperties,
	  getValue,
	  isDefault,
	  isPromise,
	  makeNodesForProperties,
	  nodeHasChildren,
	  nodeHasProperties,
	  nodeIsFunction,
	  nodeIsMissingArguments,
	  nodeIsObject,
	  nodeIsOptimizedOut,
	  nodeIsPrimitive,
	  sortProperties
	};

/***/ },
/* 51 */
/***/ function(module, exports, __webpack_require__) {

	var baseGet = __webpack_require__(52);

	/**
	 * Gets the value at `path` of `object`. If the resolved value is
	 * `undefined`, the `defaultValue` is returned in its place.
	 *
	 * @static
	 * @memberOf _
	 * @since 3.7.0
	 * @category Object
	 * @param {Object} object The object to query.
	 * @param {Array|string} path The path of the property to get.
	 * @param {*} [defaultValue] The value returned for `undefined` resolved values.
	 * @returns {*} Returns the resolved value.
	 * @example
	 *
	 * var object = { 'a': [{ 'b': { 'c': 3 } }] };
	 *
	 * _.get(object, 'a[0].b.c');
	 * // => 3
	 *
	 * _.get(object, ['a', '0', 'b', 'c']);
	 * // => 3
	 *
	 * _.get(object, 'a.b.c', 'default');
	 * // => 'default'
	 */
	function get(object, path, defaultValue) {
	  var result = object == null ? undefined : baseGet(object, path);
	  return result === undefined ? defaultValue : result;
	}

	module.exports = get;


/***/ },
/* 52 */
/***/ function(module, exports, __webpack_require__) {

	var castPath = __webpack_require__(53),
	    toKey = __webpack_require__(102);

	/**
	 * The base implementation of `_.get` without support for default values.
	 *
	 * @private
	 * @param {Object} object The object to query.
	 * @param {Array|string} path The path of the property to get.
	 * @returns {*} Returns the resolved value.
	 */
	function baseGet(object, path) {
	  path = castPath(path, object);

	  var index = 0,
	      length = path.length;

	  while (object != null && index < length) {
	    object = object[toKey(path[index++])];
	  }
	  return (index && index == length) ? object : undefined;
	}

	module.exports = baseGet;


/***/ },
/* 53 */
/***/ function(module, exports, __webpack_require__) {

	var isArray = __webpack_require__(54),
	    isKey = __webpack_require__(55),
	    stringToPath = __webpack_require__(64),
	    toString = __webpack_require__(99);

	/**
	 * Casts `value` to a path array if it's not one.
	 *
	 * @private
	 * @param {*} value The value to inspect.
	 * @param {Object} [object] The object to query keys on.
	 * @returns {Array} Returns the cast property path array.
	 */
	function castPath(value, object) {
	  if (isArray(value)) {
	    return value;
	  }
	  return isKey(value, object) ? [value] : stringToPath(toString(value));
	}

	module.exports = castPath;


/***/ },
/* 54 */
/***/ function(module, exports) {

	/**
	 * Checks if `value` is classified as an `Array` object.
	 *
	 * @static
	 * @memberOf _
	 * @since 0.1.0
	 * @category Lang
	 * @param {*} value The value to check.
	 * @returns {boolean} Returns `true` if `value` is an array, else `false`.
	 * @example
	 *
	 * _.isArray([1, 2, 3]);
	 * // => true
	 *
	 * _.isArray(document.body.children);
	 * // => false
	 *
	 * _.isArray('abc');
	 * // => false
	 *
	 * _.isArray(_.noop);
	 * // => false
	 */
	var isArray = Array.isArray;

	module.exports = isArray;


/***/ },
/* 55 */
/***/ function(module, exports, __webpack_require__) {

	var isArray = __webpack_require__(54),
	    isSymbol = __webpack_require__(56);

	/** Used to match property names within property paths. */
	var reIsDeepProp = /\.|\[(?:[^[\]]*|(["'])(?:(?!\1)[^\\]|\\.)*?\1)\]/,
	    reIsPlainProp = /^\w*$/;

	/**
	 * Checks if `value` is a property name and not a property path.
	 *
	 * @private
	 * @param {*} value The value to check.
	 * @param {Object} [object] The object to query keys on.
	 * @returns {boolean} Returns `true` if `value` is a property name, else `false`.
	 */
	function isKey(value, object) {
	  if (isArray(value)) {
	    return false;
	  }
	  var type = typeof value;
	  if (type == 'number' || type == 'symbol' || type == 'boolean' ||
	      value == null || isSymbol(value)) {
	    return true;
	  }
	  return reIsPlainProp.test(value) || !reIsDeepProp.test(value) ||
	    (object != null && value in Object(object));
	}

	module.exports = isKey;


/***/ },
/* 56 */
/***/ function(module, exports, __webpack_require__) {

	var baseGetTag = __webpack_require__(57),
	    isObjectLike = __webpack_require__(63);

	/** `Object#toString` result references. */
	var symbolTag = '[object Symbol]';

	/**
	 * Checks if `value` is classified as a `Symbol` primitive or object.
	 *
	 * @static
	 * @memberOf _
	 * @since 4.0.0
	 * @category Lang
	 * @param {*} value The value to check.
	 * @returns {boolean} Returns `true` if `value` is a symbol, else `false`.
	 * @example
	 *
	 * _.isSymbol(Symbol.iterator);
	 * // => true
	 *
	 * _.isSymbol('abc');
	 * // => false
	 */
	function isSymbol(value) {
	  return typeof value == 'symbol' ||
	    (isObjectLike(value) && baseGetTag(value) == symbolTag);
	}

	module.exports = isSymbol;


/***/ },
/* 57 */
/***/ function(module, exports, __webpack_require__) {

	var Symbol = __webpack_require__(58),
	    getRawTag = __webpack_require__(61),
	    objectToString = __webpack_require__(62);

	/** `Object#toString` result references. */
	var nullTag = '[object Null]',
	    undefinedTag = '[object Undefined]';

	/** Built-in value references. */
	var symToStringTag = Symbol ? Symbol.toStringTag : undefined;

	/**
	 * The base implementation of `getTag` without fallbacks for buggy environments.
	 *
	 * @private
	 * @param {*} value The value to query.
	 * @returns {string} Returns the `toStringTag`.
	 */
	function baseGetTag(value) {
	  if (value == null) {
	    return value === undefined ? undefinedTag : nullTag;
	  }
	  return (symToStringTag && symToStringTag in Object(value))
	    ? getRawTag(value)
	    : objectToString(value);
	}

	module.exports = baseGetTag;


/***/ },
/* 58 */
/***/ function(module, exports, __webpack_require__) {

	var root = __webpack_require__(59);

	/** Built-in value references. */
	var Symbol = root.Symbol;

	module.exports = Symbol;


/***/ },
/* 59 */
/***/ function(module, exports, __webpack_require__) {

	var freeGlobal = __webpack_require__(60);

	/** Detect free variable `self`. */
	var freeSelf = typeof self == 'object' && self && self.Object === Object && self;

	/** Used as a reference to the global object. */
	var root = freeGlobal || freeSelf || Function('return this')();

	module.exports = root;


/***/ },
/* 60 */
/***/ function(module, exports) {

	/* WEBPACK VAR INJECTION */(function(global) {/** Detect free variable `global` from Node.js. */
	var freeGlobal = typeof global == 'object' && global && global.Object === Object && global;

	module.exports = freeGlobal;

	/* WEBPACK VAR INJECTION */}.call(exports, (function() { return this; }())))

/***/ },
/* 61 */
/***/ function(module, exports, __webpack_require__) {

	var Symbol = __webpack_require__(58);

	/** Used for built-in method references. */
	var objectProto = Object.prototype;

	/** Used to check objects for own properties. */
	var hasOwnProperty = objectProto.hasOwnProperty;

	/**
	 * Used to resolve the
	 * [`toStringTag`](http://ecma-international.org/ecma-262/7.0/#sec-object.prototype.tostring)
	 * of values.
	 */
	var nativeObjectToString = objectProto.toString;

	/** Built-in value references. */
	var symToStringTag = Symbol ? Symbol.toStringTag : undefined;

	/**
	 * A specialized version of `baseGetTag` which ignores `Symbol.toStringTag` values.
	 *
	 * @private
	 * @param {*} value The value to query.
	 * @returns {string} Returns the raw `toStringTag`.
	 */
	function getRawTag(value) {
	  var isOwn = hasOwnProperty.call(value, symToStringTag),
	      tag = value[symToStringTag];

	  try {
	    value[symToStringTag] = undefined;
	    var unmasked = true;
	  } catch (e) {}

	  var result = nativeObjectToString.call(value);
	  if (unmasked) {
	    if (isOwn) {
	      value[symToStringTag] = tag;
	    } else {
	      delete value[symToStringTag];
	    }
	  }
	  return result;
	}

	module.exports = getRawTag;


/***/ },
/* 62 */
/***/ function(module, exports) {

	/** Used for built-in method references. */
	var objectProto = Object.prototype;

	/**
	 * Used to resolve the
	 * [`toStringTag`](http://ecma-international.org/ecma-262/7.0/#sec-object.prototype.tostring)
	 * of values.
	 */
	var nativeObjectToString = objectProto.toString;

	/**
	 * Converts `value` to a string using `Object.prototype.toString`.
	 *
	 * @private
	 * @param {*} value The value to convert.
	 * @returns {string} Returns the converted string.
	 */
	function objectToString(value) {
	  return nativeObjectToString.call(value);
	}

	module.exports = objectToString;


/***/ },
/* 63 */
/***/ function(module, exports) {

	/**
	 * Checks if `value` is object-like. A value is object-like if it's not `null`
	 * and has a `typeof` result of "object".
	 *
	 * @static
	 * @memberOf _
	 * @since 4.0.0
	 * @category Lang
	 * @param {*} value The value to check.
	 * @returns {boolean} Returns `true` if `value` is object-like, else `false`.
	 * @example
	 *
	 * _.isObjectLike({});
	 * // => true
	 *
	 * _.isObjectLike([1, 2, 3]);
	 * // => true
	 *
	 * _.isObjectLike(_.noop);
	 * // => false
	 *
	 * _.isObjectLike(null);
	 * // => false
	 */
	function isObjectLike(value) {
	  return value != null && typeof value == 'object';
	}

	module.exports = isObjectLike;


/***/ },
/* 64 */
/***/ function(module, exports, __webpack_require__) {

	var memoizeCapped = __webpack_require__(65);

	/** Used to match property names within property paths. */
	var reLeadingDot = /^\./,
	    rePropName = /[^.[\]]+|\[(?:(-?\d+(?:\.\d+)?)|(["'])((?:(?!\2)[^\\]|\\.)*?)\2)\]|(?=(?:\.|\[\])(?:\.|\[\]|$))/g;

	/** Used to match backslashes in property paths. */
	var reEscapeChar = /\\(\\)?/g;

	/**
	 * Converts `string` to a property path array.
	 *
	 * @private
	 * @param {string} string The string to convert.
	 * @returns {Array} Returns the property path array.
	 */
	var stringToPath = memoizeCapped(function(string) {
	  var result = [];
	  if (reLeadingDot.test(string)) {
	    result.push('');
	  }
	  string.replace(rePropName, function(match, number, quote, string) {
	    result.push(quote ? string.replace(reEscapeChar, '$1') : (number || match));
	  });
	  return result;
	});

	module.exports = stringToPath;


/***/ },
/* 65 */
/***/ function(module, exports, __webpack_require__) {

	var memoize = __webpack_require__(66);

	/** Used as the maximum memoize cache size. */
	var MAX_MEMOIZE_SIZE = 500;

	/**
	 * A specialized version of `_.memoize` which clears the memoized function's
	 * cache when it exceeds `MAX_MEMOIZE_SIZE`.
	 *
	 * @private
	 * @param {Function} func The function to have its output memoized.
	 * @returns {Function} Returns the new memoized function.
	 */
	function memoizeCapped(func) {
	  var result = memoize(func, function(key) {
	    if (cache.size === MAX_MEMOIZE_SIZE) {
	      cache.clear();
	    }
	    return key;
	  });

	  var cache = result.cache;
	  return result;
	}

	module.exports = memoizeCapped;


/***/ },
/* 66 */
/***/ function(module, exports, __webpack_require__) {

	var MapCache = __webpack_require__(67);

	/** Error message constants. */
	var FUNC_ERROR_TEXT = 'Expected a function';

	/**
	 * Creates a function that memoizes the result of `func`. If `resolver` is
	 * provided, it determines the cache key for storing the result based on the
	 * arguments provided to the memoized function. By default, the first argument
	 * provided to the memoized function is used as the map cache key. The `func`
	 * is invoked with the `this` binding of the memoized function.
	 *
	 * **Note:** The cache is exposed as the `cache` property on the memoized
	 * function. Its creation may be customized by replacing the `_.memoize.Cache`
	 * constructor with one whose instances implement the
	 * [`Map`](http://ecma-international.org/ecma-262/7.0/#sec-properties-of-the-map-prototype-object)
	 * method interface of `clear`, `delete`, `get`, `has`, and `set`.
	 *
	 * @static
	 * @memberOf _
	 * @since 0.1.0
	 * @category Function
	 * @param {Function} func The function to have its output memoized.
	 * @param {Function} [resolver] The function to resolve the cache key.
	 * @returns {Function} Returns the new memoized function.
	 * @example
	 *
	 * var object = { 'a': 1, 'b': 2 };
	 * var other = { 'c': 3, 'd': 4 };
	 *
	 * var values = _.memoize(_.values);
	 * values(object);
	 * // => [1, 2]
	 *
	 * values(other);
	 * // => [3, 4]
	 *
	 * object.a = 2;
	 * values(object);
	 * // => [1, 2]
	 *
	 * // Modify the result cache.
	 * values.cache.set(object, ['a', 'b']);
	 * values(object);
	 * // => ['a', 'b']
	 *
	 * // Replace `_.memoize.Cache`.
	 * _.memoize.Cache = WeakMap;
	 */
	function memoize(func, resolver) {
	  if (typeof func != 'function' || (resolver != null && typeof resolver != 'function')) {
	    throw new TypeError(FUNC_ERROR_TEXT);
	  }
	  var memoized = function() {
	    var args = arguments,
	        key = resolver ? resolver.apply(this, args) : args[0],
	        cache = memoized.cache;

	    if (cache.has(key)) {
	      return cache.get(key);
	    }
	    var result = func.apply(this, args);
	    memoized.cache = cache.set(key, result) || cache;
	    return result;
	  };
	  memoized.cache = new (memoize.Cache || MapCache);
	  return memoized;
	}

	// Expose `MapCache`.
	memoize.Cache = MapCache;

	module.exports = memoize;


/***/ },
/* 67 */
/***/ function(module, exports, __webpack_require__) {

	var mapCacheClear = __webpack_require__(68),
	    mapCacheDelete = __webpack_require__(93),
	    mapCacheGet = __webpack_require__(96),
	    mapCacheHas = __webpack_require__(97),
	    mapCacheSet = __webpack_require__(98);

	/**
	 * Creates a map cache object to store key-value pairs.
	 *
	 * @private
	 * @constructor
	 * @param {Array} [entries] The key-value pairs to cache.
	 */
	function MapCache(entries) {
	  var index = -1,
	      length = entries == null ? 0 : entries.length;

	  this.clear();
	  while (++index < length) {
	    var entry = entries[index];
	    this.set(entry[0], entry[1]);
	  }
	}

	// Add methods to `MapCache`.
	MapCache.prototype.clear = mapCacheClear;
	MapCache.prototype['delete'] = mapCacheDelete;
	MapCache.prototype.get = mapCacheGet;
	MapCache.prototype.has = mapCacheHas;
	MapCache.prototype.set = mapCacheSet;

	module.exports = MapCache;


/***/ },
/* 68 */
/***/ function(module, exports, __webpack_require__) {

	var Hash = __webpack_require__(69),
	    ListCache = __webpack_require__(84),
	    Map = __webpack_require__(92);

	/**
	 * Removes all key-value entries from the map.
	 *
	 * @private
	 * @name clear
	 * @memberOf MapCache
	 */
	function mapCacheClear() {
	  this.size = 0;
	  this.__data__ = {
	    'hash': new Hash,
	    'map': new (Map || ListCache),
	    'string': new Hash
	  };
	}

	module.exports = mapCacheClear;


/***/ },
/* 69 */
/***/ function(module, exports, __webpack_require__) {

	var hashClear = __webpack_require__(70),
	    hashDelete = __webpack_require__(80),
	    hashGet = __webpack_require__(81),
	    hashHas = __webpack_require__(82),
	    hashSet = __webpack_require__(83);

	/**
	 * Creates a hash object.
	 *
	 * @private
	 * @constructor
	 * @param {Array} [entries] The key-value pairs to cache.
	 */
	function Hash(entries) {
	  var index = -1,
	      length = entries == null ? 0 : entries.length;

	  this.clear();
	  while (++index < length) {
	    var entry = entries[index];
	    this.set(entry[0], entry[1]);
	  }
	}

	// Add methods to `Hash`.
	Hash.prototype.clear = hashClear;
	Hash.prototype['delete'] = hashDelete;
	Hash.prototype.get = hashGet;
	Hash.prototype.has = hashHas;
	Hash.prototype.set = hashSet;

	module.exports = Hash;


/***/ },
/* 70 */
/***/ function(module, exports, __webpack_require__) {

	var nativeCreate = __webpack_require__(71);

	/**
	 * Removes all key-value entries from the hash.
	 *
	 * @private
	 * @name clear
	 * @memberOf Hash
	 */
	function hashClear() {
	  this.__data__ = nativeCreate ? nativeCreate(null) : {};
	  this.size = 0;
	}

	module.exports = hashClear;


/***/ },
/* 71 */
/***/ function(module, exports, __webpack_require__) {

	var getNative = __webpack_require__(72);

	/* Built-in method references that are verified to be native. */
	var nativeCreate = getNative(Object, 'create');

	module.exports = nativeCreate;


/***/ },
/* 72 */
/***/ function(module, exports, __webpack_require__) {

	var baseIsNative = __webpack_require__(73),
	    getValue = __webpack_require__(79);

	/**
	 * Gets the native function at `key` of `object`.
	 *
	 * @private
	 * @param {Object} object The object to query.
	 * @param {string} key The key of the method to get.
	 * @returns {*} Returns the function if it's native, else `undefined`.
	 */
	function getNative(object, key) {
	  var value = getValue(object, key);
	  return baseIsNative(value) ? value : undefined;
	}

	module.exports = getNative;


/***/ },
/* 73 */
/***/ function(module, exports, __webpack_require__) {

	var isFunction = __webpack_require__(74),
	    isMasked = __webpack_require__(76),
	    isObject = __webpack_require__(75),
	    toSource = __webpack_require__(78);

	/**
	 * Used to match `RegExp`
	 * [syntax characters](http://ecma-international.org/ecma-262/7.0/#sec-patterns).
	 */
	var reRegExpChar = /[\\^$.*+?()[\]{}|]/g;

	/** Used to detect host constructors (Safari). */
	var reIsHostCtor = /^\[object .+?Constructor\]$/;

	/** Used for built-in method references. */
	var funcProto = Function.prototype,
	    objectProto = Object.prototype;

	/** Used to resolve the decompiled source of functions. */
	var funcToString = funcProto.toString;

	/** Used to check objects for own properties. */
	var hasOwnProperty = objectProto.hasOwnProperty;

	/** Used to detect if a method is native. */
	var reIsNative = RegExp('^' +
	  funcToString.call(hasOwnProperty).replace(reRegExpChar, '\\$&')
	  .replace(/hasOwnProperty|(function).*?(?=\\\()| for .+?(?=\\\])/g, '$1.*?') + '$'
	);

	/**
	 * The base implementation of `_.isNative` without bad shim checks.
	 *
	 * @private
	 * @param {*} value The value to check.
	 * @returns {boolean} Returns `true` if `value` is a native function,
	 *  else `false`.
	 */
	function baseIsNative(value) {
	  if (!isObject(value) || isMasked(value)) {
	    return false;
	  }
	  var pattern = isFunction(value) ? reIsNative : reIsHostCtor;
	  return pattern.test(toSource(value));
	}

	module.exports = baseIsNative;


/***/ },
/* 74 */
/***/ function(module, exports, __webpack_require__) {

	var baseGetTag = __webpack_require__(57),
	    isObject = __webpack_require__(75);

	/** `Object#toString` result references. */
	var asyncTag = '[object AsyncFunction]',
	    funcTag = '[object Function]',
	    genTag = '[object GeneratorFunction]',
	    proxyTag = '[object Proxy]';

	/**
	 * Checks if `value` is classified as a `Function` object.
	 *
	 * @static
	 * @memberOf _
	 * @since 0.1.0
	 * @category Lang
	 * @param {*} value The value to check.
	 * @returns {boolean} Returns `true` if `value` is a function, else `false`.
	 * @example
	 *
	 * _.isFunction(_);
	 * // => true
	 *
	 * _.isFunction(/abc/);
	 * // => false
	 */
	function isFunction(value) {
	  if (!isObject(value)) {
	    return false;
	  }
	  // The use of `Object#toString` avoids issues with the `typeof` operator
	  // in Safari 9 which returns 'object' for typed arrays and other constructors.
	  var tag = baseGetTag(value);
	  return tag == funcTag || tag == genTag || tag == asyncTag || tag == proxyTag;
	}

	module.exports = isFunction;


/***/ },
/* 75 */
/***/ function(module, exports) {

	/**
	 * Checks if `value` is the
	 * [language type](http://www.ecma-international.org/ecma-262/7.0/#sec-ecmascript-language-types)
	 * of `Object`. (e.g. arrays, functions, objects, regexes, `new Number(0)`, and `new String('')`)
	 *
	 * @static
	 * @memberOf _
	 * @since 0.1.0
	 * @category Lang
	 * @param {*} value The value to check.
	 * @returns {boolean} Returns `true` if `value` is an object, else `false`.
	 * @example
	 *
	 * _.isObject({});
	 * // => true
	 *
	 * _.isObject([1, 2, 3]);
	 * // => true
	 *
	 * _.isObject(_.noop);
	 * // => true
	 *
	 * _.isObject(null);
	 * // => false
	 */
	function isObject(value) {
	  var type = typeof value;
	  return value != null && (type == 'object' || type == 'function');
	}

	module.exports = isObject;


/***/ },
/* 76 */
/***/ function(module, exports, __webpack_require__) {

	var coreJsData = __webpack_require__(77);

	/** Used to detect methods masquerading as native. */
	var maskSrcKey = (function() {
	  var uid = /[^.]+$/.exec(coreJsData && coreJsData.keys && coreJsData.keys.IE_PROTO || '');
	  return uid ? ('Symbol(src)_1.' + uid) : '';
	}());

	/**
	 * Checks if `func` has its source masked.
	 *
	 * @private
	 * @param {Function} func The function to check.
	 * @returns {boolean} Returns `true` if `func` is masked, else `false`.
	 */
	function isMasked(func) {
	  return !!maskSrcKey && (maskSrcKey in func);
	}

	module.exports = isMasked;


/***/ },
/* 77 */
/***/ function(module, exports, __webpack_require__) {

	var root = __webpack_require__(59);

	/** Used to detect overreaching core-js shims. */
	var coreJsData = root['__core-js_shared__'];

	module.exports = coreJsData;


/***/ },
/* 78 */
/***/ function(module, exports) {

	/** Used for built-in method references. */
	var funcProto = Function.prototype;

	/** Used to resolve the decompiled source of functions. */
	var funcToString = funcProto.toString;

	/**
	 * Converts `func` to its source code.
	 *
	 * @private
	 * @param {Function} func The function to convert.
	 * @returns {string} Returns the source code.
	 */
	function toSource(func) {
	  if (func != null) {
	    try {
	      return funcToString.call(func);
	    } catch (e) {}
	    try {
	      return (func + '');
	    } catch (e) {}
	  }
	  return '';
	}

	module.exports = toSource;


/***/ },
/* 79 */
/***/ function(module, exports) {

	/**
	 * Gets the value at `key` of `object`.
	 *
	 * @private
	 * @param {Object} [object] The object to query.
	 * @param {string} key The key of the property to get.
	 * @returns {*} Returns the property value.
	 */
	function getValue(object, key) {
	  return object == null ? undefined : object[key];
	}

	module.exports = getValue;


/***/ },
/* 80 */
/***/ function(module, exports) {

	/**
	 * Removes `key` and its value from the hash.
	 *
	 * @private
	 * @name delete
	 * @memberOf Hash
	 * @param {Object} hash The hash to modify.
	 * @param {string} key The key of the value to remove.
	 * @returns {boolean} Returns `true` if the entry was removed, else `false`.
	 */
	function hashDelete(key) {
	  var result = this.has(key) && delete this.__data__[key];
	  this.size -= result ? 1 : 0;
	  return result;
	}

	module.exports = hashDelete;


/***/ },
/* 81 */
/***/ function(module, exports, __webpack_require__) {

	var nativeCreate = __webpack_require__(71);

	/** Used to stand-in for `undefined` hash values. */
	var HASH_UNDEFINED = '__lodash_hash_undefined__';

	/** Used for built-in method references. */
	var objectProto = Object.prototype;

	/** Used to check objects for own properties. */
	var hasOwnProperty = objectProto.hasOwnProperty;

	/**
	 * Gets the hash value for `key`.
	 *
	 * @private
	 * @name get
	 * @memberOf Hash
	 * @param {string} key The key of the value to get.
	 * @returns {*} Returns the entry value.
	 */
	function hashGet(key) {
	  var data = this.__data__;
	  if (nativeCreate) {
	    var result = data[key];
	    return result === HASH_UNDEFINED ? undefined : result;
	  }
	  return hasOwnProperty.call(data, key) ? data[key] : undefined;
	}

	module.exports = hashGet;


/***/ },
/* 82 */
/***/ function(module, exports, __webpack_require__) {

	var nativeCreate = __webpack_require__(71);

	/** Used for built-in method references. */
	var objectProto = Object.prototype;

	/** Used to check objects for own properties. */
	var hasOwnProperty = objectProto.hasOwnProperty;

	/**
	 * Checks if a hash value for `key` exists.
	 *
	 * @private
	 * @name has
	 * @memberOf Hash
	 * @param {string} key The key of the entry to check.
	 * @returns {boolean} Returns `true` if an entry for `key` exists, else `false`.
	 */
	function hashHas(key) {
	  var data = this.__data__;
	  return nativeCreate ? (data[key] !== undefined) : hasOwnProperty.call(data, key);
	}

	module.exports = hashHas;


/***/ },
/* 83 */
/***/ function(module, exports, __webpack_require__) {

	var nativeCreate = __webpack_require__(71);

	/** Used to stand-in for `undefined` hash values. */
	var HASH_UNDEFINED = '__lodash_hash_undefined__';

	/**
	 * Sets the hash `key` to `value`.
	 *
	 * @private
	 * @name set
	 * @memberOf Hash
	 * @param {string} key The key of the value to set.
	 * @param {*} value The value to set.
	 * @returns {Object} Returns the hash instance.
	 */
	function hashSet(key, value) {
	  var data = this.__data__;
	  this.size += this.has(key) ? 0 : 1;
	  data[key] = (nativeCreate && value === undefined) ? HASH_UNDEFINED : value;
	  return this;
	}

	module.exports = hashSet;


/***/ },
/* 84 */
/***/ function(module, exports, __webpack_require__) {

	var listCacheClear = __webpack_require__(85),
	    listCacheDelete = __webpack_require__(86),
	    listCacheGet = __webpack_require__(89),
	    listCacheHas = __webpack_require__(90),
	    listCacheSet = __webpack_require__(91);

	/**
	 * Creates an list cache object.
	 *
	 * @private
	 * @constructor
	 * @param {Array} [entries] The key-value pairs to cache.
	 */
	function ListCache(entries) {
	  var index = -1,
	      length = entries == null ? 0 : entries.length;

	  this.clear();
	  while (++index < length) {
	    var entry = entries[index];
	    this.set(entry[0], entry[1]);
	  }
	}

	// Add methods to `ListCache`.
	ListCache.prototype.clear = listCacheClear;
	ListCache.prototype['delete'] = listCacheDelete;
	ListCache.prototype.get = listCacheGet;
	ListCache.prototype.has = listCacheHas;
	ListCache.prototype.set = listCacheSet;

	module.exports = ListCache;


/***/ },
/* 85 */
/***/ function(module, exports) {

	/**
	 * Removes all key-value entries from the list cache.
	 *
	 * @private
	 * @name clear
	 * @memberOf ListCache
	 */
	function listCacheClear() {
	  this.__data__ = [];
	  this.size = 0;
	}

	module.exports = listCacheClear;


/***/ },
/* 86 */
/***/ function(module, exports, __webpack_require__) {

	var assocIndexOf = __webpack_require__(87);

	/** Used for built-in method references. */
	var arrayProto = Array.prototype;

	/** Built-in value references. */
	var splice = arrayProto.splice;

	/**
	 * Removes `key` and its value from the list cache.
	 *
	 * @private
	 * @name delete
	 * @memberOf ListCache
	 * @param {string} key The key of the value to remove.
	 * @returns {boolean} Returns `true` if the entry was removed, else `false`.
	 */
	function listCacheDelete(key) {
	  var data = this.__data__,
	      index = assocIndexOf(data, key);

	  if (index < 0) {
	    return false;
	  }
	  var lastIndex = data.length - 1;
	  if (index == lastIndex) {
	    data.pop();
	  } else {
	    splice.call(data, index, 1);
	  }
	  --this.size;
	  return true;
	}

	module.exports = listCacheDelete;


/***/ },
/* 87 */
/***/ function(module, exports, __webpack_require__) {

	var eq = __webpack_require__(88);

	/**
	 * Gets the index at which the `key` is found in `array` of key-value pairs.
	 *
	 * @private
	 * @param {Array} array The array to inspect.
	 * @param {*} key The key to search for.
	 * @returns {number} Returns the index of the matched value, else `-1`.
	 */
	function assocIndexOf(array, key) {
	  var length = array.length;
	  while (length--) {
	    if (eq(array[length][0], key)) {
	      return length;
	    }
	  }
	  return -1;
	}

	module.exports = assocIndexOf;


/***/ },
/* 88 */
/***/ function(module, exports) {

	/**
	 * Performs a
	 * [`SameValueZero`](http://ecma-international.org/ecma-262/7.0/#sec-samevaluezero)
	 * comparison between two values to determine if they are equivalent.
	 *
	 * @static
	 * @memberOf _
	 * @since 4.0.0
	 * @category Lang
	 * @param {*} value The value to compare.
	 * @param {*} other The other value to compare.
	 * @returns {boolean} Returns `true` if the values are equivalent, else `false`.
	 * @example
	 *
	 * var object = { 'a': 1 };
	 * var other = { 'a': 1 };
	 *
	 * _.eq(object, object);
	 * // => true
	 *
	 * _.eq(object, other);
	 * // => false
	 *
	 * _.eq('a', 'a');
	 * // => true
	 *
	 * _.eq('a', Object('a'));
	 * // => false
	 *
	 * _.eq(NaN, NaN);
	 * // => true
	 */
	function eq(value, other) {
	  return value === other || (value !== value && other !== other);
	}

	module.exports = eq;


/***/ },
/* 89 */
/***/ function(module, exports, __webpack_require__) {

	var assocIndexOf = __webpack_require__(87);

	/**
	 * Gets the list cache value for `key`.
	 *
	 * @private
	 * @name get
	 * @memberOf ListCache
	 * @param {string} key The key of the value to get.
	 * @returns {*} Returns the entry value.
	 */
	function listCacheGet(key) {
	  var data = this.__data__,
	      index = assocIndexOf(data, key);

	  return index < 0 ? undefined : data[index][1];
	}

	module.exports = listCacheGet;


/***/ },
/* 90 */
/***/ function(module, exports, __webpack_require__) {

	var assocIndexOf = __webpack_require__(87);

	/**
	 * Checks if a list cache value for `key` exists.
	 *
	 * @private
	 * @name has
	 * @memberOf ListCache
	 * @param {string} key The key of the entry to check.
	 * @returns {boolean} Returns `true` if an entry for `key` exists, else `false`.
	 */
	function listCacheHas(key) {
	  return assocIndexOf(this.__data__, key) > -1;
	}

	module.exports = listCacheHas;


/***/ },
/* 91 */
/***/ function(module, exports, __webpack_require__) {

	var assocIndexOf = __webpack_require__(87);

	/**
	 * Sets the list cache `key` to `value`.
	 *
	 * @private
	 * @name set
	 * @memberOf ListCache
	 * @param {string} key The key of the value to set.
	 * @param {*} value The value to set.
	 * @returns {Object} Returns the list cache instance.
	 */
	function listCacheSet(key, value) {
	  var data = this.__data__,
	      index = assocIndexOf(data, key);

	  if (index < 0) {
	    ++this.size;
	    data.push([key, value]);
	  } else {
	    data[index][1] = value;
	  }
	  return this;
	}

	module.exports = listCacheSet;


/***/ },
/* 92 */
/***/ function(module, exports, __webpack_require__) {

	var getNative = __webpack_require__(72),
	    root = __webpack_require__(59);

	/* Built-in method references that are verified to be native. */
	var Map = getNative(root, 'Map');

	module.exports = Map;


/***/ },
/* 93 */
/***/ function(module, exports, __webpack_require__) {

	var getMapData = __webpack_require__(94);

	/**
	 * Removes `key` and its value from the map.
	 *
	 * @private
	 * @name delete
	 * @memberOf MapCache
	 * @param {string} key The key of the value to remove.
	 * @returns {boolean} Returns `true` if the entry was removed, else `false`.
	 */
	function mapCacheDelete(key) {
	  var result = getMapData(this, key)['delete'](key);
	  this.size -= result ? 1 : 0;
	  return result;
	}

	module.exports = mapCacheDelete;


/***/ },
/* 94 */
/***/ function(module, exports, __webpack_require__) {

	var isKeyable = __webpack_require__(95);

	/**
	 * Gets the data for `map`.
	 *
	 * @private
	 * @param {Object} map The map to query.
	 * @param {string} key The reference key.
	 * @returns {*} Returns the map data.
	 */
	function getMapData(map, key) {
	  var data = map.__data__;
	  return isKeyable(key)
	    ? data[typeof key == 'string' ? 'string' : 'hash']
	    : data.map;
	}

	module.exports = getMapData;


/***/ },
/* 95 */
/***/ function(module, exports) {

	/**
	 * Checks if `value` is suitable for use as unique object key.
	 *
	 * @private
	 * @param {*} value The value to check.
	 * @returns {boolean} Returns `true` if `value` is suitable, else `false`.
	 */
	function isKeyable(value) {
	  var type = typeof value;
	  return (type == 'string' || type == 'number' || type == 'symbol' || type == 'boolean')
	    ? (value !== '__proto__')
	    : (value === null);
	}

	module.exports = isKeyable;


/***/ },
/* 96 */
/***/ function(module, exports, __webpack_require__) {

	var getMapData = __webpack_require__(94);

	/**
	 * Gets the map value for `key`.
	 *
	 * @private
	 * @name get
	 * @memberOf MapCache
	 * @param {string} key The key of the value to get.
	 * @returns {*} Returns the entry value.
	 */
	function mapCacheGet(key) {
	  return getMapData(this, key).get(key);
	}

	module.exports = mapCacheGet;


/***/ },
/* 97 */
/***/ function(module, exports, __webpack_require__) {

	var getMapData = __webpack_require__(94);

	/**
	 * Checks if a map value for `key` exists.
	 *
	 * @private
	 * @name has
	 * @memberOf MapCache
	 * @param {string} key The key of the entry to check.
	 * @returns {boolean} Returns `true` if an entry for `key` exists, else `false`.
	 */
	function mapCacheHas(key) {
	  return getMapData(this, key).has(key);
	}

	module.exports = mapCacheHas;


/***/ },
/* 98 */
/***/ function(module, exports, __webpack_require__) {

	var getMapData = __webpack_require__(94);

	/**
	 * Sets the map `key` to `value`.
	 *
	 * @private
	 * @name set
	 * @memberOf MapCache
	 * @param {string} key The key of the value to set.
	 * @param {*} value The value to set.
	 * @returns {Object} Returns the map cache instance.
	 */
	function mapCacheSet(key, value) {
	  var data = getMapData(this, key),
	      size = data.size;

	  data.set(key, value);
	  this.size += data.size == size ? 0 : 1;
	  return this;
	}

	module.exports = mapCacheSet;


/***/ },
/* 99 */
/***/ function(module, exports, __webpack_require__) {

	var baseToString = __webpack_require__(100);

	/**
	 * Converts `value` to a string. An empty string is returned for `null`
	 * and `undefined` values. The sign of `-0` is preserved.
	 *
	 * @static
	 * @memberOf _
	 * @since 4.0.0
	 * @category Lang
	 * @param {*} value The value to convert.
	 * @returns {string} Returns the converted string.
	 * @example
	 *
	 * _.toString(null);
	 * // => ''
	 *
	 * _.toString(-0);
	 * // => '-0'
	 *
	 * _.toString([1, 2, 3]);
	 * // => '1,2,3'
	 */
	function toString(value) {
	  return value == null ? '' : baseToString(value);
	}

	module.exports = toString;


/***/ },
/* 100 */
/***/ function(module, exports, __webpack_require__) {

	var Symbol = __webpack_require__(58),
	    arrayMap = __webpack_require__(101),
	    isArray = __webpack_require__(54),
	    isSymbol = __webpack_require__(56);

	/** Used as references for various `Number` constants. */
	var INFINITY = 1 / 0;

	/** Used to convert symbols to primitives and strings. */
	var symbolProto = Symbol ? Symbol.prototype : undefined,
	    symbolToString = symbolProto ? symbolProto.toString : undefined;

	/**
	 * The base implementation of `_.toString` which doesn't convert nullish
	 * values to empty strings.
	 *
	 * @private
	 * @param {*} value The value to process.
	 * @returns {string} Returns the string.
	 */
	function baseToString(value) {
	  // Exit early for strings to avoid a performance hit in some environments.
	  if (typeof value == 'string') {
	    return value;
	  }
	  if (isArray(value)) {
	    // Recursively convert values (susceptible to call stack limits).
	    return arrayMap(value, baseToString) + '';
	  }
	  if (isSymbol(value)) {
	    return symbolToString ? symbolToString.call(value) : '';
	  }
	  var result = (value + '');
	  return (result == '0' && (1 / value) == -INFINITY) ? '-0' : result;
	}

	module.exports = baseToString;


/***/ },
/* 101 */
/***/ function(module, exports) {

	/**
	 * A specialized version of `_.map` for arrays without support for iteratee
	 * shorthands.
	 *
	 * @private
	 * @param {Array} [array] The array to iterate over.
	 * @param {Function} iteratee The function invoked per iteration.
	 * @returns {Array} Returns the new mapped array.
	 */
	function arrayMap(array, iteratee) {
	  var index = -1,
	      length = array == null ? 0 : array.length,
	      result = Array(length);

	  while (++index < length) {
	    result[index] = iteratee(array[index], index, array);
	  }
	  return result;
	}

	module.exports = arrayMap;


/***/ },
/* 102 */
/***/ function(module, exports, __webpack_require__) {

	var isSymbol = __webpack_require__(56);

	/** Used as references for various `Number` constants. */
	var INFINITY = 1 / 0;

	/**
	 * Converts `value` to a string key if it's not a string or symbol.
	 *
	 * @private
	 * @param {*} value The value to inspect.
	 * @returns {string|symbol} Returns the key.
	 */
	function toKey(value) {
	  if (typeof value == 'string' || isSymbol(value)) {
	    return value;
	  }
	  var result = (value + '');
	  return (result == '0' && (1 / value) == -INFINITY) ? '-0' : result;
	}

	module.exports = toKey;


/***/ }
/******/ ])
});
;