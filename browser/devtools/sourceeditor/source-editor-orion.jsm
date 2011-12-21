/* vim:set ts=2 sw=2 sts=2 et tw=80:
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Source Editor component (Orion editor).
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mihai Sucan <mihai.sucan@gmail.com> (original author)
 *   Kenny Heaton <kennyheaton@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK *****/

"use strict";

const Cu = Components.utils;
const Ci = Components.interfaces;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "clipboardHelper",
                                   "@mozilla.org/widget/clipboardhelper;1",
                                   "nsIClipboardHelper");

const ORION_SCRIPT = "chrome://browser/content/orion.js";
const ORION_IFRAME = "data:text/html;charset=utf8,<!DOCTYPE html>" +
  "<html style='height:100%' dir='ltr'>" +
  "<body style='height:100%;margin:0;overflow:hidden'>" +
  "<div id='editor' style='height:100%'></div>" +
  "</body></html>";

const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

/**
 * Predefined themes for syntax highlighting. This objects maps
 * SourceEditor.THEMES to Orion CSS files.
 */
const ORION_THEMES = {
  mozilla: ["chrome://browser/content/orion-mozilla.css"],
};

/**
 * Known editor events you can listen for. This object maps SourceEditor.EVENTS
 * to Orion events.
 */
const ORION_EVENTS = {
  ContextMenu: "ContextMenu",
  TextChanged: "ModelChanged",
  Selection: "Selection",
};

/**
 * Known Orion annotation types.
 */
const ORION_ANNOTATION_TYPES = {
  currentBracket: "orion.annotation.currentBracket",
  matchingBracket: "orion.annotation.matchingBracket",
};

/**
 * Default key bindings in the Orion editor.
 */
const DEFAULT_KEYBINDINGS = [
  {
    action: "undo",
    code: Ci.nsIDOMKeyEvent.DOM_VK_Z,
    accel: true,
  },
  {
    action: "redo",
    code: Ci.nsIDOMKeyEvent.DOM_VK_Z,
    accel: true,
    shift: true,
  },
  {
    action: "Unindent Lines",
    code: Ci.nsIDOMKeyEvent.DOM_VK_TAB,
    shift: true,
  },
];

var EXPORTED_SYMBOLS = ["SourceEditor"];

/**
 * The SourceEditor object constructor. The SourceEditor component allows you to
 * provide users with an editor tailored to the specific needs of editing source
 * code, aimed primarily at web developers.
 *
 * The editor used here is Eclipse Orion (see http://www.eclipse.org/orion).
 *
 * @constructor
 */
function SourceEditor() {
  // Update the SourceEditor defaults from user preferences.

  SourceEditor.DEFAULTS.TAB_SIZE =
    Services.prefs.getIntPref(SourceEditor.PREFS.TAB_SIZE);
  SourceEditor.DEFAULTS.EXPAND_TAB =
    Services.prefs.getBoolPref(SourceEditor.PREFS.EXPAND_TAB);

  this._onOrionSelection = this._onOrionSelection.bind(this);
}

SourceEditor.prototype = {
  _view: null,
  _iframe: null,
  _model: null,
  _undoStack: null,
  _linesRuler: null,
  _styler: null,
  _annotationStyler: null,
  _annotationModel: null,
  _dragAndDrop: null,
  _mode: null,
  _expandTab: null,
  _tabSize: null,
  _iframeWindow: null,

  /**
   * The editor container element.
   * @type nsIDOMElement
   */
  parentElement: null,

  /**
   * Initialize the editor.
   *
   * @param nsIDOMElement aElement
   *        The DOM element where you want the editor to show.
   * @param object aConfig
   *        Editor configuration object. Properties:
   *          - placeholderText - the text you want to be shown by default.
   *          - theme - the syntax highlighting theme you want. You can use one
   *          of the predefined themes, or you can point to your CSS file.
   *          - mode - the editor mode, based on the file type you want to edit.
   *          You can use one of the predefined modes.
   *          - tabSize - define how many spaces to use for a tab character.
   *          - expandTab - tells if you want tab characters to be expanded to
   *          spaces.
   *          - readOnly - make the editor read only.
   *          - showLineNumbers - display the line numbers gutter.
   *          - undoLimit - how many steps should the undo stack hold.
   *          - keys - is an array of objects that allows you to define custom
   *          editor keyboard bindings. Each object can have:
   *              - action - name of the editor action to invoke.
   *              - code - keyCode for the shortcut.
   *              - accel - boolean for the Accel key (cmd/ctrl).
   *              - shift - boolean for the Shift key.
   *              - alt - boolean for the Alt key.
   *              - callback - optional function to invoke, if the action is not
   *              predefined in the editor.
   * @param function [aCallback]
   *        Function you want to execute once the editor is loaded and
   *        initialized.
   */
  init: function SE_init(aElement, aConfig, aCallback)
  {
    if (this._iframe) {
      throw new Error("SourceEditor is already initialized!");
    }

    let doc = aElement.ownerDocument;

    this._iframe = doc.createElementNS(XUL_NS, "iframe");
    this._iframe.flex = 1;

    let onIframeLoad = (function() {
      this._iframe.removeEventListener("load", onIframeLoad, true);
      this._onIframeLoad();
    }).bind(this);

    this._iframe.addEventListener("load", onIframeLoad, true);

    this._iframe.setAttribute("src", ORION_IFRAME);

    aElement.appendChild(this._iframe);
    this.parentElement = aElement;
    this._config = aConfig;
    this._onReadyCallback = aCallback;
  },

  /**
   * The editor iframe load event handler.
   * @private
   */
  _onIframeLoad: function SE__onIframeLoad()
  {
    this._iframeWindow = this._iframe.contentWindow.wrappedJSObject;
    let window = this._iframeWindow;
    let config = this._config;

    Services.scriptloader.loadSubScript(ORION_SCRIPT, window, "utf8");

    let TextModel = window.require("orion/textview/textModel").TextModel;
    let TextView = window.require("orion/textview/textView").TextView;

    this._expandTab = typeof config.expandTab != "undefined" ?
                      config.expandTab : SourceEditor.DEFAULTS.EXPAND_TAB;
    this._tabSize = config.tabSize || SourceEditor.DEFAULTS.TAB_SIZE;

    let theme = config.theme || SourceEditor.DEFAULTS.THEME;
    let stylesheet = theme in ORION_THEMES ? ORION_THEMES[theme] : theme;

    this._model = new TextModel(config.placeholderText);
    this._view = new TextView({
      model: this._model,
      parent: "editor",
      stylesheet: stylesheet,
      tabSize: this._tabSize,
      expandTab: this._expandTab,
      readonly: config.readOnly,
      themeClass: "mozilla" + (config.readOnly ? " readonly" : ""),
    });

    let onOrionLoad = function() {
      this._view.removeEventListener("Load", onOrionLoad);
      this._onOrionLoad();
    }.bind(this);

    this._view.addEventListener("Load", onOrionLoad);
    if (Services.appinfo.OS == "Linux") {
      this._view.addEventListener("Selection", this._onOrionSelection);
    }

    let KeyBinding = window.require("orion/textview/keyBinding").KeyBinding;
    let TextDND = window.require("orion/textview/textDND").TextDND;
    let LineNumberRuler = window.require("orion/textview/rulers").LineNumberRuler;
    let UndoStack = window.require("orion/textview/undoStack").UndoStack;
    let AnnotationModel = window.require("orion/textview/annotations").AnnotationModel;

    this._annotationModel = new AnnotationModel(this._model);

    if (config.showLineNumbers) {
      this._linesRuler = new LineNumberRuler(this._annotationModel, "left",
        {styleClass: "rulerLines"}, {styleClass: "rulerLine odd"},
        {styleClass: "rulerLine even"});

      this._view.addRuler(this._linesRuler);
    }

    this.setMode(config.mode || SourceEditor.DEFAULTS.MODE);

    this._undoStack = new UndoStack(this._view,
      config.undoLimit || SourceEditor.DEFAULTS.UNDO_LIMIT);

    this._dragAndDrop = new TextDND(this._view, this._undoStack);

    this._view.setAction("undo", this.undo.bind(this));
    this._view.setAction("redo", this.redo.bind(this));
    this._view.setAction("tab", this._doTab.bind(this));
    this._view.setAction("Unindent Lines", this._doUnindentLines.bind(this));
    this._view.setAction("enter", this._doEnter.bind(this));

    let keys = (config.keys || []).concat(DEFAULT_KEYBINDINGS);
    keys.forEach(function(aKey) {
      let binding = new KeyBinding(aKey.code, aKey.accel, aKey.shift, aKey.alt);
      this._view.setKeyBinding(binding, aKey.action);

      if (aKey.callback) {
        this._view.setAction(aKey.action, aKey.callback);
      }
    }, this);
  },

  /**
   * The Orion "Load" event handler. This is called when the Orion editor
   * completes the initialization.
   * @private
   */
  _onOrionLoad: function SE__onOrionLoad()
  {
    if (this._onReadyCallback) {
      this._onReadyCallback(this);
      this._onReadyCallback = null;
    }
  },

  /**
   * The "tab" editor action implementation. This adds support for expanded tabs
   * to spaces, and support for the indentation of multiple lines at once.
   * @private
   */
  _doTab: function SE__doTab()
  {
    let indent = "\t";
    let selection = this.getSelection();
    let model = this._model;
    let firstLine = model.getLineAtOffset(selection.start);
    let firstLineStart = model.getLineStart(firstLine);
    let lastLineOffset = selection.end > selection.start ?
                         selection.end - 1 : selection.end;
    let lastLine = model.getLineAtOffset(lastLineOffset);

    if (this._expandTab) {
      let offsetFromLineStart = firstLine == lastLine ?
                                selection.start - firstLineStart : 0;
      let spaces = this._tabSize - (offsetFromLineStart % this._tabSize);
      indent = (new Array(spaces + 1)).join(" ");
    }

    // Do selection indentation.
    if (firstLine != lastLine) {
      let lines = [""];
      let lastLineEnd = model.getLineEnd(lastLine, true);
      let selectedLines = lastLine - firstLine + 1;

      for (let i = firstLine; i <= lastLine; i++) {
        lines.push(model.getLine(i, true));
      }

      this.startCompoundChange();

      this.setText(lines.join(indent), firstLineStart, lastLineEnd);

      let newSelectionStart = firstLineStart == selection.start ?
                              selection.start : selection.start + indent.length;
      let newSelectionEnd = selection.end + (selectedLines * indent.length);

      this._view.setSelection(newSelectionStart, newSelectionEnd);

      this.endCompoundChange();
      return true;
    }

    return false;
  },

  /**
   * The "Unindent lines" editor action implementation. This method is invoked
   * when the user presses Shift-Tab.
   * @private
   */
  _doUnindentLines: function SE__doUnindentLines()
  {
    let indent = "\t";

    let selection = this.getSelection();
    let model = this._model;
    let firstLine = model.getLineAtOffset(selection.start);
    let lastLineOffset = selection.end > selection.start ?
                         selection.end - 1 : selection.end;
    let lastLine = model.getLineAtOffset(lastLineOffset);

    if (this._expandTab) {
      indent = (new Array(this._tabSize + 1)).join(" ");
    }

    let lines = [];
    for (let line, i = firstLine; i <= lastLine; i++) {
      line = model.getLine(i, true);
      if (line.indexOf(indent) != 0) {
        return true;
      }
      lines.push(line.substring(indent.length));
    }

    let firstLineStart = model.getLineStart(firstLine);
    let lastLineStart = model.getLineStart(lastLine);
    let lastLineEnd = model.getLineEnd(lastLine, true);

    this.startCompoundChange();

    this.setText(lines.join(""), firstLineStart, lastLineEnd);

    let selectedLines = lastLine - firstLine + 1;
    let newSelectionStart = firstLineStart == selection.start ?
                            selection.start :
                            Math.max(firstLineStart,
                                     selection.start - indent.length);
    let newSelectionEnd = selection.end - (selectedLines * indent.length) +
                          (selection.end == lastLineStart + 1 ? 1 : 0);
    if (firstLine == lastLine) {
      newSelectionEnd = Math.max(lastLineStart, newSelectionEnd);
    }
    this._view.setSelection(newSelectionStart, newSelectionEnd);

    this.endCompoundChange();

    return true;
  },

  /**
   * The editor Enter action implementation, which adds simple automatic
   * indentation based on the previous line when the user presses the Enter key.
   * @private
   */
  _doEnter: function SE__doEnter()
  {
    let selection = this.getSelection();
    if (selection.start != selection.end) {
      return false;
    }

    let model = this._model;
    let lineIndex = model.getLineAtOffset(selection.start);
    let lineText = model.getLine(lineIndex, true);
    let lineStart = model.getLineStart(lineIndex);
    let index = 0;
    let lineOffset = selection.start - lineStart;
    while (index < lineOffset && /[ \t]/.test(lineText.charAt(index))) {
      index++;
    }

    if (!index) {
      return false;
    }

    let prefix = lineText.substring(0, index);
    index = lineOffset;
    while (index < lineText.length &&
           /[ \t]/.test(lineText.charAt(index++))) {
      selection.end++;
    }

    this.setText(this.getLineDelimiter() + prefix, selection.start,
                 selection.end);
    return true;
  },

  /**
   * Orion Selection event handler for the X Window System users. This allows
   * one to select text and have it copied into the X11 PRIMARY.
   *
   * @private
   * @param object aEvent
   *        The Orion Selection event object.
   */
  _onOrionSelection: function SE__onOrionSelection(aEvent)
  {
    let text = this.getText(aEvent.newValue.start, aEvent.newValue.end);
    if (!text) {
      return;
    }

    clipboardHelper.copyStringToClipboard(text,
                                          Ci.nsIClipboard.kSelectionClipboard);
  },

  /**
   * Get the editor element.
   *
   * @return nsIDOMElement
   *         In this implementation a xul:iframe holds the editor.
   */
  get editorElement() {
    return this._iframe;
  },

  /**
   * Add an event listener to the editor. You can use one of the known events.
   *
   * @see SourceEditor.EVENTS
   *
   * @param string aEventType
   *        The event type you want to listen for.
   * @param function aCallback
   *        The function you want executed when the event is triggered.
   */
  addEventListener:
  function SE_addEventListener(aEventType, aCallback)
  {
    if (aEventType in ORION_EVENTS) {
      this._view.addEventListener(ORION_EVENTS[aEventType], aCallback);
    } else {
      throw new Error("SourceEditor.addEventListener() unknown event " +
                      "type " + aEventType);
    }
  },

  /**
   * Remove an event listener from the editor. You can use one of the known
   * events.
   *
   * @see SourceEditor.EVENTS
   *
   * @param string aEventType
   *        The event type you have a listener for.
   * @param function aCallback
   *        The function you have as the event handler.
   */
  removeEventListener:
  function SE_removeEventListener(aEventType, aCallback)
  {
    if (aEventType in ORION_EVENTS) {
      this._view.removeEventListener(ORION_EVENTS[aEventType], aCallback);
    } else {
      throw new Error("SourceEditor.removeEventListener() unknown event " +
                      "type " + aEventType);
    }
  },

  /**
   * Undo a change in the editor.
   */
  undo: function SE_undo()
  {
    this._undoStack.undo();
  },

  /**
   * Redo a change in the editor.
   */
  redo: function SE_redo()
  {
    this._undoStack.redo();
  },

  /**
   * Check if there are changes that can be undone.
   *
   * @return boolean
   *         True if there are changes that can be undone, false otherwise.
   */
  canUndo: function SE_canUndo()
  {
    return this._undoStack.canUndo();
  },

  /**
   * Check if there are changes that can be repeated.
   *
   * @return boolean
   *         True if there are changes that can be repeated, false otherwise.
   */
  canRedo: function SE_canRedo()
  {
    return this._undoStack.canRedo();
  },

  /**
   * Reset the Undo stack
   */
  resetUndo: function SE_resetUndo()
  {
    this._undoStack.reset();
  },

  /**
   * Start a compound change in the editor. Compound changes are grouped into
   * only one change that you can undo later, after you invoke
   * endCompoundChange().
   */
  startCompoundChange: function SE_startCompoundChange()
  {
    this._undoStack.startCompoundChange();
  },

  /**
   * End a compound change in the editor.
   */
  endCompoundChange: function SE_endCompoundChange()
  {
    this._undoStack.endCompoundChange();
  },

  /**
   * Focus the editor.
   */
  focus: function SE_focus()
  {
    this._view.focus();
  },

  /**
   * Get the first visible line number.
   *
   * @return number
   *         The line number, counting from 0.
   */
  getTopIndex: function SE_getTopIndex()
  {
    return this._view.getTopIndex();
  },

  /**
   * Set the first visible line number.
   *
   * @param number aTopIndex
   *         The line number, counting from 0.
   */
  setTopIndex: function SE_setTopIndex(aTopIndex)
  {
    this._view.setTopIndex(aTopIndex);
  },

  /**
   * Check if the editor has focus.
   *
   * @return boolean
   *         True if the editor is focused, false otherwise.
   */
  hasFocus: function SE_hasFocus()
  {
    return this._view.hasFocus();
  },

  /**
   * Get the editor content, in the given range. If no range is given you get
   * the entire editor content.
   *
   * @param number [aStart=0]
   *        Optional, start from the given offset.
   * @param number [aEnd=content char count]
   *        Optional, end offset for the text you want. If this parameter is not
   *        given, then the text returned goes until the end of the editor
   *        content.
   * @return string
   *         The text in the given range.
   */
  getText: function SE_getText(aStart, aEnd)
  {
    return this._view.getText(aStart, aEnd);
  },

  /**
   * Get the number of characters in the editor content.
   *
   * @return number
   *         The number of editor content characters.
   */
  getCharCount: function SE_getCharCount()
  {
    return this._model.getCharCount();
  },

  /**
   * Get the selected text.
   *
   * @return string
   *         The currently selected text.
   */
  getSelectedText: function SE_getSelectedText()
  {
    let selection = this.getSelection();
    return this.getText(selection.start, selection.end);
  },

  /**
   * Replace text in the source editor with the given text, in the given range.
   *
   * @param string aText
   *        The text you want to put into the editor.
   * @param number [aStart=0]
   *        Optional, the start offset, zero based, from where you want to start
   *        replacing text in the editor.
   * @param number [aEnd=char count]
   *        Optional, the end offset, zero based, where you want to stop
   *        replacing text in the editor.
   */
  setText: function SE_setText(aText, aStart, aEnd)
  {
    this._view.setText(aText, aStart, aEnd);
  },

  /**
   * Drop the current selection / deselect.
   */
  dropSelection: function SE_dropSelection()
  {
    this.setCaretOffset(this.getCaretOffset());
  },

  /**
   * Select a specific range in the editor.
   *
   * @param number aStart
   *        Selection range start.
   * @param number aEnd
   *        Selection range end.
   */
  setSelection: function SE_setSelection(aStart, aEnd)
  {
    this._view.setSelection(aStart, aEnd, true);
  },

  /**
   * Get the current selection range.
   *
   * @return object
   *         An object with two properties, start and end, that give the
   *         selection range (zero based offsets).
   */
  getSelection: function SE_getSelection()
  {
    return this._view.getSelection();
  },

  /**
   * Get the current caret offset.
   *
   * @return number
   *         The current caret offset.
   */
  getCaretOffset: function SE_getCaretOffset()
  {
    return this._view.getCaretOffset();
  },

  /**
   * Set the caret offset.
   *
   * @param number aOffset
   *        The new caret offset you want to set.
   */
  setCaretOffset: function SE_setCaretOffset(aOffset)
  {
    this._view.setCaretOffset(aOffset, true);
  },

  /**
   * Get the caret position.
   *
   * @return object
   *         An object that holds two properties:
   *         - line: the line number, counting from 0.
   *         - col: the column number, counting from 0.
   */
  getCaretPosition: function SE_getCaretPosition()
  {
    let offset = this.getCaretOffset();
    let line = this._model.getLineAtOffset(offset);
    let lineStart = this._model.getLineStart(line);
    let column = offset - lineStart;
    return {line: line, col: column};
  },

  /**
   * Set the caret position: line and column.
   *
   * @param number aLine
   *        The new caret line location. Line numbers start from 0.
   * @param number [aColumn=0]
   *        Optional. The new caret column location. Columns start from 0.
   */
  setCaretPosition: function SE_setCaretPosition(aLine, aColumn)
  {
    this.setCaretOffset(this._model.getLineStart(aLine) + (aColumn || 0));
  },

  /**
   * Get the line count.
   *
   * @return number
   *         The number of lines in the document being edited.
   */
  getLineCount: function SE_getLineCount()
  {
    return this._model.getLineCount();
  },

  /**
   * Get the line delimiter used in the document being edited.
   *
   * @return string
   *         The line delimiter.
   */
  getLineDelimiter: function SE_getLineDelimiter()
  {
    return this._model.getLineDelimiter();
  },

  /**
   * Set the source editor mode to the file type you are editing.
   *
   * @param string aMode
   *        One of the predefined SourceEditor.MODES.
   */
  setMode: function SE_setMode(aMode)
  {
    if (this._styler) {
      this._styler.destroy();
      this._styler = null;
    }
    if (this._annotationStyler) {
      this._annotationStyler.destroy();
      this._annotationStyler = null;
    }

    let window = this._iframeWindow;

    switch (aMode) {
      case SourceEditor.MODES.JAVASCRIPT:
      case SourceEditor.MODES.CSS:
        let TextStyler =
          window.require("examples/textview/textStyler").TextStyler;

        this._styler = new TextStyler(this._view, aMode, this._annotationModel);
        this._styler.setFoldingEnabled(false);
        this._styler.setHighlightCaretLine(true);

        let AnnotationStyler =
          window.require("orion/textview/annotations").AnnotationStyler;

        this._annotationStyler =
          new AnnotationStyler(this._view, this._annotationModel);
        this._annotationStyler.
          addAnnotationType(ORION_ANNOTATION_TYPES.matchingBracket);
        this._annotationStyler.
          addAnnotationType(ORION_ANNOTATION_TYPES.currentBracket);
        break;

      case SourceEditor.MODES.HTML:
      case SourceEditor.MODES.XML:
        let TextMateStyler =
          window.require("orion/editor/textMateStyler").TextMateStyler;
        let HtmlGrammar =
          window.require("orion/editor/htmlGrammar").HtmlGrammar;
        this._styler = new TextMateStyler(this._view, new HtmlGrammar().grammar);
        break;
    }

    this._mode = aMode;
  },

  /**
   * Get the current source editor mode.
   *
   * @return string
   *         Returns one of the predefined SourceEditor.MODES.
   */
  getMode: function SE_getMode()
  {
    return this._mode;
  },

  /**
   * Setter for the read-only state of the editor.
   * @param boolean aValue
   *        Tells if you want the editor to read-only or not.
   */
  set readOnly(aValue)
  {
    this._view.setOptions({
      readonly: aValue,
      themeClass: "mozilla" + (aValue ? " readonly" : ""),
    });
  },

  /**
   * Getter for the read-only state of the editor.
   * @type boolean
   */
  get readOnly()
  {
    return this._view.getOptions("readonly");
  },

  /**
   * Destroy/uninitialize the editor.
   */
  destroy: function SE_destroy()
  {
    if (Services.appinfo.OS == "Linux") {
      this._view.removeEventListener("Selection", this._onOrionSelection);
    }
    this._onOrionSelection = null;

    this._view.destroy();
    this.parentElement.removeChild(this._iframe);
    this.parentElement = null;
    this._iframeWindow = null;
    this._iframe = null;
    this._undoStack = null;
    this._styler = null;
    this._linesRuler = null;
    this._dragAndDrop = null;
    this._annotationModel = null;
    this._annotationStyler = null;
    this._view = null;
    this._model = null;
    this._config = null;
  },
};
