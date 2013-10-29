/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["StyleEditorUI"];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/PluralForm.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");
let promise = Cu.import("resource://gre/modules/commonjs/sdk/core/promise.js").Promise;
Cu.import("resource:///modules/devtools/shared/event-emitter.js");
Cu.import("resource:///modules/devtools/StyleEditorUtil.jsm");
Cu.import("resource:///modules/devtools/SplitView.jsm");
Cu.import("resource:///modules/devtools/StyleSheetEditor.jsm");


const LOAD_ERROR = "error-load";

const STYLE_EDITOR_TEMPLATE = "stylesheet";

/**
 * StyleEditorUI is controls and builds the UI of the Style Editor, including
 * maintaining a list of editors for each stylesheet on a debuggee.
 *
 * Emits events:
 *   'editor-added': A new editor was added to the UI
 *   'editor-selected': An editor was selected
 *   'error': An error occured
 *
 * @param {StyleEditorDebuggee} debuggee
 *        Debuggee of whose stylesheets should be shown in the UI
 * @param {Document} panelDoc
 *        Document of the toolbox panel to populate UI in.
 */
function StyleEditorUI(debuggee, panelDoc) {
  EventEmitter.decorate(this);

  this._debuggee = debuggee;
  this._panelDoc = panelDoc;
  this._window = this._panelDoc.defaultView;
  this._root = this._panelDoc.getElementById("style-editor-chrome");

  this.editors = [];
  this.selectedEditor = null;

  this._onStyleSheetCreated = this._onStyleSheetCreated.bind(this);
  this._onStyleSheetsCleared = this._onStyleSheetsCleared.bind(this);
  this._onDocumentLoad = this._onDocumentLoad.bind(this);
  this._onError = this._onError.bind(this);

  debuggee.on("document-load", this._onDocumentLoad);
  debuggee.on("stylesheets-cleared", this._onStyleSheetsCleared);

  this.createUI();
}

StyleEditorUI.prototype = {
  /**
   * Get whether any of the editors have unsaved changes.
   *
   * @return boolean
   */
  get isDirty()
  {
    if (this._markedDirty === true) {
      return true;
    }
    return this.editors.some((editor) => {
      return editor.sourceEditor && !editor.sourceEditor.isClean();
    });
  },

  /*
   * Mark the style editor as having or not having unsaved changes.
   */
  set isDirty(value) {
    this._markedDirty = value;
  },

  /*
   * Index of selected stylesheet in document.styleSheets
   */
  get selectedStyleSheetIndex() {
    return this.selectedEditor ?
           this.selectedEditor.styleSheet.styleSheetIndex : -1;
  },

  /**
   * Build the initial UI and wire buttons with event handlers.
   */
  createUI: function() {
    let viewRoot = this._root.parentNode.querySelector(".splitview-root");

    this._view = new SplitView(viewRoot);

    wire(this._view.rootElement, ".style-editor-newButton", function onNew() {
      this._debuggee.createStyleSheet(null, this._onStyleSheetCreated);
    }.bind(this));

    wire(this._view.rootElement, ".style-editor-importButton", function onImport() {
      this._importFromFile(this._mockImportFile || null, this._window);
    }.bind(this));
  },

  /**
   * Import a style sheet from file and asynchronously create a
   * new stylesheet on the debuggee for it.
   *
   * @param {mixed} file
   *        Optional nsIFile or filename string.
   *        If not set a file picker will be shown.
   * @param {nsIWindow} parentWindow
   *        Optional parent window for the file picker.
   */
  _importFromFile: function(file, parentWindow)
  {
    let onFileSelected = function(file) {
      if (!file) {
        this.emit("error", LOAD_ERROR);
        return;
      }
      NetUtil.asyncFetch(file, (stream, status) => {
        if (!Components.isSuccessCode(status)) {
          this.emit("error", LOAD_ERROR);
          return;
        }
        let source = NetUtil.readInputStreamToString(stream, stream.available());
        stream.close();

        this._debuggee.createStyleSheet(source, (styleSheet) => {
          this._onStyleSheetCreated(styleSheet, file);
        });
      });

    }.bind(this);

    showFilePicker(file, false, parentWindow, onFileSelected);
  },

  /**
   * Handler for debuggee's 'stylesheets-cleared' event. Remove all editors.
   */
  _onStyleSheetsCleared: function() {
    // remember selected sheet and line number for next load
    if (this.selectedEditor && this.selectedEditor.sourceEditor) {
      let href = this.selectedEditor.styleSheet.href;
      let {line, ch} = this.selectedEditor.sourceEditor.getCursor();
      this.selectStyleSheet(href, line, ch);
    }

    this._clearStyleSheetEditors();
    this._view.removeAll();

    this.selectedEditor = null;

    this._root.classList.add("loading");
  },

  /**
   * When a new or imported stylesheet has been added to the document.
   * Add an editor for it.
   */
  _onStyleSheetCreated: function(styleSheet, file) {
    this._addStyleSheetEditor(styleSheet, file, true);
  },

  /**
   * Handler for debuggee's 'document-load' event. Add editors
   * for all style sheets in the document
   *
   * @param {string} event
   *        Event name
   * @param {StyleSheet} styleSheet
   *        StyleSheet object for new sheet
   */
  _onDocumentLoad: function(event, styleSheets) {
    if (this._styleSheetToSelect) {
      // if selected stylesheet from previous load isn't here,
      // just set first stylesheet to be selected instead
      let selectedExists = styleSheets.some((sheet) => {
        return this._styleSheetToSelect.href == sheet.href;
      })
      if (!selectedExists) {
        this._styleSheetToSelect = null;
      }
    }
    for (let sheet of styleSheets) {
      this._addStyleSheetEditor(sheet);
    }

    this._root.classList.remove("loading");

    this.emit("document-load");
  },

  /**
   * Forward any error from a stylesheet.
   *
   * @param  {string} event
   *         Event name
   * @param  {string} errorCode
   *         Code represeting type of error
   */
  _onError: function(event, errorCode) {
    this.emit("error", errorCode);
  },

  /**
   * Add a new editor to the UI for a stylesheet.
   *
   * @param {StyleSheet}  styleSheet
   *        Object representing stylesheet
   * @param {nsIfile}  file
   *         Optional file object that sheet was imported from
   * @param {Boolean} isNew
   *         Optional if stylesheet is a new sheet created by user
   */
  _addStyleSheetEditor: function(styleSheet, file, isNew) {
    let editor = new StyleSheetEditor(styleSheet, this._window, file, isNew);

    editor.once("source-load", this._sourceLoaded.bind(this, editor));
    editor.on("property-change", this._summaryChange.bind(this, editor));
    editor.on("style-applied", this._summaryChange.bind(this, editor));
    editor.on("error", this._onError);

    this.editors.push(editor);

    // Queue editor loading. This helps responsivity during loading when
    // there are many heavy stylesheets.
    this._window.setTimeout(editor.fetchSource.bind(editor), 0);
  },

  /**
   * Clear all the editors from the UI.
   */
  _clearStyleSheetEditors: function() {
    for (let editor of this.editors) {
      editor.destroy();
    }
    this.editors = [];
  },

  /**
   * Handler for an StyleSheetEditor's 'source-load' event.
   * Create a summary UI for the editor.
   *
   * @param  {StyleSheetEditor} editor
   *         Editor to create UI for.
   */
  _sourceLoaded: function(editor) {
    // add new sidebar item and editor to the UI
    this._view.appendTemplatedItem(STYLE_EDITOR_TEMPLATE, {
      data: {
        editor: editor
      },
      disableAnimations: this._alwaysDisableAnimations,
      ordinal: editor.styleSheet.styleSheetIndex,
      onCreate: function(summary, details, data) {
        let editor = data.editor;
        editor.summary = summary;

        wire(summary, ".stylesheet-enabled", function onToggleDisabled(event) {
          event.stopPropagation();
          event.target.blur();

          editor.toggleDisabled();
        });

        wire(summary, ".stylesheet-name", {
          events: {
            "keypress": function onStylesheetNameActivate(aEvent) {
              if (aEvent.keyCode == aEvent.DOM_VK_RETURN) {
                this._view.activeSummary = summary;
              }
            }.bind(this)
          }
        });

        wire(summary, ".stylesheet-saveButton", function onSaveButton(event) {
          event.stopPropagation();
          event.target.blur();

          editor.saveToFile(editor.savedFile);
        });

        this._updateSummaryForEditor(editor, summary);

        summary.addEventListener("focus", function onSummaryFocus(event) {
          if (event.target == summary) {
            // autofocus the stylesheet name
            summary.querySelector(".stylesheet-name").focus();
          }
        }, false);

        // autofocus if it's a new user-created stylesheet
        if (editor.isNew) {
          this._selectEditor(editor);
        }

        if (this._styleSheetToSelect
            && this._styleSheetToSelect.href == editor.styleSheet.href) {
          this.switchToSelectedSheet();
        }

        // If this is the first stylesheet, select it
        if (this.selectedStyleSheetIndex == -1
            && !this._styleSheetToSelect
            && editor.styleSheet.styleSheetIndex == 0) {
          this._selectEditor(editor);
        }

        this.emit("editor-added", editor);
      }.bind(this),

      onShow: function(summary, details, data) {
        let editor = data.editor;
        this.selectedEditor = editor;
        this._styleSheetToSelect = null;

        if (!editor.sourceEditor) {
          // only initialize source editor when we switch to this view
          let inputElement = details.querySelector(".stylesheet-editor-input");
          editor.load(inputElement);
        }
        editor.onShow();

        this.emit("editor-selected", editor);
      }.bind(this)
    });
  },

  /**
   * Switch to the editor that has been marked to be selected.
   */
  switchToSelectedSheet: function() {
    let sheet = this._styleSheetToSelect;

    for each (let editor in this.editors) {
      if (editor.styleSheet.href == sheet.href) {
        this._selectEditor(editor, sheet.line, sheet.col);
        break;
      }
    }
  },

  /**
   * Select an editor in the UI.
   *
   * @param  {StyleSheetEditor} editor
   *         Editor to switch to.
   * @param  {number} line
   *         Line number to jump to
   * @param  {number} col
   *         Column number to jump to
   */
  _selectEditor: function(editor, line, col) {
    line = line || 0;
    col = col || 0;

    editor.getSourceEditor().then(() => {
      editor.sourceEditor.setCursor({line: line, ch: col});
    });

    this._view.activeSummary = editor.summary;
  },

  /**
   * selects a stylesheet and optionally moves the cursor to a selected line
   *
   * @param {string} [href]
   *        Href of stylesheet that should be selected. If a stylesheet is not passed
   *        and the editor is not initialized we focus the first stylesheet. If
   *        a stylesheet is not passed and the editor is initialized we ignore
   *        the call.
   * @param {Number} [line]
   *        Line to which the caret should be moved (zero-indexed).
   * @param {Number} [col]
   *        Column to which the caret should be moved (zero-indexed).
   */
  selectStyleSheet: function(href, line, col)
  {
    let alreadyCalled = !!this._styleSheetToSelect;
    let originalHref;

    if (alreadyCalled) {
      originalHref = this._styleSheetToSelect.href;
    }

    this._styleSheetToSelect = {
      href: href,
      line: line,
      col: col,
    };

    if (alreadyCalled) {
      // Just switch to the correct line and columns if the editor is already
      // selected for the requested stylesheet.
      for each (let editor in this.editors) {
        if (editor.styleSheet.href == originalHref) {
          editor.sourceEditor.setCursor({line: line, ch: col})
          break;
        }
      }
      return;
    }

    /* Switch to the editor for this sheet, if it exists yet.
       Otherwise each editor will be checked when it's created. */
    this.switchToSelectedSheet();
  },


  /**
   * Handler for an editor's 'property-changed' event.
   * Update the summary in the UI.
   *
   * @param  {StyleSheetEditor} editor
   *         Editor for which a property has changed
   */
  _summaryChange: function(editor) {
    this._updateSummaryForEditor(editor);
  },

  /**
   * Update split view summary of given StyleEditor instance.
   *
   * @param {StyleSheetEditor} editor
   * @param {DOMElement} summary
   *        Optional item's summary element to update. If none, item corresponding
   *        to passed editor is used.
   */
  _updateSummaryForEditor: function(editor, summary) {
    summary = summary || editor.summary;
    if (!summary) {
      return;
    }
    let ruleCount = "-";
    if (editor.styleSheet.ruleCount !== undefined) {
      ruleCount = editor.styleSheet.ruleCount;
    }

    var flags = [];
    if (editor.styleSheet.disabled) {
      flags.push("disabled");
    }
    if (editor.unsaved) {
      flags.push("unsaved");
    }
    this._view.setItemClassName(summary, flags.join(" "));

    let label = summary.querySelector(".stylesheet-name > label");
    label.setAttribute("value", editor.friendlyName);

    text(summary, ".stylesheet-title", editor.styleSheet.title || "");
    text(summary, ".stylesheet-rule-count",
      PluralForm.get(ruleCount, _("ruleCount.label")).replace("#1", ruleCount));
    text(summary, ".stylesheet-error-message", editor.errorMessage);
  },

  destroy: function() {
    this._clearStyleSheetEditors();

    this._debuggee.off("document-load", this._onDocumentLoad);
    this._debuggee.off("stylesheets-cleared", this._onStyleSheetsCleared);
  }
}
