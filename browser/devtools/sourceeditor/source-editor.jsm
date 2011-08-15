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

const PREF_EDITOR_COMPONENT = "devtools.editor.component";

var component = Services.prefs.getCharPref(PREF_EDITOR_COMPONENT);
var obj = {};
try {
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
  TEXTMATE: "textmate",
};

/**
 * Source editor configuration defaults.
 */
SourceEditor.DEFAULTS = {
  MODE: SourceEditor.MODES.TEXT,
  THEME: SourceEditor.THEMES.TEXTMATE,
  UNDO_LIMIT: 200,
  TAB_SIZE: 4, // overriden by pref
  EXPAND_TAB: true, // overriden by pref
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
};

