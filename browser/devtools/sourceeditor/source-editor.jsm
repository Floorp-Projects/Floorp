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
 * The Original Code is the Source Editor component.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mihai Sucan <mihai.sucan@gmail.com> (original author)
 *   Spyros Livathinos <livathinos.spyros@gmail.com>
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

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource:///modules/source-editor-ui.jsm");

const PREF_EDITOR_COMPONENT = "devtools.editor.component";
const SOURCEEDITOR_L10N = "chrome://browser/locale/devtools/sourceeditor.properties";

var component = Services.prefs.getCharPref(PREF_EDITOR_COMPONENT);
var obj = {};
try {
  if (component == "ui") {
    throw new Error("The ui editor component is not available.");
  }
  Cu.import("resource:///modules/source-editor-" + component + ".jsm", obj);
} catch (ex) {
  Cu.reportError(ex);
  Cu.reportError("SourceEditor component failed to load: " + component);

  // If the component does not exist, clear the user pref back to the default.
  Services.prefs.clearUserPref(PREF_EDITOR_COMPONENT);

  // Load the default editor component.
  component = Services.prefs.getCharPref(PREF_EDITOR_COMPONENT);
  Cu.import("resource:///modules/source-editor-" + component + ".jsm", obj);
}

// Export the SourceEditor.
var SourceEditor = obj.SourceEditor;
var EXPORTED_SYMBOLS = ["SourceEditor"];

// Add the constants used by all SourceEditors.

XPCOMUtils.defineLazyGetter(SourceEditorUI, "strings", function() {
  return Services.strings.createBundle(SOURCEEDITOR_L10N);
});

/**
 * Known SourceEditor preferences.
 */
SourceEditor.PREFS = {
  TAB_SIZE: "devtools.editor.tabsize",
  EXPAND_TAB: "devtools.editor.expandtab",
  COMPONENT: PREF_EDITOR_COMPONENT,
};

/**
 * Predefined source editor modes for JavaScript, CSS and other languages.
 */
SourceEditor.MODES = {
  JAVASCRIPT: "js",
  CSS: "css",
  TEXT: "text",
  HTML: "html",
  XML: "xml",
};

/**
 * Predefined themes for syntax highlighting.
 */
SourceEditor.THEMES = {
  MOZILLA: "mozilla",
};

/**
 * Source editor configuration defaults.
 * @see SourceEditor.init
 */
SourceEditor.DEFAULTS = {
  /**
   * The text you want shown when the editor opens up.
   * @type string
   */
  initialText: "",

  /**
   * The editor mode, based on the file type you want to edit. You can use one of
   * the predefined modes.
   *
   * @see SourceEditor.MODES
   * @type string
   */
  mode: SourceEditor.MODES.TEXT,

  /**
   * The syntax highlighting theme you want. You can use one of the predefined
   * themes, or you can point to your CSS file.
   *
   * @see SourceEditor.THEMES.
   * @type string
   */
  theme: SourceEditor.THEMES.MOZILLA,

  /**
   * How many steps should the undo stack hold.
   * @type number
   */
  undoLimit: 200,

  /**
   * Define how many spaces to use for a tab character. This value is overridden
   * by a user preference, see SourceEditor.PREFS.TAB_SIZE.
   *
   * @type number
   */
  tabSize: 4,

  /**
   * Tells if you want tab characters to be expanded to spaces. This value is
   * overridden by a user preference, see SourceEditor.PREFS.EXPAND_TAB.
   * @type boolean
   */
  expandTab: true,

  /**
   * Tells if you want the editor to be read only or not.
   * @type boolean
   */
  readOnly: false,

  /**
   * Display the line numbers gutter.
   * @type boolean
   */
  showLineNumbers: false,

  /**
   * Display the annotations gutter/ruler. This gutter currently supports
   * annotations of breakpoint type.
   * @type boolean
   */
  showAnnotationRuler: false,

  /**
   * Display the overview gutter/ruler. This gutter presents an overview of the
   * current annotations in the editor, for example the breakpoints.
   * @type boolean
   */
  showOverviewRuler: false,

  /**
   * Highlight the current line.
   * @type boolean
   */
  highlightCurrentLine: true,

  /**
   * An array of objects that allows you to define custom editor keyboard
   * bindings. Each object can have:
   *   - action - name of the editor action to invoke.
   *   - code - keyCode for the shortcut.
   *   - accel - boolean for the Accel key (Cmd on Macs, Ctrl on Linux/Windows).
   *   - ctrl - boolean for the Control key
   *   - shift - boolean for the Shift key.
   *   - alt - boolean for the Alt key.
   *   - callback - optional function to invoke, if the action is not predefined
   *   in the editor.
   * @type array
   */
  keys: null,

  /**
   * The editor context menu you want to display when the user right-clicks
   * within the editor. This property can be:
   *   - a string that tells the ID of the xul:menupopup you want. This needs to
   *   be available within the editor parentElement.ownerDocument.
   *   - an nsIDOMElement object reference pointing to the xul:menupopup you
   *   want to open when the contextmenu event is fired.
   *
   * Set this property to a falsey value to disable the default context menu.
   *
   * @see SourceEditor.EVENTS.CONTEXT_MENU for more control over the contextmenu
   * event.
   * @type string|nsIDOMElement
   */
  contextMenu: "sourceEditorContextMenu",
};

/**
 * Known editor events you can listen for.
 */
SourceEditor.EVENTS = {
  /**
   * The contextmenu event is fired when the editor context menu is invoked. The
   * event object properties:
   *   - x - the pointer location on the x axis, relative to the document the
   *   user is editing.
   *   - y - the pointer location on the y axis, relative to the document the
   *   user is editing.
   *   - screenX - the pointer location on the x axis, relative to the screen.
   *   This value comes from the DOM contextmenu event.screenX property.
   *   - screenY - the pointer location on the y axis, relative to the screen.
   *   This value comes from the DOM contextmenu event.screenY property.
   *
   * @see SourceEditor.DEFAULTS.contextMenu
   */
  CONTEXT_MENU: "ContextMenu",

  /**
   * The TextChanged event is fired when the editor content changes. The event
   * object properties:
   *   - start - the character offset in the document where the change has
   *   occured.
   *   - removedCharCount - the number of characters removed from the document.
   *   - addedCharCount - the number of characters added to the document.
   */
  TEXT_CHANGED: "TextChanged",

  /**
   * The Selection event is fired when the editor selection changes. The event
   * object properties:
   *   - oldValue - the old selection range.
   *   - newValue - the new selection range.
   * Both ranges are objects which hold two properties: start and end.
   */
  SELECTION: "Selection",

  /**
   * The focus event is fired when the editor is focused.
   */
  FOCUS: "Focus",

  /**
   * The blur event is fired when the editor goes out of focus.
   */
  BLUR: "Blur",

  /**
   * The MouseMove event is sent when the user moves the mouse over a line
   * annotation. The event object properties:
   *   - event - the DOM mousemove event object.
   *   - x and y - the mouse coordinates relative to the document being edited.
   */
  MOUSE_MOVE: "MouseMove",

  /**
   * The MouseOver event is sent when the mouse pointer enters a line
   * annotation. The event object properties:
   *   - event - the DOM mouseover event object.
   *   - x and y - the mouse coordinates relative to the document being edited.
   */
  MOUSE_OVER: "MouseOver",

  /**
   * This MouseOut event is sent when the mouse pointer exits a line
   * annotation. The event object properties:
   *   - event - the DOM mouseout event object.
   *   - x and y - the mouse coordinates relative to the document being edited.
   */
  MOUSE_OUT: "MouseOut",

  /**
   * The BreakpointChange event is fired when a new breakpoint is added or when
   * a breakpoint is removed - either through API use or through the editor UI.
   * Event object properties:
   *   - added - array that holds the new breakpoints.
   *   - removed - array that holds the breakpoints that have been removed.
   * Each object in the added/removed arrays holds two properties: line and
   * condition.
   */
  BREAKPOINT_CHANGE: "BreakpointChange",

  /**
   * The DirtyChanged event is fired when the dirty state of the editor is
   * changed. The dirty state of the editor tells if the are text changes that
   * have not been saved yet. Event object properties: oldValue and newValue.
   * Both are booleans telling the old dirty state and the new state,
   * respectively.
   */
  DIRTY_CHANGED: "DirtyChanged",
};

/**
 * Extend a destination object with properties from a source object.
 *
 * @param object aDestination
 * @param object aSource
 */
function extend(aDestination, aSource)
{
  for (let name in aSource) {
    if (!aDestination.hasOwnProperty(name)) {
      aDestination[name] = aSource[name];
    }
  }
}

/**
 * Add methods common to all components.
 */
extend(SourceEditor.prototype, {
  // Expose the static constants on the SourceEditor instances.
  EVENTS: SourceEditor.EVENTS,
  MODES: SourceEditor.MODES,
  THEMES: SourceEditor.THEMES,
  DEFAULTS: SourceEditor.DEFAULTS,

  _lastFind: null,

  /**
   * Find a string in the editor.
   *
   * @param string aString
   *        The string you want to search for. If |aString| is not given the
   *        currently selected text is used.
   * @param object [aOptions]
   *        Optional find options:
   *        - start: (integer) offset to start searching from. Default: 0 if
   *        backwards is false. If backwards is true then start = text.length.
   *        - ignoreCase: (boolean) tells if you want the search to be case
   *        insensitive or not. Default: false.
   *        - backwards: (boolean) tells if you want the search to go backwards
   *        from the given |start| offset. Default: false.
   * @return integer
   *        The offset where the string was found.
   */
  find: function SE_find(aString, aOptions)
  {
    if (typeof(aString) != "string") {
      return -1;
    }

    aOptions = aOptions || {};

    let str = aOptions.ignoreCase ? aString.toLowerCase() : aString;

    let text = this.getText();
    if (aOptions.ignoreCase) {
      text = text.toLowerCase();
    }

    let index = aOptions.backwards ?
                text.lastIndexOf(str, aOptions.start) :
                text.indexOf(str, aOptions.start);

    let lastFoundIndex = index;
    if (index == -1 && this.lastFind && this.lastFind.index > -1 &&
        this.lastFind.str === aString &&
        this.lastFind.ignoreCase === !!aOptions.ignoreCase) {
      lastFoundIndex = this.lastFind.index;
    }

    this._lastFind = {
      str: aString,
      index: index,
      lastFound: lastFoundIndex,
      ignoreCase: !!aOptions.ignoreCase,
    };

    return index;
  },

  /**
   * Find the next occurrence of the last search operation.
   *
   * @param boolean aWrap
   *        Tells if you want to restart the search from the beginning of the
   *        document if the string is not found.
   * @return integer
   *        The offset where the string was found.
   */
  findNext: function SE_findNext(aWrap)
  {
    if (!this.lastFind && this.lastFind.lastFound == -1) {
      return -1;
    }

    let options = {
      start: this.lastFind.lastFound + this.lastFind.str.length,
      ignoreCase: this.lastFind.ignoreCase,
    };

    let index = this.find(this.lastFind.str, options);
    if (index == -1 && aWrap) {
      options.start = 0;
      index = this.find(this.lastFind.str, options);
    }

    return index;
  },

  /**
   * Find the previous occurrence of the last search operation.
   *
   * @param boolean aWrap
   *        Tells if you want to restart the search from the end of the
   *        document if the string is not found.
   * @return integer
   *        The offset where the string was found.
   */
  findPrevious: function SE_findPrevious(aWrap)
  {
    if (!this.lastFind && this.lastFind.lastFound == -1) {
      return -1;
    }

    let options = {
      start: this.lastFind.lastFound - this.lastFind.str.length,
      ignoreCase: this.lastFind.ignoreCase,
      backwards: true,
    };

    let index;
    if (options.start > 0) {
      index = this.find(this.lastFind.str, options);
    } else {
      index = this._lastFind.index = -1;
    }

    if (index == -1 && aWrap) {
      options.start = this.getCharCount() - 1;
      index = this.find(this.lastFind.str, options);
    }

    return index;
  },
});

/**
 * Retrieve the last find operation result. This object holds the following
 * properties:
 *   - str: the last search string.
 *   - index: stores the result of the most recent find operation. This is the
 *   index in the text where |str| was found or -1 otherwise.
 *   - lastFound: tracks the index where |str| was last found, throughout
 *   multiple find operations. This can be -1 if |str| was never found in the
 *   document.
 *   - ignoreCase: tells if the search was case insensitive or not.
 * @type object
 */
Object.defineProperty(SourceEditor.prototype, "lastFind", {
  get: function() { return this._lastFind; },
  enumerable: true,
  configurable: true,
});

