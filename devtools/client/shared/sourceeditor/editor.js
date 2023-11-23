/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  EXPAND_TAB,
  TAB_SIZE,
  DETECT_INDENT,
  getIndentationFromIteration,
} = require("resource://devtools/shared/indentation.js");

const ENABLE_CODE_FOLDING = "devtools.editor.enableCodeFolding";
const KEYMAP_PREF = "devtools.editor.keymap";
const AUTO_CLOSE = "devtools.editor.autoclosebrackets";
const AUTOCOMPLETE = "devtools.editor.autocomplete";
const CARET_BLINK_TIME = "ui.caretBlinkTime";
const XHTML_NS = "http://www.w3.org/1999/xhtml";

const VALID_KEYMAPS = new Map([
  [
    "emacs",
    "chrome://devtools/content/shared/sourceeditor/codemirror/keymap/emacs.js",
  ],
  [
    "vim",
    "chrome://devtools/content/shared/sourceeditor/codemirror/keymap/vim.js",
  ],
  [
    "sublime",
    "chrome://devtools/content/shared/sourceeditor/codemirror/keymap/sublime.js",
  ],
]);

// Maximum allowed margin (in number of lines) from top or bottom of the editor
// while shifting to a line which was initially out of view.
const MAX_VERTICAL_OFFSET = 3;

const RE_JUMP_TO_LINE = /^(\d+):?(\d+)?/;
const AUTOCOMPLETE_MARK_CLASSNAME = "cm-auto-complete-shadow-text";

const EventEmitter = require("resource://devtools/shared/event-emitter.js");
const { PrefObserver } = require("resource://devtools/client/shared/prefs.js");
const KeyShortcuts = require("resource://devtools/client/shared/key-shortcuts.js");

const { LocalizationHelper } = require("resource://devtools/shared/l10n.js");
const L10N = new LocalizationHelper(
  "devtools/client/locales/sourceeditor.properties"
);

loader.lazyRequireGetter(
  this,
  "wasm",
  "resource://devtools/client/shared/sourceeditor/wasm.js"
);

const { OS } = Services.appinfo;

// CM_BUNDLE and CM_IFRAME represent the HTML and JavaScript that is
// injected into an iframe in order to initialize a CodeMirror instance.

const CM_BUNDLE =
  "chrome://devtools/content/shared/sourceeditor/codemirror/codemirror.bundle.js";

const CM_IFRAME =
  "chrome://devtools/content/shared/sourceeditor/codemirror/cmiframe.html";

const CM_MAPPING = [
  "clearHistory",
  "defaultCharWidth",
  "extendSelection",
  "focus",
  "getCursor",
  "getLine",
  "getScrollInfo",
  "getSelection",
  "getViewport",
  "hasFocus",
  "lineCount",
  "openDialog",
  "redo",
  "refresh",
  "replaceSelection",
  "setSelection",
  "somethingSelected",
  "undo",
];

const editors = new WeakMap();

Editor.modes = {
  cljs: { name: "text/x-clojure" },
  css: { name: "css" },
  fs: { name: "x-shader/x-fragment" },
  haxe: { name: "haxe" },
  http: { name: "http" },
  html: { name: "htmlmixed" },
  js: { name: "javascript" },
  text: { name: "text" },
  vs: { name: "x-shader/x-vertex" },
  wasm: { name: "wasm" },
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
    value: "",
    mode: Editor.modes.text,
    indentUnit: tabSize,
    tabSize,
    contextMenu: null,
    matchBrackets: true,
    highlightSelectionMatches: {
      wordsOnly: true,
    },
    extraKeys: {},
    indentWithTabs: useTabs,
    inputStyle: "accessibleTextArea",
    // This is set to the biggest value for setTimeout (See https://developer.mozilla.org/en-US/docs/Web/API/WindowOrWorkerGlobalScope/setTimeout#Maximum_delay_value)
    // This is because codeMirror queries the underlying textArea for some things that
    // can't be retrieved with events in some browser (but we're fine in Firefox).
    pollInterval: Math.pow(2, 31) - 1,
    styleActiveLine: true,
    autoCloseBrackets: "()[]{}''\"\"``",
    autoCloseEnabled: useAutoClose,
    theme: "mozilla",
    themeSwitching: true,
    autocomplete: false,
    autocompleteOpts: {},
    // Expect a CssProperties object (see devtools/client/fronts/css-properties.js)
    cssProperties: null,
    // Set to true to prevent the search addon to be activated.
    disableSearchAddon: false,
    maxHighlightLength: 1000,
    // Disable codeMirror setTimeout-based cursor blinking (will be replaced by a CSS animation)
    cursorBlinkRate: 0,
    // List of non-printable chars that will be displayed in the editor, showing their
    // unicode version. We only add a few characters to the default list:
    // - \u202d LEFT-TO-RIGHT OVERRIDE
    // - \u202e RIGHT-TO-LEFT OVERRIDE
    // - \u2066 LEFT-TO-RIGHT ISOLATE
    // - \u2067 RIGHT-TO-LEFT ISOLATE
    // - \u2069 POP DIRECTIONAL ISOLATE
    specialChars:
      // eslint-disable-next-line no-control-regex
      /[\u0000-\u001f\u007f-\u009f\u00ad\u061c\u200b-\u200f\u2028\u2029\u202d\u202e\u2066\u2067\u2069\ufeff\ufff9-\ufffc]/,
    specialCharPlaceholder: char => {
      // Use the doc provided to the setup function if we don't have a reference to a codeMirror
      // editor yet (this can happen when an Editor is being created with existing content)
      const doc = this._ownerDoc;
      const el = doc.createElement("span");
      el.classList.add("cm-non-printable-char");
      el.append(doc.createTextNode(`\\u${char.codePointAt(0).toString(16)}`));
      return el;
    },
  };

  // Additional shortcuts.
  this.config.extraKeys[Editor.keyFor("jumpToLine")] = () => this.jumpToLine();
  this.config.extraKeys[Editor.keyFor("moveLineUp", { noaccel: true })] = () =>
    this.moveLineUp();
  this.config.extraKeys[Editor.keyFor("moveLineDown", { noaccel: true })] =
    () => this.moveLineDown();
  this.config.extraKeys[Editor.keyFor("toggleComment")] = "toggleComment";

  // Disable ctrl-[ and ctrl-] because toolbox uses those shortcuts.
  this.config.extraKeys[Editor.keyFor("indentLess")] = false;
  this.config.extraKeys[Editor.keyFor("indentMore")] = false;

  // Disable Alt-B and Alt-F to navigate groups (respectively previous and next) since:
  // - it's not standard in input fields
  // - it also inserts a character which feels weird
  this.config.extraKeys["Alt-B"] = false;
  this.config.extraKeys["Alt-F"] = false;

  // Disable Ctrl/Cmd + U as it's used for "View Source". It's okay to disable Ctrl+U as
  // the underlying command, `undoSelection`, isn't standard in input fields and isn't
  // widely known.
  this.config.extraKeys[Editor.accel("U")] = false;

  // Disable keys that trigger events with a null-string `which` property.
  // It looks like some of those (e.g. the Function key), can trigger a poll
  // which fails to see that there's a selection, which end up replacing the
  // selected text with an empty string.
  // TODO: We should investigate the root cause.
  this.config.extraKeys["'\u0000'"] = false;

  // Overwrite default config with user-provided, if needed.
  Object.keys(config).forEach(k => {
    if (k != "extraKeys") {
      this.config[k] = config[k];
      return;
    }

    if (!config.extraKeys) {
      return;
    }

    Object.keys(config.extraKeys).forEach(key => {
      this.config.extraKeys[key] = config.extraKeys[key];
    });
  });

  if (!this.config.gutters) {
    this.config.gutters = [];
  }
  if (
    this.config.lineNumbers &&
    !this.config.gutters.includes("CodeMirror-linenumbers")
  ) {
    this.config.gutters.push("CodeMirror-linenumbers");
  }

  // Remember the initial value of autoCloseBrackets.
  this.config.autoCloseBracketsSaved = this.config.autoCloseBrackets;

  // Overwrite default tab behavior. If something is selected,
  // indent those lines. If nothing is selected and we're
  // indenting with tabs, insert one tab. Otherwise insert N
  // whitespaces where N == indentUnit option.
  this.config.extraKeys.Tab = cm => {
    if (config.extraKeys?.Tab) {
      // If a consumer registers its own extraKeys.Tab, we execute it before doing
      // anything else. If it returns false, that mean that all the key handling work is
      // done, so we can do an early return.
      const res = config.extraKeys.Tab(cm);
      if (res === false) {
        return;
      }
    }

    if (cm.somethingSelected()) {
      cm.indentSelection("add");
      return;
    }

    if (this.config.indentWithTabs) {
      cm.replaceSelection("\t", "end", "+input");
      return;
    }

    let num = cm.getOption("indentUnit");
    if (cm.getCursor().ch !== 0) {
      num -= cm.getCursor().ch % num;
    }
    cm.replaceSelection(" ".repeat(num), "end", "+input");
  };

  if (this.config.cssProperties) {
    // Ensure that autocompletion has cssProperties if it's passed in via the options.
    this.config.autocompleteOpts.cssProperties = this.config.cssProperties;
  }

  EventEmitter.decorate(this);
}

Editor.prototype = {
  container: null,
  version: null,
  config: null,
  Doc: null,

  /**
   * Exposes the CodeMirror class. We want to be able to
   * invoke static commands such as runMode for syntax highlighting.
   */
  get CodeMirror() {
    const codeMirror = editors.get(this);
    return codeMirror?.constructor;
  },

  /**
   * Exposes the CodeMirror instance. We want to get away from trying to
   * abstract away the API entirely, and this makes it easier to integrate in
   * various environments and do complex things.
   */
  get codeMirror() {
    if (!editors.has(this)) {
      throw new Error(
        "CodeMirror instance does not exist. You must wait " +
          "for it to be appended to the DOM."
      );
    }
    return editors.get(this);
  },

  /**
   * Return whether there is a CodeMirror instance associated with this Editor.
   */
  get hasCodeMirror() {
    return editors.has(this);
  },

  /**
   * Appends the current Editor instance to the element specified by
   * 'el'. You can also provide your own iframe to host the editor as
   * an optional second parameter. This method actually creates and
   * loads CodeMirror and all its dependencies.
   *
   * This method is asynchronous and returns a promise.
   */
  appendTo(el, env) {
    return new Promise(resolve => {
      const cm = editors.get(this);

      if (!env) {
        env = el.ownerDocument.createElementNS(XHTML_NS, "iframe");
        env.className = "source-editor-frame";
      }

      if (cm) {
        throw new Error("You can append an editor only once.");
      }

      const onLoad = () => {
        // Prevent flickering by showing the iframe once loaded.
        // See https://github.com/w3c/csswg-drafts/issues/9624
        env.style.visibility = "";
        const win = env.contentWindow.wrappedJSObject;
        this.container = env;
        this._setup(win.document.body, el.ownerDocument);
        resolve();
      };

      env.style.visibility = "hidden";
      env.addEventListener("load", onLoad, { capture: true, once: true });
      env.src = CM_IFRAME;
      el.appendChild(env);

      this.once("destroy", () => el.removeChild(env));
    });
  },

  appendToLocalElement(el) {
    this._setup(el);
  },

  /**
   * Do the actual appending and configuring of the CodeMirror instance. This is
   * used by both append functions above, and does all the hard work to
   * configure CodeMirror with all the right options/modes/etc.
   */
  _setup(el, doc) {
    this._ownerDoc = doc || el.ownerDocument;
    const win = el.ownerDocument.defaultView;

    Services.scriptloader.loadSubScript(CM_BUNDLE, win);

    if (this.config.cssProperties) {
      // Replace the propertyKeywords, colorKeywords and valueKeywords
      // properties of the CSS MIME type with the values provided by the CSS properties
      // database.
      const { propertyKeywords, colorKeywords, valueKeywords } = getCSSKeywords(
        this.config.cssProperties
      );

      const cssSpec = win.CodeMirror.resolveMode("text/css");
      cssSpec.propertyKeywords = propertyKeywords;
      cssSpec.colorKeywords = colorKeywords;
      cssSpec.valueKeywords = valueKeywords;
      win.CodeMirror.defineMIME("text/css", cssSpec);

      const scssSpec = win.CodeMirror.resolveMode("text/x-scss");
      scssSpec.propertyKeywords = propertyKeywords;
      scssSpec.colorKeywords = colorKeywords;
      scssSpec.valueKeywords = valueKeywords;
      win.CodeMirror.defineMIME("text/x-scss", scssSpec);
    }

    win.CodeMirror.commands.save = () => this.emit("saveRequested");

    // Create a CodeMirror instance add support for context menus,
    // overwrite the default controller (otherwise items in the top and
    // context menus won't work).

    const cm = win.CodeMirror(el, this.config);
    this.Doc = win.CodeMirror.Doc;

    // Disable APZ for source editors. It currently causes the line numbers to
    // "tear off" and swim around on top of the content. Bug 1160601 tracks
    // finding a solution that allows APZ to work with CodeMirror.
    cm.getScrollerElement().addEventListener("wheel", ev => {
      // By handling the wheel events ourselves, we force the platform to
      // scroll synchronously, like it did before APZ. However, we lose smooth
      // scrolling for users with mouse wheels. This seems acceptible vs.
      // doing nothing and letting the gutter slide around.
      ev.preventDefault();

      let { deltaX, deltaY } = ev;

      if (ev.deltaMode == ev.DOM_DELTA_LINE) {
        deltaX *= cm.defaultCharWidth();
        deltaY *= cm.defaultTextHeight();
      } else if (ev.deltaMode == ev.DOM_DELTA_PAGE) {
        deltaX *= cm.getWrapperElement().clientWidth;
        deltaY *= cm.getWrapperElement().clientHeight;
      }

      cm.getScrollerElement().scrollBy(deltaX, deltaY);
    });

    cm.getWrapperElement().addEventListener("contextmenu", ev => {
      if (!this.config.contextMenu) {
        return;
      }

      ev.stopPropagation();
      ev.preventDefault();

      let popup = this.config.contextMenu;
      if (typeof popup == "string") {
        popup = this._ownerDoc.getElementById(this.config.contextMenu);
      }

      this.emit("popupOpen", ev, popup);
      popup.openPopupAtScreen(ev.screenX, ev.screenY, true);
    });

    const pipedEvents = [
      "beforeChange",
      "blur",
      "changes",
      "cursorActivity",
      "focus",
      "keyHandled",
      "scroll",
    ];
    for (const eventName of pipedEvents) {
      cm.on(eventName, (...args) => this.emit(eventName, ...args));
    }

    cm.on("change", () => {
      this.emit("change");
      if (!this._lastDirty) {
        this._lastDirty = true;
        this.emit("dirty-change");
      }
    });

    cm.on("gutterClick", (cmArg, line, gutter, ev) => {
      const lineOrOffset = !this.isWasm ? line : this.lineToWasmOffset(line);
      this.emit("gutterClick", lineOrOffset, ev.button);
    });

    win.CodeMirror.defineExtension("l10n", name => {
      return L10N.getStr(name);
    });

    if (!this.config.disableSearchAddon) {
      this._initSearchShortcuts(win);
    } else {
      // Hotfix for Bug 1527898. We should remove those overrides as part of Bug 1527903.
      Object.assign(win.CodeMirror.commands, {
        find: null,
        findPersistent: null,
        findPersistentNext: null,
        findPersistentPrev: null,
        findNext: null,
        findPrev: null,
        clearSearch: null,
        replace: null,
        replaceAll: null,
      });
    }

    // Retrieve the cursor blink rate from user preference, or fall back to CodeMirror's
    // default value.
    let cursorBlinkingRate = win.CodeMirror.defaults.cursorBlinkRate;
    if (Services.prefs.prefHasUserValue(CARET_BLINK_TIME)) {
      cursorBlinkingRate = Services.prefs.getIntPref(
        CARET_BLINK_TIME,
        cursorBlinkingRate
      );
    }
    // This will be used in the animation-duration property we set on the cursor to
    // implement the blinking animation. If cursorBlinkingRate is 0 or less, the cursor
    // won't blink.
    cm.getWrapperElement().style.setProperty(
      "--caret-blink-time",
      `${Math.max(0, cursorBlinkingRate)}ms`
    );

    editors.set(this, cm);

    this.reloadPreferences = this.reloadPreferences.bind(this);
    this.setKeyMap = this.setKeyMap.bind(this, win);

    this._prefObserver = new PrefObserver("devtools.editor.");
    this._prefObserver.on(TAB_SIZE, this.reloadPreferences);
    this._prefObserver.on(EXPAND_TAB, this.reloadPreferences);
    this._prefObserver.on(AUTO_CLOSE, this.reloadPreferences);
    this._prefObserver.on(AUTOCOMPLETE, this.reloadPreferences);
    this._prefObserver.on(DETECT_INDENT, this.reloadPreferences);
    this._prefObserver.on(ENABLE_CODE_FOLDING, this.reloadPreferences);

    this.reloadPreferences();

    // Init a map of the loaded keymap files. Should be of the form Map<String->Boolean>.
    this._loadedKeyMaps = new Set();
    this._prefObserver.on(KEYMAP_PREF, this.setKeyMap);
    this.setKeyMap();

    win.editor = this;
    const editorReadyEvent = new win.CustomEvent("editorReady");
    win.dispatchEvent(editorReadyEvent);
  },

  /**
   * Returns a boolean indicating whether the editor is ready to
   * use. Use appendTo(el).then(() => {}) for most cases
   */
  isAppended() {
    return editors.has(this);
  },

  /**
   * Returns the currently active highlighting mode.
   * See Editor.modes for the list of all suppoert modes.
   */
  getMode() {
    return this.getOption("mode");
  },

  /**
   * Loads a script into editor's containing window.
   */
  loadScript(url) {
    if (!this.container) {
      throw new Error("Can't load a script until the editor is loaded.");
    }
    const win = this.container.contentWindow.wrappedJSObject;
    Services.scriptloader.loadSubScript(url, win);
  },

  /**
   * Creates a CodeMirror Document
   *
   * @param {String} text: Initial text of the document
   * @param {Object|String} mode: Mode of the document. See https://codemirror.net/5/doc/manual.html#option_mode
   * @returns CodeMirror.Doc
   */
  createDocument(text = "", mode) {
    return new this.Doc(text, mode);
  },

  /**
   * Replaces the current document with a new source document
   */
  replaceDocument(doc) {
    const cm = editors.get(this);
    cm.swapDoc(doc);
  },

  /**
   * Changes the value of a currently used highlighting mode.
   * See Editor.modes for the list of all supported modes.
   */
  setMode(value) {
    this.setOption("mode", value);

    // If autocomplete was set up and the mode is changing, then
    // turn it off and back on again so the proper mode can be used.
    if (this.config.autocomplete) {
      this.setOption("autocomplete", false);
      this.setOption("autocomplete", true);
    }
  },

  /**
   * The source editor can expose several commands linked from system and context menus.
   * Kept for backward compatibility with styleeditor.
   */
  insertCommandsController() {
    const {
      insertCommandsController,
    } = require("resource://devtools/client/shared/sourceeditor/editor-commands-controller.js");
    insertCommandsController(this);
  },

  /**
   * Returns text from the text area. If line argument is provided
   * the method returns only that line.
   */
  getText(line) {
    const cm = editors.get(this);

    if (line == null) {
      return cm.getValue();
    }

    const info = this.lineInfo(line);
    return info ? info.text : "";
  },

  getDoc() {
    const cm = editors.get(this);
    return cm.getDoc();
  },

  get isWasm() {
    return wasm.isWasm(this.getDoc());
  },

  wasmOffsetToLine(offset) {
    return wasm.wasmOffsetToLine(this.getDoc(), offset);
  },

  lineToWasmOffset(number) {
    return wasm.lineToWasmOffset(this.getDoc(), number);
  },

  toLineIfWasmOffset(maybeOffset) {
    if (typeof maybeOffset !== "number" || !this.isWasm) {
      return maybeOffset;
    }
    return this.wasmOffsetToLine(maybeOffset);
  },

  lineInfo(lineOrOffset) {
    const line = this.toLineIfWasmOffset(lineOrOffset);
    if (line == undefined) {
      return null;
    }
    const cm = editors.get(this);
    return cm.lineInfo(line);
  },

  getLineOrOffset(line) {
    return this.isWasm ? this.lineToWasmOffset(line) : line;
  },

  /**
   * Replaces whatever is in the text area with the contents of
   * the 'value' argument.
   */
  setText(value) {
    const cm = editors.get(this);

    if (typeof value !== "string" && "binary" in value) {
      // wasm?
      // binary does not survive as Uint8Array, converting from string
      const binary = value.binary;
      const data = new Uint8Array(binary.length);
      for (let i = 0; i < data.length; i++) {
        data[i] = binary.charCodeAt(i);
      }
      const { lines, done } = wasm.getWasmText(this.getDoc(), data);
      const MAX_LINES = 10000000;
      if (lines.length > MAX_LINES) {
        lines.splice(MAX_LINES, lines.length - MAX_LINES);
        lines.push(";; .... text is truncated due to the size");
      }
      if (!done) {
        lines.push(";; .... possible error during wast conversion");
      }
      // cm will try to split into lines anyway, saving memory
      value = { split: () => lines };
    }

    cm.setValue(value);

    this.resetIndentUnit();
  },

  /**
   * Reloads the state of the editor based on all current preferences.
   * This is called automatically when any of the relevant preferences
   * change.
   */
  reloadPreferences() {
    // Restore the saved autoCloseBrackets value if it is preffed on.
    const useAutoClose = Services.prefs.getBoolPref(AUTO_CLOSE);
    this.setOption(
      "autoCloseBrackets",
      useAutoClose ? this.config.autoCloseBracketsSaved : false
    );

    this.updateCodeFoldingGutter();

    this.resetIndentUnit();
    this.setupAutoCompletion();
  },

  /**
   * Set the current keyMap for CodeMirror, and load the support file if needed.
   *
   * @param {Window} win: The window on which the keymap files should be loaded.
   */
  setKeyMap(win) {
    if (this.config.isReadOnly) {
      return;
    }

    const keyMap = Services.prefs.getCharPref(KEYMAP_PREF);

    // If alternative keymap is provided, use it.
    if (VALID_KEYMAPS.has(keyMap)) {
      if (!this._loadedKeyMaps.has(keyMap)) {
        Services.scriptloader.loadSubScript(VALID_KEYMAPS.get(keyMap), win);
        this._loadedKeyMaps.add(keyMap);
      }
      this.setOption("keyMap", keyMap);
    } else {
      this.setOption("keyMap", "default");
    }
  },

  /**
   * Sets the editor's indentation based on the current prefs and
   * re-detect indentation if we should.
   */
  resetIndentUnit() {
    const cm = editors.get(this);

    const iterFn = function (start, end, callback) {
      cm.eachLine(start, end, line => {
        return callback(line.text);
      });
    };

    const { indentUnit, indentWithTabs } = getIndentationFromIteration(iterFn);

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
  replaceText(value, from, to) {
    const cm = editors.get(this);

    if (!from) {
      this.setText(value);
      return;
    }

    if (!to) {
      const text = cm.getRange({ line: 0, ch: 0 }, from);
      this.setText(text + value);
      return;
    }

    cm.replaceRange(value, from, to);
  },

  /**
   * Inserts text at the specified {line, ch} position, shifting existing
   * contents as necessary.
   */
  insertText(value, at) {
    const cm = editors.get(this);
    cm.replaceRange(value, at, at);
  },

  /**
   * Deselects contents of the text area.
   */
  dropSelection() {
    if (!this.somethingSelected()) {
      return;
    }

    this.setCursor(this.getCursor());
  },

  /**
   * Returns true if there is more than one selection in the editor.
   */
  hasMultipleSelections() {
    const cm = editors.get(this);
    return cm.listSelections().length > 1;
  },

  /**
   * Gets the first visible line number in the editor.
   */
  getFirstVisibleLine() {
    const cm = editors.get(this);
    return cm.lineAtHeight(0, "local");
  },

  /**
   * Scrolls the view such that the given line number is the first visible line.
   */
  setFirstVisibleLine(line) {
    const cm = editors.get(this);
    const { top } = cm.charCoords({ line, ch: 0 }, "local");
    cm.scrollTo(0, top);
  },

  /**
   * Sets the cursor to the specified {line, ch} position with an additional
   * option to align the line at the "top", "center" or "bottom" of the editor
   * with "top" being default value.
   */
  setCursor({ line, ch }, align) {
    const cm = editors.get(this);
    this.alignLine(line, align);
    cm.setCursor({ line, ch });
    this.emit("cursorActivity");
  },

  /**
   * Aligns the provided line to either "top", "center" or "bottom" of the
   * editor view with a maximum margin of MAX_VERTICAL_OFFSET lines from top or
   * bottom.
   */
  alignLine(line, align) {
    const cm = editors.get(this);
    const from = cm.lineAtHeight(0, "page");
    const to = cm.lineAtHeight(cm.getWrapperElement().clientHeight, "page");
    const linesVisible = to - from;
    const halfVisible = Math.round(linesVisible / 2);

    // If the target line is in view, skip the vertical alignment part.
    if (line <= to && line >= from) {
      return;
    }

    // Setting the offset so that the line always falls in the upper half
    // of visible lines (lower half for bottom aligned).
    // MAX_VERTICAL_OFFSET is the maximum allowed value.
    const offset = Math.min(halfVisible, MAX_VERTICAL_OFFSET);

    let topLine =
      {
        center: Math.max(line - halfVisible, 0),
        bottom: Math.max(line - linesVisible + offset, 0),
        top: Math.max(line - offset, 0),
      }[align || "top"] || offset;

    // Bringing down the topLine to total lines in the editor if exceeding.
    topLine = Math.min(topLine, this.lineCount());
    this.setFirstVisibleLine(topLine);
  },

  /**
   * Returns whether a marker of a specified class exists in a line's gutter.
   */
  hasMarker(line, gutterName, markerClass) {
    const marker = this.getMarker(line, gutterName);
    if (!marker) {
      return false;
    }

    return marker.classList.contains(markerClass);
  },

  /**
   * Adds a marker with a specified class to a line's gutter. If another marker
   * exists on that line, the new marker class is added to its class list.
   */
  addMarker(line, gutterName, markerClass) {
    const cm = editors.get(this);
    const info = this.lineInfo(line);
    if (!info) {
      return;
    }

    const gutterMarkers = info.gutterMarkers;
    let marker;
    if (gutterMarkers) {
      marker = gutterMarkers[gutterName];
      if (marker) {
        marker.classList.add(markerClass);
        return;
      }
    }

    marker = cm.getWrapperElement().ownerDocument.createElement("div");
    marker.className = markerClass;
    cm.setGutterMarker(info.line, gutterName, marker);
  },

  /**
   * The reverse of addMarker. Removes a marker of a specified class from a
   * line's gutter.
   */
  removeMarker(line, gutterName, markerClass) {
    if (!this.hasMarker(line, gutterName, markerClass)) {
      return;
    }

    this.lineInfo(line).gutterMarkers[gutterName].classList.remove(markerClass);
  },

  /**
   * Adds a marker with a specified class and an HTML content to a line's
   * gutter. If another marker exists on that line, it is overwritten by a new
   * marker.
   */
  addContentMarker(line, gutterName, markerClass, content) {
    const cm = editors.get(this);
    const info = this.lineInfo(line);
    if (!info) {
      return;
    }

    const marker = cm.getWrapperElement().ownerDocument.createElement("div");
    marker.className = markerClass;
    // eslint-disable-next-line no-unsanitized/property
    marker.innerHTML = content;
    cm.setGutterMarker(info.line, gutterName, marker);
  },

  /**
   * The reverse of addContentMarker. Removes any line's markers in the
   * specified gutter.
   */
  removeContentMarker(line, gutterName) {
    const cm = editors.get(this);
    const info = this.lineInfo(line);
    if (!info) {
      return;
    }

    cm.setGutterMarker(info.line, gutterName, null);
  },

  getMarker(line, gutterName) {
    const info = this.lineInfo(line);
    if (!info) {
      return null;
    }

    const gutterMarkers = info.gutterMarkers;
    if (!gutterMarkers) {
      return null;
    }

    return gutterMarkers[gutterName];
  },

  /**
   * Removes all gutter markers in the gutter with the given name.
   */
  removeAllMarkers(gutterName) {
    const cm = editors.get(this);
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
  setMarkerListeners(line, gutterName, markerClass, eventsArg, data) {
    if (!this.hasMarker(line, gutterName, markerClass)) {
      return;
    }

    const cm = editors.get(this);
    const marker = cm.lineInfo(line).gutterMarkers[gutterName];

    for (const name in eventsArg) {
      const listener = eventsArg[name].bind(this, line, marker, data);
      marker.addEventListener(name, listener);
    }
  },

  /**
   * Returns whether a line is decorated using the specified class name.
   */
  hasLineClass(line, className) {
    const info = this.lineInfo(line);

    if (!info || !info.wrapClass) {
      return false;
    }

    return info.wrapClass.split(" ").includes(className);
  },

  /**
   * Sets a CSS class name for the given line, including the text and gutter.
   */
  addLineClass(lineOrOffset, className) {
    const cm = editors.get(this);
    const line = this.toLineIfWasmOffset(lineOrOffset);
    cm.addLineClass(line, "wrap", className);
  },

  /**
   * The reverse of addLineClass.
   */
  removeLineClass(lineOrOffset, className) {
    const cm = editors.get(this);
    const line = this.toLineIfWasmOffset(lineOrOffset);
    cm.removeLineClass(line, "wrap", className);
  },

  /**
   * Mark a range of text inside the two {line, ch} bounds. Since the range may
   * be modified, for example, when typing text, this method returns a function
   * that can be used to remove the mark.
   */
  markText(from, to, className = "marked-text") {
    const cm = editors.get(this);
    const text = cm.getRange(from, to);
    const span = cm.getWrapperElement().ownerDocument.createElement("span");
    span.className = className;
    span.textContent = text;

    const mark = cm.markText(from, to, { replacedWith: span });
    return {
      anchor: span,
      clear: () => mark.clear(),
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
  getPosition(...args) {
    const cm = editors.get(this);
    const res = args.map(ind => cm.posFromIndex(ind));
    return args.length === 1 ? res[0] : res;
  },

  /**
   * The reverse of getPosition. Similarly to getPosition this
   * method returns a single value if only one argument was given
   * and an array otherwise.
   */
  getOffset(...args) {
    const cm = editors.get(this);
    const res = args.map(pos => cm.indexFromPos(pos));
    return args.length > 1 ? res : res[0];
  },

  /**
   * Returns a {line, ch} object that corresponds to the
   * left, top coordinates.
   */
  getPositionFromCoords({ left, top }) {
    const cm = editors.get(this);
    return cm.coordsChar({ left, top });
  },

  /**
   * The reverse of getPositionFromCoords. Similarly, returns a {left, top}
   * object that corresponds to the specified line and character number.
   */
  getCoordsFromPosition({ line, ch }) {
    const cm = editors.get(this);
    return cm.charCoords({ line: ~~line, ch: ~~ch });
  },

  /**
   * Returns true if there's something to undo and false otherwise.
   */
  canUndo() {
    const cm = editors.get(this);
    return cm.historySize().undo > 0;
  },

  /**
   * Returns true if there's something to redo and false otherwise.
   */
  canRedo() {
    const cm = editors.get(this);
    return cm.historySize().redo > 0;
  },

  /**
   * Marks the contents as clean and returns the current
   * version number.
   */
  setClean() {
    const cm = editors.get(this);
    this.version = cm.changeGeneration();
    this._lastDirty = false;
    this.emit("dirty-change");
    return this.version;
  },

  /**
   * Returns true if contents of the text area are
   * clean i.e. no changes were made since the last version.
   */
  isClean() {
    const cm = editors.get(this);
    return cm.isClean(this.version);
  },

  /**
   * This method opens an in-editor dialog asking for a line to
   * jump to. Once given, it changes cursor to that line.
   */
  jumpToLine() {
    const doc = editors.get(this).getWrapperElement().ownerDocument;
    const div = doc.createElement("div");
    const inp = doc.createElement("input");
    const txt = doc.createTextNode(L10N.getStr("gotoLineCmd.promptTitle"));

    inp.type = "text";
    inp.style.width = "10em";
    inp.style.marginInlineStart = "1em";

    div.appendChild(txt);
    div.appendChild(inp);

    this.openDialog(div, line => {
      // Handle LINE:COLUMN as well as LINE
      const match = line.toString().match(RE_JUMP_TO_LINE);
      if (match) {
        const [, matchLine, column] = match;
        this.setCursor({ line: matchLine - 1, ch: column ? column - 1 : 0 });
      }
    });
  },

  /**
   * Moves the content of the current line or the lines selected up a line.
   */
  moveLineUp() {
    const cm = editors.get(this);
    const start = cm.getCursor("start");
    const end = cm.getCursor("end");

    if (start.line === 0) {
      return;
    }

    // Get the text in the lines selected or the current line of the cursor
    // and append the text of the previous line.
    let value;
    if (start.line !== end.line) {
      value =
        cm.getRange(
          { line: start.line, ch: 0 },
          { line: end.line, ch: cm.getLine(end.line).length }
        ) + "\n";
    } else {
      value = cm.getLine(start.line) + "\n";
    }
    value += cm.getLine(start.line - 1);

    // Replace the previous line and the currently selected lines with the new
    // value and maintain the selection of the text.
    cm.replaceRange(
      value,
      { line: start.line - 1, ch: 0 },
      { line: end.line, ch: cm.getLine(end.line).length }
    );
    cm.setSelection(
      { line: start.line - 1, ch: start.ch },
      { line: end.line - 1, ch: end.ch }
    );
  },

  /**
   * Moves the content of the current line or the lines selected down a line.
   */
  moveLineDown() {
    const cm = editors.get(this);
    const start = cm.getCursor("start");
    const end = cm.getCursor("end");

    if (end.line + 1 === cm.lineCount()) {
      return;
    }

    // Get the text of next line and append the text in the lines selected
    // or the current line of the cursor.
    let value = cm.getLine(end.line + 1) + "\n";
    if (start.line !== end.line) {
      value += cm.getRange(
        { line: start.line, ch: 0 },
        { line: end.line, ch: cm.getLine(end.line).length }
      );
    } else {
      value += cm.getLine(start.line);
    }

    // Replace the currently selected lines and the next line with the new
    // value and maintain the selection of the text.
    cm.replaceRange(
      value,
      { line: start.line, ch: 0 },
      { line: end.line + 1, ch: cm.getLine(end.line + 1).length }
    );
    cm.setSelection(
      { line: start.line + 1, ch: start.ch },
      { line: end.line + 1, ch: end.ch }
    );
  },

  /**
   * Intercept CodeMirror's Find and replace key shortcut to select the search input
   */
  findOrReplace(node, isReplaceAll) {
    const cm = editors.get(this);
    const isInput = node.tagName === "INPUT";
    const isSearchInput = isInput && node.type === "search";
    // replace box is a different input instance than search, and it is
    // located in a code mirror dialog
    const isDialogInput =
      isInput &&
      node.parentNode &&
      node.parentNode.classList.contains("CodeMirror-dialog");
    if (!(isSearchInput || isDialogInput)) {
      return;
    }

    if (isSearchInput || isReplaceAll) {
      // select the search input
      // it's the precise reason why we reimplement these key shortcuts
      node.select();
    }

    // need to call it since we prevent the propagation of the event and
    // cancel codemirror's key handling
    cm.execCommand("find");
  },

  /**
   * Intercept CodeMirror's findNext and findPrev key shortcut to allow
   * immediately search for next occurance after typing a word to search.
   */
  findNextOrPrev(node, isFindPrev) {
    const cm = editors.get(this);
    const isInput = node.tagName === "INPUT";
    const isSearchInput = isInput && node.type === "search";
    if (!isSearchInput) {
      return;
    }
    const query = node.value;
    // cm.state.search allows to automatically start searching for the next occurance
    // it's the precise reason why we reimplement these key shortcuts
    if (!cm.state.search || cm.state.search.query !== query) {
      cm.state.search = {
        posFrom: null,
        posTo: null,
        overlay: null,
        query,
      };
    }

    // need to call it since we prevent the propagation of the event and
    // cancel codemirror's key handling
    if (isFindPrev) {
      cm.execCommand("findPrev");
    } else {
      cm.execCommand("findNext");
    }
  },

  /**
   * Returns current font size for the editor area, in pixels.
   */
  getFontSize() {
    const cm = editors.get(this);
    const el = cm.getWrapperElement();
    const win = el.ownerDocument.defaultView;

    return parseInt(win.getComputedStyle(el).getPropertyValue("font-size"), 10);
  },

  /**
   * Sets font size for the editor area.
   */
  setFontSize(size) {
    const cm = editors.get(this);
    cm.getWrapperElement().style.fontSize = parseInt(size, 10) + "px";
    cm.refresh();
  },

  /**
   * Sets an option for the editor.  For most options it just defers to
   * CodeMirror.setOption, but certain ones are maintained within the editor
   * instance.
   */
  setOption(o, v) {
    const cm = editors.get(this);

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
      this.config[o] = v;
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
  getOption(o) {
    const cm = editors.get(this);
    if (o === "autocomplete") {
      return this.config.autocomplete;
    }

    return cm.getOption(o);
  },

  /**
   * Sets up autocompletion for the editor. Lazily imports the required
   * dependencies because they vary by editor mode.
   *
   * Autocompletion is special, because we don't want to automatically use
   * it just because it is preffed on (it still needs to be requested by the
   * editor), but we do want to always disable it if it is preffed off.
   */
  setupAutoCompletion() {
    if (!this.config.autocomplete && !this.initializeAutoCompletion) {
      // Do nothing since there is no autocomplete config and no autocompletion have
      // been initialized.
      return;
    }
    // The autocomplete module will overwrite this.initializeAutoCompletion
    // with a mode specific autocompletion handler.
    if (!this.initializeAutoCompletion) {
      this.extend(
        require("resource://devtools/client/shared/sourceeditor/autocomplete.js")
      );
    }

    if (this.config.autocomplete && Services.prefs.getBoolPref(AUTOCOMPLETE)) {
      this.initializeAutoCompletion(this.config.autocompleteOpts);
    } else {
      this.destroyAutoCompletion();
    }
  },

  getAutoCompletionText() {
    const cm = editors.get(this);
    const mark = cm
      .getAllMarks()
      .find(m => m.className === AUTOCOMPLETE_MARK_CLASSNAME);
    if (!mark) {
      return "";
    }

    return mark.attributes["data-completion"] || "";
  },

  setAutoCompletionText(text) {
    const cursor = this.getCursor();
    const cm = editors.get(this);
    const className = AUTOCOMPLETE_MARK_CLASSNAME;

    cm.operation(() => {
      cm.getAllMarks().forEach(mark => {
        if (mark.className === className) {
          mark.clear();
        }
      });

      if (text) {
        cm.markText({ ...cursor, ch: cursor.ch - 1 }, cursor, {
          className,
          attributes: {
            "data-completion": text,
          },
        });
      }
    });
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
  extend(funcs) {
    Object.keys(funcs).forEach(name => {
      const cm = editors.get(this);
      const ctx = { ed: this, cm, Editor };

      if (name === "initialize") {
        funcs[name](ctx);
        return;
      }

      this[name] = funcs[name].bind(null, ctx);
    });
  },

  isDestroyed() {
    return !editors.get(this);
  },

  destroy() {
    this.container = null;
    this.config = null;
    this.version = null;
    this._ownerDoc = null;

    if (this._prefObserver) {
      this._prefObserver.off(KEYMAP_PREF, this.setKeyMap);
      this._prefObserver.off(TAB_SIZE, this.reloadPreferences);
      this._prefObserver.off(EXPAND_TAB, this.reloadPreferences);
      this._prefObserver.off(AUTO_CLOSE, this.reloadPreferences);
      this._prefObserver.off(AUTOCOMPLETE, this.reloadPreferences);
      this._prefObserver.off(DETECT_INDENT, this.reloadPreferences);
      this._prefObserver.off(ENABLE_CODE_FOLDING, this.reloadPreferences);
      this._prefObserver.destroy();
    }

    // Remove the link between the document and code-mirror.
    const cm = editors.get(this);
    if (cm?.doc) {
      cm.doc.cm = null;
    }

    this.emit("destroy");
  },

  updateCodeFoldingGutter() {
    let shouldFoldGutter = this.config.enableCodeFolding;
    const foldGutterIndex = this.config.gutters.indexOf(
      "CodeMirror-foldgutter"
    );
    const cm = editors.get(this);

    if (shouldFoldGutter === undefined) {
      shouldFoldGutter = Services.prefs.getBoolPref(ENABLE_CODE_FOLDING);
    }

    if (shouldFoldGutter) {
      // Add the gutter before enabling foldGutter
      if (foldGutterIndex === -1) {
        const gutters = this.config.gutters.slice();
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
        const gutters = this.config.gutters.slice();
        gutters.splice(foldGutterIndex, 1);
        this.setOption("gutters", gutters);
      }

      this.setOption("foldGutter", false);
    }
  },

  /**
   * Register all key shortcuts.
   */
  _initSearchShortcuts(win) {
    const shortcuts = new KeyShortcuts({
      window: win,
    });
    this._onSearchShortcut = this._onSearchShortcut.bind(this);
    const keys = ["find.key", "findNext.key", "findPrev.key"];

    if (OS === "Darwin") {
      keys.push("replaceAllMac.key");
    } else {
      keys.push("replaceAll.key");
    }
    // Process generic keys:
    keys.forEach(name => {
      const key = L10N.getStr(name);
      shortcuts.on(key, event => this._onSearchShortcut(name, event));
    });
  },
  /**
   * Key shortcut listener.
   */
  _onSearchShortcut(name, event) {
    if (!this._isInputOrTextarea(event.target)) {
      return;
    }
    const node = event.originalTarget;

    switch (name) {
      // replaceAll.key is Alt + find.key
      case "replaceAllMac.key":
        this.findOrReplace(node, true);
        break;
      // replaceAll.key is Shift + find.key
      case "replaceAll.key":
        this.findOrReplace(node, true);
        break;
      case "find.key":
        this.findOrReplace(node, false);
        break;
      // findPrev.key is Shift + findNext.key
      case "findPrev.key":
        this.findNextOrPrev(node, true);
        break;
      case "findNext.key":
        this.findNextOrPrev(node, false);
        break;
      default:
        console.error("Unexpected editor key shortcut", name);
        return;
    }
    // Prevent default for this action
    event.stopPropagation();
    event.preventDefault();
  },

  /**
   * Check if a node is an input or textarea
   */
  _isInputOrTextarea(element) {
    const name = element.tagName.toLowerCase();
    return name === "input" || name === "textarea";
  },
};

// Since Editor is a thin layer over CodeMirror some methods
// are mapped directly—without any changes.

CM_MAPPING.forEach(name => {
  Editor.prototype[name] = function (...args) {
    const cm = editors.get(this);
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
Editor.accel = function (key, modifiers = {}) {
  return (
    (modifiers.shift ? "Shift-" : "") +
    (Services.appinfo.OS == "Darwin" ? "Cmd-" : "Ctrl-") +
    (modifiers.alt ? "Alt-" : "") +
    key
  );
};

/**
 * Returns a string representation of a shortcut for a
 * specified command 'cmd'. Append Cmd- for macs, Ctrl- for other
 * platforms unless noaccel is specified in the options. Useful when overwriting
 * or disabling default shortcuts.
 */
Editor.keyFor = function (cmd, opts = { noaccel: false }) {
  const key = L10N.getStr(cmd + ".commandkey");
  return opts.noaccel ? key : Editor.accel(key);
};

/**
 * We compute the CSS property names, values, and color names to be used with
 * CodeMirror to more closely reflect what is supported by the target platform.
 * The database is used to replace the values used in CodeMirror while initiating
 * an editor object. This is done here instead of the file codemirror/css.js so
 * as to leave that file untouched and easily upgradable.
 */
function getCSSKeywords(cssProperties) {
  function keySet(array) {
    const keys = {};
    for (let i = 0; i < array.length; ++i) {
      keys[array[i]] = true;
    }
    return keys;
  }

  const propertyKeywords = cssProperties.getNames();
  const colorKeywords = {};
  const valueKeywords = {};

  propertyKeywords.forEach(property => {
    if (property.includes("color")) {
      cssProperties.getValues(property).forEach(value => {
        colorKeywords[value] = true;
      });
    } else {
      cssProperties.getValues(property).forEach(value => {
        valueKeywords[value] = true;
      });
    }
  });

  return {
    propertyKeywords: keySet(propertyKeywords),
    colorKeywords,
    valueKeywords,
  };
}

module.exports = Editor;
