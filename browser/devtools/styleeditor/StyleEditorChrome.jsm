/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["StyleEditorChrome"];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/PluralForm.jsm");
Cu.import("resource:///modules/devtools/StyleEditor.jsm");
Cu.import("resource:///modules/devtools/StyleEditorUtil.jsm");
Cu.import("resource:///modules/devtools/SplitView.jsm");
Cu.import("resource://gre/modules/commonjs/sdk/core/promise.js");

const STYLE_EDITOR_TEMPLATE = "stylesheet";


/**
 * StyleEditorChrome constructor.
 *
 * The 'chrome' of the Style Editor is all the around the actual editor (textbox).
 * Manages the sheet selector, history, and opened editor(s) for the attached
 * content window.
 *
 * @param DOMElement aRoot
 *        Element that owns the chrome UI.
 * @param DOMWindow aContentWindow
 *        Content DOMWindow to attach to this chrome.
 */
this.StyleEditorChrome = function StyleEditorChrome(aRoot, aContentWindow)
{
  assert(aRoot, "Argument 'aRoot' is required to initialize StyleEditorChrome.");

  this._root = aRoot;
  this._document = this._root.ownerDocument;
  this._window = this._document.defaultView;

  this._editors = [];
  this._listeners = []; // @see addChromeListener

  // Store the content window so that we can call the real contentWindow setter
  // in the open method.
  this._contentWindowTemp = aContentWindow;

  this._contentWindow = null;
}

StyleEditorChrome.prototype = {
  _styleSheetToSelect: null,

  open: function() {
    let deferred = Promise.defer();
    let initializeUI = function (aEvent) {
      if (aEvent) {
        this._window.removeEventListener("load", initializeUI, false);
      }
      let viewRoot = this._root.parentNode.querySelector(".splitview-root");
      this._view = new SplitView(viewRoot);
      this._setupChrome();

      // We need to juggle arount the contentWindow items because we need to
      // trigger the setter at the appropriate time.
      this.contentWindow = this._contentWindowTemp; // calls setter
      this._contentWindowTemp = null;

      deferred.resolve();
    }.bind(this);

    if (this._document.readyState == "complete") {
      initializeUI();
    } else {
      this._window.addEventListener("load", initializeUI, false);
    }

    return deferred.promise;
  },

  /**
   * Retrieve the content window attached to this chrome.
   *
   * @return DOMWindow
   *         Content window or null if no content window is attached.
   */
  get contentWindow() this._contentWindow,

  /**
   * Retrieve the ID of the content window attached to this chrome.
   *
   * @return number
   *         Window ID or -1 if no content window is attached.
   */
  get contentWindowID()
  {
    try {
      return this._contentWindow.QueryInterface(Ci.nsIInterfaceRequestor).
        getInterface(Ci.nsIDOMWindowUtils).currentInnerWindowID;
    } catch (ex) {
      return -1;
    }
  },

  /**
   * Set the content window attached to this chrome.
   * Content attach or detach events/notifications are triggered after the
   * operation is complete (possibly asynchronous if the content is not fully
   * loaded yet).
   *
   * @param DOMWindow aContentWindow
   * @see addChromeListener
   */
  set contentWindow(aContentWindow)
  {
    if (this._contentWindow == aContentWindow) {
      return; // no change
    }

    this._contentWindow = aContentWindow;

    if (!aContentWindow) {
      this._disableChrome();
      return;
    }

    let onContentUnload = function () {
      aContentWindow.removeEventListener("unload", onContentUnload, false);
      if (this.contentWindow == aContentWindow) {
        this.contentWindow = null; // detach
      }
    }.bind(this);
    aContentWindow.addEventListener("unload", onContentUnload, false);

    if (aContentWindow.document.readyState == "complete") {
      this._root.classList.remove("loading");
      this._populateChrome();
      return;
    } else {
      this._root.classList.add("loading");
      let onContentReady = function () {
        aContentWindow.removeEventListener("load", onContentReady, false);
        this._root.classList.remove("loading");
        this._populateChrome();
      }.bind(this);
      aContentWindow.addEventListener("load", onContentReady, false);
    }
  },

  /**
   * Retrieve the content document attached to this chrome.
   *
   * @return DOMDocument
   */
  get contentDocument()
  {
    return this._contentWindow ? this._contentWindow.document : null;
  },

  /**
   * Retrieve an array with the StyleEditor instance for each live style sheet,
   * ordered by style sheet index.
   *
   * @return Array<StyleEditor>
   */
  get editors()
  {
    let editors = [];
    this._editors.forEach(function (aEditor) {
      if (aEditor.styleSheetIndex >= 0) {
        editors[aEditor.styleSheetIndex] = aEditor;
      }
    });
    return editors;
  },

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
    return this.editors.some(function(editor) {
      return editor.sourceEditor && editor.sourceEditor.dirty;
    });
  },

  /*
   * Mark the style editor as having unsaved changes.
   */
  markDirty: function SEC_markDirty() {
    this._markedDirty = true;
  },

  /**
   * Add a listener for StyleEditorChrome events.
   *
   * The listener implements IStyleEditorChromeListener := {
   *   onContentDetach:        Called when the content window has been detached.
   *                           Arguments: (StyleEditorChrome aChrome)
   *                           @see contentWindow
   *
   *   onEditorAdded:          Called when a stylesheet (therefore a StyleEditor
   *                           instance) has been added to the UI.
   *                           Arguments (StyleEditorChrome aChrome,
   *                                      StyleEditor aEditor)
   * }
   *
   * All listener methods are optional.
   *
   * @param IStyleEditorChromeListener aListener
   * @see removeChromeListener
   */
  addChromeListener: function SEC_addChromeListener(aListener)
  {
    this._listeners.push(aListener);
  },

  /**
   * Remove a listener for Chrome events from the current list of listeners.
   *
   * @param IStyleEditorChromeListener aListener
   * @see addChromeListener
   */
  removeChromeListener: function SEC_removeChromeListener(aListener)
  {
    let index = this._listeners.indexOf(aListener);
    if (index != -1) {
      this._listeners.splice(index, 1);
    }
  },

  /**
   * Trigger named handlers in StyleEditorChrome listeners.
   *
   * @param string aName
   *        Name of the event to trigger.
   * @param Array aArgs
   *        Optional array of arguments to pass to the listener(s).
   * @see addActionListener
   */
  _triggerChromeListeners: function SE__triggerChromeListeners(aName, aArgs)
  {
    // insert the origin Chrome instance as first argument
    if (!aArgs) {
      aArgs = [this];
    } else {
      aArgs.unshift(this);
    }

    // copy the list of listeners to allow adding/removing listeners in handlers
    let listeners = this._listeners.concat();
    // trigger all listeners that have this named handler.
    for (let i = 0; i < listeners.length; i++) {
      let listener = listeners[i];
      let handler = listener["on" + aName];
      if (handler) {
        handler.apply(listener, aArgs);
      }
    }
  },

  /**
   * Create a new style editor, add to the list of editors, and bind this
   * object as an action listener.
   * @param DOMDocument aDocument
   *        The document that the stylesheet is being referenced in.
   * @param CSSStyleSheet aSheet
   *        Optional stylesheet to edit from the document.
   * @return StyleEditor
   */
  _createStyleEditor: function SEC__createStyleEditor(aDocument, aSheet) {
    let editor = new StyleEditor(aDocument, aSheet);
    this._editors.push(editor);
    editor.addActionListener(this);
    return editor;
  },

  /**
   * Set up the chrome UI. Install event listeners and so on.
   */
  _setupChrome: function SEC__setupChrome()
  {
    // wire up UI elements
    wire(this._view.rootElement, ".style-editor-newButton", function onNewButton() {
      let editor = this._createStyleEditor(this.contentDocument);
      editor.load();
    }.bind(this));

    wire(this._view.rootElement, ".style-editor-importButton", function onImportButton() {
      let editor = this._createStyleEditor(this.contentDocument);
      editor.importFromFile(this._mockImportFile || null, this._window);
    }.bind(this));
  },

  /**
   * Reset the chrome UI to an empty and ready state.
   */
  resetChrome: function SEC__resetChrome()
  {
    this._editors.forEach(function (aEditor) {
      aEditor.removeActionListener(this);
    }.bind(this));
    this._editors = [];

    this._view.removeAll();

    // (re)enable UI
    let matches = this._root.querySelectorAll("toolbarbutton,input,select");
    for (let i = 0; i < matches.length; i++) {
      matches[i].removeAttribute("disabled");
    }
  },

  /**
   * Add all imported stylesheets to chrome UI, recursively
   *
   * @param CSSStyleSheet aSheet
   *        A stylesheet we're going to browse to look for all imported sheets.
   */
  _showImportedStyleSheets: function SEC__showImportedStyleSheets(aSheet)
  {
    let document = this.contentDocument;
    for (let j = 0; j < aSheet.cssRules.length; j++) {
      let rule = aSheet.cssRules.item(j);
      if (rule.type == Ci.nsIDOMCSSRule.IMPORT_RULE) {
        // Associated styleSheet may be null if it has already been seen due to
        // duplicate @imports for the same URL.
        if (!rule.styleSheet) {
          continue;
        }

        this._createStyleEditor(document, rule.styleSheet);

        this._showImportedStyleSheets(rule.styleSheet);
      } else if (rule.type != Ci.nsIDOMCSSRule.CHARSET_RULE) {
        // @import rules must precede all others except @charset
        return;
      }
    }
  },

  /**
   * Populate the chrome UI according to the content document.
   *
   * @see StyleEditor._setupShadowStyleSheet
   */
  _populateChrome: function SEC__populateChrome()
  {
    this.resetChrome();

    let document = this.contentDocument;
    this._document.title = _("chromeWindowTitle",
      document.title || document.location.href);

    for (let i = 0; i < document.styleSheets.length; i++) {
      let styleSheet = document.styleSheets[i];

      this._createStyleEditor(document, styleSheet);

      this._showImportedStyleSheets(styleSheet);
    }

    // Queue editors loading so that ContentAttach is consistently triggered
    // right after all editor instances are available (this.editors) but are
    // NOT loaded/ready yet. This also helps responsivity during loading when
    // there are many heavy stylesheets.
    this._editors.forEach(function (aEditor) {
      this._window.setTimeout(aEditor.load.bind(aEditor), 0);
    }, this);
  },

  /**
   * selects a stylesheet and optionally moves the cursor to a selected line
   *
   * @param {CSSStyleSheet} [aSheet]
   *        Stylesheet that should be selected. If a stylesheet is not passed
   *        and the editor is not initialized we focus the first stylesheet. If
   *        a stylesheet is not passed and the editor is initialized we ignore
   *        the call.
   * @param {Number} [aLine]
   *        Line to which the caret should be moved (one-indexed).
   * @param {Number} [aCol]
   *        Column to which the caret should be moved (one-indexed).
   */
  selectStyleSheet: function SEC_selectSheet(aSheet, aLine, aCol)
  {
    let alreadyCalled = !!this._styleSheetToSelect;

    this._styleSheetToSelect = {
      sheet: aSheet,
      line: aLine,
      col: aCol,
    };

    if (alreadyCalled) {
      return;
    }

    let select = function DEC_select(aEditor) {
      let sheet = this._styleSheetToSelect.sheet;
      let line = this._styleSheetToSelect.line || 1;
      let col = this._styleSheetToSelect.col || 1;

      if (!aEditor.sourceEditor) {
        let onAttach = function SEC_selectSheet_onAttach() {
          aEditor.removeActionListener(this);
          this.selectedStyleSheetIndex = aEditor.styleSheetIndex;
          aEditor.sourceEditor.setCaretPosition(line - 1, col - 1);

          let newSheet = this._styleSheetToSelect.sheet;
          let newLine = this._styleSheetToSelect.line;
          let newCol = this._styleSheetToSelect.col;
          this._styleSheetToSelect = null;
          if (newSheet != sheet) {
              this.selectStyleSheet.bind(this, newSheet, newLine, newCol);
          }
        }.bind(this);

        aEditor.addActionListener({
          onAttach: onAttach
        });
      } else {
        // If a line or column was specified we move the caret appropriately.
        aEditor.sourceEditor.setCaretPosition(line - 1, col - 1);
        this._styleSheetToSelect = null;
      }

        let summary = sheet ? this.getSummaryElementForEditor(aEditor)
                            : this._view.getSummaryElementByOrdinal(0);
        this._view.activeSummary = summary;
      this.selectedStyleSheetIndex = aEditor.styleSheetIndex;
    }.bind(this);

    if (!this.editors.length) {
      // We are in the main initialization phase so we wait for the editor
      // containing the target stylesheet to be added and select the target
      // stylesheet, optionally moving the cursor to a selected line.
      let self = this;
      this.addChromeListener({
        onEditorAdded: function SEC_selectSheet_onEditorAdded(aChrome, aEditor) {
          let sheet = self._styleSheetToSelect.sheet;
          if ((sheet && aEditor.styleSheet == sheet) ||
              (aEditor.styleSheetIndex == 0 && sheet == null)) {
            aChrome.removeChromeListener(this);
            aEditor.addActionListener(self);
            select(aEditor);
          }
        }
      });
    } else if (aSheet) {
      // We are already initialized and a stylesheet has been specified. Here
      // we iterate through the editors and select the one containing the target
      // stylesheet, optionally moving the cursor to a selected line.
      for each (let editor in this.editors) {
        if (editor.styleSheet == aSheet) {
          select(editor);
          break;
        }
      }
    }
  },

  /**
   * Disable all UI, effectively making editors read-only.
   * This is automatically called when no content window is attached.
   *
   * @see contentWindow
   */
  _disableChrome: function SEC__disableChrome()
  {
    let matches = this._root.querySelectorAll("button,toolbarbutton,textbox");
    for (let i = 0; i < matches.length; i++) {
      matches[i].setAttribute("disabled", "disabled");
    }

    this.editors.forEach(function onEnterReadOnlyMode(aEditor) {
      aEditor.readOnly = true;
    });

    this._view.rootElement.setAttribute("disabled", "disabled");

    this._triggerChromeListeners("ContentDetach");
  },

  /**
   * Retrieve the summary element for a given editor.
   *
   * @param StyleEditor aEditor
   * @return DOMElement
   *         Item's summary element or null if not found.
   * @see SplitView
   */
  getSummaryElementForEditor: function SEC_getSummaryElementForEditor(aEditor)
  {
    return this._view.getSummaryElementByOrdinal(aEditor.styleSheetIndex);
  },

  /**
   * Update split view summary of given StyleEditor instance.
   *
   * @param StyleEditor aEditor
   * @param DOMElement aSummary
   *        Optional item's summary element to update. If none, item corresponding
   *        to passed aEditor is used.
   */
  _updateSummaryForEditor: function SEC__updateSummaryForEditor(aEditor, aSummary)
  {
    let summary = aSummary || this.getSummaryElementForEditor(aEditor);
    let ruleCount = aEditor.styleSheet.cssRules.length;

    this._view.setItemClassName(summary, aEditor.flags);

    let label = summary.querySelector(".stylesheet-name > label");
    label.setAttribute("value", aEditor.getFriendlyName());

    text(summary, ".stylesheet-title", aEditor.styleSheet.title || "");
    text(summary, ".stylesheet-rule-count",
      PluralForm.get(ruleCount, _("ruleCount.label")).replace("#1", ruleCount));
    text(summary, ".stylesheet-error-message", aEditor.errorMessage);
  },

  /**
   * IStyleEditorActionListener implementation
   * @See StyleEditor.addActionListener.
   */

  /**
   * Called when source has been loaded and editor is ready for some action.
   *
   * @param StyleEditor aEditor
   */
  onLoad: function SEAL_onLoad(aEditor)
  {
    let item = this._view.appendTemplatedItem(STYLE_EDITOR_TEMPLATE, {
      data: {
        editor: aEditor
      },
      disableAnimations: this._alwaysDisableAnimations,
      ordinal: aEditor.styleSheetIndex,
      onCreate: function ASV_onItemCreate(aSummary, aDetails, aData) {
        let editor = aData.editor;

        wire(aSummary, ".stylesheet-enabled", function onToggleEnabled(aEvent) {
          aEvent.stopPropagation();
          aEvent.target.blur();

          editor.enableStyleSheet(editor.styleSheet.disabled);
        });

        wire(aSummary, ".stylesheet-name", {
          events: {
            "keypress": function onStylesheetNameActivate(aEvent) {
              if (aEvent.keyCode == aEvent.DOM_VK_RETURN) {
                this._view.activeSummary = aSummary;
              }
            }.bind(this)
          }
        });

        wire(aSummary, ".stylesheet-saveButton", function onSaveButton(aEvent) {
          aEvent.stopPropagation();
          aEvent.target.blur();

          editor.saveToFile(editor.savedFile);
        });

        this._updateSummaryForEditor(editor, aSummary);

        aSummary.addEventListener("focus", function onSummaryFocus(aEvent) {
          if (aEvent.target == aSummary) {
            // autofocus the stylesheet name
            aSummary.querySelector(".stylesheet-name").focus();
          }
        }, false);

        // autofocus new stylesheets
        if (editor.hasFlag(StyleEditorFlags.NEW)) {
          this._view.activeSummary = aSummary;
        }

        this._triggerChromeListeners("EditorAdded", [editor]);
      }.bind(this),
      onHide: function ASV_onItemShow(aSummary, aDetails, aData) {
        aData.editor.onHide();
      },
      onShow: function ASV_onItemShow(aSummary, aDetails, aData) {
        let editor = aData.editor;
        if (!editor.inputElement) {
          // attach editor to input element the first time it is shown
          editor.inputElement = aDetails.querySelector(".stylesheet-editor-input");
        }
        editor.onShow();
      }
    });
  },

  /**
   * Called when an editor flag changed.
   *
   * @param StyleEditor aEditor
   * @param string aFlagName
   * @see StyleEditor.flags
   */
  onFlagChange: function SEAL_onFlagChange(aEditor, aFlagName)
  {
    this._updateSummaryForEditor(aEditor);
  },

  /**
   * Called when when changes have been committed/applied to the live DOM
   * stylesheet.
   *
   * @param StyleEditor aEditor
   */
  onCommit: function SEAL_onCommit(aEditor)
  {
    this._updateSummaryForEditor(aEditor);
  },
};
