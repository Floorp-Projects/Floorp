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
 *   Spyros Livathinos <livathinos.spyros@gmail.com>
 *   Allen Eubank <adeubank@gmail.com>
 *   Girish Sharma <scrapmachines@gmail.com>
 *   Pranav Ravichandran <prp.1111@gmail.com>
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
Cu.import("resource:///modules/source-editor-ui.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "clipboardHelper",
                                   "@mozilla.org/widget/clipboardhelper;1",
                                   "nsIClipboardHelper");

const ORION_SCRIPT = "chrome://browser/content/orion.js";
const ORION_IFRAME = "data:text/html;charset=utf8,<!DOCTYPE html>" +
  "<html style='height:100%' dir='ltr'>" +
  "<head><link rel='stylesheet'" +
  " href='chrome://browser/skin/devtools/orion-container.css'></head>" +
  "<body style='height:100%;margin:0;overflow:hidden'>" +
  "<div id='editor' style='height:100%'></div>" +
  "</body></html>";

const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

/**
 * Maximum allowed vertical offset for the line index when you call
 * SourceEditor.setCaretPosition().
 *
 * @type number
 */
const VERTICAL_OFFSET = 3;

/**
 * The primary selection update delay. On Linux, the X11 primary selection is
 * updated to hold the currently selected text.
 *
 * @type number
 */
const PRIMARY_SELECTION_DELAY = 100;

/**
 * Predefined themes for syntax highlighting. This objects maps
 * SourceEditor.THEMES to Orion CSS files.
 */
const ORION_THEMES = {
  mozilla: ["chrome://browser/skin/devtools/orion.css"],
};

/**
 * Known Orion editor events you can listen for. This object maps several of the
 * SourceEditor.EVENTS to Orion events.
 */
const ORION_EVENTS = {
  ContextMenu: "ContextMenu",
  TextChanged: "ModelChanged",
  Selection: "Selection",
  Focus: "Focus",
  Blur: "Blur",
  MouseOver: "MouseOver",
  MouseOut: "MouseOut",
  MouseMove: "MouseMove",
};

/**
 * Known Orion annotation types.
 */
const ORION_ANNOTATION_TYPES = {
  currentBracket: "orion.annotation.currentBracket",
  matchingBracket: "orion.annotation.matchingBracket",
  breakpoint: "orion.annotation.breakpoint",
  task: "orion.annotation.task",
  currentLine: "orion.annotation.currentLine",
  debugLocation: "mozilla.annotation.debugLocation",
};

/**
 * Default key bindings in the Orion editor.
 */
const DEFAULT_KEYBINDINGS = [
  {
    action: "enter",
    code: Ci.nsIDOMKeyEvent.DOM_VK_ENTER,
  },
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
  {
    action: "Move Lines Up",
    code: Ci.nsIDOMKeyEvent.DOM_VK_UP,
    ctrl: Services.appinfo.OS == "Darwin",
    alt: true,
  },
  {
    action: "Move Lines Down",
    code: Ci.nsIDOMKeyEvent.DOM_VK_DOWN,
    ctrl: Services.appinfo.OS == "Darwin",
    alt: true,
  },
  {
    action: "Comment/Uncomment",
    code: Ci.nsIDOMKeyEvent.DOM_VK_SLASH,
    accel: true,
  },
  {
    action: "Move to Bracket Opening",
    code: Ci.nsIDOMKeyEvent.DOM_VK_OPEN_BRACKET,
    accel: true,
  },
  {
    action: "Move to Bracket Closing",
    code: Ci.nsIDOMKeyEvent.DOM_VK_CLOSE_BRACKET,
    accel: true,
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

  SourceEditor.DEFAULTS.tabSize =
    Services.prefs.getIntPref(SourceEditor.PREFS.TAB_SIZE);
  SourceEditor.DEFAULTS.expandTab =
    Services.prefs.getBoolPref(SourceEditor.PREFS.EXPAND_TAB);

  this._onOrionSelection = this._onOrionSelection.bind(this);
  this._onTextChanged = this._onTextChanged.bind(this);
  this._onOrionContextMenu = this._onOrionContextMenu.bind(this);

  this._eventTarget = {};
  this._eventListenersQueue = [];
  this.ui = new SourceEditorUI(this);
}

SourceEditor.prototype = {
  _view: null,
  _iframe: null,
  _model: null,
  _undoStack: null,
  _linesRuler: null,
  _annotationRuler: null,
  _overviewRuler: null,
  _styler: null,
  _annotationStyler: null,
  _annotationModel: null,
  _dragAndDrop: null,
  _currentLineAnnotation: null,
  _primarySelectionTimeout: null,
  _mode: null,
  _expandTab: null,
  _tabSize: null,
  _iframeWindow: null,
  _eventTarget: null,
  _eventListenersQueue: null,
  _contextMenu: null,
  _dirty: false,

  /**
   * The Source Editor user interface manager.
   * @type object
   *       An instance of the SourceEditorUI.
   */
  ui: null,

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
   *        Editor configuration object. See SourceEditor.DEFAULTS for the
   *        available configuration options.
   * @param function [aCallback]
   *        Function you want to execute once the editor is loaded and
   *        initialized.
   * @see SourceEditor.DEFAULTS
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

    this._config = {};
    for (let key in SourceEditor.DEFAULTS) {
      this._config[key] = key in aConfig ?
                          aConfig[key] :
                          SourceEditor.DEFAULTS[key];
    }

    // TODO: Bug 725677 - Remove the deprecated placeholderText option from the
    // Source Editor initialization.
    if (aConfig.placeholderText) {
      this._config.initialText = aConfig.placeholderText;
      Services.console.logStringMessage("SourceEditor.init() was called with the placeholderText option which is deprecated, please use initialText.");
    }

    this._onReadyCallback = aCallback;
    this.ui.init();
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

    this._expandTab = config.expandTab;
    this._tabSize = config.tabSize;

    let theme = config.theme;
    let stylesheet = theme in ORION_THEMES ? ORION_THEMES[theme] : theme;

    this._model = new TextModel(config.initialText);
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
    if (config.highlightCurrentLine || Services.appinfo.OS == "Linux") {
      this.addEventListener(SourceEditor.EVENTS.SELECTION,
                            this._onOrionSelection);
    }
    this.addEventListener(SourceEditor.EVENTS.TEXT_CHANGED,
                           this._onTextChanged);

    if (typeof config.contextMenu == "string") {
      let chromeDocument = this.parentElement.ownerDocument;
      this._contextMenu = chromeDocument.getElementById(config.contextMenu);
    } else if (typeof config.contextMenu == "object" ) {
      this._contextMenu = config._contextMenu;
    }
    if (this._contextMenu) {
      this.addEventListener(SourceEditor.EVENTS.CONTEXT_MENU,
                            this._onOrionContextMenu);
    }

    let KeyBinding = window.require("orion/textview/keyBinding").KeyBinding;
    let TextDND = window.require("orion/textview/textDND").TextDND;
    let Rulers = window.require("orion/textview/rulers");
    let LineNumberRuler = Rulers.LineNumberRuler;
    let AnnotationRuler = Rulers.AnnotationRuler;
    let OverviewRuler = Rulers.OverviewRuler;
    let UndoStack = window.require("orion/textview/undoStack").UndoStack;
    let AnnotationModel = window.require("orion/textview/annotations").AnnotationModel;

    this._annotationModel = new AnnotationModel(this._model);

    if (config.showAnnotationRuler) {
      this._annotationRuler = new AnnotationRuler(this._annotationModel, "left",
        {styleClass: "ruler annotations"});
      this._annotationRuler.onClick = this._annotationRulerClick.bind(this);
      this._annotationRuler.addAnnotationType(ORION_ANNOTATION_TYPES.breakpoint);
      this._annotationRuler.addAnnotationType(ORION_ANNOTATION_TYPES.debugLocation);
      this._view.addRuler(this._annotationRuler);
    }

    if (config.showLineNumbers) {
      let rulerClass = this._annotationRuler ?
                       "ruler lines linesWithAnnotations" :
                       "ruler lines";

      this._linesRuler = new LineNumberRuler(this._annotationModel, "left",
        {styleClass: rulerClass}, {styleClass: "rulerLines odd"},
        {styleClass: "rulerLines even"});

      this._linesRuler.onClick = this._linesRulerClick.bind(this);
      this._linesRuler.onDblClick = this._linesRulerDblClick.bind(this);
      this._view.addRuler(this._linesRuler);
    }

    if (config.showOverviewRuler) {
      this._overviewRuler = new OverviewRuler(this._annotationModel, "right",
        {styleClass: "ruler overview"});
      this._overviewRuler.onClick = this._overviewRulerClick.bind(this);

      this._overviewRuler.addAnnotationType(ORION_ANNOTATION_TYPES.matchingBracket);
      this._overviewRuler.addAnnotationType(ORION_ANNOTATION_TYPES.currentBracket);
      this._overviewRuler.addAnnotationType(ORION_ANNOTATION_TYPES.breakpoint);
      this._overviewRuler.addAnnotationType(ORION_ANNOTATION_TYPES.debugLocation);
      this._overviewRuler.addAnnotationType(ORION_ANNOTATION_TYPES.task);
      this._view.addRuler(this._overviewRuler);
    }

    this.setMode(config.mode);

    this._undoStack = new UndoStack(this._view, config.undoLimit);

    this._dragAndDrop = new TextDND(this._view, this._undoStack);

    let actions = {
      "undo": [this.undo, this],
      "redo": [this.redo, this],
      "tab": [this._doTab, this],
      "Unindent Lines": [this._doUnindentLines, this],
      "enter": [this._doEnter, this],
      "Find...": [this.ui.find, this.ui],
      "Find Next Occurrence": [this.ui.findNext, this.ui],
      "Find Previous Occurrence": [this.ui.findPrevious, this.ui],
      "Goto Line...": [this.ui.gotoLine, this.ui],
      "Move Lines Down": [this._moveLines, this],
      "Comment/Uncomment": [this._doCommentUncomment, this],
      "Move to Bracket Opening": [this._moveToBracketOpening, this],
      "Move to Bracket Closing": [this._moveToBracketClosing, this],
    };

    for (let name in actions) {
      let action = actions[name];
      this._view.setAction(name, action[0].bind(action[1]));
    }

    this._view.setAction("Move Lines Up", this._moveLines.bind(this, true));

    let keys = (config.keys || []).concat(DEFAULT_KEYBINDINGS);
    keys.forEach(function(aKey) {
      // In Orion mod1 refers to Cmd on Macs and Ctrl on Windows and Linux.
      // So, if ctrl is in aKey we use it on Windows and Linux, otherwise
      // we use aKey.accel for mod1.
      let mod1 = Services.appinfo.OS != "Darwin" &&
                 "ctrl" in aKey ? aKey.ctrl : aKey.accel;
      let binding = new KeyBinding(aKey.code, mod1, aKey.shift, aKey.alt,
                                  aKey.ctrl);
      this._view.setKeyBinding(binding, aKey.action);

      if (aKey.callback) {
        this._view.setAction(aKey.action, aKey.callback);
      }
    }, this);

    this._initEventTarget();
  },

  /**
   * Initialize the private Orion EventTarget object. This is used for tracking
   * our own event listeners for events outside of Orion's scope.
   * @private
   */
  _initEventTarget: function SE__initEventTarget()
  {
    let EventTarget =
      this._iframeWindow.require("orion/textview/eventTarget").EventTarget;
    EventTarget.addMixin(this._eventTarget);

    this._eventListenersQueue.forEach(function(aRequest) {
      if (aRequest[0] == "add") {
        this.addEventListener(aRequest[1], aRequest[2]);
      } else {
        this.removeEventListener(aRequest[1], aRequest[2]);
      }
    }, this);

    this._eventListenersQueue = [];
  },

  /**
   * Dispatch an event to the SourceEditor event listeners. This covers only the
   * SourceEditor-specific events.
   *
   * @private
   * @param object aEvent
   *        The event object to dispatch to all listeners.
   */
  _dispatchEvent: function SE__dispatchEvent(aEvent)
  {
    this._eventTarget.dispatchEvent(aEvent);
  },

  /**
   * The Orion "Load" event handler. This is called when the Orion editor
   * completes the initialization.
   * @private
   */
  _onOrionLoad: function SE__onOrionLoad()
  {
    this.ui.onReady();
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
    if (this.readOnly) {
      return false;
    }

    let indent = "\t";
    let selection = this.getSelection();
    let model = this._model;
    let firstLine = model.getLineAtOffset(selection.start);
    let firstLineStart = this.getLineStart(firstLine);
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
      let lastLineEnd = this.getLineEnd(lastLine, true);
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
    if (this.readOnly) {
      return true;
    }

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

    let firstLineStart = this.getLineStart(firstLine);
    let lastLineStart = this.getLineStart(lastLine);
    let lastLineEnd = this.getLineEnd(lastLine, true);

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
    if (this.readOnly) {
      return false;
    }

    let selection = this.getSelection();
    if (selection.start != selection.end) {
      return false;
    }

    let model = this._model;
    let lineIndex = model.getLineAtOffset(selection.start);
    let lineText = model.getLine(lineIndex, true);
    let lineStart = this.getLineStart(lineIndex);
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
   * Move lines upwards or downwards, relative to the current caret location.
   *
   * @private
   * @param boolean aLineAbove
   *        True if moving lines up, false to move lines down.
   */
  _moveLines: function SE__moveLines(aLineAbove)
  {
    if (this.readOnly) {
      return false;
    }

    let model = this._model;
    let selection = this.getSelection();
    let firstLine = model.getLineAtOffset(selection.start);
    if (firstLine == 0 && aLineAbove) {
      return true;
    }

    let lastLine = model.getLineAtOffset(selection.end);
    let firstLineStart = this.getLineStart(firstLine);
    let lastLineStart = this.getLineStart(lastLine);
    if (selection.start != selection.end && lastLineStart == selection.end) {
      lastLine--;
    }
    if (!aLineAbove && (lastLine + 1) == this.getLineCount()) {
      return true;
    }

    let lastLineEnd = this.getLineEnd(lastLine, true);
    let text = this.getText(firstLineStart, lastLineEnd);

    if (aLineAbove) {
      let aboveLine = firstLine - 1;
      let aboveLineStart = this.getLineStart(aboveLine);

      this.startCompoundChange();
      if (lastLine == (this.getLineCount() - 1)) {
        let delimiterStart = this.getLineEnd(aboveLine);
        let delimiterEnd = this.getLineEnd(aboveLine, true);
        let lineDelimiter = this.getText(delimiterStart, delimiterEnd);
        text += lineDelimiter;
        this.setText("", firstLineStart - lineDelimiter.length, lastLineEnd);
      } else {
        this.setText("", firstLineStart, lastLineEnd);
      }
      this.setText(text, aboveLineStart, aboveLineStart);
      this.endCompoundChange();
      this.setSelection(aboveLineStart, aboveLineStart + text.length);
    } else {
      let belowLine = lastLine + 1;
      let belowLineEnd = this.getLineEnd(belowLine, true);

      let insertAt = belowLineEnd - lastLineEnd + firstLineStart;
      let lineDelimiter = "";
      if (belowLine == this.getLineCount() - 1) {
        let delimiterStart = this.getLineEnd(lastLine);
        lineDelimiter = this.getText(delimiterStart, lastLineEnd);
        text = lineDelimiter + text.substr(0, text.length -
                                              lineDelimiter.length);
      }
      this.startCompoundChange();
      this.setText("", firstLineStart, lastLineEnd);
      this.setText(text, insertAt, insertAt);
      this.endCompoundChange();
      this.setSelection(insertAt + lineDelimiter.length,
                        insertAt + text.length);
    }
    return true;
  },

  /**
   * The Orion Selection event handler. The current caret line is
   * highlighted and for Linux users the selected text is copied into the X11
   * PRIMARY buffer.
   *
   * @private
   * @param object aEvent
   *        The Orion Selection event object.
   */
  _onOrionSelection: function SE__onOrionSelection(aEvent)
  {
    if (this._config.highlightCurrentLine) {
      this._highlightCurrentLine(aEvent);
    }

    if (Services.appinfo.OS == "Linux") {
      let window = this.parentElement.ownerDocument.defaultView;

      if (this._primarySelectionTimeout) {
        window.clearTimeout(this._primarySelectionTimeout);
      }
      this._primarySelectionTimeout =
        window.setTimeout(this._updatePrimarySelection.bind(this),
                          PRIMARY_SELECTION_DELAY);
    }
  },

  /**
   * The TextChanged event handler which tracks the dirty state of the editor.
   *
   * @see SourceEditor.EVENTS.TEXT_CHANGED
   * @see SourceEditor.EVENTS.DIRTY_CHANGED
   * @see SourceEditor.dirty
   * @private
   */
  _onTextChanged: function SE__onTextChanged()
  {
    this._updateDirty();
  },

  /**
   * The Orion contextmenu event handler. This method opens the default or
   * the custom context menu popup at the pointer location.
   *
   * @param object aEvent
   *        The contextmenu event object coming from Orion. This object should
   *        hold the screenX and screenY properties.
   */
  _onOrionContextMenu: function SE__onOrionContextMenu(aEvent)
  {
    if (this._contextMenu.state == "closed") {
      this._contextMenu.openPopupAtScreen(aEvent.screenX || 0,
                                          aEvent.screenY || 0, true);
    }
  },

  /**
   * Update the dirty state of the editor based on the undo stack.
   * @private
   */
  _updateDirty: function SE__updateDirty()
  {
    this.dirty = !this._undoStack.isClean();
  },

  /**
   * Update the X11 PRIMARY buffer to hold the current selection.
   * @private
   */
  _updatePrimarySelection: function SE__updatePrimarySelection()
  {
    this._primarySelectionTimeout = null;

    let text = this.getSelectedText();
    if (!text) {
      return;
    }

    clipboardHelper.copyStringToClipboard(text,
                                          Ci.nsIClipboard.kSelectionClipboard);
  },

  /**
   * Highlight the current line using the Orion annotation model.
   *
   * @private
   * @param object aEvent
   *        The Selection event object.
   */
  _highlightCurrentLine: function SE__highlightCurrentLine(aEvent)
  {
    let annotationModel = this._annotationModel;
    let model = this._model;
    let oldAnnotation = this._currentLineAnnotation;
    let newSelection = aEvent.newValue;

    let collapsed = newSelection.start == newSelection.end;
    if (!collapsed) {
      if (oldAnnotation) {
        annotationModel.removeAnnotation(oldAnnotation);
        this._currentLineAnnotation = null;
      }
      return;
    }

    let line = model.getLineAtOffset(newSelection.start);
    let lineStart = this.getLineStart(line);
    let lineEnd = this.getLineEnd(line);

    let title = oldAnnotation ? oldAnnotation.title :
                SourceEditorUI.strings.GetStringFromName("annotation.currentLine");

    this._currentLineAnnotation = {
      start: lineStart,
      end: lineEnd,
      type: ORION_ANNOTATION_TYPES.currentLine,
      title: title,
      html: "<div class='annotationHTML currentLine'></div>",
      overviewStyle: {styleClass: "annotationOverview currentLine"},
      lineStyle: {styleClass: "annotationLine currentLine"},
    };

    annotationModel.replaceAnnotations(oldAnnotation ? [oldAnnotation] : null,
                                       [this._currentLineAnnotation]);
  },

  /**
   * The click event handler for the lines gutter. This function allows the user
   * to jump to a line or to perform line selection while holding the Shift key
   * down.
   *
   * @private
   * @param number aLineIndex
   *        The line index where the click event occurred.
   * @param object aEvent
   *        The DOM click event object.
   */
  _linesRulerClick: function SE__linesRulerClick(aLineIndex, aEvent)
  {
    if (aLineIndex === undefined) {
      return;
    }

    if (aEvent.shiftKey) {
      let model = this._model;
      let selection = this.getSelection();
      let selectionLineStart = model.getLineAtOffset(selection.start);
      let selectionLineEnd = model.getLineAtOffset(selection.end);
      let newStart = aLineIndex <= selectionLineStart ?
                     this.getLineStart(aLineIndex) : selection.start;
      let newEnd = aLineIndex <= selectionLineStart ?
                   selection.end : this.getLineEnd(aLineIndex);
      this.setSelection(newStart, newEnd);
    } else {
      this.setCaretPosition(aLineIndex);
    }
  },

  /**
   * The dblclick event handler for the lines gutter. This function selects the
   * whole line where the event occurred.
   *
   * @private
   * @param number aLineIndex
   *        The line index where the double click event occurred.
   * @param object aEvent
   *        The DOM dblclick event object.
   */
  _linesRulerDblClick: function SE__linesRulerDblClick(aLineIndex)
  {
    if (aLineIndex === undefined) {
      return;
    }

    let newStart = this.getLineStart(aLineIndex);
    let newEnd = this.getLineEnd(aLineIndex);
    this.setSelection(newStart, newEnd);
  },

  /**
   * Highlight the Orion annotations. This updates the annotation styler as
   * needed.
   * @private
   */
  _highlightAnnotations: function SE__highlightAnnotations()
  {
    if (this._annotationStyler) {
      this._annotationStyler.destroy();
      this._annotationStyler = null;
    }

    let AnnotationStyler =
      this._iframeWindow.require("orion/textview/annotations").AnnotationStyler;

    let styler = new AnnotationStyler(this._view, this._annotationModel);
    this._annotationStyler = styler;

    styler.addAnnotationType(ORION_ANNOTATION_TYPES.matchingBracket);
    styler.addAnnotationType(ORION_ANNOTATION_TYPES.currentBracket);
    styler.addAnnotationType(ORION_ANNOTATION_TYPES.task);
    styler.addAnnotationType(ORION_ANNOTATION_TYPES.debugLocation);

    if (this._config.highlightCurrentLine) {
      styler.addAnnotationType(ORION_ANNOTATION_TYPES.currentLine);
    }
  },

  /**
   * Retrieve the list of Orion Annotations filtered by type for the given text range.
   *
   * @private
   * @param string aType
   *        The annotation type to filter annotations for. Use one of the keys
   *        in ORION_ANNOTATION_TYPES.
   * @param number aStart
   *        Offset from where to start finding the annotations.
   * @param number aEnd
   *        End offset for retrieving the annotations.
   * @return array
   *         The array of annotations, filtered by type, within the given text
   *         range.
   */
  _getAnnotationsByType: function SE__getAnnotationsByType(aType, aStart, aEnd)
  {
    let annotations = this._annotationModel.getAnnotations(aStart, aEnd);
    let annotation, result = [];
    while (annotation = annotations.next()) {
      if (annotation.type == ORION_ANNOTATION_TYPES[aType]) {
        result.push(annotation);
      }
    }

    return result;
  },

  /**
   * The click event handler for the annotation ruler.
   *
   * @private
   * @param number aLineIndex
   *        The line index where the click event occurred.
   * @param object aEvent
   *        The DOM click event object.
   */
  _annotationRulerClick: function SE__annotationRulerClick(aLineIndex, aEvent)
  {
    if (aLineIndex === undefined || aLineIndex == -1) {
      return;
    }

    let lineStart = this.getLineStart(aLineIndex);
    let lineEnd = this.getLineEnd(aLineIndex);
    let annotations = this._getAnnotationsByType("breakpoint", lineStart, lineEnd);
    if (annotations.length > 0) {
      this.removeBreakpoint(aLineIndex);
    } else {
      this.addBreakpoint(aLineIndex);
    }
  },

  /**
   * The click event handler for the overview ruler. When the user clicks on an
   * annotation the editor jumps to the associated line.
   *
   * @private
   * @param number aLineIndex
   *        The line index where the click event occurred.
   * @param object aEvent
   *        The DOM click event object.
   */
  _overviewRulerClick: function SE__overviewRulerClick(aLineIndex, aEvent)
  {
    if (aLineIndex === undefined || aLineIndex == -1) {
      return;
    }

    let model = this._model;
    let lineStart = this.getLineStart(aLineIndex);
    let lineEnd = this.getLineEnd(aLineIndex);
    let annotations = this._annotationModel.getAnnotations(lineStart, lineEnd);
    let annotation = annotations.next();

    // Jump to the line where annotation is. If the annotation is specific to
    // a substring part of the line, then select the substring.
    if (!annotation || lineStart == annotation.start && lineEnd == annotation.end) {
      this.setSelection(lineStart, lineStart);
    } else {
      this.setSelection(annotation.start, annotation.end);
    }
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
   * Helper function to retrieve the strings used for comments in the current
   * editor mode.
   *
   * @private
   * @return object
   *         An object that holds the following properties:
   *         - line: the comment string used for the start of a single line
   *         comment.
   *         - blockStart: the comment string used for the start of a comment
   *         block.
   *         - blockEnd: the comment string used for the end of a block comment.
   *         Null is returned for unsupported editor modes.
   */
  _getCommentStrings: function SE__getCommentStrings()
  {
    let line = "";
    let blockCommentStart = "";
    let blockCommentEnd = "";

    switch (this.getMode()) {
      case SourceEditor.MODES.JAVASCRIPT:
        line = "//";
        blockCommentStart = "/*";
        blockCommentEnd = "*/";
        break;
      case SourceEditor.MODES.CSS:
        blockCommentStart = "/*";
        blockCommentEnd = "*/";
        break;
      case SourceEditor.MODES.HTML:
      case SourceEditor.MODES.XML:
        blockCommentStart = "<!--";
        blockCommentEnd = "-->";
        break;
      default:
        return null;
    }
    return {line: line, blockStart: blockCommentStart, blockEnd: blockCommentEnd};
  },

  /**
   * Decide whether to comment the selection/current line or to uncomment it.
   *
   * @private
   */
  _doCommentUncomment: function SE__doCommentUncomment()
  {
    if (this.readOnly) {
      return false;
    }

    let commentObject = this._getCommentStrings();
    if (!commentObject) {
      return false;
    }

    let selection = this.getSelection();
    let model = this._model;
    let firstLine = model.getLineAtOffset(selection.start);
    let lastLine = model.getLineAtOffset(selection.end);

    // Checks for block comment.
    let firstLineText = model.getLine(firstLine);
    let lastLineText = model.getLine(lastLine);
    let openIndex = firstLineText.indexOf(commentObject.blockStart);
    let closeIndex = lastLineText.lastIndexOf(commentObject.blockEnd);
    if (openIndex != -1 && closeIndex != -1 &&
        (firstLine != lastLine ||
        (closeIndex - openIndex) >= commentObject.blockStart.length)) {
      return this._doUncomment();
    }

    if (!commentObject.line) {
      return this._doComment();
    }

    // If the selection is not a block comment, check for the first and the last
    // lines to be line commented.
    let firstLastCommented = [firstLineText,
                              lastLineText].every(function(aLineText) {
      let openIndex = aLineText.indexOf(commentObject.line);
      if (openIndex != -1) {
        let textUntilComment = aLineText.slice(0, openIndex);
        if (!textUntilComment || /^\s+$/.test(textUntilComment)) {
          return true;
        }
      }
      return false;
    });
    if (firstLastCommented) {
      return this._doUncomment();
    }

    // If we reach here, then we have to comment the selection/line.
    return this._doComment();
  },

  /**
   * Wrap the selected text in comments. If nothing is selected the current
   * caret line is commented out. Single line and block comments depend on the
   * current editor mode.
   *
   * @private
   */
  _doComment: function SE__doComment()
  {
    if (this.readOnly) {
      return false;
    }

    let commentObject = this._getCommentStrings();
    if (!commentObject) {
      return false;
    }

    let selection = this.getSelection();

    if (selection.start == selection.end) {
      let selectionLine = this._model.getLineAtOffset(selection.start);
      let lineStartOffset = this.getLineStart(selectionLine);
      if (commentObject.line) {
        this.setText(commentObject.line, lineStartOffset, lineStartOffset);
      } else {
        let lineEndOffset = this.getLineEnd(selectionLine);
        this.startCompoundChange();
        this.setText(commentObject.blockStart, lineStartOffset, lineStartOffset);
        this.setText(commentObject.blockEnd,
                     lineEndOffset + commentObject.blockStart.length,
                     lineEndOffset + commentObject.blockStart.length);
        this.endCompoundChange();
      }
    } else {
      this.startCompoundChange();
      this.setText(commentObject.blockStart, selection.start, selection.start);
      this.setText(commentObject.blockEnd,
                   selection.end + commentObject.blockStart.length,
                   selection.end + commentObject.blockStart.length);
      this.endCompoundChange();
    }

    return true;
  },

  /**
   * Uncomment the selected text. If nothing is selected the current caret line
   * is umcommented. Single line and block comments depend on the current editor
   * mode.
   *
   * @private
   */
  _doUncomment: function SE__doUncomment()
  {
    if (this.readOnly) {
      return false;
    }

    let commentObject = this._getCommentStrings();
    if (!commentObject) {
      return false;
    }

    let selection = this.getSelection();
    let firstLine = this._model.getLineAtOffset(selection.start);
    let lastLine = this._model.getLineAtOffset(selection.end);

    // Uncomment a block of text.
    let firstLineText = this._model.getLine(firstLine);
    let lastLineText = this._model.getLine(lastLine);
    let openIndex = firstLineText.indexOf(commentObject.blockStart);
    let closeIndex = lastLineText.lastIndexOf(commentObject.blockEnd);
    if (openIndex != -1 && closeIndex != -1 &&
        (firstLine != lastLine ||
        (closeIndex - openIndex) >= commentObject.blockStart.length)) {
      let firstLineStartOffset = this.getLineStart(firstLine);
      let lastLineStartOffset = this.getLineStart(lastLine);
      let openOffset = firstLineStartOffset + openIndex;
      let closeOffset = lastLineStartOffset + closeIndex;

      this.startCompoundChange();
      this.setText("", closeOffset, closeOffset + commentObject.blockEnd.length);
      this.setText("", openOffset, openOffset + commentObject.blockStart.length);
      this.endCompoundChange();

      return true;
    }

    if (!commentObject.line) {
      return true;
    }

    // If the selected text is not a block of comment, then uncomment each line.
    this.startCompoundChange();
    let lineCaret = firstLine;
    while (lineCaret <= lastLine) {
      let currentLine = this._model.getLine(lineCaret);
      let lineStart = this.getLineStart(lineCaret);
      let openIndex = currentLine.indexOf(commentObject.line);
      let openOffset = lineStart + openIndex;
      let textUntilComment = this.getText(lineStart, openOffset);
      if (openIndex != -1 &&
          (!textUntilComment || /^\s+$/.test(textUntilComment))) {
        this.setText("", openOffset, openOffset + commentObject.line.length);
      }
      lineCaret++;
    }
    this.endCompoundChange();

    return true;
  },

  /**
   * Helper function for _moveToBracket{Opening/Closing} to find the offset of
   * matching bracket.
   *
   * @param number aOffset
   *        The offset of the bracket for which you want to find the bracket.
   * @private
   */
  _getMatchingBracketIndex: function SE__getMatchingBracketIndex(aOffset)
  {
    return this._styler._findMatchingBracket(this._model, aOffset);
  },

  /**
   * Move the cursor to the matching opening bracket if at corresponding closing
   * bracket, otherwise move to the opening bracket for the current block of code.
   *
   * @private
   */
  _moveToBracketOpening: function SE__moveToBracketOpening()
  {
    let mode = this.getMode();
    // Returning early if not in JavaScipt or CSS mode.
    if (mode != SourceEditor.MODES.JAVASCRIPT &&
        mode != SourceEditor.MODES.CSS) {
      return false;
    }

    let caretOffset = this.getCaretOffset() - 1;
    let matchingIndex = this._getMatchingBracketIndex(caretOffset);

    // If the caret is not at the closing bracket "}", find the index of the
    // opening bracket "{" for the current code block.
    if (matchingIndex == -1 || matchingIndex > caretOffset) {
      let text = this.getText();
      let closingOffset = text.indexOf("}", caretOffset);
      while (closingOffset > -1) {
        let closingMatchingIndex = this._getMatchingBracketIndex(closingOffset);
        if (closingMatchingIndex < caretOffset && closingMatchingIndex != -1) {
          matchingIndex = closingMatchingIndex;
          break;
        }
        closingOffset = text.indexOf("}", closingOffset + 1);
      }
    }

    if (matchingIndex > -1) {
      this.setCaretOffset(matchingIndex);
    }

    return true;
  },

  /**
   * Moves the cursor to the matching closing bracket if at corresponding opening
   * bracket, otherwise move to the closing bracket for the current block of code.
   *
   * @private
   */
  _moveToBracketClosing: function SE__moveToBracketClosing()
  {
    let mode = this.getMode();
    // Returning early if not in JavaScipt or CSS mode.
    if (mode != SourceEditor.MODES.JAVASCRIPT &&
        mode != SourceEditor.MODES.CSS) {
      return false;
    }

    let caretOffset = this.getCaretOffset();
    let matchingIndex = this._getMatchingBracketIndex(caretOffset - 1);

    // If the caret is not at the opening bracket "{", find the index of the
    // closing bracket "}" for the current code block.
    if (matchingIndex == -1 || matchingIndex < caretOffset) {
      let text = this.getText();
      let openingOffset = text.lastIndexOf("{", caretOffset);
      while (openingOffset > -1) {
        let openingMatchingIndex = this._getMatchingBracketIndex(openingOffset);
        if (openingMatchingIndex > caretOffset) {
          matchingIndex = openingMatchingIndex;
          break;
        }
        openingOffset = text.lastIndexOf("{", openingOffset - 1);
      }
    }

    if (matchingIndex > -1) {
      this.setCaretOffset(matchingIndex);
    }

    return true;
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
  addEventListener: function SE_addEventListener(aEventType, aCallback)
  {
    if (this._view && aEventType in ORION_EVENTS) {
      this._view.addEventListener(ORION_EVENTS[aEventType], aCallback);
    } else if (this._eventTarget.addEventListener) {
      this._eventTarget.addEventListener(aEventType, aCallback);
    } else {
      this._eventListenersQueue.push(["add", aEventType, aCallback]);
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
  removeEventListener: function SE_removeEventListener(aEventType, aCallback)
  {
    if (this._view && aEventType in ORION_EVENTS) {
      this._view.removeEventListener(ORION_EVENTS[aEventType], aCallback);
    } else if (this._eventTarget.removeEventListener) {
      this._eventTarget.removeEventListener(aEventType, aCallback);
    } else {
      this._eventListenersQueue.push(["remove", aEventType, aCallback]);
    }
  },

  /**
   * Undo a change in the editor.
   *
   * @return boolean
   *         True if there was a change undone, false otherwise.
   */
  undo: function SE_undo()
  {
    let result = this._undoStack.undo();
    this.ui._onUndoRedo();
    return result;
  },

  /**
   * Redo a change in the editor.
   *
   * @return boolean
   *         True if there was a change redone, false otherwise.
   */
  redo: function SE_redo()
  {
    let result = this._undoStack.redo();
    this.ui._onUndoRedo();
    return result;
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
   * Reset the Undo stack.
   */
  resetUndo: function SE_resetUndo()
  {
    this._undoStack.reset();
    this._updateDirty();
    this.ui._onUndoRedo();
  },

  /**
   * Set the "dirty" state of the editor. Set this to false when you save the
   * text being edited. The dirty state will become true once the user makes
   * changes to the text.
   *
   * @param boolean aNewValue
   *        The new dirty state: true if the text is not saved, false if you
   *        just saved the text.
   */
  set dirty(aNewValue)
  {
    if (aNewValue == this._dirty) {
      return;
    }

    let event = {
      type: SourceEditor.EVENTS.DIRTY_CHANGED,
      oldValue: this._dirty,
      newValue: aNewValue,
    };

    this._dirty = aNewValue;
    if (!this._dirty && !this._undoStack.isClean()) {
      this._undoStack.markClean();
    }
    this._dispatchEvent(event);
  },

  /**
   * Get the editor "dirty" state. This tells if the text is considered saved or
   * not.
   *
   * @see SourceEditor.EVENTS.DIRTY_CHANGED
   * @return boolean
   *         True if there are changes which are not saved, false otherwise.
   */
  get dirty()
  {
    return this._dirty;
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
   * Get the start character offset of the line with index aLineIndex.
   *
   * @param number aLineIndex
   *        Zero based index of the line.
   * @return number
   *        Line start offset or -1 if out of range.
   */
  getLineStart: function SE_getLineStart(aLineIndex)
  {
    return this._model.getLineStart(aLineIndex);
  },

  /**
   * Get the end character offset of the line with index aLineIndex,
   * excluding the end offset. When the line delimiter is present,
   * the offset is the start offset of the next line or the char count.
   * Otherwise, it is the offset of the line delimiter.
   *
   * @param number aLineIndex
   *        Zero based index of the line.
   * @param boolean [aIncludeDelimiter = false]
   *        Optional, whether or not to include the line delimiter.
   * @return number
   *        Line end offset or -1 if out of range.
   */
  getLineEnd: function SE_getLineEnd(aLineIndex, aIncludeDelimiter)
  {
    return this._model.getLineEnd(aLineIndex, aIncludeDelimiter);
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
    let lineStart = this.getLineStart(line);
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
   * @param number [aAlign=0]
   *        Optional. Position of the line with respect to viewport.
   *        Allowed values are:
   *          SourceEditor.VERTICAL_ALIGN.TOP     target line at top of view.
   *          SourceEditor.VERTICAL_ALIGN.CENTER  target line at center of view.
   *          SourceEditor.VERTICAL_ALIGN.BOTTOM  target line at bottom of view.
   */
  setCaretPosition: function SE_setCaretPosition(aLine, aColumn, aAlign)
  {
    let editorHeight = this._view.getClientArea().height;
    let lineHeight = this._view.getLineHeight();
    let linesVisible = Math.floor(editorHeight/lineHeight);
    let halfVisible = Math.round(linesVisible/2);
    let firstVisible = this.getTopIndex();
    let lastVisible = this._view.getBottomIndex();
    let caretOffset = this.getLineStart(aLine) + (aColumn || 0);

    this._view.setSelection(caretOffset, caretOffset, false);

    // If the target line is in view, skip the vertical alignment part.
    if (aLine <= lastVisible && aLine >= firstVisible) {
      this._view.showSelection();
      return;
    }

    // Setting the offset so that the line always falls in the upper half
    // of visible lines (lower half for BOTTOM aligned).
    // VERTICAL_OFFSET is the maximum allowed value.
    let offset = Math.min(halfVisible, VERTICAL_OFFSET);

    let topIndex;
    switch (aAlign) {
      case this.VERTICAL_ALIGN.CENTER:
        topIndex = Math.max(aLine - halfVisible, 0);
        break;

      case this.VERTICAL_ALIGN.BOTTOM:
        topIndex = Math.max(aLine - linesVisible + offset, 0);
        break;

      default: // this.VERTICAL_ALIGN.TOP.
        topIndex = Math.max(aLine - offset, 0);
        break;
    }
    // Bringing down the topIndex to total lines in the editor if exceeding.
    topIndex = Math.min(topIndex, this.getLineCount());
    this.setTopIndex(topIndex);

    let location = this._view.getLocationAtOffset(caretOffset);
    this._view.setHorizontalPixel(location.x);
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
   * Get the indentation string used in the document being edited.
   *
   * @return string
   *         The indentation string.
   */
  getIndentationString: function SE_getIndentationString()
  {
    if (this._expandTab) {
      return (new Array(this._tabSize + 1)).join(" ");
    }
    return "\t";
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

    let window = this._iframeWindow;

    switch (aMode) {
      case SourceEditor.MODES.JAVASCRIPT:
      case SourceEditor.MODES.CSS:
        let TextStyler =
          window.require("examples/textview/textStyler").TextStyler;

        this._styler = new TextStyler(this._view, aMode, this._annotationModel);
        this._styler.setFoldingEnabled(false);
        break;

      case SourceEditor.MODES.HTML:
      case SourceEditor.MODES.XML:
        let TextMateStyler =
          window.require("orion/editor/textMateStyler").TextMateStyler;
        let HtmlGrammar =
          window.require("orion/editor/htmlGrammar").HtmlGrammar;
        this._styler = new TextMateStyler(this._view, new HtmlGrammar());
        break;
    }

    this._highlightAnnotations();
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
   * Set the current debugger location at the given line index. This is useful in
   * a debugger or in any other context where the user needs to track the
   * current state, where a debugger-like environment is at.
   *
   * @param number aLineIndex
   *        Line index of the current debugger location, starting from 0.
   *        Use any negative number to clear the current location.
   */
  setDebugLocation: function SE_setDebugLocation(aLineIndex)
  {
    let annotations = this._getAnnotationsByType("debugLocation", 0,
                                                 this.getCharCount());
    if (annotations.length > 0) {
      annotations.forEach(this._annotationModel.removeAnnotation,
                          this._annotationModel);
    }
    if (aLineIndex < 0) {
      return;
    }

    let lineStart = this._model.getLineStart(aLineIndex);
    let lineEnd = this._model.getLineEnd(aLineIndex);
    let lineText = this._model.getLine(aLineIndex);
    let title = SourceEditorUI.strings.
                formatStringFromName("annotation.debugLocation.title",
                                     [lineText], 1);

    let annotation = {
      type: ORION_ANNOTATION_TYPES.debugLocation,
      start: lineStart,
      end: lineEnd,
      title: title,
      style: {styleClass: "annotation debugLocation"},
      html: "<div class='annotationHTML debugLocation'></div>",
      overviewStyle: {styleClass: "annotationOverview debugLocation"},
      rangeStyle: {styleClass: "annotationRange debugLocation"},
      lineStyle: {styleClass: "annotationLine debugLocation"},
    };
    this._annotationModel.addAnnotation(annotation);
  },

  /**
   * Retrieve the current debugger line index configured for this editor.
   *
   * @return number
   *         The line index starting from 0 where the current debugger is
   *         paused. If no debugger location has been set -1 is returned.
   */
  getDebugLocation: function SE_getDebugLocation()
  {
    let annotations = this._getAnnotationsByType("debugLocation", 0,
                                                 this.getCharCount());
    if (annotations.length > 0) {
      return this._model.getLineAtOffset(annotations[0].start);
    }
    return -1;
  },

  /**
   * Add a breakpoint at the given line index.
   *
   * @param number aLineIndex
   *        Line index where to add the breakpoint (starts from 0).
   * @param string [aCondition]
   *        Optional breakpoint condition.
   */
  addBreakpoint: function SE_addBreakpoint(aLineIndex, aCondition)
  {
    let lineStart = this.getLineStart(aLineIndex);
    let lineEnd = this.getLineEnd(aLineIndex);

    let annotations = this._getAnnotationsByType("breakpoint", lineStart, lineEnd);
    if (annotations.length > 0) {
      return;
    }

    let lineText = this._model.getLine(aLineIndex);
    let title = SourceEditorUI.strings.
                formatStringFromName("annotation.breakpoint.title",
                                     [lineText], 1);

    let annotation = {
      type: ORION_ANNOTATION_TYPES.breakpoint,
      start: lineStart,
      end: lineEnd,
      breakpointCondition: aCondition,
      title: title,
      style: {styleClass: "annotation breakpoint"},
      html: "<div class='annotationHTML breakpoint'></div>",
      overviewStyle: {styleClass: "annotationOverview breakpoint"},
      rangeStyle: {styleClass: "annotationRange breakpoint"}
    };
    this._annotationModel.addAnnotation(annotation);

    let event = {
      type: SourceEditor.EVENTS.BREAKPOINT_CHANGE,
      added: [{line: aLineIndex, condition: aCondition}],
      removed: [],
    };

    this._dispatchEvent(event);
  },

  /**
   * Remove the current breakpoint from the given line index.
   *
   * @param number aLineIndex
   *        Line index from where to remove the breakpoint (starts from 0).
   * @return boolean
   *         True if a breakpoint was removed, false otherwise.
   */
  removeBreakpoint: function SE_removeBreakpoint(aLineIndex)
  {
    let lineStart = this.getLineStart(aLineIndex);
    let lineEnd = this.getLineEnd(aLineIndex);

    let event = {
      type: SourceEditor.EVENTS.BREAKPOINT_CHANGE,
      added: [],
      removed: [],
    };

    let annotations = this._getAnnotationsByType("breakpoint", lineStart, lineEnd);

    annotations.forEach(function(annotation) {
      this._annotationModel.removeAnnotation(annotation);
      event.removed.push({line: aLineIndex,
                          condition: annotation.breakpointCondition});
    }, this);

    if (event.removed.length > 0) {
      this._dispatchEvent(event);
    }

    return event.removed.length > 0;
  },

  /**
   * Get the list of breakpoints in the Source Editor instance.
   *
   * @return array
   *         The array of breakpoints. Each item is an object with two
   *         properties: line and condition.
   */
  getBreakpoints: function SE_getBreakpoints()
  {
    let annotations = this._getAnnotationsByType("breakpoint", 0,
                                                 this.getCharCount());
    let breakpoints = [];

    annotations.forEach(function(annotation) {
      breakpoints.push({line: this._model.getLineAtOffset(annotation.start),
                        condition: annotation.breakpointCondition});
    }, this);

    return breakpoints;
  },

  /**
   * Destroy/uninitialize the editor.
   */
  destroy: function SE_destroy()
  {
    if (this._config.highlightCurrentLine || Services.appinfo.OS == "Linux") {
      this.removeEventListener(SourceEditor.EVENTS.SELECTION,
                               this._onOrionSelection);
    }
    this._onOrionSelection = null;

    this.removeEventListener(SourceEditor.EVENTS.TEXT_CHANGED,
                             this._onTextChanged);
    this._onTextChanged = null;

    if (this._contextMenu) {
      this.removeEventListener(SourceEditor.EVENTS.CONTEXT_MENU,
                               this._onOrionContextMenu);
      this._contextMenu = null;
    }
    this._onOrionContextMenu = null;

    if (this._primarySelectionTimeout) {
      let window = this.parentElement.ownerDocument.defaultView;
      window.clearTimeout(this._primarySelectionTimeout);
      this._primarySelectionTimeout = null;
    }

    this._view.destroy();
    this.ui.destroy();
    this.ui = null;

    this.parentElement.removeChild(this._iframe);
    this.parentElement = null;
    this._iframeWindow = null;
    this._iframe = null;
    this._undoStack = null;
    this._styler = null;
    this._linesRuler = null;
    this._annotationRuler = null;
    this._overviewRuler = null;
    this._dragAndDrop = null;
    this._annotationModel = null;
    this._annotationStyler = null;
    this._currentLineAnnotation = null;
    this._eventTarget = null;
    this._eventListenersQueue = null;
    this._view = null;
    this._model = null;
    this._config = null;
    this._lastFind = null;
  },
};
