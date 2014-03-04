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
Cu.import("resource://gre/modules/osfile.jsm");
let promise = Cu.import("resource://gre/modules/commonjs/sdk/core/promise.js").Promise;
Cu.import("resource://gre/modules/devtools/event-emitter.js");
Cu.import("resource:///modules/devtools/gDevTools.jsm");
Cu.import("resource:///modules/devtools/StyleEditorUtil.jsm");
Cu.import("resource:///modules/devtools/SplitView.jsm");
Cu.import("resource:///modules/devtools/StyleSheetEditor.jsm");

const require = Cu.import("resource://gre/modules/devtools/Loader.jsm", {}).devtools.require;
const { PrefObserver, PREF_ORIG_SOURCES } = require("devtools/styleeditor/utils");

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
 * @param {StyleEditorFront} debuggee
 *        Client-side front for interacting with the page's stylesheets
 * @param {Target} target
 *        Interface for the page we're debugging
 * @param {Document} panelDoc
 *        Document of the toolbox panel to populate UI in.
 */
function StyleEditorUI(debuggee, target, panelDoc) {
  EventEmitter.decorate(this);

  this._debuggee = debuggee;
  this._target = target;
  this._panelDoc = panelDoc;
  this._window = this._panelDoc.defaultView;
  this._root = this._panelDoc.getElementById("style-editor-chrome");

  this.editors = [];
  this.selectedEditor = null;
  this.savedLocations = {};

  this._updateSourcesLabel = this._updateSourcesLabel.bind(this);
  this._onStyleSheetCreated = this._onStyleSheetCreated.bind(this);
  this._onNewDocument = this._onNewDocument.bind(this);
  this._clear = this._clear.bind(this);
  this._onError = this._onError.bind(this);

  this._prefObserver = new PrefObserver("devtools.styleeditor.");
  this._prefObserver.on(PREF_ORIG_SOURCES, this._onNewDocument);
}

StyleEditorUI.prototype = {
  /**
   * Get whether any of the editors have unsaved changes.
   *
   * @return boolean
   */
  get isDirty() {
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
   * Initiates the style editor ui creation and the inspector front to get
   * reference to the walker.
   */
  initialize: function() {
    let toolbox = gDevTools.getToolbox(this._target);
    return toolbox.initInspector().then(() => {
      this._walker = toolbox.walker;
    }).then(() => {
      this.createUI();
      this._debuggee.getStyleSheets().then((styleSheets) => {
        this._resetStyleSheetList(styleSheets);

        this._target.on("will-navigate", this._clear);
        this._target.on("navigate", this._onNewDocument);
      });
    });
  },

  /**
   * Build the initial UI and wire buttons with event handlers.
   */
  createUI: function() {
    let viewRoot = this._root.parentNode.querySelector(".splitview-root");

    this._view = new SplitView(viewRoot);

    wire(this._view.rootElement, ".style-editor-newButton", function onNew() {
      this._debuggee.addStyleSheet(null).then(this._onStyleSheetCreated);
    }.bind(this));

    wire(this._view.rootElement, ".style-editor-importButton", function onImport() {
      this._importFromFile(this._mockImportFile || null, this._window);
    }.bind(this));

    this._contextMenu = this._panelDoc.getElementById("sidebar-context");
    this._contextMenu.addEventListener("popupshowing",
                                       this._updateSourcesLabel);

    this._sourcesItem = this._panelDoc.getElementById("context-origsources");
    this._sourcesItem.addEventListener("command",
                                       this._toggleOrigSources);
  },

  /**
   * Update text of context menu option to reflect whether we're showing
   * original sources (e.g. Sass files) or not.
   */
  _updateSourcesLabel: function() {
    let string = "showOriginalSources";
    if (Services.prefs.getBoolPref(PREF_ORIG_SOURCES)) {
      string = "showCSSSources";
    }
    this._sourcesItem.setAttribute("label", _(string + ".label"));
    this._sourcesItem.setAttribute("accesskey", _(string + ".accesskey"));
  },

  /**
   * Refresh editors to reflect the stylesheets in the document.
   *
   * @param {string} event
   *        Event name
   * @param {StyleSheet} styleSheet
   *        StyleSheet object for new sheet
   */
  _onNewDocument: function() {
    this._debuggee.getStyleSheets().then((styleSheets) => {
      this._resetStyleSheetList(styleSheets);
    })
  },

  /**
   * Add editors for all the given stylesheets to the UI.
   *
   * @param  {array} styleSheets
   *         Array of StyleSheetFront
   */
  _resetStyleSheetList: function(styleSheets) {
    this._clear();

    for (let sheet of styleSheets) {
      this._addStyleSheet(sheet);
    }

    this._root.classList.remove("loading");

    this.emit("stylesheets-reset");
  },

  /**
   * Remove all editors and add loading indicator.
   */
  _clear: function() {
    // remember selected sheet and line number for next load
    if (this.selectedEditor && this.selectedEditor.sourceEditor) {
      let href = this.selectedEditor.styleSheet.href;
      let {line, ch} = this.selectedEditor.sourceEditor.getCursor();

      this._styleSheetToSelect = {
        href: href,
        line: line,
        col: ch
      };
    }

    // remember saved file locations
    for (let editor of this.editors) {
      if (editor.savedFile) {
        let identifier = this.getStyleSheetIdentifier(editor.styleSheet);
        this.savedLocations[identifier] = editor.savedFile;
      }
    }

    this._clearStyleSheetEditors();
    this._view.removeAll();

    this.selectedEditor = null;

    this._root.classList.add("loading");
  },

  /**
   * Add an editor for this stylesheet. Add editors for its original sources
   * instead (e.g. Sass sources), if applicable.
   *
   * @param  {StyleSheetFront} styleSheet
   *         Style sheet to add to style editor
   */
  _addStyleSheet: function(styleSheet) {
    let editor = this._addStyleSheetEditor(styleSheet);

    if (!Services.prefs.getBoolPref(PREF_ORIG_SOURCES)) {
      return;
    }

    styleSheet.getOriginalSources().then((sources) => {
      if (sources && sources.length) {
        this._removeStyleSheetEditor(editor);
        sources.forEach((source) => {
          // set so the first sheet will be selected, even if it's a source
          source.styleSheetIndex = styleSheet.styleSheetIndex;
          source.relatedStyleSheet = styleSheet;

          this._addStyleSheetEditor(source);
        });
      }
    });
  },

  /**
   * Add a new editor to the UI for a source.
   *
   * @param {StyleSheet}  styleSheet
   *        Object representing stylesheet
   * @param {nsIfile}  file
   *         Optional file object that sheet was imported from
   * @param {Boolean} isNew
   *         Optional if stylesheet is a new sheet created by user
   */
  _addStyleSheetEditor: function(styleSheet, file, isNew) {
    // recall location of saved file for this sheet after page reload
    let identifier = this.getStyleSheetIdentifier(styleSheet);
    let savedFile = this.savedLocations[identifier];
    if (savedFile && !file) {
      file = savedFile;
    }

    let editor =
      new StyleSheetEditor(styleSheet, this._window, file, isNew, this._walker);

    editor.on("property-change", this._summaryChange.bind(this, editor));
    editor.on("linked-css-file", this._summaryChange.bind(this, editor));
    editor.on("linked-css-file-error", this._summaryChange.bind(this, editor));
    editor.on("error", this._onError);

    this.editors.push(editor);

    editor.fetchSource(this._sourceLoaded.bind(this, editor));
    return editor;
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
  _importFromFile: function(file, parentWindow) {
    let onFileSelected = function(file) {
      if (!file) {
        // nothing selected
        return;
      }
      NetUtil.asyncFetch(file, (stream, status) => {
        if (!Components.isSuccessCode(status)) {
          this.emit("error", LOAD_ERROR);
          return;
        }
        let source = NetUtil.readInputStreamToString(stream, stream.available());
        stream.close();

        this._debuggee.addStyleSheet(source).then((styleSheet) => {
          this._onStyleSheetCreated(styleSheet, file);
        });
      });

    }.bind(this);

    showFilePicker(file, false, parentWindow, onFileSelected);
  },


  /**
   * When a new or imported stylesheet has been added to the document.
   * Add an editor for it.
   */
  _onStyleSheetCreated: function(styleSheet, file) {
    this._addStyleSheetEditor(styleSheet, file, true);
  },

  /**
   * Forward any error from a stylesheet.
   *
   * @param  {string} event
   *         Event name
   * @param  {string} errorCode
   *         Code represeting type of error
   * @param  {string} message
   *         The full error message
   */
  _onError: function(event, errorCode, message) {
    this.emit("error", errorCode, message);
  },

  /**
   *  Toggle the original sources pref.
   */
  _toggleOrigSources: function() {
    let isEnabled = Services.prefs.getBoolPref(PREF_ORIG_SOURCES);
    Services.prefs.setBoolPref(PREF_ORIG_SOURCES, !isEnabled);
  },

  /**
   * Remove a particular stylesheet editor from the UI
   *
   * @param {StyleSheetEditor}  editor
   *        The editor to remove.
   */
  _removeStyleSheetEditor: function(editor) {
    if (editor.summary) {
      this._view.removeItem(editor.summary);
    }
    else {
      let self = this;
      this.on("editor-added", function onAdd(event, added) {
        if (editor == added) {
          self.off("editor-added", onAdd);
          self._view.removeItem(editor.summary);
        }
      })
    }

    editor.destroy();
    this.editors.splice(this.editors.indexOf(editor), 1);
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
   * Called when a StyleSheetEditor's source has been fetched. Create a
   * summary UI for the editor.
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

        // If this is the first stylesheet and there is no pending request to
        // select a particular style sheet, select this sheet.
        if (!this.selectedEditor && !this._styleSheetBoundToSelect
            && editor.styleSheet.styleSheetIndex == 0) {
          this._selectEditor(editor);
        }

        this.emit("editor-added", editor);
      }.bind(this),

      onShow: function(summary, details, data) {
        let editor = data.editor;
        this.selectedEditor = editor;

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
        // The _styleSheetBoundToSelect will always hold the latest pending
        // requested style sheet (with line and column) which is not yet
        // selected by the source editor. Only after we select that particular
        // editor and go the required line and column, it will become null.
        this._styleSheetBoundToSelect = this._styleSheetToSelect;
        this._selectEditor(editor, sheet.line, sheet.col);
        this._styleSheetToSelect = null;
        return;
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
      this._styleSheetBoundToSelect = null;
    });

    this.getEditorSummary(editor).then((summary) => {
      this._view.activeSummary = summary;
    })
  },

  getEditorSummary: function(editor) {
    if (editor.summary) {
      return promise.resolve(editor.summary);
    }

    let deferred = promise.defer();
    let self = this;

    this.on("editor-added", function onAdd(e, selected) {
      if (selected == editor) {
        self.off("editor-added", onAdd);
        deferred.resolve(editor.summary);
      }
    });

    return deferred.promise;
  },

  /**
   * Returns an identifier for the given style sheet.
   *
   * @param {StyleSheet} aStyleSheet
   *        The style sheet to be identified.
   */
  getStyleSheetIdentifier: function (aStyleSheet) {
    // Identify inline style sheets by their host page URI and index at the page.
    return aStyleSheet.href ? aStyleSheet.href :
            "inline-" + aStyleSheet.styleSheetIndex + "-at-" + aStyleSheet.nodeHref;
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
  selectStyleSheet: function(href, line, col) {
    this._styleSheetToSelect = {
      href: href,
      line: line,
      col: col,
    };

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

    let ruleCount = editor.styleSheet.ruleCount;
    if (editor.styleSheet.relatedStyleSheet && editor.linkedCSSFile) {
      ruleCount = editor.styleSheet.relatedStyleSheet.ruleCount;
    }
    if (ruleCount === undefined) {
      ruleCount = "-";
    }

    var flags = [];
    if (editor.styleSheet.disabled) {
      flags.push("disabled");
    }
    if (editor.unsaved) {
      flags.push("unsaved");
    }
    if (editor.linkedCSSFileError) {
      flags.push("linked-file-error");
    }
    this._view.setItemClassName(summary, flags.join(" "));

    let label = summary.querySelector(".stylesheet-name > label");
    label.setAttribute("value", editor.friendlyName);
    if (editor.styleSheet.href) {
      label.setAttribute("tooltiptext", editor.styleSheet.href);
    }

    let linkedCSSFile = "";
    if (editor.linkedCSSFile) {
      linkedCSSFile = OS.Path.basename(editor.linkedCSSFile);
    }
    text(summary, ".stylesheet-linked-file", linkedCSSFile);
    text(summary, ".stylesheet-title", editor.styleSheet.title || "");
    text(summary, ".stylesheet-rule-count",
      PluralForm.get(ruleCount, _("ruleCount.label")).replace("#1", ruleCount));
  },

  destroy: function() {
    this._clearStyleSheetEditors();

    this._prefObserver.off(PREF_ORIG_SOURCES, this._onNewDocument);
    this._prefObserver.destroy();
  }
}
