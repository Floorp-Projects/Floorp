/* vim:set ts=2 sw=2 sts=2 et tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cu, Cc, Ci, components } = require("chrome");

const TAB_SIZE    = "devtools.editor.tabsize";
const EXPAND_TAB  = "devtools.editor.expandtab";
const KEYMAP      = "devtools.editor.keymap";
const L10N_BUNDLE = "chrome://browser/locale/devtools/sourceeditor.properties";
const XUL_NS      = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

// Maximum allowed margin (in number of lines) from top or bottom of the editor
// while shifting to a line which was initially out of view.
const MAX_VERTICAL_OFFSET = 3;

const {Promise: promise} = Cu.import("resource://gre/modules/Promise.jsm", {});
const events  = require("devtools/shared/event-emitter");

Cu.import("resource://gre/modules/Services.jsm");
const L10N = Services.strings.createBundle(L10N_BUNDLE);

// CM_STYLES, CM_SCRIPTS and CM_IFRAME represent the HTML,
// JavaScript and CSS that is injected into an iframe in
// order to initialize a CodeMirror instance.

const CM_STYLES   = [
  "chrome://browser/skin/devtools/common.css",
  "chrome://browser/content/devtools/codemirror/codemirror.css",
  "chrome://browser/content/devtools/codemirror/dialog.css",
  "chrome://browser/content/devtools/codemirror/mozilla.css"
];

const CM_SCRIPTS  = [
  "chrome://browser/content/devtools/theme-switching.js",
  "chrome://browser/content/devtools/codemirror/codemirror.js",
  "chrome://browser/content/devtools/codemirror/dialog.js",
  "chrome://browser/content/devtools/codemirror/searchcursor.js",
  "chrome://browser/content/devtools/codemirror/search.js",
  "chrome://browser/content/devtools/codemirror/matchbrackets.js",
  "chrome://browser/content/devtools/codemirror/closebrackets.js",
  "chrome://browser/content/devtools/codemirror/comment.js",
  "chrome://browser/content/devtools/codemirror/javascript.js",
  "chrome://browser/content/devtools/codemirror/xml.js",
  "chrome://browser/content/devtools/codemirror/css.js",
  "chrome://browser/content/devtools/codemirror/htmlmixed.js",
  "chrome://browser/content/devtools/codemirror/clike.js",
  "chrome://browser/content/devtools/codemirror/activeline.js",
  "chrome://browser/content/devtools/codemirror/emacs.js",
  "chrome://browser/content/devtools/codemirror/vim.js"
];

const CM_IFRAME   =
  "data:text/html;charset=utf8,<!DOCTYPE html>" +
  "<html dir='ltr'>" +
  "  <head>" +
  "    <style>" +
  "      html, body { height: 100%; }" +
  "      body { margin: 0; overflow: hidden; }" +
  "      .CodeMirror { width: 100%; height: 100% !important; line-height: normal!important}" +
  "    </style>" +
[ "    <link rel='stylesheet' href='" + style + "'>" for (style of CM_STYLES) ].join("\n") +
  "  </head>" +
  "  <body class='theme-body devtools-monospace'></body>" +
  "</html>";

const CM_MAPPING = [
  "focus",
  "hasFocus",
  "lineCount",
  "somethingSelected",
  "getCursor",
  "setSelection",
  "getSelection",
  "replaceSelection",
  "extendSelection",
  "undo",
  "redo",
  "clearHistory",
  "openDialog",
  "refresh",
  "getOption",
  "setOption"
];

const { cssProperties, cssValues, cssColors } = getCSSKeywords();

const editors = new WeakMap();

Editor.modes = {
  text: { name: "text" },
  html: { name: "htmlmixed" },
  css:  { name: "css" },
  js:   { name: "javascript" },
  vs:   { name: "x-shader/x-vertex" },
  fs:   { name: "x-shader/x-fragment" }
};

/**
 * A very thin wrapper around CodeMirror. Provides a number
 * of helper methods to make our use of CodeMirror easier and
 * another method, appendTo, to actually create and append
 * the CodeMirror instance.
 *
 * Note that Editor doesn't expose CodeMirror instance to the
 * outside world.
 *
 * Constructor accepts one argument, config. It is very
 * similar to the CodeMirror configuration object so for most
 * properties go to CodeMirror's documentation (see below).
 *
 * Other than that, it accepts one additional and optional
 * property contextMenu. This property should be an ID of
 * an element we can use as a context menu.
 *
 * This object is also an event emitter.
 *
 * CodeMirror docs: http://codemirror.net/doc/manual.html
 */
function Editor(config) {
  const tabSize = Services.prefs.getIntPref(TAB_SIZE);
  const useTabs = !Services.prefs.getBoolPref(EXPAND_TAB);
  const keyMap = Services.prefs.getCharPref(KEYMAP);

  this.version = null;
  this.config = {
    value:             "",
    mode:              Editor.modes.text,
    indentUnit:        tabSize,
    tabSize:           tabSize,
    contextMenu:       null,
    matchBrackets:     true,
    extraKeys:         {},
    indentWithTabs:    useTabs,
    styleActiveLine:   true,
    autoCloseBrackets: true,
    theme:             "mozilla"
  };

  // Additional shortcuts.
  this.config.extraKeys[Editor.keyFor("jumpToLine")] = (cm) => this.jumpToLine(cm);
  this.config.extraKeys[Editor.keyFor("moveLineUp")] = (cm) => this.moveLineUp();
  this.config.extraKeys[Editor.keyFor("moveLineDown")] = (cm) => this.moveLineDown();
  this.config.extraKeys[Editor.keyFor("toggleComment")] = "toggleComment";

  // Disable ctrl-[ and ctrl-] because toolbox uses those shortcuts.
  this.config.extraKeys[Editor.keyFor("indentLess")] = false;
  this.config.extraKeys[Editor.keyFor("indentMore")] = false;

  // If alternative keymap is provided, use it.
  if (keyMap === "emacs" || keyMap === "vim")
    this.config.keyMap = keyMap;

  // Overwrite default config with user-provided, if needed.
  Object.keys(config).forEach((k) => {
    if (k != "extraKeys") {
      this.config[k] = config[k];
      return;
    }

    if (!config.extraKeys)
      return;

    Object.keys(config.extraKeys).forEach((key) => {
      this.config.extraKeys[key] = config.extraKeys[key];
    });
  });

  // Overwrite default tab behavior. If something is selected,
  // indent those lines. If nothing is selected and we're
  // indenting with tabs, insert one tab. Otherwise insert N
  // whitespaces where N == indentUnit option.
  this.config.extraKeys.Tab = (cm) => {
    if (cm.somethingSelected()) {
      cm.indentSelection("add");
      return;
    }

    if (this.config.indentWithTabs) {
      cm.replaceSelection("\t", "end", "+input");
      return;
    }

    var num = cm.getOption("indentUnit");
    if (cm.getCursor().ch !== 0) num -= 1;
    cm.replaceSelection(" ".repeat(num), "end", "+input");
  };

  events.decorate(this);
}

Editor.prototype = {
  container: null,
  version: null,
  config: null,

  /**
   * Appends the current Editor instance to the element specified by
   * 'el'. You can also provide your won iframe to host the editor as
   * an optional second parameter. This method actually creates and
   * loads CodeMirror and all its dependencies.
   *
   * This method is asynchronous and returns a promise.
   */
  appendTo: function (el, env) {
    let def = promise.defer();
    let cm  = editors.get(this);

    if (!env)
      env = el.ownerDocument.createElementNS(XUL_NS, "iframe");

    env.flex = 1;

    if (cm)
      throw new Error("You can append an editor only once.");

    let onLoad = () => {
      // Once the iframe is loaded, we can inject CodeMirror
      // and its dependencies into its DOM.

      env.removeEventListener("load", onLoad, true);
      let win = env.contentWindow.wrappedJSObject;

      CM_SCRIPTS.forEach((url) =>
        Services.scriptloader.loadSubScript(url, win, "utf8"));

      // Replace the propertyKeywords, colorKeywords and valueKeywords
      // properties of the CSS MIME type with the values provided by Gecko.
      let cssSpec = win.CodeMirror.resolveMode("text/css");
      cssSpec.propertyKeywords = cssProperties;
      cssSpec.colorKeywords = cssColors;
      cssSpec.valueKeywords = cssValues;
      win.CodeMirror.defineMIME("text/css", cssSpec);

      let scssSpec = win.CodeMirror.resolveMode("text/x-scss");
      scssSpec.propertyKeywords = cssProperties;
      scssSpec.colorKeywords = cssColors;
      scssSpec.valueKeywords = cssValues;
      win.CodeMirror.defineMIME("text/x-scss", scssSpec);

      win.CodeMirror.commands.save = () => this.emit("save");

      // Create a CodeMirror instance add support for context menus,
      // overwrite the default controller (otherwise items in the top and
      // context menus won't work).

      cm = win.CodeMirror(win.document.body, this.config);
      cm.getWrapperElement().addEventListener("contextmenu", (ev) => {
        ev.preventDefault();
        this.showContextMenu(el.ownerDocument, ev.screenX, ev.screenY);
      }, false);

      cm.on("focus", () => this.emit("focus"));
      cm.on("scroll", () => this.emit("scroll"));
      cm.on("change", () => {
        this.emit("change");
        if (!this._lastDirty) {
          this._lastDirty = true;
          this.emit("dirty-change");
        }
      });
      cm.on("cursorActivity", (cm) => this.emit("cursorActivity"));

      cm.on("gutterClick", (cm, line, gutter, ev) => {
        let head = { line: line, ch: 0 };
        let tail = { line: line, ch: this.getText(line).length };

        // Shift-click on a gutter selects the whole line.
        if (ev.shiftKey) {
          cm.setSelection(head, tail);
          return;
        }

        this.emit("gutterClick", line);
      });

      win.CodeMirror.defineExtension("l10n", (name) => {
        return L10N.GetStringFromName(name);
      });

      cm.getInputField().controllers.insertControllerAt(0, controller(this));

      this.container = env;
      editors.set(this, cm);
      def.resolve();
    };

    env.addEventListener("load", onLoad, true);
    env.setAttribute("src", CM_IFRAME);
    el.appendChild(env);

    this.once("destroy", () => el.removeChild(env));
    return def.promise;
  },

  /**
   * Returns the currently active highlighting mode.
   * See Editor.modes for the list of all suppoert modes.
   */
  getMode: function () {
    return this.getOption("mode");
  },

  /**
   * Changes the value of a currently used highlighting mode.
   * See Editor.modes for the list of all suppoert modes.
   */
  setMode: function (value) {
    this.setOption("mode", value);
  },

  /**
   * Returns text from the text area. If line argument is provided
   * the method returns only that line.
   */
  getText: function (line) {
    let cm = editors.get(this);

    if (line == null)
      return cm.getValue();

    let info = cm.lineInfo(line);
    return info ? cm.lineInfo(line).text : "";
  },

  /**
   * Replaces whatever is in the text area with the contents of
   * the 'value' argument.
   */
  setText: function (value) {
    let cm = editors.get(this);
    cm.setValue(value);
  },

  /**
   * Replaces contents of a text area within the from/to {line, ch}
   * range. If neither from nor to arguments are provided works
   * exactly like setText. If only from object is provided, inserts
   * text at that point, *overwriting* as many characters as needed.
   */
  replaceText: function (value, from, to) {
    let cm = editors.get(this);

    if (!from) {
      this.setText(value);
      return;
    }

    if (!to) {
      let text = cm.getRange({ line: 0, ch: 0 }, from);
      this.setText(text + value);
      return;
    }

    cm.replaceRange(value, from, to);
  },

  /**
   * Inserts text at the specified {line, ch} position, shifting existing
   * contents as necessary.
   */
  insertText: function (value, at) {
    let cm = editors.get(this);
    cm.replaceRange(value, at, at);
  },

  /**
   * Deselects contents of the text area.
   */
  dropSelection: function () {
    if (!this.somethingSelected())
      return;

    this.setCursor(this.getCursor());
  },

  /**
   * Gets the first visible line number in the editor.
   */
  getFirstVisibleLine: function () {
    let cm = editors.get(this);
    return cm.lineAtHeight(0, "local");
  },

  /**
   * Scrolls the view such that the given line number is the first visible line.
   */
  setFirstVisibleLine: function (line) {
    let cm = editors.get(this);
    let { top } = cm.charCoords({line: line, ch: 0}, "local");
    cm.scrollTo(0, top);
  },

  /**
   * Sets the cursor to the specified {line, ch} position with an additional
   * option to align the line at the "top", "center" or "bottom" of the editor
   * with "top" being default value.
   */
  setCursor: function ({line, ch}, align) {
    let cm = editors.get(this);
    this.alignLine(line, align);
    cm.setCursor({line: line, ch: ch});
  },

  /**
   * Aligns the provided line to either "top", "center" or "bottom" of the
   * editor view with a maximum margin of MAX_VERTICAL_OFFSET lines from top or
   * bottom.
   */
  alignLine: function(line, align) {
    let cm = editors.get(this);
    let from = cm.lineAtHeight(0, "page");
    let to = cm.lineAtHeight(cm.getWrapperElement().clientHeight, "page");
    let linesVisible = to - from;
    let halfVisible = Math.round(linesVisible/2);

    // If the target line is in view, skip the vertical alignment part.
    if (line <= to && line >= from) {
      return;
    }

    // Setting the offset so that the line always falls in the upper half
    // of visible lines (lower half for bottom aligned).
    // MAX_VERTICAL_OFFSET is the maximum allowed value.
    let offset = Math.min(halfVisible, MAX_VERTICAL_OFFSET);

    let topLine = {
      "center": Math.max(line - halfVisible, 0),
      "bottom": Math.max(line - linesVisible + offset, 0),
      "top": Math.max(line - offset, 0)
    }[align || "top"] || offset;

    // Bringing down the topLine to total lines in the editor if exceeding.
    topLine = Math.min(topLine, this.lineCount());
    this.setFirstVisibleLine(topLine);
  },

  /**
   * Returns whether a marker of a specified class exists in a line's gutter.
   */
  hasMarker: function (line, gutterName, markerClass) {
    let cm = editors.get(this);
    let info = cm.lineInfo(line);
    if (!info)
      return false;

    let gutterMarkers = info.gutterMarkers;
    if (!gutterMarkers)
      return false;

    let marker = gutterMarkers[gutterName];
    if (!marker)
      return false;

    return marker.classList.contains(markerClass);
  },

  /**
   * Adds a marker with a specified class to a line's gutter. If another marker
   * exists on that line, the new marker class is added to its class list.
   */
  addMarker: function (line, gutterName, markerClass) {
    let cm = editors.get(this);
    let info = cm.lineInfo(line);
    if (!info)
      return;

    let gutterMarkers = info.gutterMarkers;
    if (gutterMarkers) {
      let marker = gutterMarkers[gutterName];
      if (marker) {
        marker.classList.add(markerClass);
        return;
      }
    }

    let marker = cm.getWrapperElement().ownerDocument.createElement("div");
    marker.className = markerClass;
    cm.setGutterMarker(info.line, gutterName, marker);
  },

  /**
   * The reverse of addMarker. Removes a marker of a specified class from a
   * line's gutter.
   */
  removeMarker: function (line, gutterName, markerClass) {
    if (!this.hasMarker(line, gutterName, markerClass))
      return;

    let cm = editors.get(this);
    cm.lineInfo(line).gutterMarkers[gutterName].classList.remove(markerClass);
  },

  /**
   * Remove all gutter markers in the gutter with the given name.
   */
  removeAllMarkers: function (gutterName) {
    let cm = editors.get(this);
    cm.clearGutter(gutterName);
  },

  /**
   * Handles attaching a set of events listeners on a marker. They should
   * be passed as an object literal with keys as event names and values as
   * function listeners. The line number, marker node and optional data
   * will be passed as arguments to the function listener.
   *
   * You don't need to worry about removing these event listeners.
   * They're automatically orphaned when clearing markers.
   */
  setMarkerListeners: function(line, gutterName, markerClass, events, data) {
    if (!this.hasMarker(line, gutterName, markerClass))
      return;

    let cm = editors.get(this);
    let marker = cm.lineInfo(line).gutterMarkers[gutterName];

    for (let name in events) {
      let listener = events[name].bind(this, line, marker, data);
      marker.addEventListener(name, listener);
    }
  },

  /**
   * Returns whether a line is decorated using the specified class name.
   */
  hasLineClass: function (line, className) {
    let cm = editors.get(this);
    let info = cm.lineInfo(line);

    if (!info || !info.wrapClass)
      return false;

    return info.wrapClass.split(" ").indexOf(className) != -1;
  },

  /**
   * Set a CSS class name for the given line, including the text and gutter.
   */
  addLineClass: function (line, className) {
    let cm = editors.get(this);
    cm.addLineClass(line, "wrap", className);
  },

  /**
   * The reverse of addLineClass.
   */
  removeLineClass: function (line, className) {
    let cm = editors.get(this);
    cm.removeLineClass(line, "wrap", className);
  },

  /**
   * Mark a range of text inside the two {line, ch} bounds. Since the range may
   * be modified, for example, when typing text, this method returns a function
   * that can be used to remove the mark.
   */
  markText: function(from, to, className = "marked-text") {
    let cm = editors.get(this);
    let mark = cm.markText(from, to, { className: className });
    return { clear: () => mark.clear() };
  },

  /**
   * Calculates and returns one or more {line, ch} objects for
   * a zero-based index who's value is relative to the start of
   * the editor's text.
   *
   * If only one argument is given, this method returns a single
   * {line,ch} object. Otherwise it returns an array.
   */
  getPosition: function (...args) {
    let cm = editors.get(this);
    let res = args.map((ind) => cm.posFromIndex(ind));
    return args.length === 1 ? res[0] : res;
  },

  /**
   * The reverse of getPosition. Similarly to getPosition this
   * method returns a single value if only one argument was given
   * and an array otherwise.
   */
  getOffset: function (...args) {
    let cm = editors.get(this);
    let res = args.map((pos) => cm.indexFromPos(pos));
    return args.length > 1 ? res : res[0];
  },

  /**
   * Returns a {line, ch} object that corresponds to the
   * left, top coordinates.
   */
  getPositionFromCoords: function ({left, top}) {
    let cm = editors.get(this);
    return cm.coordsChar({ left: left, top: top });
  },

  /**
   * The reverse of getPositionFromCoords. Similarly, returns a {left, top}
   * object that corresponds to the specified line and character number.
   */
  getCoordsFromPosition: function ({line, ch}) {
    let cm = editors.get(this);
    return cm.charCoords({ line: ~~line, ch: ~~ch });
  },

  /**
   * Returns true if there's something to undo and false otherwise.
   */
  canUndo: function () {
    let cm = editors.get(this);
    return cm.historySize().undo > 0;
  },

  /**
   * Returns true if there's something to redo and false otherwise.
   */
  canRedo: function () {
    let cm = editors.get(this);
    return cm.historySize().redo > 0;
  },

  /**
   * Marks the contents as clean and returns the current
   * version number.
   */
  setClean: function () {
    let cm = editors.get(this);
    this.version = cm.changeGeneration();
    this._lastDirty = false;
    this.emit("dirty-change");
    return this.version;
  },

  /**
   * Returns true if contents of the text area are
   * clean i.e. no changes were made since the last version.
   */
  isClean: function () {
    let cm = editors.get(this);
    return cm.isClean(this.version);
  },

  /**
   * True if the editor is in the read-only mode, false otherwise.
   */
  isReadOnly: function () {
    return this.getOption("readOnly");
  },

  /**
   * Displays a context menu at the point x:y. The first
   * argument, container, should be a DOM node that contains
   * a context menu element specified by the ID from
   * config.contextMenu.
   */
  showContextMenu: function (container, x, y) {
    if (this.config.contextMenu == null)
      return;

    let popup = container.getElementById(this.config.contextMenu);
    popup.openPopupAtScreen(x, y, true);
  },

  /**
   * This method opens an in-editor dialog asking for a line to
   * jump to. Once given, it changes cursor to that line.
   */
  jumpToLine: function (cm) {
    let doc = cm.getWrapperElement().ownerDocument;
    let div = doc.createElement("div");
    let inp = doc.createElement("input");
    let txt = doc.createTextNode(L10N.GetStringFromName("gotoLineCmd.promptTitle"));

    inp.type = "text";
    inp.style.width = "10em";
    inp.style.MozMarginStart = "1em";

    div.appendChild(txt);
    div.appendChild(inp);

    this.openDialog(div, (line) => this.setCursor({ line: line - 1, ch: 0 }));
  },

  /**
   * Moves the content of the current line or the lines selected up a line.
   */
  moveLineUp: function () {
    let cm = editors.get(this);
    let start = cm.getCursor("start");
    let end = cm.getCursor("end");

    if (start.line === 0)
      return;

    // Get the text in the lines selected or the current line of the cursor
    // and append the text of the previous line.
    let value;
    if (start.line !== end.line) {
      value = cm.getRange({ line: start.line, ch: 0 },
        { line: end.line, ch: cm.getLine(end.line).length }) + "\n";
    } else {
      value = cm.getLine(start.line) + "\n";
    }
    value += cm.getLine(start.line - 1);

    // Replace the previous line and the currently selected lines with the new
    // value and maintain the selection of the text.
    cm.replaceRange(value, { line: start.line - 1, ch: 0 },
      { line: end.line, ch: cm.getLine(end.line).length });
    cm.setSelection({ line: start.line - 1, ch: start.ch },
      { line: end.line - 1, ch: end.ch });
  },

  /**
   * Moves the content of the current line or the lines selected down a line.
   */
  moveLineDown: function () {
    let cm = editors.get(this);
    let start = cm.getCursor("start");
    let end = cm.getCursor("end");

    if (end.line + 1 === cm.lineCount())
      return;

    // Get the text of next line and append the text in the lines selected
    // or the current line of the cursor.
    let value = cm.getLine(end.line + 1) + "\n";
    if (start.line !== end.line) {
      value += cm.getRange({ line: start.line, ch: 0 },
        { line: end.line, ch: cm.getLine(end.line).length });
    } else {
      value += cm.getLine(start.line);
    }

    // Replace the currently selected lines and the next line with the new
    // value and maintain the selection of the text.
    cm.replaceRange(value, { line: start.line, ch: 0 },
      { line: end.line + 1, ch: cm.getLine(end.line + 1).length});
    cm.setSelection({ line: start.line + 1, ch: start.ch },
      { line: end.line + 1, ch: end.ch });
  },

  /**
   * Extends an instance of the Editor object with additional
   * functions. Each function will be called with context as
   * the first argument. Context is a {ed, cm} object where
   * 'ed' is an instance of the Editor object and 'cm' is an
   * instance of the CodeMirror object. Example:
   *
   * function hello(ctx, name) {
   *   let { cm, ed } = ctx;
   *   cm;   // CodeMirror instance
   *   ed;   // Editor instance
   *   name; // 'Mozilla'
   * }
   *
   * editor.extend({ hello: hello });
   * editor.hello('Mozilla');
   */
  extend: function (funcs) {
    Object.keys(funcs).forEach((name) => {
      let cm  = editors.get(this);
      let ctx = { ed: this, cm: cm };

      if (name === "initialize") {
        funcs[name](ctx);
        return;
      }

      this[name] = funcs[name].bind(null, ctx);
    });
  },

  destroy: function () {
    this.container = null;
    this.config = null;
    this.version = null;
    this.emit("destroy");
  }
};

// Since Editor is a thin layer over CodeMirror some methods
// are mapped directly—without any changes.

CM_MAPPING.forEach(function (name) {
  Editor.prototype[name] = function (...args) {
    let cm = editors.get(this);
    return cm[name].apply(cm, args);
  };
});

// Static methods on the Editor object itself.

/**
 * Returns a string representation of a shortcut 'key' with
 * a OS specific modifier. Cmd- for Macs, Ctrl- for other
 * platforms. Useful with extraKeys configuration option.
 *
 * CodeMirror defines all keys with modifiers in the following
 * order: Shift - Ctrl/Cmd - Alt - Key
 */
Editor.accel = function (key, modifiers={}) {
  return (modifiers.shift ? "Shift-" : "") +
         (Services.appinfo.OS == "Darwin" ? "Cmd-" : "Ctrl-") +
         (modifiers.alt ? "Alt-" : "") + key;
};

/**
 * Returns a string representation of a shortcut for a
 * specified command 'cmd'. Cmd- for macs, Ctrl- for other
 * platforms. Useful when overwriting or disabling default
 * shortcuts.
 */
Editor.keyFor = function (cmd) {
  return Editor.accel(L10N.GetStringFromName(cmd + ".commandkey"));
};

// Since Gecko already provide complete and up to date list of CSS property
// names, values and color names, we compute them so that they can replace
// the ones used in CodeMirror while initiating an editor object. This is done
// here instead of the file codemirror/css.js so as to leave that file untouched
// and easily upgradable.
function getCSSKeywords() {
  function keySet(array) {
    var keys = {};
    for (var i = 0; i < array.length; ++i) {
      keys[array[i]] = true;
    }
    return keys;
  }

  let domUtils = Cc["@mozilla.org/inspector/dom-utils;1"]
                   .getService(Ci.inIDOMUtils);
  let cssProperties = domUtils.getCSSPropertyNames(domUtils.INCLUDE_ALIASES);
  let cssColors = {};
  let cssValues = {};
  cssProperties.forEach(property => {
    if (property.contains("color")) {
      domUtils.getCSSValuesForProperty(property).forEach(value => {
        cssColors[value] = true;
      });
    }
    else {
      domUtils.getCSSValuesForProperty(property).forEach(value => {
        cssValues[value] = true;
      });
    }
  });
  return {
    cssProperties: keySet(cssProperties),
    cssValues: cssValues,
    cssColors: cssColors
  };
}

/**
 * Returns a controller object that can be used for
 * editor-specific commands such as find, jump to line,
 * copy/paste, etc.
 */
function controller(ed) {
  return {
    supportsCommand: function (cmd) {
      switch (cmd) {
        case "cmd_find":
        case "cmd_findAgain":
        case "cmd_findPrevious":
        case "cmd_gotoLine":
        case "cmd_undo":
        case "cmd_redo":
        case "cmd_delete":
        case "cmd_selectAll":
          return true;
      }

      return false;
    },

    isCommandEnabled: function (cmd) {
      let cm = editors.get(ed);

      switch (cmd) {
        case "cmd_find":
        case "cmd_gotoLine":
        case "cmd_selectAll":
          return true;
        case "cmd_findAgain":
          return cm.state.search != null && cm.state.search.query != null;
        case "cmd_undo":
          return ed.canUndo();
        case "cmd_redo":
          return ed.canRedo();
        case "cmd_delete":
          return ed.somethingSelected();
      }

      return false;
    },

    doCommand: function (cmd) {
      let cm  = editors.get(ed);
      let map = {
        "cmd_selectAll": "selectAll",
        "cmd_find": "find",
        "cmd_undo": "undo",
        "cmd_redo": "redo",
        "cmd_delete": "delCharAfter",
        "cmd_findAgain": "findNext"
      };

      if (map[cmd]) {
        cm.execCommand(map[cmd]);
        return;
      }

      if (cmd == "cmd_gotoLine")
        ed.jumpToLine(cm);
    },

    onEvent: function () {}
  };
}

module.exports = Editor;
