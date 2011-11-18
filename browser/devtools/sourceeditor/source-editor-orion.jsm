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
  textmate: ["chrome://browser/content/orion.css",
             "chrome://browser/content/orion-mozilla.css"],
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
}

SourceEditor.prototype = {
  _view: null,
  _iframe: null,
  _undoStack: null,
  _lines_ruler: null,
  _styler: null,
  _mode: null,
  _expandTab: null,
  _tabSize: null,

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

      Services.scriptloader.loadSubScript(ORION_SCRIPT,
        this._iframe.contentWindow.wrappedJSObject, "utf8");

      this._onLoad(aCallback);
    }).bind(this);

    this._iframe.addEventListener("load", onIframeLoad, true);

    this._iframe.setAttribute("src", ORION_IFRAME);

    aElement.appendChild(this._iframe);
    this.parentElement = aElement;
    this._config = aConfig;
  },

  /**
   * The editor iframe load event handler.
   *
   * @private
   * @param function [aCallback]
   *        Optional function invoked when the editor completes loading.
   */
  _onLoad: function SE__onLoad(aCallback)
  {
    let config = this._config;
    let window = this._iframe.contentWindow.wrappedJSObject;
    let textview = window.orion.textview;

    this._expandTab = typeof config.expandTab != "undefined" ?
                      config.expandTab : SourceEditor.DEFAULTS.EXPAND_TAB;
    this._tabSize = config.tabSize || SourceEditor.DEFAULTS.TAB_SIZE;

    let theme = config.theme || SourceEditor.DEFAULTS.THEME;
    let stylesheet = theme in ORION_THEMES ? ORION_THEMES[theme] : theme;

    this._view = new textview.TextView({
      model: new textview.TextModel(config.placeholderText),
      parent: "editor",
      stylesheet: stylesheet,
      tabSize: this._tabSize,
      readonly: config.readOnly,
    });

    if (config.showLineNumbers) {
      this._lines_ruler = new textview.LineNumberRuler(null, "left",
        {styleClass: "rulerLines"}, {styleClass: "rulerLine odd"},
        {styleClass: "rulerLine even"});

      this._view.addRuler(this._lines_ruler);
    }

    this.setMode(config.mode || SourceEditor.DEFAULTS.MODE);

    this._undoStack = new textview.UndoStack(this._view,
      config.undoLimit || SourceEditor.DEFAULTS.UNDO_LIMIT);

    this._initEditorFeatures();

    (config.keys || []).forEach(function(aKey) {
      let binding = new textview.KeyBinding(aKey.code, aKey.accel, aKey.shift,
                                            aKey.alt);
      this._view.setKeyBinding(binding, aKey.action);

      if (aKey.callback) {
        this._view.setAction(aKey.action, aKey.callback);
      }
    }, this);

    if (aCallback) {
      let iframe = this._view._frame;
      let document = iframe.contentDocument;
      if (!document || document.readyState != "complete") {
        let onIframeLoad = function () {
          iframe.contentWindow.removeEventListener("load", onIframeLoad, false);
          aCallback(this);
        }.bind(this);
        iframe.contentWindow.addEventListener("load", onIframeLoad, false);
      } else {
        aCallback(this);
      }
    }
  },

  /**
   * Initialize the custom Orion editor features.
   * @private
   */
  _initEditorFeatures: function SE__initEditorFeatures()
  {
    let window = this._iframe.contentWindow.wrappedJSObject;
    let textview = window.orion.textview;

    this._view.setAction("tab", this._doTab.bind(this));

    let shiftTabKey = new textview.KeyBinding(Ci.nsIDOMKeyEvent.DOM_VK_TAB,
                                              false, true);
    this._view.setAction("Unindent Lines", this._doUnindentLines.bind(this));
    this._view.setKeyBinding(shiftTabKey, "Unindent Lines");
    this._view.setAction("enter", this._doEnter.bind(this));

    if (this._expandTab) {
      this._view.setAction("deletePrevious", this._doDeletePrevious.bind(this));
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
    } else {
      this.setText(indent, selection.start, selection.end);
    }

    return true;
  },

  /**
   * The "deletePrevious" editor action implementation. This adds unindentation
   * support to the Backspace key implementation.
   * @private
   */
  _doDeletePrevious: function SE__doDeletePrevious()
  {
    let selection = this.getSelection();
    if (selection.start == selection.end && this._expandTab) {
      let model = this._model;
      let lineIndex = model.getLineAtOffset(selection.start);
      let lineStart = model.getLineStart(lineIndex);
      let offset = selection.start - lineStart;
      if (offset >= this._tabSize && (offset % this._tabSize) == 0) {
        let text = this.getText(lineStart, selection.start);
        if (!/[^ ]/.test(text)) {
          this.setText("", selection.start - this._tabSize, selection.end);
          return true;
        }
      }
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
        return false;
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
   * Get the Orion Model, the TextModel object instance we use.
   * @private
   * @type object
   */
  get _model() {
    return this._view.getModel();
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
   * @param mixed [aData]
   *        Optional data to pass to the callback when the event is triggered.
   */
  addEventListener:
  function SE_addEventListener(aEventType, aCallback, aData)
  {
    if (aEventType in ORION_EVENTS) {
      this._view.addEventListener(ORION_EVENTS[aEventType], true,
                                  aCallback, aData);
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
   * @param mixed [aData]
   *        The optional data passed to the callback.
   */
  removeEventListener:
  function SE_removeEventListener(aEventType, aCallback, aData)
  {
    if (aEventType in ORION_EVENTS) {
      this._view.removeEventListener(ORION_EVENTS[aEventType], true,
                                     aCallback, aData);
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
    return this._iframe.ownerDocument.activeElement === this._iframe;
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

    let window = this._iframe.contentWindow.wrappedJSObject;
    let TextStyler = window.examples.textview.TextStyler;
    let TextMateStyler = window.orion.editor.TextMateStyler;
    let HtmlGrammar = window.orion.editor.HtmlGrammar;

    switch (aMode) {
      case SourceEditor.MODES.JAVASCRIPT:
      case SourceEditor.MODES.CSS:
        this._styler = new TextStyler(this._view, aMode);
        break;

      case SourceEditor.MODES.HTML:
      case SourceEditor.MODES.XML:
        this._styler = new TextMateStyler(this._view, HtmlGrammar.grammar);
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
    this._view.readonly = aValue;
  },

  /**
   * Getter for the read-only state of the editor.
   * @type boolean
   */
  get readOnly()
  {
    return this._view.readonly;
  },

  /**
   * Destroy/uninitialize the editor.
   */
  destroy: function SE_destroy()
  {
    this._view.destroy();
    this.parentElement.removeChild(this._iframe);
    this.parentElement = null;
    this._iframe = null;
    this._undoStack = null;
    this._styler = null;
    this._lines_ruler = null;
    this._view = null;
    this._config = null;
  },
};
