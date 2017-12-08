var CodeMirror =
/******/ (function(modules) { // webpackBootstrap
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
/******/ 	__webpack_require__.p = "";

/******/ 	// Load entry module and return exports
/******/ 	return __webpack_require__(0);
/******/ })
/************************************************************************/
/******/ ([
/* 0 */
/***/ (function(module, exports, __webpack_require__) {

	__webpack_require__(1);
	__webpack_require__(3);
	__webpack_require__(4);
	__webpack_require__(5);
	__webpack_require__(6);
	__webpack_require__(7);
	__webpack_require__(8);
	__webpack_require__(9);
	__webpack_require__(10);
	__webpack_require__(11);
	__webpack_require__(12);
	__webpack_require__(13);
	__webpack_require__(14);
	__webpack_require__(15);
	__webpack_require__(16);
	__webpack_require__(17);
	__webpack_require__(18);
	__webpack_require__(19);
	__webpack_require__(20);
	__webpack_require__(21);
	__webpack_require__(22);
	__webpack_require__(23);
	__webpack_require__(24);
	__webpack_require__(25);
	__webpack_require__(26);
	module.exports = __webpack_require__(2);


/***/ }),
/* 1 */
/***/ (function(module, exports, __webpack_require__) {

	// CodeMirror, copyright (c) by Marijn Haverbeke and others
	// Distributed under an MIT license: http://codemirror.net/LICENSE

	// Open simple dialogs on top of an editor. Relies on dialog.css.

	(function(mod) {
	  if (true) // CommonJS
	    mod(__webpack_require__(2));
	  else if (typeof define == "function" && define.amd) // AMD
	    define(["../../lib/codemirror"], mod);
	  else // Plain browser env
	    mod(CodeMirror);
	})(function(CodeMirror) {
	  function dialogDiv(cm, template, bottom) {
	    var wrap = cm.getWrapperElement();
	    var dialog;
	    dialog = wrap.appendChild(document.createElement("div"));
	    if (bottom)
	      dialog.className = "CodeMirror-dialog CodeMirror-dialog-bottom";
	    else
	      dialog.className = "CodeMirror-dialog CodeMirror-dialog-top";

	    if (typeof template == "string") {
	      dialog.innerHTML = template;
	    } else { // Assuming it's a detached DOM element.
	      dialog.appendChild(template);
	    }
	    return dialog;
	  }

	  function closeNotification(cm, newVal) {
	    if (cm.state.currentNotificationClose)
	      cm.state.currentNotificationClose();
	    cm.state.currentNotificationClose = newVal;
	  }

	  CodeMirror.defineExtension("openDialog", function(template, callback, options) {
	    if (!options) options = {};

	    closeNotification(this, null);

	    var dialog = dialogDiv(this, template, options.bottom);
	    var closed = false, me = this;
	    function close(newVal) {
	      if (typeof newVal == 'string') {
	        inp.value = newVal;
	      } else {
	        if (closed) return;
	        closed = true;
	        dialog.parentNode.removeChild(dialog);
	        me.focus();

	        if (options.onClose) options.onClose(dialog);
	      }
	    }

	    var inp = dialog.getElementsByTagName("input")[0], button;
	    if (inp) {
	      inp.focus();

	      if (options.value) {
	        inp.value = options.value;
	        if (options.selectValueOnOpen !== false) {
	          inp.select();
	        }
	      }

	      if (options.onInput)
	        CodeMirror.on(inp, "input", function(e) { options.onInput(e, inp.value, close);});
	      if (options.onKeyUp)
	        CodeMirror.on(inp, "keyup", function(e) {options.onKeyUp(e, inp.value, close);});

	      CodeMirror.on(inp, "keydown", function(e) {
	        if (options && options.onKeyDown && options.onKeyDown(e, inp.value, close)) { return; }
	        if (e.keyCode == 27 || (options.closeOnEnter !== false && e.keyCode == 13)) {
	          inp.blur();
	          CodeMirror.e_stop(e);
	          close();
	        }
	        if (e.keyCode == 13) callback(inp.value, e);
	      });

	      if (options.closeOnBlur !== false) CodeMirror.on(inp, "blur", close);
	    } else if (button = dialog.getElementsByTagName("button")[0]) {
	      CodeMirror.on(button, "click", function() {
	        close();
	        me.focus();
	      });

	      if (options.closeOnBlur !== false) CodeMirror.on(button, "blur", close);

	      button.focus();
	    }
	    return close;
	  });

	  CodeMirror.defineExtension("openConfirm", function(template, callbacks, options) {
	    closeNotification(this, null);
	    var dialog = dialogDiv(this, template, options && options.bottom);
	    var buttons = dialog.getElementsByTagName("button");
	    var closed = false, me = this, blurring = 1;
	    function close() {
	      if (closed) return;
	      closed = true;
	      dialog.parentNode.removeChild(dialog);
	      me.focus();
	    }
	    buttons[0].focus();
	    for (var i = 0; i < buttons.length; ++i) {
	      var b = buttons[i];
	      (function(callback) {
	        CodeMirror.on(b, "click", function(e) {
	          CodeMirror.e_preventDefault(e);
	          close();
	          if (callback) callback(me);
	        });
	      })(callbacks[i]);
	      CodeMirror.on(b, "blur", function() {
	        --blurring;
	        setTimeout(function() { if (blurring <= 0) close(); }, 200);
	      });
	      CodeMirror.on(b, "focus", function() { ++blurring; });
	    }
	  });

	  /*
	   * openNotification
	   * Opens a notification, that can be closed with an optional timer
	   * (default 5000ms timer) and always closes on click.
	   *
	   * If a notification is opened while another is opened, it will close the
	   * currently opened one and open the new one immediately.
	   */
	  CodeMirror.defineExtension("openNotification", function(template, options) {
	    closeNotification(this, close);
	    var dialog = dialogDiv(this, template, options && options.bottom);
	    var closed = false, doneTimer;
	    var duration = options && typeof options.duration !== "undefined" ? options.duration : 5000;

	    function close() {
	      if (closed) return;
	      closed = true;
	      clearTimeout(doneTimer);
	      dialog.parentNode.removeChild(dialog);
	    }

	    CodeMirror.on(dialog, 'click', function(e) {
	      CodeMirror.e_preventDefault(e);
	      close();
	    });

	    if (duration)
	      doneTimer = setTimeout(close, duration);

	    return close;
	  });
	});


/***/ }),
/* 2 */
/***/ (function(module, exports, __webpack_require__) {

	// CodeMirror, copyright (c) by Marijn Haverbeke and others
	// Distributed under an MIT license: http://codemirror.net/LICENSE

	// This is CodeMirror (http://codemirror.net), a code editor
	// implemented in JavaScript on top of the browser's DOM.
	//
	// You can find some technical background for some of the code below
	// at http://marijnhaverbeke.nl/blog/#cm-internals .

	(function (global, factory) {
	   true ? module.exports = factory() :
	  typeof define === 'function' && define.amd ? define(factory) :
	  (global.CodeMirror = factory());
	}(this, (function () { 'use strict';

	// Kludges for bugs and behavior differences that can't be feature
	// detected are enabled based on userAgent etc sniffing.
	var userAgent = navigator.userAgent
	var platform = navigator.platform

	var gecko = /gecko\/\d/i.test(userAgent)
	var ie_upto10 = /MSIE \d/.test(userAgent)
	var ie_11up = /Trident\/(?:[7-9]|\d{2,})\..*rv:(\d+)/.exec(userAgent)
	var edge = /Edge\/(\d+)/.exec(userAgent)
	var ie = ie_upto10 || ie_11up || edge
	var ie_version = ie && (ie_upto10 ? document.documentMode || 6 : +(edge || ie_11up)[1])
	var webkit = !edge && /WebKit\//.test(userAgent)
	var qtwebkit = webkit && /Qt\/\d+\.\d+/.test(userAgent)
	var chrome = !edge && /Chrome\//.test(userAgent)
	var presto = /Opera\//.test(userAgent)
	var safari = /Apple Computer/.test(navigator.vendor)
	var mac_geMountainLion = /Mac OS X 1\d\D([8-9]|\d\d)\D/.test(userAgent)
	var phantom = /PhantomJS/.test(userAgent)

	var ios = !edge && /AppleWebKit/.test(userAgent) && /Mobile\/\w+/.test(userAgent)
	var android = /Android/.test(userAgent)
	// This is woefully incomplete. Suggestions for alternative methods welcome.
	var mobile = ios || android || /webOS|BlackBerry|Opera Mini|Opera Mobi|IEMobile/i.test(userAgent)
	var mac = ios || /Mac/.test(platform)
	var chromeOS = /\bCrOS\b/.test(userAgent)
	var windows = /win/i.test(platform)

	var presto_version = presto && userAgent.match(/Version\/(\d*\.\d*)/)
	if (presto_version) { presto_version = Number(presto_version[1]) }
	if (presto_version && presto_version >= 15) { presto = false; webkit = true }
	// Some browsers use the wrong event properties to signal cmd/ctrl on OS X
	var flipCtrlCmd = mac && (qtwebkit || presto && (presto_version == null || presto_version < 12.11))
	var captureRightClick = gecko || (ie && ie_version >= 9)

	function classTest(cls) { return new RegExp("(^|\\s)" + cls + "(?:$|\\s)\\s*") }

	var rmClass = function(node, cls) {
	  var current = node.className
	  var match = classTest(cls).exec(current)
	  if (match) {
	    var after = current.slice(match.index + match[0].length)
	    node.className = current.slice(0, match.index) + (after ? match[1] + after : "")
	  }
	}

	function removeChildren(e) {
	  for (var count = e.childNodes.length; count > 0; --count)
	    { e.removeChild(e.firstChild) }
	  return e
	}

	function removeChildrenAndAdd(parent, e) {
	  return removeChildren(parent).appendChild(e)
	}

	function elt(tag, content, className, style) {
	  var e = document.createElement(tag)
	  if (className) { e.className = className }
	  if (style) { e.style.cssText = style }
	  if (typeof content == "string") { e.appendChild(document.createTextNode(content)) }
	  else if (content) { for (var i = 0; i < content.length; ++i) { e.appendChild(content[i]) } }
	  return e
	}
	// wrapper for elt, which removes the elt from the accessibility tree
	function eltP(tag, content, className, style) {
	  var e = elt(tag, content, className, style)
	  e.setAttribute("role", "presentation")
	  return e
	}

	var range
	if (document.createRange) { range = function(node, start, end, endNode) {
	  var r = document.createRange()
	  r.setEnd(endNode || node, end)
	  r.setStart(node, start)
	  return r
	} }
	else { range = function(node, start, end) {
	  var r = document.body.createTextRange()
	  try { r.moveToElementText(node.parentNode) }
	  catch(e) { return r }
	  r.collapse(true)
	  r.moveEnd("character", end)
	  r.moveStart("character", start)
	  return r
	} }

	function contains(parent, child) {
	  if (child.nodeType == 3) // Android browser always returns false when child is a textnode
	    { child = child.parentNode }
	  if (parent.contains)
	    { return parent.contains(child) }
	  do {
	    if (child.nodeType == 11) { child = child.host }
	    if (child == parent) { return true }
	  } while (child = child.parentNode)
	}

	function activeElt() {
	  // IE and Edge may throw an "Unspecified Error" when accessing document.activeElement.
	  // IE < 10 will throw when accessed while the page is loading or in an iframe.
	  // IE > 9 and Edge will throw when accessed in an iframe if document.body is unavailable.
	  var activeElement
	  try {
	    activeElement = document.activeElement
	  } catch(e) {
	    activeElement = document.body || null
	  }
	  while (activeElement && activeElement.shadowRoot && activeElement.shadowRoot.activeElement)
	    { activeElement = activeElement.shadowRoot.activeElement }
	  return activeElement
	}

	function addClass(node, cls) {
	  var current = node.className
	  if (!classTest(cls).test(current)) { node.className += (current ? " " : "") + cls }
	}
	function joinClasses(a, b) {
	  var as = a.split(" ")
	  for (var i = 0; i < as.length; i++)
	    { if (as[i] && !classTest(as[i]).test(b)) { b += " " + as[i] } }
	  return b
	}

	var selectInput = function(node) { node.select() }
	if (ios) // Mobile Safari apparently has a bug where select() is broken.
	  { selectInput = function(node) { node.selectionStart = 0; node.selectionEnd = node.value.length } }
	else if (ie) // Suppress mysterious IE10 errors
	  { selectInput = function(node) { try { node.select() } catch(_e) {} } }

	function bind(f) {
	  var args = Array.prototype.slice.call(arguments, 1)
	  return function(){return f.apply(null, args)}
	}

	function copyObj(obj, target, overwrite) {
	  if (!target) { target = {} }
	  for (var prop in obj)
	    { if (obj.hasOwnProperty(prop) && (overwrite !== false || !target.hasOwnProperty(prop)))
	      { target[prop] = obj[prop] } }
	  return target
	}

	// Counts the column offset in a string, taking tabs into account.
	// Used mostly to find indentation.
	function countColumn(string, end, tabSize, startIndex, startValue) {
	  if (end == null) {
	    end = string.search(/[^\s\u00a0]/)
	    if (end == -1) { end = string.length }
	  }
	  for (var i = startIndex || 0, n = startValue || 0;;) {
	    var nextTab = string.indexOf("\t", i)
	    if (nextTab < 0 || nextTab >= end)
	      { return n + (end - i) }
	    n += nextTab - i
	    n += tabSize - (n % tabSize)
	    i = nextTab + 1
	  }
	}

	var Delayed = function() {this.id = null};
	Delayed.prototype.set = function (ms, f) {
	  clearTimeout(this.id)
	  this.id = setTimeout(f, ms)
	};

	function indexOf(array, elt) {
	  for (var i = 0; i < array.length; ++i)
	    { if (array[i] == elt) { return i } }
	  return -1
	}

	// Number of pixels added to scroller and sizer to hide scrollbar
	var scrollerGap = 30

	// Returned or thrown by various protocols to signal 'I'm not
	// handling this'.
	var Pass = {toString: function(){return "CodeMirror.Pass"}}

	// Reused option objects for setSelection & friends
	var sel_dontScroll = {scroll: false};
	var sel_mouse = {origin: "*mouse"};
	var sel_move = {origin: "+move"};
	// The inverse of countColumn -- find the offset that corresponds to
	// a particular column.
	function findColumn(string, goal, tabSize) {
	  for (var pos = 0, col = 0;;) {
	    var nextTab = string.indexOf("\t", pos)
	    if (nextTab == -1) { nextTab = string.length }
	    var skipped = nextTab - pos
	    if (nextTab == string.length || col + skipped >= goal)
	      { return pos + Math.min(skipped, goal - col) }
	    col += nextTab - pos
	    col += tabSize - (col % tabSize)
	    pos = nextTab + 1
	    if (col >= goal) { return pos }
	  }
	}

	var spaceStrs = [""]
	function spaceStr(n) {
	  while (spaceStrs.length <= n)
	    { spaceStrs.push(lst(spaceStrs) + " ") }
	  return spaceStrs[n]
	}

	function lst(arr) { return arr[arr.length-1] }

	function map(array, f) {
	  var out = []
	  for (var i = 0; i < array.length; i++) { out[i] = f(array[i], i) }
	  return out
	}

	function insertSorted(array, value, score) {
	  var pos = 0, priority = score(value)
	  while (pos < array.length && score(array[pos]) <= priority) { pos++ }
	  array.splice(pos, 0, value)
	}

	function nothing() {}

	function createObj(base, props) {
	  var inst
	  if (Object.create) {
	    inst = Object.create(base)
	  } else {
	    nothing.prototype = base
	    inst = new nothing()
	  }
	  if (props) { copyObj(props, inst) }
	  return inst
	}

	var nonASCIISingleCaseWordChar = /[\u00df\u0587\u0590-\u05f4\u0600-\u06ff\u3040-\u309f\u30a0-\u30ff\u3400-\u4db5\u4e00-\u9fcc\uac00-\ud7af]/
	function isWordCharBasic(ch) {
	  return /\w/.test(ch) || ch > "\x80" &&
	    (ch.toUpperCase() != ch.toLowerCase() || nonASCIISingleCaseWordChar.test(ch))
	}
	function isWordChar(ch, helper) {
	  if (!helper) { return isWordCharBasic(ch) }
	  if (helper.source.indexOf("\\w") > -1 && isWordCharBasic(ch)) { return true }
	  return helper.test(ch)
	}

	function isEmpty(obj) {
	  for (var n in obj) { if (obj.hasOwnProperty(n) && obj[n]) { return false } }
	  return true
	}

	// Extending unicode characters. A series of a non-extending char +
	// any number of extending chars is treated as a single unit as far
	// as editing and measuring is concerned. This is not fully correct,
	// since some scripts/fonts/browsers also treat other configurations
	// of code points as a group.
	var extendingChars = /[\u0300-\u036f\u0483-\u0489\u0591-\u05bd\u05bf\u05c1\u05c2\u05c4\u05c5\u05c7\u0610-\u061a\u064b-\u065e\u0670\u06d6-\u06dc\u06de-\u06e4\u06e7\u06e8\u06ea-\u06ed\u0711\u0730-\u074a\u07a6-\u07b0\u07eb-\u07f3\u0816-\u0819\u081b-\u0823\u0825-\u0827\u0829-\u082d\u0900-\u0902\u093c\u0941-\u0948\u094d\u0951-\u0955\u0962\u0963\u0981\u09bc\u09be\u09c1-\u09c4\u09cd\u09d7\u09e2\u09e3\u0a01\u0a02\u0a3c\u0a41\u0a42\u0a47\u0a48\u0a4b-\u0a4d\u0a51\u0a70\u0a71\u0a75\u0a81\u0a82\u0abc\u0ac1-\u0ac5\u0ac7\u0ac8\u0acd\u0ae2\u0ae3\u0b01\u0b3c\u0b3e\u0b3f\u0b41-\u0b44\u0b4d\u0b56\u0b57\u0b62\u0b63\u0b82\u0bbe\u0bc0\u0bcd\u0bd7\u0c3e-\u0c40\u0c46-\u0c48\u0c4a-\u0c4d\u0c55\u0c56\u0c62\u0c63\u0cbc\u0cbf\u0cc2\u0cc6\u0ccc\u0ccd\u0cd5\u0cd6\u0ce2\u0ce3\u0d3e\u0d41-\u0d44\u0d4d\u0d57\u0d62\u0d63\u0dca\u0dcf\u0dd2-\u0dd4\u0dd6\u0ddf\u0e31\u0e34-\u0e3a\u0e47-\u0e4e\u0eb1\u0eb4-\u0eb9\u0ebb\u0ebc\u0ec8-\u0ecd\u0f18\u0f19\u0f35\u0f37\u0f39\u0f71-\u0f7e\u0f80-\u0f84\u0f86\u0f87\u0f90-\u0f97\u0f99-\u0fbc\u0fc6\u102d-\u1030\u1032-\u1037\u1039\u103a\u103d\u103e\u1058\u1059\u105e-\u1060\u1071-\u1074\u1082\u1085\u1086\u108d\u109d\u135f\u1712-\u1714\u1732-\u1734\u1752\u1753\u1772\u1773\u17b7-\u17bd\u17c6\u17c9-\u17d3\u17dd\u180b-\u180d\u18a9\u1920-\u1922\u1927\u1928\u1932\u1939-\u193b\u1a17\u1a18\u1a56\u1a58-\u1a5e\u1a60\u1a62\u1a65-\u1a6c\u1a73-\u1a7c\u1a7f\u1b00-\u1b03\u1b34\u1b36-\u1b3a\u1b3c\u1b42\u1b6b-\u1b73\u1b80\u1b81\u1ba2-\u1ba5\u1ba8\u1ba9\u1c2c-\u1c33\u1c36\u1c37\u1cd0-\u1cd2\u1cd4-\u1ce0\u1ce2-\u1ce8\u1ced\u1dc0-\u1de6\u1dfd-\u1dff\u200c\u200d\u20d0-\u20f0\u2cef-\u2cf1\u2de0-\u2dff\u302a-\u302f\u3099\u309a\ua66f-\ua672\ua67c\ua67d\ua6f0\ua6f1\ua802\ua806\ua80b\ua825\ua826\ua8c4\ua8e0-\ua8f1\ua926-\ua92d\ua947-\ua951\ua980-\ua982\ua9b3\ua9b6-\ua9b9\ua9bc\uaa29-\uaa2e\uaa31\uaa32\uaa35\uaa36\uaa43\uaa4c\uaab0\uaab2-\uaab4\uaab7\uaab8\uaabe\uaabf\uaac1\uabe5\uabe8\uabed\udc00-\udfff\ufb1e\ufe00-\ufe0f\ufe20-\ufe26\uff9e\uff9f]/
	function isExtendingChar(ch) { return ch.charCodeAt(0) >= 768 && extendingChars.test(ch) }

	// Returns a number from the range [`0`; `str.length`] unless `pos` is outside that range.
	function skipExtendingChars(str, pos, dir) {
	  while ((dir < 0 ? pos > 0 : pos < str.length) && isExtendingChar(str.charAt(pos))) { pos += dir }
	  return pos
	}

	// Returns the value from the range [`from`; `to`] that satisfies
	// `pred` and is closest to `from`. Assumes that at least `to`
	// satisfies `pred`. Supports `from` being greater than `to`.
	function findFirst(pred, from, to) {
	  // At any point we are certain `to` satisfies `pred`, don't know
	  // whether `from` does.
	  var dir = from > to ? -1 : 1
	  for (;;) {
	    if (from == to) { return from }
	    var midF = (from + to) / 2, mid = dir < 0 ? Math.ceil(midF) : Math.floor(midF)
	    if (mid == from) { return pred(mid) ? from : to }
	    if (pred(mid)) { to = mid }
	    else { from = mid + dir }
	  }
	}

	// The display handles the DOM integration, both for input reading
	// and content drawing. It holds references to DOM nodes and
	// display-related state.

	function Display(place, doc, input) {
	  var d = this
	  this.input = input

	  // Covers bottom-right square when both scrollbars are present.
	  d.scrollbarFiller = elt("div", null, "CodeMirror-scrollbar-filler")
	  d.scrollbarFiller.setAttribute("cm-not-content", "true")
	  // Covers bottom of gutter when coverGutterNextToScrollbar is on
	  // and h scrollbar is present.
	  d.gutterFiller = elt("div", null, "CodeMirror-gutter-filler")
	  d.gutterFiller.setAttribute("cm-not-content", "true")
	  // Will contain the actual code, positioned to cover the viewport.
	  d.lineDiv = eltP("div", null, "CodeMirror-code")
	  // Elements are added to these to represent selection and cursors.
	  d.selectionDiv = elt("div", null, null, "position: relative; z-index: 1")
	  d.cursorDiv = elt("div", null, "CodeMirror-cursors")
	  // A visibility: hidden element used to find the size of things.
	  d.measure = elt("div", null, "CodeMirror-measure")
	  // When lines outside of the viewport are measured, they are drawn in this.
	  d.lineMeasure = elt("div", null, "CodeMirror-measure")
	  // Wraps everything that needs to exist inside the vertically-padded coordinate system
	  d.lineSpace = eltP("div", [d.measure, d.lineMeasure, d.selectionDiv, d.cursorDiv, d.lineDiv],
	                    null, "position: relative; outline: none")
	  var lines = eltP("div", [d.lineSpace], "CodeMirror-lines")
	  // Moved around its parent to cover visible view.
	  d.mover = elt("div", [lines], null, "position: relative")
	  // Set to the height of the document, allowing scrolling.
	  d.sizer = elt("div", [d.mover], "CodeMirror-sizer")
	  d.sizerWidth = null
	  // Behavior of elts with overflow: auto and padding is
	  // inconsistent across browsers. This is used to ensure the
	  // scrollable area is big enough.
	  d.heightForcer = elt("div", null, null, "position: absolute; height: " + scrollerGap + "px; width: 1px;")
	  // Will contain the gutters, if any.
	  d.gutters = elt("div", null, "CodeMirror-gutters")
	  d.lineGutter = null
	  // Actual scrollable element.
	  d.scroller = elt("div", [d.sizer, d.heightForcer, d.gutters], "CodeMirror-scroll")
	  d.scroller.setAttribute("tabIndex", "-1")
	  // The element in which the editor lives.
	  d.wrapper = elt("div", [d.scrollbarFiller, d.gutterFiller, d.scroller], "CodeMirror")

	  // Work around IE7 z-index bug (not perfect, hence IE7 not really being supported)
	  if (ie && ie_version < 8) { d.gutters.style.zIndex = -1; d.scroller.style.paddingRight = 0 }
	  if (!webkit && !(gecko && mobile)) { d.scroller.draggable = true }

	  if (place) {
	    if (place.appendChild) { place.appendChild(d.wrapper) }
	    else { place(d.wrapper) }
	  }

	  // Current rendered range (may be bigger than the view window).
	  d.viewFrom = d.viewTo = doc.first
	  d.reportedViewFrom = d.reportedViewTo = doc.first
	  // Information about the rendered lines.
	  d.view = []
	  d.renderedView = null
	  // Holds info about a single rendered line when it was rendered
	  // for measurement, while not in view.
	  d.externalMeasured = null
	  // Empty space (in pixels) above the view
	  d.viewOffset = 0
	  d.lastWrapHeight = d.lastWrapWidth = 0
	  d.updateLineNumbers = null

	  d.nativeBarWidth = d.barHeight = d.barWidth = 0
	  d.scrollbarsClipped = false

	  // Used to only resize the line number gutter when necessary (when
	  // the amount of lines crosses a boundary that makes its width change)
	  d.lineNumWidth = d.lineNumInnerWidth = d.lineNumChars = null
	  // Set to true when a non-horizontal-scrolling line widget is
	  // added. As an optimization, line widget aligning is skipped when
	  // this is false.
	  d.alignWidgets = false

	  d.cachedCharWidth = d.cachedTextHeight = d.cachedPaddingH = null

	  // Tracks the maximum line length so that the horizontal scrollbar
	  // can be kept static when scrolling.
	  d.maxLine = null
	  d.maxLineLength = 0
	  d.maxLineChanged = false

	  // Used for measuring wheel scrolling granularity
	  d.wheelDX = d.wheelDY = d.wheelStartX = d.wheelStartY = null

	  // True when shift is held down.
	  d.shift = false

	  // Used to track whether anything happened since the context menu
	  // was opened.
	  d.selForContextMenu = null

	  d.activeTouch = null

	  input.init(d)
	}

	// Find the line object corresponding to the given line number.
	function getLine(doc, n) {
	  n -= doc.first
	  if (n < 0 || n >= doc.size) { throw new Error("There is no line " + (n + doc.first) + " in the document.") }
	  var chunk = doc
	  while (!chunk.lines) {
	    for (var i = 0;; ++i) {
	      var child = chunk.children[i], sz = child.chunkSize()
	      if (n < sz) { chunk = child; break }
	      n -= sz
	    }
	  }
	  return chunk.lines[n]
	}

	// Get the part of a document between two positions, as an array of
	// strings.
	function getBetween(doc, start, end) {
	  var out = [], n = start.line
	  doc.iter(start.line, end.line + 1, function (line) {
	    var text = line.text
	    if (n == end.line) { text = text.slice(0, end.ch) }
	    if (n == start.line) { text = text.slice(start.ch) }
	    out.push(text)
	    ++n
	  })
	  return out
	}
	// Get the lines between from and to, as array of strings.
	function getLines(doc, from, to) {
	  var out = []
	  doc.iter(from, to, function (line) { out.push(line.text) }) // iter aborts when callback returns truthy value
	  return out
	}

	// Update the height of a line, propagating the height change
	// upwards to parent nodes.
	function updateLineHeight(line, height) {
	  var diff = height - line.height
	  if (diff) { for (var n = line; n; n = n.parent) { n.height += diff } }
	}

	// Given a line object, find its line number by walking up through
	// its parent links.
	function lineNo(line) {
	  if (line.parent == null) { return null }
	  var cur = line.parent, no = indexOf(cur.lines, line)
	  for (var chunk = cur.parent; chunk; cur = chunk, chunk = chunk.parent) {
	    for (var i = 0;; ++i) {
	      if (chunk.children[i] == cur) { break }
	      no += chunk.children[i].chunkSize()
	    }
	  }
	  return no + cur.first
	}

	// Find the line at the given vertical position, using the height
	// information in the document tree.
	function lineAtHeight(chunk, h) {
	  var n = chunk.first
	  outer: do {
	    for (var i$1 = 0; i$1 < chunk.children.length; ++i$1) {
	      var child = chunk.children[i$1], ch = child.height
	      if (h < ch) { chunk = child; continue outer }
	      h -= ch
	      n += child.chunkSize()
	    }
	    return n
	  } while (!chunk.lines)
	  var i = 0
	  for (; i < chunk.lines.length; ++i) {
	    var line = chunk.lines[i], lh = line.height
	    if (h < lh) { break }
	    h -= lh
	  }
	  return n + i
	}

	function isLine(doc, l) {return l >= doc.first && l < doc.first + doc.size}

	function lineNumberFor(options, i) {
	  return String(options.lineNumberFormatter(i + options.firstLineNumber))
	}

	// A Pos instance represents a position within the text.
	function Pos(line, ch, sticky) {
	  if ( sticky === void 0 ) sticky = null;

	  if (!(this instanceof Pos)) { return new Pos(line, ch, sticky) }
	  this.line = line
	  this.ch = ch
	  this.sticky = sticky
	}

	// Compare two positions, return 0 if they are the same, a negative
	// number when a is less, and a positive number otherwise.
	function cmp(a, b) { return a.line - b.line || a.ch - b.ch }

	function equalCursorPos(a, b) { return a.sticky == b.sticky && cmp(a, b) == 0 }

	function copyPos(x) {return Pos(x.line, x.ch)}
	function maxPos(a, b) { return cmp(a, b) < 0 ? b : a }
	function minPos(a, b) { return cmp(a, b) < 0 ? a : b }

	// Most of the external API clips given positions to make sure they
	// actually exist within the document.
	function clipLine(doc, n) {return Math.max(doc.first, Math.min(n, doc.first + doc.size - 1))}
	function clipPos(doc, pos) {
	  if (pos.line < doc.first) { return Pos(doc.first, 0) }
	  var last = doc.first + doc.size - 1
	  if (pos.line > last) { return Pos(last, getLine(doc, last).text.length) }
	  return clipToLen(pos, getLine(doc, pos.line).text.length)
	}
	function clipToLen(pos, linelen) {
	  var ch = pos.ch
	  if (ch == null || ch > linelen) { return Pos(pos.line, linelen) }
	  else if (ch < 0) { return Pos(pos.line, 0) }
	  else { return pos }
	}
	function clipPosArray(doc, array) {
	  var out = []
	  for (var i = 0; i < array.length; i++) { out[i] = clipPos(doc, array[i]) }
	  return out
	}

	// Optimize some code when these features are not used.
	var sawReadOnlySpans = false;
	var sawCollapsedSpans = false;
	function seeReadOnlySpans() {
	  sawReadOnlySpans = true
	}

	function seeCollapsedSpans() {
	  sawCollapsedSpans = true
	}

	// TEXTMARKER SPANS

	function MarkedSpan(marker, from, to) {
	  this.marker = marker
	  this.from = from; this.to = to
	}

	// Search an array of spans for a span matching the given marker.
	function getMarkedSpanFor(spans, marker) {
	  if (spans) { for (var i = 0; i < spans.length; ++i) {
	    var span = spans[i]
	    if (span.marker == marker) { return span }
	  } }
	}
	// Remove a span from an array, returning undefined if no spans are
	// left (we don't store arrays for lines without spans).
	function removeMarkedSpan(spans, span) {
	  var r
	  for (var i = 0; i < spans.length; ++i)
	    { if (spans[i] != span) { (r || (r = [])).push(spans[i]) } }
	  return r
	}
	// Add a span to a line.
	function addMarkedSpan(line, span) {
	  line.markedSpans = line.markedSpans ? line.markedSpans.concat([span]) : [span]
	  span.marker.attachLine(line)
	}

	// Used for the algorithm that adjusts markers for a change in the
	// document. These functions cut an array of spans at a given
	// character position, returning an array of remaining chunks (or
	// undefined if nothing remains).
	function markedSpansBefore(old, startCh, isInsert) {
	  var nw
	  if (old) { for (var i = 0; i < old.length; ++i) {
	    var span = old[i], marker = span.marker
	    var startsBefore = span.from == null || (marker.inclusiveLeft ? span.from <= startCh : span.from < startCh)
	    if (startsBefore || span.from == startCh && marker.type == "bookmark" && (!isInsert || !span.marker.insertLeft)) {
	      var endsAfter = span.to == null || (marker.inclusiveRight ? span.to >= startCh : span.to > startCh)
	      ;(nw || (nw = [])).push(new MarkedSpan(marker, span.from, endsAfter ? null : span.to))
	    }
	  } }
	  return nw
	}
	function markedSpansAfter(old, endCh, isInsert) {
	  var nw
	  if (old) { for (var i = 0; i < old.length; ++i) {
	    var span = old[i], marker = span.marker
	    var endsAfter = span.to == null || (marker.inclusiveRight ? span.to >= endCh : span.to > endCh)
	    if (endsAfter || span.from == endCh && marker.type == "bookmark" && (!isInsert || span.marker.insertLeft)) {
	      var startsBefore = span.from == null || (marker.inclusiveLeft ? span.from <= endCh : span.from < endCh)
	      ;(nw || (nw = [])).push(new MarkedSpan(marker, startsBefore ? null : span.from - endCh,
	                                            span.to == null ? null : span.to - endCh))
	    }
	  } }
	  return nw
	}

	// Given a change object, compute the new set of marker spans that
	// cover the line in which the change took place. Removes spans
	// entirely within the change, reconnects spans belonging to the
	// same marker that appear on both sides of the change, and cuts off
	// spans partially within the change. Returns an array of span
	// arrays with one element for each line in (after) the change.
	function stretchSpansOverChange(doc, change) {
	  if (change.full) { return null }
	  var oldFirst = isLine(doc, change.from.line) && getLine(doc, change.from.line).markedSpans
	  var oldLast = isLine(doc, change.to.line) && getLine(doc, change.to.line).markedSpans
	  if (!oldFirst && !oldLast) { return null }

	  var startCh = change.from.ch, endCh = change.to.ch, isInsert = cmp(change.from, change.to) == 0
	  // Get the spans that 'stick out' on both sides
	  var first = markedSpansBefore(oldFirst, startCh, isInsert)
	  var last = markedSpansAfter(oldLast, endCh, isInsert)

	  // Next, merge those two ends
	  var sameLine = change.text.length == 1, offset = lst(change.text).length + (sameLine ? startCh : 0)
	  if (first) {
	    // Fix up .to properties of first
	    for (var i = 0; i < first.length; ++i) {
	      var span = first[i]
	      if (span.to == null) {
	        var found = getMarkedSpanFor(last, span.marker)
	        if (!found) { span.to = startCh }
	        else if (sameLine) { span.to = found.to == null ? null : found.to + offset }
	      }
	    }
	  }
	  if (last) {
	    // Fix up .from in last (or move them into first in case of sameLine)
	    for (var i$1 = 0; i$1 < last.length; ++i$1) {
	      var span$1 = last[i$1]
	      if (span$1.to != null) { span$1.to += offset }
	      if (span$1.from == null) {
	        var found$1 = getMarkedSpanFor(first, span$1.marker)
	        if (!found$1) {
	          span$1.from = offset
	          if (sameLine) { (first || (first = [])).push(span$1) }
	        }
	      } else {
	        span$1.from += offset
	        if (sameLine) { (first || (first = [])).push(span$1) }
	      }
	    }
	  }
	  // Make sure we didn't create any zero-length spans
	  if (first) { first = clearEmptySpans(first) }
	  if (last && last != first) { last = clearEmptySpans(last) }

	  var newMarkers = [first]
	  if (!sameLine) {
	    // Fill gap with whole-line-spans
	    var gap = change.text.length - 2, gapMarkers
	    if (gap > 0 && first)
	      { for (var i$2 = 0; i$2 < first.length; ++i$2)
	        { if (first[i$2].to == null)
	          { (gapMarkers || (gapMarkers = [])).push(new MarkedSpan(first[i$2].marker, null, null)) } } }
	    for (var i$3 = 0; i$3 < gap; ++i$3)
	      { newMarkers.push(gapMarkers) }
	    newMarkers.push(last)
	  }
	  return newMarkers
	}

	// Remove spans that are empty and don't have a clearWhenEmpty
	// option of false.
	function clearEmptySpans(spans) {
	  for (var i = 0; i < spans.length; ++i) {
	    var span = spans[i]
	    if (span.from != null && span.from == span.to && span.marker.clearWhenEmpty !== false)
	      { spans.splice(i--, 1) }
	  }
	  if (!spans.length) { return null }
	  return spans
	}

	// Used to 'clip' out readOnly ranges when making a change.
	function removeReadOnlyRanges(doc, from, to) {
	  var markers = null
	  doc.iter(from.line, to.line + 1, function (line) {
	    if (line.markedSpans) { for (var i = 0; i < line.markedSpans.length; ++i) {
	      var mark = line.markedSpans[i].marker
	      if (mark.readOnly && (!markers || indexOf(markers, mark) == -1))
	        { (markers || (markers = [])).push(mark) }
	    } }
	  })
	  if (!markers) { return null }
	  var parts = [{from: from, to: to}]
	  for (var i = 0; i < markers.length; ++i) {
	    var mk = markers[i], m = mk.find(0)
	    for (var j = 0; j < parts.length; ++j) {
	      var p = parts[j]
	      if (cmp(p.to, m.from) < 0 || cmp(p.from, m.to) > 0) { continue }
	      var newParts = [j, 1], dfrom = cmp(p.from, m.from), dto = cmp(p.to, m.to)
	      if (dfrom < 0 || !mk.inclusiveLeft && !dfrom)
	        { newParts.push({from: p.from, to: m.from}) }
	      if (dto > 0 || !mk.inclusiveRight && !dto)
	        { newParts.push({from: m.to, to: p.to}) }
	      parts.splice.apply(parts, newParts)
	      j += newParts.length - 3
	    }
	  }
	  return parts
	}

	// Connect or disconnect spans from a line.
	function detachMarkedSpans(line) {
	  var spans = line.markedSpans
	  if (!spans) { return }
	  for (var i = 0; i < spans.length; ++i)
	    { spans[i].marker.detachLine(line) }
	  line.markedSpans = null
	}
	function attachMarkedSpans(line, spans) {
	  if (!spans) { return }
	  for (var i = 0; i < spans.length; ++i)
	    { spans[i].marker.attachLine(line) }
	  line.markedSpans = spans
	}

	// Helpers used when computing which overlapping collapsed span
	// counts as the larger one.
	function extraLeft(marker) { return marker.inclusiveLeft ? -1 : 0 }
	function extraRight(marker) { return marker.inclusiveRight ? 1 : 0 }

	// Returns a number indicating which of two overlapping collapsed
	// spans is larger (and thus includes the other). Falls back to
	// comparing ids when the spans cover exactly the same range.
	function compareCollapsedMarkers(a, b) {
	  var lenDiff = a.lines.length - b.lines.length
	  if (lenDiff != 0) { return lenDiff }
	  var aPos = a.find(), bPos = b.find()
	  var fromCmp = cmp(aPos.from, bPos.from) || extraLeft(a) - extraLeft(b)
	  if (fromCmp) { return -fromCmp }
	  var toCmp = cmp(aPos.to, bPos.to) || extraRight(a) - extraRight(b)
	  if (toCmp) { return toCmp }
	  return b.id - a.id
	}

	// Find out whether a line ends or starts in a collapsed span. If
	// so, return the marker for that span.
	function collapsedSpanAtSide(line, start) {
	  var sps = sawCollapsedSpans && line.markedSpans, found
	  if (sps) { for (var sp = (void 0), i = 0; i < sps.length; ++i) {
	    sp = sps[i]
	    if (sp.marker.collapsed && (start ? sp.from : sp.to) == null &&
	        (!found || compareCollapsedMarkers(found, sp.marker) < 0))
	      { found = sp.marker }
	  } }
	  return found
	}
	function collapsedSpanAtStart(line) { return collapsedSpanAtSide(line, true) }
	function collapsedSpanAtEnd(line) { return collapsedSpanAtSide(line, false) }

	// Test whether there exists a collapsed span that partially
	// overlaps (covers the start or end, but not both) of a new span.
	// Such overlap is not allowed.
	function conflictingCollapsedRange(doc, lineNo, from, to, marker) {
	  var line = getLine(doc, lineNo)
	  var sps = sawCollapsedSpans && line.markedSpans
	  if (sps) { for (var i = 0; i < sps.length; ++i) {
	    var sp = sps[i]
	    if (!sp.marker.collapsed) { continue }
	    var found = sp.marker.find(0)
	    var fromCmp = cmp(found.from, from) || extraLeft(sp.marker) - extraLeft(marker)
	    var toCmp = cmp(found.to, to) || extraRight(sp.marker) - extraRight(marker)
	    if (fromCmp >= 0 && toCmp <= 0 || fromCmp <= 0 && toCmp >= 0) { continue }
	    if (fromCmp <= 0 && (sp.marker.inclusiveRight && marker.inclusiveLeft ? cmp(found.to, from) >= 0 : cmp(found.to, from) > 0) ||
	        fromCmp >= 0 && (sp.marker.inclusiveRight && marker.inclusiveLeft ? cmp(found.from, to) <= 0 : cmp(found.from, to) < 0))
	      { return true }
	  } }
	}

	// A visual line is a line as drawn on the screen. Folding, for
	// example, can cause multiple logical lines to appear on the same
	// visual line. This finds the start of the visual line that the
	// given line is part of (usually that is the line itself).
	function visualLine(line) {
	  var merged
	  while (merged = collapsedSpanAtStart(line))
	    { line = merged.find(-1, true).line }
	  return line
	}

	function visualLineEnd(line) {
	  var merged
	  while (merged = collapsedSpanAtEnd(line))
	    { line = merged.find(1, true).line }
	  return line
	}

	// Returns an array of logical lines that continue the visual line
	// started by the argument, or undefined if there are no such lines.
	function visualLineContinued(line) {
	  var merged, lines
	  while (merged = collapsedSpanAtEnd(line)) {
	    line = merged.find(1, true).line
	    ;(lines || (lines = [])).push(line)
	  }
	  return lines
	}

	// Get the line number of the start of the visual line that the
	// given line number is part of.
	function visualLineNo(doc, lineN) {
	  var line = getLine(doc, lineN), vis = visualLine(line)
	  if (line == vis) { return lineN }
	  return lineNo(vis)
	}

	// Get the line number of the start of the next visual line after
	// the given line.
	function visualLineEndNo(doc, lineN) {
	  if (lineN > doc.lastLine()) { return lineN }
	  var line = getLine(doc, lineN), merged
	  if (!lineIsHidden(doc, line)) { return lineN }
	  while (merged = collapsedSpanAtEnd(line))
	    { line = merged.find(1, true).line }
	  return lineNo(line) + 1
	}

	// Compute whether a line is hidden. Lines count as hidden when they
	// are part of a visual line that starts with another line, or when
	// they are entirely covered by collapsed, non-widget span.
	function lineIsHidden(doc, line) {
	  var sps = sawCollapsedSpans && line.markedSpans
	  if (sps) { for (var sp = (void 0), i = 0; i < sps.length; ++i) {
	    sp = sps[i]
	    if (!sp.marker.collapsed) { continue }
	    if (sp.from == null) { return true }
	    if (sp.marker.widgetNode) { continue }
	    if (sp.from == 0 && sp.marker.inclusiveLeft && lineIsHiddenInner(doc, line, sp))
	      { return true }
	  } }
	}
	function lineIsHiddenInner(doc, line, span) {
	  if (span.to == null) {
	    var end = span.marker.find(1, true)
	    return lineIsHiddenInner(doc, end.line, getMarkedSpanFor(end.line.markedSpans, span.marker))
	  }
	  if (span.marker.inclusiveRight && span.to == line.text.length)
	    { return true }
	  for (var sp = (void 0), i = 0; i < line.markedSpans.length; ++i) {
	    sp = line.markedSpans[i]
	    if (sp.marker.collapsed && !sp.marker.widgetNode && sp.from == span.to &&
	        (sp.to == null || sp.to != span.from) &&
	        (sp.marker.inclusiveLeft || span.marker.inclusiveRight) &&
	        lineIsHiddenInner(doc, line, sp)) { return true }
	  }
	}

	// Find the height above the given line.
	function heightAtLine(lineObj) {
	  lineObj = visualLine(lineObj)

	  var h = 0, chunk = lineObj.parent
	  for (var i = 0; i < chunk.lines.length; ++i) {
	    var line = chunk.lines[i]
	    if (line == lineObj) { break }
	    else { h += line.height }
	  }
	  for (var p = chunk.parent; p; chunk = p, p = chunk.parent) {
	    for (var i$1 = 0; i$1 < p.children.length; ++i$1) {
	      var cur = p.children[i$1]
	      if (cur == chunk) { break }
	      else { h += cur.height }
	    }
	  }
	  return h
	}

	// Compute the character length of a line, taking into account
	// collapsed ranges (see markText) that might hide parts, and join
	// other lines onto it.
	function lineLength(line) {
	  if (line.height == 0) { return 0 }
	  var len = line.text.length, merged, cur = line
	  while (merged = collapsedSpanAtStart(cur)) {
	    var found = merged.find(0, true)
	    cur = found.from.line
	    len += found.from.ch - found.to.ch
	  }
	  cur = line
	  while (merged = collapsedSpanAtEnd(cur)) {
	    var found$1 = merged.find(0, true)
	    len -= cur.text.length - found$1.from.ch
	    cur = found$1.to.line
	    len += cur.text.length - found$1.to.ch
	  }
	  return len
	}

	// Find the longest line in the document.
	function findMaxLine(cm) {
	  var d = cm.display, doc = cm.doc
	  d.maxLine = getLine(doc, doc.first)
	  d.maxLineLength = lineLength(d.maxLine)
	  d.maxLineChanged = true
	  doc.iter(function (line) {
	    var len = lineLength(line)
	    if (len > d.maxLineLength) {
	      d.maxLineLength = len
	      d.maxLine = line
	    }
	  })
	}

	// BIDI HELPERS

	function iterateBidiSections(order, from, to, f) {
	  if (!order) { return f(from, to, "ltr", 0) }
	  var found = false
	  for (var i = 0; i < order.length; ++i) {
	    var part = order[i]
	    if (part.from < to && part.to > from || from == to && part.to == from) {
	      f(Math.max(part.from, from), Math.min(part.to, to), part.level == 1 ? "rtl" : "ltr", i)
	      found = true
	    }
	  }
	  if (!found) { f(from, to, "ltr") }
	}

	var bidiOther = null
	function getBidiPartAt(order, ch, sticky) {
	  var found
	  bidiOther = null
	  for (var i = 0; i < order.length; ++i) {
	    var cur = order[i]
	    if (cur.from < ch && cur.to > ch) { return i }
	    if (cur.to == ch) {
	      if (cur.from != cur.to && sticky == "before") { found = i }
	      else { bidiOther = i }
	    }
	    if (cur.from == ch) {
	      if (cur.from != cur.to && sticky != "before") { found = i }
	      else { bidiOther = i }
	    }
	  }
	  return found != null ? found : bidiOther
	}

	// Bidirectional ordering algorithm
	// See http://unicode.org/reports/tr9/tr9-13.html for the algorithm
	// that this (partially) implements.

	// One-char codes used for character types:
	// L (L):   Left-to-Right
	// R (R):   Right-to-Left
	// r (AL):  Right-to-Left Arabic
	// 1 (EN):  European Number
	// + (ES):  European Number Separator
	// % (ET):  European Number Terminator
	// n (AN):  Arabic Number
	// , (CS):  Common Number Separator
	// m (NSM): Non-Spacing Mark
	// b (BN):  Boundary Neutral
	// s (B):   Paragraph Separator
	// t (S):   Segment Separator
	// w (WS):  Whitespace
	// N (ON):  Other Neutrals

	// Returns null if characters are ordered as they appear
	// (left-to-right), or an array of sections ({from, to, level}
	// objects) in the order in which they occur visually.
	var bidiOrdering = (function() {
	  // Character types for codepoints 0 to 0xff
	  var lowTypes = "bbbbbbbbbtstwsbbbbbbbbbbbbbbssstwNN%%%NNNNNN,N,N1111111111NNNNNNNLLLLLLLLLLLLLLLLLLLLLLLLLLNNNNNNLLLLLLLLLLLLLLLLLLLLLLLLLLNNNNbbbbbbsbbbbbbbbbbbbbbbbbbbbbbbbbb,N%%%%NNNNLNNNNN%%11NLNNN1LNNNNNLLLLLLLLLLLLLLLLLLLLLLLNLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLN"
	  // Character types for codepoints 0x600 to 0x6f9
	  var arabicTypes = "nnnnnnNNr%%r,rNNmmmmmmmmmmmrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrmmmmmmmmmmmmmmmmmmmmmnnnnnnnnnn%nnrrrmrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrmmmmmmmnNmmmmmmrrmmNmmmmrr1111111111"
	  function charType(code) {
	    if (code <= 0xf7) { return lowTypes.charAt(code) }
	    else if (0x590 <= code && code <= 0x5f4) { return "R" }
	    else if (0x600 <= code && code <= 0x6f9) { return arabicTypes.charAt(code - 0x600) }
	    else if (0x6ee <= code && code <= 0x8ac) { return "r" }
	    else if (0x2000 <= code && code <= 0x200b) { return "w" }
	    else if (code == 0x200c) { return "b" }
	    else { return "L" }
	  }

	  var bidiRE = /[\u0590-\u05f4\u0600-\u06ff\u0700-\u08ac]/
	  var isNeutral = /[stwN]/, isStrong = /[LRr]/, countsAsLeft = /[Lb1n]/, countsAsNum = /[1n]/

	  function BidiSpan(level, from, to) {
	    this.level = level
	    this.from = from; this.to = to
	  }

	  return function(str, direction) {
	    var outerType = direction == "ltr" ? "L" : "R"

	    if (str.length == 0 || direction == "ltr" && !bidiRE.test(str)) { return false }
	    var len = str.length, types = []
	    for (var i = 0; i < len; ++i)
	      { types.push(charType(str.charCodeAt(i))) }

	    // W1. Examine each non-spacing mark (NSM) in the level run, and
	    // change the type of the NSM to the type of the previous
	    // character. If the NSM is at the start of the level run, it will
	    // get the type of sor.
	    for (var i$1 = 0, prev = outerType; i$1 < len; ++i$1) {
	      var type = types[i$1]
	      if (type == "m") { types[i$1] = prev }
	      else { prev = type }
	    }

	    // W2. Search backwards from each instance of a European number
	    // until the first strong type (R, L, AL, or sor) is found. If an
	    // AL is found, change the type of the European number to Arabic
	    // number.
	    // W3. Change all ALs to R.
	    for (var i$2 = 0, cur = outerType; i$2 < len; ++i$2) {
	      var type$1 = types[i$2]
	      if (type$1 == "1" && cur == "r") { types[i$2] = "n" }
	      else if (isStrong.test(type$1)) { cur = type$1; if (type$1 == "r") { types[i$2] = "R" } }
	    }

	    // W4. A single European separator between two European numbers
	    // changes to a European number. A single common separator between
	    // two numbers of the same type changes to that type.
	    for (var i$3 = 1, prev$1 = types[0]; i$3 < len - 1; ++i$3) {
	      var type$2 = types[i$3]
	      if (type$2 == "+" && prev$1 == "1" && types[i$3+1] == "1") { types[i$3] = "1" }
	      else if (type$2 == "," && prev$1 == types[i$3+1] &&
	               (prev$1 == "1" || prev$1 == "n")) { types[i$3] = prev$1 }
	      prev$1 = type$2
	    }

	    // W5. A sequence of European terminators adjacent to European
	    // numbers changes to all European numbers.
	    // W6. Otherwise, separators and terminators change to Other
	    // Neutral.
	    for (var i$4 = 0; i$4 < len; ++i$4) {
	      var type$3 = types[i$4]
	      if (type$3 == ",") { types[i$4] = "N" }
	      else if (type$3 == "%") {
	        var end = (void 0)
	        for (end = i$4 + 1; end < len && types[end] == "%"; ++end) {}
	        var replace = (i$4 && types[i$4-1] == "!") || (end < len && types[end] == "1") ? "1" : "N"
	        for (var j = i$4; j < end; ++j) { types[j] = replace }
	        i$4 = end - 1
	      }
	    }

	    // W7. Search backwards from each instance of a European number
	    // until the first strong type (R, L, or sor) is found. If an L is
	    // found, then change the type of the European number to L.
	    for (var i$5 = 0, cur$1 = outerType; i$5 < len; ++i$5) {
	      var type$4 = types[i$5]
	      if (cur$1 == "L" && type$4 == "1") { types[i$5] = "L" }
	      else if (isStrong.test(type$4)) { cur$1 = type$4 }
	    }

	    // N1. A sequence of neutrals takes the direction of the
	    // surrounding strong text if the text on both sides has the same
	    // direction. European and Arabic numbers act as if they were R in
	    // terms of their influence on neutrals. Start-of-level-run (sor)
	    // and end-of-level-run (eor) are used at level run boundaries.
	    // N2. Any remaining neutrals take the embedding direction.
	    for (var i$6 = 0; i$6 < len; ++i$6) {
	      if (isNeutral.test(types[i$6])) {
	        var end$1 = (void 0)
	        for (end$1 = i$6 + 1; end$1 < len && isNeutral.test(types[end$1]); ++end$1) {}
	        var before = (i$6 ? types[i$6-1] : outerType) == "L"
	        var after = (end$1 < len ? types[end$1] : outerType) == "L"
	        var replace$1 = before == after ? (before ? "L" : "R") : outerType
	        for (var j$1 = i$6; j$1 < end$1; ++j$1) { types[j$1] = replace$1 }
	        i$6 = end$1 - 1
	      }
	    }

	    // Here we depart from the documented algorithm, in order to avoid
	    // building up an actual levels array. Since there are only three
	    // levels (0, 1, 2) in an implementation that doesn't take
	    // explicit embedding into account, we can build up the order on
	    // the fly, without following the level-based algorithm.
	    var order = [], m
	    for (var i$7 = 0; i$7 < len;) {
	      if (countsAsLeft.test(types[i$7])) {
	        var start = i$7
	        for (++i$7; i$7 < len && countsAsLeft.test(types[i$7]); ++i$7) {}
	        order.push(new BidiSpan(0, start, i$7))
	      } else {
	        var pos = i$7, at = order.length
	        for (++i$7; i$7 < len && types[i$7] != "L"; ++i$7) {}
	        for (var j$2 = pos; j$2 < i$7;) {
	          if (countsAsNum.test(types[j$2])) {
	            if (pos < j$2) { order.splice(at, 0, new BidiSpan(1, pos, j$2)) }
	            var nstart = j$2
	            for (++j$2; j$2 < i$7 && countsAsNum.test(types[j$2]); ++j$2) {}
	            order.splice(at, 0, new BidiSpan(2, nstart, j$2))
	            pos = j$2
	          } else { ++j$2 }
	        }
	        if (pos < i$7) { order.splice(at, 0, new BidiSpan(1, pos, i$7)) }
	      }
	    }
	    if (direction == "ltr") {
	      if (order[0].level == 1 && (m = str.match(/^\s+/))) {
	        order[0].from = m[0].length
	        order.unshift(new BidiSpan(0, 0, m[0].length))
	      }
	      if (lst(order).level == 1 && (m = str.match(/\s+$/))) {
	        lst(order).to -= m[0].length
	        order.push(new BidiSpan(0, len - m[0].length, len))
	      }
	    }

	    return direction == "rtl" ? order.reverse() : order
	  }
	})()

	// Get the bidi ordering for the given line (and cache it). Returns
	// false for lines that are fully left-to-right, and an array of
	// BidiSpan objects otherwise.
	function getOrder(line, direction) {
	  var order = line.order
	  if (order == null) { order = line.order = bidiOrdering(line.text, direction) }
	  return order
	}

	// EVENT HANDLING

	// Lightweight event framework. on/off also work on DOM nodes,
	// registering native DOM handlers.

	var noHandlers = []

	var on = function(emitter, type, f) {
	  if (emitter.addEventListener) {
	    emitter.addEventListener(type, f, false)
	  } else if (emitter.attachEvent) {
	    emitter.attachEvent("on" + type, f)
	  } else {
	    var map = emitter._handlers || (emitter._handlers = {})
	    map[type] = (map[type] || noHandlers).concat(f)
	  }
	}

	function getHandlers(emitter, type) {
	  return emitter._handlers && emitter._handlers[type] || noHandlers
	}

	function off(emitter, type, f) {
	  if (emitter.removeEventListener) {
	    emitter.removeEventListener(type, f, false)
	  } else if (emitter.detachEvent) {
	    emitter.detachEvent("on" + type, f)
	  } else {
	    var map = emitter._handlers, arr = map && map[type]
	    if (arr) {
	      var index = indexOf(arr, f)
	      if (index > -1)
	        { map[type] = arr.slice(0, index).concat(arr.slice(index + 1)) }
	    }
	  }
	}

	function signal(emitter, type /*, values...*/) {
	  var handlers = getHandlers(emitter, type)
	  if (!handlers.length) { return }
	  var args = Array.prototype.slice.call(arguments, 2)
	  for (var i = 0; i < handlers.length; ++i) { handlers[i].apply(null, args) }
	}

	// The DOM events that CodeMirror handles can be overridden by
	// registering a (non-DOM) handler on the editor for the event name,
	// and preventDefault-ing the event in that handler.
	function signalDOMEvent(cm, e, override) {
	  if (typeof e == "string")
	    { e = {type: e, preventDefault: function() { this.defaultPrevented = true }} }
	  signal(cm, override || e.type, cm, e)
	  return e_defaultPrevented(e) || e.codemirrorIgnore
	}

	function signalCursorActivity(cm) {
	  var arr = cm._handlers && cm._handlers.cursorActivity
	  if (!arr) { return }
	  var set = cm.curOp.cursorActivityHandlers || (cm.curOp.cursorActivityHandlers = [])
	  for (var i = 0; i < arr.length; ++i) { if (indexOf(set, arr[i]) == -1)
	    { set.push(arr[i]) } }
	}

	function hasHandler(emitter, type) {
	  return getHandlers(emitter, type).length > 0
	}

	// Add on and off methods to a constructor's prototype, to make
	// registering events on such objects more convenient.
	function eventMixin(ctor) {
	  ctor.prototype.on = function(type, f) {on(this, type, f)}
	  ctor.prototype.off = function(type, f) {off(this, type, f)}
	}

	// Due to the fact that we still support jurassic IE versions, some
	// compatibility wrappers are needed.

	function e_preventDefault(e) {
	  if (e.preventDefault) { e.preventDefault() }
	  else { e.returnValue = false }
	}
	function e_stopPropagation(e) {
	  if (e.stopPropagation) { e.stopPropagation() }
	  else { e.cancelBubble = true }
	}
	function e_defaultPrevented(e) {
	  return e.defaultPrevented != null ? e.defaultPrevented : e.returnValue == false
	}
	function e_stop(e) {e_preventDefault(e); e_stopPropagation(e)}

	function e_target(e) {return e.target || e.srcElement}
	function e_button(e) {
	  var b = e.which
	  if (b == null) {
	    if (e.button & 1) { b = 1 }
	    else if (e.button & 2) { b = 3 }
	    else if (e.button & 4) { b = 2 }
	  }
	  if (mac && e.ctrlKey && b == 1) { b = 3 }
	  return b
	}

	// Detect drag-and-drop
	var dragAndDrop = function() {
	  // There is *some* kind of drag-and-drop support in IE6-8, but I
	  // couldn't get it to work yet.
	  if (ie && ie_version < 9) { return false }
	  var div = elt('div')
	  return "draggable" in div || "dragDrop" in div
	}()

	var zwspSupported
	function zeroWidthElement(measure) {
	  if (zwspSupported == null) {
	    var test = elt("span", "\u200b")
	    removeChildrenAndAdd(measure, elt("span", [test, document.createTextNode("x")]))
	    if (measure.firstChild.offsetHeight != 0)
	      { zwspSupported = test.offsetWidth <= 1 && test.offsetHeight > 2 && !(ie && ie_version < 8) }
	  }
	  var node = zwspSupported ? elt("span", "\u200b") :
	    elt("span", "\u00a0", null, "display: inline-block; width: 1px; margin-right: -1px")
	  node.setAttribute("cm-text", "")
	  return node
	}

	// Feature-detect IE's crummy client rect reporting for bidi text
	var badBidiRects
	function hasBadBidiRects(measure) {
	  if (badBidiRects != null) { return badBidiRects }
	  var txt = removeChildrenAndAdd(measure, document.createTextNode("A\u062eA"))
	  var r0 = range(txt, 0, 1).getBoundingClientRect()
	  var r1 = range(txt, 1, 2).getBoundingClientRect()
	  removeChildren(measure)
	  if (!r0 || r0.left == r0.right) { return false } // Safari returns null in some cases (#2780)
	  return badBidiRects = (r1.right - r0.right < 3)
	}

	// See if "".split is the broken IE version, if so, provide an
	// alternative way to split lines.
	var splitLinesAuto = "\n\nb".split(/\n/).length != 3 ? function (string) {
	  var pos = 0, result = [], l = string.length
	  while (pos <= l) {
	    var nl = string.indexOf("\n", pos)
	    if (nl == -1) { nl = string.length }
	    var line = string.slice(pos, string.charAt(nl - 1) == "\r" ? nl - 1 : nl)
	    var rt = line.indexOf("\r")
	    if (rt != -1) {
	      result.push(line.slice(0, rt))
	      pos += rt + 1
	    } else {
	      result.push(line)
	      pos = nl + 1
	    }
	  }
	  return result
	} : function (string) { return string.split(/\r\n?|\n/); }

	var hasSelection = window.getSelection ? function (te) {
	  try { return te.selectionStart != te.selectionEnd }
	  catch(e) { return false }
	} : function (te) {
	  var range
	  try {range = te.ownerDocument.selection.createRange()}
	  catch(e) {}
	  if (!range || range.parentElement() != te) { return false }
	  return range.compareEndPoints("StartToEnd", range) != 0
	}

	var hasCopyEvent = (function () {
	  var e = elt("div")
	  if ("oncopy" in e) { return true }
	  e.setAttribute("oncopy", "return;")
	  return typeof e.oncopy == "function"
	})()

	var badZoomedRects = null
	function hasBadZoomedRects(measure) {
	  if (badZoomedRects != null) { return badZoomedRects }
	  var node = removeChildrenAndAdd(measure, elt("span", "x"))
	  var normal = node.getBoundingClientRect()
	  var fromRange = range(node, 0, 1).getBoundingClientRect()
	  return badZoomedRects = Math.abs(normal.left - fromRange.left) > 1
	}

	var modes = {};
	var mimeModes = {};
	// Extra arguments are stored as the mode's dependencies, which is
	// used by (legacy) mechanisms like loadmode.js to automatically
	// load a mode. (Preferred mechanism is the require/define calls.)
	function defineMode(name, mode) {
	  if (arguments.length > 2)
	    { mode.dependencies = Array.prototype.slice.call(arguments, 2) }
	  modes[name] = mode
	}

	function defineMIME(mime, spec) {
	  mimeModes[mime] = spec
	}

	// Given a MIME type, a {name, ...options} config object, or a name
	// string, return a mode config object.
	function resolveMode(spec) {
	  if (typeof spec == "string" && mimeModes.hasOwnProperty(spec)) {
	    spec = mimeModes[spec]
	  } else if (spec && typeof spec.name == "string" && mimeModes.hasOwnProperty(spec.name)) {
	    var found = mimeModes[spec.name]
	    if (typeof found == "string") { found = {name: found} }
	    spec = createObj(found, spec)
	    spec.name = found.name
	  } else if (typeof spec == "string" && /^[\w\-]+\/[\w\-]+\+xml$/.test(spec)) {
	    return resolveMode("application/xml")
	  } else if (typeof spec == "string" && /^[\w\-]+\/[\w\-]+\+json$/.test(spec)) {
	    return resolveMode("application/json")
	  }
	  if (typeof spec == "string") { return {name: spec} }
	  else { return spec || {name: "null"} }
	}

	// Given a mode spec (anything that resolveMode accepts), find and
	// initialize an actual mode object.
	function getMode(options, spec) {
	  spec = resolveMode(spec)
	  var mfactory = modes[spec.name]
	  if (!mfactory) { return getMode(options, "text/plain") }
	  var modeObj = mfactory(options, spec)
	  if (modeExtensions.hasOwnProperty(spec.name)) {
	    var exts = modeExtensions[spec.name]
	    for (var prop in exts) {
	      if (!exts.hasOwnProperty(prop)) { continue }
	      if (modeObj.hasOwnProperty(prop)) { modeObj["_" + prop] = modeObj[prop] }
	      modeObj[prop] = exts[prop]
	    }
	  }
	  modeObj.name = spec.name
	  if (spec.helperType) { modeObj.helperType = spec.helperType }
	  if (spec.modeProps) { for (var prop$1 in spec.modeProps)
	    { modeObj[prop$1] = spec.modeProps[prop$1] } }

	  return modeObj
	}

	// This can be used to attach properties to mode objects from
	// outside the actual mode definition.
	var modeExtensions = {}
	function extendMode(mode, properties) {
	  var exts = modeExtensions.hasOwnProperty(mode) ? modeExtensions[mode] : (modeExtensions[mode] = {})
	  copyObj(properties, exts)
	}

	function copyState(mode, state) {
	  if (state === true) { return state }
	  if (mode.copyState) { return mode.copyState(state) }
	  var nstate = {}
	  for (var n in state) {
	    var val = state[n]
	    if (val instanceof Array) { val = val.concat([]) }
	    nstate[n] = val
	  }
	  return nstate
	}

	// Given a mode and a state (for that mode), find the inner mode and
	// state at the position that the state refers to.
	function innerMode(mode, state) {
	  var info
	  while (mode.innerMode) {
	    info = mode.innerMode(state)
	    if (!info || info.mode == mode) { break }
	    state = info.state
	    mode = info.mode
	  }
	  return info || {mode: mode, state: state}
	}

	function startState(mode, a1, a2) {
	  return mode.startState ? mode.startState(a1, a2) : true
	}

	// STRING STREAM

	// Fed to the mode parsers, provides helper functions to make
	// parsers more succinct.

	var StringStream = function(string, tabSize, lineOracle) {
	  this.pos = this.start = 0
	  this.string = string
	  this.tabSize = tabSize || 8
	  this.lastColumnPos = this.lastColumnValue = 0
	  this.lineStart = 0
	  this.lineOracle = lineOracle
	};

	StringStream.prototype.eol = function () {return this.pos >= this.string.length};
	StringStream.prototype.sol = function () {return this.pos == this.lineStart};
	StringStream.prototype.peek = function () {return this.string.charAt(this.pos) || undefined};
	StringStream.prototype.next = function () {
	  if (this.pos < this.string.length)
	    { return this.string.charAt(this.pos++) }
	};
	StringStream.prototype.eat = function (match) {
	  var ch = this.string.charAt(this.pos)
	  var ok
	  if (typeof match == "string") { ok = ch == match }
	  else { ok = ch && (match.test ? match.test(ch) : match(ch)) }
	  if (ok) {++this.pos; return ch}
	};
	StringStream.prototype.eatWhile = function (match) {
	  var start = this.pos
	  while (this.eat(match)){}
	  return this.pos > start
	};
	StringStream.prototype.eatSpace = function () {
	    var this$1 = this;

	  var start = this.pos
	  while (/[\s\u00a0]/.test(this.string.charAt(this.pos))) { ++this$1.pos }
	  return this.pos > start
	};
	StringStream.prototype.skipToEnd = function () {this.pos = this.string.length};
	StringStream.prototype.skipTo = function (ch) {
	  var found = this.string.indexOf(ch, this.pos)
	  if (found > -1) {this.pos = found; return true}
	};
	StringStream.prototype.backUp = function (n) {this.pos -= n};
	StringStream.prototype.column = function () {
	  if (this.lastColumnPos < this.start) {
	    this.lastColumnValue = countColumn(this.string, this.start, this.tabSize, this.lastColumnPos, this.lastColumnValue)
	    this.lastColumnPos = this.start
	  }
	  return this.lastColumnValue - (this.lineStart ? countColumn(this.string, this.lineStart, this.tabSize) : 0)
	};
	StringStream.prototype.indentation = function () {
	  return countColumn(this.string, null, this.tabSize) -
	    (this.lineStart ? countColumn(this.string, this.lineStart, this.tabSize) : 0)
	};
	StringStream.prototype.match = function (pattern, consume, caseInsensitive) {
	  if (typeof pattern == "string") {
	    var cased = function (str) { return caseInsensitive ? str.toLowerCase() : str; }
	    var substr = this.string.substr(this.pos, pattern.length)
	    if (cased(substr) == cased(pattern)) {
	      if (consume !== false) { this.pos += pattern.length }
	      return true
	    }
	  } else {
	    var match = this.string.slice(this.pos).match(pattern)
	    if (match && match.index > 0) { return null }
	    if (match && consume !== false) { this.pos += match[0].length }
	    return match
	  }
	};
	StringStream.prototype.current = function (){return this.string.slice(this.start, this.pos)};
	StringStream.prototype.hideFirstChars = function (n, inner) {
	  this.lineStart += n
	  try { return inner() }
	  finally { this.lineStart -= n }
	};
	StringStream.prototype.lookAhead = function (n) {
	  var oracle = this.lineOracle
	  return oracle && oracle.lookAhead(n)
	};
	StringStream.prototype.baseToken = function () {
	  var oracle = this.lineOracle
	  return oracle && oracle.baseToken(this.pos)
	};

	var SavedContext = function(state, lookAhead) {
	  this.state = state
	  this.lookAhead = lookAhead
	};

	var Context = function(doc, state, line, lookAhead) {
	  this.state = state
	  this.doc = doc
	  this.line = line
	  this.maxLookAhead = lookAhead || 0
	  this.baseTokens = null
	  this.baseTokenPos = 1
	};

	Context.prototype.lookAhead = function (n) {
	  var line = this.doc.getLine(this.line + n)
	  if (line != null && n > this.maxLookAhead) { this.maxLookAhead = n }
	  return line
	};

	Context.prototype.baseToken = function (n) {
	    var this$1 = this;

	  if (!this.baseTokens) { return null }
	  while (this.baseTokens[this.baseTokenPos] <= n)
	    { this$1.baseTokenPos += 2 }
	  var type = this.baseTokens[this.baseTokenPos + 1]
	  return {type: type && type.replace(/( |^)overlay .*/, ""),
	          size: this.baseTokens[this.baseTokenPos] - n}
	};

	Context.prototype.nextLine = function () {
	  this.line++
	  if (this.maxLookAhead > 0) { this.maxLookAhead-- }
	};

	Context.fromSaved = function (doc, saved, line) {
	  if (saved instanceof SavedContext)
	    { return new Context(doc, copyState(doc.mode, saved.state), line, saved.lookAhead) }
	  else
	    { return new Context(doc, copyState(doc.mode, saved), line) }
	};

	Context.prototype.save = function (copy) {
	  var state = copy !== false ? copyState(this.doc.mode, this.state) : this.state
	  return this.maxLookAhead > 0 ? new SavedContext(state, this.maxLookAhead) : state
	};


	// Compute a style array (an array starting with a mode generation
	// -- for invalidation -- followed by pairs of end positions and
	// style strings), which is used to highlight the tokens on the
	// line.
	function highlightLine(cm, line, context, forceToEnd) {
	  // A styles array always starts with a number identifying the
	  // mode/overlays that it is based on (for easy invalidation).
	  var st = [cm.state.modeGen], lineClasses = {}
	  // Compute the base array of styles
	  runMode(cm, line.text, cm.doc.mode, context, function (end, style) { return st.push(end, style); },
	          lineClasses, forceToEnd)
	  var state = context.state

	  // Run overlays, adjust style array.
	  var loop = function ( o ) {
	    context.baseTokens = st
	    var overlay = cm.state.overlays[o], i = 1, at = 0
	    context.state = true
	    runMode(cm, line.text, overlay.mode, context, function (end, style) {
	      var start = i
	      // Ensure there's a token end at the current position, and that i points at it
	      while (at < end) {
	        var i_end = st[i]
	        if (i_end > end)
	          { st.splice(i, 1, end, st[i+1], i_end) }
	        i += 2
	        at = Math.min(end, i_end)
	      }
	      if (!style) { return }
	      if (overlay.opaque) {
	        st.splice(start, i - start, end, "overlay " + style)
	        i = start + 2
	      } else {
	        for (; start < i; start += 2) {
	          var cur = st[start+1]
	          st[start+1] = (cur ? cur + " " : "") + "overlay " + style
	        }
	      }
	    }, lineClasses)
	    context.state = state
	    context.baseTokens = null
	    context.baseTokenPos = 1
	  };

	  for (var o = 0; o < cm.state.overlays.length; ++o) loop( o );

	  return {styles: st, classes: lineClasses.bgClass || lineClasses.textClass ? lineClasses : null}
	}

	function getLineStyles(cm, line, updateFrontier) {
	  if (!line.styles || line.styles[0] != cm.state.modeGen) {
	    var context = getContextBefore(cm, lineNo(line))
	    var resetState = line.text.length > cm.options.maxHighlightLength && copyState(cm.doc.mode, context.state)
	    var result = highlightLine(cm, line, context)
	    if (resetState) { context.state = resetState }
	    line.stateAfter = context.save(!resetState)
	    line.styles = result.styles
	    if (result.classes) { line.styleClasses = result.classes }
	    else if (line.styleClasses) { line.styleClasses = null }
	    if (updateFrontier === cm.doc.highlightFrontier)
	      { cm.doc.modeFrontier = Math.max(cm.doc.modeFrontier, ++cm.doc.highlightFrontier) }
	  }
	  return line.styles
	}

	function getContextBefore(cm, n, precise) {
	  var doc = cm.doc, display = cm.display
	  if (!doc.mode.startState) { return new Context(doc, true, n) }
	  var start = findStartLine(cm, n, precise)
	  var saved = start > doc.first && getLine(doc, start - 1).stateAfter
	  var context = saved ? Context.fromSaved(doc, saved, start) : new Context(doc, startState(doc.mode), start)

	  doc.iter(start, n, function (line) {
	    processLine(cm, line.text, context)
	    var pos = context.line
	    line.stateAfter = pos == n - 1 || pos % 5 == 0 || pos >= display.viewFrom && pos < display.viewTo ? context.save() : null
	    context.nextLine()
	  })
	  if (precise) { doc.modeFrontier = context.line }
	  return context
	}

	// Lightweight form of highlight -- proceed over this line and
	// update state, but don't save a style array. Used for lines that
	// aren't currently visible.
	function processLine(cm, text, context, startAt) {
	  var mode = cm.doc.mode
	  var stream = new StringStream(text, cm.options.tabSize, context)
	  stream.start = stream.pos = startAt || 0
	  if (text == "") { callBlankLine(mode, context.state) }
	  while (!stream.eol()) {
	    readToken(mode, stream, context.state)
	    stream.start = stream.pos
	  }
	}

	function callBlankLine(mode, state) {
	  if (mode.blankLine) { return mode.blankLine(state) }
	  if (!mode.innerMode) { return }
	  var inner = innerMode(mode, state)
	  if (inner.mode.blankLine) { return inner.mode.blankLine(inner.state) }
	}

	function readToken(mode, stream, state, inner) {
	  for (var i = 0; i < 10; i++) {
	    if (inner) { inner[0] = innerMode(mode, state).mode }
	    var style = mode.token(stream, state)
	    if (stream.pos > stream.start) { return style }
	  }
	  throw new Error("Mode " + mode.name + " failed to advance stream.")
	}

	var Token = function(stream, type, state) {
	  this.start = stream.start; this.end = stream.pos
	  this.string = stream.current()
	  this.type = type || null
	  this.state = state
	};

	// Utility for getTokenAt and getLineTokens
	function takeToken(cm, pos, precise, asArray) {
	  var doc = cm.doc, mode = doc.mode, style
	  pos = clipPos(doc, pos)
	  var line = getLine(doc, pos.line), context = getContextBefore(cm, pos.line, precise)
	  var stream = new StringStream(line.text, cm.options.tabSize, context), tokens
	  if (asArray) { tokens = [] }
	  while ((asArray || stream.pos < pos.ch) && !stream.eol()) {
	    stream.start = stream.pos
	    style = readToken(mode, stream, context.state)
	    if (asArray) { tokens.push(new Token(stream, style, copyState(doc.mode, context.state))) }
	  }
	  return asArray ? tokens : new Token(stream, style, context.state)
	}

	function extractLineClasses(type, output) {
	  if (type) { for (;;) {
	    var lineClass = type.match(/(?:^|\s+)line-(background-)?(\S+)/)
	    if (!lineClass) { break }
	    type = type.slice(0, lineClass.index) + type.slice(lineClass.index + lineClass[0].length)
	    var prop = lineClass[1] ? "bgClass" : "textClass"
	    if (output[prop] == null)
	      { output[prop] = lineClass[2] }
	    else if (!(new RegExp("(?:^|\s)" + lineClass[2] + "(?:$|\s)")).test(output[prop]))
	      { output[prop] += " " + lineClass[2] }
	  } }
	  return type
	}

	// Run the given mode's parser over a line, calling f for each token.
	function runMode(cm, text, mode, context, f, lineClasses, forceToEnd) {
	  var flattenSpans = mode.flattenSpans
	  if (flattenSpans == null) { flattenSpans = cm.options.flattenSpans }
	  var curStart = 0, curStyle = null
	  var stream = new StringStream(text, cm.options.tabSize, context), style
	  var inner = cm.options.addModeClass && [null]
	  if (text == "") { extractLineClasses(callBlankLine(mode, context.state), lineClasses) }
	  while (!stream.eol()) {
	    if (stream.pos > cm.options.maxHighlightLength) {
	      flattenSpans = false
	      if (forceToEnd) { processLine(cm, text, context, stream.pos) }
	      stream.pos = text.length
	      style = null
	    } else {
	      style = extractLineClasses(readToken(mode, stream, context.state, inner), lineClasses)
	    }
	    if (inner) {
	      var mName = inner[0].name
	      if (mName) { style = "m-" + (style ? mName + " " + style : mName) }
	    }
	    if (!flattenSpans || curStyle != style) {
	      while (curStart < stream.start) {
	        curStart = Math.min(stream.start, curStart + 5000)
	        f(curStart, curStyle)
	      }
	      curStyle = style
	    }
	    stream.start = stream.pos
	  }
	  while (curStart < stream.pos) {
	    // Webkit seems to refuse to render text nodes longer than 57444
	    // characters, and returns inaccurate measurements in nodes
	    // starting around 5000 chars.
	    var pos = Math.min(stream.pos, curStart + 5000)
	    f(pos, curStyle)
	    curStart = pos
	  }
	}

	// Finds the line to start with when starting a parse. Tries to
	// find a line with a stateAfter, so that it can start with a
	// valid state. If that fails, it returns the line with the
	// smallest indentation, which tends to need the least context to
	// parse correctly.
	function findStartLine(cm, n, precise) {
	  var minindent, minline, doc = cm.doc
	  var lim = precise ? -1 : n - (cm.doc.mode.innerMode ? 1000 : 100)
	  for (var search = n; search > lim; --search) {
	    if (search <= doc.first) { return doc.first }
	    var line = getLine(doc, search - 1), after = line.stateAfter
	    if (after && (!precise || search + (after instanceof SavedContext ? after.lookAhead : 0) <= doc.modeFrontier))
	      { return search }
	    var indented = countColumn(line.text, null, cm.options.tabSize)
	    if (minline == null || minindent > indented) {
	      minline = search - 1
	      minindent = indented
	    }
	  }
	  return minline
	}

	function retreatFrontier(doc, n) {
	  doc.modeFrontier = Math.min(doc.modeFrontier, n)
	  if (doc.highlightFrontier < n - 10) { return }
	  var start = doc.first
	  for (var line = n - 1; line > start; line--) {
	    var saved = getLine(doc, line).stateAfter
	    // change is on 3
	    // state on line 1 looked ahead 2 -- so saw 3
	    // test 1 + 2 < 3 should cover this
	    if (saved && (!(saved instanceof SavedContext) || line + saved.lookAhead < n)) {
	      start = line + 1
	      break
	    }
	  }
	  doc.highlightFrontier = Math.min(doc.highlightFrontier, start)
	}

	// LINE DATA STRUCTURE

	// Line objects. These hold state related to a line, including
	// highlighting info (the styles array).
	var Line = function(text, markedSpans, estimateHeight) {
	  this.text = text
	  attachMarkedSpans(this, markedSpans)
	  this.height = estimateHeight ? estimateHeight(this) : 1
	};

	Line.prototype.lineNo = function () { return lineNo(this) };
	eventMixin(Line)

	// Change the content (text, markers) of a line. Automatically
	// invalidates cached information and tries to re-estimate the
	// line's height.
	function updateLine(line, text, markedSpans, estimateHeight) {
	  line.text = text
	  if (line.stateAfter) { line.stateAfter = null }
	  if (line.styles) { line.styles = null }
	  if (line.order != null) { line.order = null }
	  detachMarkedSpans(line)
	  attachMarkedSpans(line, markedSpans)
	  var estHeight = estimateHeight ? estimateHeight(line) : 1
	  if (estHeight != line.height) { updateLineHeight(line, estHeight) }
	}

	// Detach a line from the document tree and its markers.
	function cleanUpLine(line) {
	  line.parent = null
	  detachMarkedSpans(line)
	}

	// Convert a style as returned by a mode (either null, or a string
	// containing one or more styles) to a CSS style. This is cached,
	// and also looks for line-wide styles.
	var styleToClassCache = {};
	var styleToClassCacheWithMode = {};
	function interpretTokenStyle(style, options) {
	  if (!style || /^\s*$/.test(style)) { return null }
	  var cache = options.addModeClass ? styleToClassCacheWithMode : styleToClassCache
	  return cache[style] ||
	    (cache[style] = style.replace(/\S+/g, "cm-$&"))
	}

	// Render the DOM representation of the text of a line. Also builds
	// up a 'line map', which points at the DOM nodes that represent
	// specific stretches of text, and is used by the measuring code.
	// The returned object contains the DOM node, this map, and
	// information about line-wide styles that were set by the mode.
	function buildLineContent(cm, lineView) {
	  // The padding-right forces the element to have a 'border', which
	  // is needed on Webkit to be able to get line-level bounding
	  // rectangles for it (in measureChar).
	  var content = eltP("span", null, null, webkit ? "padding-right: .1px" : null)
	  var builder = {pre: eltP("pre", [content], "CodeMirror-line"), content: content,
	                 col: 0, pos: 0, cm: cm,
	                 trailingSpace: false,
	                 splitSpaces: (ie || webkit) && cm.getOption("lineWrapping")}
	  lineView.measure = {}

	  // Iterate over the logical lines that make up this visual line.
	  for (var i = 0; i <= (lineView.rest ? lineView.rest.length : 0); i++) {
	    var line = i ? lineView.rest[i - 1] : lineView.line, order = (void 0)
	    builder.pos = 0
	    builder.addToken = buildToken
	    // Optionally wire in some hacks into the token-rendering
	    // algorithm, to deal with browser quirks.
	    if (hasBadBidiRects(cm.display.measure) && (order = getOrder(line, cm.doc.direction)))
	      { builder.addToken = buildTokenBadBidi(builder.addToken, order) }
	    builder.map = []
	    var allowFrontierUpdate = lineView != cm.display.externalMeasured && lineNo(line)
	    insertLineContent(line, builder, getLineStyles(cm, line, allowFrontierUpdate))
	    if (line.styleClasses) {
	      if (line.styleClasses.bgClass)
	        { builder.bgClass = joinClasses(line.styleClasses.bgClass, builder.bgClass || "") }
	      if (line.styleClasses.textClass)
	        { builder.textClass = joinClasses(line.styleClasses.textClass, builder.textClass || "") }
	    }

	    // Ensure at least a single node is present, for measuring.
	    if (builder.map.length == 0)
	      { builder.map.push(0, 0, builder.content.appendChild(zeroWidthElement(cm.display.measure))) }

	    // Store the map and a cache object for the current logical line
	    if (i == 0) {
	      lineView.measure.map = builder.map
	      lineView.measure.cache = {}
	    } else {
	      ;(lineView.measure.maps || (lineView.measure.maps = [])).push(builder.map)
	      ;(lineView.measure.caches || (lineView.measure.caches = [])).push({})
	    }
	  }

	  // See issue #2901
	  if (webkit) {
	    var last = builder.content.lastChild
	    if (/\bcm-tab\b/.test(last.className) || (last.querySelector && last.querySelector(".cm-tab")))
	      { builder.content.className = "cm-tab-wrap-hack" }
	  }

	  signal(cm, "renderLine", cm, lineView.line, builder.pre)
	  if (builder.pre.className)
	    { builder.textClass = joinClasses(builder.pre.className, builder.textClass || "") }

	  return builder
	}

	function defaultSpecialCharPlaceholder(ch) {
	  var token = elt("span", "\u2022", "cm-invalidchar")
	  token.title = "\\u" + ch.charCodeAt(0).toString(16)
	  token.setAttribute("aria-label", token.title)
	  return token
	}

	// Build up the DOM representation for a single token, and add it to
	// the line map. Takes care to render special characters separately.
	function buildToken(builder, text, style, startStyle, endStyle, title, css) {
	  if (!text) { return }
	  var displayText = builder.splitSpaces ? splitSpaces(text, builder.trailingSpace) : text
	  var special = builder.cm.state.specialChars, mustWrap = false
	  var content
	  if (!special.test(text)) {
	    builder.col += text.length
	    content = document.createTextNode(displayText)
	    builder.map.push(builder.pos, builder.pos + text.length, content)
	    if (ie && ie_version < 9) { mustWrap = true }
	    builder.pos += text.length
	  } else {
	    content = document.createDocumentFragment()
	    var pos = 0
	    while (true) {
	      special.lastIndex = pos
	      var m = special.exec(text)
	      var skipped = m ? m.index - pos : text.length - pos
	      if (skipped) {
	        var txt = document.createTextNode(displayText.slice(pos, pos + skipped))
	        if (ie && ie_version < 9) { content.appendChild(elt("span", [txt])) }
	        else { content.appendChild(txt) }
	        builder.map.push(builder.pos, builder.pos + skipped, txt)
	        builder.col += skipped
	        builder.pos += skipped
	      }
	      if (!m) { break }
	      pos += skipped + 1
	      var txt$1 = (void 0)
	      if (m[0] == "\t") {
	        var tabSize = builder.cm.options.tabSize, tabWidth = tabSize - builder.col % tabSize
	        txt$1 = content.appendChild(elt("span", spaceStr(tabWidth), "cm-tab"))
	        txt$1.setAttribute("role", "presentation")
	        txt$1.setAttribute("cm-text", "\t")
	        builder.col += tabWidth
	      } else if (m[0] == "\r" || m[0] == "\n") {
	        txt$1 = content.appendChild(elt("span", m[0] == "\r" ? "\u240d" : "\u2424", "cm-invalidchar"))
	        txt$1.setAttribute("cm-text", m[0])
	        builder.col += 1
	      } else {
	        txt$1 = builder.cm.options.specialCharPlaceholder(m[0])
	        txt$1.setAttribute("cm-text", m[0])
	        if (ie && ie_version < 9) { content.appendChild(elt("span", [txt$1])) }
	        else { content.appendChild(txt$1) }
	        builder.col += 1
	      }
	      builder.map.push(builder.pos, builder.pos + 1, txt$1)
	      builder.pos++
	    }
	  }
	  builder.trailingSpace = displayText.charCodeAt(text.length - 1) == 32
	  if (style || startStyle || endStyle || mustWrap || css) {
	    var fullStyle = style || ""
	    if (startStyle) { fullStyle += startStyle }
	    if (endStyle) { fullStyle += endStyle }
	    var token = elt("span", [content], fullStyle, css)
	    if (title) { token.title = title }
	    return builder.content.appendChild(token)
	  }
	  builder.content.appendChild(content)
	}

	function splitSpaces(text, trailingBefore) {
	  if (text.length > 1 && !/  /.test(text)) { return text }
	  var spaceBefore = trailingBefore, result = ""
	  for (var i = 0; i < text.length; i++) {
	    var ch = text.charAt(i)
	    if (ch == " " && spaceBefore && (i == text.length - 1 || text.charCodeAt(i + 1) == 32))
	      { ch = "\u00a0" }
	    result += ch
	    spaceBefore = ch == " "
	  }
	  return result
	}

	// Work around nonsense dimensions being reported for stretches of
	// right-to-left text.
	function buildTokenBadBidi(inner, order) {
	  return function (builder, text, style, startStyle, endStyle, title, css) {
	    style = style ? style + " cm-force-border" : "cm-force-border"
	    var start = builder.pos, end = start + text.length
	    for (;;) {
	      // Find the part that overlaps with the start of this text
	      var part = (void 0)
	      for (var i = 0; i < order.length; i++) {
	        part = order[i]
	        if (part.to > start && part.from <= start) { break }
	      }
	      if (part.to >= end) { return inner(builder, text, style, startStyle, endStyle, title, css) }
	      inner(builder, text.slice(0, part.to - start), style, startStyle, null, title, css)
	      startStyle = null
	      text = text.slice(part.to - start)
	      start = part.to
	    }
	  }
	}

	function buildCollapsedSpan(builder, size, marker, ignoreWidget) {
	  var widget = !ignoreWidget && marker.widgetNode
	  if (widget) { builder.map.push(builder.pos, builder.pos + size, widget) }
	  if (!ignoreWidget && builder.cm.display.input.needsContentAttribute) {
	    if (!widget)
	      { widget = builder.content.appendChild(document.createElement("span")) }
	    widget.setAttribute("cm-marker", marker.id)
	  }
	  if (widget) {
	    builder.cm.display.input.setUneditable(widget)
	    builder.content.appendChild(widget)
	  }
	  builder.pos += size
	  builder.trailingSpace = false
	}

	// Outputs a number of spans to make up a line, taking highlighting
	// and marked text into account.
	function insertLineContent(line, builder, styles) {
	  var spans = line.markedSpans, allText = line.text, at = 0
	  if (!spans) {
	    for (var i$1 = 1; i$1 < styles.length; i$1+=2)
	      { builder.addToken(builder, allText.slice(at, at = styles[i$1]), interpretTokenStyle(styles[i$1+1], builder.cm.options)) }
	    return
	  }

	  var len = allText.length, pos = 0, i = 1, text = "", style, css
	  var nextChange = 0, spanStyle, spanEndStyle, spanStartStyle, title, collapsed
	  for (;;) {
	    if (nextChange == pos) { // Update current marker set
	      spanStyle = spanEndStyle = spanStartStyle = title = css = ""
	      collapsed = null; nextChange = Infinity
	      var foundBookmarks = [], endStyles = (void 0)
	      for (var j = 0; j < spans.length; ++j) {
	        var sp = spans[j], m = sp.marker
	        if (m.type == "bookmark" && sp.from == pos && m.widgetNode) {
	          foundBookmarks.push(m)
	        } else if (sp.from <= pos && (sp.to == null || sp.to > pos || m.collapsed && sp.to == pos && sp.from == pos)) {
	          if (sp.to != null && sp.to != pos && nextChange > sp.to) {
	            nextChange = sp.to
	            spanEndStyle = ""
	          }
	          if (m.className) { spanStyle += " " + m.className }
	          if (m.css) { css = (css ? css + ";" : "") + m.css }
	          if (m.startStyle && sp.from == pos) { spanStartStyle += " " + m.startStyle }
	          if (m.endStyle && sp.to == nextChange) { (endStyles || (endStyles = [])).push(m.endStyle, sp.to) }
	          if (m.title && !title) { title = m.title }
	          if (m.collapsed && (!collapsed || compareCollapsedMarkers(collapsed.marker, m) < 0))
	            { collapsed = sp }
	        } else if (sp.from > pos && nextChange > sp.from) {
	          nextChange = sp.from
	        }
	      }
	      if (endStyles) { for (var j$1 = 0; j$1 < endStyles.length; j$1 += 2)
	        { if (endStyles[j$1 + 1] == nextChange) { spanEndStyle += " " + endStyles[j$1] } } }

	      if (!collapsed || collapsed.from == pos) { for (var j$2 = 0; j$2 < foundBookmarks.length; ++j$2)
	        { buildCollapsedSpan(builder, 0, foundBookmarks[j$2]) } }
	      if (collapsed && (collapsed.from || 0) == pos) {
	        buildCollapsedSpan(builder, (collapsed.to == null ? len + 1 : collapsed.to) - pos,
	                           collapsed.marker, collapsed.from == null)
	        if (collapsed.to == null) { return }
	        if (collapsed.to == pos) { collapsed = false }
	      }
	    }
	    if (pos >= len) { break }

	    var upto = Math.min(len, nextChange)
	    while (true) {
	      if (text) {
	        var end = pos + text.length
	        if (!collapsed) {
	          var tokenText = end > upto ? text.slice(0, upto - pos) : text
	          builder.addToken(builder, tokenText, style ? style + spanStyle : spanStyle,
	                           spanStartStyle, pos + tokenText.length == nextChange ? spanEndStyle : "", title, css)
	        }
	        if (end >= upto) {text = text.slice(upto - pos); pos = upto; break}
	        pos = end
	        spanStartStyle = ""
	      }
	      text = allText.slice(at, at = styles[i++])
	      style = interpretTokenStyle(styles[i++], builder.cm.options)
	    }
	  }
	}


	// These objects are used to represent the visible (currently drawn)
	// part of the document. A LineView may correspond to multiple
	// logical lines, if those are connected by collapsed ranges.
	function LineView(doc, line, lineN) {
	  // The starting line
	  this.line = line
	  // Continuing lines, if any
	  this.rest = visualLineContinued(line)
	  // Number of logical lines in this visual line
	  this.size = this.rest ? lineNo(lst(this.rest)) - lineN + 1 : 1
	  this.node = this.text = null
	  this.hidden = lineIsHidden(doc, line)
	}

	// Create a range of LineView objects for the given lines.
	function buildViewArray(cm, from, to) {
	  var array = [], nextPos
	  for (var pos = from; pos < to; pos = nextPos) {
	    var view = new LineView(cm.doc, getLine(cm.doc, pos), pos)
	    nextPos = pos + view.size
	    array.push(view)
	  }
	  return array
	}

	var operationGroup = null

	function pushOperation(op) {
	  if (operationGroup) {
	    operationGroup.ops.push(op)
	  } else {
	    op.ownsGroup = operationGroup = {
	      ops: [op],
	      delayedCallbacks: []
	    }
	  }
	}

	function fireCallbacksForOps(group) {
	  // Calls delayed callbacks and cursorActivity handlers until no
	  // new ones appear
	  var callbacks = group.delayedCallbacks, i = 0
	  do {
	    for (; i < callbacks.length; i++)
	      { callbacks[i].call(null) }
	    for (var j = 0; j < group.ops.length; j++) {
	      var op = group.ops[j]
	      if (op.cursorActivityHandlers)
	        { while (op.cursorActivityCalled < op.cursorActivityHandlers.length)
	          { op.cursorActivityHandlers[op.cursorActivityCalled++].call(null, op.cm) } }
	    }
	  } while (i < callbacks.length)
	}

	function finishOperation(op, endCb) {
	  var group = op.ownsGroup
	  if (!group) { return }

	  try { fireCallbacksForOps(group) }
	  finally {
	    operationGroup = null
	    endCb(group)
	  }
	}

	var orphanDelayedCallbacks = null

	// Often, we want to signal events at a point where we are in the
	// middle of some work, but don't want the handler to start calling
	// other methods on the editor, which might be in an inconsistent
	// state or simply not expect any other events to happen.
	// signalLater looks whether there are any handlers, and schedules
	// them to be executed when the last operation ends, or, if no
	// operation is active, when a timeout fires.
	function signalLater(emitter, type /*, values...*/) {
	  var arr = getHandlers(emitter, type)
	  if (!arr.length) { return }
	  var args = Array.prototype.slice.call(arguments, 2), list
	  if (operationGroup) {
	    list = operationGroup.delayedCallbacks
	  } else if (orphanDelayedCallbacks) {
	    list = orphanDelayedCallbacks
	  } else {
	    list = orphanDelayedCallbacks = []
	    setTimeout(fireOrphanDelayed, 0)
	  }
	  var loop = function ( i ) {
	    list.push(function () { return arr[i].apply(null, args); })
	  };

	  for (var i = 0; i < arr.length; ++i)
	    loop( i );
	}

	function fireOrphanDelayed() {
	  var delayed = orphanDelayedCallbacks
	  orphanDelayedCallbacks = null
	  for (var i = 0; i < delayed.length; ++i) { delayed[i]() }
	}

	// When an aspect of a line changes, a string is added to
	// lineView.changes. This updates the relevant part of the line's
	// DOM structure.
	function updateLineForChanges(cm, lineView, lineN, dims) {
	  for (var j = 0; j < lineView.changes.length; j++) {
	    var type = lineView.changes[j]
	    if (type == "text") { updateLineText(cm, lineView) }
	    else if (type == "gutter") { updateLineGutter(cm, lineView, lineN, dims) }
	    else if (type == "class") { updateLineClasses(cm, lineView) }
	    else if (type == "widget") { updateLineWidgets(cm, lineView, dims) }
	  }
	  lineView.changes = null
	}

	// Lines with gutter elements, widgets or a background class need to
	// be wrapped, and have the extra elements added to the wrapper div
	function ensureLineWrapped(lineView) {
	  if (lineView.node == lineView.text) {
	    lineView.node = elt("div", null, null, "position: relative")
	    if (lineView.text.parentNode)
	      { lineView.text.parentNode.replaceChild(lineView.node, lineView.text) }
	    lineView.node.appendChild(lineView.text)
	    if (ie && ie_version < 8) { lineView.node.style.zIndex = 2 }
	  }
	  return lineView.node
	}

	function updateLineBackground(cm, lineView) {
	  var cls = lineView.bgClass ? lineView.bgClass + " " + (lineView.line.bgClass || "") : lineView.line.bgClass
	  if (cls) { cls += " CodeMirror-linebackground" }
	  if (lineView.background) {
	    if (cls) { lineView.background.className = cls }
	    else { lineView.background.parentNode.removeChild(lineView.background); lineView.background = null }
	  } else if (cls) {
	    var wrap = ensureLineWrapped(lineView)
	    lineView.background = wrap.insertBefore(elt("div", null, cls), wrap.firstChild)
	    cm.display.input.setUneditable(lineView.background)
	  }
	}

	// Wrapper around buildLineContent which will reuse the structure
	// in display.externalMeasured when possible.
	function getLineContent(cm, lineView) {
	  var ext = cm.display.externalMeasured
	  if (ext && ext.line == lineView.line) {
	    cm.display.externalMeasured = null
	    lineView.measure = ext.measure
	    return ext.built
	  }
	  return buildLineContent(cm, lineView)
	}

	// Redraw the line's text. Interacts with the background and text
	// classes because the mode may output tokens that influence these
	// classes.
	function updateLineText(cm, lineView) {
	  var cls = lineView.text.className
	  var built = getLineContent(cm, lineView)
	  if (lineView.text == lineView.node) { lineView.node = built.pre }
	  lineView.text.parentNode.replaceChild(built.pre, lineView.text)
	  lineView.text = built.pre
	  if (built.bgClass != lineView.bgClass || built.textClass != lineView.textClass) {
	    lineView.bgClass = built.bgClass
	    lineView.textClass = built.textClass
	    updateLineClasses(cm, lineView)
	  } else if (cls) {
	    lineView.text.className = cls
	  }
	}

	function updateLineClasses(cm, lineView) {
	  updateLineBackground(cm, lineView)
	  if (lineView.line.wrapClass)
	    { ensureLineWrapped(lineView).className = lineView.line.wrapClass }
	  else if (lineView.node != lineView.text)
	    { lineView.node.className = "" }
	  var textClass = lineView.textClass ? lineView.textClass + " " + (lineView.line.textClass || "") : lineView.line.textClass
	  lineView.text.className = textClass || ""
	}

	function updateLineGutter(cm, lineView, lineN, dims) {
	  if (lineView.gutter) {
	    lineView.node.removeChild(lineView.gutter)
	    lineView.gutter = null
	  }
	  if (lineView.gutterBackground) {
	    lineView.node.removeChild(lineView.gutterBackground)
	    lineView.gutterBackground = null
	  }
	  if (lineView.line.gutterClass) {
	    var wrap = ensureLineWrapped(lineView)
	    lineView.gutterBackground = elt("div", null, "CodeMirror-gutter-background " + lineView.line.gutterClass,
	                                    ("left: " + (cm.options.fixedGutter ? dims.fixedPos : -dims.gutterTotalWidth) + "px; width: " + (dims.gutterTotalWidth) + "px"))
	    cm.display.input.setUneditable(lineView.gutterBackground)
	    wrap.insertBefore(lineView.gutterBackground, lineView.text)
	  }
	  var markers = lineView.line.gutterMarkers
	  if (cm.options.lineNumbers || markers) {
	    var wrap$1 = ensureLineWrapped(lineView)
	    var gutterWrap = lineView.gutter = elt("div", null, "CodeMirror-gutter-wrapper", ("left: " + (cm.options.fixedGutter ? dims.fixedPos : -dims.gutterTotalWidth) + "px"))
	    cm.display.input.setUneditable(gutterWrap)
	    wrap$1.insertBefore(gutterWrap, lineView.text)
	    if (lineView.line.gutterClass)
	      { gutterWrap.className += " " + lineView.line.gutterClass }
	    if (cm.options.lineNumbers && (!markers || !markers["CodeMirror-linenumbers"]))
	      { lineView.lineNumber = gutterWrap.appendChild(
	        elt("div", lineNumberFor(cm.options, lineN),
	            "CodeMirror-linenumber CodeMirror-gutter-elt",
	            ("left: " + (dims.gutterLeft["CodeMirror-linenumbers"]) + "px; width: " + (cm.display.lineNumInnerWidth) + "px"))) }
	    if (markers) { for (var k = 0; k < cm.options.gutters.length; ++k) {
	      var id = cm.options.gutters[k], found = markers.hasOwnProperty(id) && markers[id]
	      if (found)
	        { gutterWrap.appendChild(elt("div", [found], "CodeMirror-gutter-elt",
	                                   ("left: " + (dims.gutterLeft[id]) + "px; width: " + (dims.gutterWidth[id]) + "px"))) }
	    } }
	  }
	}

	function updateLineWidgets(cm, lineView, dims) {
	  if (lineView.alignable) { lineView.alignable = null }
	  for (var node = lineView.node.firstChild, next = (void 0); node; node = next) {
	    next = node.nextSibling
	    if (node.className == "CodeMirror-linewidget")
	      { lineView.node.removeChild(node) }
	  }
	  insertLineWidgets(cm, lineView, dims)
	}

	// Build a line's DOM representation from scratch
	function buildLineElement(cm, lineView, lineN, dims) {
	  var built = getLineContent(cm, lineView)
	  lineView.text = lineView.node = built.pre
	  if (built.bgClass) { lineView.bgClass = built.bgClass }
	  if (built.textClass) { lineView.textClass = built.textClass }

	  updateLineClasses(cm, lineView)
	  updateLineGutter(cm, lineView, lineN, dims)
	  insertLineWidgets(cm, lineView, dims)
	  return lineView.node
	}

	// A lineView may contain multiple logical lines (when merged by
	// collapsed spans). The widgets for all of them need to be drawn.
	function insertLineWidgets(cm, lineView, dims) {
	  insertLineWidgetsFor(cm, lineView.line, lineView, dims, true)
	  if (lineView.rest) { for (var i = 0; i < lineView.rest.length; i++)
	    { insertLineWidgetsFor(cm, lineView.rest[i], lineView, dims, false) } }
	}

	function insertLineWidgetsFor(cm, line, lineView, dims, allowAbove) {
	  if (!line.widgets) { return }
	  var wrap = ensureLineWrapped(lineView)
	  for (var i = 0, ws = line.widgets; i < ws.length; ++i) {
	    var widget = ws[i], node = elt("div", [widget.node], "CodeMirror-linewidget")
	    if (!widget.handleMouseEvents) { node.setAttribute("cm-ignore-events", "true") }
	    positionLineWidget(widget, node, lineView, dims)
	    cm.display.input.setUneditable(node)
	    if (allowAbove && widget.above)
	      { wrap.insertBefore(node, lineView.gutter || lineView.text) }
	    else
	      { wrap.appendChild(node) }
	    signalLater(widget, "redraw")
	  }
	}

	function positionLineWidget(widget, node, lineView, dims) {
	  if (widget.noHScroll) {
	    ;(lineView.alignable || (lineView.alignable = [])).push(node)
	    var width = dims.wrapperWidth
	    node.style.left = dims.fixedPos + "px"
	    if (!widget.coverGutter) {
	      width -= dims.gutterTotalWidth
	      node.style.paddingLeft = dims.gutterTotalWidth + "px"
	    }
	    node.style.width = width + "px"
	  }
	  if (widget.coverGutter) {
	    node.style.zIndex = 5
	    node.style.position = "relative"
	    if (!widget.noHScroll) { node.style.marginLeft = -dims.gutterTotalWidth + "px" }
	  }
	}

	function widgetHeight(widget) {
	  if (widget.height != null) { return widget.height }
	  var cm = widget.doc.cm
	  if (!cm) { return 0 }
	  if (!contains(document.body, widget.node)) {
	    var parentStyle = "position: relative;"
	    if (widget.coverGutter)
	      { parentStyle += "margin-left: -" + cm.display.gutters.offsetWidth + "px;" }
	    if (widget.noHScroll)
	      { parentStyle += "width: " + cm.display.wrapper.clientWidth + "px;" }
	    removeChildrenAndAdd(cm.display.measure, elt("div", [widget.node], null, parentStyle))
	  }
	  return widget.height = widget.node.parentNode.offsetHeight
	}

	// Return true when the given mouse event happened in a widget
	function eventInWidget(display, e) {
	  for (var n = e_target(e); n != display.wrapper; n = n.parentNode) {
	    if (!n || (n.nodeType == 1 && n.getAttribute("cm-ignore-events") == "true") ||
	        (n.parentNode == display.sizer && n != display.mover))
	      { return true }
	  }
	}

	// POSITION MEASUREMENT

	function paddingTop(display) {return display.lineSpace.offsetTop}
	function paddingVert(display) {return display.mover.offsetHeight - display.lineSpace.offsetHeight}
	function paddingH(display) {
	  if (display.cachedPaddingH) { return display.cachedPaddingH }
	  var e = removeChildrenAndAdd(display.measure, elt("pre", "x"))
	  var style = window.getComputedStyle ? window.getComputedStyle(e) : e.currentStyle
	  var data = {left: parseInt(style.paddingLeft), right: parseInt(style.paddingRight)}
	  if (!isNaN(data.left) && !isNaN(data.right)) { display.cachedPaddingH = data }
	  return data
	}

	function scrollGap(cm) { return scrollerGap - cm.display.nativeBarWidth }
	function displayWidth(cm) {
	  return cm.display.scroller.clientWidth - scrollGap(cm) - cm.display.barWidth
	}
	function displayHeight(cm) {
	  return cm.display.scroller.clientHeight - scrollGap(cm) - cm.display.barHeight
	}

	// Ensure the lineView.wrapping.heights array is populated. This is
	// an array of bottom offsets for the lines that make up a drawn
	// line. When lineWrapping is on, there might be more than one
	// height.
	function ensureLineHeights(cm, lineView, rect) {
	  var wrapping = cm.options.lineWrapping
	  var curWidth = wrapping && displayWidth(cm)
	  if (!lineView.measure.heights || wrapping && lineView.measure.width != curWidth) {
	    var heights = lineView.measure.heights = []
	    if (wrapping) {
	      lineView.measure.width = curWidth
	      var rects = lineView.text.firstChild.getClientRects()
	      for (var i = 0; i < rects.length - 1; i++) {
	        var cur = rects[i], next = rects[i + 1]
	        if (Math.abs(cur.bottom - next.bottom) > 2)
	          { heights.push((cur.bottom + next.top) / 2 - rect.top) }
	      }
	    }
	    heights.push(rect.bottom - rect.top)
	  }
	}

	// Find a line map (mapping character offsets to text nodes) and a
	// measurement cache for the given line number. (A line view might
	// contain multiple lines when collapsed ranges are present.)
	function mapFromLineView(lineView, line, lineN) {
	  if (lineView.line == line)
	    { return {map: lineView.measure.map, cache: lineView.measure.cache} }
	  for (var i = 0; i < lineView.rest.length; i++)
	    { if (lineView.rest[i] == line)
	      { return {map: lineView.measure.maps[i], cache: lineView.measure.caches[i]} } }
	  for (var i$1 = 0; i$1 < lineView.rest.length; i$1++)
	    { if (lineNo(lineView.rest[i$1]) > lineN)
	      { return {map: lineView.measure.maps[i$1], cache: lineView.measure.caches[i$1], before: true} } }
	}

	// Render a line into the hidden node display.externalMeasured. Used
	// when measurement is needed for a line that's not in the viewport.
	function updateExternalMeasurement(cm, line) {
	  line = visualLine(line)
	  var lineN = lineNo(line)
	  var view = cm.display.externalMeasured = new LineView(cm.doc, line, lineN)
	  view.lineN = lineN
	  var built = view.built = buildLineContent(cm, view)
	  view.text = built.pre
	  removeChildrenAndAdd(cm.display.lineMeasure, built.pre)
	  return view
	}

	// Get a {top, bottom, left, right} box (in line-local coordinates)
	// for a given character.
	function measureChar(cm, line, ch, bias) {
	  return measureCharPrepared(cm, prepareMeasureForLine(cm, line), ch, bias)
	}

	// Find a line view that corresponds to the given line number.
	function findViewForLine(cm, lineN) {
	  if (lineN >= cm.display.viewFrom && lineN < cm.display.viewTo)
	    { return cm.display.view[findViewIndex(cm, lineN)] }
	  var ext = cm.display.externalMeasured
	  if (ext && lineN >= ext.lineN && lineN < ext.lineN + ext.size)
	    { return ext }
	}

	// Measurement can be split in two steps, the set-up work that
	// applies to the whole line, and the measurement of the actual
	// character. Functions like coordsChar, that need to do a lot of
	// measurements in a row, can thus ensure that the set-up work is
	// only done once.
	function prepareMeasureForLine(cm, line) {
	  var lineN = lineNo(line)
	  var view = findViewForLine(cm, lineN)
	  if (view && !view.text) {
	    view = null
	  } else if (view && view.changes) {
	    updateLineForChanges(cm, view, lineN, getDimensions(cm))
	    cm.curOp.forceUpdate = true
	  }
	  if (!view)
	    { view = updateExternalMeasurement(cm, line) }

	  var info = mapFromLineView(view, line, lineN)
	  return {
	    line: line, view: view, rect: null,
	    map: info.map, cache: info.cache, before: info.before,
	    hasHeights: false
	  }
	}

	// Given a prepared measurement object, measures the position of an
	// actual character (or fetches it from the cache).
	function measureCharPrepared(cm, prepared, ch, bias, varHeight) {
	  if (prepared.before) { ch = -1 }
	  var key = ch + (bias || ""), found
	  if (prepared.cache.hasOwnProperty(key)) {
	    found = prepared.cache[key]
	  } else {
	    if (!prepared.rect)
	      { prepared.rect = prepared.view.text.getBoundingClientRect() }
	    if (!prepared.hasHeights) {
	      ensureLineHeights(cm, prepared.view, prepared.rect)
	      prepared.hasHeights = true
	    }
	    found = measureCharInner(cm, prepared, ch, bias)
	    if (!found.bogus) { prepared.cache[key] = found }
	  }
	  return {left: found.left, right: found.right,
	          top: varHeight ? found.rtop : found.top,
	          bottom: varHeight ? found.rbottom : found.bottom}
	}

	var nullRect = {left: 0, right: 0, top: 0, bottom: 0}

	function nodeAndOffsetInLineMap(map, ch, bias) {
	  var node, start, end, collapse, mStart, mEnd
	  // First, search the line map for the text node corresponding to,
	  // or closest to, the target character.
	  for (var i = 0; i < map.length; i += 3) {
	    mStart = map[i]
	    mEnd = map[i + 1]
	    if (ch < mStart) {
	      start = 0; end = 1
	      collapse = "left"
	    } else if (ch < mEnd) {
	      start = ch - mStart
	      end = start + 1
	    } else if (i == map.length - 3 || ch == mEnd && map[i + 3] > ch) {
	      end = mEnd - mStart
	      start = end - 1
	      if (ch >= mEnd) { collapse = "right" }
	    }
	    if (start != null) {
	      node = map[i + 2]
	      if (mStart == mEnd && bias == (node.insertLeft ? "left" : "right"))
	        { collapse = bias }
	      if (bias == "left" && start == 0)
	        { while (i && map[i - 2] == map[i - 3] && map[i - 1].insertLeft) {
	          node = map[(i -= 3) + 2]
	          collapse = "left"
	        } }
	      if (bias == "right" && start == mEnd - mStart)
	        { while (i < map.length - 3 && map[i + 3] == map[i + 4] && !map[i + 5].insertLeft) {
	          node = map[(i += 3) + 2]
	          collapse = "right"
	        } }
	      break
	    }
	  }
	  return {node: node, start: start, end: end, collapse: collapse, coverStart: mStart, coverEnd: mEnd}
	}

	function getUsefulRect(rects, bias) {
	  var rect = nullRect
	  if (bias == "left") { for (var i = 0; i < rects.length; i++) {
	    if ((rect = rects[i]).left != rect.right) { break }
	  } } else { for (var i$1 = rects.length - 1; i$1 >= 0; i$1--) {
	    if ((rect = rects[i$1]).left != rect.right) { break }
	  } }
	  return rect
	}

	function measureCharInner(cm, prepared, ch, bias) {
	  var place = nodeAndOffsetInLineMap(prepared.map, ch, bias)
	  var node = place.node, start = place.start, end = place.end, collapse = place.collapse

	  var rect
	  if (node.nodeType == 3) { // If it is a text node, use a range to retrieve the coordinates.
	    for (var i$1 = 0; i$1 < 4; i$1++) { // Retry a maximum of 4 times when nonsense rectangles are returned
	      while (start && isExtendingChar(prepared.line.text.charAt(place.coverStart + start))) { --start }
	      while (place.coverStart + end < place.coverEnd && isExtendingChar(prepared.line.text.charAt(place.coverStart + end))) { ++end }
	      if (ie && ie_version < 9 && start == 0 && end == place.coverEnd - place.coverStart)
	        { rect = node.parentNode.getBoundingClientRect() }
	      else
	        { rect = getUsefulRect(range(node, start, end).getClientRects(), bias) }
	      if (rect.left || rect.right || start == 0) { break }
	      end = start
	      start = start - 1
	      collapse = "right"
	    }
	    if (ie && ie_version < 11) { rect = maybeUpdateRectForZooming(cm.display.measure, rect) }
	  } else { // If it is a widget, simply get the box for the whole widget.
	    if (start > 0) { collapse = bias = "right" }
	    var rects
	    if (cm.options.lineWrapping && (rects = node.getClientRects()).length > 1)
	      { rect = rects[bias == "right" ? rects.length - 1 : 0] }
	    else
	      { rect = node.getBoundingClientRect() }
	  }
	  if (ie && ie_version < 9 && !start && (!rect || !rect.left && !rect.right)) {
	    var rSpan = node.parentNode.getClientRects()[0]
	    if (rSpan)
	      { rect = {left: rSpan.left, right: rSpan.left + charWidth(cm.display), top: rSpan.top, bottom: rSpan.bottom} }
	    else
	      { rect = nullRect }
	  }

	  var rtop = rect.top - prepared.rect.top, rbot = rect.bottom - prepared.rect.top
	  var mid = (rtop + rbot) / 2
	  var heights = prepared.view.measure.heights
	  var i = 0
	  for (; i < heights.length - 1; i++)
	    { if (mid < heights[i]) { break } }
	  var top = i ? heights[i - 1] : 0, bot = heights[i]
	  var result = {left: (collapse == "right" ? rect.right : rect.left) - prepared.rect.left,
	                right: (collapse == "left" ? rect.left : rect.right) - prepared.rect.left,
	                top: top, bottom: bot}
	  if (!rect.left && !rect.right) { result.bogus = true }
	  if (!cm.options.singleCursorHeightPerLine) { result.rtop = rtop; result.rbottom = rbot }

	  return result
	}

	// Work around problem with bounding client rects on ranges being
	// returned incorrectly when zoomed on IE10 and below.
	function maybeUpdateRectForZooming(measure, rect) {
	  if (!window.screen || screen.logicalXDPI == null ||
	      screen.logicalXDPI == screen.deviceXDPI || !hasBadZoomedRects(measure))
	    { return rect }
	  var scaleX = screen.logicalXDPI / screen.deviceXDPI
	  var scaleY = screen.logicalYDPI / screen.deviceYDPI
	  return {left: rect.left * scaleX, right: rect.right * scaleX,
	          top: rect.top * scaleY, bottom: rect.bottom * scaleY}
	}

	function clearLineMeasurementCacheFor(lineView) {
	  if (lineView.measure) {
	    lineView.measure.cache = {}
	    lineView.measure.heights = null
	    if (lineView.rest) { for (var i = 0; i < lineView.rest.length; i++)
	      { lineView.measure.caches[i] = {} } }
	  }
	}

	function clearLineMeasurementCache(cm) {
	  cm.display.externalMeasure = null
	  removeChildren(cm.display.lineMeasure)
	  for (var i = 0; i < cm.display.view.length; i++)
	    { clearLineMeasurementCacheFor(cm.display.view[i]) }
	}

	function clearCaches(cm) {
	  clearLineMeasurementCache(cm)
	  cm.display.cachedCharWidth = cm.display.cachedTextHeight = cm.display.cachedPaddingH = null
	  if (!cm.options.lineWrapping) { cm.display.maxLineChanged = true }
	  cm.display.lineNumChars = null
	}

	function pageScrollX() {
	  // Work around https://bugs.chromium.org/p/chromium/issues/detail?id=489206
	  // which causes page_Offset and bounding client rects to use
	  // different reference viewports and invalidate our calculations.
	  if (chrome && android) { return -(document.body.getBoundingClientRect().left - parseInt(getComputedStyle(document.body).marginLeft)) }
	  return window.pageXOffset || (document.documentElement || document.body).scrollLeft
	}
	function pageScrollY() {
	  if (chrome && android) { return -(document.body.getBoundingClientRect().top - parseInt(getComputedStyle(document.body).marginTop)) }
	  return window.pageYOffset || (document.documentElement || document.body).scrollTop
	}

	function widgetTopHeight(lineObj) {
	  var height = 0
	  if (lineObj.widgets) { for (var i = 0; i < lineObj.widgets.length; ++i) { if (lineObj.widgets[i].above)
	    { height += widgetHeight(lineObj.widgets[i]) } } }
	  return height
	}

	// Converts a {top, bottom, left, right} box from line-local
	// coordinates into another coordinate system. Context may be one of
	// "line", "div" (display.lineDiv), "local"./null (editor), "window",
	// or "page".
	function intoCoordSystem(cm, lineObj, rect, context, includeWidgets) {
	  if (!includeWidgets) {
	    var height = widgetTopHeight(lineObj)
	    rect.top += height; rect.bottom += height
	  }
	  if (context == "line") { return rect }
	  if (!context) { context = "local" }
	  var yOff = heightAtLine(lineObj)
	  if (context == "local") { yOff += paddingTop(cm.display) }
	  else { yOff -= cm.display.viewOffset }
	  if (context == "page" || context == "window") {
	    var lOff = cm.display.lineSpace.getBoundingClientRect()
	    yOff += lOff.top + (context == "window" ? 0 : pageScrollY())
	    var xOff = lOff.left + (context == "window" ? 0 : pageScrollX())
	    rect.left += xOff; rect.right += xOff
	  }
	  rect.top += yOff; rect.bottom += yOff
	  return rect
	}

	// Coverts a box from "div" coords to another coordinate system.
	// Context may be "window", "page", "div", or "local"./null.
	function fromCoordSystem(cm, coords, context) {
	  if (context == "div") { return coords }
	  var left = coords.left, top = coords.top
	  // First move into "page" coordinate system
	  if (context == "page") {
	    left -= pageScrollX()
	    top -= pageScrollY()
	  } else if (context == "local" || !context) {
	    var localBox = cm.display.sizer.getBoundingClientRect()
	    left += localBox.left
	    top += localBox.top
	  }

	  var lineSpaceBox = cm.display.lineSpace.getBoundingClientRect()
	  return {left: left - lineSpaceBox.left, top: top - lineSpaceBox.top}
	}

	function charCoords(cm, pos, context, lineObj, bias) {
	  if (!lineObj) { lineObj = getLine(cm.doc, pos.line) }
	  return intoCoordSystem(cm, lineObj, measureChar(cm, lineObj, pos.ch, bias), context)
	}

	// Returns a box for a given cursor position, which may have an
	// 'other' property containing the position of the secondary cursor
	// on a bidi boundary.
	// A cursor Pos(line, char, "before") is on the same visual line as `char - 1`
	// and after `char - 1` in writing order of `char - 1`
	// A cursor Pos(line, char, "after") is on the same visual line as `char`
	// and before `char` in writing order of `char`
	// Examples (upper-case letters are RTL, lower-case are LTR):
	//     Pos(0, 1, ...)
	//     before   after
	// ab     a|b     a|b
	// aB     a|B     aB|
	// Ab     |Ab     A|b
	// AB     B|A     B|A
	// Every position after the last character on a line is considered to stick
	// to the last character on the line.
	function cursorCoords(cm, pos, context, lineObj, preparedMeasure, varHeight) {
	  lineObj = lineObj || getLine(cm.doc, pos.line)
	  if (!preparedMeasure) { preparedMeasure = prepareMeasureForLine(cm, lineObj) }
	  function get(ch, right) {
	    var m = measureCharPrepared(cm, preparedMeasure, ch, right ? "right" : "left", varHeight)
	    if (right) { m.left = m.right; } else { m.right = m.left }
	    return intoCoordSystem(cm, lineObj, m, context)
	  }
	  var order = getOrder(lineObj, cm.doc.direction), ch = pos.ch, sticky = pos.sticky
	  if (ch >= lineObj.text.length) {
	    ch = lineObj.text.length
	    sticky = "before"
	  } else if (ch <= 0) {
	    ch = 0
	    sticky = "after"
	  }
	  if (!order) { return get(sticky == "before" ? ch - 1 : ch, sticky == "before") }

	  function getBidi(ch, partPos, invert) {
	    var part = order[partPos], right = part.level == 1
	    return get(invert ? ch - 1 : ch, right != invert)
	  }
	  var partPos = getBidiPartAt(order, ch, sticky)
	  var other = bidiOther
	  var val = getBidi(ch, partPos, sticky == "before")
	  if (other != null) { val.other = getBidi(ch, other, sticky != "before") }
	  return val
	}

	// Used to cheaply estimate the coordinates for a position. Used for
	// intermediate scroll updates.
	function estimateCoords(cm, pos) {
	  var left = 0
	  pos = clipPos(cm.doc, pos)
	  if (!cm.options.lineWrapping) { left = charWidth(cm.display) * pos.ch }
	  var lineObj = getLine(cm.doc, pos.line)
	  var top = heightAtLine(lineObj) + paddingTop(cm.display)
	  return {left: left, right: left, top: top, bottom: top + lineObj.height}
	}

	// Positions returned by coordsChar contain some extra information.
	// xRel is the relative x position of the input coordinates compared
	// to the found position (so xRel > 0 means the coordinates are to
	// the right of the character position, for example). When outside
	// is true, that means the coordinates lie outside the line's
	// vertical range.
	function PosWithInfo(line, ch, sticky, outside, xRel) {
	  var pos = Pos(line, ch, sticky)
	  pos.xRel = xRel
	  if (outside) { pos.outside = true }
	  return pos
	}

	// Compute the character position closest to the given coordinates.
	// Input must be lineSpace-local ("div" coordinate system).
	function coordsChar(cm, x, y) {
	  var doc = cm.doc
	  y += cm.display.viewOffset
	  if (y < 0) { return PosWithInfo(doc.first, 0, null, true, -1) }
	  var lineN = lineAtHeight(doc, y), last = doc.first + doc.size - 1
	  if (lineN > last)
	    { return PosWithInfo(doc.first + doc.size - 1, getLine(doc, last).text.length, null, true, 1) }
	  if (x < 0) { x = 0 }

	  var lineObj = getLine(doc, lineN)
	  for (;;) {
	    var found = coordsCharInner(cm, lineObj, lineN, x, y)
	    var merged = collapsedSpanAtEnd(lineObj)
	    var mergedPos = merged && merged.find(0, true)
	    if (merged && (found.ch > mergedPos.from.ch || found.ch == mergedPos.from.ch && found.xRel > 0))
	      { lineN = lineNo(lineObj = mergedPos.to.line) }
	    else
	      { return found }
	  }
	}

	function wrappedLineExtent(cm, lineObj, preparedMeasure, y) {
	  y -= widgetTopHeight(lineObj)
	  var end = lineObj.text.length
	  var begin = findFirst(function (ch) { return measureCharPrepared(cm, preparedMeasure, ch - 1).bottom <= y; }, end, 0)
	  end = findFirst(function (ch) { return measureCharPrepared(cm, preparedMeasure, ch).top > y; }, begin, end)
	  return {begin: begin, end: end}
	}

	function wrappedLineExtentChar(cm, lineObj, preparedMeasure, target) {
	  if (!preparedMeasure) { preparedMeasure = prepareMeasureForLine(cm, lineObj) }
	  var targetTop = intoCoordSystem(cm, lineObj, measureCharPrepared(cm, preparedMeasure, target), "line").top
	  return wrappedLineExtent(cm, lineObj, preparedMeasure, targetTop)
	}

	// Returns true if the given side of a box is after the given
	// coordinates, in top-to-bottom, left-to-right order.
	function boxIsAfter(box, x, y, left) {
	  return box.bottom <= y ? false : box.top > y ? true : (left ? box.left : box.right) > x
	}

	function coordsCharInner(cm, lineObj, lineNo, x, y) {
	  // Move y into line-local coordinate space
	  y -= heightAtLine(lineObj)
	  var preparedMeasure = prepareMeasureForLine(cm, lineObj)
	  // When directly calling `measureCharPrepared`, we have to adjust
	  // for the widgets at this line.
	  var widgetHeight = widgetTopHeight(lineObj)
	  var begin = 0, end = lineObj.text.length, ltr = true

	  var order = getOrder(lineObj, cm.doc.direction)
	  // If the line isn't plain left-to-right text, first figure out
	  // which bidi section the coordinates fall into.
	  if (order) {
	    var part = (cm.options.lineWrapping ? coordsBidiPartWrapped : coordsBidiPart)
	                 (cm, lineObj, lineNo, preparedMeasure, order, x, y)
	    ltr = part.level != 1
	    // The awkward -1 offsets are needed because findFirst (called
	    // on these below) will treat its first bound as inclusive,
	    // second as exclusive, but we want to actually address the
	    // characters in the part's range
	    begin = ltr ? part.from : part.to - 1
	    end = ltr ? part.to : part.from - 1
	  }

	  // A binary search to find the first character whose bounding box
	  // starts after the coordinates. If we run across any whose box wrap
	  // the coordinates, store that.
	  var chAround = null, boxAround = null
	  var ch = findFirst(function (ch) {
	    var box = measureCharPrepared(cm, preparedMeasure, ch)
	    box.top += widgetHeight; box.bottom += widgetHeight
	    if (!boxIsAfter(box, x, y, false)) { return false }
	    if (box.top <= y && box.left <= x) {
	      chAround = ch
	      boxAround = box
	    }
	    return true
	  }, begin, end)

	  var baseX, sticky, outside = false
	  // If a box around the coordinates was found, use that
	  if (boxAround) {
	    // Distinguish coordinates nearer to the left or right side of the box
	    var atLeft = x - boxAround.left < boxAround.right - x, atStart = atLeft == ltr
	    ch = chAround + (atStart ? 0 : 1)
	    sticky = atStart ? "after" : "before"
	    baseX = atLeft ? boxAround.left : boxAround.right
	  } else {
	    // (Adjust for extended bound, if necessary.)
	    if (!ltr && (ch == end || ch == begin)) { ch++ }
	    // To determine which side to associate with, get the box to the
	    // left of the character and compare it's vertical position to the
	    // coordinates
	    sticky = ch == 0 ? "after" : ch == lineObj.text.length ? "before" :
	      (measureCharPrepared(cm, preparedMeasure, ch - (ltr ? 1 : 0)).bottom + widgetHeight <= y) == ltr ?
	      "after" : "before"
	    // Now get accurate coordinates for this place, in order to get a
	    // base X position
	    var coords = cursorCoords(cm, Pos(lineNo, ch, sticky), "line", lineObj, preparedMeasure)
	    baseX = coords.left
	    outside = y < coords.top || y >= coords.bottom
	  }

	  ch = skipExtendingChars(lineObj.text, ch, 1)
	  return PosWithInfo(lineNo, ch, sticky, outside, x - baseX)
	}

	function coordsBidiPart(cm, lineObj, lineNo, preparedMeasure, order, x, y) {
	  // Bidi parts are sorted left-to-right, and in a non-line-wrapping
	  // situation, we can take this ordering to correspond to the visual
	  // ordering. This finds the first part whose end is after the given
	  // coordinates.
	  var index = findFirst(function (i) {
	    var part = order[i], ltr = part.level != 1
	    return boxIsAfter(cursorCoords(cm, Pos(lineNo, ltr ? part.to : part.from, ltr ? "before" : "after"),
	                                   "line", lineObj, preparedMeasure), x, y, true)
	  }, 0, order.length - 1)
	  var part = order[index]
	  // If this isn't the first part, the part's start is also after
	  // the coordinates, and the coordinates aren't on the same line as
	  // that start, move one part back.
	  if (index > 0) {
	    var ltr = part.level != 1
	    var start = cursorCoords(cm, Pos(lineNo, ltr ? part.from : part.to, ltr ? "after" : "before"),
	                             "line", lineObj, preparedMeasure)
	    if (boxIsAfter(start, x, y, true) && start.top > y)
	      { part = order[index - 1] }
	  }
	  return part
	}

	function coordsBidiPartWrapped(cm, lineObj, _lineNo, preparedMeasure, order, x, y) {
	  // In a wrapped line, rtl text on wrapping boundaries can do things
	  // that don't correspond to the ordering in our `order` array at
	  // all, so a binary search doesn't work, and we want to return a
	  // part that only spans one line so that the binary search in
	  // coordsCharInner is safe. As such, we first find the extent of the
	  // wrapped line, and then do a flat search in which we discard any
	  // spans that aren't on the line.
	  var ref = wrappedLineExtent(cm, lineObj, preparedMeasure, y);
	  var begin = ref.begin;
	  var end = ref.end;
	  if (/\s/.test(lineObj.text.charAt(end - 1))) { end-- }
	  var part = null, closestDist = null
	  for (var i = 0; i < order.length; i++) {
	    var p = order[i]
	    if (p.from >= end || p.to <= begin) { continue }
	    var ltr = p.level != 1
	    var endX = measureCharPrepared(cm, preparedMeasure, ltr ? Math.min(end, p.to) - 1 : Math.max(begin, p.from)).right
	    // Weigh against spans ending before this, so that they are only
	    // picked if nothing ends after
	    var dist = endX < x ? x - endX + 1e9 : endX - x
	    if (!part || closestDist > dist) {
	      part = p
	      closestDist = dist
	    }
	  }
	  if (!part) { part = order[order.length - 1] }
	  // Clip the part to the wrapped line.
	  if (part.from < begin) { part = {from: begin, to: part.to, level: part.level} }
	  if (part.to > end) { part = {from: part.from, to: end, level: part.level} }
	  return part
	}

	var measureText
	// Compute the default text height.
	function textHeight(display) {
	  if (display.cachedTextHeight != null) { return display.cachedTextHeight }
	  if (measureText == null) {
	    measureText = elt("pre")
	    // Measure a bunch of lines, for browsers that compute
	    // fractional heights.
	    for (var i = 0; i < 49; ++i) {
	      measureText.appendChild(document.createTextNode("x"))
	      measureText.appendChild(elt("br"))
	    }
	    measureText.appendChild(document.createTextNode("x"))
	  }
	  removeChildrenAndAdd(display.measure, measureText)
	  var height = measureText.offsetHeight / 50
	  if (height > 3) { display.cachedTextHeight = height }
	  removeChildren(display.measure)
	  return height || 1
	}

	// Compute the default character width.
	function charWidth(display) {
	  if (display.cachedCharWidth != null) { return display.cachedCharWidth }
	  var anchor = elt("span", "xxxxxxxxxx")
	  var pre = elt("pre", [anchor])
	  removeChildrenAndAdd(display.measure, pre)
	  var rect = anchor.getBoundingClientRect(), width = (rect.right - rect.left) / 10
	  if (width > 2) { display.cachedCharWidth = width }
	  return width || 10
	}

	// Do a bulk-read of the DOM positions and sizes needed to draw the
	// view, so that we don't interleave reading and writing to the DOM.
	function getDimensions(cm) {
	  var d = cm.display, left = {}, width = {}
	  var gutterLeft = d.gutters.clientLeft
	  for (var n = d.gutters.firstChild, i = 0; n; n = n.nextSibling, ++i) {
	    left[cm.options.gutters[i]] = n.offsetLeft + n.clientLeft + gutterLeft
	    width[cm.options.gutters[i]] = n.clientWidth
	  }
	  return {fixedPos: compensateForHScroll(d),
	          gutterTotalWidth: d.gutters.offsetWidth,
	          gutterLeft: left,
	          gutterWidth: width,
	          wrapperWidth: d.wrapper.clientWidth}
	}

	// Computes display.scroller.scrollLeft + display.gutters.offsetWidth,
	// but using getBoundingClientRect to get a sub-pixel-accurate
	// result.
	function compensateForHScroll(display) {
	  return display.scroller.getBoundingClientRect().left - display.sizer.getBoundingClientRect().left
	}

	// Returns a function that estimates the height of a line, to use as
	// first approximation until the line becomes visible (and is thus
	// properly measurable).
	function estimateHeight(cm) {
	  var th = textHeight(cm.display), wrapping = cm.options.lineWrapping
	  var perLine = wrapping && Math.max(5, cm.display.scroller.clientWidth / charWidth(cm.display) - 3)
	  return function (line) {
	    if (lineIsHidden(cm.doc, line)) { return 0 }

	    var widgetsHeight = 0
	    if (line.widgets) { for (var i = 0; i < line.widgets.length; i++) {
	      if (line.widgets[i].height) { widgetsHeight += line.widgets[i].height }
	    } }

	    if (wrapping)
	      { return widgetsHeight + (Math.ceil(line.text.length / perLine) || 1) * th }
	    else
	      { return widgetsHeight + th }
	  }
	}

	function estimateLineHeights(cm) {
	  var doc = cm.doc, est = estimateHeight(cm)
	  doc.iter(function (line) {
	    var estHeight = est(line)
	    if (estHeight != line.height) { updateLineHeight(line, estHeight) }
	  })
	}

	// Given a mouse event, find the corresponding position. If liberal
	// is false, it checks whether a gutter or scrollbar was clicked,
	// and returns null if it was. forRect is used by rectangular
	// selections, and tries to estimate a character position even for
	// coordinates beyond the right of the text.
	function posFromMouse(cm, e, liberal, forRect) {
	  var display = cm.display
	  if (!liberal && e_target(e).getAttribute("cm-not-content") == "true") { return null }

	  var x, y, space = display.lineSpace.getBoundingClientRect()
	  // Fails unpredictably on IE[67] when mouse is dragged around quickly.
	  try { x = e.clientX - space.left; y = e.clientY - space.top }
	  catch (e) { return null }
	  var coords = coordsChar(cm, x, y), line
	  if (forRect && coords.xRel == 1 && (line = getLine(cm.doc, coords.line).text).length == coords.ch) {
	    var colDiff = countColumn(line, line.length, cm.options.tabSize) - line.length
	    coords = Pos(coords.line, Math.max(0, Math.round((x - paddingH(cm.display).left) / charWidth(cm.display)) - colDiff))
	  }
	  return coords
	}

	// Find the view element corresponding to a given line. Return null
	// when the line isn't visible.
	function findViewIndex(cm, n) {
	  if (n >= cm.display.viewTo) { return null }
	  n -= cm.display.viewFrom
	  if (n < 0) { return null }
	  var view = cm.display.view
	  for (var i = 0; i < view.length; i++) {
	    n -= view[i].size
	    if (n < 0) { return i }
	  }
	}

	function updateSelection(cm) {
	  cm.display.input.showSelection(cm.display.input.prepareSelection())
	}

	function prepareSelection(cm, primary) {
	  if ( primary === void 0 ) primary = true;

	  var doc = cm.doc, result = {}
	  var curFragment = result.cursors = document.createDocumentFragment()
	  var selFragment = result.selection = document.createDocumentFragment()

	  for (var i = 0; i < doc.sel.ranges.length; i++) {
	    if (!primary && i == doc.sel.primIndex) { continue }
	    var range = doc.sel.ranges[i]
	    if (range.from().line >= cm.display.viewTo || range.to().line < cm.display.viewFrom) { continue }
	    var collapsed = range.empty()
	    if (collapsed || cm.options.showCursorWhenSelecting)
	      { drawSelectionCursor(cm, range.head, curFragment) }
	    if (!collapsed)
	      { drawSelectionRange(cm, range, selFragment) }
	  }
	  return result
	}

	// Draws a cursor for the given range
	function drawSelectionCursor(cm, head, output) {
	  var pos = cursorCoords(cm, head, "div", null, null, !cm.options.singleCursorHeightPerLine)

	  var cursor = output.appendChild(elt("div", "\u00a0", "CodeMirror-cursor"))
	  cursor.style.left = pos.left + "px"
	  cursor.style.top = pos.top + "px"
	  cursor.style.height = Math.max(0, pos.bottom - pos.top) * cm.options.cursorHeight + "px"

	  if (pos.other) {
	    // Secondary cursor, shown when on a 'jump' in bi-directional text
	    var otherCursor = output.appendChild(elt("div", "\u00a0", "CodeMirror-cursor CodeMirror-secondarycursor"))
	    otherCursor.style.display = ""
	    otherCursor.style.left = pos.other.left + "px"
	    otherCursor.style.top = pos.other.top + "px"
	    otherCursor.style.height = (pos.other.bottom - pos.other.top) * .85 + "px"
	  }
	}

	function cmpCoords(a, b) { return a.top - b.top || a.left - b.left }

	// Draws the given range as a highlighted selection
	function drawSelectionRange(cm, range, output) {
	  var display = cm.display, doc = cm.doc
	  var fragment = document.createDocumentFragment()
	  var padding = paddingH(cm.display), leftSide = padding.left
	  var rightSide = Math.max(display.sizerWidth, displayWidth(cm) - display.sizer.offsetLeft) - padding.right
	  var docLTR = doc.direction == "ltr"

	  function add(left, top, width, bottom) {
	    if (top < 0) { top = 0 }
	    top = Math.round(top)
	    bottom = Math.round(bottom)
	    fragment.appendChild(elt("div", null, "CodeMirror-selected", ("position: absolute; left: " + left + "px;\n                             top: " + top + "px; width: " + (width == null ? rightSide - left : width) + "px;\n                             height: " + (bottom - top) + "px")))
	  }

	  function drawForLine(line, fromArg, toArg) {
	    var lineObj = getLine(doc, line)
	    var lineLen = lineObj.text.length
	    var start, end
	    function coords(ch, bias) {
	      return charCoords(cm, Pos(line, ch), "div", lineObj, bias)
	    }

	    function wrapX(pos, dir, side) {
	      var extent = wrappedLineExtentChar(cm, lineObj, null, pos)
	      var prop = (dir == "ltr") == (side == "after") ? "left" : "right"
	      var ch = side == "after" ? extent.begin : extent.end - (/\s/.test(lineObj.text.charAt(extent.end - 1)) ? 2 : 1)
	      return coords(ch, prop)[prop]
	    }

	    var order = getOrder(lineObj, doc.direction)
	    iterateBidiSections(order, fromArg || 0, toArg == null ? lineLen : toArg, function (from, to, dir, i) {
	      var ltr = dir == "ltr"
	      var fromPos = coords(from, ltr ? "left" : "right")
	      var toPos = coords(to - 1, ltr ? "right" : "left")

	      var openStart = fromArg == null && from == 0, openEnd = toArg == null && to == lineLen
	      var first = i == 0, last = !order || i == order.length - 1
	      if (toPos.top - fromPos.top <= 3) { // Single line
	        var openLeft = (docLTR ? openStart : openEnd) && first
	        var openRight = (docLTR ? openEnd : openStart) && last
	        var left = openLeft ? leftSide : (ltr ? fromPos : toPos).left
	        var right = openRight ? rightSide : (ltr ? toPos : fromPos).right
	        add(left, fromPos.top, right - left, fromPos.bottom)
	      } else { // Multiple lines
	        var topLeft, topRight, botLeft, botRight
	        if (ltr) {
	          topLeft = docLTR && openStart && first ? leftSide : fromPos.left
	          topRight = docLTR ? rightSide : wrapX(from, dir, "before")
	          botLeft = docLTR ? leftSide : wrapX(to, dir, "after")
	          botRight = docLTR && openEnd && last ? rightSide : toPos.right
	        } else {
	          topLeft = !docLTR ? leftSide : wrapX(from, dir, "before")
	          topRight = !docLTR && openStart && first ? rightSide : fromPos.right
	          botLeft = !docLTR && openEnd && last ? leftSide : toPos.left
	          botRight = !docLTR ? rightSide : wrapX(to, dir, "after")
	        }
	        add(topLeft, fromPos.top, topRight - topLeft, fromPos.bottom)
	        if (fromPos.bottom < toPos.top) { add(leftSide, fromPos.bottom, null, toPos.top) }
	        add(botLeft, toPos.top, botRight - botLeft, toPos.bottom)
	      }

	      if (!start || cmpCoords(fromPos, start) < 0) { start = fromPos }
	      if (cmpCoords(toPos, start) < 0) { start = toPos }
	      if (!end || cmpCoords(fromPos, end) < 0) { end = fromPos }
	      if (cmpCoords(toPos, end) < 0) { end = toPos }
	    })
	    return {start: start, end: end}
	  }

	  var sFrom = range.from(), sTo = range.to()
	  if (sFrom.line == sTo.line) {
	    drawForLine(sFrom.line, sFrom.ch, sTo.ch)
	  } else {
	    var fromLine = getLine(doc, sFrom.line), toLine = getLine(doc, sTo.line)
	    var singleVLine = visualLine(fromLine) == visualLine(toLine)
	    var leftEnd = drawForLine(sFrom.line, sFrom.ch, singleVLine ? fromLine.text.length + 1 : null).end
	    var rightStart = drawForLine(sTo.line, singleVLine ? 0 : null, sTo.ch).start
	    if (singleVLine) {
	      if (leftEnd.top < rightStart.top - 2) {
	        add(leftEnd.right, leftEnd.top, null, leftEnd.bottom)
	        add(leftSide, rightStart.top, rightStart.left, rightStart.bottom)
	      } else {
	        add(leftEnd.right, leftEnd.top, rightStart.left - leftEnd.right, leftEnd.bottom)
	      }
	    }
	    if (leftEnd.bottom < rightStart.top)
	      { add(leftSide, leftEnd.bottom, null, rightStart.top) }
	  }

	  output.appendChild(fragment)
	}

	// Cursor-blinking
	function restartBlink(cm) {
	  if (!cm.state.focused) { return }
	  var display = cm.display
	  clearInterval(display.blinker)
	  var on = true
	  display.cursorDiv.style.visibility = ""
	  if (cm.options.cursorBlinkRate > 0)
	    { display.blinker = setInterval(function () { return display.cursorDiv.style.visibility = (on = !on) ? "" : "hidden"; },
	      cm.options.cursorBlinkRate) }
	  else if (cm.options.cursorBlinkRate < 0)
	    { display.cursorDiv.style.visibility = "hidden" }
	}

	function ensureFocus(cm) {
	  if (!cm.state.focused) { cm.display.input.focus(); onFocus(cm) }
	}

	function delayBlurEvent(cm) {
	  cm.state.delayingBlurEvent = true
	  setTimeout(function () { if (cm.state.delayingBlurEvent) {
	    cm.state.delayingBlurEvent = false
	    onBlur(cm)
	  } }, 100)
	}

	function onFocus(cm, e) {
	  if (cm.state.delayingBlurEvent) { cm.state.delayingBlurEvent = false }

	  if (cm.options.readOnly == "nocursor") { return }
	  if (!cm.state.focused) {
	    signal(cm, "focus", cm, e)
	    cm.state.focused = true
	    addClass(cm.display.wrapper, "CodeMirror-focused")
	    // This test prevents this from firing when a context
	    // menu is closed (since the input reset would kill the
	    // select-all detection hack)
	    if (!cm.curOp && cm.display.selForContextMenu != cm.doc.sel) {
	      cm.display.input.reset()
	      if (webkit) { setTimeout(function () { return cm.display.input.reset(true); }, 20) } // Issue #1730
	    }
	    cm.display.input.receivedFocus()
	  }
	  restartBlink(cm)
	}
	function onBlur(cm, e) {
	  if (cm.state.delayingBlurEvent) { return }

	  if (cm.state.focused) {
	    signal(cm, "blur", cm, e)
	    cm.state.focused = false
	    rmClass(cm.display.wrapper, "CodeMirror-focused")
	  }
	  clearInterval(cm.display.blinker)
	  setTimeout(function () { if (!cm.state.focused) { cm.display.shift = false } }, 150)
	}

	// Read the actual heights of the rendered lines, and update their
	// stored heights to match.
	function updateHeightsInViewport(cm) {
	  var display = cm.display
	  var prevBottom = display.lineDiv.offsetTop
	  for (var i = 0; i < display.view.length; i++) {
	    var cur = display.view[i], height = (void 0)
	    if (cur.hidden) { continue }
	    if (ie && ie_version < 8) {
	      var bot = cur.node.offsetTop + cur.node.offsetHeight
	      height = bot - prevBottom
	      prevBottom = bot
	    } else {
	      var box = cur.node.getBoundingClientRect()
	      height = box.bottom - box.top
	    }
	    var diff = cur.line.height - height
	    if (height < 2) { height = textHeight(display) }
	    if (diff > .005 || diff < -.005) {
	      updateLineHeight(cur.line, height)
	      updateWidgetHeight(cur.line)
	      if (cur.rest) { for (var j = 0; j < cur.rest.length; j++)
	        { updateWidgetHeight(cur.rest[j]) } }
	    }
	  }
	}

	// Read and store the height of line widgets associated with the
	// given line.
	function updateWidgetHeight(line) {
	  if (line.widgets) { for (var i = 0; i < line.widgets.length; ++i) {
	    var w = line.widgets[i], parent = w.node.parentNode
	    if (parent) { w.height = parent.offsetHeight }
	  } }
	}

	// Compute the lines that are visible in a given viewport (defaults
	// the the current scroll position). viewport may contain top,
	// height, and ensure (see op.scrollToPos) properties.
	function visibleLines(display, doc, viewport) {
	  var top = viewport && viewport.top != null ? Math.max(0, viewport.top) : display.scroller.scrollTop
	  top = Math.floor(top - paddingTop(display))
	  var bottom = viewport && viewport.bottom != null ? viewport.bottom : top + display.wrapper.clientHeight

	  var from = lineAtHeight(doc, top), to = lineAtHeight(doc, bottom)
	  // Ensure is a {from: {line, ch}, to: {line, ch}} object, and
	  // forces those lines into the viewport (if possible).
	  if (viewport && viewport.ensure) {
	    var ensureFrom = viewport.ensure.from.line, ensureTo = viewport.ensure.to.line
	    if (ensureFrom < from) {
	      from = ensureFrom
	      to = lineAtHeight(doc, heightAtLine(getLine(doc, ensureFrom)) + display.wrapper.clientHeight)
	    } else if (Math.min(ensureTo, doc.lastLine()) >= to) {
	      from = lineAtHeight(doc, heightAtLine(getLine(doc, ensureTo)) - display.wrapper.clientHeight)
	      to = ensureTo
	    }
	  }
	  return {from: from, to: Math.max(to, from + 1)}
	}

	// Re-align line numbers and gutter marks to compensate for
	// horizontal scrolling.
	function alignHorizontally(cm) {
	  var display = cm.display, view = display.view
	  if (!display.alignWidgets && (!display.gutters.firstChild || !cm.options.fixedGutter)) { return }
	  var comp = compensateForHScroll(display) - display.scroller.scrollLeft + cm.doc.scrollLeft
	  var gutterW = display.gutters.offsetWidth, left = comp + "px"
	  for (var i = 0; i < view.length; i++) { if (!view[i].hidden) {
	    if (cm.options.fixedGutter) {
	      if (view[i].gutter)
	        { view[i].gutter.style.left = left }
	      if (view[i].gutterBackground)
	        { view[i].gutterBackground.style.left = left }
	    }
	    var align = view[i].alignable
	    if (align) { for (var j = 0; j < align.length; j++)
	      { align[j].style.left = left } }
	  } }
	  if (cm.options.fixedGutter)
	    { display.gutters.style.left = (comp + gutterW) + "px" }
	}

	// Used to ensure that the line number gutter is still the right
	// size for the current document size. Returns true when an update
	// is needed.
	function maybeUpdateLineNumberWidth(cm) {
	  if (!cm.options.lineNumbers) { return false }
	  var doc = cm.doc, last = lineNumberFor(cm.options, doc.first + doc.size - 1), display = cm.display
	  if (last.length != display.lineNumChars) {
	    var test = display.measure.appendChild(elt("div", [elt("div", last)],
	                                               "CodeMirror-linenumber CodeMirror-gutter-elt"))
	    var innerW = test.firstChild.offsetWidth, padding = test.offsetWidth - innerW
	    display.lineGutter.style.width = ""
	    display.lineNumInnerWidth = Math.max(innerW, display.lineGutter.offsetWidth - padding) + 1
	    display.lineNumWidth = display.lineNumInnerWidth + padding
	    display.lineNumChars = display.lineNumInnerWidth ? last.length : -1
	    display.lineGutter.style.width = display.lineNumWidth + "px"
	    updateGutterSpace(cm)
	    return true
	  }
	  return false
	}

	// SCROLLING THINGS INTO VIEW

	// If an editor sits on the top or bottom of the window, partially
	// scrolled out of view, this ensures that the cursor is visible.
	function maybeScrollWindow(cm, rect) {
	  if (signalDOMEvent(cm, "scrollCursorIntoView")) { return }

	  var display = cm.display, box = display.sizer.getBoundingClientRect(), doScroll = null
	  if (rect.top + box.top < 0) { doScroll = true }
	  else if (rect.bottom + box.top > (window.innerHeight || document.documentElement.clientHeight)) { doScroll = false }
	  if (doScroll != null && !phantom) {
	    var scrollNode = elt("div", "\u200b", null, ("position: absolute;\n                         top: " + (rect.top - display.viewOffset - paddingTop(cm.display)) + "px;\n                         height: " + (rect.bottom - rect.top + scrollGap(cm) + display.barHeight) + "px;\n                         left: " + (rect.left) + "px; width: " + (Math.max(2, rect.right - rect.left)) + "px;"))
	    cm.display.lineSpace.appendChild(scrollNode)
	    scrollNode.scrollIntoView(doScroll)
	    cm.display.lineSpace.removeChild(scrollNode)
	  }
	}

	// Scroll a given position into view (immediately), verifying that
	// it actually became visible (as line heights are accurately
	// measured, the position of something may 'drift' during drawing).
	function scrollPosIntoView(cm, pos, end, margin) {
	  if (margin == null) { margin = 0 }
	  var rect
	  if (!cm.options.lineWrapping && pos == end) {
	    // Set pos and end to the cursor positions around the character pos sticks to
	    // If pos.sticky == "before", that is around pos.ch - 1, otherwise around pos.ch
	    // If pos == Pos(_, 0, "before"), pos and end are unchanged
	    pos = pos.ch ? Pos(pos.line, pos.sticky == "before" ? pos.ch - 1 : pos.ch, "after") : pos
	    end = pos.sticky == "before" ? Pos(pos.line, pos.ch + 1, "before") : pos
	  }
	  for (var limit = 0; limit < 5; limit++) {
	    var changed = false
	    var coords = cursorCoords(cm, pos)
	    var endCoords = !end || end == pos ? coords : cursorCoords(cm, end)
	    rect = {left: Math.min(coords.left, endCoords.left),
	            top: Math.min(coords.top, endCoords.top) - margin,
	            right: Math.max(coords.left, endCoords.left),
	            bottom: Math.max(coords.bottom, endCoords.bottom) + margin}
	    var scrollPos = calculateScrollPos(cm, rect)
	    var startTop = cm.doc.scrollTop, startLeft = cm.doc.scrollLeft
	    if (scrollPos.scrollTop != null) {
	      updateScrollTop(cm, scrollPos.scrollTop)
	      if (Math.abs(cm.doc.scrollTop - startTop) > 1) { changed = true }
	    }
	    if (scrollPos.scrollLeft != null) {
	      setScrollLeft(cm, scrollPos.scrollLeft)
	      if (Math.abs(cm.doc.scrollLeft - startLeft) > 1) { changed = true }
	    }
	    if (!changed) { break }
	  }
	  return rect
	}

	// Scroll a given set of coordinates into view (immediately).
	function scrollIntoView(cm, rect) {
	  var scrollPos = calculateScrollPos(cm, rect)
	  if (scrollPos.scrollTop != null) { updateScrollTop(cm, scrollPos.scrollTop) }
	  if (scrollPos.scrollLeft != null) { setScrollLeft(cm, scrollPos.scrollLeft) }
	}

	// Calculate a new scroll position needed to scroll the given
	// rectangle into view. Returns an object with scrollTop and
	// scrollLeft properties. When these are undefined, the
	// vertical/horizontal position does not need to be adjusted.
	function calculateScrollPos(cm, rect) {
	  var display = cm.display, snapMargin = textHeight(cm.display)
	  if (rect.top < 0) { rect.top = 0 }
	  var screentop = cm.curOp && cm.curOp.scrollTop != null ? cm.curOp.scrollTop : display.scroller.scrollTop
	  var screen = displayHeight(cm), result = {}
	  if (rect.bottom - rect.top > screen) { rect.bottom = rect.top + screen }
	  var docBottom = cm.doc.height + paddingVert(display)
	  var atTop = rect.top < snapMargin, atBottom = rect.bottom > docBottom - snapMargin
	  if (rect.top < screentop) {
	    result.scrollTop = atTop ? 0 : rect.top
	  } else if (rect.bottom > screentop + screen) {
	    var newTop = Math.min(rect.top, (atBottom ? docBottom : rect.bottom) - screen)
	    if (newTop != screentop) { result.scrollTop = newTop }
	  }

	  var screenleft = cm.curOp && cm.curOp.scrollLeft != null ? cm.curOp.scrollLeft : display.scroller.scrollLeft
	  var screenw = displayWidth(cm) - (cm.options.fixedGutter ? display.gutters.offsetWidth : 0)
	  var tooWide = rect.right - rect.left > screenw
	  if (tooWide) { rect.right = rect.left + screenw }
	  if (rect.left < 10)
	    { result.scrollLeft = 0 }
	  else if (rect.left < screenleft)
	    { result.scrollLeft = Math.max(0, rect.left - (tooWide ? 0 : 10)) }
	  else if (rect.right > screenw + screenleft - 3)
	    { result.scrollLeft = rect.right + (tooWide ? 0 : 10) - screenw }
	  return result
	}

	// Store a relative adjustment to the scroll position in the current
	// operation (to be applied when the operation finishes).
	function addToScrollTop(cm, top) {
	  if (top == null) { return }
	  resolveScrollToPos(cm)
	  cm.curOp.scrollTop = (cm.curOp.scrollTop == null ? cm.doc.scrollTop : cm.curOp.scrollTop) + top
	}

	// Make sure that at the end of the operation the current cursor is
	// shown.
	function ensureCursorVisible(cm) {
	  resolveScrollToPos(cm)
	  var cur = cm.getCursor()
	  cm.curOp.scrollToPos = {from: cur, to: cur, margin: cm.options.cursorScrollMargin}
	}

	function scrollToCoords(cm, x, y) {
	  if (x != null || y != null) { resolveScrollToPos(cm) }
	  if (x != null) { cm.curOp.scrollLeft = x }
	  if (y != null) { cm.curOp.scrollTop = y }
	}

	function scrollToRange(cm, range) {
	  resolveScrollToPos(cm)
	  cm.curOp.scrollToPos = range
	}

	// When an operation has its scrollToPos property set, and another
	// scroll action is applied before the end of the operation, this
	// 'simulates' scrolling that position into view in a cheap way, so
	// that the effect of intermediate scroll commands is not ignored.
	function resolveScrollToPos(cm) {
	  var range = cm.curOp.scrollToPos
	  if (range) {
	    cm.curOp.scrollToPos = null
	    var from = estimateCoords(cm, range.from), to = estimateCoords(cm, range.to)
	    scrollToCoordsRange(cm, from, to, range.margin)
	  }
	}

	function scrollToCoordsRange(cm, from, to, margin) {
	  var sPos = calculateScrollPos(cm, {
	    left: Math.min(from.left, to.left),
	    top: Math.min(from.top, to.top) - margin,
	    right: Math.max(from.right, to.right),
	    bottom: Math.max(from.bottom, to.bottom) + margin
	  })
	  scrollToCoords(cm, sPos.scrollLeft, sPos.scrollTop)
	}

	// Sync the scrollable area and scrollbars, ensure the viewport
	// covers the visible area.
	function updateScrollTop(cm, val) {
	  if (Math.abs(cm.doc.scrollTop - val) < 2) { return }
	  if (!gecko) { updateDisplaySimple(cm, {top: val}) }
	  setScrollTop(cm, val, true)
	  if (gecko) { updateDisplaySimple(cm) }
	  startWorker(cm, 100)
	}

	function setScrollTop(cm, val, forceScroll) {
	  val = Math.min(cm.display.scroller.scrollHeight - cm.display.scroller.clientHeight, val)
	  if (cm.display.scroller.scrollTop == val && !forceScroll) { return }
	  cm.doc.scrollTop = val
	  cm.display.scrollbars.setScrollTop(val)
	  if (cm.display.scroller.scrollTop != val) { cm.display.scroller.scrollTop = val }
	}

	// Sync scroller and scrollbar, ensure the gutter elements are
	// aligned.
	function setScrollLeft(cm, val, isScroller, forceScroll) {
	  val = Math.min(val, cm.display.scroller.scrollWidth - cm.display.scroller.clientWidth)
	  if ((isScroller ? val == cm.doc.scrollLeft : Math.abs(cm.doc.scrollLeft - val) < 2) && !forceScroll) { return }
	  cm.doc.scrollLeft = val
	  alignHorizontally(cm)
	  if (cm.display.scroller.scrollLeft != val) { cm.display.scroller.scrollLeft = val }
	  cm.display.scrollbars.setScrollLeft(val)
	}

	// SCROLLBARS

	// Prepare DOM reads needed to update the scrollbars. Done in one
	// shot to minimize update/measure roundtrips.
	function measureForScrollbars(cm) {
	  var d = cm.display, gutterW = d.gutters.offsetWidth
	  var docH = Math.round(cm.doc.height + paddingVert(cm.display))
	  return {
	    clientHeight: d.scroller.clientHeight,
	    viewHeight: d.wrapper.clientHeight,
	    scrollWidth: d.scroller.scrollWidth, clientWidth: d.scroller.clientWidth,
	    viewWidth: d.wrapper.clientWidth,
	    barLeft: cm.options.fixedGutter ? gutterW : 0,
	    docHeight: docH,
	    scrollHeight: docH + scrollGap(cm) + d.barHeight,
	    nativeBarWidth: d.nativeBarWidth,
	    gutterWidth: gutterW
	  }
	}

	var NativeScrollbars = function(place, scroll, cm) {
	  this.cm = cm
	  var vert = this.vert = elt("div", [elt("div", null, null, "min-width: 1px")], "CodeMirror-vscrollbar")
	  var horiz = this.horiz = elt("div", [elt("div", null, null, "height: 100%; min-height: 1px")], "CodeMirror-hscrollbar")
	  place(vert); place(horiz)

	  on(vert, "scroll", function () {
	    if (vert.clientHeight) { scroll(vert.scrollTop, "vertical") }
	  })
	  on(horiz, "scroll", function () {
	    if (horiz.clientWidth) { scroll(horiz.scrollLeft, "horizontal") }
	  })

	  this.checkedZeroWidth = false
	  // Need to set a minimum width to see the scrollbar on IE7 (but must not set it on IE8).
	  if (ie && ie_version < 8) { this.horiz.style.minHeight = this.vert.style.minWidth = "18px" }
	};

	NativeScrollbars.prototype.update = function (measure) {
	  var needsH = measure.scrollWidth > measure.clientWidth + 1
	  var needsV = measure.scrollHeight > measure.clientHeight + 1
	  var sWidth = measure.nativeBarWidth

	  if (needsV) {
	    this.vert.style.display = "block"
	    this.vert.style.bottom = needsH ? sWidth + "px" : "0"
	    var totalHeight = measure.viewHeight - (needsH ? sWidth : 0)
	    // A bug in IE8 can cause this value to be negative, so guard it.
	    this.vert.firstChild.style.height =
	      Math.max(0, measure.scrollHeight - measure.clientHeight + totalHeight) + "px"
	  } else {
	    this.vert.style.display = ""
	    this.vert.firstChild.style.height = "0"
	  }

	  if (needsH) {
	    this.horiz.style.display = "block"
	    this.horiz.style.right = needsV ? sWidth + "px" : "0"
	    this.horiz.style.left = measure.barLeft + "px"
	    var totalWidth = measure.viewWidth - measure.barLeft - (needsV ? sWidth : 0)
	    this.horiz.firstChild.style.width =
	      Math.max(0, measure.scrollWidth - measure.clientWidth + totalWidth) + "px"
	  } else {
	    this.horiz.style.display = ""
	    this.horiz.firstChild.style.width = "0"
	  }

	  if (!this.checkedZeroWidth && measure.clientHeight > 0) {
	    if (sWidth == 0) { this.zeroWidthHack() }
	    this.checkedZeroWidth = true
	  }

	  return {right: needsV ? sWidth : 0, bottom: needsH ? sWidth : 0}
	};

	NativeScrollbars.prototype.setScrollLeft = function (pos) {
	  if (this.horiz.scrollLeft != pos) { this.horiz.scrollLeft = pos }
	  if (this.disableHoriz) { this.enableZeroWidthBar(this.horiz, this.disableHoriz, "horiz") }
	};

	NativeScrollbars.prototype.setScrollTop = function (pos) {
	  if (this.vert.scrollTop != pos) { this.vert.scrollTop = pos }
	  if (this.disableVert) { this.enableZeroWidthBar(this.vert, this.disableVert, "vert") }
	};

	NativeScrollbars.prototype.zeroWidthHack = function () {
	  var w = mac && !mac_geMountainLion ? "12px" : "18px"
	  this.horiz.style.height = this.vert.style.width = w
	  this.horiz.style.pointerEvents = this.vert.style.pointerEvents = "none"
	  this.disableHoriz = new Delayed
	  this.disableVert = new Delayed
	};

	NativeScrollbars.prototype.enableZeroWidthBar = function (bar, delay, type) {
	  bar.style.pointerEvents = "auto"
	  function maybeDisable() {
	    // To find out whether the scrollbar is still visible, we
	    // check whether the element under the pixel in the bottom
	    // right corner of the scrollbar box is the scrollbar box
	    // itself (when the bar is still visible) or its filler child
	    // (when the bar is hidden). If it is still visible, we keep
	    // it enabled, if it's hidden, we disable pointer events.
	    var box = bar.getBoundingClientRect()
	    var elt = type == "vert" ? document.elementFromPoint(box.right - 1, (box.top + box.bottom) / 2)
	        : document.elementFromPoint((box.right + box.left) / 2, box.bottom - 1)
	    if (elt != bar) { bar.style.pointerEvents = "none" }
	    else { delay.set(1000, maybeDisable) }
	  }
	  delay.set(1000, maybeDisable)
	};

	NativeScrollbars.prototype.clear = function () {
	  var parent = this.horiz.parentNode
	  parent.removeChild(this.horiz)
	  parent.removeChild(this.vert)
	};

	var NullScrollbars = function () {};

	NullScrollbars.prototype.update = function () { return {bottom: 0, right: 0} };
	NullScrollbars.prototype.setScrollLeft = function () {};
	NullScrollbars.prototype.setScrollTop = function () {};
	NullScrollbars.prototype.clear = function () {};

	function updateScrollbars(cm, measure) {
	  if (!measure) { measure = measureForScrollbars(cm) }
	  var startWidth = cm.display.barWidth, startHeight = cm.display.barHeight
	  updateScrollbarsInner(cm, measure)
	  for (var i = 0; i < 4 && startWidth != cm.display.barWidth || startHeight != cm.display.barHeight; i++) {
	    if (startWidth != cm.display.barWidth && cm.options.lineWrapping)
	      { updateHeightsInViewport(cm) }
	    updateScrollbarsInner(cm, measureForScrollbars(cm))
	    startWidth = cm.display.barWidth; startHeight = cm.display.barHeight
	  }
	}

	// Re-synchronize the fake scrollbars with the actual size of the
	// content.
	function updateScrollbarsInner(cm, measure) {
	  var d = cm.display
	  var sizes = d.scrollbars.update(measure)

	  d.sizer.style.paddingRight = (d.barWidth = sizes.right) + "px"
	  d.sizer.style.paddingBottom = (d.barHeight = sizes.bottom) + "px"
	  d.heightForcer.style.borderBottom = sizes.bottom + "px solid transparent"

	  if (sizes.right && sizes.bottom) {
	    d.scrollbarFiller.style.display = "block"
	    d.scrollbarFiller.style.height = sizes.bottom + "px"
	    d.scrollbarFiller.style.width = sizes.right + "px"
	  } else { d.scrollbarFiller.style.display = "" }
	  if (sizes.bottom && cm.options.coverGutterNextToScrollbar && cm.options.fixedGutter) {
	    d.gutterFiller.style.display = "block"
	    d.gutterFiller.style.height = sizes.bottom + "px"
	    d.gutterFiller.style.width = measure.gutterWidth + "px"
	  } else { d.gutterFiller.style.display = "" }
	}

	var scrollbarModel = {"native": NativeScrollbars, "null": NullScrollbars}

	function initScrollbars(cm) {
	  if (cm.display.scrollbars) {
	    cm.display.scrollbars.clear()
	    if (cm.display.scrollbars.addClass)
	      { rmClass(cm.display.wrapper, cm.display.scrollbars.addClass) }
	  }

	  cm.display.scrollbars = new scrollbarModel[cm.options.scrollbarStyle](function (node) {
	    cm.display.wrapper.insertBefore(node, cm.display.scrollbarFiller)
	    // Prevent clicks in the scrollbars from killing focus
	    on(node, "mousedown", function () {
	      if (cm.state.focused) { setTimeout(function () { return cm.display.input.focus(); }, 0) }
	    })
	    node.setAttribute("cm-not-content", "true")
	  }, function (pos, axis) {
	    if (axis == "horizontal") { setScrollLeft(cm, pos) }
	    else { updateScrollTop(cm, pos) }
	  }, cm)
	  if (cm.display.scrollbars.addClass)
	    { addClass(cm.display.wrapper, cm.display.scrollbars.addClass) }
	}

	// Operations are used to wrap a series of changes to the editor
	// state in such a way that each change won't have to update the
	// cursor and display (which would be awkward, slow, and
	// error-prone). Instead, display updates are batched and then all
	// combined and executed at once.

	var nextOpId = 0
	// Start a new operation.
	function startOperation(cm) {
	  cm.curOp = {
	    cm: cm,
	    viewChanged: false,      // Flag that indicates that lines might need to be redrawn
	    startHeight: cm.doc.height, // Used to detect need to update scrollbar
	    forceUpdate: false,      // Used to force a redraw
	    updateInput: null,       // Whether to reset the input textarea
	    typing: false,           // Whether this reset should be careful to leave existing text (for compositing)
	    changeObjs: null,        // Accumulated changes, for firing change events
	    cursorActivityHandlers: null, // Set of handlers to fire cursorActivity on
	    cursorActivityCalled: 0, // Tracks which cursorActivity handlers have been called already
	    selectionChanged: false, // Whether the selection needs to be redrawn
	    updateMaxLine: false,    // Set when the widest line needs to be determined anew
	    scrollLeft: null, scrollTop: null, // Intermediate scroll position, not pushed to DOM yet
	    scrollToPos: null,       // Used to scroll to a specific position
	    focus: false,
	    id: ++nextOpId           // Unique ID
	  }
	  pushOperation(cm.curOp)
	}

	// Finish an operation, updating the display and signalling delayed events
	function endOperation(cm) {
	  var op = cm.curOp
	  finishOperation(op, function (group) {
	    for (var i = 0; i < group.ops.length; i++)
	      { group.ops[i].cm.curOp = null }
	    endOperations(group)
	  })
	}

	// The DOM updates done when an operation finishes are batched so
	// that the minimum number of relayouts are required.
	function endOperations(group) {
	  var ops = group.ops
	  for (var i = 0; i < ops.length; i++) // Read DOM
	    { endOperation_R1(ops[i]) }
	  for (var i$1 = 0; i$1 < ops.length; i$1++) // Write DOM (maybe)
	    { endOperation_W1(ops[i$1]) }
	  for (var i$2 = 0; i$2 < ops.length; i$2++) // Read DOM
	    { endOperation_R2(ops[i$2]) }
	  for (var i$3 = 0; i$3 < ops.length; i$3++) // Write DOM (maybe)
	    { endOperation_W2(ops[i$3]) }
	  for (var i$4 = 0; i$4 < ops.length; i$4++) // Read DOM
	    { endOperation_finish(ops[i$4]) }
	}

	function endOperation_R1(op) {
	  var cm = op.cm, display = cm.display
	  maybeClipScrollbars(cm)
	  if (op.updateMaxLine) { findMaxLine(cm) }

	  op.mustUpdate = op.viewChanged || op.forceUpdate || op.scrollTop != null ||
	    op.scrollToPos && (op.scrollToPos.from.line < display.viewFrom ||
	                       op.scrollToPos.to.line >= display.viewTo) ||
	    display.maxLineChanged && cm.options.lineWrapping
	  op.update = op.mustUpdate &&
	    new DisplayUpdate(cm, op.mustUpdate && {top: op.scrollTop, ensure: op.scrollToPos}, op.forceUpdate)
	}

	function endOperation_W1(op) {
	  op.updatedDisplay = op.mustUpdate && updateDisplayIfNeeded(op.cm, op.update)
	}

	function endOperation_R2(op) {
	  var cm = op.cm, display = cm.display
	  if (op.updatedDisplay) { updateHeightsInViewport(cm) }

	  op.barMeasure = measureForScrollbars(cm)

	  // If the max line changed since it was last measured, measure it,
	  // and ensure the document's width matches it.
	  // updateDisplay_W2 will use these properties to do the actual resizing
	  if (display.maxLineChanged && !cm.options.lineWrapping) {
	    op.adjustWidthTo = measureChar(cm, display.maxLine, display.maxLine.text.length).left + 3
	    cm.display.sizerWidth = op.adjustWidthTo
	    op.barMeasure.scrollWidth =
	      Math.max(display.scroller.clientWidth, display.sizer.offsetLeft + op.adjustWidthTo + scrollGap(cm) + cm.display.barWidth)
	    op.maxScrollLeft = Math.max(0, display.sizer.offsetLeft + op.adjustWidthTo - displayWidth(cm))
	  }

	  if (op.updatedDisplay || op.selectionChanged)
	    { op.preparedSelection = display.input.prepareSelection() }
	}

	function endOperation_W2(op) {
	  var cm = op.cm

	  if (op.adjustWidthTo != null) {
	    cm.display.sizer.style.minWidth = op.adjustWidthTo + "px"
	    if (op.maxScrollLeft < cm.doc.scrollLeft)
	      { setScrollLeft(cm, Math.min(cm.display.scroller.scrollLeft, op.maxScrollLeft), true) }
	    cm.display.maxLineChanged = false
	  }

	  var takeFocus = op.focus && op.focus == activeElt()
	  if (op.preparedSelection)
	    { cm.display.input.showSelection(op.preparedSelection, takeFocus) }
	  if (op.updatedDisplay || op.startHeight != cm.doc.height)
	    { updateScrollbars(cm, op.barMeasure) }
	  if (op.updatedDisplay)
	    { setDocumentHeight(cm, op.barMeasure) }

	  if (op.selectionChanged) { restartBlink(cm) }

	  if (cm.state.focused && op.updateInput)
	    { cm.display.input.reset(op.typing) }
	  if (takeFocus) { ensureFocus(op.cm) }
	}

	function endOperation_finish(op) {
	  var cm = op.cm, display = cm.display, doc = cm.doc

	  if (op.updatedDisplay) { postUpdateDisplay(cm, op.update) }

	  // Abort mouse wheel delta measurement, when scrolling explicitly
	  if (display.wheelStartX != null && (op.scrollTop != null || op.scrollLeft != null || op.scrollToPos))
	    { display.wheelStartX = display.wheelStartY = null }

	  // Propagate the scroll position to the actual DOM scroller
	  if (op.scrollTop != null) { setScrollTop(cm, op.scrollTop, op.forceScroll) }

	  if (op.scrollLeft != null) { setScrollLeft(cm, op.scrollLeft, true, true) }
	  // If we need to scroll a specific position into view, do so.
	  if (op.scrollToPos) {
	    var rect = scrollPosIntoView(cm, clipPos(doc, op.scrollToPos.from),
	                                 clipPos(doc, op.scrollToPos.to), op.scrollToPos.margin)
	    maybeScrollWindow(cm, rect)
	  }

	  // Fire events for markers that are hidden/unidden by editing or
	  // undoing
	  var hidden = op.maybeHiddenMarkers, unhidden = op.maybeUnhiddenMarkers
	  if (hidden) { for (var i = 0; i < hidden.length; ++i)
	    { if (!hidden[i].lines.length) { signal(hidden[i], "hide") } } }
	  if (unhidden) { for (var i$1 = 0; i$1 < unhidden.length; ++i$1)
	    { if (unhidden[i$1].lines.length) { signal(unhidden[i$1], "unhide") } } }

	  if (display.wrapper.offsetHeight)
	    { doc.scrollTop = cm.display.scroller.scrollTop }

	  // Fire change events, and delayed event handlers
	  if (op.changeObjs)
	    { signal(cm, "changes", cm, op.changeObjs) }
	  if (op.update)
	    { op.update.finish() }
	}

	// Run the given function in an operation
	function runInOp(cm, f) {
	  if (cm.curOp) { return f() }
	  startOperation(cm)
	  try { return f() }
	  finally { endOperation(cm) }
	}
	// Wraps a function in an operation. Returns the wrapped function.
	function operation(cm, f) {
	  return function() {
	    if (cm.curOp) { return f.apply(cm, arguments) }
	    startOperation(cm)
	    try { return f.apply(cm, arguments) }
	    finally { endOperation(cm) }
	  }
	}
	// Used to add methods to editor and doc instances, wrapping them in
	// operations.
	function methodOp(f) {
	  return function() {
	    if (this.curOp) { return f.apply(this, arguments) }
	    startOperation(this)
	    try { return f.apply(this, arguments) }
	    finally { endOperation(this) }
	  }
	}
	function docMethodOp(f) {
	  return function() {
	    var cm = this.cm
	    if (!cm || cm.curOp) { return f.apply(this, arguments) }
	    startOperation(cm)
	    try { return f.apply(this, arguments) }
	    finally { endOperation(cm) }
	  }
	}

	// Updates the display.view data structure for a given change to the
	// document. From and to are in pre-change coordinates. Lendiff is
	// the amount of lines added or subtracted by the change. This is
	// used for changes that span multiple lines, or change the way
	// lines are divided into visual lines. regLineChange (below)
	// registers single-line changes.
	function regChange(cm, from, to, lendiff) {
	  if (from == null) { from = cm.doc.first }
	  if (to == null) { to = cm.doc.first + cm.doc.size }
	  if (!lendiff) { lendiff = 0 }

	  var display = cm.display
	  if (lendiff && to < display.viewTo &&
	      (display.updateLineNumbers == null || display.updateLineNumbers > from))
	    { display.updateLineNumbers = from }

	  cm.curOp.viewChanged = true

	  if (from >= display.viewTo) { // Change after
	    if (sawCollapsedSpans && visualLineNo(cm.doc, from) < display.viewTo)
	      { resetView(cm) }
	  } else if (to <= display.viewFrom) { // Change before
	    if (sawCollapsedSpans && visualLineEndNo(cm.doc, to + lendiff) > display.viewFrom) {
	      resetView(cm)
	    } else {
	      display.viewFrom += lendiff
	      display.viewTo += lendiff
	    }
	  } else if (from <= display.viewFrom && to >= display.viewTo) { // Full overlap
	    resetView(cm)
	  } else if (from <= display.viewFrom) { // Top overlap
	    var cut = viewCuttingPoint(cm, to, to + lendiff, 1)
	    if (cut) {
	      display.view = display.view.slice(cut.index)
	      display.viewFrom = cut.lineN
	      display.viewTo += lendiff
	    } else {
	      resetView(cm)
	    }
	  } else if (to >= display.viewTo) { // Bottom overlap
	    var cut$1 = viewCuttingPoint(cm, from, from, -1)
	    if (cut$1) {
	      display.view = display.view.slice(0, cut$1.index)
	      display.viewTo = cut$1.lineN
	    } else {
	      resetView(cm)
	    }
	  } else { // Gap in the middle
	    var cutTop = viewCuttingPoint(cm, from, from, -1)
	    var cutBot = viewCuttingPoint(cm, to, to + lendiff, 1)
	    if (cutTop && cutBot) {
	      display.view = display.view.slice(0, cutTop.index)
	        .concat(buildViewArray(cm, cutTop.lineN, cutBot.lineN))
	        .concat(display.view.slice(cutBot.index))
	      display.viewTo += lendiff
	    } else {
	      resetView(cm)
	    }
	  }

	  var ext = display.externalMeasured
	  if (ext) {
	    if (to < ext.lineN)
	      { ext.lineN += lendiff }
	    else if (from < ext.lineN + ext.size)
	      { display.externalMeasured = null }
	  }
	}

	// Register a change to a single line. Type must be one of "text",
	// "gutter", "class", "widget"
	function regLineChange(cm, line, type) {
	  cm.curOp.viewChanged = true
	  var display = cm.display, ext = cm.display.externalMeasured
	  if (ext && line >= ext.lineN && line < ext.lineN + ext.size)
	    { display.externalMeasured = null }

	  if (line < display.viewFrom || line >= display.viewTo) { return }
	  var lineView = display.view[findViewIndex(cm, line)]
	  if (lineView.node == null) { return }
	  var arr = lineView.changes || (lineView.changes = [])
	  if (indexOf(arr, type) == -1) { arr.push(type) }
	}

	// Clear the view.
	function resetView(cm) {
	  cm.display.viewFrom = cm.display.viewTo = cm.doc.first
	  cm.display.view = []
	  cm.display.viewOffset = 0
	}

	function viewCuttingPoint(cm, oldN, newN, dir) {
	  var index = findViewIndex(cm, oldN), diff, view = cm.display.view
	  if (!sawCollapsedSpans || newN == cm.doc.first + cm.doc.size)
	    { return {index: index, lineN: newN} }
	  var n = cm.display.viewFrom
	  for (var i = 0; i < index; i++)
	    { n += view[i].size }
	  if (n != oldN) {
	    if (dir > 0) {
	      if (index == view.length - 1) { return null }
	      diff = (n + view[index].size) - oldN
	      index++
	    } else {
	      diff = n - oldN
	    }
	    oldN += diff; newN += diff
	  }
	  while (visualLineNo(cm.doc, newN) != newN) {
	    if (index == (dir < 0 ? 0 : view.length - 1)) { return null }
	    newN += dir * view[index - (dir < 0 ? 1 : 0)].size
	    index += dir
	  }
	  return {index: index, lineN: newN}
	}

	// Force the view to cover a given range, adding empty view element
	// or clipping off existing ones as needed.
	function adjustView(cm, from, to) {
	  var display = cm.display, view = display.view
	  if (view.length == 0 || from >= display.viewTo || to <= display.viewFrom) {
	    display.view = buildViewArray(cm, from, to)
	    display.viewFrom = from
	  } else {
	    if (display.viewFrom > from)
	      { display.view = buildViewArray(cm, from, display.viewFrom).concat(display.view) }
	    else if (display.viewFrom < from)
	      { display.view = display.view.slice(findViewIndex(cm, from)) }
	    display.viewFrom = from
	    if (display.viewTo < to)
	      { display.view = display.view.concat(buildViewArray(cm, display.viewTo, to)) }
	    else if (display.viewTo > to)
	      { display.view = display.view.slice(0, findViewIndex(cm, to)) }
	  }
	  display.viewTo = to
	}

	// Count the number of lines in the view whose DOM representation is
	// out of date (or nonexistent).
	function countDirtyView(cm) {
	  var view = cm.display.view, dirty = 0
	  for (var i = 0; i < view.length; i++) {
	    var lineView = view[i]
	    if (!lineView.hidden && (!lineView.node || lineView.changes)) { ++dirty }
	  }
	  return dirty
	}

	// HIGHLIGHT WORKER

	function startWorker(cm, time) {
	  if (cm.doc.highlightFrontier < cm.display.viewTo)
	    { cm.state.highlight.set(time, bind(highlightWorker, cm)) }
	}

	function highlightWorker(cm) {
	  var doc = cm.doc
	  if (doc.highlightFrontier >= cm.display.viewTo) { return }
	  var end = +new Date + cm.options.workTime
	  var context = getContextBefore(cm, doc.highlightFrontier)
	  var changedLines = []

	  doc.iter(context.line, Math.min(doc.first + doc.size, cm.display.viewTo + 500), function (line) {
	    if (context.line >= cm.display.viewFrom) { // Visible
	      var oldStyles = line.styles
	      var resetState = line.text.length > cm.options.maxHighlightLength ? copyState(doc.mode, context.state) : null
	      var highlighted = highlightLine(cm, line, context, true)
	      if (resetState) { context.state = resetState }
	      line.styles = highlighted.styles
	      var oldCls = line.styleClasses, newCls = highlighted.classes
	      if (newCls) { line.styleClasses = newCls }
	      else if (oldCls) { line.styleClasses = null }
	      var ischange = !oldStyles || oldStyles.length != line.styles.length ||
	        oldCls != newCls && (!oldCls || !newCls || oldCls.bgClass != newCls.bgClass || oldCls.textClass != newCls.textClass)
	      for (var i = 0; !ischange && i < oldStyles.length; ++i) { ischange = oldStyles[i] != line.styles[i] }
	      if (ischange) { changedLines.push(context.line) }
	      line.stateAfter = context.save()
	      context.nextLine()
	    } else {
	      if (line.text.length <= cm.options.maxHighlightLength)
	        { processLine(cm, line.text, context) }
	      line.stateAfter = context.line % 5 == 0 ? context.save() : null
	      context.nextLine()
	    }
	    if (+new Date > end) {
	      startWorker(cm, cm.options.workDelay)
	      return true
	    }
	  })
	  doc.highlightFrontier = context.line
	  doc.modeFrontier = Math.max(doc.modeFrontier, context.line)
	  if (changedLines.length) { runInOp(cm, function () {
	    for (var i = 0; i < changedLines.length; i++)
	      { regLineChange(cm, changedLines[i], "text") }
	  }) }
	}

	// DISPLAY DRAWING

	var DisplayUpdate = function(cm, viewport, force) {
	  var display = cm.display

	  this.viewport = viewport
	  // Store some values that we'll need later (but don't want to force a relayout for)
	  this.visible = visibleLines(display, cm.doc, viewport)
	  this.editorIsHidden = !display.wrapper.offsetWidth
	  this.wrapperHeight = display.wrapper.clientHeight
	  this.wrapperWidth = display.wrapper.clientWidth
	  this.oldDisplayWidth = displayWidth(cm)
	  this.force = force
	  this.dims = getDimensions(cm)
	  this.events = []
	};

	DisplayUpdate.prototype.signal = function (emitter, type) {
	  if (hasHandler(emitter, type))
	    { this.events.push(arguments) }
	};
	DisplayUpdate.prototype.finish = function () {
	    var this$1 = this;

	  for (var i = 0; i < this.events.length; i++)
	    { signal.apply(null, this$1.events[i]) }
	};

	function maybeClipScrollbars(cm) {
	  var display = cm.display
	  if (!display.scrollbarsClipped && display.scroller.offsetWidth) {
	    display.nativeBarWidth = display.scroller.offsetWidth - display.scroller.clientWidth
	    display.heightForcer.style.height = scrollGap(cm) + "px"
	    display.sizer.style.marginBottom = -display.nativeBarWidth + "px"
	    display.sizer.style.borderRightWidth = scrollGap(cm) + "px"
	    display.scrollbarsClipped = true
	  }
	}

	function selectionSnapshot(cm) {
	  if (cm.hasFocus()) { return null }
	  var active = activeElt()
	  if (!active || !contains(cm.display.lineDiv, active)) { return null }
	  var result = {activeElt: active}
	  if (window.getSelection) {
	    var sel = window.getSelection()
	    if (sel.anchorNode && sel.extend && contains(cm.display.lineDiv, sel.anchorNode)) {
	      result.anchorNode = sel.anchorNode
	      result.anchorOffset = sel.anchorOffset
	      result.focusNode = sel.focusNode
	      result.focusOffset = sel.focusOffset
	    }
	  }
	  return result
	}

	function restoreSelection(snapshot) {
	  if (!snapshot || !snapshot.activeElt || snapshot.activeElt == activeElt()) { return }
	  snapshot.activeElt.focus()
	  if (snapshot.anchorNode && contains(document.body, snapshot.anchorNode) && contains(document.body, snapshot.focusNode)) {
	    var sel = window.getSelection(), range = document.createRange()
	    range.setEnd(snapshot.anchorNode, snapshot.anchorOffset)
	    range.collapse(false)
	    sel.removeAllRanges()
	    sel.addRange(range)
	    sel.extend(snapshot.focusNode, snapshot.focusOffset)
	  }
	}

	// Does the actual updating of the line display. Bails out
	// (returning false) when there is nothing to be done and forced is
	// false.
	function updateDisplayIfNeeded(cm, update) {
	  var display = cm.display, doc = cm.doc

	  if (update.editorIsHidden) {
	    resetView(cm)
	    return false
	  }

	  // Bail out if the visible area is already rendered and nothing changed.
	  if (!update.force &&
	      update.visible.from >= display.viewFrom && update.visible.to <= display.viewTo &&
	      (display.updateLineNumbers == null || display.updateLineNumbers >= display.viewTo) &&
	      display.renderedView == display.view && countDirtyView(cm) == 0)
	    { return false }

	  if (maybeUpdateLineNumberWidth(cm)) {
	    resetView(cm)
	    update.dims = getDimensions(cm)
	  }

	  // Compute a suitable new viewport (from & to)
	  var end = doc.first + doc.size
	  var from = Math.max(update.visible.from - cm.options.viewportMargin, doc.first)
	  var to = Math.min(end, update.visible.to + cm.options.viewportMargin)
	  if (display.viewFrom < from && from - display.viewFrom < 20) { from = Math.max(doc.first, display.viewFrom) }
	  if (display.viewTo > to && display.viewTo - to < 20) { to = Math.min(end, display.viewTo) }
	  if (sawCollapsedSpans) {
	    from = visualLineNo(cm.doc, from)
	    to = visualLineEndNo(cm.doc, to)
	  }

	  var different = from != display.viewFrom || to != display.viewTo ||
	    display.lastWrapHeight != update.wrapperHeight || display.lastWrapWidth != update.wrapperWidth
	  adjustView(cm, from, to)

	  display.viewOffset = heightAtLine(getLine(cm.doc, display.viewFrom))
	  // Position the mover div to align with the current scroll position
	  cm.display.mover.style.top = display.viewOffset + "px"

	  var toUpdate = countDirtyView(cm)
	  if (!different && toUpdate == 0 && !update.force && display.renderedView == display.view &&
	      (display.updateLineNumbers == null || display.updateLineNumbers >= display.viewTo))
	    { return false }

	  // For big changes, we hide the enclosing element during the
	  // update, since that speeds up the operations on most browsers.
	  var selSnapshot = selectionSnapshot(cm)
	  if (toUpdate > 4) { display.lineDiv.style.display = "none" }
	  patchDisplay(cm, display.updateLineNumbers, update.dims)
	  if (toUpdate > 4) { display.lineDiv.style.display = "" }
	  display.renderedView = display.view
	  // There might have been a widget with a focused element that got
	  // hidden or updated, if so re-focus it.
	  restoreSelection(selSnapshot)

	  // Prevent selection and cursors from interfering with the scroll
	  // width and height.
	  removeChildren(display.cursorDiv)
	  removeChildren(display.selectionDiv)
	  display.gutters.style.height = display.sizer.style.minHeight = 0

	  if (different) {
	    display.lastWrapHeight = update.wrapperHeight
	    display.lastWrapWidth = update.wrapperWidth
	    startWorker(cm, 400)
	  }

	  display.updateLineNumbers = null

	  return true
	}

	function postUpdateDisplay(cm, update) {
	  var viewport = update.viewport

	  for (var first = true;; first = false) {
	    if (!first || !cm.options.lineWrapping || update.oldDisplayWidth == displayWidth(cm)) {
	      // Clip forced viewport to actual scrollable area.
	      if (viewport && viewport.top != null)
	        { viewport = {top: Math.min(cm.doc.height + paddingVert(cm.display) - displayHeight(cm), viewport.top)} }
	      // Updated line heights might result in the drawn area not
	      // actually covering the viewport. Keep looping until it does.
	      update.visible = visibleLines(cm.display, cm.doc, viewport)
	      if (update.visible.from >= cm.display.viewFrom && update.visible.to <= cm.display.viewTo)
	        { break }
	    }
	    if (!updateDisplayIfNeeded(cm, update)) { break }
	    updateHeightsInViewport(cm)
	    var barMeasure = measureForScrollbars(cm)
	    updateSelection(cm)
	    updateScrollbars(cm, barMeasure)
	    setDocumentHeight(cm, barMeasure)
	    update.force = false
	  }

	  update.signal(cm, "update", cm)
	  if (cm.display.viewFrom != cm.display.reportedViewFrom || cm.display.viewTo != cm.display.reportedViewTo) {
	    update.signal(cm, "viewportChange", cm, cm.display.viewFrom, cm.display.viewTo)
	    cm.display.reportedViewFrom = cm.display.viewFrom; cm.display.reportedViewTo = cm.display.viewTo
	  }
	}

	function updateDisplaySimple(cm, viewport) {
	  var update = new DisplayUpdate(cm, viewport)
	  if (updateDisplayIfNeeded(cm, update)) {
	    updateHeightsInViewport(cm)
	    postUpdateDisplay(cm, update)
	    var barMeasure = measureForScrollbars(cm)
	    updateSelection(cm)
	    updateScrollbars(cm, barMeasure)
	    setDocumentHeight(cm, barMeasure)
	    update.finish()
	  }
	}

	// Sync the actual display DOM structure with display.view, removing
	// nodes for lines that are no longer in view, and creating the ones
	// that are not there yet, and updating the ones that are out of
	// date.
	function patchDisplay(cm, updateNumbersFrom, dims) {
	  var display = cm.display, lineNumbers = cm.options.lineNumbers
	  var container = display.lineDiv, cur = container.firstChild

	  function rm(node) {
	    var next = node.nextSibling
	    // Works around a throw-scroll bug in OS X Webkit
	    if (webkit && mac && cm.display.currentWheelTarget == node)
	      { node.style.display = "none" }
	    else
	      { node.parentNode.removeChild(node) }
	    return next
	  }

	  var view = display.view, lineN = display.viewFrom
	  // Loop over the elements in the view, syncing cur (the DOM nodes
	  // in display.lineDiv) with the view as we go.
	  for (var i = 0; i < view.length; i++) {
	    var lineView = view[i]
	    if (lineView.hidden) {
	    } else if (!lineView.node || lineView.node.parentNode != container) { // Not drawn yet
	      var node = buildLineElement(cm, lineView, lineN, dims)
	      container.insertBefore(node, cur)
	    } else { // Already drawn
	      while (cur != lineView.node) { cur = rm(cur) }
	      var updateNumber = lineNumbers && updateNumbersFrom != null &&
	        updateNumbersFrom <= lineN && lineView.lineNumber
	      if (lineView.changes) {
	        if (indexOf(lineView.changes, "gutter") > -1) { updateNumber = false }
	        updateLineForChanges(cm, lineView, lineN, dims)
	      }
	      if (updateNumber) {
	        removeChildren(lineView.lineNumber)
	        lineView.lineNumber.appendChild(document.createTextNode(lineNumberFor(cm.options, lineN)))
	      }
	      cur = lineView.node.nextSibling
	    }
	    lineN += lineView.size
	  }
	  while (cur) { cur = rm(cur) }
	}

	function updateGutterSpace(cm) {
	  var width = cm.display.gutters.offsetWidth
	  cm.display.sizer.style.marginLeft = width + "px"
	}

	function setDocumentHeight(cm, measure) {
	  cm.display.sizer.style.minHeight = measure.docHeight + "px"
	  cm.display.heightForcer.style.top = measure.docHeight + "px"
	  cm.display.gutters.style.height = (measure.docHeight + cm.display.barHeight + scrollGap(cm)) + "px"
	}

	// Rebuild the gutter elements, ensure the margin to the left of the
	// code matches their width.
	function updateGutters(cm) {
	  var gutters = cm.display.gutters, specs = cm.options.gutters
	  removeChildren(gutters)
	  var i = 0
	  for (; i < specs.length; ++i) {
	    var gutterClass = specs[i]
	    var gElt = gutters.appendChild(elt("div", null, "CodeMirror-gutter " + gutterClass))
	    if (gutterClass == "CodeMirror-linenumbers") {
	      cm.display.lineGutter = gElt
	      gElt.style.width = (cm.display.lineNumWidth || 1) + "px"
	    }
	  }
	  gutters.style.display = i ? "" : "none"
	  updateGutterSpace(cm)
	}

	// Make sure the gutters options contains the element
	// "CodeMirror-linenumbers" when the lineNumbers option is true.
	function setGuttersForLineNumbers(options) {
	  var found = indexOf(options.gutters, "CodeMirror-linenumbers")
	  if (found == -1 && options.lineNumbers) {
	    options.gutters = options.gutters.concat(["CodeMirror-linenumbers"])
	  } else if (found > -1 && !options.lineNumbers) {
	    options.gutters = options.gutters.slice(0)
	    options.gutters.splice(found, 1)
	  }
	}

	var wheelSamples = 0;
	var wheelPixelsPerUnit = null;
	// Fill in a browser-detected starting value on browsers where we
	// know one. These don't have to be accurate -- the result of them
	// being wrong would just be a slight flicker on the first wheel
	// scroll (if it is large enough).
	if (ie) { wheelPixelsPerUnit = -.53 }
	else if (gecko) { wheelPixelsPerUnit = 15 }
	else if (chrome) { wheelPixelsPerUnit = -.7 }
	else if (safari) { wheelPixelsPerUnit = -1/3 }

	function wheelEventDelta(e) {
	  var dx = e.wheelDeltaX, dy = e.wheelDeltaY
	  if (dx == null && e.detail && e.axis == e.HORIZONTAL_AXIS) { dx = e.detail }
	  if (dy == null && e.detail && e.axis == e.VERTICAL_AXIS) { dy = e.detail }
	  else if (dy == null) { dy = e.wheelDelta }
	  return {x: dx, y: dy}
	}
	function wheelEventPixels(e) {
	  var delta = wheelEventDelta(e)
	  delta.x *= wheelPixelsPerUnit
	  delta.y *= wheelPixelsPerUnit
	  return delta
	}

	function onScrollWheel(cm, e) {
	  var delta = wheelEventDelta(e), dx = delta.x, dy = delta.y

	  var display = cm.display, scroll = display.scroller
	  // Quit if there's nothing to scroll here
	  var canScrollX = scroll.scrollWidth > scroll.clientWidth
	  var canScrollY = scroll.scrollHeight > scroll.clientHeight
	  if (!(dx && canScrollX || dy && canScrollY)) { return }

	  // Webkit browsers on OS X abort momentum scrolls when the target
	  // of the scroll event is removed from the scrollable element.
	  // This hack (see related code in patchDisplay) makes sure the
	  // element is kept around.
	  if (dy && mac && webkit) {
	    outer: for (var cur = e.target, view = display.view; cur != scroll; cur = cur.parentNode) {
	      for (var i = 0; i < view.length; i++) {
	        if (view[i].node == cur) {
	          cm.display.currentWheelTarget = cur
	          break outer
	        }
	      }
	    }
	  }

	  // On some browsers, horizontal scrolling will cause redraws to
	  // happen before the gutter has been realigned, causing it to
	  // wriggle around in a most unseemly way. When we have an
	  // estimated pixels/delta value, we just handle horizontal
	  // scrolling entirely here. It'll be slightly off from native, but
	  // better than glitching out.
	  if (dx && !gecko && !presto && wheelPixelsPerUnit != null) {
	    if (dy && canScrollY)
	      { updateScrollTop(cm, Math.max(0, scroll.scrollTop + dy * wheelPixelsPerUnit)) }
	    setScrollLeft(cm, Math.max(0, scroll.scrollLeft + dx * wheelPixelsPerUnit))
	    // Only prevent default scrolling if vertical scrolling is
	    // actually possible. Otherwise, it causes vertical scroll
	    // jitter on OSX trackpads when deltaX is small and deltaY
	    // is large (issue #3579)
	    if (!dy || (dy && canScrollY))
	      { e_preventDefault(e) }
	    display.wheelStartX = null // Abort measurement, if in progress
	    return
	  }

	  // 'Project' the visible viewport to cover the area that is being
	  // scrolled into view (if we know enough to estimate it).
	  if (dy && wheelPixelsPerUnit != null) {
	    var pixels = dy * wheelPixelsPerUnit
	    var top = cm.doc.scrollTop, bot = top + display.wrapper.clientHeight
	    if (pixels < 0) { top = Math.max(0, top + pixels - 50) }
	    else { bot = Math.min(cm.doc.height, bot + pixels + 50) }
	    updateDisplaySimple(cm, {top: top, bottom: bot})
	  }

	  if (wheelSamples < 20) {
	    if (display.wheelStartX == null) {
	      display.wheelStartX = scroll.scrollLeft; display.wheelStartY = scroll.scrollTop
	      display.wheelDX = dx; display.wheelDY = dy
	      setTimeout(function () {
	        if (display.wheelStartX == null) { return }
	        var movedX = scroll.scrollLeft - display.wheelStartX
	        var movedY = scroll.scrollTop - display.wheelStartY
	        var sample = (movedY && display.wheelDY && movedY / display.wheelDY) ||
	          (movedX && display.wheelDX && movedX / display.wheelDX)
	        display.wheelStartX = display.wheelStartY = null
	        if (!sample) { return }
	        wheelPixelsPerUnit = (wheelPixelsPerUnit * wheelSamples + sample) / (wheelSamples + 1)
	        ++wheelSamples
	      }, 200)
	    } else {
	      display.wheelDX += dx; display.wheelDY += dy
	    }
	  }
	}

	// Selection objects are immutable. A new one is created every time
	// the selection changes. A selection is one or more non-overlapping
	// (and non-touching) ranges, sorted, and an integer that indicates
	// which one is the primary selection (the one that's scrolled into
	// view, that getCursor returns, etc).
	var Selection = function(ranges, primIndex) {
	  this.ranges = ranges
	  this.primIndex = primIndex
	};

	Selection.prototype.primary = function () { return this.ranges[this.primIndex] };

	Selection.prototype.equals = function (other) {
	    var this$1 = this;

	  if (other == this) { return true }
	  if (other.primIndex != this.primIndex || other.ranges.length != this.ranges.length) { return false }
	  for (var i = 0; i < this.ranges.length; i++) {
	    var here = this$1.ranges[i], there = other.ranges[i]
	    if (!equalCursorPos(here.anchor, there.anchor) || !equalCursorPos(here.head, there.head)) { return false }
	  }
	  return true
	};

	Selection.prototype.deepCopy = function () {
	    var this$1 = this;

	  var out = []
	  for (var i = 0; i < this.ranges.length; i++)
	    { out[i] = new Range(copyPos(this$1.ranges[i].anchor), copyPos(this$1.ranges[i].head)) }
	  return new Selection(out, this.primIndex)
	};

	Selection.prototype.somethingSelected = function () {
	    var this$1 = this;

	  for (var i = 0; i < this.ranges.length; i++)
	    { if (!this$1.ranges[i].empty()) { return true } }
	  return false
	};

	Selection.prototype.contains = function (pos, end) {
	    var this$1 = this;

	  if (!end) { end = pos }
	  for (var i = 0; i < this.ranges.length; i++) {
	    var range = this$1.ranges[i]
	    if (cmp(end, range.from()) >= 0 && cmp(pos, range.to()) <= 0)
	      { return i }
	  }
	  return -1
	};

	var Range = function(anchor, head) {
	  this.anchor = anchor; this.head = head
	};

	Range.prototype.from = function () { return minPos(this.anchor, this.head) };
	Range.prototype.to = function () { return maxPos(this.anchor, this.head) };
	Range.prototype.empty = function () { return this.head.line == this.anchor.line && this.head.ch == this.anchor.ch };

	// Take an unsorted, potentially overlapping set of ranges, and
	// build a selection out of it. 'Consumes' ranges array (modifying
	// it).
	function normalizeSelection(ranges, primIndex) {
	  var prim = ranges[primIndex]
	  ranges.sort(function (a, b) { return cmp(a.from(), b.from()); })
	  primIndex = indexOf(ranges, prim)
	  for (var i = 1; i < ranges.length; i++) {
	    var cur = ranges[i], prev = ranges[i - 1]
	    if (cmp(prev.to(), cur.from()) >= 0) {
	      var from = minPos(prev.from(), cur.from()), to = maxPos(prev.to(), cur.to())
	      var inv = prev.empty() ? cur.from() == cur.head : prev.from() == prev.head
	      if (i <= primIndex) { --primIndex }
	      ranges.splice(--i, 2, new Range(inv ? to : from, inv ? from : to))
	    }
	  }
	  return new Selection(ranges, primIndex)
	}

	function simpleSelection(anchor, head) {
	  return new Selection([new Range(anchor, head || anchor)], 0)
	}

	// Compute the position of the end of a change (its 'to' property
	// refers to the pre-change end).
	function changeEnd(change) {
	  if (!change.text) { return change.to }
	  return Pos(change.from.line + change.text.length - 1,
	             lst(change.text).length + (change.text.length == 1 ? change.from.ch : 0))
	}

	// Adjust a position to refer to the post-change position of the
	// same text, or the end of the change if the change covers it.
	function adjustForChange(pos, change) {
	  if (cmp(pos, change.from) < 0) { return pos }
	  if (cmp(pos, change.to) <= 0) { return changeEnd(change) }

	  var line = pos.line + change.text.length - (change.to.line - change.from.line) - 1, ch = pos.ch
	  if (pos.line == change.to.line) { ch += changeEnd(change).ch - change.to.ch }
	  return Pos(line, ch)
	}

	function computeSelAfterChange(doc, change) {
	  var out = []
	  for (var i = 0; i < doc.sel.ranges.length; i++) {
	    var range = doc.sel.ranges[i]
	    out.push(new Range(adjustForChange(range.anchor, change),
	                       adjustForChange(range.head, change)))
	  }
	  return normalizeSelection(out, doc.sel.primIndex)
	}

	function offsetPos(pos, old, nw) {
	  if (pos.line == old.line)
	    { return Pos(nw.line, pos.ch - old.ch + nw.ch) }
	  else
	    { return Pos(nw.line + (pos.line - old.line), pos.ch) }
	}

	// Used by replaceSelections to allow moving the selection to the
	// start or around the replaced test. Hint may be "start" or "around".
	function computeReplacedSel(doc, changes, hint) {
	  var out = []
	  var oldPrev = Pos(doc.first, 0), newPrev = oldPrev
	  for (var i = 0; i < changes.length; i++) {
	    var change = changes[i]
	    var from = offsetPos(change.from, oldPrev, newPrev)
	    var to = offsetPos(changeEnd(change), oldPrev, newPrev)
	    oldPrev = change.to
	    newPrev = to
	    if (hint == "around") {
	      var range = doc.sel.ranges[i], inv = cmp(range.head, range.anchor) < 0
	      out[i] = new Range(inv ? to : from, inv ? from : to)
	    } else {
	      out[i] = new Range(from, from)
	    }
	  }
	  return new Selection(out, doc.sel.primIndex)
	}

	// Used to get the editor into a consistent state again when options change.

	function loadMode(cm) {
	  cm.doc.mode = getMode(cm.options, cm.doc.modeOption)
	  resetModeState(cm)
	}

	function resetModeState(cm) {
	  cm.doc.iter(function (line) {
	    if (line.stateAfter) { line.stateAfter = null }
	    if (line.styles) { line.styles = null }
	  })
	  cm.doc.modeFrontier = cm.doc.highlightFrontier = cm.doc.first
	  startWorker(cm, 100)
	  cm.state.modeGen++
	  if (cm.curOp) { regChange(cm) }
	}

	// DOCUMENT DATA STRUCTURE

	// By default, updates that start and end at the beginning of a line
	// are treated specially, in order to make the association of line
	// widgets and marker elements with the text behave more intuitive.
	function isWholeLineUpdate(doc, change) {
	  return change.from.ch == 0 && change.to.ch == 0 && lst(change.text) == "" &&
	    (!doc.cm || doc.cm.options.wholeLineUpdateBefore)
	}

	// Perform a change on the document data structure.
	function updateDoc(doc, change, markedSpans, estimateHeight) {
	  function spansFor(n) {return markedSpans ? markedSpans[n] : null}
	  function update(line, text, spans) {
	    updateLine(line, text, spans, estimateHeight)
	    signalLater(line, "change", line, change)
	  }
	  function linesFor(start, end) {
	    var result = []
	    for (var i = start; i < end; ++i)
	      { result.push(new Line(text[i], spansFor(i), estimateHeight)) }
	    return result
	  }

	  var from = change.from, to = change.to, text = change.text
	  var firstLine = getLine(doc, from.line), lastLine = getLine(doc, to.line)
	  var lastText = lst(text), lastSpans = spansFor(text.length - 1), nlines = to.line - from.line

	  // Adjust the line structure
	  if (change.full) {
	    doc.insert(0, linesFor(0, text.length))
	    doc.remove(text.length, doc.size - text.length)
	  } else if (isWholeLineUpdate(doc, change)) {
	    // This is a whole-line replace. Treated specially to make
	    // sure line objects move the way they are supposed to.
	    var added = linesFor(0, text.length - 1)
	    update(lastLine, lastLine.text, lastSpans)
	    if (nlines) { doc.remove(from.line, nlines) }
	    if (added.length) { doc.insert(from.line, added) }
	  } else if (firstLine == lastLine) {
	    if (text.length == 1) {
	      update(firstLine, firstLine.text.slice(0, from.ch) + lastText + firstLine.text.slice(to.ch), lastSpans)
	    } else {
	      var added$1 = linesFor(1, text.length - 1)
	      added$1.push(new Line(lastText + firstLine.text.slice(to.ch), lastSpans, estimateHeight))
	      update(firstLine, firstLine.text.slice(0, from.ch) + text[0], spansFor(0))
	      doc.insert(from.line + 1, added$1)
	    }
	  } else if (text.length == 1) {
	    update(firstLine, firstLine.text.slice(0, from.ch) + text[0] + lastLine.text.slice(to.ch), spansFor(0))
	    doc.remove(from.line + 1, nlines)
	  } else {
	    update(firstLine, firstLine.text.slice(0, from.ch) + text[0], spansFor(0))
	    update(lastLine, lastText + lastLine.text.slice(to.ch), lastSpans)
	    var added$2 = linesFor(1, text.length - 1)
	    if (nlines > 1) { doc.remove(from.line + 1, nlines - 1) }
	    doc.insert(from.line + 1, added$2)
	  }

	  signalLater(doc, "change", doc, change)
	}

	// Call f for all linked documents.
	function linkedDocs(doc, f, sharedHistOnly) {
	  function propagate(doc, skip, sharedHist) {
	    if (doc.linked) { for (var i = 0; i < doc.linked.length; ++i) {
	      var rel = doc.linked[i]
	      if (rel.doc == skip) { continue }
	      var shared = sharedHist && rel.sharedHist
	      if (sharedHistOnly && !shared) { continue }
	      f(rel.doc, shared)
	      propagate(rel.doc, doc, shared)
	    } }
	  }
	  propagate(doc, null, true)
	}

	// Attach a document to an editor.
	function attachDoc(cm, doc) {
	  if (doc.cm) { throw new Error("This document is already in use.") }
	  cm.doc = doc
	  doc.cm = cm
	  estimateLineHeights(cm)
	  loadMode(cm)
	  setDirectionClass(cm)
	  if (!cm.options.lineWrapping) { findMaxLine(cm) }
	  cm.options.mode = doc.modeOption
	  regChange(cm)
	}

	function setDirectionClass(cm) {
	  ;(cm.doc.direction == "rtl" ? addClass : rmClass)(cm.display.lineDiv, "CodeMirror-rtl")
	}

	function directionChanged(cm) {
	  runInOp(cm, function () {
	    setDirectionClass(cm)
	    regChange(cm)
	  })
	}

	function History(startGen) {
	  // Arrays of change events and selections. Doing something adds an
	  // event to done and clears undo. Undoing moves events from done
	  // to undone, redoing moves them in the other direction.
	  this.done = []; this.undone = []
	  this.undoDepth = Infinity
	  // Used to track when changes can be merged into a single undo
	  // event
	  this.lastModTime = this.lastSelTime = 0
	  this.lastOp = this.lastSelOp = null
	  this.lastOrigin = this.lastSelOrigin = null
	  // Used by the isClean() method
	  this.generation = this.maxGeneration = startGen || 1
	}

	// Create a history change event from an updateDoc-style change
	// object.
	function historyChangeFromChange(doc, change) {
	  var histChange = {from: copyPos(change.from), to: changeEnd(change), text: getBetween(doc, change.from, change.to)}
	  attachLocalSpans(doc, histChange, change.from.line, change.to.line + 1)
	  linkedDocs(doc, function (doc) { return attachLocalSpans(doc, histChange, change.from.line, change.to.line + 1); }, true)
	  return histChange
	}

	// Pop all selection events off the end of a history array. Stop at
	// a change event.
	function clearSelectionEvents(array) {
	  while (array.length) {
	    var last = lst(array)
	    if (last.ranges) { array.pop() }
	    else { break }
	  }
	}

	// Find the top change event in the history. Pop off selection
	// events that are in the way.
	function lastChangeEvent(hist, force) {
	  if (force) {
	    clearSelectionEvents(hist.done)
	    return lst(hist.done)
	  } else if (hist.done.length && !lst(hist.done).ranges) {
	    return lst(hist.done)
	  } else if (hist.done.length > 1 && !hist.done[hist.done.length - 2].ranges) {
	    hist.done.pop()
	    return lst(hist.done)
	  }
	}

	// Register a change in the history. Merges changes that are within
	// a single operation, or are close together with an origin that
	// allows merging (starting with "+") into a single event.
	function addChangeToHistory(doc, change, selAfter, opId) {
	  var hist = doc.history
	  hist.undone.length = 0
	  var time = +new Date, cur
	  var last

	  if ((hist.lastOp == opId ||
	       hist.lastOrigin == change.origin && change.origin &&
	       ((change.origin.charAt(0) == "+" && doc.cm && hist.lastModTime > time - doc.cm.options.historyEventDelay) ||
	        change.origin.charAt(0) == "*")) &&
	      (cur = lastChangeEvent(hist, hist.lastOp == opId))) {
	    // Merge this change into the last event
	    last = lst(cur.changes)
	    if (cmp(change.from, change.to) == 0 && cmp(change.from, last.to) == 0) {
	      // Optimized case for simple insertion -- don't want to add
	      // new changesets for every character typed
	      last.to = changeEnd(change)
	    } else {
	      // Add new sub-event
	      cur.changes.push(historyChangeFromChange(doc, change))
	    }
	  } else {
	    // Can not be merged, start a new event.
	    var before = lst(hist.done)
	    if (!before || !before.ranges)
	      { pushSelectionToHistory(doc.sel, hist.done) }
	    cur = {changes: [historyChangeFromChange(doc, change)],
	           generation: hist.generation}
	    hist.done.push(cur)
	    while (hist.done.length > hist.undoDepth) {
	      hist.done.shift()
	      if (!hist.done[0].ranges) { hist.done.shift() }
	    }
	  }
	  hist.done.push(selAfter)
	  hist.generation = ++hist.maxGeneration
	  hist.lastModTime = hist.lastSelTime = time
	  hist.lastOp = hist.lastSelOp = opId
	  hist.lastOrigin = hist.lastSelOrigin = change.origin

	  if (!last) { signal(doc, "historyAdded") }
	}

	function selectionEventCanBeMerged(doc, origin, prev, sel) {
	  var ch = origin.charAt(0)
	  return ch == "*" ||
	    ch == "+" &&
	    prev.ranges.length == sel.ranges.length &&
	    prev.somethingSelected() == sel.somethingSelected() &&
	    new Date - doc.history.lastSelTime <= (doc.cm ? doc.cm.options.historyEventDelay : 500)
	}

	// Called whenever the selection changes, sets the new selection as
	// the pending selection in the history, and pushes the old pending
	// selection into the 'done' array when it was significantly
	// different (in number of selected ranges, emptiness, or time).
	function addSelectionToHistory(doc, sel, opId, options) {
	  var hist = doc.history, origin = options && options.origin

	  // A new event is started when the previous origin does not match
	  // the current, or the origins don't allow matching. Origins
	  // starting with * are always merged, those starting with + are
	  // merged when similar and close together in time.
	  if (opId == hist.lastSelOp ||
	      (origin && hist.lastSelOrigin == origin &&
	       (hist.lastModTime == hist.lastSelTime && hist.lastOrigin == origin ||
	        selectionEventCanBeMerged(doc, origin, lst(hist.done), sel))))
	    { hist.done[hist.done.length - 1] = sel }
	  else
	    { pushSelectionToHistory(sel, hist.done) }

	  hist.lastSelTime = +new Date
	  hist.lastSelOrigin = origin
	  hist.lastSelOp = opId
	  if (options && options.clearRedo !== false)
	    { clearSelectionEvents(hist.undone) }
	}

	function pushSelectionToHistory(sel, dest) {
	  var top = lst(dest)
	  if (!(top && top.ranges && top.equals(sel)))
	    { dest.push(sel) }
	}

	// Used to store marked span information in the history.
	function attachLocalSpans(doc, change, from, to) {
	  var existing = change["spans_" + doc.id], n = 0
	  doc.iter(Math.max(doc.first, from), Math.min(doc.first + doc.size, to), function (line) {
	    if (line.markedSpans)
	      { (existing || (existing = change["spans_" + doc.id] = {}))[n] = line.markedSpans }
	    ++n
	  })
	}

	// When un/re-doing restores text containing marked spans, those
	// that have been explicitly cleared should not be restored.
	function removeClearedSpans(spans) {
	  if (!spans) { return null }
	  var out
	  for (var i = 0; i < spans.length; ++i) {
	    if (spans[i].marker.explicitlyCleared) { if (!out) { out = spans.slice(0, i) } }
	    else if (out) { out.push(spans[i]) }
	  }
	  return !out ? spans : out.length ? out : null
	}

	// Retrieve and filter the old marked spans stored in a change event.
	function getOldSpans(doc, change) {
	  var found = change["spans_" + doc.id]
	  if (!found) { return null }
	  var nw = []
	  for (var i = 0; i < change.text.length; ++i)
	    { nw.push(removeClearedSpans(found[i])) }
	  return nw
	}

	// Used for un/re-doing changes from the history. Combines the
	// result of computing the existing spans with the set of spans that
	// existed in the history (so that deleting around a span and then
	// undoing brings back the span).
	function mergeOldSpans(doc, change) {
	  var old = getOldSpans(doc, change)
	  var stretched = stretchSpansOverChange(doc, change)
	  if (!old) { return stretched }
	  if (!stretched) { return old }

	  for (var i = 0; i < old.length; ++i) {
	    var oldCur = old[i], stretchCur = stretched[i]
	    if (oldCur && stretchCur) {
	      spans: for (var j = 0; j < stretchCur.length; ++j) {
	        var span = stretchCur[j]
	        for (var k = 0; k < oldCur.length; ++k)
	          { if (oldCur[k].marker == span.marker) { continue spans } }
	        oldCur.push(span)
	      }
	    } else if (stretchCur) {
	      old[i] = stretchCur
	    }
	  }
	  return old
	}

	// Used both to provide a JSON-safe object in .getHistory, and, when
	// detaching a document, to split the history in two
	function copyHistoryArray(events, newGroup, instantiateSel) {
	  var copy = []
	  for (var i = 0; i < events.length; ++i) {
	    var event = events[i]
	    if (event.ranges) {
	      copy.push(instantiateSel ? Selection.prototype.deepCopy.call(event) : event)
	      continue
	    }
	    var changes = event.changes, newChanges = []
	    copy.push({changes: newChanges})
	    for (var j = 0; j < changes.length; ++j) {
	      var change = changes[j], m = (void 0)
	      newChanges.push({from: change.from, to: change.to, text: change.text})
	      if (newGroup) { for (var prop in change) { if (m = prop.match(/^spans_(\d+)$/)) {
	        if (indexOf(newGroup, Number(m[1])) > -1) {
	          lst(newChanges)[prop] = change[prop]
	          delete change[prop]
	        }
	      } } }
	    }
	  }
	  return copy
	}

	// The 'scroll' parameter given to many of these indicated whether
	// the new cursor position should be scrolled into view after
	// modifying the selection.

	// If shift is held or the extend flag is set, extends a range to
	// include a given position (and optionally a second position).
	// Otherwise, simply returns the range between the given positions.
	// Used for cursor motion and such.
	function extendRange(range, head, other, extend) {
	  if (extend) {
	    var anchor = range.anchor
	    if (other) {
	      var posBefore = cmp(head, anchor) < 0
	      if (posBefore != (cmp(other, anchor) < 0)) {
	        anchor = head
	        head = other
	      } else if (posBefore != (cmp(head, other) < 0)) {
	        head = other
	      }
	    }
	    return new Range(anchor, head)
	  } else {
	    return new Range(other || head, head)
	  }
	}

	// Extend the primary selection range, discard the rest.
	function extendSelection(doc, head, other, options, extend) {
	  if (extend == null) { extend = doc.cm && (doc.cm.display.shift || doc.extend) }
	  setSelection(doc, new Selection([extendRange(doc.sel.primary(), head, other, extend)], 0), options)
	}

	// Extend all selections (pos is an array of selections with length
	// equal the number of selections)
	function extendSelections(doc, heads, options) {
	  var out = []
	  var extend = doc.cm && (doc.cm.display.shift || doc.extend)
	  for (var i = 0; i < doc.sel.ranges.length; i++)
	    { out[i] = extendRange(doc.sel.ranges[i], heads[i], null, extend) }
	  var newSel = normalizeSelection(out, doc.sel.primIndex)
	  setSelection(doc, newSel, options)
	}

	// Updates a single range in the selection.
	function replaceOneSelection(doc, i, range, options) {
	  var ranges = doc.sel.ranges.slice(0)
	  ranges[i] = range
	  setSelection(doc, normalizeSelection(ranges, doc.sel.primIndex), options)
	}

	// Reset the selection to a single range.
	function setSimpleSelection(doc, anchor, head, options) {
	  setSelection(doc, simpleSelection(anchor, head), options)
	}

	// Give beforeSelectionChange handlers a change to influence a
	// selection update.
	function filterSelectionChange(doc, sel, options) {
	  var obj = {
	    ranges: sel.ranges,
	    update: function(ranges) {
	      var this$1 = this;

	      this.ranges = []
	      for (var i = 0; i < ranges.length; i++)
	        { this$1.ranges[i] = new Range(clipPos(doc, ranges[i].anchor),
	                                   clipPos(doc, ranges[i].head)) }
	    },
	    origin: options && options.origin
	  }
	  signal(doc, "beforeSelectionChange", doc, obj)
	  if (doc.cm) { signal(doc.cm, "beforeSelectionChange", doc.cm, obj) }
	  if (obj.ranges != sel.ranges) { return normalizeSelection(obj.ranges, obj.ranges.length - 1) }
	  else { return sel }
	}

	function setSelectionReplaceHistory(doc, sel, options) {
	  var done = doc.history.done, last = lst(done)
	  if (last && last.ranges) {
	    done[done.length - 1] = sel
	    setSelectionNoUndo(doc, sel, options)
	  } else {
	    setSelection(doc, sel, options)
	  }
	}

	// Set a new selection.
	function setSelection(doc, sel, options) {
	  setSelectionNoUndo(doc, sel, options)
	  addSelectionToHistory(doc, doc.sel, doc.cm ? doc.cm.curOp.id : NaN, options)
	}

	function setSelectionNoUndo(doc, sel, options) {
	  if (hasHandler(doc, "beforeSelectionChange") || doc.cm && hasHandler(doc.cm, "beforeSelectionChange"))
	    { sel = filterSelectionChange(doc, sel, options) }

	  var bias = options && options.bias ||
	    (cmp(sel.primary().head, doc.sel.primary().head) < 0 ? -1 : 1)
	  setSelectionInner(doc, skipAtomicInSelection(doc, sel, bias, true))

	  if (!(options && options.scroll === false) && doc.cm)
	    { ensureCursorVisible(doc.cm) }
	}

	function setSelectionInner(doc, sel) {
	  if (sel.equals(doc.sel)) { return }

	  doc.sel = sel

	  if (doc.cm) {
	    doc.cm.curOp.updateInput = doc.cm.curOp.selectionChanged = true
	    signalCursorActivity(doc.cm)
	  }
	  signalLater(doc, "cursorActivity", doc)
	}

	// Verify that the selection does not partially select any atomic
	// marked ranges.
	function reCheckSelection(doc) {
	  setSelectionInner(doc, skipAtomicInSelection(doc, doc.sel, null, false))
	}

	// Return a selection that does not partially select any atomic
	// ranges.
	function skipAtomicInSelection(doc, sel, bias, mayClear) {
	  var out
	  for (var i = 0; i < sel.ranges.length; i++) {
	    var range = sel.ranges[i]
	    var old = sel.ranges.length == doc.sel.ranges.length && doc.sel.ranges[i]
	    var newAnchor = skipAtomic(doc, range.anchor, old && old.anchor, bias, mayClear)
	    var newHead = skipAtomic(doc, range.head, old && old.head, bias, mayClear)
	    if (out || newAnchor != range.anchor || newHead != range.head) {
	      if (!out) { out = sel.ranges.slice(0, i) }
	      out[i] = new Range(newAnchor, newHead)
	    }
	  }
	  return out ? normalizeSelection(out, sel.primIndex) : sel
	}

	function skipAtomicInner(doc, pos, oldPos, dir, mayClear) {
	  var line = getLine(doc, pos.line)
	  if (line.markedSpans) { for (var i = 0; i < line.markedSpans.length; ++i) {
	    var sp = line.markedSpans[i], m = sp.marker
	    if ((sp.from == null || (m.inclusiveLeft ? sp.from <= pos.ch : sp.from < pos.ch)) &&
	        (sp.to == null || (m.inclusiveRight ? sp.to >= pos.ch : sp.to > pos.ch))) {
	      if (mayClear) {
	        signal(m, "beforeCursorEnter")
	        if (m.explicitlyCleared) {
	          if (!line.markedSpans) { break }
	          else {--i; continue}
	        }
	      }
	      if (!m.atomic) { continue }

	      if (oldPos) {
	        var near = m.find(dir < 0 ? 1 : -1), diff = (void 0)
	        if (dir < 0 ? m.inclusiveRight : m.inclusiveLeft)
	          { near = movePos(doc, near, -dir, near && near.line == pos.line ? line : null) }
	        if (near && near.line == pos.line && (diff = cmp(near, oldPos)) && (dir < 0 ? diff < 0 : diff > 0))
	          { return skipAtomicInner(doc, near, pos, dir, mayClear) }
	      }

	      var far = m.find(dir < 0 ? -1 : 1)
	      if (dir < 0 ? m.inclusiveLeft : m.inclusiveRight)
	        { far = movePos(doc, far, dir, far.line == pos.line ? line : null) }
	      return far ? skipAtomicInner(doc, far, pos, dir, mayClear) : null
	    }
	  } }
	  return pos
	}

	// Ensure a given position is not inside an atomic range.
	function skipAtomic(doc, pos, oldPos, bias, mayClear) {
	  var dir = bias || 1
	  var found = skipAtomicInner(doc, pos, oldPos, dir, mayClear) ||
	      (!mayClear && skipAtomicInner(doc, pos, oldPos, dir, true)) ||
	      skipAtomicInner(doc, pos, oldPos, -dir, mayClear) ||
	      (!mayClear && skipAtomicInner(doc, pos, oldPos, -dir, true))
	  if (!found) {
	    doc.cantEdit = true
	    return Pos(doc.first, 0)
	  }
	  return found
	}

	function movePos(doc, pos, dir, line) {
	  if (dir < 0 && pos.ch == 0) {
	    if (pos.line > doc.first) { return clipPos(doc, Pos(pos.line - 1)) }
	    else { return null }
	  } else if (dir > 0 && pos.ch == (line || getLine(doc, pos.line)).text.length) {
	    if (pos.line < doc.first + doc.size - 1) { return Pos(pos.line + 1, 0) }
	    else { return null }
	  } else {
	    return new Pos(pos.line, pos.ch + dir)
	  }
	}

	function selectAll(cm) {
	  cm.setSelection(Pos(cm.firstLine(), 0), Pos(cm.lastLine()), sel_dontScroll)
	}

	// UPDATING

	// Allow "beforeChange" event handlers to influence a change
	function filterChange(doc, change, update) {
	  var obj = {
	    canceled: false,
	    from: change.from,
	    to: change.to,
	    text: change.text,
	    origin: change.origin,
	    cancel: function () { return obj.canceled = true; }
	  }
	  if (update) { obj.update = function (from, to, text, origin) {
	    if (from) { obj.from = clipPos(doc, from) }
	    if (to) { obj.to = clipPos(doc, to) }
	    if (text) { obj.text = text }
	    if (origin !== undefined) { obj.origin = origin }
	  } }
	  signal(doc, "beforeChange", doc, obj)
	  if (doc.cm) { signal(doc.cm, "beforeChange", doc.cm, obj) }

	  if (obj.canceled) { return null }
	  return {from: obj.from, to: obj.to, text: obj.text, origin: obj.origin}
	}

	// Apply a change to a document, and add it to the document's
	// history, and propagating it to all linked documents.
	function makeChange(doc, change, ignoreReadOnly) {
	  if (doc.cm) {
	    if (!doc.cm.curOp) { return operation(doc.cm, makeChange)(doc, change, ignoreReadOnly) }
	    if (doc.cm.state.suppressEdits) { return }
	  }

	  if (hasHandler(doc, "beforeChange") || doc.cm && hasHandler(doc.cm, "beforeChange")) {
	    change = filterChange(doc, change, true)
	    if (!change) { return }
	  }

	  // Possibly split or suppress the update based on the presence
	  // of read-only spans in its range.
	  var split = sawReadOnlySpans && !ignoreReadOnly && removeReadOnlyRanges(doc, change.from, change.to)
	  if (split) {
	    for (var i = split.length - 1; i >= 0; --i)
	      { makeChangeInner(doc, {from: split[i].from, to: split[i].to, text: i ? [""] : change.text, origin: change.origin}) }
	  } else {
	    makeChangeInner(doc, change)
	  }
	}

	function makeChangeInner(doc, change) {
	  if (change.text.length == 1 && change.text[0] == "" && cmp(change.from, change.to) == 0) { return }
	  var selAfter = computeSelAfterChange(doc, change)
	  addChangeToHistory(doc, change, selAfter, doc.cm ? doc.cm.curOp.id : NaN)

	  makeChangeSingleDoc(doc, change, selAfter, stretchSpansOverChange(doc, change))
	  var rebased = []

	  linkedDocs(doc, function (doc, sharedHist) {
	    if (!sharedHist && indexOf(rebased, doc.history) == -1) {
	      rebaseHist(doc.history, change)
	      rebased.push(doc.history)
	    }
	    makeChangeSingleDoc(doc, change, null, stretchSpansOverChange(doc, change))
	  })
	}

	// Revert a change stored in a document's history.
	function makeChangeFromHistory(doc, type, allowSelectionOnly) {
	  if (doc.cm && doc.cm.state.suppressEdits && !allowSelectionOnly) { return }

	  var hist = doc.history, event, selAfter = doc.sel
	  var source = type == "undo" ? hist.done : hist.undone, dest = type == "undo" ? hist.undone : hist.done

	  // Verify that there is a useable event (so that ctrl-z won't
	  // needlessly clear selection events)
	  var i = 0
	  for (; i < source.length; i++) {
	    event = source[i]
	    if (allowSelectionOnly ? event.ranges && !event.equals(doc.sel) : !event.ranges)
	      { break }
	  }
	  if (i == source.length) { return }
	  hist.lastOrigin = hist.lastSelOrigin = null

	  for (;;) {
	    event = source.pop()
	    if (event.ranges) {
	      pushSelectionToHistory(event, dest)
	      if (allowSelectionOnly && !event.equals(doc.sel)) {
	        setSelection(doc, event, {clearRedo: false})
	        return
	      }
	      selAfter = event
	    }
	    else { break }
	  }

	  // Build up a reverse change object to add to the opposite history
	  // stack (redo when undoing, and vice versa).
	  var antiChanges = []
	  pushSelectionToHistory(selAfter, dest)
	  dest.push({changes: antiChanges, generation: hist.generation})
	  hist.generation = event.generation || ++hist.maxGeneration

	  var filter = hasHandler(doc, "beforeChange") || doc.cm && hasHandler(doc.cm, "beforeChange")

	  var loop = function ( i ) {
	    var change = event.changes[i]
	    change.origin = type
	    if (filter && !filterChange(doc, change, false)) {
	      source.length = 0
	      return {}
	    }

	    antiChanges.push(historyChangeFromChange(doc, change))

	    var after = i ? computeSelAfterChange(doc, change) : lst(source)
	    makeChangeSingleDoc(doc, change, after, mergeOldSpans(doc, change))
	    if (!i && doc.cm) { doc.cm.scrollIntoView({from: change.from, to: changeEnd(change)}) }
	    var rebased = []

	    // Propagate to the linked documents
	    linkedDocs(doc, function (doc, sharedHist) {
	      if (!sharedHist && indexOf(rebased, doc.history) == -1) {
	        rebaseHist(doc.history, change)
	        rebased.push(doc.history)
	      }
	      makeChangeSingleDoc(doc, change, null, mergeOldSpans(doc, change))
	    })
	  };

	  for (var i$1 = event.changes.length - 1; i$1 >= 0; --i$1) {
	    var returned = loop( i$1 );

	    if ( returned ) return returned.v;
	  }
	}

	// Sub-views need their line numbers shifted when text is added
	// above or below them in the parent document.
	function shiftDoc(doc, distance) {
	  if (distance == 0) { return }
	  doc.first += distance
	  doc.sel = new Selection(map(doc.sel.ranges, function (range) { return new Range(
	    Pos(range.anchor.line + distance, range.anchor.ch),
	    Pos(range.head.line + distance, range.head.ch)
	  ); }), doc.sel.primIndex)
	  if (doc.cm) {
	    regChange(doc.cm, doc.first, doc.first - distance, distance)
	    for (var d = doc.cm.display, l = d.viewFrom; l < d.viewTo; l++)
	      { regLineChange(doc.cm, l, "gutter") }
	  }
	}

	// More lower-level change function, handling only a single document
	// (not linked ones).
	function makeChangeSingleDoc(doc, change, selAfter, spans) {
	  if (doc.cm && !doc.cm.curOp)
	    { return operation(doc.cm, makeChangeSingleDoc)(doc, change, selAfter, spans) }

	  if (change.to.line < doc.first) {
	    shiftDoc(doc, change.text.length - 1 - (change.to.line - change.from.line))
	    return
	  }
	  if (change.from.line > doc.lastLine()) { return }

	  // Clip the change to the size of this doc
	  if (change.from.line < doc.first) {
	    var shift = change.text.length - 1 - (doc.first - change.from.line)
	    shiftDoc(doc, shift)
	    change = {from: Pos(doc.first, 0), to: Pos(change.to.line + shift, change.to.ch),
	              text: [lst(change.text)], origin: change.origin}
	  }
	  var last = doc.lastLine()
	  if (change.to.line > last) {
	    change = {from: change.from, to: Pos(last, getLine(doc, last).text.length),
	              text: [change.text[0]], origin: change.origin}
	  }

	  change.removed = getBetween(doc, change.from, change.to)

	  if (!selAfter) { selAfter = computeSelAfterChange(doc, change) }
	  if (doc.cm) { makeChangeSingleDocInEditor(doc.cm, change, spans) }
	  else { updateDoc(doc, change, spans) }
	  setSelectionNoUndo(doc, selAfter, sel_dontScroll)
	}

	// Handle the interaction of a change to a document with the editor
	// that this document is part of.
	function makeChangeSingleDocInEditor(cm, change, spans) {
	  var doc = cm.doc, display = cm.display, from = change.from, to = change.to

	  var recomputeMaxLength = false, checkWidthStart = from.line
	  if (!cm.options.lineWrapping) {
	    checkWidthStart = lineNo(visualLine(getLine(doc, from.line)))
	    doc.iter(checkWidthStart, to.line + 1, function (line) {
	      if (line == display.maxLine) {
	        recomputeMaxLength = true
	        return true
	      }
	    })
	  }

	  if (doc.sel.contains(change.from, change.to) > -1)
	    { signalCursorActivity(cm) }

	  updateDoc(doc, change, spans, estimateHeight(cm))

	  if (!cm.options.lineWrapping) {
	    doc.iter(checkWidthStart, from.line + change.text.length, function (line) {
	      var len = lineLength(line)
	      if (len > display.maxLineLength) {
	        display.maxLine = line
	        display.maxLineLength = len
	        display.maxLineChanged = true
	        recomputeMaxLength = false
	      }
	    })
	    if (recomputeMaxLength) { cm.curOp.updateMaxLine = true }
	  }

	  retreatFrontier(doc, from.line)
	  startWorker(cm, 400)

	  var lendiff = change.text.length - (to.line - from.line) - 1
	  // Remember that these lines changed, for updating the display
	  if (change.full)
	    { regChange(cm) }
	  else if (from.line == to.line && change.text.length == 1 && !isWholeLineUpdate(cm.doc, change))
	    { regLineChange(cm, from.line, "text") }
	  else
	    { regChange(cm, from.line, to.line + 1, lendiff) }

	  var changesHandler = hasHandler(cm, "changes"), changeHandler = hasHandler(cm, "change")
	  if (changeHandler || changesHandler) {
	    var obj = {
	      from: from, to: to,
	      text: change.text,
	      removed: change.removed,
	      origin: change.origin
	    }
	    if (changeHandler) { signalLater(cm, "change", cm, obj) }
	    if (changesHandler) { (cm.curOp.changeObjs || (cm.curOp.changeObjs = [])).push(obj) }
	  }
	  cm.display.selForContextMenu = null
	}

	function replaceRange(doc, code, from, to, origin) {
	  if (!to) { to = from }
	  if (cmp(to, from) < 0) { var assign;
	    (assign = [to, from], from = assign[0], to = assign[1], assign) }
	  if (typeof code == "string") { code = doc.splitLines(code) }
	  makeChange(doc, {from: from, to: to, text: code, origin: origin})
	}

	// Rebasing/resetting history to deal with externally-sourced changes

	function rebaseHistSelSingle(pos, from, to, diff) {
	  if (to < pos.line) {
	    pos.line += diff
	  } else if (from < pos.line) {
	    pos.line = from
	    pos.ch = 0
	  }
	}

	// Tries to rebase an array of history events given a change in the
	// document. If the change touches the same lines as the event, the
	// event, and everything 'behind' it, is discarded. If the change is
	// before the event, the event's positions are updated. Uses a
	// copy-on-write scheme for the positions, to avoid having to
	// reallocate them all on every rebase, but also avoid problems with
	// shared position objects being unsafely updated.
	function rebaseHistArray(array, from, to, diff) {
	  for (var i = 0; i < array.length; ++i) {
	    var sub = array[i], ok = true
	    if (sub.ranges) {
	      if (!sub.copied) { sub = array[i] = sub.deepCopy(); sub.copied = true }
	      for (var j = 0; j < sub.ranges.length; j++) {
	        rebaseHistSelSingle(sub.ranges[j].anchor, from, to, diff)
	        rebaseHistSelSingle(sub.ranges[j].head, from, to, diff)
	      }
	      continue
	    }
	    for (var j$1 = 0; j$1 < sub.changes.length; ++j$1) {
	      var cur = sub.changes[j$1]
	      if (to < cur.from.line) {
	        cur.from = Pos(cur.from.line + diff, cur.from.ch)
	        cur.to = Pos(cur.to.line + diff, cur.to.ch)
	      } else if (from <= cur.to.line) {
	        ok = false
	        break
	      }
	    }
	    if (!ok) {
	      array.splice(0, i + 1)
	      i = 0
	    }
	  }
	}

	function rebaseHist(hist, change) {
	  var from = change.from.line, to = change.to.line, diff = change.text.length - (to - from) - 1
	  rebaseHistArray(hist.done, from, to, diff)
	  rebaseHistArray(hist.undone, from, to, diff)
	}

	// Utility for applying a change to a line by handle or number,
	// returning the number and optionally registering the line as
	// changed.
	function changeLine(doc, handle, changeType, op) {
	  var no = handle, line = handle
	  if (typeof handle == "number") { line = getLine(doc, clipLine(doc, handle)) }
	  else { no = lineNo(handle) }
	  if (no == null) { return null }
	  if (op(line, no) && doc.cm) { regLineChange(doc.cm, no, changeType) }
	  return line
	}

	// The document is represented as a BTree consisting of leaves, with
	// chunk of lines in them, and branches, with up to ten leaves or
	// other branch nodes below them. The top node is always a branch
	// node, and is the document object itself (meaning it has
	// additional methods and properties).
	//
	// All nodes have parent links. The tree is used both to go from
	// line numbers to line objects, and to go from objects to numbers.
	// It also indexes by height, and is used to convert between height
	// and line object, and to find the total height of the document.
	//
	// See also http://marijnhaverbeke.nl/blog/codemirror-line-tree.html

	function LeafChunk(lines) {
	  var this$1 = this;

	  this.lines = lines
	  this.parent = null
	  var height = 0
	  for (var i = 0; i < lines.length; ++i) {
	    lines[i].parent = this$1
	    height += lines[i].height
	  }
	  this.height = height
	}

	LeafChunk.prototype = {
	  chunkSize: function chunkSize() { return this.lines.length },

	  // Remove the n lines at offset 'at'.
	  removeInner: function removeInner(at, n) {
	    var this$1 = this;

	    for (var i = at, e = at + n; i < e; ++i) {
	      var line = this$1.lines[i]
	      this$1.height -= line.height
	      cleanUpLine(line)
	      signalLater(line, "delete")
	    }
	    this.lines.splice(at, n)
	  },

	  // Helper used to collapse a small branch into a single leaf.
	  collapse: function collapse(lines) {
	    lines.push.apply(lines, this.lines)
	  },

	  // Insert the given array of lines at offset 'at', count them as
	  // having the given height.
	  insertInner: function insertInner(at, lines, height) {
	    var this$1 = this;

	    this.height += height
	    this.lines = this.lines.slice(0, at).concat(lines).concat(this.lines.slice(at))
	    for (var i = 0; i < lines.length; ++i) { lines[i].parent = this$1 }
	  },

	  // Used to iterate over a part of the tree.
	  iterN: function iterN(at, n, op) {
	    var this$1 = this;

	    for (var e = at + n; at < e; ++at)
	      { if (op(this$1.lines[at])) { return true } }
	  }
	}

	function BranchChunk(children) {
	  var this$1 = this;

	  this.children = children
	  var size = 0, height = 0
	  for (var i = 0; i < children.length; ++i) {
	    var ch = children[i]
	    size += ch.chunkSize(); height += ch.height
	    ch.parent = this$1
	  }
	  this.size = size
	  this.height = height
	  this.parent = null
	}

	BranchChunk.prototype = {
	  chunkSize: function chunkSize() { return this.size },

	  removeInner: function removeInner(at, n) {
	    var this$1 = this;

	    this.size -= n
	    for (var i = 0; i < this.children.length; ++i) {
	      var child = this$1.children[i], sz = child.chunkSize()
	      if (at < sz) {
	        var rm = Math.min(n, sz - at), oldHeight = child.height
	        child.removeInner(at, rm)
	        this$1.height -= oldHeight - child.height
	        if (sz == rm) { this$1.children.splice(i--, 1); child.parent = null }
	        if ((n -= rm) == 0) { break }
	        at = 0
	      } else { at -= sz }
	    }
	    // If the result is smaller than 25 lines, ensure that it is a
	    // single leaf node.
	    if (this.size - n < 25 &&
	        (this.children.length > 1 || !(this.children[0] instanceof LeafChunk))) {
	      var lines = []
	      this.collapse(lines)
	      this.children = [new LeafChunk(lines)]
	      this.children[0].parent = this
	    }
	  },

	  collapse: function collapse(lines) {
	    var this$1 = this;

	    for (var i = 0; i < this.children.length; ++i) { this$1.children[i].collapse(lines) }
	  },

	  insertInner: function insertInner(at, lines, height) {
	    var this$1 = this;

	    this.size += lines.length
	    this.height += height
	    for (var i = 0; i < this.children.length; ++i) {
	      var child = this$1.children[i], sz = child.chunkSize()
	      if (at <= sz) {
	        child.insertInner(at, lines, height)
	        if (child.lines && child.lines.length > 50) {
	          // To avoid memory thrashing when child.lines is huge (e.g. first view of a large file), it's never spliced.
	          // Instead, small slices are taken. They're taken in order because sequential memory accesses are fastest.
	          var remaining = child.lines.length % 25 + 25
	          for (var pos = remaining; pos < child.lines.length;) {
	            var leaf = new LeafChunk(child.lines.slice(pos, pos += 25))
	            child.height -= leaf.height
	            this$1.children.splice(++i, 0, leaf)
	            leaf.parent = this$1
	          }
	          child.lines = child.lines.slice(0, remaining)
	          this$1.maybeSpill()
	        }
	        break
	      }
	      at -= sz
	    }
	  },

	  // When a node has grown, check whether it should be split.
	  maybeSpill: function maybeSpill() {
	    if (this.children.length <= 10) { return }
	    var me = this
	    do {
	      var spilled = me.children.splice(me.children.length - 5, 5)
	      var sibling = new BranchChunk(spilled)
	      if (!me.parent) { // Become the parent node
	        var copy = new BranchChunk(me.children)
	        copy.parent = me
	        me.children = [copy, sibling]
	        me = copy
	     } else {
	        me.size -= sibling.size
	        me.height -= sibling.height
	        var myIndex = indexOf(me.parent.children, me)
	        me.parent.children.splice(myIndex + 1, 0, sibling)
	      }
	      sibling.parent = me.parent
	    } while (me.children.length > 10)
	    me.parent.maybeSpill()
	  },

	  iterN: function iterN(at, n, op) {
	    var this$1 = this;

	    for (var i = 0; i < this.children.length; ++i) {
	      var child = this$1.children[i], sz = child.chunkSize()
	      if (at < sz) {
	        var used = Math.min(n, sz - at)
	        if (child.iterN(at, used, op)) { return true }
	        if ((n -= used) == 0) { break }
	        at = 0
	      } else { at -= sz }
	    }
	  }
	}

	// Line widgets are block elements displayed above or below a line.

	var LineWidget = function(doc, node, options) {
	  var this$1 = this;

	  if (options) { for (var opt in options) { if (options.hasOwnProperty(opt))
	    { this$1[opt] = options[opt] } } }
	  this.doc = doc
	  this.node = node
	};

	LineWidget.prototype.clear = function () {
	    var this$1 = this;

	  var cm = this.doc.cm, ws = this.line.widgets, line = this.line, no = lineNo(line)
	  if (no == null || !ws) { return }
	  for (var i = 0; i < ws.length; ++i) { if (ws[i] == this$1) { ws.splice(i--, 1) } }
	  if (!ws.length) { line.widgets = null }
	  var height = widgetHeight(this)
	  updateLineHeight(line, Math.max(0, line.height - height))
	  if (cm) {
	    runInOp(cm, function () {
	      adjustScrollWhenAboveVisible(cm, line, -height)
	      regLineChange(cm, no, "widget")
	    })
	    signalLater(cm, "lineWidgetCleared", cm, this, no)
	  }
	};

	LineWidget.prototype.changed = function () {
	    var this$1 = this;

	  var oldH = this.height, cm = this.doc.cm, line = this.line
	  this.height = null
	  var diff = widgetHeight(this) - oldH
	  if (!diff) { return }
	  updateLineHeight(line, line.height + diff)
	  if (cm) {
	    runInOp(cm, function () {
	      cm.curOp.forceUpdate = true
	      adjustScrollWhenAboveVisible(cm, line, diff)
	      signalLater(cm, "lineWidgetChanged", cm, this$1, lineNo(line))
	    })
	  }
	};
	eventMixin(LineWidget)

	function adjustScrollWhenAboveVisible(cm, line, diff) {
	  if (heightAtLine(line) < ((cm.curOp && cm.curOp.scrollTop) || cm.doc.scrollTop))
	    { addToScrollTop(cm, diff) }
	}

	function addLineWidget(doc, handle, node, options) {
	  var widget = new LineWidget(doc, node, options)
	  var cm = doc.cm
	  if (cm && widget.noHScroll) { cm.display.alignWidgets = true }
	  changeLine(doc, handle, "widget", function (line) {
	    var widgets = line.widgets || (line.widgets = [])
	    if (widget.insertAt == null) { widgets.push(widget) }
	    else { widgets.splice(Math.min(widgets.length - 1, Math.max(0, widget.insertAt)), 0, widget) }
	    widget.line = line
	    if (cm && !lineIsHidden(doc, line)) {
	      var aboveVisible = heightAtLine(line) < doc.scrollTop
	      updateLineHeight(line, line.height + widgetHeight(widget))
	      if (aboveVisible) { addToScrollTop(cm, widget.height) }
	      cm.curOp.forceUpdate = true
	    }
	    return true
	  })
	  signalLater(cm, "lineWidgetAdded", cm, widget, typeof handle == "number" ? handle : lineNo(handle))
	  return widget
	}

	// TEXTMARKERS

	// Created with markText and setBookmark methods. A TextMarker is a
	// handle that can be used to clear or find a marked position in the
	// document. Line objects hold arrays (markedSpans) containing
	// {from, to, marker} object pointing to such marker objects, and
	// indicating that such a marker is present on that line. Multiple
	// lines may point to the same marker when it spans across lines.
	// The spans will have null for their from/to properties when the
	// marker continues beyond the start/end of the line. Markers have
	// links back to the lines they currently touch.

	// Collapsed markers have unique ids, in order to be able to order
	// them, which is needed for uniquely determining an outer marker
	// when they overlap (they may nest, but not partially overlap).
	var nextMarkerId = 0

	var TextMarker = function(doc, type) {
	  this.lines = []
	  this.type = type
	  this.doc = doc
	  this.id = ++nextMarkerId
	};

	// Clear the marker.
	TextMarker.prototype.clear = function () {
	    var this$1 = this;

	  if (this.explicitlyCleared) { return }
	  var cm = this.doc.cm, withOp = cm && !cm.curOp
	  if (withOp) { startOperation(cm) }
	  if (hasHandler(this, "clear")) {
	    var found = this.find()
	    if (found) { signalLater(this, "clear", found.from, found.to) }
	  }
	  var min = null, max = null
	  for (var i = 0; i < this.lines.length; ++i) {
	    var line = this$1.lines[i]
	    var span = getMarkedSpanFor(line.markedSpans, this$1)
	    if (cm && !this$1.collapsed) { regLineChange(cm, lineNo(line), "text") }
	    else if (cm) {
	      if (span.to != null) { max = lineNo(line) }
	      if (span.from != null) { min = lineNo(line) }
	    }
	    line.markedSpans = removeMarkedSpan(line.markedSpans, span)
	    if (span.from == null && this$1.collapsed && !lineIsHidden(this$1.doc, line) && cm)
	      { updateLineHeight(line, textHeight(cm.display)) }
	  }
	  if (cm && this.collapsed && !cm.options.lineWrapping) { for (var i$1 = 0; i$1 < this.lines.length; ++i$1) {
	    var visual = visualLine(this$1.lines[i$1]), len = lineLength(visual)
	    if (len > cm.display.maxLineLength) {
	      cm.display.maxLine = visual
	      cm.display.maxLineLength = len
	      cm.display.maxLineChanged = true
	    }
	  } }

	  if (min != null && cm && this.collapsed) { regChange(cm, min, max + 1) }
	  this.lines.length = 0
	  this.explicitlyCleared = true
	  if (this.atomic && this.doc.cantEdit) {
	    this.doc.cantEdit = false
	    if (cm) { reCheckSelection(cm.doc) }
	  }
	  if (cm) { signalLater(cm, "markerCleared", cm, this, min, max) }
	  if (withOp) { endOperation(cm) }
	  if (this.parent) { this.parent.clear() }
	};

	// Find the position of the marker in the document. Returns a {from,
	// to} object by default. Side can be passed to get a specific side
	// -- 0 (both), -1 (left), or 1 (right). When lineObj is true, the
	// Pos objects returned contain a line object, rather than a line
	// number (used to prevent looking up the same line twice).
	TextMarker.prototype.find = function (side, lineObj) {
	    var this$1 = this;

	  if (side == null && this.type == "bookmark") { side = 1 }
	  var from, to
	  for (var i = 0; i < this.lines.length; ++i) {
	    var line = this$1.lines[i]
	    var span = getMarkedSpanFor(line.markedSpans, this$1)
	    if (span.from != null) {
	      from = Pos(lineObj ? line : lineNo(line), span.from)
	      if (side == -1) { return from }
	    }
	    if (span.to != null) {
	      to = Pos(lineObj ? line : lineNo(line), span.to)
	      if (side == 1) { return to }
	    }
	  }
	  return from && {from: from, to: to}
	};

	// Signals that the marker's widget changed, and surrounding layout
	// should be recomputed.
	TextMarker.prototype.changed = function () {
	    var this$1 = this;

	  var pos = this.find(-1, true), widget = this, cm = this.doc.cm
	  if (!pos || !cm) { return }
	  runInOp(cm, function () {
	    var line = pos.line, lineN = lineNo(pos.line)
	    var view = findViewForLine(cm, lineN)
	    if (view) {
	      clearLineMeasurementCacheFor(view)
	      cm.curOp.selectionChanged = cm.curOp.forceUpdate = true
	    }
	    cm.curOp.updateMaxLine = true
	    if (!lineIsHidden(widget.doc, line) && widget.height != null) {
	      var oldHeight = widget.height
	      widget.height = null
	      var dHeight = widgetHeight(widget) - oldHeight
	      if (dHeight)
	        { updateLineHeight(line, line.height + dHeight) }
	    }
	    signalLater(cm, "markerChanged", cm, this$1)
	  })
	};

	TextMarker.prototype.attachLine = function (line) {
	  if (!this.lines.length && this.doc.cm) {
	    var op = this.doc.cm.curOp
	    if (!op.maybeHiddenMarkers || indexOf(op.maybeHiddenMarkers, this) == -1)
	      { (op.maybeUnhiddenMarkers || (op.maybeUnhiddenMarkers = [])).push(this) }
	  }
	  this.lines.push(line)
	};

	TextMarker.prototype.detachLine = function (line) {
	  this.lines.splice(indexOf(this.lines, line), 1)
	  if (!this.lines.length && this.doc.cm) {
	    var op = this.doc.cm.curOp
	    ;(op.maybeHiddenMarkers || (op.maybeHiddenMarkers = [])).push(this)
	  }
	};
	eventMixin(TextMarker)

	// Create a marker, wire it up to the right lines, and
	function markText(doc, from, to, options, type) {
	  // Shared markers (across linked documents) are handled separately
	  // (markTextShared will call out to this again, once per
	  // document).
	  if (options && options.shared) { return markTextShared(doc, from, to, options, type) }
	  // Ensure we are in an operation.
	  if (doc.cm && !doc.cm.curOp) { return operation(doc.cm, markText)(doc, from, to, options, type) }

	  var marker = new TextMarker(doc, type), diff = cmp(from, to)
	  if (options) { copyObj(options, marker, false) }
	  // Don't connect empty markers unless clearWhenEmpty is false
	  if (diff > 0 || diff == 0 && marker.clearWhenEmpty !== false)
	    { return marker }
	  if (marker.replacedWith) {
	    // Showing up as a widget implies collapsed (widget replaces text)
	    marker.collapsed = true
	    marker.widgetNode = eltP("span", [marker.replacedWith], "CodeMirror-widget")
	    if (!options.handleMouseEvents) { marker.widgetNode.setAttribute("cm-ignore-events", "true") }
	    if (options.insertLeft) { marker.widgetNode.insertLeft = true }
	  }
	  if (marker.collapsed) {
	    if (conflictingCollapsedRange(doc, from.line, from, to, marker) ||
	        from.line != to.line && conflictingCollapsedRange(doc, to.line, from, to, marker))
	      { throw new Error("Inserting collapsed marker partially overlapping an existing one") }
	    seeCollapsedSpans()
	  }

	  if (marker.addToHistory)
	    { addChangeToHistory(doc, {from: from, to: to, origin: "markText"}, doc.sel, NaN) }

	  var curLine = from.line, cm = doc.cm, updateMaxLine
	  doc.iter(curLine, to.line + 1, function (line) {
	    if (cm && marker.collapsed && !cm.options.lineWrapping && visualLine(line) == cm.display.maxLine)
	      { updateMaxLine = true }
	    if (marker.collapsed && curLine != from.line) { updateLineHeight(line, 0) }
	    addMarkedSpan(line, new MarkedSpan(marker,
	                                       curLine == from.line ? from.ch : null,
	                                       curLine == to.line ? to.ch : null))
	    ++curLine
	  })
	  // lineIsHidden depends on the presence of the spans, so needs a second pass
	  if (marker.collapsed) { doc.iter(from.line, to.line + 1, function (line) {
	    if (lineIsHidden(doc, line)) { updateLineHeight(line, 0) }
	  }) }

	  if (marker.clearOnEnter) { on(marker, "beforeCursorEnter", function () { return marker.clear(); }) }

	  if (marker.readOnly) {
	    seeReadOnlySpans()
	    if (doc.history.done.length || doc.history.undone.length)
	      { doc.clearHistory() }
	  }
	  if (marker.collapsed) {
	    marker.id = ++nextMarkerId
	    marker.atomic = true
	  }
	  if (cm) {
	    // Sync editor state
	    if (updateMaxLine) { cm.curOp.updateMaxLine = true }
	    if (marker.collapsed)
	      { regChange(cm, from.line, to.line + 1) }
	    else if (marker.className || marker.title || marker.startStyle || marker.endStyle || marker.css)
	      { for (var i = from.line; i <= to.line; i++) { regLineChange(cm, i, "text") } }
	    if (marker.atomic) { reCheckSelection(cm.doc) }
	    signalLater(cm, "markerAdded", cm, marker)
	  }
	  return marker
	}

	// SHARED TEXTMARKERS

	// A shared marker spans multiple linked documents. It is
	// implemented as a meta-marker-object controlling multiple normal
	// markers.
	var SharedTextMarker = function(markers, primary) {
	  var this$1 = this;

	  this.markers = markers
	  this.primary = primary
	  for (var i = 0; i < markers.length; ++i)
	    { markers[i].parent = this$1 }
	};

	SharedTextMarker.prototype.clear = function () {
	    var this$1 = this;

	  if (this.explicitlyCleared) { return }
	  this.explicitlyCleared = true
	  for (var i = 0; i < this.markers.length; ++i)
	    { this$1.markers[i].clear() }
	  signalLater(this, "clear")
	};

	SharedTextMarker.prototype.find = function (side, lineObj) {
	  return this.primary.find(side, lineObj)
	};
	eventMixin(SharedTextMarker)

	function markTextShared(doc, from, to, options, type) {
	  options = copyObj(options)
	  options.shared = false
	  var markers = [markText(doc, from, to, options, type)], primary = markers[0]
	  var widget = options.widgetNode
	  linkedDocs(doc, function (doc) {
	    if (widget) { options.widgetNode = widget.cloneNode(true) }
	    markers.push(markText(doc, clipPos(doc, from), clipPos(doc, to), options, type))
	    for (var i = 0; i < doc.linked.length; ++i)
	      { if (doc.linked[i].isParent) { return } }
	    primary = lst(markers)
	  })
	  return new SharedTextMarker(markers, primary)
	}

	function findSharedMarkers(doc) {
	  return doc.findMarks(Pos(doc.first, 0), doc.clipPos(Pos(doc.lastLine())), function (m) { return m.parent; })
	}

	function copySharedMarkers(doc, markers) {
	  for (var i = 0; i < markers.length; i++) {
	    var marker = markers[i], pos = marker.find()
	    var mFrom = doc.clipPos(pos.from), mTo = doc.clipPos(pos.to)
	    if (cmp(mFrom, mTo)) {
	      var subMark = markText(doc, mFrom, mTo, marker.primary, marker.primary.type)
	      marker.markers.push(subMark)
	      subMark.parent = marker
	    }
	  }
	}

	function detachSharedMarkers(markers) {
	  var loop = function ( i ) {
	    var marker = markers[i], linked = [marker.primary.doc]
	    linkedDocs(marker.primary.doc, function (d) { return linked.push(d); })
	    for (var j = 0; j < marker.markers.length; j++) {
	      var subMarker = marker.markers[j]
	      if (indexOf(linked, subMarker.doc) == -1) {
	        subMarker.parent = null
	        marker.markers.splice(j--, 1)
	      }
	    }
	  };

	  for (var i = 0; i < markers.length; i++) loop( i );
	}

	var nextDocId = 0
	var Doc = function(text, mode, firstLine, lineSep, direction) {
	  if (!(this instanceof Doc)) { return new Doc(text, mode, firstLine, lineSep, direction) }
	  if (firstLine == null) { firstLine = 0 }

	  BranchChunk.call(this, [new LeafChunk([new Line("", null)])])
	  this.first = firstLine
	  this.scrollTop = this.scrollLeft = 0
	  this.cantEdit = false
	  this.cleanGeneration = 1
	  this.modeFrontier = this.highlightFrontier = firstLine
	  var start = Pos(firstLine, 0)
	  this.sel = simpleSelection(start)
	  this.history = new History(null)
	  this.id = ++nextDocId
	  this.modeOption = mode
	  this.lineSep = lineSep
	  this.direction = (direction == "rtl") ? "rtl" : "ltr"
	  this.extend = false

	  if (typeof text == "string") { text = this.splitLines(text) }
	  updateDoc(this, {from: start, to: start, text: text})
	  setSelection(this, simpleSelection(start), sel_dontScroll)
	}

	Doc.prototype = createObj(BranchChunk.prototype, {
	  constructor: Doc,
	  // Iterate over the document. Supports two forms -- with only one
	  // argument, it calls that for each line in the document. With
	  // three, it iterates over the range given by the first two (with
	  // the second being non-inclusive).
	  iter: function(from, to, op) {
	    if (op) { this.iterN(from - this.first, to - from, op) }
	    else { this.iterN(this.first, this.first + this.size, from) }
	  },

	  // Non-public interface for adding and removing lines.
	  insert: function(at, lines) {
	    var height = 0
	    for (var i = 0; i < lines.length; ++i) { height += lines[i].height }
	    this.insertInner(at - this.first, lines, height)
	  },
	  remove: function(at, n) { this.removeInner(at - this.first, n) },

	  // From here, the methods are part of the public interface. Most
	  // are also available from CodeMirror (editor) instances.

	  getValue: function(lineSep) {
	    var lines = getLines(this, this.first, this.first + this.size)
	    if (lineSep === false) { return lines }
	    return lines.join(lineSep || this.lineSeparator())
	  },
	  setValue: docMethodOp(function(code) {
	    var top = Pos(this.first, 0), last = this.first + this.size - 1
	    makeChange(this, {from: top, to: Pos(last, getLine(this, last).text.length),
	                      text: this.splitLines(code), origin: "setValue", full: true}, true)
	    if (this.cm) { scrollToCoords(this.cm, 0, 0) }
	    setSelection(this, simpleSelection(top), sel_dontScroll)
	  }),
	  replaceRange: function(code, from, to, origin) {
	    from = clipPos(this, from)
	    to = to ? clipPos(this, to) : from
	    replaceRange(this, code, from, to, origin)
	  },
	  getRange: function(from, to, lineSep) {
	    var lines = getBetween(this, clipPos(this, from), clipPos(this, to))
	    if (lineSep === false) { return lines }
	    return lines.join(lineSep || this.lineSeparator())
	  },

	  getLine: function(line) {var l = this.getLineHandle(line); return l && l.text},

	  getLineHandle: function(line) {if (isLine(this, line)) { return getLine(this, line) }},
	  getLineNumber: function(line) {return lineNo(line)},

	  getLineHandleVisualStart: function(line) {
	    if (typeof line == "number") { line = getLine(this, line) }
	    return visualLine(line)
	  },

	  lineCount: function() {return this.size},
	  firstLine: function() {return this.first},
	  lastLine: function() {return this.first + this.size - 1},

	  clipPos: function(pos) {return clipPos(this, pos)},

	  getCursor: function(start) {
	    var range = this.sel.primary(), pos
	    if (start == null || start == "head") { pos = range.head }
	    else if (start == "anchor") { pos = range.anchor }
	    else if (start == "end" || start == "to" || start === false) { pos = range.to() }
	    else { pos = range.from() }
	    return pos
	  },
	  listSelections: function() { return this.sel.ranges },
	  somethingSelected: function() {return this.sel.somethingSelected()},

	  setCursor: docMethodOp(function(line, ch, options) {
	    setSimpleSelection(this, clipPos(this, typeof line == "number" ? Pos(line, ch || 0) : line), null, options)
	  }),
	  setSelection: docMethodOp(function(anchor, head, options) {
	    setSimpleSelection(this, clipPos(this, anchor), clipPos(this, head || anchor), options)
	  }),
	  extendSelection: docMethodOp(function(head, other, options) {
	    extendSelection(this, clipPos(this, head), other && clipPos(this, other), options)
	  }),
	  extendSelections: docMethodOp(function(heads, options) {
	    extendSelections(this, clipPosArray(this, heads), options)
	  }),
	  extendSelectionsBy: docMethodOp(function(f, options) {
	    var heads = map(this.sel.ranges, f)
	    extendSelections(this, clipPosArray(this, heads), options)
	  }),
	  setSelections: docMethodOp(function(ranges, primary, options) {
	    var this$1 = this;

	    if (!ranges.length) { return }
	    var out = []
	    for (var i = 0; i < ranges.length; i++)
	      { out[i] = new Range(clipPos(this$1, ranges[i].anchor),
	                         clipPos(this$1, ranges[i].head)) }
	    if (primary == null) { primary = Math.min(ranges.length - 1, this.sel.primIndex) }
	    setSelection(this, normalizeSelection(out, primary), options)
	  }),
	  addSelection: docMethodOp(function(anchor, head, options) {
	    var ranges = this.sel.ranges.slice(0)
	    ranges.push(new Range(clipPos(this, anchor), clipPos(this, head || anchor)))
	    setSelection(this, normalizeSelection(ranges, ranges.length - 1), options)
	  }),

	  getSelection: function(lineSep) {
	    var this$1 = this;

	    var ranges = this.sel.ranges, lines
	    for (var i = 0; i < ranges.length; i++) {
	      var sel = getBetween(this$1, ranges[i].from(), ranges[i].to())
	      lines = lines ? lines.concat(sel) : sel
	    }
	    if (lineSep === false) { return lines }
	    else { return lines.join(lineSep || this.lineSeparator()) }
	  },
	  getSelections: function(lineSep) {
	    var this$1 = this;

	    var parts = [], ranges = this.sel.ranges
	    for (var i = 0; i < ranges.length; i++) {
	      var sel = getBetween(this$1, ranges[i].from(), ranges[i].to())
	      if (lineSep !== false) { sel = sel.join(lineSep || this$1.lineSeparator()) }
	      parts[i] = sel
	    }
	    return parts
	  },
	  replaceSelection: function(code, collapse, origin) {
	    var dup = []
	    for (var i = 0; i < this.sel.ranges.length; i++)
	      { dup[i] = code }
	    this.replaceSelections(dup, collapse, origin || "+input")
	  },
	  replaceSelections: docMethodOp(function(code, collapse, origin) {
	    var this$1 = this;

	    var changes = [], sel = this.sel
	    for (var i = 0; i < sel.ranges.length; i++) {
	      var range = sel.ranges[i]
	      changes[i] = {from: range.from(), to: range.to(), text: this$1.splitLines(code[i]), origin: origin}
	    }
	    var newSel = collapse && collapse != "end" && computeReplacedSel(this, changes, collapse)
	    for (var i$1 = changes.length - 1; i$1 >= 0; i$1--)
	      { makeChange(this$1, changes[i$1]) }
	    if (newSel) { setSelectionReplaceHistory(this, newSel) }
	    else if (this.cm) { ensureCursorVisible(this.cm) }
	  }),
	  undo: docMethodOp(function() {makeChangeFromHistory(this, "undo")}),
	  redo: docMethodOp(function() {makeChangeFromHistory(this, "redo")}),
	  undoSelection: docMethodOp(function() {makeChangeFromHistory(this, "undo", true)}),
	  redoSelection: docMethodOp(function() {makeChangeFromHistory(this, "redo", true)}),

	  setExtending: function(val) {this.extend = val},
	  getExtending: function() {return this.extend},

	  historySize: function() {
	    var hist = this.history, done = 0, undone = 0
	    for (var i = 0; i < hist.done.length; i++) { if (!hist.done[i].ranges) { ++done } }
	    for (var i$1 = 0; i$1 < hist.undone.length; i$1++) { if (!hist.undone[i$1].ranges) { ++undone } }
	    return {undo: done, redo: undone}
	  },
	  clearHistory: function() {this.history = new History(this.history.maxGeneration)},

	  markClean: function() {
	    this.cleanGeneration = this.changeGeneration(true)
	  },
	  changeGeneration: function(forceSplit) {
	    if (forceSplit)
	      { this.history.lastOp = this.history.lastSelOp = this.history.lastOrigin = null }
	    return this.history.generation
	  },
	  isClean: function (gen) {
	    return this.history.generation == (gen || this.cleanGeneration)
	  },

	  getHistory: function() {
	    return {done: copyHistoryArray(this.history.done),
	            undone: copyHistoryArray(this.history.undone)}
	  },
	  setHistory: function(histData) {
	    var hist = this.history = new History(this.history.maxGeneration)
	    hist.done = copyHistoryArray(histData.done.slice(0), null, true)
	    hist.undone = copyHistoryArray(histData.undone.slice(0), null, true)
	  },

	  setGutterMarker: docMethodOp(function(line, gutterID, value) {
	    return changeLine(this, line, "gutter", function (line) {
	      var markers = line.gutterMarkers || (line.gutterMarkers = {})
	      markers[gutterID] = value
	      if (!value && isEmpty(markers)) { line.gutterMarkers = null }
	      return true
	    })
	  }),

	  clearGutter: docMethodOp(function(gutterID) {
	    var this$1 = this;

	    this.iter(function (line) {
	      if (line.gutterMarkers && line.gutterMarkers[gutterID]) {
	        changeLine(this$1, line, "gutter", function () {
	          line.gutterMarkers[gutterID] = null
	          if (isEmpty(line.gutterMarkers)) { line.gutterMarkers = null }
	          return true
	        })
	      }
	    })
	  }),

	  lineInfo: function(line) {
	    var n
	    if (typeof line == "number") {
	      if (!isLine(this, line)) { return null }
	      n = line
	      line = getLine(this, line)
	      if (!line) { return null }
	    } else {
	      n = lineNo(line)
	      if (n == null) { return null }
	    }
	    return {line: n, handle: line, text: line.text, gutterMarkers: line.gutterMarkers,
	            textClass: line.textClass, bgClass: line.bgClass, wrapClass: line.wrapClass,
	            widgets: line.widgets}
	  },

	  addLineClass: docMethodOp(function(handle, where, cls) {
	    return changeLine(this, handle, where == "gutter" ? "gutter" : "class", function (line) {
	      var prop = where == "text" ? "textClass"
	               : where == "background" ? "bgClass"
	               : where == "gutter" ? "gutterClass" : "wrapClass"
	      if (!line[prop]) { line[prop] = cls }
	      else if (classTest(cls).test(line[prop])) { return false }
	      else { line[prop] += " " + cls }
	      return true
	    })
	  }),
	  removeLineClass: docMethodOp(function(handle, where, cls) {
	    return changeLine(this, handle, where == "gutter" ? "gutter" : "class", function (line) {
	      var prop = where == "text" ? "textClass"
	               : where == "background" ? "bgClass"
	               : where == "gutter" ? "gutterClass" : "wrapClass"
	      var cur = line[prop]
	      if (!cur) { return false }
	      else if (cls == null) { line[prop] = null }
	      else {
	        var found = cur.match(classTest(cls))
	        if (!found) { return false }
	        var end = found.index + found[0].length
	        line[prop] = cur.slice(0, found.index) + (!found.index || end == cur.length ? "" : " ") + cur.slice(end) || null
	      }
	      return true
	    })
	  }),

	  addLineWidget: docMethodOp(function(handle, node, options) {
	    return addLineWidget(this, handle, node, options)
	  }),
	  removeLineWidget: function(widget) { widget.clear() },

	  markText: function(from, to, options) {
	    return markText(this, clipPos(this, from), clipPos(this, to), options, options && options.type || "range")
	  },
	  setBookmark: function(pos, options) {
	    var realOpts = {replacedWith: options && (options.nodeType == null ? options.widget : options),
	                    insertLeft: options && options.insertLeft,
	                    clearWhenEmpty: false, shared: options && options.shared,
	                    handleMouseEvents: options && options.handleMouseEvents}
	    pos = clipPos(this, pos)
	    return markText(this, pos, pos, realOpts, "bookmark")
	  },
	  findMarksAt: function(pos) {
	    pos = clipPos(this, pos)
	    var markers = [], spans = getLine(this, pos.line).markedSpans
	    if (spans) { for (var i = 0; i < spans.length; ++i) {
	      var span = spans[i]
	      if ((span.from == null || span.from <= pos.ch) &&
	          (span.to == null || span.to >= pos.ch))
	        { markers.push(span.marker.parent || span.marker) }
	    } }
	    return markers
	  },
	  findMarks: function(from, to, filter) {
	    from = clipPos(this, from); to = clipPos(this, to)
	    var found = [], lineNo = from.line
	    this.iter(from.line, to.line + 1, function (line) {
	      var spans = line.markedSpans
	      if (spans) { for (var i = 0; i < spans.length; i++) {
	        var span = spans[i]
	        if (!(span.to != null && lineNo == from.line && from.ch >= span.to ||
	              span.from == null && lineNo != from.line ||
	              span.from != null && lineNo == to.line && span.from >= to.ch) &&
	            (!filter || filter(span.marker)))
	          { found.push(span.marker.parent || span.marker) }
	      } }
	      ++lineNo
	    })
	    return found
	  },
	  getAllMarks: function() {
	    var markers = []
	    this.iter(function (line) {
	      var sps = line.markedSpans
	      if (sps) { for (var i = 0; i < sps.length; ++i)
	        { if (sps[i].from != null) { markers.push(sps[i].marker) } } }
	    })
	    return markers
	  },

	  posFromIndex: function(off) {
	    var ch, lineNo = this.first, sepSize = this.lineSeparator().length
	    this.iter(function (line) {
	      var sz = line.text.length + sepSize
	      if (sz > off) { ch = off; return true }
	      off -= sz
	      ++lineNo
	    })
	    return clipPos(this, Pos(lineNo, ch))
	  },
	  indexFromPos: function (coords) {
	    coords = clipPos(this, coords)
	    var index = coords.ch
	    if (coords.line < this.first || coords.ch < 0) { return 0 }
	    var sepSize = this.lineSeparator().length
	    this.iter(this.first, coords.line, function (line) { // iter aborts when callback returns a truthy value
	      index += line.text.length + sepSize
	    })
	    return index
	  },

	  copy: function(copyHistory) {
	    var doc = new Doc(getLines(this, this.first, this.first + this.size),
	                      this.modeOption, this.first, this.lineSep, this.direction)
	    doc.scrollTop = this.scrollTop; doc.scrollLeft = this.scrollLeft
	    doc.sel = this.sel
	    doc.extend = false
	    if (copyHistory) {
	      doc.history.undoDepth = this.history.undoDepth
	      doc.setHistory(this.getHistory())
	    }
	    return doc
	  },

	  linkedDoc: function(options) {
	    if (!options) { options = {} }
	    var from = this.first, to = this.first + this.size
	    if (options.from != null && options.from > from) { from = options.from }
	    if (options.to != null && options.to < to) { to = options.to }
	    var copy = new Doc(getLines(this, from, to), options.mode || this.modeOption, from, this.lineSep, this.direction)
	    if (options.sharedHist) { copy.history = this.history
	    ; }(this.linked || (this.linked = [])).push({doc: copy, sharedHist: options.sharedHist})
	    copy.linked = [{doc: this, isParent: true, sharedHist: options.sharedHist}]
	    copySharedMarkers(copy, findSharedMarkers(this))
	    return copy
	  },
	  unlinkDoc: function(other) {
	    var this$1 = this;

	    if (other instanceof CodeMirror) { other = other.doc }
	    if (this.linked) { for (var i = 0; i < this.linked.length; ++i) {
	      var link = this$1.linked[i]
	      if (link.doc != other) { continue }
	      this$1.linked.splice(i, 1)
	      other.unlinkDoc(this$1)
	      detachSharedMarkers(findSharedMarkers(this$1))
	      break
	    } }
	    // If the histories were shared, split them again
	    if (other.history == this.history) {
	      var splitIds = [other.id]
	      linkedDocs(other, function (doc) { return splitIds.push(doc.id); }, true)
	      other.history = new History(null)
	      other.history.done = copyHistoryArray(this.history.done, splitIds)
	      other.history.undone = copyHistoryArray(this.history.undone, splitIds)
	    }
	  },
	  iterLinkedDocs: function(f) {linkedDocs(this, f)},

	  getMode: function() {return this.mode},
	  getEditor: function() {return this.cm},

	  splitLines: function(str) {
	    if (this.lineSep) { return str.split(this.lineSep) }
	    return splitLinesAuto(str)
	  },
	  lineSeparator: function() { return this.lineSep || "\n" },

	  setDirection: docMethodOp(function (dir) {
	    if (dir != "rtl") { dir = "ltr" }
	    if (dir == this.direction) { return }
	    this.direction = dir
	    this.iter(function (line) { return line.order = null; })
	    if (this.cm) { directionChanged(this.cm) }
	  })
	})

	// Public alias.
	Doc.prototype.eachLine = Doc.prototype.iter

	// Kludge to work around strange IE behavior where it'll sometimes
	// re-fire a series of drag-related events right after the drop (#1551)
	var lastDrop = 0

	function onDrop(e) {
	  var cm = this
	  clearDragCursor(cm)
	  if (signalDOMEvent(cm, e) || eventInWidget(cm.display, e))
	    { return }
	  e_preventDefault(e)
	  if (ie) { lastDrop = +new Date }
	  var pos = posFromMouse(cm, e, true), files = e.dataTransfer.files
	  if (!pos || cm.isReadOnly()) { return }
	  // Might be a file drop, in which case we simply extract the text
	  // and insert it.
	  if (files && files.length && window.FileReader && window.File) {
	    var n = files.length, text = Array(n), read = 0
	    var loadFile = function (file, i) {
	      if (cm.options.allowDropFileTypes &&
	          indexOf(cm.options.allowDropFileTypes, file.type) == -1)
	        { return }

	      var reader = new FileReader
	      reader.onload = operation(cm, function () {
	        var content = reader.result
	        if (/[\x00-\x08\x0e-\x1f]{2}/.test(content)) { content = "" }
	        text[i] = content
	        if (++read == n) {
	          pos = clipPos(cm.doc, pos)
	          var change = {from: pos, to: pos,
	                        text: cm.doc.splitLines(text.join(cm.doc.lineSeparator())),
	                        origin: "paste"}
	          makeChange(cm.doc, change)
	          setSelectionReplaceHistory(cm.doc, simpleSelection(pos, changeEnd(change)))
	        }
	      })
	      reader.readAsText(file)
	    }
	    for (var i = 0; i < n; ++i) { loadFile(files[i], i) }
	  } else { // Normal drop
	    // Don't do a replace if the drop happened inside of the selected text.
	    if (cm.state.draggingText && cm.doc.sel.contains(pos) > -1) {
	      cm.state.draggingText(e)
	      // Ensure the editor is re-focused
	      setTimeout(function () { return cm.display.input.focus(); }, 20)
	      return
	    }
	    try {
	      var text$1 = e.dataTransfer.getData("Text")
	      if (text$1) {
	        var selected
	        if (cm.state.draggingText && !cm.state.draggingText.copy)
	          { selected = cm.listSelections() }
	        setSelectionNoUndo(cm.doc, simpleSelection(pos, pos))
	        if (selected) { for (var i$1 = 0; i$1 < selected.length; ++i$1)
	          { replaceRange(cm.doc, "", selected[i$1].anchor, selected[i$1].head, "drag") } }
	        cm.replaceSelection(text$1, "around", "paste")
	        cm.display.input.focus()
	      }
	    }
	    catch(e){}
	  }
	}

	function onDragStart(cm, e) {
	  if (ie && (!cm.state.draggingText || +new Date - lastDrop < 100)) { e_stop(e); return }
	  if (signalDOMEvent(cm, e) || eventInWidget(cm.display, e)) { return }

	  e.dataTransfer.setData("Text", cm.getSelection())
	  e.dataTransfer.effectAllowed = "copyMove"

	  // Use dummy image instead of default browsers image.
	  // Recent Safari (~6.0.2) have a tendency to segfault when this happens, so we don't do it there.
	  if (e.dataTransfer.setDragImage && !safari) {
	    var img = elt("img", null, null, "position: fixed; left: 0; top: 0;")
	    img.src = "data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEALAAAAAABAAEAAAICTAEAOw=="
	    if (presto) {
	      img.width = img.height = 1
	      cm.display.wrapper.appendChild(img)
	      // Force a relayout, or Opera won't use our image for some obscure reason
	      img._top = img.offsetTop
	    }
	    e.dataTransfer.setDragImage(img, 0, 0)
	    if (presto) { img.parentNode.removeChild(img) }
	  }
	}

	function onDragOver(cm, e) {
	  var pos = posFromMouse(cm, e)
	  if (!pos) { return }
	  var frag = document.createDocumentFragment()
	  drawSelectionCursor(cm, pos, frag)
	  if (!cm.display.dragCursor) {
	    cm.display.dragCursor = elt("div", null, "CodeMirror-cursors CodeMirror-dragcursors")
	    cm.display.lineSpace.insertBefore(cm.display.dragCursor, cm.display.cursorDiv)
	  }
	  removeChildrenAndAdd(cm.display.dragCursor, frag)
	}

	function clearDragCursor(cm) {
	  if (cm.display.dragCursor) {
	    cm.display.lineSpace.removeChild(cm.display.dragCursor)
	    cm.display.dragCursor = null
	  }
	}

	// These must be handled carefully, because naively registering a
	// handler for each editor will cause the editors to never be
	// garbage collected.

	function forEachCodeMirror(f) {
	  if (!document.getElementsByClassName) { return }
	  var byClass = document.getElementsByClassName("CodeMirror")
	  for (var i = 0; i < byClass.length; i++) {
	    var cm = byClass[i].CodeMirror
	    if (cm) { f(cm) }
	  }
	}

	var globalsRegistered = false
	function ensureGlobalHandlers() {
	  if (globalsRegistered) { return }
	  registerGlobalHandlers()
	  globalsRegistered = true
	}
	function registerGlobalHandlers() {
	  // When the window resizes, we need to refresh active editors.
	  var resizeTimer
	  on(window, "resize", function () {
	    if (resizeTimer == null) { resizeTimer = setTimeout(function () {
	      resizeTimer = null
	      forEachCodeMirror(onResize)
	    }, 100) }
	  })
	  // When the window loses focus, we want to show the editor as blurred
	  on(window, "blur", function () { return forEachCodeMirror(onBlur); })
	}
	// Called when the window resizes
	function onResize(cm) {
	  var d = cm.display
	  if (d.lastWrapHeight == d.wrapper.clientHeight && d.lastWrapWidth == d.wrapper.clientWidth)
	    { return }
	  // Might be a text scaling operation, clear size caches.
	  d.cachedCharWidth = d.cachedTextHeight = d.cachedPaddingH = null
	  d.scrollbarsClipped = false
	  cm.setSize()
	}

	var keyNames = {
	  3: "Enter", 8: "Backspace", 9: "Tab", 13: "Enter", 16: "Shift", 17: "Ctrl", 18: "Alt",
	  19: "Pause", 20: "CapsLock", 27: "Esc", 32: "Space", 33: "PageUp", 34: "PageDown", 35: "End",
	  36: "Home", 37: "Left", 38: "Up", 39: "Right", 40: "Down", 44: "PrintScrn", 45: "Insert",
	  46: "Delete", 59: ";", 61: "=", 91: "Mod", 92: "Mod", 93: "Mod",
	  106: "*", 107: "=", 109: "-", 110: ".", 111: "/", 127: "Delete",
	  173: "-", 186: ";", 187: "=", 188: ",", 189: "-", 190: ".", 191: "/", 192: "`", 219: "[", 220: "\\",
	  221: "]", 222: "'", 63232: "Up", 63233: "Down", 63234: "Left", 63235: "Right", 63272: "Delete",
	  63273: "Home", 63275: "End", 63276: "PageUp", 63277: "PageDown", 63302: "Insert"
	}

	// Number keys
	for (var i = 0; i < 10; i++) { keyNames[i + 48] = keyNames[i + 96] = String(i) }
	// Alphabetic keys
	for (var i$1 = 65; i$1 <= 90; i$1++) { keyNames[i$1] = String.fromCharCode(i$1) }
	// Function keys
	for (var i$2 = 1; i$2 <= 12; i$2++) { keyNames[i$2 + 111] = keyNames[i$2 + 63235] = "F" + i$2 }

	var keyMap = {}

	keyMap.basic = {
	  "Left": "goCharLeft", "Right": "goCharRight", "Up": "goLineUp", "Down": "goLineDown",
	  "End": "goLineEnd", "Home": "goLineStartSmart", "PageUp": "goPageUp", "PageDown": "goPageDown",
	  "Delete": "delCharAfter", "Backspace": "delCharBefore", "Shift-Backspace": "delCharBefore",
	  "Tab": "defaultTab", "Shift-Tab": "indentAuto",
	  "Enter": "newlineAndIndent", "Insert": "toggleOverwrite",
	  "Esc": "singleSelection"
	}
	// Note that the save and find-related commands aren't defined by
	// default. User code or addons can define them. Unknown commands
	// are simply ignored.
	keyMap.pcDefault = {
	  "Ctrl-A": "selectAll", "Ctrl-D": "deleteLine", "Ctrl-Z": "undo", "Shift-Ctrl-Z": "redo", "Ctrl-Y": "redo",
	  "Ctrl-Home": "goDocStart", "Ctrl-End": "goDocEnd", "Ctrl-Up": "goLineUp", "Ctrl-Down": "goLineDown",
	  "Ctrl-Left": "goGroupLeft", "Ctrl-Right": "goGroupRight", "Alt-Left": "goLineStart", "Alt-Right": "goLineEnd",
	  "Ctrl-Backspace": "delGroupBefore", "Ctrl-Delete": "delGroupAfter", "Ctrl-S": "save", "Ctrl-F": "find",
	  "Ctrl-G": "findNext", "Shift-Ctrl-G": "findPrev", "Shift-Ctrl-F": "replace", "Shift-Ctrl-R": "replaceAll",
	  "Ctrl-[": "indentLess", "Ctrl-]": "indentMore",
	  "Ctrl-U": "undoSelection", "Shift-Ctrl-U": "redoSelection", "Alt-U": "redoSelection",
	  fallthrough: "basic"
	}
	// Very basic readline/emacs-style bindings, which are standard on Mac.
	keyMap.emacsy = {
	  "Ctrl-F": "goCharRight", "Ctrl-B": "goCharLeft", "Ctrl-P": "goLineUp", "Ctrl-N": "goLineDown",
	  "Alt-F": "goWordRight", "Alt-B": "goWordLeft", "Ctrl-A": "goLineStart", "Ctrl-E": "goLineEnd",
	  "Ctrl-V": "goPageDown", "Shift-Ctrl-V": "goPageUp", "Ctrl-D": "delCharAfter", "Ctrl-H": "delCharBefore",
	  "Alt-D": "delWordAfter", "Alt-Backspace": "delWordBefore", "Ctrl-K": "killLine", "Ctrl-T": "transposeChars",
	  "Ctrl-O": "openLine"
	}
	keyMap.macDefault = {
	  "Cmd-A": "selectAll", "Cmd-D": "deleteLine", "Cmd-Z": "undo", "Shift-Cmd-Z": "redo", "Cmd-Y": "redo",
	  "Cmd-Home": "goDocStart", "Cmd-Up": "goDocStart", "Cmd-End": "goDocEnd", "Cmd-Down": "goDocEnd", "Alt-Left": "goGroupLeft",
	  "Alt-Right": "goGroupRight", "Cmd-Left": "goLineLeft", "Cmd-Right": "goLineRight", "Alt-Backspace": "delGroupBefore",
	  "Ctrl-Alt-Backspace": "delGroupAfter", "Alt-Delete": "delGroupAfter", "Cmd-S": "save", "Cmd-F": "find",
	  "Cmd-G": "findNext", "Shift-Cmd-G": "findPrev", "Cmd-Alt-F": "replace", "Shift-Cmd-Alt-F": "replaceAll",
	  "Cmd-[": "indentLess", "Cmd-]": "indentMore", "Cmd-Backspace": "delWrappedLineLeft", "Cmd-Delete": "delWrappedLineRight",
	  "Cmd-U": "undoSelection", "Shift-Cmd-U": "redoSelection", "Ctrl-Up": "goDocStart", "Ctrl-Down": "goDocEnd",
	  fallthrough: ["basic", "emacsy"]
	}
	keyMap["default"] = mac ? keyMap.macDefault : keyMap.pcDefault

	// KEYMAP DISPATCH

	function normalizeKeyName(name) {
	  var parts = name.split(/-(?!$)/)
	  name = parts[parts.length - 1]
	  var alt, ctrl, shift, cmd
	  for (var i = 0; i < parts.length - 1; i++) {
	    var mod = parts[i]
	    if (/^(cmd|meta|m)$/i.test(mod)) { cmd = true }
	    else if (/^a(lt)?$/i.test(mod)) { alt = true }
	    else if (/^(c|ctrl|control)$/i.test(mod)) { ctrl = true }
	    else if (/^s(hift)?$/i.test(mod)) { shift = true }
	    else { throw new Error("Unrecognized modifier name: " + mod) }
	  }
	  if (alt) { name = "Alt-" + name }
	  if (ctrl) { name = "Ctrl-" + name }
	  if (cmd) { name = "Cmd-" + name }
	  if (shift) { name = "Shift-" + name }
	  return name
	}

	// This is a kludge to keep keymaps mostly working as raw objects
	// (backwards compatibility) while at the same time support features
	// like normalization and multi-stroke key bindings. It compiles a
	// new normalized keymap, and then updates the old object to reflect
	// this.
	function normalizeKeyMap(keymap) {
	  var copy = {}
	  for (var keyname in keymap) { if (keymap.hasOwnProperty(keyname)) {
	    var value = keymap[keyname]
	    if (/^(name|fallthrough|(de|at)tach)$/.test(keyname)) { continue }
	    if (value == "...") { delete keymap[keyname]; continue }

	    var keys = map(keyname.split(" "), normalizeKeyName)
	    for (var i = 0; i < keys.length; i++) {
	      var val = (void 0), name = (void 0)
	      if (i == keys.length - 1) {
	        name = keys.join(" ")
	        val = value
	      } else {
	        name = keys.slice(0, i + 1).join(" ")
	        val = "..."
	      }
	      var prev = copy[name]
	      if (!prev) { copy[name] = val }
	      else if (prev != val) { throw new Error("Inconsistent bindings for " + name) }
	    }
	    delete keymap[keyname]
	  } }
	  for (var prop in copy) { keymap[prop] = copy[prop] }
	  return keymap
	}

	function lookupKey(key, map, handle, context) {
	  map = getKeyMap(map)
	  var found = map.call ? map.call(key, context) : map[key]
	  if (found === false) { return "nothing" }
	  if (found === "...") { return "multi" }
	  if (found != null && handle(found)) { return "handled" }

	  if (map.fallthrough) {
	    if (Object.prototype.toString.call(map.fallthrough) != "[object Array]")
	      { return lookupKey(key, map.fallthrough, handle, context) }
	    for (var i = 0; i < map.fallthrough.length; i++) {
	      var result = lookupKey(key, map.fallthrough[i], handle, context)
	      if (result) { return result }
	    }
	  }
	}

	// Modifier key presses don't count as 'real' key presses for the
	// purpose of keymap fallthrough.
	function isModifierKey(value) {
	  var name = typeof value == "string" ? value : keyNames[value.keyCode]
	  return name == "Ctrl" || name == "Alt" || name == "Shift" || name == "Mod"
	}

	function addModifierNames(name, event, noShift) {
	  var base = name
	  if (event.altKey && base != "Alt") { name = "Alt-" + name }
	  if ((flipCtrlCmd ? event.metaKey : event.ctrlKey) && base != "Ctrl") { name = "Ctrl-" + name }
	  if ((flipCtrlCmd ? event.ctrlKey : event.metaKey) && base != "Cmd") { name = "Cmd-" + name }
	  if (!noShift && event.shiftKey && base != "Shift") { name = "Shift-" + name }
	  return name
	}

	// Look up the name of a key as indicated by an event object.
	function keyName(event, noShift) {
	  if (presto && event.keyCode == 34 && event["char"]) { return false }
	  var name = keyNames[event.keyCode]
	  if (name == null || event.altGraphKey) { return false }
	  return addModifierNames(name, event, noShift)
	}

	function getKeyMap(val) {
	  return typeof val == "string" ? keyMap[val] : val
	}

	// Helper for deleting text near the selection(s), used to implement
	// backspace, delete, and similar functionality.
	function deleteNearSelection(cm, compute) {
	  var ranges = cm.doc.sel.ranges, kill = []
	  // Build up a set of ranges to kill first, merging overlapping
	  // ranges.
	  for (var i = 0; i < ranges.length; i++) {
	    var toKill = compute(ranges[i])
	    while (kill.length && cmp(toKill.from, lst(kill).to) <= 0) {
	      var replaced = kill.pop()
	      if (cmp(replaced.from, toKill.from) < 0) {
	        toKill.from = replaced.from
	        break
	      }
	    }
	    kill.push(toKill)
	  }
	  // Next, remove those actual ranges.
	  runInOp(cm, function () {
	    for (var i = kill.length - 1; i >= 0; i--)
	      { replaceRange(cm.doc, "", kill[i].from, kill[i].to, "+delete") }
	    ensureCursorVisible(cm)
	  })
	}

	function moveCharLogically(line, ch, dir) {
	  var target = skipExtendingChars(line.text, ch + dir, dir)
	  return target < 0 || target > line.text.length ? null : target
	}

	function moveLogically(line, start, dir) {
	  var ch = moveCharLogically(line, start.ch, dir)
	  return ch == null ? null : new Pos(start.line, ch, dir < 0 ? "after" : "before")
	}

	function endOfLine(visually, cm, lineObj, lineNo, dir) {
	  if (visually) {
	    var order = getOrder(lineObj, cm.doc.direction)
	    if (order) {
	      var part = dir < 0 ? lst(order) : order[0]
	      var moveInStorageOrder = (dir < 0) == (part.level == 1)
	      var sticky = moveInStorageOrder ? "after" : "before"
	      var ch
	      // With a wrapped rtl chunk (possibly spanning multiple bidi parts),
	      // it could be that the last bidi part is not on the last visual line,
	      // since visual lines contain content order-consecutive chunks.
	      // Thus, in rtl, we are looking for the first (content-order) character
	      // in the rtl chunk that is on the last line (that is, the same line
	      // as the last (content-order) character).
	      if (part.level > 0 || cm.doc.direction == "rtl") {
	        var prep = prepareMeasureForLine(cm, lineObj)
	        ch = dir < 0 ? lineObj.text.length - 1 : 0
	        var targetTop = measureCharPrepared(cm, prep, ch).top
	        ch = findFirst(function (ch) { return measureCharPrepared(cm, prep, ch).top == targetTop; }, (dir < 0) == (part.level == 1) ? part.from : part.to - 1, ch)
	        if (sticky == "before") { ch = moveCharLogically(lineObj, ch, 1) }
	      } else { ch = dir < 0 ? part.to : part.from }
	      return new Pos(lineNo, ch, sticky)
	    }
	  }
	  return new Pos(lineNo, dir < 0 ? lineObj.text.length : 0, dir < 0 ? "before" : "after")
	}

	function moveVisually(cm, line, start, dir) {
	  var bidi = getOrder(line, cm.doc.direction)
	  if (!bidi) { return moveLogically(line, start, dir) }
	  if (start.ch >= line.text.length) {
	    start.ch = line.text.length
	    start.sticky = "before"
	  } else if (start.ch <= 0) {
	    start.ch = 0
	    start.sticky = "after"
	  }
	  var partPos = getBidiPartAt(bidi, start.ch, start.sticky), part = bidi[partPos]
	  if (cm.doc.direction == "ltr" && part.level % 2 == 0 && (dir > 0 ? part.to > start.ch : part.from < start.ch)) {
	    // Case 1: We move within an ltr part in an ltr editor. Even with wrapped lines,
	    // nothing interesting happens.
	    return moveLogically(line, start, dir)
	  }

	  var mv = function (pos, dir) { return moveCharLogically(line, pos instanceof Pos ? pos.ch : pos, dir); }
	  var prep
	  var getWrappedLineExtent = function (ch) {
	    if (!cm.options.lineWrapping) { return {begin: 0, end: line.text.length} }
	    prep = prep || prepareMeasureForLine(cm, line)
	    return wrappedLineExtentChar(cm, line, prep, ch)
	  }
	  var wrappedLineExtent = getWrappedLineExtent(start.sticky == "before" ? mv(start, -1) : start.ch)

	  if (cm.doc.direction == "rtl" || part.level == 1) {
	    var moveInStorageOrder = (part.level == 1) == (dir < 0)
	    var ch = mv(start, moveInStorageOrder ? 1 : -1)
	    if (ch != null && (!moveInStorageOrder ? ch >= part.from && ch >= wrappedLineExtent.begin : ch <= part.to && ch <= wrappedLineExtent.end)) {
	      // Case 2: We move within an rtl part or in an rtl editor on the same visual line
	      var sticky = moveInStorageOrder ? "before" : "after"
	      return new Pos(start.line, ch, sticky)
	    }
	  }

	  // Case 3: Could not move within this bidi part in this visual line, so leave
	  // the current bidi part

	  var searchInVisualLine = function (partPos, dir, wrappedLineExtent) {
	    var getRes = function (ch, moveInStorageOrder) { return moveInStorageOrder
	      ? new Pos(start.line, mv(ch, 1), "before")
	      : new Pos(start.line, ch, "after"); }

	    for (; partPos >= 0 && partPos < bidi.length; partPos += dir) {
	      var part = bidi[partPos]
	      var moveInStorageOrder = (dir > 0) == (part.level != 1)
	      var ch = moveInStorageOrder ? wrappedLineExtent.begin : mv(wrappedLineExtent.end, -1)
	      if (part.from <= ch && ch < part.to) { return getRes(ch, moveInStorageOrder) }
	      ch = moveInStorageOrder ? part.from : mv(part.to, -1)
	      if (wrappedLineExtent.begin <= ch && ch < wrappedLineExtent.end) { return getRes(ch, moveInStorageOrder) }
	    }
	  }

	  // Case 3a: Look for other bidi parts on the same visual line
	  var res = searchInVisualLine(partPos + dir, dir, wrappedLineExtent)
	  if (res) { return res }

	  // Case 3b: Look for other bidi parts on the next visual line
	  var nextCh = dir > 0 ? wrappedLineExtent.end : mv(wrappedLineExtent.begin, -1)
	  if (nextCh != null && !(dir > 0 && nextCh == line.text.length)) {
	    res = searchInVisualLine(dir > 0 ? 0 : bidi.length - 1, dir, getWrappedLineExtent(nextCh))
	    if (res) { return res }
	  }

	  // Case 4: Nowhere to move
	  return null
	}

	// Commands are parameter-less actions that can be performed on an
	// editor, mostly used for keybindings.
	var commands = {
	  selectAll: selectAll,
	  singleSelection: function (cm) { return cm.setSelection(cm.getCursor("anchor"), cm.getCursor("head"), sel_dontScroll); },
	  killLine: function (cm) { return deleteNearSelection(cm, function (range) {
	    if (range.empty()) {
	      var len = getLine(cm.doc, range.head.line).text.length
	      if (range.head.ch == len && range.head.line < cm.lastLine())
	        { return {from: range.head, to: Pos(range.head.line + 1, 0)} }
	      else
	        { return {from: range.head, to: Pos(range.head.line, len)} }
	    } else {
	      return {from: range.from(), to: range.to()}
	    }
	  }); },
	  deleteLine: function (cm) { return deleteNearSelection(cm, function (range) { return ({
	    from: Pos(range.from().line, 0),
	    to: clipPos(cm.doc, Pos(range.to().line + 1, 0))
	  }); }); },
	  delLineLeft: function (cm) { return deleteNearSelection(cm, function (range) { return ({
	    from: Pos(range.from().line, 0), to: range.from()
	  }); }); },
	  delWrappedLineLeft: function (cm) { return deleteNearSelection(cm, function (range) {
	    var top = cm.charCoords(range.head, "div").top + 5
	    var leftPos = cm.coordsChar({left: 0, top: top}, "div")
	    return {from: leftPos, to: range.from()}
	  }); },
	  delWrappedLineRight: function (cm) { return deleteNearSelection(cm, function (range) {
	    var top = cm.charCoords(range.head, "div").top + 5
	    var rightPos = cm.coordsChar({left: cm.display.lineDiv.offsetWidth + 100, top: top}, "div")
	    return {from: range.from(), to: rightPos }
	  }); },
	  undo: function (cm) { return cm.undo(); },
	  redo: function (cm) { return cm.redo(); },
	  undoSelection: function (cm) { return cm.undoSelection(); },
	  redoSelection: function (cm) { return cm.redoSelection(); },
	  goDocStart: function (cm) { return cm.extendSelection(Pos(cm.firstLine(), 0)); },
	  goDocEnd: function (cm) { return cm.extendSelection(Pos(cm.lastLine())); },
	  goLineStart: function (cm) { return cm.extendSelectionsBy(function (range) { return lineStart(cm, range.head.line); },
	    {origin: "+move", bias: 1}
	  ); },
	  goLineStartSmart: function (cm) { return cm.extendSelectionsBy(function (range) { return lineStartSmart(cm, range.head); },
	    {origin: "+move", bias: 1}
	  ); },
	  goLineEnd: function (cm) { return cm.extendSelectionsBy(function (range) { return lineEnd(cm, range.head.line); },
	    {origin: "+move", bias: -1}
	  ); },
	  goLineRight: function (cm) { return cm.extendSelectionsBy(function (range) {
	    var top = cm.cursorCoords(range.head, "div").top + 5
	    return cm.coordsChar({left: cm.display.lineDiv.offsetWidth + 100, top: top}, "div")
	  }, sel_move); },
	  goLineLeft: function (cm) { return cm.extendSelectionsBy(function (range) {
	    var top = cm.cursorCoords(range.head, "div").top + 5
	    return cm.coordsChar({left: 0, top: top}, "div")
	  }, sel_move); },
	  goLineLeftSmart: function (cm) { return cm.extendSelectionsBy(function (range) {
	    var top = cm.cursorCoords(range.head, "div").top + 5
	    var pos = cm.coordsChar({left: 0, top: top}, "div")
	    if (pos.ch < cm.getLine(pos.line).search(/\S/)) { return lineStartSmart(cm, range.head) }
	    return pos
	  }, sel_move); },
	  goLineUp: function (cm) { return cm.moveV(-1, "line"); },
	  goLineDown: function (cm) { return cm.moveV(1, "line"); },
	  goPageUp: function (cm) { return cm.moveV(-1, "page"); },
	  goPageDown: function (cm) { return cm.moveV(1, "page"); },
	  goCharLeft: function (cm) { return cm.moveH(-1, "char"); },
	  goCharRight: function (cm) { return cm.moveH(1, "char"); },
	  goColumnLeft: function (cm) { return cm.moveH(-1, "column"); },
	  goColumnRight: function (cm) { return cm.moveH(1, "column"); },
	  goWordLeft: function (cm) { return cm.moveH(-1, "word"); },
	  goGroupRight: function (cm) { return cm.moveH(1, "group"); },
	  goGroupLeft: function (cm) { return cm.moveH(-1, "group"); },
	  goWordRight: function (cm) { return cm.moveH(1, "word"); },
	  delCharBefore: function (cm) { return cm.deleteH(-1, "char"); },
	  delCharAfter: function (cm) { return cm.deleteH(1, "char"); },
	  delWordBefore: function (cm) { return cm.deleteH(-1, "word"); },
	  delWordAfter: function (cm) { return cm.deleteH(1, "word"); },
	  delGroupBefore: function (cm) { return cm.deleteH(-1, "group"); },
	  delGroupAfter: function (cm) { return cm.deleteH(1, "group"); },
	  indentAuto: function (cm) { return cm.indentSelection("smart"); },
	  indentMore: function (cm) { return cm.indentSelection("add"); },
	  indentLess: function (cm) { return cm.indentSelection("subtract"); },
	  insertTab: function (cm) { return cm.replaceSelection("\t"); },
	  insertSoftTab: function (cm) {
	    var spaces = [], ranges = cm.listSelections(), tabSize = cm.options.tabSize
	    for (var i = 0; i < ranges.length; i++) {
	      var pos = ranges[i].from()
	      var col = countColumn(cm.getLine(pos.line), pos.ch, tabSize)
	      spaces.push(spaceStr(tabSize - col % tabSize))
	    }
	    cm.replaceSelections(spaces)
	  },
	  defaultTab: function (cm) {
	    if (cm.somethingSelected()) { cm.indentSelection("add") }
	    else { cm.execCommand("insertTab") }
	  },
	  // Swap the two chars left and right of each selection's head.
	  // Move cursor behind the two swapped characters afterwards.
	  //
	  // Doesn't consider line feeds a character.
	  // Doesn't scan more than one line above to find a character.
	  // Doesn't do anything on an empty line.
	  // Doesn't do anything with non-empty selections.
	  transposeChars: function (cm) { return runInOp(cm, function () {
	    var ranges = cm.listSelections(), newSel = []
	    for (var i = 0; i < ranges.length; i++) {
	      if (!ranges[i].empty()) { continue }
	      var cur = ranges[i].head, line = getLine(cm.doc, cur.line).text
	      if (line) {
	        if (cur.ch == line.length) { cur = new Pos(cur.line, cur.ch - 1) }
	        if (cur.ch > 0) {
	          cur = new Pos(cur.line, cur.ch + 1)
	          cm.replaceRange(line.charAt(cur.ch - 1) + line.charAt(cur.ch - 2),
	                          Pos(cur.line, cur.ch - 2), cur, "+transpose")
	        } else if (cur.line > cm.doc.first) {
	          var prev = getLine(cm.doc, cur.line - 1).text
	          if (prev) {
	            cur = new Pos(cur.line, 1)
	            cm.replaceRange(line.charAt(0) + cm.doc.lineSeparator() +
	                            prev.charAt(prev.length - 1),
	                            Pos(cur.line - 1, prev.length - 1), cur, "+transpose")
	          }
	        }
	      }
	      newSel.push(new Range(cur, cur))
	    }
	    cm.setSelections(newSel)
	  }); },
	  newlineAndIndent: function (cm) { return runInOp(cm, function () {
	    var sels = cm.listSelections()
	    for (var i = sels.length - 1; i >= 0; i--)
	      { cm.replaceRange(cm.doc.lineSeparator(), sels[i].anchor, sels[i].head, "+input") }
	    sels = cm.listSelections()
	    for (var i$1 = 0; i$1 < sels.length; i$1++)
	      { cm.indentLine(sels[i$1].from().line, null, true) }
	    ensureCursorVisible(cm)
	  }); },
	  openLine: function (cm) { return cm.replaceSelection("\n", "start"); },
	  toggleOverwrite: function (cm) { return cm.toggleOverwrite(); }
	}


	function lineStart(cm, lineN) {
	  var line = getLine(cm.doc, lineN)
	  var visual = visualLine(line)
	  if (visual != line) { lineN = lineNo(visual) }
	  return endOfLine(true, cm, visual, lineN, 1)
	}
	function lineEnd(cm, lineN) {
	  var line = getLine(cm.doc, lineN)
	  var visual = visualLineEnd(line)
	  if (visual != line) { lineN = lineNo(visual) }
	  return endOfLine(true, cm, line, lineN, -1)
	}
	function lineStartSmart(cm, pos) {
	  var start = lineStart(cm, pos.line)
	  var line = getLine(cm.doc, start.line)
	  var order = getOrder(line, cm.doc.direction)
	  if (!order || order[0].level == 0) {
	    var firstNonWS = Math.max(0, line.text.search(/\S/))
	    var inWS = pos.line == start.line && pos.ch <= firstNonWS && pos.ch
	    return Pos(start.line, inWS ? 0 : firstNonWS, start.sticky)
	  }
	  return start
	}

	// Run a handler that was bound to a key.
	function doHandleBinding(cm, bound, dropShift) {
	  if (typeof bound == "string") {
	    bound = commands[bound]
	    if (!bound) { return false }
	  }
	  // Ensure previous input has been read, so that the handler sees a
	  // consistent view of the document
	  cm.display.input.ensurePolled()
	  var prevShift = cm.display.shift, done = false
	  try {
	    if (cm.isReadOnly()) { cm.state.suppressEdits = true }
	    if (dropShift) { cm.display.shift = false }
	    done = bound(cm) != Pass
	  } finally {
	    cm.display.shift = prevShift
	    cm.state.suppressEdits = false
	  }
	  return done
	}

	function lookupKeyForEditor(cm, name, handle) {
	  for (var i = 0; i < cm.state.keyMaps.length; i++) {
	    var result = lookupKey(name, cm.state.keyMaps[i], handle, cm)
	    if (result) { return result }
	  }
	  return (cm.options.extraKeys && lookupKey(name, cm.options.extraKeys, handle, cm))
	    || lookupKey(name, cm.options.keyMap, handle, cm)
	}

	// Note that, despite the name, this function is also used to check
	// for bound mouse clicks.

	var stopSeq = new Delayed

	function dispatchKey(cm, name, e, handle) {
	  var seq = cm.state.keySeq
	  if (seq) {
	    if (isModifierKey(name)) { return "handled" }
	    if (/\'$/.test(name))
	      { cm.state.keySeq = null }
	    else
	      { stopSeq.set(50, function () {
	        if (cm.state.keySeq == seq) {
	          cm.state.keySeq = null
	          cm.display.input.reset()
	        }
	      }) }
	    if (dispatchKeyInner(cm, seq + " " + name, e, handle)) { return true }
	  }
	  return dispatchKeyInner(cm, name, e, handle)
	}

	function dispatchKeyInner(cm, name, e, handle) {
	  var result = lookupKeyForEditor(cm, name, handle)

	  if (result == "multi")
	    { cm.state.keySeq = name }
	  if (result == "handled")
	    { signalLater(cm, "keyHandled", cm, name, e) }

	  if (result == "handled" || result == "multi") {
	    e_preventDefault(e)
	    restartBlink(cm)
	  }

	  return !!result
	}

	// Handle a key from the keydown event.
	function handleKeyBinding(cm, e) {
	  var name = keyName(e, true)
	  if (!name) { return false }

	  if (e.shiftKey && !cm.state.keySeq) {
	    // First try to resolve full name (including 'Shift-'). Failing
	    // that, see if there is a cursor-motion command (starting with
	    // 'go') bound to the keyname without 'Shift-'.
	    return dispatchKey(cm, "Shift-" + name, e, function (b) { return doHandleBinding(cm, b, true); })
	        || dispatchKey(cm, name, e, function (b) {
	             if (typeof b == "string" ? /^go[A-Z]/.test(b) : b.motion)
	               { return doHandleBinding(cm, b) }
	           })
	  } else {
	    return dispatchKey(cm, name, e, function (b) { return doHandleBinding(cm, b); })
	  }
	}

	// Handle a key from the keypress event
	function handleCharBinding(cm, e, ch) {
	  return dispatchKey(cm, "'" + ch + "'", e, function (b) { return doHandleBinding(cm, b, true); })
	}

	var lastStoppedKey = null
	function onKeyDown(e) {
	  var cm = this
	  cm.curOp.focus = activeElt()
	  if (signalDOMEvent(cm, e)) { return }
	  // IE does strange things with escape.
	  if (ie && ie_version < 11 && e.keyCode == 27) { e.returnValue = false }
	  var code = e.keyCode
	  cm.display.shift = code == 16 || e.shiftKey
	  var handled = handleKeyBinding(cm, e)
	  if (presto) {
	    lastStoppedKey = handled ? code : null
	    // Opera has no cut event... we try to at least catch the key combo
	    if (!handled && code == 88 && !hasCopyEvent && (mac ? e.metaKey : e.ctrlKey))
	      { cm.replaceSelection("", null, "cut") }
	  }

	  // Turn mouse into crosshair when Alt is held on Mac.
	  if (code == 18 && !/\bCodeMirror-crosshair\b/.test(cm.display.lineDiv.className))
	    { showCrossHair(cm) }
	}

	function showCrossHair(cm) {
	  var lineDiv = cm.display.lineDiv
	  addClass(lineDiv, "CodeMirror-crosshair")

	  function up(e) {
	    if (e.keyCode == 18 || !e.altKey) {
	      rmClass(lineDiv, "CodeMirror-crosshair")
	      off(document, "keyup", up)
	      off(document, "mouseover", up)
	    }
	  }
	  on(document, "keyup", up)
	  on(document, "mouseover", up)
	}

	function onKeyUp(e) {
	  if (e.keyCode == 16) { this.doc.sel.shift = false }
	  signalDOMEvent(this, e)
	}

	function onKeyPress(e) {
	  var cm = this
	  if (eventInWidget(cm.display, e) || signalDOMEvent(cm, e) || e.ctrlKey && !e.altKey || mac && e.metaKey) { return }
	  var keyCode = e.keyCode, charCode = e.charCode
	  if (presto && keyCode == lastStoppedKey) {lastStoppedKey = null; e_preventDefault(e); return}
	  if ((presto && (!e.which || e.which < 10)) && handleKeyBinding(cm, e)) { return }
	  var ch = String.fromCharCode(charCode == null ? keyCode : charCode)
	  // Some browsers fire keypress events for backspace
	  if (ch == "\x08") { return }
	  if (handleCharBinding(cm, e, ch)) { return }
	  cm.display.input.onKeyPress(e)
	}

	var DOUBLECLICK_DELAY = 400

	var PastClick = function(time, pos, button) {
	  this.time = time
	  this.pos = pos
	  this.button = button
	};

	PastClick.prototype.compare = function (time, pos, button) {
	  return this.time + DOUBLECLICK_DELAY > time &&
	    cmp(pos, this.pos) == 0 && button == this.button
	};

	var lastClick;
	var lastDoubleClick;
	function clickRepeat(pos, button) {
	  var now = +new Date
	  if (lastDoubleClick && lastDoubleClick.compare(now, pos, button)) {
	    lastClick = lastDoubleClick = null
	    return "triple"
	  } else if (lastClick && lastClick.compare(now, pos, button)) {
	    lastDoubleClick = new PastClick(now, pos, button)
	    lastClick = null
	    return "double"
	  } else {
	    lastClick = new PastClick(now, pos, button)
	    lastDoubleClick = null
	    return "single"
	  }
	}

	// A mouse down can be a single click, double click, triple click,
	// start of selection drag, start of text drag, new cursor
	// (ctrl-click), rectangle drag (alt-drag), or xwin
	// middle-click-paste. Or it might be a click on something we should
	// not interfere with, such as a scrollbar or widget.
	function onMouseDown(e) {
	  var cm = this, display = cm.display
	  if (signalDOMEvent(cm, e) || display.activeTouch && display.input.supportsTouch()) { return }
	  display.input.ensurePolled()
	  display.shift = e.shiftKey

	  if (eventInWidget(display, e)) {
	    if (!webkit) {
	      // Briefly turn off draggability, to allow widgets to do
	      // normal dragging things.
	      display.scroller.draggable = false
	      setTimeout(function () { return display.scroller.draggable = true; }, 100)
	    }
	    return
	  }
	  if (clickInGutter(cm, e)) { return }
	  var pos = posFromMouse(cm, e), button = e_button(e), repeat = pos ? clickRepeat(pos, button) : "single"
	  window.focus()

	  // #3261: make sure, that we're not starting a second selection
	  if (button == 1 && cm.state.selectingText)
	    { cm.state.selectingText(e) }

	  if (pos && handleMappedButton(cm, button, pos, repeat, e)) { return }

	  if (button == 1) {
	    if (pos) { leftButtonDown(cm, pos, repeat, e) }
	    else if (e_target(e) == display.scroller) { e_preventDefault(e) }
	  } else if (button == 2) {
	    if (pos) { extendSelection(cm.doc, pos) }
	    setTimeout(function () { return display.input.focus(); }, 20)
	  } else if (button == 3) {
	    if (captureRightClick) { onContextMenu(cm, e) }
	    else { delayBlurEvent(cm) }
	  }
	}

	function handleMappedButton(cm, button, pos, repeat, event) {
	  var name = "Click"
	  if (repeat == "double") { name = "Double" + name }
	  else if (repeat == "triple") { name = "Triple" + name }
	  name = (button == 1 ? "Left" : button == 2 ? "Middle" : "Right") + name

	  return dispatchKey(cm,  addModifierNames(name, event), event, function (bound) {
	    if (typeof bound == "string") { bound = commands[bound] }
	    if (!bound) { return false }
	    var done = false
	    try {
	      if (cm.isReadOnly()) { cm.state.suppressEdits = true }
	      done = bound(cm, pos) != Pass
	    } finally {
	      cm.state.suppressEdits = false
	    }
	    return done
	  })
	}

	function configureMouse(cm, repeat, event) {
	  var option = cm.getOption("configureMouse")
	  var value = option ? option(cm, repeat, event) : {}
	  if (value.unit == null) {
	    var rect = chromeOS ? event.shiftKey && event.metaKey : event.altKey
	    value.unit = rect ? "rectangle" : repeat == "single" ? "char" : repeat == "double" ? "word" : "line"
	  }
	  if (value.extend == null || cm.doc.extend) { value.extend = cm.doc.extend || event.shiftKey }
	  if (value.addNew == null) { value.addNew = mac ? event.metaKey : event.ctrlKey }
	  if (value.moveOnDrag == null) { value.moveOnDrag = !(mac ? event.altKey : event.ctrlKey) }
	  return value
	}

	function leftButtonDown(cm, pos, repeat, event) {
	  if (ie) { setTimeout(bind(ensureFocus, cm), 0) }
	  else { cm.curOp.focus = activeElt() }

	  var behavior = configureMouse(cm, repeat, event)

	  var sel = cm.doc.sel, contained
	  if (cm.options.dragDrop && dragAndDrop && !cm.isReadOnly() &&
	      repeat == "single" && (contained = sel.contains(pos)) > -1 &&
	      (cmp((contained = sel.ranges[contained]).from(), pos) < 0 || pos.xRel > 0) &&
	      (cmp(contained.to(), pos) > 0 || pos.xRel < 0))
	    { leftButtonStartDrag(cm, event, pos, behavior) }
	  else
	    { leftButtonSelect(cm, event, pos, behavior) }
	}

	// Start a text drag. When it ends, see if any dragging actually
	// happen, and treat as a click if it didn't.
	function leftButtonStartDrag(cm, event, pos, behavior) {
	  var display = cm.display, moved = false
	  var dragEnd = operation(cm, function (e) {
	    if (webkit) { display.scroller.draggable = false }
	    cm.state.draggingText = false
	    off(document, "mouseup", dragEnd)
	    off(document, "mousemove", mouseMove)
	    off(display.scroller, "dragstart", dragStart)
	    off(display.scroller, "drop", dragEnd)
	    if (!moved) {
	      e_preventDefault(e)
	      if (!behavior.addNew)
	        { extendSelection(cm.doc, pos, null, null, behavior.extend) }
	      // Work around unexplainable focus problem in IE9 (#2127) and Chrome (#3081)
	      if (webkit || ie && ie_version == 9)
	        { setTimeout(function () {document.body.focus(); display.input.focus()}, 20) }
	      else
	        { display.input.focus() }
	    }
	  })
	  var mouseMove = function(e2) {
	    moved = moved || Math.abs(event.clientX - e2.clientX) + Math.abs(event.clientY - e2.clientY) >= 10
	  }
	  var dragStart = function () { return moved = true; }
	  // Let the drag handler handle this.
	  if (webkit) { display.scroller.draggable = true }
	  cm.state.draggingText = dragEnd
	  dragEnd.copy = !behavior.moveOnDrag
	  // IE's approach to draggable
	  if (display.scroller.dragDrop) { display.scroller.dragDrop() }
	  on(document, "mouseup", dragEnd)
	  on(document, "mousemove", mouseMove)
	  on(display.scroller, "dragstart", dragStart)
	  on(display.scroller, "drop", dragEnd)

	  delayBlurEvent(cm)
	  setTimeout(function () { return display.input.focus(); }, 20)
	}

	function rangeForUnit(cm, pos, unit) {
	  if (unit == "char") { return new Range(pos, pos) }
	  if (unit == "word") { return cm.findWordAt(pos) }
	  if (unit == "line") { return new Range(Pos(pos.line, 0), clipPos(cm.doc, Pos(pos.line + 1, 0))) }
	  var result = unit(cm, pos)
	  return new Range(result.from, result.to)
	}

	// Normal selection, as opposed to text dragging.
	function leftButtonSelect(cm, event, start, behavior) {
	  var display = cm.display, doc = cm.doc
	  e_preventDefault(event)

	  var ourRange, ourIndex, startSel = doc.sel, ranges = startSel.ranges
	  if (behavior.addNew && !behavior.extend) {
	    ourIndex = doc.sel.contains(start)
	    if (ourIndex > -1)
	      { ourRange = ranges[ourIndex] }
	    else
	      { ourRange = new Range(start, start) }
	  } else {
	    ourRange = doc.sel.primary()
	    ourIndex = doc.sel.primIndex
	  }

	  if (behavior.unit == "rectangle") {
	    if (!behavior.addNew) { ourRange = new Range(start, start) }
	    start = posFromMouse(cm, event, true, true)
	    ourIndex = -1
	  } else {
	    var range = rangeForUnit(cm, start, behavior.unit)
	    if (behavior.extend)
	      { ourRange = extendRange(ourRange, range.anchor, range.head, behavior.extend) }
	    else
	      { ourRange = range }
	  }

	  if (!behavior.addNew) {
	    ourIndex = 0
	    setSelection(doc, new Selection([ourRange], 0), sel_mouse)
	    startSel = doc.sel
	  } else if (ourIndex == -1) {
	    ourIndex = ranges.length
	    setSelection(doc, normalizeSelection(ranges.concat([ourRange]), ourIndex),
	                 {scroll: false, origin: "*mouse"})
	  } else if (ranges.length > 1 && ranges[ourIndex].empty() && behavior.unit == "char" && !behavior.extend) {
	    setSelection(doc, normalizeSelection(ranges.slice(0, ourIndex).concat(ranges.slice(ourIndex + 1)), 0),
	                 {scroll: false, origin: "*mouse"})
	    startSel = doc.sel
	  } else {
	    replaceOneSelection(doc, ourIndex, ourRange, sel_mouse)
	  }

	  var lastPos = start
	  function extendTo(pos) {
	    if (cmp(lastPos, pos) == 0) { return }
	    lastPos = pos

	    if (behavior.unit == "rectangle") {
	      var ranges = [], tabSize = cm.options.tabSize
	      var startCol = countColumn(getLine(doc, start.line).text, start.ch, tabSize)
	      var posCol = countColumn(getLine(doc, pos.line).text, pos.ch, tabSize)
	      var left = Math.min(startCol, posCol), right = Math.max(startCol, posCol)
	      for (var line = Math.min(start.line, pos.line), end = Math.min(cm.lastLine(), Math.max(start.line, pos.line));
	           line <= end; line++) {
	        var text = getLine(doc, line).text, leftPos = findColumn(text, left, tabSize)
	        if (left == right)
	          { ranges.push(new Range(Pos(line, leftPos), Pos(line, leftPos))) }
	        else if (text.length > leftPos)
	          { ranges.push(new Range(Pos(line, leftPos), Pos(line, findColumn(text, right, tabSize)))) }
	      }
	      if (!ranges.length) { ranges.push(new Range(start, start)) }
	      setSelection(doc, normalizeSelection(startSel.ranges.slice(0, ourIndex).concat(ranges), ourIndex),
	                   {origin: "*mouse", scroll: false})
	      cm.scrollIntoView(pos)
	    } else {
	      var oldRange = ourRange
	      var range = rangeForUnit(cm, pos, behavior.unit)
	      var anchor = oldRange.anchor, head
	      if (cmp(range.anchor, anchor) > 0) {
	        head = range.head
	        anchor = minPos(oldRange.from(), range.anchor)
	      } else {
	        head = range.anchor
	        anchor = maxPos(oldRange.to(), range.head)
	      }
	      var ranges$1 = startSel.ranges.slice(0)
	      ranges$1[ourIndex] = bidiSimplify(cm, new Range(clipPos(doc, anchor), head))
	      setSelection(doc, normalizeSelection(ranges$1, ourIndex), sel_mouse)
	    }
	  }

	  var editorSize = display.wrapper.getBoundingClientRect()
	  // Used to ensure timeout re-tries don't fire when another extend
	  // happened in the meantime (clearTimeout isn't reliable -- at
	  // least on Chrome, the timeouts still happen even when cleared,
	  // if the clear happens after their scheduled firing time).
	  var counter = 0

	  function extend(e) {
	    var curCount = ++counter
	    var cur = posFromMouse(cm, e, true, behavior.unit == "rectangle")
	    if (!cur) { return }
	    if (cmp(cur, lastPos) != 0) {
	      cm.curOp.focus = activeElt()
	      extendTo(cur)
	      var visible = visibleLines(display, doc)
	      if (cur.line >= visible.to || cur.line < visible.from)
	        { setTimeout(operation(cm, function () {if (counter == curCount) { extend(e) }}), 150) }
	    } else {
	      var outside = e.clientY < editorSize.top ? -20 : e.clientY > editorSize.bottom ? 20 : 0
	      if (outside) { setTimeout(operation(cm, function () {
	        if (counter != curCount) { return }
	        display.scroller.scrollTop += outside
	        extend(e)
	      }), 50) }
	    }
	  }

	  function done(e) {
	    cm.state.selectingText = false
	    counter = Infinity
	    e_preventDefault(e)
	    display.input.focus()
	    off(document, "mousemove", move)
	    off(document, "mouseup", up)
	    doc.history.lastSelOrigin = null
	  }

	  var move = operation(cm, function (e) {
	    if (!e_button(e)) { done(e) }
	    else { extend(e) }
	  })
	  var up = operation(cm, done)
	  cm.state.selectingText = up
	  on(document, "mousemove", move)
	  on(document, "mouseup", up)
	}

	// Used when mouse-selecting to adjust the anchor to the proper side
	// of a bidi jump depending on the visual position of the head.
	function bidiSimplify(cm, range) {
	  var anchor = range.anchor;
	  var head = range.head;
	  var anchorLine = getLine(cm.doc, anchor.line)
	  if (cmp(anchor, head) == 0 && anchor.sticky == head.sticky) { return range }
	  var order = getOrder(anchorLine)
	  if (!order) { return range }
	  var index = getBidiPartAt(order, anchor.ch, anchor.sticky), part = order[index]
	  if (part.from != anchor.ch && part.to != anchor.ch) { return range }
	  var boundary = index + ((part.from == anchor.ch) == (part.level != 1) ? 0 : 1)
	  if (boundary == 0 || boundary == order.length) { return range }

	  // Compute the relative visual position of the head compared to the
	  // anchor (<0 is to the left, >0 to the right)
	  var leftSide
	  if (head.line != anchor.line) {
	    leftSide = (head.line - anchor.line) * (cm.doc.direction == "ltr" ? 1 : -1) > 0
	  } else {
	    var headIndex = getBidiPartAt(order, head.ch, head.sticky)
	    var dir = headIndex - index || (head.ch - anchor.ch) * (part.level == 1 ? -1 : 1)
	    if (headIndex == boundary - 1 || headIndex == boundary)
	      { leftSide = dir < 0 }
	    else
	      { leftSide = dir > 0 }
	  }

	  var usePart = order[boundary + (leftSide ? -1 : 0)]
	  var from = leftSide == (usePart.level == 1)
	  var ch = from ? usePart.from : usePart.to, sticky = from ? "after" : "before"
	  return anchor.ch == ch && anchor.sticky == sticky ? range : new Range(new Pos(anchor.line, ch, sticky), head)
	}


	// Determines whether an event happened in the gutter, and fires the
	// handlers for the corresponding event.
	function gutterEvent(cm, e, type, prevent) {
	  var mX, mY
	  if (e.touches) {
	    mX = e.touches[0].clientX
	    mY = e.touches[0].clientY
	  } else {
	    try { mX = e.clientX; mY = e.clientY }
	    catch(e) { return false }
	  }
	  if (mX >= Math.floor(cm.display.gutters.getBoundingClientRect().right)) { return false }
	  if (prevent) { e_preventDefault(e) }

	  var display = cm.display
	  var lineBox = display.lineDiv.getBoundingClientRect()

	  if (mY > lineBox.bottom || !hasHandler(cm, type)) { return e_defaultPrevented(e) }
	  mY -= lineBox.top - display.viewOffset

	  for (var i = 0; i < cm.options.gutters.length; ++i) {
	    var g = display.gutters.childNodes[i]
	    if (g && g.getBoundingClientRect().right >= mX) {
	      var line = lineAtHeight(cm.doc, mY)
	      var gutter = cm.options.gutters[i]
	      signal(cm, type, cm, line, gutter, e)
	      return e_defaultPrevented(e)
	    }
	  }
	}

	function clickInGutter(cm, e) {
	  return gutterEvent(cm, e, "gutterClick", true)
	}

	// CONTEXT MENU HANDLING

	// To make the context menu work, we need to briefly unhide the
	// textarea (making it as unobtrusive as possible) to let the
	// right-click take effect on it.
	function onContextMenu(cm, e) {
	  if (eventInWidget(cm.display, e) || contextMenuInGutter(cm, e)) { return }
	  if (signalDOMEvent(cm, e, "contextmenu")) { return }
	  cm.display.input.onContextMenu(e)
	}

	function contextMenuInGutter(cm, e) {
	  if (!hasHandler(cm, "gutterContextMenu")) { return false }
	  return gutterEvent(cm, e, "gutterContextMenu", false)
	}

	function themeChanged(cm) {
	  cm.display.wrapper.className = cm.display.wrapper.className.replace(/\s*cm-s-\S+/g, "") +
	    cm.options.theme.replace(/(^|\s)\s*/g, " cm-s-")
	  clearCaches(cm)
	}

	var Init = {toString: function(){return "CodeMirror.Init"}}

	var defaults = {}
	var optionHandlers = {}

	function defineOptions(CodeMirror) {
	  var optionHandlers = CodeMirror.optionHandlers

	  function option(name, deflt, handle, notOnInit) {
	    CodeMirror.defaults[name] = deflt
	    if (handle) { optionHandlers[name] =
	      notOnInit ? function (cm, val, old) {if (old != Init) { handle(cm, val, old) }} : handle }
	  }

	  CodeMirror.defineOption = option

	  // Passed to option handlers when there is no old value.
	  CodeMirror.Init = Init

	  // These two are, on init, called from the constructor because they
	  // have to be initialized before the editor can start at all.
	  option("value", "", function (cm, val) { return cm.setValue(val); }, true)
	  option("mode", null, function (cm, val) {
	    cm.doc.modeOption = val
	    loadMode(cm)
	  }, true)

	  option("indentUnit", 2, loadMode, true)
	  option("indentWithTabs", false)
	  option("smartIndent", true)
	  option("tabSize", 4, function (cm) {
	    resetModeState(cm)
	    clearCaches(cm)
	    regChange(cm)
	  }, true)
	  option("lineSeparator", null, function (cm, val) {
	    cm.doc.lineSep = val
	    if (!val) { return }
	    var newBreaks = [], lineNo = cm.doc.first
	    cm.doc.iter(function (line) {
	      for (var pos = 0;;) {
	        var found = line.text.indexOf(val, pos)
	        if (found == -1) { break }
	        pos = found + val.length
	        newBreaks.push(Pos(lineNo, found))
	      }
	      lineNo++
	    })
	    for (var i = newBreaks.length - 1; i >= 0; i--)
	      { replaceRange(cm.doc, val, newBreaks[i], Pos(newBreaks[i].line, newBreaks[i].ch + val.length)) }
	  })
	  option("specialChars", /[\u0000-\u001f\u007f-\u009f\u00ad\u061c\u200b-\u200f\u2028\u2029\ufeff]/g, function (cm, val, old) {
	    cm.state.specialChars = new RegExp(val.source + (val.test("\t") ? "" : "|\t"), "g")
	    if (old != Init) { cm.refresh() }
	  })
	  option("specialCharPlaceholder", defaultSpecialCharPlaceholder, function (cm) { return cm.refresh(); }, true)
	  option("electricChars", true)
	  option("inputStyle", mobile ? "contenteditable" : "textarea", function () {
	    throw new Error("inputStyle can not (yet) be changed in a running editor") // FIXME
	  }, true)
	  option("spellcheck", false, function (cm, val) { return cm.getInputField().spellcheck = val; }, true)
	  option("rtlMoveVisually", !windows)
	  option("wholeLineUpdateBefore", true)

	  option("theme", "default", function (cm) {
	    themeChanged(cm)
	    guttersChanged(cm)
	  }, true)
	  option("keyMap", "default", function (cm, val, old) {
	    var next = getKeyMap(val)
	    var prev = old != Init && getKeyMap(old)
	    if (prev && prev.detach) { prev.detach(cm, next) }
	    if (next.attach) { next.attach(cm, prev || null) }
	  })
	  option("extraKeys", null)
	  option("configureMouse", null)

	  option("lineWrapping", false, wrappingChanged, true)
	  option("gutters", [], function (cm) {
	    setGuttersForLineNumbers(cm.options)
	    guttersChanged(cm)
	  }, true)
	  option("fixedGutter", true, function (cm, val) {
	    cm.display.gutters.style.left = val ? compensateForHScroll(cm.display) + "px" : "0"
	    cm.refresh()
	  }, true)
	  option("coverGutterNextToScrollbar", false, function (cm) { return updateScrollbars(cm); }, true)
	  option("scrollbarStyle", "native", function (cm) {
	    initScrollbars(cm)
	    updateScrollbars(cm)
	    cm.display.scrollbars.setScrollTop(cm.doc.scrollTop)
	    cm.display.scrollbars.setScrollLeft(cm.doc.scrollLeft)
	  }, true)
	  option("lineNumbers", false, function (cm) {
	    setGuttersForLineNumbers(cm.options)
	    guttersChanged(cm)
	  }, true)
	  option("firstLineNumber", 1, guttersChanged, true)
	  option("lineNumberFormatter", function (integer) { return integer; }, guttersChanged, true)
	  option("showCursorWhenSelecting", false, updateSelection, true)

	  option("resetSelectionOnContextMenu", true)
	  option("lineWiseCopyCut", true)
	  option("pasteLinesPerSelection", true)

	  option("readOnly", false, function (cm, val) {
	    if (val == "nocursor") {
	      onBlur(cm)
	      cm.display.input.blur()
	    }
	    cm.display.input.readOnlyChanged(val)
	  })
	  option("disableInput", false, function (cm, val) {if (!val) { cm.display.input.reset() }}, true)
	  option("dragDrop", true, dragDropChanged)
	  option("allowDropFileTypes", null)

	  option("cursorBlinkRate", 530)
	  option("cursorScrollMargin", 0)
	  option("cursorHeight", 1, updateSelection, true)
	  option("singleCursorHeightPerLine", true, updateSelection, true)
	  option("workTime", 100)
	  option("workDelay", 100)
	  option("flattenSpans", true, resetModeState, true)
	  option("addModeClass", false, resetModeState, true)
	  option("pollInterval", 100)
	  option("undoDepth", 200, function (cm, val) { return cm.doc.history.undoDepth = val; })
	  option("historyEventDelay", 1250)
	  option("viewportMargin", 10, function (cm) { return cm.refresh(); }, true)
	  option("maxHighlightLength", 10000, resetModeState, true)
	  option("moveInputWithCursor", true, function (cm, val) {
	    if (!val) { cm.display.input.resetPosition() }
	  })

	  option("tabindex", null, function (cm, val) { return cm.display.input.getField().tabIndex = val || ""; })
	  option("autofocus", null)
	  option("direction", "ltr", function (cm, val) { return cm.doc.setDirection(val); }, true)
	}

	function guttersChanged(cm) {
	  updateGutters(cm)
	  regChange(cm)
	  alignHorizontally(cm)
	}

	function dragDropChanged(cm, value, old) {
	  var wasOn = old && old != Init
	  if (!value != !wasOn) {
	    var funcs = cm.display.dragFunctions
	    var toggle = value ? on : off
	    toggle(cm.display.scroller, "dragstart", funcs.start)
	    toggle(cm.display.scroller, "dragenter", funcs.enter)
	    toggle(cm.display.scroller, "dragover", funcs.over)
	    toggle(cm.display.scroller, "dragleave", funcs.leave)
	    toggle(cm.display.scroller, "drop", funcs.drop)
	  }
	}

	function wrappingChanged(cm) {
	  if (cm.options.lineWrapping) {
	    addClass(cm.display.wrapper, "CodeMirror-wrap")
	    cm.display.sizer.style.minWidth = ""
	    cm.display.sizerWidth = null
	  } else {
	    rmClass(cm.display.wrapper, "CodeMirror-wrap")
	    findMaxLine(cm)
	  }
	  estimateLineHeights(cm)
	  regChange(cm)
	  clearCaches(cm)
	  setTimeout(function () { return updateScrollbars(cm); }, 100)
	}

	// A CodeMirror instance represents an editor. This is the object
	// that user code is usually dealing with.

	function CodeMirror(place, options) {
	  var this$1 = this;

	  if (!(this instanceof CodeMirror)) { return new CodeMirror(place, options) }

	  this.options = options = options ? copyObj(options) : {}
	  // Determine effective options based on given values and defaults.
	  copyObj(defaults, options, false)
	  setGuttersForLineNumbers(options)

	  var doc = options.value
	  if (typeof doc == "string") { doc = new Doc(doc, options.mode, null, options.lineSeparator, options.direction) }
	  this.doc = doc

	  var input = new CodeMirror.inputStyles[options.inputStyle](this)
	  var display = this.display = new Display(place, doc, input)
	  display.wrapper.CodeMirror = this
	  updateGutters(this)
	  themeChanged(this)
	  if (options.lineWrapping)
	    { this.display.wrapper.className += " CodeMirror-wrap" }
	  initScrollbars(this)

	  this.state = {
	    keyMaps: [],  // stores maps added by addKeyMap
	    overlays: [], // highlighting overlays, as added by addOverlay
	    modeGen: 0,   // bumped when mode/overlay changes, used to invalidate highlighting info
	    overwrite: false,
	    delayingBlurEvent: false,
	    focused: false,
	    suppressEdits: false, // used to disable editing during key handlers when in readOnly mode
	    pasteIncoming: false, cutIncoming: false, // help recognize paste/cut edits in input.poll
	    selectingText: false,
	    draggingText: false,
	    highlight: new Delayed(), // stores highlight worker timeout
	    keySeq: null,  // Unfinished key sequence
	    specialChars: null
	  }

	  if (options.autofocus && !mobile) { display.input.focus() }

	  // Override magic textarea content restore that IE sometimes does
	  // on our hidden textarea on reload
	  if (ie && ie_version < 11) { setTimeout(function () { return this$1.display.input.reset(true); }, 20) }

	  registerEventHandlers(this)
	  ensureGlobalHandlers()

	  startOperation(this)
	  this.curOp.forceUpdate = true
	  attachDoc(this, doc)

	  if ((options.autofocus && !mobile) || this.hasFocus())
	    { setTimeout(bind(onFocus, this), 20) }
	  else
	    { onBlur(this) }

	  for (var opt in optionHandlers) { if (optionHandlers.hasOwnProperty(opt))
	    { optionHandlers[opt](this$1, options[opt], Init) } }
	  maybeUpdateLineNumberWidth(this)
	  if (options.finishInit) { options.finishInit(this) }
	  for (var i = 0; i < initHooks.length; ++i) { initHooks[i](this$1) }
	  endOperation(this)
	  // Suppress optimizelegibility in Webkit, since it breaks text
	  // measuring on line wrapping boundaries.
	  if (webkit && options.lineWrapping &&
	      getComputedStyle(display.lineDiv).textRendering == "optimizelegibility")
	    { display.lineDiv.style.textRendering = "auto" }
	}

	// The default configuration options.
	CodeMirror.defaults = defaults
	// Functions to run when options are changed.
	CodeMirror.optionHandlers = optionHandlers

	// Attach the necessary event handlers when initializing the editor
	function registerEventHandlers(cm) {
	  var d = cm.display
	  on(d.scroller, "mousedown", operation(cm, onMouseDown))
	  // Older IE's will not fire a second mousedown for a double click
	  if (ie && ie_version < 11)
	    { on(d.scroller, "dblclick", operation(cm, function (e) {
	      if (signalDOMEvent(cm, e)) { return }
	      var pos = posFromMouse(cm, e)
	      if (!pos || clickInGutter(cm, e) || eventInWidget(cm.display, e)) { return }
	      e_preventDefault(e)
	      var word = cm.findWordAt(pos)
	      extendSelection(cm.doc, word.anchor, word.head)
	    })) }
	  else
	    { on(d.scroller, "dblclick", function (e) { return signalDOMEvent(cm, e) || e_preventDefault(e); }) }
	  // Some browsers fire contextmenu *after* opening the menu, at
	  // which point we can't mess with it anymore. Context menu is
	  // handled in onMouseDown for these browsers.
	  if (!captureRightClick) { on(d.scroller, "contextmenu", function (e) { return onContextMenu(cm, e); }) }

	  // Used to suppress mouse event handling when a touch happens
	  var touchFinished, prevTouch = {end: 0}
	  function finishTouch() {
	    if (d.activeTouch) {
	      touchFinished = setTimeout(function () { return d.activeTouch = null; }, 1000)
	      prevTouch = d.activeTouch
	      prevTouch.end = +new Date
	    }
	  }
	  function isMouseLikeTouchEvent(e) {
	    if (e.touches.length != 1) { return false }
	    var touch = e.touches[0]
	    return touch.radiusX <= 1 && touch.radiusY <= 1
	  }
	  function farAway(touch, other) {
	    if (other.left == null) { return true }
	    var dx = other.left - touch.left, dy = other.top - touch.top
	    return dx * dx + dy * dy > 20 * 20
	  }
	  on(d.scroller, "touchstart", function (e) {
	    if (!signalDOMEvent(cm, e) && !isMouseLikeTouchEvent(e) && !clickInGutter(cm, e)) {
	      d.input.ensurePolled()
	      clearTimeout(touchFinished)
	      var now = +new Date
	      d.activeTouch = {start: now, moved: false,
	                       prev: now - prevTouch.end <= 300 ? prevTouch : null}
	      if (e.touches.length == 1) {
	        d.activeTouch.left = e.touches[0].pageX
	        d.activeTouch.top = e.touches[0].pageY
	      }
	    }
	  })
	  on(d.scroller, "touchmove", function () {
	    if (d.activeTouch) { d.activeTouch.moved = true }
	  })
	  on(d.scroller, "touchend", function (e) {
	    var touch = d.activeTouch
	    if (touch && !eventInWidget(d, e) && touch.left != null &&
	        !touch.moved && new Date - touch.start < 300) {
	      var pos = cm.coordsChar(d.activeTouch, "page"), range
	      if (!touch.prev || farAway(touch, touch.prev)) // Single tap
	        { range = new Range(pos, pos) }
	      else if (!touch.prev.prev || farAway(touch, touch.prev.prev)) // Double tap
	        { range = cm.findWordAt(pos) }
	      else // Triple tap
	        { range = new Range(Pos(pos.line, 0), clipPos(cm.doc, Pos(pos.line + 1, 0))) }
	      cm.setSelection(range.anchor, range.head)
	      cm.focus()
	      e_preventDefault(e)
	    }
	    finishTouch()
	  })
	  on(d.scroller, "touchcancel", finishTouch)

	  // Sync scrolling between fake scrollbars and real scrollable
	  // area, ensure viewport is updated when scrolling.
	  on(d.scroller, "scroll", function () {
	    if (d.scroller.clientHeight) {
	      updateScrollTop(cm, d.scroller.scrollTop)
	      setScrollLeft(cm, d.scroller.scrollLeft, true)
	      signal(cm, "scroll", cm)
	    }
	  })

	  // Listen to wheel events in order to try and update the viewport on time.
	  on(d.scroller, "mousewheel", function (e) { return onScrollWheel(cm, e); })
	  on(d.scroller, "DOMMouseScroll", function (e) { return onScrollWheel(cm, e); })

	  // Prevent wrapper from ever scrolling
	  on(d.wrapper, "scroll", function () { return d.wrapper.scrollTop = d.wrapper.scrollLeft = 0; })

	  d.dragFunctions = {
	    enter: function (e) {if (!signalDOMEvent(cm, e)) { e_stop(e) }},
	    over: function (e) {if (!signalDOMEvent(cm, e)) { onDragOver(cm, e); e_stop(e) }},
	    start: function (e) { return onDragStart(cm, e); },
	    drop: operation(cm, onDrop),
	    leave: function (e) {if (!signalDOMEvent(cm, e)) { clearDragCursor(cm) }}
	  }

	  var inp = d.input.getField()
	  on(inp, "keyup", function (e) { return onKeyUp.call(cm, e); })
	  on(inp, "keydown", operation(cm, onKeyDown))
	  on(inp, "keypress", operation(cm, onKeyPress))
	  on(inp, "focus", function (e) { return onFocus(cm, e); })
	  on(inp, "blur", function (e) { return onBlur(cm, e); })
	}

	var initHooks = []
	CodeMirror.defineInitHook = function (f) { return initHooks.push(f); }

	// Indent the given line. The how parameter can be "smart",
	// "add"/null, "subtract", or "prev". When aggressive is false
	// (typically set to true for forced single-line indents), empty
	// lines are not indented, and places where the mode returns Pass
	// are left alone.
	function indentLine(cm, n, how, aggressive) {
	  var doc = cm.doc, state
	  if (how == null) { how = "add" }
	  if (how == "smart") {
	    // Fall back to "prev" when the mode doesn't have an indentation
	    // method.
	    if (!doc.mode.indent) { how = "prev" }
	    else { state = getContextBefore(cm, n).state }
	  }

	  var tabSize = cm.options.tabSize
	  var line = getLine(doc, n), curSpace = countColumn(line.text, null, tabSize)
	  if (line.stateAfter) { line.stateAfter = null }
	  var curSpaceString = line.text.match(/^\s*/)[0], indentation
	  if (!aggressive && !/\S/.test(line.text)) {
	    indentation = 0
	    how = "not"
	  } else if (how == "smart") {
	    indentation = doc.mode.indent(state, line.text.slice(curSpaceString.length), line.text)
	    if (indentation == Pass || indentation > 150) {
	      if (!aggressive) { return }
	      how = "prev"
	    }
	  }
	  if (how == "prev") {
	    if (n > doc.first) { indentation = countColumn(getLine(doc, n-1).text, null, tabSize) }
	    else { indentation = 0 }
	  } else if (how == "add") {
	    indentation = curSpace + cm.options.indentUnit
	  } else if (how == "subtract") {
	    indentation = curSpace - cm.options.indentUnit
	  } else if (typeof how == "number") {
	    indentation = curSpace + how
	  }
	  indentation = Math.max(0, indentation)

	  var indentString = "", pos = 0
	  if (cm.options.indentWithTabs)
	    { for (var i = Math.floor(indentation / tabSize); i; --i) {pos += tabSize; indentString += "\t"} }
	  if (pos < indentation) { indentString += spaceStr(indentation - pos) }

	  if (indentString != curSpaceString) {
	    replaceRange(doc, indentString, Pos(n, 0), Pos(n, curSpaceString.length), "+input")
	    line.stateAfter = null
	    return true
	  } else {
	    // Ensure that, if the cursor was in the whitespace at the start
	    // of the line, it is moved to the end of that space.
	    for (var i$1 = 0; i$1 < doc.sel.ranges.length; i$1++) {
	      var range = doc.sel.ranges[i$1]
	      if (range.head.line == n && range.head.ch < curSpaceString.length) {
	        var pos$1 = Pos(n, curSpaceString.length)
	        replaceOneSelection(doc, i$1, new Range(pos$1, pos$1))
	        break
	      }
	    }
	  }
	}

	// This will be set to a {lineWise: bool, text: [string]} object, so
	// that, when pasting, we know what kind of selections the copied
	// text was made out of.
	var lastCopied = null

	function setLastCopied(newLastCopied) {
	  lastCopied = newLastCopied
	}

	function applyTextInput(cm, inserted, deleted, sel, origin) {
	  var doc = cm.doc
	  cm.display.shift = false
	  if (!sel) { sel = doc.sel }

	  var paste = cm.state.pasteIncoming || origin == "paste"
	  var textLines = splitLinesAuto(inserted), multiPaste = null
	  // When pasing N lines into N selections, insert one line per selection
	  if (paste && sel.ranges.length > 1) {
	    if (lastCopied && lastCopied.text.join("\n") == inserted) {
	      if (sel.ranges.length % lastCopied.text.length == 0) {
	        multiPaste = []
	        for (var i = 0; i < lastCopied.text.length; i++)
	          { multiPaste.push(doc.splitLines(lastCopied.text[i])) }
	      }
	    } else if (textLines.length == sel.ranges.length && cm.options.pasteLinesPerSelection) {
	      multiPaste = map(textLines, function (l) { return [l]; })
	    }
	  }

	  var updateInput
	  // Normal behavior is to insert the new text into every selection
	  for (var i$1 = sel.ranges.length - 1; i$1 >= 0; i$1--) {
	    var range = sel.ranges[i$1]
	    var from = range.from(), to = range.to()
	    if (range.empty()) {
	      if (deleted && deleted > 0) // Handle deletion
	        { from = Pos(from.line, from.ch - deleted) }
	      else if (cm.state.overwrite && !paste) // Handle overwrite
	        { to = Pos(to.line, Math.min(getLine(doc, to.line).text.length, to.ch + lst(textLines).length)) }
	      else if (lastCopied && lastCopied.lineWise && lastCopied.text.join("\n") == inserted)
	        { from = to = Pos(from.line, 0) }
	    }
	    updateInput = cm.curOp.updateInput
	    var changeEvent = {from: from, to: to, text: multiPaste ? multiPaste[i$1 % multiPaste.length] : textLines,
	                       origin: origin || (paste ? "paste" : cm.state.cutIncoming ? "cut" : "+input")}
	    makeChange(cm.doc, changeEvent)
	    signalLater(cm, "inputRead", cm, changeEvent)
	  }
	  if (inserted && !paste)
	    { triggerElectric(cm, inserted) }

	  ensureCursorVisible(cm)
	  cm.curOp.updateInput = updateInput
	  cm.curOp.typing = true
	  cm.state.pasteIncoming = cm.state.cutIncoming = false
	}

	function handlePaste(e, cm) {
	  var pasted = e.clipboardData && e.clipboardData.getData("Text")
	  if (pasted) {
	    e.preventDefault()
	    if (!cm.isReadOnly() && !cm.options.disableInput)
	      { runInOp(cm, function () { return applyTextInput(cm, pasted, 0, null, "paste"); }) }
	    return true
	  }
	}

	function triggerElectric(cm, inserted) {
	  // When an 'electric' character is inserted, immediately trigger a reindent
	  if (!cm.options.electricChars || !cm.options.smartIndent) { return }
	  var sel = cm.doc.sel

	  for (var i = sel.ranges.length - 1; i >= 0; i--) {
	    var range = sel.ranges[i]
	    if (range.head.ch > 100 || (i && sel.ranges[i - 1].head.line == range.head.line)) { continue }
	    var mode = cm.getModeAt(range.head)
	    var indented = false
	    if (mode.electricChars) {
	      for (var j = 0; j < mode.electricChars.length; j++)
	        { if (inserted.indexOf(mode.electricChars.charAt(j)) > -1) {
	          indented = indentLine(cm, range.head.line, "smart")
	          break
	        } }
	    } else if (mode.electricInput) {
	      if (mode.electricInput.test(getLine(cm.doc, range.head.line).text.slice(0, range.head.ch)))
	        { indented = indentLine(cm, range.head.line, "smart") }
	    }
	    if (indented) { signalLater(cm, "electricInput", cm, range.head.line) }
	  }
	}

	function copyableRanges(cm) {
	  var text = [], ranges = []
	  for (var i = 0; i < cm.doc.sel.ranges.length; i++) {
	    var line = cm.doc.sel.ranges[i].head.line
	    var lineRange = {anchor: Pos(line, 0), head: Pos(line + 1, 0)}
	    ranges.push(lineRange)
	    text.push(cm.getRange(lineRange.anchor, lineRange.head))
	  }
	  return {text: text, ranges: ranges}
	}

	function disableBrowserMagic(field, spellcheck) {
	  field.setAttribute("autocorrect", "off")
	  field.setAttribute("autocapitalize", "off")
	  field.setAttribute("spellcheck", !!spellcheck)
	}

	function hiddenTextarea() {
	  var te = elt("textarea", null, null, "position: absolute; bottom: -1em; padding: 0; width: 1px; height: 1em; outline: none")
	  var div = elt("div", [te], null, "overflow: hidden; position: relative; width: 3px; height: 0px;")
	  // The textarea is kept positioned near the cursor to prevent the
	  // fact that it'll be scrolled into view on input from scrolling
	  // our fake cursor out of view. On webkit, when wrap=off, paste is
	  // very slow. So make the area wide instead.
	  if (webkit) { te.style.width = "1000px" }
	  else { te.setAttribute("wrap", "off") }
	  // If border: 0; -- iOS fails to open keyboard (issue #1287)
	  if (ios) { te.style.border = "1px solid black" }
	  disableBrowserMagic(te)
	  return div
	}

	// The publicly visible API. Note that methodOp(f) means
	// 'wrap f in an operation, performed on its `this` parameter'.

	// This is not the complete set of editor methods. Most of the
	// methods defined on the Doc type are also injected into
	// CodeMirror.prototype, for backwards compatibility and
	// convenience.

	function addEditorMethods(CodeMirror) {
	  var optionHandlers = CodeMirror.optionHandlers

	  var helpers = CodeMirror.helpers = {}

	  CodeMirror.prototype = {
	    constructor: CodeMirror,
	    focus: function(){window.focus(); this.display.input.focus()},

	    setOption: function(option, value) {
	      var options = this.options, old = options[option]
	      if (options[option] == value && option != "mode") { return }
	      options[option] = value
	      if (optionHandlers.hasOwnProperty(option))
	        { operation(this, optionHandlers[option])(this, value, old) }
	      signal(this, "optionChange", this, option)
	    },

	    getOption: function(option) {return this.options[option]},
	    getDoc: function() {return this.doc},

	    addKeyMap: function(map, bottom) {
	      this.state.keyMaps[bottom ? "push" : "unshift"](getKeyMap(map))
	    },
	    removeKeyMap: function(map) {
	      var maps = this.state.keyMaps
	      for (var i = 0; i < maps.length; ++i)
	        { if (maps[i] == map || maps[i].name == map) {
	          maps.splice(i, 1)
	          return true
	        } }
	    },

	    addOverlay: methodOp(function(spec, options) {
	      var mode = spec.token ? spec : CodeMirror.getMode(this.options, spec)
	      if (mode.startState) { throw new Error("Overlays may not be stateful.") }
	      insertSorted(this.state.overlays,
	                   {mode: mode, modeSpec: spec, opaque: options && options.opaque,
	                    priority: (options && options.priority) || 0},
	                   function (overlay) { return overlay.priority; })
	      this.state.modeGen++
	      regChange(this)
	    }),
	    removeOverlay: methodOp(function(spec) {
	      var this$1 = this;

	      var overlays = this.state.overlays
	      for (var i = 0; i < overlays.length; ++i) {
	        var cur = overlays[i].modeSpec
	        if (cur == spec || typeof spec == "string" && cur.name == spec) {
	          overlays.splice(i, 1)
	          this$1.state.modeGen++
	          regChange(this$1)
	          return
	        }
	      }
	    }),

	    indentLine: methodOp(function(n, dir, aggressive) {
	      if (typeof dir != "string" && typeof dir != "number") {
	        if (dir == null) { dir = this.options.smartIndent ? "smart" : "prev" }
	        else { dir = dir ? "add" : "subtract" }
	      }
	      if (isLine(this.doc, n)) { indentLine(this, n, dir, aggressive) }
	    }),
	    indentSelection: methodOp(function(how) {
	      var this$1 = this;

	      var ranges = this.doc.sel.ranges, end = -1
	      for (var i = 0; i < ranges.length; i++) {
	        var range = ranges[i]
	        if (!range.empty()) {
	          var from = range.from(), to = range.to()
	          var start = Math.max(end, from.line)
	          end = Math.min(this$1.lastLine(), to.line - (to.ch ? 0 : 1)) + 1
	          for (var j = start; j < end; ++j)
	            { indentLine(this$1, j, how) }
	          var newRanges = this$1.doc.sel.ranges
	          if (from.ch == 0 && ranges.length == newRanges.length && newRanges[i].from().ch > 0)
	            { replaceOneSelection(this$1.doc, i, new Range(from, newRanges[i].to()), sel_dontScroll) }
	        } else if (range.head.line > end) {
	          indentLine(this$1, range.head.line, how, true)
	          end = range.head.line
	          if (i == this$1.doc.sel.primIndex) { ensureCursorVisible(this$1) }
	        }
	      }
	    }),

	    // Fetch the parser token for a given character. Useful for hacks
	    // that want to inspect the mode state (say, for completion).
	    getTokenAt: function(pos, precise) {
	      return takeToken(this, pos, precise)
	    },

	    getLineTokens: function(line, precise) {
	      return takeToken(this, Pos(line), precise, true)
	    },

	    getTokenTypeAt: function(pos) {
	      pos = clipPos(this.doc, pos)
	      var styles = getLineStyles(this, getLine(this.doc, pos.line))
	      var before = 0, after = (styles.length - 1) / 2, ch = pos.ch
	      var type
	      if (ch == 0) { type = styles[2] }
	      else { for (;;) {
	        var mid = (before + after) >> 1
	        if ((mid ? styles[mid * 2 - 1] : 0) >= ch) { after = mid }
	        else if (styles[mid * 2 + 1] < ch) { before = mid + 1 }
	        else { type = styles[mid * 2 + 2]; break }
	      } }
	      var cut = type ? type.indexOf("overlay ") : -1
	      return cut < 0 ? type : cut == 0 ? null : type.slice(0, cut - 1)
	    },

	    getModeAt: function(pos) {
	      var mode = this.doc.mode
	      if (!mode.innerMode) { return mode }
	      return CodeMirror.innerMode(mode, this.getTokenAt(pos).state).mode
	    },

	    getHelper: function(pos, type) {
	      return this.getHelpers(pos, type)[0]
	    },

	    getHelpers: function(pos, type) {
	      var this$1 = this;

	      var found = []
	      if (!helpers.hasOwnProperty(type)) { return found }
	      var help = helpers[type], mode = this.getModeAt(pos)
	      if (typeof mode[type] == "string") {
	        if (help[mode[type]]) { found.push(help[mode[type]]) }
	      } else if (mode[type]) {
	        for (var i = 0; i < mode[type].length; i++) {
	          var val = help[mode[type][i]]
	          if (val) { found.push(val) }
	        }
	      } else if (mode.helperType && help[mode.helperType]) {
	        found.push(help[mode.helperType])
	      } else if (help[mode.name]) {
	        found.push(help[mode.name])
	      }
	      for (var i$1 = 0; i$1 < help._global.length; i$1++) {
	        var cur = help._global[i$1]
	        if (cur.pred(mode, this$1) && indexOf(found, cur.val) == -1)
	          { found.push(cur.val) }
	      }
	      return found
	    },

	    getStateAfter: function(line, precise) {
	      var doc = this.doc
	      line = clipLine(doc, line == null ? doc.first + doc.size - 1: line)
	      return getContextBefore(this, line + 1, precise).state
	    },

	    cursorCoords: function(start, mode) {
	      var pos, range = this.doc.sel.primary()
	      if (start == null) { pos = range.head }
	      else if (typeof start == "object") { pos = clipPos(this.doc, start) }
	      else { pos = start ? range.from() : range.to() }
	      return cursorCoords(this, pos, mode || "page")
	    },

	    charCoords: function(pos, mode) {
	      return charCoords(this, clipPos(this.doc, pos), mode || "page")
	    },

	    coordsChar: function(coords, mode) {
	      coords = fromCoordSystem(this, coords, mode || "page")
	      return coordsChar(this, coords.left, coords.top)
	    },

	    lineAtHeight: function(height, mode) {
	      height = fromCoordSystem(this, {top: height, left: 0}, mode || "page").top
	      return lineAtHeight(this.doc, height + this.display.viewOffset)
	    },
	    heightAtLine: function(line, mode, includeWidgets) {
	      var end = false, lineObj
	      if (typeof line == "number") {
	        var last = this.doc.first + this.doc.size - 1
	        if (line < this.doc.first) { line = this.doc.first }
	        else if (line > last) { line = last; end = true }
	        lineObj = getLine(this.doc, line)
	      } else {
	        lineObj = line
	      }
	      return intoCoordSystem(this, lineObj, {top: 0, left: 0}, mode || "page", includeWidgets || end).top +
	        (end ? this.doc.height - heightAtLine(lineObj) : 0)
	    },

	    defaultTextHeight: function() { return textHeight(this.display) },
	    defaultCharWidth: function() { return charWidth(this.display) },

	    getViewport: function() { return {from: this.display.viewFrom, to: this.display.viewTo}},

	    addWidget: function(pos, node, scroll, vert, horiz) {
	      var display = this.display
	      pos = cursorCoords(this, clipPos(this.doc, pos))
	      var top = pos.bottom, left = pos.left
	      node.style.position = "absolute"
	      node.setAttribute("cm-ignore-events", "true")
	      this.display.input.setUneditable(node)
	      display.sizer.appendChild(node)
	      if (vert == "over") {
	        top = pos.top
	      } else if (vert == "above" || vert == "near") {
	        var vspace = Math.max(display.wrapper.clientHeight, this.doc.height),
	        hspace = Math.max(display.sizer.clientWidth, display.lineSpace.clientWidth)
	        // Default to positioning above (if specified and possible); otherwise default to positioning below
	        if ((vert == 'above' || pos.bottom + node.offsetHeight > vspace) && pos.top > node.offsetHeight)
	          { top = pos.top - node.offsetHeight }
	        else if (pos.bottom + node.offsetHeight <= vspace)
	          { top = pos.bottom }
	        if (left + node.offsetWidth > hspace)
	          { left = hspace - node.offsetWidth }
	      }
	      node.style.top = top + "px"
	      node.style.left = node.style.right = ""
	      if (horiz == "right") {
	        left = display.sizer.clientWidth - node.offsetWidth
	        node.style.right = "0px"
	      } else {
	        if (horiz == "left") { left = 0 }
	        else if (horiz == "middle") { left = (display.sizer.clientWidth - node.offsetWidth) / 2 }
	        node.style.left = left + "px"
	      }
	      if (scroll)
	        { scrollIntoView(this, {left: left, top: top, right: left + node.offsetWidth, bottom: top + node.offsetHeight}) }
	    },

	    triggerOnKeyDown: methodOp(onKeyDown),
	    triggerOnKeyPress: methodOp(onKeyPress),
	    triggerOnKeyUp: onKeyUp,
	    triggerOnMouseDown: methodOp(onMouseDown),

	    execCommand: function(cmd) {
	      if (commands.hasOwnProperty(cmd))
	        { return commands[cmd].call(null, this) }
	    },

	    triggerElectric: methodOp(function(text) { triggerElectric(this, text) }),

	    findPosH: function(from, amount, unit, visually) {
	      var this$1 = this;

	      var dir = 1
	      if (amount < 0) { dir = -1; amount = -amount }
	      var cur = clipPos(this.doc, from)
	      for (var i = 0; i < amount; ++i) {
	        cur = findPosH(this$1.doc, cur, dir, unit, visually)
	        if (cur.hitSide) { break }
	      }
	      return cur
	    },

	    moveH: methodOp(function(dir, unit) {
	      var this$1 = this;

	      this.extendSelectionsBy(function (range) {
	        if (this$1.display.shift || this$1.doc.extend || range.empty())
	          { return findPosH(this$1.doc, range.head, dir, unit, this$1.options.rtlMoveVisually) }
	        else
	          { return dir < 0 ? range.from() : range.to() }
	      }, sel_move)
	    }),

	    deleteH: methodOp(function(dir, unit) {
	      var sel = this.doc.sel, doc = this.doc
	      if (sel.somethingSelected())
	        { doc.replaceSelection("", null, "+delete") }
	      else
	        { deleteNearSelection(this, function (range) {
	          var other = findPosH(doc, range.head, dir, unit, false)
	          return dir < 0 ? {from: other, to: range.head} : {from: range.head, to: other}
	        }) }
	    }),

	    findPosV: function(from, amount, unit, goalColumn) {
	      var this$1 = this;

	      var dir = 1, x = goalColumn
	      if (amount < 0) { dir = -1; amount = -amount }
	      var cur = clipPos(this.doc, from)
	      for (var i = 0; i < amount; ++i) {
	        var coords = cursorCoords(this$1, cur, "div")
	        if (x == null) { x = coords.left }
	        else { coords.left = x }
	        cur = findPosV(this$1, coords, dir, unit)
	        if (cur.hitSide) { break }
	      }
	      return cur
	    },

	    moveV: methodOp(function(dir, unit) {
	      var this$1 = this;

	      var doc = this.doc, goals = []
	      var collapse = !this.display.shift && !doc.extend && doc.sel.somethingSelected()
	      doc.extendSelectionsBy(function (range) {
	        if (collapse)
	          { return dir < 0 ? range.from() : range.to() }
	        var headPos = cursorCoords(this$1, range.head, "div")
	        if (range.goalColumn != null) { headPos.left = range.goalColumn }
	        goals.push(headPos.left)
	        var pos = findPosV(this$1, headPos, dir, unit)
	        if (unit == "page" && range == doc.sel.primary())
	          { addToScrollTop(this$1, charCoords(this$1, pos, "div").top - headPos.top) }
	        return pos
	      }, sel_move)
	      if (goals.length) { for (var i = 0; i < doc.sel.ranges.length; i++)
	        { doc.sel.ranges[i].goalColumn = goals[i] } }
	    }),

	    // Find the word at the given position (as returned by coordsChar).
	    findWordAt: function(pos) {
	      var doc = this.doc, line = getLine(doc, pos.line).text
	      var start = pos.ch, end = pos.ch
	      if (line) {
	        var helper = this.getHelper(pos, "wordChars")
	        if ((pos.sticky == "before" || end == line.length) && start) { --start; } else { ++end }
	        var startChar = line.charAt(start)
	        var check = isWordChar(startChar, helper)
	          ? function (ch) { return isWordChar(ch, helper); }
	          : /\s/.test(startChar) ? function (ch) { return /\s/.test(ch); }
	          : function (ch) { return (!/\s/.test(ch) && !isWordChar(ch)); }
	        while (start > 0 && check(line.charAt(start - 1))) { --start }
	        while (end < line.length && check(line.charAt(end))) { ++end }
	      }
	      return new Range(Pos(pos.line, start), Pos(pos.line, end))
	    },

	    toggleOverwrite: function(value) {
	      if (value != null && value == this.state.overwrite) { return }
	      if (this.state.overwrite = !this.state.overwrite)
	        { addClass(this.display.cursorDiv, "CodeMirror-overwrite") }
	      else
	        { rmClass(this.display.cursorDiv, "CodeMirror-overwrite") }

	      signal(this, "overwriteToggle", this, this.state.overwrite)
	    },
	    hasFocus: function() { return this.display.input.getField() == activeElt() },
	    isReadOnly: function() { return !!(this.options.readOnly || this.doc.cantEdit) },

	    scrollTo: methodOp(function (x, y) { scrollToCoords(this, x, y) }),
	    getScrollInfo: function() {
	      var scroller = this.display.scroller
	      return {left: scroller.scrollLeft, top: scroller.scrollTop,
	              height: scroller.scrollHeight - scrollGap(this) - this.display.barHeight,
	              width: scroller.scrollWidth - scrollGap(this) - this.display.barWidth,
	              clientHeight: displayHeight(this), clientWidth: displayWidth(this)}
	    },

	    scrollIntoView: methodOp(function(range, margin) {
	      if (range == null) {
	        range = {from: this.doc.sel.primary().head, to: null}
	        if (margin == null) { margin = this.options.cursorScrollMargin }
	      } else if (typeof range == "number") {
	        range = {from: Pos(range, 0), to: null}
	      } else if (range.from == null) {
	        range = {from: range, to: null}
	      }
	      if (!range.to) { range.to = range.from }
	      range.margin = margin || 0

	      if (range.from.line != null) {
	        scrollToRange(this, range)
	      } else {
	        scrollToCoordsRange(this, range.from, range.to, range.margin)
	      }
	    }),

	    setSize: methodOp(function(width, height) {
	      var this$1 = this;

	      var interpret = function (val) { return typeof val == "number" || /^\d+$/.test(String(val)) ? val + "px" : val; }
	      if (width != null) { this.display.wrapper.style.width = interpret(width) }
	      if (height != null) { this.display.wrapper.style.height = interpret(height) }
	      if (this.options.lineWrapping) { clearLineMeasurementCache(this) }
	      var lineNo = this.display.viewFrom
	      this.doc.iter(lineNo, this.display.viewTo, function (line) {
	        if (line.widgets) { for (var i = 0; i < line.widgets.length; i++)
	          { if (line.widgets[i].noHScroll) { regLineChange(this$1, lineNo, "widget"); break } } }
	        ++lineNo
	      })
	      this.curOp.forceUpdate = true
	      signal(this, "refresh", this)
	    }),

	    operation: function(f){return runInOp(this, f)},
	    startOperation: function(){return startOperation(this)},
	    endOperation: function(){return endOperation(this)},

	    refresh: methodOp(function() {
	      var oldHeight = this.display.cachedTextHeight
	      regChange(this)
	      this.curOp.forceUpdate = true
	      clearCaches(this)
	      scrollToCoords(this, this.doc.scrollLeft, this.doc.scrollTop)
	      updateGutterSpace(this)
	      if (oldHeight == null || Math.abs(oldHeight - textHeight(this.display)) > .5)
	        { estimateLineHeights(this) }
	      signal(this, "refresh", this)
	    }),

	    swapDoc: methodOp(function(doc) {
	      var old = this.doc
	      old.cm = null
	      attachDoc(this, doc)
	      clearCaches(this)
	      this.display.input.reset()
	      scrollToCoords(this, doc.scrollLeft, doc.scrollTop)
	      this.curOp.forceScroll = true
	      signalLater(this, "swapDoc", this, old)
	      return old
	    }),

	    getInputField: function(){return this.display.input.getField()},
	    getWrapperElement: function(){return this.display.wrapper},
	    getScrollerElement: function(){return this.display.scroller},
	    getGutterElement: function(){return this.display.gutters}
	  }
	  eventMixin(CodeMirror)

	  CodeMirror.registerHelper = function(type, name, value) {
	    if (!helpers.hasOwnProperty(type)) { helpers[type] = CodeMirror[type] = {_global: []} }
	    helpers[type][name] = value
	  }
	  CodeMirror.registerGlobalHelper = function(type, name, predicate, value) {
	    CodeMirror.registerHelper(type, name, value)
	    helpers[type]._global.push({pred: predicate, val: value})
	  }
	}

	// Used for horizontal relative motion. Dir is -1 or 1 (left or
	// right), unit can be "char", "column" (like char, but doesn't
	// cross line boundaries), "word" (across next word), or "group" (to
	// the start of next group of word or non-word-non-whitespace
	// chars). The visually param controls whether, in right-to-left
	// text, direction 1 means to move towards the next index in the
	// string, or towards the character to the right of the current
	// position. The resulting position will have a hitSide=true
	// property if it reached the end of the document.
	function findPosH(doc, pos, dir, unit, visually) {
	  var oldPos = pos
	  var origDir = dir
	  var lineObj = getLine(doc, pos.line)
	  function findNextLine() {
	    var l = pos.line + dir
	    if (l < doc.first || l >= doc.first + doc.size) { return false }
	    pos = new Pos(l, pos.ch, pos.sticky)
	    return lineObj = getLine(doc, l)
	  }
	  function moveOnce(boundToLine) {
	    var next
	    if (visually) {
	      next = moveVisually(doc.cm, lineObj, pos, dir)
	    } else {
	      next = moveLogically(lineObj, pos, dir)
	    }
	    if (next == null) {
	      if (!boundToLine && findNextLine())
	        { pos = endOfLine(visually, doc.cm, lineObj, pos.line, dir) }
	      else
	        { return false }
	    } else {
	      pos = next
	    }
	    return true
	  }

	  if (unit == "char") {
	    moveOnce()
	  } else if (unit == "column") {
	    moveOnce(true)
	  } else if (unit == "word" || unit == "group") {
	    var sawType = null, group = unit == "group"
	    var helper = doc.cm && doc.cm.getHelper(pos, "wordChars")
	    for (var first = true;; first = false) {
	      if (dir < 0 && !moveOnce(!first)) { break }
	      var cur = lineObj.text.charAt(pos.ch) || "\n"
	      var type = isWordChar(cur, helper) ? "w"
	        : group && cur == "\n" ? "n"
	        : !group || /\s/.test(cur) ? null
	        : "p"
	      if (group && !first && !type) { type = "s" }
	      if (sawType && sawType != type) {
	        if (dir < 0) {dir = 1; moveOnce(); pos.sticky = "after"}
	        break
	      }

	      if (type) { sawType = type }
	      if (dir > 0 && !moveOnce(!first)) { break }
	    }
	  }
	  var result = skipAtomic(doc, pos, oldPos, origDir, true)
	  if (equalCursorPos(oldPos, result)) { result.hitSide = true }
	  return result
	}

	// For relative vertical movement. Dir may be -1 or 1. Unit can be
	// "page" or "line". The resulting position will have a hitSide=true
	// property if it reached the end of the document.
	function findPosV(cm, pos, dir, unit) {
	  var doc = cm.doc, x = pos.left, y
	  if (unit == "page") {
	    var pageSize = Math.min(cm.display.wrapper.clientHeight, window.innerHeight || document.documentElement.clientHeight)
	    var moveAmount = Math.max(pageSize - .5 * textHeight(cm.display), 3)
	    y = (dir > 0 ? pos.bottom : pos.top) + dir * moveAmount

	  } else if (unit == "line") {
	    y = dir > 0 ? pos.bottom + 3 : pos.top - 3
	  }
	  var target
	  for (;;) {
	    target = coordsChar(cm, x, y)
	    if (!target.outside) { break }
	    if (dir < 0 ? y <= 0 : y >= doc.height) { target.hitSide = true; break }
	    y += dir * 5
	  }
	  return target
	}

	// CONTENTEDITABLE INPUT STYLE

	var ContentEditableInput = function(cm) {
	  this.cm = cm
	  this.lastAnchorNode = this.lastAnchorOffset = this.lastFocusNode = this.lastFocusOffset = null
	  this.polling = new Delayed()
	  this.composing = null
	  this.gracePeriod = false
	  this.readDOMTimeout = null
	};

	ContentEditableInput.prototype.init = function (display) {
	    var this$1 = this;

	  var input = this, cm = input.cm
	  var div = input.div = display.lineDiv
	  disableBrowserMagic(div, cm.options.spellcheck)

	  on(div, "paste", function (e) {
	    if (signalDOMEvent(cm, e) || handlePaste(e, cm)) { return }
	    // IE doesn't fire input events, so we schedule a read for the pasted content in this way
	    if (ie_version <= 11) { setTimeout(operation(cm, function () { return this$1.updateFromDOM(); }), 20) }
	  })

	  on(div, "compositionstart", function (e) {
	    this$1.composing = {data: e.data, done: false}
	  })
	  on(div, "compositionupdate", function (e) {
	    if (!this$1.composing) { this$1.composing = {data: e.data, done: false} }
	  })
	  on(div, "compositionend", function (e) {
	    if (this$1.composing) {
	      if (e.data != this$1.composing.data) { this$1.readFromDOMSoon() }
	      this$1.composing.done = true
	    }
	  })

	  on(div, "touchstart", function () { return input.forceCompositionEnd(); })

	  on(div, "input", function () {
	    if (!this$1.composing) { this$1.readFromDOMSoon() }
	  })

	  function onCopyCut(e) {
	    if (signalDOMEvent(cm, e)) { return }
	    if (cm.somethingSelected()) {
	      setLastCopied({lineWise: false, text: cm.getSelections()})
	      if (e.type == "cut") { cm.replaceSelection("", null, "cut") }
	    } else if (!cm.options.lineWiseCopyCut) {
	      return
	    } else {
	      var ranges = copyableRanges(cm)
	      setLastCopied({lineWise: true, text: ranges.text})
	      if (e.type == "cut") {
	        cm.operation(function () {
	          cm.setSelections(ranges.ranges, 0, sel_dontScroll)
	          cm.replaceSelection("", null, "cut")
	        })
	      }
	    }
	    if (e.clipboardData) {
	      e.clipboardData.clearData()
	      var content = lastCopied.text.join("\n")
	      // iOS exposes the clipboard API, but seems to discard content inserted into it
	      e.clipboardData.setData("Text", content)
	      if (e.clipboardData.getData("Text") == content) {
	        e.preventDefault()
	        return
	      }
	    }
	    // Old-fashioned briefly-focus-a-textarea hack
	    var kludge = hiddenTextarea(), te = kludge.firstChild
	    cm.display.lineSpace.insertBefore(kludge, cm.display.lineSpace.firstChild)
	    te.value = lastCopied.text.join("\n")
	    var hadFocus = document.activeElement
	    selectInput(te)
	    setTimeout(function () {
	      cm.display.lineSpace.removeChild(kludge)
	      hadFocus.focus()
	      if (hadFocus == div) { input.showPrimarySelection() }
	    }, 50)
	  }
	  on(div, "copy", onCopyCut)
	  on(div, "cut", onCopyCut)
	};

	ContentEditableInput.prototype.prepareSelection = function () {
	  var result = prepareSelection(this.cm, false)
	  result.focus = this.cm.state.focused
	  return result
	};

	ContentEditableInput.prototype.showSelection = function (info, takeFocus) {
	  if (!info || !this.cm.display.view.length) { return }
	  if (info.focus || takeFocus) { this.showPrimarySelection() }
	  this.showMultipleSelections(info)
	};

	ContentEditableInput.prototype.showPrimarySelection = function () {
	  var sel = window.getSelection(), cm = this.cm, prim = cm.doc.sel.primary()
	  var from = prim.from(), to = prim.to()

	  if (cm.display.viewTo == cm.display.viewFrom || from.line >= cm.display.viewTo || to.line < cm.display.viewFrom) {
	    sel.removeAllRanges()
	    return
	  }

	  var curAnchor = domToPos(cm, sel.anchorNode, sel.anchorOffset)
	  var curFocus = domToPos(cm, sel.focusNode, sel.focusOffset)
	  if (curAnchor && !curAnchor.bad && curFocus && !curFocus.bad &&
	      cmp(minPos(curAnchor, curFocus), from) == 0 &&
	      cmp(maxPos(curAnchor, curFocus), to) == 0)
	    { return }

	  var view = cm.display.view
	  var start = (from.line >= cm.display.viewFrom && posToDOM(cm, from)) ||
	      {node: view[0].measure.map[2], offset: 0}
	  var end = to.line < cm.display.viewTo && posToDOM(cm, to)
	  if (!end) {
	    var measure = view[view.length - 1].measure
	    var map = measure.maps ? measure.maps[measure.maps.length - 1] : measure.map
	    end = {node: map[map.length - 1], offset: map[map.length - 2] - map[map.length - 3]}
	  }

	  if (!start || !end) {
	    sel.removeAllRanges()
	    return
	  }

	  var old = sel.rangeCount && sel.getRangeAt(0), rng
	  try { rng = range(start.node, start.offset, end.offset, end.node) }
	  catch(e) {} // Our model of the DOM might be outdated, in which case the range we try to set can be impossible
	  if (rng) {
	    if (!gecko && cm.state.focused) {
	      sel.collapse(start.node, start.offset)
	      if (!rng.collapsed) {
	        sel.removeAllRanges()
	        sel.addRange(rng)
	      }
	    } else {
	      sel.removeAllRanges()
	      sel.addRange(rng)
	    }
	    if (old && sel.anchorNode == null) { sel.addRange(old) }
	    else if (gecko) { this.startGracePeriod() }
	  }
	  this.rememberSelection()
	};

	ContentEditableInput.prototype.startGracePeriod = function () {
	    var this$1 = this;

	  clearTimeout(this.gracePeriod)
	  this.gracePeriod = setTimeout(function () {
	    this$1.gracePeriod = false
	    if (this$1.selectionChanged())
	      { this$1.cm.operation(function () { return this$1.cm.curOp.selectionChanged = true; }) }
	  }, 20)
	};

	ContentEditableInput.prototype.showMultipleSelections = function (info) {
	  removeChildrenAndAdd(this.cm.display.cursorDiv, info.cursors)
	  removeChildrenAndAdd(this.cm.display.selectionDiv, info.selection)
	};

	ContentEditableInput.prototype.rememberSelection = function () {
	  var sel = window.getSelection()
	  this.lastAnchorNode = sel.anchorNode; this.lastAnchorOffset = sel.anchorOffset
	  this.lastFocusNode = sel.focusNode; this.lastFocusOffset = sel.focusOffset
	};

	ContentEditableInput.prototype.selectionInEditor = function () {
	  var sel = window.getSelection()
	  if (!sel.rangeCount) { return false }
	  var node = sel.getRangeAt(0).commonAncestorContainer
	  return contains(this.div, node)
	};

	ContentEditableInput.prototype.focus = function () {
	  if (this.cm.options.readOnly != "nocursor") {
	    if (!this.selectionInEditor())
	      { this.showSelection(this.prepareSelection(), true) }
	    this.div.focus()
	  }
	};
	ContentEditableInput.prototype.blur = function () { this.div.blur() };
	ContentEditableInput.prototype.getField = function () { return this.div };

	ContentEditableInput.prototype.supportsTouch = function () { return true };

	ContentEditableInput.prototype.receivedFocus = function () {
	  var input = this
	  if (this.selectionInEditor())
	    { this.pollSelection() }
	  else
	    { runInOp(this.cm, function () { return input.cm.curOp.selectionChanged = true; }) }

	  function poll() {
	    if (input.cm.state.focused) {
	      input.pollSelection()
	      input.polling.set(input.cm.options.pollInterval, poll)
	    }
	  }
	  this.polling.set(this.cm.options.pollInterval, poll)
	};

	ContentEditableInput.prototype.selectionChanged = function () {
	  var sel = window.getSelection()
	  return sel.anchorNode != this.lastAnchorNode || sel.anchorOffset != this.lastAnchorOffset ||
	    sel.focusNode != this.lastFocusNode || sel.focusOffset != this.lastFocusOffset
	};

	ContentEditableInput.prototype.pollSelection = function () {
	  if (this.readDOMTimeout != null || this.gracePeriod || !this.selectionChanged()) { return }
	  var sel = window.getSelection(), cm = this.cm
	  // On Android Chrome (version 56, at least), backspacing into an
	  // uneditable block element will put the cursor in that element,
	  // and then, because it's not editable, hide the virtual keyboard.
	  // Because Android doesn't allow us to actually detect backspace
	  // presses in a sane way, this code checks for when that happens
	  // and simulates a backspace press in this case.
	  if (android && chrome && this.cm.options.gutters.length && isInGutter(sel.anchorNode)) {
	    this.cm.triggerOnKeyDown({type: "keydown", keyCode: 8, preventDefault: Math.abs})
	    this.blur()
	    this.focus()
	    return
	  }
	  if (this.composing) { return }
	  this.rememberSelection()
	  var anchor = domToPos(cm, sel.anchorNode, sel.anchorOffset)
	  var head = domToPos(cm, sel.focusNode, sel.focusOffset)
	  if (anchor && head) { runInOp(cm, function () {
	    setSelection(cm.doc, simpleSelection(anchor, head), sel_dontScroll)
	    if (anchor.bad || head.bad) { cm.curOp.selectionChanged = true }
	  }) }
	};

	ContentEditableInput.prototype.pollContent = function () {
	  if (this.readDOMTimeout != null) {
	    clearTimeout(this.readDOMTimeout)
	    this.readDOMTimeout = null
	  }

	  var cm = this.cm, display = cm.display, sel = cm.doc.sel.primary()
	  var from = sel.from(), to = sel.to()
	  if (from.ch == 0 && from.line > cm.firstLine())
	    { from = Pos(from.line - 1, getLine(cm.doc, from.line - 1).length) }
	  if (to.ch == getLine(cm.doc, to.line).text.length && to.line < cm.lastLine())
	    { to = Pos(to.line + 1, 0) }
	  if (from.line < display.viewFrom || to.line > display.viewTo - 1) { return false }

	  var fromIndex, fromLine, fromNode
	  if (from.line == display.viewFrom || (fromIndex = findViewIndex(cm, from.line)) == 0) {
	    fromLine = lineNo(display.view[0].line)
	    fromNode = display.view[0].node
	  } else {
	    fromLine = lineNo(display.view[fromIndex].line)
	    fromNode = display.view[fromIndex - 1].node.nextSibling
	  }
	  var toIndex = findViewIndex(cm, to.line)
	  var toLine, toNode
	  if (toIndex == display.view.length - 1) {
	    toLine = display.viewTo - 1
	    toNode = display.lineDiv.lastChild
	  } else {
	    toLine = lineNo(display.view[toIndex + 1].line) - 1
	    toNode = display.view[toIndex + 1].node.previousSibling
	  }

	  if (!fromNode) { return false }
	  var newText = cm.doc.splitLines(domTextBetween(cm, fromNode, toNode, fromLine, toLine))
	  var oldText = getBetween(cm.doc, Pos(fromLine, 0), Pos(toLine, getLine(cm.doc, toLine).text.length))
	  while (newText.length > 1 && oldText.length > 1) {
	    if (lst(newText) == lst(oldText)) { newText.pop(); oldText.pop(); toLine-- }
	    else if (newText[0] == oldText[0]) { newText.shift(); oldText.shift(); fromLine++ }
	    else { break }
	  }

	  var cutFront = 0, cutEnd = 0
	  var newTop = newText[0], oldTop = oldText[0], maxCutFront = Math.min(newTop.length, oldTop.length)
	  while (cutFront < maxCutFront && newTop.charCodeAt(cutFront) == oldTop.charCodeAt(cutFront))
	    { ++cutFront }
	  var newBot = lst(newText), oldBot = lst(oldText)
	  var maxCutEnd = Math.min(newBot.length - (newText.length == 1 ? cutFront : 0),
	                           oldBot.length - (oldText.length == 1 ? cutFront : 0))
	  while (cutEnd < maxCutEnd &&
	         newBot.charCodeAt(newBot.length - cutEnd - 1) == oldBot.charCodeAt(oldBot.length - cutEnd - 1))
	    { ++cutEnd }
	  // Try to move start of change to start of selection if ambiguous
	  if (newText.length == 1 && oldText.length == 1 && fromLine == from.line) {
	    while (cutFront && cutFront > from.ch &&
	           newBot.charCodeAt(newBot.length - cutEnd - 1) == oldBot.charCodeAt(oldBot.length - cutEnd - 1)) {
	      cutFront--
	      cutEnd++
	    }
	  }

	  newText[newText.length - 1] = newBot.slice(0, newBot.length - cutEnd).replace(/^\u200b+/, "")
	  newText[0] = newText[0].slice(cutFront).replace(/\u200b+$/, "")

	  var chFrom = Pos(fromLine, cutFront)
	  var chTo = Pos(toLine, oldText.length ? lst(oldText).length - cutEnd : 0)
	  if (newText.length > 1 || newText[0] || cmp(chFrom, chTo)) {
	    replaceRange(cm.doc, newText, chFrom, chTo, "+input")
	    return true
	  }
	};

	ContentEditableInput.prototype.ensurePolled = function () {
	  this.forceCompositionEnd()
	};
	ContentEditableInput.prototype.reset = function () {
	  this.forceCompositionEnd()
	};
	ContentEditableInput.prototype.forceCompositionEnd = function () {
	  if (!this.composing) { return }
	  clearTimeout(this.readDOMTimeout)
	  this.composing = null
	  this.updateFromDOM()
	  this.div.blur()
	  this.div.focus()
	};
	ContentEditableInput.prototype.readFromDOMSoon = function () {
	    var this$1 = this;

	  if (this.readDOMTimeout != null) { return }
	  this.readDOMTimeout = setTimeout(function () {
	    this$1.readDOMTimeout = null
	    if (this$1.composing) {
	      if (this$1.composing.done) { this$1.composing = null }
	      else { return }
	    }
	    this$1.updateFromDOM()
	  }, 80)
	};

	ContentEditableInput.prototype.updateFromDOM = function () {
	    var this$1 = this;

	  if (this.cm.isReadOnly() || !this.pollContent())
	    { runInOp(this.cm, function () { return regChange(this$1.cm); }) }
	};

	ContentEditableInput.prototype.setUneditable = function (node) {
	  node.contentEditable = "false"
	};

	ContentEditableInput.prototype.onKeyPress = function (e) {
	  if (e.charCode == 0) { return }
	  e.preventDefault()
	  if (!this.cm.isReadOnly())
	    { operation(this.cm, applyTextInput)(this.cm, String.fromCharCode(e.charCode == null ? e.keyCode : e.charCode), 0) }
	};

	ContentEditableInput.prototype.readOnlyChanged = function (val) {
	  this.div.contentEditable = String(val != "nocursor")
	};

	ContentEditableInput.prototype.onContextMenu = function () {};
	ContentEditableInput.prototype.resetPosition = function () {};

	ContentEditableInput.prototype.needsContentAttribute = true

	function posToDOM(cm, pos) {
	  var view = findViewForLine(cm, pos.line)
	  if (!view || view.hidden) { return null }
	  var line = getLine(cm.doc, pos.line)
	  var info = mapFromLineView(view, line, pos.line)

	  var order = getOrder(line, cm.doc.direction), side = "left"
	  if (order) {
	    var partPos = getBidiPartAt(order, pos.ch)
	    side = partPos % 2 ? "right" : "left"
	  }
	  var result = nodeAndOffsetInLineMap(info.map, pos.ch, side)
	  result.offset = result.collapse == "right" ? result.end : result.start
	  return result
	}

	function isInGutter(node) {
	  for (var scan = node; scan; scan = scan.parentNode)
	    { if (/CodeMirror-gutter-wrapper/.test(scan.className)) { return true } }
	  return false
	}

	function badPos(pos, bad) { if (bad) { pos.bad = true; } return pos }

	function domTextBetween(cm, from, to, fromLine, toLine) {
	  var text = "", closing = false, lineSep = cm.doc.lineSeparator()
	  function recognizeMarker(id) { return function (marker) { return marker.id == id; } }
	  function close() {
	    if (closing) {
	      text += lineSep
	      closing = false
	    }
	  }
	  function addText(str) {
	    if (str) {
	      close()
	      text += str
	    }
	  }
	  function walk(node) {
	    if (node.nodeType == 1) {
	      var cmText = node.getAttribute("cm-text")
	      if (cmText != null) {
	        addText(cmText || node.textContent.replace(/\u200b/g, ""))
	        return
	      }
	      var markerID = node.getAttribute("cm-marker"), range
	      if (markerID) {
	        var found = cm.findMarks(Pos(fromLine, 0), Pos(toLine + 1, 0), recognizeMarker(+markerID))
	        if (found.length && (range = found[0].find(0)))
	          { addText(getBetween(cm.doc, range.from, range.to).join(lineSep)) }
	        return
	      }
	      if (node.getAttribute("contenteditable") == "false") { return }
	      var isBlock = /^(pre|div|p)$/i.test(node.nodeName)
	      if (isBlock) { close() }
	      for (var i = 0; i < node.childNodes.length; i++)
	        { walk(node.childNodes[i]) }
	      if (isBlock) { closing = true }
	    } else if (node.nodeType == 3) {
	      addText(node.nodeValue)
	    }
	  }
	  for (;;) {
	    walk(from)
	    if (from == to) { break }
	    from = from.nextSibling
	  }
	  return text
	}

	function domToPos(cm, node, offset) {
	  var lineNode
	  if (node == cm.display.lineDiv) {
	    lineNode = cm.display.lineDiv.childNodes[offset]
	    if (!lineNode) { return badPos(cm.clipPos(Pos(cm.display.viewTo - 1)), true) }
	    node = null; offset = 0
	  } else {
	    for (lineNode = node;; lineNode = lineNode.parentNode) {
	      if (!lineNode || lineNode == cm.display.lineDiv) { return null }
	      if (lineNode.parentNode && lineNode.parentNode == cm.display.lineDiv) { break }
	    }
	  }
	  for (var i = 0; i < cm.display.view.length; i++) {
	    var lineView = cm.display.view[i]
	    if (lineView.node == lineNode)
	      { return locateNodeInLineView(lineView, node, offset) }
	  }
	}

	function locateNodeInLineView(lineView, node, offset) {
	  var wrapper = lineView.text.firstChild, bad = false
	  if (!node || !contains(wrapper, node)) { return badPos(Pos(lineNo(lineView.line), 0), true) }
	  if (node == wrapper) {
	    bad = true
	    node = wrapper.childNodes[offset]
	    offset = 0
	    if (!node) {
	      var line = lineView.rest ? lst(lineView.rest) : lineView.line
	      return badPos(Pos(lineNo(line), line.text.length), bad)
	    }
	  }

	  var textNode = node.nodeType == 3 ? node : null, topNode = node
	  if (!textNode && node.childNodes.length == 1 && node.firstChild.nodeType == 3) {
	    textNode = node.firstChild
	    if (offset) { offset = textNode.nodeValue.length }
	  }
	  while (topNode.parentNode != wrapper) { topNode = topNode.parentNode }
	  var measure = lineView.measure, maps = measure.maps

	  function find(textNode, topNode, offset) {
	    for (var i = -1; i < (maps ? maps.length : 0); i++) {
	      var map = i < 0 ? measure.map : maps[i]
	      for (var j = 0; j < map.length; j += 3) {
	        var curNode = map[j + 2]
	        if (curNode == textNode || curNode == topNode) {
	          var line = lineNo(i < 0 ? lineView.line : lineView.rest[i])
	          var ch = map[j] + offset
	          if (offset < 0 || curNode != textNode) { ch = map[j + (offset ? 1 : 0)] }
	          return Pos(line, ch)
	        }
	      }
	    }
	  }
	  var found = find(textNode, topNode, offset)
	  if (found) { return badPos(found, bad) }

	  // FIXME this is all really shaky. might handle the few cases it needs to handle, but likely to cause problems
	  for (var after = topNode.nextSibling, dist = textNode ? textNode.nodeValue.length - offset : 0; after; after = after.nextSibling) {
	    found = find(after, after.firstChild, 0)
	    if (found)
	      { return badPos(Pos(found.line, found.ch - dist), bad) }
	    else
	      { dist += after.textContent.length }
	  }
	  for (var before = topNode.previousSibling, dist$1 = offset; before; before = before.previousSibling) {
	    found = find(before, before.firstChild, -1)
	    if (found)
	      { return badPos(Pos(found.line, found.ch + dist$1), bad) }
	    else
	      { dist$1 += before.textContent.length }
	  }
	}

	// TEXTAREA INPUT STYLE

	var TextareaInput = function(cm) {
	  this.cm = cm
	  // See input.poll and input.reset
	  this.prevInput = ""

	  // Flag that indicates whether we expect input to appear real soon
	  // now (after some event like 'keypress' or 'input') and are
	  // polling intensively.
	  this.pollingFast = false
	  // Self-resetting timeout for the poller
	  this.polling = new Delayed()
	  // Used to work around IE issue with selection being forgotten when focus moves away from textarea
	  this.hasSelection = false
	  this.composing = null
	};

	TextareaInput.prototype.init = function (display) {
	    var this$1 = this;

	  var input = this, cm = this.cm

	  // Wraps and hides input textarea
	  var div = this.wrapper = hiddenTextarea()
	  // The semihidden textarea that is focused when the editor is
	  // focused, and receives input.
	  var te = this.textarea = div.firstChild
	  display.wrapper.insertBefore(div, display.wrapper.firstChild)

	  // Needed to hide big blue blinking cursor on Mobile Safari (doesn't seem to work in iOS 8 anymore)
	  if (ios) { te.style.width = "0px" }

	  on(te, "input", function () {
	    if (ie && ie_version >= 9 && this$1.hasSelection) { this$1.hasSelection = null }
	    input.poll()
	  })

	  on(te, "paste", function (e) {
	    if (signalDOMEvent(cm, e) || handlePaste(e, cm)) { return }

	    cm.state.pasteIncoming = true
	    input.fastPoll()
	  })

	  function prepareCopyCut(e) {
	    if (signalDOMEvent(cm, e)) { return }
	    if (cm.somethingSelected()) {
	      setLastCopied({lineWise: false, text: cm.getSelections()})
	    } else if (!cm.options.lineWiseCopyCut) {
	      return
	    } else {
	      var ranges = copyableRanges(cm)
	      setLastCopied({lineWise: true, text: ranges.text})
	      if (e.type == "cut") {
	        cm.setSelections(ranges.ranges, null, sel_dontScroll)
	      } else {
	        input.prevInput = ""
	        te.value = ranges.text.join("\n")
	        selectInput(te)
	      }
	    }
	    if (e.type == "cut") { cm.state.cutIncoming = true }
	  }
	  on(te, "cut", prepareCopyCut)
	  on(te, "copy", prepareCopyCut)

	  on(display.scroller, "paste", function (e) {
	    if (eventInWidget(display, e) || signalDOMEvent(cm, e)) { return }
	    cm.state.pasteIncoming = true
	    input.focus()
	  })

	  // Prevent normal selection in the editor (we handle our own)
	  on(display.lineSpace, "selectstart", function (e) {
	    if (!eventInWidget(display, e)) { e_preventDefault(e) }
	  })

	  on(te, "compositionstart", function () {
	    var start = cm.getCursor("from")
	    if (input.composing) { input.composing.range.clear() }
	    input.composing = {
	      start: start,
	      range: cm.markText(start, cm.getCursor("to"), {className: "CodeMirror-composing"})
	    }
	  })
	  on(te, "compositionend", function () {
	    if (input.composing) {
	      input.poll()
	      input.composing.range.clear()
	      input.composing = null
	    }
	  })
	};

	TextareaInput.prototype.prepareSelection = function () {
	  // Redraw the selection and/or cursor
	  var cm = this.cm, display = cm.display, doc = cm.doc
	  var result = prepareSelection(cm)

	  // Move the hidden textarea near the cursor to prevent scrolling artifacts
	  if (cm.options.moveInputWithCursor) {
	    var headPos = cursorCoords(cm, doc.sel.primary().head, "div")
	    var wrapOff = display.wrapper.getBoundingClientRect(), lineOff = display.lineDiv.getBoundingClientRect()
	    result.teTop = Math.max(0, Math.min(display.wrapper.clientHeight - 10,
	                                        headPos.top + lineOff.top - wrapOff.top))
	    result.teLeft = Math.max(0, Math.min(display.wrapper.clientWidth - 10,
	                                         headPos.left + lineOff.left - wrapOff.left))
	  }

	  return result
	};

	TextareaInput.prototype.showSelection = function (drawn) {
	  var cm = this.cm, display = cm.display
	  removeChildrenAndAdd(display.cursorDiv, drawn.cursors)
	  removeChildrenAndAdd(display.selectionDiv, drawn.selection)
	  if (drawn.teTop != null) {
	    this.wrapper.style.top = drawn.teTop + "px"
	    this.wrapper.style.left = drawn.teLeft + "px"
	  }
	};

	// Reset the input to correspond to the selection (or to be empty,
	// when not typing and nothing is selected)
	TextareaInput.prototype.reset = function (typing) {
	  if (this.contextMenuPending || this.composing) { return }
	  var cm = this.cm
	  if (cm.somethingSelected()) {
	    this.prevInput = ""
	    var content = cm.getSelection()
	    this.textarea.value = content
	    if (cm.state.focused) { selectInput(this.textarea) }
	    if (ie && ie_version >= 9) { this.hasSelection = content }
	  } else if (!typing) {
	    this.prevInput = this.textarea.value = ""
	    if (ie && ie_version >= 9) { this.hasSelection = null }
	  }
	};

	TextareaInput.prototype.getField = function () { return this.textarea };

	TextareaInput.prototype.supportsTouch = function () { return false };

	TextareaInput.prototype.focus = function () {
	  if (this.cm.options.readOnly != "nocursor" && (!mobile || activeElt() != this.textarea)) {
	    try { this.textarea.focus() }
	    catch (e) {} // IE8 will throw if the textarea is display: none or not in DOM
	  }
	};

	TextareaInput.prototype.blur = function () { this.textarea.blur() };

	TextareaInput.prototype.resetPosition = function () {
	  this.wrapper.style.top = this.wrapper.style.left = 0
	};

	TextareaInput.prototype.receivedFocus = function () { this.slowPoll() };

	// Poll for input changes, using the normal rate of polling. This
	// runs as long as the editor is focused.
	TextareaInput.prototype.slowPoll = function () {
	    var this$1 = this;

	  if (this.pollingFast) { return }
	  this.polling.set(this.cm.options.pollInterval, function () {
	    this$1.poll()
	    if (this$1.cm.state.focused) { this$1.slowPoll() }
	  })
	};

	// When an event has just come in that is likely to add or change
	// something in the input textarea, we poll faster, to ensure that
	// the change appears on the screen quickly.
	TextareaInput.prototype.fastPoll = function () {
	  var missed = false, input = this
	  input.pollingFast = true
	  function p() {
	    var changed = input.poll()
	    if (!changed && !missed) {missed = true; input.polling.set(60, p)}
	    else {input.pollingFast = false; input.slowPoll()}
	  }
	  input.polling.set(20, p)
	};

	// Read input from the textarea, and update the document to match.
	// When something is selected, it is present in the textarea, and
	// selected (unless it is huge, in which case a placeholder is
	// used). When nothing is selected, the cursor sits after previously
	// seen text (can be empty), which is stored in prevInput (we must
	// not reset the textarea when typing, because that breaks IME).
	TextareaInput.prototype.poll = function () {
	    var this$1 = this;

	  var cm = this.cm, input = this.textarea, prevInput = this.prevInput
	  // Since this is called a *lot*, try to bail out as cheaply as
	  // possible when it is clear that nothing happened. hasSelection
	  // will be the case when there is a lot of text in the textarea,
	  // in which case reading its value would be expensive.
	  if (this.contextMenuPending || !cm.state.focused ||
	      (hasSelection(input) && !prevInput && !this.composing) ||
	      cm.isReadOnly() || cm.options.disableInput || cm.state.keySeq)
	    { return false }

	  var text = input.value
	  // If nothing changed, bail.
	  if (text == prevInput && !cm.somethingSelected()) { return false }
	  // Work around nonsensical selection resetting in IE9/10, and
	  // inexplicable appearance of private area unicode characters on
	  // some key combos in Mac (#2689).
	  if (ie && ie_version >= 9 && this.hasSelection === text ||
	      mac && /[\uf700-\uf7ff]/.test(text)) {
	    cm.display.input.reset()
	    return false
	  }

	  if (cm.doc.sel == cm.display.selForContextMenu) {
	    var first = text.charCodeAt(0)
	    if (first == 0x200b && !prevInput) { prevInput = "\u200b" }
	    if (first == 0x21da) { this.reset(); return this.cm.execCommand("undo") }
	  }
	  // Find the part of the input that is actually new
	  var same = 0, l = Math.min(prevInput.length, text.length)
	  while (same < l && prevInput.charCodeAt(same) == text.charCodeAt(same)) { ++same }

	  runInOp(cm, function () {
	    applyTextInput(cm, text.slice(same), prevInput.length - same,
	                   null, this$1.composing ? "*compose" : null)

	    // Don't leave long text in the textarea, since it makes further polling slow
	    if (text.length > 1000 || text.indexOf("\n") > -1) { input.value = this$1.prevInput = "" }
	    else { this$1.prevInput = text }

	    if (this$1.composing) {
	      this$1.composing.range.clear()
	      this$1.composing.range = cm.markText(this$1.composing.start, cm.getCursor("to"),
	                                         {className: "CodeMirror-composing"})
	    }
	  })
	  return true
	};

	TextareaInput.prototype.ensurePolled = function () {
	  if (this.pollingFast && this.poll()) { this.pollingFast = false }
	};

	TextareaInput.prototype.onKeyPress = function () {
	  if (ie && ie_version >= 9) { this.hasSelection = null }
	  this.fastPoll()
	};

	TextareaInput.prototype.onContextMenu = function (e) {
	  var input = this, cm = input.cm, display = cm.display, te = input.textarea
	  var pos = posFromMouse(cm, e), scrollPos = display.scroller.scrollTop
	  if (!pos || presto) { return } // Opera is difficult.

	  // Reset the current text selection only if the click is done outside of the selection
	  // and 'resetSelectionOnContextMenu' option is true.
	  var reset = cm.options.resetSelectionOnContextMenu
	  if (reset && cm.doc.sel.contains(pos) == -1)
	    { operation(cm, setSelection)(cm.doc, simpleSelection(pos), sel_dontScroll) }

	  var oldCSS = te.style.cssText, oldWrapperCSS = input.wrapper.style.cssText
	  input.wrapper.style.cssText = "position: absolute"
	  var wrapperBox = input.wrapper.getBoundingClientRect()
	  te.style.cssText = "position: absolute; width: 30px; height: 30px;\n      top: " + (e.clientY - wrapperBox.top - 5) + "px; left: " + (e.clientX - wrapperBox.left - 5) + "px;\n      z-index: 1000; background: " + (ie ? "rgba(255, 255, 255, .05)" : "transparent") + ";\n      outline: none; border-width: 0; outline: none; overflow: hidden; opacity: .05; filter: alpha(opacity=5);"
	  var oldScrollY
	  if (webkit) { oldScrollY = window.scrollY } // Work around Chrome issue (#2712)
	  display.input.focus()
	  if (webkit) { window.scrollTo(null, oldScrollY) }
	  display.input.reset()
	  // Adds "Select all" to context menu in FF
	  if (!cm.somethingSelected()) { te.value = input.prevInput = " " }
	  input.contextMenuPending = true
	  display.selForContextMenu = cm.doc.sel
	  clearTimeout(display.detectingSelectAll)

	  // Select-all will be greyed out if there's nothing to select, so
	  // this adds a zero-width space so that we can later check whether
	  // it got selected.
	  function prepareSelectAllHack() {
	    if (te.selectionStart != null) {
	      var selected = cm.somethingSelected()
	      var extval = "\u200b" + (selected ? te.value : "")
	      te.value = "\u21da" // Used to catch context-menu undo
	      te.value = extval
	      input.prevInput = selected ? "" : "\u200b"
	      te.selectionStart = 1; te.selectionEnd = extval.length
	      // Re-set this, in case some other handler touched the
	      // selection in the meantime.
	      display.selForContextMenu = cm.doc.sel
	    }
	  }
	  function rehide() {
	    input.contextMenuPending = false
	    input.wrapper.style.cssText = oldWrapperCSS
	    te.style.cssText = oldCSS
	    if (ie && ie_version < 9) { display.scrollbars.setScrollTop(display.scroller.scrollTop = scrollPos) }

	    // Try to detect the user choosing select-all
	    if (te.selectionStart != null) {
	      if (!ie || (ie && ie_version < 9)) { prepareSelectAllHack() }
	      var i = 0, poll = function () {
	        if (display.selForContextMenu == cm.doc.sel && te.selectionStart == 0 &&
	            te.selectionEnd > 0 && input.prevInput == "\u200b") {
	          operation(cm, selectAll)(cm)
	        } else if (i++ < 10) {
	          display.detectingSelectAll = setTimeout(poll, 500)
	        } else {
	          display.selForContextMenu = null
	          display.input.reset()
	        }
	      }
	      display.detectingSelectAll = setTimeout(poll, 200)
	    }
	  }

	  if (ie && ie_version >= 9) { prepareSelectAllHack() }
	  if (captureRightClick) {
	    e_stop(e)
	    var mouseup = function () {
	      off(window, "mouseup", mouseup)
	      setTimeout(rehide, 20)
	    }
	    on(window, "mouseup", mouseup)
	  } else {
	    setTimeout(rehide, 50)
	  }
	};

	TextareaInput.prototype.readOnlyChanged = function (val) {
	  if (!val) { this.reset() }
	  this.textarea.disabled = val == "nocursor"
	};

	TextareaInput.prototype.setUneditable = function () {};

	TextareaInput.prototype.needsContentAttribute = false

	function fromTextArea(textarea, options) {
	  options = options ? copyObj(options) : {}
	  options.value = textarea.value
	  if (!options.tabindex && textarea.tabIndex)
	    { options.tabindex = textarea.tabIndex }
	  if (!options.placeholder && textarea.placeholder)
	    { options.placeholder = textarea.placeholder }
	  // Set autofocus to true if this textarea is focused, or if it has
	  // autofocus and no other element is focused.
	  if (options.autofocus == null) {
	    var hasFocus = activeElt()
	    options.autofocus = hasFocus == textarea ||
	      textarea.getAttribute("autofocus") != null && hasFocus == document.body
	  }

	  function save() {textarea.value = cm.getValue()}

	  var realSubmit
	  if (textarea.form) {
	    on(textarea.form, "submit", save)
	    // Deplorable hack to make the submit method do the right thing.
	    if (!options.leaveSubmitMethodAlone) {
	      var form = textarea.form
	      realSubmit = form.submit
	      try {
	        var wrappedSubmit = form.submit = function () {
	          save()
	          form.submit = realSubmit
	          form.submit()
	          form.submit = wrappedSubmit
	        }
	      } catch(e) {}
	    }
	  }

	  options.finishInit = function (cm) {
	    cm.save = save
	    cm.getTextArea = function () { return textarea; }
	    cm.toTextArea = function () {
	      cm.toTextArea = isNaN // Prevent this from being ran twice
	      save()
	      textarea.parentNode.removeChild(cm.getWrapperElement())
	      textarea.style.display = ""
	      if (textarea.form) {
	        off(textarea.form, "submit", save)
	        if (typeof textarea.form.submit == "function")
	          { textarea.form.submit = realSubmit }
	      }
	    }
	  }

	  textarea.style.display = "none"
	  var cm = CodeMirror(function (node) { return textarea.parentNode.insertBefore(node, textarea.nextSibling); },
	    options)
	  return cm
	}

	function addLegacyProps(CodeMirror) {
	  CodeMirror.off = off
	  CodeMirror.on = on
	  CodeMirror.wheelEventPixels = wheelEventPixels
	  CodeMirror.Doc = Doc
	  CodeMirror.splitLines = splitLinesAuto
	  CodeMirror.countColumn = countColumn
	  CodeMirror.findColumn = findColumn
	  CodeMirror.isWordChar = isWordCharBasic
	  CodeMirror.Pass = Pass
	  CodeMirror.signal = signal
	  CodeMirror.Line = Line
	  CodeMirror.changeEnd = changeEnd
	  CodeMirror.scrollbarModel = scrollbarModel
	  CodeMirror.Pos = Pos
	  CodeMirror.cmpPos = cmp
	  CodeMirror.modes = modes
	  CodeMirror.mimeModes = mimeModes
	  CodeMirror.resolveMode = resolveMode
	  CodeMirror.getMode = getMode
	  CodeMirror.modeExtensions = modeExtensions
	  CodeMirror.extendMode = extendMode
	  CodeMirror.copyState = copyState
	  CodeMirror.startState = startState
	  CodeMirror.innerMode = innerMode
	  CodeMirror.commands = commands
	  CodeMirror.keyMap = keyMap
	  CodeMirror.keyName = keyName
	  CodeMirror.isModifierKey = isModifierKey
	  CodeMirror.lookupKey = lookupKey
	  CodeMirror.normalizeKeyMap = normalizeKeyMap
	  CodeMirror.StringStream = StringStream
	  CodeMirror.SharedTextMarker = SharedTextMarker
	  CodeMirror.TextMarker = TextMarker
	  CodeMirror.LineWidget = LineWidget
	  CodeMirror.e_preventDefault = e_preventDefault
	  CodeMirror.e_stopPropagation = e_stopPropagation
	  CodeMirror.e_stop = e_stop
	  CodeMirror.addClass = addClass
	  CodeMirror.contains = contains
	  CodeMirror.rmClass = rmClass
	  CodeMirror.keyNames = keyNames
	}

	// EDITOR CONSTRUCTOR

	defineOptions(CodeMirror)

	addEditorMethods(CodeMirror)

	// Set up methods on CodeMirror's prototype to redirect to the editor's document.
	var dontDelegate = "iter insert remove copy getEditor constructor".split(" ")
	for (var prop in Doc.prototype) { if (Doc.prototype.hasOwnProperty(prop) && indexOf(dontDelegate, prop) < 0)
	  { CodeMirror.prototype[prop] = (function(method) {
	    return function() {return method.apply(this.doc, arguments)}
	  })(Doc.prototype[prop]) } }

	eventMixin(Doc)

	// INPUT HANDLING

	CodeMirror.inputStyles = {"textarea": TextareaInput, "contenteditable": ContentEditableInput}

	// MODE DEFINITION AND QUERYING

	// Extra arguments are stored as the mode's dependencies, which is
	// used by (legacy) mechanisms like loadmode.js to automatically
	// load a mode. (Preferred mechanism is the require/define calls.)
	CodeMirror.defineMode = function(name/*, mode, …*/) {
	  if (!CodeMirror.defaults.mode && name != "null") { CodeMirror.defaults.mode = name }
	  defineMode.apply(this, arguments)
	}

	CodeMirror.defineMIME = defineMIME

	// Minimal default mode.
	CodeMirror.defineMode("null", function () { return ({token: function (stream) { return stream.skipToEnd(); }}); })
	CodeMirror.defineMIME("text/plain", "null")

	// EXTENSIONS

	CodeMirror.defineExtension = function (name, func) {
	  CodeMirror.prototype[name] = func
	}
	CodeMirror.defineDocExtension = function (name, func) {
	  Doc.prototype[name] = func
	}

	CodeMirror.fromTextArea = fromTextArea

	addLegacyProps(CodeMirror)

	CodeMirror.version = "5.32.0"

	return CodeMirror;

	})));

/***/ }),
/* 3 */
/***/ (function(module, exports, __webpack_require__) {

	// CodeMirror, copyright (c) by Marijn Haverbeke and others
	// Distributed under an MIT license: http://codemirror.net/LICENSE

	(function(mod) {
	  if (true) // CommonJS
	    mod(__webpack_require__(2))
	  else if (typeof define == "function" && define.amd) // AMD
	    define(["../../lib/codemirror"], mod)
	  else // Plain browser env
	    mod(CodeMirror)
	})(function(CodeMirror) {
	  "use strict"
	  var Pos = CodeMirror.Pos

	  function regexpFlags(regexp) {
	    var flags = regexp.flags
	    return flags != null ? flags : (regexp.ignoreCase ? "i" : "")
	      + (regexp.global ? "g" : "")
	      + (regexp.multiline ? "m" : "")
	  }

	  function ensureGlobal(regexp) {
	    return regexp.global ? regexp : new RegExp(regexp.source, regexpFlags(regexp) + "g")
	  }

	  function maybeMultiline(regexp) {
	    return /\\s|\\n|\n|\\W|\\D|\[\^/.test(regexp.source)
	  }

	  function searchRegexpForward(doc, regexp, start) {
	    regexp = ensureGlobal(regexp)
	    for (var line = start.line, ch = start.ch, last = doc.lastLine(); line <= last; line++, ch = 0) {
	      regexp.lastIndex = ch
	      var string = doc.getLine(line), match = regexp.exec(string)
	      if (match)
	        return {from: Pos(line, match.index),
	                to: Pos(line, match.index + match[0].length),
	                match: match}
	    }
	  }

	  function searchRegexpForwardMultiline(doc, regexp, start) {
	    if (!maybeMultiline(regexp)) return searchRegexpForward(doc, regexp, start)

	    regexp = ensureGlobal(regexp)
	    var string, chunk = 1
	    for (var line = start.line, last = doc.lastLine(); line <= last;) {
	      // This grows the search buffer in exponentially-sized chunks
	      // between matches, so that nearby matches are fast and don't
	      // require concatenating the whole document (in case we're
	      // searching for something that has tons of matches), but at the
	      // same time, the amount of retries is limited.
	      for (var i = 0; i < chunk; i++) {
	        var curLine = doc.getLine(line++)
	        string = string == null ? curLine : string + "\n" + curLine
	      }
	      chunk = chunk * 2
	      regexp.lastIndex = start.ch
	      var match = regexp.exec(string)
	      if (match) {
	        var before = string.slice(0, match.index).split("\n"), inside = match[0].split("\n")
	        var startLine = start.line + before.length - 1, startCh = before[before.length - 1].length
	        return {from: Pos(startLine, startCh),
	                to: Pos(startLine + inside.length - 1,
	                        inside.length == 1 ? startCh + inside[0].length : inside[inside.length - 1].length),
	                match: match}
	      }
	    }
	  }

	  function lastMatchIn(string, regexp) {
	    var cutOff = 0, match
	    for (;;) {
	      regexp.lastIndex = cutOff
	      var newMatch = regexp.exec(string)
	      if (!newMatch) return match
	      match = newMatch
	      cutOff = match.index + (match[0].length || 1)
	      if (cutOff == string.length) return match
	    }
	  }

	  function searchRegexpBackward(doc, regexp, start) {
	    regexp = ensureGlobal(regexp)
	    for (var line = start.line, ch = start.ch, first = doc.firstLine(); line >= first; line--, ch = -1) {
	      var string = doc.getLine(line)
	      if (ch > -1) string = string.slice(0, ch)
	      var match = lastMatchIn(string, regexp)
	      if (match)
	        return {from: Pos(line, match.index),
	                to: Pos(line, match.index + match[0].length),
	                match: match}
	    }
	  }

	  function searchRegexpBackwardMultiline(doc, regexp, start) {
	    regexp = ensureGlobal(regexp)
	    var string, chunk = 1
	    for (var line = start.line, first = doc.firstLine(); line >= first;) {
	      for (var i = 0; i < chunk; i++) {
	        var curLine = doc.getLine(line--)
	        string = string == null ? curLine.slice(0, start.ch) : curLine + "\n" + string
	      }
	      chunk *= 2

	      var match = lastMatchIn(string, regexp)
	      if (match) {
	        var before = string.slice(0, match.index).split("\n"), inside = match[0].split("\n")
	        var startLine = line + before.length, startCh = before[before.length - 1].length
	        return {from: Pos(startLine, startCh),
	                to: Pos(startLine + inside.length - 1,
	                        inside.length == 1 ? startCh + inside[0].length : inside[inside.length - 1].length),
	                match: match}
	      }
	    }
	  }

	  var doFold, noFold
	  if (String.prototype.normalize) {
	    doFold = function(str) { return str.normalize("NFD").toLowerCase() }
	    noFold = function(str) { return str.normalize("NFD") }
	  } else {
	    doFold = function(str) { return str.toLowerCase() }
	    noFold = function(str) { return str }
	  }

	  // Maps a position in a case-folded line back to a position in the original line
	  // (compensating for codepoints increasing in number during folding)
	  function adjustPos(orig, folded, pos, foldFunc) {
	    if (orig.length == folded.length) return pos
	    for (var min = 0, max = pos + Math.max(0, orig.length - folded.length);;) {
	      if (min == max) return min
	      var mid = (min + max) >> 1
	      var len = foldFunc(orig.slice(0, mid)).length
	      if (len == pos) return mid
	      else if (len > pos) max = mid
	      else min = mid + 1
	    }
	  }

	  function searchStringForward(doc, query, start, caseFold) {
	    // Empty string would match anything and never progress, so we
	    // define it to match nothing instead.
	    if (!query.length) return null
	    var fold = caseFold ? doFold : noFold
	    var lines = fold(query).split(/\r|\n\r?/)

	    search: for (var line = start.line, ch = start.ch, last = doc.lastLine() + 1 - lines.length; line <= last; line++, ch = 0) {
	      var orig = doc.getLine(line).slice(ch), string = fold(orig)
	      if (lines.length == 1) {
	        var found = string.indexOf(lines[0])
	        if (found == -1) continue search
	        var start = adjustPos(orig, string, found, fold) + ch
	        return {from: Pos(line, adjustPos(orig, string, found, fold) + ch),
	                to: Pos(line, adjustPos(orig, string, found + lines[0].length, fold) + ch)}
	      } else {
	        var cutFrom = string.length - lines[0].length
	        if (string.slice(cutFrom) != lines[0]) continue search
	        for (var i = 1; i < lines.length - 1; i++)
	          if (fold(doc.getLine(line + i)) != lines[i]) continue search
	        var end = doc.getLine(line + lines.length - 1), endString = fold(end), lastLine = lines[lines.length - 1]
	        if (endString.slice(0, lastLine.length) != lastLine) continue search
	        return {from: Pos(line, adjustPos(orig, string, cutFrom, fold) + ch),
	                to: Pos(line + lines.length - 1, adjustPos(end, endString, lastLine.length, fold))}
	      }
	    }
	  }

	  function searchStringBackward(doc, query, start, caseFold) {
	    if (!query.length) return null
	    var fold = caseFold ? doFold : noFold
	    var lines = fold(query).split(/\r|\n\r?/)

	    search: for (var line = start.line, ch = start.ch, first = doc.firstLine() - 1 + lines.length; line >= first; line--, ch = -1) {
	      var orig = doc.getLine(line)
	      if (ch > -1) orig = orig.slice(0, ch)
	      var string = fold(orig)
	      if (lines.length == 1) {
	        var found = string.lastIndexOf(lines[0])
	        if (found == -1) continue search
	        return {from: Pos(line, adjustPos(orig, string, found, fold)),
	                to: Pos(line, adjustPos(orig, string, found + lines[0].length, fold))}
	      } else {
	        var lastLine = lines[lines.length - 1]
	        if (string.slice(0, lastLine.length) != lastLine) continue search
	        for (var i = 1, start = line - lines.length + 1; i < lines.length - 1; i++)
	          if (fold(doc.getLine(start + i)) != lines[i]) continue search
	        var top = doc.getLine(line + 1 - lines.length), topString = fold(top)
	        if (topString.slice(topString.length - lines[0].length) != lines[0]) continue search
	        return {from: Pos(line + 1 - lines.length, adjustPos(top, topString, top.length - lines[0].length, fold)),
	                to: Pos(line, adjustPos(orig, string, lastLine.length, fold))}
	      }
	    }
	  }

	  function SearchCursor(doc, query, pos, options) {
	    this.atOccurrence = false
	    this.doc = doc
	    pos = pos ? doc.clipPos(pos) : Pos(0, 0)
	    this.pos = {from: pos, to: pos}

	    var caseFold
	    if (typeof options == "object") {
	      caseFold = options.caseFold
	    } else { // Backwards compat for when caseFold was the 4th argument
	      caseFold = options
	      options = null
	    }

	    if (typeof query == "string") {
	      if (caseFold == null) caseFold = false
	      this.matches = function(reverse, pos) {
	        return (reverse ? searchStringBackward : searchStringForward)(doc, query, pos, caseFold)
	      }
	    } else {
	      query = ensureGlobal(query)
	      if (!options || options.multiline !== false)
	        this.matches = function(reverse, pos) {
	          return (reverse ? searchRegexpBackwardMultiline : searchRegexpForwardMultiline)(doc, query, pos)
	        }
	      else
	        this.matches = function(reverse, pos) {
	          return (reverse ? searchRegexpBackward : searchRegexpForward)(doc, query, pos)
	        }
	    }
	  }

	  SearchCursor.prototype = {
	    findNext: function() {return this.find(false)},
	    findPrevious: function() {return this.find(true)},

	    find: function(reverse) {
	      var result = this.matches(reverse, this.doc.clipPos(reverse ? this.pos.from : this.pos.to))

	      // Implements weird auto-growing behavior on null-matches for
	      // backwards-compatiblity with the vim code (unfortunately)
	      while (result && CodeMirror.cmpPos(result.from, result.to) == 0) {
	        if (reverse) {
	          if (result.from.ch) result.from = Pos(result.from.line, result.from.ch - 1)
	          else if (result.from.line == this.doc.firstLine()) result = null
	          else result = this.matches(reverse, this.doc.clipPos(Pos(result.from.line - 1)))
	        } else {
	          if (result.to.ch < this.doc.getLine(result.to.line).length) result.to = Pos(result.to.line, result.to.ch + 1)
	          else if (result.to.line == this.doc.lastLine()) result = null
	          else result = this.matches(reverse, Pos(result.to.line + 1, 0))
	        }
	      }

	      if (result) {
	        this.pos = result
	        this.atOccurrence = true
	        return this.pos.match || true
	      } else {
	        var end = Pos(reverse ? this.doc.firstLine() : this.doc.lastLine() + 1, 0)
	        this.pos = {from: end, to: end}
	        return this.atOccurrence = false
	      }
	    },

	    from: function() {if (this.atOccurrence) return this.pos.from},
	    to: function() {if (this.atOccurrence) return this.pos.to},

	    replace: function(newText, origin) {
	      if (!this.atOccurrence) return
	      var lines = CodeMirror.splitLines(newText)
	      this.doc.replaceRange(lines, this.pos.from, this.pos.to, origin)
	      this.pos.to = Pos(this.pos.from.line + lines.length - 1,
	                        lines[lines.length - 1].length + (lines.length == 1 ? this.pos.from.ch : 0))
	    }
	  }

	  CodeMirror.defineExtension("getSearchCursor", function(query, pos, caseFold) {
	    return new SearchCursor(this.doc, query, pos, caseFold)
	  })
	  CodeMirror.defineDocExtension("getSearchCursor", function(query, pos, caseFold) {
	    return new SearchCursor(this, query, pos, caseFold)
	  })

	  CodeMirror.defineExtension("selectMatches", function(query, caseFold) {
	    var ranges = []
	    var cur = this.getSearchCursor(query, this.getCursor("from"), caseFold)
	    while (cur.findNext()) {
	      if (CodeMirror.cmpPos(cur.to(), this.getCursor("to")) > 0) break
	      ranges.push({anchor: cur.from(), head: cur.to()})
	    }
	    if (ranges.length)
	      this.setSelections(ranges, 0)
	  })
	});


/***/ }),
/* 4 */
/***/ (function(module, exports, __webpack_require__) {

	// CodeMirror, copyright (c) by Marijn Haverbeke and others
	// Distributed under an MIT license: http://codemirror.net/LICENSE

	// Define search commands. Depends on dialog.js or another
	// implementation of the openDialog method.

	// Replace works a little oddly -- it will do the replace on the next
	// Ctrl-G (or whatever is bound to findNext) press. You prevent a
	// replace by making sure the match is no longer selected when hitting
	// Ctrl-G.

	(function(mod) {
	  if (true) // CommonJS
	    mod(__webpack_require__(2), __webpack_require__(3), __webpack_require__(1));
	  else if (typeof define == "function" && define.amd) // AMD
	    define(["../../lib/codemirror", "./searchcursor", "../dialog/dialog"], mod);
	  else // Plain browser env
	    mod(CodeMirror);
	})(function(CodeMirror) {
	  "use strict";

	  function searchOverlay(query, caseInsensitive) {
	    if (typeof query == "string")
	      query = new RegExp(query.replace(/[\-\[\]\/\{\}\(\)\*\+\?\.\\\^\$\|]/g, "\\$&"), caseInsensitive ? "gi" : "g");
	    else if (!query.global)
	      query = new RegExp(query.source, query.ignoreCase ? "gi" : "g");

	    return {token: function(stream) {
	      query.lastIndex = stream.pos;
	      var match = query.exec(stream.string);
	      if (match && match.index == stream.pos) {
	        stream.pos += match[0].length || 1;
	        return "searching";
	      } else if (match) {
	        stream.pos = match.index;
	      } else {
	        stream.skipToEnd();
	      }
	    }};
	  }

	  function SearchState() {
	    this.posFrom = this.posTo = this.lastQuery = this.query = null;
	    this.overlay = null;
	  }

	  function getSearchState(cm) {
	    return cm.state.search || (cm.state.search = new SearchState());
	  }

	  function queryCaseInsensitive(query) {
	    return typeof query == "string" && query == query.toLowerCase();
	  }

	  function getSearchCursor(cm, query, pos) {
	    // Heuristic: if the query string is all lowercase, do a case insensitive search.
	    return cm.getSearchCursor(query, pos, {caseFold: queryCaseInsensitive(query), multiline: true});
	  }

	  function persistentDialog(cm, text, deflt, onEnter, onKeyDown) {
	    cm.openDialog(text, onEnter, {
	      value: deflt,
	      selectValueOnOpen: true,
	      closeOnEnter: false,
	      onClose: function() { clearSearch(cm); },
	      onKeyDown: onKeyDown
	    });
	  }

	  function dialog(cm, text, shortText, deflt, f) {
	    if (cm.openDialog) cm.openDialog(text, f, {value: deflt, selectValueOnOpen: true});
	    else f(prompt(shortText, deflt));
	  }

	  function confirmDialog(cm, text, shortText, fs) {
	    if (cm.openConfirm) cm.openConfirm(text, fs);
	    else if (confirm(shortText)) fs[0]();
	  }

	  function parseString(string) {
	    return string.replace(/\\(.)/g, function(_, ch) {
	      if (ch == "n") return "\n"
	      if (ch == "r") return "\r"
	      return ch
	    })
	  }

	  function parseQuery(query) {
	    var isRE = query.match(/^\/(.*)\/([a-z]*)$/);
	    if (isRE) {
	      try { query = new RegExp(isRE[1], isRE[2].indexOf("i") == -1 ? "" : "i"); }
	      catch(e) {} // Not a regular expression after all, do a string search
	    } else {
	      query = parseString(query)
	    }
	    if (typeof query == "string" ? query == "" : query.test(""))
	      query = /x^/;
	    return query;
	  }

	  var queryDialog;

	  function startSearch(cm, state, query) {
	    state.queryText = query;
	    state.query = parseQuery(query);
	    cm.removeOverlay(state.overlay, queryCaseInsensitive(state.query));
	    state.overlay = searchOverlay(state.query, queryCaseInsensitive(state.query));
	    cm.addOverlay(state.overlay);
	    if (cm.showMatchesOnScrollbar) {
	      if (state.annotate) { state.annotate.clear(); state.annotate = null; }
	      state.annotate = cm.showMatchesOnScrollbar(state.query, queryCaseInsensitive(state.query));
	    }
	  }

	  function doSearch(cm, rev, persistent, immediate) {
	    if (!queryDialog) {
	      let doc = cm.getWrapperElement().ownerDocument;
	      let inp = doc.createElement("input");

	      inp.type = "search";
	      inp.placeholder = cm.l10n("findCmd.promptMessage");
	      inp.style.marginInlineStart = "1em";
	      inp.style.marginInlineEnd = "1em";
	      inp.style.flexGrow = "1";
	      inp.addEventListener("focus", () => inp.select());

	      queryDialog = doc.createElement("div");
	      queryDialog.appendChild(inp);
	      queryDialog.style.display = "flex";
	    }

	    var state = getSearchState(cm);
	    if (state.query) return findNext(cm, rev);
	    var q = cm.getSelection() || state.lastQuery;
	    if (q instanceof RegExp && q.source == "x^") q = null
	    if (persistent && cm.openDialog) {
	      var hiding = null
	      var searchNext = function(query, event) {
	        CodeMirror.e_stop(event);
	        if (!query) return;
	        if (query != state.queryText) {
	          startSearch(cm, state, query);
	          state.posFrom = state.posTo = cm.getCursor();
	        }
	        if (hiding) hiding.style.opacity = 1
	        findNext(cm, event.shiftKey, function(_, to) {
	          var dialog
	          if (to.line < 3 && document.querySelector &&
	              (dialog = cm.display.wrapper.querySelector(".CodeMirror-dialog")) &&
	              dialog.getBoundingClientRect().bottom - 4 > cm.cursorCoords(to, "window").top)
	            (hiding = dialog).style.opacity = .4
	        })
	      };
	      persistentDialog(cm, queryDialog, q, searchNext, function(event, query) {
	        var keyName = CodeMirror.keyName(event)
	        var extra = cm.getOption('extraKeys'), cmd = (extra && extra[keyName]) || CodeMirror.keyMap[cm.getOption("keyMap")][keyName]
	        if (cmd == "findNext" || cmd == "findPrev" ||
	          cmd == "findPersistentNext" || cmd == "findPersistentPrev") {
	          CodeMirror.e_stop(event);
	          startSearch(cm, getSearchState(cm), query);
	          cm.execCommand(cmd);
	        } else if (cmd == "find" || cmd == "findPersistent") {
	          CodeMirror.e_stop(event);
	          searchNext(query, event);
	        }
	      });
	      if (immediate && q) {
	        startSearch(cm, state, q);
	        findNext(cm, rev);
	      }
	    } else {
	      dialog(cm, queryDialog, "Search for:", q, function(query) {
	        if (query && !state.query) cm.operation(function() {
	          startSearch(cm, state, query);
	          state.posFrom = state.posTo = cm.getCursor();
	          findNext(cm, rev);
	        });
	      });
	    }
	  }

	  function findNext(cm, rev, callback) {cm.operation(function() {
	    var state = getSearchState(cm);
	    var cursor = getSearchCursor(cm, state.query, rev ? state.posFrom : state.posTo);
	    if (!cursor.find(rev)) {
	      cursor = getSearchCursor(cm, state.query, rev ? CodeMirror.Pos(cm.lastLine()) : CodeMirror.Pos(cm.firstLine(), 0));
	      if (!cursor.find(rev)) return;
	    }
	    cm.setSelection(cursor.from(), cursor.to());
	    cm.scrollIntoView({from: cursor.from(), to: cursor.to()}, 20);
	    state.posFrom = cursor.from(); state.posTo = cursor.to();
	    if (callback) callback(cursor.from(), cursor.to())
	  });}

	  function clearSearch(cm) {cm.operation(function() {
	    var state = getSearchState(cm);
	    state.lastQuery = state.query;
	    if (!state.query) return;
	    state.query = state.queryText = null;
	    cm.removeOverlay(state.overlay);
	    if (state.annotate) { state.annotate.clear(); state.annotate = null; }
	  });}

	  var replaceQueryDialog =
	    ' <input type="text" style="width: 10em" class="CodeMirror-search-field"/> <span style="color: #888" class="CodeMirror-search-hint">(Use /re/ syntax for regexp search)</span>';
	  var replacementQueryDialog = '<span class="CodeMirror-search-label">With:</span> <input type="text" style="width: 10em" class="CodeMirror-search-field"/>';
	  var doReplaceConfirm = '<span class="CodeMirror-search-label">Replace?</span> <button>Yes</button> <button>No</button> <button>All</button> <button>Stop</button>';

	  function replaceAll(cm, query, text) {
	    cm.operation(function() {
	      for (var cursor = getSearchCursor(cm, query); cursor.findNext();) {
	        if (typeof query != "string") {
	          var match = cm.getRange(cursor.from(), cursor.to()).match(query);
	          cursor.replace(text.replace(/\$(\d)/g, function(_, i) {return match[i];}));
	        } else cursor.replace(text);
	      }
	    });
	  }

	  function replace(cm, all) {
	    if (cm.getOption("readOnly")) return;
	    var query = cm.getSelection() || getSearchState(cm).lastQuery;
	    var dialogText = '<span class="CodeMirror-search-label">' + (all ? 'Replace all:' : 'Replace:') + '</span>';
	    dialog(cm, dialogText + replaceQueryDialog, dialogText, query, function(query) {
	      if (!query) return;
	      query = parseQuery(query);
	      dialog(cm, replacementQueryDialog, "Replace with:", "", function(text) {
	        text = parseString(text)
	        if (all) {
	          replaceAll(cm, query, text)
	        } else {
	          clearSearch(cm);
	          var cursor = getSearchCursor(cm, query, cm.getCursor("from"));
	          var advance = function() {
	            var start = cursor.from(), match;
	            if (!(match = cursor.findNext())) {
	              cursor = getSearchCursor(cm, query);
	              if (!(match = cursor.findNext()) ||
	                  (start && cursor.from().line == start.line && cursor.from().ch == start.ch)) return;
	            }
	            cm.setSelection(cursor.from(), cursor.to());
	            cm.scrollIntoView({from: cursor.from(), to: cursor.to()});
	            confirmDialog(cm, doReplaceConfirm, "Replace?",
	                          [function() {doReplace(match);}, advance,
	                           function() {replaceAll(cm, query, text)}]);
	          };
	          var doReplace = function(match) {
	            cursor.replace(typeof query == "string" ? text :
	                           text.replace(/\$(\d)/g, function(_, i) {return match[i];}));
	            advance();
	          };
	          advance();
	        }
	      });
	    });
	  }

	  CodeMirror.commands.find = function(cm) {clearSearch(cm); doSearch(cm);};
	  CodeMirror.commands.findPersistent = function(cm) {clearSearch(cm); doSearch(cm, false, true);};
	  CodeMirror.commands.findPersistentNext = function(cm) {doSearch(cm, false, true, true);};
	  CodeMirror.commands.findPersistentPrev = function(cm) {doSearch(cm, true, true, true);};
	  CodeMirror.commands.findNext = doSearch;
	  CodeMirror.commands.findPrev = function(cm) {doSearch(cm, true);};
	  CodeMirror.commands.clearSearch = clearSearch;
	  CodeMirror.commands.replace = replace;
	  CodeMirror.commands.replaceAll = function(cm) {replace(cm, true);};
	});


/***/ }),
/* 5 */
/***/ (function(module, exports, __webpack_require__) {

	// CodeMirror, copyright (c) by Marijn Haverbeke and others
	// Distributed under an MIT license: http://codemirror.net/LICENSE

	(function(mod) {
	  if (true) // CommonJS
	    mod(__webpack_require__(2));
	  else if (typeof define == "function" && define.amd) // AMD
	    define(["../../lib/codemirror"], mod);
	  else // Plain browser env
	    mod(CodeMirror);
	})(function(CodeMirror) {
	  var ie_lt8 = /MSIE \d/.test(navigator.userAgent) &&
	    (document.documentMode == null || document.documentMode < 8);

	  var Pos = CodeMirror.Pos;

	  var matching = {"(": ")>", ")": "(<", "[": "]>", "]": "[<", "{": "}>", "}": "{<"};

	  function findMatchingBracket(cm, where, config) {
	    var line = cm.getLineHandle(where.line), pos = where.ch - 1;
	    var afterCursor = config && config.afterCursor
	    if (afterCursor == null)
	      afterCursor = /(^| )cm-fat-cursor($| )/.test(cm.getWrapperElement().className)

	    // A cursor is defined as between two characters, but in in vim command mode
	    // (i.e. not insert mode), the cursor is visually represented as a
	    // highlighted box on top of the 2nd character. Otherwise, we allow matches
	    // from before or after the cursor.
	    var match = (!afterCursor && pos >= 0 && matching[line.text.charAt(pos)]) ||
	        matching[line.text.charAt(++pos)];
	    if (!match) return null;
	    var dir = match.charAt(1) == ">" ? 1 : -1;
	    if (config && config.strict && (dir > 0) != (pos == where.ch)) return null;
	    var style = cm.getTokenTypeAt(Pos(where.line, pos + 1));

	    var found = scanForBracket(cm, Pos(where.line, pos + (dir > 0 ? 1 : 0)), dir, style || null, config);
	    if (found == null) return null;
	    return {from: Pos(where.line, pos), to: found && found.pos,
	            match: found && found.ch == match.charAt(0), forward: dir > 0};
	  }

	  // bracketRegex is used to specify which type of bracket to scan
	  // should be a regexp, e.g. /[[\]]/
	  //
	  // Note: If "where" is on an open bracket, then this bracket is ignored.
	  //
	  // Returns false when no bracket was found, null when it reached
	  // maxScanLines and gave up
	  function scanForBracket(cm, where, dir, style, config) {
	    var maxScanLen = (config && config.maxScanLineLength) || 10000;
	    var maxScanLines = (config && config.maxScanLines) || 1000;

	    var stack = [];
	    var re = config && config.bracketRegex ? config.bracketRegex : /[(){}[\]]/;
	    var lineEnd = dir > 0 ? Math.min(where.line + maxScanLines, cm.lastLine() + 1)
	                          : Math.max(cm.firstLine() - 1, where.line - maxScanLines);
	    for (var lineNo = where.line; lineNo != lineEnd; lineNo += dir) {
	      var line = cm.getLine(lineNo);
	      if (!line) continue;
	      var pos = dir > 0 ? 0 : line.length - 1, end = dir > 0 ? line.length : -1;
	      if (line.length > maxScanLen) continue;
	      if (lineNo == where.line) pos = where.ch - (dir < 0 ? 1 : 0);
	      for (; pos != end; pos += dir) {
	        var ch = line.charAt(pos);
	        if (re.test(ch) && (style === undefined || cm.getTokenTypeAt(Pos(lineNo, pos + 1)) == style)) {
	          var match = matching[ch];
	          if ((match.charAt(1) == ">") == (dir > 0)) stack.push(ch);
	          else if (!stack.length) return {pos: Pos(lineNo, pos), ch: ch};
	          else stack.pop();
	        }
	      }
	    }
	    return lineNo - dir == (dir > 0 ? cm.lastLine() : cm.firstLine()) ? false : null;
	  }

	  function matchBrackets(cm, autoclear, config) {
	    // Disable brace matching in long lines, since it'll cause hugely slow updates
	    var maxHighlightLen = cm.state.matchBrackets.maxHighlightLineLength || 1000;
	    var marks = [], ranges = cm.listSelections();
	    for (var i = 0; i < ranges.length; i++) {
	      var match = ranges[i].empty() && findMatchingBracket(cm, ranges[i].head, config);
	      if (match && cm.getLine(match.from.line).length <= maxHighlightLen) {
	        var style = match.match ? "CodeMirror-matchingbracket" : "CodeMirror-nonmatchingbracket";
	        marks.push(cm.markText(match.from, Pos(match.from.line, match.from.ch + 1), {className: style}));
	        if (match.to && cm.getLine(match.to.line).length <= maxHighlightLen)
	          marks.push(cm.markText(match.to, Pos(match.to.line, match.to.ch + 1), {className: style}));
	      }
	    }

	    if (marks.length) {
	      // Kludge to work around the IE bug from issue #1193, where text
	      // input stops going to the textare whever this fires.
	      if (ie_lt8 && cm.state.focused) cm.focus();

	      var clear = function() {
	        cm.operation(function() {
	          for (var i = 0; i < marks.length; i++) marks[i].clear();
	        });
	      };
	      if (autoclear) setTimeout(clear, 800);
	      else return clear;
	    }
	  }

	  var currentlyHighlighted = null;
	  function doMatchBrackets(cm) {
	    cm.operation(function() {
	      if (currentlyHighlighted) {currentlyHighlighted(); currentlyHighlighted = null;}
	      currentlyHighlighted = matchBrackets(cm, false, cm.state.matchBrackets);
	    });
	  }

	  CodeMirror.defineOption("matchBrackets", false, function(cm, val, old) {
	    if (old && old != CodeMirror.Init) {
	      cm.off("cursorActivity", doMatchBrackets);
	      if (currentlyHighlighted) {currentlyHighlighted(); currentlyHighlighted = null;}
	    }
	    if (val) {
	      cm.state.matchBrackets = typeof val == "object" ? val : {};
	      cm.on("cursorActivity", doMatchBrackets);
	    }
	  });

	  CodeMirror.defineExtension("matchBrackets", function() {matchBrackets(this, true);});
	  CodeMirror.defineExtension("findMatchingBracket", function(pos, config, oldConfig){
	    // Backwards-compatibility kludge
	    if (oldConfig || typeof config == "boolean") {
	      if (!oldConfig) {
	        config = config ? {strict: true} : null
	      } else {
	        oldConfig.strict = config
	        config = oldConfig
	      }
	    }
	    return findMatchingBracket(this, pos, config)
	  });
	  CodeMirror.defineExtension("scanForBracket", function(pos, dir, style, config){
	    return scanForBracket(this, pos, dir, style, config);
	  });
	});


/***/ }),
/* 6 */
/***/ (function(module, exports, __webpack_require__) {

	// CodeMirror, copyright (c) by Marijn Haverbeke and others
	// Distributed under an MIT license: http://codemirror.net/LICENSE

	(function(mod) {
	  if (true) // CommonJS
	    mod(__webpack_require__(2));
	  else if (typeof define == "function" && define.amd) // AMD
	    define(["../../lib/codemirror"], mod);
	  else // Plain browser env
	    mod(CodeMirror);
	})(function(CodeMirror) {
	  var defaults = {
	    pairs: "()[]{}''\"\"",
	    triples: "",
	    explode: "[]{}"
	  };

	  var Pos = CodeMirror.Pos;

	  CodeMirror.defineOption("autoCloseBrackets", false, function(cm, val, old) {
	    if (old && old != CodeMirror.Init) {
	      cm.removeKeyMap(keyMap);
	      cm.state.closeBrackets = null;
	    }
	    if (val) {
	      ensureBound(getOption(val, "pairs"))
	      cm.state.closeBrackets = val;
	      cm.addKeyMap(keyMap);
	    }
	  });

	  function getOption(conf, name) {
	    if (name == "pairs" && typeof conf == "string") return conf;
	    if (typeof conf == "object" && conf[name] != null) return conf[name];
	    return defaults[name];
	  }

	  var keyMap = {Backspace: handleBackspace, Enter: handleEnter};
	  function ensureBound(chars) {
	    for (var i = 0; i < chars.length; i++) {
	      var ch = chars.charAt(i), key = "'" + ch + "'"
	      if (!keyMap[key]) keyMap[key] = handler(ch)
	    }
	  }
	  ensureBound(defaults.pairs + "`")

	  function handler(ch) {
	    return function(cm) { return handleChar(cm, ch); };
	  }

	  function getConfig(cm) {
	    var deflt = cm.state.closeBrackets;
	    if (!deflt || deflt.override) return deflt;
	    var mode = cm.getModeAt(cm.getCursor());
	    return mode.closeBrackets || deflt;
	  }

	  function handleBackspace(cm) {
	    var conf = getConfig(cm);
	    if (!conf || cm.getOption("disableInput")) return CodeMirror.Pass;

	    var pairs = getOption(conf, "pairs");
	    var ranges = cm.listSelections();
	    for (var i = 0; i < ranges.length; i++) {
	      if (!ranges[i].empty()) return CodeMirror.Pass;
	      var around = charsAround(cm, ranges[i].head);
	      if (!around || pairs.indexOf(around) % 2 != 0) return CodeMirror.Pass;
	    }
	    for (var i = ranges.length - 1; i >= 0; i--) {
	      var cur = ranges[i].head;
	      cm.replaceRange("", Pos(cur.line, cur.ch - 1), Pos(cur.line, cur.ch + 1), "+delete");
	    }
	  }

	  function handleEnter(cm) {
	    var conf = getConfig(cm);
	    var explode = conf && getOption(conf, "explode");
	    if (!explode || cm.getOption("disableInput")) return CodeMirror.Pass;

	    var ranges = cm.listSelections();
	    for (var i = 0; i < ranges.length; i++) {
	      if (!ranges[i].empty()) return CodeMirror.Pass;
	      var around = charsAround(cm, ranges[i].head);
	      if (!around || explode.indexOf(around) % 2 != 0) return CodeMirror.Pass;
	    }
	    cm.operation(function() {
	      var linesep = cm.lineSeparator() || "\n";
	      cm.replaceSelection(linesep + linesep, null);
	      cm.execCommand("goCharLeft");
	      ranges = cm.listSelections();
	      for (var i = 0; i < ranges.length; i++) {
	        var line = ranges[i].head.line;
	        cm.indentLine(line, null, true);
	        cm.indentLine(line + 1, null, true);
	      }
	    });
	  }

	  function contractSelection(sel) {
	    var inverted = CodeMirror.cmpPos(sel.anchor, sel.head) > 0;
	    return {anchor: new Pos(sel.anchor.line, sel.anchor.ch + (inverted ? -1 : 1)),
	            head: new Pos(sel.head.line, sel.head.ch + (inverted ? 1 : -1))};
	  }

	  function handleChar(cm, ch) {
	    var conf = getConfig(cm);
	    if (!conf || cm.getOption("disableInput")) return CodeMirror.Pass;

	    var pairs = getOption(conf, "pairs");
	    var pos = pairs.indexOf(ch);
	    if (pos == -1) return CodeMirror.Pass;
	    var triples = getOption(conf, "triples");

	    var identical = pairs.charAt(pos + 1) == ch;
	    var ranges = cm.listSelections();
	    var opening = pos % 2 == 0;

	    var type;
	    for (var i = 0; i < ranges.length; i++) {
	      var range = ranges[i], cur = range.head, curType;
	      var next = cm.getRange(cur, Pos(cur.line, cur.ch + 1));
	      if (opening && !range.empty()) {
	        curType = "surround";
	      } else if ((identical || !opening) && next == ch) {
	        if (identical && stringStartsAfter(cm, cur))
	          curType = "both";
	        else if (triples.indexOf(ch) >= 0 && cm.getRange(cur, Pos(cur.line, cur.ch + 3)) == ch + ch + ch)
	          curType = "skipThree";
	        else
	          curType = "skip";
	      } else if (identical && cur.ch > 1 && triples.indexOf(ch) >= 0 &&
	                 cm.getRange(Pos(cur.line, cur.ch - 2), cur) == ch + ch &&
	                 (cur.ch <= 2 || cm.getRange(Pos(cur.line, cur.ch - 3), Pos(cur.line, cur.ch - 2)) != ch)) {
	        curType = "addFour";
	      } else if (identical) {
	        var prev = cur.ch == 0 ? " " : cm.getRange(Pos(cur.line, cur.ch - 1), cur)
	        if (!CodeMirror.isWordChar(next) && prev != ch && !CodeMirror.isWordChar(prev)) curType = "both";
	        else return CodeMirror.Pass;
	      } else if (opening && (cm.getLine(cur.line).length == cur.ch ||
	                             isClosingBracket(next, pairs) ||
	                             /\s/.test(next))) {
	        curType = "both";
	      } else {
	        return CodeMirror.Pass;
	      }
	      if (!type) type = curType;
	      else if (type != curType) return CodeMirror.Pass;
	    }

	    var left = pos % 2 ? pairs.charAt(pos - 1) : ch;
	    var right = pos % 2 ? ch : pairs.charAt(pos + 1);
	    cm.operation(function() {
	      if (type == "skip") {
	        cm.execCommand("goCharRight");
	      } else if (type == "skipThree") {
	        for (var i = 0; i < 3; i++)
	          cm.execCommand("goCharRight");
	      } else if (type == "surround") {
	        var sels = cm.getSelections();
	        for (var i = 0; i < sels.length; i++)
	          sels[i] = left + sels[i] + right;
	        cm.replaceSelections(sels, "around");
	        sels = cm.listSelections().slice();
	        for (var i = 0; i < sels.length; i++)
	          sels[i] = contractSelection(sels[i]);
	        cm.setSelections(sels);
	      } else if (type == "both") {
	        cm.replaceSelection(left + right, null);
	        cm.triggerElectric(left + right);
	        cm.execCommand("goCharLeft");
	      } else if (type == "addFour") {
	        cm.replaceSelection(left + left + left + left, "before");
	        cm.execCommand("goCharRight");
	      }
	    });
	  }

	  function isClosingBracket(ch, pairs) {
	    var pos = pairs.lastIndexOf(ch);
	    return pos > -1 && pos % 2 == 1;
	  }

	  function charsAround(cm, pos) {
	    var str = cm.getRange(Pos(pos.line, pos.ch - 1),
	                          Pos(pos.line, pos.ch + 1));
	    return str.length == 2 ? str : null;
	  }

	  function stringStartsAfter(cm, pos) {
	    var token = cm.getTokenAt(Pos(pos.line, pos.ch + 1))
	    return /\bstring/.test(token.type) && token.start == pos.ch &&
	      (pos.ch == 0 || !/\bstring/.test(cm.getTokenTypeAt(pos)))
	  }
	});


/***/ }),
/* 7 */
/***/ (function(module, exports, __webpack_require__) {

	// CodeMirror, copyright (c) by Marijn Haverbeke and others
	// Distributed under an MIT license: http://codemirror.net/LICENSE

	(function(mod) {
	  if (true) // CommonJS
	    mod(__webpack_require__(2));
	  else if (typeof define == "function" && define.amd) // AMD
	    define(["../../lib/codemirror"], mod);
	  else // Plain browser env
	    mod(CodeMirror);
	})(function(CodeMirror) {
	  "use strict";

	  var noOptions = {};
	  var nonWS = /[^\s\u00a0]/;
	  var Pos = CodeMirror.Pos;

	  function firstNonWS(str) {
	    var found = str.search(nonWS);
	    return found == -1 ? 0 : found;
	  }

	  CodeMirror.commands.toggleComment = function(cm) {
	    cm.toggleComment();
	  };

	  CodeMirror.defineExtension("toggleComment", function(options) {
	    if (!options) options = noOptions;
	    var cm = this;
	    var minLine = Infinity, ranges = this.listSelections(), mode = null;
	    for (var i = ranges.length - 1; i >= 0; i--) {
	      var from = ranges[i].from(), to = ranges[i].to();
	      if (from.line >= minLine) continue;
	      if (to.line >= minLine) to = Pos(minLine, 0);
	      minLine = from.line;
	      if (mode == null) {
	        if (cm.uncomment(from, to, options)) mode = "un";
	        else { cm.lineComment(from, to, options); mode = "line"; }
	      } else if (mode == "un") {
	        cm.uncomment(from, to, options);
	      } else {
	        cm.lineComment(from, to, options);
	      }
	    }
	  });

	  // Rough heuristic to try and detect lines that are part of multi-line string
	  function probablyInsideString(cm, pos, line) {
	    return /\bstring\b/.test(cm.getTokenTypeAt(Pos(pos.line, 0))) && !/^[\'\"\`]/.test(line)
	  }

	  function getMode(cm, pos) {
	    var mode = cm.getMode()
	    return mode.useInnerComments === false || !mode.innerMode ? mode : cm.getModeAt(pos)
	  }

	  CodeMirror.defineExtension("lineComment", function(from, to, options) {
	    if (!options) options = noOptions;
	    var self = this, mode = getMode(self, from);
	    var firstLine = self.getLine(from.line);
	    if (firstLine == null || probablyInsideString(self, from, firstLine)) return;

	    var commentString = options.lineComment || mode.lineComment;
	    if (!commentString) {
	      if (options.blockCommentStart || mode.blockCommentStart) {
	        options.fullLines = true;
	        self.blockComment(from, to, options);
	      }
	      return;
	    }

	    var end = Math.min(to.ch != 0 || to.line == from.line ? to.line + 1 : to.line, self.lastLine() + 1);
	    var pad = options.padding == null ? " " : options.padding;
	    var blankLines = options.commentBlankLines || from.line == to.line;

	    self.operation(function() {
	      if (options.indent) {
	        var baseString = null;
	        for (var i = from.line; i < end; ++i) {
	          var line = self.getLine(i);
	          var whitespace = line.slice(0, firstNonWS(line));
	          if (baseString == null || baseString.length > whitespace.length) {
	            baseString = whitespace;
	          }
	        }
	        for (var i = from.line; i < end; ++i) {
	          var line = self.getLine(i), cut = baseString.length;
	          if (!blankLines && !nonWS.test(line)) continue;
	          if (line.slice(0, cut) != baseString) cut = firstNonWS(line);
	          self.replaceRange(baseString + commentString + pad, Pos(i, 0), Pos(i, cut));
	        }
	      } else {
	        for (var i = from.line; i < end; ++i) {
	          if (blankLines || nonWS.test(self.getLine(i)))
	            self.replaceRange(commentString + pad, Pos(i, 0));
	        }
	      }
	    });
	  });

	  CodeMirror.defineExtension("blockComment", function(from, to, options) {
	    if (!options) options = noOptions;
	    var self = this, mode = getMode(self, from);
	    var startString = options.blockCommentStart || mode.blockCommentStart;
	    var endString = options.blockCommentEnd || mode.blockCommentEnd;
	    if (!startString || !endString) {
	      if ((options.lineComment || mode.lineComment) && options.fullLines != false)
	        self.lineComment(from, to, options);
	      return;
	    }
	    if (/\bcomment\b/.test(self.getTokenTypeAt(Pos(from.line, 0)))) return

	    var end = Math.min(to.line, self.lastLine());
	    if (end != from.line && to.ch == 0 && nonWS.test(self.getLine(end))) --end;

	    var pad = options.padding == null ? " " : options.padding;
	    if (from.line > end) return;

	    self.operation(function() {
	      if (options.fullLines != false) {
	        var lastLineHasText = nonWS.test(self.getLine(end));
	        self.replaceRange(pad + endString, Pos(end));
	        self.replaceRange(startString + pad, Pos(from.line, 0));
	        var lead = options.blockCommentLead || mode.blockCommentLead;
	        if (lead != null) for (var i = from.line + 1; i <= end; ++i)
	          if (i != end || lastLineHasText)
	            self.replaceRange(lead + pad, Pos(i, 0));
	      } else {
	        self.replaceRange(endString, to);
	        self.replaceRange(startString, from);
	      }
	    });
	  });

	  CodeMirror.defineExtension("uncomment", function(from, to, options) {
	    if (!options) options = noOptions;
	    var self = this, mode = getMode(self, from);
	    var end = Math.min(to.ch != 0 || to.line == from.line ? to.line : to.line - 1, self.lastLine()), start = Math.min(from.line, end);

	    // Try finding line comments
	    var lineString = options.lineComment || mode.lineComment, lines = [];
	    var pad = options.padding == null ? " " : options.padding, didSomething;
	    lineComment: {
	      if (!lineString) break lineComment;
	      for (var i = start; i <= end; ++i) {
	        var line = self.getLine(i);
	        var found = line.indexOf(lineString);
	        if (found > -1 && !/comment/.test(self.getTokenTypeAt(Pos(i, found + 1)))) found = -1;
	        if (found == -1 && nonWS.test(line)) break lineComment;
	        if (found > -1 && nonWS.test(line.slice(0, found))) break lineComment;
	        lines.push(line);
	      }
	      self.operation(function() {
	        for (var i = start; i <= end; ++i) {
	          var line = lines[i - start];
	          var pos = line.indexOf(lineString), endPos = pos + lineString.length;
	          if (pos < 0) continue;
	          if (line.slice(endPos, endPos + pad.length) == pad) endPos += pad.length;
	          didSomething = true;
	          self.replaceRange("", Pos(i, pos), Pos(i, endPos));
	        }
	      });
	      if (didSomething) return true;
	    }

	    // Try block comments
	    var startString = options.blockCommentStart || mode.blockCommentStart;
	    var endString = options.blockCommentEnd || mode.blockCommentEnd;
	    if (!startString || !endString) return false;
	    var lead = options.blockCommentLead || mode.blockCommentLead;
	    var startLine = self.getLine(start), open = startLine.indexOf(startString)
	    if (open == -1) return false
	    var endLine = end == start ? startLine : self.getLine(end)
	    var close = endLine.indexOf(endString, end == start ? open + startString.length : 0);
	    var insideStart = Pos(start, open + 1), insideEnd = Pos(end, close + 1)
	    if (close == -1 ||
	        !/comment/.test(self.getTokenTypeAt(insideStart)) ||
	        !/comment/.test(self.getTokenTypeAt(insideEnd)) ||
	        self.getRange(insideStart, insideEnd, "\n").indexOf(endString) > -1)
	      return false;

	    // Avoid killing block comments completely outside the selection.
	    // Positions of the last startString before the start of the selection, and the first endString after it.
	    var lastStart = startLine.lastIndexOf(startString, from.ch);
	    var firstEnd = lastStart == -1 ? -1 : startLine.slice(0, from.ch).indexOf(endString, lastStart + startString.length);
	    if (lastStart != -1 && firstEnd != -1 && firstEnd + endString.length != from.ch) return false;
	    // Positions of the first endString after the end of the selection, and the last startString before it.
	    firstEnd = endLine.indexOf(endString, to.ch);
	    var almostLastStart = endLine.slice(to.ch).lastIndexOf(startString, firstEnd - to.ch);
	    lastStart = (firstEnd == -1 || almostLastStart == -1) ? -1 : to.ch + almostLastStart;
	    if (firstEnd != -1 && lastStart != -1 && lastStart != to.ch) return false;

	    self.operation(function() {
	      self.replaceRange("", Pos(end, close - (pad && endLine.slice(close - pad.length, close) == pad ? pad.length : 0)),
	                        Pos(end, close + endString.length));
	      var openEnd = open + startString.length;
	      if (pad && startLine.slice(openEnd, openEnd + pad.length) == pad) openEnd += pad.length;
	      self.replaceRange("", Pos(start, open), Pos(start, openEnd));
	      if (lead) for (var i = start + 1; i <= end; ++i) {
	        var line = self.getLine(i), found = line.indexOf(lead);
	        if (found == -1 || nonWS.test(line.slice(0, found))) continue;
	        var foundEnd = found + lead.length;
	        if (pad && line.slice(foundEnd, foundEnd + pad.length) == pad) foundEnd += pad.length;
	        self.replaceRange("", Pos(i, found), Pos(i, foundEnd));
	      }
	    });
	    return true;
	  });
	});


/***/ }),
/* 8 */
/***/ (function(module, exports, __webpack_require__) {

	// CodeMirror, copyright (c) by Marijn Haverbeke and others
	// Distributed under an MIT license: http://codemirror.net/LICENSE

	(function(mod) {
	  if (true) // CommonJS
	    mod(__webpack_require__(2));
	  else if (typeof define == "function" && define.amd) // AMD
	    define(["../../lib/codemirror"], mod);
	  else // Plain browser env
	    mod(CodeMirror);
	})(function(CodeMirror) {
	"use strict";

	CodeMirror.defineMode("javascript", function(config, parserConfig) {
	  var indentUnit = config.indentUnit;
	  var statementIndent = parserConfig.statementIndent;
	  var jsonldMode = parserConfig.jsonld;
	  var jsonMode = parserConfig.json || jsonldMode;
	  var isTS = parserConfig.typescript;
	  var wordRE = parserConfig.wordCharacters || /[\w$\xa1-\uffff]/;

	  // Tokenizer

	  var keywords = function(){
	    function kw(type) {return {type: type, style: "keyword"};}
	    var A = kw("keyword a"), B = kw("keyword b"), C = kw("keyword c"), D = kw("keyword d");
	    var operator = kw("operator"), atom = {type: "atom", style: "atom"};

	    var jsKeywords = {
	      "if": kw("if"), "while": A, "with": A, "else": B, "do": B, "try": B, "finally": B,
	      "return": D, "break": D, "continue": D, "new": kw("new"), "delete": C, "void": C, "throw": C,
	      "debugger": kw("debugger"), "var": kw("var"), "const": kw("var"), "let": kw("var"),
	      "function": kw("function"), "catch": kw("catch"),
	      "for": kw("for"), "switch": kw("switch"), "case": kw("case"), "default": kw("default"),
	      "in": operator, "typeof": operator, "instanceof": operator,
	      "true": atom, "false": atom, "null": atom, "undefined": atom, "NaN": atom, "Infinity": atom,
	      "this": kw("this"), "class": kw("class"), "super": kw("atom"),
	      "yield": C, "export": kw("export"), "import": kw("import"), "extends": C,
	      "await": C
	    };

	    // Extend the 'normal' keywords with the TypeScript language extensions
	    if (isTS) {
	      var type = {type: "variable", style: "type"};
	      var tsKeywords = {
	        // object-like things
	        "interface": kw("class"),
	        "implements": C,
	        "namespace": C,

	        // scope modifiers
	        "public": kw("modifier"),
	        "private": kw("modifier"),
	        "protected": kw("modifier"),
	        "abstract": kw("modifier"),
	        "readonly": kw("modifier"),

	        // types
	        "string": type, "number": type, "boolean": type, "any": type
	      };

	      for (var attr in tsKeywords) {
	        jsKeywords[attr] = tsKeywords[attr];
	      }
	    }

	    return jsKeywords;
	  }();

	  var isOperatorChar = /[+\-*&%=<>!?|~^@]/;
	  var isJsonldKeyword = /^@(context|id|value|language|type|container|list|set|reverse|index|base|vocab|graph)"/;

	  function readRegexp(stream) {
	    var escaped = false, next, inSet = false;
	    while ((next = stream.next()) != null) {
	      if (!escaped) {
	        if (next == "/" && !inSet) return;
	        if (next == "[") inSet = true;
	        else if (inSet && next == "]") inSet = false;
	      }
	      escaped = !escaped && next == "\\";
	    }
	  }

	  // Used as scratch variables to communicate multiple values without
	  // consing up tons of objects.
	  var type, content;
	  function ret(tp, style, cont) {
	    type = tp; content = cont;
	    return style;
	  }
	  function tokenBase(stream, state) {
	    var ch = stream.next();
	    if (ch == '"' || ch == "'") {
	      state.tokenize = tokenString(ch);
	      return state.tokenize(stream, state);
	    } else if (ch == "." && stream.match(/^\d+(?:[eE][+\-]?\d+)?/)) {
	      return ret("number", "number");
	    } else if (ch == "." && stream.match("..")) {
	      return ret("spread", "meta");
	    } else if (/[\[\]{}\(\),;\:\.]/.test(ch)) {
	      return ret(ch);
	    } else if (ch == "=" && stream.eat(">")) {
	      return ret("=>", "operator");
	    } else if (ch == "0" && stream.eat(/x/i)) {
	      stream.eatWhile(/[\da-f]/i);
	      return ret("number", "number");
	    } else if (ch == "0" && stream.eat(/o/i)) {
	      stream.eatWhile(/[0-7]/i);
	      return ret("number", "number");
	    } else if (ch == "0" && stream.eat(/b/i)) {
	      stream.eatWhile(/[01]/i);
	      return ret("number", "number");
	    } else if (/\d/.test(ch)) {
	      stream.match(/^\d*(?:\.\d*)?(?:[eE][+\-]?\d+)?/);
	      return ret("number", "number");
	    } else if (ch == "/") {
	      if (stream.eat("*")) {
	        state.tokenize = tokenComment;
	        return tokenComment(stream, state);
	      } else if (stream.eat("/")) {
	        stream.skipToEnd();
	        return ret("comment", "comment");
	      } else if (expressionAllowed(stream, state, 1)) {
	        readRegexp(stream);
	        stream.match(/^\b(([gimyu])(?![gimyu]*\2))+\b/);
	        return ret("regexp", "string-2");
	      } else {
	        stream.eat("=");
	        return ret("operator", "operator", stream.current());
	      }
	    } else if (ch == "`") {
	      state.tokenize = tokenQuasi;
	      return tokenQuasi(stream, state);
	    } else if (ch == "#") {
	      stream.skipToEnd();
	      return ret("error", "error");
	    } else if (isOperatorChar.test(ch)) {
	      if (ch != ">" || !state.lexical || state.lexical.type != ">") {
	        if (stream.eat("=")) {
	          if (ch == "!" || ch == "=") stream.eat("=")
	        } else if (/[<>*+\-]/.test(ch)) {
	          stream.eat(ch)
	          if (ch == ">") stream.eat(ch)
	        }
	      }
	      return ret("operator", "operator", stream.current());
	    } else if (wordRE.test(ch)) {
	      stream.eatWhile(wordRE);
	      var word = stream.current()
	      if (state.lastType != ".") {
	        if (keywords.propertyIsEnumerable(word)) {
	          var kw = keywords[word]
	          return ret(kw.type, kw.style, word)
	        }
	        if (word == "async" && stream.match(/^(\s|\/\*.*?\*\/)*[\(\w]/, false))
	          return ret("async", "keyword", word)
	      }
	      return ret("variable", "variable", word)
	    }
	  }

	  function tokenString(quote) {
	    return function(stream, state) {
	      var escaped = false, next;
	      if (jsonldMode && stream.peek() == "@" && stream.match(isJsonldKeyword)){
	        state.tokenize = tokenBase;
	        return ret("jsonld-keyword", "meta");
	      }
	      while ((next = stream.next()) != null) {
	        if (next == quote && !escaped) break;
	        escaped = !escaped && next == "\\";
	      }
	      if (!escaped) state.tokenize = tokenBase;
	      return ret("string", "string");
	    };
	  }

	  function tokenComment(stream, state) {
	    var maybeEnd = false, ch;
	    while (ch = stream.next()) {
	      if (ch == "/" && maybeEnd) {
	        state.tokenize = tokenBase;
	        break;
	      }
	      maybeEnd = (ch == "*");
	    }
	    return ret("comment", "comment");
	  }

	  function tokenQuasi(stream, state) {
	    var escaped = false, next;
	    while ((next = stream.next()) != null) {
	      if (!escaped && (next == "`" || next == "$" && stream.eat("{"))) {
	        state.tokenize = tokenBase;
	        break;
	      }
	      escaped = !escaped && next == "\\";
	    }
	    return ret("quasi", "string-2", stream.current());
	  }

	  var brackets = "([{}])";
	  // This is a crude lookahead trick to try and notice that we're
	  // parsing the argument patterns for a fat-arrow function before we
	  // actually hit the arrow token. It only works if the arrow is on
	  // the same line as the arguments and there's no strange noise
	  // (comments) in between. Fallback is to only notice when we hit the
	  // arrow, and not declare the arguments as locals for the arrow
	  // body.
	  function findFatArrow(stream, state) {
	    if (state.fatArrowAt) state.fatArrowAt = null;
	    var arrow = stream.string.indexOf("=>", stream.start);
	    if (arrow < 0) return;

	    if (isTS) { // Try to skip TypeScript return type declarations after the arguments
	      var m = /:\s*(?:\w+(?:<[^>]*>|\[\])?|\{[^}]*\})\s*$/.exec(stream.string.slice(stream.start, arrow))
	      if (m) arrow = m.index
	    }

	    var depth = 0, sawSomething = false;
	    for (var pos = arrow - 1; pos >= 0; --pos) {
	      var ch = stream.string.charAt(pos);
	      var bracket = brackets.indexOf(ch);
	      if (bracket >= 0 && bracket < 3) {
	        if (!depth) { ++pos; break; }
	        if (--depth == 0) { if (ch == "(") sawSomething = true; break; }
	      } else if (bracket >= 3 && bracket < 6) {
	        ++depth;
	      } else if (wordRE.test(ch)) {
	        sawSomething = true;
	      } else if (/["'\/]/.test(ch)) {
	        return;
	      } else if (sawSomething && !depth) {
	        ++pos;
	        break;
	      }
	    }
	    if (sawSomething && !depth) state.fatArrowAt = pos;
	  }

	  // Parser

	  var atomicTypes = {"atom": true, "number": true, "variable": true, "string": true, "regexp": true, "this": true, "jsonld-keyword": true};

	  function JSLexical(indented, column, type, align, prev, info) {
	    this.indented = indented;
	    this.column = column;
	    this.type = type;
	    this.prev = prev;
	    this.info = info;
	    if (align != null) this.align = align;
	  }

	  function inScope(state, varname) {
	    for (var v = state.localVars; v; v = v.next)
	      if (v.name == varname) return true;
	    for (var cx = state.context; cx; cx = cx.prev) {
	      for (var v = cx.vars; v; v = v.next)
	        if (v.name == varname) return true;
	    }
	  }

	  function parseJS(state, style, type, content, stream) {
	    var cc = state.cc;
	    // Communicate our context to the combinators.
	    // (Less wasteful than consing up a hundred closures on every call.)
	    cx.state = state; cx.stream = stream; cx.marked = null, cx.cc = cc; cx.style = style;

	    if (!state.lexical.hasOwnProperty("align"))
	      state.lexical.align = true;

	    while(true) {
	      var combinator = cc.length ? cc.pop() : jsonMode ? expression : statement;
	      if (combinator(type, content)) {
	        while(cc.length && cc[cc.length - 1].lex)
	          cc.pop()();
	        if (cx.marked) return cx.marked;
	        if (type == "variable" && inScope(state, content)) return "variable-2";
	        return style;
	      }
	    }
	  }

	  // Combinator utils

	  var cx = {state: null, column: null, marked: null, cc: null};
	  function pass() {
	    for (var i = arguments.length - 1; i >= 0; i--) cx.cc.push(arguments[i]);
	  }
	  function cont() {
	    pass.apply(null, arguments);
	    return true;
	  }
	  function register(varname) {
	    function inList(list) {
	      for (var v = list; v; v = v.next)
	        if (v.name == varname) return true;
	      return false;
	    }
	    var state = cx.state;
	    cx.marked = "def";
	    if (state.context) {
	      if (inList(state.localVars)) return;
	      state.localVars = {name: varname, next: state.localVars};
	    } else {
	      if (inList(state.globalVars)) return;
	      if (parserConfig.globalVars)
	        state.globalVars = {name: varname, next: state.globalVars};
	    }
	  }

	  // Combinators

	  var defaultVars = {name: "this", next: {name: "arguments"}};
	  function pushcontext() {
	    cx.state.context = {prev: cx.state.context, vars: cx.state.localVars};
	    cx.state.localVars = defaultVars;
	  }
	  function popcontext() {
	    cx.state.localVars = cx.state.context.vars;
	    cx.state.context = cx.state.context.prev;
	  }
	  function pushlex(type, info) {
	    var result = function() {
	      var state = cx.state, indent = state.indented;
	      if (state.lexical.type == "stat") indent = state.lexical.indented;
	      else for (var outer = state.lexical; outer && outer.type == ")" && outer.align; outer = outer.prev)
	        indent = outer.indented;
	      state.lexical = new JSLexical(indent, cx.stream.column(), type, null, state.lexical, info);
	    };
	    result.lex = true;
	    return result;
	  }
	  function poplex() {
	    var state = cx.state;
	    if (state.lexical.prev) {
	      if (state.lexical.type == ")")
	        state.indented = state.lexical.indented;
	      state.lexical = state.lexical.prev;
	    }
	  }
	  poplex.lex = true;

	  function expect(wanted) {
	    function exp(type) {
	      if (type == wanted) return cont();
	      else if (wanted == ";") return pass();
	      else return cont(exp);
	    };
	    return exp;
	  }

	  function statement(type, value) {
	    if (type == "var") return cont(pushlex("vardef", value.length), vardef, expect(";"), poplex);
	    if (type == "keyword a") return cont(pushlex("form"), parenExpr, statement, poplex);
	    if (type == "keyword b") return cont(pushlex("form"), statement, poplex);
	    if (type == "keyword d") return cx.stream.match(/^\s*$/, false) ? cont() : cont(pushlex("stat"), maybeexpression, expect(";"), poplex);
	    if (type == "debugger") return cont(expect(";"));
	    if (type == "{") return cont(pushlex("}"), block, poplex);
	    if (type == ";") return cont();
	    if (type == "if") {
	      if (cx.state.lexical.info == "else" && cx.state.cc[cx.state.cc.length - 1] == poplex)
	        cx.state.cc.pop()();
	      return cont(pushlex("form"), parenExpr, statement, poplex, maybeelse);
	    }
	    if (type == "function") return cont(functiondef);
	    if (type == "for") return cont(pushlex("form"), forspec, statement, poplex);
	    if (type == "variable") {
	      if (isTS && value == "type") {
	        cx.marked = "keyword"
	        return cont(typeexpr, expect("operator"), typeexpr, expect(";"));
	      } else if (isTS && value == "declare") {
	        cx.marked = "keyword"
	        return cont(statement)
	      } else if (isTS && (value == "module" || value == "enum") && cx.stream.match(/^\s*\w/, false)) {
	        cx.marked = "keyword"
	        return cont(pushlex("form"), pattern, expect("{"), pushlex("}"), block, poplex, poplex)
	      } else {
	        return cont(pushlex("stat"), maybelabel);
	      }
	    }
	    if (type == "switch") return cont(pushlex("form"), parenExpr, expect("{"), pushlex("}", "switch"),
	                                      block, poplex, poplex);
	    if (type == "case") return cont(expression, expect(":"));
	    if (type == "default") return cont(expect(":"));
	    if (type == "catch") return cont(pushlex("form"), pushcontext, expect("("), funarg, expect(")"),
	                                     statement, poplex, popcontext);
	    if (type == "class") return cont(pushlex("form"), className, poplex);
	    if (type == "export") return cont(pushlex("stat"), afterExport, poplex);
	    if (type == "import") return cont(pushlex("stat"), afterImport, poplex);
	    if (type == "async") return cont(statement)
	    if (value == "@") return cont(expression, statement)
	    return pass(pushlex("stat"), expression, expect(";"), poplex);
	  }
	  function expression(type) {
	    return expressionInner(type, false);
	  }
	  function expressionNoComma(type) {
	    return expressionInner(type, true);
	  }
	  function parenExpr(type) {
	    if (type != "(") return pass()
	    return cont(pushlex(")"), expression, expect(")"), poplex)
	  }
	  function expressionInner(type, noComma) {
	    if (cx.state.fatArrowAt == cx.stream.start) {
	      var body = noComma ? arrowBodyNoComma : arrowBody;
	      if (type == "(") return cont(pushcontext, pushlex(")"), commasep(funarg, ")"), poplex, expect("=>"), body, popcontext);
	      else if (type == "variable") return pass(pushcontext, pattern, expect("=>"), body, popcontext);
	    }

	    var maybeop = noComma ? maybeoperatorNoComma : maybeoperatorComma;
	    if (atomicTypes.hasOwnProperty(type)) return cont(maybeop);
	    if (type == "function") return cont(functiondef, maybeop);
	    if (type == "class") return cont(pushlex("form"), classExpression, poplex);
	    if (type == "keyword c" || type == "async") return cont(noComma ? expressionNoComma : expression);
	    if (type == "(") return cont(pushlex(")"), maybeexpression, expect(")"), poplex, maybeop);
	    if (type == "operator" || type == "spread") return cont(noComma ? expressionNoComma : expression);
	    if (type == "[") return cont(pushlex("]"), arrayLiteral, poplex, maybeop);
	    if (type == "{") return contCommasep(objprop, "}", null, maybeop);
	    if (type == "quasi") return pass(quasi, maybeop);
	    if (type == "new") return cont(maybeTarget(noComma));
	    return cont();
	  }
	  function maybeexpression(type) {
	    if (type.match(/[;\}\)\],]/)) return pass();
	    return pass(expression);
	  }

	  function maybeoperatorComma(type, value) {
	    if (type == ",") return cont(expression);
	    return maybeoperatorNoComma(type, value, false);
	  }
	  function maybeoperatorNoComma(type, value, noComma) {
	    var me = noComma == false ? maybeoperatorComma : maybeoperatorNoComma;
	    var expr = noComma == false ? expression : expressionNoComma;
	    if (type == "=>") return cont(pushcontext, noComma ? arrowBodyNoComma : arrowBody, popcontext);
	    if (type == "operator") {
	      if (/\+\+|--/.test(value) || isTS && value == "!") return cont(me);
	      if (isTS && value == "<" && cx.stream.match(/^([^>]|<.*?>)*>\s*\(/, false))
	        return cont(pushlex(">"), commasep(typeexpr, ">"), poplex, me);
	      if (value == "?") return cont(expression, expect(":"), expr);
	      return cont(expr);
	    }
	    if (type == "quasi") { return pass(quasi, me); }
	    if (type == ";") return;
	    if (type == "(") return contCommasep(expressionNoComma, ")", "call", me);
	    if (type == ".") return cont(property, me);
	    if (type == "[") return cont(pushlex("]"), maybeexpression, expect("]"), poplex, me);
	    if (isTS && value == "as") { cx.marked = "keyword"; return cont(typeexpr, me) }
	    if (type == "regexp") {
	      cx.state.lastType = cx.marked = "operator"
	      cx.stream.backUp(cx.stream.pos - cx.stream.start - 1)
	      return cont(expr)
	    }
	  }
	  function quasi(type, value) {
	    if (type != "quasi") return pass();
	    if (value.slice(value.length - 2) != "${") return cont(quasi);
	    return cont(expression, continueQuasi);
	  }
	  function continueQuasi(type) {
	    if (type == "}") {
	      cx.marked = "string-2";
	      cx.state.tokenize = tokenQuasi;
	      return cont(quasi);
	    }
	  }
	  function arrowBody(type) {
	    findFatArrow(cx.stream, cx.state);
	    return pass(type == "{" ? statement : expression);
	  }
	  function arrowBodyNoComma(type) {
	    findFatArrow(cx.stream, cx.state);
	    return pass(type == "{" ? statement : expressionNoComma);
	  }
	  function maybeTarget(noComma) {
	    return function(type) {
	      if (type == ".") return cont(noComma ? targetNoComma : target);
	      else if (type == "variable" && isTS) return cont(maybeTypeArgs, noComma ? maybeoperatorNoComma : maybeoperatorComma)
	      else return pass(noComma ? expressionNoComma : expression);
	    };
	  }
	  function target(_, value) {
	    if (value == "target") { cx.marked = "keyword"; return cont(maybeoperatorComma); }
	  }
	  function targetNoComma(_, value) {
	    if (value == "target") { cx.marked = "keyword"; return cont(maybeoperatorNoComma); }
	  }
	  function maybelabel(type) {
	    if (type == ":") return cont(poplex, statement);
	    return pass(maybeoperatorComma, expect(";"), poplex);
	  }
	  function property(type) {
	    if (type == "variable") {cx.marked = "property"; return cont();}
	  }
	  function objprop(type, value) {
	    if (type == "async") {
	      cx.marked = "property";
	      return cont(objprop);
	    } else if (type == "variable" || cx.style == "keyword") {
	      cx.marked = "property";
	      if (value == "get" || value == "set") return cont(getterSetter);
	      var m // Work around fat-arrow-detection complication for detecting typescript typed arrow params
	      if (isTS && cx.state.fatArrowAt == cx.stream.start && (m = cx.stream.match(/^\s*:\s*/, false)))
	        cx.state.fatArrowAt = cx.stream.pos + m[0].length
	      return cont(afterprop);
	    } else if (type == "number" || type == "string") {
	      cx.marked = jsonldMode ? "property" : (cx.style + " property");
	      return cont(afterprop);
	    } else if (type == "jsonld-keyword") {
	      return cont(afterprop);
	    } else if (type == "modifier") {
	      return cont(objprop)
	    } else if (type == "[") {
	      return cont(expression, expect("]"), afterprop);
	    } else if (type == "spread") {
	      return cont(expressionNoComma, afterprop);
	    } else if (value == "*") {
	      cx.marked = "keyword";
	      return cont(objprop);
	    } else if (type == ":") {
	      return pass(afterprop)
	    }
	  }
	  function getterSetter(type) {
	    if (type != "variable") return pass(afterprop);
	    cx.marked = "property";
	    return cont(functiondef);
	  }
	  function afterprop(type) {
	    if (type == ":") return cont(expressionNoComma);
	    if (type == "(") return pass(functiondef);
	  }
	  function commasep(what, end, sep) {
	    function proceed(type, value) {
	      if (sep ? sep.indexOf(type) > -1 : type == ",") {
	        var lex = cx.state.lexical;
	        if (lex.info == "call") lex.pos = (lex.pos || 0) + 1;
	        return cont(function(type, value) {
	          if (type == end || value == end) return pass()
	          return pass(what)
	        }, proceed);
	      }
	      if (type == end || value == end) return cont();
	      return cont(expect(end));
	    }
	    return function(type, value) {
	      if (type == end || value == end) return cont();
	      return pass(what, proceed);
	    };
	  }
	  function contCommasep(what, end, info) {
	    for (var i = 3; i < arguments.length; i++)
	      cx.cc.push(arguments[i]);
	    return cont(pushlex(end, info), commasep(what, end), poplex);
	  }
	  function block(type) {
	    if (type == "}") return cont();
	    return pass(statement, block);
	  }
	  function maybetype(type, value) {
	    if (isTS) {
	      if (type == ":") return cont(typeexpr);
	      if (value == "?") return cont(maybetype);
	    }
	  }
	  function mayberettype(type) {
	    if (isTS && type == ":") {
	      if (cx.stream.match(/^\s*\w+\s+is\b/, false)) return cont(expression, isKW, typeexpr)
	      else return cont(typeexpr)
	    }
	  }
	  function isKW(_, value) {
	    if (value == "is") {
	      cx.marked = "keyword"
	      return cont()
	    }
	  }
	  function typeexpr(type, value) {
	    if (type == "variable" || value == "void") {
	      if (value == "keyof") {
	        cx.marked = "keyword"
	        return cont(typeexpr)
	      } else {
	        cx.marked = "type"
	        return cont(afterType)
	      }
	    }
	    if (type == "string" || type == "number" || type == "atom") return cont(afterType);
	    if (type == "[") return cont(pushlex("]"), commasep(typeexpr, "]", ","), poplex, afterType)
	    if (type == "{") return cont(pushlex("}"), commasep(typeprop, "}", ",;"), poplex, afterType)
	    if (type == "(") return cont(commasep(typearg, ")"), maybeReturnType)
	  }
	  function maybeReturnType(type) {
	    if (type == "=>") return cont(typeexpr)
	  }
	  function typeprop(type, value) {
	    if (type == "variable" || cx.style == "keyword") {
	      cx.marked = "property"
	      return cont(typeprop)
	    } else if (value == "?") {
	      return cont(typeprop)
	    } else if (type == ":") {
	      return cont(typeexpr)
	    } else if (type == "[") {
	      return cont(expression, maybetype, expect("]"), typeprop)
	    }
	  }
	  function typearg(type) {
	    if (type == "variable") return cont(typearg)
	    else if (type == ":") return cont(typeexpr)
	  }
	  function afterType(type, value) {
	    if (value == "<") return cont(pushlex(">"), commasep(typeexpr, ">"), poplex, afterType)
	    if (value == "|" || type == ".") return cont(typeexpr)
	    if (type == "[") return cont(expect("]"), afterType)
	    if (value == "extends") return cont(typeexpr)
	  }
	  function maybeTypeArgs(_, value) {
	    if (value == "<") return cont(pushlex(">"), commasep(typeexpr, ">"), poplex, afterType)
	  }
	  function typeparam() {
	    return pass(typeexpr, maybeTypeDefault)
	  }
	  function maybeTypeDefault(_, value) {
	    if (value == "=") return cont(typeexpr)
	  }
	  function vardef() {
	    return pass(pattern, maybetype, maybeAssign, vardefCont);
	  }
	  function pattern(type, value) {
	    if (type == "modifier") return cont(pattern)
	    if (type == "variable") { register(value); return cont(); }
	    if (type == "spread") return cont(pattern);
	    if (type == "[") return contCommasep(pattern, "]");
	    if (type == "{") return contCommasep(proppattern, "}");
	  }
	  function proppattern(type, value) {
	    if (type == "variable" && !cx.stream.match(/^\s*:/, false)) {
	      register(value);
	      return cont(maybeAssign);
	    }
	    if (type == "variable") cx.marked = "property";
	    if (type == "spread") return cont(pattern);
	    if (type == "}") return pass();
	    return cont(expect(":"), pattern, maybeAssign);
	  }
	  function maybeAssign(_type, value) {
	    if (value == "=") return cont(expressionNoComma);
	  }
	  function vardefCont(type) {
	    if (type == ",") return cont(vardef);
	  }
	  function maybeelse(type, value) {
	    if (type == "keyword b" && value == "else") return cont(pushlex("form", "else"), statement, poplex);
	  }
	  function forspec(type) {
	    if (type == "(") return cont(pushlex(")"), forspec1, expect(")"), poplex);
	  }
	  function forspec1(type) {
	    if (type == "var") return cont(vardef, expect(";"), forspec2);
	    if (type == ";") return cont(forspec2);
	    if (type == "variable") return cont(formaybeinof);
	    return pass(expression, expect(";"), forspec2);
	  }
	  function formaybeinof(_type, value) {
	    if (value == "in" || value == "of") { cx.marked = "keyword"; return cont(expression); }
	    return cont(maybeoperatorComma, forspec2);
	  }
	  function forspec2(type, value) {
	    if (type == ";") return cont(forspec3);
	    if (value == "in" || value == "of") { cx.marked = "keyword"; return cont(expression); }
	    return pass(expression, expect(";"), forspec3);
	  }
	  function forspec3(type) {
	    if (type != ")") cont(expression);
	  }
	  function functiondef(type, value) {
	    if (value == "*") {cx.marked = "keyword"; return cont(functiondef);}
	    if (type == "variable") {register(value); return cont(functiondef);}
	    if (type == "(") return cont(pushcontext, pushlex(")"), commasep(funarg, ")"), poplex, mayberettype, statement, popcontext);
	    if (isTS && value == "<") return cont(pushlex(">"), commasep(typeparam, ">"), poplex, functiondef)
	  }
	  function funarg(type, value) {
	    if (value == "@") cont(expression, funarg)
	    if (type == "spread" || type == "modifier") return cont(funarg);
	    return pass(pattern, maybetype, maybeAssign);
	  }
	  function classExpression(type, value) {
	    // Class expressions may have an optional name.
	    if (type == "variable") return className(type, value);
	    return classNameAfter(type, value);
	  }
	  function className(type, value) {
	    if (type == "variable") {register(value); return cont(classNameAfter);}
	  }
	  function classNameAfter(type, value) {
	    if (value == "<") return cont(pushlex(">"), commasep(typeparam, ">"), poplex, classNameAfter)
	    if (value == "extends" || value == "implements" || (isTS && type == ","))
	      return cont(isTS ? typeexpr : expression, classNameAfter);
	    if (type == "{") return cont(pushlex("}"), classBody, poplex);
	  }
	  function classBody(type, value) {
	    if (type == "modifier" || type == "async" ||
	        (type == "variable" &&
	         (value == "static" || value == "get" || value == "set") &&
	         cx.stream.match(/^\s+[\w$\xa1-\uffff]/, false))) {
	      cx.marked = "keyword";
	      return cont(classBody);
	    }
	    if (type == "variable" || cx.style == "keyword") {
	      cx.marked = "property";
	      return cont(isTS ? classfield : functiondef, classBody);
	    }
	    if (type == "[")
	      return cont(expression, expect("]"), isTS ? classfield : functiondef, classBody)
	    if (value == "*") {
	      cx.marked = "keyword";
	      return cont(classBody);
	    }
	    if (type == ";") return cont(classBody);
	    if (type == "}") return cont();
	    if (value == "@") return cont(expression, classBody)
	  }
	  function classfield(type, value) {
	    if (value == "?") return cont(classfield)
	    if (type == ":") return cont(typeexpr, maybeAssign)
	    if (value == "=") return cont(expressionNoComma)
	    return pass(functiondef)
	  }
	  function afterExport(type, value) {
	    if (value == "*") { cx.marked = "keyword"; return cont(maybeFrom, expect(";")); }
	    if (value == "default") { cx.marked = "keyword"; return cont(expression, expect(";")); }
	    if (type == "{") return cont(commasep(exportField, "}"), maybeFrom, expect(";"));
	    return pass(statement);
	  }
	  function exportField(type, value) {
	    if (value == "as") { cx.marked = "keyword"; return cont(expect("variable")); }
	    if (type == "variable") return pass(expressionNoComma, exportField);
	  }
	  function afterImport(type) {
	    if (type == "string") return cont();
	    return pass(importSpec, maybeMoreImports, maybeFrom);
	  }
	  function importSpec(type, value) {
	    if (type == "{") return contCommasep(importSpec, "}");
	    if (type == "variable") register(value);
	    if (value == "*") cx.marked = "keyword";
	    return cont(maybeAs);
	  }
	  function maybeMoreImports(type) {
	    if (type == ",") return cont(importSpec, maybeMoreImports)
	  }
	  function maybeAs(_type, value) {
	    if (value == "as") { cx.marked = "keyword"; return cont(importSpec); }
	  }
	  function maybeFrom(_type, value) {
	    if (value == "from") { cx.marked = "keyword"; return cont(expression); }
	  }
	  function arrayLiteral(type) {
	    if (type == "]") return cont();
	    return pass(commasep(expressionNoComma, "]"));
	  }

	  function isContinuedStatement(state, textAfter) {
	    return state.lastType == "operator" || state.lastType == "," ||
	      isOperatorChar.test(textAfter.charAt(0)) ||
	      /[,.]/.test(textAfter.charAt(0));
	  }

	  function expressionAllowed(stream, state, backUp) {
	    return state.tokenize == tokenBase &&
	      /^(?:operator|sof|keyword [bcd]|case|new|export|default|spread|[\[{}\(,;:]|=>)$/.test(state.lastType) ||
	      (state.lastType == "quasi" && /\{\s*$/.test(stream.string.slice(0, stream.pos - (backUp || 0))))
	  }

	  // Interface

	  return {
	    startState: function(basecolumn) {
	      var state = {
	        tokenize: tokenBase,
	        lastType: "sof",
	        cc: [],
	        lexical: new JSLexical((basecolumn || 0) - indentUnit, 0, "block", false),
	        localVars: parserConfig.localVars,
	        context: parserConfig.localVars && {vars: parserConfig.localVars},
	        indented: basecolumn || 0
	      };
	      if (parserConfig.globalVars && typeof parserConfig.globalVars == "object")
	        state.globalVars = parserConfig.globalVars;
	      return state;
	    },

	    token: function(stream, state) {
	      if (stream.sol()) {
	        if (!state.lexical.hasOwnProperty("align"))
	          state.lexical.align = false;
	        state.indented = stream.indentation();
	        findFatArrow(stream, state);
	      }
	      if (state.tokenize != tokenComment && stream.eatSpace()) return null;
	      var style = state.tokenize(stream, state);
	      if (type == "comment") return style;
	      state.lastType = type == "operator" && (content == "++" || content == "--") ? "incdec" : type;
	      return parseJS(state, style, type, content, stream);
	    },

	    indent: function(state, textAfter) {
	      if (state.tokenize == tokenComment) return CodeMirror.Pass;
	      if (state.tokenize != tokenBase) return 0;
	      var firstChar = textAfter && textAfter.charAt(0), lexical = state.lexical, top
	      // Kludge to prevent 'maybelse' from blocking lexical scope pops
	      if (!/^\s*else\b/.test(textAfter)) for (var i = state.cc.length - 1; i >= 0; --i) {
	        var c = state.cc[i];
	        if (c == poplex) lexical = lexical.prev;
	        else if (c != maybeelse) break;
	      }
	      while ((lexical.type == "stat" || lexical.type == "form") &&
	             (firstChar == "}" || ((top = state.cc[state.cc.length - 1]) &&
	                                   (top == maybeoperatorComma || top == maybeoperatorNoComma) &&
	                                   !/^[,\.=+\-*:?[\(]/.test(textAfter))))
	        lexical = lexical.prev;
	      if (statementIndent && lexical.type == ")" && lexical.prev.type == "stat")
	        lexical = lexical.prev;
	      var type = lexical.type, closing = firstChar == type;

	      if (type == "vardef") return lexical.indented + (state.lastType == "operator" || state.lastType == "," ? lexical.info + 1 : 0);
	      else if (type == "form" && firstChar == "{") return lexical.indented;
	      else if (type == "form") return lexical.indented + indentUnit;
	      else if (type == "stat")
	        return lexical.indented + (isContinuedStatement(state, textAfter) ? statementIndent || indentUnit : 0);
	      else if (lexical.info == "switch" && !closing && parserConfig.doubleIndentSwitch != false)
	        return lexical.indented + (/^(?:case|default)\b/.test(textAfter) ? indentUnit : 2 * indentUnit);
	      else if (lexical.align) return lexical.column + (closing ? 0 : 1);
	      else return lexical.indented + (closing ? 0 : indentUnit);
	    },

	    electricInput: /^\s*(?:case .*?:|default:|\{|\})$/,
	    blockCommentStart: jsonMode ? null : "/*",
	    blockCommentEnd: jsonMode ? null : "*/",
	    blockCommentContinue: jsonMode ? null : " * ",
	    lineComment: jsonMode ? null : "//",
	    fold: "brace",
	    closeBrackets: "()[]{}''\"\"``",

	    helperType: jsonMode ? "json" : "javascript",
	    jsonldMode: jsonldMode,
	    jsonMode: jsonMode,

	    expressionAllowed: expressionAllowed,

	    skipExpression: function(state) {
	      var top = state.cc[state.cc.length - 1]
	      if (top == expression || top == expressionNoComma) state.cc.pop()
	    }
	  };
	});

	CodeMirror.registerHelper("wordChars", "javascript", /[\w$]/);

	CodeMirror.defineMIME("text/javascript", "javascript");
	CodeMirror.defineMIME("text/ecmascript", "javascript");
	CodeMirror.defineMIME("application/javascript", "javascript");
	CodeMirror.defineMIME("application/x-javascript", "javascript");
	CodeMirror.defineMIME("application/ecmascript", "javascript");
	CodeMirror.defineMIME("application/json", {name: "javascript", json: true});
	CodeMirror.defineMIME("application/x-json", {name: "javascript", json: true});
	CodeMirror.defineMIME("application/ld+json", {name: "javascript", jsonld: true});
	CodeMirror.defineMIME("text/typescript", { name: "javascript", typescript: true });
	CodeMirror.defineMIME("application/typescript", { name: "javascript", typescript: true });

	});


/***/ }),
/* 9 */
/***/ (function(module, exports, __webpack_require__) {

	// CodeMirror, copyright (c) by Marijn Haverbeke and others
	// Distributed under an MIT license: http://codemirror.net/LICENSE

	(function(mod) {
	  if (true) // CommonJS
	    mod(__webpack_require__(2));
	  else if (typeof define == "function" && define.amd) // AMD
	    define(["../../lib/codemirror"], mod);
	  else // Plain browser env
	    mod(CodeMirror);
	})(function(CodeMirror) {
	"use strict";

	var htmlConfig = {
	  autoSelfClosers: {'area': true, 'base': true, 'br': true, 'col': true, 'command': true,
	                    'embed': true, 'frame': true, 'hr': true, 'img': true, 'input': true,
	                    'keygen': true, 'link': true, 'meta': true, 'param': true, 'source': true,
	                    'track': true, 'wbr': true, 'menuitem': true},
	  implicitlyClosed: {'dd': true, 'li': true, 'optgroup': true, 'option': true, 'p': true,
	                     'rp': true, 'rt': true, 'tbody': true, 'td': true, 'tfoot': true,
	                     'th': true, 'tr': true},
	  contextGrabbers: {
	    'dd': {'dd': true, 'dt': true},
	    'dt': {'dd': true, 'dt': true},
	    'li': {'li': true},
	    'option': {'option': true, 'optgroup': true},
	    'optgroup': {'optgroup': true},
	    'p': {'address': true, 'article': true, 'aside': true, 'blockquote': true, 'dir': true,
	          'div': true, 'dl': true, 'fieldset': true, 'footer': true, 'form': true,
	          'h1': true, 'h2': true, 'h3': true, 'h4': true, 'h5': true, 'h6': true,
	          'header': true, 'hgroup': true, 'hr': true, 'menu': true, 'nav': true, 'ol': true,
	          'p': true, 'pre': true, 'section': true, 'table': true, 'ul': true},
	    'rp': {'rp': true, 'rt': true},
	    'rt': {'rp': true, 'rt': true},
	    'tbody': {'tbody': true, 'tfoot': true},
	    'td': {'td': true, 'th': true},
	    'tfoot': {'tbody': true},
	    'th': {'td': true, 'th': true},
	    'thead': {'tbody': true, 'tfoot': true},
	    'tr': {'tr': true}
	  },
	  doNotIndent: {"pre": true},
	  allowUnquoted: true,
	  allowMissing: true,
	  caseFold: true
	}

	var xmlConfig = {
	  autoSelfClosers: {},
	  implicitlyClosed: {},
	  contextGrabbers: {},
	  doNotIndent: {},
	  allowUnquoted: false,
	  allowMissing: false,
	  caseFold: false
	}

	CodeMirror.defineMode("xml", function(editorConf, config_) {
	  var indentUnit = editorConf.indentUnit
	  var config = {}
	  var defaults = config_.htmlMode ? htmlConfig : xmlConfig
	  for (var prop in defaults) config[prop] = defaults[prop]
	  for (var prop in config_) config[prop] = config_[prop]

	  // Return variables for tokenizers
	  var type, setStyle;

	  function inText(stream, state) {
	    function chain(parser) {
	      state.tokenize = parser;
	      return parser(stream, state);
	    }

	    var ch = stream.next();
	    if (ch == "<") {
	      if (stream.eat("!")) {
	        if (stream.eat("[")) {
	          if (stream.match("CDATA[")) return chain(inBlock("atom", "]]>"));
	          else return null;
	        } else if (stream.match("--")) {
	          return chain(inBlock("comment", "-->"));
	        } else if (stream.match("DOCTYPE", true, true)) {
	          stream.eatWhile(/[\w\._\-]/);
	          return chain(doctype(1));
	        } else {
	          return null;
	        }
	      } else if (stream.eat("?")) {
	        stream.eatWhile(/[\w\._\-]/);
	        state.tokenize = inBlock("meta", "?>");
	        return "meta";
	      } else {
	        type = stream.eat("/") ? "closeTag" : "openTag";
	        state.tokenize = inTag;
	        return "tag bracket";
	      }
	    } else if (ch == "&") {
	      var ok;
	      if (stream.eat("#")) {
	        if (stream.eat("x")) {
	          ok = stream.eatWhile(/[a-fA-F\d]/) && stream.eat(";");
	        } else {
	          ok = stream.eatWhile(/[\d]/) && stream.eat(";");
	        }
	      } else {
	        ok = stream.eatWhile(/[\w\.\-:]/) && stream.eat(";");
	      }
	      return ok ? "atom" : "error";
	    } else {
	      stream.eatWhile(/[^&<]/);
	      return null;
	    }
	  }
	  inText.isInText = true;

	  function inTag(stream, state) {
	    var ch = stream.next();
	    if (ch == ">" || (ch == "/" && stream.eat(">"))) {
	      state.tokenize = inText;
	      type = ch == ">" ? "endTag" : "selfcloseTag";
	      return "tag bracket";
	    } else if (ch == "=") {
	      type = "equals";
	      return null;
	    } else if (ch == "<") {
	      state.tokenize = inText;
	      state.state = baseState;
	      state.tagName = state.tagStart = null;
	      var next = state.tokenize(stream, state);
	      return next ? next + " tag error" : "tag error";
	    } else if (/[\'\"]/.test(ch)) {
	      state.tokenize = inAttribute(ch);
	      state.stringStartCol = stream.column();
	      return state.tokenize(stream, state);
	    } else {
	      stream.match(/^[^\s\u00a0=<>\"\']*[^\s\u00a0=<>\"\'\/]/);
	      return "word";
	    }
	  }

	  function inAttribute(quote) {
	    var closure = function(stream, state) {
	      while (!stream.eol()) {
	        if (stream.next() == quote) {
	          state.tokenize = inTag;
	          break;
	        }
	      }
	      return "string";
	    };
	    closure.isInAttribute = true;
	    return closure;
	  }

	  function inBlock(style, terminator) {
	    return function(stream, state) {
	      while (!stream.eol()) {
	        if (stream.match(terminator)) {
	          state.tokenize = inText;
	          break;
	        }
	        stream.next();
	      }
	      return style;
	    };
	  }
	  function doctype(depth) {
	    return function(stream, state) {
	      var ch;
	      while ((ch = stream.next()) != null) {
	        if (ch == "<") {
	          state.tokenize = doctype(depth + 1);
	          return state.tokenize(stream, state);
	        } else if (ch == ">") {
	          if (depth == 1) {
	            state.tokenize = inText;
	            break;
	          } else {
	            state.tokenize = doctype(depth - 1);
	            return state.tokenize(stream, state);
	          }
	        }
	      }
	      return "meta";
	    };
	  }

	  function Context(state, tagName, startOfLine) {
	    this.prev = state.context;
	    this.tagName = tagName;
	    this.indent = state.indented;
	    this.startOfLine = startOfLine;
	    if (config.doNotIndent.hasOwnProperty(tagName) || (state.context && state.context.noIndent))
	      this.noIndent = true;
	  }
	  function popContext(state) {
	    if (state.context) state.context = state.context.prev;
	  }
	  function maybePopContext(state, nextTagName) {
	    var parentTagName;
	    while (true) {
	      if (!state.context) {
	        return;
	      }
	      parentTagName = state.context.tagName;
	      if (!config.contextGrabbers.hasOwnProperty(parentTagName) ||
	          !config.contextGrabbers[parentTagName].hasOwnProperty(nextTagName)) {
	        return;
	      }
	      popContext(state);
	    }
	  }

	  function baseState(type, stream, state) {
	    if (type == "openTag") {
	      state.tagStart = stream.column();
	      return tagNameState;
	    } else if (type == "closeTag") {
	      return closeTagNameState;
	    } else {
	      return baseState;
	    }
	  }
	  function tagNameState(type, stream, state) {
	    if (type == "word") {
	      state.tagName = stream.current();
	      setStyle = "tag";
	      return attrState;
	    } else {
	      setStyle = "error";
	      return tagNameState;
	    }
	  }
	  function closeTagNameState(type, stream, state) {
	    if (type == "word") {
	      var tagName = stream.current();
	      if (state.context && state.context.tagName != tagName &&
	          config.implicitlyClosed.hasOwnProperty(state.context.tagName))
	        popContext(state);
	      if ((state.context && state.context.tagName == tagName) || config.matchClosing === false) {
	        setStyle = "tag";
	        return closeState;
	      } else {
	        setStyle = "tag error";
	        return closeStateErr;
	      }
	    } else {
	      setStyle = "error";
	      return closeStateErr;
	    }
	  }

	  function closeState(type, _stream, state) {
	    if (type != "endTag") {
	      setStyle = "error";
	      return closeState;
	    }
	    popContext(state);
	    return baseState;
	  }
	  function closeStateErr(type, stream, state) {
	    setStyle = "error";
	    return closeState(type, stream, state);
	  }

	  function attrState(type, _stream, state) {
	    if (type == "word") {
	      setStyle = "attribute";
	      return attrEqState;
	    } else if (type == "endTag" || type == "selfcloseTag") {
	      var tagName = state.tagName, tagStart = state.tagStart;
	      state.tagName = state.tagStart = null;
	      if (type == "selfcloseTag" ||
	          config.autoSelfClosers.hasOwnProperty(tagName)) {
	        maybePopContext(state, tagName);
	      } else {
	        maybePopContext(state, tagName);
	        state.context = new Context(state, tagName, tagStart == state.indented);
	      }
	      return baseState;
	    }
	    setStyle = "error";
	    return attrState;
	  }
	  function attrEqState(type, stream, state) {
	    if (type == "equals") return attrValueState;
	    if (!config.allowMissing) setStyle = "error";
	    return attrState(type, stream, state);
	  }
	  function attrValueState(type, stream, state) {
	    if (type == "string") return attrContinuedState;
	    if (type == "word" && config.allowUnquoted) {setStyle = "string"; return attrState;}
	    setStyle = "error";
	    return attrState(type, stream, state);
	  }
	  function attrContinuedState(type, stream, state) {
	    if (type == "string") return attrContinuedState;
	    return attrState(type, stream, state);
	  }

	  return {
	    startState: function(baseIndent) {
	      var state = {tokenize: inText,
	                   state: baseState,
	                   indented: baseIndent || 0,
	                   tagName: null, tagStart: null,
	                   context: null}
	      if (baseIndent != null) state.baseIndent = baseIndent
	      return state
	    },

	    token: function(stream, state) {
	      if (!state.tagName && stream.sol())
	        state.indented = stream.indentation();

	      if (stream.eatSpace()) return null;
	      type = null;
	      var style = state.tokenize(stream, state);
	      if ((style || type) && style != "comment") {
	        setStyle = null;
	        state.state = state.state(type || style, stream, state);
	        if (setStyle)
	          style = setStyle == "error" ? style + " error" : setStyle;
	      }
	      return style;
	    },

	    indent: function(state, textAfter, fullLine) {
	      var context = state.context;
	      // Indent multi-line strings (e.g. css).
	      if (state.tokenize.isInAttribute) {
	        if (state.tagStart == state.indented)
	          return state.stringStartCol + 1;
	        else
	          return state.indented + indentUnit;
	      }
	      if (context && context.noIndent) return CodeMirror.Pass;
	      if (state.tokenize != inTag && state.tokenize != inText)
	        return fullLine ? fullLine.match(/^(\s*)/)[0].length : 0;
	      // Indent the starts of attribute names.
	      if (state.tagName) {
	        if (config.multilineTagIndentPastTag !== false)
	          return state.tagStart + state.tagName.length + 2;
	        else
	          return state.tagStart + indentUnit * (config.multilineTagIndentFactor || 1);
	      }
	      if (config.alignCDATA && /<!\[CDATA\[/.test(textAfter)) return 0;
	      var tagAfter = textAfter && /^<(\/)?([\w_:\.-]*)/.exec(textAfter);
	      if (tagAfter && tagAfter[1]) { // Closing tag spotted
	        while (context) {
	          if (context.tagName == tagAfter[2]) {
	            context = context.prev;
	            break;
	          } else if (config.implicitlyClosed.hasOwnProperty(context.tagName)) {
	            context = context.prev;
	          } else {
	            break;
	          }
	        }
	      } else if (tagAfter) { // Opening tag spotted
	        while (context) {
	          var grabbers = config.contextGrabbers[context.tagName];
	          if (grabbers && grabbers.hasOwnProperty(tagAfter[2]))
	            context = context.prev;
	          else
	            break;
	        }
	      }
	      while (context && context.prev && !context.startOfLine)
	        context = context.prev;
	      if (context) return context.indent + indentUnit;
	      else return state.baseIndent || 0;
	    },

	    electricInput: /<\/[\s\w:]+>$/,
	    blockCommentStart: "<!--",
	    blockCommentEnd: "-->",

	    configuration: config.htmlMode ? "html" : "xml",
	    helperType: config.htmlMode ? "html" : "xml",

	    skipAttribute: function(state) {
	      if (state.state == attrValueState)
	        state.state = attrState
	    }
	  };
	});

	CodeMirror.defineMIME("text/xml", "xml");
	CodeMirror.defineMIME("application/xml", "xml");
	if (!CodeMirror.mimeModes.hasOwnProperty("text/html"))
	  CodeMirror.defineMIME("text/html", {name: "xml", htmlMode: true});

	});


/***/ }),
/* 10 */
/***/ (function(module, exports, __webpack_require__) {

	// CodeMirror, copyright (c) by Marijn Haverbeke and others
	// Distributed under an MIT license: http://codemirror.net/LICENSE

	(function(mod) {
	  if (true) // CommonJS
	    mod(__webpack_require__(2));
	  else if (typeof define == "function" && define.amd) // AMD
	    define(["../../lib/codemirror"], mod);
	  else // Plain browser env
	    mod(CodeMirror);
	})(function(CodeMirror) {
	"use strict";

	CodeMirror.defineMode("css", function(config, parserConfig) {
	  var inline = parserConfig.inline
	  if (!parserConfig.propertyKeywords) parserConfig = CodeMirror.resolveMode("text/css");

	  var indentUnit = config.indentUnit,
	      tokenHooks = parserConfig.tokenHooks,
	      documentTypes = parserConfig.documentTypes || {},
	      mediaTypes = parserConfig.mediaTypes || {},
	      mediaFeatures = parserConfig.mediaFeatures || {},
	      mediaValueKeywords = parserConfig.mediaValueKeywords || {},
	      propertyKeywords = parserConfig.propertyKeywords || {},
	      nonStandardPropertyKeywords = parserConfig.nonStandardPropertyKeywords || {},
	      fontProperties = parserConfig.fontProperties || {},
	      counterDescriptors = parserConfig.counterDescriptors || {},
	      colorKeywords = parserConfig.colorKeywords || {},
	      valueKeywords = parserConfig.valueKeywords || {},
	      allowNested = parserConfig.allowNested,
	      lineComment = parserConfig.lineComment,
	      supportsAtComponent = parserConfig.supportsAtComponent === true;

	  var type, override;
	  function ret(style, tp) { type = tp; return style; }

	  // Tokenizers

	  function tokenBase(stream, state) {
	    var ch = stream.next();
	    if (tokenHooks[ch]) {
	      var result = tokenHooks[ch](stream, state);
	      if (result !== false) return result;
	    }
	    if (ch == "@") {
	      stream.eatWhile(/[\w\\\-]/);
	      return ret("def", stream.current());
	    } else if (ch == "=" || (ch == "~" || ch == "|") && stream.eat("=")) {
	      return ret(null, "compare");
	    } else if (ch == "\"" || ch == "'") {
	      state.tokenize = tokenString(ch);
	      return state.tokenize(stream, state);
	    } else if (ch == "#") {
	      stream.eatWhile(/[\w\\\-]/);
	      return ret("atom", "hash");
	    } else if (ch == "!") {
	      stream.match(/^\s*\w*/);
	      return ret("keyword", "important");
	    } else if (/\d/.test(ch) || ch == "." && stream.eat(/\d/)) {
	      stream.eatWhile(/[\w.%]/);
	      return ret("number", "unit");
	    } else if (ch === "-") {
	      if (/[\d.]/.test(stream.peek())) {
	        stream.eatWhile(/[\w.%]/);
	        return ret("number", "unit");
	      } else if (stream.match(/^-[\w\\\-]+/)) {
	        stream.eatWhile(/[\w\\\-]/);
	        if (stream.match(/^\s*:/, false))
	          return ret("variable-2", "variable-definition");
	        return ret("variable-2", "variable");
	      } else if (stream.match(/^\w+-/)) {
	        return ret("meta", "meta");
	      }
	    } else if (/[,+>*\/]/.test(ch)) {
	      return ret(null, "select-op");
	    } else if (ch == "." && stream.match(/^-?[_a-z][_a-z0-9-]*/i)) {
	      return ret("qualifier", "qualifier");
	    } else if (/[:;{}\[\]\(\)]/.test(ch)) {
	      return ret(null, ch);
	    } else if ((ch == "u" && stream.match(/rl(-prefix)?\(/)) ||
	               (ch == "d" && stream.match("omain(")) ||
	               (ch == "r" && stream.match("egexp("))) {
	      stream.backUp(1);
	      state.tokenize = tokenParenthesized;
	      return ret("property", "word");
	    } else if (/[\w\\\-]/.test(ch)) {
	      stream.eatWhile(/[\w\\\-]/);
	      return ret("property", "word");
	    } else {
	      return ret(null, null);
	    }
	  }

	  function tokenString(quote) {
	    return function(stream, state) {
	      var escaped = false, ch;
	      while ((ch = stream.next()) != null) {
	        if (ch == quote && !escaped) {
	          if (quote == ")") stream.backUp(1);
	          break;
	        }
	        escaped = !escaped && ch == "\\";
	      }
	      if (ch == quote || !escaped && quote != ")") state.tokenize = null;
	      return ret("string", "string");
	    };
	  }

	  function tokenParenthesized(stream, state) {
	    stream.next(); // Must be '('
	    if (!stream.match(/\s*[\"\')]/, false))
	      state.tokenize = tokenString(")");
	    else
	      state.tokenize = null;
	    return ret(null, "(");
	  }

	  // Context management

	  function Context(type, indent, prev) {
	    this.type = type;
	    this.indent = indent;
	    this.prev = prev;
	  }

	  function pushContext(state, stream, type, indent) {
	    state.context = new Context(type, stream.indentation() + (indent === false ? 0 : indentUnit), state.context);
	    return type;
	  }

	  function popContext(state) {
	    if (state.context.prev)
	      state.context = state.context.prev;
	    return state.context.type;
	  }

	  function pass(type, stream, state) {
	    return states[state.context.type](type, stream, state);
	  }
	  function popAndPass(type, stream, state, n) {
	    for (var i = n || 1; i > 0; i--)
	      state.context = state.context.prev;
	    return pass(type, stream, state);
	  }

	  // Parser

	  function wordAsValue(stream) {
	    var word = stream.current().toLowerCase();
	    if (valueKeywords.hasOwnProperty(word))
	      override = "atom";
	    else if (colorKeywords.hasOwnProperty(word))
	      override = "keyword";
	    else
	      override = "variable";
	  }

	  var states = {};

	  states.top = function(type, stream, state) {
	    if (type == "{") {
	      return pushContext(state, stream, "block");
	    } else if (type == "}" && state.context.prev) {
	      return popContext(state);
	    } else if (supportsAtComponent && /@component/.test(type)) {
	      return pushContext(state, stream, "atComponentBlock");
	    } else if (/^@(-moz-)?document$/.test(type)) {
	      return pushContext(state, stream, "documentTypes");
	    } else if (/^@(media|supports|(-moz-)?document|import)$/.test(type)) {
	      return pushContext(state, stream, "atBlock");
	    } else if (/^@(font-face|counter-style)/.test(type)) {
	      state.stateArg = type;
	      return "restricted_atBlock_before";
	    } else if (/^@(-(moz|ms|o|webkit)-)?keyframes$/.test(type)) {
	      return "keyframes";
	    } else if (type && type.charAt(0) == "@") {
	      return pushContext(state, stream, "at");
	    } else if (type == "hash") {
	      override = "builtin";
	    } else if (type == "word") {
	      override = "tag";
	    } else if (type == "variable-definition") {
	      return "maybeprop";
	    } else if (type == "interpolation") {
	      return pushContext(state, stream, "interpolation");
	    } else if (type == ":") {
	      return "pseudo";
	    } else if (allowNested && type == "(") {
	      return pushContext(state, stream, "parens");
	    }
	    return state.context.type;
	  };

	  states.block = function(type, stream, state) {
	    if (type == "word") {
	      var word = stream.current().toLowerCase();
	      if (propertyKeywords.hasOwnProperty(word)) {
	        override = "property";
	        return "maybeprop";
	      } else if (nonStandardPropertyKeywords.hasOwnProperty(word)) {
	        override = "string-2";
	        return "maybeprop";
	      } else if (allowNested) {
	        override = stream.match(/^\s*:(?:\s|$)/, false) ? "property" : "tag";
	        return "block";
	      } else {
	        override += " error";
	        return "maybeprop";
	      }
	    } else if (type == "meta") {
	      return "block";
	    } else if (!allowNested && (type == "hash" || type == "qualifier")) {
	      override = "error";
	      return "block";
	    } else {
	      return states.top(type, stream, state);
	    }
	  };

	  states.maybeprop = function(type, stream, state) {
	    if (type == ":") return pushContext(state, stream, "prop");
	    return pass(type, stream, state);
	  };

	  states.prop = function(type, stream, state) {
	    if (type == ";") return popContext(state);
	    if (type == "{" && allowNested) return pushContext(state, stream, "propBlock");
	    if (type == "}" || type == "{") return popAndPass(type, stream, state);
	    if (type == "(") return pushContext(state, stream, "parens");

	    if (type == "hash" && !/^#([0-9a-fA-f]{3,4}|[0-9a-fA-f]{6}|[0-9a-fA-f]{8})$/.test(stream.current())) {
	      override += " error";
	    } else if (type == "word") {
	      wordAsValue(stream);
	    } else if (type == "interpolation") {
	      return pushContext(state, stream, "interpolation");
	    }
	    return "prop";
	  };

	  states.propBlock = function(type, _stream, state) {
	    if (type == "}") return popContext(state);
	    if (type == "word") { override = "property"; return "maybeprop"; }
	    return state.context.type;
	  };

	  states.parens = function(type, stream, state) {
	    if (type == "{" || type == "}") return popAndPass(type, stream, state);
	    if (type == ")") return popContext(state);
	    if (type == "(") return pushContext(state, stream, "parens");
	    if (type == "interpolation") return pushContext(state, stream, "interpolation");
	    if (type == "word") wordAsValue(stream);
	    return "parens";
	  };

	  states.pseudo = function(type, stream, state) {
	    if (type == "meta") return "pseudo";

	    if (type == "word") {
	      override = "variable-3";
	      return state.context.type;
	    }
	    return pass(type, stream, state);
	  };

	  states.documentTypes = function(type, stream, state) {
	    if (type == "word" && documentTypes.hasOwnProperty(stream.current())) {
	      override = "tag";
	      return state.context.type;
	    } else {
	      return states.atBlock(type, stream, state);
	    }
	  };

	  states.atBlock = function(type, stream, state) {
	    if (type == "(") return pushContext(state, stream, "atBlock_parens");
	    if (type == "}" || type == ";") return popAndPass(type, stream, state);
	    if (type == "{") return popContext(state) && pushContext(state, stream, allowNested ? "block" : "top");

	    if (type == "interpolation") return pushContext(state, stream, "interpolation");

	    if (type == "word") {
	      var word = stream.current().toLowerCase();
	      if (word == "only" || word == "not" || word == "and" || word == "or")
	        override = "keyword";
	      else if (mediaTypes.hasOwnProperty(word))
	        override = "attribute";
	      else if (mediaFeatures.hasOwnProperty(word))
	        override = "property";
	      else if (mediaValueKeywords.hasOwnProperty(word))
	        override = "keyword";
	      else if (propertyKeywords.hasOwnProperty(word))
	        override = "property";
	      else if (nonStandardPropertyKeywords.hasOwnProperty(word))
	        override = "string-2";
	      else if (valueKeywords.hasOwnProperty(word))
	        override = "atom";
	      else if (colorKeywords.hasOwnProperty(word))
	        override = "keyword";
	      else
	        override = "error";
	    }
	    return state.context.type;
	  };

	  states.atComponentBlock = function(type, stream, state) {
	    if (type == "}")
	      return popAndPass(type, stream, state);
	    if (type == "{")
	      return popContext(state) && pushContext(state, stream, allowNested ? "block" : "top", false);
	    if (type == "word")
	      override = "error";
	    return state.context.type;
	  };

	  states.atBlock_parens = function(type, stream, state) {
	    if (type == ")") return popContext(state);
	    if (type == "{" || type == "}") return popAndPass(type, stream, state, 2);
	    return states.atBlock(type, stream, state);
	  };

	  states.restricted_atBlock_before = function(type, stream, state) {
	    if (type == "{")
	      return pushContext(state, stream, "restricted_atBlock");
	    if (type == "word" && state.stateArg == "@counter-style") {
	      override = "variable";
	      return "restricted_atBlock_before";
	    }
	    return pass(type, stream, state);
	  };

	  states.restricted_atBlock = function(type, stream, state) {
	    if (type == "}") {
	      state.stateArg = null;
	      return popContext(state);
	    }
	    if (type == "word") {
	      if ((state.stateArg == "@font-face" && !fontProperties.hasOwnProperty(stream.current().toLowerCase())) ||
	          (state.stateArg == "@counter-style" && !counterDescriptors.hasOwnProperty(stream.current().toLowerCase())))
	        override = "error";
	      else
	        override = "property";
	      return "maybeprop";
	    }
	    return "restricted_atBlock";
	  };

	  states.keyframes = function(type, stream, state) {
	    if (type == "word") { override = "variable"; return "keyframes"; }
	    if (type == "{") return pushContext(state, stream, "top");
	    return pass(type, stream, state);
	  };

	  states.at = function(type, stream, state) {
	    if (type == ";") return popContext(state);
	    if (type == "{" || type == "}") return popAndPass(type, stream, state);
	    if (type == "word") override = "tag";
	    else if (type == "hash") override = "builtin";
	    return "at";
	  };

	  states.interpolation = function(type, stream, state) {
	    if (type == "}") return popContext(state);
	    if (type == "{" || type == ";") return popAndPass(type, stream, state);
	    if (type == "word") override = "variable";
	    else if (type != "variable" && type != "(" && type != ")") override = "error";
	    return "interpolation";
	  };

	  return {
	    startState: function(base) {
	      return {tokenize: null,
	              state: inline ? "block" : "top",
	              stateArg: null,
	              context: new Context(inline ? "block" : "top", base || 0, null)};
	    },

	    token: function(stream, state) {
	      if (!state.tokenize && stream.eatSpace()) return null;
	      var style = (state.tokenize || tokenBase)(stream, state);
	      if (style && typeof style == "object") {
	        type = style[1];
	        style = style[0];
	      }
	      override = style;
	      if (type != "comment")
	        state.state = states[state.state](type, stream, state);
	      return override;
	    },

	    indent: function(state, textAfter) {
	      var cx = state.context, ch = textAfter && textAfter.charAt(0);
	      var indent = cx.indent;
	      if (cx.type == "prop" && (ch == "}" || ch == ")")) cx = cx.prev;
	      if (cx.prev) {
	        if (ch == "}" && (cx.type == "block" || cx.type == "top" ||
	                          cx.type == "interpolation" || cx.type == "restricted_atBlock")) {
	          // Resume indentation from parent context.
	          cx = cx.prev;
	          indent = cx.indent;
	        } else if (ch == ")" && (cx.type == "parens" || cx.type == "atBlock_parens") ||
	            ch == "{" && (cx.type == "at" || cx.type == "atBlock")) {
	          // Dedent relative to current context.
	          indent = Math.max(0, cx.indent - indentUnit);
	        }
	      }
	      return indent;
	    },

	    electricChars: "}",
	    blockCommentStart: "/*",
	    blockCommentEnd: "*/",
	    blockCommentContinue: " * ",
	    lineComment: lineComment,
	    fold: "brace"
	  };
	});

	  function keySet(array) {
	    var keys = {};
	    for (var i = 0; i < array.length; ++i) {
	      keys[array[i].toLowerCase()] = true;
	    }
	    return keys;
	  }

	  var documentTypes_ = [
	    "domain", "regexp", "url", "url-prefix"
	  ], documentTypes = keySet(documentTypes_);

	  var mediaTypes_ = [
	    "all", "aural", "braille", "handheld", "print", "projection", "screen",
	    "tty", "tv", "embossed"
	  ], mediaTypes = keySet(mediaTypes_);

	  var mediaFeatures_ = [
	    "width", "min-width", "max-width", "height", "min-height", "max-height",
	    "device-width", "min-device-width", "max-device-width", "device-height",
	    "min-device-height", "max-device-height", "aspect-ratio",
	    "min-aspect-ratio", "max-aspect-ratio", "device-aspect-ratio",
	    "min-device-aspect-ratio", "max-device-aspect-ratio", "color", "min-color",
	    "max-color", "color-index", "min-color-index", "max-color-index",
	    "monochrome", "min-monochrome", "max-monochrome", "resolution",
	    "min-resolution", "max-resolution", "scan", "grid", "orientation",
	    "device-pixel-ratio", "min-device-pixel-ratio", "max-device-pixel-ratio",
	    "pointer", "any-pointer", "hover", "any-hover"
	  ], mediaFeatures = keySet(mediaFeatures_);

	  var mediaValueKeywords_ = [
	    "landscape", "portrait", "none", "coarse", "fine", "on-demand", "hover",
	    "interlace", "progressive"
	  ], mediaValueKeywords = keySet(mediaValueKeywords_);

	  var propertyKeywords_ = [
	    "align-content", "align-items", "align-self", "alignment-adjust",
	    "alignment-baseline", "anchor-point", "animation", "animation-delay",
	    "animation-direction", "animation-duration", "animation-fill-mode",
	    "animation-iteration-count", "animation-name", "animation-play-state",
	    "animation-timing-function", "appearance", "azimuth", "backface-visibility",
	    "background", "background-attachment", "background-blend-mode", "background-clip",
	    "background-color", "background-image", "background-origin", "background-position",
	    "background-repeat", "background-size", "baseline-shift", "binding",
	    "bleed", "bookmark-label", "bookmark-level", "bookmark-state",
	    "bookmark-target", "border", "border-bottom", "border-bottom-color",
	    "border-bottom-left-radius", "border-bottom-right-radius",
	    "border-bottom-style", "border-bottom-width", "border-collapse",
	    "border-color", "border-image", "border-image-outset",
	    "border-image-repeat", "border-image-slice", "border-image-source",
	    "border-image-width", "border-left", "border-left-color",
	    "border-left-style", "border-left-width", "border-radius", "border-right",
	    "border-right-color", "border-right-style", "border-right-width",
	    "border-spacing", "border-style", "border-top", "border-top-color",
	    "border-top-left-radius", "border-top-right-radius", "border-top-style",
	    "border-top-width", "border-width", "bottom", "box-decoration-break",
	    "box-shadow", "box-sizing", "break-after", "break-before", "break-inside",
	    "caption-side", "caret-color", "clear", "clip", "color", "color-profile", "column-count",
	    "column-fill", "column-gap", "column-rule", "column-rule-color",
	    "column-rule-style", "column-rule-width", "column-span", "column-width",
	    "columns", "content", "counter-increment", "counter-reset", "crop", "cue",
	    "cue-after", "cue-before", "cursor", "direction", "display",
	    "dominant-baseline", "drop-initial-after-adjust",
	    "drop-initial-after-align", "drop-initial-before-adjust",
	    "drop-initial-before-align", "drop-initial-size", "drop-initial-value",
	    "elevation", "empty-cells", "fit", "fit-position", "flex", "flex-basis",
	    "flex-direction", "flex-flow", "flex-grow", "flex-shrink", "flex-wrap",
	    "float", "float-offset", "flow-from", "flow-into", "font", "font-feature-settings",
	    "font-family", "font-kerning", "font-language-override", "font-size", "font-size-adjust",
	    "font-stretch", "font-style", "font-synthesis", "font-variant",
	    "font-variant-alternates", "font-variant-caps", "font-variant-east-asian",
	    "font-variant-ligatures", "font-variant-numeric", "font-variant-position",
	    "font-weight", "grid", "grid-area", "grid-auto-columns", "grid-auto-flow",
	    "grid-auto-rows", "grid-column", "grid-column-end", "grid-column-gap",
	    "grid-column-start", "grid-gap", "grid-row", "grid-row-end", "grid-row-gap",
	    "grid-row-start", "grid-template", "grid-template-areas", "grid-template-columns",
	    "grid-template-rows", "hanging-punctuation", "height", "hyphens",
	    "icon", "image-orientation", "image-rendering", "image-resolution",
	    "inline-box-align", "justify-content", "justify-items", "justify-self", "left", "letter-spacing",
	    "line-break", "line-height", "line-stacking", "line-stacking-ruby",
	    "line-stacking-shift", "line-stacking-strategy", "list-style",
	    "list-style-image", "list-style-position", "list-style-type", "margin",
	    "margin-bottom", "margin-left", "margin-right", "margin-top",
	    "marks", "marquee-direction", "marquee-loop",
	    "marquee-play-count", "marquee-speed", "marquee-style", "max-height",
	    "max-width", "min-height", "min-width", "move-to", "nav-down", "nav-index",
	    "nav-left", "nav-right", "nav-up", "object-fit", "object-position",
	    "opacity", "order", "orphans", "outline",
	    "outline-color", "outline-offset", "outline-style", "outline-width",
	    "overflow", "overflow-style", "overflow-wrap", "overflow-x", "overflow-y",
	    "padding", "padding-bottom", "padding-left", "padding-right", "padding-top",
	    "page", "page-break-after", "page-break-before", "page-break-inside",
	    "page-policy", "pause", "pause-after", "pause-before", "perspective",
	    "perspective-origin", "pitch", "pitch-range", "place-content", "place-items", "place-self", "play-during", "position",
	    "presentation-level", "punctuation-trim", "quotes", "region-break-after",
	    "region-break-before", "region-break-inside", "region-fragment",
	    "rendering-intent", "resize", "rest", "rest-after", "rest-before", "richness",
	    "right", "rotation", "rotation-point", "ruby-align", "ruby-overhang",
	    "ruby-position", "ruby-span", "shape-image-threshold", "shape-inside", "shape-margin",
	    "shape-outside", "size", "speak", "speak-as", "speak-header",
	    "speak-numeral", "speak-punctuation", "speech-rate", "stress", "string-set",
	    "tab-size", "table-layout", "target", "target-name", "target-new",
	    "target-position", "text-align", "text-align-last", "text-decoration",
	    "text-decoration-color", "text-decoration-line", "text-decoration-skip",
	    "text-decoration-style", "text-emphasis", "text-emphasis-color",
	    "text-emphasis-position", "text-emphasis-style", "text-height",
	    "text-indent", "text-justify", "text-outline", "text-overflow", "text-shadow",
	    "text-size-adjust", "text-space-collapse", "text-transform", "text-underline-position",
	    "text-wrap", "top", "transform", "transform-origin", "transform-style",
	    "transition", "transition-delay", "transition-duration",
	    "transition-property", "transition-timing-function", "unicode-bidi",
	    "user-select", "vertical-align", "visibility", "voice-balance", "voice-duration",
	    "voice-family", "voice-pitch", "voice-range", "voice-rate", "voice-stress",
	    "voice-volume", "volume", "white-space", "widows", "width", "will-change", "word-break",
	    "word-spacing", "word-wrap", "z-index",
	    // SVG-specific
	    "clip-path", "clip-rule", "mask", "enable-background", "filter", "flood-color",
	    "flood-opacity", "lighting-color", "stop-color", "stop-opacity", "pointer-events",
	    "color-interpolation", "color-interpolation-filters",
	    "color-rendering", "fill", "fill-opacity", "fill-rule", "image-rendering",
	    "marker", "marker-end", "marker-mid", "marker-start", "shape-rendering", "stroke",
	    "stroke-dasharray", "stroke-dashoffset", "stroke-linecap", "stroke-linejoin",
	    "stroke-miterlimit", "stroke-opacity", "stroke-width", "text-rendering",
	    "baseline-shift", "dominant-baseline", "glyph-orientation-horizontal",
	    "glyph-orientation-vertical", "text-anchor", "writing-mode"
	  ], propertyKeywords = keySet(propertyKeywords_);

	  var nonStandardPropertyKeywords_ = [
	    "scrollbar-arrow-color", "scrollbar-base-color", "scrollbar-dark-shadow-color",
	    "scrollbar-face-color", "scrollbar-highlight-color", "scrollbar-shadow-color",
	    "scrollbar-3d-light-color", "scrollbar-track-color", "shape-inside",
	    "searchfield-cancel-button", "searchfield-decoration", "searchfield-results-button",
	    "searchfield-results-decoration", "zoom"
	  ], nonStandardPropertyKeywords = keySet(nonStandardPropertyKeywords_);

	  var fontProperties_ = [
	    "font-family", "src", "unicode-range", "font-variant", "font-feature-settings",
	    "font-stretch", "font-weight", "font-style"
	  ], fontProperties = keySet(fontProperties_);

	  var counterDescriptors_ = [
	    "additive-symbols", "fallback", "negative", "pad", "prefix", "range",
	    "speak-as", "suffix", "symbols", "system"
	  ], counterDescriptors = keySet(counterDescriptors_);

	  var colorKeywords_ = [
	    "aliceblue", "antiquewhite", "aqua", "aquamarine", "azure", "beige",
	    "bisque", "black", "blanchedalmond", "blue", "blueviolet", "brown",
	    "burlywood", "cadetblue", "chartreuse", "chocolate", "coral", "cornflowerblue",
	    "cornsilk", "crimson", "cyan", "darkblue", "darkcyan", "darkgoldenrod",
	    "darkgray", "darkgreen", "darkkhaki", "darkmagenta", "darkolivegreen",
	    "darkorange", "darkorchid", "darkred", "darksalmon", "darkseagreen",
	    "darkslateblue", "darkslategray", "darkturquoise", "darkviolet",
	    "deeppink", "deepskyblue", "dimgray", "dodgerblue", "firebrick",
	    "floralwhite", "forestgreen", "fuchsia", "gainsboro", "ghostwhite",
	    "gold", "goldenrod", "gray", "grey", "green", "greenyellow", "honeydew",
	    "hotpink", "indianred", "indigo", "ivory", "khaki", "lavender",
	    "lavenderblush", "lawngreen", "lemonchiffon", "lightblue", "lightcoral",
	    "lightcyan", "lightgoldenrodyellow", "lightgray", "lightgreen", "lightpink",
	    "lightsalmon", "lightseagreen", "lightskyblue", "lightslategray",
	    "lightsteelblue", "lightyellow", "lime", "limegreen", "linen", "magenta",
	    "maroon", "mediumaquamarine", "mediumblue", "mediumorchid", "mediumpurple",
	    "mediumseagreen", "mediumslateblue", "mediumspringgreen", "mediumturquoise",
	    "mediumvioletred", "midnightblue", "mintcream", "mistyrose", "moccasin",
	    "navajowhite", "navy", "oldlace", "olive", "olivedrab", "orange", "orangered",
	    "orchid", "palegoldenrod", "palegreen", "paleturquoise", "palevioletred",
	    "papayawhip", "peachpuff", "peru", "pink", "plum", "powderblue",
	    "purple", "rebeccapurple", "red", "rosybrown", "royalblue", "saddlebrown",
	    "salmon", "sandybrown", "seagreen", "seashell", "sienna", "silver", "skyblue",
	    "slateblue", "slategray", "snow", "springgreen", "steelblue", "tan",
	    "teal", "thistle", "tomato", "turquoise", "violet", "wheat", "white",
	    "whitesmoke", "yellow", "yellowgreen"
	  ], colorKeywords = keySet(colorKeywords_);

	  var valueKeywords_ = [
	    "above", "absolute", "activeborder", "additive", "activecaption", "afar",
	    "after-white-space", "ahead", "alias", "all", "all-scroll", "alphabetic", "alternate",
	    "always", "amharic", "amharic-abegede", "antialiased", "appworkspace",
	    "arabic-indic", "armenian", "asterisks", "attr", "auto", "auto-flow", "avoid", "avoid-column", "avoid-page",
	    "avoid-region", "background", "backwards", "baseline", "below", "bidi-override", "binary",
	    "bengali", "blink", "block", "block-axis", "bold", "bolder", "border", "border-box",
	    "both", "bottom", "break", "break-all", "break-word", "bullets", "button", "button-bevel",
	    "buttonface", "buttonhighlight", "buttonshadow", "buttontext", "calc", "cambodian",
	    "capitalize", "caps-lock-indicator", "caption", "captiontext", "caret",
	    "cell", "center", "checkbox", "circle", "cjk-decimal", "cjk-earthly-branch",
	    "cjk-heavenly-stem", "cjk-ideographic", "clear", "clip", "close-quote",
	    "col-resize", "collapse", "color", "color-burn", "color-dodge", "column", "column-reverse",
	    "compact", "condensed", "contain", "content", "contents",
	    "content-box", "context-menu", "continuous", "copy", "counter", "counters", "cover", "crop",
	    "cross", "crosshair", "currentcolor", "cursive", "cyclic", "darken", "dashed", "decimal",
	    "decimal-leading-zero", "default", "default-button", "dense", "destination-atop",
	    "destination-in", "destination-out", "destination-over", "devanagari", "difference",
	    "disc", "discard", "disclosure-closed", "disclosure-open", "document",
	    "dot-dash", "dot-dot-dash",
	    "dotted", "double", "down", "e-resize", "ease", "ease-in", "ease-in-out", "ease-out",
	    "element", "ellipse", "ellipsis", "embed", "end", "ethiopic", "ethiopic-abegede",
	    "ethiopic-abegede-am-et", "ethiopic-abegede-gez", "ethiopic-abegede-ti-er",
	    "ethiopic-abegede-ti-et", "ethiopic-halehame-aa-er",
	    "ethiopic-halehame-aa-et", "ethiopic-halehame-am-et",
	    "ethiopic-halehame-gez", "ethiopic-halehame-om-et",
	    "ethiopic-halehame-sid-et", "ethiopic-halehame-so-et",
	    "ethiopic-halehame-ti-er", "ethiopic-halehame-ti-et", "ethiopic-halehame-tig",
	    "ethiopic-numeric", "ew-resize", "exclusion", "expanded", "extends", "extra-condensed",
	    "extra-expanded", "fantasy", "fast", "fill", "fixed", "flat", "flex", "flex-end", "flex-start", "footnotes",
	    "forwards", "from", "geometricPrecision", "georgian", "graytext", "grid", "groove",
	    "gujarati", "gurmukhi", "hand", "hangul", "hangul-consonant", "hard-light", "hebrew",
	    "help", "hidden", "hide", "higher", "highlight", "highlighttext",
	    "hiragana", "hiragana-iroha", "horizontal", "hsl", "hsla", "hue", "icon", "ignore",
	    "inactiveborder", "inactivecaption", "inactivecaptiontext", "infinite",
	    "infobackground", "infotext", "inherit", "initial", "inline", "inline-axis",
	    "inline-block", "inline-flex", "inline-grid", "inline-table", "inset", "inside", "intrinsic", "invert",
	    "italic", "japanese-formal", "japanese-informal", "justify", "kannada",
	    "katakana", "katakana-iroha", "keep-all", "khmer",
	    "korean-hangul-formal", "korean-hanja-formal", "korean-hanja-informal",
	    "landscape", "lao", "large", "larger", "left", "level", "lighter", "lighten",
	    "line-through", "linear", "linear-gradient", "lines", "list-item", "listbox", "listitem",
	    "local", "logical", "loud", "lower", "lower-alpha", "lower-armenian",
	    "lower-greek", "lower-hexadecimal", "lower-latin", "lower-norwegian",
	    "lower-roman", "lowercase", "ltr", "luminosity", "malayalam", "match", "matrix", "matrix3d",
	    "media-controls-background", "media-current-time-display",
	    "media-fullscreen-button", "media-mute-button", "media-play-button",
	    "media-return-to-realtime-button", "media-rewind-button",
	    "media-seek-back-button", "media-seek-forward-button", "media-slider",
	    "media-sliderthumb", "media-time-remaining-display", "media-volume-slider",
	    "media-volume-slider-container", "media-volume-sliderthumb", "medium",
	    "menu", "menulist", "menulist-button", "menulist-text",
	    "menulist-textfield", "menutext", "message-box", "middle", "min-intrinsic",
	    "mix", "mongolian", "monospace", "move", "multiple", "multiply", "myanmar", "n-resize",
	    "narrower", "ne-resize", "nesw-resize", "no-close-quote", "no-drop",
	    "no-open-quote", "no-repeat", "none", "normal", "not-allowed", "nowrap",
	    "ns-resize", "numbers", "numeric", "nw-resize", "nwse-resize", "oblique", "octal", "opacity", "open-quote",
	    "optimizeLegibility", "optimizeSpeed", "oriya", "oromo", "outset",
	    "outside", "outside-shape", "overlay", "overline", "padding", "padding-box",
	    "painted", "page", "paused", "persian", "perspective", "plus-darker", "plus-lighter",
	    "pointer", "polygon", "portrait", "pre", "pre-line", "pre-wrap", "preserve-3d",
	    "progress", "push-button", "radial-gradient", "radio", "read-only",
	    "read-write", "read-write-plaintext-only", "rectangle", "region",
	    "relative", "repeat", "repeating-linear-gradient",
	    "repeating-radial-gradient", "repeat-x", "repeat-y", "reset", "reverse",
	    "rgb", "rgba", "ridge", "right", "rotate", "rotate3d", "rotateX", "rotateY",
	    "rotateZ", "round", "row", "row-resize", "row-reverse", "rtl", "run-in", "running",
	    "s-resize", "sans-serif", "saturation", "scale", "scale3d", "scaleX", "scaleY", "scaleZ", "screen",
	    "scroll", "scrollbar", "scroll-position", "se-resize", "searchfield",
	    "searchfield-cancel-button", "searchfield-decoration",
	    "searchfield-results-button", "searchfield-results-decoration", "self-start", "self-end",
	    "semi-condensed", "semi-expanded", "separate", "serif", "show", "sidama",
	    "simp-chinese-formal", "simp-chinese-informal", "single",
	    "skew", "skewX", "skewY", "skip-white-space", "slide", "slider-horizontal",
	    "slider-vertical", "sliderthumb-horizontal", "sliderthumb-vertical", "slow",
	    "small", "small-caps", "small-caption", "smaller", "soft-light", "solid", "somali",
	    "source-atop", "source-in", "source-out", "source-over", "space", "space-around", "space-between", "space-evenly", "spell-out", "square",
	    "square-button", "start", "static", "status-bar", "stretch", "stroke", "sub",
	    "subpixel-antialiased", "super", "sw-resize", "symbolic", "symbols", "system-ui", "table",
	    "table-caption", "table-cell", "table-column", "table-column-group",
	    "table-footer-group", "table-header-group", "table-row", "table-row-group",
	    "tamil",
	    "telugu", "text", "text-bottom", "text-top", "textarea", "textfield", "thai",
	    "thick", "thin", "threeddarkshadow", "threedface", "threedhighlight",
	    "threedlightshadow", "threedshadow", "tibetan", "tigre", "tigrinya-er",
	    "tigrinya-er-abegede", "tigrinya-et", "tigrinya-et-abegede", "to", "top",
	    "trad-chinese-formal", "trad-chinese-informal", "transform",
	    "translate", "translate3d", "translateX", "translateY", "translateZ",
	    "transparent", "ultra-condensed", "ultra-expanded", "underline", "unset", "up",
	    "upper-alpha", "upper-armenian", "upper-greek", "upper-hexadecimal",
	    "upper-latin", "upper-norwegian", "upper-roman", "uppercase", "urdu", "url",
	    "var", "vertical", "vertical-text", "visible", "visibleFill", "visiblePainted",
	    "visibleStroke", "visual", "w-resize", "wait", "wave", "wider",
	    "window", "windowframe", "windowtext", "words", "wrap", "wrap-reverse", "x-large", "x-small", "xor",
	    "xx-large", "xx-small"
	  ], valueKeywords = keySet(valueKeywords_);

	  var allWords = documentTypes_.concat(mediaTypes_).concat(mediaFeatures_).concat(mediaValueKeywords_)
	    .concat(propertyKeywords_).concat(nonStandardPropertyKeywords_).concat(colorKeywords_)
	    .concat(valueKeywords_);
	  CodeMirror.registerHelper("hintWords", "css", allWords);

	  function tokenCComment(stream, state) {
	    var maybeEnd = false, ch;
	    while ((ch = stream.next()) != null) {
	      if (maybeEnd && ch == "/") {
	        state.tokenize = null;
	        break;
	      }
	      maybeEnd = (ch == "*");
	    }
	    return ["comment", "comment"];
	  }

	  CodeMirror.defineMIME("text/css", {
	    documentTypes: documentTypes,
	    mediaTypes: mediaTypes,
	    mediaFeatures: mediaFeatures,
	    mediaValueKeywords: mediaValueKeywords,
	    propertyKeywords: propertyKeywords,
	    nonStandardPropertyKeywords: nonStandardPropertyKeywords,
	    fontProperties: fontProperties,
	    counterDescriptors: counterDescriptors,
	    colorKeywords: colorKeywords,
	    valueKeywords: valueKeywords,
	    tokenHooks: {
	      "/": function(stream, state) {
	        if (!stream.eat("*")) return false;
	        state.tokenize = tokenCComment;
	        return tokenCComment(stream, state);
	      }
	    },
	    name: "css"
	  });

	  CodeMirror.defineMIME("text/x-scss", {
	    mediaTypes: mediaTypes,
	    mediaFeatures: mediaFeatures,
	    mediaValueKeywords: mediaValueKeywords,
	    propertyKeywords: propertyKeywords,
	    nonStandardPropertyKeywords: nonStandardPropertyKeywords,
	    colorKeywords: colorKeywords,
	    valueKeywords: valueKeywords,
	    fontProperties: fontProperties,
	    allowNested: true,
	    lineComment: "//",
	    tokenHooks: {
	      "/": function(stream, state) {
	        if (stream.eat("/")) {
	          stream.skipToEnd();
	          return ["comment", "comment"];
	        } else if (stream.eat("*")) {
	          state.tokenize = tokenCComment;
	          return tokenCComment(stream, state);
	        } else {
	          return ["operator", "operator"];
	        }
	      },
	      ":": function(stream) {
	        if (stream.match(/\s*\{/, false))
	          return [null, null]
	        return false;
	      },
	      "$": function(stream) {
	        stream.match(/^[\w-]+/);
	        if (stream.match(/^\s*:/, false))
	          return ["variable-2", "variable-definition"];
	        return ["variable-2", "variable"];
	      },
	      "#": function(stream) {
	        if (!stream.eat("{")) return false;
	        return [null, "interpolation"];
	      }
	    },
	    name: "css",
	    helperType: "scss"
	  });

	  CodeMirror.defineMIME("text/x-less", {
	    mediaTypes: mediaTypes,
	    mediaFeatures: mediaFeatures,
	    mediaValueKeywords: mediaValueKeywords,
	    propertyKeywords: propertyKeywords,
	    nonStandardPropertyKeywords: nonStandardPropertyKeywords,
	    colorKeywords: colorKeywords,
	    valueKeywords: valueKeywords,
	    fontProperties: fontProperties,
	    allowNested: true,
	    lineComment: "//",
	    tokenHooks: {
	      "/": function(stream, state) {
	        if (stream.eat("/")) {
	          stream.skipToEnd();
	          return ["comment", "comment"];
	        } else if (stream.eat("*")) {
	          state.tokenize = tokenCComment;
	          return tokenCComment(stream, state);
	        } else {
	          return ["operator", "operator"];
	        }
	      },
	      "@": function(stream) {
	        if (stream.eat("{")) return [null, "interpolation"];
	        if (stream.match(/^(charset|document|font-face|import|(-(moz|ms|o|webkit)-)?keyframes|media|namespace|page|supports)\b/, false)) return false;
	        stream.eatWhile(/[\w\\\-]/);
	        if (stream.match(/^\s*:/, false))
	          return ["variable-2", "variable-definition"];
	        return ["variable-2", "variable"];
	      },
	      "&": function() {
	        return ["atom", "atom"];
	      }
	    },
	    name: "css",
	    helperType: "less"
	  });

	  CodeMirror.defineMIME("text/x-gss", {
	    documentTypes: documentTypes,
	    mediaTypes: mediaTypes,
	    mediaFeatures: mediaFeatures,
	    propertyKeywords: propertyKeywords,
	    nonStandardPropertyKeywords: nonStandardPropertyKeywords,
	    fontProperties: fontProperties,
	    counterDescriptors: counterDescriptors,
	    colorKeywords: colorKeywords,
	    valueKeywords: valueKeywords,
	    supportsAtComponent: true,
	    tokenHooks: {
	      "/": function(stream, state) {
	        if (!stream.eat("*")) return false;
	        state.tokenize = tokenCComment;
	        return tokenCComment(stream, state);
	      }
	    },
	    name: "css",
	    helperType: "gss"
	  });

	});


/***/ }),
/* 11 */
/***/ (function(module, exports, __webpack_require__) {

	// CodeMirror, copyright (c) by Marijn Haverbeke and others
	// Distributed under an MIT license: http://codemirror.net/LICENSE

	(function(mod) {
	  if (true) // CommonJS
	    mod(__webpack_require__(2), __webpack_require__(9), __webpack_require__(8), __webpack_require__(10));
	  else if (typeof define == "function" && define.amd) // AMD
	    define(["../../lib/codemirror", "../xml/xml", "../javascript/javascript", "../css/css"], mod);
	  else // Plain browser env
	    mod(CodeMirror);
	})(function(CodeMirror) {
	  "use strict";

	  var defaultTags = {
	    script: [
	      ["lang", /(javascript|babel)/i, "javascript"],
	      ["type", /^(?:text|application)\/(?:x-)?(?:java|ecma)script$|^module$|^$/i, "javascript"],
	      ["type", /./, "text/plain"],
	      [null, null, "javascript"]
	    ],
	    style:  [
	      ["lang", /^css$/i, "css"],
	      ["type", /^(text\/)?(x-)?(stylesheet|css)$/i, "css"],
	      ["type", /./, "text/plain"],
	      [null, null, "css"]
	    ]
	  };

	  function maybeBackup(stream, pat, style) {
	    var cur = stream.current(), close = cur.search(pat);
	    if (close > -1) {
	      stream.backUp(cur.length - close);
	    } else if (cur.match(/<\/?$/)) {
	      stream.backUp(cur.length);
	      if (!stream.match(pat, false)) stream.match(cur);
	    }
	    return style;
	  }

	  var attrRegexpCache = {};
	  function getAttrRegexp(attr) {
	    var regexp = attrRegexpCache[attr];
	    if (regexp) return regexp;
	    return attrRegexpCache[attr] = new RegExp("\\s+" + attr + "\\s*=\\s*('|\")?([^'\"]+)('|\")?\\s*");
	  }

	  function getAttrValue(text, attr) {
	    var match = text.match(getAttrRegexp(attr))
	    return match ? /^\s*(.*?)\s*$/.exec(match[2])[1] : ""
	  }

	  function getTagRegexp(tagName, anchored) {
	    return new RegExp((anchored ? "^" : "") + "<\/\s*" + tagName + "\s*>", "i");
	  }

	  function addTags(from, to) {
	    for (var tag in from) {
	      var dest = to[tag] || (to[tag] = []);
	      var source = from[tag];
	      for (var i = source.length - 1; i >= 0; i--)
	        dest.unshift(source[i])
	    }
	  }

	  function findMatchingMode(tagInfo, tagText) {
	    for (var i = 0; i < tagInfo.length; i++) {
	      var spec = tagInfo[i];
	      if (!spec[0] || spec[1].test(getAttrValue(tagText, spec[0]))) return spec[2];
	    }
	  }

	  CodeMirror.defineMode("htmlmixed", function (config, parserConfig) {
	    var htmlMode = CodeMirror.getMode(config, {
	      name: "xml",
	      htmlMode: true,
	      multilineTagIndentFactor: parserConfig.multilineTagIndentFactor,
	      multilineTagIndentPastTag: parserConfig.multilineTagIndentPastTag
	    });

	    var tags = {};
	    var configTags = parserConfig && parserConfig.tags, configScript = parserConfig && parserConfig.scriptTypes;
	    addTags(defaultTags, tags);
	    if (configTags) addTags(configTags, tags);
	    if (configScript) for (var i = configScript.length - 1; i >= 0; i--)
	      tags.script.unshift(["type", configScript[i].matches, configScript[i].mode])

	    function html(stream, state) {
	      var style = htmlMode.token(stream, state.htmlState), tag = /\btag\b/.test(style), tagName
	      if (tag && !/[<>\s\/]/.test(stream.current()) &&
	          (tagName = state.htmlState.tagName && state.htmlState.tagName.toLowerCase()) &&
	          tags.hasOwnProperty(tagName)) {
	        state.inTag = tagName + " "
	      } else if (state.inTag && tag && />$/.test(stream.current())) {
	        var inTag = /^([\S]+) (.*)/.exec(state.inTag)
	        state.inTag = null
	        var modeSpec = stream.current() == ">" && findMatchingMode(tags[inTag[1]], inTag[2])
	        var mode = CodeMirror.getMode(config, modeSpec)
	        var endTagA = getTagRegexp(inTag[1], true), endTag = getTagRegexp(inTag[1], false);
	        state.token = function (stream, state) {
	          if (stream.match(endTagA, false)) {
	            state.token = html;
	            state.localState = state.localMode = null;
	            return null;
	          }
	          return maybeBackup(stream, endTag, state.localMode.token(stream, state.localState));
	        };
	        state.localMode = mode;
	        state.localState = CodeMirror.startState(mode, htmlMode.indent(state.htmlState, ""));
	      } else if (state.inTag) {
	        state.inTag += stream.current()
	        if (stream.eol()) state.inTag += " "
	      }
	      return style;
	    };

	    return {
	      startState: function () {
	        var state = CodeMirror.startState(htmlMode);
	        return {token: html, inTag: null, localMode: null, localState: null, htmlState: state};
	      },

	      copyState: function (state) {
	        var local;
	        if (state.localState) {
	          local = CodeMirror.copyState(state.localMode, state.localState);
	        }
	        return {token: state.token, inTag: state.inTag,
	                localMode: state.localMode, localState: local,
	                htmlState: CodeMirror.copyState(htmlMode, state.htmlState)};
	      },

	      token: function (stream, state) {
	        return state.token(stream, state);
	      },

	      indent: function (state, textAfter, line) {
	        if (!state.localMode || /^\s*<\//.test(textAfter))
	          return htmlMode.indent(state.htmlState, textAfter);
	        else if (state.localMode.indent)
	          return state.localMode.indent(state.localState, textAfter, line);
	        else
	          return CodeMirror.Pass;
	      },

	      innerMode: function (state) {
	        return {state: state.localState || state.htmlState, mode: state.localMode || htmlMode};
	      }
	    };
	  }, "xml", "javascript", "css");

	  CodeMirror.defineMIME("text/html", "htmlmixed");
	});


/***/ }),
/* 12 */
/***/ (function(module, exports, __webpack_require__) {

	// CodeMirror, copyright (c) by Marijn Haverbeke and others
	// Distributed under an MIT license: http://codemirror.net/LICENSE

	(function(mod) {
	  if (true) // CommonJS
	    mod(__webpack_require__(2), __webpack_require__(9), __webpack_require__(8))
	  else if (typeof define == "function" && define.amd) // AMD
	    define(["../../lib/codemirror", "../xml/xml", "../javascript/javascript"], mod)
	  else // Plain browser env
	    mod(CodeMirror)
	})(function(CodeMirror) {
	  "use strict"

	  // Depth means the amount of open braces in JS context, in XML
	  // context 0 means not in tag, 1 means in tag, and 2 means in tag
	  // and js block comment.
	  function Context(state, mode, depth, prev) {
	    this.state = state; this.mode = mode; this.depth = depth; this.prev = prev
	  }

	  function copyContext(context) {
	    return new Context(CodeMirror.copyState(context.mode, context.state),
	                       context.mode,
	                       context.depth,
	                       context.prev && copyContext(context.prev))
	  }

	  CodeMirror.defineMode("jsx", function(config, modeConfig) {
	    var xmlMode = CodeMirror.getMode(config, {name: "xml", allowMissing: true, multilineTagIndentPastTag: false})
	    var jsMode = CodeMirror.getMode(config, modeConfig && modeConfig.base || "javascript")

	    function flatXMLIndent(state) {
	      var tagName = state.tagName
	      state.tagName = null
	      var result = xmlMode.indent(state, "")
	      state.tagName = tagName
	      return result
	    }

	    function token(stream, state) {
	      if (state.context.mode == xmlMode)
	        return xmlToken(stream, state, state.context)
	      else
	        return jsToken(stream, state, state.context)
	    }

	    function xmlToken(stream, state, cx) {
	      if (cx.depth == 2) { // Inside a JS /* */ comment
	        if (stream.match(/^.*?\*\//)) cx.depth = 1
	        else stream.skipToEnd()
	        return "comment"
	      }

	      if (stream.peek() == "{") {
	        xmlMode.skipAttribute(cx.state)

	        var indent = flatXMLIndent(cx.state), xmlContext = cx.state.context
	        // If JS starts on same line as tag
	        if (xmlContext && stream.match(/^[^>]*>\s*$/, false)) {
	          while (xmlContext.prev && !xmlContext.startOfLine)
	            xmlContext = xmlContext.prev
	          // If tag starts the line, use XML indentation level
	          if (xmlContext.startOfLine) indent -= config.indentUnit
	          // Else use JS indentation level
	          else if (cx.prev.state.lexical) indent = cx.prev.state.lexical.indented
	        // Else if inside of tag
	        } else if (cx.depth == 1) {
	          indent += config.indentUnit
	        }

	        state.context = new Context(CodeMirror.startState(jsMode, indent),
	                                    jsMode, 0, state.context)
	        return null
	      }

	      if (cx.depth == 1) { // Inside of tag
	        if (stream.peek() == "<") { // Tag inside of tag
	          xmlMode.skipAttribute(cx.state)
	          state.context = new Context(CodeMirror.startState(xmlMode, flatXMLIndent(cx.state)),
	                                      xmlMode, 0, state.context)
	          return null
	        } else if (stream.match("//")) {
	          stream.skipToEnd()
	          return "comment"
	        } else if (stream.match("/*")) {
	          cx.depth = 2
	          return token(stream, state)
	        }
	      }

	      var style = xmlMode.token(stream, cx.state), cur = stream.current(), stop
	      if (/\btag\b/.test(style)) {
	        if (/>$/.test(cur)) {
	          if (cx.state.context) cx.depth = 0
	          else state.context = state.context.prev
	        } else if (/^</.test(cur)) {
	          cx.depth = 1
	        }
	      } else if (!style && (stop = cur.indexOf("{")) > -1) {
	        stream.backUp(cur.length - stop)
	      }
	      return style
	    }

	    function jsToken(stream, state, cx) {
	      if (stream.peek() == "<" && jsMode.expressionAllowed(stream, cx.state)) {
	        jsMode.skipExpression(cx.state)
	        state.context = new Context(CodeMirror.startState(xmlMode, jsMode.indent(cx.state, "")),
	                                    xmlMode, 0, state.context)
	        return null
	      }

	      var style = jsMode.token(stream, cx.state)
	      if (!style && cx.depth != null) {
	        var cur = stream.current()
	        if (cur == "{") {
	          cx.depth++
	        } else if (cur == "}") {
	          if (--cx.depth == 0) state.context = state.context.prev
	        }
	      }
	      return style
	    }

	    return {
	      startState: function() {
	        return {context: new Context(CodeMirror.startState(jsMode), jsMode)}
	      },

	      copyState: function(state) {
	        return {context: copyContext(state.context)}
	      },

	      token: token,

	      indent: function(state, textAfter, fullLine) {
	        return state.context.mode.indent(state.context.state, textAfter, fullLine)
	      },

	      innerMode: function(state) {
	        return state.context
	      }
	    }
	  }, "xml", "javascript")

	  CodeMirror.defineMIME("text/jsx", "jsx")
	  CodeMirror.defineMIME("text/typescript-jsx", {name: "jsx", base: {name: "javascript", typescript: true}})
	});


/***/ }),
/* 13 */
/***/ (function(module, exports, __webpack_require__) {

	// CodeMirror, copyright (c) by Marijn Haverbeke and others
	// Distributed under an MIT license: http://codemirror.net/LICENSE

	/**
	 * Link to the project's GitHub page:
	 * https://github.com/pickhardt/coffeescript-codemirror-mode
	 */
	(function(mod) {
	  if (true) // CommonJS
	    mod(__webpack_require__(2));
	  else if (typeof define == "function" && define.amd) // AMD
	    define(["../../lib/codemirror"], mod);
	  else // Plain browser env
	    mod(CodeMirror);
	})(function(CodeMirror) {
	"use strict";

	CodeMirror.defineMode("coffeescript", function(conf, parserConf) {
	  var ERRORCLASS = "error";

	  function wordRegexp(words) {
	    return new RegExp("^((" + words.join(")|(") + "))\\b");
	  }

	  var operators = /^(?:->|=>|\+[+=]?|-[\-=]?|\*[\*=]?|\/[\/=]?|[=!]=|<[><]?=?|>>?=?|%=?|&=?|\|=?|\^=?|\~|!|\?|(or|and|\|\||&&|\?)=)/;
	  var delimiters = /^(?:[()\[\]{},:`=;]|\.\.?\.?)/;
	  var identifiers = /^[_A-Za-z$][_A-Za-z$0-9]*/;
	  var atProp = /^@[_A-Za-z$][_A-Za-z$0-9]*/;

	  var wordOperators = wordRegexp(["and", "or", "not",
	                                  "is", "isnt", "in",
	                                  "instanceof", "typeof"]);
	  var indentKeywords = ["for", "while", "loop", "if", "unless", "else",
	                        "switch", "try", "catch", "finally", "class"];
	  var commonKeywords = ["break", "by", "continue", "debugger", "delete",
	                        "do", "in", "of", "new", "return", "then",
	                        "this", "@", "throw", "when", "until", "extends"];

	  var keywords = wordRegexp(indentKeywords.concat(commonKeywords));

	  indentKeywords = wordRegexp(indentKeywords);


	  var stringPrefixes = /^('{3}|\"{3}|['\"])/;
	  var regexPrefixes = /^(\/{3}|\/)/;
	  var commonConstants = ["Infinity", "NaN", "undefined", "null", "true", "false", "on", "off", "yes", "no"];
	  var constants = wordRegexp(commonConstants);

	  // Tokenizers
	  function tokenBase(stream, state) {
	    // Handle scope changes
	    if (stream.sol()) {
	      if (state.scope.align === null) state.scope.align = false;
	      var scopeOffset = state.scope.offset;
	      if (stream.eatSpace()) {
	        var lineOffset = stream.indentation();
	        if (lineOffset > scopeOffset && state.scope.type == "coffee") {
	          return "indent";
	        } else if (lineOffset < scopeOffset) {
	          return "dedent";
	        }
	        return null;
	      } else {
	        if (scopeOffset > 0) {
	          dedent(stream, state);
	        }
	      }
	    }
	    if (stream.eatSpace()) {
	      return null;
	    }

	    var ch = stream.peek();

	    // Handle docco title comment (single line)
	    if (stream.match("####")) {
	      stream.skipToEnd();
	      return "comment";
	    }

	    // Handle multi line comments
	    if (stream.match("###")) {
	      state.tokenize = longComment;
	      return state.tokenize(stream, state);
	    }

	    // Single line comment
	    if (ch === "#") {
	      stream.skipToEnd();
	      return "comment";
	    }

	    // Handle number literals
	    if (stream.match(/^-?[0-9\.]/, false)) {
	      var floatLiteral = false;
	      // Floats
	      if (stream.match(/^-?\d*\.\d+(e[\+\-]?\d+)?/i)) {
	        floatLiteral = true;
	      }
	      if (stream.match(/^-?\d+\.\d*/)) {
	        floatLiteral = true;
	      }
	      if (stream.match(/^-?\.\d+/)) {
	        floatLiteral = true;
	      }

	      if (floatLiteral) {
	        // prevent from getting extra . on 1..
	        if (stream.peek() == "."){
	          stream.backUp(1);
	        }
	        return "number";
	      }
	      // Integers
	      var intLiteral = false;
	      // Hex
	      if (stream.match(/^-?0x[0-9a-f]+/i)) {
	        intLiteral = true;
	      }
	      // Decimal
	      if (stream.match(/^-?[1-9]\d*(e[\+\-]?\d+)?/)) {
	        intLiteral = true;
	      }
	      // Zero by itself with no other piece of number.
	      if (stream.match(/^-?0(?![\dx])/i)) {
	        intLiteral = true;
	      }
	      if (intLiteral) {
	        return "number";
	      }
	    }

	    // Handle strings
	    if (stream.match(stringPrefixes)) {
	      state.tokenize = tokenFactory(stream.current(), false, "string");
	      return state.tokenize(stream, state);
	    }
	    // Handle regex literals
	    if (stream.match(regexPrefixes)) {
	      if (stream.current() != "/" || stream.match(/^.*\//, false)) { // prevent highlight of division
	        state.tokenize = tokenFactory(stream.current(), true, "string-2");
	        return state.tokenize(stream, state);
	      } else {
	        stream.backUp(1);
	      }
	    }



	    // Handle operators and delimiters
	    if (stream.match(operators) || stream.match(wordOperators)) {
	      return "operator";
	    }
	    if (stream.match(delimiters)) {
	      return "punctuation";
	    }

	    if (stream.match(constants)) {
	      return "atom";
	    }

	    if (stream.match(atProp) || state.prop && stream.match(identifiers)) {
	      return "property";
	    }

	    if (stream.match(keywords)) {
	      return "keyword";
	    }

	    if (stream.match(identifiers)) {
	      return "variable";
	    }

	    // Handle non-detected items
	    stream.next();
	    return ERRORCLASS;
	  }

	  function tokenFactory(delimiter, singleline, outclass) {
	    return function(stream, state) {
	      while (!stream.eol()) {
	        stream.eatWhile(/[^'"\/\\]/);
	        if (stream.eat("\\")) {
	          stream.next();
	          if (singleline && stream.eol()) {
	            return outclass;
	          }
	        } else if (stream.match(delimiter)) {
	          state.tokenize = tokenBase;
	          return outclass;
	        } else {
	          stream.eat(/['"\/]/);
	        }
	      }
	      if (singleline) {
	        if (parserConf.singleLineStringErrors) {
	          outclass = ERRORCLASS;
	        } else {
	          state.tokenize = tokenBase;
	        }
	      }
	      return outclass;
	    };
	  }

	  function longComment(stream, state) {
	    while (!stream.eol()) {
	      stream.eatWhile(/[^#]/);
	      if (stream.match("###")) {
	        state.tokenize = tokenBase;
	        break;
	      }
	      stream.eatWhile("#");
	    }
	    return "comment";
	  }

	  function indent(stream, state, type) {
	    type = type || "coffee";
	    var offset = 0, align = false, alignOffset = null;
	    for (var scope = state.scope; scope; scope = scope.prev) {
	      if (scope.type === "coffee" || scope.type == "}") {
	        offset = scope.offset + conf.indentUnit;
	        break;
	      }
	    }
	    if (type !== "coffee") {
	      align = null;
	      alignOffset = stream.column() + stream.current().length;
	    } else if (state.scope.align) {
	      state.scope.align = false;
	    }
	    state.scope = {
	      offset: offset,
	      type: type,
	      prev: state.scope,
	      align: align,
	      alignOffset: alignOffset
	    };
	  }

	  function dedent(stream, state) {
	    if (!state.scope.prev) return;
	    if (state.scope.type === "coffee") {
	      var _indent = stream.indentation();
	      var matched = false;
	      for (var scope = state.scope; scope; scope = scope.prev) {
	        if (_indent === scope.offset) {
	          matched = true;
	          break;
	        }
	      }
	      if (!matched) {
	        return true;
	      }
	      while (state.scope.prev && state.scope.offset !== _indent) {
	        state.scope = state.scope.prev;
	      }
	      return false;
	    } else {
	      state.scope = state.scope.prev;
	      return false;
	    }
	  }

	  function tokenLexer(stream, state) {
	    var style = state.tokenize(stream, state);
	    var current = stream.current();

	    // Handle scope changes.
	    if (current === "return") {
	      state.dedent = true;
	    }
	    if (((current === "->" || current === "=>") && stream.eol())
	        || style === "indent") {
	      indent(stream, state);
	    }
	    var delimiter_index = "[({".indexOf(current);
	    if (delimiter_index !== -1) {
	      indent(stream, state, "])}".slice(delimiter_index, delimiter_index+1));
	    }
	    if (indentKeywords.exec(current)){
	      indent(stream, state);
	    }
	    if (current == "then"){
	      dedent(stream, state);
	    }


	    if (style === "dedent") {
	      if (dedent(stream, state)) {
	        return ERRORCLASS;
	      }
	    }
	    delimiter_index = "])}".indexOf(current);
	    if (delimiter_index !== -1) {
	      while (state.scope.type == "coffee" && state.scope.prev)
	        state.scope = state.scope.prev;
	      if (state.scope.type == current)
	        state.scope = state.scope.prev;
	    }
	    if (state.dedent && stream.eol()) {
	      if (state.scope.type == "coffee" && state.scope.prev)
	        state.scope = state.scope.prev;
	      state.dedent = false;
	    }

	    return style;
	  }

	  var external = {
	    startState: function(basecolumn) {
	      return {
	        tokenize: tokenBase,
	        scope: {offset:basecolumn || 0, type:"coffee", prev: null, align: false},
	        prop: false,
	        dedent: 0
	      };
	    },

	    token: function(stream, state) {
	      var fillAlign = state.scope.align === null && state.scope;
	      if (fillAlign && stream.sol()) fillAlign.align = false;

	      var style = tokenLexer(stream, state);
	      if (style && style != "comment") {
	        if (fillAlign) fillAlign.align = true;
	        state.prop = style == "punctuation" && stream.current() == "."
	      }

	      return style;
	    },

	    indent: function(state, text) {
	      if (state.tokenize != tokenBase) return 0;
	      var scope = state.scope;
	      var closer = text && "])}".indexOf(text.charAt(0)) > -1;
	      if (closer) while (scope.type == "coffee" && scope.prev) scope = scope.prev;
	      var closes = closer && scope.type === text.charAt(0);
	      if (scope.align)
	        return scope.alignOffset - (closes ? 1 : 0);
	      else
	        return (closes ? scope.prev : scope).offset;
	    },

	    lineComment: "#",
	    fold: "indent"
	  };
	  return external;
	});

	// IANA registered media type
	// https://www.iana.org/assignments/media-types/
	CodeMirror.defineMIME("application/vnd.coffeescript", "coffeescript");

	CodeMirror.defineMIME("text/x-coffeescript", "coffeescript");
	CodeMirror.defineMIME("text/coffeescript", "coffeescript");

	});


/***/ }),
/* 14 */
/***/ (function(module, exports, __webpack_require__) {

	// CodeMirror, copyright (c) by Marijn Haverbeke and others
	// Distributed under an MIT license: http://codemirror.net/LICENSE

	(function(mod) {
	  if (true) // CommonJS
	    mod(__webpack_require__(2));
	  else if (typeof define == "function" && define.amd) // AMD
	    define(["../../lib/codemirror"], mod);
	  else // Plain browser env
	    mod(CodeMirror);
	})(function(CodeMirror) {
	  "use strict";

	  CodeMirror.defineMode("elm", function() {

	    function switchState(source, setState, f) {
	      setState(f);
	      return f(source, setState);
	    }

	    // These should all be Unicode extended, as per the Haskell 2010 report
	    var smallRE = /[a-z_]/;
	    var largeRE = /[A-Z]/;
	    var digitRE = /[0-9]/;
	    var hexitRE = /[0-9A-Fa-f]/;
	    var octitRE = /[0-7]/;
	    var idRE = /[a-z_A-Z0-9\']/;
	    var symbolRE = /[-!#$%&*+.\/<=>?@\\^|~:\u03BB\u2192]/;
	    var specialRE = /[(),;[\]`{}]/;
	    var whiteCharRE = /[ \t\v\f]/; // newlines are handled in tokenizer

	    function normal() {
	      return function (source, setState) {
	        if (source.eatWhile(whiteCharRE)) {
	          return null;
	        }

	        var ch = source.next();
	        if (specialRE.test(ch)) {
	          if (ch == '{' && source.eat('-')) {
	            var t = "comment";
	            if (source.eat('#')) t = "meta";
	            return switchState(source, setState, ncomment(t, 1));
	          }
	          return null;
	        }

	        if (ch == '\'') {
	          if (source.eat('\\'))
	            source.next();  // should handle other escapes here
	          else
	            source.next();

	          if (source.eat('\''))
	            return "string";
	          return "error";
	        }

	        if (ch == '"') {
	          return switchState(source, setState, stringLiteral);
	        }

	        if (largeRE.test(ch)) {
	          source.eatWhile(idRE);
	          if (source.eat('.'))
	            return "qualifier";
	          return "variable-2";
	        }

	        if (smallRE.test(ch)) {
	          var isDef = source.pos === 1;
	          source.eatWhile(idRE);
	          return isDef ? "type" : "variable";
	        }

	        if (digitRE.test(ch)) {
	          if (ch == '0') {
	            if (source.eat(/[xX]/)) {
	              source.eatWhile(hexitRE); // should require at least 1
	              return "integer";
	            }
	            if (source.eat(/[oO]/)) {
	              source.eatWhile(octitRE); // should require at least 1
	              return "number";
	            }
	          }
	          source.eatWhile(digitRE);
	          var t = "number";
	          if (source.eat('.')) {
	            t = "number";
	            source.eatWhile(digitRE); // should require at least 1
	          }
	          if (source.eat(/[eE]/)) {
	            t = "number";
	            source.eat(/[-+]/);
	            source.eatWhile(digitRE); // should require at least 1
	          }
	          return t;
	        }

	        if (symbolRE.test(ch)) {
	          if (ch == '-' && source.eat(/-/)) {
	            source.eatWhile(/-/);
	            if (!source.eat(symbolRE)) {
	              source.skipToEnd();
	              return "comment";
	            }
	          }
	          source.eatWhile(symbolRE);
	          return "builtin";
	        }

	        return "error";
	      }
	    }

	    function ncomment(type, nest) {
	      if (nest == 0) {
	        return normal();
	      }
	      return function(source, setState) {
	        var currNest = nest;
	        while (!source.eol()) {
	          var ch = source.next();
	          if (ch == '{' && source.eat('-')) {
	            ++currNest;
	          } else if (ch == '-' && source.eat('}')) {
	            --currNest;
	            if (currNest == 0) {
	              setState(normal());
	              return type;
	            }
	          }
	        }
	        setState(ncomment(type, currNest));
	        return type;
	      }
	    }

	    function stringLiteral(source, setState) {
	      while (!source.eol()) {
	        var ch = source.next();
	        if (ch == '"') {
	          setState(normal());
	          return "string";
	        }
	        if (ch == '\\') {
	          if (source.eol() || source.eat(whiteCharRE)) {
	            setState(stringGap);
	            return "string";
	          }
	          if (!source.eat('&')) source.next(); // should handle other escapes here
	        }
	      }
	      setState(normal());
	      return "error";
	    }

	    function stringGap(source, setState) {
	      if (source.eat('\\')) {
	        return switchState(source, setState, stringLiteral);
	      }
	      source.next();
	      setState(normal());
	      return "error";
	    }


	    var wellKnownWords = (function() {
	      var wkw = {};

	      var keywords = [
	        "case", "of", "as",
	        "if", "then", "else",
	        "let", "in",
	        "infix", "infixl", "infixr",
	        "type", "alias",
	        "input", "output", "foreign", "loopback",
	        "module", "where", "import", "exposing",
	        "_", "..", "|", ":", "=", "\\", "\"", "->", "<-"
	      ];

	      for (var i = keywords.length; i--;)
	        wkw[keywords[i]] = "keyword";

	      return wkw;
	    })();



	    return {
	      startState: function ()  { return { f: normal() }; },
	      copyState:  function (s) { return { f: s.f }; },

	      token: function(stream, state) {
	        var t = state.f(stream, function(s) { state.f = s; });
	        var w = stream.current();
	        return (wellKnownWords.hasOwnProperty(w)) ? wellKnownWords[w] : t;
	      }
	    };

	  });

	  CodeMirror.defineMIME("text/x-elm", "elm");
	});


/***/ }),
/* 15 */
/***/ (function(module, exports, __webpack_require__) {

	// CodeMirror, copyright (c) by Marijn Haverbeke and others
	// Distributed under an MIT license: http://codemirror.net/LICENSE

	(function(mod) {
	  if (true) // CommonJS
	    mod(__webpack_require__(2));
	  else if (typeof define == "function" && define.amd) // AMD
	    define(["../../lib/codemirror"], mod);
	  else // Plain browser env
	    mod(CodeMirror);
	})(function(CodeMirror) {
	"use strict";

	function Context(indented, column, type, info, align, prev) {
	  this.indented = indented;
	  this.column = column;
	  this.type = type;
	  this.info = info;
	  this.align = align;
	  this.prev = prev;
	}
	function pushContext(state, col, type, info) {
	  var indent = state.indented;
	  if (state.context && state.context.type == "statement" && type != "statement")
	    indent = state.context.indented;
	  return state.context = new Context(indent, col, type, info, null, state.context);
	}
	function popContext(state) {
	  var t = state.context.type;
	  if (t == ")" || t == "]" || t == "}")
	    state.indented = state.context.indented;
	  return state.context = state.context.prev;
	}

	function typeBefore(stream, state, pos) {
	  if (state.prevToken == "variable" || state.prevToken == "type") return true;
	  if (/\S(?:[^- ]>|[*\]])\s*$|\*$/.test(stream.string.slice(0, pos))) return true;
	  if (state.typeAtEndOfLine && stream.column() == stream.indentation()) return true;
	}

	function isTopScope(context) {
	  for (;;) {
	    if (!context || context.type == "top") return true;
	    if (context.type == "}" && context.prev.info != "namespace") return false;
	    context = context.prev;
	  }
	}

	CodeMirror.defineMode("clike", function(config, parserConfig) {
	  var indentUnit = config.indentUnit,
	      statementIndentUnit = parserConfig.statementIndentUnit || indentUnit,
	      dontAlignCalls = parserConfig.dontAlignCalls,
	      keywords = parserConfig.keywords || {},
	      types = parserConfig.types || {},
	      builtin = parserConfig.builtin || {},
	      blockKeywords = parserConfig.blockKeywords || {},
	      defKeywords = parserConfig.defKeywords || {},
	      atoms = parserConfig.atoms || {},
	      hooks = parserConfig.hooks || {},
	      multiLineStrings = parserConfig.multiLineStrings,
	      indentStatements = parserConfig.indentStatements !== false,
	      indentSwitch = parserConfig.indentSwitch !== false,
	      namespaceSeparator = parserConfig.namespaceSeparator,
	      isPunctuationChar = parserConfig.isPunctuationChar || /[\[\]{}\(\),;\:\.]/,
	      numberStart = parserConfig.numberStart || /[\d\.]/,
	      number = parserConfig.number || /^(?:0x[a-f\d]+|0b[01]+|(?:\d+\.?\d*|\.\d+)(?:e[-+]?\d+)?)(u|ll?|l|f)?/i,
	      isOperatorChar = parserConfig.isOperatorChar || /[+\-*&%=<>!?|\/]/,
	      isIdentifierChar = parserConfig.isIdentifierChar || /[\w\$_\xa1-\uffff]/;

	  var curPunc, isDefKeyword;

	  function tokenBase(stream, state) {
	    var ch = stream.next();
	    if (hooks[ch]) {
	      var result = hooks[ch](stream, state);
	      if (result !== false) return result;
	    }
	    if (ch == '"' || ch == "'") {
	      state.tokenize = tokenString(ch);
	      return state.tokenize(stream, state);
	    }
	    if (isPunctuationChar.test(ch)) {
	      curPunc = ch;
	      return null;
	    }
	    if (numberStart.test(ch)) {
	      stream.backUp(1)
	      if (stream.match(number)) return "number"
	      stream.next()
	    }
	    if (ch == "/") {
	      if (stream.eat("*")) {
	        state.tokenize = tokenComment;
	        return tokenComment(stream, state);
	      }
	      if (stream.eat("/")) {
	        stream.skipToEnd();
	        return "comment";
	      }
	    }
	    if (isOperatorChar.test(ch)) {
	      while (!stream.match(/^\/[\/*]/, false) && stream.eat(isOperatorChar)) {}
	      return "operator";
	    }
	    stream.eatWhile(isIdentifierChar);
	    if (namespaceSeparator) while (stream.match(namespaceSeparator))
	      stream.eatWhile(isIdentifierChar);

	    var cur = stream.current();
	    if (contains(keywords, cur)) {
	      if (contains(blockKeywords, cur)) curPunc = "newstatement";
	      if (contains(defKeywords, cur)) isDefKeyword = true;
	      return "keyword";
	    }
	    if (contains(types, cur)) return "type";
	    if (contains(builtin, cur)) {
	      if (contains(blockKeywords, cur)) curPunc = "newstatement";
	      return "builtin";
	    }
	    if (contains(atoms, cur)) return "atom";
	    return "variable";
	  }

	  function tokenString(quote) {
	    return function(stream, state) {
	      var escaped = false, next, end = false;
	      while ((next = stream.next()) != null) {
	        if (next == quote && !escaped) {end = true; break;}
	        escaped = !escaped && next == "\\";
	      }
	      if (end || !(escaped || multiLineStrings))
	        state.tokenize = null;
	      return "string";
	    };
	  }

	  function tokenComment(stream, state) {
	    var maybeEnd = false, ch;
	    while (ch = stream.next()) {
	      if (ch == "/" && maybeEnd) {
	        state.tokenize = null;
	        break;
	      }
	      maybeEnd = (ch == "*");
	    }
	    return "comment";
	  }

	  function maybeEOL(stream, state) {
	    if (parserConfig.typeFirstDefinitions && stream.eol() && isTopScope(state.context))
	      state.typeAtEndOfLine = typeBefore(stream, state, stream.pos)
	  }

	  // Interface

	  return {
	    startState: function(basecolumn) {
	      return {
	        tokenize: null,
	        context: new Context((basecolumn || 0) - indentUnit, 0, "top", null, false),
	        indented: 0,
	        startOfLine: true,
	        prevToken: null
	      };
	    },

	    token: function(stream, state) {
	      var ctx = state.context;
	      if (stream.sol()) {
	        if (ctx.align == null) ctx.align = false;
	        state.indented = stream.indentation();
	        state.startOfLine = true;
	      }
	      if (stream.eatSpace()) { maybeEOL(stream, state); return null; }
	      curPunc = isDefKeyword = null;
	      var style = (state.tokenize || tokenBase)(stream, state);
	      if (style == "comment" || style == "meta") return style;
	      if (ctx.align == null) ctx.align = true;

	      if (curPunc == ";" || curPunc == ":" || (curPunc == "," && stream.match(/^\s*(?:\/\/.*)?$/, false)))
	        while (state.context.type == "statement") popContext(state);
	      else if (curPunc == "{") pushContext(state, stream.column(), "}");
	      else if (curPunc == "[") pushContext(state, stream.column(), "]");
	      else if (curPunc == "(") pushContext(state, stream.column(), ")");
	      else if (curPunc == "}") {
	        while (ctx.type == "statement") ctx = popContext(state);
	        if (ctx.type == "}") ctx = popContext(state);
	        while (ctx.type == "statement") ctx = popContext(state);
	      }
	      else if (curPunc == ctx.type) popContext(state);
	      else if (indentStatements &&
	               (((ctx.type == "}" || ctx.type == "top") && curPunc != ";") ||
	                (ctx.type == "statement" && curPunc == "newstatement"))) {
	        pushContext(state, stream.column(), "statement", stream.current());
	      }

	      if (style == "variable" &&
	          ((state.prevToken == "def" ||
	            (parserConfig.typeFirstDefinitions && typeBefore(stream, state, stream.start) &&
	             isTopScope(state.context) && stream.match(/^\s*\(/, false)))))
	        style = "def";

	      if (hooks.token) {
	        var result = hooks.token(stream, state, style);
	        if (result !== undefined) style = result;
	      }

	      if (style == "def" && parserConfig.styleDefs === false) style = "variable";

	      state.startOfLine = false;
	      state.prevToken = isDefKeyword ? "def" : style || curPunc;
	      maybeEOL(stream, state);
	      return style;
	    },

	    indent: function(state, textAfter) {
	      if (state.tokenize != tokenBase && state.tokenize != null || state.typeAtEndOfLine) return CodeMirror.Pass;
	      var ctx = state.context, firstChar = textAfter && textAfter.charAt(0);
	      if (ctx.type == "statement" && firstChar == "}") ctx = ctx.prev;
	      if (parserConfig.dontIndentStatements)
	        while (ctx.type == "statement" && parserConfig.dontIndentStatements.test(ctx.info))
	          ctx = ctx.prev
	      if (hooks.indent) {
	        var hook = hooks.indent(state, ctx, textAfter);
	        if (typeof hook == "number") return hook
	      }
	      var closing = firstChar == ctx.type;
	      var switchBlock = ctx.prev && ctx.prev.info == "switch";
	      if (parserConfig.allmanIndentation && /[{(]/.test(firstChar)) {
	        while (ctx.type != "top" && ctx.type != "}") ctx = ctx.prev
	        return ctx.indented
	      }
	      if (ctx.type == "statement")
	        return ctx.indented + (firstChar == "{" ? 0 : statementIndentUnit);
	      if (ctx.align && (!dontAlignCalls || ctx.type != ")"))
	        return ctx.column + (closing ? 0 : 1);
	      if (ctx.type == ")" && !closing)
	        return ctx.indented + statementIndentUnit;

	      return ctx.indented + (closing ? 0 : indentUnit) +
	        (!closing && switchBlock && !/^(?:case|default)\b/.test(textAfter) ? indentUnit : 0);
	    },

	    electricInput: indentSwitch ? /^\s*(?:case .*?:|default:|\{\}?|\})$/ : /^\s*[{}]$/,
	    blockCommentStart: "/*",
	    blockCommentEnd: "*/",
	    blockCommentContinue: " * ",
	    lineComment: "//",
	    fold: "brace"
	  };
	});

	  function words(str) {
	    var obj = {}, words = str.split(" ");
	    for (var i = 0; i < words.length; ++i) obj[words[i]] = true;
	    return obj;
	  }
	  function contains(words, word) {
	    if (typeof words === "function") {
	      return words(word);
	    } else {
	      return words.propertyIsEnumerable(word);
	    }
	  }
	  var cKeywords = "auto if break case register continue return default do sizeof " +
	    "static else struct switch extern typedef union for goto while enum const volatile";
	  var cTypes = "int long char short double float unsigned signed void size_t ptrdiff_t";

	  function cppHook(stream, state) {
	    if (!state.startOfLine) return false
	    for (var ch, next = null; ch = stream.peek();) {
	      if (ch == "\\" && stream.match(/^.$/)) {
	        next = cppHook
	        break
	      } else if (ch == "/" && stream.match(/^\/[\/\*]/, false)) {
	        break
	      }
	      stream.next()
	    }
	    state.tokenize = next
	    return "meta"
	  }

	  function pointerHook(_stream, state) {
	    if (state.prevToken == "type") return "type";
	    return false;
	  }

	  function cpp14Literal(stream) {
	    stream.eatWhile(/[\w\.']/);
	    return "number";
	  }

	  function cpp11StringHook(stream, state) {
	    stream.backUp(1);
	    // Raw strings.
	    if (stream.match(/(R|u8R|uR|UR|LR)/)) {
	      var match = stream.match(/"([^\s\\()]{0,16})\(/);
	      if (!match) {
	        return false;
	      }
	      state.cpp11RawStringDelim = match[1];
	      state.tokenize = tokenRawString;
	      return tokenRawString(stream, state);
	    }
	    // Unicode strings/chars.
	    if (stream.match(/(u8|u|U|L)/)) {
	      if (stream.match(/["']/, /* eat */ false)) {
	        return "string";
	      }
	      return false;
	    }
	    // Ignore this hook.
	    stream.next();
	    return false;
	  }

	  function cppLooksLikeConstructor(word) {
	    var lastTwo = /(\w+)::~?(\w+)$/.exec(word);
	    return lastTwo && lastTwo[1] == lastTwo[2];
	  }

	  // C#-style strings where "" escapes a quote.
	  function tokenAtString(stream, state) {
	    var next;
	    while ((next = stream.next()) != null) {
	      if (next == '"' && !stream.eat('"')) {
	        state.tokenize = null;
	        break;
	      }
	    }
	    return "string";
	  }

	  // C++11 raw string literal is <prefix>"<delim>( anything )<delim>", where
	  // <delim> can be a string up to 16 characters long.
	  function tokenRawString(stream, state) {
	    // Escape characters that have special regex meanings.
	    var delim = state.cpp11RawStringDelim.replace(/[^\w\s]/g, '\\$&');
	    var match = stream.match(new RegExp(".*?\\)" + delim + '"'));
	    if (match)
	      state.tokenize = null;
	    else
	      stream.skipToEnd();
	    return "string";
	  }

	  function def(mimes, mode) {
	    if (typeof mimes == "string") mimes = [mimes];
	    var words = [];
	    function add(obj) {
	      if (obj) for (var prop in obj) if (obj.hasOwnProperty(prop))
	        words.push(prop);
	    }
	    add(mode.keywords);
	    add(mode.types);
	    add(mode.builtin);
	    add(mode.atoms);
	    if (words.length) {
	      mode.helperType = mimes[0];
	      CodeMirror.registerHelper("hintWords", mimes[0], words);
	    }

	    for (var i = 0; i < mimes.length; ++i)
	      CodeMirror.defineMIME(mimes[i], mode);
	  }

	  def(["text/x-csrc", "text/x-c", "text/x-chdr"], {
	    name: "clike",
	    keywords: words(cKeywords),
	    types: words(cTypes + " bool _Complex _Bool float_t double_t intptr_t intmax_t " +
	                 "int8_t int16_t int32_t int64_t uintptr_t uintmax_t uint8_t uint16_t " +
	                 "uint32_t uint64_t"),
	    blockKeywords: words("case do else for if switch while struct"),
	    defKeywords: words("struct"),
	    typeFirstDefinitions: true,
	    atoms: words("null true false"),
	    hooks: {"#": cppHook, "*": pointerHook},
	    modeProps: {fold: ["brace", "include"]}
	  });

	  def(["text/x-c++src", "text/x-c++hdr"], {
	    name: "clike",
	    keywords: words(cKeywords + " asm dynamic_cast namespace reinterpret_cast try explicit new " +
	                    "static_cast typeid catch operator template typename class friend private " +
	                    "this using const_cast inline public throw virtual delete mutable protected " +
	                    "alignas alignof constexpr decltype nullptr noexcept thread_local final " +
	                    "static_assert override"),
	    types: words(cTypes + " bool wchar_t"),
	    blockKeywords: words("catch class do else finally for if struct switch try while"),
	    defKeywords: words("class namespace struct enum union"),
	    typeFirstDefinitions: true,
	    atoms: words("true false null"),
	    dontIndentStatements: /^template$/,
	    isIdentifierChar: /[\w\$_~\xa1-\uffff]/,
	    hooks: {
	      "#": cppHook,
	      "*": pointerHook,
	      "u": cpp11StringHook,
	      "U": cpp11StringHook,
	      "L": cpp11StringHook,
	      "R": cpp11StringHook,
	      "0": cpp14Literal,
	      "1": cpp14Literal,
	      "2": cpp14Literal,
	      "3": cpp14Literal,
	      "4": cpp14Literal,
	      "5": cpp14Literal,
	      "6": cpp14Literal,
	      "7": cpp14Literal,
	      "8": cpp14Literal,
	      "9": cpp14Literal,
	      token: function(stream, state, style) {
	        if (style == "variable" && stream.peek() == "(" &&
	            (state.prevToken == ";" || state.prevToken == null ||
	             state.prevToken == "}") &&
	            cppLooksLikeConstructor(stream.current()))
	          return "def";
	      }
	    },
	    namespaceSeparator: "::",
	    modeProps: {fold: ["brace", "include"]}
	  });

	  def("text/x-java", {
	    name: "clike",
	    keywords: words("abstract assert break case catch class const continue default " +
	                    "do else enum extends final finally float for goto if implements import " +
	                    "instanceof interface native new package private protected public " +
	                    "return static strictfp super switch synchronized this throw throws transient " +
	                    "try volatile while @interface"),
	    types: words("byte short int long float double boolean char void Boolean Byte Character Double Float " +
	                 "Integer Long Number Object Short String StringBuffer StringBuilder Void"),
	    blockKeywords: words("catch class do else finally for if switch try while"),
	    defKeywords: words("class interface enum @interface"),
	    typeFirstDefinitions: true,
	    atoms: words("true false null"),
	    number: /^(?:0x[a-f\d_]+|0b[01_]+|(?:[\d_]+\.?\d*|\.\d+)(?:e[-+]?[\d_]+)?)(u|ll?|l|f)?/i,
	    hooks: {
	      "@": function(stream) {
	        // Don't match the @interface keyword.
	        if (stream.match('interface', false)) return false;

	        stream.eatWhile(/[\w\$_]/);
	        return "meta";
	      }
	    },
	    modeProps: {fold: ["brace", "import"]}
	  });

	  def("text/x-csharp", {
	    name: "clike",
	    keywords: words("abstract as async await base break case catch checked class const continue" +
	                    " default delegate do else enum event explicit extern finally fixed for" +
	                    " foreach goto if implicit in interface internal is lock namespace new" +
	                    " operator out override params private protected public readonly ref return sealed" +
	                    " sizeof stackalloc static struct switch this throw try typeof unchecked" +
	                    " unsafe using virtual void volatile while add alias ascending descending dynamic from get" +
	                    " global group into join let orderby partial remove select set value var yield"),
	    types: words("Action Boolean Byte Char DateTime DateTimeOffset Decimal Double Func" +
	                 " Guid Int16 Int32 Int64 Object SByte Single String Task TimeSpan UInt16 UInt32" +
	                 " UInt64 bool byte char decimal double short int long object"  +
	                 " sbyte float string ushort uint ulong"),
	    blockKeywords: words("catch class do else finally for foreach if struct switch try while"),
	    defKeywords: words("class interface namespace struct var"),
	    typeFirstDefinitions: true,
	    atoms: words("true false null"),
	    hooks: {
	      "@": function(stream, state) {
	        if (stream.eat('"')) {
	          state.tokenize = tokenAtString;
	          return tokenAtString(stream, state);
	        }
	        stream.eatWhile(/[\w\$_]/);
	        return "meta";
	      }
	    }
	  });

	  function tokenTripleString(stream, state) {
	    var escaped = false;
	    while (!stream.eol()) {
	      if (!escaped && stream.match('"""')) {
	        state.tokenize = null;
	        break;
	      }
	      escaped = stream.next() == "\\" && !escaped;
	    }
	    return "string";
	  }

	  def("text/x-scala", {
	    name: "clike",
	    keywords: words(

	      /* scala */
	      "abstract case catch class def do else extends final finally for forSome if " +
	      "implicit import lazy match new null object override package private protected return " +
	      "sealed super this throw trait try type val var while with yield _ " +

	      /* package scala */
	      "assert assume require print println printf readLine readBoolean readByte readShort " +
	      "readChar readInt readLong readFloat readDouble"
	    ),
	    types: words(
	      "AnyVal App Application Array BufferedIterator BigDecimal BigInt Char Console Either " +
	      "Enumeration Equiv Error Exception Fractional Function IndexedSeq Int Integral Iterable " +
	      "Iterator List Map Numeric Nil NotNull Option Ordered Ordering PartialFunction PartialOrdering " +
	      "Product Proxy Range Responder Seq Serializable Set Specializable Stream StringBuilder " +
	      "StringContext Symbol Throwable Traversable TraversableOnce Tuple Unit Vector " +

	      /* package java.lang */
	      "Boolean Byte Character CharSequence Class ClassLoader Cloneable Comparable " +
	      "Compiler Double Exception Float Integer Long Math Number Object Package Pair Process " +
	      "Runtime Runnable SecurityManager Short StackTraceElement StrictMath String " +
	      "StringBuffer System Thread ThreadGroup ThreadLocal Throwable Triple Void"
	    ),
	    multiLineStrings: true,
	    blockKeywords: words("catch class enum do else finally for forSome if match switch try while"),
	    defKeywords: words("class enum def object package trait type val var"),
	    atoms: words("true false null"),
	    indentStatements: false,
	    indentSwitch: false,
	    isOperatorChar: /[+\-*&%=<>!?|\/#:@]/,
	    hooks: {
	      "@": function(stream) {
	        stream.eatWhile(/[\w\$_]/);
	        return "meta";
	      },
	      '"': function(stream, state) {
	        if (!stream.match('""')) return false;
	        state.tokenize = tokenTripleString;
	        return state.tokenize(stream, state);
	      },
	      "'": function(stream) {
	        stream.eatWhile(/[\w\$_\xa1-\uffff]/);
	        return "atom";
	      },
	      "=": function(stream, state) {
	        var cx = state.context
	        if (cx.type == "}" && cx.align && stream.eat(">")) {
	          state.context = new Context(cx.indented, cx.column, cx.type, cx.info, null, cx.prev)
	          return "operator"
	        } else {
	          return false
	        }
	      }
	    },
	    modeProps: {closeBrackets: {triples: '"'}}
	  });

	  function tokenKotlinString(tripleString){
	    return function (stream, state) {
	      var escaped = false, next, end = false;
	      while (!stream.eol()) {
	        if (!tripleString && !escaped && stream.match('"') ) {end = true; break;}
	        if (tripleString && stream.match('"""')) {end = true; break;}
	        next = stream.next();
	        if(!escaped && next == "$" && stream.match('{'))
	          stream.skipTo("}");
	        escaped = !escaped && next == "\\" && !tripleString;
	      }
	      if (end || !tripleString)
	        state.tokenize = null;
	      return "string";
	    }
	  }

	  def("text/x-kotlin", {
	    name: "clike",
	    keywords: words(
	      /*keywords*/
	      "package as typealias class interface this super val " +
	      "var fun for is in This throw return " +
	      "break continue object if else while do try when !in !is as? " +

	      /*soft keywords*/
	      "file import where by get set abstract enum open inner override private public internal " +
	      "protected catch finally out final vararg reified dynamic companion constructor init " +
	      "sealed field property receiver param sparam lateinit data inline noinline tailrec " +
	      "external annotation crossinline const operator infix suspend"
	    ),
	    types: words(
	      /* package java.lang */
	      "Boolean Byte Character CharSequence Class ClassLoader Cloneable Comparable " +
	      "Compiler Double Exception Float Integer Long Math Number Object Package Pair Process " +
	      "Runtime Runnable SecurityManager Short StackTraceElement StrictMath String " +
	      "StringBuffer System Thread ThreadGroup ThreadLocal Throwable Triple Void"
	    ),
	    intendSwitch: false,
	    indentStatements: false,
	    multiLineStrings: true,
	    number: /^(?:0x[a-f\d_]+|0b[01_]+|(?:[\d_]+\.?\d*|\.\d+)(?:e[-+]?[\d_]+)?)(u|ll?|l|f)?/i,
	    blockKeywords: words("catch class do else finally for if where try while enum"),
	    defKeywords: words("class val var object interface fun"),
	    atoms: words("true false null this"),
	    hooks: {
	      '"': function(stream, state) {
	        state.tokenize = tokenKotlinString(stream.match('""'));
	        return state.tokenize(stream, state);
	      }
	    },
	    modeProps: {closeBrackets: {triples: '"'}}
	  });

	  def(["x-shader/x-vertex", "x-shader/x-fragment"], {
	    name: "clike",
	    keywords: words("sampler1D sampler2D sampler3D samplerCube " +
	                    "sampler1DShadow sampler2DShadow " +
	                    "const attribute uniform varying " +
	                    "break continue discard return " +
	                    "for while do if else struct " +
	                    "in out inout"),
	    types: words("float int bool void " +
	                 "vec2 vec3 vec4 ivec2 ivec3 ivec4 bvec2 bvec3 bvec4 " +
	                 "mat2 mat3 mat4"),
	    blockKeywords: words("for while do if else struct"),
	    builtin: words("radians degrees sin cos tan asin acos atan " +
	                    "pow exp log exp2 sqrt inversesqrt " +
	                    "abs sign floor ceil fract mod min max clamp mix step smoothstep " +
	                    "length distance dot cross normalize ftransform faceforward " +
	                    "reflect refract matrixCompMult " +
	                    "lessThan lessThanEqual greaterThan greaterThanEqual " +
	                    "equal notEqual any all not " +
	                    "texture1D texture1DProj texture1DLod texture1DProjLod " +
	                    "texture2D texture2DProj texture2DLod texture2DProjLod " +
	                    "texture3D texture3DProj texture3DLod texture3DProjLod " +
	                    "textureCube textureCubeLod " +
	                    "shadow1D shadow2D shadow1DProj shadow2DProj " +
	                    "shadow1DLod shadow2DLod shadow1DProjLod shadow2DProjLod " +
	                    "dFdx dFdy fwidth " +
	                    "noise1 noise2 noise3 noise4"),
	    atoms: words("true false " +
	                "gl_FragColor gl_SecondaryColor gl_Normal gl_Vertex " +
	                "gl_MultiTexCoord0 gl_MultiTexCoord1 gl_MultiTexCoord2 gl_MultiTexCoord3 " +
	                "gl_MultiTexCoord4 gl_MultiTexCoord5 gl_MultiTexCoord6 gl_MultiTexCoord7 " +
	                "gl_FogCoord gl_PointCoord " +
	                "gl_Position gl_PointSize gl_ClipVertex " +
	                "gl_FrontColor gl_BackColor gl_FrontSecondaryColor gl_BackSecondaryColor " +
	                "gl_TexCoord gl_FogFragCoord " +
	                "gl_FragCoord gl_FrontFacing " +
	                "gl_FragData gl_FragDepth " +
	                "gl_ModelViewMatrix gl_ProjectionMatrix gl_ModelViewProjectionMatrix " +
	                "gl_TextureMatrix gl_NormalMatrix gl_ModelViewMatrixInverse " +
	                "gl_ProjectionMatrixInverse gl_ModelViewProjectionMatrixInverse " +
	                "gl_TexureMatrixTranspose gl_ModelViewMatrixInverseTranspose " +
	                "gl_ProjectionMatrixInverseTranspose " +
	                "gl_ModelViewProjectionMatrixInverseTranspose " +
	                "gl_TextureMatrixInverseTranspose " +
	                "gl_NormalScale gl_DepthRange gl_ClipPlane " +
	                "gl_Point gl_FrontMaterial gl_BackMaterial gl_LightSource gl_LightModel " +
	                "gl_FrontLightModelProduct gl_BackLightModelProduct " +
	                "gl_TextureColor gl_EyePlaneS gl_EyePlaneT gl_EyePlaneR gl_EyePlaneQ " +
	                "gl_FogParameters " +
	                "gl_MaxLights gl_MaxClipPlanes gl_MaxTextureUnits gl_MaxTextureCoords " +
	                "gl_MaxVertexAttribs gl_MaxVertexUniformComponents gl_MaxVaryingFloats " +
	                "gl_MaxVertexTextureImageUnits gl_MaxTextureImageUnits " +
	                "gl_MaxFragmentUniformComponents gl_MaxCombineTextureImageUnits " +
	                "gl_MaxDrawBuffers"),
	    indentSwitch: false,
	    hooks: {"#": cppHook},
	    modeProps: {fold: ["brace", "include"]}
	  });

	  def("text/x-nesc", {
	    name: "clike",
	    keywords: words(cKeywords + "as atomic async call command component components configuration event generic " +
	                    "implementation includes interface module new norace nx_struct nx_union post provides " +
	                    "signal task uses abstract extends"),
	    types: words(cTypes),
	    blockKeywords: words("case do else for if switch while struct"),
	    atoms: words("null true false"),
	    hooks: {"#": cppHook},
	    modeProps: {fold: ["brace", "include"]}
	  });

	  def("text/x-objectivec", {
	    name: "clike",
	    keywords: words(cKeywords + "inline restrict _Bool _Complex _Imaginary BOOL Class bycopy byref id IMP in " +
	                    "inout nil oneway out Protocol SEL self super atomic nonatomic retain copy readwrite readonly"),
	    types: words(cTypes),
	    atoms: words("YES NO NULL NILL ON OFF true false"),
	    hooks: {
	      "@": function(stream) {
	        stream.eatWhile(/[\w\$]/);
	        return "keyword";
	      },
	      "#": cppHook,
	      indent: function(_state, ctx, textAfter) {
	        if (ctx.type == "statement" && /^@\w/.test(textAfter)) return ctx.indented
	      }
	    },
	    modeProps: {fold: "brace"}
	  });

	  def("text/x-squirrel", {
	    name: "clike",
	    keywords: words("base break clone continue const default delete enum extends function in class" +
	                    " foreach local resume return this throw typeof yield constructor instanceof static"),
	    types: words(cTypes),
	    blockKeywords: words("case catch class else for foreach if switch try while"),
	    defKeywords: words("function local class"),
	    typeFirstDefinitions: true,
	    atoms: words("true false null"),
	    hooks: {"#": cppHook},
	    modeProps: {fold: ["brace", "include"]}
	  });

	  // Ceylon Strings need to deal with interpolation
	  var stringTokenizer = null;
	  function tokenCeylonString(type) {
	    return function(stream, state) {
	      var escaped = false, next, end = false;
	      while (!stream.eol()) {
	        if (!escaped && stream.match('"') &&
	              (type == "single" || stream.match('""'))) {
	          end = true;
	          break;
	        }
	        if (!escaped && stream.match('``')) {
	          stringTokenizer = tokenCeylonString(type);
	          end = true;
	          break;
	        }
	        next = stream.next();
	        escaped = type == "single" && !escaped && next == "\\";
	      }
	      if (end)
	          state.tokenize = null;
	      return "string";
	    }
	  }

	  def("text/x-ceylon", {
	    name: "clike",
	    keywords: words("abstracts alias assembly assert assign break case catch class continue dynamic else" +
	                    " exists extends finally for function given if import in interface is let module new" +
	                    " nonempty object of out outer package return satisfies super switch then this throw" +
	                    " try value void while"),
	    types: function(word) {
	        // In Ceylon all identifiers that start with an uppercase are types
	        var first = word.charAt(0);
	        return (first === first.toUpperCase() && first !== first.toLowerCase());
	    },
	    blockKeywords: words("case catch class dynamic else finally for function if interface module new object switch try while"),
	    defKeywords: words("class dynamic function interface module object package value"),
	    builtin: words("abstract actual aliased annotation by default deprecated doc final formal late license" +
	                   " native optional sealed see serializable shared suppressWarnings tagged throws variable"),
	    isPunctuationChar: /[\[\]{}\(\),;\:\.`]/,
	    isOperatorChar: /[+\-*&%=<>!?|^~:\/]/,
	    numberStart: /[\d#$]/,
	    number: /^(?:#[\da-fA-F_]+|\$[01_]+|[\d_]+[kMGTPmunpf]?|[\d_]+\.[\d_]+(?:[eE][-+]?\d+|[kMGTPmunpf]|)|)/i,
	    multiLineStrings: true,
	    typeFirstDefinitions: true,
	    atoms: words("true false null larger smaller equal empty finished"),
	    indentSwitch: false,
	    styleDefs: false,
	    hooks: {
	      "@": function(stream) {
	        stream.eatWhile(/[\w\$_]/);
	        return "meta";
	      },
	      '"': function(stream, state) {
	          state.tokenize = tokenCeylonString(stream.match('""') ? "triple" : "single");
	          return state.tokenize(stream, state);
	        },
	      '`': function(stream, state) {
	          if (!stringTokenizer || !stream.match('`')) return false;
	          state.tokenize = stringTokenizer;
	          stringTokenizer = null;
	          return state.tokenize(stream, state);
	        },
	      "'": function(stream) {
	        stream.eatWhile(/[\w\$_\xa1-\uffff]/);
	        return "atom";
	      },
	      token: function(_stream, state, style) {
	          if ((style == "variable" || style == "type") &&
	              state.prevToken == ".") {
	            return "variable-2";
	          }
	        }
	    },
	    modeProps: {
	        fold: ["brace", "import"],
	        closeBrackets: {triples: '"'}
	    }
	  });

	});


/***/ }),
/* 16 */
/***/ (function(module, exports, __webpack_require__) {

	var __WEBPACK_AMD_DEFINE_FACTORY__, __WEBPACK_AMD_DEFINE_ARRAY__, __WEBPACK_AMD_DEFINE_RESULT__;/* This Source Code Form is subject to the terms of the Mozilla Public
	 * License, v. 2.0. If a copy of the MPL was not distributed with this
	 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

	// WebAssembly experimental syntax highlight add-on for CodeMirror.

	(function (root, factory) {
	  if (true) {
	    !(__WEBPACK_AMD_DEFINE_ARRAY__ = [__webpack_require__(2)], __WEBPACK_AMD_DEFINE_FACTORY__ = (factory), __WEBPACK_AMD_DEFINE_RESULT__ = (typeof __WEBPACK_AMD_DEFINE_FACTORY__ === 'function' ? (__WEBPACK_AMD_DEFINE_FACTORY__.apply(exports, __WEBPACK_AMD_DEFINE_ARRAY__)) : __WEBPACK_AMD_DEFINE_FACTORY__), __WEBPACK_AMD_DEFINE_RESULT__ !== undefined && (module.exports = __WEBPACK_AMD_DEFINE_RESULT__));
	  } else if (typeof exports !== 'undefined') {
	    factory(require("../../lib/codemirror"));
	  } else {
	    factory(root.CodeMirror);
	  }
	}(this, function (CodeMirror) {
	"use strict";

	var isWordChar = /[\w\$_\.\\\/@]/;

	function createLookupTable(list) {
	  var obj = Object.create(null);
	  list.forEach(function (key) {
	    obj[key] = true;
	  });
	  return obj;
	}

	CodeMirror.defineMode("wasm", function() {
	  var keywords = createLookupTable([
	    "function", "import", "export", "table", "memory", "segment", "as", "type",
	    "of", "from", "typeof", "br", "br_if", "loop", "br_table", "if", "else",
	    "call", "call_import", "call_indirect", "nop", "unreachable", "var",
	    "align", "select", "return"]);
	  var builtins = createLookupTable([
	    "i32:8", "i32:8u", "i32:8s", "i32:16", "i32:16u", "i32:16s",
	    "i64:8", "i64:8u", "i64:8s", "i64:16", "i64:16u", "i64:16s",
	    "i64:32", "i64:32u", "i64:32s",
	    "i32.add", "i32.sub", "i32.mul", "i32.div_s", "i32.div_u",
	    "i32.rem_s", "i32.rem_u", "i32.and", "i32.or", "i32.xor",
	    "i32.shl", "i32.shr_u", "i32.shr_s", "i32.rotr", "i32.rotl",
	    "i32.eq", "i32.ne", "i32.lt_s", "i32.le_s", "i32.lt_u",
	    "i32.le_u", "i32.gt_s", "i32.ge_s", "i32.gt_u", "i32.ge_u",
	    "i32.clz", "i32.ctz", "i32.popcnt", "i32.eqz", "i64.add",
	    "i64.sub", "i64.mul", "i64.div_s", "i64.div_u", "i64.rem_s",
	    "i64.rem_u", "i64.and", "i64.or", "i64.xor", "i64.shl",
	    "i64.shr_u", "i64.shr_s", "i64.rotr", "i64.rotl", "i64.eq",
	    "i64.ne", "i64.lt_s", "i64.le_s", "i64.lt_u", "i64.le_u",
	    "i64.gt_s", "i64.ge_s", "i64.gt_u", "i64.ge_u", "i64.clz",
	    "i64.ctz", "i64.popcnt", "i64.eqz", "f32.add", "f32.sub",
	    "f32.mul", "f32.div", "f32.min", "f32.max", "f32.abs",
	    "f32.neg", "f32.copysign", "f32.ceil", "f32.floor", "f32.trunc",
	    "f32.nearest", "f32.sqrt", "f32.eq", "f32.ne", "f32.lt",
	    "f32.le", "f32.gt", "f32.ge", "f64.add", "f64.sub", "f64.mul",
	    "f64.div", "f64.min", "f64.max", "f64.abs", "f64.neg",
	    "f64.copysign", "f64.ceil", "f64.floor", "f64.trunc", "f64.nearest",
	    "f64.sqrt", "f64.eq", "f64.ne", "f64.lt", "f64.le", "f64.gt",
	    "f64.ge", "i32.trunc_s/f32", "i32.trunc_s/f64", "i32.trunc_u/f32",
	    "i32.trunc_u/f64", "i32.wrap/i64", "i64.trunc_s/f32",
	    "i64.trunc_s/f64", "i64.trunc_u/f32", "i64.trunc_u/f64",
	    "i64.extend_s/i32", "i64.extend_u/i32", "f32.convert_s/i32",
	    "f32.convert_u/i32", "f32.convert_s/i64", "f32.convert_u/i64",
	    "f32.demote/f64", "f32.reinterpret/i32", "f64.convert_s/i32",
	    "f64.convert_u/i32", "f64.convert_s/i64", "f64.convert_u/i64",
	    "f64.promote/f32", "f64.reinterpret/i64", "i32.reinterpret/f32",
	    "i64.reinterpret/f64"]);
	  var dataTypes = createLookupTable(["i32", "i64", "f32", "f64"]);
	  var isUnaryOperator = /[\-!]/;
	  var operators = createLookupTable([
	    "+", "-", "*", "/", "/s", "/u", "%", "%s", "%u",
	    "<<", ">>u", ">>s", ">=", "<=", "==", "!=",
	    "<s", "<u", "<=s", "<=u", ">=s", ">=u", ">s", ">u",
	    "<", ">", "=", "&", "|", "^", "!"]);

	  function tokenBase(stream, state) {
	    var ch = stream.next();
	    if (ch === "$") {
	      stream.eatWhile(isWordChar);
	      return "variable";
	    }
	    if (ch === "@") {
	      stream.eatWhile(isWordChar);
	      return "meta";
	    }
	    if (ch === '"') {
	      state.tokenize = tokenString(ch);
	      return state.tokenize(stream, state);
	    }
	    if (ch == "/") {
	      if (stream.eat("*")) {
	        state.tokenize = tokenComment;
	        return tokenComment(stream, state);
	      } else if (stream.eat("/")) {
	        stream.skipToEnd();
	        return "comment";
	      }
	    }
	    if (/\d/.test(ch) ||
	        ((ch === "-" || ch === "+") && /\d/.test(stream.peek()))) {
	      stream.eatWhile(/[\w\._\-+]/);
	      return "number";
	    }
	    if (/[\[\]\(\)\{\},:]/.test(ch)) {
	      return null;
	    }
	    if (isUnaryOperator.test(ch)) {
	      return "operator";
	    }
	    stream.eatWhile(isWordChar);
	    var word = stream.current();

	    if (word in operators) {
	      return "operator";
	    }
	    if (word in keywords){
	      return "keyword";
	    }
	    if (word in dataTypes) {
	      if (!stream.eat(":")) {
	        return "builtin";
	      }
	      stream.eatWhile(isWordChar);
	      word = stream.current();
	      // fall thru for "builtin" check
	    }
	    if (word in builtins) {
	      return "builtin";
	    }

	    if (word === "Temporary") {
	      // Nightly has header with some text graphics -- skipping it.
	      state.tokenize = tokenTemporary;
	      return state.tokenize(stream, state);
	    }
	    return null;
	  }

	  function tokenComment(stream, state) {
	    state.commentDepth = 1;
	    var next;
	    while ((next = stream.next()) != null) {
	      if (next === "*" && stream.eat("/")) {
	        if (--state.commentDepth === 0) {
	          state.tokenize = null;
	          return "comment";
	        }
	      }
	      if (next === "/" && stream.eat("*")) {
	        // Nested comment
	        state.commentDepth++;
	      }
	    }
	    return "comment";
	  }

	  function tokenTemporary(stream, state) {
	    var next, endState = state.commentState;
	    // Skipping until "text support (Work In Progress):" is found.
	    while ((next = stream.next()) != null) {
	      if (endState === 0 && next === "t") {
	        endState = 1;
	      } else if (endState === 1 && next === ":") {
	        state.tokenize = null;
	        state.commentState = 0;
	        endState = 2;
	        return "comment";
	      }
	    }
	    state.commentState = endState;
	    return "comment";
	  }

	  function tokenString(quote) {
	    return function(stream, state) {
	      var escaped = false, next, end = false;
	      while ((next = stream.next()) != null) {
	        if (next == quote && !escaped) {
	          state.tokenize = null;
	          return "string";
	        }
	        escaped = !escaped && next === "\\";
	      }
	      return "string";
	    };
	  }

	  return {
	    startState: function() {
	      return {tokenize: null, commentState: 0, commentDepth: 0};
	    },

	    token: function(stream, state) {
	      if (stream.eatSpace()) return null;
	      var style = (state.tokenize || tokenBase)(stream, state);
	      return style;
	    }
	  };
	});

	CodeMirror.registerHelper("wordChars", "wasm", isWordChar);

	CodeMirror.defineMIME("text/wasm", "wasm");

	}));


/***/ }),
/* 17 */
/***/ (function(module, exports, __webpack_require__) {

	// CodeMirror, copyright (c) by Marijn Haverbeke and others
	// Distributed under an MIT license: http://codemirror.net/LICENSE

	(function(mod) {
	  if (true) // CommonJS
	    mod(__webpack_require__(2));
	  else if (typeof define == "function" && define.amd) // AMD
	    define(["../../lib/codemirror"], mod);
	  else // Plain browser env
	    mod(CodeMirror);
	})(function(CodeMirror) {
	  "use strict";
	  var WRAP_CLASS = "CodeMirror-activeline";
	  var BACK_CLASS = "CodeMirror-activeline-background";
	  var GUTT_CLASS = "CodeMirror-activeline-gutter";

	  CodeMirror.defineOption("styleActiveLine", false, function(cm, val, old) {
	    var prev = old == CodeMirror.Init ? false : old;
	    if (val == prev) return
	    if (prev) {
	      cm.off("beforeSelectionChange", selectionChange);
	      clearActiveLines(cm);
	      delete cm.state.activeLines;
	    }
	    if (val) {
	      cm.state.activeLines = [];
	      updateActiveLines(cm, cm.listSelections());
	      cm.on("beforeSelectionChange", selectionChange);
	    }
	  });

	  function clearActiveLines(cm) {
	    for (var i = 0; i < cm.state.activeLines.length; i++) {
	      cm.removeLineClass(cm.state.activeLines[i], "wrap", WRAP_CLASS);
	      cm.removeLineClass(cm.state.activeLines[i], "background", BACK_CLASS);
	      cm.removeLineClass(cm.state.activeLines[i], "gutter", GUTT_CLASS);
	    }
	  }

	  function sameArray(a, b) {
	    if (a.length != b.length) return false;
	    for (var i = 0; i < a.length; i++)
	      if (a[i] != b[i]) return false;
	    return true;
	  }

	  function updateActiveLines(cm, ranges) {
	    var active = [];
	    for (var i = 0; i < ranges.length; i++) {
	      var range = ranges[i];
	      var option = cm.getOption("styleActiveLine");
	      if (typeof option == "object" && option.nonEmpty ? range.anchor.line != range.head.line : !range.empty())
	        continue
	      var line = cm.getLineHandleVisualStart(range.head.line);
	      if (active[active.length - 1] != line) active.push(line);
	    }
	    if (sameArray(cm.state.activeLines, active)) return;
	    cm.operation(function() {
	      clearActiveLines(cm);
	      for (var i = 0; i < active.length; i++) {
	        cm.addLineClass(active[i], "wrap", WRAP_CLASS);
	        cm.addLineClass(active[i], "background", BACK_CLASS);
	        cm.addLineClass(active[i], "gutter", GUTT_CLASS);
	      }
	      cm.state.activeLines = active;
	    });
	  }

	  function selectionChange(cm, sel) {
	    updateActiveLines(cm, sel.ranges);
	  }
	});


/***/ }),
/* 18 */
/***/ (function(module, exports, __webpack_require__) {

	// CodeMirror, copyright (c) by Marijn Haverbeke and others
	// Distributed under an MIT license: http://codemirror.net/LICENSE

	(function(mod) {
	  if (true) // CommonJS
	    mod(__webpack_require__(2));
	  else if (typeof define == "function" && define.amd) // AMD
	    define(["../../lib/codemirror"], mod);
	  else // Plain browser env
	    mod(CodeMirror);
	})(function(CodeMirror) {
	  CodeMirror.defineOption("showTrailingSpace", false, function(cm, val, prev) {
	    if (prev == CodeMirror.Init) prev = false;
	    if (prev && !val)
	      cm.removeOverlay("trailingspace");
	    else if (!prev && val)
	      cm.addOverlay({
	        token: function(stream) {
	          for (var l = stream.string.length, i = l; i && /\s/.test(stream.string.charAt(i - 1)); --i) {}
	          if (i > stream.pos) { stream.pos = i; return null; }
	          stream.pos = l;
	          return "trailingspace";
	        },
	        name: "trailingspace"
	      });
	  });
	});


/***/ }),
/* 19 */
/***/ (function(module, exports, __webpack_require__) {

	// CodeMirror, copyright (c) by Marijn Haverbeke and others
	// Distributed under an MIT license: http://codemirror.net/LICENSE

	(function(mod) {
	  if (true) // CommonJS
	    mod(__webpack_require__(2));
	  else if (typeof define == "function" && define.amd) // AMD
	    define(["../lib/codemirror"], mod);
	  else // Plain browser env
	    mod(CodeMirror);
	})(function(CodeMirror) {
	  "use strict";

	  var Pos = CodeMirror.Pos;
	  function posEq(a, b) { return a.line == b.line && a.ch == b.ch; }

	  // Kill 'ring'

	  var killRing = [];
	  function addToRing(str) {
	    killRing.push(str);
	    if (killRing.length > 50) killRing.shift();
	  }
	  function growRingTop(str) {
	    if (!killRing.length) return addToRing(str);
	    killRing[killRing.length - 1] += str;
	  }
	  function getFromRing(n) { return killRing[killRing.length - (n ? Math.min(n, 1) : 1)] || ""; }
	  function popFromRing() { if (killRing.length > 1) killRing.pop(); return getFromRing(); }

	  var lastKill = null;

	  function kill(cm, from, to, ring, text) {
	    if (text == null) text = cm.getRange(from, to);

	    if (ring == "grow" && lastKill && lastKill.cm == cm && posEq(from, lastKill.pos) && cm.isClean(lastKill.gen))
	      growRingTop(text);
	    else if (ring !== false)
	      addToRing(text);
	    cm.replaceRange("", from, to, "+delete");

	    if (ring == "grow") lastKill = {cm: cm, pos: from, gen: cm.changeGeneration()};
	    else lastKill = null;
	  }

	  // Boundaries of various units

	  function byChar(cm, pos, dir) {
	    return cm.findPosH(pos, dir, "char", true);
	  }

	  function byWord(cm, pos, dir) {
	    return cm.findPosH(pos, dir, "word", true);
	  }

	  function byLine(cm, pos, dir) {
	    return cm.findPosV(pos, dir, "line", cm.doc.sel.goalColumn);
	  }

	  function byPage(cm, pos, dir) {
	    return cm.findPosV(pos, dir, "page", cm.doc.sel.goalColumn);
	  }

	  function byParagraph(cm, pos, dir) {
	    var no = pos.line, line = cm.getLine(no);
	    var sawText = /\S/.test(dir < 0 ? line.slice(0, pos.ch) : line.slice(pos.ch));
	    var fst = cm.firstLine(), lst = cm.lastLine();
	    for (;;) {
	      no += dir;
	      if (no < fst || no > lst)
	        return cm.clipPos(Pos(no - dir, dir < 0 ? 0 : null));
	      line = cm.getLine(no);
	      var hasText = /\S/.test(line);
	      if (hasText) sawText = true;
	      else if (sawText) return Pos(no, 0);
	    }
	  }

	  function bySentence(cm, pos, dir) {
	    var line = pos.line, ch = pos.ch;
	    var text = cm.getLine(pos.line), sawWord = false;
	    for (;;) {
	      var next = text.charAt(ch + (dir < 0 ? -1 : 0));
	      if (!next) { // End/beginning of line reached
	        if (line == (dir < 0 ? cm.firstLine() : cm.lastLine())) return Pos(line, ch);
	        text = cm.getLine(line + dir);
	        if (!/\S/.test(text)) return Pos(line, ch);
	        line += dir;
	        ch = dir < 0 ? text.length : 0;
	        continue;
	      }
	      if (sawWord && /[!?.]/.test(next)) return Pos(line, ch + (dir > 0 ? 1 : 0));
	      if (!sawWord) sawWord = /\w/.test(next);
	      ch += dir;
	    }
	  }

	  function byExpr(cm, pos, dir) {
	    var wrap;
	    if (cm.findMatchingBracket && (wrap = cm.findMatchingBracket(pos, {strict: true}))
	        && wrap.match && (wrap.forward ? 1 : -1) == dir)
	      return dir > 0 ? Pos(wrap.to.line, wrap.to.ch + 1) : wrap.to;

	    for (var first = true;; first = false) {
	      var token = cm.getTokenAt(pos);
	      var after = Pos(pos.line, dir < 0 ? token.start : token.end);
	      if (first && dir > 0 && token.end == pos.ch || !/\w/.test(token.string)) {
	        var newPos = cm.findPosH(after, dir, "char");
	        if (posEq(after, newPos)) return pos;
	        else pos = newPos;
	      } else {
	        return after;
	      }
	    }
	  }

	  // Prefixes (only crudely supported)

	  function getPrefix(cm, precise) {
	    var digits = cm.state.emacsPrefix;
	    if (!digits) return precise ? null : 1;
	    clearPrefix(cm);
	    return digits == "-" ? -1 : Number(digits);
	  }

	  function repeated(cmd) {
	    var f = typeof cmd == "string" ? function(cm) { cm.execCommand(cmd); } : cmd;
	    return function(cm) {
	      var prefix = getPrefix(cm);
	      f(cm);
	      for (var i = 1; i < prefix; ++i) f(cm);
	    };
	  }

	  function findEnd(cm, pos, by, dir) {
	    var prefix = getPrefix(cm);
	    if (prefix < 0) { dir = -dir; prefix = -prefix; }
	    for (var i = 0; i < prefix; ++i) {
	      var newPos = by(cm, pos, dir);
	      if (posEq(newPos, pos)) break;
	      pos = newPos;
	    }
	    return pos;
	  }

	  function move(by, dir) {
	    var f = function(cm) {
	      cm.extendSelection(findEnd(cm, cm.getCursor(), by, dir));
	    };
	    f.motion = true;
	    return f;
	  }

	  function killTo(cm, by, dir, ring) {
	    var selections = cm.listSelections(), cursor;
	    var i = selections.length;
	    while (i--) {
	      cursor = selections[i].head;
	      kill(cm, cursor, findEnd(cm, cursor, by, dir), ring);
	    }
	  }

	  function killRegion(cm, ring) {
	    if (cm.somethingSelected()) {
	      var selections = cm.listSelections(), selection;
	      var i = selections.length;
	      while (i--) {
	        selection = selections[i];
	        kill(cm, selection.anchor, selection.head, ring);
	      }
	      return true;
	    }
	  }

	  function addPrefix(cm, digit) {
	    if (cm.state.emacsPrefix) {
	      if (digit != "-") cm.state.emacsPrefix += digit;
	      return;
	    }
	    // Not active yet
	    cm.state.emacsPrefix = digit;
	    cm.on("keyHandled", maybeClearPrefix);
	    cm.on("inputRead", maybeDuplicateInput);
	  }

	  var prefixPreservingKeys = {"Alt-G": true, "Ctrl-X": true, "Ctrl-Q": true, "Ctrl-U": true};

	  function maybeClearPrefix(cm, arg) {
	    if (!cm.state.emacsPrefixMap && !prefixPreservingKeys.hasOwnProperty(arg))
	      clearPrefix(cm);
	  }

	  function clearPrefix(cm) {
	    cm.state.emacsPrefix = null;
	    cm.off("keyHandled", maybeClearPrefix);
	    cm.off("inputRead", maybeDuplicateInput);
	  }

	  function maybeDuplicateInput(cm, event) {
	    var dup = getPrefix(cm);
	    if (dup > 1 && event.origin == "+input") {
	      var one = event.text.join("\n"), txt = "";
	      for (var i = 1; i < dup; ++i) txt += one;
	      cm.replaceSelection(txt);
	    }
	  }

	  function addPrefixMap(cm) {
	    cm.state.emacsPrefixMap = true;
	    cm.addKeyMap(prefixMap);
	    cm.on("keyHandled", maybeRemovePrefixMap);
	    cm.on("inputRead", maybeRemovePrefixMap);
	  }

	  function maybeRemovePrefixMap(cm, arg) {
	    if (typeof arg == "string" && (/^\d$/.test(arg) || arg == "Ctrl-U")) return;
	    cm.removeKeyMap(prefixMap);
	    cm.state.emacsPrefixMap = false;
	    cm.off("keyHandled", maybeRemovePrefixMap);
	    cm.off("inputRead", maybeRemovePrefixMap);
	  }

	  // Utilities

	  function setMark(cm) {
	    cm.setCursor(cm.getCursor());
	    cm.setExtending(!cm.getExtending());
	    cm.on("change", function() { cm.setExtending(false); });
	  }

	  function clearMark(cm) {
	    cm.setExtending(false);
	    cm.setCursor(cm.getCursor());
	  }

	  function getInput(cm, msg, f) {
	    if (cm.openDialog)
	      cm.openDialog(msg + ": <input type=\"text\" style=\"width: 10em\"/>", f, {bottom: true});
	    else
	      f(prompt(msg, ""));
	  }

	  function operateOnWord(cm, op) {
	    var start = cm.getCursor(), end = cm.findPosH(start, 1, "word");
	    cm.replaceRange(op(cm.getRange(start, end)), start, end);
	    cm.setCursor(end);
	  }

	  function toEnclosingExpr(cm) {
	    var pos = cm.getCursor(), line = pos.line, ch = pos.ch;
	    var stack = [];
	    while (line >= cm.firstLine()) {
	      var text = cm.getLine(line);
	      for (var i = ch == null ? text.length : ch; i > 0;) {
	        var ch = text.charAt(--i);
	        if (ch == ")")
	          stack.push("(");
	        else if (ch == "]")
	          stack.push("[");
	        else if (ch == "}")
	          stack.push("{");
	        else if (/[\(\{\[]/.test(ch) && (!stack.length || stack.pop() != ch))
	          return cm.extendSelection(Pos(line, i));
	      }
	      --line; ch = null;
	    }
	  }

	  function quit(cm) {
	    cm.execCommand("clearSearch");
	    clearMark(cm);
	  }

	  CodeMirror.emacs = {kill: kill, killRegion: killRegion, repeated: repeated};

	  // Actual keymap

	  var keyMap = CodeMirror.keyMap.emacs = CodeMirror.normalizeKeyMap({
	    "Ctrl-W": function(cm) {kill(cm, cm.getCursor("start"), cm.getCursor("end"), true);},
	    "Ctrl-K": repeated(function(cm) {
	      var start = cm.getCursor(), end = cm.clipPos(Pos(start.line));
	      var text = cm.getRange(start, end);
	      if (!/\S/.test(text)) {
	        text += "\n";
	        end = Pos(start.line + 1, 0);
	      }
	      kill(cm, start, end, "grow", text);
	    }),
	    "Alt-W": function(cm) {
	      addToRing(cm.getSelection());
	      clearMark(cm);
	    },
	    "Ctrl-Y": function(cm) {
	      var start = cm.getCursor();
	      cm.replaceRange(getFromRing(getPrefix(cm)), start, start, "paste");
	      cm.setSelection(start, cm.getCursor());
	    },
	    "Alt-Y": function(cm) {cm.replaceSelection(popFromRing(), "around", "paste");},

	    "Ctrl-Space": setMark, "Ctrl-Shift-2": setMark,

	    "Ctrl-F": move(byChar, 1), "Ctrl-B": move(byChar, -1),
	    "Right": move(byChar, 1), "Left": move(byChar, -1),
	    "Ctrl-D": function(cm) { killTo(cm, byChar, 1, false); },
	    "Delete": function(cm) { killRegion(cm, false) || killTo(cm, byChar, 1, false); },
	    "Ctrl-H": function(cm) { killTo(cm, byChar, -1, false); },
	    "Backspace": function(cm) { killRegion(cm, false) || killTo(cm, byChar, -1, false); },

	    "Alt-F": move(byWord, 1), "Alt-B": move(byWord, -1),
	    "Alt-D": function(cm) { killTo(cm, byWord, 1, "grow"); },
	    "Alt-Backspace": function(cm) { killTo(cm, byWord, -1, "grow"); },

	    "Ctrl-N": move(byLine, 1), "Ctrl-P": move(byLine, -1),
	    "Down": move(byLine, 1), "Up": move(byLine, -1),
	    "Ctrl-A": "goLineStart", "Ctrl-E": "goLineEnd",
	    "End": "goLineEnd", "Home": "goLineStart",

	    "Alt-V": move(byPage, -1), "Ctrl-V": move(byPage, 1),
	    "PageUp": move(byPage, -1), "PageDown": move(byPage, 1),

	    "Ctrl-Up": move(byParagraph, -1), "Ctrl-Down": move(byParagraph, 1),

	    "Alt-A": move(bySentence, -1), "Alt-E": move(bySentence, 1),
	    "Alt-K": function(cm) { killTo(cm, bySentence, 1, "grow"); },

	    "Ctrl-Alt-K": function(cm) { killTo(cm, byExpr, 1, "grow"); },
	    "Ctrl-Alt-Backspace": function(cm) { killTo(cm, byExpr, -1, "grow"); },
	    "Ctrl-Alt-F": move(byExpr, 1), "Ctrl-Alt-B": move(byExpr, -1, "grow"),

	    "Shift-Ctrl-Alt-2": function(cm) {
	      var cursor = cm.getCursor();
	      cm.setSelection(findEnd(cm, cursor, byExpr, 1), cursor);
	    },
	    "Ctrl-Alt-T": function(cm) {
	      var leftStart = byExpr(cm, cm.getCursor(), -1), leftEnd = byExpr(cm, leftStart, 1);
	      var rightEnd = byExpr(cm, leftEnd, 1), rightStart = byExpr(cm, rightEnd, -1);
	      cm.replaceRange(cm.getRange(rightStart, rightEnd) + cm.getRange(leftEnd, rightStart) +
	                      cm.getRange(leftStart, leftEnd), leftStart, rightEnd);
	    },
	    "Ctrl-Alt-U": repeated(toEnclosingExpr),

	    "Alt-Space": function(cm) {
	      var pos = cm.getCursor(), from = pos.ch, to = pos.ch, text = cm.getLine(pos.line);
	      while (from && /\s/.test(text.charAt(from - 1))) --from;
	      while (to < text.length && /\s/.test(text.charAt(to))) ++to;
	      cm.replaceRange(" ", Pos(pos.line, from), Pos(pos.line, to));
	    },
	    "Ctrl-O": repeated(function(cm) { cm.replaceSelection("\n", "start"); }),
	    "Ctrl-T": repeated(function(cm) {
	      cm.execCommand("transposeChars");
	    }),

	    "Alt-C": repeated(function(cm) {
	      operateOnWord(cm, function(w) {
	        var letter = w.search(/\w/);
	        if (letter == -1) return w;
	        return w.slice(0, letter) + w.charAt(letter).toUpperCase() + w.slice(letter + 1).toLowerCase();
	      });
	    }),
	    "Alt-U": repeated(function(cm) {
	      operateOnWord(cm, function(w) { return w.toUpperCase(); });
	    }),
	    "Alt-L": repeated(function(cm) {
	      operateOnWord(cm, function(w) { return w.toLowerCase(); });
	    }),

	    "Alt-;": "toggleComment",

	    "Ctrl-/": repeated("undo"), "Shift-Ctrl--": repeated("undo"),
	    "Ctrl-Z": repeated("undo"), "Cmd-Z": repeated("undo"),
	    "Shift-Alt-,": "goDocStart", "Shift-Alt-.": "goDocEnd",
	    "Ctrl-S": "findPersistentNext", "Ctrl-R": "findPersistentPrev", "Ctrl-G": quit, "Shift-Alt-5": "replace",
	    "Alt-/": "autocomplete",
	    "Enter": "newlineAndIndent",
	    "Ctrl-J": repeated(function(cm) { cm.replaceSelection("\n", "end"); }),
	    "Tab": "indentAuto",

	    "Alt-G G": function(cm) {
	      var prefix = getPrefix(cm, true);
	      if (prefix != null && prefix > 0) return cm.setCursor(prefix - 1);

	      getInput(cm, "Goto line", function(str) {
	        var num;
	        if (str && !isNaN(num = Number(str)) && num == (num|0) && num > 0)
	          cm.setCursor(num - 1);
	      });
	    },

	    "Ctrl-X Tab": function(cm) {
	      cm.indentSelection(getPrefix(cm, true) || cm.getOption("indentUnit"));
	    },
	    "Ctrl-X Ctrl-X": function(cm) {
	      cm.setSelection(cm.getCursor("head"), cm.getCursor("anchor"));
	    },
	    "Ctrl-X Ctrl-S": "save",
	    "Ctrl-X Ctrl-W": "save",
	    "Ctrl-X S": "saveAll",
	    "Ctrl-X F": "open",
	    "Ctrl-X U": repeated("undo"),
	    "Ctrl-X K": "close",
	    "Ctrl-X Delete": function(cm) { kill(cm, cm.getCursor(), bySentence(cm, cm.getCursor(), 1), "grow"); },
	    "Ctrl-X H": "selectAll",

	    "Ctrl-Q Tab": repeated("insertTab"),
	    "Ctrl-U": addPrefixMap
	  });

	  var prefixMap = {"Ctrl-G": clearPrefix};
	  function regPrefix(d) {
	    prefixMap[d] = function(cm) { addPrefix(cm, d); };
	    keyMap["Ctrl-" + d] = function(cm) { addPrefix(cm, d); };
	    prefixPreservingKeys["Ctrl-" + d] = true;
	  }
	  for (var i = 0; i < 10; ++i) regPrefix(String(i));
	  regPrefix("-");
	});


/***/ }),
/* 20 */
/***/ (function(module, exports, __webpack_require__) {

	// CodeMirror, copyright (c) by Marijn Haverbeke and others
	// Distributed under an MIT license: http://codemirror.net/LICENSE

	/**
	 * Supported keybindings:
	 *   Too many to list. Refer to defaultKeyMap below.
	 *
	 * Supported Ex commands:
	 *   Refer to defaultExCommandMap below.
	 *
	 * Registers: unnamed, -, a-z, A-Z, 0-9
	 *   (Does not respect the special case for number registers when delete
	 *    operator is made with these commands: %, (, ),  , /, ?, n, N, {, } )
	 *   TODO: Implement the remaining registers.
	 *
	 * Marks: a-z, A-Z, and 0-9
	 *   TODO: Implement the remaining special marks. They have more complex
	 *       behavior.
	 *
	 * Events:
	 *  'vim-mode-change' - raised on the editor anytime the current mode changes,
	 *                      Event object: {mode: "visual", subMode: "linewise"}
	 *
	 * Code structure:
	 *  1. Default keymap
	 *  2. Variable declarations and short basic helpers
	 *  3. Instance (External API) implementation
	 *  4. Internal state tracking objects (input state, counter) implementation
	 *     and instantiation
	 *  5. Key handler (the main command dispatcher) implementation
	 *  6. Motion, operator, and action implementations
	 *  7. Helper functions for the key handler, motions, operators, and actions
	 *  8. Set up Vim to work as a keymap for CodeMirror.
	 *  9. Ex command implementations.
	 */

	(function(mod) {
	  if (true) // CommonJS
	    mod(__webpack_require__(2), __webpack_require__(3), __webpack_require__(1), __webpack_require__(5));
	  else if (typeof define == "function" && define.amd) // AMD
	    define(["../lib/codemirror", "../addon/search/searchcursor", "../addon/dialog/dialog", "../addon/edit/matchbrackets"], mod);
	  else // Plain browser env
	    mod(CodeMirror);
	})(function(CodeMirror) {
	  'use strict';

	  var defaultKeymap = [
	    // Key to key mapping. This goes first to make it possible to override
	    // existing mappings.
	    { keys: '<Left>', type: 'keyToKey', toKeys: 'h' },
	    { keys: '<Right>', type: 'keyToKey', toKeys: 'l' },
	    { keys: '<Up>', type: 'keyToKey', toKeys: 'k' },
	    { keys: '<Down>', type: 'keyToKey', toKeys: 'j' },
	    { keys: '<Space>', type: 'keyToKey', toKeys: 'l' },
	    { keys: '<BS>', type: 'keyToKey', toKeys: 'h', context: 'normal'},
	    { keys: '<C-Space>', type: 'keyToKey', toKeys: 'W' },
	    { keys: '<C-BS>', type: 'keyToKey', toKeys: 'B', context: 'normal' },
	    { keys: '<S-Space>', type: 'keyToKey', toKeys: 'w' },
	    { keys: '<S-BS>', type: 'keyToKey', toKeys: 'b', context: 'normal' },
	    { keys: '<C-n>', type: 'keyToKey', toKeys: 'j' },
	    { keys: '<C-p>', type: 'keyToKey', toKeys: 'k' },
	    { keys: '<C-[>', type: 'keyToKey', toKeys: '<Esc>' },
	    { keys: '<C-c>', type: 'keyToKey', toKeys: '<Esc>' },
	    { keys: '<C-[>', type: 'keyToKey', toKeys: '<Esc>', context: 'insert' },
	    { keys: '<C-c>', type: 'keyToKey', toKeys: '<Esc>', context: 'insert' },
	    { keys: 's', type: 'keyToKey', toKeys: 'cl', context: 'normal' },
	    { keys: 's', type: 'keyToKey', toKeys: 'c', context: 'visual'},
	    { keys: 'S', type: 'keyToKey', toKeys: 'cc', context: 'normal' },
	    { keys: 'S', type: 'keyToKey', toKeys: 'VdO', context: 'visual' },
	    { keys: '<Home>', type: 'keyToKey', toKeys: '0' },
	    { keys: '<End>', type: 'keyToKey', toKeys: '$' },
	    { keys: '<PageUp>', type: 'keyToKey', toKeys: '<C-b>' },
	    { keys: '<PageDown>', type: 'keyToKey', toKeys: '<C-f>' },
	    { keys: '<CR>', type: 'keyToKey', toKeys: 'j^', context: 'normal' },
	    { keys: '<Ins>', type: 'action', action: 'toggleOverwrite', context: 'insert' },
	    // Motions
	    { keys: 'H', type: 'motion', motion: 'moveToTopLine', motionArgs: { linewise: true, toJumplist: true }},
	    { keys: 'M', type: 'motion', motion: 'moveToMiddleLine', motionArgs: { linewise: true, toJumplist: true }},
	    { keys: 'L', type: 'motion', motion: 'moveToBottomLine', motionArgs: { linewise: true, toJumplist: true }},
	    { keys: 'h', type: 'motion', motion: 'moveByCharacters', motionArgs: { forward: false }},
	    { keys: 'l', type: 'motion', motion: 'moveByCharacters', motionArgs: { forward: true }},
	    { keys: 'j', type: 'motion', motion: 'moveByLines', motionArgs: { forward: true, linewise: true }},
	    { keys: 'k', type: 'motion', motion: 'moveByLines', motionArgs: { forward: false, linewise: true }},
	    { keys: 'gj', type: 'motion', motion: 'moveByDisplayLines', motionArgs: { forward: true }},
	    { keys: 'gk', type: 'motion', motion: 'moveByDisplayLines', motionArgs: { forward: false }},
	    { keys: 'w', type: 'motion', motion: 'moveByWords', motionArgs: { forward: true, wordEnd: false }},
	    { keys: 'W', type: 'motion', motion: 'moveByWords', motionArgs: { forward: true, wordEnd: false, bigWord: true }},
	    { keys: 'e', type: 'motion', motion: 'moveByWords', motionArgs: { forward: true, wordEnd: true, inclusive: true }},
	    { keys: 'E', type: 'motion', motion: 'moveByWords', motionArgs: { forward: true, wordEnd: true, bigWord: true, inclusive: true }},
	    { keys: 'b', type: 'motion', motion: 'moveByWords', motionArgs: { forward: false, wordEnd: false }},
	    { keys: 'B', type: 'motion', motion: 'moveByWords', motionArgs: { forward: false, wordEnd: false, bigWord: true }},
	    { keys: 'ge', type: 'motion', motion: 'moveByWords', motionArgs: { forward: false, wordEnd: true, inclusive: true }},
	    { keys: 'gE', type: 'motion', motion: 'moveByWords', motionArgs: { forward: false, wordEnd: true, bigWord: true, inclusive: true }},
	    { keys: '{', type: 'motion', motion: 'moveByParagraph', motionArgs: { forward: false, toJumplist: true }},
	    { keys: '}', type: 'motion', motion: 'moveByParagraph', motionArgs: { forward: true, toJumplist: true }},
	    { keys: '<C-f>', type: 'motion', motion: 'moveByPage', motionArgs: { forward: true }},
	    { keys: '<C-b>', type: 'motion', motion: 'moveByPage', motionArgs: { forward: false }},
	    { keys: '<C-d>', type: 'motion', motion: 'moveByScroll', motionArgs: { forward: true, explicitRepeat: true }},
	    { keys: '<C-u>', type: 'motion', motion: 'moveByScroll', motionArgs: { forward: false, explicitRepeat: true }},
	    { keys: 'gg', type: 'motion', motion: 'moveToLineOrEdgeOfDocument', motionArgs: { forward: false, explicitRepeat: true, linewise: true, toJumplist: true }},
	    { keys: 'G', type: 'motion', motion: 'moveToLineOrEdgeOfDocument', motionArgs: { forward: true, explicitRepeat: true, linewise: true, toJumplist: true }},
	    { keys: '0', type: 'motion', motion: 'moveToStartOfLine' },
	    { keys: '^', type: 'motion', motion: 'moveToFirstNonWhiteSpaceCharacter' },
	    { keys: '+', type: 'motion', motion: 'moveByLines', motionArgs: { forward: true, toFirstChar:true }},
	    { keys: '-', type: 'motion', motion: 'moveByLines', motionArgs: { forward: false, toFirstChar:true }},
	    { keys: '_', type: 'motion', motion: 'moveByLines', motionArgs: { forward: true, toFirstChar:true, repeatOffset:-1 }},
	    { keys: '$', type: 'motion', motion: 'moveToEol', motionArgs: { inclusive: true }},
	    { keys: '%', type: 'motion', motion: 'moveToMatchedSymbol', motionArgs: { inclusive: true, toJumplist: true }},
	    { keys: 'f<character>', type: 'motion', motion: 'moveToCharacter', motionArgs: { forward: true , inclusive: true }},
	    { keys: 'F<character>', type: 'motion', motion: 'moveToCharacter', motionArgs: { forward: false }},
	    { keys: 't<character>', type: 'motion', motion: 'moveTillCharacter', motionArgs: { forward: true, inclusive: true }},
	    { keys: 'T<character>', type: 'motion', motion: 'moveTillCharacter', motionArgs: { forward: false }},
	    { keys: ';', type: 'motion', motion: 'repeatLastCharacterSearch', motionArgs: { forward: true }},
	    { keys: ',', type: 'motion', motion: 'repeatLastCharacterSearch', motionArgs: { forward: false }},
	    { keys: '\'<character>', type: 'motion', motion: 'goToMark', motionArgs: {toJumplist: true, linewise: true}},
	    { keys: '`<character>', type: 'motion', motion: 'goToMark', motionArgs: {toJumplist: true}},
	    { keys: ']`', type: 'motion', motion: 'jumpToMark', motionArgs: { forward: true } },
	    { keys: '[`', type: 'motion', motion: 'jumpToMark', motionArgs: { forward: false } },
	    { keys: ']\'', type: 'motion', motion: 'jumpToMark', motionArgs: { forward: true, linewise: true } },
	    { keys: '[\'', type: 'motion', motion: 'jumpToMark', motionArgs: { forward: false, linewise: true } },
	    // the next two aren't motions but must come before more general motion declarations
	    { keys: ']p', type: 'action', action: 'paste', isEdit: true, actionArgs: { after: true, isEdit: true, matchIndent: true}},
	    { keys: '[p', type: 'action', action: 'paste', isEdit: true, actionArgs: { after: false, isEdit: true, matchIndent: true}},
	    { keys: ']<character>', type: 'motion', motion: 'moveToSymbol', motionArgs: { forward: true, toJumplist: true}},
	    { keys: '[<character>', type: 'motion', motion: 'moveToSymbol', motionArgs: { forward: false, toJumplist: true}},
	    { keys: '|', type: 'motion', motion: 'moveToColumn'},
	    { keys: 'o', type: 'motion', motion: 'moveToOtherHighlightedEnd', context:'visual'},
	    { keys: 'O', type: 'motion', motion: 'moveToOtherHighlightedEnd', motionArgs: {sameLine: true}, context:'visual'},
	    // Operators
	    { keys: 'd', type: 'operator', operator: 'delete' },
	    { keys: 'y', type: 'operator', operator: 'yank' },
	    { keys: 'c', type: 'operator', operator: 'change' },
	    { keys: '>', type: 'operator', operator: 'indent', operatorArgs: { indentRight: true }},
	    { keys: '<', type: 'operator', operator: 'indent', operatorArgs: { indentRight: false }},
	    { keys: 'g~', type: 'operator', operator: 'changeCase' },
	    { keys: 'gu', type: 'operator', operator: 'changeCase', operatorArgs: {toLower: true}, isEdit: true },
	    { keys: 'gU', type: 'operator', operator: 'changeCase', operatorArgs: {toLower: false}, isEdit: true },
	    { keys: 'n', type: 'motion', motion: 'findNext', motionArgs: { forward: true, toJumplist: true }},
	    { keys: 'N', type: 'motion', motion: 'findNext', motionArgs: { forward: false, toJumplist: true }},
	    // Operator-Motion dual commands
	    { keys: 'x', type: 'operatorMotion', operator: 'delete', motion: 'moveByCharacters', motionArgs: { forward: true }, operatorMotionArgs: { visualLine: false }},
	    { keys: 'X', type: 'operatorMotion', operator: 'delete', motion: 'moveByCharacters', motionArgs: { forward: false }, operatorMotionArgs: { visualLine: true }},
	    { keys: 'D', type: 'operatorMotion', operator: 'delete', motion: 'moveToEol', motionArgs: { inclusive: true }, context: 'normal'},
	    { keys: 'D', type: 'operator', operator: 'delete', operatorArgs: { linewise: true }, context: 'visual'},
	    { keys: 'Y', type: 'operatorMotion', operator: 'yank', motion: 'expandToLine', motionArgs: { linewise: true }, context: 'normal'},
	    { keys: 'Y', type: 'operator', operator: 'yank', operatorArgs: { linewise: true }, context: 'visual'},
	    { keys: 'C', type: 'operatorMotion', operator: 'change', motion: 'moveToEol', motionArgs: { inclusive: true }, context: 'normal'},
	    { keys: 'C', type: 'operator', operator: 'change', operatorArgs: { linewise: true }, context: 'visual'},
	    { keys: '~', type: 'operatorMotion', operator: 'changeCase', motion: 'moveByCharacters', motionArgs: { forward: true }, operatorArgs: { shouldMoveCursor: true }, context: 'normal'},
	    { keys: '~', type: 'operator', operator: 'changeCase', context: 'visual'},
	    { keys: '<C-w>', type: 'operatorMotion', operator: 'delete', motion: 'moveByWords', motionArgs: { forward: false, wordEnd: false }, context: 'insert' },
	    // Actions
	    { keys: '<C-i>', type: 'action', action: 'jumpListWalk', actionArgs: { forward: true }},
	    { keys: '<C-o>', type: 'action', action: 'jumpListWalk', actionArgs: { forward: false }},
	    { keys: '<C-e>', type: 'action', action: 'scroll', actionArgs: { forward: true, linewise: true }},
	    { keys: '<C-y>', type: 'action', action: 'scroll', actionArgs: { forward: false, linewise: true }},
	    { keys: 'a', type: 'action', action: 'enterInsertMode', isEdit: true, actionArgs: { insertAt: 'charAfter' }, context: 'normal' },
	    { keys: 'A', type: 'action', action: 'enterInsertMode', isEdit: true, actionArgs: { insertAt: 'eol' }, context: 'normal' },
	    { keys: 'A', type: 'action', action: 'enterInsertMode', isEdit: true, actionArgs: { insertAt: 'endOfSelectedArea' }, context: 'visual' },
	    { keys: 'i', type: 'action', action: 'enterInsertMode', isEdit: true, actionArgs: { insertAt: 'inplace' }, context: 'normal' },
	    { keys: 'I', type: 'action', action: 'enterInsertMode', isEdit: true, actionArgs: { insertAt: 'firstNonBlank'}, context: 'normal' },
	    { keys: 'I', type: 'action', action: 'enterInsertMode', isEdit: true, actionArgs: { insertAt: 'startOfSelectedArea' }, context: 'visual' },
	    { keys: 'o', type: 'action', action: 'newLineAndEnterInsertMode', isEdit: true, interlaceInsertRepeat: true, actionArgs: { after: true }, context: 'normal' },
	    { keys: 'O', type: 'action', action: 'newLineAndEnterInsertMode', isEdit: true, interlaceInsertRepeat: true, actionArgs: { after: false }, context: 'normal' },
	    { keys: 'v', type: 'action', action: 'toggleVisualMode' },
	    { keys: 'V', type: 'action', action: 'toggleVisualMode', actionArgs: { linewise: true }},
	    { keys: '<C-v>', type: 'action', action: 'toggleVisualMode', actionArgs: { blockwise: true }},
	    { keys: '<C-q>', type: 'action', action: 'toggleVisualMode', actionArgs: { blockwise: true }},
	    { keys: 'gv', type: 'action', action: 'reselectLastSelection' },
	    { keys: 'J', type: 'action', action: 'joinLines', isEdit: true },
	    { keys: 'p', type: 'action', action: 'paste', isEdit: true, actionArgs: { after: true, isEdit: true }},
	    { keys: 'P', type: 'action', action: 'paste', isEdit: true, actionArgs: { after: false, isEdit: true }},
	    { keys: 'r<character>', type: 'action', action: 'replace', isEdit: true },
	    { keys: '@<character>', type: 'action', action: 'replayMacro' },
	    { keys: 'q<character>', type: 'action', action: 'enterMacroRecordMode' },
	    // Handle Replace-mode as a special case of insert mode.
	    { keys: 'R', type: 'action', action: 'enterInsertMode', isEdit: true, actionArgs: { replace: true }},
	    { keys: 'u', type: 'action', action: 'undo', context: 'normal' },
	    { keys: 'u', type: 'operator', operator: 'changeCase', operatorArgs: {toLower: true}, context: 'visual', isEdit: true },
	    { keys: 'U', type: 'operator', operator: 'changeCase', operatorArgs: {toLower: false}, context: 'visual', isEdit: true },
	    { keys: '<C-r>', type: 'action', action: 'redo' },
	    { keys: 'm<character>', type: 'action', action: 'setMark' },
	    { keys: '"<character>', type: 'action', action: 'setRegister' },
	    { keys: 'zz', type: 'action', action: 'scrollToCursor', actionArgs: { position: 'center' }},
	    { keys: 'z.', type: 'action', action: 'scrollToCursor', actionArgs: { position: 'center' }, motion: 'moveToFirstNonWhiteSpaceCharacter' },
	    { keys: 'zt', type: 'action', action: 'scrollToCursor', actionArgs: { position: 'top' }},
	    { keys: 'z<CR>', type: 'action', action: 'scrollToCursor', actionArgs: { position: 'top' }, motion: 'moveToFirstNonWhiteSpaceCharacter' },
	    { keys: 'z-', type: 'action', action: 'scrollToCursor', actionArgs: { position: 'bottom' }},
	    { keys: 'zb', type: 'action', action: 'scrollToCursor', actionArgs: { position: 'bottom' }, motion: 'moveToFirstNonWhiteSpaceCharacter' },
	    { keys: '.', type: 'action', action: 'repeatLastEdit' },
	    { keys: '<C-a>', type: 'action', action: 'incrementNumberToken', isEdit: true, actionArgs: {increase: true, backtrack: false}},
	    { keys: '<C-x>', type: 'action', action: 'incrementNumberToken', isEdit: true, actionArgs: {increase: false, backtrack: false}},
	    { keys: '<C-t>', type: 'action', action: 'indent', actionArgs: { indentRight: true }, context: 'insert' },
	    { keys: '<C-d>', type: 'action', action: 'indent', actionArgs: { indentRight: false }, context: 'insert' },
	    // Text object motions
	    { keys: 'a<character>', type: 'motion', motion: 'textObjectManipulation' },
	    { keys: 'i<character>', type: 'motion', motion: 'textObjectManipulation', motionArgs: { textObjectInner: true }},
	    // Search
	    { keys: '/', type: 'search', searchArgs: { forward: true, querySrc: 'prompt', toJumplist: true }},
	    { keys: '?', type: 'search', searchArgs: { forward: false, querySrc: 'prompt', toJumplist: true }},
	    { keys: '*', type: 'search', searchArgs: { forward: true, querySrc: 'wordUnderCursor', wholeWordOnly: true, toJumplist: true }},
	    { keys: '#', type: 'search', searchArgs: { forward: false, querySrc: 'wordUnderCursor', wholeWordOnly: true, toJumplist: true }},
	    { keys: 'g*', type: 'search', searchArgs: { forward: true, querySrc: 'wordUnderCursor', toJumplist: true }},
	    { keys: 'g#', type: 'search', searchArgs: { forward: false, querySrc: 'wordUnderCursor', toJumplist: true }},
	    // Ex command
	    { keys: ':', type: 'ex' }
	  ];

	  /**
	   * Ex commands
	   * Care must be taken when adding to the default Ex command map. For any
	   * pair of commands that have a shared prefix, at least one of their
	   * shortNames must not match the prefix of the other command.
	   */
	  var defaultExCommandMap = [
	    { name: 'colorscheme', shortName: 'colo' },
	    { name: 'map' },
	    { name: 'imap', shortName: 'im' },
	    { name: 'nmap', shortName: 'nm' },
	    { name: 'vmap', shortName: 'vm' },
	    { name: 'unmap' },
	    { name: 'write', shortName: 'w' },
	    { name: 'undo', shortName: 'u' },
	    { name: 'redo', shortName: 'red' },
	    { name: 'set', shortName: 'se' },
	    { name: 'set', shortName: 'se' },
	    { name: 'setlocal', shortName: 'setl' },
	    { name: 'setglobal', shortName: 'setg' },
	    { name: 'sort', shortName: 'sor' },
	    { name: 'substitute', shortName: 's', possiblyAsync: true },
	    { name: 'nohlsearch', shortName: 'noh' },
	    { name: 'yank', shortName: 'y' },
	    { name: 'delmarks', shortName: 'delm' },
	    { name: 'registers', shortName: 'reg', excludeFromCommandHistory: true },
	    { name: 'global', shortName: 'g' }
	  ];

	  var Pos = CodeMirror.Pos;

	  var Vim = function() {
	    function enterVimMode(cm) {
	      cm.setOption('disableInput', true);
	      cm.setOption('showCursorWhenSelecting', false);
	      CodeMirror.signal(cm, "vim-mode-change", {mode: "normal"});
	      cm.on('cursorActivity', onCursorActivity);
	      maybeInitVimState(cm);
	      CodeMirror.on(cm.getInputField(), 'paste', getOnPasteFn(cm));
	    }

	    function leaveVimMode(cm) {
	      cm.setOption('disableInput', false);
	      cm.off('cursorActivity', onCursorActivity);
	      CodeMirror.off(cm.getInputField(), 'paste', getOnPasteFn(cm));
	      cm.state.vim = null;
	    }

	    function detachVimMap(cm, next) {
	      if (this == CodeMirror.keyMap.vim) {
	        CodeMirror.rmClass(cm.getWrapperElement(), "cm-fat-cursor");
	        if (cm.getOption("inputStyle") == "contenteditable" && document.body.style.caretColor != null) {
	          disableFatCursorMark(cm);
	          cm.getInputField().style.caretColor = "";
	        }
	      }

	      if (!next || next.attach != attachVimMap)
	        leaveVimMode(cm);
	    }
	    function attachVimMap(cm, prev) {
	      if (this == CodeMirror.keyMap.vim) {
	        CodeMirror.addClass(cm.getWrapperElement(), "cm-fat-cursor");
	        if (cm.getOption("inputStyle") == "contenteditable" && document.body.style.caretColor != null) {
	          enableFatCursorMark(cm);
	          cm.getInputField().style.caretColor = "transparent";
	        }
	      }

	      if (!prev || prev.attach != attachVimMap)
	        enterVimMode(cm);
	    }

	    function fatCursorMarks(cm) {
	      var ranges = cm.listSelections(), result = []
	      for (var i = 0; i < ranges.length; i++) {
	        var range = ranges[i]
	        if (range.empty()) {
	          if (range.anchor.ch < cm.getLine(range.anchor.line).length) {
	            result.push(cm.markText(range.anchor, Pos(range.anchor.line, range.anchor.ch + 1),
	                                    {className: "cm-fat-cursor-mark"}))
	          } else {
	            var widget = document.createElement("span")
	            widget.textContent = "\u00a0"
	            widget.className = "cm-fat-cursor-mark"
	            result.push(cm.setBookmark(range.anchor, {widget: widget}))
	          }
	        }
	      }
	      return result
	    }

	    function updateFatCursorMark(cm) {
	      var marks = cm.state.fatCursorMarks
	      if (marks) for (var i = 0; i < marks.length; i++) marks[i].clear()
	      cm.state.fatCursorMarks = fatCursorMarks(cm)
	    }

	    function enableFatCursorMark(cm) {
	      cm.state.fatCursorMarks = fatCursorMarks(cm)
	      cm.on("cursorActivity", updateFatCursorMark)
	    }

	    function disableFatCursorMark(cm) {
	      var marks = cm.state.fatCursorMarks
	      if (marks) for (var i = 0; i < marks.length; i++) marks[i].clear()
	      cm.state.fatCursorMarks = null
	      cm.off("cursorActivity", updateFatCursorMark)
	    }

	    // Deprecated, simply setting the keymap works again.
	    CodeMirror.defineOption('vimMode', false, function(cm, val, prev) {
	      if (val && cm.getOption("keyMap") != "vim")
	        cm.setOption("keyMap", "vim");
	      else if (!val && prev != CodeMirror.Init && /^vim/.test(cm.getOption("keyMap")))
	        cm.setOption("keyMap", "default");
	    });

	    function cmKey(key, cm) {
	      if (!cm) { return undefined; }
	      if (this[key]) { return this[key]; }
	      var vimKey = cmKeyToVimKey(key);
	      if (!vimKey) {
	        return false;
	      }
	      var cmd = CodeMirror.Vim.findKey(cm, vimKey);
	      if (typeof cmd == 'function') {
	        CodeMirror.signal(cm, 'vim-keypress', vimKey);
	      }
	      return cmd;
	    }

	    var modifiers = {'Shift': 'S', 'Ctrl': 'C', 'Alt': 'A', 'Cmd': 'D', 'Mod': 'A'};
	    var specialKeys = {Enter:'CR',Backspace:'BS',Delete:'Del',Insert:'Ins'};
	    function cmKeyToVimKey(key) {
	      if (key.charAt(0) == '\'') {
	        // Keypress character binding of format "'a'"
	        return key.charAt(1);
	      }
	      var pieces = key.split(/-(?!$)/);
	      var lastPiece = pieces[pieces.length - 1];
	      if (pieces.length == 1 && pieces[0].length == 1) {
	        // No-modifier bindings use literal character bindings above. Skip.
	        return false;
	      } else if (pieces.length == 2 && pieces[0] == 'Shift' && lastPiece.length == 1) {
	        // Ignore Shift+char bindings as they should be handled by literal character.
	        return false;
	      }
	      var hasCharacter = false;
	      for (var i = 0; i < pieces.length; i++) {
	        var piece = pieces[i];
	        if (piece in modifiers) { pieces[i] = modifiers[piece]; }
	        else { hasCharacter = true; }
	        if (piece in specialKeys) { pieces[i] = specialKeys[piece]; }
	      }
	      if (!hasCharacter) {
	        // Vim does not support modifier only keys.
	        return false;
	      }
	      // TODO: Current bindings expect the character to be lower case, but
	      // it looks like vim key notation uses upper case.
	      if (isUpperCase(lastPiece)) {
	        pieces[pieces.length - 1] = lastPiece.toLowerCase();
	      }
	      return '<' + pieces.join('-') + '>';
	    }

	    function getOnPasteFn(cm) {
	      var vim = cm.state.vim;
	      if (!vim.onPasteFn) {
	        vim.onPasteFn = function() {
	          if (!vim.insertMode) {
	            cm.setCursor(offsetCursor(cm.getCursor(), 0, 1));
	            actions.enterInsertMode(cm, {}, vim);
	          }
	        };
	      }
	      return vim.onPasteFn;
	    }

	    var numberRegex = /[\d]/;
	    var wordCharTest = [CodeMirror.isWordChar, function(ch) {
	      return ch && !CodeMirror.isWordChar(ch) && !/\s/.test(ch);
	    }], bigWordCharTest = [function(ch) {
	      return /\S/.test(ch);
	    }];
	    function makeKeyRange(start, size) {
	      var keys = [];
	      for (var i = start; i < start + size; i++) {
	        keys.push(String.fromCharCode(i));
	      }
	      return keys;
	    }
	    var upperCaseAlphabet = makeKeyRange(65, 26);
	    var lowerCaseAlphabet = makeKeyRange(97, 26);
	    var numbers = makeKeyRange(48, 10);
	    var validMarks = [].concat(upperCaseAlphabet, lowerCaseAlphabet, numbers, ['<', '>']);
	    var validRegisters = [].concat(upperCaseAlphabet, lowerCaseAlphabet, numbers, ['-', '"', '.', ':', '/']);

	    function isLine(cm, line) {
	      return line >= cm.firstLine() && line <= cm.lastLine();
	    }
	    function isLowerCase(k) {
	      return (/^[a-z]$/).test(k);
	    }
	    function isMatchableSymbol(k) {
	      return '()[]{}'.indexOf(k) != -1;
	    }
	    function isNumber(k) {
	      return numberRegex.test(k);
	    }
	    function isUpperCase(k) {
	      return (/^[A-Z]$/).test(k);
	    }
	    function isWhiteSpaceString(k) {
	      return (/^\s*$/).test(k);
	    }
	    function inArray(val, arr) {
	      for (var i = 0; i < arr.length; i++) {
	        if (arr[i] == val) {
	          return true;
	        }
	      }
	      return false;
	    }

	    var options = {};
	    function defineOption(name, defaultValue, type, aliases, callback) {
	      if (defaultValue === undefined && !callback) {
	        throw Error('defaultValue is required unless callback is provided');
	      }
	      if (!type) { type = 'string'; }
	      options[name] = {
	        type: type,
	        defaultValue: defaultValue,
	        callback: callback
	      };
	      if (aliases) {
	        for (var i = 0; i < aliases.length; i++) {
	          options[aliases[i]] = options[name];
	        }
	      }
	      if (defaultValue) {
	        setOption(name, defaultValue);
	      }
	    }

	    function setOption(name, value, cm, cfg) {
	      var option = options[name];
	      cfg = cfg || {};
	      var scope = cfg.scope;
	      if (!option) {
	        return new Error('Unknown option: ' + name);
	      }
	      if (option.type == 'boolean') {
	        if (value && value !== true) {
	          return new Error('Invalid argument: ' + name + '=' + value);
	        } else if (value !== false) {
	          // Boolean options are set to true if value is not defined.
	          value = true;
	        }
	      }
	      if (option.callback) {
	        if (scope !== 'local') {
	          option.callback(value, undefined);
	        }
	        if (scope !== 'global' && cm) {
	          option.callback(value, cm);
	        }
	      } else {
	        if (scope !== 'local') {
	          option.value = option.type == 'boolean' ? !!value : value;
	        }
	        if (scope !== 'global' && cm) {
	          cm.state.vim.options[name] = {value: value};
	        }
	      }
	    }

	    function getOption(name, cm, cfg) {
	      var option = options[name];
	      cfg = cfg || {};
	      var scope = cfg.scope;
	      if (!option) {
	        return new Error('Unknown option: ' + name);
	      }
	      if (option.callback) {
	        var local = cm && option.callback(undefined, cm);
	        if (scope !== 'global' && local !== undefined) {
	          return local;
	        }
	        if (scope !== 'local') {
	          return option.callback();
	        }
	        return;
	      } else {
	        var local = (scope !== 'global') && (cm && cm.state.vim.options[name]);
	        return (local || (scope !== 'local') && option || {}).value;
	      }
	    }

	    defineOption('filetype', undefined, 'string', ['ft'], function(name, cm) {
	      // Option is local. Do nothing for global.
	      if (cm === undefined) {
	        return;
	      }
	      // The 'filetype' option proxies to the CodeMirror 'mode' option.
	      if (name === undefined) {
	        var mode = cm.getOption('mode');
	        return mode == 'null' ? '' : mode;
	      } else {
	        var mode = name == '' ? 'null' : name;
	        cm.setOption('mode', mode);
	      }
	    });

	    var createCircularJumpList = function() {
	      var size = 100;
	      var pointer = -1;
	      var head = 0;
	      var tail = 0;
	      var buffer = new Array(size);
	      function add(cm, oldCur, newCur) {
	        var current = pointer % size;
	        var curMark = buffer[current];
	        function useNextSlot(cursor) {
	          var next = ++pointer % size;
	          var trashMark = buffer[next];
	          if (trashMark) {
	            trashMark.clear();
	          }
	          buffer[next] = cm.setBookmark(cursor);
	        }
	        if (curMark) {
	          var markPos = curMark.find();
	          // avoid recording redundant cursor position
	          if (markPos && !cursorEqual(markPos, oldCur)) {
	            useNextSlot(oldCur);
	          }
	        } else {
	          useNextSlot(oldCur);
	        }
	        useNextSlot(newCur);
	        head = pointer;
	        tail = pointer - size + 1;
	        if (tail < 0) {
	          tail = 0;
	        }
	      }
	      function move(cm, offset) {
	        pointer += offset;
	        if (pointer > head) {
	          pointer = head;
	        } else if (pointer < tail) {
	          pointer = tail;
	        }
	        var mark = buffer[(size + pointer) % size];
	        // skip marks that are temporarily removed from text buffer
	        if (mark && !mark.find()) {
	          var inc = offset > 0 ? 1 : -1;
	          var newCur;
	          var oldCur = cm.getCursor();
	          do {
	            pointer += inc;
	            mark = buffer[(size + pointer) % size];
	            // skip marks that are the same as current position
	            if (mark &&
	                (newCur = mark.find()) &&
	                !cursorEqual(oldCur, newCur)) {
	              break;
	            }
	          } while (pointer < head && pointer > tail);
	        }
	        return mark;
	      }
	      return {
	        cachedCursor: undefined, //used for # and * jumps
	        add: add,
	        move: move
	      };
	    };

	    // Returns an object to track the changes associated insert mode.  It
	    // clones the object that is passed in, or creates an empty object one if
	    // none is provided.
	    var createInsertModeChanges = function(c) {
	      if (c) {
	        // Copy construction
	        return {
	          changes: c.changes,
	          expectCursorActivityForChange: c.expectCursorActivityForChange
	        };
	      }
	      return {
	        // Change list
	        changes: [],
	        // Set to true on change, false on cursorActivity.
	        expectCursorActivityForChange: false
	      };
	    };

	    function MacroModeState() {
	      this.latestRegister = undefined;
	      this.isPlaying = false;
	      this.isRecording = false;
	      this.replaySearchQueries = [];
	      this.onRecordingDone = undefined;
	      this.lastInsertModeChanges = createInsertModeChanges();
	    }
	    MacroModeState.prototype = {
	      exitMacroRecordMode: function() {
	        var macroModeState = vimGlobalState.macroModeState;
	        if (macroModeState.onRecordingDone) {
	          macroModeState.onRecordingDone(); // close dialog
	        }
	        macroModeState.onRecordingDone = undefined;
	        macroModeState.isRecording = false;
	      },
	      enterMacroRecordMode: function(cm, registerName) {
	        var register =
	            vimGlobalState.registerController.getRegister(registerName);
	        if (register) {
	          register.clear();
	          this.latestRegister = registerName;
	          if (cm.openDialog) {
	            this.onRecordingDone = cm.openDialog(
	                '(recording)['+registerName+']', null, {bottom:true});
	          }
	          this.isRecording = true;
	        }
	      }
	    };

	    function maybeInitVimState(cm) {
	      if (!cm.state.vim) {
	        // Store instance state in the CodeMirror object.
	        cm.state.vim = {
	          inputState: new InputState(),
	          // Vim's input state that triggered the last edit, used to repeat
	          // motions and operators with '.'.
	          lastEditInputState: undefined,
	          // Vim's action command before the last edit, used to repeat actions
	          // with '.' and insert mode repeat.
	          lastEditActionCommand: undefined,
	          // When using jk for navigation, if you move from a longer line to a
	          // shorter line, the cursor may clip to the end of the shorter line.
	          // If j is pressed again and cursor goes to the next line, the
	          // cursor should go back to its horizontal position on the longer
	          // line if it can. This is to keep track of the horizontal position.
	          lastHPos: -1,
	          // Doing the same with screen-position for gj/gk
	          lastHSPos: -1,
	          // The last motion command run. Cleared if a non-motion command gets
	          // executed in between.
	          lastMotion: null,
	          marks: {},
	          // Mark for rendering fake cursor for visual mode.
	          fakeCursor: null,
	          insertMode: false,
	          // Repeat count for changes made in insert mode, triggered by key
	          // sequences like 3,i. Only exists when insertMode is true.
	          insertModeRepeat: undefined,
	          visualMode: false,
	          // If we are in visual line mode. No effect if visualMode is false.
	          visualLine: false,
	          visualBlock: false,
	          lastSelection: null,
	          lastPastedText: null,
	          sel: {},
	          // Buffer-local/window-local values of vim options.
	          options: {}
	        };
	      }
	      return cm.state.vim;
	    }
	    var vimGlobalState;
	    function resetVimGlobalState() {
	      vimGlobalState = {
	        // The current search query.
	        searchQuery: null,
	        // Whether we are searching backwards.
	        searchIsReversed: false,
	        // Replace part of the last substituted pattern
	        lastSubstituteReplacePart: undefined,
	        jumpList: createCircularJumpList(),
	        macroModeState: new MacroModeState,
	        // Recording latest f, t, F or T motion command.
	        lastCharacterSearch: {increment:0, forward:true, selectedCharacter:''},
	        registerController: new RegisterController({}),
	        // search history buffer
	        searchHistoryController: new HistoryController(),
	        // ex Command history buffer
	        exCommandHistoryController : new HistoryController()
	      };
	      for (var optionName in options) {
	        var option = options[optionName];
	        option.value = option.defaultValue;
	      }
	    }

	    var lastInsertModeKeyTimer;
	    var vimApi= {
	      buildKeyMap: function() {
	        // TODO: Convert keymap into dictionary format for fast lookup.
	      },
	      // Testing hook, though it might be useful to expose the register
	      // controller anyways.
	      getRegisterController: function() {
	        return vimGlobalState.registerController;
	      },
	      // Testing hook.
	      resetVimGlobalState_: resetVimGlobalState,

	      // Testing hook.
	      getVimGlobalState_: function() {
	        return vimGlobalState;
	      },

	      // Testing hook.
	      maybeInitVimState_: maybeInitVimState,

	      suppressErrorLogging: false,

	      InsertModeKey: InsertModeKey,
	      map: function(lhs, rhs, ctx) {
	        // Add user defined key bindings.
	        exCommandDispatcher.map(lhs, rhs, ctx);
	      },
	      unmap: function(lhs, ctx) {
	        exCommandDispatcher.unmap(lhs, ctx);
	      },
	      // TODO: Expose setOption and getOption as instance methods. Need to decide how to namespace
	      // them, or somehow make them work with the existing CodeMirror setOption/getOption API.
	      setOption: setOption,
	      getOption: getOption,
	      defineOption: defineOption,
	      defineEx: function(name, prefix, func){
	        if (!prefix) {
	          prefix = name;
	        } else if (name.indexOf(prefix) !== 0) {
	          throw new Error('(Vim.defineEx) "'+prefix+'" is not a prefix of "'+name+'", command not registered');
	        }
	        exCommands[name]=func;
	        exCommandDispatcher.commandMap_[prefix]={name:name, shortName:prefix, type:'api'};
	      },
	      handleKey: function (cm, key, origin) {
	        var command = this.findKey(cm, key, origin);
	        if (typeof command === 'function') {
	          return command();
	        }
	      },
	      /**
	       * This is the outermost function called by CodeMirror, after keys have
	       * been mapped to their Vim equivalents.
	       *
	       * Finds a command based on the key (and cached keys if there is a
	       * multi-key sequence). Returns `undefined` if no key is matched, a noop
	       * function if a partial match is found (multi-key), and a function to
	       * execute the bound command if a a key is matched. The function always
	       * returns true.
	       */
	      findKey: function(cm, key, origin) {
	        var vim = maybeInitVimState(cm);
	        function handleMacroRecording() {
	          var macroModeState = vimGlobalState.macroModeState;
	          if (macroModeState.isRecording) {
	            if (key == 'q') {
	              macroModeState.exitMacroRecordMode();
	              clearInputState(cm);
	              return true;
	            }
	            if (origin != 'mapping') {
	              logKey(macroModeState, key);
	            }
	          }
	        }
	        function handleEsc() {
	          if (key == '<Esc>') {
	            // Clear input state and get back to normal mode.
	            clearInputState(cm);
	            if (vim.visualMode) {
	              exitVisualMode(cm);
	            } else if (vim.insertMode) {
	              exitInsertMode(cm);
	            }
	            return true;
	          }
	        }
	        function doKeyToKey(keys) {
	          // TODO: prevent infinite recursion.
	          var match;
	          while (keys) {
	            // Pull off one command key, which is either a single character
	            // or a special sequence wrapped in '<' and '>', e.g. '<Space>'.
	            match = (/<\w+-.+?>|<\w+>|./).exec(keys);
	            key = match[0];
	            keys = keys.substring(match.index + key.length);
	            CodeMirror.Vim.handleKey(cm, key, 'mapping');
	          }
	        }

	        function handleKeyInsertMode() {
	          if (handleEsc()) { return true; }
	          var keys = vim.inputState.keyBuffer = vim.inputState.keyBuffer + key;
	          var keysAreChars = key.length == 1;
	          var match = commandDispatcher.matchCommand(keys, defaultKeymap, vim.inputState, 'insert');
	          // Need to check all key substrings in insert mode.
	          while (keys.length > 1 && match.type != 'full') {
	            var keys = vim.inputState.keyBuffer = keys.slice(1);
	            var thisMatch = commandDispatcher.matchCommand(keys, defaultKeymap, vim.inputState, 'insert');
	            if (thisMatch.type != 'none') { match = thisMatch; }
	          }
	          if (match.type == 'none') { clearInputState(cm); return false; }
	          else if (match.type == 'partial') {
	            if (lastInsertModeKeyTimer) { window.clearTimeout(lastInsertModeKeyTimer); }
	            lastInsertModeKeyTimer = window.setTimeout(
	              function() { if (vim.insertMode && vim.inputState.keyBuffer) { clearInputState(cm); } },
	              getOption('insertModeEscKeysTimeout'));
	            return !keysAreChars;
	          }

	          if (lastInsertModeKeyTimer) { window.clearTimeout(lastInsertModeKeyTimer); }
	          if (keysAreChars) {
	            var selections = cm.listSelections();
	            for (var i = 0; i < selections.length; i++) {
	              var here = selections[i].head;
	              cm.replaceRange('', offsetCursor(here, 0, -(keys.length - 1)), here, '+input');
	            }
	            vimGlobalState.macroModeState.lastInsertModeChanges.changes.pop();
	          }
	          clearInputState(cm);
	          return match.command;
	        }

	        function handleKeyNonInsertMode() {
	          if (handleMacroRecording() || handleEsc()) { return true; };

	          var keys = vim.inputState.keyBuffer = vim.inputState.keyBuffer + key;
	          if (/^[1-9]\d*$/.test(keys)) { return true; }

	          var keysMatcher = /^(\d*)(.*)$/.exec(keys);
	          if (!keysMatcher) { clearInputState(cm); return false; }
	          var context = vim.visualMode ? 'visual' :
	                                         'normal';
	          var match = commandDispatcher.matchCommand(keysMatcher[2] || keysMatcher[1], defaultKeymap, vim.inputState, context);
	          if (match.type == 'none') { clearInputState(cm); return false; }
	          else if (match.type == 'partial') { return true; }

	          vim.inputState.keyBuffer = '';
	          var keysMatcher = /^(\d*)(.*)$/.exec(keys);
	          if (keysMatcher[1] && keysMatcher[1] != '0') {
	            vim.inputState.pushRepeatDigit(keysMatcher[1]);
	          }
	          return match.command;
	        }

	        var command;
	        if (vim.insertMode) { command = handleKeyInsertMode(); }
	        else { command = handleKeyNonInsertMode(); }
	        if (command === false) {
	          return undefined;
	        } else if (command === true) {
	          // TODO: Look into using CodeMirror's multi-key handling.
	          // Return no-op since we are caching the key. Counts as handled, but
	          // don't want act on it just yet.
	          return function() { return true; };
	        } else {
	          return function() {
	            return cm.operation(function() {
	              cm.curOp.isVimOp = true;
	              try {
	                if (command.type == 'keyToKey') {
	                  doKeyToKey(command.toKeys);
	                } else {
	                  commandDispatcher.processCommand(cm, vim, command);
	                }
	              } catch (e) {
	                // clear VIM state in case it's in a bad state.
	                cm.state.vim = undefined;
	                maybeInitVimState(cm);
	                if (!CodeMirror.Vim.suppressErrorLogging) {
	                  console['log'](e);
	                }
	                throw e;
	              }
	              return true;
	            });
	          };
	        }
	      },
	      handleEx: function(cm, input) {
	        exCommandDispatcher.processCommand(cm, input);
	      },

	      defineMotion: defineMotion,
	      defineAction: defineAction,
	      defineOperator: defineOperator,
	      mapCommand: mapCommand,
	      _mapCommand: _mapCommand,

	      defineRegister: defineRegister,

	      exitVisualMode: exitVisualMode,
	      exitInsertMode: exitInsertMode
	    };

	    // Represents the current input state.
	    function InputState() {
	      this.prefixRepeat = [];
	      this.motionRepeat = [];

	      this.operator = null;
	      this.operatorArgs = null;
	      this.motion = null;
	      this.motionArgs = null;
	      this.keyBuffer = []; // For matching multi-key commands.
	      this.registerName = null; // Defaults to the unnamed register.
	    }
	    InputState.prototype.pushRepeatDigit = function(n) {
	      if (!this.operator) {
	        this.prefixRepeat = this.prefixRepeat.concat(n);
	      } else {
	        this.motionRepeat = this.motionRepeat.concat(n);
	      }
	    };
	    InputState.prototype.getRepeat = function() {
	      var repeat = 0;
	      if (this.prefixRepeat.length > 0 || this.motionRepeat.length > 0) {
	        repeat = 1;
	        if (this.prefixRepeat.length > 0) {
	          repeat *= parseInt(this.prefixRepeat.join(''), 10);
	        }
	        if (this.motionRepeat.length > 0) {
	          repeat *= parseInt(this.motionRepeat.join(''), 10);
	        }
	      }
	      return repeat;
	    };

	    function clearInputState(cm, reason) {
	      cm.state.vim.inputState = new InputState();
	      CodeMirror.signal(cm, 'vim-command-done', reason);
	    }

	    /*
	     * Register stores information about copy and paste registers.  Besides
	     * text, a register must store whether it is linewise (i.e., when it is
	     * pasted, should it insert itself into a new line, or should the text be
	     * inserted at the cursor position.)
	     */
	    function Register(text, linewise, blockwise) {
	      this.clear();
	      this.keyBuffer = [text || ''];
	      this.insertModeChanges = [];
	      this.searchQueries = [];
	      this.linewise = !!linewise;
	      this.blockwise = !!blockwise;
	    }
	    Register.prototype = {
	      setText: function(text, linewise, blockwise) {
	        this.keyBuffer = [text || ''];
	        this.linewise = !!linewise;
	        this.blockwise = !!blockwise;
	      },
	      pushText: function(text, linewise) {
	        // if this register has ever been set to linewise, use linewise.
	        if (linewise) {
	          if (!this.linewise) {
	            this.keyBuffer.push('\n');
	          }
	          this.linewise = true;
	        }
	        this.keyBuffer.push(text);
	      },
	      pushInsertModeChanges: function(changes) {
	        this.insertModeChanges.push(createInsertModeChanges(changes));
	      },
	      pushSearchQuery: function(query) {
	        this.searchQueries.push(query);
	      },
	      clear: function() {
	        this.keyBuffer = [];
	        this.insertModeChanges = [];
	        this.searchQueries = [];
	        this.linewise = false;
	      },
	      toString: function() {
	        return this.keyBuffer.join('');
	      }
	    };

	    /**
	     * Defines an external register.
	     *
	     * The name should be a single character that will be used to reference the register.
	     * The register should support setText, pushText, clear, and toString(). See Register
	     * for a reference implementation.
	     */
	    function defineRegister(name, register) {
	      var registers = vimGlobalState.registerController.registers;
	      if (!name || name.length != 1) {
	        throw Error('Register name must be 1 character');
	      }
	      if (registers[name]) {
	        throw Error('Register already defined ' + name);
	      }
	      registers[name] = register;
	      validRegisters.push(name);
	    }

	    /*
	     * vim registers allow you to keep many independent copy and paste buffers.
	     * See http://usevim.com/2012/04/13/registers/ for an introduction.
	     *
	     * RegisterController keeps the state of all the registers.  An initial
	     * state may be passed in.  The unnamed register '"' will always be
	     * overridden.
	     */
	    function RegisterController(registers) {
	      this.registers = registers;
	      this.unnamedRegister = registers['"'] = new Register();
	      registers['.'] = new Register();
	      registers[':'] = new Register();
	      registers['/'] = new Register();
	    }
	    RegisterController.prototype = {
	      pushText: function(registerName, operator, text, linewise, blockwise) {
	        if (linewise && text.charAt(text.length - 1) !== '\n'){
	          text += '\n';
	        }
	        // Lowercase and uppercase registers refer to the same register.
	        // Uppercase just means append.
	        var register = this.isValidRegister(registerName) ?
	            this.getRegister(registerName) : null;
	        // if no register/an invalid register was specified, things go to the
	        // default registers
	        if (!register) {
	          switch (operator) {
	            case 'yank':
	              // The 0 register contains the text from the most recent yank.
	              this.registers['0'] = new Register(text, linewise, blockwise);
	              break;
	            case 'delete':
	            case 'change':
	              if (text.indexOf('\n') == -1) {
	                // Delete less than 1 line. Update the small delete register.
	                this.registers['-'] = new Register(text, linewise);
	              } else {
	                // Shift down the contents of the numbered registers and put the
	                // deleted text into register 1.
	                this.shiftNumericRegisters_();
	                this.registers['1'] = new Register(text, linewise);
	              }
	              break;
	          }
	          // Make sure the unnamed register is set to what just happened
	          this.unnamedRegister.setText(text, linewise, blockwise);
	          return;
	        }

	        // If we've gotten to this point, we've actually specified a register
	        var append = isUpperCase(registerName);
	        if (append) {
	          register.pushText(text, linewise);
	        } else {
	          register.setText(text, linewise, blockwise);
	        }
	        // The unnamed register always has the same value as the last used
	        // register.
	        this.unnamedRegister.setText(register.toString(), linewise);
	      },
	      // Gets the register named @name.  If one of @name doesn't already exist,
	      // create it.  If @name is invalid, return the unnamedRegister.
	      getRegister: function(name) {
	        if (!this.isValidRegister(name)) {
	          return this.unnamedRegister;
	        }
	        name = name.toLowerCase();
	        if (!this.registers[name]) {
	          this.registers[name] = new Register();
	        }
	        return this.registers[name];
	      },
	      isValidRegister: function(name) {
	        return name && inArray(name, validRegisters);
	      },
	      shiftNumericRegisters_: function() {
	        for (var i = 9; i >= 2; i--) {
	          this.registers[i] = this.getRegister('' + (i - 1));
	        }
	      }
	    };
	    function HistoryController() {
	        this.historyBuffer = [];
	        this.iterator = 0;
	        this.initialPrefix = null;
	    }
	    HistoryController.prototype = {
	      // the input argument here acts a user entered prefix for a small time
	      // until we start autocompletion in which case it is the autocompleted.
	      nextMatch: function (input, up) {
	        var historyBuffer = this.historyBuffer;
	        var dir = up ? -1 : 1;
	        if (this.initialPrefix === null) this.initialPrefix = input;
	        for (var i = this.iterator + dir; up ? i >= 0 : i < historyBuffer.length; i+= dir) {
	          var element = historyBuffer[i];
	          for (var j = 0; j <= element.length; j++) {
	            if (this.initialPrefix == element.substring(0, j)) {
	              this.iterator = i;
	              return element;
	            }
	          }
	        }
	        // should return the user input in case we reach the end of buffer.
	        if (i >= historyBuffer.length) {
	          this.iterator = historyBuffer.length;
	          return this.initialPrefix;
	        }
	        // return the last autocompleted query or exCommand as it is.
	        if (i < 0 ) return input;
	      },
	      pushInput: function(input) {
	        var index = this.historyBuffer.indexOf(input);
	        if (index > -1) this.historyBuffer.splice(index, 1);
	        if (input.length) this.historyBuffer.push(input);
	      },
	      reset: function() {
	        this.initialPrefix = null;
	        this.iterator = this.historyBuffer.length;
	      }
	    };
	    var commandDispatcher = {
	      matchCommand: function(keys, keyMap, inputState, context) {
	        var matches = commandMatches(keys, keyMap, context, inputState);
	        if (!matches.full && !matches.partial) {
	          return {type: 'none'};
	        } else if (!matches.full && matches.partial) {
	          return {type: 'partial'};
	        }

	        var bestMatch;
	        for (var i = 0; i < matches.full.length; i++) {
	          var match = matches.full[i];
	          if (!bestMatch) {
	            bestMatch = match;
	          }
	        }
	        if (bestMatch.keys.slice(-11) == '<character>') {
	          var character = lastChar(keys);
	          if (!character) return {type: 'none'};
	          inputState.selectedCharacter = character;
	        }
	        return {type: 'full', command: bestMatch};
	      },
	      processCommand: function(cm, vim, command) {
	        vim.inputState.repeatOverride = command.repeatOverride;
	        switch (command.type) {
	          case 'motion':
	            this.processMotion(cm, vim, command);
	            break;
	          case 'operator':
	            this.processOperator(cm, vim, command);
	            break;
	          case 'operatorMotion':
	            this.processOperatorMotion(cm, vim, command);
	            break;
	          case 'action':
	            this.processAction(cm, vim, command);
	            break;
	          case 'search':
	            this.processSearch(cm, vim, command);
	            break;
	          case 'ex':
	          case 'keyToEx':
	            this.processEx(cm, vim, command);
	            break;
	          default:
	            break;
	        }
	      },
	      processMotion: function(cm, vim, command) {
	        vim.inputState.motion = command.motion;
	        vim.inputState.motionArgs = copyArgs(command.motionArgs);
	        this.evalInput(cm, vim);
	      },
	      processOperator: function(cm, vim, command) {
	        var inputState = vim.inputState;
	        if (inputState.operator) {
	          if (inputState.operator == command.operator) {
	            // Typing an operator twice like 'dd' makes the operator operate
	            // linewise
	            inputState.motion = 'expandToLine';
	            inputState.motionArgs = { linewise: true };
	            this.evalInput(cm, vim);
	            return;
	          } else {
	            // 2 different operators in a row doesn't make sense.
	            clearInputState(cm);
	          }
	        }
	        inputState.operator = command.operator;
	        inputState.operatorArgs = copyArgs(command.operatorArgs);
	        if (vim.visualMode) {
	          // Operating on a selection in visual mode. We don't need a motion.
	          this.evalInput(cm, vim);
	        }
	      },
	      processOperatorMotion: function(cm, vim, command) {
	        var visualMode = vim.visualMode;
	        var operatorMotionArgs = copyArgs(command.operatorMotionArgs);
	        if (operatorMotionArgs) {
	          // Operator motions may have special behavior in visual mode.
	          if (visualMode && operatorMotionArgs.visualLine) {
	            vim.visualLine = true;
	          }
	        }
	        this.processOperator(cm, vim, command);
	        if (!visualMode) {
	          this.processMotion(cm, vim, command);
	        }
	      },
	      processAction: function(cm, vim, command) {
	        var inputState = vim.inputState;
	        var repeat = inputState.getRepeat();
	        var repeatIsExplicit = !!repeat;
	        var actionArgs = copyArgs(command.actionArgs) || {};
	        if (inputState.selectedCharacter) {
	          actionArgs.selectedCharacter = inputState.selectedCharacter;
	        }
	        // Actions may or may not have motions and operators. Do these first.
	        if (command.operator) {
	          this.processOperator(cm, vim, command);
	        }
	        if (command.motion) {
	          this.processMotion(cm, vim, command);
	        }
	        if (command.motion || command.operator) {
	          this.evalInput(cm, vim);
	        }
	        actionArgs.repeat = repeat || 1;
	        actionArgs.repeatIsExplicit = repeatIsExplicit;
	        actionArgs.registerName = inputState.registerName;
	        clearInputState(cm);
	        vim.lastMotion = null;
	        if (command.isEdit) {
	          this.recordLastEdit(vim, inputState, command);
	        }
	        actions[command.action](cm, actionArgs, vim);
	      },
	      processSearch: function(cm, vim, command) {
	        if (!cm.getSearchCursor) {
	          // Search depends on SearchCursor.
	          return;
	        }
	        var forward = command.searchArgs.forward;
	        var wholeWordOnly = command.searchArgs.wholeWordOnly;
	        getSearchState(cm).setReversed(!forward);
	        var promptPrefix = (forward) ? '/' : '?';
	        var originalQuery = getSearchState(cm).getQuery();
	        var originalScrollPos = cm.getScrollInfo();
	        function handleQuery(query, ignoreCase, smartCase) {
	          vimGlobalState.searchHistoryController.pushInput(query);
	          vimGlobalState.searchHistoryController.reset();
	          try {
	            updateSearchQuery(cm, query, ignoreCase, smartCase);
	          } catch (e) {
	            showConfirm(cm, 'Invalid regex: ' + query);
	            clearInputState(cm);
	            return;
	          }
	          commandDispatcher.processMotion(cm, vim, {
	            type: 'motion',
	            motion: 'findNext',
	            motionArgs: { forward: true, toJumplist: command.searchArgs.toJumplist }
	          });
	        }
	        function onPromptClose(query) {
	          cm.scrollTo(originalScrollPos.left, originalScrollPos.top);
	          handleQuery(query, true /** ignoreCase */, true /** smartCase */);
	          var macroModeState = vimGlobalState.macroModeState;
	          if (macroModeState.isRecording) {
	            logSearchQuery(macroModeState, query);
	          }
	        }
	        function onPromptKeyUp(e, query, close) {
	          var keyName = CodeMirror.keyName(e), up, offset;
	          if (keyName == 'Up' || keyName == 'Down') {
	            up = keyName == 'Up' ? true : false;
	            offset = e.target ? e.target.selectionEnd : 0;
	            query = vimGlobalState.searchHistoryController.nextMatch(query, up) || '';
	            close(query);
	            if (offset && e.target) e.target.selectionEnd = e.target.selectionStart = Math.min(offset, e.target.value.length);
	          } else {
	            if ( keyName != 'Left' && keyName != 'Right' && keyName != 'Ctrl' && keyName != 'Alt' && keyName != 'Shift')
	              vimGlobalState.searchHistoryController.reset();
	          }
	          var parsedQuery;
	          try {
	            parsedQuery = updateSearchQuery(cm, query,
	                true /** ignoreCase */, true /** smartCase */);
	          } catch (e) {
	            // Swallow bad regexes for incremental search.
	          }
	          if (parsedQuery) {
	            cm.scrollIntoView(findNext(cm, !forward, parsedQuery), 30);
	          } else {
	            clearSearchHighlight(cm);
	            cm.scrollTo(originalScrollPos.left, originalScrollPos.top);
	          }
	        }
	        function onPromptKeyDown(e, query, close) {
	          var keyName = CodeMirror.keyName(e);
	          if (keyName == 'Esc' || keyName == 'Ctrl-C' || keyName == 'Ctrl-[' ||
	              (keyName == 'Backspace' && query == '')) {
	            vimGlobalState.searchHistoryController.pushInput(query);
	            vimGlobalState.searchHistoryController.reset();
	            updateSearchQuery(cm, originalQuery);
	            clearSearchHighlight(cm);
	            cm.scrollTo(originalScrollPos.left, originalScrollPos.top);
	            CodeMirror.e_stop(e);
	            clearInputState(cm);
	            close();
	            cm.focus();
	          } else if (keyName == 'Up' || keyName == 'Down') {
	            CodeMirror.e_stop(e);
	          } else if (keyName == 'Ctrl-U') {
	            // Ctrl-U clears input.
	            CodeMirror.e_stop(e);
	            close('');
	          }
	        }
	        switch (command.searchArgs.querySrc) {
	          case 'prompt':
	            var macroModeState = vimGlobalState.macroModeState;
	            if (macroModeState.isPlaying) {
	              var query = macroModeState.replaySearchQueries.shift();
	              handleQuery(query, true /** ignoreCase */, false /** smartCase */);
	            } else {
	              showPrompt(cm, {
	                  onClose: onPromptClose,
	                  prefix: promptPrefix,
	                  desc: searchPromptDesc,
	                  onKeyUp: onPromptKeyUp,
	                  onKeyDown: onPromptKeyDown
	              });
	            }
	            break;
	          case 'wordUnderCursor':
	            var word = expandWordUnderCursor(cm, false /** inclusive */,
	                true /** forward */, false /** bigWord */,
	                true /** noSymbol */);
	            var isKeyword = true;
	            if (!word) {
	              word = expandWordUnderCursor(cm, false /** inclusive */,
	                  true /** forward */, false /** bigWord */,
	                  false /** noSymbol */);
	              isKeyword = false;
	            }
	            if (!word) {
	              return;
	            }
	            var query = cm.getLine(word.start.line).substring(word.start.ch,
	                word.end.ch);
	            if (isKeyword && wholeWordOnly) {
	                query = '\\b' + query + '\\b';
	            } else {
	              query = escapeRegex(query);
	            }

	            // cachedCursor is used to save the old position of the cursor
	            // when * or # causes vim to seek for the nearest word and shift
	            // the cursor before entering the motion.
	            vimGlobalState.jumpList.cachedCursor = cm.getCursor();
	            cm.setCursor(word.start);

	            handleQuery(query, true /** ignoreCase */, false /** smartCase */);
	            break;
	        }
	      },
	      processEx: function(cm, vim, command) {
	        function onPromptClose(input) {
	          // Give the prompt some time to close so that if processCommand shows
	          // an error, the elements don't overlap.
	          vimGlobalState.exCommandHistoryController.pushInput(input);
	          vimGlobalState.exCommandHistoryController.reset();
	          exCommandDispatcher.processCommand(cm, input);
	        }
	        function onPromptKeyDown(e, input, close) {
	          var keyName = CodeMirror.keyName(e), up, offset;
	          if (keyName == 'Esc' || keyName == 'Ctrl-C' || keyName == 'Ctrl-[' ||
	              (keyName == 'Backspace' && input == '')) {
	            vimGlobalState.exCommandHistoryController.pushInput(input);
	            vimGlobalState.exCommandHistoryController.reset();
	            CodeMirror.e_stop(e);
	            clearInputState(cm);
	            close();
	            cm.focus();
	          }
	          if (keyName == 'Up' || keyName == 'Down') {
	            CodeMirror.e_stop(e);
	            up = keyName == 'Up' ? true : false;
	            offset = e.target ? e.target.selectionEnd : 0;
	            input = vimGlobalState.exCommandHistoryController.nextMatch(input, up) || '';
	            close(input);
	            if (offset && e.target) e.target.selectionEnd = e.target.selectionStart = Math.min(offset, e.target.value.length);
	          } else if (keyName == 'Ctrl-U') {
	            // Ctrl-U clears input.
	            CodeMirror.e_stop(e);
	            close('');
	          } else {
	            if ( keyName != 'Left' && keyName != 'Right' && keyName != 'Ctrl' && keyName != 'Alt' && keyName != 'Shift')
	              vimGlobalState.exCommandHistoryController.reset();
	          }
	        }
	        if (command.type == 'keyToEx') {
	          // Handle user defined Ex to Ex mappings
	          exCommandDispatcher.processCommand(cm, command.exArgs.input);
	        } else {
	          if (vim.visualMode) {
	            showPrompt(cm, { onClose: onPromptClose, prefix: ':', value: '\'<,\'>',
	                onKeyDown: onPromptKeyDown});
	          } else {
	            showPrompt(cm, { onClose: onPromptClose, prefix: ':',
	                onKeyDown: onPromptKeyDown});
	          }
	        }
	      },
	      evalInput: function(cm, vim) {
	        // If the motion command is set, execute both the operator and motion.
	        // Otherwise return.
	        var inputState = vim.inputState;
	        var motion = inputState.motion;
	        var motionArgs = inputState.motionArgs || {};
	        var operator = inputState.operator;
	        var operatorArgs = inputState.operatorArgs || {};
	        var registerName = inputState.registerName;
	        var sel = vim.sel;
	        // TODO: Make sure cm and vim selections are identical outside visual mode.
	        var origHead = copyCursor(vim.visualMode ? clipCursorToContent(cm, sel.head): cm.getCursor('head'));
	        var origAnchor = copyCursor(vim.visualMode ? clipCursorToContent(cm, sel.anchor) : cm.getCursor('anchor'));
	        var oldHead = copyCursor(origHead);
	        var oldAnchor = copyCursor(origAnchor);
	        var newHead, newAnchor;
	        var repeat;
	        if (operator) {
	          this.recordLastEdit(vim, inputState);
	        }
	        if (inputState.repeatOverride !== undefined) {
	          // If repeatOverride is specified, that takes precedence over the
	          // input state's repeat. Used by Ex mode and can be user defined.
	          repeat = inputState.repeatOverride;
	        } else {
	          repeat = inputState.getRepeat();
	        }
	        if (repeat > 0 && motionArgs.explicitRepeat) {
	          motionArgs.repeatIsExplicit = true;
	        } else if (motionArgs.noRepeat ||
	            (!motionArgs.explicitRepeat && repeat === 0)) {
	          repeat = 1;
	          motionArgs.repeatIsExplicit = false;
	        }
	        if (inputState.selectedCharacter) {
	          // If there is a character input, stick it in all of the arg arrays.
	          motionArgs.selectedCharacter = operatorArgs.selectedCharacter =
	              inputState.selectedCharacter;
	        }
	        motionArgs.repeat = repeat;
	        clearInputState(cm);
	        if (motion) {
	          var motionResult = motions[motion](cm, origHead, motionArgs, vim);
	          vim.lastMotion = motions[motion];
	          if (!motionResult) {
	            return;
	          }
	          if (motionArgs.toJumplist) {
	            var jumpList = vimGlobalState.jumpList;
	            // if the current motion is # or *, use cachedCursor
	            var cachedCursor = jumpList.cachedCursor;
	            if (cachedCursor) {
	              recordJumpPosition(cm, cachedCursor, motionResult);
	              delete jumpList.cachedCursor;
	            } else {
	              recordJumpPosition(cm, origHead, motionResult);
	            }
	          }
	          if (motionResult instanceof Array) {
	            newAnchor = motionResult[0];
	            newHead = motionResult[1];
	          } else {
	            newHead = motionResult;
	          }
	          // TODO: Handle null returns from motion commands better.
	          if (!newHead) {
	            newHead = copyCursor(origHead);
	          }
	          if (vim.visualMode) {
	            if (!(vim.visualBlock && newHead.ch === Infinity)) {
	              newHead = clipCursorToContent(cm, newHead, vim.visualBlock);
	            }
	            if (newAnchor) {
	              newAnchor = clipCursorToContent(cm, newAnchor, true);
	            }
	            newAnchor = newAnchor || oldAnchor;
	            sel.anchor = newAnchor;
	            sel.head = newHead;
	            updateCmSelection(cm);
	            updateMark(cm, vim, '<',
	                cursorIsBefore(newAnchor, newHead) ? newAnchor
	                    : newHead);
	            updateMark(cm, vim, '>',
	                cursorIsBefore(newAnchor, newHead) ? newHead
	                    : newAnchor);
	          } else if (!operator) {
	            newHead = clipCursorToContent(cm, newHead);
	            cm.setCursor(newHead.line, newHead.ch);
	          }
	        }
	        if (operator) {
	          if (operatorArgs.lastSel) {
	            // Replaying a visual mode operation
	            newAnchor = oldAnchor;
	            var lastSel = operatorArgs.lastSel;
	            var lineOffset = Math.abs(lastSel.head.line - lastSel.anchor.line);
	            var chOffset = Math.abs(lastSel.head.ch - lastSel.anchor.ch);
	            if (lastSel.visualLine) {
	              // Linewise Visual mode: The same number of lines.
	              newHead = Pos(oldAnchor.line + lineOffset, oldAnchor.ch);
	            } else if (lastSel.visualBlock) {
	              // Blockwise Visual mode: The same number of lines and columns.
	              newHead = Pos(oldAnchor.line + lineOffset, oldAnchor.ch + chOffset);
	            } else if (lastSel.head.line == lastSel.anchor.line) {
	              // Normal Visual mode within one line: The same number of characters.
	              newHead = Pos(oldAnchor.line, oldAnchor.ch + chOffset);
	            } else {
	              // Normal Visual mode with several lines: The same number of lines, in the
	              // last line the same number of characters as in the last line the last time.
	              newHead = Pos(oldAnchor.line + lineOffset, oldAnchor.ch);
	            }
	            vim.visualMode = true;
	            vim.visualLine = lastSel.visualLine;
	            vim.visualBlock = lastSel.visualBlock;
	            sel = vim.sel = {
	              anchor: newAnchor,
	              head: newHead
	            };
	            updateCmSelection(cm);
	          } else if (vim.visualMode) {
	            operatorArgs.lastSel = {
	              anchor: copyCursor(sel.anchor),
	              head: copyCursor(sel.head),
	              visualBlock: vim.visualBlock,
	              visualLine: vim.visualLine
	            };
	          }
	          var curStart, curEnd, linewise, mode;
	          var cmSel;
	          if (vim.visualMode) {
	            // Init visual op
	            curStart = cursorMin(sel.head, sel.anchor);
	            curEnd = cursorMax(sel.head, sel.anchor);
	            linewise = vim.visualLine || operatorArgs.linewise;
	            mode = vim.visualBlock ? 'block' :
	                   linewise ? 'line' :
	                   'char';
	            cmSel = makeCmSelection(cm, {
	              anchor: curStart,
	              head: curEnd
	            }, mode);
	            if (linewise) {
	              var ranges = cmSel.ranges;
	              if (mode == 'block') {
	                // Linewise operators in visual block mode extend to end of line
	                for (var i = 0; i < ranges.length; i++) {
	                  ranges[i].head.ch = lineLength(cm, ranges[i].head.line);
	                }
	              } else if (mode == 'line') {
	                ranges[0].head = Pos(ranges[0].head.line + 1, 0);
	              }
	            }
	          } else {
	            // Init motion op
	            curStart = copyCursor(newAnchor || oldAnchor);
	            curEnd = copyCursor(newHead || oldHead);
	            if (cursorIsBefore(curEnd, curStart)) {
	              var tmp = curStart;
	              curStart = curEnd;
	              curEnd = tmp;
	            }
	            linewise = motionArgs.linewise || operatorArgs.linewise;
	            if (linewise) {
	              // Expand selection to entire line.
	              expandSelectionToLine(cm, curStart, curEnd);
	            } else if (motionArgs.forward) {
	              // Clip to trailing newlines only if the motion goes forward.
	              clipToLine(cm, curStart, curEnd);
	            }
	            mode = 'char';
	            var exclusive = !motionArgs.inclusive || linewise;
	            cmSel = makeCmSelection(cm, {
	              anchor: curStart,
	              head: curEnd
	            }, mode, exclusive);
	          }
	          cm.setSelections(cmSel.ranges, cmSel.primary);
	          vim.lastMotion = null;
	          operatorArgs.repeat = repeat; // For indent in visual mode.
	          operatorArgs.registerName = registerName;
	          // Keep track of linewise as it affects how paste and change behave.
	          operatorArgs.linewise = linewise;
	          var operatorMoveTo = operators[operator](
	            cm, operatorArgs, cmSel.ranges, oldAnchor, newHead);
	          if (vim.visualMode) {
	            exitVisualMode(cm, operatorMoveTo != null);
	          }
	          if (operatorMoveTo) {
	            cm.setCursor(operatorMoveTo);
	          }
	        }
	      },
	      recordLastEdit: function(vim, inputState, actionCommand) {
	        var macroModeState = vimGlobalState.macroModeState;
	        if (macroModeState.isPlaying) { return; }
	        vim.lastEditInputState = inputState;
	        vim.lastEditActionCommand = actionCommand;
	        macroModeState.lastInsertModeChanges.changes = [];
	        macroModeState.lastInsertModeChanges.expectCursorActivityForChange = false;
	      }
	    };

	    /**
	     * typedef {Object{line:number,ch:number}} Cursor An object containing the
	     *     position of the cursor.
	     */
	    // All of the functions below return Cursor objects.
	    var motions = {
	      moveToTopLine: function(cm, _head, motionArgs) {
	        var line = getUserVisibleLines(cm).top + motionArgs.repeat -1;
	        return Pos(line, findFirstNonWhiteSpaceCharacter(cm.getLine(line)));
	      },
	      moveToMiddleLine: function(cm) {
	        var range = getUserVisibleLines(cm);
	        var line = Math.floor((range.top + range.bottom) * 0.5);
	        return Pos(line, findFirstNonWhiteSpaceCharacter(cm.getLine(line)));
	      },
	      moveToBottomLine: function(cm, _head, motionArgs) {
	        var line = getUserVisibleLines(cm).bottom - motionArgs.repeat +1;
	        return Pos(line, findFirstNonWhiteSpaceCharacter(cm.getLine(line)));
	      },
	      expandToLine: function(_cm, head, motionArgs) {
	        // Expands forward to end of line, and then to next line if repeat is
	        // >1. Does not handle backward motion!
	        var cur = head;
	        return Pos(cur.line + motionArgs.repeat - 1, Infinity);
	      },
	      findNext: function(cm, _head, motionArgs) {
	        var state = getSearchState(cm);
	        var query = state.getQuery();
	        if (!query) {
	          return;
	        }
	        var prev = !motionArgs.forward;
	        // If search is initiated with ? instead of /, negate direction.
	        prev = (state.isReversed()) ? !prev : prev;
	        highlightSearchMatches(cm, query);
	        return findNext(cm, prev/** prev */, query, motionArgs.repeat);
	      },
	      goToMark: function(cm, _head, motionArgs, vim) {
	        var pos = getMarkPos(cm, vim, motionArgs.selectedCharacter);
	        if (pos) {
	          return motionArgs.linewise ? { line: pos.line, ch: findFirstNonWhiteSpaceCharacter(cm.getLine(pos.line)) } : pos;
	        }
	        return null;
	      },
	      moveToOtherHighlightedEnd: function(cm, _head, motionArgs, vim) {
	        if (vim.visualBlock && motionArgs.sameLine) {
	          var sel = vim.sel;
	          return [
	            clipCursorToContent(cm, Pos(sel.anchor.line, sel.head.ch)),
	            clipCursorToContent(cm, Pos(sel.head.line, sel.anchor.ch))
	          ];
	        } else {
	          return ([vim.sel.head, vim.sel.anchor]);
	        }
	      },
	      jumpToMark: function(cm, head, motionArgs, vim) {
	        var best = head;
	        for (var i = 0; i < motionArgs.repeat; i++) {
	          var cursor = best;
	          for (var key in vim.marks) {
	            if (!isLowerCase(key)) {
	              continue;
	            }
	            var mark = vim.marks[key].find();
	            var isWrongDirection = (motionArgs.forward) ?
	              cursorIsBefore(mark, cursor) : cursorIsBefore(cursor, mark);

	            if (isWrongDirection) {
	              continue;
	            }
	            if (motionArgs.linewise && (mark.line == cursor.line)) {
	              continue;
	            }

	            var equal = cursorEqual(cursor, best);
	            var between = (motionArgs.forward) ?
	              cursorIsBetween(cursor, mark, best) :
	              cursorIsBetween(best, mark, cursor);

	            if (equal || between) {
	              best = mark;
	            }
	          }
	        }

	        if (motionArgs.linewise) {
	          // Vim places the cursor on the first non-whitespace character of
	          // the line if there is one, else it places the cursor at the end
	          // of the line, regardless of whether a mark was found.
	          best = Pos(best.line, findFirstNonWhiteSpaceCharacter(cm.getLine(best.line)));
	        }
	        return best;
	      },
	      moveByCharacters: function(_cm, head, motionArgs) {
	        var cur = head;
	        var repeat = motionArgs.repeat;
	        var ch = motionArgs.forward ? cur.ch + repeat : cur.ch - repeat;
	        return Pos(cur.line, ch);
	      },
	      moveByLines: function(cm, head, motionArgs, vim) {
	        var cur = head;
	        var endCh = cur.ch;
	        // Depending what our last motion was, we may want to do different
	        // things. If our last motion was moving vertically, we want to
	        // preserve the HPos from our last horizontal move.  If our last motion
	        // was going to the end of a line, moving vertically we should go to
	        // the end of the line, etc.
	        switch (vim.lastMotion) {
	          case this.moveByLines:
	          case this.moveByDisplayLines:
	          case this.moveByScroll:
	          case this.moveToColumn:
	          case this.moveToEol:
	            endCh = vim.lastHPos;
	            break;
	          default:
	            vim.lastHPos = endCh;
	        }
	        var repeat = motionArgs.repeat+(motionArgs.repeatOffset||0);
	        var line = motionArgs.forward ? cur.line + repeat : cur.line - repeat;
	        var first = cm.firstLine();
	        var last = cm.lastLine();
	        // Vim go to line begin or line end when cursor at first/last line and
	        // move to previous/next line is triggered.
	        if (line < first && cur.line == first){
	          return this.moveToStartOfLine(cm, head, motionArgs, vim);
	        }else if (line > last && cur.line == last){
	            return this.moveToEol(cm, head, motionArgs, vim);
	        }
	        if (motionArgs.toFirstChar){
	          endCh=findFirstNonWhiteSpaceCharacter(cm.getLine(line));
	          vim.lastHPos = endCh;
	        }
	        vim.lastHSPos = cm.charCoords(Pos(line, endCh),'div').left;
	        return Pos(line, endCh);
	      },
	      moveByDisplayLines: function(cm, head, motionArgs, vim) {
	        var cur = head;
	        switch (vim.lastMotion) {
	          case this.moveByDisplayLines:
	          case this.moveByScroll:
	          case this.moveByLines:
	          case this.moveToColumn:
	          case this.moveToEol:
	            break;
	          default:
	            vim.lastHSPos = cm.charCoords(cur,'div').left;
	        }
	        var repeat = motionArgs.repeat;
	        var res=cm.findPosV(cur,(motionArgs.forward ? repeat : -repeat),'line',vim.lastHSPos);
	        if (res.hitSide) {
	          if (motionArgs.forward) {
	            var lastCharCoords = cm.charCoords(res, 'div');
	            var goalCoords = { top: lastCharCoords.top + 8, left: vim.lastHSPos };
	            var res = cm.coordsChar(goalCoords, 'div');
	          } else {
	            var resCoords = cm.charCoords(Pos(cm.firstLine(), 0), 'div');
	            resCoords.left = vim.lastHSPos;
	            res = cm.coordsChar(resCoords, 'div');
	          }
	        }
	        vim.lastHPos = res.ch;
	        return res;
	      },
	      moveByPage: function(cm, head, motionArgs) {
	        // CodeMirror only exposes functions that move the cursor page down, so
	        // doing this bad hack to move the cursor and move it back. evalInput
	        // will move the cursor to where it should be in the end.
	        var curStart = head;
	        var repeat = motionArgs.repeat;
	        return cm.findPosV(curStart, (motionArgs.forward ? repeat : -repeat), 'page');
	      },
	      moveByParagraph: function(cm, head, motionArgs) {
	        var dir = motionArgs.forward ? 1 : -1;
	        return findParagraph(cm, head, motionArgs.repeat, dir);
	      },
	      moveByScroll: function(cm, head, motionArgs, vim) {
	        var scrollbox = cm.getScrollInfo();
	        var curEnd = null;
	        var repeat = motionArgs.repeat;
	        if (!repeat) {
	          repeat = scrollbox.clientHeight / (2 * cm.defaultTextHeight());
	        }
	        var orig = cm.charCoords(head, 'local');
	        motionArgs.repeat = repeat;
	        var curEnd = motions.moveByDisplayLines(cm, head, motionArgs, vim);
	        if (!curEnd) {
	          return null;
	        }
	        var dest = cm.charCoords(curEnd, 'local');
	        cm.scrollTo(null, scrollbox.top + dest.top - orig.top);
	        return curEnd;
	      },
	      moveByWords: function(cm, head, motionArgs) {
	        return moveToWord(cm, head, motionArgs.repeat, !!motionArgs.forward,
	            !!motionArgs.wordEnd, !!motionArgs.bigWord);
	      },
	      moveTillCharacter: function(cm, _head, motionArgs) {
	        var repeat = motionArgs.repeat;
	        var curEnd = moveToCharacter(cm, repeat, motionArgs.forward,
	            motionArgs.selectedCharacter);
	        var increment = motionArgs.forward ? -1 : 1;
	        recordLastCharacterSearch(increment, motionArgs);
	        if (!curEnd) return null;
	        curEnd.ch += increment;
	        return curEnd;
	      },
	      moveToCharacter: function(cm, head, motionArgs) {
	        var repeat = motionArgs.repeat;
	        recordLastCharacterSearch(0, motionArgs);
	        return moveToCharacter(cm, repeat, motionArgs.forward,
	            motionArgs.selectedCharacter) || head;
	      },
	      moveToSymbol: function(cm, head, motionArgs) {
	        var repeat = motionArgs.repeat;
	        return findSymbol(cm, repeat, motionArgs.forward,
	            motionArgs.selectedCharacter) || head;
	      },
	      moveToColumn: function(cm, head, motionArgs, vim) {
	        var repeat = motionArgs.repeat;
	        // repeat is equivalent to which column we want to move to!
	        vim.lastHPos = repeat - 1;
	        vim.lastHSPos = cm.charCoords(head,'div').left;
	        return moveToColumn(cm, repeat);
	      },
	      moveToEol: function(cm, head, motionArgs, vim) {
	        var cur = head;
	        vim.lastHPos = Infinity;
	        var retval= Pos(cur.line + motionArgs.repeat - 1, Infinity);
	        var end=cm.clipPos(retval);
	        end.ch--;
	        vim.lastHSPos = cm.charCoords(end,'div').left;
	        return retval;
	      },
	      moveToFirstNonWhiteSpaceCharacter: function(cm, head) {
	        // Go to the start of the line where the text begins, or the end for
	        // whitespace-only lines
	        var cursor = head;
	        return Pos(cursor.line,
	                   findFirstNonWhiteSpaceCharacter(cm.getLine(cursor.line)));
	      },
	      moveToMatchedSymbol: function(cm, head) {
	        var cursor = head;
	        var line = cursor.line;
	        var ch = cursor.ch;
	        var lineText = cm.getLine(line);
	        var symbol;
	        for (; ch < lineText.length; ch++) {
	          symbol = lineText.charAt(ch);
	          if (symbol && isMatchableSymbol(symbol)) {
	            var style = cm.getTokenTypeAt(Pos(line, ch + 1));
	            if (style !== "string" && style !== "comment") {
	              break;
	            }
	          }
	        }
	        if (ch < lineText.length) {
	          var matched = cm.findMatchingBracket(Pos(line, ch));
	          return matched.to;
	        } else {
	          return cursor;
	        }
	      },
	      moveToStartOfLine: function(_cm, head) {
	        return Pos(head.line, 0);
	      },
	      moveToLineOrEdgeOfDocument: function(cm, _head, motionArgs) {
	        var lineNum = motionArgs.forward ? cm.lastLine() : cm.firstLine();
	        if (motionArgs.repeatIsExplicit) {
	          lineNum = motionArgs.repeat - cm.getOption('firstLineNumber');
	        }
	        return Pos(lineNum,
	                   findFirstNonWhiteSpaceCharacter(cm.getLine(lineNum)));
	      },
	      textObjectManipulation: function(cm, head, motionArgs, vim) {
	        // TODO: lots of possible exceptions that can be thrown here. Try da(
	        //     outside of a () block.

	        // TODO: adding <> >< to this map doesn't work, presumably because
	        // they're operators
	        var mirroredPairs = {'(': ')', ')': '(',
	                             '{': '}', '}': '{',
	                             '[': ']', ']': '['};
	        var selfPaired = {'\'': true, '"': true};

	        var character = motionArgs.selectedCharacter;
	        // 'b' refers to  '()' block.
	        // 'B' refers to  '{}' block.
	        if (character == 'b') {
	          character = '(';
	        } else if (character == 'B') {
	          character = '{';
	        }

	        // Inclusive is the difference between a and i
	        // TODO: Instead of using the additional text object map to perform text
	        //     object operations, merge the map into the defaultKeyMap and use
	        //     motionArgs to define behavior. Define separate entries for 'aw',
	        //     'iw', 'a[', 'i[', etc.
	        var inclusive = !motionArgs.textObjectInner;

	        var tmp;
	        if (mirroredPairs[character]) {
	          tmp = selectCompanionObject(cm, head, character, inclusive);
	        } else if (selfPaired[character]) {
	          tmp = findBeginningAndEnd(cm, head, character, inclusive);
	        } else if (character === 'W') {
	          tmp = expandWordUnderCursor(cm, inclusive, true /** forward */,
	                                                     true /** bigWord */);
	        } else if (character === 'w') {
	          tmp = expandWordUnderCursor(cm, inclusive, true /** forward */,
	                                                     false /** bigWord */);
	        } else if (character === 'p') {
	          tmp = findParagraph(cm, head, motionArgs.repeat, 0, inclusive);
	          motionArgs.linewise = true;
	          if (vim.visualMode) {
	            if (!vim.visualLine) { vim.visualLine = true; }
	          } else {
	            var operatorArgs = vim.inputState.operatorArgs;
	            if (operatorArgs) { operatorArgs.linewise = true; }
	            tmp.end.line--;
	          }
	        } else {
	          // No text object defined for this, don't move.
	          return null;
	        }

	        if (!cm.state.vim.visualMode) {
	          return [tmp.start, tmp.end];
	        } else {
	          return expandSelection(cm, tmp.start, tmp.end);
	        }
	      },

	      repeatLastCharacterSearch: function(cm, head, motionArgs) {
	        var lastSearch = vimGlobalState.lastCharacterSearch;
	        var repeat = motionArgs.repeat;
	        var forward = motionArgs.forward === lastSearch.forward;
	        var increment = (lastSearch.increment ? 1 : 0) * (forward ? -1 : 1);
	        cm.moveH(-increment, 'char');
	        motionArgs.inclusive = forward ? true : false;
	        var curEnd = moveToCharacter(cm, repeat, forward, lastSearch.selectedCharacter);
	        if (!curEnd) {
	          cm.moveH(increment, 'char');
	          return head;
	        }
	        curEnd.ch += increment;
	        return curEnd;
	      }
	    };

	    function defineMotion(name, fn) {
	      motions[name] = fn;
	    }

	    function fillArray(val, times) {
	      var arr = [];
	      for (var i = 0; i < times; i++) {
	        arr.push(val);
	      }
	      return arr;
	    }
	    /**
	     * An operator acts on a text selection. It receives the list of selections
	     * as input. The corresponding CodeMirror selection is guaranteed to
	    * match the input selection.
	     */
	    var operators = {
	      change: function(cm, args, ranges) {
	        var finalHead, text;
	        var vim = cm.state.vim;
	        vimGlobalState.macroModeState.lastInsertModeChanges.inVisualBlock = vim.visualBlock;
	        if (!vim.visualMode) {
	          var anchor = ranges[0].anchor,
	              head = ranges[0].head;
	          text = cm.getRange(anchor, head);
	          var lastState = vim.lastEditInputState || {};
	          if (lastState.motion == "moveByWords" && !isWhiteSpaceString(text)) {
	            // Exclude trailing whitespace if the range is not all whitespace.
	            var match = (/\s+$/).exec(text);
	            if (match && lastState.motionArgs && lastState.motionArgs.forward) {
	              head = offsetCursor(head, 0, - match[0].length);
	              text = text.slice(0, - match[0].length);
	            }
	          }
	          var prevLineEnd = new Pos(anchor.line - 1, Number.MAX_VALUE);
	          var wasLastLine = cm.firstLine() == cm.lastLine();
	          if (head.line > cm.lastLine() && args.linewise && !wasLastLine) {
	            cm.replaceRange('', prevLineEnd, head);
	          } else {
	            cm.replaceRange('', anchor, head);
	          }
	          if (args.linewise) {
	            // Push the next line back down, if there is a next line.
	            if (!wasLastLine) {
	              cm.setCursor(prevLineEnd);
	              CodeMirror.commands.newlineAndIndent(cm);
	            }
	            // make sure cursor ends up at the end of the line.
	            anchor.ch = Number.MAX_VALUE;
	          }
	          finalHead = anchor;
	        } else {
	          text = cm.getSelection();
	          var replacement = fillArray('', ranges.length);
	          cm.replaceSelections(replacement);
	          finalHead = cursorMin(ranges[0].head, ranges[0].anchor);
	        }
	        vimGlobalState.registerController.pushText(
	            args.registerName, 'change', text,
	            args.linewise, ranges.length > 1);
	        actions.enterInsertMode(cm, {head: finalHead}, cm.state.vim);
	      },
	      // delete is a javascript keyword.
	      'delete': function(cm, args, ranges) {
	        var finalHead, text;
	        var vim = cm.state.vim;
	        if (!vim.visualBlock) {
	          var anchor = ranges[0].anchor,
	              head = ranges[0].head;
	          if (args.linewise &&
	              head.line != cm.firstLine() &&
	              anchor.line == cm.lastLine() &&
	              anchor.line == head.line - 1) {
	            // Special case for dd on last line (and first line).
	            if (anchor.line == cm.firstLine()) {
	              anchor.ch = 0;
	            } else {
	              anchor = Pos(anchor.line - 1, lineLength(cm, anchor.line - 1));
	            }
	          }
	          text = cm.getRange(anchor, head);
	          cm.replaceRange('', anchor, head);
	          finalHead = anchor;
	          if (args.linewise) {
	            finalHead = motions.moveToFirstNonWhiteSpaceCharacter(cm, anchor);
	          }
	        } else {
	          text = cm.getSelection();
	          var replacement = fillArray('', ranges.length);
	          cm.replaceSelections(replacement);
	          finalHead = ranges[0].anchor;
	        }
	        vimGlobalState.registerController.pushText(
	            args.registerName, 'delete', text,
	            args.linewise, vim.visualBlock);
	        var includeLineBreak = vim.insertMode
	        return clipCursorToContent(cm, finalHead, includeLineBreak);
	      },
	      indent: function(cm, args, ranges) {
	        var vim = cm.state.vim;
	        var startLine = ranges[0].anchor.line;
	        var endLine = vim.visualBlock ?
	          ranges[ranges.length - 1].anchor.line :
	          ranges[0].head.line;
	        // In visual mode, n> shifts the selection right n times, instead of
	        // shifting n lines right once.
	        var repeat = (vim.visualMode) ? args.repeat : 1;
	        if (args.linewise) {
	          // The only way to delete a newline is to delete until the start of
	          // the next line, so in linewise mode evalInput will include the next
	          // line. We don't want this in indent, so we go back a line.
	          endLine--;
	        }
	        for (var i = startLine; i <= endLine; i++) {
	          for (var j = 0; j < repeat; j++) {
	            cm.indentLine(i, args.indentRight);
	          }
	        }
	        return motions.moveToFirstNonWhiteSpaceCharacter(cm, ranges[0].anchor);
	      },
	      changeCase: function(cm, args, ranges, oldAnchor, newHead) {
	        var selections = cm.getSelections();
	        var swapped = [];
	        var toLower = args.toLower;
	        for (var j = 0; j < selections.length; j++) {
	          var toSwap = selections[j];
	          var text = '';
	          if (toLower === true) {
	            text = toSwap.toLowerCase();
	          } else if (toLower === false) {
	            text = toSwap.toUpperCase();
	          } else {
	            for (var i = 0; i < toSwap.length; i++) {
	              var character = toSwap.charAt(i);
	              text += isUpperCase(character) ? character.toLowerCase() :
	                  character.toUpperCase();
	            }
	          }
	          swapped.push(text);
	        }
	        cm.replaceSelections(swapped);
	        if (args.shouldMoveCursor){
	          return newHead;
	        } else if (!cm.state.vim.visualMode && args.linewise && ranges[0].anchor.line + 1 == ranges[0].head.line) {
	          return motions.moveToFirstNonWhiteSpaceCharacter(cm, oldAnchor);
	        } else if (args.linewise){
	          return oldAnchor;
	        } else {
	          return cursorMin(ranges[0].anchor, ranges[0].head);
	        }
	      },
	      yank: function(cm, args, ranges, oldAnchor) {
	        var vim = cm.state.vim;
	        var text = cm.getSelection();
	        var endPos = vim.visualMode
	          ? cursorMin(vim.sel.anchor, vim.sel.head, ranges[0].head, ranges[0].anchor)
	          : oldAnchor;
	        vimGlobalState.registerController.pushText(
	            args.registerName, 'yank',
	            text, args.linewise, vim.visualBlock);
	        return endPos;
	      }
	    };

	    function defineOperator(name, fn) {
	      operators[name] = fn;
	    }

	    var actions = {
	      jumpListWalk: function(cm, actionArgs, vim) {
	        if (vim.visualMode) {
	          return;
	        }
	        var repeat = actionArgs.repeat;
	        var forward = actionArgs.forward;
	        var jumpList = vimGlobalState.jumpList;

	        var mark = jumpList.move(cm, forward ? repeat : -repeat);
	        var markPos = mark ? mark.find() : undefined;
	        markPos = markPos ? markPos : cm.getCursor();
	        cm.setCursor(markPos);
	      },
	      scroll: function(cm, actionArgs, vim) {
	        if (vim.visualMode) {
	          return;
	        }
	        var repeat = actionArgs.repeat || 1;
	        var lineHeight = cm.defaultTextHeight();
	        var top = cm.getScrollInfo().top;
	        var delta = lineHeight * repeat;
	        var newPos = actionArgs.forward ? top + delta : top - delta;
	        var cursor = copyCursor(cm.getCursor());
	        var cursorCoords = cm.charCoords(cursor, 'local');
	        if (actionArgs.forward) {
	          if (newPos > cursorCoords.top) {
	             cursor.line += (newPos - cursorCoords.top) / lineHeight;
	             cursor.line = Math.ceil(cursor.line);
	             cm.setCursor(cursor);
	             cursorCoords = cm.charCoords(cursor, 'local');
	             cm.scrollTo(null, cursorCoords.top);
	          } else {
	             // Cursor stays within bounds.  Just reposition the scroll window.
	             cm.scrollTo(null, newPos);
	          }
	        } else {
	          var newBottom = newPos + cm.getScrollInfo().clientHeight;
	          if (newBottom < cursorCoords.bottom) {
	             cursor.line -= (cursorCoords.bottom - newBottom) / lineHeight;
	             cursor.line = Math.floor(cursor.line);
	             cm.setCursor(cursor);
	             cursorCoords = cm.charCoords(cursor, 'local');
	             cm.scrollTo(
	                 null, cursorCoords.bottom - cm.getScrollInfo().clientHeight);
	          } else {
	             // Cursor stays within bounds.  Just reposition the scroll window.
	             cm.scrollTo(null, newPos);
	          }
	        }
	      },
	      scrollToCursor: function(cm, actionArgs) {
	        var lineNum = cm.getCursor().line;
	        var charCoords = cm.charCoords(Pos(lineNum, 0), 'local');
	        var height = cm.getScrollInfo().clientHeight;
	        var y = charCoords.top;
	        var lineHeight = charCoords.bottom - y;
	        switch (actionArgs.position) {
	          case 'center': y = y - (height / 2) + lineHeight;
	            break;
	          case 'bottom': y = y - height + lineHeight;
	            break;
	        }
	        cm.scrollTo(null, y);
	      },
	      replayMacro: function(cm, actionArgs, vim) {
	        var registerName = actionArgs.selectedCharacter;
	        var repeat = actionArgs.repeat;
	        var macroModeState = vimGlobalState.macroModeState;
	        if (registerName == '@') {
	          registerName = macroModeState.latestRegister;
	        }
	        while(repeat--){
	          executeMacroRegister(cm, vim, macroModeState, registerName);
	        }
	      },
	      enterMacroRecordMode: function(cm, actionArgs) {
	        var macroModeState = vimGlobalState.macroModeState;
	        var registerName = actionArgs.selectedCharacter;
	        if (vimGlobalState.registerController.isValidRegister(registerName)) {
	          macroModeState.enterMacroRecordMode(cm, registerName);
	        }
	      },
	      toggleOverwrite: function(cm) {
	        if (!cm.state.overwrite) {
	          cm.toggleOverwrite(true);
	          cm.setOption('keyMap', 'vim-replace');
	          CodeMirror.signal(cm, "vim-mode-change", {mode: "replace"});
	        } else {
	          cm.toggleOverwrite(false);
	          cm.setOption('keyMap', 'vim-insert');
	          CodeMirror.signal(cm, "vim-mode-change", {mode: "insert"});
	        }
	      },
	      enterInsertMode: function(cm, actionArgs, vim) {
	        if (cm.getOption('readOnly')) { return; }
	        vim.insertMode = true;
	        vim.insertModeRepeat = actionArgs && actionArgs.repeat || 1;
	        var insertAt = (actionArgs) ? actionArgs.insertAt : null;
	        var sel = vim.sel;
	        var head = actionArgs.head || cm.getCursor('head');
	        var height = cm.listSelections().length;
	        if (insertAt == 'eol') {
	          head = Pos(head.line, lineLength(cm, head.line));
	        } else if (insertAt == 'charAfter') {
	          head = offsetCursor(head, 0, 1);
	        } else if (insertAt == 'firstNonBlank') {
	          head = motions.moveToFirstNonWhiteSpaceCharacter(cm, head);
	        } else if (insertAt == 'startOfSelectedArea') {
	          if (!vim.visualBlock) {
	            if (sel.head.line < sel.anchor.line) {
	              head = sel.head;
	            } else {
	              head = Pos(sel.anchor.line, 0);
	            }
	          } else {
	            head = Pos(
	                Math.min(sel.head.line, sel.anchor.line),
	                Math.min(sel.head.ch, sel.anchor.ch));
	            height = Math.abs(sel.head.line - sel.anchor.line) + 1;
	          }
	        } else if (insertAt == 'endOfSelectedArea') {
	          if (!vim.visualBlock) {
	            if (sel.head.line >= sel.anchor.line) {
	              head = offsetCursor(sel.head, 0, 1);
	            } else {
	              head = Pos(sel.anchor.line, 0);
	            }
	          } else {
	            head = Pos(
	                Math.min(sel.head.line, sel.anchor.line),
	                Math.max(sel.head.ch + 1, sel.anchor.ch));
	            height = Math.abs(sel.head.line - sel.anchor.line) + 1;
	          }
	        } else if (insertAt == 'inplace') {
	          if (vim.visualMode){
	            return;
	          }
	        }
	        cm.setOption('disableInput', false);
	        if (actionArgs && actionArgs.replace) {
	          // Handle Replace-mode as a special case of insert mode.
	          cm.toggleOverwrite(true);
	          cm.setOption('keyMap', 'vim-replace');
	          CodeMirror.signal(cm, "vim-mode-change", {mode: "replace"});
	        } else {
	          cm.toggleOverwrite(false);
	          cm.setOption('keyMap', 'vim-insert');
	          CodeMirror.signal(cm, "vim-mode-change", {mode: "insert"});
	        }
	        if (!vimGlobalState.macroModeState.isPlaying) {
	          // Only record if not replaying.
	          cm.on('change', onChange);
	          CodeMirror.on(cm.getInputField(), 'keydown', onKeyEventTargetKeyDown);
	        }
	        if (vim.visualMode) {
	          exitVisualMode(cm);
	        }
	        selectForInsert(cm, head, height);
	      },
	      toggleVisualMode: function(cm, actionArgs, vim) {
	        var repeat = actionArgs.repeat;
	        var anchor = cm.getCursor();
	        var head;
	        // TODO: The repeat should actually select number of characters/lines
	        //     equal to the repeat times the size of the previous visual
	        //     operation.
	        if (!vim.visualMode) {
	          // Entering visual mode
	          vim.visualMode = true;
	          vim.visualLine = !!actionArgs.linewise;
	          vim.visualBlock = !!actionArgs.blockwise;
	          head = clipCursorToContent(
	              cm, Pos(anchor.line, anchor.ch + repeat - 1),
	              true /** includeLineBreak */);
	          vim.sel = {
	            anchor: anchor,
	            head: head
	          };
	          CodeMirror.signal(cm, "vim-mode-change", {mode: "visual", subMode: vim.visualLine ? "linewise" : vim.visualBlock ? "blockwise" : ""});
	          updateCmSelection(cm);
	          updateMark(cm, vim, '<', cursorMin(anchor, head));
	          updateMark(cm, vim, '>', cursorMax(anchor, head));
	        } else if (vim.visualLine ^ actionArgs.linewise ||
	            vim.visualBlock ^ actionArgs.blockwise) {
	          // Toggling between modes
	          vim.visualLine = !!actionArgs.linewise;
	          vim.visualBlock = !!actionArgs.blockwise;
	          CodeMirror.signal(cm, "vim-mode-change", {mode: "visual", subMode: vim.visualLine ? "linewise" : vim.visualBlock ? "blockwise" : ""});
	          updateCmSelection(cm);
	        } else {
	          exitVisualMode(cm);
	        }
	      },
	      reselectLastSelection: function(cm, _actionArgs, vim) {
	        var lastSelection = vim.lastSelection;
	        if (vim.visualMode) {
	          updateLastSelection(cm, vim);
	        }
	        if (lastSelection) {
	          var anchor = lastSelection.anchorMark.find();
	          var head = lastSelection.headMark.find();
	          if (!anchor || !head) {
	            // If the marks have been destroyed due to edits, do nothing.
	            return;
	          }
	          vim.sel = {
	            anchor: anchor,
	            head: head
	          };
	          vim.visualMode = true;
	          vim.visualLine = lastSelection.visualLine;
	          vim.visualBlock = lastSelection.visualBlock;
	          updateCmSelection(cm);
	          updateMark(cm, vim, '<', cursorMin(anchor, head));
	          updateMark(cm, vim, '>', cursorMax(anchor, head));
	          CodeMirror.signal(cm, 'vim-mode-change', {
	            mode: 'visual',
	            subMode: vim.visualLine ? 'linewise' :
	                     vim.visualBlock ? 'blockwise' : ''});
	        }
	      },
	      joinLines: function(cm, actionArgs, vim) {
	        var curStart, curEnd;
	        if (vim.visualMode) {
	          curStart = cm.getCursor('anchor');
	          curEnd = cm.getCursor('head');
	          if (cursorIsBefore(curEnd, curStart)) {
	            var tmp = curEnd;
	            curEnd = curStart;
	            curStart = tmp;
	          }
	          curEnd.ch = lineLength(cm, curEnd.line) - 1;
	        } else {
	          // Repeat is the number of lines to join. Minimum 2 lines.
	          var repeat = Math.max(actionArgs.repeat, 2);
	          curStart = cm.getCursor();
	          curEnd = clipCursorToContent(cm, Pos(curStart.line + repeat - 1,
	                                               Infinity));
	        }
	        var finalCh = 0;
	        for (var i = curStart.line; i < curEnd.line; i++) {
	          finalCh = lineLength(cm, curStart.line);
	          var tmp = Pos(curStart.line + 1,
	                        lineLength(cm, curStart.line + 1));
	          var text = cm.getRange(curStart, tmp);
	          text = text.replace(/\n\s*/g, ' ');
	          cm.replaceRange(text, curStart, tmp);
	        }
	        var curFinalPos = Pos(curStart.line, finalCh);
	        if (vim.visualMode) {
	          exitVisualMode(cm, false);
	        }
	        cm.setCursor(curFinalPos);
	      },
	      newLineAndEnterInsertMode: function(cm, actionArgs, vim) {
	        vim.insertMode = true;
	        var insertAt = copyCursor(cm.getCursor());
	        if (insertAt.line === cm.firstLine() && !actionArgs.after) {
	          // Special case for inserting newline before start of document.
	          cm.replaceRange('\n', Pos(cm.firstLine(), 0));
	          cm.setCursor(cm.firstLine(), 0);
	        } else {
	          insertAt.line = (actionArgs.after) ? insertAt.line :
	              insertAt.line - 1;
	          insertAt.ch = lineLength(cm, insertAt.line);
	          cm.setCursor(insertAt);
	          var newlineFn = CodeMirror.commands.newlineAndIndentContinueComment ||
	              CodeMirror.commands.newlineAndIndent;
	          newlineFn(cm);
	        }
	        this.enterInsertMode(cm, { repeat: actionArgs.repeat }, vim);
	      },
	      paste: function(cm, actionArgs, vim) {
	        var cur = copyCursor(cm.getCursor());
	        var register = vimGlobalState.registerController.getRegister(
	            actionArgs.registerName);
	        var text = register.toString();
	        if (!text) {
	          return;
	        }
	        if (actionArgs.matchIndent) {
	          var tabSize = cm.getOption("tabSize");
	          // length that considers tabs and tabSize
	          var whitespaceLength = function(str) {
	            var tabs = (str.split("\t").length - 1);
	            var spaces = (str.split(" ").length - 1);
	            return tabs * tabSize + spaces * 1;
	          };
	          var currentLine = cm.getLine(cm.getCursor().line);
	          var indent = whitespaceLength(currentLine.match(/^\s*/)[0]);
	          // chomp last newline b/c don't want it to match /^\s*/gm
	          var chompedText = text.replace(/\n$/, '');
	          var wasChomped = text !== chompedText;
	          var firstIndent = whitespaceLength(text.match(/^\s*/)[0]);
	          var text = chompedText.replace(/^\s*/gm, function(wspace) {
	            var newIndent = indent + (whitespaceLength(wspace) - firstIndent);
	            if (newIndent < 0) {
	              return "";
	            }
	            else if (cm.getOption("indentWithTabs")) {
	              var quotient = Math.floor(newIndent / tabSize);
	              return Array(quotient + 1).join('\t');
	            }
	            else {
	              return Array(newIndent + 1).join(' ');
	            }
	          });
	          text += wasChomped ? "\n" : "";
	        }
	        if (actionArgs.repeat > 1) {
	          var text = Array(actionArgs.repeat + 1).join(text);
	        }
	        var linewise = register.linewise;
	        var blockwise = register.blockwise;
	        if (linewise) {
	          if(vim.visualMode) {
	            text = vim.visualLine ? text.slice(0, -1) : '\n' + text.slice(0, text.length - 1) + '\n';
	          } else if (actionArgs.after) {
	            // Move the newline at the end to the start instead, and paste just
	            // before the newline character of the line we are on right now.
	            text = '\n' + text.slice(0, text.length - 1);
	            cur.ch = lineLength(cm, cur.line);
	          } else {
	            cur.ch = 0;
	          }
	        } else {
	          if (blockwise) {
	            text = text.split('\n');
	            for (var i = 0; i < text.length; i++) {
	              text[i] = (text[i] == '') ? ' ' : text[i];
	            }
	          }
	          cur.ch += actionArgs.after ? 1 : 0;
	        }
	        var curPosFinal;
	        var idx;
	        if (vim.visualMode) {
	          //  save the pasted text for reselection if the need arises
	          vim.lastPastedText = text;
	          var lastSelectionCurEnd;
	          var selectedArea = getSelectedAreaRange(cm, vim);
	          var selectionStart = selectedArea[0];
	          var selectionEnd = selectedArea[1];
	          var selectedText = cm.getSelection();
	          var selections = cm.listSelections();
	          var emptyStrings = new Array(selections.length).join('1').split('1');
	          // save the curEnd marker before it get cleared due to cm.replaceRange.
	          if (vim.lastSelection) {
	            lastSelectionCurEnd = vim.lastSelection.headMark.find();
	          }
	          // push the previously selected text to unnamed register
	          vimGlobalState.registerController.unnamedRegister.setText(selectedText);
	          if (blockwise) {
	            // first delete the selected text
	            cm.replaceSelections(emptyStrings);
	            // Set new selections as per the block length of the yanked text
	            selectionEnd = Pos(selectionStart.line + text.length-1, selectionStart.ch);
	            cm.setCursor(selectionStart);
	            selectBlock(cm, selectionEnd);
	            cm.replaceSelections(text);
	            curPosFinal = selectionStart;
	          } else if (vim.visualBlock) {
	            cm.replaceSelections(emptyStrings);
	            cm.setCursor(selectionStart);
	            cm.replaceRange(text, selectionStart, selectionStart);
	            curPosFinal = selectionStart;
	          } else {
	            cm.replaceRange(text, selectionStart, selectionEnd);
	            curPosFinal = cm.posFromIndex(cm.indexFromPos(selectionStart) + text.length - 1);
	          }
	          // restore the the curEnd marker
	          if(lastSelectionCurEnd) {
	            vim.lastSelection.headMark = cm.setBookmark(lastSelectionCurEnd);
	          }
	          if (linewise) {
	            curPosFinal.ch=0;
	          }
	        } else {
	          if (blockwise) {
	            cm.setCursor(cur);
	            for (var i = 0; i < text.length; i++) {
	              var line = cur.line+i;
	              if (line > cm.lastLine()) {
	                cm.replaceRange('\n',  Pos(line, 0));
	              }
	              var lastCh = lineLength(cm, line);
	              if (lastCh < cur.ch) {
	                extendLineToColumn(cm, line, cur.ch);
	              }
	            }
	            cm.setCursor(cur);
	            selectBlock(cm, Pos(cur.line + text.length-1, cur.ch));
	            cm.replaceSelections(text);
	            curPosFinal = cur;
	          } else {
	            cm.replaceRange(text, cur);
	            // Now fine tune the cursor to where we want it.
	            if (linewise && actionArgs.after) {
	              curPosFinal = Pos(
	              cur.line + 1,
	              findFirstNonWhiteSpaceCharacter(cm.getLine(cur.line + 1)));
	            } else if (linewise && !actionArgs.after) {
	              curPosFinal = Pos(
	                cur.line,
	                findFirstNonWhiteSpaceCharacter(cm.getLine(cur.line)));
	            } else if (!linewise && actionArgs.after) {
	              idx = cm.indexFromPos(cur);
	              curPosFinal = cm.posFromIndex(idx + text.length - 1);
	            } else {
	              idx = cm.indexFromPos(cur);
	              curPosFinal = cm.posFromIndex(idx + text.length);
	            }
	          }
	        }
	        if (vim.visualMode) {
	          exitVisualMode(cm, false);
	        }
	        cm.setCursor(curPosFinal);
	      },
	      undo: function(cm, actionArgs) {
	        cm.operation(function() {
	          repeatFn(cm, CodeMirror.commands.undo, actionArgs.repeat)();
	          cm.setCursor(cm.getCursor('anchor'));
	        });
	      },
	      redo: function(cm, actionArgs) {
	        repeatFn(cm, CodeMirror.commands.redo, actionArgs.repeat)();
	      },
	      setRegister: function(_cm, actionArgs, vim) {
	        vim.inputState.registerName = actionArgs.selectedCharacter;
	      },
	      setMark: function(cm, actionArgs, vim) {
	        var markName = actionArgs.selectedCharacter;
	        updateMark(cm, vim, markName, cm.getCursor());
	      },
	      replace: function(cm, actionArgs, vim) {
	        var replaceWith = actionArgs.selectedCharacter;
	        var curStart = cm.getCursor();
	        var replaceTo;
	        var curEnd;
	        var selections = cm.listSelections();
	        if (vim.visualMode) {
	          curStart = cm.getCursor('start');
	          curEnd = cm.getCursor('end');
	        } else {
	          var line = cm.getLine(curStart.line);
	          replaceTo = curStart.ch + actionArgs.repeat;
	          if (replaceTo > line.length) {
	            replaceTo=line.length;
	          }
	          curEnd = Pos(curStart.line, replaceTo);
	        }
	        if (replaceWith=='\n') {
	          if (!vim.visualMode) cm.replaceRange('', curStart, curEnd);
	          // special case, where vim help says to replace by just one line-break
	          (CodeMirror.commands.newlineAndIndentContinueComment || CodeMirror.commands.newlineAndIndent)(cm);
	        } else {
	          var replaceWithStr = cm.getRange(curStart, curEnd);
	          //replace all characters in range by selected, but keep linebreaks
	          replaceWithStr = replaceWithStr.replace(/[^\n]/g, replaceWith);
	          if (vim.visualBlock) {
	            // Tabs are split in visua block before replacing
	            var spaces = new Array(cm.getOption("tabSize")+1).join(' ');
	            replaceWithStr = cm.getSelection();
	            replaceWithStr = replaceWithStr.replace(/\t/g, spaces).replace(/[^\n]/g, replaceWith).split('\n');
	            cm.replaceSelections(replaceWithStr);
	          } else {
	            cm.replaceRange(replaceWithStr, curStart, curEnd);
	          }
	          if (vim.visualMode) {
	            curStart = cursorIsBefore(selections[0].anchor, selections[0].head) ?
	                         selections[0].anchor : selections[0].head;
	            cm.setCursor(curStart);
	            exitVisualMode(cm, false);
	          } else {
	            cm.setCursor(offsetCursor(curEnd, 0, -1));
	          }
	        }
	      },
	      incrementNumberToken: function(cm, actionArgs) {
	        var cur = cm.getCursor();
	        var lineStr = cm.getLine(cur.line);
	        var re = /-?\d+/g;
	        var match;
	        var start;
	        var end;
	        var numberStr;
	        var token;
	        while ((match = re.exec(lineStr)) !== null) {
	          token = match[0];
	          start = match.index;
	          end = start + token.length;
	          if (cur.ch < end)break;
	        }
	        if (!actionArgs.backtrack && (end <= cur.ch))return;
	        if (token) {
	          var increment = actionArgs.increase ? 1 : -1;
	          var number = parseInt(token) + (increment * actionArgs.repeat);
	          var from = Pos(cur.line, start);
	          var to = Pos(cur.line, end);
	          numberStr = number.toString();
	          cm.replaceRange(numberStr, from, to);
	        } else {
	          return;
	        }
	        cm.setCursor(Pos(cur.line, start + numberStr.length - 1));
	      },
	      repeatLastEdit: function(cm, actionArgs, vim) {
	        var lastEditInputState = vim.lastEditInputState;
	        if (!lastEditInputState) { return; }
	        var repeat = actionArgs.repeat;
	        if (repeat && actionArgs.repeatIsExplicit) {
	          vim.lastEditInputState.repeatOverride = repeat;
	        } else {
	          repeat = vim.lastEditInputState.repeatOverride || repeat;
	        }
	        repeatLastEdit(cm, vim, repeat, false /** repeatForInsert */);
	      },
	      indent: function(cm, actionArgs) {
	        cm.indentLine(cm.getCursor().line, actionArgs.indentRight);
	      },
	      exitInsertMode: exitInsertMode
	    };

	    function defineAction(name, fn) {
	      actions[name] = fn;
	    }

	    /*
	     * Below are miscellaneous utility functions used by vim.js
	     */

	    /**
	     * Clips cursor to ensure that line is within the buffer's range
	     * If includeLineBreak is true, then allow cur.ch == lineLength.
	     */
	    function clipCursorToContent(cm, cur, includeLineBreak) {
	      var line = Math.min(Math.max(cm.firstLine(), cur.line), cm.lastLine() );
	      var maxCh = lineLength(cm, line) - 1;
	      maxCh = (includeLineBreak) ? maxCh + 1 : maxCh;
	      var ch = Math.min(Math.max(0, cur.ch), maxCh);
	      return Pos(line, ch);
	    }
	    function copyArgs(args) {
	      var ret = {};
	      for (var prop in args) {
	        if (args.hasOwnProperty(prop)) {
	          ret[prop] = args[prop];
	        }
	      }
	      return ret;
	    }
	    function offsetCursor(cur, offsetLine, offsetCh) {
	      if (typeof offsetLine === 'object') {
	        offsetCh = offsetLine.ch;
	        offsetLine = offsetLine.line;
	      }
	      return Pos(cur.line + offsetLine, cur.ch + offsetCh);
	    }
	    function getOffset(anchor, head) {
	      return {
	        line: head.line - anchor.line,
	        ch: head.line - anchor.line
	      };
	    }
	    function commandMatches(keys, keyMap, context, inputState) {
	      // Partial matches are not applied. They inform the key handler
	      // that the current key sequence is a subsequence of a valid key
	      // sequence, so that the key buffer is not cleared.
	      var match, partial = [], full = [];
	      for (var i = 0; i < keyMap.length; i++) {
	        var command = keyMap[i];
	        if (context == 'insert' && command.context != 'insert' ||
	            command.context && command.context != context ||
	            inputState.operator && command.type == 'action' ||
	            !(match = commandMatch(keys, command.keys))) { continue; }
	        if (match == 'partial') { partial.push(command); }
	        if (match == 'full') { full.push(command); }
	      }
	      return {
	        partial: partial.length && partial,
	        full: full.length && full
	      };
	    }
	    function commandMatch(pressed, mapped) {
	      if (mapped.slice(-11) == '<character>') {
	        // Last character matches anything.
	        var prefixLen = mapped.length - 11;
	        var pressedPrefix = pressed.slice(0, prefixLen);
	        var mappedPrefix = mapped.slice(0, prefixLen);
	        return pressedPrefix == mappedPrefix && pressed.length > prefixLen ? 'full' :
	               mappedPrefix.indexOf(pressedPrefix) == 0 ? 'partial' : false;
	      } else {
	        return pressed == mapped ? 'full' :
	               mapped.indexOf(pressed) == 0 ? 'partial' : false;
	      }
	    }
	    function lastChar(keys) {
	      var match = /^.*(<[^>]+>)$/.exec(keys);
	      var selectedCharacter = match ? match[1] : keys.slice(-1);
	      if (selectedCharacter.length > 1){
	        switch(selectedCharacter){
	          case '<CR>':
	            selectedCharacter='\n';
	            break;
	          case '<Space>':
	            selectedCharacter=' ';
	            break;
	          default:
	            selectedCharacter='';
	            break;
	        }
	      }
	      return selectedCharacter;
	    }
	    function repeatFn(cm, fn, repeat) {
	      return function() {
	        for (var i = 0; i < repeat; i++) {
	          fn(cm);
	        }
	      };
	    }
	    function copyCursor(cur) {
	      return Pos(cur.line, cur.ch);
	    }
	    function cursorEqual(cur1, cur2) {
	      return cur1.ch == cur2.ch && cur1.line == cur2.line;
	    }
	    function cursorIsBefore(cur1, cur2) {
	      if (cur1.line < cur2.line) {
	        return true;
	      }
	      if (cur1.line == cur2.line && cur1.ch < cur2.ch) {
	        return true;
	      }
	      return false;
	    }
	    function cursorMin(cur1, cur2) {
	      if (arguments.length > 2) {
	        cur2 = cursorMin.apply(undefined, Array.prototype.slice.call(arguments, 1));
	      }
	      return cursorIsBefore(cur1, cur2) ? cur1 : cur2;
	    }
	    function cursorMax(cur1, cur2) {
	      if (arguments.length > 2) {
	        cur2 = cursorMax.apply(undefined, Array.prototype.slice.call(arguments, 1));
	      }
	      return cursorIsBefore(cur1, cur2) ? cur2 : cur1;
	    }
	    function cursorIsBetween(cur1, cur2, cur3) {
	      // returns true if cur2 is between cur1 and cur3.
	      var cur1before2 = cursorIsBefore(cur1, cur2);
	      var cur2before3 = cursorIsBefore(cur2, cur3);
	      return cur1before2 && cur2before3;
	    }
	    function lineLength(cm, lineNum) {
	      return cm.getLine(lineNum).length;
	    }
	    function trim(s) {
	      if (s.trim) {
	        return s.trim();
	      }
	      return s.replace(/^\s+|\s+$/g, '');
	    }
	    function escapeRegex(s) {
	      return s.replace(/([.?*+$\[\]\/\\(){}|\-])/g, '\\$1');
	    }
	    function extendLineToColumn(cm, lineNum, column) {
	      var endCh = lineLength(cm, lineNum);
	      var spaces = new Array(column-endCh+1).join(' ');
	      cm.setCursor(Pos(lineNum, endCh));
	      cm.replaceRange(spaces, cm.getCursor());
	    }
	    // This functions selects a rectangular block
	    // of text with selectionEnd as any of its corner
	    // Height of block:
	    // Difference in selectionEnd.line and first/last selection.line
	    // Width of the block:
	    // Distance between selectionEnd.ch and any(first considered here) selection.ch
	    function selectBlock(cm, selectionEnd) {
	      var selections = [], ranges = cm.listSelections();
	      var head = copyCursor(cm.clipPos(selectionEnd));
	      var isClipped = !cursorEqual(selectionEnd, head);
	      var curHead = cm.getCursor('head');
	      var primIndex = getIndex(ranges, curHead);
	      var wasClipped = cursorEqual(ranges[primIndex].head, ranges[primIndex].anchor);
	      var max = ranges.length - 1;
	      var index = max - primIndex > primIndex ? max : 0;
	      var base = ranges[index].anchor;

	      var firstLine = Math.min(base.line, head.line);
	      var lastLine = Math.max(base.line, head.line);
	      var baseCh = base.ch, headCh = head.ch;

	      var dir = ranges[index].head.ch - baseCh;
	      var newDir = headCh - baseCh;
	      if (dir > 0 && newDir <= 0) {
	        baseCh++;
	        if (!isClipped) { headCh--; }
	      } else if (dir < 0 && newDir >= 0) {
	        baseCh--;
	        if (!wasClipped) { headCh++; }
	      } else if (dir < 0 && newDir == -1) {
	        baseCh--;
	        headCh++;
	      }
	      for (var line = firstLine; line <= lastLine; line++) {
	        var range = {anchor: new Pos(line, baseCh), head: new Pos(line, headCh)};
	        selections.push(range);
	      }
	      cm.setSelections(selections);
	      selectionEnd.ch = headCh;
	      base.ch = baseCh;
	      return base;
	    }
	    function selectForInsert(cm, head, height) {
	      var sel = [];
	      for (var i = 0; i < height; i++) {
	        var lineHead = offsetCursor(head, i, 0);
	        sel.push({anchor: lineHead, head: lineHead});
	      }
	      cm.setSelections(sel, 0);
	    }
	    // getIndex returns the index of the cursor in the selections.
	    function getIndex(ranges, cursor, end) {
	      for (var i = 0; i < ranges.length; i++) {
	        var atAnchor = end != 'head' && cursorEqual(ranges[i].anchor, cursor);
	        var atHead = end != 'anchor' && cursorEqual(ranges[i].head, cursor);
	        if (atAnchor || atHead) {
	          return i;
	        }
	      }
	      return -1;
	    }
	    function getSelectedAreaRange(cm, vim) {
	      var lastSelection = vim.lastSelection;
	      var getCurrentSelectedAreaRange = function() {
	        var selections = cm.listSelections();
	        var start =  selections[0];
	        var end = selections[selections.length-1];
	        var selectionStart = cursorIsBefore(start.anchor, start.head) ? start.anchor : start.head;
	        var selectionEnd = cursorIsBefore(end.anchor, end.head) ? end.head : end.anchor;
	        return [selectionStart, selectionEnd];
	      };
	      var getLastSelectedAreaRange = function() {
	        var selectionStart = cm.getCursor();
	        var selectionEnd = cm.getCursor();
	        var block = lastSelection.visualBlock;
	        if (block) {
	          var width = block.width;
	          var height = block.height;
	          selectionEnd = Pos(selectionStart.line + height, selectionStart.ch + width);
	          var selections = [];
	          // selectBlock creates a 'proper' rectangular block.
	          // We do not want that in all cases, so we manually set selections.
	          for (var i = selectionStart.line; i < selectionEnd.line; i++) {
	            var anchor = Pos(i, selectionStart.ch);
	            var head = Pos(i, selectionEnd.ch);
	            var range = {anchor: anchor, head: head};
	            selections.push(range);
	          }
	          cm.setSelections(selections);
	        } else {
	          var start = lastSelection.anchorMark.find();
	          var end = lastSelection.headMark.find();
	          var line = end.line - start.line;
	          var ch = end.ch - start.ch;
	          selectionEnd = {line: selectionEnd.line + line, ch: line ? selectionEnd.ch : ch + selectionEnd.ch};
	          if (lastSelection.visualLine) {
	            selectionStart = Pos(selectionStart.line, 0);
	            selectionEnd = Pos(selectionEnd.line, lineLength(cm, selectionEnd.line));
	          }
	          cm.setSelection(selectionStart, selectionEnd);
	        }
	        return [selectionStart, selectionEnd];
	      };
	      if (!vim.visualMode) {
	      // In case of replaying the action.
	        return getLastSelectedAreaRange();
	      } else {
	        return getCurrentSelectedAreaRange();
	      }
	    }
	    // Updates the previous selection with the current selection's values. This
	    // should only be called in visual mode.
	    function updateLastSelection(cm, vim) {
	      var anchor = vim.sel.anchor;
	      var head = vim.sel.head;
	      // To accommodate the effect of lastPastedText in the last selection
	      if (vim.lastPastedText) {
	        head = cm.posFromIndex(cm.indexFromPos(anchor) + vim.lastPastedText.length);
	        vim.lastPastedText = null;
	      }
	      vim.lastSelection = {'anchorMark': cm.setBookmark(anchor),
	                           'headMark': cm.setBookmark(head),
	                           'anchor': copyCursor(anchor),
	                           'head': copyCursor(head),
	                           'visualMode': vim.visualMode,
	                           'visualLine': vim.visualLine,
	                           'visualBlock': vim.visualBlock};
	    }
	    function expandSelection(cm, start, end) {
	      var sel = cm.state.vim.sel;
	      var head = sel.head;
	      var anchor = sel.anchor;
	      var tmp;
	      if (cursorIsBefore(end, start)) {
	        tmp = end;
	        end = start;
	        start = tmp;
	      }
	      if (cursorIsBefore(head, anchor)) {
	        head = cursorMin(start, head);
	        anchor = cursorMax(anchor, end);
	      } else {
	        anchor = cursorMin(start, anchor);
	        head = cursorMax(head, end);
	        head = offsetCursor(head, 0, -1);
	        if (head.ch == -1 && head.line != cm.firstLine()) {
	          head = Pos(head.line - 1, lineLength(cm, head.line - 1));
	        }
	      }
	      return [anchor, head];
	    }
	    /**
	     * Updates the CodeMirror selection to match the provided vim selection.
	     * If no arguments are given, it uses the current vim selection state.
	     */
	    function updateCmSelection(cm, sel, mode) {
	      var vim = cm.state.vim;
	      sel = sel || vim.sel;
	      var mode = mode ||
	        vim.visualLine ? 'line' : vim.visualBlock ? 'block' : 'char';
	      var cmSel = makeCmSelection(cm, sel, mode);
	      cm.setSelections(cmSel.ranges, cmSel.primary);
	      updateFakeCursor(cm);
	    }
	    function makeCmSelection(cm, sel, mode, exclusive) {
	      var head = copyCursor(sel.head);
	      var anchor = copyCursor(sel.anchor);
	      if (mode == 'char') {
	        var headOffset = !exclusive && !cursorIsBefore(sel.head, sel.anchor) ? 1 : 0;
	        var anchorOffset = cursorIsBefore(sel.head, sel.anchor) ? 1 : 0;
	        head = offsetCursor(sel.head, 0, headOffset);
	        anchor = offsetCursor(sel.anchor, 0, anchorOffset);
	        return {
	          ranges: [{anchor: anchor, head: head}],
	          primary: 0
	        };
	      } else if (mode == 'line') {
	        if (!cursorIsBefore(sel.head, sel.anchor)) {
	          anchor.ch = 0;

	          var lastLine = cm.lastLine();
	          if (head.line > lastLine) {
	            head.line = lastLine;
	          }
	          head.ch = lineLength(cm, head.line);
	        } else {
	          head.ch = 0;
	          anchor.ch = lineLength(cm, anchor.line);
	        }
	        return {
	          ranges: [{anchor: anchor, head: head}],
	          primary: 0
	        };
	      } else if (mode == 'block') {
	        var top = Math.min(anchor.line, head.line),
	            left = Math.min(anchor.ch, head.ch),
	            bottom = Math.max(anchor.line, head.line),
	            right = Math.max(anchor.ch, head.ch) + 1;
	        var height = bottom - top + 1;
	        var primary = head.line == top ? 0 : height - 1;
	        var ranges = [];
	        for (var i = 0; i < height; i++) {
	          ranges.push({
	            anchor: Pos(top + i, left),
	            head: Pos(top + i, right)
	          });
	        }
	        return {
	          ranges: ranges,
	          primary: primary
	        };
	      }
	    }
	    function getHead(cm) {
	      var cur = cm.getCursor('head');
	      if (cm.getSelection().length == 1) {
	        // Small corner case when only 1 character is selected. The "real"
	        // head is the left of head and anchor.
	        cur = cursorMin(cur, cm.getCursor('anchor'));
	      }
	      return cur;
	    }

	    /**
	     * If moveHead is set to false, the CodeMirror selection will not be
	     * touched. The caller assumes the responsibility of putting the cursor
	    * in the right place.
	     */
	    function exitVisualMode(cm, moveHead) {
	      var vim = cm.state.vim;
	      if (moveHead !== false) {
	        cm.setCursor(clipCursorToContent(cm, vim.sel.head));
	      }
	      updateLastSelection(cm, vim);
	      vim.visualMode = false;
	      vim.visualLine = false;
	      vim.visualBlock = false;
	      CodeMirror.signal(cm, "vim-mode-change", {mode: "normal"});
	      if (vim.fakeCursor) {
	        vim.fakeCursor.clear();
	      }
	    }

	    // Remove any trailing newlines from the selection. For
	    // example, with the caret at the start of the last word on the line,
	    // 'dw' should word, but not the newline, while 'w' should advance the
	    // caret to the first character of the next line.
	    function clipToLine(cm, curStart, curEnd) {
	      var selection = cm.getRange(curStart, curEnd);
	      // Only clip if the selection ends with trailing newline + whitespace
	      if (/\n\s*$/.test(selection)) {
	        var lines = selection.split('\n');
	        // We know this is all whitespace.
	        lines.pop();

	        // Cases:
	        // 1. Last word is an empty line - do not clip the trailing '\n'
	        // 2. Last word is not an empty line - clip the trailing '\n'
	        var line;
	        // Find the line containing the last word, and clip all whitespace up
	        // to it.
	        for (var line = lines.pop(); lines.length > 0 && line && isWhiteSpaceString(line); line = lines.pop()) {
	          curEnd.line--;
	          curEnd.ch = 0;
	        }
	        // If the last word is not an empty line, clip an additional newline
	        if (line) {
	          curEnd.line--;
	          curEnd.ch = lineLength(cm, curEnd.line);
	        } else {
	          curEnd.ch = 0;
	        }
	      }
	    }

	    // Expand the selection to line ends.
	    function expandSelectionToLine(_cm, curStart, curEnd) {
	      curStart.ch = 0;
	      curEnd.ch = 0;
	      curEnd.line++;
	    }

	    function findFirstNonWhiteSpaceCharacter(text) {
	      if (!text) {
	        return 0;
	      }
	      var firstNonWS = text.search(/\S/);
	      return firstNonWS == -1 ? text.length : firstNonWS;
	    }

	    function expandWordUnderCursor(cm, inclusive, _forward, bigWord, noSymbol) {
	      var cur = getHead(cm);
	      var line = cm.getLine(cur.line);
	      var idx = cur.ch;

	      // Seek to first word or non-whitespace character, depending on if
	      // noSymbol is true.
	      var test = noSymbol ? wordCharTest[0] : bigWordCharTest [0];
	      while (!test(line.charAt(idx))) {
	        idx++;
	        if (idx >= line.length) { return null; }
	      }

	      if (bigWord) {
	        test = bigWordCharTest[0];
	      } else {
	        test = wordCharTest[0];
	        if (!test(line.charAt(idx))) {
	          test = wordCharTest[1];
	        }
	      }

	      var end = idx, start = idx;
	      while (test(line.charAt(end)) && end < line.length) { end++; }
	      while (test(line.charAt(start)) && start >= 0) { start--; }
	      start++;

	      if (inclusive) {
	        // If present, include all whitespace after word.
	        // Otherwise, include all whitespace before word, except indentation.
	        var wordEnd = end;
	        while (/\s/.test(line.charAt(end)) && end < line.length) { end++; }
	        if (wordEnd == end) {
	          var wordStart = start;
	          while (/\s/.test(line.charAt(start - 1)) && start > 0) { start--; }
	          if (!start) { start = wordStart; }
	        }
	      }
	      return { start: Pos(cur.line, start), end: Pos(cur.line, end) };
	    }

	    function recordJumpPosition(cm, oldCur, newCur) {
	      if (!cursorEqual(oldCur, newCur)) {
	        vimGlobalState.jumpList.add(cm, oldCur, newCur);
	      }
	    }

	    function recordLastCharacterSearch(increment, args) {
	        vimGlobalState.lastCharacterSearch.increment = increment;
	        vimGlobalState.lastCharacterSearch.forward = args.forward;
	        vimGlobalState.lastCharacterSearch.selectedCharacter = args.selectedCharacter;
	    }

	    var symbolToMode = {
	        '(': 'bracket', ')': 'bracket', '{': 'bracket', '}': 'bracket',
	        '[': 'section', ']': 'section',
	        '*': 'comment', '/': 'comment',
	        'm': 'method', 'M': 'method',
	        '#': 'preprocess'
	    };
	    var findSymbolModes = {
	      bracket: {
	        isComplete: function(state) {
	          if (state.nextCh === state.symb) {
	            state.depth++;
	            if (state.depth >= 1)return true;
	          } else if (state.nextCh === state.reverseSymb) {
	            state.depth--;
	          }
	          return false;
	        }
	      },
	      section: {
	        init: function(state) {
	          state.curMoveThrough = true;
	          state.symb = (state.forward ? ']' : '[') === state.symb ? '{' : '}';
	        },
	        isComplete: function(state) {
	          return state.index === 0 && state.nextCh === state.symb;
	        }
	      },
	      comment: {
	        isComplete: function(state) {
	          var found = state.lastCh === '*' && state.nextCh === '/';
	          state.lastCh = state.nextCh;
	          return found;
	        }
	      },
	      // TODO: The original Vim implementation only operates on level 1 and 2.
	      // The current implementation doesn't check for code block level and
	      // therefore it operates on any levels.
	      method: {
	        init: function(state) {
	          state.symb = (state.symb === 'm' ? '{' : '}');
	          state.reverseSymb = state.symb === '{' ? '}' : '{';
	        },
	        isComplete: function(state) {
	          if (state.nextCh === state.symb)return true;
	          return false;
	        }
	      },
	      preprocess: {
	        init: function(state) {
	          state.index = 0;
	        },
	        isComplete: function(state) {
	          if (state.nextCh === '#') {
	            var token = state.lineText.match(/#(\w+)/)[1];
	            if (token === 'endif') {
	              if (state.forward && state.depth === 0) {
	                return true;
	              }
	              state.depth++;
	            } else if (token === 'if') {
	              if (!state.forward && state.depth === 0) {
	                return true;
	              }
	              state.depth--;
	            }
	            if (token === 'else' && state.depth === 0)return true;
	          }
	          return false;
	        }
	      }
	    };
	    function findSymbol(cm, repeat, forward, symb) {
	      var cur = copyCursor(cm.getCursor());
	      var increment = forward ? 1 : -1;
	      var endLine = forward ? cm.lineCount() : -1;
	      var curCh = cur.ch;
	      var line = cur.line;
	      var lineText = cm.getLine(line);
	      var state = {
	        lineText: lineText,
	        nextCh: lineText.charAt(curCh),
	        lastCh: null,
	        index: curCh,
	        symb: symb,
	        reverseSymb: (forward ?  { ')': '(', '}': '{' } : { '(': ')', '{': '}' })[symb],
	        forward: forward,
	        depth: 0,
	        curMoveThrough: false
	      };
	      var mode = symbolToMode[symb];
	      if (!mode)return cur;
	      var init = findSymbolModes[mode].init;
	      var isComplete = findSymbolModes[mode].isComplete;
	      if (init) { init(state); }
	      while (line !== endLine && repeat) {
	        state.index += increment;
	        state.nextCh = state.lineText.charAt(state.index);
	        if (!state.nextCh) {
	          line += increment;
	          state.lineText = cm.getLine(line) || '';
	          if (increment > 0) {
	            state.index = 0;
	          } else {
	            var lineLen = state.lineText.length;
	            state.index = (lineLen > 0) ? (lineLen-1) : 0;
	          }
	          state.nextCh = state.lineText.charAt(state.index);
	        }
	        if (isComplete(state)) {
	          cur.line = line;
	          cur.ch = state.index;
	          repeat--;
	        }
	      }
	      if (state.nextCh || state.curMoveThrough) {
	        return Pos(line, state.index);
	      }
	      return cur;
	    }

	    /*
	     * Returns the boundaries of the next word. If the cursor in the middle of
	     * the word, then returns the boundaries of the current word, starting at
	     * the cursor. If the cursor is at the start/end of a word, and we are going
	     * forward/backward, respectively, find the boundaries of the next word.
	     *
	     * @param {CodeMirror} cm CodeMirror object.
	     * @param {Cursor} cur The cursor position.
	     * @param {boolean} forward True to search forward. False to search
	     *     backward.
	     * @param {boolean} bigWord True if punctuation count as part of the word.
	     *     False if only [a-zA-Z0-9] characters count as part of the word.
	     * @param {boolean} emptyLineIsWord True if empty lines should be treated
	     *     as words.
	     * @return {Object{from:number, to:number, line: number}} The boundaries of
	     *     the word, or null if there are no more words.
	     */
	    function findWord(cm, cur, forward, bigWord, emptyLineIsWord) {
	      var lineNum = cur.line;
	      var pos = cur.ch;
	      var line = cm.getLine(lineNum);
	      var dir = forward ? 1 : -1;
	      var charTests = bigWord ? bigWordCharTest: wordCharTest;

	      if (emptyLineIsWord && line == '') {
	        lineNum += dir;
	        line = cm.getLine(lineNum);
	        if (!isLine(cm, lineNum)) {
	          return null;
	        }
	        pos = (forward) ? 0 : line.length;
	      }

	      while (true) {
	        if (emptyLineIsWord && line == '') {
	          return { from: 0, to: 0, line: lineNum };
	        }
	        var stop = (dir > 0) ? line.length : -1;
	        var wordStart = stop, wordEnd = stop;
	        // Find bounds of next word.
	        while (pos != stop) {
	          var foundWord = false;
	          for (var i = 0; i < charTests.length && !foundWord; ++i) {
	            if (charTests[i](line.charAt(pos))) {
	              wordStart = pos;
	              // Advance to end of word.
	              while (pos != stop && charTests[i](line.charAt(pos))) {
	                pos += dir;
	              }
	              wordEnd = pos;
	              foundWord = wordStart != wordEnd;
	              if (wordStart == cur.ch && lineNum == cur.line &&
	                  wordEnd == wordStart + dir) {
	                // We started at the end of a word. Find the next one.
	                continue;
	              } else {
	                return {
	                  from: Math.min(wordStart, wordEnd + 1),
	                  to: Math.max(wordStart, wordEnd),
	                  line: lineNum };
	              }
	            }
	          }
	          if (!foundWord) {
	            pos += dir;
	          }
	        }
	        // Advance to next/prev line.
	        lineNum += dir;
	        if (!isLine(cm, lineNum)) {
	          return null;
	        }
	        line = cm.getLine(lineNum);
	        pos = (dir > 0) ? 0 : line.length;
	      }
	    }

	    /**
	     * @param {CodeMirror} cm CodeMirror object.
	     * @param {Pos} cur The position to start from.
	     * @param {int} repeat Number of words to move past.
	     * @param {boolean} forward True to search forward. False to search
	     *     backward.
	     * @param {boolean} wordEnd True to move to end of word. False to move to
	     *     beginning of word.
	     * @param {boolean} bigWord True if punctuation count as part of the word.
	     *     False if only alphabet characters count as part of the word.
	     * @return {Cursor} The position the cursor should move to.
	     */
	    function moveToWord(cm, cur, repeat, forward, wordEnd, bigWord) {
	      var curStart = copyCursor(cur);
	      var words = [];
	      if (forward && !wordEnd || !forward && wordEnd) {
	        repeat++;
	      }
	      // For 'e', empty lines are not considered words, go figure.
	      var emptyLineIsWord = !(forward && wordEnd);
	      for (var i = 0; i < repeat; i++) {
	        var word = findWord(cm, cur, forward, bigWord, emptyLineIsWord);
	        if (!word) {
	          var eodCh = lineLength(cm, cm.lastLine());
	          words.push(forward
	              ? {line: cm.lastLine(), from: eodCh, to: eodCh}
	              : {line: 0, from: 0, to: 0});
	          break;
	        }
	        words.push(word);
	        cur = Pos(word.line, forward ? (word.to - 1) : word.from);
	      }
	      var shortCircuit = words.length != repeat;
	      var firstWord = words[0];
	      var lastWord = words.pop();
	      if (forward && !wordEnd) {
	        // w
	        if (!shortCircuit && (firstWord.from != curStart.ch || firstWord.line != curStart.line)) {
	          // We did not start in the middle of a word. Discard the extra word at the end.
	          lastWord = words.pop();
	        }
	        return Pos(lastWord.line, lastWord.from);
	      } else if (forward && wordEnd) {
	        return Pos(lastWord.line, lastWord.to - 1);
	      } else if (!forward && wordEnd) {
	        // ge
	        if (!shortCircuit && (firstWord.to != curStart.ch || firstWord.line != curStart.line)) {
	          // We did not start in the middle of a word. Discard the extra word at the end.
	          lastWord = words.pop();
	        }
	        return Pos(lastWord.line, lastWord.to);
	      } else {
	        // b
	        return Pos(lastWord.line, lastWord.from);
	      }
	    }

	    function moveToCharacter(cm, repeat, forward, character) {
	      var cur = cm.getCursor();
	      var start = cur.ch;
	      var idx;
	      for (var i = 0; i < repeat; i ++) {
	        var line = cm.getLine(cur.line);
	        idx = charIdxInLine(start, line, character, forward, true);
	        if (idx == -1) {
	          return null;
	        }
	        start = idx;
	      }
	      return Pos(cm.getCursor().line, idx);
	    }

	    function moveToColumn(cm, repeat) {
	      // repeat is always >= 1, so repeat - 1 always corresponds
	      // to the column we want to go to.
	      var line = cm.getCursor().line;
	      return clipCursorToContent(cm, Pos(line, repeat - 1));
	    }

	    function updateMark(cm, vim, markName, pos) {
	      if (!inArray(markName, validMarks)) {
	        return;
	      }
	      if (vim.marks[markName]) {
	        vim.marks[markName].clear();
	      }
	      vim.marks[markName] = cm.setBookmark(pos);
	    }

	    function charIdxInLine(start, line, character, forward, includeChar) {
	      // Search for char in line.
	      // motion_options: {forward, includeChar}
	      // If includeChar = true, include it too.
	      // If forward = true, search forward, else search backwards.
	      // If char is not found on this line, do nothing
	      var idx;
	      if (forward) {
	        idx = line.indexOf(character, start + 1);
	        if (idx != -1 && !includeChar) {
	          idx -= 1;
	        }
	      } else {
	        idx = line.lastIndexOf(character, start - 1);
	        if (idx != -1 && !includeChar) {
	          idx += 1;
	        }
	      }
	      return idx;
	    }

	    function findParagraph(cm, head, repeat, dir, inclusive) {
	      var line = head.line;
	      var min = cm.firstLine();
	      var max = cm.lastLine();
	      var start, end, i = line;
	      function isEmpty(i) { return !cm.getLine(i); }
	      function isBoundary(i, dir, any) {
	        if (any) { return isEmpty(i) != isEmpty(i + dir); }
	        return !isEmpty(i) && isEmpty(i + dir);
	      }
	      if (dir) {
	        while (min <= i && i <= max && repeat > 0) {
	          if (isBoundary(i, dir)) { repeat--; }
	          i += dir;
	        }
	        return new Pos(i, 0);
	      }

	      var vim = cm.state.vim;
	      if (vim.visualLine && isBoundary(line, 1, true)) {
	        var anchor = vim.sel.anchor;
	        if (isBoundary(anchor.line, -1, true)) {
	          if (!inclusive || anchor.line != line) {
	            line += 1;
	          }
	        }
	      }
	      var startState = isEmpty(line);
	      for (i = line; i <= max && repeat; i++) {
	        if (isBoundary(i, 1, true)) {
	          if (!inclusive || isEmpty(i) != startState) {
	            repeat--;
	          }
	        }
	      }
	      end = new Pos(i, 0);
	      // select boundary before paragraph for the last one
	      if (i > max && !startState) { startState = true; }
	      else { inclusive = false; }
	      for (i = line; i > min; i--) {
	        if (!inclusive || isEmpty(i) == startState || i == line) {
	          if (isBoundary(i, -1, true)) { break; }
	        }
	      }
	      start = new Pos(i, 0);
	      return { start: start, end: end };
	    }

	    // TODO: perhaps this finagling of start and end positions belonds
	    // in codemirror/replaceRange?
	    function selectCompanionObject(cm, head, symb, inclusive) {
	      var cur = head, start, end;

	      var bracketRegexp = ({
	        '(': /[()]/, ')': /[()]/,
	        '[': /[[\]]/, ']': /[[\]]/,
	        '{': /[{}]/, '}': /[{}]/})[symb];
	      var openSym = ({
	        '(': '(', ')': '(',
	        '[': '[', ']': '[',
	        '{': '{', '}': '{'})[symb];
	      var curChar = cm.getLine(cur.line).charAt(cur.ch);
	      // Due to the behavior of scanForBracket, we need to add an offset if the
	      // cursor is on a matching open bracket.
	      var offset = curChar === openSym ? 1 : 0;

	      start = cm.scanForBracket(Pos(cur.line, cur.ch + offset), -1, null, {'bracketRegex': bracketRegexp});
	      end = cm.scanForBracket(Pos(cur.line, cur.ch + offset), 1, null, {'bracketRegex': bracketRegexp});

	      if (!start || !end) {
	        return { start: cur, end: cur };
	      }

	      start = start.pos;
	      end = end.pos;

	      if ((start.line == end.line && start.ch > end.ch)
	          || (start.line > end.line)) {
	        var tmp = start;
	        start = end;
	        end = tmp;
	      }

	      if (inclusive) {
	        end.ch += 1;
	      } else {
	        start.ch += 1;
	      }

	      return { start: start, end: end };
	    }

	    // Takes in a symbol and a cursor and tries to simulate text objects that
	    // have identical opening and closing symbols
	    // TODO support across multiple lines
	    function findBeginningAndEnd(cm, head, symb, inclusive) {
	      var cur = copyCursor(head);
	      var line = cm.getLine(cur.line);
	      var chars = line.split('');
	      var start, end, i, len;
	      var firstIndex = chars.indexOf(symb);

	      // the decision tree is to always look backwards for the beginning first,
	      // but if the cursor is in front of the first instance of the symb,
	      // then move the cursor forward
	      if (cur.ch < firstIndex) {
	        cur.ch = firstIndex;
	        // Why is this line even here???
	        // cm.setCursor(cur.line, firstIndex+1);
	      }
	      // otherwise if the cursor is currently on the closing symbol
	      else if (firstIndex < cur.ch && chars[cur.ch] == symb) {
	        end = cur.ch; // assign end to the current cursor
	        --cur.ch; // make sure to look backwards
	      }

	      // if we're currently on the symbol, we've got a start
	      if (chars[cur.ch] == symb && !end) {
	        start = cur.ch + 1; // assign start to ahead of the cursor
	      } else {
	        // go backwards to find the start
	        for (i = cur.ch; i > -1 && !start; i--) {
	          if (chars[i] == symb) {
	            start = i + 1;
	          }
	        }
	      }

	      // look forwards for the end symbol
	      if (start && !end) {
	        for (i = start, len = chars.length; i < len && !end; i++) {
	          if (chars[i] == symb) {
	            end = i;
	          }
	        }
	      }

	      // nothing found
	      if (!start || !end) {
	        return { start: cur, end: cur };
	      }

	      // include the symbols
	      if (inclusive) {
	        --start; ++end;
	      }

	      return {
	        start: Pos(cur.line, start),
	        end: Pos(cur.line, end)
	      };
	    }

	    // Search functions
	    defineOption('pcre', true, 'boolean');
	    function SearchState() {}
	    SearchState.prototype = {
	      getQuery: function() {
	        return vimGlobalState.query;
	      },
	      setQuery: function(query) {
	        vimGlobalState.query = query;
	      },
	      getOverlay: function() {
	        return this.searchOverlay;
	      },
	      setOverlay: function(overlay) {
	        this.searchOverlay = overlay;
	      },
	      isReversed: function() {
	        return vimGlobalState.isReversed;
	      },
	      setReversed: function(reversed) {
	        vimGlobalState.isReversed = reversed;
	      },
	      getScrollbarAnnotate: function() {
	        return this.annotate;
	      },
	      setScrollbarAnnotate: function(annotate) {
	        this.annotate = annotate;
	      }
	    };
	    function getSearchState(cm) {
	      var vim = cm.state.vim;
	      return vim.searchState_ || (vim.searchState_ = new SearchState());
	    }
	    function dialog(cm, template, shortText, onClose, options) {
	      if (cm.openDialog) {
	        cm.openDialog(template, onClose, { bottom: true, value: options.value,
	            onKeyDown: options.onKeyDown, onKeyUp: options.onKeyUp,
	            selectValueOnOpen: false});
	      }
	      else {
	        onClose(prompt(shortText, ''));
	      }
	    }
	    function splitBySlash(argString) {
	      var slashes = findUnescapedSlashes(argString) || [];
	      if (!slashes.length) return [];
	      var tokens = [];
	      // in case of strings like foo/bar
	      if (slashes[0] !== 0) return;
	      for (var i = 0; i < slashes.length; i++) {
	        if (typeof slashes[i] == 'number')
	          tokens.push(argString.substring(slashes[i] + 1, slashes[i+1]));
	      }
	      return tokens;
	    }

	    function findUnescapedSlashes(str) {
	      var escapeNextChar = false;
	      var slashes = [];
	      for (var i = 0; i < str.length; i++) {
	        var c = str.charAt(i);
	        if (!escapeNextChar && c == '/') {
	          slashes.push(i);
	        }
	        escapeNextChar = !escapeNextChar && (c == '\\');
	      }
	      return slashes;
	    }

	    // Translates a search string from ex (vim) syntax into javascript form.
	    function translateRegex(str) {
	      // When these match, add a '\' if unescaped or remove one if escaped.
	      var specials = '|(){';
	      // Remove, but never add, a '\' for these.
	      var unescape = '}';
	      var escapeNextChar = false;
	      var out = [];
	      for (var i = -1; i < str.length; i++) {
	        var c = str.charAt(i) || '';
	        var n = str.charAt(i+1) || '';
	        var specialComesNext = (n && specials.indexOf(n) != -1);
	        if (escapeNextChar) {
	          if (c !== '\\' || !specialComesNext) {
	            out.push(c);
	          }
	          escapeNextChar = false;
	        } else {
	          if (c === '\\') {
	            escapeNextChar = true;
	            // Treat the unescape list as special for removing, but not adding '\'.
	            if (n && unescape.indexOf(n) != -1) {
	              specialComesNext = true;
	            }
	            // Not passing this test means removing a '\'.
	            if (!specialComesNext || n === '\\') {
	              out.push(c);
	            }
	          } else {
	            out.push(c);
	            if (specialComesNext && n !== '\\') {
	              out.push('\\');
	            }
	          }
	        }
	      }
	      return out.join('');
	    }

	    // Translates the replace part of a search and replace from ex (vim) syntax into
	    // javascript form.  Similar to translateRegex, but additionally fixes back references
	    // (translates '\[0..9]' to '$[0..9]') and follows different rules for escaping '$'.
	    var charUnescapes = {'\\n': '\n', '\\r': '\r', '\\t': '\t'};
	    function translateRegexReplace(str) {
	      var escapeNextChar = false;
	      var out = [];
	      for (var i = -1; i < str.length; i++) {
	        var c = str.charAt(i) || '';
	        var n = str.charAt(i+1) || '';
	        if (charUnescapes[c + n]) {
	          out.push(charUnescapes[c+n]);
	          i++;
	        } else if (escapeNextChar) {
	          // At any point in the loop, escapeNextChar is true if the previous
	          // character was a '\' and was not escaped.
	          out.push(c);
	          escapeNextChar = false;
	        } else {
	          if (c === '\\') {
	            escapeNextChar = true;
	            if ((isNumber(n) || n === '$')) {
	              out.push('$');
	            } else if (n !== '/' && n !== '\\') {
	              out.push('\\');
	            }
	          } else {
	            if (c === '$') {
	              out.push('$');
	            }
	            out.push(c);
	            if (n === '/') {
	              out.push('\\');
	            }
	          }
	        }
	      }
	      return out.join('');
	    }

	    // Unescape \ and / in the replace part, for PCRE mode.
	    var unescapes = {'\\/': '/', '\\\\': '\\', '\\n': '\n', '\\r': '\r', '\\t': '\t'};
	    function unescapeRegexReplace(str) {
	      var stream = new CodeMirror.StringStream(str);
	      var output = [];
	      while (!stream.eol()) {
	        // Search for \.
	        while (stream.peek() && stream.peek() != '\\') {
	          output.push(stream.next());
	        }
	        var matched = false;
	        for (var matcher in unescapes) {
	          if (stream.match(matcher, true)) {
	            matched = true;
	            output.push(unescapes[matcher]);
	            break;
	          }
	        }
	        if (!matched) {
	          // Don't change anything
	          output.push(stream.next());
	        }
	      }
	      return output.join('');
	    }

	    /**
	     * Extract the regular expression from the query and return a Regexp object.
	     * Returns null if the query is blank.
	     * If ignoreCase is passed in, the Regexp object will have the 'i' flag set.
	     * If smartCase is passed in, and the query contains upper case letters,
	     *   then ignoreCase is overridden, and the 'i' flag will not be set.
	     * If the query contains the /i in the flag part of the regular expression,
	     *   then both ignoreCase and smartCase are ignored, and 'i' will be passed
	     *   through to the Regex object.
	     */
	    function parseQuery(query, ignoreCase, smartCase) {
	      // First update the last search register
	      var lastSearchRegister = vimGlobalState.registerController.getRegister('/');
	      lastSearchRegister.setText(query);
	      // Check if the query is already a regex.
	      if (query instanceof RegExp) { return query; }
	      // First try to extract regex + flags from the input. If no flags found,
	      // extract just the regex. IE does not accept flags directly defined in
	      // the regex string in the form /regex/flags
	      var slashes = findUnescapedSlashes(query);
	      var regexPart;
	      var forceIgnoreCase;
	      if (!slashes.length) {
	        // Query looks like 'regexp'
	        regexPart = query;
	      } else {
	        // Query looks like 'regexp/...'
	        regexPart = query.substring(0, slashes[0]);
	        var flagsPart = query.substring(slashes[0]);
	        forceIgnoreCase = (flagsPart.indexOf('i') != -1);
	      }
	      if (!regexPart) {
	        return null;
	      }
	      if (!getOption('pcre')) {
	        regexPart = translateRegex(regexPart);
	      }
	      if (smartCase) {
	        ignoreCase = (/^[^A-Z]*$/).test(regexPart);
	      }
	      var regexp = new RegExp(regexPart,
	          (ignoreCase || forceIgnoreCase) ? 'i' : undefined);
	      return regexp;
	    }
	    function showConfirm(cm, text) {
	      if (cm.openNotification) {
	        cm.openNotification('<span style="color: red">' + text + '</span>',
	                            {bottom: true, duration: 5000});
	      } else {
	        alert(text);
	      }
	    }
	    function makePrompt(prefix, desc) {
	      var raw = '<span style="font-family: monospace; white-space: pre">' +
	          (prefix || "") + '<input type="text"></span>';
	      if (desc)
	        raw += ' <span style="color: #888">' + desc + '</span>';
	      return raw;
	    }
	    var searchPromptDesc = '(Javascript regexp)';
	    function showPrompt(cm, options) {
	      var shortText = (options.prefix || '') + ' ' + (options.desc || '');
	      var prompt = makePrompt(options.prefix, options.desc);
	      dialog(cm, prompt, shortText, options.onClose, options);
	    }
	    function regexEqual(r1, r2) {
	      if (r1 instanceof RegExp && r2 instanceof RegExp) {
	          var props = ['global', 'multiline', 'ignoreCase', 'source'];
	          for (var i = 0; i < props.length; i++) {
	              var prop = props[i];
	              if (r1[prop] !== r2[prop]) {
	                  return false;
	              }
	          }
	          return true;
	      }
	      return false;
	    }
	    // Returns true if the query is valid.
	    function updateSearchQuery(cm, rawQuery, ignoreCase, smartCase) {
	      if (!rawQuery) {
	        return;
	      }
	      var state = getSearchState(cm);
	      var query = parseQuery(rawQuery, !!ignoreCase, !!smartCase);
	      if (!query) {
	        return;
	      }
	      highlightSearchMatches(cm, query);
	      if (regexEqual(query, state.getQuery())) {
	        return query;
	      }
	      state.setQuery(query);
	      return query;
	    }
	    function searchOverlay(query) {
	      if (query.source.charAt(0) == '^') {
	        var matchSol = true;
	      }
	      return {
	        token: function(stream) {
	          if (matchSol && !stream.sol()) {
	            stream.skipToEnd();
	            return;
	          }
	          var match = stream.match(query, false);
	          if (match) {
	            if (match[0].length == 0) {
	              // Matched empty string, skip to next.
	              stream.next();
	              return 'searching';
	            }
	            if (!stream.sol()) {
	              // Backtrack 1 to match \b
	              stream.backUp(1);
	              if (!query.exec(stream.next() + match[0])) {
	                stream.next();
	                return null;
	              }
	            }
	            stream.match(query);
	            return 'searching';
	          }
	          while (!stream.eol()) {
	            stream.next();
	            if (stream.match(query, false)) break;
	          }
	        },
	        query: query
	      };
	    }
	    function highlightSearchMatches(cm, query) {
	      var searchState = getSearchState(cm);
	      var overlay = searchState.getOverlay();
	      if (!overlay || query != overlay.query) {
	        if (overlay) {
	          cm.removeOverlay(overlay);
	        }
	        overlay = searchOverlay(query);
	        cm.addOverlay(overlay);
	        if (cm.showMatchesOnScrollbar) {
	          if (searchState.getScrollbarAnnotate()) {
	            searchState.getScrollbarAnnotate().clear();
	          }
	          searchState.setScrollbarAnnotate(cm.showMatchesOnScrollbar(query));
	        }
	        searchState.setOverlay(overlay);
	      }
	    }
	    function findNext(cm, prev, query, repeat) {
	      if (repeat === undefined) { repeat = 1; }
	      return cm.operation(function() {
	        var pos = cm.getCursor();
	        var cursor = cm.getSearchCursor(query, pos);
	        for (var i = 0; i < repeat; i++) {
	          var found = cursor.find(prev);
	          if (i == 0 && found && cursorEqual(cursor.from(), pos)) { found = cursor.find(prev); }
	          if (!found) {
	            // SearchCursor may have returned null because it hit EOF, wrap
	            // around and try again.
	            cursor = cm.getSearchCursor(query,
	                (prev) ? Pos(cm.lastLine()) : Pos(cm.firstLine(), 0) );
	            if (!cursor.find(prev)) {
	              return;
	            }
	          }
	        }
	        return cursor.from();
	      });
	    }
	    function clearSearchHighlight(cm) {
	      var state = getSearchState(cm);
	      cm.removeOverlay(getSearchState(cm).getOverlay());
	      state.setOverlay(null);
	      if (state.getScrollbarAnnotate()) {
	        state.getScrollbarAnnotate().clear();
	        state.setScrollbarAnnotate(null);
	      }
	    }
	    /**
	     * Check if pos is in the specified range, INCLUSIVE.
	     * Range can be specified with 1 or 2 arguments.
	     * If the first range argument is an array, treat it as an array of line
	     * numbers. Match pos against any of the lines.
	     * If the first range argument is a number,
	     *   if there is only 1 range argument, check if pos has the same line
	     *       number
	     *   if there are 2 range arguments, then check if pos is in between the two
	     *       range arguments.
	     */
	    function isInRange(pos, start, end) {
	      if (typeof pos != 'number') {
	        // Assume it is a cursor position. Get the line number.
	        pos = pos.line;
	      }
	      if (start instanceof Array) {
	        return inArray(pos, start);
	      } else {
	        if (end) {
	          return (pos >= start && pos <= end);
	        } else {
	          return pos == start;
	        }
	      }
	    }
	    function getUserVisibleLines(cm) {
	      var scrollInfo = cm.getScrollInfo();
	      var occludeToleranceTop = 6;
	      var occludeToleranceBottom = 10;
	      var from = cm.coordsChar({left:0, top: occludeToleranceTop + scrollInfo.top}, 'local');
	      var bottomY = scrollInfo.clientHeight - occludeToleranceBottom + scrollInfo.top;
	      var to = cm.coordsChar({left:0, top: bottomY}, 'local');
	      return {top: from.line, bottom: to.line};
	    }

	    function getMarkPos(cm, vim, markName) {
	      if (markName == '\'') {
	        var history = cm.doc.history.done;
	        var event = history[history.length - 2];
	        return event && event.ranges && event.ranges[0].head;
	      } else if (markName == '.') {
	        if (cm.doc.history.lastModTime == 0) {
	          return  // If no changes, bail out; don't bother to copy or reverse history array.
	        } else {
	          var changeHistory = cm.doc.history.done.filter(function(el){ if (el.changes !== undefined) { return el } });
	          changeHistory.reverse();
	          var lastEditPos = changeHistory[0].changes[0].to;
	        }
	        return lastEditPos;
	      }

	      var mark = vim.marks[markName];
	      return mark && mark.find();
	    }

	    var ExCommandDispatcher = function() {
	      this.buildCommandMap_();
	    };
	    ExCommandDispatcher.prototype = {
	      processCommand: function(cm, input, opt_params) {
	        var that = this;
	        cm.operation(function () {
	          cm.curOp.isVimOp = true;
	          that._processCommand(cm, input, opt_params);
	        });
	      },
	      _processCommand: function(cm, input, opt_params) {
	        var vim = cm.state.vim;
	        var commandHistoryRegister = vimGlobalState.registerController.getRegister(':');
	        var previousCommand = commandHistoryRegister.toString();
	        if (vim.visualMode) {
	          exitVisualMode(cm);
	        }
	        var inputStream = new CodeMirror.StringStream(input);
	        // update ": with the latest command whether valid or invalid
	        commandHistoryRegister.setText(input);
	        var params = opt_params || {};
	        params.input = input;
	        try {
	          this.parseInput_(cm, inputStream, params);
	        } catch(e) {
	          showConfirm(cm, e);
	          throw e;
	        }
	        var command;
	        var commandName;
	        if (!params.commandName) {
	          // If only a line range is defined, move to the line.
	          if (params.line !== undefined) {
	            commandName = 'move';
	          }
	        } else {
	          command = this.matchCommand_(params.commandName);
	          if (command) {
	            commandName = command.name;
	            if (command.excludeFromCommandHistory) {
	              commandHistoryRegister.setText(previousCommand);
	            }
	            this.parseCommandArgs_(inputStream, params, command);
	            if (command.type == 'exToKey') {
	              // Handle Ex to Key mapping.
	              for (var i = 0; i < command.toKeys.length; i++) {
	                CodeMirror.Vim.handleKey(cm, command.toKeys[i], 'mapping');
	              }
	              return;
	            } else if (command.type == 'exToEx') {
	              // Handle Ex to Ex mapping.
	              this.processCommand(cm, command.toInput);
	              return;
	            }
	          }
	        }
	        if (!commandName) {
	          showConfirm(cm, 'Not an editor command ":' + input + '"');
	          return;
	        }
	        try {
	          exCommands[commandName](cm, params);
	          // Possibly asynchronous commands (e.g. substitute, which might have a
	          // user confirmation), are responsible for calling the callback when
	          // done. All others have it taken care of for them here.
	          if ((!command || !command.possiblyAsync) && params.callback) {
	            params.callback();
	          }
	        } catch(e) {
	          showConfirm(cm, e);
	          throw e;
	        }
	      },
	      parseInput_: function(cm, inputStream, result) {
	        inputStream.eatWhile(':');
	        // Parse range.
	        if (inputStream.eat('%')) {
	          result.line = cm.firstLine();
	          result.lineEnd = cm.lastLine();
	        } else {
	          result.line = this.parseLineSpec_(cm, inputStream);
	          if (result.line !== undefined && inputStream.eat(',')) {
	            result.lineEnd = this.parseLineSpec_(cm, inputStream);
	          }
	        }

	        // Parse command name.
	        var commandMatch = inputStream.match(/^(\w+)/);
	        if (commandMatch) {
	          result.commandName = commandMatch[1];
	        } else {
	          result.commandName = inputStream.match(/.*/)[0];
	        }

	        return result;
	      },
	      parseLineSpec_: function(cm, inputStream) {
	        var numberMatch = inputStream.match(/^(\d+)/);
	        if (numberMatch) {
	          // Absolute line number plus offset (N+M or N-M) is probably a typo,
	          // not something the user actually wanted. (NB: vim does allow this.)
	          return parseInt(numberMatch[1], 10) - 1;
	        }
	        switch (inputStream.next()) {
	          case '.':
	            return this.parseLineSpecOffset_(inputStream, cm.getCursor().line);
	          case '$':
	            return this.parseLineSpecOffset_(inputStream, cm.lastLine());
	          case '\'':
	            var markName = inputStream.next();
	            var markPos = getMarkPos(cm, cm.state.vim, markName);
	            if (!markPos) throw new Error('Mark not set');
	            return this.parseLineSpecOffset_(inputStream, markPos.line);
	          case '-':
	          case '+':
	            inputStream.backUp(1);
	            // Offset is relative to current line if not otherwise specified.
	            return this.parseLineSpecOffset_(inputStream, cm.getCursor().line);
	          default:
	            inputStream.backUp(1);
	            return undefined;
	        }
	      },
	      parseLineSpecOffset_: function(inputStream, line) {
	        var offsetMatch = inputStream.match(/^([+-])?(\d+)/);
	        if (offsetMatch) {
	          var offset = parseInt(offsetMatch[2], 10);
	          if (offsetMatch[1] == "-") {
	            line -= offset;
	          } else {
	            line += offset;
	          }
	        }
	        return line;
	      },
	      parseCommandArgs_: function(inputStream, params, command) {
	        if (inputStream.eol()) {
	          return;
	        }
	        params.argString = inputStream.match(/.*/)[0];
	        // Parse command-line arguments
	        var delim = command.argDelimiter || /\s+/;
	        var args = trim(params.argString).split(delim);
	        if (args.length && args[0]) {
	          params.args = args;
	        }
	      },
	      matchCommand_: function(commandName) {
	        // Return the command in the command map that matches the shortest
	        // prefix of the passed in command name. The match is guaranteed to be
	        // unambiguous if the defaultExCommandMap's shortNames are set up
	        // correctly. (see @code{defaultExCommandMap}).
	        for (var i = commandName.length; i > 0; i--) {
	          var prefix = commandName.substring(0, i);
	          if (this.commandMap_[prefix]) {
	            var command = this.commandMap_[prefix];
	            if (command.name.indexOf(commandName) === 0) {
	              return command;
	            }
	          }
	        }
	        return null;
	      },
	      buildCommandMap_: function() {
	        this.commandMap_ = {};
	        for (var i = 0; i < defaultExCommandMap.length; i++) {
	          var command = defaultExCommandMap[i];
	          var key = command.shortName || command.name;
	          this.commandMap_[key] = command;
	        }
	      },
	      map: function(lhs, rhs, ctx) {
	        if (lhs != ':' && lhs.charAt(0) == ':') {
	          if (ctx) { throw Error('Mode not supported for ex mappings'); }
	          var commandName = lhs.substring(1);
	          if (rhs != ':' && rhs.charAt(0) == ':') {
	            // Ex to Ex mapping
	            this.commandMap_[commandName] = {
	              name: commandName,
	              type: 'exToEx',
	              toInput: rhs.substring(1),
	              user: true
	            };
	          } else {
	            // Ex to key mapping
	            this.commandMap_[commandName] = {
	              name: commandName,
	              type: 'exToKey',
	              toKeys: rhs,
	              user: true
	            };
	          }
	        } else {
	          if (rhs != ':' && rhs.charAt(0) == ':') {
	            // Key to Ex mapping.
	            var mapping = {
	              keys: lhs,
	              type: 'keyToEx',
	              exArgs: { input: rhs.substring(1) }
	            };
	            if (ctx) { mapping.context = ctx; }
	            defaultKeymap.unshift(mapping);
	          } else {
	            // Key to key mapping
	            var mapping = {
	              keys: lhs,
	              type: 'keyToKey',
	              toKeys: rhs
	            };
	            if (ctx) { mapping.context = ctx; }
	            defaultKeymap.unshift(mapping);
	          }
	        }
	      },
	      unmap: function(lhs, ctx) {
	        if (lhs != ':' && lhs.charAt(0) == ':') {
	          // Ex to Ex or Ex to key mapping
	          if (ctx) { throw Error('Mode not supported for ex mappings'); }
	          var commandName = lhs.substring(1);
	          if (this.commandMap_[commandName] && this.commandMap_[commandName].user) {
	            delete this.commandMap_[commandName];
	            return;
	          }
	        } else {
	          // Key to Ex or key to key mapping
	          var keys = lhs;
	          for (var i = 0; i < defaultKeymap.length; i++) {
	            if (keys == defaultKeymap[i].keys
	                && defaultKeymap[i].context === ctx) {
	              defaultKeymap.splice(i, 1);
	              return;
	            }
	          }
	        }
	        throw Error('No such mapping.');
	      }
	    };

	    var exCommands = {
	      colorscheme: function(cm, params) {
	        if (!params.args || params.args.length < 1) {
	          showConfirm(cm, cm.getOption('theme'));
	          return;
	        }
	        cm.setOption('theme', params.args[0]);
	      },
	      map: function(cm, params, ctx) {
	        var mapArgs = params.args;
	        if (!mapArgs || mapArgs.length < 2) {
	          if (cm) {
	            showConfirm(cm, 'Invalid mapping: ' + params.input);
	          }
	          return;
	        }
	        exCommandDispatcher.map(mapArgs[0], mapArgs[1], ctx);
	      },
	      imap: function(cm, params) { this.map(cm, params, 'insert'); },
	      nmap: function(cm, params) { this.map(cm, params, 'normal'); },
	      vmap: function(cm, params) { this.map(cm, params, 'visual'); },
	      unmap: function(cm, params, ctx) {
	        var mapArgs = params.args;
	        if (!mapArgs || mapArgs.length < 1) {
	          if (cm) {
	            showConfirm(cm, 'No such mapping: ' + params.input);
	          }
	          return;
	        }
	        exCommandDispatcher.unmap(mapArgs[0], ctx);
	      },
	      move: function(cm, params) {
	        commandDispatcher.processCommand(cm, cm.state.vim, {
	            type: 'motion',
	            motion: 'moveToLineOrEdgeOfDocument',
	            motionArgs: { forward: false, explicitRepeat: true,
	              linewise: true },
	            repeatOverride: params.line+1});
	      },
	      set: function(cm, params) {
	        var setArgs = params.args;
	        // Options passed through to the setOption/getOption calls. May be passed in by the
	        // local/global versions of the set command
	        var setCfg = params.setCfg || {};
	        if (!setArgs || setArgs.length < 1) {
	          if (cm) {
	            showConfirm(cm, 'Invalid mapping: ' + params.input);
	          }
	          return;
	        }
	        var expr = setArgs[0].split('=');
	        var optionName = expr[0];
	        var value = expr[1];
	        var forceGet = false;

	        if (optionName.charAt(optionName.length - 1) == '?') {
	          // If post-fixed with ?, then the set is actually a get.
	          if (value) { throw Error('Trailing characters: ' + params.argString); }
	          optionName = optionName.substring(0, optionName.length - 1);
	          forceGet = true;
	        }
	        if (value === undefined && optionName.substring(0, 2) == 'no') {
	          // To set boolean options to false, the option name is prefixed with
	          // 'no'.
	          optionName = optionName.substring(2);
	          value = false;
	        }

	        var optionIsBoolean = options[optionName] && options[optionName].type == 'boolean';
	        if (optionIsBoolean && value == undefined) {
	          // Calling set with a boolean option sets it to true.
	          value = true;
	        }
	        // If no value is provided, then we assume this is a get.
	        if (!optionIsBoolean && value === undefined || forceGet) {
	          var oldValue = getOption(optionName, cm, setCfg);
	          if (oldValue instanceof Error) {
	            showConfirm(cm, oldValue.message);
	          } else if (oldValue === true || oldValue === false) {
	            showConfirm(cm, ' ' + (oldValue ? '' : 'no') + optionName);
	          } else {
	            showConfirm(cm, '  ' + optionName + '=' + oldValue);
	          }
	        } else {
	          var setOptionReturn = setOption(optionName, value, cm, setCfg);
	          if (setOptionReturn instanceof Error) {
	            showConfirm(cm, setOptionReturn.message);
	          }
	        }
	      },
	      setlocal: function (cm, params) {
	        // setCfg is passed through to setOption
	        params.setCfg = {scope: 'local'};
	        this.set(cm, params);
	      },
	      setglobal: function (cm, params) {
	        // setCfg is passed through to setOption
	        params.setCfg = {scope: 'global'};
	        this.set(cm, params);
	      },
	      registers: function(cm, params) {
	        var regArgs = params.args;
	        var registers = vimGlobalState.registerController.registers;
	        var regInfo = '----------Registers----------<br><br>';
	        if (!regArgs) {
	          for (var registerName in registers) {
	            var text = registers[registerName].toString();
	            if (text.length) {
	              regInfo += '"' + registerName + '    ' + text + '<br>';
	            }
	          }
	        } else {
	          var registerName;
	          regArgs = regArgs.join('');
	          for (var i = 0; i < regArgs.length; i++) {
	            registerName = regArgs.charAt(i);
	            if (!vimGlobalState.registerController.isValidRegister(registerName)) {
	              continue;
	            }
	            var register = registers[registerName] || new Register();
	            regInfo += '"' + registerName + '    ' + register.toString() + '<br>';
	          }
	        }
	        showConfirm(cm, regInfo);
	      },
	      sort: function(cm, params) {
	        var reverse, ignoreCase, unique, number, pattern;
	        function parseArgs() {
	          if (params.argString) {
	            var args = new CodeMirror.StringStream(params.argString);
	            if (args.eat('!')) { reverse = true; }
	            if (args.eol()) { return; }
	            if (!args.eatSpace()) { return 'Invalid arguments'; }
	            var opts = args.match(/([dinuox]+)?\s*(\/.+\/)?\s*/);
	            if (!opts && !args.eol()) { return 'Invalid arguments'; }
	            if (opts[1]) {
	              ignoreCase = opts[1].indexOf('i') != -1;
	              unique = opts[1].indexOf('u') != -1;
	              var decimal = opts[1].indexOf('d') != -1 || opts[1].indexOf('n') != -1 && 1;
	              var hex = opts[1].indexOf('x') != -1 && 1;
	              var octal = opts[1].indexOf('o') != -1 && 1;
	              if (decimal + hex + octal > 1) { return 'Invalid arguments'; }
	              number = decimal && 'decimal' || hex && 'hex' || octal && 'octal';
	            }
	            if (opts[2]) {
	              pattern = new RegExp(opts[2].substr(1, opts[2].length - 2), ignoreCase ? 'i' : '');
	            }
	          }
	        }
	        var err = parseArgs();
	        if (err) {
	          showConfirm(cm, err + ': ' + params.argString);
	          return;
	        }
	        var lineStart = params.line || cm.firstLine();
	        var lineEnd = params.lineEnd || params.line || cm.lastLine();
	        if (lineStart == lineEnd) { return; }
	        var curStart = Pos(lineStart, 0);
	        var curEnd = Pos(lineEnd, lineLength(cm, lineEnd));
	        var text = cm.getRange(curStart, curEnd).split('\n');
	        var numberRegex = pattern ? pattern :
	           (number == 'decimal') ? /(-?)([\d]+)/ :
	           (number == 'hex') ? /(-?)(?:0x)?([0-9a-f]+)/i :
	           (number == 'octal') ? /([0-7]+)/ : null;
	        var radix = (number == 'decimal') ? 10 : (number == 'hex') ? 16 : (number == 'octal') ? 8 : null;
	        var numPart = [], textPart = [];
	        if (number || pattern) {
	          for (var i = 0; i < text.length; i++) {
	            var matchPart = pattern ? text[i].match(pattern) : null;
	            if (matchPart && matchPart[0] != '') {
	              numPart.push(matchPart);
	            } else if (!pattern && numberRegex.exec(text[i])) {
	              numPart.push(text[i]);
	            } else {
	              textPart.push(text[i]);
	            }
	          }
	        } else {
	          textPart = text;
	        }
	        function compareFn(a, b) {
	          if (reverse) { var tmp; tmp = a; a = b; b = tmp; }
	          if (ignoreCase) { a = a.toLowerCase(); b = b.toLowerCase(); }
	          var anum = number && numberRegex.exec(a);
	          var bnum = number && numberRegex.exec(b);
	          if (!anum) { return a < b ? -1 : 1; }
	          anum = parseInt((anum[1] + anum[2]).toLowerCase(), radix);
	          bnum = parseInt((bnum[1] + bnum[2]).toLowerCase(), radix);
	          return anum - bnum;
	        }
	        function comparePatternFn(a, b) {
	          if (reverse) { var tmp; tmp = a; a = b; b = tmp; }
	          if (ignoreCase) { a[0] = a[0].toLowerCase(); b[0] = b[0].toLowerCase(); }
	          return (a[0] < b[0]) ? -1 : 1;
	        }
	        numPart.sort(pattern ? comparePatternFn : compareFn);
	        if (pattern) {
	          for (var i = 0; i < numPart.length; i++) {
	            numPart[i] = numPart[i].input;
	          }
	        } else if (!number) { textPart.sort(compareFn); }
	        text = (!reverse) ? textPart.concat(numPart) : numPart.concat(textPart);
	        if (unique) { // Remove duplicate lines
	          var textOld = text;
	          var lastLine;
	          text = [];
	          for (var i = 0; i < textOld.length; i++) {
	            if (textOld[i] != lastLine) {
	              text.push(textOld[i]);
	            }
	            lastLine = textOld[i];
	          }
	        }
	        cm.replaceRange(text.join('\n'), curStart, curEnd);
	      },
	      global: function(cm, params) {
	        // a global command is of the form
	        // :[range]g/pattern/[cmd]
	        // argString holds the string /pattern/[cmd]
	        var argString = params.argString;
	        if (!argString) {
	          showConfirm(cm, 'Regular Expression missing from global');
	          return;
	        }
	        // range is specified here
	        var lineStart = (params.line !== undefined) ? params.line : cm.firstLine();
	        var lineEnd = params.lineEnd || params.line || cm.lastLine();
	        // get the tokens from argString
	        var tokens = splitBySlash(argString);
	        var regexPart = argString, cmd;
	        if (tokens.length) {
	          regexPart = tokens[0];
	          cmd = tokens.slice(1, tokens.length).join('/');
	        }
	        if (regexPart) {
	          // If regex part is empty, then use the previous query. Otherwise
	          // use the regex part as the new query.
	          try {
	           updateSearchQuery(cm, regexPart, true /** ignoreCase */,
	             true /** smartCase */);
	          } catch (e) {
	           showConfirm(cm, 'Invalid regex: ' + regexPart);
	           return;
	          }
	        }
	        // now that we have the regexPart, search for regex matches in the
	        // specified range of lines
	        var query = getSearchState(cm).getQuery();
	        var matchedLines = [], content = '';
	        for (var i = lineStart; i <= lineEnd; i++) {
	          var matched = query.test(cm.getLine(i));
	          if (matched) {
	            matchedLines.push(i+1);
	            content+= cm.getLine(i) + '<br>';
	          }
	        }
	        // if there is no [cmd], just display the list of matched lines
	        if (!cmd) {
	          showConfirm(cm, content);
	          return;
	        }
	        var index = 0;
	        var nextCommand = function() {
	          if (index < matchedLines.length) {
	            var command = matchedLines[index] + cmd;
	            exCommandDispatcher.processCommand(cm, command, {
	              callback: nextCommand
	            });
	          }
	          index++;
	        };
	        nextCommand();
	      },
	      substitute: function(cm, params) {
	        if (!cm.getSearchCursor) {
	          throw new Error('Search feature not available. Requires searchcursor.js or ' +
	              'any other getSearchCursor implementation.');
	        }
	        var argString = params.argString;
	        var tokens = argString ? splitBySlash(argString) : [];
	        var regexPart, replacePart = '', trailing, flagsPart, count;
	        var confirm = false; // Whether to confirm each replace.
	        var global = false; // True to replace all instances on a line, false to replace only 1.
	        if (tokens.length) {
	          regexPart = tokens[0];
	          replacePart = tokens[1];
	          if (regexPart && regexPart[regexPart.length - 1] === '$') {
	            regexPart = regexPart.slice(0, regexPart.length - 1) + '\\n';
	            replacePart = replacePart ? replacePart + '\n' : '\n';
	          }
	          if (replacePart !== undefined) {
	            if (getOption('pcre')) {
	              replacePart = unescapeRegexReplace(replacePart);
	            } else {
	              replacePart = translateRegexReplace(replacePart);
	            }
	            vimGlobalState.lastSubstituteReplacePart = replacePart;
	          }
	          trailing = tokens[2] ? tokens[2].split(' ') : [];
	        } else {
	          // either the argString is empty or its of the form ' hello/world'
	          // actually splitBySlash returns a list of tokens
	          // only if the string starts with a '/'
	          if (argString && argString.length) {
	            showConfirm(cm, 'Substitutions should be of the form ' +
	                ':s/pattern/replace/');
	            return;
	          }
	        }
	        // After the 3rd slash, we can have flags followed by a space followed
	        // by count.
	        if (trailing) {
	          flagsPart = trailing[0];
	          count = parseInt(trailing[1]);
	          if (flagsPart) {
	            if (flagsPart.indexOf('c') != -1) {
	              confirm = true;
	              flagsPart.replace('c', '');
	            }
	            if (flagsPart.indexOf('g') != -1) {
	              global = true;
	              flagsPart.replace('g', '');
	            }
	            regexPart = regexPart + '/' + flagsPart;
	          }
	        }
	        if (regexPart) {
	          // If regex part is empty, then use the previous query. Otherwise use
	          // the regex part as the new query.
	          try {
	            updateSearchQuery(cm, regexPart, true /** ignoreCase */,
	              true /** smartCase */);
	          } catch (e) {
	            showConfirm(cm, 'Invalid regex: ' + regexPart);
	            return;
	          }
	        }
	        replacePart = replacePart || vimGlobalState.lastSubstituteReplacePart;
	        if (replacePart === undefined) {
	          showConfirm(cm, 'No previous substitute regular expression');
	          return;
	        }
	        var state = getSearchState(cm);
	        var query = state.getQuery();
	        var lineStart = (params.line !== undefined) ? params.line : cm.getCursor().line;
	        var lineEnd = params.lineEnd || lineStart;
	        if (lineStart == cm.firstLine() && lineEnd == cm.lastLine()) {
	          lineEnd = Infinity;
	        }
	        if (count) {
	          lineStart = lineEnd;
	          lineEnd = lineStart + count - 1;
	        }
	        var startPos = clipCursorToContent(cm, Pos(lineStart, 0));
	        var cursor = cm.getSearchCursor(query, startPos);
	        doReplace(cm, confirm, global, lineStart, lineEnd, cursor, query, replacePart, params.callback);
	      },
	      redo: CodeMirror.commands.redo,
	      undo: CodeMirror.commands.undo,
	      write: function(cm) {
	        if (CodeMirror.commands.save) {
	          // If a save command is defined, call it.
	          CodeMirror.commands.save(cm);
	        } else if (cm.save) {
	          // Saves to text area if no save command is defined and cm.save() is available.
	          cm.save();
	        }
	      },
	      nohlsearch: function(cm) {
	        clearSearchHighlight(cm);
	      },
	      yank: function (cm) {
	        var cur = copyCursor(cm.getCursor());
	        var line = cur.line;
	        var lineText = cm.getLine(line);
	        vimGlobalState.registerController.pushText(
	          '0', 'yank', lineText, true, true);
	      },
	      delmarks: function(cm, params) {
	        if (!params.argString || !trim(params.argString)) {
	          showConfirm(cm, 'Argument required');
	          return;
	        }

	        var state = cm.state.vim;
	        var stream = new CodeMirror.StringStream(trim(params.argString));
	        while (!stream.eol()) {
	          stream.eatSpace();

	          // Record the streams position at the beginning of the loop for use
	          // in error messages.
	          var count = stream.pos;

	          if (!stream.match(/[a-zA-Z]/, false)) {
	            showConfirm(cm, 'Invalid argument: ' + params.argString.substring(count));
	            return;
	          }

	          var sym = stream.next();
	          // Check if this symbol is part of a range
	          if (stream.match('-', true)) {
	            // This symbol is part of a range.

	            // The range must terminate at an alphabetic character.
	            if (!stream.match(/[a-zA-Z]/, false)) {
	              showConfirm(cm, 'Invalid argument: ' + params.argString.substring(count));
	              return;
	            }

	            var startMark = sym;
	            var finishMark = stream.next();
	            // The range must terminate at an alphabetic character which
	            // shares the same case as the start of the range.
	            if (isLowerCase(startMark) && isLowerCase(finishMark) ||
	                isUpperCase(startMark) && isUpperCase(finishMark)) {
	              var start = startMark.charCodeAt(0);
	              var finish = finishMark.charCodeAt(0);
	              if (start >= finish) {
	                showConfirm(cm, 'Invalid argument: ' + params.argString.substring(count));
	                return;
	              }

	              // Because marks are always ASCII values, and we have
	              // determined that they are the same case, we can use
	              // their char codes to iterate through the defined range.
	              for (var j = 0; j <= finish - start; j++) {
	                var mark = String.fromCharCode(start + j);
	                delete state.marks[mark];
	              }
	            } else {
	              showConfirm(cm, 'Invalid argument: ' + startMark + '-');
	              return;
	            }
	          } else {
	            // This symbol is a valid mark, and is not part of a range.
	            delete state.marks[sym];
	          }
	        }
	      }
	    };

	    var exCommandDispatcher = new ExCommandDispatcher();

	    /**
	    * @param {CodeMirror} cm CodeMirror instance we are in.
	    * @param {boolean} confirm Whether to confirm each replace.
	    * @param {Cursor} lineStart Line to start replacing from.
	    * @param {Cursor} lineEnd Line to stop replacing at.
	    * @param {RegExp} query Query for performing matches with.
	    * @param {string} replaceWith Text to replace matches with. May contain $1,
	    *     $2, etc for replacing captured groups using Javascript replace.
	    * @param {function()} callback A callback for when the replace is done.
	    */
	    function doReplace(cm, confirm, global, lineStart, lineEnd, searchCursor, query,
	        replaceWith, callback) {
	      // Set up all the functions.
	      cm.state.vim.exMode = true;
	      var done = false;
	      var lastPos = searchCursor.from();
	      function replaceAll() {
	        cm.operation(function() {
	          while (!done) {
	            replace();
	            next();
	          }
	          stop();
	        });
	      }
	      function replace() {
	        var text = cm.getRange(searchCursor.from(), searchCursor.to());
	        var newText = text.replace(query, replaceWith);
	        searchCursor.replace(newText);
	      }
	      function next() {
	        // The below only loops to skip over multiple occurrences on the same
	        // line when 'global' is not true.
	        while(searchCursor.findNext() &&
	              isInRange(searchCursor.from(), lineStart, lineEnd)) {
	          if (!global && lastPos && searchCursor.from().line == lastPos.line) {
	            continue;
	          }
	          cm.scrollIntoView(searchCursor.from(), 30);
	          cm.setSelection(searchCursor.from(), searchCursor.to());
	          lastPos = searchCursor.from();
	          done = false;
	          return;
	        }
	        done = true;
	      }
	      function stop(close) {
	        if (close) { close(); }
	        cm.focus();
	        if (lastPos) {
	          cm.setCursor(lastPos);
	          var vim = cm.state.vim;
	          vim.exMode = false;
	          vim.lastHPos = vim.lastHSPos = lastPos.ch;
	        }
	        if (callback) { callback(); }
	      }
	      function onPromptKeyDown(e, _value, close) {
	        // Swallow all keys.
	        CodeMirror.e_stop(e);
	        var keyName = CodeMirror.keyName(e);
	        switch (keyName) {
	          case 'Y':
	            replace(); next(); break;
	          case 'N':
	            next(); break;
	          case 'A':
	            // replaceAll contains a call to close of its own. We don't want it
	            // to fire too early or multiple times.
	            var savedCallback = callback;
	            callback = undefined;
	            cm.operation(replaceAll);
	            callback = savedCallback;
	            break;
	          case 'L':
	            replace();
	            // fall through and exit.
	          case 'Q':
	          case 'Esc':
	          case 'Ctrl-C':
	          case 'Ctrl-[':
	            stop(close);
	            break;
	        }
	        if (done) { stop(close); }
	        return true;
	      }

	      // Actually do replace.
	      next();
	      if (done) {
	        showConfirm(cm, 'No matches for ' + query.source);
	        return;
	      }
	      if (!confirm) {
	        replaceAll();
	        if (callback) { callback(); };
	        return;
	      }
	      showPrompt(cm, {
	        prefix: 'replace with <strong>' + replaceWith + '</strong> (y/n/a/q/l)',
	        onKeyDown: onPromptKeyDown
	      });
	    }

	    CodeMirror.keyMap.vim = {
	      attach: attachVimMap,
	      detach: detachVimMap,
	      call: cmKey
	    };

	    function exitInsertMode(cm) {
	      var vim = cm.state.vim;
	      var macroModeState = vimGlobalState.macroModeState;
	      var insertModeChangeRegister = vimGlobalState.registerController.getRegister('.');
	      var isPlaying = macroModeState.isPlaying;
	      var lastChange = macroModeState.lastInsertModeChanges;
	      // In case of visual block, the insertModeChanges are not saved as a
	      // single word, so we convert them to a single word
	      // so as to update the ". register as expected in real vim.
	      var text = [];
	      if (!isPlaying) {
	        var selLength = lastChange.inVisualBlock && vim.lastSelection ?
	            vim.lastSelection.visualBlock.height : 1;
	        var changes = lastChange.changes;
	        var text = [];
	        var i = 0;
	        // In case of multiple selections in blockwise visual,
	        // the inserted text, for example: 'f<Backspace>oo', is stored as
	        // 'f', 'f', InsertModeKey 'o', 'o', 'o', 'o'. (if you have a block with 2 lines).
	        // We push the contents of the changes array as per the following:
	        // 1. In case of InsertModeKey, just increment by 1.
	        // 2. In case of a character, jump by selLength (2 in the example).
	        while (i < changes.length) {
	          // This loop will convert 'ff<bs>oooo' to 'f<bs>oo'.
	          text.push(changes[i]);
	          if (changes[i] instanceof InsertModeKey) {
	             i++;
	          } else {
	             i+= selLength;
	          }
	        }
	        lastChange.changes = text;
	        cm.off('change', onChange);
	        CodeMirror.off(cm.getInputField(), 'keydown', onKeyEventTargetKeyDown);
	      }
	      if (!isPlaying && vim.insertModeRepeat > 1) {
	        // Perform insert mode repeat for commands like 3,a and 3,o.
	        repeatLastEdit(cm, vim, vim.insertModeRepeat - 1,
	            true /** repeatForInsert */);
	        vim.lastEditInputState.repeatOverride = vim.insertModeRepeat;
	      }
	      delete vim.insertModeRepeat;
	      vim.insertMode = false;
	      cm.setCursor(cm.getCursor().line, cm.getCursor().ch-1);
	      cm.setOption('keyMap', 'vim');
	      cm.setOption('disableInput', true);
	      cm.toggleOverwrite(false); // exit replace mode if we were in it.
	      // update the ". register before exiting insert mode
	      insertModeChangeRegister.setText(lastChange.changes.join(''));
	      CodeMirror.signal(cm, "vim-mode-change", {mode: "normal"});
	      if (macroModeState.isRecording) {
	        logInsertModeChange(macroModeState);
	      }
	    }

	    function _mapCommand(command) {
	      defaultKeymap.unshift(command);
	    }

	    function mapCommand(keys, type, name, args, extra) {
	      var command = {keys: keys, type: type};
	      command[type] = name;
	      command[type + "Args"] = args;
	      for (var key in extra)
	        command[key] = extra[key];
	      _mapCommand(command);
	    }

	    // The timeout in milliseconds for the two-character ESC keymap should be
	    // adjusted according to your typing speed to prevent false positives.
	    defineOption('insertModeEscKeysTimeout', 200, 'number');

	    CodeMirror.keyMap['vim-insert'] = {
	      // TODO: override navigation keys so that Esc will cancel automatic
	      // indentation from o, O, i_<CR>
	      fallthrough: ['default'],
	      attach: attachVimMap,
	      detach: detachVimMap,
	      call: cmKey
	    };

	    CodeMirror.keyMap['vim-replace'] = {
	      'Backspace': 'goCharLeft',
	      fallthrough: ['vim-insert'],
	      attach: attachVimMap,
	      detach: detachVimMap,
	      call: cmKey
	    };

	    function executeMacroRegister(cm, vim, macroModeState, registerName) {
	      var register = vimGlobalState.registerController.getRegister(registerName);
	      if (registerName == ':') {
	        // Read-only register containing last Ex command.
	        if (register.keyBuffer[0]) {
	          exCommandDispatcher.processCommand(cm, register.keyBuffer[0]);
	        }
	        macroModeState.isPlaying = false;
	        return;
	      }
	      var keyBuffer = register.keyBuffer;
	      var imc = 0;
	      macroModeState.isPlaying = true;
	      macroModeState.replaySearchQueries = register.searchQueries.slice(0);
	      for (var i = 0; i < keyBuffer.length; i++) {
	        var text = keyBuffer[i];
	        var match, key;
	        while (text) {
	          // Pull off one command key, which is either a single character
	          // or a special sequence wrapped in '<' and '>', e.g. '<Space>'.
	          match = (/<\w+-.+?>|<\w+>|./).exec(text);
	          key = match[0];
	          text = text.substring(match.index + key.length);
	          CodeMirror.Vim.handleKey(cm, key, 'macro');
	          if (vim.insertMode) {
	            var changes = register.insertModeChanges[imc++].changes;
	            vimGlobalState.macroModeState.lastInsertModeChanges.changes =
	                changes;
	            repeatInsertModeChanges(cm, changes, 1);
	            exitInsertMode(cm);
	          }
	        }
	      };
	      macroModeState.isPlaying = false;
	    }

	    function logKey(macroModeState, key) {
	      if (macroModeState.isPlaying) { return; }
	      var registerName = macroModeState.latestRegister;
	      var register = vimGlobalState.registerController.getRegister(registerName);
	      if (register) {
	        register.pushText(key);
	      }
	    }

	    function logInsertModeChange(macroModeState) {
	      if (macroModeState.isPlaying) { return; }
	      var registerName = macroModeState.latestRegister;
	      var register = vimGlobalState.registerController.getRegister(registerName);
	      if (register && register.pushInsertModeChanges) {
	        register.pushInsertModeChanges(macroModeState.lastInsertModeChanges);
	      }
	    }

	    function logSearchQuery(macroModeState, query) {
	      if (macroModeState.isPlaying) { return; }
	      var registerName = macroModeState.latestRegister;
	      var register = vimGlobalState.registerController.getRegister(registerName);
	      if (register && register.pushSearchQuery) {
	        register.pushSearchQuery(query);
	      }
	    }

	    /**
	     * Listens for changes made in insert mode.
	     * Should only be active in insert mode.
	     */
	    function onChange(cm, changeObj) {
	      var macroModeState = vimGlobalState.macroModeState;
	      var lastChange = macroModeState.lastInsertModeChanges;
	      if (!macroModeState.isPlaying) {
	        while(changeObj) {
	          lastChange.expectCursorActivityForChange = true;
	          if (changeObj.origin == '+input' || changeObj.origin == 'paste'
	              || changeObj.origin === undefined /* only in testing */) {
	            var text = changeObj.text.join('\n');
	            if (lastChange.maybeReset) {
	              lastChange.changes = [];
	              lastChange.maybeReset = false;
	            }
	            if (cm.state.overwrite && !/\n/.test(text)) {
	                lastChange.changes.push([text]);
	            } else {
	                lastChange.changes.push(text);
	            }
	          }
	          // Change objects may be chained with next.
	          changeObj = changeObj.next;
	        }
	      }
	    }

	    /**
	    * Listens for any kind of cursor activity on CodeMirror.
	    */
	    function onCursorActivity(cm) {
	      var vim = cm.state.vim;
	      if (vim.insertMode) {
	        // Tracking cursor activity in insert mode (for macro support).
	        var macroModeState = vimGlobalState.macroModeState;
	        if (macroModeState.isPlaying) { return; }
	        var lastChange = macroModeState.lastInsertModeChanges;
	        if (lastChange.expectCursorActivityForChange) {
	          lastChange.expectCursorActivityForChange = false;
	        } else {
	          // Cursor moved outside the context of an edit. Reset the change.
	          lastChange.maybeReset = true;
	        }
	      } else if (!cm.curOp.isVimOp) {
	        handleExternalSelection(cm, vim);
	      }
	      if (vim.visualMode) {
	        updateFakeCursor(cm);
	      }
	    }
	    function updateFakeCursor(cm) {
	      var vim = cm.state.vim;
	      var from = clipCursorToContent(cm, copyCursor(vim.sel.head));
	      var to = offsetCursor(from, 0, 1);
	      if (vim.fakeCursor) {
	        vim.fakeCursor.clear();
	      }
	      vim.fakeCursor = cm.markText(from, to, {className: 'cm-animate-fat-cursor'});
	    }
	    function handleExternalSelection(cm, vim) {
	      var anchor = cm.getCursor('anchor');
	      var head = cm.getCursor('head');
	      // Enter or exit visual mode to match mouse selection.
	      if (vim.visualMode && !cm.somethingSelected()) {
	        exitVisualMode(cm, false);
	      } else if (!vim.visualMode && !vim.insertMode && cm.somethingSelected()) {
	        vim.visualMode = true;
	        vim.visualLine = false;
	        CodeMirror.signal(cm, "vim-mode-change", {mode: "visual"});
	      }
	      if (vim.visualMode) {
	        // Bind CodeMirror selection model to vim selection model.
	        // Mouse selections are considered visual characterwise.
	        var headOffset = !cursorIsBefore(head, anchor) ? -1 : 0;
	        var anchorOffset = cursorIsBefore(head, anchor) ? -1 : 0;
	        head = offsetCursor(head, 0, headOffset);
	        anchor = offsetCursor(anchor, 0, anchorOffset);
	        vim.sel = {
	          anchor: anchor,
	          head: head
	        };
	        updateMark(cm, vim, '<', cursorMin(head, anchor));
	        updateMark(cm, vim, '>', cursorMax(head, anchor));
	      } else if (!vim.insertMode) {
	        // Reset lastHPos if selection was modified by something outside of vim mode e.g. by mouse.
	        vim.lastHPos = cm.getCursor().ch;
	      }
	    }

	    /** Wrapper for special keys pressed in insert mode */
	    function InsertModeKey(keyName) {
	      this.keyName = keyName;
	    }

	    /**
	    * Handles raw key down events from the text area.
	    * - Should only be active in insert mode.
	    * - For recording deletes in insert mode.
	    */
	    function onKeyEventTargetKeyDown(e) {
	      var macroModeState = vimGlobalState.macroModeState;
	      var lastChange = macroModeState.lastInsertModeChanges;
	      var keyName = CodeMirror.keyName(e);
	      if (!keyName) { return; }
	      function onKeyFound() {
	        if (lastChange.maybeReset) {
	          lastChange.changes = [];
	          lastChange.maybeReset = false;
	        }
	        lastChange.changes.push(new InsertModeKey(keyName));
	        return true;
	      }
	      if (keyName.indexOf('Delete') != -1 || keyName.indexOf('Backspace') != -1) {
	        CodeMirror.lookupKey(keyName, 'vim-insert', onKeyFound);
	      }
	    }

	    /**
	     * Repeats the last edit, which includes exactly 1 command and at most 1
	     * insert. Operator and motion commands are read from lastEditInputState,
	     * while action commands are read from lastEditActionCommand.
	     *
	     * If repeatForInsert is true, then the function was called by
	     * exitInsertMode to repeat the insert mode changes the user just made. The
	     * corresponding enterInsertMode call was made with a count.
	     */
	    function repeatLastEdit(cm, vim, repeat, repeatForInsert) {
	      var macroModeState = vimGlobalState.macroModeState;
	      macroModeState.isPlaying = true;
	      var isAction = !!vim.lastEditActionCommand;
	      var cachedInputState = vim.inputState;
	      function repeatCommand() {
	        if (isAction) {
	          commandDispatcher.processAction(cm, vim, vim.lastEditActionCommand);
	        } else {
	          commandDispatcher.evalInput(cm, vim);
	        }
	      }
	      function repeatInsert(repeat) {
	        if (macroModeState.lastInsertModeChanges.changes.length > 0) {
	          // For some reason, repeat cw in desktop VIM does not repeat
	          // insert mode changes. Will conform to that behavior.
	          repeat = !vim.lastEditActionCommand ? 1 : repeat;
	          var changeObject = macroModeState.lastInsertModeChanges;
	          repeatInsertModeChanges(cm, changeObject.changes, repeat);
	        }
	      }
	      vim.inputState = vim.lastEditInputState;
	      if (isAction && vim.lastEditActionCommand.interlaceInsertRepeat) {
	        // o and O repeat have to be interlaced with insert repeats so that the
	        // insertions appear on separate lines instead of the last line.
	        for (var i = 0; i < repeat; i++) {
	          repeatCommand();
	          repeatInsert(1);
	        }
	      } else {
	        if (!repeatForInsert) {
	          // Hack to get the cursor to end up at the right place. If I is
	          // repeated in insert mode repeat, cursor will be 1 insert
	          // change set left of where it should be.
	          repeatCommand();
	        }
	        repeatInsert(repeat);
	      }
	      vim.inputState = cachedInputState;
	      if (vim.insertMode && !repeatForInsert) {
	        // Don't exit insert mode twice. If repeatForInsert is set, then we
	        // were called by an exitInsertMode call lower on the stack.
	        exitInsertMode(cm);
	      }
	      macroModeState.isPlaying = false;
	    };

	    function repeatInsertModeChanges(cm, changes, repeat) {
	      function keyHandler(binding) {
	        if (typeof binding == 'string') {
	          CodeMirror.commands[binding](cm);
	        } else {
	          binding(cm);
	        }
	        return true;
	      }
	      var head = cm.getCursor('head');
	      var inVisualBlock = vimGlobalState.macroModeState.lastInsertModeChanges.inVisualBlock;
	      if (inVisualBlock) {
	        // Set up block selection again for repeating the changes.
	        var vim = cm.state.vim;
	        var lastSel = vim.lastSelection;
	        var offset = getOffset(lastSel.anchor, lastSel.head);
	        selectForInsert(cm, head, offset.line + 1);
	        repeat = cm.listSelections().length;
	        cm.setCursor(head);
	      }
	      for (var i = 0; i < repeat; i++) {
	        if (inVisualBlock) {
	          cm.setCursor(offsetCursor(head, i, 0));
	        }
	        for (var j = 0; j < changes.length; j++) {
	          var change = changes[j];
	          if (change instanceof InsertModeKey) {
	            CodeMirror.lookupKey(change.keyName, 'vim-insert', keyHandler);
	          } else if (typeof change == "string") {
	            var cur = cm.getCursor();
	            cm.replaceRange(change, cur, cur);
	          } else {
	            var start = cm.getCursor();
	            var end = offsetCursor(start, 0, change[0].length);
	            cm.replaceRange(change[0], start, end);
	          }
	        }
	      }
	      if (inVisualBlock) {
	        cm.setCursor(offsetCursor(head, 0, 1));
	      }
	    }

	    resetVimGlobalState();
	    return vimApi;
	  };
	  // Initialize Vim and make it available as an API.
	  CodeMirror.Vim = Vim();
	});


/***/ }),
/* 21 */
/***/ (function(module, exports, __webpack_require__) {

	// CodeMirror, copyright (c) by Marijn Haverbeke and others
	// Distributed under an MIT license: http://codemirror.net/LICENSE

	// A rough approximation of Sublime Text's keybindings
	// Depends on addon/search/searchcursor.js and optionally addon/dialog/dialogs.js

	(function(mod) {
	  if (true) // CommonJS
	    mod(__webpack_require__(2), __webpack_require__(3), __webpack_require__(5));
	  else if (typeof define == "function" && define.amd) // AMD
	    define(["../lib/codemirror", "../addon/search/searchcursor", "../addon/edit/matchbrackets"], mod);
	  else // Plain browser env
	    mod(CodeMirror);
	})(function(CodeMirror) {
	  "use strict";

	  var cmds = CodeMirror.commands;
	  var Pos = CodeMirror.Pos;

	  // This is not exactly Sublime's algorithm. I couldn't make heads or tails of that.
	  function findPosSubword(doc, start, dir) {
	    if (dir < 0 && start.ch == 0) return doc.clipPos(Pos(start.line - 1));
	    var line = doc.getLine(start.line);
	    if (dir > 0 && start.ch >= line.length) return doc.clipPos(Pos(start.line + 1, 0));
	    var state = "start", type;
	    for (var pos = start.ch, e = dir < 0 ? 0 : line.length, i = 0; pos != e; pos += dir, i++) {
	      var next = line.charAt(dir < 0 ? pos - 1 : pos);
	      var cat = next != "_" && CodeMirror.isWordChar(next) ? "w" : "o";
	      if (cat == "w" && next.toUpperCase() == next) cat = "W";
	      if (state == "start") {
	        if (cat != "o") { state = "in"; type = cat; }
	      } else if (state == "in") {
	        if (type != cat) {
	          if (type == "w" && cat == "W" && dir < 0) pos--;
	          if (type == "W" && cat == "w" && dir > 0) { type = "w"; continue; }
	          break;
	        }
	      }
	    }
	    return Pos(start.line, pos);
	  }

	  function moveSubword(cm, dir) {
	    cm.extendSelectionsBy(function(range) {
	      if (cm.display.shift || cm.doc.extend || range.empty())
	        return findPosSubword(cm.doc, range.head, dir);
	      else
	        return dir < 0 ? range.from() : range.to();
	    });
	  }

	  cmds.goSubwordLeft = function(cm) { moveSubword(cm, -1); };
	  cmds.goSubwordRight = function(cm) { moveSubword(cm, 1); };

	  cmds.scrollLineUp = function(cm) {
	    var info = cm.getScrollInfo();
	    if (!cm.somethingSelected()) {
	      var visibleBottomLine = cm.lineAtHeight(info.top + info.clientHeight, "local");
	      if (cm.getCursor().line >= visibleBottomLine)
	        cm.execCommand("goLineUp");
	    }
	    cm.scrollTo(null, info.top - cm.defaultTextHeight());
	  };
	  cmds.scrollLineDown = function(cm) {
	    var info = cm.getScrollInfo();
	    if (!cm.somethingSelected()) {
	      var visibleTopLine = cm.lineAtHeight(info.top, "local")+1;
	      if (cm.getCursor().line <= visibleTopLine)
	        cm.execCommand("goLineDown");
	    }
	    cm.scrollTo(null, info.top + cm.defaultTextHeight());
	  };

	  cmds.splitSelectionByLine = function(cm) {
	    var ranges = cm.listSelections(), lineRanges = [];
	    for (var i = 0; i < ranges.length; i++) {
	      var from = ranges[i].from(), to = ranges[i].to();
	      for (var line = from.line; line <= to.line; ++line)
	        if (!(to.line > from.line && line == to.line && to.ch == 0))
	          lineRanges.push({anchor: line == from.line ? from : Pos(line, 0),
	                           head: line == to.line ? to : Pos(line)});
	    }
	    cm.setSelections(lineRanges, 0);
	  };

	  cmds.singleSelectionTop = function(cm) {
	    var range = cm.listSelections()[0];
	    cm.setSelection(range.anchor, range.head, {scroll: false});
	  };

	  cmds.selectLine = function(cm) {
	    var ranges = cm.listSelections(), extended = [];
	    for (var i = 0; i < ranges.length; i++) {
	      var range = ranges[i];
	      extended.push({anchor: Pos(range.from().line, 0),
	                     head: Pos(range.to().line + 1, 0)});
	    }
	    cm.setSelections(extended);
	  };

	  function insertLine(cm, above) {
	    if (cm.isReadOnly()) return CodeMirror.Pass
	    cm.operation(function() {
	      var len = cm.listSelections().length, newSelection = [], last = -1;
	      for (var i = 0; i < len; i++) {
	        var head = cm.listSelections()[i].head;
	        if (head.line <= last) continue;
	        var at = Pos(head.line + (above ? 0 : 1), 0);
	        cm.replaceRange("\n", at, null, "+insertLine");
	        cm.indentLine(at.line, null, true);
	        newSelection.push({head: at, anchor: at});
	        last = head.line + 1;
	      }
	      cm.setSelections(newSelection);
	    });
	    cm.execCommand("indentAuto");
	  }

	  cmds.insertLineAfter = function(cm) { return insertLine(cm, false); };

	  cmds.insertLineBefore = function(cm) { return insertLine(cm, true); };

	  function wordAt(cm, pos) {
	    var start = pos.ch, end = start, line = cm.getLine(pos.line);
	    while (start && CodeMirror.isWordChar(line.charAt(start - 1))) --start;
	    while (end < line.length && CodeMirror.isWordChar(line.charAt(end))) ++end;
	    return {from: Pos(pos.line, start), to: Pos(pos.line, end), word: line.slice(start, end)};
	  }

	  cmds.selectNextOccurrence = function(cm) {
	    var from = cm.getCursor("from"), to = cm.getCursor("to");
	    var fullWord = cm.state.sublimeFindFullWord == cm.doc.sel;
	    if (CodeMirror.cmpPos(from, to) == 0) {
	      var word = wordAt(cm, from);
	      if (!word.word) return;
	      cm.setSelection(word.from, word.to);
	      fullWord = true;
	    } else {
	      var text = cm.getRange(from, to);
	      var query = fullWord ? new RegExp("\\b" + text + "\\b") : text;
	      var cur = cm.getSearchCursor(query, to);
	      var found = cur.findNext();
	      if (!found) {
	        cur = cm.getSearchCursor(query, Pos(cm.firstLine(), 0));
	        found = cur.findNext();
	      }
	      if (!found || isSelectedRange(cm.listSelections(), cur.from(), cur.to()))
	        return CodeMirror.Pass
	      cm.addSelection(cur.from(), cur.to());
	    }
	    if (fullWord)
	      cm.state.sublimeFindFullWord = cm.doc.sel;
	  };

	  function addCursorToSelection(cm, dir) {
	    var ranges = cm.listSelections(), newRanges = [];
	    for (var i = 0; i < ranges.length; i++) {
	      var range = ranges[i];
	      var newAnchor = cm.findPosV(range.anchor, dir, "line");
	      var newHead = cm.findPosV(range.head, dir, "line");
	      var newRange = {anchor: newAnchor, head: newHead};
	      newRanges.push(range);
	      newRanges.push(newRange);
	    }
	    cm.setSelections(newRanges);
	  }
	  cmds.addCursorToPrevLine = function(cm) { addCursorToSelection(cm, -1); };
	  cmds.addCursorToNextLine = function(cm) { addCursorToSelection(cm, 1); };

	  function isSelectedRange(ranges, from, to) {
	    for (var i = 0; i < ranges.length; i++)
	      if (ranges[i].from() == from && ranges[i].to() == to) return true
	    return false
	  }

	  var mirror = "(){}[]";
	  function selectBetweenBrackets(cm) {
	    var ranges = cm.listSelections(), newRanges = []
	    for (var i = 0; i < ranges.length; i++) {
	      var range = ranges[i], pos = range.head, opening = cm.scanForBracket(pos, -1);
	      if (!opening) return false;
	      for (;;) {
	        var closing = cm.scanForBracket(pos, 1);
	        if (!closing) return false;
	        if (closing.ch == mirror.charAt(mirror.indexOf(opening.ch) + 1)) {
	          newRanges.push({anchor: Pos(opening.pos.line, opening.pos.ch + 1),
	                          head: closing.pos});
	          break;
	        }
	        pos = Pos(closing.pos.line, closing.pos.ch + 1);
	      }
	    }
	    cm.setSelections(newRanges);
	    return true;
	  }

	  cmds.selectScope = function(cm) {
	    selectBetweenBrackets(cm) || cm.execCommand("selectAll");
	  };
	  cmds.selectBetweenBrackets = function(cm) {
	    if (!selectBetweenBrackets(cm)) return CodeMirror.Pass;
	  };

	  cmds.goToBracket = function(cm) {
	    cm.extendSelectionsBy(function(range) {
	      var next = cm.scanForBracket(range.head, 1);
	      if (next && CodeMirror.cmpPos(next.pos, range.head) != 0) return next.pos;
	      var prev = cm.scanForBracket(range.head, -1);
	      return prev && Pos(prev.pos.line, prev.pos.ch + 1) || range.head;
	    });
	  };

	  cmds.swapLineUp = function(cm) {
	    if (cm.isReadOnly()) return CodeMirror.Pass
	    var ranges = cm.listSelections(), linesToMove = [], at = cm.firstLine() - 1, newSels = [];
	    for (var i = 0; i < ranges.length; i++) {
	      var range = ranges[i], from = range.from().line - 1, to = range.to().line;
	      newSels.push({anchor: Pos(range.anchor.line - 1, range.anchor.ch),
	                    head: Pos(range.head.line - 1, range.head.ch)});
	      if (range.to().ch == 0 && !range.empty()) --to;
	      if (from > at) linesToMove.push(from, to);
	      else if (linesToMove.length) linesToMove[linesToMove.length - 1] = to;
	      at = to;
	    }
	    cm.operation(function() {
	      for (var i = 0; i < linesToMove.length; i += 2) {
	        var from = linesToMove[i], to = linesToMove[i + 1];
	        var line = cm.getLine(from);
	        cm.replaceRange("", Pos(from, 0), Pos(from + 1, 0), "+swapLine");
	        if (to > cm.lastLine())
	          cm.replaceRange("\n" + line, Pos(cm.lastLine()), null, "+swapLine");
	        else
	          cm.replaceRange(line + "\n", Pos(to, 0), null, "+swapLine");
	      }
	      cm.setSelections(newSels);
	      cm.scrollIntoView();
	    });
	  };

	  cmds.swapLineDown = function(cm) {
	    if (cm.isReadOnly()) return CodeMirror.Pass
	    var ranges = cm.listSelections(), linesToMove = [], at = cm.lastLine() + 1;
	    for (var i = ranges.length - 1; i >= 0; i--) {
	      var range = ranges[i], from = range.to().line + 1, to = range.from().line;
	      if (range.to().ch == 0 && !range.empty()) from--;
	      if (from < at) linesToMove.push(from, to);
	      else if (linesToMove.length) linesToMove[linesToMove.length - 1] = to;
	      at = to;
	    }
	    cm.operation(function() {
	      for (var i = linesToMove.length - 2; i >= 0; i -= 2) {
	        var from = linesToMove[i], to = linesToMove[i + 1];
	        var line = cm.getLine(from);
	        if (from == cm.lastLine())
	          cm.replaceRange("", Pos(from - 1), Pos(from), "+swapLine");
	        else
	          cm.replaceRange("", Pos(from, 0), Pos(from + 1, 0), "+swapLine");
	        cm.replaceRange(line + "\n", Pos(to, 0), null, "+swapLine");
	      }
	      cm.scrollIntoView();
	    });
	  };

	  cmds.toggleCommentIndented = function(cm) {
	    cm.toggleComment({ indent: true });
	  }

	  cmds.joinLines = function(cm) {
	    var ranges = cm.listSelections(), joined = [];
	    for (var i = 0; i < ranges.length; i++) {
	      var range = ranges[i], from = range.from();
	      var start = from.line, end = range.to().line;
	      while (i < ranges.length - 1 && ranges[i + 1].from().line == end)
	        end = ranges[++i].to().line;
	      joined.push({start: start, end: end, anchor: !range.empty() && from});
	    }
	    cm.operation(function() {
	      var offset = 0, ranges = [];
	      for (var i = 0; i < joined.length; i++) {
	        var obj = joined[i];
	        var anchor = obj.anchor && Pos(obj.anchor.line - offset, obj.anchor.ch), head;
	        for (var line = obj.start; line <= obj.end; line++) {
	          var actual = line - offset;
	          if (line == obj.end) head = Pos(actual, cm.getLine(actual).length + 1);
	          if (actual < cm.lastLine()) {
	            cm.replaceRange(" ", Pos(actual), Pos(actual + 1, /^\s*/.exec(cm.getLine(actual + 1))[0].length));
	            ++offset;
	          }
	        }
	        ranges.push({anchor: anchor || head, head: head});
	      }
	      cm.setSelections(ranges, 0);
	    });
	  };

	  cmds.duplicateLine = function(cm) {
	    cm.operation(function() {
	      var rangeCount = cm.listSelections().length;
	      for (var i = 0; i < rangeCount; i++) {
	        var range = cm.listSelections()[i];
	        if (range.empty())
	          cm.replaceRange(cm.getLine(range.head.line) + "\n", Pos(range.head.line, 0));
	        else
	          cm.replaceRange(cm.getRange(range.from(), range.to()), range.from());
	      }
	      cm.scrollIntoView();
	    });
	  };


	  function sortLines(cm, caseSensitive) {
	    if (cm.isReadOnly()) return CodeMirror.Pass
	    var ranges = cm.listSelections(), toSort = [], selected;
	    for (var i = 0; i < ranges.length; i++) {
	      var range = ranges[i];
	      if (range.empty()) continue;
	      var from = range.from().line, to = range.to().line;
	      while (i < ranges.length - 1 && ranges[i + 1].from().line == to)
	        to = ranges[++i].to().line;
	      if (!ranges[i].to().ch) to--;
	      toSort.push(from, to);
	    }
	    if (toSort.length) selected = true;
	    else toSort.push(cm.firstLine(), cm.lastLine());

	    cm.operation(function() {
	      var ranges = [];
	      for (var i = 0; i < toSort.length; i += 2) {
	        var from = toSort[i], to = toSort[i + 1];
	        var start = Pos(from, 0), end = Pos(to);
	        var lines = cm.getRange(start, end, false);
	        if (caseSensitive)
	          lines.sort();
	        else
	          lines.sort(function(a, b) {
	            var au = a.toUpperCase(), bu = b.toUpperCase();
	            if (au != bu) { a = au; b = bu; }
	            return a < b ? -1 : a == b ? 0 : 1;
	          });
	        cm.replaceRange(lines, start, end);
	        if (selected) ranges.push({anchor: start, head: Pos(to + 1, 0)});
	      }
	      if (selected) cm.setSelections(ranges, 0);
	    });
	  }

	  cmds.sortLines = function(cm) { sortLines(cm, true); };
	  cmds.sortLinesInsensitive = function(cm) { sortLines(cm, false); };

	  cmds.nextBookmark = function(cm) {
	    var marks = cm.state.sublimeBookmarks;
	    if (marks) while (marks.length) {
	      var current = marks.shift();
	      var found = current.find();
	      if (found) {
	        marks.push(current);
	        return cm.setSelection(found.from, found.to);
	      }
	    }
	  };

	  cmds.prevBookmark = function(cm) {
	    var marks = cm.state.sublimeBookmarks;
	    if (marks) while (marks.length) {
	      marks.unshift(marks.pop());
	      var found = marks[marks.length - 1].find();
	      if (!found)
	        marks.pop();
	      else
	        return cm.setSelection(found.from, found.to);
	    }
	  };

	  cmds.toggleBookmark = function(cm) {
	    var ranges = cm.listSelections();
	    var marks = cm.state.sublimeBookmarks || (cm.state.sublimeBookmarks = []);
	    for (var i = 0; i < ranges.length; i++) {
	      var from = ranges[i].from(), to = ranges[i].to();
	      var found = cm.findMarks(from, to);
	      for (var j = 0; j < found.length; j++) {
	        if (found[j].sublimeBookmark) {
	          found[j].clear();
	          for (var k = 0; k < marks.length; k++)
	            if (marks[k] == found[j])
	              marks.splice(k--, 1);
	          break;
	        }
	      }
	      if (j == found.length)
	        marks.push(cm.markText(from, to, {sublimeBookmark: true, clearWhenEmpty: false}));
	    }
	  };

	  cmds.clearBookmarks = function(cm) {
	    var marks = cm.state.sublimeBookmarks;
	    if (marks) for (var i = 0; i < marks.length; i++) marks[i].clear();
	    marks.length = 0;
	  };

	  cmds.selectBookmarks = function(cm) {
	    var marks = cm.state.sublimeBookmarks, ranges = [];
	    if (marks) for (var i = 0; i < marks.length; i++) {
	      var found = marks[i].find();
	      if (!found)
	        marks.splice(i--, 0);
	      else
	        ranges.push({anchor: found.from, head: found.to});
	    }
	    if (ranges.length)
	      cm.setSelections(ranges, 0);
	  };

	  function modifyWordOrSelection(cm, mod) {
	    cm.operation(function() {
	      var ranges = cm.listSelections(), indices = [], replacements = [];
	      for (var i = 0; i < ranges.length; i++) {
	        var range = ranges[i];
	        if (range.empty()) { indices.push(i); replacements.push(""); }
	        else replacements.push(mod(cm.getRange(range.from(), range.to())));
	      }
	      cm.replaceSelections(replacements, "around", "case");
	      for (var i = indices.length - 1, at; i >= 0; i--) {
	        var range = ranges[indices[i]];
	        if (at && CodeMirror.cmpPos(range.head, at) > 0) continue;
	        var word = wordAt(cm, range.head);
	        at = word.from;
	        cm.replaceRange(mod(word.word), word.from, word.to);
	      }
	    });
	  }

	  cmds.smartBackspace = function(cm) {
	    if (cm.somethingSelected()) return CodeMirror.Pass;

	    cm.operation(function() {
	      var cursors = cm.listSelections();
	      var indentUnit = cm.getOption("indentUnit");

	      for (var i = cursors.length - 1; i >= 0; i--) {
	        var cursor = cursors[i].head;
	        var toStartOfLine = cm.getRange({line: cursor.line, ch: 0}, cursor);
	        var column = CodeMirror.countColumn(toStartOfLine, null, cm.getOption("tabSize"));

	        // Delete by one character by default
	        var deletePos = cm.findPosH(cursor, -1, "char", false);

	        if (toStartOfLine && !/\S/.test(toStartOfLine) && column % indentUnit == 0) {
	          var prevIndent = new Pos(cursor.line,
	            CodeMirror.findColumn(toStartOfLine, column - indentUnit, indentUnit));

	          // Smart delete only if we found a valid prevIndent location
	          if (prevIndent.ch != cursor.ch) deletePos = prevIndent;
	        }

	        cm.replaceRange("", deletePos, cursor, "+delete");
	      }
	    });
	  };

	  cmds.delLineRight = function(cm) {
	    cm.operation(function() {
	      var ranges = cm.listSelections();
	      for (var i = ranges.length - 1; i >= 0; i--)
	        cm.replaceRange("", ranges[i].anchor, Pos(ranges[i].to().line), "+delete");
	      cm.scrollIntoView();
	    });
	  };

	  cmds.upcaseAtCursor = function(cm) {
	    modifyWordOrSelection(cm, function(str) { return str.toUpperCase(); });
	  };
	  cmds.downcaseAtCursor = function(cm) {
	    modifyWordOrSelection(cm, function(str) { return str.toLowerCase(); });
	  };

	  cmds.setSublimeMark = function(cm) {
	    if (cm.state.sublimeMark) cm.state.sublimeMark.clear();
	    cm.state.sublimeMark = cm.setBookmark(cm.getCursor());
	  };
	  cmds.selectToSublimeMark = function(cm) {
	    var found = cm.state.sublimeMark && cm.state.sublimeMark.find();
	    if (found) cm.setSelection(cm.getCursor(), found);
	  };
	  cmds.deleteToSublimeMark = function(cm) {
	    var found = cm.state.sublimeMark && cm.state.sublimeMark.find();
	    if (found) {
	      var from = cm.getCursor(), to = found;
	      if (CodeMirror.cmpPos(from, to) > 0) { var tmp = to; to = from; from = tmp; }
	      cm.state.sublimeKilled = cm.getRange(from, to);
	      cm.replaceRange("", from, to);
	    }
	  };
	  cmds.swapWithSublimeMark = function(cm) {
	    var found = cm.state.sublimeMark && cm.state.sublimeMark.find();
	    if (found) {
	      cm.state.sublimeMark.clear();
	      cm.state.sublimeMark = cm.setBookmark(cm.getCursor());
	      cm.setCursor(found);
	    }
	  };
	  cmds.sublimeYank = function(cm) {
	    if (cm.state.sublimeKilled != null)
	      cm.replaceSelection(cm.state.sublimeKilled, null, "paste");
	  };

	  cmds.showInCenter = function(cm) {
	    var pos = cm.cursorCoords(null, "local");
	    cm.scrollTo(null, (pos.top + pos.bottom) / 2 - cm.getScrollInfo().clientHeight / 2);
	  };

	  cmds.selectLinesUpward = function(cm) {
	    cm.operation(function() {
	      var ranges = cm.listSelections();
	      for (var i = 0; i < ranges.length; i++) {
	        var range = ranges[i];
	        if (range.head.line > cm.firstLine())
	          cm.addSelection(Pos(range.head.line - 1, range.head.ch));
	      }
	    });
	  };
	  cmds.selectLinesDownward = function(cm) {
	    cm.operation(function() {
	      var ranges = cm.listSelections();
	      for (var i = 0; i < ranges.length; i++) {
	        var range = ranges[i];
	        if (range.head.line < cm.lastLine())
	          cm.addSelection(Pos(range.head.line + 1, range.head.ch));
	      }
	    });
	  };

	  function getTarget(cm) {
	    var from = cm.getCursor("from"), to = cm.getCursor("to");
	    if (CodeMirror.cmpPos(from, to) == 0) {
	      var word = wordAt(cm, from);
	      if (!word.word) return;
	      from = word.from;
	      to = word.to;
	    }
	    return {from: from, to: to, query: cm.getRange(from, to), word: word};
	  }

	  function findAndGoTo(cm, forward) {
	    var target = getTarget(cm);
	    if (!target) return;
	    var query = target.query;
	    var cur = cm.getSearchCursor(query, forward ? target.to : target.from);

	    if (forward ? cur.findNext() : cur.findPrevious()) {
	      cm.setSelection(cur.from(), cur.to());
	    } else {
	      cur = cm.getSearchCursor(query, forward ? Pos(cm.firstLine(), 0)
	                                              : cm.clipPos(Pos(cm.lastLine())));
	      if (forward ? cur.findNext() : cur.findPrevious())
	        cm.setSelection(cur.from(), cur.to());
	      else if (target.word)
	        cm.setSelection(target.from, target.to);
	    }
	  };
	  cmds.findUnder = function(cm) { findAndGoTo(cm, true); };
	  cmds.findUnderPrevious = function(cm) { findAndGoTo(cm,false); };
	  cmds.findAllUnder = function(cm) {
	    var target = getTarget(cm);
	    if (!target) return;
	    var cur = cm.getSearchCursor(target.query);
	    var matches = [];
	    var primaryIndex = -1;
	    while (cur.findNext()) {
	      matches.push({anchor: cur.from(), head: cur.to()});
	      if (cur.from().line <= target.from.line && cur.from().ch <= target.from.ch)
	        primaryIndex++;
	    }
	    cm.setSelections(matches, primaryIndex);
	  };


	  var keyMap = CodeMirror.keyMap;
	  keyMap.macSublime = {
	    "Cmd-Left": "goLineStartSmart",
	    "Shift-Tab": "indentLess",
	    "Shift-Ctrl-K": "deleteLine",
	    "Alt-Q": "wrapLines",
	    "Ctrl-Left": "goSubwordLeft",
	    "Ctrl-Right": "goSubwordRight",
	    "Ctrl-Alt-Up": "scrollLineUp",
	    "Ctrl-Alt-Down": "scrollLineDown",
	    "Cmd-L": "selectLine",
	    "Shift-Cmd-L": "splitSelectionByLine",
	    "Esc": "singleSelectionTop",
	    "Cmd-Enter": "insertLineAfter",
	    "Shift-Cmd-Enter": "insertLineBefore",
	    "Cmd-D": "selectNextOccurrence",
	    "Shift-Cmd-Up": "addCursorToPrevLine",
	    "Shift-Cmd-Down": "addCursorToNextLine",
	    "Shift-Cmd-Space": "selectScope",
	    "Shift-Cmd-M": "selectBetweenBrackets",
	    "Cmd-M": "goToBracket",
	    "Cmd-Ctrl-Up": "swapLineUp",
	    "Cmd-Ctrl-Down": "swapLineDown",
	    "Cmd-/": "toggleCommentIndented",
	    "Cmd-J": "joinLines",
	    "Shift-Cmd-D": "duplicateLine",
	    "F9": "sortLines",
	    "Cmd-F9": "sortLinesInsensitive",
	    "F2": "nextBookmark",
	    "Shift-F2": "prevBookmark",
	    "Cmd-F2": "toggleBookmark",
	    "Shift-Cmd-F2": "clearBookmarks",
	    "Alt-F2": "selectBookmarks",
	    "Backspace": "smartBackspace",
	    "Cmd-K Cmd-K": "delLineRight",
	    "Cmd-K Cmd-U": "upcaseAtCursor",
	    "Cmd-K Cmd-L": "downcaseAtCursor",
	    "Cmd-K Cmd-Space": "setSublimeMark",
	    "Cmd-K Cmd-A": "selectToSublimeMark",
	    "Cmd-K Cmd-W": "deleteToSublimeMark",
	    "Cmd-K Cmd-X": "swapWithSublimeMark",
	    "Cmd-K Cmd-Y": "sublimeYank",
	    "Cmd-K Cmd-C": "showInCenter",
	    "Cmd-K Cmd-G": "clearBookmarks",
	    "Cmd-K Cmd-Backspace": "delLineLeft",
	    "Cmd-K Cmd-0": "unfoldAll",
	    "Cmd-K Cmd-J": "unfoldAll",
	    "Ctrl-Shift-Up": "selectLinesUpward",
	    "Ctrl-Shift-Down": "selectLinesDownward",
	    "Cmd-F3": "findUnder",
	    "Shift-Cmd-F3": "findUnderPrevious",
	    "Alt-F3": "findAllUnder",
	    "Shift-Cmd-[": "fold",
	    "Shift-Cmd-]": "unfold",
	    "Cmd-I": "findIncremental",
	    "Shift-Cmd-I": "findIncrementalReverse",
	    "Cmd-H": "replace",
	    "F3": "findNext",
	    "Shift-F3": "findPrev",
	    "fallthrough": "macDefault"
	  };
	  CodeMirror.normalizeKeyMap(keyMap.macSublime);

	  keyMap.pcSublime = {
	    "Shift-Tab": "indentLess",
	    "Shift-Ctrl-K": "deleteLine",
	    "Alt-Q": "wrapLines",
	    "Ctrl-T": "transposeChars",
	    "Alt-Left": "goSubwordLeft",
	    "Alt-Right": "goSubwordRight",
	    "Ctrl-Up": "scrollLineUp",
	    "Ctrl-Down": "scrollLineDown",
	    "Ctrl-L": "selectLine",
	    "Shift-Ctrl-L": "splitSelectionByLine",
	    "Esc": "singleSelectionTop",
	    "Ctrl-Enter": "insertLineAfter",
	    "Shift-Ctrl-Enter": "insertLineBefore",
	    "Ctrl-D": "selectNextOccurrence",
	    "Alt-CtrlUp": "addCursorToPrevLine",
	    "Alt-CtrlDown": "addCursorToNextLine",
	    "Shift-Ctrl-Space": "selectScope",
	    "Shift-Ctrl-M": "selectBetweenBrackets",
	    "Ctrl-M": "goToBracket",
	    "Shift-Ctrl-Up": "swapLineUp",
	    "Shift-Ctrl-Down": "swapLineDown",
	    "Ctrl-/": "toggleCommentIndented",
	    "Ctrl-J": "joinLines",
	    "Shift-Ctrl-D": "duplicateLine",
	    "F9": "sortLines",
	    "Ctrl-F9": "sortLinesInsensitive",
	    "F2": "nextBookmark",
	    "Shift-F2": "prevBookmark",
	    "Ctrl-F2": "toggleBookmark",
	    "Shift-Ctrl-F2": "clearBookmarks",
	    "Alt-F2": "selectBookmarks",
	    "Backspace": "smartBackspace",
	    "Ctrl-K Ctrl-K": "delLineRight",
	    "Ctrl-K Ctrl-U": "upcaseAtCursor",
	    "Ctrl-K Ctrl-L": "downcaseAtCursor",
	    "Ctrl-K Ctrl-Space": "setSublimeMark",
	    "Ctrl-K Ctrl-A": "selectToSublimeMark",
	    "Ctrl-K Ctrl-W": "deleteToSublimeMark",
	    "Ctrl-K Ctrl-X": "swapWithSublimeMark",
	    "Ctrl-K Ctrl-Y": "sublimeYank",
	    "Ctrl-K Ctrl-C": "showInCenter",
	    "Ctrl-K Ctrl-G": "clearBookmarks",
	    "Ctrl-K Ctrl-Backspace": "delLineLeft",
	    "Ctrl-K Ctrl-0": "unfoldAll",
	    "Ctrl-K Ctrl-J": "unfoldAll",
	    "Ctrl-Alt-Up": "selectLinesUpward",
	    "Ctrl-Alt-Down": "selectLinesDownward",
	    "Ctrl-F3": "findUnder",
	    "Shift-Ctrl-F3": "findUnderPrevious",
	    "Alt-F3": "findAllUnder",
	    "Shift-Ctrl-[": "fold",
	    "Shift-Ctrl-]": "unfold",
	    "Ctrl-I": "findIncremental",
	    "Shift-Ctrl-I": "findIncrementalReverse",
	    "Ctrl-H": "replace",
	    "F3": "findNext",
	    "Shift-F3": "findPrev",
	    "fallthrough": "pcDefault"
	  };
	  CodeMirror.normalizeKeyMap(keyMap.pcSublime);

	  var mac = keyMap.default == keyMap.macDefault;
	  keyMap.sublime = mac ? keyMap.macSublime : keyMap.pcSublime;
	});


/***/ }),
/* 22 */
/***/ (function(module, exports, __webpack_require__) {

	// CodeMirror, copyright (c) by Marijn Haverbeke and others
	// Distributed under an MIT license: http://codemirror.net/LICENSE

	(function(mod) {
	  if (true) // CommonJS
	    mod(__webpack_require__(2));
	  else if (typeof define == "function" && define.amd) // AMD
	    define(["../../lib/codemirror"], mod);
	  else // Plain browser env
	    mod(CodeMirror);
	})(function(CodeMirror) {
	  "use strict";

	  function doFold(cm, pos, options, force) {
	    if (options && options.call) {
	      var finder = options;
	      options = null;
	    } else {
	      var finder = getOption(cm, options, "rangeFinder");
	    }
	    if (typeof pos == "number") pos = CodeMirror.Pos(pos, 0);
	    var minSize = getOption(cm, options, "minFoldSize");

	    function getRange(allowFolded) {
	      var range = finder(cm, pos);
	      if (!range || range.to.line - range.from.line < minSize) return null;
	      var marks = cm.findMarksAt(range.from);
	      for (var i = 0; i < marks.length; ++i) {
	        if (marks[i].__isFold && force !== "fold") {
	          if (!allowFolded) return null;
	          range.cleared = true;
	          marks[i].clear();
	        }
	      }
	      return range;
	    }

	    var range = getRange(true);
	    if (getOption(cm, options, "scanUp")) while (!range && pos.line > cm.firstLine()) {
	      pos = CodeMirror.Pos(pos.line - 1, 0);
	      range = getRange(false);
	    }
	    if (!range || range.cleared || force === "unfold") return;

	    var myWidget = makeWidget(cm, options);
	    CodeMirror.on(myWidget, "mousedown", function(e) {
	      myRange.clear();
	      CodeMirror.e_preventDefault(e);
	    });
	    var myRange = cm.markText(range.from, range.to, {
	      replacedWith: myWidget,
	      clearOnEnter: getOption(cm, options, "clearOnEnter"),
	      __isFold: true
	    });
	    myRange.on("clear", function(from, to) {
	      CodeMirror.signal(cm, "unfold", cm, from, to);
	    });
	    CodeMirror.signal(cm, "fold", cm, range.from, range.to);
	  }

	  function makeWidget(cm, options) {
	    var widget = getOption(cm, options, "widget");
	    if (typeof widget == "string") {
	      var text = document.createTextNode(widget);
	      widget = document.createElement("span");
	      widget.appendChild(text);
	      widget.className = "CodeMirror-foldmarker";
	    } else if (widget) {
	      widget = widget.cloneNode(true)
	    }
	    return widget;
	  }

	  // Clumsy backwards-compatible interface
	  CodeMirror.newFoldFunction = function(rangeFinder, widget) {
	    return function(cm, pos) { doFold(cm, pos, {rangeFinder: rangeFinder, widget: widget}); };
	  };

	  // New-style interface
	  CodeMirror.defineExtension("foldCode", function(pos, options, force) {
	    doFold(this, pos, options, force);
	  });

	  CodeMirror.defineExtension("isFolded", function(pos) {
	    var marks = this.findMarksAt(pos);
	    for (var i = 0; i < marks.length; ++i)
	      if (marks[i].__isFold) return true;
	  });

	  CodeMirror.commands.toggleFold = function(cm) {
	    cm.foldCode(cm.getCursor());
	  };
	  CodeMirror.commands.fold = function(cm) {
	    cm.foldCode(cm.getCursor(), null, "fold");
	  };
	  CodeMirror.commands.unfold = function(cm) {
	    cm.foldCode(cm.getCursor(), null, "unfold");
	  };
	  CodeMirror.commands.foldAll = function(cm) {
	    cm.operation(function() {
	      for (var i = cm.firstLine(), e = cm.lastLine(); i <= e; i++)
	        cm.foldCode(CodeMirror.Pos(i, 0), null, "fold");
	    });
	  };
	  CodeMirror.commands.unfoldAll = function(cm) {
	    cm.operation(function() {
	      for (var i = cm.firstLine(), e = cm.lastLine(); i <= e; i++)
	        cm.foldCode(CodeMirror.Pos(i, 0), null, "unfold");
	    });
	  };

	  CodeMirror.registerHelper("fold", "combine", function() {
	    var funcs = Array.prototype.slice.call(arguments, 0);
	    return function(cm, start) {
	      for (var i = 0; i < funcs.length; ++i) {
	        var found = funcs[i](cm, start);
	        if (found) return found;
	      }
	    };
	  });

	  CodeMirror.registerHelper("fold", "auto", function(cm, start) {
	    var helpers = cm.getHelpers(start, "fold");
	    for (var i = 0; i < helpers.length; i++) {
	      var cur = helpers[i](cm, start);
	      if (cur) return cur;
	    }
	  });

	  var defaultOptions = {
	    rangeFinder: CodeMirror.fold.auto,
	    widget: "\u2194",
	    minFoldSize: 0,
	    scanUp: false,
	    clearOnEnter: true
	  };

	  CodeMirror.defineOption("foldOptions", null);

	  function getOption(cm, options, name) {
	    if (options && options[name] !== undefined)
	      return options[name];
	    var editorOptions = cm.options.foldOptions;
	    if (editorOptions && editorOptions[name] !== undefined)
	      return editorOptions[name];
	    return defaultOptions[name];
	  }

	  CodeMirror.defineExtension("foldOption", function(options, name) {
	    return getOption(this, options, name);
	  });
	});


/***/ }),
/* 23 */
/***/ (function(module, exports, __webpack_require__) {

	// CodeMirror, copyright (c) by Marijn Haverbeke and others
	// Distributed under an MIT license: http://codemirror.net/LICENSE

	(function(mod) {
	  if (true) // CommonJS
	    mod(__webpack_require__(2));
	  else if (typeof define == "function" && define.amd) // AMD
	    define(["../../lib/codemirror"], mod);
	  else // Plain browser env
	    mod(CodeMirror);
	})(function(CodeMirror) {
	"use strict";

	CodeMirror.registerHelper("fold", "brace", function(cm, start) {
	  var line = start.line, lineText = cm.getLine(line);
	  var tokenType;

	  function findOpening(openCh) {
	    for (var at = start.ch, pass = 0;;) {
	      var found = at <= 0 ? -1 : lineText.lastIndexOf(openCh, at - 1);
	      if (found == -1) {
	        if (pass == 1) break;
	        pass = 1;
	        at = lineText.length;
	        continue;
	      }
	      if (pass == 1 && found < start.ch) break;
	      tokenType = cm.getTokenTypeAt(CodeMirror.Pos(line, found + 1));
	      if (!/^(comment|string)/.test(tokenType)) return found + 1;
	      at = found - 1;
	    }
	  }

	  var startToken = "{", endToken = "}", startCh = findOpening("{");
	  if (startCh == null) {
	    startToken = "[", endToken = "]";
	    startCh = findOpening("[");
	  }

	  if (startCh == null) return;
	  var count = 1, lastLine = cm.lastLine(), end, endCh;
	  outer: for (var i = line; i <= lastLine; ++i) {
	    var text = cm.getLine(i), pos = i == line ? startCh : 0;
	    for (;;) {
	      var nextOpen = text.indexOf(startToken, pos), nextClose = text.indexOf(endToken, pos);
	      if (nextOpen < 0) nextOpen = text.length;
	      if (nextClose < 0) nextClose = text.length;
	      pos = Math.min(nextOpen, nextClose);
	      if (pos == text.length) break;
	      if (cm.getTokenTypeAt(CodeMirror.Pos(i, pos + 1)) == tokenType) {
	        if (pos == nextOpen) ++count;
	        else if (!--count) { end = i; endCh = pos; break outer; }
	      }
	      ++pos;
	    }
	  }
	  if (end == null || line == end && endCh == startCh) return;
	  return {from: CodeMirror.Pos(line, startCh),
	          to: CodeMirror.Pos(end, endCh)};
	});

	CodeMirror.registerHelper("fold", "import", function(cm, start) {
	  function hasImport(line) {
	    if (line < cm.firstLine() || line > cm.lastLine()) return null;
	    var start = cm.getTokenAt(CodeMirror.Pos(line, 1));
	    if (!/\S/.test(start.string)) start = cm.getTokenAt(CodeMirror.Pos(line, start.end + 1));
	    if (start.type != "keyword" || start.string != "import") return null;
	    // Now find closing semicolon, return its position
	    for (var i = line, e = Math.min(cm.lastLine(), line + 10); i <= e; ++i) {
	      var text = cm.getLine(i), semi = text.indexOf(";");
	      if (semi != -1) return {startCh: start.end, end: CodeMirror.Pos(i, semi)};
	    }
	  }

	  var startLine = start.line, has = hasImport(startLine), prev;
	  if (!has || hasImport(startLine - 1) || ((prev = hasImport(startLine - 2)) && prev.end.line == startLine - 1))
	    return null;
	  for (var end = has.end;;) {
	    var next = hasImport(end.line + 1);
	    if (next == null) break;
	    end = next.end;
	  }
	  return {from: cm.clipPos(CodeMirror.Pos(startLine, has.startCh + 1)), to: end};
	});

	CodeMirror.registerHelper("fold", "include", function(cm, start) {
	  function hasInclude(line) {
	    if (line < cm.firstLine() || line > cm.lastLine()) return null;
	    var start = cm.getTokenAt(CodeMirror.Pos(line, 1));
	    if (!/\S/.test(start.string)) start = cm.getTokenAt(CodeMirror.Pos(line, start.end + 1));
	    if (start.type == "meta" && start.string.slice(0, 8) == "#include") return start.start + 8;
	  }

	  var startLine = start.line, has = hasInclude(startLine);
	  if (has == null || hasInclude(startLine - 1) != null) return null;
	  for (var end = startLine;;) {
	    var next = hasInclude(end + 1);
	    if (next == null) break;
	    ++end;
	  }
	  return {from: CodeMirror.Pos(startLine, has + 1),
	          to: cm.clipPos(CodeMirror.Pos(end))};
	});

	});


/***/ }),
/* 24 */
/***/ (function(module, exports, __webpack_require__) {

	// CodeMirror, copyright (c) by Marijn Haverbeke and others
	// Distributed under an MIT license: http://codemirror.net/LICENSE

	(function(mod) {
	  if (true) // CommonJS
	    mod(__webpack_require__(2));
	  else if (typeof define == "function" && define.amd) // AMD
	    define(["../../lib/codemirror"], mod);
	  else // Plain browser env
	    mod(CodeMirror);
	})(function(CodeMirror) {
	"use strict";

	CodeMirror.registerGlobalHelper("fold", "comment", function(mode) {
	  return mode.blockCommentStart && mode.blockCommentEnd;
	}, function(cm, start) {
	  var mode = cm.getModeAt(start), startToken = mode.blockCommentStart, endToken = mode.blockCommentEnd;
	  if (!startToken || !endToken) return;
	  var line = start.line, lineText = cm.getLine(line);

	  var startCh;
	  for (var at = start.ch, pass = 0;;) {
	    var found = at <= 0 ? -1 : lineText.lastIndexOf(startToken, at - 1);
	    if (found == -1) {
	      if (pass == 1) return;
	      pass = 1;
	      at = lineText.length;
	      continue;
	    }
	    if (pass == 1 && found < start.ch) return;
	    if (/comment/.test(cm.getTokenTypeAt(CodeMirror.Pos(line, found + 1))) &&
	        (found == 0 || lineText.slice(found - endToken.length, found) == endToken ||
	         !/comment/.test(cm.getTokenTypeAt(CodeMirror.Pos(line, found))))) {
	      startCh = found + startToken.length;
	      break;
	    }
	    at = found - 1;
	  }

	  var depth = 1, lastLine = cm.lastLine(), end, endCh;
	  outer: for (var i = line; i <= lastLine; ++i) {
	    var text = cm.getLine(i), pos = i == line ? startCh : 0;
	    for (;;) {
	      var nextOpen = text.indexOf(startToken, pos), nextClose = text.indexOf(endToken, pos);
	      if (nextOpen < 0) nextOpen = text.length;
	      if (nextClose < 0) nextClose = text.length;
	      pos = Math.min(nextOpen, nextClose);
	      if (pos == text.length) break;
	      if (pos == nextOpen) ++depth;
	      else if (!--depth) { end = i; endCh = pos; break outer; }
	      ++pos;
	    }
	  }
	  if (end == null || line == end && endCh == startCh) return;
	  return {from: CodeMirror.Pos(line, startCh),
	          to: CodeMirror.Pos(end, endCh)};
	});

	});


/***/ }),
/* 25 */
/***/ (function(module, exports, __webpack_require__) {

	// CodeMirror, copyright (c) by Marijn Haverbeke and others
	// Distributed under an MIT license: http://codemirror.net/LICENSE

	(function(mod) {
	  if (true) // CommonJS
	    mod(__webpack_require__(2));
	  else if (typeof define == "function" && define.amd) // AMD
	    define(["../../lib/codemirror"], mod);
	  else // Plain browser env
	    mod(CodeMirror);
	})(function(CodeMirror) {
	  "use strict";

	  var Pos = CodeMirror.Pos;
	  function cmp(a, b) { return a.line - b.line || a.ch - b.ch; }

	  var nameStartChar = "A-Z_a-z\\u00C0-\\u00D6\\u00D8-\\u00F6\\u00F8-\\u02FF\\u0370-\\u037D\\u037F-\\u1FFF\\u200C-\\u200D\\u2070-\\u218F\\u2C00-\\u2FEF\\u3001-\\uD7FF\\uF900-\\uFDCF\\uFDF0-\\uFFFD";
	  var nameChar = nameStartChar + "\-\:\.0-9\\u00B7\\u0300-\\u036F\\u203F-\\u2040";
	  var xmlTagStart = new RegExp("<(/?)([" + nameStartChar + "][" + nameChar + "]*)", "g");

	  function Iter(cm, line, ch, range) {
	    this.line = line; this.ch = ch;
	    this.cm = cm; this.text = cm.getLine(line);
	    this.min = range ? Math.max(range.from, cm.firstLine()) : cm.firstLine();
	    this.max = range ? Math.min(range.to - 1, cm.lastLine()) : cm.lastLine();
	  }

	  function tagAt(iter, ch) {
	    var type = iter.cm.getTokenTypeAt(Pos(iter.line, ch));
	    return type && /\btag\b/.test(type);
	  }

	  function nextLine(iter) {
	    if (iter.line >= iter.max) return;
	    iter.ch = 0;
	    iter.text = iter.cm.getLine(++iter.line);
	    return true;
	  }
	  function prevLine(iter) {
	    if (iter.line <= iter.min) return;
	    iter.text = iter.cm.getLine(--iter.line);
	    iter.ch = iter.text.length;
	    return true;
	  }

	  function toTagEnd(iter) {
	    for (;;) {
	      var gt = iter.text.indexOf(">", iter.ch);
	      if (gt == -1) { if (nextLine(iter)) continue; else return; }
	      if (!tagAt(iter, gt + 1)) { iter.ch = gt + 1; continue; }
	      var lastSlash = iter.text.lastIndexOf("/", gt);
	      var selfClose = lastSlash > -1 && !/\S/.test(iter.text.slice(lastSlash + 1, gt));
	      iter.ch = gt + 1;
	      return selfClose ? "selfClose" : "regular";
	    }
	  }
	  function toTagStart(iter) {
	    for (;;) {
	      var lt = iter.ch ? iter.text.lastIndexOf("<", iter.ch - 1) : -1;
	      if (lt == -1) { if (prevLine(iter)) continue; else return; }
	      if (!tagAt(iter, lt + 1)) { iter.ch = lt; continue; }
	      xmlTagStart.lastIndex = lt;
	      iter.ch = lt;
	      var match = xmlTagStart.exec(iter.text);
	      if (match && match.index == lt) return match;
	    }
	  }

	  function toNextTag(iter) {
	    for (;;) {
	      xmlTagStart.lastIndex = iter.ch;
	      var found = xmlTagStart.exec(iter.text);
	      if (!found) { if (nextLine(iter)) continue; else return; }
	      if (!tagAt(iter, found.index + 1)) { iter.ch = found.index + 1; continue; }
	      iter.ch = found.index + found[0].length;
	      return found;
	    }
	  }
	  function toPrevTag(iter) {
	    for (;;) {
	      var gt = iter.ch ? iter.text.lastIndexOf(">", iter.ch - 1) : -1;
	      if (gt == -1) { if (prevLine(iter)) continue; else return; }
	      if (!tagAt(iter, gt + 1)) { iter.ch = gt; continue; }
	      var lastSlash = iter.text.lastIndexOf("/", gt);
	      var selfClose = lastSlash > -1 && !/\S/.test(iter.text.slice(lastSlash + 1, gt));
	      iter.ch = gt + 1;
	      return selfClose ? "selfClose" : "regular";
	    }
	  }

	  function findMatchingClose(iter, tag) {
	    var stack = [];
	    for (;;) {
	      var next = toNextTag(iter), end, startLine = iter.line, startCh = iter.ch - (next ? next[0].length : 0);
	      if (!next || !(end = toTagEnd(iter))) return;
	      if (end == "selfClose") continue;
	      if (next[1]) { // closing tag
	        for (var i = stack.length - 1; i >= 0; --i) if (stack[i] == next[2]) {
	          stack.length = i;
	          break;
	        }
	        if (i < 0 && (!tag || tag == next[2])) return {
	          tag: next[2],
	          from: Pos(startLine, startCh),
	          to: Pos(iter.line, iter.ch)
	        };
	      } else { // opening tag
	        stack.push(next[2]);
	      }
	    }
	  }
	  function findMatchingOpen(iter, tag) {
	    var stack = [];
	    for (;;) {
	      var prev = toPrevTag(iter);
	      if (!prev) return;
	      if (prev == "selfClose") { toTagStart(iter); continue; }
	      var endLine = iter.line, endCh = iter.ch;
	      var start = toTagStart(iter);
	      if (!start) return;
	      if (start[1]) { // closing tag
	        stack.push(start[2]);
	      } else { // opening tag
	        for (var i = stack.length - 1; i >= 0; --i) if (stack[i] == start[2]) {
	          stack.length = i;
	          break;
	        }
	        if (i < 0 && (!tag || tag == start[2])) return {
	          tag: start[2],
	          from: Pos(iter.line, iter.ch),
	          to: Pos(endLine, endCh)
	        };
	      }
	    }
	  }

	  CodeMirror.registerHelper("fold", "xml", function(cm, start) {
	    var iter = new Iter(cm, start.line, 0);
	    for (;;) {
	      var openTag = toNextTag(iter), end;
	      if (!openTag || iter.line != start.line || !(end = toTagEnd(iter))) return;
	      if (!openTag[1] && end != "selfClose") {
	        var startPos = Pos(iter.line, iter.ch);
	        var endPos = findMatchingClose(iter, openTag[2]);
	        return endPos && {from: startPos, to: endPos.from};
	      }
	    }
	  });
	  CodeMirror.findMatchingTag = function(cm, pos, range) {
	    var iter = new Iter(cm, pos.line, pos.ch, range);
	    if (iter.text.indexOf(">") == -1 && iter.text.indexOf("<") == -1) return;
	    var end = toTagEnd(iter), to = end && Pos(iter.line, iter.ch);
	    var start = end && toTagStart(iter);
	    if (!end || !start || cmp(iter, pos) > 0) return;
	    var here = {from: Pos(iter.line, iter.ch), to: to, tag: start[2]};
	    if (end == "selfClose") return {open: here, close: null, at: "open"};

	    if (start[1]) { // closing tag
	      return {open: findMatchingOpen(iter, start[2]), close: here, at: "close"};
	    } else { // opening tag
	      iter = new Iter(cm, to.line, to.ch, range);
	      return {open: here, close: findMatchingClose(iter, start[2]), at: "open"};
	    }
	  };

	  CodeMirror.findEnclosingTag = function(cm, pos, range, tag) {
	    var iter = new Iter(cm, pos.line, pos.ch, range);
	    for (;;) {
	      var open = findMatchingOpen(iter, tag);
	      if (!open) break;
	      var forward = new Iter(cm, pos.line, pos.ch, range);
	      var close = findMatchingClose(forward, open.tag);
	      if (close) return {open: open, close: close};
	    }
	  };

	  // Used by addon/edit/closetag.js
	  CodeMirror.scanForClosingTag = function(cm, pos, name, end) {
	    var iter = new Iter(cm, pos.line, pos.ch, end ? {from: 0, to: end} : null);
	    return findMatchingClose(iter, name);
	  };
	});


/***/ }),
/* 26 */
/***/ (function(module, exports, __webpack_require__) {

	// CodeMirror, copyright (c) by Marijn Haverbeke and others
	// Distributed under an MIT license: http://codemirror.net/LICENSE

	(function(mod) {
	  if (true) // CommonJS
	    mod(__webpack_require__(2), __webpack_require__(22));
	  else if (typeof define == "function" && define.amd) // AMD
	    define(["../../lib/codemirror", "./foldcode"], mod);
	  else // Plain browser env
	    mod(CodeMirror);
	})(function(CodeMirror) {
	  "use strict";

	  CodeMirror.defineOption("foldGutter", false, function(cm, val, old) {
	    if (old && old != CodeMirror.Init) {
	      cm.clearGutter(cm.state.foldGutter.options.gutter);
	      cm.state.foldGutter = null;
	      cm.off("gutterClick", onGutterClick);
	      cm.off("change", onChange);
	      cm.off("viewportChange", onViewportChange);
	      cm.off("fold", onFold);
	      cm.off("unfold", onFold);
	      cm.off("swapDoc", onChange);
	    }
	    if (val) {
	      cm.state.foldGutter = new State(parseOptions(val));
	      updateInViewport(cm);
	      cm.on("gutterClick", onGutterClick);
	      cm.on("change", onChange);
	      cm.on("viewportChange", onViewportChange);
	      cm.on("fold", onFold);
	      cm.on("unfold", onFold);
	      cm.on("swapDoc", onChange);
	    }
	  });

	  var Pos = CodeMirror.Pos;

	  function State(options) {
	    this.options = options;
	    this.from = this.to = 0;
	  }

	  function parseOptions(opts) {
	    if (opts === true) opts = {};
	    if (opts.gutter == null) opts.gutter = "CodeMirror-foldgutter";
	    if (opts.indicatorOpen == null) opts.indicatorOpen = "CodeMirror-foldgutter-open";
	    if (opts.indicatorFolded == null) opts.indicatorFolded = "CodeMirror-foldgutter-folded";
	    return opts;
	  }

	  function isFolded(cm, line) {
	    var marks = cm.findMarks(Pos(line, 0), Pos(line + 1, 0));
	    for (var i = 0; i < marks.length; ++i)
	      if (marks[i].__isFold && marks[i].find().from.line == line) return marks[i];
	  }

	  function marker(spec) {
	    if (typeof spec == "string") {
	      var elt = document.createElement("div");
	      elt.className = spec + " CodeMirror-guttermarker-subtle";
	      return elt;
	    } else {
	      return spec.cloneNode(true);
	    }
	  }

	  function updateFoldInfo(cm, from, to) {
	    var opts = cm.state.foldGutter.options, cur = from;
	    var minSize = cm.foldOption(opts, "minFoldSize");
	    var func = cm.foldOption(opts, "rangeFinder");
	    cm.eachLine(from, to, function(line) {
	      var mark = null;
	      if (isFolded(cm, cur)) {
	        mark = marker(opts.indicatorFolded);
	      } else {
	        var pos = Pos(cur, 0);
	        var range = func && func(cm, pos);
	        if (range && range.to.line - range.from.line >= minSize)
	          mark = marker(opts.indicatorOpen);
	      }
	      cm.setGutterMarker(line, opts.gutter, mark);
	      ++cur;
	    });
	  }

	  function updateInViewport(cm) {
	    var vp = cm.getViewport(), state = cm.state.foldGutter;
	    if (!state) return;
	    cm.operation(function() {
	      updateFoldInfo(cm, vp.from, vp.to);
	    });
	    state.from = vp.from; state.to = vp.to;
	  }

	  function onGutterClick(cm, line, gutter) {
	    var state = cm.state.foldGutter;
	    if (!state) return;
	    var opts = state.options;
	    if (gutter != opts.gutter) return;
	    var folded = isFolded(cm, line);
	    if (folded) folded.clear();
	    else cm.foldCode(Pos(line, 0), opts.rangeFinder);
	  }

	  function onChange(cm) {
	    var state = cm.state.foldGutter;
	    if (!state) return;
	    var opts = state.options;
	    state.from = state.to = 0;
	    clearTimeout(state.changeUpdate);
	    state.changeUpdate = setTimeout(function() { updateInViewport(cm); }, opts.foldOnChangeTimeSpan || 600);
	  }

	  function onViewportChange(cm) {
	    var state = cm.state.foldGutter;
	    if (!state) return;
	    var opts = state.options;
	    clearTimeout(state.changeUpdate);
	    state.changeUpdate = setTimeout(function() {
	      var vp = cm.getViewport();
	      if (state.from == state.to || vp.from - state.to > 20 || state.from - vp.to > 20) {
	        updateInViewport(cm);
	      } else {
	        cm.operation(function() {
	          if (vp.from < state.from) {
	            updateFoldInfo(cm, vp.from, state.from);
	            state.from = vp.from;
	          }
	          if (vp.to > state.to) {
	            updateFoldInfo(cm, state.to, vp.to);
	            state.to = vp.to;
	          }
	        });
	      }
	    }, opts.updateViewportTimeSpan || 400);
	  }

	  function onFold(cm, from) {
	    var state = cm.state.foldGutter;
	    if (!state) return;
	    var line = from.line;
	    if (line >= state.from && line < state.to)
	      updateFoldInfo(cm, line, line + 1);
	  }
	});


/***/ })
/******/ ]);