/* -*- indent-tabs-mode: nil; js-indent-level: 2; fill-column: 80 -*- */
/* vim:set ts=2 sw=2 sts=2 et tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cu, Cc, Ci, components } = require("chrome");

const {
  EXPAND_TAB,
  TAB_SIZE,
  DETECT_INDENT,
  getIndentationFromIteration
} = require("devtools/toolkit/shared/indentation");

const ENABLE_CODE_FOLDING = "devtools.editor.enableCodeFolding";
const KEYMAP      = "devtools.editor.keymap";
const AUTO_CLOSE  = "devtools.editor.autoclosebrackets";
const AUTOCOMPLETE  = "devtools.editor.autocomplete";
const L10N_BUNDLE = "chrome://browser/locale/devtools/sourceeditor.properties";
const XUL_NS      = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
const VALID_KEYMAPS = new Set(["emacs", "vim", "sublime"]);

// Maximum allowed margin (in number of lines) from top or bottom of the editor
// while shifting to a line which was initially out of view.
const MAX_VERTICAL_OFFSET = 3;

// Match @Scratchpad/N:LINE[:COLUMN] or (LINE[:COLUMN]) anywhere at an end of
// line in text selection.
const RE_SCRATCHPAD_ERROR = /(?:@Scratchpad\/\d+:|\()(\d+):?(\d+)?(?:\)|\n)/;
const RE_JUMP_TO_LINE = /^(\d+):?(\d+)?/;

const promise = require("promise");
const events  = require("devtools/toolkit/event-emitter");
const { PrefObserver } = require("devtools/styleeditor/utils");

Cu.import("resource://gre/modules/Services.jsm");
const L10N = Services.strings.createBundle(L10N_BUNDLE);

const { OS } = Services.appinfo;

// CM_STYLES, CM_SCRIPTS and CM_IFRAME represent the HTML,
// JavaScript and CSS that is injected into an iframe in
// order to initialize a CodeMirror instance.

const CM_STYLES   = [
  "chrome://browser/skin/devtools/common.css",
  "chrome://devtools/content/sourceeditor/codemirror/codemirror.css",
  "chrome://devtools/content/sourceeditor/codemirror/dialog/dialog.css",
  "chrome://devtools/content/sourceeditor/codemirror/mozilla.css"
];

const CM_SCRIPTS  = [
  "chrome://devtools/content/shared/theme-switching.js",
  "chrome://devtools/content/sourceeditor/codemirror/codemirror.js",
  "chrome://devtools/content/sourceeditor/codemirror/dialog/dialog.js",
  "chrome://devtools/content/sourceeditor/codemirror/search/searchcursor.js",
  "chrome://devtools/content/sourceeditor/codemirror/search/search.js",
  "chrome://devtools/content/sourceeditor/codemirror/edit/matchbrackets.js",
  "chrome://devtools/content/sourceeditor/codemirror/edit/closebrackets.js",
  "chrome://devtools/content/sourceeditor/codemirror/comment/comment.js",
  "chrome://devtools/content/sourceeditor/codemirror/mode/javascript.js",
  "chrome://devtools/content/sourceeditor/codemirror/mode/xml.js",
  "chrome://devtools/content/sourceeditor/codemirror/mode/css.js",
  "chrome://devtools/content/sourceeditor/codemirror/mode/htmlmixed.js",
  "chrome://devtools/content/sourceeditor/codemirror/mode/clike.js",
  "chrome://devtools/content/sourceeditor/codemirror/selection/active-line.js",
  "chrome://devtools/content/sourceeditor/codemirror/edit/trailingspace.js",
  "chrome://devtools/content/sourceeditor/codemirror/keymap/emacs.js",
  "chrome://devtools/content/sourceeditor/codemirror/keymap/vim.js",
  "chrome://devtools/content/sourceeditor/codemirror/keymap/sublime.js",
  "chrome://devtools/content/sourceeditor/codemirror/fold/foldcode.js",
  "chrome://devtools/content/sourceeditor/codemirror/fold/brace-fold.js",
  "chrome://devtools/content/sourceeditor/codemirror/fold/comment-fold.js",
  "chrome://devtools/content/sourceeditor/codemirror/fold/xml-fold.js",
  "chrome://devtools/content/sourceeditor/codemirror/fold/foldgutter.js"
];

const CM_IFRAME   =
  "data:text/html;charset=utf8,<!DOCTYPE html>" +
  "<html dir='ltr'>" +
  "  <head>" +
  "    <style>" +
  "      html, body { height: 100%; }" +
  "      body { margin: 0; overflow: hidden; }" +
  "      .CodeMirror { width: 100%; height: 100% !important; line-height: 1.25 !important;}" +
  "    </style>" +
  CM_STYLES.map(style => "<link rel='stylesheet' href='" + style + "'>").join("\n") +
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
  "getScrollInfo",
  "getViewport"
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
 * property contextMenu. This property should be an element, or
 * an ID of an element that we can use as a context menu.
 *
 * This object is also an event emitter.
 *
 * CodeMirror docs: http://codemirror.net/doc/manual.html
 */
function Editor(config) {
  const tabSize = Services.prefs.getIntPref(TAB_SIZE);
  const useTabs = !Services.prefs.getBoolPref(EXPAND_TAB);
  const useAutoClose = Services.prefs.getBoolPref(AUTO_CLOSE);

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
    autoCloseBrackets: "()[]{}''\"\"``",
    autoCloseEnabled:  useAutoClose,
    theme:             "mozilla",
    themeSwitching:    true,
    autocomplete:      false,
    autocompleteOpts:  {}
  };

  // Additional shortcuts.
  this.config.extraKeys[Editor.keyFor("jumpToLine")] = () => this.jumpToLine();
  this.config.extraKeys[Editor.keyFor("moveLineUp", { noaccel: true })] = () => this.moveLineUp();
  this.config.extraKeys[Editor.keyFor("moveLineDown", { noaccel: true })] = () => this.moveLineDown();
  this.config.extraKeys[Editor.keyFor("toggleComment")] = "toggleComment";

  // Disable ctrl-[ and ctrl-] because toolbox uses those shortcuts.
  this.config.extraKeys[Editor.keyFor("indentLess")] = false;
  this.config.extraKeys[Editor.keyFor("indentMore")] = false;


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

  if (!this.config.gutters) {
    this.config.gutters = [];
  }
  if (this.config.lineNumbers
      && this.config.gutters.indexOf("CodeMirror-linenumbers") === -1) {
    this.config.gutters.push("CodeMirror-linenumbers");
  }

  // Remember the initial value of autoCloseBrackets.
  this.config.autoCloseBracketsSaved = this.config.autoCloseBrackets;

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

  // Allow add-ons to inject scripts for their editor instances
  if (!this.config.externalScripts) {
    this.config.externalScripts = [];
  }

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

      if (!this.config.themeSwitching)
        win.document.documentElement.setAttribute("force-theme", "light");

      let scriptsToInject = CM_SCRIPTS.concat(this.config.externalScripts);
      scriptsToInject.forEach((url) => {
        if (url.startsWith("chrome://"))
          Services.scriptloader.loadSubScript(url, win, "utf8");
      });
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

      win.CodeMirror.commands.save = () => this.emit("saveRequested");

      // Create a CodeMirror instance add support for context menus,
      // overwrite the default controller (otherwise items in the top and
      // context menus won't work).

      cm = win.CodeMirror(win.document.body, this.config);
      cm.getWrapperElement().addEventListener("contextmenu", (ev) => {
        ev.preventDefault();
        if (!this.config.contextMenu) return;
        let popup = this.config.contextMenu;
        if (typeof popup == "string")
          popup = el.ownerDocument.getElementById(this.config.contextMenu);
        popup.openPopupAtScreen(ev.screenX, ev.screenY, true);
      }, false);

      // Intercept the find and find again keystroke on CodeMirror, to avoid
      // the browser's search

      let findKey = L10N.GetStringFromName("find.commandkey");
      let findAgainKey = L10N.GetStringFromName("findAgain.commandkey");
      let [accel, modifier] = OS === "Darwin"
                                      ? ["metaKey", "altKey"]
                                      : ["ctrlKey", "shiftKey"];

      cm.getWrapperElement().addEventListener("keydown", (ev) => {
        let key = ev.key.toUpperCase();
        let node = ev.originalTarget;
        let isInput = node.tagName === "INPUT";
        let isSearchInput = isInput && node.type === "search";

        // replace box is a different input instance than search, and it is
        // located in a code mirror dialog
        let isDialogInput = isInput &&
                       node.parentNode &&
                       node.parentNode.classList.contains("CodeMirror-dialog");

        if (!ev[accel] || !(isSearchInput || isDialogInput)) return;

        if (key === findKey) {
          ev.preventDefault();

          if (isSearchInput || ev[modifier]) {
            node.select();
          }
        } else if (key === findAgainKey) {
          ev.preventDefault();

          if (!isSearchInput) return;

          let query = node.value;

          // If there isn't a search state, or the text in the input does not
          // match with the current search state, we need to create a new one
          if (!cm.state.search || cm.state.search.query !== query) {
            cm.state.search = {
              posFrom: null,
              posTo: null,
              overlay: null,
              query
            };
          }

          if (ev.shiftKey) {
            cm.execCommand("findPrev");
          } else {
            cm.execCommand("findNext");
          }
        }
      });


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

        this.emit("gutterClick", line, ev.button);
      });

      win.CodeMirror.defineExtension("l10n", (name) => {
        return L10N.GetStringFromName(name);
      });

      cm.getInputField().controllers.insertControllerAt(0, controller(this));

      this.container = env;
      editors.set(this, cm);

      this.reloadPreferences = this.reloadPreferences.bind(this);
      this._prefObserver = new PrefObserver("devtools.editor.");
      this._prefObserver.on(TAB_SIZE, this.reloadPreferences);
      this._prefObserver.on(EXPAND_TAB, this.reloadPreferences);
      this._prefObserver.on(KEYMAP, this.reloadPreferences);
      this._prefObserver.on(AUTO_CLOSE, this.reloadPreferences);
      this._prefObserver.on(AUTOCOMPLETE, this.reloadPreferences);
      this._prefObserver.on(DETECT_INDENT, this.reloadPreferences);
      this._prefObserver.on(ENABLE_CODE_FOLDING, this.reloadPreferences);

      this.reloadPreferences();

      win.editor = this;
      let editorReadyEvent = new win.CustomEvent("editorReady");
      win.dispatchEvent(editorReadyEvent);

      def.resolve();
    };

    env.addEventListener("load", onLoad, true);
    env.setAttribute("src", CM_IFRAME);
    el.appendChild(env);

    this.once("destroy", () => el.removeChild(env));
    return def.promise;
  },

  /**
   * Returns a boolean indicating whether the editor is ready to
   * use.  Use appendTo(el).then(() => {}) for most cases
   */
  isAppended: function() {
    return editors.has(this);
  },

  /**
   * Returns the currently active highlighting mode.
   * See Editor.modes for the list of all suppoert modes.
   */
  getMode: function () {
    return this.getOption("mode");
  },

  /**
   * Load a script into editor's containing window.
   */
  loadScript: function (url) {
    if (!this.container) {
      throw new Error("Can't load a script until the editor is loaded.")
    }
    let win = this.container.contentWindow.wrappedJSObject;
    Services.scriptloader.loadSubScript(url, win, "utf8");
  },

  /**
   * Changes the value of a currently used highlighting mode.
   * See Editor.modes for the list of all supported modes.
   */
  setMode: function (value) {
    this.setOption("mode", value);

    // If autocomplete was set up and the mode is changing, then
    // turn it off and back on again so the proper mode can be used.
    if (this.config.autocomplete) {
      this.setOption("autocomplete", false);
      this.setOption("autocomplete", true);
    }
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

    this.resetIndentUnit();
  },

  /**
   * Reload the state of the editor based on all current preferences.
   * This is called automatically when any of the relevant preferences
   * change.
   */
  reloadPreferences: function() {
    // Restore the saved autoCloseBrackets value if it is preffed on.
    let useAutoClose = Services.prefs.getBoolPref(AUTO_CLOSE);
    this.setOption("autoCloseBrackets",
      useAutoClose ? this.config.autoCloseBracketsSaved : false);

    // If alternative keymap is provided, use it.
    const keyMap = Services.prefs.getCharPref(KEYMAP);
    if (VALID_KEYMAPS.has(keyMap))
      this.setOption("keyMap", keyMap)
    else
      this.setOption("keyMap", "default");
    this.updateCodeFoldingGutter();

    this.resetIndentUnit();
    this.setupAutoCompletion();
  },

  /**
   * Set the editor's indentation based on the current prefs and
   * re-detect indentation if we should.
   */
  resetIndentUnit: function() {
    let cm = editors.get(this);

    let iterFn = function(start, end, callback) {
      cm.eachLine(start, end, (line) => {
        return callback(line.text);
      });
    };

    let {indentUnit, indentWithTabs} = getIndentationFromIteration(iterFn);

    cm.setOption("tabSize", indentUnit);
    cm.setOption("indentUnit", indentUnit);
    cm.setOption("indentWithTabs", indentWithTabs);
  },

  /**
   * Replaces contents of a text area within the from/to {line, ch}
   * range. If neither `from` nor `to` arguments are provided works
   * exactly like setText. If only `from` object is provided, inserts
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
   * Returns true if there is more than one selection in the editor.
   */
  hasMultipleSelections: function () {
    let cm = editors.get(this);
    return cm.listSelections().length > 1;
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
    let marker = this.getMarker(line, gutterName);
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
   * Adds a marker with a specified class and an HTML content to a line's
   * gutter. If another marker exists on that line, it is overwritten by a new
   * marker.
   */
  addContentMarker: function (line, gutterName, markerClass, content) {
    let cm = editors.get(this);
    let info = cm.lineInfo(line);
    if (!info)
      return;

    let marker = cm.getWrapperElement().ownerDocument.createElement("div");
    marker.className = markerClass;
    marker.innerHTML = content;
    cm.setGutterMarker(info.line, gutterName, marker);
  },

  /**
   * The reverse of addContentMarker. Removes any line's markers in the
   * specified gutter.
   */
  removeContentMarker: function (line, gutterName) {
    let cm = editors.get(this);
    cm.setGutterMarker(info.line, gutterName, null);
  },

  getMarker: function(line, gutterName) {
    let cm = editors.get(this);
    let info = cm.lineInfo(line);
    if (!info)
      return null;

    let gutterMarkers = info.gutterMarkers;
    if (!gutterMarkers)
      return null;

    return gutterMarkers[gutterName];
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
    let text = cm.getRange(from, to);
    let span = cm.getWrapperElement().ownerDocument.createElement("span");
    span.className = className;
    span.textContent = text;

    let mark = cm.markText(from, to, { replacedWith: span });
    return {
      anchor: span,
      clear: () => mark.clear()
    };
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
   * This method opens an in-editor dialog asking for a line to
   * jump to. Once given, it changes cursor to that line.
   */
  jumpToLine: function () {
    let doc = editors.get(this).getWrapperElement().ownerDocument;
    let div = doc.createElement("div");
    let inp = doc.createElement("input");
    let txt = doc.createTextNode(L10N.GetStringFromName("gotoLineCmd.promptTitle"));

    inp.type = "text";
    inp.style.width = "10em";
    inp.style.MozMarginStart = "1em";

    div.appendChild(txt);
    div.appendChild(inp);

    if (!this.hasMultipleSelections()) {
      let cm = editors.get(this);
      let sel = cm.getSelection();
      // Scratchpad inserts and selects a comment after an error happens:
      // "@Scratchpad/1:10:2". Parse this to get the line and column.
      // In the string above this is line 10, column 2.
      let match = sel.match(RE_SCRATCHPAD_ERROR);
      if (match) {
        let [ , line, column ] = match;
        inp.value = column ? line + ":" + column : line;
        inp.selectionStart = inp.selectionEnd = inp.value.length;
      }
    }

    this.openDialog(div, (line) => {
      // Handle LINE:COLUMN as well as LINE
      let match = line.toString().match(RE_JUMP_TO_LINE);
      if (match) {
        let [ , line, column ] = match;
        this.setCursor({line: line - 1, ch: column ? column - 1 : 0 });
      }
    });
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
   * Returns current font size for the editor area, in pixels.
   */
  getFontSize: function () {
    let cm  = editors.get(this);
    let el  = cm.getWrapperElement();
    let win = el.ownerDocument.defaultView;

    return parseInt(win.getComputedStyle(el).getPropertyValue("font-size"), 10);
  },

  /**
   * Sets font size for the editor area.
   */
  setFontSize: function (size) {
    let cm = editors.get(this);
    cm.getWrapperElement().style.fontSize = parseInt(size, 10) + "px";
    cm.refresh();
  },

  /**
   * Sets an option for the editor.  For most options it just defers to
   * CodeMirror.setOption, but certain ones are maintained within the editor
   * instance.
   */
  setOption: function(o, v) {
    let cm = editors.get(this);

    // Save the state of a valid autoCloseBrackets string, so we can reset
    // it if it gets preffed off and back on.
    if (o === "autoCloseBrackets" && v) {
      this.config.autoCloseBracketsSaved = v;
    }

    if (o === "autocomplete") {
      this.config.autocomplete = v;
      this.setupAutoCompletion();
    } else {
      cm.setOption(o, v);
    }

    if (o === "enableCodeFolding") {
      // The new value maybe explicitly force foldGUtter on or off, ignoring
      // the prefs service.
      this.updateCodeFoldingGutter();
    }
  },

  /**
   * Gets an option for the editor.  For most options it just defers to
   * CodeMirror.getOption, but certain ones are maintained within the editor
   * instance.
   */
  getOption: function(o) {
    let cm = editors.get(this);
    if (o === "autocomplete") {
      return this.config.autocomplete;
    } else {
      return cm.getOption(o);
    }
  },

  /**
   * Sets up autocompletion for the editor. Lazily imports the required
   * dependencies because they vary by editor mode.
   *
   * Autocompletion is special, because we don't want to automatically use
   * it just because it is preffed on (it still needs to be requested by the
   * editor), but we do want to always disable it if it is preffed off.
   */
  setupAutoCompletion: function () {
    // The autocomplete module will overwrite this.initializeAutoCompletion
    // with a mode specific autocompletion handler.
    if (!this.initializeAutoCompletion) {
      this.extend(require("./autocomplete"));
    }

    if (this.config.autocomplete && Services.prefs.getBoolPref(AUTOCOMPLETE)) {
      this.initializeAutoCompletion(this.config.autocompleteOpts);
    } else {
      this.destroyAutoCompletion();
    }
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
      let ctx = { ed: this, cm: cm, Editor: Editor};

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

    if (this._prefObserver) {
      this._prefObserver.off(TAB_SIZE, this.reloadPreferences);
      this._prefObserver.off(EXPAND_TAB, this.reloadPreferences);
      this._prefObserver.off(KEYMAP, this.reloadPreferences);
      this._prefObserver.off(AUTO_CLOSE, this.reloadPreferences);
      this._prefObserver.off(AUTOCOMPLETE, this.reloadPreferences);
      this._prefObserver.off(DETECT_INDENT, this.reloadPreferences);
      this._prefObserver.off(ENABLE_CODE_FOLDING, this.reloadPreferences);
      this._prefObserver.destroy();
    }

    this.emit("destroy");
  },

  updateCodeFoldingGutter: function () {
    let shouldFoldGutter = this.config.enableCodeFolding,
        foldGutterIndex = this.config.gutters.indexOf("CodeMirror-foldgutter"),
        cm = editors.get(this);

    if (shouldFoldGutter === undefined) {
      shouldFoldGutter = Services.prefs.getBoolPref(ENABLE_CODE_FOLDING);
    }

    if (shouldFoldGutter) {
      // Add the gutter before enabling foldGutter
      if (foldGutterIndex === -1) {
        let gutters = this.config.gutters.slice();
        gutters.push("CodeMirror-foldgutter");
        this.setOption("gutters", gutters);
      }

      this.setOption("foldGutter", true);
    } else {
      // No code should remain folded when folding is off.
      if (cm) {
        cm.execCommand("unfoldAll");
      }

      // Remove the gutter so it doesn't take up space
      if (foldGutterIndex !== -1) {
        let gutters = this.config.gutters.slice();
        gutters.splice(foldGutterIndex, 1);
        this.setOption("gutters", gutters);
      }

      this.setOption("foldGutter", false);
    }
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
 * specified command 'cmd'. Append Cmd- for macs, Ctrl- for other
 * platforms unless noaccel is specified in the options. Useful when overwriting
 * or disabling default shortcuts.
 */
Editor.keyFor = function (cmd, opts={ noaccel: false }) {
  let key = L10N.GetStringFromName(cmd + ".commandkey");
  return opts.noaccel ? key : Editor.accel(key);
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
    if (property.includes("color")) {
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
        ed.jumpToLine();
    },

    onEvent: function () {}
  };
}

module.exports = Editor;
