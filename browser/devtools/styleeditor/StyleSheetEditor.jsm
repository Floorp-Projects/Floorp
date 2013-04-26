/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["StyleSheetEditor"];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/commonjs/sdk/core/promise.js");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/FileUtils.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource:///modules/devtools/EventEmitter.jsm");
Cu.import("resource:///modules/source-editor.jsm");
Cu.import("resource:///modules/devtools/StyleEditorUtil.jsm");


const SAVE_ERROR = "error-save";

// max update frequency in ms (avoid potential typing lag and/or flicker)
// @see StyleEditor.updateStylesheet
const UPDATE_STYLESHEET_THROTTLE_DELAY = 500;

/**
 * StyleSheetEditor controls the editor linked to a particular StyleSheet
 * object.
 *
 * Emits events:
 *   'source-load': The source of the stylesheet has been fetched
 *   'property-change': A property on the underlying stylesheet has changed
 *   'source-editor-load': The source editor for this editor has been loaded
 *   'error': An error has occured
 *
 * @param {StyleSheet}  styleSheet
 * @param {DOMWindow}  win
 *        panel window for style editor
 * @param {nsIFile}  file
 *        Optional file that the sheet was imported from
 * @param {boolean} isNew
 *        Optional whether the sheet was created by the user
 */
function StyleSheetEditor(styleSheet, win, file, isNew) {
  EventEmitter.decorate(this);

  this.styleSheet = styleSheet;
  this._inputElement = null;
  this._sourceEditor = null;
  this._window = win;
  this._isNew = isNew;
  this.savedFile = file;

  this.errorMessage = null;

  this._state = {   // state to use when inputElement attaches
    text: "",
    selection: {start: 0, end: 0},
    readOnly: false,
    topIndex: 0,              // the first visible line
  };

  this._styleSheetFilePath = null;
  if (styleSheet.href &&
      Services.io.extractScheme(this.styleSheet.href) == "file") {
    this._styleSheetFilePath = this.styleSheet.href;
  }

  this._onSourceLoad = this._onSourceLoad.bind(this);
  this._onPropertyChange = this._onPropertyChange.bind(this);
  this._onError = this._onError.bind(this);

  this._focusOnSourceEditorReady = false;

  this.styleSheet.once("source-load", this._onSourceLoad);
  this.styleSheet.on("property-change", this._onPropertyChange);
  this.styleSheet.on("error", this._onError);
}

StyleSheetEditor.prototype = {
  /**
   * This editor's source editor
   */
  get sourceEditor() {
    return this._sourceEditor;
  },

  /**
   * Whether there are unsaved changes in the editor
   */
  get unsaved() {
    return this._sourceEditor && this._sourceEditor.dirty;
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
    if (this.savedFile) { // reuse the saved filename if any
      return this.savedFile.leafName;
    }

    if (this._isNew) {
      let index = this.styleSheet.styleSheetIndex + 1; // 0-indexing only works for devs
      return _("newStyleSheet", index);
    }

    if (!this.styleSheet.href) {
      let index = this.styleSheet.styleSheetIndex + 1; // 0-indexing only works for devs
      return _("inlineStyleSheet", index);
    }

    if (!this._friendlyName) {
      let sheetURI = this.styleSheet.href;
      let contentURI = this.styleSheet.debuggee.baseURI;
      let contentURIScheme = contentURI.scheme;
      let contentURILeafIndex = contentURI.specIgnoringRef.lastIndexOf("/");
      contentURI = contentURI.specIgnoringRef;

      // get content base URI without leaf name (if any)
      if (contentURILeafIndex > contentURIScheme.length) {
        contentURI = contentURI.substring(0, contentURILeafIndex + 1);
      }

      // avoid verbose repetition of absolute URI when the style sheet URI
      // is relative to the content URI
      this._friendlyName = (sheetURI.indexOf(contentURI) == 0)
                           ? sheetURI.substring(contentURI.length)
                           : sheetURI;
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
  fetchSource: function() {
    this.styleSheet.fetchSource();
  },

  /**
   * Handle source fetched event. Forward source-load event.
   *
   * @param  {string} event
   *         Event type
   * @param  {string} source
   *         Full-text source of the stylesheet
   */
  _onSourceLoad: function(event, source) {
    this._state.text = prettifyCSS(source);
    this.sourceLoaded = true;
    this.emit("source-load");
  },

  /**
   * Forward property-change event from stylesheet.
   *
   * @param  {string} event
   *         Event type
   * @param  {string} property
   *         Property that has changed on sheet
   */
  _onPropertyChange: function(event, property) {
    this.emit("property-change", property);
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

    let sourceEditor = new SourceEditor();
    let config = {
      initialText: this._state.text,
      showLineNumbers: true,
      mode: SourceEditor.MODES.CSS,
      readOnly: this._state.readOnly,
      keys: this._getKeyBindings()
    };

    sourceEditor.init(inputElement, config, function onSourceEditorReady() {
      setupBracketCompletion(sourceEditor);
      sourceEditor.addEventListener(SourceEditor.EVENTS.TEXT_CHANGED,
                                    function onTextChanged(event) {
        this.updateStyleSheet();
      }.bind(this));

      this._sourceEditor = sourceEditor;

      if (this._focusOnSourceEditorReady) {
        this._focusOnSourceEditorReady = false;
        sourceEditor.focus();
      }

      sourceEditor.setTopIndex(this._state.topIndex);
      sourceEditor.setSelection(this._state.selection.start,
                                this._state.selection.end);

      this.emit("source-editor-load");
    }.bind(this));

    sourceEditor.addEventListener(SourceEditor.EVENTS.DIRTY_CHANGED,
                                  this._onPropertyChange);
  },

  /**
   * Get the source editor for this editor.
   *
   * @return {Promise}
   *         Promise that will resolve with the editor.
   */
  getSourceEditor: function() {
    let deferred = Promise.defer();

    if (this.sourceEditor) {
      return Promise.resolve(this);
    }
    this.on("source-editor-load", (event) => {
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
      this._sourceEditor.setTopIndex(this._state.topIndex);
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

    this.styleSheet.update(this._state.text);
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
        this.sourceEditor.dirty = false;
      }.bind(this));
    };

    showFilePicker(file || this._styleSheetFilePath, true, this._window, onFile);
  },

  /**
    * Retrieve custom key bindings objects as expected by SourceEditor.
    * SourceEditor action names are not displayed to the user.
    *
    * @return {array} key binding objects for the source editor
    */
  _getKeyBindings: function() {
    let bindings = [];

    bindings.push({
      action: "StyleEditor.save",
      code: _("saveStyleSheet.commandkey"),
      accel: true,
      callback: function save() {
        this.saveToFile(this.savedFile);
        return true;
      }.bind(this)
    });

    bindings.push({
      action: "StyleEditor.saveAs",
      code: _("saveStyleSheet.commandkey"),
      accel: true,
      shift: true,
      callback: function saveAs() {
        this.saveToFile();
        return true;
      }.bind(this)
    });

    return bindings;
  },

  /**
   * Clean up for this editor.
   */
  destroy: function() {
    this.styleSheet.off("source-load", this._onSourceLoad);
    this.styleSheet.off("property-change", this._onPropertyChange);
    this.styleSheet.off("error", this._onError);
  }
}


const TAB_CHARS = "\t";

const OS = Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime).OS;
const LINE_SEPARATOR = OS === "WINNT" ? "\r\n" : "\n";

/**
  * Return string that repeats text for aCount times.
  *
  * @param string text
  * @param number aCount
  * @return string
  */
function repeat(text, aCount)
{
  return (new Array(aCount + 1)).join(text);
}

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
        indent = repeat(TAB_CHARS, --indentLevel);
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
      indent = repeat(TAB_CHARS, ++indentLevel);
    }
  }
  return parts.join(LINE_SEPARATOR);
}


/**
 * Set up bracket completion on a given SourceEditor.
 * This automatically closes the following CSS brackets: "{", "(", "["
 *
 * @param SourceEditor sourceEditor
 */
function setupBracketCompletion(sourceEditor)
{
  let editorElement = sourceEditor.editorElement;
  let pairs = {
    123: { // {
      closeString: "}",
      closeKeyCode: Ci.nsIDOMKeyEvent.DOM_VK_CLOSE_BRACKET
    },
    40: { // (
      closeString: ")",
      closeKeyCode: Ci.nsIDOMKeyEvent.DOM_VK_0
    },
    91: { // [
      closeString: "]",
      closeKeyCode: Ci.nsIDOMKeyEvent.DOM_VK_CLOSE_BRACKET
    },
  };

  editorElement.addEventListener("keypress", function onKeyPress(event) {
    let pair = pairs[event.charCode];
    if (!pair || event.ctrlKey || event.metaKey ||
        event.accelKey || event.altKey) {
      return true;
    }

    // We detected an open bracket, sending closing character
    let keyCode = pair.closeKeyCode;
    let charCode = pair.closeString.charCodeAt(0);
    let modifiers = 0;
    let utils = editorElement.ownerDocument.defaultView.
                  QueryInterface(Ci.nsIInterfaceRequestor).
                  getInterface(Ci.nsIDOMWindowUtils);
    let handled = utils.sendKeyEvent("keydown", keyCode, 0, modifiers);
    utils.sendKeyEvent("keypress", 0, charCode, modifiers, !handled);
    utils.sendKeyEvent("keyup", keyCode, 0, modifiers);
    // and rewind caret
    sourceEditor.setCaretOffset(sourceEditor.getCaretOffset() - 1);
  }, false);
}

