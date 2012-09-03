/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["StyleEditor", "StyleEditorFlags", "StyleEditorManager"];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

const DOMUtils = Cc["@mozilla.org/inspector/dom-utils;1"]
                   .getService(Ci.inIDOMUtils);

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/FileUtils.jsm");
Cu.import("resource:///modules/devtools/StyleEditorUtil.jsm");
Cu.import("resource:///modules/source-editor.jsm");

const LOAD_ERROR = "error-load";
const SAVE_ERROR = "error-save";

// max update frequency in ms (avoid potential typing lag and/or flicker)
// @see StyleEditor.updateStylesheet
const UPDATE_STYLESHEET_THROTTLE_DELAY = 500;

// @see StyleEditor._persistExpando
const STYLESHEET_EXPANDO = "-moz-styleeditor-stylesheet-";

const TRANSITIONS_PREF = "devtools.styleeditor.transitions";

const TRANSITION_CLASS = "moz-styleeditor-transitioning";
const TRANSITION_DURATION_MS = 500;
const TRANSITION_RULE = "\
:root.moz-styleeditor-transitioning, :root.moz-styleeditor-transitioning * {\
transition-duration: " + TRANSITION_DURATION_MS + "ms !important; \
transition-delay: 0ms !important;\
transition-timing-function: ease-out !important;\
transition-property: all !important;\
}";

/**
 * Style Editor module-global preferences
 */
const TRANSITIONS_ENABLED = Services.prefs.getBoolPref(TRANSITIONS_PREF);


/**
 * StyleEditor constructor.
 *
 * The StyleEditor is initialized 'headless', it does not display source
 * or receive input. Setting inputElement attaches a DOMElement to handle this.
 *
 * An editor can be created stand-alone or created by StyleEditorChrome to
 * manage all the style sheets of a document, including @import'ed sheets.
 *
 * @param DOMDocument aDocument
 *        The content document where changes will be applied to.
 * @param DOMStyleSheet aStyleSheet
 *        Optional. The DOMStyleSheet to edit.
 *        If not set, a new empty style sheet will be appended to the document.
 * @see inputElement
 * @see StyleEditorChrome
 */
function StyleEditor(aDocument, aStyleSheet)
{
  assert(aDocument, "Argument 'aDocument' is required.");

  this._document = aDocument; // @see contentDocument
  this._inputElement = null;  // @see inputElement
  this._sourceEditor = null;  // @see sourceEditor

  this._state = {             // state to handle inputElement attach/detach
    text: "",                 // seamlessly
    selection: {start: 0, end: 0},
    readOnly: false,
    topIndex: 0,              // the first visible line
  };

  this._styleSheet = aStyleSheet;
  this._styleSheetIndex = -1; // unknown for now, will be set after load
  this._styleSheetFilePath = null; // original file path for the style sheet

  this._loaded = false;

  this._flags = [];           // @see flags
  this._savedFile = null;     // @see savedFile

  this._errorMessage = null;  // @see errorMessage

  // listeners for significant editor actions. @see addActionListener
  this._actionListeners = [];

  // this is to perform pending updates before editor closing
  this._onWindowUnloadBinding = this._onWindowUnload.bind(this);

  this._transitionRefCount = 0;

  this._focusOnSourceEditorReady = false;
}

StyleEditor.prototype = {
  /**
   * Retrieve the content document this editor will apply changes to.
   *
   * @return DOMDocument
   */
  get contentDocument() this._document,

  /**
   * Retrieve the stylesheet this editor is attached to.
   *
   * @return DOMStyleSheet
   */
  get styleSheet()
  {
    assert(this._styleSheet, "StyleSheet must be loaded first.");
    return this._styleSheet;
  },

  /**
   * Retrieve the index (order) of stylesheet in the document.
   *
   * @return number
   */
  get styleSheetIndex()
  {
    let document = this.contentDocument;
    if (this._styleSheetIndex == -1) {
      for (let i = 0; i < document.styleSheets.length; ++i) {
        if (document.styleSheets[i] == this.styleSheet) {
          this._styleSheetIndex = i;
          break;
        }
      }
    }
    return this._styleSheetIndex;
  },

  /**
   * Retrieve the input element that handles display and input for this editor.
   * Can be null if the editor is detached/headless, which means that this
   * StyleEditor is not attached to an input element.
   *
   * @return DOMElement
   */
  get inputElement() this._inputElement,

  /**
   * Set the input element that handles display and input for this editor.
   * This detaches the previous input element if previously set.
   *
   * @param DOMElement aElement
   */
  set inputElement(aElement)
  {
    if (aElement == this._inputElement) {
      return; // no change
    }

    if (this._inputElement) {
      // detach from current input element
      if (this._sourceEditor) {
        // save existing state first (for seamless reattach)
        this._state = {
          text: this._sourceEditor.getText(),
          selection: this._sourceEditor.getSelection(),
          readOnly: this._sourceEditor.readOnly,
          topIndex: this._sourceEditor.getTopIndex(),
        };
        this._sourceEditor.destroy();
        this._sourceEditor = null;
      }

      this.window.removeEventListener("unload",
                                      this._onWindowUnloadBinding, false);
      this._triggerAction("Detach");
    }

    this._inputElement = aElement;
    if (!aElement) {
      return;
    }

    // attach to new input element
    this.window.addEventListener("unload", this._onWindowUnloadBinding, false);
    this._focusOnSourceEditorReady = false;

    this._sourceEditor = null; // set it only when ready (safe to use)

    let sourceEditor = new SourceEditor();
    let config = {
      initialText: this._state.text,
      showLineNumbers: true,
      mode: SourceEditor.MODES.CSS,
      readOnly: this._state.readOnly,
      keys: this._getKeyBindings()
    };

    sourceEditor.init(aElement, config, function onSourceEditorReady() {
      setupBracketCompletion(sourceEditor);

      sourceEditor.addEventListener(SourceEditor.EVENTS.TEXT_CHANGED,
                                    function onTextChanged(aEvent) {
        this.updateStyleSheet();
      }.bind(this));

      this._sourceEditor = sourceEditor;

      if (this._focusOnSourceEditorReady) {
        this._focusOnSourceEditorReady = false;
        sourceEditor.focus();
      }

      sourceEditor.setTopIndex(this._state.topIndex);
      sourceEditor.setSelection(this._state.selection.start,
                                this._state.selection.end);

      this._triggerAction("Attach");
    }.bind(this));
  },

  /**
   * Retrieve the underlying SourceEditor instance for this StyleEditor.
   * Can be null if not ready or Style Editor is detached/headless.
   *
   * @return SourceEditor
   */
  get sourceEditor() this._sourceEditor,

  /**
   * Setter for the read-only state of the editor.
   *
   * @param boolean aValue
   *        Tells if you want the editor to be read-only or not.
   */
  set readOnly(aValue)
  {
    this._state.readOnly = aValue;
    if (this._sourceEditor) {
      this._sourceEditor.readOnly = aValue;
    }
  },

  /**
   * Getter for the read-only state of the editor.
   *
   * @return boolean
   */
  get readOnly()
  {
    return this._state.readOnly;
  },

  /**
   * Retrieve the window that contains the editor.
   * Can be null if the editor is detached/headless.
   *
   * @return DOMWindow
   */
  get window()
  {
    if (!this.inputElement) {
      return null;
    }
    return this.inputElement.ownerDocument.defaultView;
  },

  /**
   * Retrieve the last file this editor has been saved to or null if none.
   *
   * @return nsIFile
   */
  get savedFile() this._savedFile,

  /**
   * Import style sheet from file and load it into the editor asynchronously.
   * "Load" action triggers when complete.
   *
   * @param mixed aFile
   *        Optional nsIFile or filename string.
   *        If not set a file picker will be shown.
   * @param nsIWindow aParentWindow
   *        Optional parent window for the file picker.
   */
  importFromFile: function SE_importFromFile(aFile, aParentWindow)
  {
    aFile = this._showFilePicker(aFile, false, aParentWindow);
    if (!aFile) {
      return;
    }
    this._savedFile = aFile; // remember filename for next save if any

    NetUtil.asyncFetch(aFile, function onAsyncFetch(aStream, aStatus) {
      if (!Components.isSuccessCode(aStatus)) {
        return this._signalError(LOAD_ERROR);
      }
      let source = NetUtil.readInputStreamToString(aStream, aStream.available());
      aStream.close();

      this._appendNewStyleSheet(source);
      this.clearFlag(StyleEditorFlags.ERROR);
    }.bind(this));
  },

  /**
    * Retrieve localized error message of last error condition, or null if none.
    * This is set when the editor has flag StyleEditorFlags.ERROR.
    *
    * @see addActionListener
    */
  get errorMessage() this._errorMessage,

  /**
   * Tell whether the stylesheet has been loaded and ready for modifications.
   *
   * @return boolean
   */
  get isLoaded() this._loaded,

  /**
   * Load style sheet source into the editor, asynchronously.
   * "Load" handler triggers when complete.
   *
   * @see addActionListener
   */
  load: function SE_load()
  {
    if (!this._styleSheet) {
      this._appendNewStyleSheet();
    }
    this._loadSource();
  },

  /**
   * Get a user-friendly name for the style sheet.
   *
   * @return string
   */
  getFriendlyName: function SE_getFriendlyName()
  {
    if (this.savedFile) { // reuse the saved filename if any
      return this.savedFile.leafName;
    }

    if (this.hasFlag(StyleEditorFlags.NEW)) {
      let index = this.styleSheetIndex + 1; // 0-indexing only works for devs
      return _("newStyleSheet", index);
    }

    if (this.hasFlag(StyleEditorFlags.INLINE)) {
      let index = this.styleSheetIndex + 1; // 0-indexing only works for devs
      return _("inlineStyleSheet", index);
    }

    if (!this._friendlyName) {
      let sheetURI = this.styleSheet.href;
      let contentURI = this.contentDocument.baseURIObject;
      let contentURIScheme = contentURI.scheme;
      let contentURILeafIndex = contentURI.specIgnoringRef.lastIndexOf("/");
      contentURI = contentURI.specIgnoringRef;

      // get content base URI without leaf name (if any)
      if (contentURILeafIndex > contentURIScheme.length) {
        contentURI = contentURI.substring(0, contentURILeafIndex + 1);
      }

      // avoid verbose repetition of absolute URI when the style sheet URI
      // is relative to the content URI
      this._friendlyName = (sheetURI.indexOf(contentURI) == 0)
                           ? sheetURI.substring(contentURI.length)
                           : sheetURI;
    }
    return this._friendlyName;
  },

  /**
   * Add a listener for significant StyleEditor actions.
   *
   * The listener implements IStyleEditorActionListener := {
   *   onLoad:                 Called when the style sheet has been loaded and
   *                           parsed.
   *                           Arguments: (StyleEditor editor)
   *                           @see load
   *
   *   onFlagChange:           Called when a flag has been set or cleared.
   *                           Arguments: (StyleEditor editor, string flagName)
   *                           @see setFlag
   *
   *   onAttach:               Called when an input element has been attached.
   *                           Arguments: (StyleEditor editor)
   *                           @see inputElement
   *
   *   onDetach:               Called when input element has been detached.
   *                           Arguments: (StyleEditor editor)
   *                           @see inputElement
   *
   *   onUpdate:               Called when changes are being applied to the live
   *                           DOM style sheet but might not be complete from
   *                           a WYSIWYG perspective (eg. transitioned update).
   *                           Arguments: (StyleEditor editor)
   *
   *   onCommit:               Called when changes have been completely committed
   *                           /applied to the live DOM style sheet.
   *                           Arguments: (StyleEditor editor)
   * }
   *
   * All listener methods are optional.
   *
   * @param IStyleEditorActionListener aListener
   * @see removeActionListener
   */
  addActionListener: function SE_addActionListener(aListener)
  {
    this._actionListeners.push(aListener);
  },

  /**
   * Remove a listener for editor actions from the current list of listeners.
   *
   * @param IStyleEditorActionListener aListener
   * @see addActionListener
   */
  removeActionListener: function SE_removeActionListener(aListener)
  {
    let index = this._actionListeners.indexOf(aListener);
    if (index != -1) {
      this._actionListeners.splice(index, 1);
    }
  },

  /**
   * Editor UI flags.
   *
   * These are 1-bit indicators that can be used for UI feedback/indicators or
   * extensions to track the editor status.
   * Since they are simple strings, they promote loose coupling and can simply
   * map to CSS class names, which allows to 'expose' indicators declaratively
   * via CSS (including possibly complex combinations).
   *
   * Flag changes can be tracked via onFlagChange (@see addActionListener).
   *
   * @see StyleEditorFlags
   */

  /**
   * Retrieve a space-separated string of all UI flags set on this editor.
   *
   * @return string
   * @see setFlag
   * @see clearFlag
   */
  get flags() this._flags.join(" "),

  /**
   * Set a flag.
   *
   * @param string aName
   *        Name of the flag to set. One of StyleEditorFlags members.
   * @return boolean
   *         True if the flag has been set, false if flag is already set.
   * @see StyleEditorFlags
   */
  setFlag: function SE_setFlag(aName)
  {
    let prop = aName.toUpperCase();
    assert(StyleEditorFlags[prop], "Unknown flag: " + prop);

    if (this.hasFlag(aName)) {
      return false;
    }
    this._flags.push(aName);
    this._triggerAction("FlagChange", [aName]);
    return true;
  },

  /**
   * Clear a flag.
   *
   * @param string aName
   *        Name of the flag to clear.
   * @return boolean
   *         True if the flag has been cleared, false if already clear.
   */
  clearFlag: function SE_clearFlag(aName)
  {
    let index = this._flags.indexOf(aName);
    if (index == -1) {
      return false;
    }
    this._flags.splice(index, 1);
    this._triggerAction("FlagChange", [aName]);
    return true;
  },

  /**
   * Toggle a flag, according to a condition.
   *
   * @param aCondition
   *        If true the flag is set, otherwise cleared.
   * @param string aName
   *        Name of the flag to toggle.
   * @return boolean
   *        True if the flag has been set or cleared, ie. the flag got switched.
   */
  toggleFlag: function SE_toggleFlag(aCondition, aName)
  {
    return (aCondition) ? this.setFlag(aName) : this.clearFlag(aName);
  },

  /**
   * Check if given flag is set.
   *
   * @param string aName
   *        Name of the flag to check presence for.
   * @return boolean
   *         True if the flag is set, false otherwise.
   */
  hasFlag: function SE_hasFlag(aName) (this._flags.indexOf(aName) != -1),

  /**
   * Enable or disable style sheet.
   *
   * @param boolean aEnabled
   */
  enableStyleSheet: function SE_enableStyleSheet(aEnabled)
  {
    this.styleSheet.disabled = !aEnabled;
    this.toggleFlag(this.styleSheet.disabled, StyleEditorFlags.DISABLED);

    if (this._updateTask) {
      this._updateStyleSheet(); // perform cancelled update
    }
  },

  /**
   * Save the editor contents into a file and set savedFile property.
   * A file picker UI will open if file is not set and editor is not headless.
   *
   * @param mixed aFile
   *        Optional nsIFile or string representing the filename to save in the
   *        background, no UI will be displayed.
   *        If not specified, the original style sheet URI is used.
   *        To implement 'Save' instead of 'Save as', you can pass savedFile here.
   * @param function(nsIFile aFile) aCallback
   *        Optional callback called when the operation has finished.
   *        aFile has the nsIFile object for saved file or null if the operation
   *        has failed or has been canceled by the user.
   * @see savedFile
   */
  saveToFile: function SE_saveToFile(aFile, aCallback)
  {
    aFile = this._showFilePicker(aFile || this._styleSheetFilePath, true);

    if (!aFile) {
      if (aCallback) {
        aCallback(null);
      }
      return;
    }

    if (this._sourceEditor) {
      this._state.text = this._sourceEditor.getText();
    }

    let ostream = FileUtils.openSafeFileOutputStream(aFile);
    let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
                      .createInstance(Ci.nsIScriptableUnicodeConverter);
    converter.charset = "UTF-8";
    let istream = converter.convertToInputStream(this._state.text);

    NetUtil.asyncCopy(istream, ostream, function SE_onStreamCopied(status) {
      if (!Components.isSuccessCode(status)) {
        if (aCallback) {
          aCallback(null);
        }
        this._signalError(SAVE_ERROR);
        return;
      }
      FileUtils.closeSafeFileOutputStream(ostream);

      // remember filename for next save if any
      this._friendlyName = null;
      this._savedFile = aFile;
      this._persistExpando();

      if (aCallback) {
        aCallback(aFile);
      }
      this.clearFlag(StyleEditorFlags.UNSAVED);
      this.clearFlag(StyleEditorFlags.ERROR);
    }.bind(this));
  },

  /**
   * Queue a throttled task to update the live style sheet.
   *
   * @param boolean aImmediate
   *        Optional. If true the update is performed immediately.
   */
  updateStyleSheet: function SE_updateStyleSheet(aImmediate)
  {
    let window = this.window;

    if (this._updateTask) {
      // cancel previous queued task not executed within throttle delay
      window.clearTimeout(this._updateTask);
    }

    if (aImmediate) {
      this._updateStyleSheet();
    } else {
      this._updateTask = window.setTimeout(this._updateStyleSheet.bind(this),
                                           UPDATE_STYLESHEET_THROTTLE_DELAY);
    }
  },

  /**
   * Update live style sheet according to modifications.
   */
  _updateStyleSheet: function SE__updateStyleSheet()
  {
    this.setFlag(StyleEditorFlags.UNSAVED);

    if (this.styleSheet.disabled) {
      return;
    }

    this._updateTask = null; // reset only if we actually perform an update
                             // (stylesheet is enabled) so that 'missed' updates
                             // while the stylesheet is disabled can be performed
                             // when it is enabled back. @see enableStylesheet

    if (this.sourceEditor) {
      this._state.text = this.sourceEditor.getText();
    }
    DOMUtils.parseStyleSheet(this.styleSheet, this._state.text);
    this._persistExpando();

    if (!TRANSITIONS_ENABLED) {
      this._triggerAction("Update");
      this._triggerAction("Commit");
      return;
    }

    let content = this.contentDocument;

    // Insert the global transition rule
    // Use a ref count to make sure we do not add it multiple times.. and remove
    // it only when all pending StyleEditor-generated transitions ended.
    if (!this._transitionRefCount) {
      this._styleSheet.insertRule(TRANSITION_RULE, 0);
      content.documentElement.classList.add(TRANSITION_CLASS);
    }

    this._transitionRefCount++;

    // Set up clean up and commit after transition duration (+10% buffer)
    // @see _onTransitionEnd
    content.defaultView.setTimeout(this._onTransitionEnd.bind(this),
                                   Math.floor(TRANSITION_DURATION_MS * 1.1));

    this._triggerAction("Update");
  },

  /**
    * This cleans up class and rule added for transition effect and then trigger
    * Commit as the changes have been completed.
    */
  _onTransitionEnd: function SE__onTransitionEnd()
  {
    if (--this._transitionRefCount == 0) {
      this.contentDocument.documentElement.classList.remove(TRANSITION_CLASS);
      this.styleSheet.deleteRule(0);
    }

    this._triggerAction("Commit");
  },

  /**
   * Show file picker and return the file user selected.
   *
   * @param mixed aFile
   *        Optional nsIFile or string representing the filename to auto-select.
   * @param boolean aSave
   *        If true, the user is selecting a filename to save.
   * @param nsIWindow aParentWindow
   *        Optional parent window. If null the parent window of the file picker
   *        will be the window of the attached input element.
   * @return nsIFile
   *         The selected file or null if the user did not pick one.
   */
  _showFilePicker: function SE__showFilePicker(aFile, aSave, aParentWindow)
  {
    if (typeof(aFile) == "string") {
      try {
        if (Services.io.extractScheme(aFile) == "file") {
          let uri = Services.io.newURI(aFile, null, null);
          let file = uri.QueryInterface(Ci.nsIFileURL).file;
          return file;
        }
      } catch (ex) {
      }
      try {
        let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
        file.initWithPath(aFile);
        return file;
      } catch (ex) {
        this._signalError(aSave ? SAVE_ERROR : LOAD_ERROR);
        return null;
      }
    }
    if (aFile) {
      return aFile;
    }

    let window = aParentWindow
                 ? aParentWindow
                 : this.inputElement.ownerDocument.defaultView;
    let fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
    let mode = aSave ? fp.modeSave : fp.modeOpen;
    let key = aSave ? "saveStyleSheet" : "importStyleSheet";

    fp.init(window, _(key + ".title"), mode);
    fp.appendFilters(_(key + ".filter"), "*.css");
    fp.appendFilters(fp.filterAll);

    let rv = fp.show();
    return (rv == fp.returnCancel) ? null : fp.file;
  },

  /**
   * Retrieve the style sheet source from the cache or from a local file.
   */
  _loadSource: function SE__loadSource()
  {
    if (!this.styleSheet.href) {
      // this is an inline <style> sheet
      this._flags.push(StyleEditorFlags.INLINE);
      this._onSourceLoad(this.styleSheet.ownerNode.textContent);
      return;
    }

    let scheme = Services.io.extractScheme(this.styleSheet.href);
    switch (scheme) {
      case "file":
        this._styleSheetFilePath = this.styleSheet.href;
      case "chrome":
      case "resource":
        this._loadSourceFromFile(this.styleSheet.href);
        break;
      default:
        this._loadSourceFromCache(this.styleSheet.href);
        break;
    }
  },

  /**
   * Load source from a file or file-like resource.
   *
   * @param string aHref
   *        URL for the stylesheet.
   */
  _loadSourceFromFile: function SE__loadSourceFromFile(aHref)
  {
    try {
      NetUtil.asyncFetch(aHref, function onFetch(aStream, aStatus) {
        if (!Components.isSuccessCode(aStatus)) {
          return this._signalError(LOAD_ERROR);
        }
        let source = NetUtil.readInputStreamToString(aStream, aStream.available());
        aStream.close();
        this._onSourceLoad(source);
      }.bind(this));
    } catch (ex) {
      this._signalError(LOAD_ERROR);
    }
  },

  /**
   * Load source from the HTTP cache.
   *
   * @param string aHref
   *        URL for the stylesheet.
   */
  _loadSourceFromCache: function SE__loadSourceFromCache(aHref)
  {
    let channel = Services.io.newChannel(aHref, null, null);
    let chunks = [];
    let streamListener = { // nsIStreamListener inherits nsIRequestObserver
      onStartRequest: function (aRequest, aContext, aStatusCode) {
        if (!Components.isSuccessCode(aStatusCode)) {
          return this._signalError(LOAD_ERROR);
        }
      }.bind(this),
      onDataAvailable: function (aRequest, aContext, aStream, aOffset, aCount) {
        chunks.push(NetUtil.readInputStreamToString(aStream, aCount));
      },
      onStopRequest: function (aRequest, aContext, aStatusCode) {
        if (!Components.isSuccessCode(aStatusCode)) {
          return this._signalError(LOAD_ERROR);
        }

        this._onSourceLoad(chunks.join(""));
      }.bind(this)
    };

    channel.loadFlags = channel.LOAD_FROM_CACHE;
    channel.asyncOpen(streamListener, null);
  },

  /**
   * Called when source has been loaded.
   *
   * @param string aSourceText
   */
  _onSourceLoad: function SE__onSourceLoad(aSourceText)
  {
    this._restoreExpando();
    this._state.text = prettifyCSS(aSourceText);
    this._loaded = true;
    this._triggerAction("Load");
  },

  /**
   * Create a new style sheet and append it to the content document.
   *
   * @param string aText
   *        Optional CSS text.
   */
  _appendNewStyleSheet: function SE__appendNewStyleSheet(aText)
  {
    let document = this.contentDocument;
    let parent = document.documentElement;
    let style = document.createElement("style");
    style.setAttribute("type", "text/css");
    if (aText) {
      style.appendChild(document.createTextNode(aText));
    }
    parent.appendChild(style);

    this._styleSheet = document.styleSheets[document.styleSheets.length - 1];
    if (aText) {
      this._onSourceLoad(aText);
      this._flags.push(StyleEditorFlags.IMPORTED);
    } else {
      this._flags.push(StyleEditorFlags.NEW);
      this._flags.push(StyleEditorFlags.UNSAVED);
    }
  },

  /**
   * Signal an error to the user.
   *
   * @param string aErrorCode
   *        String name for the localized error property in the string bundle.
   * @param ...rest
   *        Optional arguments to pass for message formatting.
   * @see StyleEditorUtil._
   */
  _signalError: function SE__signalError(aErrorCode)
  {
    this._errorMessage = _.apply(null, arguments);
    this.setFlag(StyleEditorFlags.ERROR);
  },

  /**
   * Trigger named action handler in listeners.
   *
   * @param string aName
   *        Name of the action to trigger.
   * @param Array aArgs
   *        Optional array of arguments to pass to the listener(s).
   * @see addActionListener
   */
  _triggerAction: function SE__triggerAction(aName, aArgs)
  {
    // insert the origin editor instance as first argument
    if (!aArgs) {
      aArgs = [this];
    } else {
      aArgs.unshift(this);
    }

    // copy the list of listeners to allow adding/removing listeners in handlers
    let listeners = this._actionListeners.concat();
    // trigger all listeners that have this action handler
    for (let i = 0; i < listeners.length; ++i) {
      let listener = listeners[i];
      let actionHandler = listener["on" + aName];
      if (actionHandler) {
        actionHandler.apply(listener, aArgs);
      }
    }

    // when a flag got changed, user-facing state need to be persisted
    if (aName == "FlagChange") {
      this._persistExpando();
    }
  },

  /**
    * Unload event handler to perform any pending update before closing
    */
  _onWindowUnload: function SE__onWindowUnload(aEvent)
  {
    if (this._updateTask) {
      this.updateStyleSheet(true);
    }
  },

  /**
   * Focus the Style Editor input.
   */
  focus: function SE_focus()
  {
    if (this._sourceEditor) {
      this._sourceEditor.focus();
    } else {
      this._focusOnSourceEditorReady = true;
    }
  },

  /**
   * Event handler for when the editor is shown. Call this after the editor is
   * shown.
   */
  onShow: function SE_onShow()
  {
    if (this._sourceEditor) {
      this._sourceEditor.setTopIndex(this._state.topIndex);
    }
    this.focus();
  },

  /**
   * Event handler for when the editor is hidden. Call this before the editor is
   * hidden.
   */
  onHide: function SE_onHide()
  {
    if (this._sourceEditor) {
      this._state.topIndex = this._sourceEditor.getTopIndex();
    }
  },

  /**
    * Persist StyleEditor extra data to the attached DOM stylesheet expando.
    * The expando on the DOM stylesheet is used to restore user-facing state
    * when the StyleEditor is closed and then reopened again.
    *
    * @see styleSheet
    */
  _persistExpando: function SE__persistExpando()
  {
    if (!this._styleSheet) {
      return; // not loaded
    }
    let name = STYLESHEET_EXPANDO + this.styleSheetIndex;
    let expando = this.contentDocument.getUserData(name);
    if (!expando) {
      expando = {};
      this.contentDocument.setUserData(name, expando, null);
    }
    expando._flags = this._flags;
    expando._savedFile = this._savedFile;
  },

  /**
    * Restore the attached DOM stylesheet expando into this editor state.
    *
    * @see styleSheet
    */
  _restoreExpando: function SE__restoreExpando()
  {
    if (!this._styleSheet) {
      return; // not loaded
    }
    let name = STYLESHEET_EXPANDO + this.styleSheetIndex;
    let expando = this.contentDocument.getUserData(name);
    if (expando) {
      this._flags = expando._flags;
      this._savedFile = expando._savedFile;
    }
  },

  /**
    * Retrieve custom key bindings objects as expected by SourceEditor.
    * SourceEditor action names are not displayed to the user.
    *
    * @return Array
    */
  _getKeyBindings: function SE__getKeyBindings()
  {
    let bindings = [];

    bindings.push({
      action: "StyleEditor.save",
      code: _("saveStyleSheet.commandkey"),
      accel: true,
      callback: function save() {
        this.saveToFile(this._savedFile);
      }.bind(this)
    });

    bindings.push({
      action: "StyleEditor.saveAs",
      code: _("saveStyleSheet.commandkey"),
      accel: true,
      shift: true,
      callback: function saveAs() {
        this.saveToFile();
      }.bind(this)
    });

    return bindings;
  }
};

/**
 * List of StyleEditor UI flags.
 * A Style Editor add-on using its own flag needs to add it to this object.
 *
 * @see StyleEditor.setFlag
 */
let StyleEditorFlags = {
  DISABLED:      "disabled",
  ERROR:         "error",
  IMPORTED:      "imported",
  INLINE:        "inline",
  MODIFIED:      "modified",
  NEW:           "new",
  UNSAVED:       "unsaved"
};


const TAB_CHARS   = "\t";

const OS = Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime).OS;
const LINE_SEPARATOR = OS === "WINNT" ? "\r\n" : "\n";

/**
 * Prettify minified CSS text.
 * This prettifies CSS code where there is no indentation in usual places while
 * keeping original indentation as-is elsewhere.
 *
 * @param string aText
 *        The CSS source to prettify.
 * @return string
 *         Prettified CSS source
 */
function prettifyCSS(aText)
{
  // remove initial and terminating HTML comments and surrounding whitespace
  aText = aText.replace(/(?:^\s*<!--[\r\n]*)|(?:\s*-->\s*$)/g, "");

  let parts = [];    // indented parts
  let partStart = 0; // start offset of currently parsed part
  let indent = "";
  let indentLevel = 0;

  for (let i = 0; i < aText.length; i++) {
    let c = aText[i];
    let shouldIndent = false;

    switch (c) {
      case "}":
        if (i - partStart > 1) {
          // there's more than just } on the line, add line
          parts.push(indent + aText.substring(partStart, i));
          partStart = i;
        }
        indent = repeat(TAB_CHARS, --indentLevel);
        /* fallthrough */
      case ";":
      case "{":
        shouldIndent = true;
        break;
    }

    if (shouldIndent) {
      let la = aText[i+1]; // one-character lookahead
      if (!/\s/.test(la)) {
        // following character should be a new line (or whitespace) but it isn't
        // force indentation then
        parts.push(indent + aText.substring(partStart, i + 1));
        if (c == "}") {
          parts.push(""); // for extra line separator
        }
        partStart = i + 1;
      } else {
        return aText; // assume it is not minified, early exit
      }
    }

    if (c == "{") {
      indent = repeat(TAB_CHARS, ++indentLevel);
    }
  }
  return parts.join(LINE_SEPARATOR);
}

/**
  * Return string that repeats aText for aCount times.
  *
  * @param string aText
  * @param number aCount
  * @return string
  */
function repeat(aText, aCount)
{
  return (new Array(aCount + 1)).join(aText);
}

/**
 * Set up bracket completion on a given SourceEditor.
 * This automatically closes the following CSS brackets: "{", "(", "["
 *
 * @param SourceEditor aSourceEditor
 */
function setupBracketCompletion(aSourceEditor)
{
  let editorElement = aSourceEditor.editorElement;
  let pairs = {
    123: { // {
      closeString: "}",
      closeKeyCode: Ci.nsIDOMKeyEvent.DOM_VK_CLOSE_BRACKET
    },
    40: { // (
      closeString: ")",
      closeKeyCode: Ci.nsIDOMKeyEvent.DOM_VK_0
    },
    91: { // [
      closeString: "]",
      closeKeyCode: Ci.nsIDOMKeyEvent.DOM_VK_CLOSE_BRACKET
    },
  };

  editorElement.addEventListener("keypress", function onKeyPress(aEvent) {
    let pair = pairs[aEvent.charCode];
    if (!pair || aEvent.ctrlKey || aEvent.metaKey ||
        aEvent.accelKey || aEvent.altKey) {
      return true;
    }

    // We detected an open bracket, sending closing character
    let keyCode = pair.closeKeyCode;
    let charCode = pair.closeString.charCodeAt(0);
    let modifiers = 0;
    let utils = editorElement.ownerDocument.defaultView.
                  QueryInterface(Ci.nsIInterfaceRequestor).
                  getInterface(Ci.nsIDOMWindowUtils);
    let handled = utils.sendKeyEvent("keydown", keyCode, 0, modifiers);
    utils.sendKeyEvent("keypress", 0, charCode, modifiers, !handled);
    utils.sendKeyEvent("keyup", keyCode, 0, modifiers);
    // and rewind caret
    aSourceEditor.setCaretOffset(aSourceEditor.getCaretOffset() - 1);
  }, false);
}

/**
  * Manage the different editors instances.
  */

function StyleEditorManager(aWindow) {
  this.chromeWindow = aWindow;
  this.listenToTabs();
  this.editors = new WeakMap();
}

StyleEditorManager.prototype = {

  /**
   * Get the editor for a specific content window.
   */
  getEditorForWindow: function SEM_getEditorForWindow(aContentWindow) {
    return this.editors.get(aContentWindow);
  },

  /**
   * Focus the editor and select a stylesheet.
   *
   * @param {CSSStyleSheet} [aSelectedStyleSheet] default Stylesheet.
   * @param {Number} [aLine] Line to which the caret should be moved (one-indexed).
   * @param {Number} [aCol] Column to which the caret should be moved (one-indexed).
   */
  selectEditor: function SEM_selectEditor(aWindow, aSelectedStyleSheet, aLine, aCol) {
    if (aSelectedStyleSheet) {
      aWindow.styleEditorChrome.selectStyleSheet(aSelectedStyleSheet, aLine, aCol);
    }
    aWindow.focus();
  },

  /**
   * Open a new editor.
   *
   * @param {Window} content window.
   * @param {CSSStyleSheet} [aSelectedStyleSheet] default Stylesheet.
   * @param {Number} [aLine] Line to which the caret should be moved (one-indexed).
   * @param {Number} [aCol] Column to which the caret should be moved (one-indexed).
   */
  newEditor: function SEM_newEditor(aContentWindow, aSelectedStyleSheet, aLine, aCol) {
    const CHROME_URL = "chrome://browser/content/styleeditor.xul";
    const CHROME_WINDOW_FLAGS = "chrome,centerscreen,resizable,dialog=no";

    let args = {
      contentWindow: aContentWindow,
      selectedStyleSheet: aSelectedStyleSheet,
      line: aLine,
      col: aCol
    };
    args.wrappedJSObject = args;
    let chromeWindow = Services.ww.openWindow(null, CHROME_URL, "_blank",
                                              CHROME_WINDOW_FLAGS, args);

    chromeWindow.onunload = function() {
      if (chromeWindow.location == CHROME_URL) {
        // not about:blank being unloaded
        this.unregisterEditor(aContentWindow);
      }
    }.bind(this);
    chromeWindow.focus();

    this.editors.set(aContentWindow, chromeWindow);

    this.refreshCommand();

    return chromeWindow;
  },

  /**
   * Toggle an editor.
   *
   * @param {Window} associated content window.
   */
  toggleEditor: function SEM_toggleEditor(aContentWindow) {
    let editor = this.getEditorForWindow(aContentWindow);
    if (editor) {
      editor.close();
    } else {
      this.newEditor(aContentWindow);
    }
  },

  /**
   * Close an editor.
   *
   * @param {Window} associated content window.
   */
  unregisterEditor: function SEM_unregisterEditor(aContentWindow) {
    let chromeWindow = this.editors.get(aContentWindow);
    if (chromeWindow) {
      chromeWindow.close();
    }
    this.editors.delete(aContentWindow);
    this.refreshCommand();
  },

  /**
   * Update the status of tool's menuitems and buttons.
   */
  refreshCommand: function SEM_refreshCommand() {
    let contentWindow = this.chromeWindow.gBrowser.contentWindow;
    let command = this.chromeWindow.document.getElementById("Tools:StyleEditor");

    let win = this.getEditorForWindow(contentWindow);
    if (win) {
      command.setAttribute("checked", "true");
    } else {
      command.setAttribute("checked", "false");
    }
  },

  /**
   * Trigger refreshCommand when needed.
   */
  listenToTabs: function SEM_listenToTabs() {
    let win = this.chromeWindow;
    let tabs = win.gBrowser.tabContainer;

    let bound_refreshCommand = this.refreshCommand.bind(this);
    tabs.addEventListener("TabSelect", bound_refreshCommand, true);

    win.addEventListener("unload", function onClose(aEvent) {
      tabs.removeEventListener("TabSelect", bound_refreshCommand, true);
      win.removeEventListener("unload", onClose, false);
    }, false);
  },
}
