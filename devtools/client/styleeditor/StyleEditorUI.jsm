/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["StyleEditorUI"];

const Ci = Components.interfaces;
const Cu = Components.utils;

const {loader, require} = Cu.import("resource://devtools/shared/Loader.jsm", {});
const Services = require("Services");
const {NetUtil} = require("resource://gre/modules/NetUtil.jsm");
const {OS} = require("resource://gre/modules/osfile.jsm");
const {Task} = require("devtools/shared/task");
const EventEmitter = require("devtools/shared/old-event-emitter");
const {gDevTools} = require("devtools/client/framework/devtools");
const {
  getString,
  text,
  wire,
  showFilePicker,
} = require("resource://devtools/client/styleeditor/StyleEditorUtil.jsm");
const {SplitView} = require("resource://devtools/client/shared/SplitView.jsm");
const {StyleSheetEditor} = require("resource://devtools/client/styleeditor/StyleSheetEditor.jsm");
const {PluralForm} = require("devtools/shared/plural-form");
const {PrefObserver} = require("devtools/client/shared/prefs");
const csscoverage = require("devtools/shared/fronts/csscoverage");
const {console} = require("resource://gre/modules/Console.jsm");
const {KeyCodes} = require("devtools/client/shared/keycodes");
const {OriginalSource} = require("devtools/client/styleeditor/original-source");

loader.lazyRequireGetter(this, "ResponsiveUIManager", "devtools/client/responsive.html/manager", true);

const LOAD_ERROR = "error-load";
const STYLE_EDITOR_TEMPLATE = "stylesheet";
const SELECTOR_HIGHLIGHTER_TYPE = "SelectorHighlighter";
const PREF_MEDIA_SIDEBAR = "devtools.styleeditor.showMediaSidebar";
const PREF_SIDEBAR_WIDTH = "devtools.styleeditor.mediaSidebarWidth";
const PREF_NAV_WIDTH = "devtools.styleeditor.navSidebarWidth";
const PREF_ORIG_SOURCES = "devtools.source-map.client-service.enabled";

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
 * @param {CssProperties} A css properties database.
 */
function StyleEditorUI(debuggee, target, panelDoc, cssProperties) {
  EventEmitter.decorate(this);

  this._debuggee = debuggee;
  this._target = target;
  this._panelDoc = panelDoc;
  this._cssProperties = cssProperties;
  this._window = this._panelDoc.defaultView;
  this._root = this._panelDoc.getElementById("style-editor-chrome");

  this.editors = [];
  this.selectedEditor = null;
  this.savedLocations = {};
  this._seenSheets = new Map();

  // Don't add any style sheets that might arrive via events, until
  // the call to initialize.  Style sheets can arrive from the server
  // at any time, for example if a new style sheet was added, or if
  // the style sheet actor was just created and is walking the style
  // sheets for the first time.  In any case, in |initialize| we're
  // going to fetch the list of sheets anyway.
  this._suppressAdd = true;

  this._onOptionsPopupShowing = this._onOptionsPopupShowing.bind(this);
  this._onOptionsPopupHiding = this._onOptionsPopupHiding.bind(this);
  this._onNewDocument = this._onNewDocument.bind(this);
  this._onMediaPrefChanged = this._onMediaPrefChanged.bind(this);
  this._updateMediaList = this._updateMediaList.bind(this);
  this._clear = this._clear.bind(this);
  this._onError = this._onError.bind(this);
  this._updateOpenLinkItem = this._updateOpenLinkItem.bind(this);
  this._openLinkNewTab = this._openLinkNewTab.bind(this);
  this._addStyleSheet = this._addStyleSheet.bind(this);

  this._prefObserver = new PrefObserver("devtools.styleeditor.");
  this._prefObserver.on(PREF_MEDIA_SIDEBAR, this._onMediaPrefChanged);
  this._sourceMapPrefObserver = new PrefObserver("devtools.source-map.client-service.");
  this._sourceMapPrefObserver.on(PREF_ORIG_SOURCES, this._onNewDocument);

  this._debuggee.on("stylesheet-added", this._addStyleSheet);
}
this.StyleEditorUI = StyleEditorUI;

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
   * Initiates the style editor ui creation, the inspector front to get
   * reference to the walker and the selector highlighter if available
   */
  initialize: Task.async(function* () {
    yield this.initializeHighlighter();

    this.createUI();

    let styleSheets = yield this._debuggee.getStyleSheets();
    yield this._resetStyleSheetList(styleSheets);

    this._target.on("will-navigate", this._clear);
    this._target.on("navigate", this._onNewDocument);
  }),

  initializeHighlighter: Task.async(function* () {
    let toolbox = gDevTools.getToolbox(this._target);
    yield toolbox.initInspector();
    this._walker = toolbox.walker;

    let hUtils = toolbox.highlighterUtils;
    if (hUtils.supportsCustomHighlighters()) {
      try {
        this._highlighter =
          yield hUtils.getHighlighterByType(SELECTOR_HIGHLIGHTER_TYPE);
      } catch (e) {
        // The selectorHighlighter can't always be instantiated, for example
        // it doesn't work with XUL windows (until bug 1094959 gets fixed);
        // or the selectorHighlighter doesn't exist on the backend.
        console.warn("The selectorHighlighter couldn't be instantiated, " +
          "elements matching hovered selectors will not be highlighted");
      }
    }
  }),

  /**
   * Build the initial UI and wire buttons with event handlers.
   */
  createUI: function () {
    let viewRoot = this._root.parentNode.querySelector(".splitview-root");

    this._view = new SplitView(viewRoot);

    wire(this._view.rootElement, ".style-editor-newButton", () =>{
      this._debuggee.addStyleSheet(null);
    });

    wire(this._view.rootElement, ".style-editor-importButton", ()=> {
      this._importFromFile(this._mockImportFile || null, this._window);
    });

    this._optionsButton = this._panelDoc.getElementById("style-editor-options");
    this._panelDoc.addEventListener("contextmenu", () => {
      this._contextMenuStyleSheet = null;
    }, true);

    this._contextMenu = this._panelDoc.getElementById("sidebar-context");
    this._contextMenu.addEventListener("popupshowing",
                                       this._updateOpenLinkItem);

    this._optionsMenu =
      this._panelDoc.getElementById("style-editor-options-popup");
    this._optionsMenu.addEventListener("popupshowing",
                                       this._onOptionsPopupShowing);
    this._optionsMenu.addEventListener("popuphiding",
                                       this._onOptionsPopupHiding);

    this._sourcesItem = this._panelDoc.getElementById("options-origsources");
    this._sourcesItem.addEventListener("command",
                                       this._toggleOrigSources);

    this._mediaItem = this._panelDoc.getElementById("options-show-media");
    this._mediaItem.addEventListener("command",
                                     this._toggleMediaSidebar);

    this._openLinkNewTabItem =
      this._panelDoc.getElementById("context-openlinknewtab");
    this._openLinkNewTabItem.addEventListener("command",
                                              this._openLinkNewTab);

    let nav = this._panelDoc.querySelector(".splitview-controller");
    nav.setAttribute("width", Services.prefs.getIntPref(PREF_NAV_WIDTH));
  },

  /**
   * Listener handling the 'gear menu' popup showing event.
   * Update options menu items to reflect current preference settings.
   */
  _onOptionsPopupShowing: function () {
    this._optionsButton.setAttribute("open", "true");
    this._sourcesItem.setAttribute("checked",
      Services.prefs.getBoolPref(PREF_ORIG_SOURCES));
    this._mediaItem.setAttribute("checked",
      Services.prefs.getBoolPref(PREF_MEDIA_SIDEBAR));
  },

  /**
   * Listener handling the 'gear menu' popup hiding event.
   */
  _onOptionsPopupHiding: function () {
    this._optionsButton.removeAttribute("open");
  },

  /**
   * Refresh editors to reflect the stylesheets in the document.
   *
   * @param {string} event
   *        Event name
   * @param {StyleSheet} styleSheet
   *        StyleSheet object for new sheet
   */
  _onNewDocument: function () {
    this._suppressAdd = true;
    this._debuggee.getStyleSheets().then((styleSheets) => {
      return this._resetStyleSheetList(styleSheets);
    }).catch(console.error);
  },

  /**
   * Add editors for all the given stylesheets to the UI.
   *
   * @param  {array} styleSheets
   *         Array of StyleSheetFront
   */
  _resetStyleSheetList: Task.async(function* (styleSheets) {
    this._clear();
    this._suppressAdd = false;

    for (let sheet of styleSheets) {
      try {
        yield this._addStyleSheet(sheet);
      } catch (e) {
        console.error(e);
        this.emit("error", { key: LOAD_ERROR });
      }
    }

    this._root.classList.remove("loading");

    this.emit("stylesheets-reset");
  }),

  /**
   * Remove all editors and add loading indicator.
   */
  _clear: function () {
    // remember selected sheet and line number for next load
    if (this.selectedEditor && this.selectedEditor.sourceEditor) {
      let href = this.selectedEditor.styleSheet.href;
      let {line, ch} = this.selectedEditor.sourceEditor.getCursor();

      this._styleSheetToSelect = {
        stylesheet: href,
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
    // Here the keys are style sheet actors, and the values are
    // promises that resolve to the sheet's editor.  See |_addStyleSheet|.
    this._seenSheets = new Map();
    this._suppressAdd = true;

    this._root.classList.add("loading");
  },

  /**
   * Add an editor for this stylesheet. Add editors for its original sources
   * instead (e.g. Sass sources), if applicable.
   *
   * @param  {StyleSheetFront} styleSheet
   *         Style sheet to add to style editor
   * @param {Boolean} isNew
   *        True if this style sheet was created by a call to the
   *        style sheets actor's @see addStyleSheet method.
   * @return {Promise}
   *         A promise that resolves to the style sheet's editor when the style sheet has
   *         been fully loaded.  If the style sheet has a source map, and source mapping
   *         is enabled, then the promise resolves to null.
   */
  _addStyleSheet: function (styleSheet, isNew) {
    if (this._suppressAdd) {
      return null;
    }

    if (!this._seenSheets.has(styleSheet)) {
      let promise = (async () => {
        let editor = await this._addStyleSheetEditor(styleSheet, isNew);

        let toolbox = gDevTools.getToolbox(this._target);
        let sourceMapService = toolbox.sourceMapService;
        if (!sourceMapService) {
          return editor;
        }

        let {href, nodeHref, actorID: id, sourceMapURL} = styleSheet;
        let url = href || nodeHref;
        let sources = await sourceMapService.getOriginalURLs({
          id,
          url,
          sourceMapURL,
        });
        // A single generated sheet might map to multiple original
        // sheets, so make editors for each of them.
        if (sources && sources.length) {
          let parentEditorName = editor.friendlyName;
          this._removeStyleSheetEditor(editor);
          editor = null;

          for (let source of sources) {
            let generatedId = sourceMapService.generatedToOriginalId(id, source);
            let original = new OriginalSource(source, generatedId, sourceMapService);

            // set so the first sheet will be selected, even if it's a source
            original.styleSheetIndex = styleSheet.styleSheetIndex;
            original.relatedStyleSheet = styleSheet;
            original.relatedEditorName = parentEditorName;
            await this._addStyleSheetEditor(original);
          }
        }

        return editor;
      })();
      this._seenSheets.set(styleSheet, promise);
    }
    return this._seenSheets.get(styleSheet);
  },

  /**
   * Add a new editor to the UI for a source.
   *
   * @param {StyleSheet}  styleSheet
   *        Object representing stylesheet
   * @param {Boolean} isNew
   *         Optional if stylesheet is a new sheet created by user
   * @return {Promise} that is resolved with the created StyleSheetEditor when
   *                   the editor is fully initialized or rejected on error.
   */
  _addStyleSheetEditor: Task.async(function* (styleSheet, isNew) {
    // recall location of saved file for this sheet after page reload
    let file = null;
    let identifier = this.getStyleSheetIdentifier(styleSheet);
    let savedFile = this.savedLocations[identifier];
    if (savedFile) {
      file = savedFile;
    }

    let editor = new StyleSheetEditor(styleSheet, this._window, file, isNew,
                                      this._walker, this._highlighter);

    editor.on("property-change", this._summaryChange.bind(this, editor));
    editor.on("media-rules-changed", this._updateMediaList.bind(this, editor));
    editor.on("linked-css-file", this._summaryChange.bind(this, editor));
    editor.on("linked-css-file-error", this._summaryChange.bind(this, editor));
    editor.on("error", this._onError);

    this.editors.push(editor);

    yield editor.fetchSource();
    this._sourceLoaded(editor);

    return editor;
  }),

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
  _importFromFile: function (file, parentWindow) {
    let onFileSelected = (selectedFile) => {
      if (!selectedFile) {
        // nothing selected
        return;
      }
      NetUtil.asyncFetch({
        uri: NetUtil.newURI(selectedFile),
        loadingNode: this._window.document,
        securityFlags: Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_DATA_INHERITS,
        contentPolicyType: Ci.nsIContentPolicy.TYPE_OTHER
      }, (stream, status) => {
        if (!Components.isSuccessCode(status)) {
          this.emit("error", { key: LOAD_ERROR });
          return;
        }
        let source =
            NetUtil.readInputStreamToString(stream, stream.available());
        stream.close();

        this._suppressAdd = true;
        this._debuggee.addStyleSheet(source).then((styleSheet) => {
          this._suppressAdd = false;
          this._addStyleSheet(styleSheet, true).then(editor => {
            if (editor) {
              editor.savedFile = selectedFile;
            }
            // Just for testing purposes.
            this.emit("test:editor-updated", editor);
          });
        });
      });
    };

    showFilePicker(file, false, parentWindow, onFileSelected);
  },

  /**
   * Forward any error from a stylesheet.
   *
   * @param  {string} event
   *         Event name
   * @param  {data} data
   *         The event data
   */
  _onError: function (event, data) {
    this.emit("error", data);
  },

  /**
   * Toggle the original sources pref.
   */
  _toggleOrigSources: function () {
    let isEnabled = Services.prefs.getBoolPref(PREF_ORIG_SOURCES);
    Services.prefs.setBoolPref(PREF_ORIG_SOURCES, !isEnabled);
  },

  /**
   * Toggle the pref for showing a @media rules sidebar in each editor.
   */
  _toggleMediaSidebar: function () {
    let isEnabled = Services.prefs.getBoolPref(PREF_MEDIA_SIDEBAR);
    Services.prefs.setBoolPref(PREF_MEDIA_SIDEBAR, !isEnabled);
  },

  /**
   * Toggle the @media sidebar in each editor depending on the setting.
   */
  _onMediaPrefChanged: function () {
    this.editors.forEach(this._updateMediaList);
  },

  /**
   * This method handles the following cases related to the context
   * menu item "_openLinkNewTabItem":
   *
   * 1) There was a stylesheet clicked on and it is external: show and
   * enable the context menu item
   * 2) There was a stylesheet clicked on and it is inline: show and
   * disable the context menu item
   * 3) There was no stylesheet clicked on (the right click happened
   * below the list): hide the context menu
   */
  _updateOpenLinkItem: function () {
    this._openLinkNewTabItem.setAttribute("hidden",
                                          !this._contextMenuStyleSheet);
    if (this._contextMenuStyleSheet) {
      this._openLinkNewTabItem.setAttribute("disabled",
                                            !this._contextMenuStyleSheet.href);
    }
  },

  /**
   * Open a particular stylesheet in a new tab.
   */
  _openLinkNewTab: function () {
    if (this._contextMenuStyleSheet) {
      this._window.openUILinkIn(this._contextMenuStyleSheet.href, "tab");
    }
  },

  /**
   * Remove a particular stylesheet editor from the UI
   *
   * @param {StyleSheetEditor}  editor
   *        The editor to remove.
   */
  _removeStyleSheetEditor: function (editor) {
    if (editor.summary) {
      this._view.removeItem(editor.summary);
    } else {
      let self = this;
      this.on("editor-added", function onAdd(event, added) {
        if (editor == added) {
          self.off("editor-added", onAdd);
          self._view.removeItem(editor.summary);
        }
      });
    }

    editor.destroy();
    this.editors.splice(this.editors.indexOf(editor), 1);
  },

  /**
   * Clear all the editors from the UI.
   */
  _clearStyleSheetEditors: function () {
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
  _sourceLoaded: function (editor) {
    let ordinal = editor.styleSheet.styleSheetIndex;
    ordinal = ordinal == -1 ? Number.MAX_SAFE_INTEGER : ordinal;
    // add new sidebar item and editor to the UI
    this._view.appendTemplatedItem(STYLE_EDITOR_TEMPLATE, {
      data: {
        editor: editor
      },
      disableAnimations: this._alwaysDisableAnimations,
      ordinal: ordinal,
      onCreate: (summary, details, data) => {
        let createdEditor = data.editor;
        createdEditor.summary = summary;
        createdEditor.details = details;

        wire(summary, ".stylesheet-enabled", function onToggleDisabled(event) {
          event.stopPropagation();
          event.target.blur();

          createdEditor.toggleDisabled();
        });

        wire(summary, ".stylesheet-name", {
          events: {
            "keypress": (event) => {
              if (event.keyCode == KeyCodes.DOM_VK_RETURN) {
                this._view.activeSummary = summary;
              }
            }
          }
        });

        wire(summary, ".stylesheet-saveButton", function onSaveButton(event) {
          event.stopPropagation();
          event.target.blur();

          createdEditor.saveToFile(createdEditor.savedFile);
        });

        this._updateSummaryForEditor(createdEditor, summary);

        summary.addEventListener("contextmenu", () => {
          this._contextMenuStyleSheet = createdEditor.styleSheet;
        });

        summary.addEventListener("focus", function onSummaryFocus(event) {
          if (event.target == summary) {
            // autofocus the stylesheet name
            summary.querySelector(".stylesheet-name").focus();
          }
        });

        let sidebar = details.querySelector(".stylesheet-sidebar");
        sidebar.setAttribute("width",
            Services.prefs.getIntPref(PREF_SIDEBAR_WIDTH));

        let splitter = details.querySelector(".devtools-side-splitter");
        splitter.addEventListener("mousemove", () => {
          let sidebarWidth = sidebar.getAttribute("width");
          Services.prefs.setIntPref(PREF_SIDEBAR_WIDTH, sidebarWidth);

          // update all @media sidebars for consistency
          let sidebars =
              [...this._panelDoc.querySelectorAll(".stylesheet-sidebar")];
          for (let mediaSidebar of sidebars) {
            mediaSidebar.setAttribute("width", sidebarWidth);
          }
        });

        // autofocus if it's a new user-created stylesheet
        if (createdEditor.isNew) {
          this._selectEditor(createdEditor);
        }

        if (this._isEditorToSelect(createdEditor)) {
          this.switchToSelectedSheet();
        }

        // If this is the first stylesheet and there is no pending request to
        // select a particular style sheet, select this sheet.
        if (!this.selectedEditor && !this._styleSheetBoundToSelect
            && createdEditor.styleSheet.styleSheetIndex == 0) {
          this._selectEditor(createdEditor);
        }
        this.emit("editor-added", createdEditor);
      },

      onShow: (summary, details, data) => {
        let showEditor = data.editor;
        this.selectedEditor = showEditor;

        Task.spawn(function* () {
          if (!showEditor.sourceEditor) {
            // only initialize source editor when we switch to this view
            let inputElement =
                details.querySelector(".stylesheet-editor-input");
            yield showEditor.load(inputElement, this._cssProperties);
          }

          showEditor.onShow();

          this.emit("editor-selected", showEditor);

          // Is there any CSS coverage markup to include?
          let usage = yield csscoverage.getUsage(this._target);
          if (usage == null) {
            return;
          }

          let sheet = showEditor.styleSheet;
          let {reports} = yield usage.createEditorReportForSheet(sheet);

          showEditor.removeAllUnusedRegions();

          if (reports.length > 0) {
            // Only apply if this file isn't compressed. We detect a
            // compressed file if there are more rules than lines.
            let editorText = showEditor.sourceEditor.getText();
            let lineCount = editorText.split("\n").length;
            let ruleCount = showEditor.styleSheet.ruleCount;
            if (lineCount >= ruleCount) {
              showEditor.addUnusedRegions(reports);
            } else {
              this.emit("error", { key: "error-compressed", level: "info" });
            }
          }
        }.bind(this)).catch(console.error);
      }
    });
  },

  /**
   * Switch to the editor that has been marked to be selected.
   *
   * @return {Promise}
   *         Promise that will resolve when the editor is selected.
   */
  switchToSelectedSheet: function () {
    let toSelect = this._styleSheetToSelect;

    for (let editor of this.editors) {
      if (this._isEditorToSelect(editor)) {
        // The _styleSheetBoundToSelect will always hold the latest pending
        // requested style sheet (with line and column) which is not yet
        // selected by the source editor. Only after we select that particular
        // editor and go the required line and column, it will become null.
        this._styleSheetBoundToSelect = this._styleSheetToSelect;
        this._styleSheetToSelect = null;
        return this._selectEditor(editor, toSelect.line, toSelect.col);
      }
    }

    return Promise.resolve();
  },

  /**
   * Returns whether a given editor is the current editor to be selected. Tests
   * based on href or underlying stylesheet.
   *
   * @param {StyleSheetEditor} editor
   *        The editor to test.
   */
  _isEditorToSelect: function (editor) {
    let toSelect = this._styleSheetToSelect;
    if (!toSelect) {
      return false;
    }
    let isHref = toSelect.stylesheet === null ||
                 typeof toSelect.stylesheet == "string";

    return (isHref && editor.styleSheet.href == toSelect.stylesheet) ||
           (toSelect.stylesheet == editor.styleSheet);
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
   * @return {Promise}
   *         Promise that will resolve when the editor is selected and ready
   *         to be used.
   */
  _selectEditor: function (editor, line, col) {
    line = line || 0;
    col = col || 0;

    let editorPromise = editor.getSourceEditor().then(() => {
      editor.sourceEditor.setCursor({line: line, ch: col});
      this._styleSheetBoundToSelect = null;
    });

    let summaryPromise = this.getEditorSummary(editor).then((summary) => {
      this._view.activeSummary = summary;
    });

    return Promise.all([editorPromise, summaryPromise]);
  },

  getEditorSummary: function (editor) {
    let self = this;

    if (editor.summary) {
      return Promise.resolve(editor.summary);
    }

    return new Promise(resolve => {
      this.on("editor-added", function onAdd(e, selected) {
        if (selected == editor) {
          self.off("editor-added", onAdd);
          resolve(editor.summary);
        }
      });
    });
  },

  getEditorDetails: function (editor) {
    let self = this;

    if (editor.details) {
      return Promise.resolve(editor.details);
    }

    return new Promise(resolve => {
      this.on("editor-added", function onAdd(e, selected) {
        if (selected == editor) {
          self.off("editor-added", onAdd);
          resolve(editor.details);
        }
      });
    });
  },

  /**
   * Returns an identifier for the given style sheet.
   *
   * @param {StyleSheet} styleSheet
   *        The style sheet to be identified.
   */
  getStyleSheetIdentifier: function (styleSheet) {
    // Identify inline style sheets by their host page URI and index
    // at the page.
    return styleSheet.href ? styleSheet.href :
      "inline-" + styleSheet.styleSheetIndex + "-at-" + styleSheet.nodeHref;
  },

  /**
   * selects a stylesheet and optionally moves the cursor to a selected line
   *
   * @param {StyleSheetFront} [stylesheet]
   *        Stylesheet to select or href of stylesheet to select
   * @param {Number} [line]
   *        Line to which the caret should be moved (zero-indexed).
   * @param {Number} [col]
   *        Column to which the caret should be moved (zero-indexed).
   * @return {Promise}
   *         Promise that will resolve when the editor is selected and ready
   *         to be used.
   */
  selectStyleSheet: function (stylesheet, line, col) {
    this._styleSheetToSelect = {
      stylesheet: stylesheet,
      line: line,
      col: col,
    };

    /* Switch to the editor for this sheet, if it exists yet.
       Otherwise each editor will be checked when it's created. */
    return this.switchToSelectedSheet();
  },

  /**
   * Handler for an editor's 'property-changed' event.
   * Update the summary in the UI.
   *
   * @param  {StyleSheetEditor} editor
   *         Editor for which a property has changed
   */
  _summaryChange: function (editor) {
    this._updateSummaryForEditor(editor);
  },

  /**
   * Update split view summary of given StyleEditor instance.
   *
   * @param {StyleSheetEditor} editor
   * @param {DOMElement} summary
   *        Optional item's summary element to update. If none, item
   *        corresponding to passed editor is used.
   */
  _updateSummaryForEditor: function (editor, summary) {
    summary = summary || editor.summary;
    if (!summary) {
      return;
    }

    let ruleCount = editor.styleSheet.ruleCount;
    if (editor.styleSheet.relatedStyleSheet) {
      ruleCount = editor.styleSheet.relatedStyleSheet.ruleCount;
    }
    if (ruleCount === undefined) {
      ruleCount = "-";
    }

    let flags = [];
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

    let linkedCSSSource = "";
    if (editor.linkedCSSFile) {
      linkedCSSSource = OS.Path.basename(editor.linkedCSSFile);
    } else if (editor.styleSheet.relatedEditorName) {
      linkedCSSSource = editor.styleSheet.relatedEditorName;
    }
    text(summary, ".stylesheet-linked-file", linkedCSSSource);
    text(summary, ".stylesheet-title", editor.styleSheet.title || "");
    text(summary, ".stylesheet-rule-count",
      PluralForm.get(ruleCount,
                     getString("ruleCount.label")).replace("#1", ruleCount));
  },

  /**
   * Update the @media rules sidebar for an editor. Hide if there are no rules
   * Display a list of the @media rules in the editor's associated style sheet.
   * Emits a 'media-list-changed' event after updating the UI.
   *
   * @param  {StyleSheetEditor} editor
   *         Editor to update @media sidebar of
   */
  _updateMediaList: function (editor) {
    Task.spawn(function* () {
      let details = yield this.getEditorDetails(editor);
      let list = details.querySelector(".stylesheet-media-list");

      while (list.firstChild) {
        list.firstChild.remove();
      }

      let rules = editor.mediaRules;
      let showSidebar = Services.prefs.getBoolPref(PREF_MEDIA_SIDEBAR);
      let sidebar = details.querySelector(".stylesheet-sidebar");

      let inSource = false;

      for (let rule of rules) {
        let {line, column, parentStyleSheet} = rule;

        let location = {
          line: line,
          column: column,
          source: editor.styleSheet.href,
          styleSheet: parentStyleSheet
        };
        if (editor.styleSheet.isOriginalSource) {
          let styleSheet = editor.cssSheet;
          location = yield editor.styleSheet.getOriginalLocation(styleSheet, line,
                                                                 column);
        }

        // this @media rule is from a different original source
        if (location.source != editor.styleSheet.href) {
          continue;
        }
        inSource = true;

        let div = this._panelDoc.createElement("div");
        div.className = "media-rule-label";
        div.addEventListener("click",
                             this._jumpToLocation.bind(this, location));

        let cond = this._panelDoc.createElement("div");
        cond.className = "media-rule-condition";
        if (!rule.matches) {
          cond.classList.add("media-condition-unmatched");
        }
        if (this._target.isLocalTab) {
          this._setConditionContents(cond, rule.conditionText);
        } else {
          cond.textContent = rule.conditionText;
        }
        div.appendChild(cond);

        let link = this._panelDoc.createElement("div");
        link.className = "media-rule-line theme-link";
        if (location.line != -1) {
          link.textContent = ":" + location.line;
        }
        div.appendChild(link);

        list.appendChild(div);
      }

      sidebar.hidden = !showSidebar || !inSource;

      this.emit("media-list-changed", editor);
    }.bind(this)).catch(console.error);
  },

  /**
   * Used to safely inject media query links
   *
   * @param {HTMLElement} element
   *        The element corresponding to the media sidebar condition
   * @param {String} rawText
   *        The raw condition text to parse
   */
  _setConditionContents(element, rawText) {
    const minMaxPattern = /(min\-|max\-)(width|height):\s\d+(px)/ig;

    let match = minMaxPattern.exec(rawText);
    let lastParsed = 0;
    while (match && match.index != minMaxPattern.lastIndex) {
      let matchEnd = match.index + match[0].length;
      let node = this._panelDoc.createTextNode(
        rawText.substring(lastParsed, match.index)
      );
      element.appendChild(node);

      let link = this._panelDoc.createElement("a");
      link.href = "#";
      link.className = "media-responsive-mode-toggle";
      link.textContent = rawText.substring(match.index, matchEnd);
      link.addEventListener("click", this._onMediaConditionClick.bind(this));
      element.appendChild(link);

      match = minMaxPattern.exec(rawText);
      lastParsed = matchEnd;
    }

    let node = this._panelDoc.createTextNode(
      rawText.substring(lastParsed, rawText.length)
    );
    element.appendChild(node);
  },

  /**
   * Called when a media condition is clicked
   * If a responsive mode link is clicked, it will launch it.
   *
   * @param {object} e
   *        Event object
   */
  _onMediaConditionClick: function (e) {
    let conditionText = e.target.textContent;
    let isWidthCond = conditionText.toLowerCase().indexOf("width") > -1;
    let mediaVal = parseInt(/\d+/.exec(conditionText), 10);

    let options = isWidthCond ? {width: mediaVal} : {height: mediaVal};
    this._launchResponsiveMode(options);
    e.preventDefault();
    e.stopPropagation();
  },

  /**
   * Launches the responsive mode with a specific width or height
   *
   * @param  {object} options
   *         Object with width or/and height properties.
   */
  _launchResponsiveMode: Task.async(function* (options = {}) {
    let tab = this._target.tab;
    let win = this._target.tab.ownerDocument.defaultView;

    yield ResponsiveUIManager.openIfNeeded(win, tab);
    ResponsiveUIManager.getResponsiveUIForTab(tab).setViewportSize(options);
  }),

  /**
   * Jump cursor to the editor for a stylesheet and line number for a rule.
   *
   * @param  {object} location
   *         Location object with 'line', 'column', and 'source' properties.
   */
  _jumpToLocation: function (location) {
    let source = location.styleSheet || location.source;
    this.selectStyleSheet(source, location.line - 1, location.column - 1);
  },

  destroy: function () {
    if (this._highlighter) {
      this._highlighter.finalize();
      this._highlighter = null;
    }

    this._clearStyleSheetEditors();

    this._seenSheets = null;
    this._suppressAdd = false;

    let sidebar = this._panelDoc.querySelector(".splitview-controller");
    let sidebarWidth = sidebar.getAttribute("width");
    Services.prefs.setIntPref(PREF_NAV_WIDTH, sidebarWidth);

    this._optionsMenu.removeEventListener("popupshowing",
                                          this._onOptionsPopupShowing);
    this._optionsMenu.removeEventListener("popuphiding",
                                          this._onOptionsPopupHiding);

    this._sourceMapPrefObserver.off(PREF_ORIG_SOURCES, this._onNewDocument);
    this._sourceMapPrefObserver.destroy();
    this._prefObserver.off(PREF_MEDIA_SIDEBAR, this._onMediaPrefChanged);
    this._prefObserver.destroy();

    this._debuggee.off("stylesheet-added", this._addStyleSheet);
  }
};
