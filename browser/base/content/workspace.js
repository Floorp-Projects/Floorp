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
 * The Original Code is Workspace.
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

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource:///modules/PropertyPanel.jsm");

const WORKSPACE_CONTEXT_CONTENT = 1;
const WORKSPACE_CONTEXT_CHROME = 2;
const WORKSPACE_WINDOW_URL = "chrome://browser/content/workspace.xul";
const WORKSPACE_L10N = "chrome://browser/locale/workspace.properties";
const WORKSPACE_WINDOW_FEATURES = "chrome,titlebar,toolbar,centerscreen,resizable,dialog=no";

/**
 * The workspace object handles the Workspace window functionality.
 */
var Workspace = {
  /**
   * The script execution context. This tells Workspace in which context the
   * script shall execute.
   *
   * Possible values:
   *   - WORKSPACE_CONTEXT_CONTENT to execute code in the context of the current
   *   tab content window object.
   *   - WORKSPACE_CONTEXT_CHROME to execute code in the context of the
   *   currently active chrome window object.
   */
  executionContext: WORKSPACE_CONTEXT_CONTENT,

  /**
   * Retrieve the xul:textbox DOM element. This element holds the source code
   * the user writes and executes.
   */
  get textbox() document.getElementById("workspace-textbox"),

  /**
   * Retrieve the xul:statusbarpanel DOM element. The status bar tells the
   * current code execution context.
   */
  get statusbarStatus() document.getElementById("workspace-status"),

  /**
   * Get the selected text from the textbox.
   */
  get selectedText()
  {
    return this.textbox.value.substring(this.textbox.selectionStart,
                                        this.textbox.selectionEnd);
  },

  /**
   * Get the most recent chrome window of type navigator:browser.
   */
  get browserWindow() Services.wm.getMostRecentWindow("navigator:browser"),

  /**
   * Get the gBrowser object of the most recent browser window.
   */
  get gBrowser()
  {
    let recentWin = this.browserWindow;
    return recentWin ? recentWin.gBrowser : null;
  },

  /**
   * Get the Cu.Sandbox object for the active tab content window object.
   */
  get contentSandbox()
  {
    if (!this.browserWindow) {
      Cu.reportError(this.strings.
                     GetStringFromName("browserWindow.unavailable"));
      return;
    }

    // TODO: bug 646524 - need to cache sandboxes if
    // currentBrowser == previousBrowser
    let contentWindow = this.gBrowser.selectedBrowser.contentWindow;
    return new Cu.Sandbox(contentWindow,
                          { sandboxPrototype: contentWindow,
                            wantXrays: false });
  },

  /**
   * Get the Cu.Sandbox object for the most recently active navigator:browser
   * chrome window object.
   */
  get chromeSandbox()
  {
    if (!this.browserWindow) {
      Cu.reportError(this.strings.
                     GetStringFromName("browserWindow.unavailable"));
      return;
    }

    return new Cu.Sandbox(this.browserWindow,
                          { sandboxPrototype: this.browserWindow,
                            wantXrays: false });
  },

  /**
   * Drop the textbox selection.
   */
  deselect: function WS_deselect()
  {
    this.textbox.selectionEnd = this.textbox.selectionStart;
  },

  /**
   * Select a specific range in the Workspace xul:textbox.
   *
   * @param number aStart
   *        Selection range start.
   * @param number aEnd
   *        Selection range end.
   */
  selectRange: function WS_selectRange(aStart, aEnd)
  {
    this.textbox.selectionStart = aStart;
    this.textbox.selectionEnd = aEnd;
  },

  /**
   * Evaluate a string in the active tab content window.
   *
   * @param string aString
   *        The script you want evaluated.
   * @return mixed
   *         The script evaluation result.
   */
  evalInContentSandbox: function WS_evalInContentSandbox(aString)
  {
    let result;
    try {
      result = Cu.evalInSandbox(aString, this.contentSandbox, "1.8",
                                "Workspace", 1);
    }
    catch (ex) {
      this.openWebConsole();

      let contentWindow = this.gBrowser.selectedBrowser.contentWindow;

      let scriptError = Cc["@mozilla.org/scripterror;1"].
                        createInstance(Ci.nsIScriptError2);

      scriptError.initWithWindowID(ex.message + "\n" + ex.stack, ex.fileName,
                                   "", ex.lineNumber, 0, scriptError.errorFlag,
                                   "content javascript",
                                   this.getWindowId(contentWindow));

      Services.console.logMessage(scriptError);
    }

    return result;
  },

  /**
   * Evaluate a string in the most recent navigator:browser chrome window.
   *
   * @param string aString
   *        The script you want evaluated.
   * @return mixed
   *         The script evaluation result.
   */
  evalInChromeSandbox: function WS_evalInChromeSandbox(aString)
  {
    let result;
    try {
      result = Cu.evalInSandbox(aString, this.chromeSandbox, "1.8",
                                "Workspace", 1);
    }
    catch (ex) {
      Cu.reportError(ex);
      Cu.reportError(ex.stack);
      this.openErrorConsole();
    }

    return result;
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
  evalForContext: function WS_evaluateForContext(aString)
  {
    return this.executionContext == WORKSPACE_CONTEXT_CONTENT ?
           this.evalInContentSandbox(aString) :
           this.evalInChromeSandbox(aString);
  },

  /**
   * Execute the selected text (if any) or the entire textbox content in the
   * current context.
   */
  execute: function WS_execute()
  {
    let selection = this.selectedText || this.textbox.value;
    let result = this.evalForContext(selection);
    this.deselect();
    return [selection, result];
  },

  /**
   * Execute the selected text (if any) or the entire textbox content in the
   * current context. The resulting object is opened up in the Property Panel
   * for inspection.
   */
  inspect: function WS_inspect()
  {
    let [selection, result] = this.execute();

    if (result) {
      this.openPropertyPanel(selection, result);
    }
  },

  /**
   * Execute the selected text (if any) or the entire textbox content in the
   * current context. The evaluation result is "printed" in the textbox after
   * the selected text, or at the end of the textbox value if there is no
   * selected text.
   */
  print: function WS_print()
  {
    let selectionStart = this.textbox.selectionStart;
    let selectionEnd = this.textbox.selectionEnd;
    if (selectionStart == selectionEnd) {
      selectionEnd = this.textbox.value.length;
    }

    let [selection, result] = this.execute();
    if (!result) {
      return;
    }

    let firstPiece = this.textbox.value.slice(0, selectionEnd);
    let lastPiece = this.textbox.value.
                    slice(selectionEnd, this.textbox.value.length);

    let newComment = "/*\n" + result.toString() + "\n*/";

    this.textbox.value = firstPiece + newComment + lastPiece;

    // Select the added comment.
    this.selectRange(firstPiece.length, firstPiece.length + newComment.length);
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
  openPropertyPanel: function WS_openPropertyPanel(aEvalString, aOutputObject)
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
          try {
            let result = self.evalForContext(aEvalString);

            if (result !== undefined) {
              propPanel.treeView.data = result;
            }
          }
          catch (ex) { }
        }
      });
    }

    let doc = this.browserWindow.document;
    let parent = doc.getElementById("mainPopupSet");
    let title = aOutputObject.toString();
    propPanel = new PropertyPanel(parent, doc, title, aOutputObject, buttons);

    let panel = propPanel.panel;
    panel.setAttribute("class", "workspace_propertyPanel");
    panel.openPopup(null, "after_pointer", 0, 0, false, false);
    panel.sizeTo(200, 400);

    return propPanel;
  },

  // Menu Operations

  /**
   * Open a new Workspace window.
   */
  openWorkspace: function WS_openWorkspace()
  {
    Services.ww.openWindow(null, WORKSPACE_WINDOW_URL, "_blank",
                           WORKSPACE_WINDOW_FEATURES, null);
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
  exportToFile: function WS_exportToFile(aFile, aNoConfirmation, aSilentError,
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
    fs.init(aFile, modeFlags, 0644, fs.DEFER_OPEN);

    let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"].
                    createInstance(Ci.nsIScriptableUnicodeConverter);
    converter.charset = "UTF-8";
    let input = converter.convertToInputStream(this.textbox.value);

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
  importFromFile: function WS_importFromFile(aFile, aSilentError, aCallback)
  {
    // Prevent file type detection.
    let channel = NetUtil.newChannel(aFile);
    channel.contentType = "application/javascript";

    let self = this;
    NetUtil.asyncFetch(channel, function(aInputStream, aStatus) {
      let content = null;

      if (Components.isSuccessCode(aStatus)) {
        content = NetUtil.readInputStreamToString(aInputStream,
                                                  aInputStream.available());
        self.textbox.value = content;
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
   * Open a file to edit in the Workspace.
   */
  openFile: function WS_openFile()
  {
    let fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
    fp.init(window, this.strings.GetStringFromName("openFile.title"),
            Ci.nsIFilePicker.modeOpen);
    fp.defaultString = "";
    if (fp.show() != Ci.nsIFilePicker.returnCancel) {
      document.title = this.filename = fp.file.path;
      this.importFromFile(fp.file);
    }
  },

  /**
   * Save the textbox content to the currently open file.
   */
  saveFile: function WS_saveFile()
  {
    if (!this.filename) {
      return this.saveFileAs();
    }

    let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
    file.initWithPath(this.filename);
    this.exportToFile(file, true);
  },

  /**
   * Save the textbox content to a new file.
   */
  saveFileAs: function WS_saveFileAs()
  {
    let fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
    fp.init(window, this.strings.GetStringFromName("saveFileAs"),
            Ci.nsIFilePicker.modeSave);
    fp.defaultString = "workspace.js";
    if (fp.show() != Ci.nsIFilePicker.returnCancel) {
      document.title = this.filename = fp.file.path;
      this.exportToFile(fp.file);
    }
  },

  /**
   * Open the Error Console.
   */
  openErrorConsole: function WS_openErrorConsole()
  {
    this.browserWindow.toJavaScriptConsole();
  },

  /**
   * Open the Web Console.
   */
  openWebConsole: function WS_openWebConsole()
  {
    if (!this.browserWindow.HUDConsoleUI.getOpenHUD()) {
      this.browserWindow.HUDConsoleUI.toggleHUD();
    }
    this.browserWindow.focus();
  },

  /**
   * Set the current execution context to be the active tab content window.
   */
  setContentContext: function WS_setContentContext()
  {
    let content = document.getElementById("ws-menu-content");
    document.getElementById("ws-menu-chrome").removeAttribute("checked");
    content.setAttribute("checked", true);
    this.statusbarStatus.label = content.getAttribute("label");
    this.executionContext = WORKSPACE_CONTEXT_CONTENT;
  },

  /**
   * Set the current execution context to be the most recent chrome window.
   */
  setChromeContext: function WS_setChromeContext()
  {
    let chrome = document.getElementById("ws-menu-chrome");
    document.getElementById("ws-menu-content").removeAttribute("checked");
    chrome.setAttribute("checked", true);
    this.statusbarStatus.label = chrome.getAttribute("label");
    this.executionContext = WORKSPACE_CONTEXT_CHROME;
  },

  /**
   * Gets the ID of the outer window of the given DOM window object.
   *
   * @param nsIDOMWindow aWindow
   * @return integer
   *         the outer window ID
   */
  getWindowId: function HS_getWindowId(aWindow)
  {
    return aWindow.QueryInterface(Ci.nsIInterfaceRequestor).
           getInterface(Ci.nsIDOMWindowUtils).outerWindowID;
  },
};

XPCOMUtils.defineLazyGetter(Workspace, "strings", function () {
  return Services.strings.createBundle(WORKSPACE_L10N);
});

