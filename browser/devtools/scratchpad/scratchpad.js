/* vim:set ts=2 sw=2 sts=2 et:
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
 * The Original Code is Scratchpad.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Rob Campbell <robcee@mozilla.com> (original author)
 *   Erik Vold <erikvvold@gmail.com>
 *   David Dahl <ddahl@mozilla.com>
 *   Mihai Sucan <mihai.sucan@gmail.com>
 *   Kenny Heaton <kennyheaton@gmail.com>
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

/*
 * Original version history can be found here:
 * https://github.com/mozilla/workspace
 *
 * Copied and relicensed from the Public Domain.
 * See bug 653934 for details.
 * https://bugzilla.mozilla.org/show_bug.cgi?id=653934
 */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource:///modules/PropertyPanel.jsm");
Cu.import("resource:///modules/source-editor.jsm");
Cu.import("resource:///modules/devtools/scratchpad-manager.jsm");


const SCRATCHPAD_CONTEXT_CONTENT = 1;
const SCRATCHPAD_CONTEXT_BROWSER = 2;
const SCRATCHPAD_L10N = "chrome://browser/locale/devtools/scratchpad.properties";
const DEVTOOLS_CHROME_ENABLED = "devtools.chrome.enabled";
const BUTTON_POSITION_SAVE = 0;
const BUTTON_POSITION_CANCEL = 1;
const BUTTON_POSITION_DONT_SAVE = 2;

/**
 * The scratchpad object handles the Scratchpad window functionality.
 */
var Scratchpad = {
  _initialWindowTitle: document.title,

  /**
   * The script execution context. This tells Scratchpad in which context the
   * script shall execute.
   *
   * Possible values:
   *   - SCRATCHPAD_CONTEXT_CONTENT to execute code in the context of the current
   *   tab content window object.
   *   - SCRATCHPAD_CONTEXT_BROWSER to execute code in the context of the
   *   currently active chrome window object.
   */
  executionContext: SCRATCHPAD_CONTEXT_CONTENT,

  /**
   * Tells if this Scratchpad is initialized and ready for use.
   * @boolean
   * @see addObserver
   */
  initialized: false,

  /**
   * Retrieve the xul:notificationbox DOM element. It notifies the user when
   * the current code execution context is SCRATCHPAD_CONTEXT_BROWSER.
   */
  get notificationBox() document.getElementById("scratchpad-notificationbox"),

  /**
   * Get the selected text from the editor.
   *
   * @return string
   *         The selected text.
   */
  get selectedText() this.editor.getSelectedText(),

  /**
   * Get the editor content, in the given range. If no range is given you get
   * the entire editor content.
   *
   * @param number [aStart=0]
   *        Optional, start from the given offset.
   * @param number [aEnd=content char count]
   *        Optional, end offset for the text you want. If this parameter is not
   *        given, then the text returned goes until the end of the editor
   *        content.
   * @return string
   *         The text in the given range.
   */
  getText: function SP_getText(aStart, aEnd)
  {
    return this.editor.getText(aStart, aEnd);
  },

  /**
   * Replace text in the source editor with the given text, in the given range.
   *
   * @param string aText
   *        The text you want to put into the editor.
   * @param number [aStart=0]
   *        Optional, the start offset, zero based, from where you want to start
   *        replacing text in the editor.
   * @param number [aEnd=char count]
   *        Optional, the end offset, zero based, where you want to stop
   *        replacing text in the editor.
   */
  setText: function SP_setText(aText, aStart, aEnd)
  {
    this.editor.setText(aText, aStart, aEnd);
  },

  /**
   * Set the filename in the scratchpad UI and object
   *
   * @param string aFilename
   *        The new filename
   */
  setFilename: function SP_setFilename(aFilename)
  {
    this.filename = aFilename;
    this._updateTitle();
  },

  /**
   * Update the Scratchpad window title based on the current state.
   * @private
   */
  _updateTitle: function SP__updateTitle()
  {
    if (this.filename) {
      document.title = (this.editor && this.editor.dirty ? "*" : "") +
                       this.filename;
    } else {
      document.title = this._initialWindowTitle;
    }
  },

  /**
   * Get the current state of the scratchpad. Called by the
   * Scratchpad Manager for session storing.
   *
   * @return object
   *        An object with 3 properties: filename, text, and
   *        executionContext.
   */
  getState: function SP_getState()
  {
    return {
      filename: this.filename,
      text: this.getText(),
      executionContext: this.executionContext,
      saved: !this.editor.dirty,
    };
  },

  /**
   * Set the filename and execution context using the given state. Called
   * when scratchpad is being restored from a previous session.
   *
   * @param object aState
   *        An object with filename and executionContext properties.
   */
  setState: function SP_getState(aState)
  {
    if (aState.filename) {
      this.setFilename(aState.filename);
    }
    if (this.editor) {
      this.editor.dirty = !aState.saved;
    }

    if (aState.executionContext == SCRATCHPAD_CONTEXT_BROWSER) {
      this.setBrowserContext();
    }
    else {
      this.setContentContext();
    }
  },

  /**
   * Get the most recent chrome window of type navigator:browser.
   */
  get browserWindow() Services.wm.getMostRecentWindow("navigator:browser"),

  /**
   * Reference to the last chrome window of type navigator:browser. We use this
   * to check if the chrome window changed since the last code evaluation.
   */
  _previousWindow: null,

  /**
   * Get the gBrowser object of the most recent browser window.
   */
  get gBrowser()
  {
    let recentWin = this.browserWindow;
    return recentWin ? recentWin.gBrowser : null;
  },

  /**
   * Cached Cu.Sandbox object for the active tab content window object.
   */
  _contentSandbox: null,

  /**
   * Get the Cu.Sandbox object for the active tab content window object. Note
   * that the returned object is cached for later reuse. The cached object is
   * kept only for the current location in the current tab of the current
   * browser window and it is reset for each context switch,
   * navigator:browser window switch, tab switch or navigation.
   */
  get contentSandbox()
  {
    if (!this.browserWindow) {
      Cu.reportError(this.strings.
                     GetStringFromName("browserWindow.unavailable"));
      return;
    }

    if (!this._contentSandbox ||
        this.browserWindow != this._previousBrowserWindow ||
        this._previousBrowser != this.gBrowser.selectedBrowser ||
        this._previousLocation != this.gBrowser.contentWindow.location.href) {
      let contentWindow = this.gBrowser.selectedBrowser.contentWindow;
      this._contentSandbox = new Cu.Sandbox(contentWindow,
        { sandboxPrototype: contentWindow, wantXrays: false, 
          sandboxName: 'scratchpad-content'});

      this._previousBrowserWindow = this.browserWindow;
      this._previousBrowser = this.gBrowser.selectedBrowser;
      this._previousLocation = contentWindow.location.href;
    }

    return this._contentSandbox;
  },

  /**
   * Cached Cu.Sandbox object for the most recently active navigator:browser
   * chrome window object.
   */
  _chromeSandbox: null,

  /**
   * Get the Cu.Sandbox object for the most recently active navigator:browser
   * chrome window object. Note that the returned object is cached for later
   * reuse. The cached object is kept only for the current browser window and it
   * is reset for each context switch or navigator:browser window switch.
   */
  get chromeSandbox()
  {
    if (!this.browserWindow) {
      Cu.reportError(this.strings.
                     GetStringFromName("browserWindow.unavailable"));
      return;
    }

    if (!this._chromeSandbox ||
        this.browserWindow != this._previousBrowserWindow) {
      this._chromeSandbox = new Cu.Sandbox(this.browserWindow,
        { sandboxPrototype: this.browserWindow, wantXrays: false, 
          sandboxName: 'scratchpad-chrome'});

      this._previousBrowserWindow = this.browserWindow;
    }

    return this._chromeSandbox;
  },

  /**
   * Drop the editor selection.
   */
  deselect: function SP_deselect()
  {
    this.editor.dropSelection();
  },

  /**
   * Select a specific range in the Scratchpad editor.
   *
   * @param number aStart
   *        Selection range start.
   * @param number aEnd
   *        Selection range end.
   */
  selectRange: function SP_selectRange(aStart, aEnd)
  {
    this.editor.setSelection(aStart, aEnd);
  },

  /**
   * Get the current selection range.
   *
   * @return object
   *         An object with two properties, start and end, that give the
   *         selection range (zero based offsets).
   */
  getSelectionRange: function SP_getSelection()
  {
    return this.editor.getSelection();
  },

  /**
   * Evaluate a string in the active tab content window.
   *
   * @param string aString
   *        The script you want evaluated.
   * @return mixed
   *         The script evaluation result.
   */
  evalInContentSandbox: function SP_evalInContentSandbox(aString)
  {
    let error, result;
    try {
      result = Cu.evalInSandbox(aString, this.contentSandbox, "1.8",
                                "Scratchpad", 1);
    }
    catch (ex) {
      error = ex;
    }

    return [error, result];
  },

  /**
   * Evaluate a string in the most recent navigator:browser chrome window.
   *
   * @param string aString
   *        The script you want evaluated.
   * @return mixed
   *         The script evaluation result.
   */
  evalInChromeSandbox: function SP_evalInChromeSandbox(aString)
  {
    let error, result;
    try {
      result = Cu.evalInSandbox(aString, this.chromeSandbox, "1.8",
                                "Scratchpad", 1);
    }
    catch (ex) {
      error = ex;
    }

    return [error, result];
  },

  /**
   * Evaluate a string in the currently desired context, that is either the
   * chrome window or the tab content window object.
   *
   * @param string aString
   *        The script you want to evaluate.
   * @return mixed
   *         The script evaluation result.
   */
  evalForContext: function SP_evaluateForContext(aString)
  {
    return this.executionContext == SCRATCHPAD_CONTEXT_CONTENT ?
           this.evalInContentSandbox(aString) :
           this.evalInChromeSandbox(aString);
  },

  /**
   * Execute the selected text (if any) or the entire editor content in the
   * current context.
   * @return mixed
   *         The script evaluation result.
   */
  execute: function SP_execute()
  {
    let selection = this.selectedText || this.getText();
    let [error, result] = this.evalForContext(selection);
    return [selection, error, result];
  },

  /**
   * Execute the selected text (if any) or the entire editor content in the
   * current context.
   */
  run: function SP_run()
  {
    let [selection, error, result] = this.execute();

    if (!error) {
      this.deselect();
    } else {
      this.writeAsErrorComment(error);
    }

    return [selection, error, result];
  },

  /**
   * Execute the selected text (if any) or the entire editor content in the
   * current context. The resulting object is opened up in the Property Panel
   * for inspection.
   */
  inspect: function SP_inspect()
  {
    let [selection, error, result] = this.execute();

    if (!error) {
      this.deselect();
      this.openPropertyPanel(selection, result);
    } else {
      this.writeAsErrorComment(error);
    }
  },

  /**
   * Execute the selected text (if any) or the entire editor content in the
   * current context. The evaluation result is inserted into the editor after
   * the selected text, or at the end of the editor content if there is no
   * selected text.
   */
  display: function SP_display()
  {
    let [selectedText, error, result] = this.execute();

    if (!error) {
      this.writeAsComment(result);
    } else {
      this.writeAsErrorComment(error);
    }
  },

  /**
   * Write out a value at the next line from the current insertion point.
   * The comment block will always be preceded by a newline character.
   * @param object aValue
   *        The Object to write out as a string
   */
  writeAsComment: function SP_writeAsComment(aValue)
  {
    let selection = this.getSelectionRange();
    let insertionPoint = selection.start != selection.end ?
                         selection.end : // after selected text
                         this.editor.getCharCount(); // after text end
                         
    let newComment = "\n/*\n" + aValue + "\n*/";
    
    this.setText(newComment, insertionPoint, insertionPoint);

    // Select the new comment.
    this.selectRange(insertionPoint, insertionPoint + newComment.length);
  },

  /**
   * Write out an error at the current insertion point as a block comment
   * @param object aValue
   *        The Error object to write out the message and stack trace
   */
  writeAsErrorComment: function SP_writeAsErrorComment(aError)
  {
    let newComment = "Exception: " + aError.message + "\n" + aError.stack.substring(0, aError.stack.length - 1);
    
    this.writeAsComment(newComment);
  },

  /**
   * Open the Property Panel to inspect the given object.
   *
   * @param string aEvalString
   *        The string that was evaluated. This is re-used when the user updates
   *        the properties list, by clicking the Update button.
   * @param object aOutputObject
   *        The object to inspect, which is the aEvalString evaluation result.
   * @return object
   *         The PropertyPanel object instance.
   */
  openPropertyPanel: function SP_openPropertyPanel(aEvalString, aOutputObject)
  {
    let self = this;
    let propPanel;
    // The property panel has a button:
    // `Update`: reexecutes the string executed on the command line. The
    // result will be inspected by this panel.
    let buttons = [];

    // If there is a evalString passed to this function, then add a `Update`
    // button to the panel so that the evalString can be reexecuted to update
    // the content of the panel.
    if (aEvalString !== null) {
      buttons.push({
        label: this.strings.
               GetStringFromName("propertyPanel.updateButton.label"),
        accesskey: this.strings.
                   GetStringFromName("propertyPanel.updateButton.accesskey"),
        oncommand: function () {
          let [error, result] = self.evalForContext(aEvalString);

          if (!error) {
            propPanel.treeView.data = result;
          }
        }
      });
    }

    let doc = this.browserWindow.document;
    let parent = doc.getElementById("mainPopupSet");
    let title = aOutputObject.toString();
    propPanel = new PropertyPanel(parent, doc, title, aOutputObject, buttons);

    let panel = propPanel.panel;
    panel.setAttribute("class", "scratchpad_propertyPanel");
    panel.openPopup(null, "after_pointer", 0, 0, false, false);
    panel.sizeTo(200, 400);

    return propPanel;
  },

  // Menu Operations

  /**
   * Open a new Scratchpad window.
   *
   * @return nsIWindow
   */
  openScratchpad: function SP_openScratchpad()
  {
    return ScratchpadManager.openScratchpad();
  },

  /**
   * Export the textbox content to a file.
   *
   * @param nsILocalFile aFile
   *        The file where you want to save the textbox content.
   * @param boolean aNoConfirmation
   *        If the file already exists, ask for confirmation?
   * @param boolean aSilentError
   *        True if you do not want to display an error when file save fails,
   *        false otherwise.
   * @param function aCallback
   *        Optional function you want to call when file save completes. It will
   *        get the following arguments:
   *        1) the nsresult status code for the export operation.
   */
  exportToFile: function SP_exportToFile(aFile, aNoConfirmation, aSilentError,
                                         aCallback)
  {
    if (!aNoConfirmation && aFile.exists() &&
        !window.confirm(this.strings.
                        GetStringFromName("export.fileOverwriteConfirmation"))) {
      return;
    }

    let fs = Cc["@mozilla.org/network/file-output-stream;1"].
             createInstance(Ci.nsIFileOutputStream);
    let modeFlags = 0x02 | 0x08 | 0x20;
    fs.init(aFile, modeFlags, 420 /* 0644 */, fs.DEFER_OPEN);

    let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"].
                    createInstance(Ci.nsIScriptableUnicodeConverter);
    converter.charset = "UTF-8";
    let input = converter.convertToInputStream(this.getText());

    let self = this;
    NetUtil.asyncCopy(input, fs, function(aStatus) {
      if (!aSilentError && !Components.isSuccessCode(aStatus)) {
        window.alert(self.strings.GetStringFromName("saveFile.failed"));
      }

      if (aCallback) {
        aCallback.call(self, aStatus);
      }
    });
  },

  /**
   * Read the content of a file and put it into the textbox.
   *
   * @param nsILocalFile aFile
   *        The file you want to save the textbox content into.
   * @param boolean aSilentError
   *        True if you do not want to display an error when file load fails,
   *        false otherwise.
   * @param function aCallback
   *        Optional function you want to call when file load completes. It will
   *        get the following arguments:
   *        1) the nsresult status code for the import operation.
   *        2) the data that was read from the file, if any.
   */
  importFromFile: function SP_importFromFile(aFile, aSilentError, aCallback)
  {
    // Prevent file type detection.
    let channel = NetUtil.newChannel(aFile);
    channel.contentType = "application/javascript";

    let self = this;
    NetUtil.asyncFetch(channel, function(aInputStream, aStatus) {
      let content = null;

      if (Components.isSuccessCode(aStatus)) {
        let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"].
                        createInstance(Ci.nsIScriptableUnicodeConverter);
        converter.charset = "UTF-8";
        content = NetUtil.readInputStreamToString(aInputStream,
                                                  aInputStream.available());
        content = converter.ConvertToUnicode(content);
        self.setText(content);
        self.editor.resetUndo();
      }
      else if (!aSilentError) {
        window.alert(self.strings.GetStringFromName("openFile.failed"));
      }

      if (aCallback) {
        aCallback.call(self, aStatus, content);
      }
    });
  },

  /**
   * Open a file to edit in the Scratchpad.
   */
  openFile: function SP_openFile()
  {
    let fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
    fp.init(window, this.strings.GetStringFromName("openFile.title"),
            Ci.nsIFilePicker.modeOpen);
    fp.defaultString = "";
    if (fp.show() != Ci.nsIFilePicker.returnCancel) {
      this.setFilename(fp.file.path);
      this.importFromFile(fp.file, false);
    }
  },

  /**
   * Save the textbox content to the currently open file.
   *
   * @param function aCallback
   *        Optional function you want to call when file is saved
   */
  saveFile: function SP_saveFile(aCallback)
  {
    if (!this.filename) {
      return this.saveFileAs(aCallback);
    }

    let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
    file.initWithPath(this.filename);

    this.exportToFile(file, true, false, function(aStatus) {
      if (Components.isSuccessCode(aStatus)) {
        this.editor.dirty = false;
      }
      if (aCallback) {
        aCallback(aStatus);
      }
    });
  },

  /**
   * Save the textbox content to a new file.
   *
   * @param function aCallback
   *        Optional function you want to call when file is saved
   */
  saveFileAs: function SP_saveFileAs(aCallback)
  {
    let fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
    fp.init(window, this.strings.GetStringFromName("saveFileAs"),
            Ci.nsIFilePicker.modeSave);
    fp.defaultString = "scratchpad.js";
    if (fp.show() != Ci.nsIFilePicker.returnCancel) {
      this.setFilename(fp.file.path);

      this.exportToFile(fp.file, true, false, function(aStatus) {
        if (Components.isSuccessCode(aStatus)) {
          this.editor.dirty = false;
        }
        if (aCallback) {
          aCallback(aStatus);
        }
      });
    }
  },

  /**
   * Open the Error Console.
   */
  openErrorConsole: function SP_openErrorConsole()
  {
    this.browserWindow.toJavaScriptConsole();
  },

  /**
   * Open the Web Console.
   */
  openWebConsole: function SP_openWebConsole()
  {
    if (!this.browserWindow.HUDConsoleUI.getOpenHUD()) {
      this.browserWindow.HUDConsoleUI.toggleHUD();
    }
    this.browserWindow.focus();
  },

  /**
   * Set the current execution context to be the active tab content window.
   */
  setContentContext: function SP_setContentContext()
  {
    if (this.executionContext == SCRATCHPAD_CONTEXT_CONTENT) {
      return;
    }

    let content = document.getElementById("sp-menu-content");
    document.getElementById("sp-menu-browser").removeAttribute("checked");
    content.setAttribute("checked", true);
    this.executionContext = SCRATCHPAD_CONTEXT_CONTENT;
    this.notificationBox.removeAllNotifications(false);
    this.resetContext();
  },

  /**
   * Set the current execution context to be the most recent chrome window.
   */
  setBrowserContext: function SP_setBrowserContext()
  {
    if (this.executionContext == SCRATCHPAD_CONTEXT_BROWSER) {
      return;
    }

    let browser = document.getElementById("sp-menu-browser");
    document.getElementById("sp-menu-content").removeAttribute("checked");
    browser.setAttribute("checked", true);
    this.executionContext = SCRATCHPAD_CONTEXT_BROWSER;
    this.notificationBox.appendNotification(
      this.strings.GetStringFromName("browserContext.notification"),
      SCRATCHPAD_CONTEXT_BROWSER,
      null,
      this.notificationBox.PRIORITY_WARNING_HIGH,
      null);
    this.resetContext();
  },

  /**
   * Reset the cached Cu.Sandbox object for the current context.
   */
  resetContext: function SP_resetContext()
  {
    this._chromeSandbox = null;
    this._contentSandbox = null;
    this._previousWindow = null;
    this._previousBrowser = null;
    this._previousLocation = null;
  },

  /**
   * Gets the ID of the inner window of the given DOM window object.
   *
   * @param nsIDOMWindow aWindow
   * @return integer
   *         the inner window ID
   */
  getInnerWindowId: function SP_getInnerWindowId(aWindow)
  {
    return aWindow.QueryInterface(Ci.nsIInterfaceRequestor).
           getInterface(Ci.nsIDOMWindowUtils).currentInnerWindowID;
  },

  /**
   * The Scratchpad window load event handler. This method
   * initializes the Scratchpad window and source editor.
   *
   * @param nsIDOMEvent aEvent
   */
  onLoad: function SP_onLoad(aEvent)
  {
    if (aEvent.target != document) {
      return;
    }
    let chrome = Services.prefs.getBoolPref(DEVTOOLS_CHROME_ENABLED);
    if (chrome) {
      let environmentMenu = document.getElementById("sp-environment-menu");
      let errorConsoleCommand = document.getElementById("sp-cmd-errorConsole");
      let chromeContextCommand = document.getElementById("sp-cmd-browserContext");
      environmentMenu.removeAttribute("hidden");
      chromeContextCommand.removeAttribute("disabled");
      errorConsoleCommand.removeAttribute("disabled");
    }

    let state = null;
    let initialText = this.strings.GetStringFromName("scratchpadIntro");
    if ("arguments" in window &&
         window.arguments[0] instanceof Ci.nsIDialogParamBlock) {
      state = JSON.parse(window.arguments[0].GetString(0));
      this.setState(state);
      initialText = state.text;
    }

    this.editor = new SourceEditor();

    let config = {
      mode: SourceEditor.MODES.JAVASCRIPT,
      showLineNumbers: true,
      initialText: initialText,
      contextMenu: "scratchpad-text-popup",
    };

    let editorPlaceholder = document.getElementById("scratchpad-editor");
    this.editor.init(editorPlaceholder, config,
                     this._onEditorLoad.bind(this, state));
  },

  /**
   * The load event handler for the source editor. This method does post-load
   * editor initialization.
   *
   * @private
   * @param object aState
   *        The initial Scratchpad state object.
   */
  _onEditorLoad: function SP__onEditorLoad(aState)
  {
    this.editor.addEventListener(SourceEditor.EVENTS.DIRTY_CHANGED,
                                 this._onDirtyChanged);
    this.editor.focus();
    this.editor.setCaretOffset(this.editor.getCharCount());
    if (aState) {
      this.editor.dirty = !aState.saved;
    }

    this.initialized = true;

    this._triggerObservers("Ready");
  },

  /**
   * Insert text at the current caret location.
   *
   * @param string aText
   *        The text you want to insert.
   */
  insertTextAtCaret: function SP_insertTextAtCaret(aText)
  {
    let caretOffset = this.editor.getCaretOffset();
    this.setText(aText, caretOffset, caretOffset);
    this.editor.setCaretOffset(caretOffset + aText.length);
  },

  /**
   * The Source Editor DirtyChanged event handler. This function updates the
   * Scratchpad window title to show an asterisk when there are unsaved changes.
   *
   * @private
   * @see SourceEditor.EVENTS.DIRTY_CHANGED
   * @param object aEvent
   *        The DirtyChanged event object.
   */
  _onDirtyChanged: function SP__onDirtyChanged(aEvent)
  {
    Scratchpad._updateTitle();
  },

  /**
   * Undo the last action of the user.
   */
  undo: function SP_undo()
  {
    this.editor.undo();
  },

  /**
   * Redo the previously undone action.
   */
  redo: function SP_redo()
  {
    this.editor.redo();
  },

  /**
   * The Scratchpad window unload event handler. This method unloads/destroys
   * the source editor.
   *
   * @param nsIDOMEvent aEvent
   */
  onUnload: function SP_onUnload(aEvent)
  {
    if (aEvent.target != document) {
      return;
    }

    this.resetContext();
    this.editor.removeEventListener(SourceEditor.EVENTS.DIRTY_CHANGED,
                                    this._onDirtyChanged);
    this.editor.destroy();
    this.editor = null;
    this.initialized = false;
  },

  /**
   * Prompt to save scratchpad if it has unsaved changes.
   *
   * @param function aCallback
   *        Optional function you want to call when file is saved. The callback
   *        receives three arguments:
   *          - toClose (boolean) - tells if the window should be closed.
   *          - saved (boolen) - tells if the file has been saved.
   *          - status (number) - the file save status result (if the file was
   *          saved).
   * @return boolean
   *         Whether the window should be closed
   */
  promptSave: function SP_promptSave(aCallback)
  {
    if (this.filename && this.editor.dirty) {
      let ps = Services.prompt;
      let flags = ps.BUTTON_POS_0 * ps.BUTTON_TITLE_SAVE +
                  ps.BUTTON_POS_1 * ps.BUTTON_TITLE_CANCEL +
                  ps.BUTTON_POS_2 * ps.BUTTON_TITLE_DONT_SAVE;

      let button = ps.confirmEx(window,
                          this.strings.GetStringFromName("confirmClose.title"),
                          this.strings.GetStringFromName("confirmClose"),
                          flags, null, null, null, null, {});

      if (button == BUTTON_POSITION_CANCEL) {
        if (aCallback) {
          aCallback(false, false);
        }
        return false;
      }

      if (button == BUTTON_POSITION_SAVE) {
        this.saveFile(function(aStatus) {
          if (aCallback) {
            aCallback(true, true, aStatus);
          }
        });
        return true;
      }
    }

    if (aCallback) {
      aCallback(true, false);
    }
    return true;
  },

  /**
   * Handler for window close event. Prompts to save scratchpad if
   * there are unsaved changes.
   *
   * @param nsIDOMEvent aEvent
   */
  onClose: function SP_onClose(aEvent)
  {
    if (this._skipClosePrompt) {
      return;
    }

    this.promptSave(function(aShouldClose, aSaved, aStatus) {
      let shouldClose = aShouldClose;
      if (aSaved && !Components.isSuccessCode(aStatus)) {
        shouldClose = false;
      }

      if (shouldClose) {
        this._skipClosePrompt = true;
        window.close();
      }
    }.bind(this));
    aEvent.preventDefault();
  },

  /**
   * Close the scratchpad window. Prompts before closing if the scratchpad
   * has unsaved changes.
   *
   * @param function aCallback
   *        Optional function you want to call when file is saved
   */
  close: function SP_close(aCallback)
  {
    this.promptSave(function(aShouldClose, aSaved, aStatus) {
      let shouldClose = aShouldClose;
      if (aSaved && !Components.isSuccessCode(aStatus)) {
        shouldClose = false;
      }

      if (shouldClose) {
        this._skipClosePrompt = true;
        window.close();
      }
      if (aCallback) {
        aCallback();
      }
    }.bind(this));
  },

  _observers: [],

  /**
   * Add an observer for Scratchpad events.
   *
   * The observer implements IScratchpadObserver := {
   *   onReady:      Called when the Scratchpad and its SourceEditor are ready.
   *                 Arguments: (Scratchpad aScratchpad)
   * }
   *
   * All observer handlers are optional.
   *
   * @param IScratchpadObserver aObserver
   * @see removeObserver
   */
  addObserver: function SP_addObserver(aObserver)
  {
    this._observers.push(aObserver);
  },

  /**
   * Remove an observer for Scratchpad events.
   *
   * @param IScratchpadObserver aObserver
   * @see addObserver
   */
  removeObserver: function SP_removeObserver(aObserver)
  {
    let index = this._observers.indexOf(aObserver);
    if (index != -1) {
      this._observers.splice(index, 1);
    }
  },

  /**
   * Trigger named handlers in Scratchpad observers.
   *
   * @param string aName
   *        Name of the handler to trigger.
   * @param Array aArgs
   *        Optional array of arguments to pass to the observer(s).
   * @see addObserver
   */
  _triggerObservers: function SP_triggerObservers(aName, aArgs)
  {
    // insert this Scratchpad instance as the first argument
    if (!aArgs) {
      aArgs = [this];
    } else {
      aArgs.unshift(this);
    }

    // trigger all observers that implement this named handler
    for (let i = 0; i < this._observers.length; ++i) {
      let observer = this._observers[i];
      let handler = observer["on" + aName];
      if (handler) {
        handler.apply(observer, aArgs);
      }
    }
  },

  openDocumentationPage: function SP_openDocumentationPage()
  {
    let url = this.strings.GetStringFromName("help.openDocumentationPage");
    let newTab = this.gBrowser.addTab(url);
    this.browserWindow.focus();
    this.gBrowser.selectedTab = newTab;
  },
};

XPCOMUtils.defineLazyGetter(Scratchpad, "strings", function () {
  return Services.strings.createBundle(SCRATCHPAD_L10N);
});

addEventListener("load", Scratchpad.onLoad.bind(Scratchpad), false);
addEventListener("unload", Scratchpad.onUnload.bind(Scratchpad), false);
addEventListener("close", Scratchpad.onClose.bind(Scratchpad), false);
