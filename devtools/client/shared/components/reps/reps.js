(function webpackUniversalModuleDefinition(root, factory) {
	if(typeof exports === 'object' && typeof module === 'object')
		module.exports = factory(require("devtools/client/shared/vendor/react"));
	else if(typeof define === 'function' && define.amd)
		define(["devtools/client/shared/vendor/react"], factory);
	else {
		var a = typeof exports === 'object' ? factory(require("devtools/client/shared/vendor/react")) : factory(root["devtools/client/shared/vendor/react"]);
		for(var i in a) (typeof exports === 'object' ? exports : root)[i] = a[i];
	}
})(this, function(__WEBPACK_EXTERNAL_MODULE_4__) {
return /******/ (function(modules) { // webpackBootstrap
/******/ 	// The module cache
/******/ 	var installedModules = {};
/******/
/******/ 	// The require function
/******/ 	function __webpack_require__(moduleId) {
/******/
/******/ 		// Check if module is in cache
/******/ 		if(installedModules[moduleId])
/******/ 			return installedModules[moduleId].exports;
/******/
/******/ 		// Create a new module (and put it into the cache)
/******/ 		var module = installedModules[moduleId] = {
/******/ 			exports: {},
/******/ 			id: moduleId,
/******/ 			loaded: false
/******/ 		};
/******/
/******/ 		// Execute the module function
/******/ 		modules[moduleId].call(module.exports, module, module.exports, __webpack_require__);
/******/
/******/ 		// Flag the module as loaded
/******/ 		module.loaded = true;
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
/******/ 	// __webpack_public_path__
/******/ 	__webpack_require__.p = "/assets/build";
/******/
/******/ 	// Load entry module and return exports
/******/ 	return __webpack_require__(0);
/******/ })
/************************************************************************/
/******/ ([
/* 0 */
/***/ function(module, exports, __webpack_require__) {

	const { MODE } = __webpack_require__(1);
	const { REPS, getRep } = __webpack_require__(2);
	const {
	  createFactories,
	  parseURLEncodedText,
	  parseURLParams,
	  getSelectableInInspectorGrips,
	  maybeEscapePropertyName
	} = __webpack_require__(3);
	
	module.exports = {
	  REPS,
	  getRep,
	  MODE,
	  createFactories,
	  maybeEscapePropertyName,
	  parseURLEncodedText,
	  parseURLParams,
	  getSelectableInInspectorGrips
	};

/***/ },
/* 1 */
/***/ function(module, exports) {

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

	const { isGrip } = __webpack_require__(3);
	
	// Load all existing rep templates
	const Undefined = __webpack_require__(6);
	const Null = __webpack_require__(7);
	const StringRep = __webpack_require__(8);
	const LongStringRep = __webpack_require__(9);
	const Number = __webpack_require__(10);
	const ArrayRep = __webpack_require__(11);
	const Obj = __webpack_require__(13);
	const SymbolRep = __webpack_require__(16);
	const InfinityRep = __webpack_require__(17);
	const NaNRep = __webpack_require__(18);
	
	// DOM types (grips)
	const Attribute = __webpack_require__(19);
	const DateTime = __webpack_require__(20);
	const Document = __webpack_require__(21);
	const Event = __webpack_require__(22);
	const Func = __webpack_require__(23);
	const PromiseRep = __webpack_require__(24);
	const RegExp = __webpack_require__(25);
	const StyleSheet = __webpack_require__(26);
	const CommentNode = __webpack_require__(27);
	const ElementNode = __webpack_require__(28);
	const TextNode = __webpack_require__(32);
	const ErrorRep = __webpack_require__(33);
	const Window = __webpack_require__(34);
	const ObjectWithText = __webpack_require__(35);
	const ObjectWithURL = __webpack_require__(36);
	const GripArray = __webpack_require__(37);
	const GripMap = __webpack_require__(38);
	const Grip = __webpack_require__(15);
	
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
	  let rep = getRep(object, defaultRep);
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
	 */
	function getRep(object, defaultRep = Obj) {
	  let type = typeof object;
	  if (type == "object" && object instanceof String) {
	    type = "string";
	  } else if (object && type == "object" && object.type) {
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
	      if (rep.supportsObject(object, type)) {
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
/***/ function(module, exports, __webpack_require__) {

	// Dependencies
	const React = __webpack_require__(4);
	
	// Utils
	const nodeConstants = __webpack_require__(5);
	
	/**
	 * Create React factories for given arguments.
	 * Example:
	 *   const { Rep } = createFactories(require("./rep"));
	 */
	function createFactories(args) {
	  let result = {};
	  for (let p in args) {
	    result[p] = React.createFactory(args[p]);
	  }
	  return result;
	}
	
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
	 * Get an array of all the items from the grip in parameter (including the grip itself)
	 * which can be selected in the inspector.
	 *
	 * @param {Object} Grip
	 * @return {Array} Flat array of the grips which can be selected in the inspector
	 */
	function getSelectableInInspectorGrips(grip) {
	  let grips = new Set(getFlattenedGrips([grip]));
	  return [...grips].filter(isGripSelectableInInspector);
	}
	
	/**
	 * Indicate if a Grip can be selected in the inspector,
	 * i.e. if it represents a node element.
	 *
	 * @param {Object} Grip
	 * @return {Boolean}
	 */
	function isGripSelectableInInspector(grip) {
	  return grip && typeof grip === "object" && grip.preview && [nodeConstants.TEXT_NODE, nodeConstants.ELEMENT_NODE].includes(grip.preview.nodeType);
	}
	
	/**
	 * Get a flat array of all the grips and their preview items.
	 *
	 * @param {Array} Grips
	 * @return {Array} Flat array of the grips and their preview items
	 */
	function getFlattenedGrips(grips) {
	  return grips.reduce((res, grip) => {
	    let previewItems = getGripPreviewItems(grip);
	    let flatPreviewItems = previewItems.length > 0 ? getFlattenedGrips(previewItems) : [];
	
	    return [...res, grip, ...flatPreviewItems];
	  }, []);
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
	    return [grip.preview.target];
	  }
	
	  // Generic Grip
	  if (grip.preview && grip.preview.ownProperties) {
	    let propertiesValues = Object.values(grip.preview.ownProperties).map(property => property.value || property);
	
	    // ArrayBuffer Grip
	    if (grip.preview.safeGetterValues) {
	      propertiesValues = propertiesValues.concat(Object.values(grip.preview.safeGetterValues).map(property => property.getterValue || property));
	    }
	
	    return propertiesValues;
	  }
	
	  return [];
	}
	
	/**
	 * Returns a new element wrapped with a component, props.objectLink if it exists,
	 * or a span if there are multiple childs, or directly the child if only one is passed.
	 *
	 * @param {Object} props A Rep "props" object that may contain `objectLink`
	 *                 and `object` properties.
	 * @param {Object} config Object to pass as props to the `objectLink` component.
	 * @param {...Element} children Elements to be wrapped with the `objectLink` component.
	 * @return {Element} Element, wrapped or not, depending if `objectLink`
	 *                   was supplied in props.
	 */
	function safeObjectLink(props, config, ...children) {
	  const {
	    objectLink,
	    object
	  } = props;
	
	  if (objectLink) {
	    return objectLink(Object.assign({
	      object
	    }, config), ...children);
	  }
	
	  if ((!config || Object.keys(config).length === 0) && children.length === 1) {
	    return children[0];
	  }
	
	  return React.DOM.span(config, ...children);
	}
	
	module.exports = {
	  createFactories,
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
	  getSelectableInInspectorGrips,
	  maybeEscapePropertyName,
	  safeObjectLink
	};

/***/ },
/* 4 */
/***/ function(module, exports) {

	module.exports = __WEBPACK_EXTERNAL_MODULE_4__;

/***/ },
/* 5 */
/***/ function(module, exports) {

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
/* 6 */
/***/ function(module, exports, __webpack_require__) {

	// Dependencies
	const React = __webpack_require__(4);
	
	const { wrapRender } = __webpack_require__(3);
	
	// Shortcuts
	const { span } = React.DOM;
	
	/**
	 * Renders undefined value
	 */
	const Undefined = function () {
	  return span({ className: "objectBox objectBox-undefined" }, "undefined");
	};
	
	function supportsObject(object, type) {
	  if (object && object.type && object.type == "undefined") {
	    return true;
	  }
	
	  return type == "undefined";
	}
	
	// Exports from this module
	
	module.exports = {
	  rep: wrapRender(Undefined),
	  supportsObject
	};

/***/ },
/* 7 */
/***/ function(module, exports, __webpack_require__) {

	// Dependencies
	const React = __webpack_require__(4);
	
	const { wrapRender } = __webpack_require__(3);
	
	// Shortcuts
	const { span } = React.DOM;
	
	/**
	 * Renders null value
	 */
	function Null(props) {
	  return span({ className: "objectBox objectBox-null" }, "null");
	}
	
	function supportsObject(object, type) {
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
/* 8 */
/***/ function(module, exports, __webpack_require__) {

	// Dependencies
	const React = __webpack_require__(4);
	
	const {
	  escapeString,
	  rawCropString,
	  sanitizeString,
	  wrapRender
	} = __webpack_require__(3);
	
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
/* 9 */
/***/ function(module, exports, __webpack_require__) {

	// Dependencies
	const React = __webpack_require__(4);
	const {
	  escapeString,
	  sanitizeString,
	  isGrip,
	  wrapRender
	} = __webpack_require__(3);
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
	
	  let config = { className: "objectBox objectBox-string" };
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
	
	function supportsObject(object, type) {
	  if (!isGrip(object)) {
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
/* 10 */
/***/ function(module, exports, __webpack_require__) {

	// Dependencies
	const React = __webpack_require__(4);
	
	const { wrapRender } = __webpack_require__(3);
	
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
/* 11 */
/***/ function(module, exports, __webpack_require__) {

	// Dependencies
	const React = __webpack_require__(4);
	const {
	  safeObjectLink,
	  wrapRender
	} = __webpack_require__(3);
	const Caption = __webpack_require__(12);
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
	  objectLink: React.PropTypes.func,
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
	    let max = mode === MODE.SHORT ? 3 : 10;
	    items = arrayIterator(props, object, max);
	    brackets = needSpace(items.length > 0);
	  }
	
	  return DOM.span({
	    className: "objectBox objectBox-array" }, safeObjectLink(props, {
	    className: "arrayLeftBracket",
	    object: object
	  }, brackets.left), ...items, safeObjectLink(props, {
	    className: "arrayRightBracket",
	    object: object
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
	      object: safeObjectLink(props, {
	        object: props.object
	      }, array.length - max + " more…")
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
	
	// Exports from this module
	module.exports = {
	  rep: wrapRender(ArrayRep),
	  supportsObject
	};

/***/ },
/* 12 */
/***/ function(module, exports, __webpack_require__) {

	// Dependencies
	const React = __webpack_require__(4);
	const DOM = React.DOM;
	
	const { wrapRender } = __webpack_require__(3);
	
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
/* 13 */
/***/ function(module, exports, __webpack_require__) {

	// Dependencies
	const React = __webpack_require__(4);
	const {
	  safeObjectLink,
	  wrapRender
	} = __webpack_require__(3);
	const Caption = __webpack_require__(12);
	const PropRep = __webpack_require__(14);
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
	  objectLink: React.PropTypes.func,
	  title: React.PropTypes.string
	};
	
	function ObjectRep(props) {
	  let object = props.object;
	  let propsArray = safePropIterator(props, object);
	
	  if (props.mode === MODE.TINY || !propsArray.length) {
	    return span({ className: "objectBox objectBox-object" }, getTitle(props, object));
	  }
	
	  return span({ className: "objectBox objectBox-object" }, getTitle(props, object), safeObjectLink(props, {
	    className: "objectLeftBrace"
	  }, " { "), ...propsArray, safeObjectLink(props, {
	    className: "objectRightBrace"
	  }, " }"));
	}
	
	function getTitle(props, object) {
	  let title = props.title || object.class || "Object";
	  return safeObjectLink(props, { className: "objectTitle" }, title);
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
	
	  const truncated = Object.keys(object).length > max;
	  let propsArray = getPropsArray(interestingObject, truncated);
	  if (truncated) {
	    propsArray.push(Caption({
	      object: safeObjectLink(props, {}, Object.keys(object).length - max + " more…")
	    }));
	  }
	
	  return propsArray;
	}
	
	/**
	 * Get an array of components representing the properties of the object
	 *
	 * @param {Object} object
	 * @param {Boolean} truncated true if the object is truncated.
	 * @return {Array} Array of PropRep.
	 */
	function getPropsArray(object, truncated) {
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
	    equal: ": ",
	    delim: i !== objectKeys.length - 1 || truncated ? ", " : null
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
/* 14 */
/***/ function(module, exports, __webpack_require__) {

	// Dependencies
	const React = __webpack_require__(4);
	const {
	  maybeEscapePropertyName,
	  wrapRender
	} = __webpack_require__(3);
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
	  // Delimiter character used to separate individual properties.
	  delim: React.PropTypes.string,
	  // @TODO Change this to Object.values once it's supported in Node's version of V8
	  mode: React.PropTypes.oneOf(Object.keys(MODE).map(key => MODE[key])),
	  objectLink: React.PropTypes.func,
	  onDOMNodeMouseOver: React.PropTypes.func,
	  onDOMNodeMouseOut: React.PropTypes.func,
	  onInspectIconClick: React.PropTypes.func,
	  // Normally a PropRep will quote a property name that isn't valid
	  // when unquoted; but this flag can be used to suppress the
	  // quoting.
	  suppressQuotes: React.PropTypes.bool
	};
	
	function PropRep(props) {
	  const Grip = __webpack_require__(15);
	  const { Rep } = __webpack_require__(2);
	
	  let {
	    name,
	    mode,
	    equal,
	    delim,
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
	
	  let delimElement;
	  if (delim) {
	    delimElement = span({
	      "className": "objectComma"
	    }, delim);
	  }
	
	  return span({}, key, span({
	    "className": "objectEqual"
	  }, equal), Rep(Object.assign({}, props)), delimElement);
	}
	
	// Exports from this module
	module.exports = wrapRender(PropRep);

/***/ },
/* 15 */
/***/ function(module, exports, __webpack_require__) {

	// ReactJS
	const React = __webpack_require__(4);
	// Dependencies
	const {
	  isGrip,
	  safeObjectLink,
	  wrapRender
	} = __webpack_require__(3);
	const Caption = __webpack_require__(12);
	const PropRep = __webpack_require__(14);
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
	  objectLink: React.PropTypes.func,
	  onDOMNodeMouseOver: React.PropTypes.func,
	  onDOMNodeMouseOut: React.PropTypes.func,
	  onInspectIconClick: React.PropTypes.func
	};
	
	function GripRep(props) {
	  let object = props.object;
	  let propsArray = safePropIterator(props, object, props.mode === MODE.LONG ? 10 : 3);
	
	  if (props.mode === MODE.TINY) {
	    return span({ className: "objectBox objectBox-object" }, getTitle(props, object));
	  }
	
	  return span({ className: "objectBox objectBox-object" }, getTitle(props, object), safeObjectLink(props, {
	    className: "objectLeftBrace"
	  }, " { "), ...propsArray, safeObjectLink(props, {
	    className: "objectRightBrace"
	  }, " }"));
	}
	
	function getTitle(props, object) {
	  let title = props.title || object.class || "Object";
	  return safeObjectLink(props, {}, title);
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
	
	  const truncate = Object.keys(properties).length > max;
	  // The server synthesizes some property names for a Proxy, like
	  // <target> and <handler>; we don't want to quote these because,
	  // as synthetic properties, they appear more natural when
	  // unquoted.
	  const suppressQuotes = object.class === "Proxy";
	  let propsArray = getProps(props, properties, indexes, truncate, suppressQuotes);
	  if (truncate) {
	    // There are some undisplayed props. Then display "more...".
	    propsArray.push(Caption({
	      object: safeObjectLink(props, {}, `${propertiesLength - max} more…`)
	    }));
	  }
	
	  return propsArray;
	}
	
	/**
	 * Get props ordered by index.
	 *
	 * @param {Object} componentProps Grip Component props.
	 * @param {Object} properties Properties of the object the Grip describes.
	 * @param {Array} indexes Indexes of properties.
	 * @param {Boolean} truncate true if the grip will be truncated.
	 * @param {Boolean} suppressQuotes true if we should suppress quotes
	 *                  on property names.
	 * @return {Array} Props.
	 */
	function getProps(componentProps, properties, indexes, truncate, suppressQuotes) {
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
	      delim: i !== indexes.length - 1 || truncate ? ", " : null,
	      defaultRep: Grip,
	      // Do not propagate title and objectLink to properties reps
	      title: null,
	      objectLink: null,
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
	function supportsObject(object, type) {
	  if (!isGrip(object)) {
	    return false;
	  }
	  return object.preview && object.preview.ownProperties;
	}
	
	// Grip is used in propIterator and has to be defined here.
	let Grip = {
	  rep: wrapRender(GripRep),
	  supportsObject
	};
	
	// Exports from this module
	module.exports = Grip;

/***/ },
/* 16 */
/***/ function(module, exports, __webpack_require__) {

	// Dependencies
	const React = __webpack_require__(4);
	
	const { wrapRender } = __webpack_require__(3);
	
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
/* 17 */
/***/ function(module, exports, __webpack_require__) {

	// Dependencies
	const React = __webpack_require__(4);
	
	const { wrapRender } = __webpack_require__(3);
	
	// Shortcuts
	const { span } = React.DOM;
	
	/**
	 * Renders a Infinity object
	 */
	InfinityRep.propTypes = {
	  object: React.PropTypes.object.isRequired
	};
	
	function InfinityRep(props) {
	  return span({ className: "objectBox objectBox-number" }, props.object.type);
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
/* 18 */
/***/ function(module, exports, __webpack_require__) {

	// Dependencies
	const React = __webpack_require__(4);
	
	const { wrapRender } = __webpack_require__(3);
	
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
/* 19 */
/***/ function(module, exports, __webpack_require__) {

	// ReactJS
	const React = __webpack_require__(4);
	
	// Reps
	const {
	  isGrip,
	  safeObjectLink,
	  wrapRender
	} = __webpack_require__(3);
	const { rep: StringRep } = __webpack_require__(8);
	
	// Shortcuts
	const { span } = React.DOM;
	
	/**
	 * Renders DOM attribute
	 */
	Attribute.propTypes = {
	  object: React.PropTypes.object.isRequired,
	  objectLink: React.PropTypes.func
	};
	
	function Attribute(props) {
	  let {
	    object
	  } = props;
	  let value = object.preview.value;
	
	  return safeObjectLink(props, { className: "objectLink-Attr" }, span({ className: "attrTitle" }, getTitle(object)), span({ className: "attrEqual" }, "="), StringRep({ object: value }));
	}
	
	function getTitle(grip) {
	  return grip.preview.nodeName;
	}
	
	// Registration
	function supportsObject(grip, type) {
	  if (!isGrip(grip)) {
	    return false;
	  }
	
	  return type == "Attr" && grip.preview;
	}
	
	module.exports = {
	  rep: wrapRender(Attribute),
	  supportsObject
	};

/***/ },
/* 20 */
/***/ function(module, exports, __webpack_require__) {

	// ReactJS
	const React = __webpack_require__(4);
	
	// Reps
	const {
	  isGrip,
	  safeObjectLink,
	  wrapRender
	} = __webpack_require__(3);
	
	// Shortcuts
	const { span } = React.DOM;
	
	/**
	 * Used to render JS built-in Date() object.
	 */
	DateTime.propTypes = {
	  object: React.PropTypes.object.isRequired,
	  objectLink: React.PropTypes.func
	};
	
	function DateTime(props) {
	  let grip = props.object;
	  let date;
	  try {
	    date = span({ className: "objectBox" }, getTitle(props, grip), span({ className: "Date" }, new Date(grip.preview.timestamp).toISOString()));
	  } catch (e) {
	    date = span({ className: "objectBox" }, "Invalid Date");
	  }
	
	  return date;
	}
	
	function getTitle(props, grip) {
	  return safeObjectLink(props, {}, grip.class + " ");
	}
	
	// Registration
	function supportsObject(grip, type) {
	  if (!isGrip(grip)) {
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
/* 21 */
/***/ function(module, exports, __webpack_require__) {

	// ReactJS
	const React = __webpack_require__(4);
	
	// Reps
	const {
	  isGrip,
	  getURLDisplayString,
	  safeObjectLink,
	  wrapRender
	} = __webpack_require__(3);
	
	// Shortcuts
	const { span } = React.DOM;
	
	/**
	 * Renders DOM document object.
	 */
	Document.propTypes = {
	  object: React.PropTypes.object.isRequired,
	  objectLink: React.PropTypes.func
	};
	
	function Document(props) {
	  let grip = props.object;
	
	  return span({ className: "objectBox objectBox-object" }, getTitle(props, grip), span({ className: "objectPropValue" }, getLocation(grip)));
	}
	
	function getLocation(grip) {
	  let location = grip.preview.location;
	  return location ? getURLDisplayString(location) : "";
	}
	
	function getTitle(props, grip) {
	  return safeObjectLink(props, {}, grip.class + " ");
	}
	
	// Registration
	function supportsObject(object, type) {
	  if (!isGrip(object)) {
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
/* 22 */
/***/ function(module, exports, __webpack_require__) {

	// ReactJS
	const React = __webpack_require__(4);
	
	// Reps
	const {
	  isGrip,
	  wrapRender
	} = __webpack_require__(3);
	
	const { MODE } = __webpack_require__(1);
	const { rep } = __webpack_require__(15);
	
	/**
	 * Renders DOM event objects.
	 */
	Event.propTypes = {
	  object: React.PropTypes.object.isRequired,
	  objectLink: React.PropTypes.func,
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
	function supportsObject(grip, type) {
	  if (!isGrip(grip)) {
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
/* 23 */
/***/ function(module, exports, __webpack_require__) {

	// ReactJS
	const React = __webpack_require__(4);
	
	// Reps
	const {
	  isGrip,
	  cropString,
	  safeObjectLink,
	  wrapRender
	} = __webpack_require__(3);
	
	// Shortcuts
	const { span } = React.DOM;
	
	/**
	 * This component represents a template for Function objects.
	 */
	FunctionRep.propTypes = {
	  object: React.PropTypes.object.isRequired,
	  objectLink: React.PropTypes.func
	};
	
	function FunctionRep(props) {
	  let grip = props.object;
	
	  return (
	    // Set dir="ltr" to prevent function parentheses from
	    // appearing in the wrong direction
	    span({ dir: "ltr", className: "objectBox objectBox-function" }, getTitle(props, grip), summarizeFunction(grip))
	  );
	}
	
	function getTitle(props, grip) {
	  let title = "function ";
	  if (grip.isGenerator) {
	    title = "function* ";
	  }
	  if (grip.isAsync) {
	    title = "async " + title;
	  }
	
	  return safeObjectLink(props, {}, title);
	}
	
	function summarizeFunction(grip) {
	  let name = grip.userDisplayName || grip.displayName || grip.name || "";
	  return cropString(name + "()", 100);
	}
	
	// Registration
	function supportsObject(grip, type) {
	  if (!isGrip(grip)) {
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
/* 24 */
/***/ function(module, exports, __webpack_require__) {

	// ReactJS
	const React = __webpack_require__(4);
	// Dependencies
	const {
	  isGrip,
	  safeObjectLink,
	  wrapRender
	} = __webpack_require__(3);
	
	const PropRep = __webpack_require__(14);
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
	  objectLink: React.PropTypes.func,
	  onDOMNodeMouseOver: React.PropTypes.func,
	  onDOMNodeMouseOut: React.PropTypes.func,
	  onInspectIconClick: React.PropTypes.func
	};
	
	function PromiseRep(props) {
	  const object = props.object;
	  const { promiseState } = object;
	
	  if (props.mode === MODE.TINY) {
	    let { Rep } = __webpack_require__(2);
	
	    return span({ className: "objectBox objectBox-object" }, getTitle(props, object), safeObjectLink(props, {
	      className: "objectLeftBrace"
	    }, " { "), Rep({ object: promiseState.state }), safeObjectLink(props, {
	      className: "objectRightBrace"
	    }, " }"));
	  }
	
	  const propsArray = getProps(props, promiseState);
	  return span({ className: "objectBox objectBox-object" }, getTitle(props, object), safeObjectLink(props, {
	    className: "objectLeftBrace"
	  }, " { "), ...propsArray, safeObjectLink(props, {
	    className: "objectRightBrace"
	  }, " }"));
	}
	
	function getTitle(props, object) {
	  const title = object.class;
	  return safeObjectLink(props, {}, title);
	}
	
	function getProps(props, promiseState) {
	  const keys = ["state"];
	  if (Object.keys(promiseState).includes("value")) {
	    keys.push("value");
	  }
	
	  return keys.map((key, i) => {
	    let object = promiseState[key];
	    return PropRep(Object.assign({}, props, {
	      mode: MODE.TINY,
	      name: `<${key}>`,
	      object,
	      equal: ": ",
	      delim: i < keys.length - 1 ? ", " : null,
	      suppressQuotes: true
	    }));
	  });
	}
	
	// Registration
	function supportsObject(object, type) {
	  if (!isGrip(object)) {
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
/* 25 */
/***/ function(module, exports, __webpack_require__) {

	// ReactJS
	const React = __webpack_require__(4);
	
	// Reps
	const {
	  isGrip,
	  safeObjectLink,
	  wrapRender
	} = __webpack_require__(3);
	
	/**
	 * Renders a grip object with regular expression.
	 */
	RegExp.propTypes = {
	  object: React.PropTypes.object.isRequired,
	  objectLink: React.PropTypes.func
	};
	
	function RegExp(props) {
	  let { object } = props;
	
	  return safeObjectLink(props, {
	    className: "objectBox objectBox-regexp regexpSource"
	  }, getSource(object));
	}
	
	function getSource(grip) {
	  return grip.displayString;
	}
	
	// Registration
	function supportsObject(object, type) {
	  if (!isGrip(object)) {
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
/* 26 */
/***/ function(module, exports, __webpack_require__) {

	// ReactJS
	const React = __webpack_require__(4);
	
	// Reps
	const {
	  isGrip,
	  getURLDisplayString,
	  safeObjectLink,
	  wrapRender
	} = __webpack_require__(3);
	
	// Shortcuts
	const { span } = React.DOM;
	
	/**
	 * Renders a grip representing CSSStyleSheet
	 */
	StyleSheet.propTypes = {
	  object: React.PropTypes.object.isRequired,
	  objectLink: React.PropTypes.func
	};
	
	function StyleSheet(props) {
	  let grip = props.object;
	
	  return span({ className: "objectBox objectBox-object" }, getTitle(props, grip), span({ className: "objectPropValue" }, getLocation(grip)));
	}
	
	function getTitle(props, grip) {
	  let title = "StyleSheet ";
	  return safeObjectLink(props, { className: "objectBox" }, title);
	}
	
	function getLocation(grip) {
	  // Embedded stylesheets don't have URL and so, no preview.
	  let url = grip.preview ? grip.preview.url : "";
	  return url ? getURLDisplayString(url) : "";
	}
	
	// Registration
	function supportsObject(object, type) {
	  if (!isGrip(object)) {
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
/* 27 */
/***/ function(module, exports, __webpack_require__) {

	// Dependencies
	const React = __webpack_require__(4);
	const {
	  isGrip,
	  cropString,
	  cropMultipleLines,
	  wrapRender
	} = __webpack_require__(3);
	const { MODE } = __webpack_require__(1);
	const nodeConstants = __webpack_require__(5);
	
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
	
	  return span({ className: "objectBox theme-comment" }, `<!-- ${textContent} -->`);
	}
	
	// Registration
	function supportsObject(object, type) {
	  if (!isGrip(object)) {
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
/* 28 */
/***/ function(module, exports, __webpack_require__) {

	// ReactJS
	const React = __webpack_require__(4);
	
	// Utils
	const {
	  isGrip,
	  safeObjectLink,
	  wrapRender
	} = __webpack_require__(3);
	const { MODE } = __webpack_require__(1);
	const nodeConstants = __webpack_require__(5);
	const Svg = __webpack_require__(29);
	
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
	  onInspectIconClick: React.PropTypes.func,
	  objectLink: React.PropTypes.func
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
	
	  let baseConfig = { className: "objectBox objectBox-node" };
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
	
	  return span(baseConfig, safeObjectLink(props, {}, ...elements), inspectIcon);
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
	  let attributeElements = Object.keys(attributes).sort(function getIdAndClassFirst(a1, a2) {
	    if ([a1, a2].includes("id")) {
	      return 3 * (a1 === "id" ? -1 : 1);
	    }
	    if ([a1, a2].includes("class")) {
	      return 2 * (a1 === "class" ? -1 : 1);
	    }
	
	    // `id` and `class` excepted,
	    // we want to keep the same order that in `attributes`.
	    return 0;
	  }).reduce((arr, name, i, keys) => {
	    let value = attributes[name];
	    let attribute = span({}, span({ className: "attr-name theme-fg-color2" }, `${name}`), `="`, span({ className: "attr-value theme-fg-color6" }, `${value}`), `"`);
	
	    return arr.concat([" ", attribute]);
	  }, []);
	
	  return ["<", nodeNameElement, ...attributeElements, ">"];
	}
	
	// Registration
	function supportsObject(object, type) {
	  if (!isGrip(object)) {
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
/* 29 */
/***/ function(module, exports, __webpack_require__) {

	const React = __webpack_require__(4);
	const InlineSVG = __webpack_require__(30);
	
	const svg = {
	  "open-inspector": __webpack_require__(31)
	};
	
	module.exports = function (name, props) {
	  // eslint-disable-line
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
	};

/***/ },
/* 30 */
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
	
	var _react = __webpack_require__(4);
	
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
/* 31 */
/***/ function(module, exports) {

	module.exports = "<!-- This Source Code Form is subject to the terms of the Mozilla Public - License, v. 2.0. If a copy of the MPL was not distributed with this - file, You can obtain one at http://mozilla.org/MPL/2.0/. --><svg viewBox=\"0 0 16 16\" xmlns=\"http://www.w3.org/2000/svg\"><path d=\"M8,3L12,3L12,7L14,7L14,8L12,8L12,12L8,12L8,14L7,14L7,12L3,12L3,8L1,8L1,7L3,7L3,3L7,3L7,1L8,1L8,3ZM10,10L10,5L5,5L5,10L10,10Z\"></path></svg>"

/***/ },
/* 32 */
/***/ function(module, exports, __webpack_require__) {

	// ReactJS
	const React = __webpack_require__(4);
	
	// Reps
	const {
	  isGrip,
	  cropString,
	  safeObjectLink,
	  wrapRender
	} = __webpack_require__(3);
	const { MODE } = __webpack_require__(1);
	const Svg = __webpack_require__(29);
	
	// Shortcuts
	const DOM = React.DOM;
	
	/**
	 * Renders DOM #text node.
	 */
	TextNode.propTypes = {
	  object: React.PropTypes.object.isRequired,
	  // @TODO Change this to Object.values once it's supported in Node's version of V8
	  mode: React.PropTypes.oneOf(Object.keys(MODE).map(key => MODE[key])),
	  objectLink: React.PropTypes.func,
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
	
	  let baseConfig = { className: "objectBox objectBox-textNode" };
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
	    return DOM.span(baseConfig, getTitle(props, grip), inspectIcon);
	  }
	
	  return DOM.span(baseConfig, getTitle(props, grip), DOM.span({ className: "nodeValue" }, " ", `"${getTextContent(grip)}"`), inspectIcon);
	}
	
	function getTextContent(grip) {
	  return cropString(grip.preview.textContent);
	}
	
	function getTitle(props, grip) {
	  const title = "#text";
	  return safeObjectLink(props, {}, title);
	}
	
	// Registration
	function supportsObject(grip, type) {
	  if (!isGrip(grip)) {
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
/* 33 */
/***/ function(module, exports, __webpack_require__) {

	// ReactJS
	const React = __webpack_require__(4);
	// Utils
	const {
	  isGrip,
	  safeObjectLink,
	  wrapRender
	} = __webpack_require__(3);
	const { MODE } = __webpack_require__(1);
	
	/**
	 * Renders Error objects.
	 */
	ErrorRep.propTypes = {
	  object: React.PropTypes.object.isRequired,
	  // @TODO Change this to Object.values once it's supported in Node's version of V8
	  mode: React.PropTypes.oneOf(Object.keys(MODE).map(key => MODE[key])),
	  objectLink: React.PropTypes.func
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
	
	  return safeObjectLink(props, { className: "objectBox-stackTrace" }, content);
	}
	
	// Registration
	function supportsObject(object, type) {
	  if (!isGrip(object)) {
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
/* 34 */
/***/ function(module, exports, __webpack_require__) {

	// ReactJS
	const React = __webpack_require__(4);
	
	// Reps
	const {
	  isGrip,
	  getURLDisplayString,
	  safeObjectLink,
	  wrapRender
	} = __webpack_require__(3);
	
	const { MODE } = __webpack_require__(1);
	
	// Shortcuts
	const { span } = React.DOM;
	
	/**
	 * Renders a grip representing a window.
	 */
	WindowRep.propTypes = {
	  // @TODO Change this to Object.values once it's supported in Node's version of V8
	  mode: React.PropTypes.oneOf(Object.keys(MODE).map(key => MODE[key])),
	  object: React.PropTypes.object.isRequired,
	  objectLink: React.PropTypes.func
	};
	
	function WindowRep(props) {
	  let {
	    mode,
	    object
	  } = props;
	
	  if (mode === MODE.TINY) {
	    return span({ className: "objectBox objectBox-Window" }, getTitle(props, object));
	  }
	
	  return span({ className: "objectBox objectBox-Window" }, getTitle(props, object), " ", span({ className: "objectPropValue" }, getLocation(object)));
	}
	
	function getTitle(props, object) {
	  let title = object.displayClass || object.class || "Window";
	  return safeObjectLink(props, { className: "objectBox" }, title);
	}
	
	function getLocation(object) {
	  return getURLDisplayString(object.preview.url);
	}
	
	// Registration
	function supportsObject(object, type) {
	  if (!isGrip(object)) {
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
/* 35 */
/***/ function(module, exports, __webpack_require__) {

	// ReactJS
	const React = __webpack_require__(4);
	
	// Reps
	const {
	  isGrip,
	  wrapRender
	} = __webpack_require__(3);
	
	// Shortcuts
	const { span } = React.DOM;
	
	/**
	 * Renders a grip object with textual data.
	 */
	ObjectWithText.propTypes = {
	  object: React.PropTypes.object.isRequired,
	  objectLink: React.PropTypes.func
	};
	
	function ObjectWithText(props) {
	  let grip = props.object;
	  return span({ className: "objectBox objectBox-" + getType(grip) }, getTitle(props, grip), span({ className: "objectPropValue" }, getDescription(grip)));
	}
	
	function getTitle(props, grip) {
	  if (props.objectLink) {
	    return span({ className: "objectBox" }, props.objectLink({
	      object: grip
	    }, getType(grip) + " "));
	  }
	  return "";
	}
	
	function getType(grip) {
	  return grip.class;
	}
	
	function getDescription(grip) {
	  return "\"" + grip.preview.text + "\"";
	}
	
	// Registration
	function supportsObject(grip, type) {
	  if (!isGrip(grip)) {
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
/* 36 */
/***/ function(module, exports, __webpack_require__) {

	// ReactJS
	const React = __webpack_require__(4);
	
	// Reps
	const {
	  isGrip,
	  getURLDisplayString,
	  safeObjectLink,
	  wrapRender
	} = __webpack_require__(3);
	
	// Shortcuts
	const { span } = React.DOM;
	
	/**
	 * Renders a grip object with URL data.
	 */
	ObjectWithURL.propTypes = {
	  object: React.PropTypes.object.isRequired,
	  objectLink: React.PropTypes.func
	};
	
	function ObjectWithURL(props) {
	  let grip = props.object;
	  return span({ className: "objectBox objectBox-" + getType(grip) }, getTitle(props, grip), span({ className: "objectPropValue" }, getDescription(grip)));
	}
	
	function getTitle(props, grip) {
	  return safeObjectLink(props, { className: "objectBox" }, getType(grip) + " ");
	}
	
	function getType(grip) {
	  return grip.class;
	}
	
	function getDescription(grip) {
	  return getURLDisplayString(grip.preview.url);
	}
	
	// Registration
	function supportsObject(grip, type) {
	  if (!isGrip(grip)) {
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
/* 37 */
/***/ function(module, exports, __webpack_require__) {

	// Dependencies
	const React = __webpack_require__(4);
	const {
	  isGrip,
	  safeObjectLink,
	  wrapRender
	} = __webpack_require__(3);
	const Caption = __webpack_require__(12);
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
	  objectLink: React.PropTypes.func,
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
	    items = [span({ className: "length" }, isEmpty ? "" : objectLength)];
	    brackets = needSpace(false);
	  } else {
	    let max = mode === MODE.SHORT ? 3 : 10;
	    items = arrayIterator(props, object, max);
	    brackets = needSpace(items.length > 0);
	  }
	
	  let title = getTitle(props, object);
	
	  return span({
	    className: "objectBox objectBox-array" }, title, safeObjectLink(props, {
	    className: "arrayLeftBracket"
	  }, brackets.left), ...items, safeObjectLink(props, {
	    className: "arrayRightBracket"
	  }, brackets.right), span({
	    className: "arrayProperties",
	    role: "group" }));
	}
	
	/**
	 * Renders array item. Individual values are separated by
	 * a delimiter (a comma by default).
	 */
	GripArrayItem.propTypes = {
	  delim: React.PropTypes.string,
	  object: React.PropTypes.oneOfType([React.PropTypes.object, React.PropTypes.number, React.PropTypes.string]).isRequired,
	  objectLink: React.PropTypes.func,
	  // @TODO Change this to Object.values once it's supported in Node's version of V8
	  mode: React.PropTypes.oneOf(Object.keys(MODE).map(key => MODE[key])),
	  provider: React.PropTypes.object,
	  onDOMNodeMouseOver: React.PropTypes.func,
	  onDOMNodeMouseOut: React.PropTypes.func,
	  onInspectIconClick: React.PropTypes.func
	};
	
	function GripArrayItem(props) {
	  let { Rep } = __webpack_require__(2);
	  let {
	    delim
	  } = props;
	
	  return span({}, Rep(Object.assign({}, props, {
	    mode: MODE.TINY
	  })), delim);
	}
	
	function getLength(grip) {
	  if (!grip.preview) {
	    return 0;
	  }
	
	  return grip.preview.length || grip.preview.childNodesLength || 0;
	}
	
	function getTitle(props, object, context) {
	  if (props.mode === MODE.TINY) {
	    return "";
	  }
	
	  let title = props.title || object.class || "Array";
	  return safeObjectLink(props, {}, title + " ");
	}
	
	function getPreviewItems(grip) {
	  if (!grip.preview) {
	    return null;
	  }
	
	  return grip.preview.items || grip.preview.childNodes || null;
	}
	
	function arrayIterator(props, grip, max) {
	  let items = [];
	  const gripLength = getLength(grip);
	
	  if (!gripLength) {
	    return items;
	  }
	
	  const previewItems = getPreviewItems(grip);
	  if (!previewItems) {
	    return items;
	  }
	
	  let delim;
	  // number of grip preview items is limited to 10, but we may have more
	  // items in grip-array.
	  let delimMax = gripLength > previewItems.length ? previewItems.length : previewItems.length - 1;
	  let provider = props.provider;
	
	  for (let i = 0; i < previewItems.length && i < max; i++) {
	    try {
	      let itemGrip = previewItems[i];
	      let value = provider ? provider.getValue(itemGrip) : itemGrip;
	
	      delim = i == delimMax ? "" : ", ";
	
	      items.push(GripArrayItem(Object.assign({}, props, {
	        object: value,
	        delim: delim,
	        // Do not propagate title to array items reps
	        title: undefined
	      })));
	    } catch (exc) {
	      items.push(GripArrayItem(Object.assign({}, props, {
	        object: exc,
	        delim: delim,
	        // Do not propagate title to array items reps
	        title: undefined
	      })));
	    }
	  }
	  if (previewItems.length > max || gripLength > previewItems.length) {
	    let leftItemNum = gripLength - max > 0 ? gripLength - max : gripLength - previewItems.length;
	    items.push(Caption({
	      object: safeObjectLink(props, {}, leftItemNum + " more…")
	    }));
	  }
	
	  return items;
	}
	
	function supportsObject(grip, type) {
	  if (!isGrip(grip)) {
	    return false;
	  }
	
	  return grip.preview && (grip.preview.kind == "ArrayLike" || type === "DocumentFragment");
	}
	
	// Exports from this module
	module.exports = {
	  rep: wrapRender(GripArray),
	  supportsObject
	};

/***/ },
/* 38 */
/***/ function(module, exports, __webpack_require__) {

	// Dependencies
	const React = __webpack_require__(4);
	const {
	  isGrip,
	  safeObjectLink,
	  wrapRender
	} = __webpack_require__(3);
	const Caption = __webpack_require__(12);
	const PropRep = __webpack_require__(14);
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
	  objectLink: React.PropTypes.func,
	  isInterestingEntry: React.PropTypes.func,
	  onDOMNodeMouseOver: React.PropTypes.func,
	  onDOMNodeMouseOut: React.PropTypes.func,
	  onInspectIconClick: React.PropTypes.func,
	  title: React.PropTypes.string
	};
	
	function GripMap(props) {
	  let object = props.object;
	  let propsArray = safeEntriesIterator(props, object, props.mode === MODE.LONG ? 10 : 3);
	
	  if (props.mode === MODE.TINY) {
	    return span({ className: "objectBox objectBox-object" }, getTitle(props, object));
	  }
	
	  return span({ className: "objectBox objectBox-object" }, getTitle(props, object), safeObjectLink(props, {
	    className: "objectLeftBrace"
	  }, " { "), propsArray, safeObjectLink(props, {
	    className: "objectRightBrace"
	  }, " }"));
	}
	
	function getTitle(props, object) {
	  let title = props.title || (object && object.class ? object.class : "Map");
	  return safeObjectLink(props, {}, title);
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
	      object: safeObjectLink(props, {}, `${mapEntries.length - max} more…`)
	    }));
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
	  let {
	    objectLink,
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
	      // key,
	      name: key,
	      equal: ": ",
	      object: value,
	      // Do not add a trailing comma on the last entry
	      // if there won't be a "more..." item.
	      delim: i < indexes.length - 1 || indexes.length < entries.length ? ", " : null,
	      mode: MODE.TINY,
	      objectLink,
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
	
	function supportsObject(grip, type) {
	  if (!isGrip(grip)) {
	    return false;
	  }
	  return grip.preview && grip.preview.kind == "MapLike";
	}
	
	// Exports from this module
	module.exports = {
	  rep: wrapRender(GripMap),
	  supportsObject
	};

/***/ }
/******/ ])
});
;
//# sourceMappingURL=reps.js.map