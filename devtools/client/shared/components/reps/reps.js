(function webpackUniversalModuleDefinition(root, factory) {
	if(typeof exports === 'object' && typeof module === 'object')
		module.exports = factory(require("devtools/client/shared/vendor/react"));
	else if(typeof define === 'function' && define.amd)
		define(["devtools/client/shared/vendor/react"], factory);
	else {
		var a = typeof exports === 'object' ? factory(require("devtools/client/shared/vendor/react")) : factory(root["devtools/client/shared/vendor/react"]);
		for(var i in a) (typeof exports === 'object' ? exports : root)[i] = a[i];
	}
})(this, function(__WEBPACK_EXTERNAL_MODULE_1__) {
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

	const React = __webpack_require__(1);
	
	const { MODE } = __webpack_require__(2);
	const { REPS } = __webpack_require__(3);
	const { createFactories, parseURLEncodedText, parseURLParams } = __webpack_require__(4);
	
	module.exports = {
	  REPS,
	  MODE,
	  createFactories,
	  parseURLEncodedText,
	  parseURLParams
	};

/***/ },
/* 1 */
/***/ function(module, exports) {

	module.exports = __WEBPACK_EXTERNAL_MODULE_1__;

/***/ },
/* 2 */
/***/ function(module, exports) {

	module.exports = {
	  MODE: {
	    TINY: Symbol("TINY"),
	    SHORT: Symbol("SHORT"),
	    LONG: Symbol("LONG")
	  }
	};

/***/ },
/* 3 */
/***/ function(module, exports, __webpack_require__) {

	const React = __webpack_require__(1);
	
	const { isGrip } = __webpack_require__(4);
	const { MODE } = __webpack_require__(2);
	
	// Load all existing rep templates
	const Undefined = __webpack_require__(5);
	const Null = __webpack_require__(6);
	const StringRep = __webpack_require__(7);
	const LongStringRep = __webpack_require__(8);
	const Number = __webpack_require__(9);
	const ArrayRep = __webpack_require__(10);
	const Obj = __webpack_require__(12);
	const SymbolRep = __webpack_require__(15);
	const InfinityRep = __webpack_require__(16);
	const NaNRep = __webpack_require__(17);
	
	// DOM types (grips)
	const Attribute = __webpack_require__(18);
	const DateTime = __webpack_require__(19);
	const Document = __webpack_require__(20);
	const Event = __webpack_require__(21);
	const Func = __webpack_require__(22);
	const PromiseRep = __webpack_require__(23);
	const RegExp = __webpack_require__(24);
	const StyleSheet = __webpack_require__(25);
	const CommentNode = __webpack_require__(26);
	const ElementNode = __webpack_require__(28);
	const TextNode = __webpack_require__(29);
	const ErrorRep = __webpack_require__(30);
	const Window = __webpack_require__(31);
	const ObjectWithText = __webpack_require__(32);
	const ObjectWithURL = __webpack_require__(33);
	const GripArray = __webpack_require__(34);
	const GripMap = __webpack_require__(35);
	const Grip = __webpack_require__(14);
	
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
	const Rep = React.createClass({
	  displayName: "Rep",
	
	  propTypes: {
	    object: React.PropTypes.any,
	    defaultRep: React.PropTypes.object,
	    // @TODO Change this to Object.values once it's supported in Node's version of V8
	    mode: React.PropTypes.oneOf(Object.keys(MODE).map(key => MODE[key]))
	  },
	
	  render: function () {
	    let rep = getRep(this.props.object, this.props.defaultRep);
	    return rep(this.props);
	  }
	});
	
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
	        return React.createFactory(rep.rep);
	      }
	    } catch (err) {
	      console.error(err);
	    }
	  }
	
	  return React.createFactory(defaultRep.rep);
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
	  }
	};

/***/ },
/* 4 */
/***/ function(module, exports, __webpack_require__) {

	// Dependencies
	const React = __webpack_require__(1);
	
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
	
	function cropMultipleLines(text, limit) {
	  return escapeNewLines(cropString(text, limit));
	}
	
	function cropString(text, limit, alternativeText) {
	  if (!alternativeText) {
	    alternativeText = "\u2026";
	  }
	
	  // Make sure it's a string and sanitize it.
	  text = sanitizeString(text + "");
	
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
	
	function sanitizeString(text) {
	  // Replace all non-printable characters, except of
	  // (horizontal) tab (HT: \x09) and newline (LF: \x0A, CR: \x0D),
	  // with unicode replacement character (u+fffd).
	  // eslint-disable-next-line no-control-regex
	  let re = new RegExp("[\x00-\x08\x0B\x0C\x0E-\x1F\x80-\x9F]", "g");
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
	  return function () {
	    try {
	      return renderMethod.call(this);
	    } catch (e) {
	      return React.DOM.span({
	        className: "objectBox objectBox-failure",
	        title: "This object could not be rendered, " + "please file a bug on bugzilla.mozilla.org"
	      },
	      /* Labels have to be hardcoded for reps, see Bug 1317038. */
	      "Invalid object");
	    }
	  };
	}
	
	module.exports = {
	  createFactories,
	  isGrip,
	  cropString,
	  sanitizeString,
	  wrapRender,
	  cropMultipleLines,
	  parseURLParams,
	  parseURLEncodedText,
	  getFileName,
	  getURLDisplayString
	};

/***/ },
/* 5 */
/***/ function(module, exports, __webpack_require__) {

	// Dependencies
	const React = __webpack_require__(1);
	
	const { wrapRender } = __webpack_require__(4);
	
	// Shortcuts
	const { span } = React.DOM;
	
	/**
	 * Renders undefined value
	 */
	const Undefined = React.createClass({
	  displayName: "UndefinedRep",
	
	  render: wrapRender(function () {
	    return span({ className: "objectBox objectBox-undefined" }, "undefined");
	  })
	});
	
	function supportsObject(object, type) {
	  if (object && object.type && object.type == "undefined") {
	    return true;
	  }
	
	  return type == "undefined";
	}
	
	// Exports from this module
	
	module.exports = {
	  rep: Undefined,
	  supportsObject: supportsObject
	};

/***/ },
/* 6 */
/***/ function(module, exports, __webpack_require__) {

	// Dependencies
	const React = __webpack_require__(1);
	
	const { wrapRender } = __webpack_require__(4);
	
	// Shortcuts
	const { span } = React.DOM;
	
	/**
	 * Renders null value
	 */
	const Null = React.createClass({
	  displayName: "NullRep",
	
	  render: wrapRender(function () {
	    return span({ className: "objectBox objectBox-null" }, "null");
	  })
	});
	
	function supportsObject(object, type) {
	  if (object && object.type && object.type == "null") {
	    return true;
	  }
	
	  return object == null;
	}
	
	// Exports from this module
	
	module.exports = {
	  rep: Null,
	  supportsObject: supportsObject
	};

/***/ },
/* 7 */
/***/ function(module, exports, __webpack_require__) {

	// Dependencies
	const React = __webpack_require__(1);
	
	const {
	  cropString,
	  wrapRender
	} = __webpack_require__(4);
	
	// Shortcuts
	const { span } = React.DOM;
	
	/**
	 * Renders a string. String value is enclosed within quotes.
	 */
	const StringRep = React.createClass({
	  displayName: "StringRep",
	
	  propTypes: {
	    useQuotes: React.PropTypes.bool,
	    style: React.PropTypes.object,
	    object: React.PropTypes.string.isRequired,
	    member: React.PropTypes.any,
	    cropLimit: React.PropTypes.number
	  },
	
	  getDefaultProps: function () {
	    return {
	      useQuotes: true
	    };
	  },
	
	  render: wrapRender(function () {
	    let text = this.props.object;
	    let member = this.props.member;
	    let style = this.props.style;
	
	    let config = { className: "objectBox objectBox-string" };
	    if (style) {
	      config.style = style;
	    }
	
	    if (member && member.open) {
	      return span(config, "\"" + text + "\"");
	    }
	
	    let croppedString = this.props.cropLimit ? cropString(text, this.props.cropLimit) : cropString(text);
	
	    let formattedString = this.props.useQuotes ? "\"" + croppedString + "\"" : croppedString;
	
	    return span(config, formattedString);
	  })
	});
	
	function supportsObject(object, type) {
	  return type == "string";
	}
	
	// Exports from this module
	
	module.exports = {
	  rep: StringRep,
	  supportsObject: supportsObject
	};

/***/ },
/* 8 */
/***/ function(module, exports, __webpack_require__) {

	// Dependencies
	const React = __webpack_require__(1);
	const {
	  sanitizeString,
	  isGrip,
	  wrapRender
	} = __webpack_require__(4);
	// Shortcuts
	const { span } = React.DOM;
	
	/**
	 * Renders a long string grip.
	 */
	const LongStringRep = React.createClass({
	  displayName: "LongStringRep",
	
	  propTypes: {
	    useQuotes: React.PropTypes.bool,
	    style: React.PropTypes.object,
	    cropLimit: React.PropTypes.number.isRequired,
	    member: React.PropTypes.string,
	    object: React.PropTypes.object.isRequired
	  },
	
	  getDefaultProps: function () {
	    return {
	      useQuotes: true
	    };
	  },
	
	  render: wrapRender(function () {
	    let {
	      cropLimit,
	      member,
	      object,
	      style,
	      useQuotes
	    } = this.props;
	    let { fullText, initial, length } = object;
	
	    let config = { className: "objectBox objectBox-string" };
	    if (style) {
	      config.style = style;
	    }
	
	    let string = member && member.open ? fullText || initial : initial.substring(0, cropLimit);
	
	    if (string.length < length) {
	      string += "\u2026";
	    }
	    let formattedString = useQuotes ? `"${string}"` : string;
	    return span(config, sanitizeString(formattedString));
	  })
	});
	
	function supportsObject(object, type) {
	  if (!isGrip(object)) {
	    return false;
	  }
	  return object.type === "longString";
	}
	
	// Exports from this module
	module.exports = {
	  rep: LongStringRep,
	  supportsObject: supportsObject
	};

/***/ },
/* 9 */
/***/ function(module, exports, __webpack_require__) {

	// Dependencies
	const React = __webpack_require__(1);
	
	const { wrapRender } = __webpack_require__(4);
	
	// Shortcuts
	const { span } = React.DOM;
	
	/**
	 * Renders a number
	 */
	const Number = React.createClass({
	  displayName: "Number",
	
	  propTypes: {
	    object: React.PropTypes.oneOfType([React.PropTypes.object, React.PropTypes.number]).isRequired
	  },
	
	  stringify: function (object) {
	    let isNegativeZero = Object.is(object, -0) || object.type && object.type == "-0";
	
	    return isNegativeZero ? "-0" : String(object);
	  },
	
	  render: wrapRender(function () {
	    let value = this.props.object;
	
	    return span({ className: "objectBox objectBox-number" }, this.stringify(value));
	  })
	});
	
	function supportsObject(object, type) {
	  return ["boolean", "number", "-0"].includes(type);
	}
	
	// Exports from this module
	
	module.exports = {
	  rep: Number,
	  supportsObject: supportsObject
	};

/***/ },
/* 10 */
/***/ function(module, exports, __webpack_require__) {

	// Dependencies
	const React = __webpack_require__(1);
	const {
	  createFactories,
	  wrapRender
	} = __webpack_require__(4);
	const Caption = React.createFactory(__webpack_require__(11));
	const { MODE } = __webpack_require__(2);
	
	const ModePropType = React.PropTypes.oneOf(
	// @TODO Change this to Object.values once it's supported in Node's version of V8
	Object.keys(MODE).map(key => MODE[key]));
	
	// Shortcuts
	const DOM = React.DOM;
	
	/**
	 * Renders an array. The array is enclosed by left and right bracket
	 * and the max number of rendered items depends on the current mode.
	 */
	let ArrayRep = React.createClass({
	  displayName: "ArrayRep",
	
	  propTypes: {
	    mode: ModePropType,
	    objectLink: React.PropTypes.func,
	    object: React.PropTypes.array.isRequired
	  },
	
	  getTitle: function (object, context) {
	    return "[" + object.length + "]";
	  },
	
	  arrayIterator: function (array, max) {
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
	      let objectLink = this.props.objectLink || DOM.span;
	      items.push(Caption({
	        object: objectLink({
	          object: this.props.object
	        }, array.length - max + " more…")
	      }));
	    }
	
	    return items;
	  },
	
	  /**
	   * Returns true if the passed object is an array with additional (custom)
	   * properties, otherwise returns false. Custom properties should be
	   * displayed in extra expandable section.
	   *
	   * Example array with a custom property.
	   * let arr = [0, 1];
	   * arr.myProp = "Hello";
	   *
	   * @param {Array} array The array object.
	   */
	  hasSpecialProperties: function (array) {
	    function isInteger(x) {
	      let y = parseInt(x, 10);
	      if (isNaN(y)) {
	        return false;
	      }
	      return x === y.toString();
	    }
	
	    let props = Object.getOwnPropertyNames(array);
	    for (let i = 0; i < props.length; i++) {
	      let p = props[i];
	
	      // Valid indexes are skipped
	      if (isInteger(p)) {
	        continue;
	      }
	
	      // Ignore standard 'length' property, anything else is custom.
	      if (p != "length") {
	        return true;
	      }
	    }
	
	    return false;
	  },
	
	  // Event Handlers
	
	  onToggleProperties: function (event) {},
	
	  onClickBracket: function (event) {},
	
	  render: wrapRender(function () {
	    let {
	      object,
	      mode = MODE.SHORT
	    } = this.props;
	
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
	      items = this.arrayIterator(object, max);
	      brackets = needSpace(items.length > 0);
	    }
	
	    let objectLink = this.props.objectLink || DOM.span;
	
	    return DOM.span({
	      className: "objectBox objectBox-array" }, objectLink({
	      className: "arrayLeftBracket",
	      object: object
	    }, brackets.left), ...items, objectLink({
	      className: "arrayRightBracket",
	      object: object
	    }, brackets.right), DOM.span({
	      className: "arrayProperties",
	      role: "group" }));
	  })
	});
	
	/**
	 * Renders array item. Individual values are separated by a comma.
	 */
	let ItemRep = React.createFactory(React.createClass({
	  displayName: "ItemRep",
	
	  propTypes: {
	    object: React.PropTypes.any.isRequired,
	    delim: React.PropTypes.string.isRequired,
	    mode: ModePropType
	  },
	
	  render: wrapRender(function () {
	    const { Rep } = createFactories(__webpack_require__(3));
	
	    let object = this.props.object;
	    let delim = this.props.delim;
	    let mode = this.props.mode;
	    return DOM.span({}, Rep({ object: object, mode: mode }), delim);
	  })
	}));
	
	function supportsObject(object, type) {
	  return Array.isArray(object) || Object.prototype.toString.call(object) === "[object Arguments]";
	}
	
	// Exports from this module
	module.exports = {
	  rep: ArrayRep,
	  supportsObject: supportsObject
	};

/***/ },
/* 11 */
/***/ function(module, exports, __webpack_require__) {

	// Dependencies
	const React = __webpack_require__(1);
	const DOM = React.DOM;
	
	const { wrapRender } = __webpack_require__(4);
	
	/**
	 * Renders a caption. This template is used by other components
	 * that needs to distinguish between a simple text/value and a label.
	 */
	const Caption = React.createClass({
	  displayName: "Caption",
	
	  propTypes: {
	    object: React.PropTypes.object
	  },
	
	  render: wrapRender(function () {
	    return DOM.span({ "className": "caption" }, this.props.object);
	  })
	});
	
	// Exports from this module
	module.exports = Caption;

/***/ },
/* 12 */
/***/ function(module, exports, __webpack_require__) {

	// Dependencies
	const React = __webpack_require__(1);
	const {
	  wrapRender
	} = __webpack_require__(4);
	const Caption = React.createFactory(__webpack_require__(11));
	const PropRep = React.createFactory(__webpack_require__(13));
	const { MODE } = __webpack_require__(2);
	// Shortcuts
	const { span } = React.DOM;
	/**
	 * Renders an object. An object is represented by a list of its
	 * properties enclosed in curly brackets.
	 */
	const Obj = React.createClass({
	  displayName: "Obj",
	
	  propTypes: {
	    object: React.PropTypes.object.isRequired,
	    // @TODO Change this to Object.values once it's supported in Node's version of V8
	    mode: React.PropTypes.oneOf(Object.keys(MODE).map(key => MODE[key])),
	    objectLink: React.PropTypes.func,
	    title: React.PropTypes.string
	  },
	
	  getTitle: function (object) {
	    let title = this.props.title || object.class || "Object";
	    if (this.props.objectLink) {
	      return this.props.objectLink({
	        object: object
	      }, title);
	    }
	    return title;
	  },
	
	  safePropIterator: function (object, max) {
	    max = typeof max === "undefined" ? 3 : max;
	    try {
	      return this.propIterator(object, max);
	    } catch (err) {
	      console.error(err);
	    }
	    return [];
	  },
	
	  propIterator: function (object, max) {
	    let isInterestingProp = (t, value) => {
	      // Do not pick objects, it could cause recursion.
	      return t == "boolean" || t == "number" || t == "string" && value;
	    };
	
	    // Work around https://bugzilla.mozilla.org/show_bug.cgi?id=945377
	    if (Object.prototype.toString.call(object) === "[object Generator]") {
	      object = Object.getPrototypeOf(object);
	    }
	
	    // Object members with non-empty values are preferred since it gives the
	    // user a better overview of the object.
	    let propsArray = this.getPropsArray(object, max, isInterestingProp);
	
	    if (propsArray.length <= max) {
	      // There are not enough props yet (or at least, not enough props to
	      // be able to know whether we should print "more…" or not).
	      // Let's display also empty members and functions.
	      propsArray = propsArray.concat(this.getPropsArray(object, max, (t, value) => {
	        return !isInterestingProp(t, value);
	      }));
	    }
	
	    if (propsArray.length > max) {
	      propsArray.pop();
	      let objectLink = this.props.objectLink || span;
	
	      propsArray.push(Caption({
	        object: objectLink({
	          object: object
	        }, Object.keys(object).length - max + " more…")
	      }));
	    } else if (propsArray.length > 0) {
	      // Remove the last comma.
	      propsArray[propsArray.length - 1] = React.cloneElement(propsArray[propsArray.length - 1], { delim: "" });
	    }
	
	    return propsArray;
	  },
	
	  getPropsArray: function (object, max, filter) {
	    let propsArray = [];
	
	    max = max || 3;
	    if (!object) {
	      return propsArray;
	    }
	
	    // Hardcode tiny mode to avoid recursive handling.
	    let mode = MODE.TINY;
	
	    try {
	      for (let name in object) {
	        if (propsArray.length > max) {
	          return propsArray;
	        }
	
	        let value;
	        try {
	          value = object[name];
	        } catch (exc) {
	          continue;
	        }
	
	        let t = typeof value;
	        if (filter(t, value)) {
	          propsArray.push(PropRep({
	            mode: mode,
	            name: name,
	            object: value,
	            equal: ": ",
	            delim: ", "
	          }));
	        }
	      }
	    } catch (err) {
	      console.error(err);
	    }
	
	    return propsArray;
	  },
	
	  render: wrapRender(function () {
	    let object = this.props.object;
	    let propsArray = this.safePropIterator(object);
	    let objectLink = this.props.objectLink || span;
	
	    if (this.props.mode === MODE.TINY || !propsArray.length) {
	      return span({ className: "objectBox objectBox-object" }, objectLink({ className: "objectTitle" }, this.getTitle(object)));
	    }
	
	    return span({ className: "objectBox objectBox-object" }, this.getTitle(object), objectLink({
	      className: "objectLeftBrace",
	      object: object
	    }, " { "), ...propsArray, objectLink({
	      className: "objectRightBrace",
	      object: object
	    }, " }"));
	  })
	});
	function supportsObject(object, type) {
	  return true;
	}
	
	// Exports from this module
	module.exports = {
	  rep: Obj,
	  supportsObject: supportsObject
	};

/***/ },
/* 13 */
/***/ function(module, exports, __webpack_require__) {

	// Dependencies
	const React = __webpack_require__(1);
	const {
	  createFactories,
	  wrapRender
	} = __webpack_require__(4);
	const { MODE } = __webpack_require__(2);
	// Shortcuts
	const { span } = React.DOM;
	
	/**
	 * Property for Obj (local JS objects), Grip (remote JS objects)
	 * and GripMap (remote JS maps and weakmaps) reps.
	 * It's used to render object properties.
	 */
	let PropRep = React.createClass({
	  displayName: "PropRep",
	
	  propTypes: {
	    // Property name.
	    name: React.PropTypes.oneOfType([React.PropTypes.string, React.PropTypes.object]).isRequired,
	    // Equal character rendered between property name and value.
	    equal: React.PropTypes.string,
	    // Delimiter character used to separate individual properties.
	    delim: React.PropTypes.string,
	    // @TODO Change this to Object.values once it's supported in Node's version of V8
	    mode: React.PropTypes.oneOf(Object.keys(MODE).map(key => MODE[key])),
	    objectLink: React.PropTypes.func
	  },
	
	  render: wrapRender(function () {
	    const Grip = __webpack_require__(14);
	    let { Rep } = createFactories(__webpack_require__(3));
	
	    let key;
	    // The key can be a simple string, for plain objects,
	    // or another object for maps and weakmaps.
	    if (typeof this.props.name === "string") {
	      key = span({ "className": "nodeName" }, this.props.name);
	    } else {
	      key = Rep({
	        object: this.props.name,
	        mode: this.props.mode || MODE.TINY,
	        defaultRep: Grip,
	        objectLink: this.props.objectLink
	      });
	    }
	
	    return span({}, key, span({
	      "className": "objectEqual"
	    }, this.props.equal), Rep(this.props), span({
	      "className": "objectComma"
	    }, this.props.delim));
	  })
	});
	
	// Exports from this module
	module.exports = PropRep;

/***/ },
/* 14 */
/***/ function(module, exports, __webpack_require__) {

	// ReactJS
	const React = __webpack_require__(1);
	// Dependencies
	const {
	  createFactories,
	  isGrip,
	  wrapRender
	} = __webpack_require__(4);
	const Caption = React.createFactory(__webpack_require__(11));
	const PropRep = React.createFactory(__webpack_require__(13));
	const { MODE } = __webpack_require__(2);
	// Shortcuts
	const { span } = React.DOM;
	
	/**
	 * Renders generic grip. Grip is client representation
	 * of remote JS object and is used as an input object
	 * for this rep component.
	 */
	const GripRep = React.createClass({
	  displayName: "Grip",
	
	  propTypes: {
	    object: React.PropTypes.object.isRequired,
	    // @TODO Change this to Object.values once it's supported in Node's version of V8
	    mode: React.PropTypes.oneOf(Object.keys(MODE).map(key => MODE[key])),
	    isInterestingProp: React.PropTypes.func,
	    title: React.PropTypes.string,
	    objectLink: React.PropTypes.func
	  },
	
	  getTitle: function (object) {
	    let title = this.props.title || object.class || "Object";
	    if (this.props.objectLink) {
	      return this.props.objectLink({
	        object: object
	      }, title);
	    }
	    return title;
	  },
	
	  safePropIterator: function (object, max) {
	    max = typeof max === "undefined" ? 3 : max;
	    try {
	      return this.propIterator(object, max);
	    } catch (err) {
	      console.error(err);
	    }
	    return [];
	  },
	
	  propIterator: function (object, max) {
	    if (object.preview && Object.keys(object.preview).includes("wrappedValue")) {
	      const { Rep } = createFactories(__webpack_require__(3));
	
	      return [Rep({
	        object: object.preview.wrappedValue,
	        mode: this.props.mode || MODE.TINY,
	        defaultRep: Grip
	      })];
	    }
	
	    // Property filter. Show only interesting properties to the user.
	    let isInterestingProp = this.props.isInterestingProp || ((type, value) => {
	      return type == "boolean" || type == "number" || type == "string" && value.length != 0;
	    });
	
	    let properties = object.preview ? object.preview.ownProperties : {};
	    let propertiesLength = object.preview && object.preview.ownPropertiesLength ? object.preview.ownPropertiesLength : object.ownPropertyLength;
	
	    if (object.preview && object.preview.safeGetterValues) {
	      properties = Object.assign({}, properties, object.preview.safeGetterValues);
	      propertiesLength += Object.keys(object.preview.safeGetterValues).length;
	    }
	
	    let indexes = this.getPropIndexes(properties, max, isInterestingProp);
	    if (indexes.length < max && indexes.length < propertiesLength) {
	      // There are not enough props yet. Then add uninteresting props to display them.
	      indexes = indexes.concat(this.getPropIndexes(properties, max - indexes.length, (t, value, name) => {
	        return !isInterestingProp(t, value, name);
	      }));
	    }
	
	    const truncate = Object.keys(properties).length > max;
	    let props = this.getProps(properties, indexes, truncate);
	    if (truncate) {
	      // There are some undisplayed props. Then display "more...".
	      let objectLink = this.props.objectLink || span;
	
	      props.push(Caption({
	        object: objectLink({
	          object: object
	        }, `${propertiesLength - max} more…`)
	      }));
	    }
	
	    return props;
	  },
	
	  /**
	   * Get props ordered by index.
	   *
	   * @param {Object} properties Props object.
	   * @param {Array} indexes Indexes of props.
	   * @param {Boolean} truncate true if the grip will be truncated.
	   * @return {Array} Props.
	   */
	  getProps: function (properties, indexes, truncate) {
	    let props = [];
	
	    // Make indexes ordered by ascending.
	    indexes.sort(function (a, b) {
	      return a - b;
	    });
	
	    indexes.forEach(i => {
	      let name = Object.keys(properties)[i];
	      let value = this.getPropValue(properties[name]);
	
	      props.push(PropRep(Object.assign({}, this.props, {
	        mode: MODE.TINY,
	        name: name,
	        object: value,
	        equal: ": ",
	        delim: i !== indexes.length - 1 || truncate ? ", " : "",
	        defaultRep: Grip
	      })));
	    });
	
	    return props;
	  },
	
	  /**
	   * Get the indexes of props in the object.
	   *
	   * @param {Object} properties Props object.
	   * @param {Number} max The maximum length of indexes array.
	   * @param {Function} filter Filter the props you want.
	   * @return {Array} Indexes of interesting props in the object.
	   */
	  getPropIndexes: function (properties, max, filter) {
	    let indexes = [];
	
	    try {
	      let i = 0;
	      for (let name in properties) {
	        if (indexes.length >= max) {
	          return indexes;
	        }
	
	        // Type is specified in grip's "class" field and for primitive
	        // values use typeof.
	        let value = this.getPropValue(properties[name]);
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
	  },
	
	  /**
	   * Get the actual value of a property.
	   *
	   * @param {Object} property
	   * @return {Object} Value of the property.
	   */
	  getPropValue: function (property) {
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
	  },
	
	  render: wrapRender(function () {
	    let object = this.props.object;
	    let props = this.safePropIterator(object, this.props.mode === MODE.LONG ? 10 : 3);
	
	    let objectLink = this.props.objectLink || span;
	    if (this.props.mode === MODE.TINY) {
	      return span({ className: "objectBox objectBox-object" }, this.getTitle(object), objectLink({
	        className: "objectLeftBrace",
	        object: object
	      }, ""));
	    }
	
	    return span({ className: "objectBox objectBox-object" }, this.getTitle(object), objectLink({
	      className: "objectLeftBrace",
	      object: object
	    }, " { "), ...props, objectLink({
	      className: "objectRightBrace",
	      object: object
	    }, " }"));
	  })
	});
	
	// Registration
	function supportsObject(object, type) {
	  if (!isGrip(object)) {
	    return false;
	  }
	  return object.preview && object.preview.ownProperties;
	}
	
	// Grip is used in propIterator and has to be defined here.
	let Grip = {
	  rep: GripRep,
	  supportsObject: supportsObject
	};
	
	// Exports from this module
	module.exports = Grip;

/***/ },
/* 15 */
/***/ function(module, exports, __webpack_require__) {

	// Dependencies
	const React = __webpack_require__(1);
	
	const { wrapRender } = __webpack_require__(4);
	
	// Shortcuts
	const { span } = React.DOM;
	
	/**
	 * Renders a symbol.
	 */
	const SymbolRep = React.createClass({
	  displayName: "SymbolRep",
	
	  propTypes: {
	    object: React.PropTypes.object.isRequired
	  },
	
	  render: wrapRender(function () {
	    let { object } = this.props;
	    let { name } = object;
	
	    return span({ className: "objectBox objectBox-symbol" }, `Symbol(${name || ""})`);
	  })
	});
	
	function supportsObject(object, type) {
	  return type == "symbol";
	}
	
	// Exports from this module
	module.exports = {
	  rep: SymbolRep,
	  supportsObject: supportsObject
	};

/***/ },
/* 16 */
/***/ function(module, exports, __webpack_require__) {

	// Dependencies
	const React = __webpack_require__(1);
	
	const { wrapRender } = __webpack_require__(4);
	
	// Shortcuts
	const { span } = React.DOM;
	
	/**
	 * Renders a Infinity object
	 */
	const InfinityRep = React.createClass({
	  displayName: "Infinity",
	
	  propTypes: {
	    object: React.PropTypes.object.isRequired
	  },
	
	  render: wrapRender(function () {
	    return span({ className: "objectBox objectBox-number" }, this.props.object.type);
	  })
	});
	
	function supportsObject(object, type) {
	  return type == "Infinity" || type == "-Infinity";
	}
	
	// Exports from this module
	module.exports = {
	  rep: InfinityRep,
	  supportsObject: supportsObject
	};

/***/ },
/* 17 */
/***/ function(module, exports, __webpack_require__) {

	// Dependencies
	const React = __webpack_require__(1);
	
	const { wrapRender } = __webpack_require__(4);
	
	// Shortcuts
	const { span } = React.DOM;
	
	/**
	 * Renders a NaN object
	 */
	const NaNRep = React.createClass({
	  displayName: "NaN",
	
	  render: wrapRender(function () {
	    return span({ className: "objectBox objectBox-nan" }, "NaN");
	  })
	});
	
	function supportsObject(object, type) {
	  return type == "NaN";
	}
	
	// Exports from this module
	module.exports = {
	  rep: NaNRep,
	  supportsObject: supportsObject
	};

/***/ },
/* 18 */
/***/ function(module, exports, __webpack_require__) {

	// ReactJS
	const React = __webpack_require__(1);
	
	// Reps
	const {
	  createFactories,
	  isGrip,
	  wrapRender
	} = __webpack_require__(4);
	const StringRep = __webpack_require__(7);
	
	// Shortcuts
	const { span } = React.DOM;
	const { rep: StringRepFactory } = createFactories(StringRep);
	
	/**
	 * Renders DOM attribute
	 */
	let Attribute = React.createClass({
	  displayName: "Attr",
	
	  propTypes: {
	    object: React.PropTypes.object.isRequired,
	    objectLink: React.PropTypes.func
	  },
	
	  getTitle: function (grip) {
	    return grip.preview.nodeName;
	  },
	
	  render: wrapRender(function () {
	    let object = this.props.object;
	    let value = object.preview.value;
	    let objectLink = this.props.objectLink || span;
	
	    return objectLink({ className: "objectLink-Attr", object }, span({}, span({ className: "attrTitle" }, this.getTitle(object)), span({ className: "attrEqual" }, "="), StringRepFactory({ object: value })));
	  })
	});
	
	// Registration
	
	function supportsObject(grip, type) {
	  if (!isGrip(grip)) {
	    return false;
	  }
	
	  return type == "Attr" && grip.preview;
	}
	
	module.exports = {
	  rep: Attribute,
	  supportsObject: supportsObject
	};

/***/ },
/* 19 */
/***/ function(module, exports, __webpack_require__) {

	// ReactJS
	const React = __webpack_require__(1);
	
	// Reps
	const {
	  isGrip,
	  wrapRender
	} = __webpack_require__(4);
	
	// Shortcuts
	const { span } = React.DOM;
	
	/**
	 * Used to render JS built-in Date() object.
	 */
	let DateTime = React.createClass({
	  displayName: "Date",
	
	  propTypes: {
	    object: React.PropTypes.object.isRequired,
	    objectLink: React.PropTypes.func
	  },
	
	  getTitle: function (grip) {
	    if (this.props.objectLink) {
	      return this.props.objectLink({
	        object: grip
	      }, grip.class + " ");
	    }
	    return "";
	  },
	
	  render: wrapRender(function () {
	    let grip = this.props.object;
	    let date;
	    try {
	      date = span({ className: "objectBox" }, this.getTitle(grip), span({ className: "Date" }, new Date(grip.preview.timestamp).toISOString()));
	    } catch (e) {
	      date = span({ className: "objectBox" }, "Invalid Date");
	    }
	
	    return date;
	  })
	});
	
	// Registration
	
	function supportsObject(grip, type) {
	  if (!isGrip(grip)) {
	    return false;
	  }
	
	  return type == "Date" && grip.preview;
	}
	
	// Exports from this module
	module.exports = {
	  rep: DateTime,
	  supportsObject: supportsObject
	};

/***/ },
/* 20 */
/***/ function(module, exports, __webpack_require__) {

	// ReactJS
	const React = __webpack_require__(1);
	
	// Reps
	const {
	  isGrip,
	  getURLDisplayString,
	  wrapRender
	} = __webpack_require__(4);
	
	// Shortcuts
	const { span } = React.DOM;
	
	/**
	 * Renders DOM document object.
	 */
	let Document = React.createClass({
	  displayName: "Document",
	
	  propTypes: {
	    object: React.PropTypes.object.isRequired,
	    objectLink: React.PropTypes.func
	  },
	
	  getLocation: function (grip) {
	    let location = grip.preview.location;
	    return location ? getURLDisplayString(location) : "";
	  },
	
	  getTitle: function (grip) {
	    if (this.props.objectLink) {
	      return span({ className: "objectBox" }, this.props.objectLink({
	        object: grip
	      }, grip.class + " "));
	    }
	    return "";
	  },
	
	  getTooltip: function (doc) {
	    return doc.location.href;
	  },
	
	  render: wrapRender(function () {
	    let grip = this.props.object;
	
	    return span({ className: "objectBox objectBox-object" }, this.getTitle(grip), span({ className: "objectPropValue" }, this.getLocation(grip)));
	  })
	});
	
	// Registration
	
	function supportsObject(object, type) {
	  if (!isGrip(object)) {
	    return false;
	  }
	
	  return object.preview && type == "HTMLDocument";
	}
	
	// Exports from this module
	module.exports = {
	  rep: Document,
	  supportsObject: supportsObject
	};

/***/ },
/* 21 */
/***/ function(module, exports, __webpack_require__) {

	// ReactJS
	const React = __webpack_require__(1);
	
	// Reps
	const {
	  createFactories,
	  isGrip,
	  wrapRender
	} = __webpack_require__(4);
	const { rep } = createFactories(__webpack_require__(14));
	
	/**
	 * Renders DOM event objects.
	 */
	let Event = React.createClass({
	  displayName: "event",
	
	  propTypes: {
	    object: React.PropTypes.object.isRequired
	  },
	
	  getTitle: function (props) {
	    let preview = props.object.preview;
	    let title = preview.type;
	
	    if (preview.eventKind == "key" && preview.modifiers && preview.modifiers.length) {
	      title = `${title} ${preview.modifiers.join("-")}`;
	    }
	    return title;
	  },
	
	  render: wrapRender(function () {
	    // Use `Object.assign` to keep `this.props` without changes because:
	    // 1. JSON.stringify/JSON.parse is slow.
	    // 2. Immutable.js is planned for the future.
	    let props = Object.assign({
	      title: this.getTitle(this.props)
	    }, this.props);
	    props.object = Object.assign({}, this.props.object);
	    props.object.preview = Object.assign({}, this.props.object.preview);
	
	    props.object.preview.ownProperties = {};
	    if (props.object.preview.target) {
	      Object.assign(props.object.preview.ownProperties, {
	        target: props.object.preview.target
	      });
	    }
	    Object.assign(props.object.preview.ownProperties, props.object.preview.properties);
	
	    delete props.object.preview.properties;
	    props.object.ownPropertyLength = Object.keys(props.object.preview.ownProperties).length;
	
	    switch (props.object.class) {
	      case "MouseEvent":
	        props.isInterestingProp = (type, value, name) => {
	          return ["target", "clientX", "clientY", "layerX", "layerY"].includes(name);
	        };
	        break;
	      case "KeyboardEvent":
	        props.isInterestingProp = (type, value, name) => {
	          return ["target", "key", "charCode", "keyCode"].includes(name);
	        };
	        break;
	      case "MessageEvent":
	        props.isInterestingProp = (type, value, name) => {
	          return ["target", "isTrusted", "data"].includes(name);
	        };
	        break;
	      default:
	        props.isInterestingProp = (type, value, name) => {
	          // We want to show the properties in the order they are declared.
	          return Object.keys(props.object.preview.ownProperties).includes(name);
	        };
	    }
	
	    return rep(props);
	  })
	});
	
	// Registration
	
	function supportsObject(grip, type) {
	  if (!isGrip(grip)) {
	    return false;
	  }
	
	  return grip.preview && grip.preview.kind == "DOMEvent";
	}
	
	// Exports from this module
	module.exports = {
	  rep: Event,
	  supportsObject: supportsObject
	};

/***/ },
/* 22 */
/***/ function(module, exports, __webpack_require__) {

	// ReactJS
	const React = __webpack_require__(1);
	
	// Reps
	const {
	  isGrip,
	  cropString,
	  wrapRender
	} = __webpack_require__(4);
	
	// Shortcuts
	const { span } = React.DOM;
	
	/**
	 * This component represents a template for Function objects.
	 */
	let Func = React.createClass({
	  displayName: "Func",
	
	  propTypes: {
	    object: React.PropTypes.object.isRequired,
	    objectLink: React.PropTypes.func
	  },
	
	  getTitle: function (grip) {
	    let title = "function ";
	    if (grip.isGenerator) {
	      title = "function* ";
	    }
	    if (grip.isAsync) {
	      title = "async " + title;
	    }
	
	    if (this.props.objectLink) {
	      return this.props.objectLink({
	        object: grip
	      }, title);
	    }
	
	    return title;
	  },
	
	  summarizeFunction: function (grip) {
	    let name = grip.userDisplayName || grip.displayName || grip.name || "";
	    return cropString(name + "()", 100);
	  },
	
	  render: wrapRender(function () {
	    let grip = this.props.object;
	
	    return (
	      // Set dir="ltr" to prevent function parentheses from
	      // appearing in the wrong direction
	      span({ dir: "ltr", className: "objectBox objectBox-function" }, this.getTitle(grip), this.summarizeFunction(grip))
	    );
	  })
	});
	
	// Registration
	
	function supportsObject(grip, type) {
	  if (!isGrip(grip)) {
	    return type == "function";
	  }
	
	  return type == "Function";
	}
	
	// Exports from this module
	
	module.exports = {
	  rep: Func,
	  supportsObject: supportsObject
	};

/***/ },
/* 23 */
/***/ function(module, exports, __webpack_require__) {

	// ReactJS
	const React = __webpack_require__(1);
	// Dependencies
	const {
	  createFactories,
	  isGrip,
	  wrapRender
	} = __webpack_require__(4);
	
	const PropRep = React.createFactory(__webpack_require__(13));
	const { MODE } = __webpack_require__(2);
	// Shortcuts
	const { span } = React.DOM;
	
	/**
	 * Renders a DOM Promise object.
	 */
	const PromiseRep = React.createClass({
	  displayName: "Promise",
	
	  propTypes: {
	    object: React.PropTypes.object.isRequired,
	    // @TODO Change this to Object.values once it's supported in Node's version of V8
	    mode: React.PropTypes.oneOf(Object.keys(MODE).map(key => MODE[key])),
	    objectLink: React.PropTypes.func
	  },
	
	  getTitle: function (object) {
	    const title = object.class;
	    if (this.props.objectLink) {
	      return this.props.objectLink({
	        object: object
	      }, title);
	    }
	    return title;
	  },
	
	  getProps: function (promiseState) {
	    const keys = ["state"];
	    if (Object.keys(promiseState).includes("value")) {
	      keys.push("value");
	    }
	
	    return keys.map((key, i) => {
	      return PropRep(Object.assign({}, this.props, {
	        mode: MODE.TINY,
	        name: `<${key}>`,
	        object: promiseState[key],
	        equal: ": ",
	        delim: i < keys.length - 1 ? ", " : ""
	      }));
	    });
	  },
	
	  render: wrapRender(function () {
	    const object = this.props.object;
	    const { promiseState } = object;
	    let objectLink = this.props.objectLink || span;
	
	    if (this.props.mode === MODE.TINY) {
	      let { Rep } = createFactories(__webpack_require__(3));
	
	      return span({ className: "objectBox objectBox-object" }, this.getTitle(object), objectLink({
	        className: "objectLeftBrace",
	        object: object
	      }, " { "), Rep({ object: promiseState.state }), objectLink({
	        className: "objectRightBrace",
	        object: object
	      }, " }"));
	    }
	
	    const props = this.getProps(promiseState);
	    return span({ className: "objectBox objectBox-object" }, this.getTitle(object), objectLink({
	      className: "objectLeftBrace",
	      object: object
	    }, " { "), ...props, objectLink({
	      className: "objectRightBrace",
	      object: object
	    }, " }"));
	  })
	});
	
	// Registration
	function supportsObject(object, type) {
	  if (!isGrip(object)) {
	    return false;
	  }
	  return type === "Promise";
	}
	
	// Exports from this module
	module.exports = {
	  rep: PromiseRep,
	  supportsObject: supportsObject
	};

/***/ },
/* 24 */
/***/ function(module, exports, __webpack_require__) {

	// ReactJS
	const React = __webpack_require__(1);
	
	// Reps
	const {
	  isGrip,
	  wrapRender
	} = __webpack_require__(4);
	
	// Shortcuts
	const { span } = React.DOM;
	
	/**
	 * Renders a grip object with regular expression.
	 */
	let RegExp = React.createClass({
	  displayName: "regexp",
	
	  propTypes: {
	    object: React.PropTypes.object.isRequired,
	    objectLink: React.PropTypes.func
	  },
	
	  getSource: function (grip) {
	    return grip.displayString;
	  },
	
	  render: wrapRender(function () {
	    let grip = this.props.object;
	    let objectLink = this.props.objectLink || span;
	
	    return span({ className: "objectBox objectBox-regexp" }, objectLink({
	      object: grip,
	      className: "regexpSource"
	    }, this.getSource(grip)));
	  })
	});
	
	// Registration
	
	function supportsObject(object, type) {
	  if (!isGrip(object)) {
	    return false;
	  }
	
	  return type == "RegExp";
	}
	
	// Exports from this module
	module.exports = {
	  rep: RegExp,
	  supportsObject: supportsObject
	};

/***/ },
/* 25 */
/***/ function(module, exports, __webpack_require__) {

	// ReactJS
	const React = __webpack_require__(1);
	
	// Reps
	const {
	  isGrip,
	  getURLDisplayString,
	  wrapRender
	} = __webpack_require__(4);
	
	// Shortcuts
	const DOM = React.DOM;
	
	/**
	 * Renders a grip representing CSSStyleSheet
	 */
	let StyleSheet = React.createClass({
	  displayName: "object",
	
	  propTypes: {
	    object: React.PropTypes.object.isRequired,
	    objectLink: React.PropTypes.func
	  },
	
	  getTitle: function (grip) {
	    let title = "StyleSheet ";
	    if (this.props.objectLink) {
	      return DOM.span({ className: "objectBox" }, this.props.objectLink({
	        object: grip
	      }, title));
	    }
	    return title;
	  },
	
	  getLocation: function (grip) {
	    // Embedded stylesheets don't have URL and so, no preview.
	    let url = grip.preview ? grip.preview.url : "";
	    return url ? getURLDisplayString(url) : "";
	  },
	
	  render: wrapRender(function () {
	    let grip = this.props.object;
	
	    return DOM.span({ className: "objectBox objectBox-object" }, this.getTitle(grip), DOM.span({ className: "objectPropValue" }, this.getLocation(grip)));
	  })
	});
	
	// Registration
	
	function supportsObject(object, type) {
	  if (!isGrip(object)) {
	    return false;
	  }
	
	  return type == "CSSStyleSheet";
	}
	
	// Exports from this module
	
	module.exports = {
	  rep: StyleSheet,
	  supportsObject: supportsObject
	};

/***/ },
/* 26 */
/***/ function(module, exports, __webpack_require__) {

	// Dependencies
	const React = __webpack_require__(1);
	const {
	  isGrip,
	  cropString,
	  cropMultipleLines,
	  wrapRender
	} = __webpack_require__(4);
	const { MODE } = __webpack_require__(2);
	const nodeConstants = __webpack_require__(27);
	
	// Shortcuts
	const { span } = React.DOM;
	
	/**
	 * Renders DOM comment node.
	 */
	const CommentNode = React.createClass({
	  displayName: "CommentNode",
	
	  propTypes: {
	    object: React.PropTypes.object.isRequired,
	    // @TODO Change this to Object.values once it's supported in Node's version of V8
	    mode: React.PropTypes.oneOf(Object.keys(MODE).map(key => MODE[key]))
	  },
	
	  render: wrapRender(function () {
	    let {
	      object,
	      mode = MODE.SHORT
	    } = this.props;
	
	    let { textContent } = object.preview;
	    if (mode === MODE.TINY) {
	      textContent = cropMultipleLines(textContent, 30);
	    } else if (mode === MODE.SHORT) {
	      textContent = cropString(textContent, 50);
	    }
	
	    return span({ className: "objectBox theme-comment" }, `<!-- ${textContent} -->`);
	  })
	});
	
	// Registration
	function supportsObject(object, type) {
	  if (!isGrip(object)) {
	    return false;
	  }
	  return object.preview && object.preview.nodeType === nodeConstants.COMMENT_NODE;
	}
	
	// Exports from this module
	module.exports = {
	  rep: CommentNode,
	  supportsObject: supportsObject
	};

/***/ },
/* 27 */
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
/* 28 */
/***/ function(module, exports, __webpack_require__) {

	// ReactJS
	const React = __webpack_require__(1);
	
	// Utils
	const {
	  isGrip,
	  wrapRender
	} = __webpack_require__(4);
	const { MODE } = __webpack_require__(2);
	const nodeConstants = __webpack_require__(27);
	
	// Shortcuts
	const { span } = React.DOM;
	
	/**
	 * Renders DOM element node.
	 */
	const ElementNode = React.createClass({
	  displayName: "ElementNode",
	
	  propTypes: {
	    object: React.PropTypes.object.isRequired,
	    // @TODO Change this to Object.values once it's supported in Node's version of V8
	    mode: React.PropTypes.oneOf(Object.keys(MODE).map(key => MODE[key])),
	    onDOMNodeMouseOver: React.PropTypes.func,
	    onDOMNodeMouseOut: React.PropTypes.func,
	    objectLink: React.PropTypes.func
	  },
	
	  getElements: function (grip, mode) {
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
	  },
	
	  render: wrapRender(function () {
	    let {
	      object,
	      mode,
	      onDOMNodeMouseOver,
	      onDOMNodeMouseOut
	    } = this.props;
	    let elements = this.getElements(object, mode);
	    let objectLink = this.props.objectLink || span;
	
	    let baseConfig = { className: "objectBox objectBox-node" };
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
	
	    return objectLink({ object }, span(baseConfig, ...elements));
	  })
	});
	
	// Registration
	function supportsObject(object, type) {
	  if (!isGrip(object)) {
	    return false;
	  }
	  return object.preview && object.preview.nodeType === nodeConstants.ELEMENT_NODE;
	}
	
	// Exports from this module
	module.exports = {
	  rep: ElementNode,
	  supportsObject: supportsObject
	};

/***/ },
/* 29 */
/***/ function(module, exports, __webpack_require__) {

	// ReactJS
	const React = __webpack_require__(1);
	
	// Reps
	const {
	  isGrip,
	  cropString,
	  wrapRender
	} = __webpack_require__(4);
	const { MODE } = __webpack_require__(2);
	
	// Shortcuts
	const DOM = React.DOM;
	
	/**
	 * Renders DOM #text node.
	 */
	let TextNode = React.createClass({
	  displayName: "TextNode",
	
	  propTypes: {
	    object: React.PropTypes.object.isRequired,
	    // @TODO Change this to Object.values once it's supported in Node's version of V8
	    mode: React.PropTypes.oneOf(Object.keys(MODE).map(key => MODE[key])),
	    objectLink: React.PropTypes.func,
	    onDOMNodeMouseOver: React.PropTypes.func,
	    onDOMNodeMouseOut: React.PropTypes.func
	  },
	
	  getTextContent: function (grip) {
	    return cropString(grip.preview.textContent);
	  },
	
	  getTitle: function (grip) {
	    const title = "#text";
	    if (this.props.objectLink) {
	      return this.props.objectLink({
	        object: grip
	      }, title);
	    }
	    return title;
	  },
	
	  render: wrapRender(function () {
	    let {
	      object: grip,
	      mode = MODE.SHORT
	    } = this.props;
	
	    let baseConfig = { className: "objectBox objectBox-textNode" };
	    if (this.props.onDOMNodeMouseOver) {
	      Object.assign(baseConfig, {
	        onMouseOver: _ => this.props.onDOMNodeMouseOver(grip)
	      });
	    }
	
	    if (this.props.onDOMNodeMouseOut) {
	      Object.assign(baseConfig, {
	        onMouseOut: this.props.onDOMNodeMouseOut
	      });
	    }
	
	    if (mode === MODE.TINY) {
	      return DOM.span(baseConfig, this.getTitle(grip));
	    }
	
	    return DOM.span(baseConfig, this.getTitle(grip), DOM.span({ className: "nodeValue" }, " ", `"${this.getTextContent(grip)}"`));
	  })
	});
	
	// Registration
	
	function supportsObject(grip, type) {
	  if (!isGrip(grip)) {
	    return false;
	  }
	
	  return grip.preview && grip.class == "Text";
	}
	
	// Exports from this module
	module.exports = {
	  rep: TextNode,
	  supportsObject: supportsObject
	};

/***/ },
/* 30 */
/***/ function(module, exports, __webpack_require__) {

	// ReactJS
	const React = __webpack_require__(1);
	// Utils
	const {
	  isGrip,
	  wrapRender
	} = __webpack_require__(4);
	const { MODE } = __webpack_require__(2);
	// Shortcuts
	const { span } = React.DOM;
	
	/**
	 * Renders Error objects.
	 */
	const ErrorRep = React.createClass({
	  displayName: "Error",
	
	  propTypes: {
	    object: React.PropTypes.object.isRequired,
	    // @TODO Change this to Object.values once it's supported in Node's version of V8
	    mode: React.PropTypes.oneOf(Object.keys(MODE).map(key => MODE[key])),
	    objectLink: React.PropTypes.func
	  },
	
	  render: wrapRender(function () {
	    let object = this.props.object;
	    let preview = object.preview;
	    let name = preview && preview.name ? preview.name : "Error";
	
	    let content = this.props.mode === MODE.TINY ? name : `${name}: ${preview.message}`;
	
	    if (preview.stack && this.props.mode !== MODE.TINY) {
	      /*
	       * Since Reps are used in the JSON Viewer, we can't localize
	       * the "Stack trace" label (defined in debugger.properties as
	       * "variablesViewErrorStacktrace" property), until Bug 1317038 lands.
	       */
	      content = `${content}\nStack trace:\n${preview.stack}`;
	    }
	
	    let objectLink = this.props.objectLink || span;
	    return objectLink({ object, className: "objectBox-stackTrace" }, span({}, content));
	  })
	});
	
	// Registration
	function supportsObject(object, type) {
	  if (!isGrip(object)) {
	    return false;
	  }
	  return object.preview && type === "Error";
	}
	
	// Exports from this module
	module.exports = {
	  rep: ErrorRep,
	  supportsObject: supportsObject
	};

/***/ },
/* 31 */
/***/ function(module, exports, __webpack_require__) {

	// ReactJS
	const React = __webpack_require__(1);
	
	// Reps
	const {
	  isGrip,
	  getURLDisplayString,
	  wrapRender
	} = __webpack_require__(4);
	
	// Shortcuts
	const DOM = React.DOM;
	
	/**
	 * Renders a grip representing a window.
	 */
	let Window = React.createClass({
	  displayName: "Window",
	
	  propTypes: {
	    object: React.PropTypes.object.isRequired,
	    objectLink: React.PropTypes.func
	  },
	
	  getTitle: function (grip) {
	    if (this.props.objectLink) {
	      return DOM.span({ className: "objectBox" }, this.props.objectLink({
	        object: grip
	      }, grip.class + " "));
	    }
	    return "";
	  },
	
	  getLocation: function (grip) {
	    return getURLDisplayString(grip.preview.url);
	  },
	
	  render: wrapRender(function () {
	    let grip = this.props.object;
	
	    return DOM.span({ className: "objectBox objectBox-Window" }, this.getTitle(grip), DOM.span({ className: "objectPropValue" }, this.getLocation(grip)));
	  })
	});
	
	// Registration
	
	function supportsObject(object, type) {
	  if (!isGrip(object)) {
	    return false;
	  }
	
	  return object.preview && type == "Window";
	}
	
	// Exports from this module
	module.exports = {
	  rep: Window,
	  supportsObject: supportsObject
	};

/***/ },
/* 32 */
/***/ function(module, exports, __webpack_require__) {

	// ReactJS
	const React = __webpack_require__(1);
	
	// Reps
	const {
	  isGrip,
	  wrapRender
	} = __webpack_require__(4);
	
	// Shortcuts
	const { span } = React.DOM;
	
	/**
	 * Renders a grip object with textual data.
	 */
	let ObjectWithText = React.createClass({
	  displayName: "ObjectWithText",
	
	  propTypes: {
	    object: React.PropTypes.object.isRequired,
	    objectLink: React.PropTypes.func
	  },
	
	  getTitle: function (grip) {
	    if (this.props.objectLink) {
	      return span({ className: "objectBox" }, this.props.objectLink({
	        object: grip
	      }, this.getType(grip) + " "));
	    }
	    return "";
	  },
	
	  getType: function (grip) {
	    return grip.class;
	  },
	
	  getDescription: function (grip) {
	    return "\"" + grip.preview.text + "\"";
	  },
	
	  render: wrapRender(function () {
	    let grip = this.props.object;
	    return span({ className: "objectBox objectBox-" + this.getType(grip) }, this.getTitle(grip), span({ className: "objectPropValue" }, this.getDescription(grip)));
	  })
	});
	
	// Registration
	
	function supportsObject(grip, type) {
	  if (!isGrip(grip)) {
	    return false;
	  }
	
	  return grip.preview && grip.preview.kind == "ObjectWithText";
	}
	
	// Exports from this module
	module.exports = {
	  rep: ObjectWithText,
	  supportsObject: supportsObject
	};

/***/ },
/* 33 */
/***/ function(module, exports, __webpack_require__) {

	// ReactJS
	const React = __webpack_require__(1);
	
	// Reps
	const {
	  isGrip,
	  getURLDisplayString,
	  wrapRender
	} = __webpack_require__(4);
	
	// Shortcuts
	const { span } = React.DOM;
	
	/**
	 * Renders a grip object with URL data.
	 */
	let ObjectWithURL = React.createClass({
	  displayName: "ObjectWithURL",
	
	  propTypes: {
	    object: React.PropTypes.object.isRequired,
	    objectLink: React.PropTypes.func
	  },
	
	  getTitle: function (grip) {
	    if (this.props.objectLink) {
	      return span({ className: "objectBox" }, this.props.objectLink({
	        object: grip
	      }, this.getType(grip) + " "));
	    }
	    return "";
	  },
	
	  getType: function (grip) {
	    return grip.class;
	  },
	
	  getDescription: function (grip) {
	    return getURLDisplayString(grip.preview.url);
	  },
	
	  render: wrapRender(function () {
	    let grip = this.props.object;
	    return span({ className: "objectBox objectBox-" + this.getType(grip) }, this.getTitle(grip), span({ className: "objectPropValue" }, this.getDescription(grip)));
	  })
	});
	
	// Registration
	
	function supportsObject(grip, type) {
	  if (!isGrip(grip)) {
	    return false;
	  }
	
	  return grip.preview && grip.preview.kind == "ObjectWithURL";
	}
	
	// Exports from this module
	module.exports = {
	  rep: ObjectWithURL,
	  supportsObject: supportsObject
	};

/***/ },
/* 34 */
/***/ function(module, exports, __webpack_require__) {

	// Dependencies
	const React = __webpack_require__(1);
	const {
	  createFactories,
	  isGrip,
	  wrapRender
	} = __webpack_require__(4);
	const Caption = React.createFactory(__webpack_require__(11));
	const { MODE } = __webpack_require__(2);
	
	// Shortcuts
	const { span } = React.DOM;
	
	/**
	 * Renders an array. The array is enclosed by left and right bracket
	 * and the max number of rendered items depends on the current mode.
	 */
	let GripArray = React.createClass({
	  displayName: "GripArray",
	
	  propTypes: {
	    object: React.PropTypes.object.isRequired,
	    // @TODO Change this to Object.values once it's supported in Node's version of V8
	    mode: React.PropTypes.oneOf(Object.keys(MODE).map(key => MODE[key])),
	    provider: React.PropTypes.object,
	    objectLink: React.PropTypes.func
	  },
	
	  getLength: function (grip) {
	    if (!grip.preview) {
	      return 0;
	    }
	
	    return grip.preview.length || grip.preview.childNodesLength || 0;
	  },
	
	  getTitle: function (object, context) {
	    let objectLink = this.props.objectLink || span;
	    if (this.props.mode !== MODE.TINY) {
	      return objectLink({
	        object: object
	      }, object.class + " ");
	    }
	    return "";
	  },
	
	  getPreviewItems: function (grip) {
	    if (!grip.preview) {
	      return null;
	    }
	
	    return grip.preview.items || grip.preview.childNodes || null;
	  },
	
	  arrayIterator: function (grip, max) {
	    let items = [];
	    const gripLength = this.getLength(grip);
	
	    if (!gripLength) {
	      return items;
	    }
	
	    const previewItems = this.getPreviewItems(grip);
	    if (!previewItems) {
	      return items;
	    }
	
	    let delim;
	    // number of grip preview items is limited to 10, but we may have more
	    // items in grip-array.
	    let delimMax = gripLength > previewItems.length ? previewItems.length : previewItems.length - 1;
	    let provider = this.props.provider;
	
	    for (let i = 0; i < previewItems.length && i < max; i++) {
	      try {
	        let itemGrip = previewItems[i];
	        let value = provider ? provider.getValue(itemGrip) : itemGrip;
	
	        delim = i == delimMax ? "" : ", ";
	
	        items.push(GripArrayItem(Object.assign({}, this.props, {
	          object: value,
	          delim: delim
	        })));
	      } catch (exc) {
	        items.push(GripArrayItem(Object.assign({}, this.props, {
	          object: exc,
	          delim: delim
	        })));
	      }
	    }
	    if (previewItems.length > max || gripLength > previewItems.length) {
	      let objectLink = this.props.objectLink || span;
	      let leftItemNum = gripLength - max > 0 ? gripLength - max : gripLength - previewItems.length;
	      items.push(Caption({
	        object: objectLink({
	          object: this.props.object
	        }, leftItemNum + " more…")
	      }));
	    }
	
	    return items;
	  },
	
	  render: wrapRender(function () {
	    let {
	      object,
	      mode = MODE.SHORT
	    } = this.props;
	
	    let items;
	    let brackets;
	    let needSpace = function (space) {
	      return space ? { left: "[ ", right: " ]" } : { left: "[", right: "]" };
	    };
	
	    if (mode === MODE.TINY) {
	      let objectLength = this.getLength(object);
	      let isEmpty = objectLength === 0;
	      items = [span({ className: "length" }, isEmpty ? "" : objectLength)];
	      brackets = needSpace(false);
	    } else {
	      let max = mode === MODE.SHORT ? 3 : 10;
	      items = this.arrayIterator(object, max);
	      brackets = needSpace(items.length > 0);
	    }
	
	    let objectLink = this.props.objectLink || span;
	    let title = this.getTitle(object);
	
	    return span({
	      className: "objectBox objectBox-array" }, title, objectLink({
	      className: "arrayLeftBracket",
	      object: object
	    }, brackets.left), ...items, objectLink({
	      className: "arrayRightBracket",
	      object: object
	    }, brackets.right), span({
	      className: "arrayProperties",
	      role: "group" }));
	  })
	});
	
	/**
	 * Renders array item. Individual values are separated by
	 * a delimiter (a comma by default).
	 */
	let GripArrayItem = React.createFactory(React.createClass({
	  displayName: "GripArrayItem",
	
	  propTypes: {
	    delim: React.PropTypes.string
	  },
	
	  render: function () {
	    let { Rep } = createFactories(__webpack_require__(3));
	
	    return span({}, Rep(Object.assign({}, this.props, {
	      mode: MODE.TINY
	    })), this.props.delim);
	  }
	}));
	
	function supportsObject(grip, type) {
	  if (!isGrip(grip)) {
	    return false;
	  }
	
	  return grip.preview && (grip.preview.kind == "ArrayLike" || type === "DocumentFragment");
	}
	
	// Exports from this module
	module.exports = {
	  rep: GripArray,
	  supportsObject: supportsObject
	};

/***/ },
/* 35 */
/***/ function(module, exports, __webpack_require__) {

	// Dependencies
	const React = __webpack_require__(1);
	const {
	  isGrip,
	  wrapRender
	} = __webpack_require__(4);
	const Caption = React.createFactory(__webpack_require__(11));
	const PropRep = React.createFactory(__webpack_require__(13));
	const { MODE } = __webpack_require__(2);
	// Shortcuts
	const { span } = React.DOM;
	/**
	 * Renders an map. A map is represented by a list of its
	 * entries enclosed in curly brackets.
	 */
	const GripMap = React.createClass({
	  displayName: "GripMap",
	
	  propTypes: {
	    object: React.PropTypes.object,
	    // @TODO Change this to Object.values once it's supported in Node's version of V8
	    mode: React.PropTypes.oneOf(Object.keys(MODE).map(key => MODE[key])),
	    objectLink: React.PropTypes.func,
	    isInterestingEntry: React.PropTypes.func
	  },
	
	  getTitle: function (object) {
	    let title = object && object.class ? object.class : "Map";
	    if (this.props.objectLink) {
	      return this.props.objectLink({
	        object: object
	      }, title);
	    }
	    return title;
	  },
	
	  safeEntriesIterator: function (object, max) {
	    max = typeof max === "undefined" ? 3 : max;
	    try {
	      return this.entriesIterator(object, max);
	    } catch (err) {
	      console.error(err);
	    }
	    return [];
	  },
	
	  entriesIterator: function (object, max) {
	    // Entry filter. Show only interesting entries to the user.
	    let isInterestingEntry = this.props.isInterestingEntry || ((type, value) => {
	      return type == "boolean" || type == "number" || type == "string" && value.length != 0;
	    });
	
	    let mapEntries = object.preview && object.preview.entries ? object.preview.entries : [];
	
	    let indexes = this.getEntriesIndexes(mapEntries, max, isInterestingEntry);
	    if (indexes.length < max && indexes.length < mapEntries.length) {
	      // There are not enough entries yet, so we add uninteresting entries.
	      indexes = indexes.concat(this.getEntriesIndexes(mapEntries, max - indexes.length, (t, value, name) => {
	        return !isInterestingEntry(t, value, name);
	      }));
	    }
	
	    let entries = this.getEntries(mapEntries, indexes);
	    if (entries.length < mapEntries.length) {
	      // There are some undisplayed entries. Then display "more…".
	      let objectLink = this.props.objectLink || span;
	
	      entries.push(Caption({
	        key: "more",
	        object: objectLink({
	          object: object
	        }, `${mapEntries.length - max} more…`)
	      }));
	    }
	
	    return entries;
	  },
	
	  /**
	   * Get entries ordered by index.
	   *
	   * @param {Array} entries Entries array.
	   * @param {Array} indexes Indexes of entries.
	   * @return {Array} Array of PropRep.
	   */
	  getEntries: function (entries, indexes) {
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
	        delim: i < indexes.length - 1 || indexes.length < entries.length ? ", " : "",
	        mode: MODE.TINY,
	        objectLink: this.props.objectLink
	      });
	    });
	  },
	
	  /**
	   * Get the indexes of entries in the map.
	   *
	   * @param {Array} entries Entries array.
	   * @param {Number} max The maximum length of indexes array.
	   * @param {Function} filter Filter the entry you want.
	   * @return {Array} Indexes of filtered entries in the map.
	   */
	  getEntriesIndexes: function (entries, max, filter) {
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
	  },
	
	  render: wrapRender(function () {
	    let object = this.props.object;
	    let props = this.safeEntriesIterator(object, this.props.mode === MODE.LONG ? 10 : 3);
	
	    let objectLink = this.props.objectLink || span;
	    if (this.props.mode === MODE.TINY) {
	      return span({ className: "objectBox objectBox-object" }, this.getTitle(object), objectLink({
	        className: "objectLeftBrace",
	        object: object
	      }, ""));
	    }
	
	    return span({ className: "objectBox objectBox-object" }, this.getTitle(object), objectLink({
	      className: "objectLeftBrace",
	      object: object
	    }, " { "), props, objectLink({
	      className: "objectRightBrace",
	      object: object
	    }, " }"));
	  })
	});
	
	function supportsObject(grip, type) {
	  if (!isGrip(grip)) {
	    return false;
	  }
	  return grip.preview && grip.preview.kind == "MapLike";
	}
	
	// Exports from this module
	module.exports = {
	  rep: GripMap,
	  supportsObject: supportsObject
	};

/***/ }
/******/ ])
});
;
//# sourceMappingURL=reps.js.map