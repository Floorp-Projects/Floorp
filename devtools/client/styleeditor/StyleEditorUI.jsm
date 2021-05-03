/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["StyleEditorUI"];

const { loader, require } = ChromeUtils.import(
  "resource://devtools/shared/Loader.jsm"
);
const Services = require("Services");
const { FileUtils } = require("resource://gre/modules/FileUtils.jsm");
const { NetUtil } = require("resource://gre/modules/NetUtil.jsm");
const { OS } = require("resource://gre/modules/osfile.jsm");
const EventEmitter = require("devtools/shared/event-emitter");
const {
  getString,
  text,
  wire,
  showFilePicker,
  optionsPopupMenu,
} = require("resource://devtools/client/styleeditor/StyleEditorUtil.jsm");
const {
  SplitView,
} = require("resource://devtools/client/shared/SplitView.jsm");
const {
  StyleSheetEditor,
} = require("resource://devtools/client/styleeditor/StyleSheetEditor.jsm");
const { PluralForm } = require("devtools/shared/plural-form");
const { PrefObserver } = require("devtools/client/shared/prefs");
const { KeyCodes } = require("devtools/client/shared/keycodes");
const {
  OriginalSource,
} = require("devtools/client/styleeditor/original-source");

loader.lazyRequireGetter(
  this,
  "ResponsiveUIManager",
  "devtools/client/responsive/manager"
);
loader.lazyRequireGetter(
  this,
  "openContentLink",
  "devtools/client/shared/link",
  true
);
loader.lazyRequireGetter(
  this,
  "copyString",
  "devtools/shared/platform/clipboard",
  true
);

const LOAD_ERROR = "error-load";
const STYLE_EDITOR_TEMPLATE = "stylesheet";
const SELECTOR_HIGHLIGHTER_TYPE = "SelectorHighlighter";
const PREF_MEDIA_SIDEBAR = "devtools.styleeditor.showMediaSidebar";
const PREF_SIDEBAR_WIDTH = "devtools.styleeditor.mediaSidebarWidth";
const PREF_NAV_WIDTH = "devtools.styleeditor.navSidebarWidth";
const PREF_ORIG_SOURCES = "devtools.source-map.client-service.enabled";

const HTML_NS = "http://www.w3.org/1999/xhtml";

/**
 * StyleEditorUI is controls and builds the UI of the Style Editor, including
 * maintaining a list of editors for each stylesheet on a debuggee.
 *
 * Emits events:
 *   'editor-added': A new editor was added to the UI
 *   'editor-selected': An editor was selected
 *   'error': An error occured
 *
 * @param {Toolbox} toolbox
 * @param {Object} commands Object defined from devtools/shared/commands to interact with the devtools backend
 * @param {Document} panelDoc
 *        Document of the toolbox panel to populate UI in.
 * @param {CssProperties} A css properties database.
 */
function StyleEditorUI(toolbox, commands, panelDoc, cssProperties) {
  EventEmitter.decorate(this);

  this._toolbox = toolbox;
  this._commands = commands;
  this._panelDoc = panelDoc;
  this._cssProperties = cssProperties;
  this._window = this._panelDoc.defaultView;
  this._root = this._panelDoc.getElementById("style-editor-chrome");

  this.editors = [];
  this.selectedEditor = null;
  this.savedLocations = {};
  this._seenSheets = new Map();

  this._onOptionsButtonClick = this._onOptionsButtonClick.bind(this);
  this._onOrigSourcesPrefChanged = this._onOrigSourcesPrefChanged.bind(this);
  this._onMediaPrefChanged = this._onMediaPrefChanged.bind(this);
  this._updateMediaList = this._updateMediaList.bind(this);
  this._clear = this._clear.bind(this);
  this._onError = this._onError.bind(this);
  this._updateContextMenuItems = this._updateContextMenuItems.bind(this);
  this._openLinkNewTab = this._openLinkNewTab.bind(this);
  this._copyUrl = this._copyUrl.bind(this);
  this._onTargetAvailable = this._onTargetAvailable.bind(this);
  this._onResourceAvailable = this._onResourceAvailable.bind(this);
  this._onResourceUpdated = this._onResourceUpdated.bind(this);

  this._prefObserver = new PrefObserver("devtools.styleeditor.");
  this._prefObserver.on(PREF_MEDIA_SIDEBAR, this._onMediaPrefChanged);
  this._sourceMapPrefObserver = new PrefObserver(
    "devtools.source-map.client-service."
  );
  this._sourceMapPrefObserver.on(
    PREF_ORIG_SOURCES,
    this._onOrigSourcesPrefChanged
  );
}

StyleEditorUI.prototype = {
  get cssProperties() {
    return this._cssProperties;
  },

  get currentTarget() {
    return this._commands.targetCommand.targetFront;
  },

  /*
   * Index of selected stylesheet in document.styleSheets
   */
  get selectedStyleSheetIndex() {
    return this.selectedEditor
      ? this.selectedEditor.styleSheet.styleSheetIndex
      : -1;
  },

  /**
   * Initiates the style editor ui creation, and start to track TargetCommand updates.
   */
  async initialize() {
    this.createUI();

    await this._commands.targetCommand.watchTargets(
      [this._commands.targetCommand.TYPES.FRAME],
      this._onTargetAvailable
    );

    await this._toolbox.resourceWatcher.watchResources(
      [this._toolbox.resourceWatcher.TYPES.DOCUMENT_EVENT],
      { onAvailable: this._onResourceAvailable }
    );

    this._startLoadingStyleSheets();
    await this._toolbox.resourceWatcher.watchResources(
      [this._toolbox.resourceWatcher.TYPES.STYLESHEET],
      {
        onAvailable: this._onResourceAvailable,
        onUpdated: this._onResourceUpdated,
      }
    );
    await this._waitForLoadingStyleSheets();
  },

  async initializeHighlighter(targetFront) {
    const inspectorFront = await targetFront.getFront("inspector");
    this._walker = inspectorFront.walker;

    try {
      this._highlighter = await inspectorFront.getHighlighterByType(
        SELECTOR_HIGHLIGHTER_TYPE
      );
    } catch (e) {
      // The selectorHighlighter can't always be instantiated, for example
      // it doesn't work with XUL windows (until bug 1094959 gets fixed);
      // or the selectorHighlighter doesn't exist on the backend.
      console.warn(
        "The selectorHighlighter couldn't be instantiated, " +
          "elements matching hovered selectors will not be highlighted"
      );
    }
  },

  /**
   * Build the initial UI and wire buttons with event handlers.
   */
  createUI: function() {
    this._view = new SplitView(this._root);

    wire(this._view.rootElement, ".style-editor-newButton", async () => {
      const stylesheetsFront = await this.currentTarget.getFront("stylesheets");
      stylesheetsFront.addStyleSheet(null);
    });

    wire(this._view.rootElement, ".style-editor-importButton", () => {
      this._importFromFile(this._mockImportFile || null, this._window);
    });

    wire(this._view.rootElement, "#style-editor-options", event => {
      this._onOptionsButtonClick(event);
    });

    this._panelDoc.addEventListener(
      "contextmenu",
      () => {
        this._contextMenuStyleSheet = null;
      },
      true
    );

    this._optionsButton = this._panelDoc.getElementById("style-editor-options");

    this._contextMenu = this._panelDoc.getElementById("sidebar-context");
    this._contextMenu.addEventListener(
      "popupshowing",
      this._updateContextMenuItems
    );

    this._openLinkNewTabItem = this._panelDoc.getElementById(
      "context-openlinknewtab"
    );
    this._openLinkNewTabItem.addEventListener("command", this._openLinkNewTab);

    this._copyUrlItem = this._panelDoc.getElementById("context-copyurl");
    this._copyUrlItem.addEventListener("command", this._copyUrl);

    const nav = this._panelDoc.querySelector(".splitview-controller");
    nav.setAttribute("width", Services.prefs.getIntPref(PREF_NAV_WIDTH));
  },

  /**
   * Opens the Options Popup Menu
   *
   * @params {number} screenX
   * @params {number} screenY
   *   Both obtained from the event object, used to position the popup
   */
  _onOptionsButtonClick({ screenX, screenY }) {
    this._optionsMenu = optionsPopupMenu(
      this._toggleOrigSources,
      this._toggleMediaSidebar
    );

    this._optionsMenu.once("open", () => {
      this._optionsButton.setAttribute("open", true);
    });
    this._optionsMenu.once("close", () => {
      this._optionsButton.removeAttribute("open");
    });

    this._optionsMenu.popup(screenX, screenY, this._toolbox.doc);
  },

  /**
   * Be called when changing the original sources pref.
   */
  async _onOrigSourcesPrefChanged() {
    this._clear();
    // When we toggle the source-map preference, we clear the panel and re-fetch the exact
    // same stylesheet resources from ResourceCommand, but `_addStyleSheet` will trigger
    // or ignore the additional source-map mapping.
    this._root.classList.add("loading");
    for (const resource of this._toolbox.resourceWatcher.getAllResources(
      this._toolbox.resourceWatcher.TYPES.STYLESHEET
    )) {
      await this._handleStyleSheetResource(resource);
    }
    this._root.classList.remove("loading");

    this.emit("stylesheets-refreshed");
  },

  /**
   * Remove all editors and add loading indicator.
   */
  _clear: function() {
    // remember selected sheet and line number for next load
    if (this.selectedEditor && this.selectedEditor.sourceEditor) {
      const href = this.selectedEditor.styleSheet.href;
      const { line, ch } = this.selectedEditor.sourceEditor.getCursor();

      this._styleSheetToSelect = {
        stylesheet: href,
        line: line,
        col: ch,
      };
    }

    // remember saved file locations
    for (const editor of this.editors) {
      if (editor.savedFile) {
        const identifier = this.getStyleSheetIdentifier(editor.styleSheet);
        this.savedLocations[identifier] = editor.savedFile;
      }
    }

    this._clearStyleSheetEditors();
    this._view.removeAll();

    this.selectedEditor = null;
    // Here the keys are style sheet actors, and the values are
    // promises that resolve to the sheet's editor.  See |_addStyleSheet|.
    this._seenSheets = new Map();

    this.emit("stylesheets-clear");
  },

  /**
   * Add an editor for this stylesheet. Add editors for its original sources
   * instead (e.g. Sass sources), if applicable.
   *
   * @param  {Resource} resource
   *         The STYLESHEET resource which is received from resource watcher.
   * @return {Promise}
   *         A promise that resolves to the style sheet's editor when the style sheet has
   *         been fully loaded.  If the style sheet has a source map, and source mapping
   *         is enabled, then the promise resolves to null.
   */
  _addStyleSheet: function(resource) {
    if (!this._seenSheets.has(resource)) {
      const promise = (async () => {
        let editor = await this._addStyleSheetEditor(resource);

        const sourceMapService = this._toolbox.sourceMapService;

        if (
          !sourceMapService ||
          !Services.prefs.getBoolPref(PREF_ORIG_SOURCES)
        ) {
          return editor;
        }

        const {
          href,
          nodeHref,
          resourceId: id,
          sourceMapURL,
          sourceMapBaseURL,
        } = resource;
        const sources = await sourceMapService.getOriginalURLs({
          id,
          url: href || nodeHref,
          sourceMapBaseURL,
          sourceMapURL,
        });
        // A single generated sheet might map to multiple original
        // sheets, so make editors for each of them.
        if (sources && sources.length) {
          const parentEditorName = editor.friendlyName;
          this._removeStyleSheetEditor(editor);
          editor = null;

          for (const { id: originalId, url: originalURL } of sources) {
            const original = new OriginalSource(
              originalURL,
              originalId,
              sourceMapService
            );

            // set so the first sheet will be selected, even if it's a source
            original.styleSheetIndex = resource.styleSheetIndex;
            original.relatedStyleSheet = resource;
            original.relatedEditorName = parentEditorName;
            original.resourceId = resource.resourceId;
            original.targetFront = resource.targetFront;
            original.mediaRules = resource.mediaRules;
            await this._addStyleSheetEditor(original);
          }
        }

        return editor;
      })();
      this._seenSheets.set(resource, promise);
    }
    return this._seenSheets.get(resource);
  },

  _getInlineStyleSheetsCount() {
    return this.editors.filter(editor => !editor.styleSheet.href).length;
  },

  _getNewStyleSheetsCount() {
    return this.editors.filter(editor => editor.isNew).length;
  },

  /**
   * Finds the index to be shown in the Style Editor for inline or
   * user-created style sheets, returns undefined if not of either type.
   *
   * @param {StyleSheet}  styleSheet
   *        Object representing stylesheet
   * @return {(Number|undefined)}
   *         Optional Integer representing the index of the current stylesheet
   *         among all stylesheets of its type (inline or user-created)
   */
  _getNextFriendlyIndex: function(styleSheet) {
    if (styleSheet.href) {
      return undefined;
    }

    return styleSheet.isNew
      ? this._getNewStyleSheetsCount()
      : this._getInlineStyleSheetsCount();
  },

  /**
   * Add a new editor to the UI for a source.
   *
   * @param  {Resource} resource
   *         The resource which is received from resource watcher.
   * @return {Promise} that is resolved with the created StyleSheetEditor when
   *                   the editor is fully initialized or rejected on error.
   */
  async _addStyleSheetEditor(resource) {
    const editor = new StyleSheetEditor(
      resource,
      this._window,
      this._walker,
      this._highlighter,
      this._getNextFriendlyIndex(resource)
    );

    editor.on("property-change", this._summaryChange.bind(this, editor));
    editor.on("media-rules-changed", this._updateMediaList.bind(this, editor));
    editor.on("linked-css-file", this._summaryChange.bind(this, editor));
    editor.on("linked-css-file-error", this._summaryChange.bind(this, editor));
    editor.on("error", this._onError);

    // onMediaRulesChanged fires media-rules-changed, so call the function after
    // registering the listener in order to ensure to get media-rules-changed event.
    editor.onMediaRulesChanged(resource.mediaRules);

    this.editors.push(editor);

    await editor.fetchSource();
    this._sourceLoaded(editor);

    if (resource.fileName) {
      this.emit("test:editor-updated", editor);
    }

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
    const onFileSelected = selectedFile => {
      if (!selectedFile) {
        // nothing selected
        return;
      }
      NetUtil.asyncFetch(
        {
          uri: NetUtil.newURI(selectedFile),
          loadingNode: this._window.document,
          securityFlags:
            Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_INHERITS_SEC_CONTEXT,
          contentPolicyType: Ci.nsIContentPolicy.TYPE_OTHER,
        },
        async (stream, status) => {
          if (!Components.isSuccessCode(status)) {
            this.emit("error", { key: LOAD_ERROR, level: "warning" });
            return;
          }
          const source = NetUtil.readInputStreamToString(
            stream,
            stream.available()
          );
          stream.close();

          const stylesheetsFront = await this.currentTarget.getFront(
            "stylesheets"
          );
          stylesheetsFront.addStyleSheet(source, selectedFile.path);
        }
      );
    };

    showFilePicker(file, false, parentWindow, onFileSelected);
  },

  /**
   * Forward any error from a stylesheet.
   *
   * @param  {data} data
   *         The event data
   */
  _onError: function(data) {
    this.emit("error", data);
  },

  /**
   * Toggle the original sources pref.
   */
  _toggleOrigSources: function() {
    const isEnabled = Services.prefs.getBoolPref(PREF_ORIG_SOURCES);
    Services.prefs.setBoolPref(PREF_ORIG_SOURCES, !isEnabled);
  },

  /**
   * Toggle the pref for showing a @media rules sidebar in each editor.
   */
  _toggleMediaSidebar: function() {
    const isEnabled = Services.prefs.getBoolPref(PREF_MEDIA_SIDEBAR);
    Services.prefs.setBoolPref(PREF_MEDIA_SIDEBAR, !isEnabled);
  },

  /**
   * Toggle the @media sidebar in each editor depending on the setting.
   */
  _onMediaPrefChanged: function() {
    this.editors.forEach(this._updateMediaList);
  },

  /**
   * This method handles the following cases related to the context
   * menu items "_openLinkNewTabItem" and "_copyUrlItem":
   *
   * 1) There was a stylesheet clicked on and it is external: show and
   * enable the context menu item
   * 2) There was a stylesheet clicked on and it is inline: show and
   * disable the context menu item
   * 3) There was no stylesheet clicked on (the right click happened
   * below the list): hide the context menu
   */
  _updateContextMenuItems: function() {
    this._openLinkNewTabItem.hidden = !this._contextMenuStyleSheet;
    this._copyUrlItem.hidden = !this._contextMenuStyleSheet;

    if (this._contextMenuStyleSheet) {
      this._openLinkNewTabItem.setAttribute(
        "disabled",
        !this._contextMenuStyleSheet.href
      );
      this._copyUrlItem.setAttribute(
        "disabled",
        !this._contextMenuStyleSheet.href
      );
    }
  },

  /**
   * Open a particular stylesheet in a new tab.
   */
  _openLinkNewTab: function() {
    if (this._contextMenuStyleSheet) {
      openContentLink(this._contextMenuStyleSheet.href);
    }
  },

  /**
   * Copies a stylesheet's URL.
   */
  _copyUrl: function() {
    if (this._contextMenuStyleSheet) {
      copyString(this._contextMenuStyleSheet.href);
    }
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
    } else {
      const self = this;
      this.on("editor-added", function onAdd(added) {
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
  _clearStyleSheetEditors: function() {
    for (const editor of this.editors) {
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
    let ordinal = editor.styleSheet.styleSheetIndex;
    ordinal = ordinal == -1 ? Number.MAX_SAFE_INTEGER : ordinal;
    // add new sidebar item and editor to the UI
    this._view.appendTemplatedItem(STYLE_EDITOR_TEMPLATE, {
      data: {
        editor: editor,
      },
      disableAnimations: this._alwaysDisableAnimations,
      ordinal: ordinal,
      onCreate: (summary, details, data) => {
        const createdEditor = data.editor;
        createdEditor.summary = summary;
        createdEditor.details = details;

        wire(summary, ".stylesheet-enabled", function onToggleDisabled(event) {
          event.stopPropagation();
          event.target.blur();

          createdEditor.toggleDisabled();
        });

        wire(summary, ".stylesheet-name", {
          events: {
            keypress: event => {
              if (event.keyCode == KeyCodes.DOM_VK_RETURN) {
                this._view.activeSummary = summary;
              }
            },
          },
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

        const sidebar = details.querySelector(".stylesheet-sidebar");
        sidebar.setAttribute(
          "width",
          Services.prefs.getIntPref(PREF_SIDEBAR_WIDTH)
        );

        const splitter = details.querySelector(".devtools-side-splitter");
        splitter.addEventListener("mousemove", () => {
          const sidebarWidth = sidebar.getAttribute("width");
          Services.prefs.setIntPref(PREF_SIDEBAR_WIDTH, sidebarWidth);

          // update all @media sidebars for consistency
          const sidebars = [
            ...this._panelDoc.querySelectorAll(".stylesheet-sidebar"),
          ];
          for (const mediaSidebar of sidebars) {
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
        if (
          !this.selectedEditor &&
          !this._styleSheetBoundToSelect &&
          createdEditor.styleSheet.styleSheetIndex == 0
        ) {
          this._selectEditor(createdEditor);
        }
        this.emit("editor-added", createdEditor);
      },

      onShow: (summary, details, data) => {
        const showEditor = data.editor;
        this.selectedEditor = showEditor;

        (async function() {
          if (!showEditor.sourceEditor) {
            // only initialize source editor when we switch to this view
            const inputElement = details.querySelector(
              ".stylesheet-editor-input"
            );
            await showEditor.load(inputElement, this._cssProperties);
          }

          showEditor.onShow();

          this.emit("editor-selected", showEditor);
        }
          .bind(this)()
          .catch(console.error));
      },
    });
  },

  /**
   * Switch to the editor that has been marked to be selected.
   *
   * @return {Promise}
   *         Promise that will resolve when the editor is selected.
   */
  switchToSelectedSheet: function() {
    const toSelect = this._styleSheetToSelect;

    for (const editor of this.editors) {
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
  _isEditorToSelect: function(editor) {
    const toSelect = this._styleSheetToSelect;
    if (!toSelect) {
      return false;
    }
    const isHref =
      toSelect.stylesheet === null || typeof toSelect.stylesheet == "string";

    return (
      (isHref && editor.styleSheet.href == toSelect.stylesheet) ||
      toSelect.stylesheet == editor.styleSheet
    );
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
  _selectEditor: function(editor, line, col) {
    line = line || 0;
    col = col || 0;

    const editorPromise = editor.getSourceEditor().then(() => {
      editor.setCursor(line, col);
      this._styleSheetBoundToSelect = null;
    });

    const summaryPromise = this.getEditorSummary(editor).then(summary => {
      this._view.activeSummary = summary;
    });

    return Promise.all([editorPromise, summaryPromise]);
  },

  getEditorSummary: function(editor) {
    const self = this;

    if (editor.summary) {
      return Promise.resolve(editor.summary);
    }

    return new Promise(resolve => {
      this.on("editor-added", function onAdd(selected) {
        if (selected == editor) {
          self.off("editor-added", onAdd);
          resolve(editor.summary);
        }
      });
    });
  },

  getEditorDetails: function(editor) {
    const self = this;

    if (editor.details) {
      return Promise.resolve(editor.details);
    }

    return new Promise(resolve => {
      this.on("editor-added", function onAdd(selected) {
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
  getStyleSheetIdentifier: function(styleSheet) {
    // Identify inline style sheets by their host page URI and index
    // at the page.
    return styleSheet.href
      ? styleSheet.href
      : "inline-" + styleSheet.styleSheetIndex + "-at-" + styleSheet.nodeHref;
  },

  /**
   * Get the OriginalSource object for a given original sourceId returned from
   * the sourcemap worker service.
   *
   * @param {string} sourceId
   *        The ID to search for from the sourcemap worker.
   *
   * @return {OriginalSource | null}
   */
  getOriginalSourceSheet: function(sourceId) {
    for (const editor of this.editors) {
      const { styleSheet } = editor;
      if (styleSheet.isOriginalSource && styleSheet.sourceId === sourceId) {
        return styleSheet;
      }
    }
    return null;
  },

  /**
   * Given an URL, find a stylesheet front with that URL, if one has been
   * loaded into the editor.js
   *
   * Do not use this unless you have no other way to get a StyleSheetFront
   * multiple sheets could share the same URL, so this will give you _one_
   * of possibly many sheets with that URL.
   *
   * @param {string} url
   *        An arbitrary URL to search for.
   *
   * @return {StyleSheetFront|null}
   */
  getStylesheetFrontForGeneratedURL: function(url) {
    for (const styleSheet of this._seenSheets.keys()) {
      const sheetURL = styleSheet.href || styleSheet.nodeHref;
      if (!styleSheet.isOriginalSource && sheetURL === url) {
        return styleSheet;
      }
    }
    return null;
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
  selectStyleSheet: function(stylesheet, line, col) {
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
  _summaryChange: function(editor) {
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
  _updateSummaryForEditor: function(editor, summary) {
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

    const flags = [];
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

    const label = summary.querySelector(".stylesheet-name > label");
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
    text(
      summary,
      ".stylesheet-rule-count",
      PluralForm.get(ruleCount, getString("ruleCount.label")).replace(
        "#1",
        ruleCount
      )
    );
  },

  /**
   * Update the @media rules sidebar for an editor. Hide if there are no rules
   * Display a list of the @media rules in the editor's associated style sheet.
   * Emits a 'media-list-changed' event after updating the UI.
   *
   * @param  {StyleSheetEditor} editor
   *         Editor to update @media sidebar of
   */
  _updateMediaList: function(editor) {
    (async function() {
      const details = await this.getEditorDetails(editor);
      const list = details.querySelector(".stylesheet-media-list");

      while (list.firstChild) {
        list.firstChild.remove();
      }

      const rules = editor.mediaRules;
      const showSidebar = Services.prefs.getBoolPref(PREF_MEDIA_SIDEBAR);
      const sidebar = details.querySelector(".stylesheet-sidebar");

      let inSource = false;

      for (const rule of rules) {
        const { line, column, parentStyleSheet } = rule;

        let location = {
          line: line,
          column: column,
          source: editor.styleSheet.href,
          styleSheet: parentStyleSheet,
        };
        if (editor.styleSheet.isOriginalSource) {
          const styleSheet = editor.cssSheet;
          location = await editor.styleSheet.getOriginalLocation(
            styleSheet,
            line,
            column
          );
        }

        // this @media rule is from a different original source
        if (location.source != editor.styleSheet.href) {
          continue;
        }
        inSource = true;

        const div = this._panelDoc.createElementNS(HTML_NS, "div");
        div.className = "media-rule-label";
        div.addEventListener(
          "click",
          this._jumpToLocation.bind(this, location)
        );

        const cond = this._panelDoc.createElementNS(HTML_NS, "div");
        cond.className = "media-rule-condition";
        if (!rule.matches) {
          cond.classList.add("media-condition-unmatched");
        }
        if (this._toolbox.descriptorFront.isLocalTab) {
          this._setConditionContents(cond, rule.conditionText);
        } else {
          cond.textContent = rule.conditionText;
        }
        div.appendChild(cond);

        const link = this._panelDoc.createElementNS(HTML_NS, "div");
        link.className = "media-rule-line theme-link";
        if (location.line != -1) {
          link.textContent = ":" + location.line;
        }
        div.appendChild(link);

        list.appendChild(div);
      }

      sidebar.hidden = !showSidebar || !inSource;

      this.emit("media-list-changed", editor);
    }
      .bind(this)()
      .catch(console.error));
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
    const minMaxPattern = /(min\-|max\-)(width|height):\s\d+(px)/gi;

    let match = minMaxPattern.exec(rawText);
    let lastParsed = 0;
    while (match && match.index != minMaxPattern.lastIndex) {
      const matchEnd = match.index + match[0].length;
      const node = this._panelDoc.createTextNode(
        rawText.substring(lastParsed, match.index)
      );
      element.appendChild(node);

      const link = this._panelDoc.createElementNS(HTML_NS, "a");
      link.href = "#";
      link.className = "media-responsive-mode-toggle";
      link.textContent = rawText.substring(match.index, matchEnd);
      link.addEventListener("click", this._onMediaConditionClick.bind(this));
      element.appendChild(link);

      match = minMaxPattern.exec(rawText);
      lastParsed = matchEnd;
    }

    const node = this._panelDoc.createTextNode(
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
  _onMediaConditionClick: function(e) {
    const conditionText = e.target.textContent;
    const isWidthCond = conditionText.toLowerCase().indexOf("width") > -1;
    const mediaVal = parseInt(/\d+/.exec(conditionText), 10);

    const options = isWidthCond ? { width: mediaVal } : { height: mediaVal };
    this._launchResponsiveMode(options);
    e.preventDefault();
    e.stopPropagation();
  },

  /**
   * Launches the responsive mode with a specific width or height.
   *
   * @param  {object} options
   *         Object with width or/and height properties.
   */
  async _launchResponsiveMode(options = {}) {
    const tab = this.currentTarget.localTab;
    const win = this.currentTarget.localTab.ownerDocument.defaultView;

    await ResponsiveUIManager.openIfNeeded(win, tab, {
      trigger: "style_editor",
    });
    this.emit("responsive-mode-opened");

    ResponsiveUIManager.getResponsiveUIForTab(tab).setViewportSize(options);
  },

  /**
   * Jump cursor to the editor for a stylesheet and line number for a rule.
   *
   * @param  {object} location
   *         Location object with 'line', 'column', and 'source' properties.
   */
  _jumpToLocation: function(location) {
    const source = location.styleSheet || location.source;
    this.selectStyleSheet(source, location.line - 1, location.column - 1);
  },

  _startLoadingStyleSheets() {
    this._root.classList.add("loading");
    this._loadingStyleSheets = [];
  },

  async _waitForLoadingStyleSheets() {
    while (this._loadingStyleSheets?.length > 0) {
      const pending = this._loadingStyleSheets;
      this._loadingStyleSheets = [];
      await Promise.all(pending);
    }

    this._loadingStyleSheets = null;
    this._root.classList.remove("loading");
  },

  async _handleStyleSheetResource(resource) {
    try {
      // The fileName is in resource means this stylesheet was imported from file by user.
      const { fileName } = resource;
      let file = fileName ? new FileUtils.File(fileName) : null;

      // recall location of saved file for this sheet after page reload
      if (!file) {
        const identifier = this.getStyleSheetIdentifier(resource);
        const savedFile = this.savedLocations[identifier];
        if (savedFile) {
          file = savedFile;
        }
      }
      resource.file = file;

      await this._addStyleSheet(resource);
    } catch (e) {
      console.error(e);
      this.emit("error", { key: LOAD_ERROR, level: "warning" });
    }
  },

  async _onResourceAvailable(resources) {
    const promises = [];
    for (const resource of resources) {
      if (
        resource.resourceType === this._toolbox.resourceWatcher.TYPES.STYLESHEET
      ) {
        const onStyleSheetHandled = this._handleStyleSheetResource(resource);

        if (this._loadingStyleSheets) {
          // In case of reloading/navigating and panel's opening
          this._loadingStyleSheets.push(onStyleSheetHandled);
        }
        promises.push(onStyleSheetHandled);
        continue;
      }

      if (!resource.targetFront.isTopLevel) {
        continue;
      }

      if (resource.name === "dom-loading") {
        // will-navigate doesn't work when we navigate to a new process,
        // and for now, onTargetAvailable/onTargetDestroyed doesn't fire on navigation and
        // only when navigating to another process.
        // So we fallback on DOCUMENT_EVENTS to be notified when we navigates.
        // The only catch is that when starting up a DevToolsServer, a dom-loading event
        // is fired, and it might come _after_ the stylesheet resources; and in such case
        // we shouldn't clear the stylesheets that where already handled.
        // So here we bail out if we get this kind of event. At the moment, this will only
        // happen for the initial target and for target switching (and luckily, in those
        // case onTargetAvailable is called before we get any stylesheets).
        // This means we still want to clear the UI on dom-loading for same-process navigation.
        // (We would probably revisit that in bug 1632141)
        if (resource.shouldBeIgnoredAsRedundantWithTargetAvailable) {
          continue;
        }

        this._startLoadingStyleSheets();
        this._clear();
      } else if (resource.name === "dom-complete") {
        promises.push(this._waitForLoadingStyleSheets());
      }
    }
    await Promise.all(promises);
  },

  async _onResourceUpdated(updates) {
    for (const { resource, update } of updates) {
      if (
        update.resourceType === this._toolbox.resourceWatcher.TYPES.STYLESHEET
      ) {
        const editor = this.editors.find(
          e => e.resourceId === update.resourceId
        );

        switch (update.updateType) {
          case "style-applied": {
            editor.onStyleApplied(update);
            break;
          }
          case "property-change": {
            for (const [property, value] of Object.entries(
              update.resourceUpdates
            )) {
              editor.onPropertyChange(property, value);
            }
            break;
          }
          case "media-rules-changed":
          case "matches-change": {
            const { mediaRules } = resource;
            editor.onMediaRulesChanged(mediaRules);
            break;
          }
        }
      }
    }
  },

  async _onTargetAvailable({ targetFront }) {
    if (targetFront.isTopLevel) {
      this._startLoadingStyleSheets();
      this._clear();
      await this.initializeHighlighter(targetFront);
    }
  },

  destroy: function() {
    this._commands.targetCommand.unwatchTargets(
      [this._commands.targetCommand.TYPES.FRAME],
      this._onTargetAvailable
    );

    this._toolbox.resourceWatcher.unwatchResources(
      [
        this._toolbox.resourceWatcher.TYPES.DOCUMENT_EVENT,
        this._toolbox.resourceWatcher.TYPES.STYLESHEET,
      ],
      {
        onAvailable: this._onResourceAvailable,
        onUpdated: this._onResourceUpdated,
      }
    );

    this._clearStyleSheetEditors();

    this._seenSheets = null;

    const sidebar = this._panelDoc.querySelector(".splitview-controller");
    const sidebarWidth = sidebar.getAttribute("width");
    Services.prefs.setIntPref(PREF_NAV_WIDTH, sidebarWidth);

    this._sourceMapPrefObserver.off(
      PREF_ORIG_SOURCES,
      this._onOrigSourcesPrefChanged
    );
    this._sourceMapPrefObserver.destroy();
    this._prefObserver.off(PREF_MEDIA_SIDEBAR, this._onMediaPrefChanged);
    this._prefObserver.destroy();
  },
};
