/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Cu } = require("chrome");
const { Class } = require("sdk/core/heritage");
const { EventTarget } = require("sdk/event/target");
const { emit } = require("sdk/event/core");
const promise = require("promise");
const Editor = require("devtools/client/sourceeditor/editor");
const HTML_NS = "http://www.w3.org/1999/xhtml";
const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

/**
 * ItchEditor is extended to implement an editor, which is the main view
 * that shows up when a file is selected.  This object should not be used
 * directly - use TextEditor for a basic code editor.
 */
var ItchEditor = Class({
  extends: EventTarget,

  /**
   * A boolean specifying if the toolbar above the editor should be hidden.
   */
  hidesToolbar: false,

  /**
   * A boolean specifying whether the editor can be edited / saved.
   * For instance, a 'save' doesn't make sense on an image.
   */
  isEditable: false,

  toString: function () {
    return this.label || "";
  },

  emit: function (name, ...args) {
    emit(this, name, ...args);
  },

  /* Does the editor not have any unsaved changes? */
  isClean: function () {
    return true;
  },

  /**
   * Initialize the editor with a single host.  This should be called
   * by objects extending this object with:
   * ItchEditor.prototype.initialize.apply(this, arguments)
   */
  initialize: function (host) {
    this.host = host;
    this.doc = host.document;
    this.label = "";
    this.elt = this.doc.createElement("vbox");
    this.elt.setAttribute("flex", "1");
    this.elt.editor = this;
    this.toolbar = this.doc.querySelector("#projecteditor-toolbar");
    this.projectEditorKeyset = host.projectEditorKeyset;
    this.projectEditorCommandset = host.projectEditorCommandset;
  },

  /**
   * Sets the visibility of the element that shows up above the editor
   * based on the this.hidesToolbar property.
   */
  setToolbarVisibility: function () {
    if (this.hidesToolbar) {
      this.toolbar.setAttribute("hidden", "true");
    } else {
      this.toolbar.removeAttribute("hidden");
    }
  },


  /**
   * Load a single resource into the editor.
   *
   * @param Resource resource
   *        The single file / item that is being dealt with (see stores/base)
   * @returns Promise
   *          A promise that is resolved once the editor has loaded the contents
   *          of the resource.
   */
  load: function (resource) {
    return promise.resolve();
  },

  /**
   * Clean up the editor.  This can have different meanings
   * depending on the type of editor.
   */
  destroy: function () {

  },

  /**
   * Give focus to the editor.  This can have different meanings
   * depending on the type of editor.
   *
   * @returns Promise
   *          A promise that is resolved once the editor has been focused.
   */
  focus: function () {
    return promise.resolve();
  }
});
exports.ItchEditor = ItchEditor;

/**
 * The main implementation of the ItchEditor class.  The TextEditor is used
 * when editing any sort of plain text file, and can be created with different
 * modes for syntax highlighting depending on the language.
 */
var TextEditor = Class({
  extends: ItchEditor,

  isEditable: true,

  /**
   * Extra keyboard shortcuts to use with the editor.  Shortcuts defined
   * within projecteditor should be triggered when they happen in the editor, and
   * they would usually be swallowed without registering them.
   * See "devtools/sourceeditor/editor" for more information.
   */
  get extraKeys() {
    let extraKeys = {};

    // Copy all of the registered keys into extraKeys object, to notify CodeMirror
    // that it should be ignoring these keys
    [...this.projectEditorKeyset.querySelectorAll("key")].forEach((key) => {
      let keyUpper = key.getAttribute("key").toUpperCase();
      let toolModifiers = key.getAttribute("modifiers");
      let modifiers = {
        alt: toolModifiers.includes("alt"),
        shift: toolModifiers.includes("shift")
      };

      // On the key press, we will dispatch the event within projecteditor.
      extraKeys[Editor.accel(keyUpper, modifiers)] = () => {
        let doc = this.projectEditorCommandset.ownerDocument;
        let event = doc.createEvent("Event");
        event.initEvent("command", true, true);
        let command = this.projectEditorCommandset.querySelector("#" + key.getAttribute("command"));
        command.dispatchEvent(event);
      };
    });

    return extraKeys;
  },

  isClean: function () {
    if (!this.editor.isAppended()) {
      return true;
    }
    return this.editor.getText() === this._savedResourceContents;
  },

  initialize: function (document, mode = Editor.modes.text) {
    ItchEditor.prototype.initialize.apply(this, arguments);
    this.label = mode.name;
    this.editor = new Editor({
      mode: mode,
      lineNumbers: true,
      extraKeys: this.extraKeys,
      themeSwitching: false,
      autocomplete: true,
      contextMenu:  this.host.textEditorContextMenuPopup
    });

    // Trigger a few editor specific events on `this`.
    this.editor.on("change", (...args) => {
      this.emit("change", ...args);
    });
    this.editor.on("cursorActivity", (...args) => {
      this.emit("cursorActivity", ...args);
    });
    this.editor.on("focus", (...args) => {
      this.emit("focus", ...args);
    });
    this.editor.on("saveRequested", (...args) => {
      this.emit("saveRequested", ...args);
    });

    this.appended = this.editor.appendTo(this.elt);
  },

  /**
   * Clean up the editor.  This can have different meanings
   * depending on the type of editor.
   */
  destroy: function () {
    this.editor.destroy();
    this.editor = null;
  },

  /**
   * Load a single resource into the text editor.
   *
   * @param Resource resource
   *        The single file / item that is being dealt with (see stores/base)
   * @returns Promise
   *          A promise that is resolved once the text editor has loaded the
   *          contents of the resource.
   */
  load: function (resource) {
    // Wait for the editor.appendTo and resource.load before proceeding.
    // They can run in parallel.
    return promise.all([
      resource.load(),
      this.appended
    ]).then(([resourceContents])=> {
      if (!this.editor) {
        return;
      }
      this._savedResourceContents = resourceContents;
      this.editor.setText(resourceContents);
      this.editor.clearHistory();
      this.editor.setClean();
      this.emit("load");
    }, console.error);
  },

  /**
   * Save the resource based on the current state of the editor
   *
   * @param Resource resource
   *        The single file / item to be saved
   * @returns Promise
   *          A promise that is resolved once the resource has been
   *          saved.
   */
  save: function (resource) {
    let newText = this.editor.getText();
    return resource.save(newText).then(() => {
      this._savedResourceContents = newText;
      this.emit("save", resource);
    });
  },

  /**
   * Give focus to the code editor.
   *
   * @returns Promise
   *          A promise that is resolved once the editor has been focused.
   */
  focus: function () {
    return this.appended.then(() => {
      if (this.editor) {
        this.editor.focus();
      }
    });
  }
});

/**
 * Wrapper for TextEditor using JavaScript syntax highlighting.
 */
function JSEditor(host) {
  return TextEditor(host, Editor.modes.js);
}

/**
 * Wrapper for TextEditor using CSS syntax highlighting.
 */
function CSSEditor(host) {
  return TextEditor(host, Editor.modes.css);
}

/**
 * Wrapper for TextEditor using HTML syntax highlighting.
 */
function HTMLEditor(host) {
  return TextEditor(host, Editor.modes.html);
}

/**
 * Get the type of editor that can handle a particular resource.
 * @param Resource resource
 *        The single file that is going to be opened.
 * @returns Type:Editor
 *          The type of editor that can handle this resource.  The
 *          return value is a constructor function.
 */
function EditorTypeForResource(resource) {
  const categoryMap = {
    "txt": TextEditor,
    "html": HTMLEditor,
    "xml": HTMLEditor,
    "css": CSSEditor,
    "js": JSEditor,
    "json": JSEditor
  };
  return categoryMap[resource.contentCategory] || TextEditor;
}

exports.TextEditor = TextEditor;
exports.JSEditor = JSEditor;
exports.CSSEditor = CSSEditor;
exports.HTMLEditor = HTMLEditor;
exports.EditorTypeForResource = EditorTypeForResource;
