/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["StyleEditorUI"];

const { loader, require } = ChromeUtils.import(
  "resource://devtools/shared/loader/Loader.jsm"
);
const Services = require("Services");
const EventEmitter = require("devtools/shared/event-emitter");
const {
  getString,
  text,
  showFilePicker,
  optionsPopupMenu,
} = require("resource://devtools/client/styleeditor/StyleEditorUtil.jsm");
const {
  StyleSheetEditor,
} = require("resource://devtools/client/styleeditor/StyleSheetEditor.jsm");
const { PluralForm } = require("devtools/shared/plural-form");
const { PrefObserver } = require("devtools/client/shared/prefs");

const KeyShortcuts = require("devtools/client/shared/key-shortcuts");

const lazy = {};

loader.lazyRequireGetter(
  lazy,
  "KeyCodes",
  "devtools/client/shared/keycodes",
  true
);

loader.lazyRequireGetter(
  lazy,
  "OriginalSource",
  "devtools/client/styleeditor/original-source",
  true
);

loader.lazyRequireGetter(
  lazy,
  "FileUtils",
  "resource://gre/modules/FileUtils.jsm",
  true
);
loader.lazyRequireGetter(
  lazy,
  "NetUtil",
  "resource://gre/modules/NetUtil.jsm",
  true
);
loader.lazyRequireGetter(
  lazy,
  "ResponsiveUIManager",
  "devtools/client/responsive/manager"
);
loader.lazyRequireGetter(
  lazy,
  "openContentLink",
  "devtools/client/shared/link",
  true
);
loader.lazyRequireGetter(
  lazy,
  "copyString",
  "devtools/shared/platform/clipboard",
  true
);

const LOAD_ERROR = "error-load";
const PREF_MEDIA_SIDEBAR = "devtools.styleeditor.showMediaSidebar";
const PREF_SIDEBAR_WIDTH = "devtools.styleeditor.mediaSidebarWidth";
const PREF_NAV_WIDTH = "devtools.styleeditor.navSidebarWidth";
const PREF_ORIG_SOURCES = "devtools.source-map.client-service.enabled";

const FILTERED_CLASSNAME = "splitview-filtered";
const ALL_FILTERED_CLASSNAME = "splitview-all-filtered";

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
 */
class StyleEditorUI extends EventEmitter {
  #activeSummary = null;
  #commands;
  #contextMenu;
  #contextMenuStyleSheet;
  #copyUrlItem;
  #cssProperties;
  #filter;
  #filterInput;
  #filterInputClearButton;
  #loadingStyleSheets;
  #nav;
  #openLinkNewTabItem;
  #optionsButton;
  #optionsMenu;
  #panelDoc;
  #prefObserver;
  #root;
  #seenSheets = new Map();
  #shortcuts;
  #side;
  #sourceMapPrefObserver;
  #styleSheetBoundToSelect;
  #styleSheetToSelect;
  /**
   * Maps keyed by summary element whose value is an object containing:
   * - {Element} details: The associated details element (i.e. container for CodeMirror)
   * - {StyleSheetEditor} editor: The associated editor, for easy retrieval
   */
  #summaryDataMap = new WeakMap();
  #toolbox;
  #tplDetails;
  #tplSummary;
  #uiAbortController = new AbortController();
  #window;

  /**
   * @param {Toolbox} toolbox
   * @param {Object} commands Object defined from devtools/shared/commands to interact with the devtools backend
   * @param {Document} panelDoc
   *        Document of the toolbox panel to populate UI in.
   * @param {CssProperties} A css properties database.
   */
  constructor(toolbox, commands, panelDoc, cssProperties) {
    super();

    this.#toolbox = toolbox;
    this.#commands = commands;
    this.#panelDoc = panelDoc;
    this.#cssProperties = cssProperties;
    this.#window = this.#panelDoc.defaultView;
    this.#root = this.#panelDoc.getElementById("style-editor-chrome");

    this.editors = [];
    this.selectedEditor = null;
    this.savedLocations = {};

    this.#prefObserver = new PrefObserver("devtools.styleeditor.");
    this.#prefObserver.on(PREF_MEDIA_SIDEBAR, this.#onMediaPrefChanged);
    this.#sourceMapPrefObserver = new PrefObserver(
      "devtools.source-map.client-service."
    );
    this.#sourceMapPrefObserver.on(
      PREF_ORIG_SOURCES,
      this.#onOrigSourcesPrefChanged
    );
  }

  get cssProperties() {
    return this.#cssProperties;
  }

  get currentTarget() {
    return this.#commands.targetCommand.targetFront;
  }

  /*
   * Index of selected stylesheet in document.styleSheets
   */
  get selectedStyleSheetIndex() {
    return this.selectedEditor
      ? this.selectedEditor.styleSheet.styleSheetIndex
      : -1;
  }

  /**
   * Initiates the style editor ui creation, and start to track TargetCommand updates.
   *
   * @params {Object} options
   * @params {Object} options.stylesheetToSelect
   * @params {StyleSheetResource} options.stylesheetToSelect.stylesheet
   * @params {Integer} options.stylesheetToSelect.line
   * @params {Integer} options.stylesheetToSelect.column
   */
  async initialize(options = {}) {
    this.createUI();

    if (options.stylesheetToSelect) {
      const { stylesheet, line, column } = options.stylesheetToSelect;
      // If a stylesheet resource and its location was passed (e.g. user clicked on a stylesheet
      // location in the rule view), we can directly add it to the list and select it
      // before watching for resources, for improved performance.
      if (stylesheet.resourceId) {
        try {
          await this.#handleStyleSheetResource(stylesheet);
          await this.selectStyleSheet(
            stylesheet,
            line - 1,
            column ? column - 1 : 0
          );
        } catch (e) {
          console.error(e);
        }
      }
    }

    await this.#toolbox.resourceCommand.watchResources(
      [this.#toolbox.resourceCommand.TYPES.DOCUMENT_EVENT],
      { onAvailable: this.#onResourceAvailable }
    );
    await this.#commands.targetCommand.watchTargets({
      types: [this.#commands.targetCommand.TYPES.FRAME],
      onAvailable: this.#onTargetAvailable,
      onDestroyed: this.#onTargetDestroyed,
    });

    this.#startLoadingStyleSheets();
    await this.#toolbox.resourceCommand.watchResources(
      [this.#toolbox.resourceCommand.TYPES.STYLESHEET],
      {
        onAvailable: this.#onResourceAvailable,
        onUpdated: this.#onResourceUpdated,
      }
    );
    await this.#waitForLoadingStyleSheets();
  }

  /**
   * Build the initial UI and wire buttons with event handlers.
   */
  createUI() {
    this.#filterInput = this.#root.querySelector(".devtools-filterinput");
    this.#filterInputClearButton = this.#root.querySelector(
      ".devtools-searchinput-clear"
    );
    this.#nav = this.#root.querySelector(".splitview-nav");
    this.#side = this.#root.querySelector(".splitview-side-details");
    this.#tplSummary = this.#root.querySelector(
      "#splitview-tpl-summary-stylesheet"
    );
    this.#tplDetails = this.#root.querySelector(
      "#splitview-tpl-details-stylesheet"
    );

    const eventListenersConfig = { signal: this.#uiAbortController.signal };

    // Add click event on the "new stylesheet" button in the toolbar and on the
    // "append a new stylesheet" link (visible when there are no stylesheets).
    for (const el of this.#root.querySelectorAll(".style-editor-newButton")) {
      el.addEventListener(
        "click",
        async () => {
          const stylesheetsFront = await this.currentTarget.getFront(
            "stylesheets"
          );
          stylesheetsFront.addStyleSheet(null);
          this.#clearFilterInput();
        },
        eventListenersConfig
      );
    }

    this.#root.querySelector(".style-editor-importButton").addEventListener(
      "click",
      () => {
        this.#importFromFile(this._mockImportFile || null, this.#window);
        this.#clearFilterInput();
      },
      eventListenersConfig
    );

    this.#root
      .querySelector("#style-editor-options")
      .addEventListener(
        "click",
        this.#onOptionsButtonClick,
        eventListenersConfig
      );

    this.#filterInput.addEventListener(
      "input",
      this.#onFilterInputChange,
      eventListenersConfig
    );

    this.#filterInputClearButton.addEventListener(
      "click",
      () => this.#clearFilterInput(),
      eventListenersConfig
    );

    this.#panelDoc.addEventListener(
      "contextmenu",
      () => {
        this.#contextMenuStyleSheet = null;
      },
      { ...eventListenersConfig, capture: true }
    );

    this.#optionsButton = this.#panelDoc.getElementById("style-editor-options");

    this.#contextMenu = this.#panelDoc.getElementById("sidebar-context");
    this.#contextMenu.addEventListener(
      "popupshowing",
      this.#updateContextMenuItems,
      eventListenersConfig
    );

    this.#openLinkNewTabItem = this.#panelDoc.getElementById(
      "context-openlinknewtab"
    );
    this.#openLinkNewTabItem.addEventListener(
      "command",
      this.#openLinkNewTab,
      eventListenersConfig
    );

    this.#copyUrlItem = this.#panelDoc.getElementById("context-copyurl");
    this.#copyUrlItem.addEventListener(
      "command",
      this.#copyUrl,
      eventListenersConfig
    );

    // items list focus and search-on-type handling
    this.#nav.addEventListener(
      "keydown",
      this.#onNavKeyDown,
      eventListenersConfig
    );

    this.#shortcuts = new KeyShortcuts({
      window: this.#window,
    });
    this.#shortcuts.on(
      `CmdOrCtrl+${getString("focusFilterInput.commandkey")}`,
      this.#onFocusFilterInputKeyboardShortcut
    );

    const nav = this.#panelDoc.querySelector(".splitview-controller");
    nav.setAttribute("width", Services.prefs.getIntPref(PREF_NAV_WIDTH));
  }

  #clearFilterInput() {
    this.#filterInput.value = "";
    this.#onFilterInputChange();
  }

  #onFilterInputChange = () => {
    this.#filter = this.#filterInput.value;
    this.#filterInputClearButton.toggleAttribute("hidden", !this.#filter);

    for (const summary of this.#nav.childNodes) {
      // Don't update nav class for every element, we do it after the loop.
      this.handleSummaryVisibility(summary, {
        triggerOnFilterStateChange: false,
      });
    }

    this.#onFilterStateChange();

    if (this.#activeSummary == null) {
      const firstVisibleSummary = Array.from(this.#nav.childNodes).find(
        node => !node.classList.contains(FILTERED_CLASSNAME)
      );

      if (firstVisibleSummary) {
        this.setActiveSummary(firstVisibleSummary, { reason: "filter-auto" });
      }
    }
  };

  #onFilterStateChange() {
    const summaries = Array.from(this.#nav.childNodes);
    const hasVisibleSummary = summaries.some(
      node => !node.classList.contains(FILTERED_CLASSNAME)
    );
    const allFiltered = summaries.length > 0 && !hasVisibleSummary;

    this.#nav.classList.toggle(ALL_FILTERED_CLASSNAME, allFiltered);

    this.#filterInput
      .closest(".devtools-searchbox")
      .classList.toggle("devtools-searchbox-no-match", !!allFiltered);
  }

  #onFocusFilterInputKeyboardShortcut = e => {
    // Prevent the print modal to be displayed.
    if (e) {
      e.stopPropagation();
      e.preventDefault();
    }
    this.#filterInput.select();
  };

  #onNavKeyDown = event => {
    function getFocusedItemWithin(nav) {
      let node = nav.ownerDocument.activeElement;
      while (node && node.parentNode != nav) {
        node = node.parentNode;
      }
      return node;
    }

    // do not steal focus from inside iframes or textboxes
    if (
      event.target.ownerDocument != this.#nav.ownerDocument ||
      event.target.tagName == "input" ||
      event.target.tagName == "textarea" ||
      event.target.classList.contains("textbox")
    ) {
      return false;
    }

    // handle keyboard navigation within the items list
    const visibleElements = Array.from(
      this.#nav.querySelectorAll(`li:not(.${FILTERED_CLASSNAME})`)
    );
    // Elements have a different visual order (due to the use of MozBoxOrdinalGroup), so
    // we need to sort them by their data-ordinal attribute
    visibleElements.sort(
      (a, b) => a.getAttribute("data-ordinal") - b.getAttribute("data-ordinal")
    );

    let elementToFocus;
    if (
      event.keyCode == lazy.KeyCodes.DOM_VK_PAGE_UP ||
      event.keyCode == lazy.KeyCodes.DOM_VK_HOME
    ) {
      elementToFocus = visibleElements[0];
    } else if (
      event.keyCode == lazy.KeyCodes.DOM_VK_PAGE_DOWN ||
      event.keyCode == lazy.KeyCodes.DOM_VK_END
    ) {
      elementToFocus = visibleElements.at(-1);
    } else if (event.keyCode == lazy.KeyCodes.DOM_VK_UP) {
      const focusedIndex = visibleElements.indexOf(
        getFocusedItemWithin(this.#nav)
      );
      elementToFocus = visibleElements[focusedIndex - 1];
    } else if (event.keyCode == lazy.KeyCodes.DOM_VK_DOWN) {
      const focusedIndex = visibleElements.indexOf(
        getFocusedItemWithin(this.#nav)
      );
      elementToFocus = visibleElements[focusedIndex + 1];
    }

    if (elementToFocus !== undefined) {
      event.stopPropagation();
      event.preventDefault();
      elementToFocus.focus();
      return false;
    }

    return true;
  };

  /**
   * Opens the Options Popup Menu
   *
   * @params {number} screenX
   * @params {number} screenY
   *   Both obtained from the event object, used to position the popup
   */
  #onOptionsButtonClick = ({ screenX, screenY }) => {
    this.#optionsMenu = optionsPopupMenu(
      this.#toggleOrigSources,
      this.#toggleMediaSidebar
    );

    this.#optionsMenu.once("open", () => {
      this.#optionsButton.setAttribute("open", true);
    });
    this.#optionsMenu.once("close", () => {
      this.#optionsButton.removeAttribute("open");
    });

    this.#optionsMenu.popup(screenX, screenY, this.#toolbox.doc);
  };

  /**
   * Be called when changing the original sources pref.
   */
  #onOrigSourcesPrefChanged = async () => {
    this.#clear();
    // When we toggle the source-map preference, we clear the panel and re-fetch the exact
    // same stylesheet resources from ResourceCommand, but `_addStyleSheet` will trigger
    // or ignore the additional source-map mapping.
    this.#root.classList.add("loading");
    for (const resource of this.#toolbox.resourceCommand.getAllResources(
      this.#toolbox.resourceCommand.TYPES.STYLESHEET
    )) {
      await this.#handleStyleSheetResource(resource);
    }

    this.#root.classList.remove("loading");

    this.emit("stylesheets-refreshed");
  };

  /**
   * Remove all editors and add loading indicator.
   */
  #clear = () => {
    // remember selected sheet and line number for next load
    if (this.selectedEditor && this.selectedEditor.sourceEditor) {
      const href = this.selectedEditor.styleSheet.href;
      const { line, ch } = this.selectedEditor.sourceEditor.getCursor();

      this.#styleSheetToSelect = {
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

    this.#clearStyleSheetEditors();
    // Clear the left sidebar items and their associated elements.
    while (this.#nav.hasChildNodes()) {
      this.removeSplitViewItem(this.#nav.firstChild);
    }

    this.selectedEditor = null;
    // Here the keys are style sheet actors, and the values are
    // promises that resolve to the sheet's editor.  See |_addStyleSheet|.
    this.#seenSheets = new Map();

    this.emit("stylesheets-clear");
  };

  /**
   * Add an editor for this stylesheet. Add editors for its original sources
   * instead (e.g. Sass sources), if applicable.
   *
   * @param  {Resource} resource
   *         The STYLESHEET resource which is received from resource command.
   * @return {Promise}
   *         A promise that resolves to the style sheet's editor when the style sheet has
   *         been fully loaded.  If the style sheet has a source map, and source mapping
   *         is enabled, then the promise resolves to null.
   */
  #addStyleSheet(resource) {
    if (!this.#seenSheets.has(resource)) {
      const promise = (async () => {
        let editor = await this.#addStyleSheetEditor(resource);

        const sourceMapService = this.#toolbox.sourceMapService;

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
          this.#removeStyleSheetEditor(editor);
          editor = null;

          for (const { id: originalId, url: originalURL } of sources) {
            const original = new lazy.OriginalSource(
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
            await this.#addStyleSheetEditor(original);
          }
        }

        return editor;
      })();
      this.#seenSheets.set(resource, promise);
    }
    return this.#seenSheets.get(resource);
  }

  #removeStyleSheet(resource, editor) {
    this.#seenSheets.delete(resource);
    this.#removeStyleSheetEditor(editor);
  }

  #getInlineStyleSheetsCount() {
    return this.editors.filter(editor => !editor.styleSheet.href).length;
  }

  #getNewStyleSheetsCount() {
    return this.editors.filter(editor => editor.isNew).length;
  }

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
  #getNextFriendlyIndex(styleSheet) {
    if (styleSheet.href) {
      return undefined;
    }

    return styleSheet.isNew
      ? this.#getNewStyleSheetsCount()
      : this.#getInlineStyleSheetsCount();
  }

  /**
   * Add a new editor to the UI for a source.
   *
   * @param  {Resource} resource
   *         The resource which is received from resource command.
   * @return {Promise} that is resolved with the created StyleSheetEditor when
   *                   the editor is fully initialized or rejected on error.
   */
  async #addStyleSheetEditor(resource) {
    const editor = new StyleSheetEditor(
      resource,
      this.#window,
      this.#getNextFriendlyIndex(resource)
    );

    editor.on("property-change", this.#summaryChange.bind(this, editor));
    editor.on("media-rules-changed", this.#updateMediaList.bind(this, editor));
    editor.on("linked-css-file", this.#summaryChange.bind(this, editor));
    editor.on("linked-css-file-error", this.#summaryChange.bind(this, editor));
    editor.on("error", this.#onError);
    editor.on(
      "filter-input-keyboard-shortcut",
      this.#onFocusFilterInputKeyboardShortcut
    );

    // onMediaRulesChanged fires media-rules-changed, so call the function after
    // registering the listener in order to ensure to get media-rules-changed event.
    editor.onMediaRulesChanged(resource.mediaRules);

    this.editors.push(editor);

    try {
      await editor.fetchSource();
    } catch (e) {
      // if the editor was destroyed while fetching dependencies, we don't want to go further.
      if (!this.editors.includes(editor)) {
        return null;
      }
      throw e;
    }

    this.#sourceLoaded(editor);

    if (resource.fileName) {
      this.emit("test:editor-updated", editor);
    }

    return editor;
  }

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
  #importFromFile(file, parentWindow) {
    const onFileSelected = selectedFile => {
      if (!selectedFile) {
        // nothing selected
        return;
      }
      lazy.NetUtil.asyncFetch(
        {
          uri: lazy.NetUtil.newURI(selectedFile),
          loadingNode: this.#window.document,
          securityFlags:
            Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_INHERITS_SEC_CONTEXT,
          contentPolicyType: Ci.nsIContentPolicy.TYPE_OTHER,
        },
        async (stream, status) => {
          if (!Components.isSuccessCode(status)) {
            this.emit("error", { key: LOAD_ERROR, level: "warning" });
            return;
          }
          const source = lazy.NetUtil.readInputStreamToString(
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
  }

  /**
   * Forward any error from a stylesheet.
   *
   * @param  {data} data
   *         The event data
   */
  #onError = data => {
    this.emit("error", data);
  };

  /**
   * Toggle the original sources pref.
   */
  #toggleOrigSources() {
    const isEnabled = Services.prefs.getBoolPref(PREF_ORIG_SOURCES);
    Services.prefs.setBoolPref(PREF_ORIG_SOURCES, !isEnabled);
  }

  /**
   * Toggle the pref for showing a @media rules sidebar in each editor.
   */
  #toggleMediaSidebar() {
    const isEnabled = Services.prefs.getBoolPref(PREF_MEDIA_SIDEBAR);
    Services.prefs.setBoolPref(PREF_MEDIA_SIDEBAR, !isEnabled);
  }

  /**
   * Toggle the @media sidebar in each editor depending on the setting.
   */
  #onMediaPrefChanged = () => {
    this.editors.forEach(this.#updateMediaList);
  };

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
  #updateContextMenuItems = async () => {
    this.#openLinkNewTabItem.hidden = !this.#contextMenuStyleSheet;
    this.#copyUrlItem.hidden = !this.#contextMenuStyleSheet;

    if (this.#contextMenuStyleSheet) {
      this.#openLinkNewTabItem.setAttribute(
        "disabled",
        !this.#contextMenuStyleSheet.href
      );
      this.#copyUrlItem.setAttribute(
        "disabled",
        !this.#contextMenuStyleSheet.href
      );
    }
  };

  /**
   * Open a particular stylesheet in a new tab.
   */
  #openLinkNewTab = () => {
    if (this.#contextMenuStyleSheet) {
      lazy.openContentLink(this.#contextMenuStyleSheet.href);
    }
  };

  /**
   * Copies a stylesheet's URL.
   */
  #copyUrl = () => {
    if (this.#contextMenuStyleSheet) {
      lazy.copyString(this.#contextMenuStyleSheet.href);
    }
  };

  /**
   * Remove a particular stylesheet editor from the UI
   *
   * @param {StyleSheetEditor}  editor
   *        The editor to remove.
   */
  #removeStyleSheetEditor(editor) {
    if (editor.summary) {
      this.removeSplitViewItem(editor.summary);
    } else {
      const self = this;
      this.on("editor-added", function onAdd(added) {
        if (editor == added) {
          self.off("editor-added", onAdd);
          self.removeSplitViewItem(editor.summary);
        }
      });
    }

    editor.destroy();
    this.editors.splice(this.editors.indexOf(editor), 1);
  }

  /**
   * Clear all the editors from the UI.
   */
  #clearStyleSheetEditors() {
    for (const editor of this.editors) {
      editor.destroy();
    }
    this.editors = [];
  }

  /**
   * Called when a StyleSheetEditor's source has been fetched.
   * Add new sidebar item and editor to the UI
   *
   * @param  {StyleSheetEditor} editor
   *         Editor to create UI for.
   */
  #sourceLoaded(editor) {
    // Create the detail and summary nodes from the templates node (declared in index.xhtml)
    const details = this.#tplDetails.cloneNode(true);
    details.id = "";
    const summary = this.#tplSummary.cloneNode(true);
    summary.id = "";

    let ordinal = editor.styleSheet.styleSheetIndex;
    ordinal = ordinal == -1 ? Number.MAX_SAFE_INTEGER : ordinal;
    summary.style.MozBoxOrdinalGroup = ordinal;
    summary.setAttribute("data-ordinal", ordinal);

    this.#nav.appendChild(summary);
    this.#side.appendChild(details);

    this.#summaryDataMap.set(summary, {
      details,
      editor,
    });

    const createdEditor = editor;
    createdEditor.summary = summary;
    createdEditor.details = details;

    const eventListenersConfig = { signal: this.#uiAbortController.signal };

    summary.addEventListener(
      "click",
      event => {
        event.stopPropagation();
        this.setActiveSummary(summary);
      },
      eventListenersConfig
    );

    summary.querySelector(".stylesheet-enabled").addEventListener(
      "click",
      event => {
        event.stopPropagation();
        event.target.blur();

        createdEditor.toggleDisabled();
      },
      eventListenersConfig
    );

    summary.querySelector(".stylesheet-name").addEventListener(
      "keypress",
      event => {
        if (event.keyCode == lazy.KeyCodes.DOM_VK_RETURN) {
          this.setActiveSummary(summary);
        }
      },
      eventListenersConfig
    );

    summary.querySelector(".stylesheet-saveButton").addEventListener(
      "click",
      event => {
        event.stopPropagation();
        event.target.blur();

        createdEditor.saveToFile(createdEditor.savedFile);
      },
      eventListenersConfig
    );

    this.#updateSummaryForEditor(createdEditor, summary);

    summary.addEventListener(
      "contextmenu",
      () => {
        this.#contextMenuStyleSheet = createdEditor.styleSheet;
      },
      eventListenersConfig
    );

    summary.addEventListener(
      "focus",
      function onSummaryFocus(event) {
        if (event.target == summary) {
          // autofocus the stylesheet name
          summary.querySelector(".stylesheet-name").focus();
        }
      },
      eventListenersConfig
    );

    const sidebar = details.querySelector(".stylesheet-sidebar");
    sidebar.setAttribute(
      "width",
      Services.prefs.getIntPref(PREF_SIDEBAR_WIDTH)
    );

    const splitter = details.querySelector(".devtools-side-splitter");
    splitter.addEventListener(
      "mousemove",
      () => {
        const sidebarWidth = sidebar.getAttribute("width");
        Services.prefs.setIntPref(PREF_SIDEBAR_WIDTH, sidebarWidth);

        // update all @media sidebars for consistency
        const sidebars = [
          ...this.#panelDoc.querySelectorAll(".stylesheet-sidebar"),
        ];
        for (const mediaSidebar of sidebars) {
          mediaSidebar.setAttribute("width", sidebarWidth);
        }
      },
      eventListenersConfig
    );

    // autofocus if it's a new user-created stylesheet
    if (createdEditor.isNew) {
      this.#selectEditor(createdEditor);
    }

    if (this.#isEditorToSelect(createdEditor)) {
      this.switchToSelectedSheet();
    }

    // If this is the first stylesheet and there is no pending request to
    // select a particular style sheet, select this sheet.
    if (
      !this.selectedEditor &&
      !this.#styleSheetBoundToSelect &&
      createdEditor.styleSheet.styleSheetIndex == 0 &&
      !summary.classList.contains(FILTERED_CLASSNAME)
    ) {
      this.#selectEditor(createdEditor);
    }
    this.emit("editor-added", createdEditor);
  }

  /**
   * Switch to the editor that has been marked to be selected.
   *
   * @return {Promise}
   *         Promise that will resolve when the editor is selected.
   */
  switchToSelectedSheet() {
    const toSelect = this.#styleSheetToSelect;

    for (const editor of this.editors) {
      if (this.#isEditorToSelect(editor)) {
        // The _styleSheetBoundToSelect will always hold the latest pending
        // requested style sheet (with line and column) which is not yet
        // selected by the source editor. Only after we select that particular
        // editor and go the required line and column, it will become null.
        this.#styleSheetBoundToSelect = this.#styleSheetToSelect;
        this.#styleSheetToSelect = null;
        return this.#selectEditor(editor, toSelect.line, toSelect.col);
      }
    }

    return Promise.resolve();
  }

  /**
   * Returns whether a given editor is the current editor to be selected. Tests
   * based on href or underlying stylesheet.
   *
   * @param {StyleSheetEditor} editor
   *        The editor to test.
   */
  #isEditorToSelect(editor) {
    const toSelect = this.#styleSheetToSelect;
    if (!toSelect) {
      return false;
    }
    const isHref =
      toSelect.stylesheet === null || typeof toSelect.stylesheet == "string";

    return (
      (isHref && editor.styleSheet.href == toSelect.stylesheet) ||
      toSelect.stylesheet == editor.styleSheet
    );
  }

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
  #selectEditor(editor, line = null, col = null) {
    // Don't go further if the editor was destroyed in the meantime
    if (!this.editors.includes(editor)) {
      return null;
    }

    const editorPromise = editor.getSourceEditor().then(() => {
      // line/col are null when the style editor is initialized and the first stylesheet
      // editor is selected. Unfortunately, this function might be called also when the
      // panel is opened from clicking on a CSS warning in the WebConsole panel, in which
      // case we have specific line+col.
      // There's no guarantee which one could be called first, and it happened that we
      // were setting the cursor once for the correct line coming from the webconsole,
      // and then re-setting it to the default value (which was <0,0>).
      // To avoid the race, we simply don't explicitly set the cursor to any default value,
      // which is not a big deal as CodeMirror does init it to <0,0> anyway.
      // See Bug 1738124 for more information.
      if (line !== null || col !== null) {
        editor.setCursor(line, col);
      }
      this.#styleSheetBoundToSelect = null;
    });

    const summaryPromise = this.getEditorSummary(editor).then(summary => {
      // Don't go further if the editor was destroyed in the meantime
      if (!this.editors.includes(editor)) {
        throw new Error("Editor was destroyed");
      }
      this.setActiveSummary(summary);
    });

    return Promise.all([editorPromise, summaryPromise]);
  }

  getEditorSummary(editor) {
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
  }

  getEditorDetails(editor) {
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
  }

  /**
   * Returns an identifier for the given style sheet.
   *
   * @param {StyleSheet} styleSheet
   *        The style sheet to be identified.
   */
  getStyleSheetIdentifier(styleSheet) {
    // Identify inline style sheets by their host page URI and index
    // at the page.
    return styleSheet.href
      ? styleSheet.href
      : "inline-" + styleSheet.styleSheetIndex + "-at-" + styleSheet.nodeHref;
  }

  /**
   * Get the OriginalSource object for a given original sourceId returned from
   * the sourcemap worker service.
   *
   * @param {string} sourceId
   *        The ID to search for from the sourcemap worker.
   *
   * @return {OriginalSource | null}
   */
  getOriginalSourceSheet(sourceId) {
    for (const editor of this.editors) {
      const { styleSheet } = editor;
      if (styleSheet.isOriginalSource && styleSheet.sourceId === sourceId) {
        return styleSheet;
      }
    }
    return null;
  }

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
  getStylesheetFrontForGeneratedURL(url) {
    for (const styleSheet of this.#seenSheets.keys()) {
      const sheetURL = styleSheet.href || styleSheet.nodeHref;
      if (!styleSheet.isOriginalSource && sheetURL === url) {
        return styleSheet;
      }
    }
    return null;
  }

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
  selectStyleSheet(stylesheet, line, col) {
    this.#styleSheetToSelect = {
      stylesheet: stylesheet,
      line: line,
      col: col,
    };

    /* Switch to the editor for this sheet, if it exists yet.
       Otherwise each editor will be checked when it's created. */
    return this.switchToSelectedSheet();
  }

  /**
   * Handler for an editor's 'property-changed' event.
   * Update the summary in the UI.
   *
   * @param  {StyleSheetEditor} editor
   *         Editor for which a property has changed
   */
  #summaryChange(editor) {
    this.#updateSummaryForEditor(editor);
  }

  /**
   * Update split view summary of given StyleEditor instance.
   *
   * @param {StyleSheetEditor} editor
   * @param {DOMElement} summary
   *        Optional item's summary element to update. If none, item
   *        corresponding to passed editor is used.
   */
  #updateSummaryForEditor(editor, summary) {
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

    summary.classList.toggle("disabled", !!editor.styleSheet.disabled);
    summary.classList.toggle("unsaved", !!editor.unsaved);
    summary.classList.toggle("linked-file-error", !!editor.linkedCSSFileError);

    const label = summary.querySelector(".stylesheet-name > label");
    label.setAttribute("value", editor.friendlyName);
    if (editor.styleSheet.href) {
      label.setAttribute("tooltiptext", editor.styleSheet.href);
    }

    let linkedCSSSource = "";
    if (editor.linkedCSSFile) {
      linkedCSSSource = PathUtils.filename(editor.linkedCSSFile);
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

    // We may need to change the summary visibility as a result of the changes.
    this.handleSummaryVisibility(summary);
  }

  /**
   * Update the @media rules sidebar for an editor. Hide if there are no rules
   * Display a list of the @media rules in the editor's associated style sheet.
   * Emits a 'media-list-changed' event after updating the UI.
   *
   * @param  {StyleSheetEditor} editor
   *         Editor to update @media sidebar of
   */
  #updateMediaList = editor => {
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

        const div = this.#panelDoc.createElementNS(HTML_NS, "div");
        div.className = "media-rule-label";
        div.addEventListener(
          "click",
          this.#jumpToLocation.bind(this, location)
        );

        const cond = this.#panelDoc.createElementNS(HTML_NS, "div");
        cond.className = "media-rule-condition";
        if (!rule.matches) {
          cond.classList.add("media-condition-unmatched");
        }
        if (this.#toolbox.descriptorFront.isLocalTab) {
          this.#setConditionContents(cond, rule.conditionText);
        } else {
          cond.textContent = rule.conditionText;
        }
        div.appendChild(cond);

        const link = this.#panelDoc.createElementNS(HTML_NS, "div");
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
  };

  /**
   * Used to safely inject media query links
   *
   * @param {HTMLElement} element
   *        The element corresponding to the media sidebar condition
   * @param {String} rawText
   *        The raw condition text to parse
   */
  #setConditionContents(element, rawText) {
    const minMaxPattern = /(min\-|max\-)(width|height):\s\d+(px)/gi;

    let match = minMaxPattern.exec(rawText);
    let lastParsed = 0;
    while (match && match.index != minMaxPattern.lastIndex) {
      const matchEnd = match.index + match[0].length;
      const node = this.#panelDoc.createTextNode(
        rawText.substring(lastParsed, match.index)
      );
      element.appendChild(node);

      const link = this.#panelDoc.createElementNS(HTML_NS, "a");
      link.href = "#";
      link.className = "media-responsive-mode-toggle";
      link.textContent = rawText.substring(match.index, matchEnd);
      link.addEventListener("click", this.#onMediaConditionClick.bind(this));
      element.appendChild(link);

      match = minMaxPattern.exec(rawText);
      lastParsed = matchEnd;
    }

    const node = this.#panelDoc.createTextNode(
      rawText.substring(lastParsed, rawText.length)
    );
    element.appendChild(node);
  }

  /**
   * Called when a media condition is clicked
   * If a responsive mode link is clicked, it will launch it.
   *
   * @param {object} e
   *        Event object
   */
  #onMediaConditionClick(e) {
    const conditionText = e.target.textContent;
    const isWidthCond = conditionText.toLowerCase().indexOf("width") > -1;
    const mediaVal = parseInt(/\d+/.exec(conditionText), 10);

    const options = isWidthCond ? { width: mediaVal } : { height: mediaVal };
    this.#launchResponsiveMode(options);
    e.preventDefault();
    e.stopPropagation();
  }

  /**
   * Launches the responsive mode with a specific width or height.
   *
   * @param  {object} options
   *         Object with width or/and height properties.
   */
  async #launchResponsiveMode(options = {}) {
    const tab = this.currentTarget.localTab;
    const win = this.currentTarget.localTab.ownerDocument.defaultView;

    await lazy.ResponsiveUIManager.openIfNeeded(win, tab, {
      trigger: "style_editor",
    });
    this.emit("responsive-mode-opened");

    lazy.ResponsiveUIManager.getResponsiveUIForTab(tab).setViewportSize(
      options
    );
  }

  /**
   * Jump cursor to the editor for a stylesheet and line number for a rule.
   *
   * @param  {object} location
   *         Location object with 'line', 'column', and 'source' properties.
   */
  #jumpToLocation(location) {
    const source = location.styleSheet || location.source;
    this.selectStyleSheet(source, location.line - 1, location.column - 1);
  }

  #startLoadingStyleSheets() {
    this.#root.classList.add("loading");
    this.#loadingStyleSheets = [];
  }

  async #waitForLoadingStyleSheets() {
    while (this.#loadingStyleSheets?.length > 0) {
      const pending = this.#loadingStyleSheets;
      this.#loadingStyleSheets = [];
      await Promise.all(pending);
    }

    this.#loadingStyleSheets = null;
    this.#root.classList.remove("loading");
  }

  async #handleStyleSheetResource(resource) {
    try {
      // The fileName is in resource means this stylesheet was imported from file by user.
      const { fileName } = resource;
      let file = fileName ? new lazy.FileUtils.File(fileName) : null;

      // recall location of saved file for this sheet after page reload
      if (!file) {
        const identifier = this.getStyleSheetIdentifier(resource);
        const savedFile = this.savedLocations[identifier];
        if (savedFile) {
          file = savedFile;
        }
      }
      resource.file = file;

      await this.#addStyleSheet(resource);
    } catch (e) {
      console.error(e);
      this.emit("error", { key: LOAD_ERROR, level: "warning" });
    }
  }

  // onAvailable is a mandatory argument for watchTargets,
  // but we don't do anything when a new target gets created.
  #onTargetAvailable = ({ targetFront }) => {};

  #onTargetDestroyed = ({ targetFront }) => {
    // Iterate over a copy of the list in order to prevent skipping
    // over some items when removing items of this list
    const editorsCopy = [...this.editors];
    for (const editor of editorsCopy) {
      const { styleSheet } = editor;
      if (styleSheet.targetFront == targetFront) {
        this.#removeStyleSheet(styleSheet, editor);
      }
    }
  };

  #onResourceAvailable = async resources => {
    const promises = [];
    for (const resource of resources) {
      if (
        resource.resourceType === this.#toolbox.resourceCommand.TYPES.STYLESHEET
      ) {
        const onStyleSheetHandled = this.#handleStyleSheetResource(resource);

        if (this.#loadingStyleSheets) {
          // In case of reloading/navigating and panel's opening
          this.#loadingStyleSheets.push(onStyleSheetHandled);
        }
        promises.push(onStyleSheetHandled);
        continue;
      }

      if (!resource.targetFront.isTopLevel) {
        continue;
      }

      if (resource.name === "will-navigate") {
        this.#startLoadingStyleSheets();
        this.#clear();
      } else if (resource.name === "dom-complete") {
        promises.push(this.#waitForLoadingStyleSheets());
      }
    }
    await Promise.all(promises);
  };

  #onResourceUpdated = async updates => {
    for (const { resource, update } of updates) {
      if (
        update.resourceType === this.#toolbox.resourceCommand.TYPES.STYLESHEET
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
  };

  /**
   * Set the active item's summary element.
   *
   * @param DOMElement summary
   * @param {Object} options
   * @param {String=} options.reason: Indicates why the summary was selected. It's set to
   *                  "filter-auto" when the summary was automatically selected as the result
   *                  of the previous active summary being filtered out.
   */
  setActiveSummary(summary, options = {}) {
    if (summary == this.#activeSummary) {
      return;
    }

    if (this.#activeSummary) {
      const binding = this.#summaryDataMap.get(this.#activeSummary);

      this.#activeSummary.classList.remove("splitview-active");
      binding.details.classList.remove("splitview-active");
    }

    this.#activeSummary = summary;
    if (!summary) {
      this.selectedEditor = null;
      return;
    }

    const { details } = this.#summaryDataMap.get(summary);
    summary.classList.add("splitview-active");
    details.classList.add("splitview-active");

    this.showSummaryEditor(summary, options);
  }

  /**
   * Show summary's associated editor
   *
   * @param DOMElement summary
   * @param {Object} options
   * @param {String=} options.reason: Indicates why the summary was selected. It's set to
   *                  "filter-auto" when the summary was automatically selected as the result
   *                  of the previous active summary being filtered out.
   */
  async showSummaryEditor(summary, options) {
    const { details, editor } = this.#summaryDataMap.get(summary);
    this.selectedEditor = editor;

    try {
      if (!editor.sourceEditor) {
        // only initialize source editor when we switch to this view
        const inputElement = details.querySelector(".stylesheet-editor-input");
        await editor.load(inputElement, this.#cssProperties);
      }

      editor.onShow(options);

      this.emit("editor-selected", editor);
    } catch (e) {
      console.error(e);
    }
  }

  /**
   * Remove an item from the split view.
   *
   * @param DOMElement summary
   *        Summary element of the item to remove.
   */
  removeSplitViewItem(summary) {
    if (summary == this.#activeSummary) {
      this.setActiveSummary(null);
    }

    const data = this.#summaryDataMap.get(summary);
    if (!data) {
      return;
    }

    summary.remove();
    data.details.remove();
  }

  /**
   * Make the passed element visible or not, depending if it matches the current filter
   *
   * @param {Element} summary
   * @param {Object} options
   * @param {Boolean} options.triggerOnFilterStateChange: Set to false to avoid calling
   *                  #onFilterStateChange directly here. This can be useful when this
   *                  function is called for every item of the list, like in `setFilter`.
   */
  handleSummaryVisibility(summary, { triggerOnFilterStateChange = true } = {}) {
    if (!this.#filter) {
      summary.classList.remove(FILTERED_CLASSNAME);
      return;
    }

    const label = summary.querySelector(".stylesheet-name label");
    const itemText = label.value.toLowerCase();
    const matchesSearch = itemText.includes(this.#filter.toLowerCase());
    summary.classList.toggle(FILTERED_CLASSNAME, !matchesSearch);

    if (this.#activeSummary == summary && !matchesSearch) {
      this.setActiveSummary(null);
    }

    if (triggerOnFilterStateChange) {
      this.#onFilterStateChange();
    }
  }

  destroy() {
    this.#toolbox.resourceCommand.unwatchResources(
      [
        this.#toolbox.resourceCommand.TYPES.DOCUMENT_EVENT,
        this.#toolbox.resourceCommand.TYPES.STYLESHEET,
      ],
      {
        onAvailable: this.#onResourceAvailable,
        onUpdated: this.#onResourceUpdated,
      }
    );
    this.#commands.targetCommand.unwatchTargets({
      types: [this.#commands.targetCommand.TYPES.FRAME],
      onAvailable: this.#onTargetAvailable,
      onDestroyed: this.#onTargetDestroyed,
    });

    if (this.#uiAbortController) {
      this.#uiAbortController.abort();
      this.#uiAbortController = null;
    }
    this.#clearStyleSheetEditors();

    this.#seenSheets = null;
    this.#filterInput = null;
    this.#filterInputClearButton = null;
    this.#nav = null;
    this.#side = null;
    this.#tplDetails = null;
    this.#tplSummary = null;

    const sidebar = this.#panelDoc.querySelector(".splitview-controller");
    const sidebarWidth = sidebar.getAttribute("width");
    Services.prefs.setIntPref(PREF_NAV_WIDTH, sidebarWidth);

    if (this.#sourceMapPrefObserver) {
      this.#sourceMapPrefObserver.off(
        PREF_ORIG_SOURCES,
        this.#onOrigSourcesPrefChanged
      );
      this.#sourceMapPrefObserver.destroy();
      this.#sourceMapPrefObserver = null;
    }

    if (this.#prefObserver) {
      this.#prefObserver.off(PREF_MEDIA_SIDEBAR, this.#onMediaPrefChanged);
      this.#prefObserver.destroy();
      this.#prefObserver = null;
    }

    if (this.#shortcuts) {
      this.#shortcuts.destroy();
      this.#shortcuts = null;
    }
  }
}
