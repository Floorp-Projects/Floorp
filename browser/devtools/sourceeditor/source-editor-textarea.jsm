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
 * The Original Code is the Source Editor component (textarea fallback).
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
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

var EXPORTED_SYMBOLS = ["SourceEditor"];

/**
 * The SourceEditor object constructor. The SourceEditor component allows you to
 * provide users with an editor tailored to the specific needs of editing source
 * code, aimed primarily at web developers.
 *
 * The editor used here is a simple textarea. This is used as a fallback
 * mechanism for when the user disables the code editor feature.
 *
 * @constructor
 */
function SourceEditor() {
  // Update the SourceEditor defaults from user preferences.

  SourceEditor.DEFAULTS.TAB_SIZE =
    Services.prefs.getIntPref(SourceEditor.PREFS.TAB_SIZE);
  SourceEditor.DEFAULTS.EXPAND_TAB =
    Services.prefs.getBoolPref(SourceEditor.PREFS.EXPAND_TAB);

  this._listeners = {};
  this._lastSelection = {};
}

SourceEditor.prototype = {
  _textbox: null,
  _editor: null,
  _listeners: null,
  _lineDelimiter: null,
  _editActionListener: null,
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
   *          - mode - the editor mode, based on the file type you want to edit.
   *          You can use one of the predefined modes.
   *          - tabSize - define how many spaces to use for a tab character.
   *          - expandTab - tells if you want tab characters to be expanded to
   *          spaces.
   *          - readOnly - make the editor read only.
   *          - undoLimit - how many steps should the undo stack hold.
   * @param function [aCallback]
   *        Function you want to execute once the editor is loaded and
   *        initialized.
   */
  init: function SE_init(aElement, aConfig, aCallback)
  {
    if (this._textbox) {
      throw new Error("SourceEditor is already initialized!");
    }

    let doc = aElement.ownerDocument;
    let win = doc.defaultView;

    this._textbox = doc.createElementNS(XUL_NS, "textbox");
    this._textbox.flex = 1;
    this._textbox.setAttribute("multiline", true);
    this._textbox.setAttribute("dir", "ltr");

    aElement.appendChild(this._textbox);

    this.parentElement = aElement;
    this._editor = this._textbox.editor;

    this._expandTab = aConfig.expandTab !== undefined ?
                      aConfig.expandTab : SourceEditor.DEFAULTS.EXPAND_TAB;
    this._tabSize = aConfig.tabSize || SourceEditor.DEFAULTS.TAB_SIZE;

    this._textbox.style.MozTabSize = this._tabSize;

    this._textbox.setAttribute("value", aConfig.placeholderText);
    this._textbox.setAttribute("class", "monospace");
    this._textbox.style.direction = "ltr";
    this._textbox.readOnly = aConfig.readOnly;

    // Make sure that the SourceEditor Selection events are fired properly.
    // Also make sure that the Tab key inserts spaces when expandTab is true.
    this._textbox.addEventListener("select", this._onSelect.bind(this), false);
    this._textbox.addEventListener("keypress", this._onKeyPress.bind(this), false);
    this._textbox.addEventListener("keyup", this._onSelect.bind(this), false);
    this._textbox.addEventListener("click", this._onSelect.bind(this), false);

    // Mimic the mode change.
    this.setMode(aConfig.mode || SourceEditor.DEFAULTS.MODE);

    this._editor.transactionManager.maxTransactionCount =
      aConfig.undoLimit || SourceEditor.DEFAULTS.UNDO_LIMIT;

    // Make sure that the transactions stack is clean.
    this._editor.transactionManager.clear();
    this._editor.resetModificationCount();

    // Add the edit action listener so we can fire the SourceEditor TextChanged
    // events.
    this._editActionListener = new EditActionListener(this);
    this._editor.addEditActionListener(this._editActionListener);

    this._lineDelimiter = win.navigator.platform.indexOf("Win") > -1 ?
                          "\r\n" : "\n";

    this._config = aConfig;

    if (aCallback) {
      aCallback(this);
    }
  },

  /**
   * The textbox keypress event handler allows users to indent code using the
   * Tab key.
   *
   * @private
   * @param nsIDOMEvent aEvent
   *        The DOM object for the event.
   */
  _onKeyPress: function SE__onKeyPress(aEvent)
  {
    if (aEvent.keyCode != aEvent.DOM_VK_TAB || aEvent.shiftKey ||
        aEvent.metaKey || aEvent.ctrlKey || aEvent.altKey) {
      return;
    }

    aEvent.preventDefault();

    let caret = this.getCaretOffset();
    let indent = "\t";

    if (this._expandTab) {
      let text = this._textbox.value;
      let lineStart = caret;
      while (lineStart > 0) {
        let c = text.charAt(lineStart - 1);
        if (c == "\r" || c == "\n") {
          break;
        }
        lineStart--;
      }
      let offset = caret - lineStart;
      let spaces = this._tabSize - (offset % this._tabSize);
      indent = (new Array(spaces + 1)).join(" ");
    }

    this.setText(indent, caret, caret);
    this.setCaretOffset(caret + indent.length);
  },

  /**
   * The textbox keyup, click and select event handler tracks selection
   * changes. This method invokes the SourceEditor Selection event handlers.
   *
   * @see SourceEditor.EVENTS.SELECTION
   * @private
   */
  _onSelect: function SE__onSelect()
  {
    let selection = this.getSelection();
    selection.collapsed = selection.start == selection.end;
    if (selection.collapsed && this._lastSelection.collapsed) {
      this._lastSelection = selection;
      return; // just a cursor move.
    }

    if (this._lastSelection.start != selection.start ||
        this._lastSelection.end != selection.end) {
      let sendEvent = {
        oldValue: {start: this._lastSelection.start,
                   end: this._lastSelection.end},
        newValue: {start: selection.start, end: selection.end},
      };

      let listeners = this._listeners[SourceEditor.EVENTS.SELECTION] || [];
      listeners.forEach(function(aListener) {
        aListener.callback.call(null, sendEvent, aListener.data);
      }, this);

      this._lastSelection = selection;
    }
  },

  /**
   * The TextChanged event dispatcher. This method is called when a change in
   * the text occurs. All of the SourceEditor TextChanged event handlers are
   * notified about the lower level change.
   *
   * @see SourceEditor.EVENTS.TEXT_CHANGED
   * @see EditActionListener
   *
   * @private
   *
   * @param object aEvent
   *        The TextChanged event object that is going to be sent to the
   *        SourceEditor event handlers.
   */
  _onTextChanged: function SE__onTextChanged(aEvent)
  {
    let listeners = this._listeners[SourceEditor.EVENTS.TEXT_CHANGED] || [];
    listeners.forEach(function(aListener) {
      aListener.callback.call(null, aEvent, aListener.data);
    }, this);
  },

  /**
   * Get the editor element.
   *
   * @return nsIDOMElement
   *         In this implementation a xul:textbox is returned.
   */
  get editorElement() {
    return this._textbox;
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
    const EVENTS = SourceEditor.EVENTS;
    let listener = {
      type: aEventType,
      data: aData,
      callback: aCallback,
    };

    if (aEventType == EVENTS.CONTEXT_MENU) {
      listener.domType = "contextmenu";
      listener.target = this._textbox;
      listener.handler = this._onContextMenu.bind(this, listener);
      listener.target.addEventListener(listener.domType, listener.handler, false);
    }

    if (!(aEventType in this._listeners)) {
      this._listeners[aEventType] = [];
    }

    this._listeners[aEventType].push(listener);
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
    let listeners = this._listeners[aEventType];
    if (!listeners) {
      throw new Error("SourceEditor.removeEventListener() called for an " +
                      "unknown event.");
    }

    const EVENTS = SourceEditor.EVENTS;

    this._listeners[aEventType] = listeners.filter(function(aListener) {
      let isSameListener = aListener.type == aEventType &&
                           aListener.callback === aCallback &&
                           aListener.data === aData;
      if (isSameListener && aListener.domType) {
        aListener.target.removeEventListener(aListener.domType,
                                             aListener.handler, false);
      }
      return !isSameListener;
    }, this);
  },

  /**
   * The xul:textbox contextmenu event handler. This is used a wrapper for each
   * contextmenu event listener added by the SourceEditor client.
   *
   * @param object aListener
   *        The object that holds listener information, see this._listener and
   *        this.addEventListener().
   * @param nsIDOMEvent aDOMEvent
   *        The nsIDOMEvent object that triggered the context menu.
   */
  _onContextMenu: function SE__onContextMenu(aListener, aDOMEvent)
  {
    let input = this._textbox.inputField;
    let rect = this._textbox.getBoundingClientRect();

    // Prepare the event object we send to the event handler.
    let sendEvent = {
      x: aDOMEvent.clientX - rect.left + input.scrollLeft,
      y: aDOMEvent.clientY - rect.top + input.scrollTop,
      screenX: aDOMEvent.screenX,
      screenY: aDOMEvent.screenY,
    };

    aDOMEvent.preventDefault();
    aListener.callback.call(null, sendEvent, aListener.data);
  },

  /**
   * Undo a change in the editor.
   */
  undo: function SE_undo()
  {
    this._editor.undo(1);
  },

  /**
   * Redo a change in the editor.
   */
  redo: function SE_redo()
  {
    this._editor.redo(1);
  },

  /**
   * Check if there are changes that can be undone.
   *
   * @return boolean
   *         True if there are changes that can be undone, false otherwise.
   */
  canUndo: function SE_canUndo()
  {
    let isEnabled = {};
    let canUndo = {};
    this._editor.canUndo(isEnabled, canUndo);
    return canUndo.value;
  },

  /**
   * Check if there are changes that can be repeated.
   *
   * @return boolean
   *         True if there are changes that can be repeated, false otherwise.
   */
  canRedo: function SE_canRedo()
  {
    let isEnabled = {};
    let canRedo = {};
    this._editor.canRedo(isEnabled, canRedo);
    return canRedo.value;
  },

  /**
   * Start a compound change in the editor. Compound changes are grouped into
   * only one change that you can undo later, after you invoke
   * endCompoundChange().
   */
  startCompoundChange: function SE_startCompoundChange()
  {
    this._editor.beginTransaction();
  },

  /**
   * End a compound change in the editor.
   */
  endCompoundChange: function SE_endCompoundChange()
  {
    this._editor.endTransaction();
  },

  /**
   * Focus the editor.
   */
  focus: function SE_focus()
  {
    this._textbox.focus();
  },

  /**
   * Check if the editor has focus.
   *
   * @return boolean
   *         True if the editor is focused, false otherwise.
   */
  hasFocus: function SE_hasFocus()
  {
    return this._textbox.ownerDocument.activeElement ===
           this._textbox.inputField;
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
    let value = this._textbox.value || "";
    if (aStart === undefined || aStart === null) {
      aStart = 0;
    }
    if (aEnd === undefined || aEnd === null) {
      aEnd = value.length;
    }
    return value.substring(aStart, aEnd);
  },

  /**
   * Get the number of characters in the editor content.
   *
   * @return number
   *         The number of editor content characters.
   */
  getCharCount: function SE_getCharCount()
  {
    return this._textbox.textLength;
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
    return selection.start != selection.end ?
           this.getText(selection.start, selection.end) : "";
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
    if (aStart === undefined) {
      this._textbox.value = aText;
    } else {
      if (aEnd === undefined) {
        aEnd = this._textbox.textLength;
      }

      let value = this._textbox.value || "";
      let removedText = value.substring(aStart, aEnd);
      let prefix = value.substr(0, aStart);
      let suffix = value.substr(aEnd);

      if (suffix) {
        this._editActionListener._setTextRangeEvent = {
          start: aStart,
          removedCharCount: removedText.length,
          addedCharCount: aText.length,
        };
      }

      this._textbox.value = prefix + aText + suffix;
    }
  },

  /**
   * Drop the current selection / deselect.
   */
  dropSelection: function SE_dropSelection()
  {
    let selection = this._editor.selection;
    selection.collapse(selection.focusNode, selection.focusOffset);
    this._onSelect();
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
    this._textbox.setSelectionRange(aStart, aEnd);
    this._onSelect();
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
    return {
      start: this._textbox.selectionStart,
      end: this._textbox.selectionEnd
    };
  },

  /**
   * Get the current caret offset.
   *
   * @return number
   *         The current caret offset.
   */
  getCaretOffset: function SE_getCaretOffset()
  {
    let selection = this.getSelection();
    return selection.start < selection.end ?
           selection.end : selection.start;
  },

  /**
   * Set the caret offset.
   *
   * @param number aOffset
   *        The new caret offset you want to set.
   */
  setCaretOffset: function SE_setCaretOffset(aOffset)
  {
    this.setSelection(aOffset, aOffset);
  },

  /**
   * Get the line delimiter used in the document being edited.
   *
   * @return string
   *         The line delimiter.
   */
  getLineDelimiter: function SE_getLineDelimiter()
  {
    return this._lineDelimiter;
  },

  /**
   * Set the source editor mode to the file type you are editing.
   *
   * Note: this implementation makes no difference between any of the available
   * modes.
   *
   * @param string aMode
   *        One of the predefined SourceEditor.MODES.
   */
  setMode: function SE_setMode(aMode)
  {
    // nothing to do here
  },

  /**
   * Get the current source editor mode.
   *
   * @return string
   *         Returns one of the predefined SourceEditor.MODES. In this
   *         implementation SourceEditor.MODES.TEXT is always returned.
   */
  getMode: function SE_getMode()
  {
    return SourceEditor.MODES.TEXT;
  },

  /**
   * Setter for the read-only state of the editor.
   * @param boolean aValue
   *        Tells if you want the editor to read-only or not.
   */
  set readOnly(aValue)
  {
    this._textbox.readOnly = aValue;
  },

  /**
   * Getter for the read-only state of the editor.
   * @type boolean
   */
  get readOnly()
  {
    return this._textbox.readOnly;
  },

  /**
   * Destroy/uninitialize the editor.
   */
  destroy: function SE_destroy()
  {
    for (let eventType in this._listeners) {
      this._listeners[eventType].forEach(function(aListener) {
        if (aListener.domType) {
          aListener.target.removeEventListener(aListener.domType,
                                               aListener.handler, false);
        }
      }, this);
    }

    this._editor.removeEditActionListener(this._editActionListener);
    this.parentElement.removeChild(this._textbox);
    this.parentElement = null;
    this._editor = null;
    this._textbox = null;
    this._config = null;
    this._listeners = null;
    this._lastSelection = null;
    this._editActionListener = null;
  },
};

/**
 * The nsIEditActionListener for the nsIEditor of the xul:textbox used by the
 * SourceEditor. This listener traces text changes such that SourceEditor
 * TextChanged event handlers get their events.
 *
 * @see
 * http://mxr.mozilla.org/mozilla-central/source/editor/idl/nsIEditActionListener.idl
 *
 * @constructor
 * @param object aSourceEditor
 *        An instance of the SourceEditor to notify when text changes happen.
 */
function EditActionListener(aSourceEditor) {
  this._sourceEditor = aSourceEditor;
}

EditActionListener.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIEditActionListener]),

  WillCreateNode: function() { },
  DidCreateNode: function() { },
  WillInsertNode: function() { },

  DidInsertNode: function EAL_DidInsertNode(aNode)
  {
    if (aNode.nodeType != aNode.TEXT_NODE) {
      return;
    }

    let event;

    if (this._setTextRangeEvent) {
      event = this._setTextRangeEvent;
      delete this._setTextRangeEvent;
    } else {
      event = {
        start: 0,
        removedCharCount: 0,
        addedCharCount: aNode.textContent.length,
      };
    }

    this._sourceEditor._onTextChanged(event);
  },

  WillDeleteNode: function() { },
  DidDeleteNode: function() { },
  WillSplitNode: function() { },
  DidSplitNode: function() { },
  WillJoinNodes: function() { },
  DidJoinNodes: function() { },
  WillInsertText: function() { },

  DidInsertText: function EAL_DidInsertText(aTextNode, aOffset, aString)
  {
    let event = {
      start: aOffset,
      removedCharCount: 0,
      addedCharCount: aString.length,
    };

    this._sourceEditor._onTextChanged(event);
  },

  WillDeleteText: function() { },

  DidDeleteText: function EAL_DidDeleteText(aTextNode, aOffset, aLength)
  {
    let event = {
      start: aOffset,
      removedCharCount: aLength,
      addedCharCount: 0,
    };

    this._sourceEditor._onTextChanged(event);
  },

  WillDeleteSelection: function EAL_WillDeleteSelection()
  {
    if (this._setTextRangeEvent) {
      return;
    }

    let selection = this._sourceEditor.getSelection();
    let str = this._sourceEditor.getSelectedText();

    let event = {
      start: selection.start,
      removedCharCount: str.length,
      addedCharCount: 0,
    };

    this._sourceEditor._onTextChanged(event);
  },

  DidDeleteSelection: function() { },
};
