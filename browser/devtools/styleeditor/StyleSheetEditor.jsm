/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["StyleSheetEditor"];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

const require = Cu.import("resource://gre/modules/devtools/Loader.jsm", {}).devtools.require;
const Editor  = require("devtools/sourceeditor/editor");
const promise = require("sdk/core/promise");
const {CssLogic} = require("devtools/styleinspector/css-logic");
const AutoCompleter = require("devtools/sourceeditor/autocomplete");

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/FileUtils.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource:///modules/devtools/shared/event-emitter.js");
Cu.import("resource:///modules/devtools/StyleEditorUtil.jsm");

const LOAD_ERROR = "error-load";
const SAVE_ERROR = "error-save";

// max update frequency in ms (avoid potential typing lag and/or flicker)
// @see StyleEditor.updateStylesheet
const UPDATE_STYLESHEET_THROTTLE_DELAY = 500;

// Pref which decides if CSS autocompletion is enabled in Style Editor or not.
const AUTOCOMPLETION_PREF = "devtools.styleeditor.autocompletion-enabled";

/**
 * StyleSheetEditor controls the editor linked to a particular StyleSheet
 * object.
 *
 * Emits events:
 *   'property-change': A property on the underlying stylesheet has changed
 *   'source-editor-load': The source editor for this editor has been loaded
 *   'error': An error has occured
 *
 * @param {StyleSheet|OriginalSource}  styleSheet
 *        Stylesheet or original source to show
 * @param {DOMWindow}  win
 *        panel window for style editor
 * @param {nsIFile}  file
 *        Optional file that the sheet was imported from
 * @param {boolean} isNew
 *        Optional whether the sheet was created by the user
 * @param {Walker} walker
 *        Optional walker used for selectors autocompletion
 */
function StyleSheetEditor(styleSheet, win, file, isNew, walker) {
  EventEmitter.decorate(this);

  this.styleSheet = styleSheet;
  this._inputElement = null;
  this._sourceEditor = null;
  this._window = win;
  this._isNew = isNew;
  this.savedFile = file;
  this.walker = walker;

  this.errorMessage = null;

  let readOnly = false;
  if (styleSheet.isOriginalSource) {
    // live-preview won't work with sources that need compilation
    readOnly = true;
  }

  this._state = {   // state to use when inputElement attaches
    text: "",
    selection: {
      start: {line: 0, ch: 0},
      end: {line: 0, ch: 0}
    },
    readOnly: readOnly,
    topIndex: 0,              // the first visible line
  };

  this._styleSheetFilePath = null;
  if (styleSheet.href &&
      Services.io.extractScheme(this.styleSheet.href) == "file") {
    this._styleSheetFilePath = this.styleSheet.href;
  }

  this._onPropertyChange = this._onPropertyChange.bind(this);
  this._onError = this._onError.bind(this);

  this._focusOnSourceEditorReady = false;

  this.styleSheet.on("property-change", this._onPropertyChange);
  this.styleSheet.on("error", this._onError);
}

StyleSheetEditor.prototype = {
  /**
   * Whether there are unsaved changes in the editor
   */
  get unsaved() {
    return this.sourceEditor && !this.sourceEditor.isClean();
  },

  /**
   * Whether the editor is for a stylesheet created by the user
   * through the style editor UI.
   */
  get isNew() {
    return this._isNew;
  },

  /**
   * Get a user-friendly name for the style sheet.
   *
   * @return string
   */
  get friendlyName() {
    if (this.savedFile) {
      return this.savedFile.leafName;
    }

    if (this._isNew) {
      let index = this.styleSheet.styleSheetIndex + 1;
      return _("newStyleSheet", index);
    }

    if (!this.styleSheet.href) {
      let index = this.styleSheet.styleSheetIndex + 1;
      return _("inlineStyleSheet", index);
    }

    if (!this._friendlyName) {
      let sheetURI = this.styleSheet.href;
      this._friendlyName = CssLogic.shortSource({ href: sheetURI });
      try {
        this._friendlyName = decodeURI(this._friendlyName);
      } catch (ex) {
      }
    }
    return this._friendlyName;
  },

  /**
   * Start fetching the full text source for this editor's sheet.
   */
  fetchSource: function(callback) {
    this.styleSheet.getText().then((longStr) => {
      longStr.string().then((source) => {
        this._state.text = prettifyCSS(source);
        this.sourceLoaded = true;

        callback(source);
      });
    }, e => {
      this.emit("error", LOAD_ERROR, this.styleSheet.href);
    })
  },

  /**
   * Forward property-change event from stylesheet.
   *
   * @param  {string} event
   *         Event type
   * @param  {string} property
   *         Property that has changed on sheet
   */
  _onPropertyChange: function(property, value) {
    this.emit("property-change", property, value);
  },

  /**
   * Forward error event from stylesheet.
   *
   * @param  {string} event
   *         Event type
   * @param  {string} errorCode
   */
  _onError: function(event, errorCode) {
    this.emit("error", errorCode);
  },

  /**
   * Create source editor and load state into it.
   * @param  {DOMElement} inputElement
   *         Element to load source editor in
   */
  load: function(inputElement) {
    this._inputElement = inputElement;

    let config = {
      value: this._state.text,
      lineNumbers: true,
      mode: Editor.modes.css,
      readOnly: this._state.readOnly,
      autoCloseBrackets: "{}()[]",
      extraKeys: this._getKeyBindings(),
      contextMenu: "sourceEditorContextMenu"
    };
    let sourceEditor = new Editor(config);

    sourceEditor.appendTo(inputElement).then(() => {
      if (Services.prefs.getBoolPref(AUTOCOMPLETION_PREF)) {
        sourceEditor.extend(AutoCompleter);
        sourceEditor.setupAutoCompletion(this.walker);
      }
      sourceEditor.on("save", () => {
        this.saveToFile();
      });

      sourceEditor.on("change", () => {
        this.updateStyleSheet();
      });

      this.sourceEditor = sourceEditor;

      if (this._focusOnSourceEditorReady) {
        this._focusOnSourceEditorReady = false;
        sourceEditor.focus();
      }

      sourceEditor.setFirstVisibleLine(this._state.topIndex);
      sourceEditor.setSelection(this._state.selection.start,
                                this._state.selection.end);

      this.emit("source-editor-load");
    });

    sourceEditor.on("dirty-change", this._onPropertyChange);
  },

  /**
   * Get the source editor for this editor.
   *
   * @return {Promise}
   *         Promise that will resolve with the editor.
   */
  getSourceEditor: function() {
    let deferred = promise.defer();

    if (this.sourceEditor) {
      return promise.resolve(this);
    }
    this.on("source-editor-load", () => {
      deferred.resolve(this);
    });
    return deferred.promise;
  },

  /**
   * Focus the Style Editor input.
   */
  focus: function() {
    if (this._sourceEditor) {
      this._sourceEditor.focus();
    } else {
      this._focusOnSourceEditorReady = true;
    }
  },

  /**
   * Event handler for when the editor is shown.
   */
  onShow: function() {
    if (this._sourceEditor) {
      this._sourceEditor.setFirstVisibleLine(this._state.topIndex);
    }
    this.focus();
  },

  /**
   * Toggled the disabled state of the underlying stylesheet.
   */
  toggleDisabled: function() {
    this.styleSheet.toggleDisabled();
  },

  /**
   * Queue a throttled task to update the live style sheet.
   *
   * @param boolean immediate
   *        Optional. If true the update is performed immediately.
   */
  updateStyleSheet: function(immediate) {
    if (this._updateTask) {
      // cancel previous queued task not executed within throttle delay
      this._window.clearTimeout(this._updateTask);
    }

    if (immediate) {
      this._updateStyleSheet();
    } else {
      this._updateTask = this._window.setTimeout(this._updateStyleSheet.bind(this),
                                           UPDATE_STYLESHEET_THROTTLE_DELAY);
    }
  },

  /**
   * Update live style sheet according to modifications.
   */
  _updateStyleSheet: function() {
    if (this.styleSheet.disabled) {
      return;  // TODO: do we want to do this?
    }

    this._updateTask = null; // reset only if we actually perform an update
                             // (stylesheet is enabled) so that 'missed' updates
                             // while the stylesheet is disabled can be performed
                             // when it is enabled back. @see enableStylesheet

    if (this.sourceEditor) {
      this._state.text = this.sourceEditor.getText();
    }

    this.styleSheet.update(this._state.text, true);
  },

  /**
   * Save the editor contents into a file and set savedFile property.
   * A file picker UI will open if file is not set and editor is not headless.
   *
   * @param mixed file
   *        Optional nsIFile or string representing the filename to save in the
   *        background, no UI will be displayed.
   *        If not specified, the original style sheet URI is used.
   *        To implement 'Save' instead of 'Save as', you can pass savedFile here.
   * @param function(nsIFile aFile) callback
   *        Optional callback called when the operation has finished.
   *        aFile has the nsIFile object for saved file or null if the operation
   *        has failed or has been canceled by the user.
   * @see savedFile
   */
  saveToFile: function(file, callback) {
    let onFile = (returnFile) => {
      if (!returnFile) {
        if (callback) {
          callback(null);
        }
        return;
      }

      if (this._sourceEditor) {
        this._state.text = this._sourceEditor.getText();
      }

      let ostream = FileUtils.openSafeFileOutputStream(returnFile);
      let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
                        .createInstance(Ci.nsIScriptableUnicodeConverter);
      converter.charset = "UTF-8";
      let istream = converter.convertToInputStream(this._state.text);

      NetUtil.asyncCopy(istream, ostream, function onStreamCopied(status) {
        if (!Components.isSuccessCode(status)) {
          if (callback) {
            callback(null);
          }
          this.emit("error", SAVE_ERROR);
          return;
        }
        FileUtils.closeSafeFileOutputStream(ostream);
        // remember filename for next save if any
        this._friendlyName = null;
        this.savedFile = returnFile;

        if (callback) {
          callback(returnFile);
        }
        this.sourceEditor.setClean();

        this.emit("property-change");
      }.bind(this));
    };

    let defaultName;
    if (this._friendlyName) {
      defaultName = OS.Path.basename(this._friendlyName);
    }
    showFilePicker(file || this._styleSheetFilePath, true, this._window,
                   onFile, defaultName);
 },

  /**
    * Retrieve custom key bindings objects as expected by Editor.
    * Editor action names are not displayed to the user.
    *
    * @return {array} key binding objects for the source editor
    */
  _getKeyBindings: function() {
    let bindings = {};

    bindings[Editor.accel(_("saveStyleSheet.commandkey"))] = () => {
      this.saveToFile(this.savedFile);
    };

    bindings["Shift-" + Editor.accel(_("saveStyleSheet.commandkey"))] = () => {
      this.saveToFile();
    };

    return bindings;
  },

  /**
   * Clean up for this editor.
   */
  destroy: function() {
    if (this.sourceEditor) {
      this.sourceEditor.destroy();
    }
    this.styleSheet.off("property-change", this._onPropertyChange);
    this.styleSheet.off("error", this._onError);
  }
}


const TAB_CHARS = "\t";

const CURRENT_OS = Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime).OS;
const LINE_SEPARATOR = CURRENT_OS === "WINNT" ? "\r\n" : "\n";

/**
 * Prettify minified CSS text.
 * This prettifies CSS code where there is no indentation in usual places while
 * keeping original indentation as-is elsewhere.
 *
 * @param string text
 *        The CSS source to prettify.
 * @return string
 *         Prettified CSS source
 */
function prettifyCSS(text)
{
  // remove initial and terminating HTML comments and surrounding whitespace
  text = text.replace(/(?:^\s*<!--[\r\n]*)|(?:\s*-->\s*$)/g, "");

  let parts = [];    // indented parts
  let partStart = 0; // start offset of currently parsed part
  let indent = "";
  let indentLevel = 0;

  for (let i = 0; i < text.length; i++) {
    let c = text[i];
    let shouldIndent = false;

    switch (c) {
      case "}":
        if (i - partStart > 1) {
          // there's more than just } on the line, add line
          parts.push(indent + text.substring(partStart, i));
          partStart = i;
        }
        indent = TAB_CHARS.repeat(--indentLevel);
        /* fallthrough */
      case ";":
      case "{":
        shouldIndent = true;
        break;
    }

    if (shouldIndent) {
      let la = text[i+1]; // one-character lookahead
      if (!/\s/.test(la)) {
        // following character should be a new line (or whitespace) but it isn't
        // force indentation then
        parts.push(indent + text.substring(partStart, i + 1));
        if (c == "}") {
          parts.push(""); // for extra line separator
        }
        partStart = i + 1;
      } else {
        return text; // assume it is not minified, early exit
      }
    }

    if (c == "{") {
      indent = TAB_CHARS.repeat(++indentLevel);
    }
  }
  return parts.join(LINE_SEPARATOR);
}

