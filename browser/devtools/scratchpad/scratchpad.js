/* vim:set ts=2 sw=2 sts=2 et:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
Cu.import("resource:///modules/source-editor.jsm");
Cu.import("resource:///modules/devtools/LayoutHelpers.jsm");
Cu.import("resource:///modules/devtools/scratchpad-manager.jsm");
Cu.import("resource://gre/modules/jsdebugger.jsm");
Cu.import("resource:///modules/devtools/gDevTools.jsm");
Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource://gre/modules/commonjs/sdk/core/promise.js");

XPCOMUtils.defineLazyModuleGetter(this, "VariablesView",
                                  "resource:///modules/devtools/VariablesView.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "devtools",
                                  "resource://gre/modules/devtools/Loader.jsm");

let Telemetry = devtools.require("devtools/shared/telemetry");

const SCRATCHPAD_CONTEXT_CONTENT = 1;
const SCRATCHPAD_CONTEXT_BROWSER = 2;
const SCRATCHPAD_L10N = "chrome://browser/locale/devtools/scratchpad.properties";
const DEVTOOLS_CHROME_ENABLED = "devtools.chrome.enabled";
const PREF_RECENT_FILES_MAX = "devtools.scratchpad.recentFilesMax";
const BUTTON_POSITION_SAVE = 0;
const BUTTON_POSITION_CANCEL = 1;
const BUTTON_POSITION_DONT_SAVE = 2;
const BUTTON_POSITION_REVERT = 0;
const VARIABLES_VIEW_URL = "chrome://browser/content/devtools/widgets/VariablesView.xul";

// Because we have no constructor / destructor where we can log metrics we need
// to do so here.
let telemetry = new Telemetry();
telemetry.toolOpened("scratchpad");

/**
 * The scratchpad object handles the Scratchpad window functionality.
 */
var Scratchpad = {
  _instanceId: null,
  _initialWindowTitle: document.title,

  /**
   * Check if provided string is a mode-line and, if it is, return an
   * object with its values.
   *
   * @param string aLine
   * @return string
   */
  _scanModeLine: function SP__scanModeLine(aLine="")
  {
    aLine = aLine.trim();

    let obj = {};
    let ch1 = aLine.charAt(0);
    let ch2 = aLine.charAt(1);

    if (ch1 !== "/" || (ch2 !== "*" && ch2 !== "/")) {
      return obj;
    }

    aLine = aLine
      .replace(/^\/\//, "")
      .replace(/^\/\*/, "")
      .replace(/\*\/$/, "");

    aLine.split(",").forEach(pair => {
      let [key, val] = pair.split(":");

      if (key && val) {
        obj[key.trim()] = val.trim();
      }
    });

    return obj;
  },

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
    let title = this.filename || this._initialWindowTitle;

    if (this.editor && this.editor.dirty) {
      title = "*" + title;
    }

    document.title = title;
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
  setState: function SP_setState(aState)
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
   * Unique name for the current Scratchpad instance. Used to distinguish
   * Scratchpad windows between each other. See bug 661762.
   */
  get uniqueName()
  {
    return "Scratchpad/" + this._instanceId;
  },


  /**
   * Sidebar that contains the VariablesView for object inspection.
   */
  get sidebar()
  {
    if (!this._sidebar) {
      this._sidebar = new ScratchpadSidebar(this);
    }
    return this._sidebar;
  },

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
      this._contentSandbox.__SCRATCHPAD__ = this;

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
      this._chromeSandbox.__SCRATCHPAD__ = this;
      addDebuggerToGlobal(this._chromeSandbox);

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
   * Evaluate a string in the currently desired context, that is either the
   * chrome window or the tab content window object.
   *
   * @param string aString
   *        The script you want to evaluate.
   * @return Promise
   *         The promise for the script evaluation result.
   */
  evalForContext: function SP_evaluateForContext(aString)
  {
    let deferred = Promise.defer();

    // This setTimeout is temporary and will be replaced by DebuggerClient
    // execution in a future patch (bug 825039). The purpose for using
    // setTimeout is to ensure there is no accidental dependency on the
    // promise being resolved synchronously, which can cause subtle bugs.
    setTimeout(() => {
      let chrome = this.executionContext != SCRATCHPAD_CONTEXT_CONTENT;
      let sandbox = chrome ? this.chromeSandbox : this.contentSandbox;
      let name = this.uniqueName;

      try {
        let result = Cu.evalInSandbox(aString, sandbox, "1.8", name, 1);
        deferred.resolve([aString, undefined, result]);
      }
      catch (ex) {
        deferred.resolve([aString, ex]);
      }
    }, 0);

    return deferred.promise;
  },

  /**
   * Execute the selected text (if any) or the entire editor content in the
   * current context.
   *
   * @return Promise
   *         The promise for the script evaluation result.
   */
  execute: function SP_execute()
  {
    let selection = this.selectedText || this.getText();
    return this.evalForContext(selection);
  },

  /**
   * Execute the selected text (if any) or the entire editor content in the
   * current context.
   *
   * @return Promise
   *         The promise for the script evaluation result.
   */
  run: function SP_run()
  {
    let promise = this.execute();
    promise.then(([, aError, ]) => {
      if (aError) {
        this.writeAsErrorComment(aError);
      }
      else {
        this.deselect();
      }
    });
    return promise;
  },

  /**
   * Execute the selected text (if any) or the entire editor content in the
   * current context. If the result is primitive then it is written as a
   * comment. Otherwise, the resulting object is inspected up in the sidebar.
   *
   * @return Promise
   *         The promise for the script evaluation result.
   */
  inspect: function SP_inspect()
  {
    let deferred = Promise.defer();
    let reject = aReason => deferred.reject(aReason);

    this.execute().then(([aString, aError, aResult]) => {
      let resolve = () => deferred.resolve([aString, aError, aResult]);

      if (aError) {
        this.writeAsErrorComment(aError);
        resolve();
      }
      else if (!isObject(aResult)) {
        this.writeAsComment(aResult);
        resolve();
      }
      else {
        this.deselect();
        this.sidebar.open(aString, aResult).then(resolve, reject);
      }
    }, reject);

    return deferred.promise;
  },

  /**
   * Reload the current page and execute the entire editor content when
   * the page finishes loading. Note that this operation should be available
   * only in the content context.
   *
   * @return Promise
   *         The promise for the script evaluation result.
   */
  reloadAndRun: function SP_reloadAndRun()
  {
    let deferred = Promise.defer();

    if (this.executionContext !== SCRATCHPAD_CONTEXT_CONTENT) {
      Cu.reportError(this.strings.
          GetStringFromName("scratchpadContext.invalid"));
      return;
    }

    let browser = this.gBrowser.selectedBrowser;

    this._reloadAndRunEvent = evt => {
      if (evt.target !== browser.contentDocument) {
        return;
      }

      browser.removeEventListener("load", this._reloadAndRunEvent, true);

      this.run().then(aResults => deferred.resolve(aResults));
    };

    browser.addEventListener("load", this._reloadAndRunEvent, true);
    browser.contentWindow.location.reload();

    return deferred.promise;
  },

  /**
   * Execute the selected text (if any) or the entire editor content in the
   * current context. The evaluation result is inserted into the editor after
   * the selected text, or at the end of the editor content if there is no
   * selected text.
   *
   * @return Promise
   *         The promise for the script evaluation result.
   */
  display: function SP_display()
  {
    let promise = this.execute();
    promise.then(([aString, aError, aResult]) => {
      if (aError) {
        this.writeAsErrorComment(aError);
      }
      else {
        this.writeAsComment(aResult);
      }
    });
    return promise;
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
    let stack = "";
    if (aError.stack) {
      stack = aError.stack;
    }
    else if (aError.fileName) {
      if (aError.lineNumber) {
        stack = "@" + aError.fileName + ":" + aError.lineNumber;
      }
      else {
        stack = "@" + aError.fileName;
      }
    }
    else if (aError.lineNumber) {
      stack = "@" + aError.lineNumber;
    }

    let newComment = "Exception: " + ( aError.message || aError) + ( stack == "" ? stack : "\n" + stack.replace(/\n$/, "") );

    this.writeAsComment(newComment);
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

    let encoder = new TextEncoder();
    let buffer = encoder.encode(this.getText());
    let promise = OS.File.writeAtomic(aFile.path, buffer,{tmpPath: aFile.path + ".tmp"});
    promise.then(value => {
      if (aCallback) {
        aCallback.call(this, Components.results.NS_OK);
      }
    }, reason => {
      if (!aSilentError) {
        window.alert(this.strings.GetStringFromName("saveFile.failed"));
      }
      if (aCallback) {
        aCallback.call(this, Components.results.NS_ERROR_UNEXPECTED);
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

    NetUtil.asyncFetch(channel, (aInputStream, aStatus) => {
      let content = null;

      if (Components.isSuccessCode(aStatus)) {
        let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"].
                        createInstance(Ci.nsIScriptableUnicodeConverter);
        converter.charset = "UTF-8";
        content = NetUtil.readInputStreamToString(aInputStream,
                                                  aInputStream.available());
        content = converter.ConvertToUnicode(content);

        // Check to see if the first line is a mode-line comment.
        let line = content.split("\n")[0];
        let modeline = this._scanModeLine(line);
        let chrome = Services.prefs.getBoolPref(DEVTOOLS_CHROME_ENABLED);

        if (chrome && modeline["-sp-context"] === "browser") {
          this.setBrowserContext();
        }

        this.setText(content);
        this.editor.resetUndo();
      }
      else if (!aSilentError) {
        window.alert(this.strings.GetStringFromName("openFile.failed"));
      }

      if (aCallback) {
        aCallback.call(this, aStatus, content);
      }
    });
  },

  /**
   * Open a file to edit in the Scratchpad.
   *
   * @param integer aIndex
   *        Optional integer: clicked menuitem in the 'Open Recent'-menu.
   */
  openFile: function SP_openFile(aIndex)
  {
    let promptCallback = aFile => {
      this.promptSave((aCloseFile, aSaved, aStatus) => {
        let shouldOpen = aCloseFile;
        if (aSaved && !Components.isSuccessCode(aStatus)) {
          shouldOpen = false;
        }

        if (shouldOpen) {
          let file;
          if (aFile) {
            file = aFile;
          } else {
            file = Components.classes["@mozilla.org/file/local;1"].
                   createInstance(Components.interfaces.nsILocalFile);
            let filePath = this.getRecentFiles()[aIndex];
            file.initWithPath(filePath);
          }

          if (!file.exists()) {
            this.notificationBox.appendNotification(
              this.strings.GetStringFromName("fileNoLongerExists.notification"),
              "file-no-longer-exists",
              null,
              this.notificationBox.PRIORITY_WARNING_HIGH,
              null);

            this.clearFiles(aIndex, 1);
            return;
          }

          this.setFilename(file.path);
          this.importFromFile(file, false);
          this.setRecentFile(file);
        }
      });
    };

    if (aIndex > -1) {
      promptCallback();
    } else {
      let fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
      fp.init(window, this.strings.GetStringFromName("openFile.title"),
              Ci.nsIFilePicker.modeOpen);
      fp.defaultString = "";
      fp.open(aResult => {
        if (aResult != Ci.nsIFilePicker.returnCancel) {
          promptCallback(fp.file);
        }
      });
    }
  },

  /**
   * Get recent files.
   *
   * @return Array
   *         File paths.
   */
  getRecentFiles: function SP_getRecentFiles()
  {
    let branch = Services.prefs.getBranch("devtools.scratchpad.");
    let filePaths = [];

    // WARNING: Do not use getCharPref here, it doesn't play nicely with
    // Unicode strings.

    if (branch.prefHasUserValue("recentFilePaths")) {
      let data = branch.getComplexValue("recentFilePaths",
        Ci.nsISupportsString).data;
      filePaths = JSON.parse(data);
    }

    return filePaths;
  },

  /**
   * Save a recent file in a JSON parsable string.
   *
   * @param nsILocalFile aFile
   *        The nsILocalFile we want to save as a recent file.
   */
  setRecentFile: function SP_setRecentFile(aFile)
  {
    let maxRecent = Services.prefs.getIntPref(PREF_RECENT_FILES_MAX);
    if (maxRecent < 1) {
      return;
    }

    let filePaths = this.getRecentFiles();
    let filesCount = filePaths.length;
    let pathIndex = filePaths.indexOf(aFile.path);

    // We are already storing this file in the list of recent files.
    if (pathIndex > -1) {
      // If it's already the most recent file, we don't have to do anything.
      if (pathIndex === (filesCount - 1)) {
        // Updating the menu to clear the disabled state from the wrong menuitem
        // in rare cases when two or more Scratchpad windows are open and the
        // same file has been opened in two or more windows.
        this.populateRecentFilesMenu();
        return;
      }

      // It is not the most recent file. Remove it from the list, we add it as
      // the most recent farther down.
      filePaths.splice(pathIndex, 1);
    }
    // If we are not storing the file and the 'recent files'-list is full,
    // remove the oldest file from the list.
    else if (filesCount === maxRecent) {
      filePaths.shift();
    }

    filePaths.push(aFile.path);

    // WARNING: Do not use setCharPref here, it doesn't play nicely with
    // Unicode strings.

    let str = Cc["@mozilla.org/supports-string;1"]
      .createInstance(Ci.nsISupportsString);
    str.data = JSON.stringify(filePaths);

    let branch = Services.prefs.getBranch("devtools.scratchpad.");
    branch.setComplexValue("recentFilePaths",
      Ci.nsISupportsString, str);
  },

  /**
   * Populates the 'Open Recent'-menu.
   */
  populateRecentFilesMenu: function SP_populateRecentFilesMenu()
  {
    let maxRecent = Services.prefs.getIntPref(PREF_RECENT_FILES_MAX);
    let recentFilesMenu = document.getElementById("sp-open_recent-menu");

    if (maxRecent < 1) {
      recentFilesMenu.setAttribute("hidden", true);
      return;
    }

    let recentFilesPopup = recentFilesMenu.firstChild;
    let filePaths = this.getRecentFiles();
    let filename = this.getState().filename;

    recentFilesMenu.setAttribute("disabled", true);
    while (recentFilesPopup.hasChildNodes()) {
      recentFilesPopup.removeChild(recentFilesPopup.firstChild);
    }

    if (filePaths.length > 0) {
      recentFilesMenu.removeAttribute("disabled");

      // Print out menuitems with the most recent file first.
      for (let i = filePaths.length - 1; i >= 0; --i) {
        let menuitem = document.createElement("menuitem");
        menuitem.setAttribute("type", "radio");
        menuitem.setAttribute("label", filePaths[i]);

        if (filePaths[i] === filename) {
          menuitem.setAttribute("checked", true);
          menuitem.setAttribute("disabled", true);
        }

        menuitem.setAttribute("oncommand", "Scratchpad.openFile(" + i + ");");
        recentFilesPopup.appendChild(menuitem);
      }

      recentFilesPopup.appendChild(document.createElement("menuseparator"));
      let clearItems = document.createElement("menuitem");
      clearItems.setAttribute("id", "sp-menu-clear_recent");
      clearItems.setAttribute("label",
                              this.strings.
                              GetStringFromName("clearRecentMenuItems.label"));
      clearItems.setAttribute("command", "sp-cmd-clearRecentFiles");
      recentFilesPopup.appendChild(clearItems);
    }
  },

  /**
   * Clear a range of files from the list.
   *
   * @param integer aIndex
   *        Index of file in menu to remove.
   * @param integer aLength
   *        Number of files from the index 'aIndex' to remove.
   */
  clearFiles: function SP_clearFile(aIndex, aLength)
  {
    let filePaths = this.getRecentFiles();
    filePaths.splice(aIndex, aLength);

    // WARNING: Do not use setCharPref here, it doesn't play nicely with
    // Unicode strings.

    let str = Cc["@mozilla.org/supports-string;1"]
      .createInstance(Ci.nsISupportsString);
    str.data = JSON.stringify(filePaths);

    let branch = Services.prefs.getBranch("devtools.scratchpad.");
    branch.setComplexValue("recentFilePaths",
      Ci.nsISupportsString, str);
  },

  /**
   * Clear all recent files.
   */
  clearRecentFiles: function SP_clearRecentFiles()
  {
    Services.prefs.clearUserPref("devtools.scratchpad.recentFilePaths");
  },

  /**
   * Handle changes to the 'PREF_RECENT_FILES_MAX'-preference.
   */
  handleRecentFileMaxChange: function SP_handleRecentFileMaxChange()
  {
    let maxRecent = Services.prefs.getIntPref(PREF_RECENT_FILES_MAX);
    let menu = document.getElementById("sp-open_recent-menu");

    // Hide the menu if the 'PREF_RECENT_FILES_MAX'-pref is set to zero or less.
    if (maxRecent < 1) {
      menu.setAttribute("hidden", true);
    } else {
      if (menu.hasAttribute("hidden")) {
        if (!menu.firstChild.hasChildNodes()) {
          this.populateRecentFilesMenu();
        }

        menu.removeAttribute("hidden");
      }

      let filePaths = this.getRecentFiles();
      if (maxRecent < filePaths.length) {
        let diff = filePaths.length - maxRecent;
        this.clearFiles(0, diff);
      }
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

    this.exportToFile(file, true, false, aStatus => {
      if (Components.isSuccessCode(aStatus)) {
        this.editor.dirty = false;
        this.setRecentFile(file);
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
    let fpCallback = aResult => {
      if (aResult != Ci.nsIFilePicker.returnCancel) {
        this.setFilename(fp.file.path);
        this.exportToFile(fp.file, true, false, aStatus => {
          if (Components.isSuccessCode(aStatus)) {
            this.editor.dirty = false;
            this.setRecentFile(fp.file);
          }
          if (aCallback) {
            aCallback(aStatus);
          }
        });
      }
    };

    fp.init(window, this.strings.GetStringFromName("saveFileAs"),
            Ci.nsIFilePicker.modeSave);
    fp.defaultString = "scratchpad.js";
    fp.open(fpCallback);
  },

  /**
   * Restore content from saved version of current file.
   *
   * @param function aCallback
   *        Optional function you want to call when file is saved
   */
  revertFile: function SP_revertFile(aCallback)
  {
    let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
    file.initWithPath(this.filename);

    if (!file.exists()) {
      return;
    }

    this.importFromFile(file, false, (aStatus, aContent) => {
      if (aCallback) {
        aCallback(aStatus);
      }
    });
  },

  /**
   * Prompt to revert scratchpad if it has unsaved changes.
   *
   * @param function aCallback
   *        Optional function you want to call when file is saved. The callback
   *        receives three arguments:
   *          - aRevert (boolean) - tells if the file has been reverted.
   *          - status (number) - the file revert status result (if the file was
   *          saved).
   */
  promptRevert: function SP_promptRervert(aCallback)
  {
    if (this.filename) {
      let ps = Services.prompt;
      let flags = ps.BUTTON_POS_0 * ps.BUTTON_TITLE_REVERT +
                  ps.BUTTON_POS_1 * ps.BUTTON_TITLE_CANCEL;

      let button = ps.confirmEx(window,
                          this.strings.GetStringFromName("confirmRevert.title"),
                          this.strings.GetStringFromName("confirmRevert"),
                          flags, null, null, null, null, {});
      if (button == BUTTON_POSITION_CANCEL) {
        if (aCallback) {
          aCallback(false);
        }

        return;
      }
      if (button == BUTTON_POSITION_REVERT) {
        this.revertFile(aStatus => {
          if (aCallback) {
            aCallback(true, aStatus);
          }
        });

        return;
      }
    }
    if (aCallback) {
      aCallback(false);
    }
  },

  /**
   * Open the Error Console.
   */
  openErrorConsole: function SP_openErrorConsole()
  {
    this.browserWindow.HUDConsoleUI.toggleBrowserConsole();
  },

  /**
   * Open the Web Console.
   */
  openWebConsole: function SP_openWebConsole()
  {
    let target = devtools.TargetFactory.forTab(this.gBrowser.selectedTab);
    gDevTools.showToolbox(target, "webconsole");
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
    document.getElementById("sp-cmd-reloadAndRun").removeAttribute("disabled");
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
    let reloadAndRun = document.getElementById("sp-cmd-reloadAndRun");

    document.getElementById("sp-menu-content").removeAttribute("checked");
    reloadAndRun.setAttribute("disabled", true);
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

    let initialText = this.strings.formatStringFromName(
      "scratchpadIntro1",
      [LayoutHelpers.prettyKey(document.getElementById("sp-key-run")),
       LayoutHelpers.prettyKey(document.getElementById("sp-key-inspect")),
       LayoutHelpers.prettyKey(document.getElementById("sp-key-display"))],
      3);

    let args = window.arguments;

    if (args && args[0] instanceof Ci.nsIDialogParamBlock) {
      args = args[0];
    } else {
      // If this Scratchpad window doesn't have any arguments, horrible
      // things might happen so we need to report an error.
      Cu.reportError(this.strings. GetStringFromName("scratchpad.noargs"));
    }

    this._instanceId = args.GetString(0);

    let state = args.GetString(1) || null;
    if (state) {
      state = JSON.parse(state);
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

    this.populateRecentFilesMenu();
    PreferenceObserver.init();
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
    if (Scratchpad.filename) {
      if (Scratchpad.editor.dirty) {
        document.getElementById("sp-cmd-revert").removeAttribute("disabled");
      }
      else {
        document.getElementById("sp-cmd-revert").setAttribute("disabled", true);
      }
    }
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

    // This event is created only after user uses 'reload and run' feature.
    if (this._reloadAndRunEvent) {
      this.gBrowser.selectedBrowser.removeEventListener("load",
          this._reloadAndRunEvent, true);
    }

    this.editor.removeEventListener(SourceEditor.EVENTS.DIRTY_CHANGED,
                                    this._onDirtyChanged);
    PreferenceObserver.uninit();

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
    if (this.editor.dirty) {
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
        this.saveFile(aStatus => {
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
   * @param function aCallback
   *        Optional function you want to call when file is saved/closed.
   *        Used mainly for tests.
   */
  onClose: function SP_onClose(aEvent, aCallback)
  {
    aEvent.preventDefault();
    this.close(aCallback);
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
    this.promptSave((aShouldClose, aSaved, aStatus) => {
      let shouldClose = aShouldClose;
      if (aSaved && !Components.isSuccessCode(aStatus)) {
        shouldClose = false;
      }

      if (shouldClose) {
        telemetry.toolClosed("scratchpad");
        window.close();
      }
      if (aCallback) {
        aCallback();
      }
    });
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


/**
 * Encapsulates management of the sidebar containing the VariablesView for
 * object inspection.
 */
function ScratchpadSidebar(aScratchpad)
{
  let ToolSidebar = devtools.require("devtools/framework/sidebar").ToolSidebar;
  let tabbox = document.querySelector("#scratchpad-sidebar");
  this._sidebar = new ToolSidebar(tabbox, this, "scratchpad");
  this._scratchpad = aScratchpad;
}

ScratchpadSidebar.prototype = {
  /*
   * The ToolSidebar for this sidebar.
   */
  _sidebar: null,

  /*
   * The VariablesView for this sidebar.
   */
  variablesView: null,

  /*
   * Whether the sidebar is currently shown.
   */
  visible: false,

  /**
   * Open the sidebar, if not open already, and populate it with the properties
   * of the given object.
   *
   * @param string aString
   *        The string that was evaluated.
   * @param object aObject
   *        The object to inspect, which is the aEvalString evaluation result.
   * @return Promise
   *         A promise that will resolve once the sidebar is open.
   */
  open: function SS_open(aEvalString, aObject)
  {
    this.show();

    let deferred = Promise.defer();

    let onTabReady = () => {
      if (!this.variablesView) {
        let window = this._sidebar.getWindowForTab("variablesview");
        let container = window.document.querySelector("#variables");
        this.variablesView = new VariablesView(container, {
          searchEnabled: true,
          searchPlaceholder: this._scratchpad.strings
                             .GetStringFromName("propertiesFilterPlaceholder")
        });
      }
      this._update(aObject).then(() => deferred.resolve());
    };

    if (this._sidebar.getCurrentTabID() == "variablesview") {
      onTabReady();
    }
    else {
      this._sidebar.once("variablesview-ready", onTabReady);
      this._sidebar.addTab("variablesview", VARIABLES_VIEW_URL, true);
    }

    return deferred.promise;
  },

  /**
   * Show the sidebar.
   */
  show: function SS_show()
  {
    if (!this.visible) {
      this.visible = true;
      this._sidebar.show();
    }
  },

  /**
   * Hide the sidebar.
   */
  hide: function SS_hide()
  {
    if (this.visible) {
      this.visible = false;
      this._sidebar.hide();
    }
  },

  /**
   * Update the object currently inspected by the sidebar.
   *
   * @param object aObject
   *        The object to inspect in the sidebar.
   * @return Promise
   *         A promise that resolves when the update completes.
   */
  _update: function SS__update(aObject)
  {
    let deferred = Promise.defer();

    this.variablesView.rawObject = aObject;

    // In the future this will work on remote values (bug 825039).
    setTimeout(() => deferred.resolve(), 0);
    return deferred.promise;
  }
};


/**
 * Check whether a value is non-primitive.
 */
function isObject(aValue)
{
  let type = typeof aValue;
  return type == "object" ? aValue != null : type == "function";
}


/**
 * The PreferenceObserver listens for preference changes while Scratchpad is
 * running.
 */
var PreferenceObserver = {
  _initialized: false,

  init: function PO_init()
  {
    if (this._initialized) {
      return;
    }

    this.branch = Services.prefs.getBranch("devtools.scratchpad.");
    this.branch.addObserver("", this, false);
    this._initialized = true;
  },

  observe: function PO_observe(aMessage, aTopic, aData)
  {
    if (aTopic != "nsPref:changed") {
      return;
    }

    if (aData == "recentFilesMax") {
      Scratchpad.handleRecentFileMaxChange();
    }
    else if (aData == "recentFilePaths") {
      Scratchpad.populateRecentFilesMenu();
    }
  },

  uninit: function PO_uninit () {
    if (!this.branch) {
      return;
    }

    this.branch.removeObserver("", this);
    this.branch = null;
  }
};

XPCOMUtils.defineLazyGetter(Scratchpad, "strings", function () {
  return Services.strings.createBundle(SCRATCHPAD_L10N);
});

addEventListener("load", Scratchpad.onLoad.bind(Scratchpad), false);
addEventListener("unload", Scratchpad.onUnload.bind(Scratchpad), false);
addEventListener("close", Scratchpad.onClose.bind(Scratchpad), false);
