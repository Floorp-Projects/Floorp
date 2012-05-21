/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["StyleEditorChrome"];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/PluralForm.jsm");
Cu.import("resource:///modules/devtools/StyleEditor.jsm");
Cu.import("resource:///modules/devtools/StyleEditorUtil.jsm");
Cu.import("resource:///modules/devtools/SplitView.jsm");

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
function StyleEditorChrome(aRoot, aContentWindow)
{
  assert(aRoot, "Argument 'aRoot' is required to initialize StyleEditorChrome.");

  this._root = aRoot;
  this._document = this._root.ownerDocument;
  this._window = this._document.defaultView;

  this._editors = [];
  this._listeners = []; // @see addChromeListener

  this._contentWindow = null;
  this._isContentAttached = false;

  let initializeUI = function (aEvent) {
    if (aEvent) {
      this._window.removeEventListener("load", initializeUI, false);
    }

    let viewRoot = this._root.parentNode.querySelector(".splitview-root");
    this._view = new SplitView(viewRoot);

    this._setupChrome();

    // attach to the content window
    this.contentWindow = aContentWindow;
    this._contentWindowID = null;
  }.bind(this);

  if (this._document.readyState == "complete") {
    initializeUI();
  } else {
    this._window.addEventListener("load", initializeUI, false);
  }
}

StyleEditorChrome.prototype = {
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
    * Retrieve whether the content has been attached and StyleEditor instances
    * exist for all of its stylesheets.
    *
    * @return boolean
    * @see addChromeListener
    */
  get isContentAttached() this._isContentAttached,

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
   * Add a listener for StyleEditorChrome events.
   *
   * The listener implements IStyleEditorChromeListener := {
   *   onContentAttach:        Called when a content window has been attached.
   *                           All editors are instantiated, though they might
   *                           not be loaded yet.
   *                           Arguments: (StyleEditorChrome aChrome)
   *                           @see contentWindow
   *                           @see StyleEditor.isLoaded
   *                           @see StyleEditor.addActionListener
   *
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
   * Set up the chrome UI. Install event listeners and so on.
   */
  _setupChrome: function SEC__setupChrome()
  {
    // wire up UI elements
    wire(this._view.rootElement, ".style-editor-newButton", function onNewButton() {
      let editor = new StyleEditor(this.contentDocument);
      this._editors.push(editor);
      editor.addActionListener(this);
      editor.load();
    }.bind(this));

    wire(this._view.rootElement, ".style-editor-importButton", function onImportButton() {
      let editor = new StyleEditor(this.contentDocument);
      this._editors.push(editor);
      editor.addActionListener(this);
      editor.importFromFile(this._mockImportFile || null, this._window);
    }.bind(this));
  },

  /**
   * Reset the chrome UI to an empty and ready state.
   */
  _resetChrome: function SEC__resetChrome()
  {
    this._editors.forEach(function (aEditor) {
      aEditor.removeActionListener(this);
    }.bind(this));
    this._editors = [];

    this._view.removeAll();

    // (re)enable UI
    let matches = this._root.querySelectorAll("toolbarbutton,input,select");
    for (let i = 0; i < matches.length; ++i) {
      matches[i].removeAttribute("disabled");
    }
  },

  /**
   * Populate the chrome UI according to the content document.
   *
   * @see StyleEditor._setupShadowStyleSheet
   */
  _populateChrome: function SEC__populateChrome()
  {
    this._resetChrome();

    let document = this.contentDocument;
    this._document.title = _("chromeWindowTitle",
      document.title || document.location.href);

    for (let i = 0; i < document.styleSheets.length; ++i) {
      let styleSheet = document.styleSheets[i];

      let editor = new StyleEditor(document, styleSheet);
      editor.addActionListener(this);
      this._editors.push(editor);
    }

    this._triggerChromeListeners("ContentAttach");

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
    let select = function DEC_select(aEditor) {
      let summary = aSheet ? this.getSummaryElementForEditor(aEditor)
                           : this._view.getSummaryElementByOrdinal(0);
      let setCaret = false;

      if (aLine || aCol) {
        aLine = aLine || 1;
        aCol = aCol || 1;
        setCaret = true;
      }
      if (!aEditor.sourceEditor) {
        // If a line or column was specified we move the caret appropriately.
        if (setCaret) {
          aEditor.addActionListener({
            onAttach: function SEC_selectSheet_onAttach()
            {
              aEditor.removeActionListener(this);
              aEditor.sourceEditor.setCaretPosition(aLine - 1, aCol - 1);
            }
          });
        }
        this._view.activeSummary = summary;
      } else {
        this._view.activeSummary = summary;

        // If a line or column was specified we move the caret appropriately.
        if (setCaret) {
          aEditor.sourceEditor.setCaretPosition(aLine - 1, aCol - 1);
        }
      }
    }.bind(this);

    if (!this.editors.length) {
      // We are in the main initialization phase so we wait for the editor
      // containing the target stylesheet to be added and select the target
      // stylesheet, optionally moving the cursor to a selected line.
      this.addChromeListener({
        onEditorAdded: function SEC_selectSheet_onEditorAdded(aChrome, aEditor) {
          if ((!aSheet && aEditor.styleSheetIndex == 0) ||
              aEditor.styleSheet == aSheet) {
            aChrome.removeChromeListener(this);
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
    for (let i = 0; i < matches.length; ++i) {
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
