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

const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;

const SCRATCHPAD_CONTEXT_CONTENT = 1;
const SCRATCHPAD_CONTEXT_BROWSER = 2;
const BUTTON_POSITION_SAVE       = 0;
const BUTTON_POSITION_CANCEL     = 1;
const BUTTON_POSITION_DONT_SAVE  = 2;
const BUTTON_POSITION_REVERT     = 0;
const EVAL_FUNCTION_TIMEOUT      = 1000; // milliseconds

const SCRATCHPAD_L10N = "chrome://browser/locale/devtools/scratchpad.properties";
const DEVTOOLS_CHROME_ENABLED = "devtools.chrome.enabled";
const PREF_RECENT_FILES_MAX = "devtools.scratchpad.recentFilesMax";

const VARIABLES_VIEW_URL = "chrome://browser/content/devtools/widgets/VariablesView.xul";

const require   = Cu.import("resource://gre/modules/devtools/Loader.jsm", {}).devtools.require;

const Telemetry = require("devtools/shared/telemetry");
const Editor    = require("devtools/sourceeditor/editor");
const TargetFactory = require("devtools/framework/target").TargetFactory;

const { Promise: promise } = Cu.import("resource://gre/modules/Promise.jsm", {});
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource:///modules/devtools/scratchpad-manager.jsm");
Cu.import("resource://gre/modules/jsdebugger.jsm");
Cu.import("resource:///modules/devtools/gDevTools.jsm");
Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource:///modules/devtools/ViewHelpers.jsm");
Cu.import("resource://gre/modules/reflect.jsm");
Cu.import("resource://gre/modules/devtools/DevToolsUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "VariablesView",
  "resource:///modules/devtools/VariablesView.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "VariablesViewController",
  "resource:///modules/devtools/VariablesViewController.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "EnvironmentClient",
  "resource://gre/modules/devtools/dbg-client.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "ObjectClient",
  "resource://gre/modules/devtools/dbg-client.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "WebConsoleUtils",
  "resource://gre/modules/devtools/WebConsoleUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "DebuggerServer",
  "resource://gre/modules/devtools/dbg-server.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "DebuggerClient",
  "resource://gre/modules/devtools/dbg-client.jsm");

XPCOMUtils.defineLazyGetter(this, "REMOTE_TIMEOUT", () =>
  Services.prefs.getIntPref("devtools.debugger.remote-timeout"));

XPCOMUtils.defineLazyModuleGetter(this, "ShortcutUtils",
  "resource://gre/modules/ShortcutUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Reflect",
  "resource://gre/modules/reflect.jsm");

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
  _dirty: false,

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
   * Returns the 'dirty' state of this Scratchpad.
   */
  get dirty()
  {
    let clean = this.editor && this.editor.isClean();
    return this._dirty || !clean;
  },

  /**
   * Sets the 'dirty' state of this Scratchpad.
   */
  set dirty(aValue)
  {
    this._dirty = aValue;
    if (!aValue && this.editor)
      this.editor.setClean();
    this._updateTitle();
  },

  /**
   * Retrieve the xul:notificationbox DOM element. It notifies the user when
   * the current code execution context is SCRATCHPAD_CONTEXT_BROWSER.
   */
  get notificationBox()
  {
    return document.getElementById("scratchpad-notificationbox");
  },

  /**
   * Hide the menu bar.
   */
  hideMenu: function SP_hideMenu()
  {
    document.getElementById("sp-menubar").style.display = "none";
  },

  /**
   * Show the menu bar.
   */
  showMenu: function SP_showMenu()
  {
    document.getElementById("sp-menubar").style.display = "";
  },

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
    var value = this.editor.getText();
    return value.slice(aStart || 0, aEnd || value.length);
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

    if (this.dirty)
      title = "*" + title;

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
      saved: !this.dirty
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
    if (aState.filename)
      this.setFilename(aState.filename);

    this.dirty = !aState.saved;

    if (aState.executionContext == SCRATCHPAD_CONTEXT_BROWSER)
      this.setBrowserContext();
    else
      this.setContentContext();
  },

  /**
   * Get the most recent chrome window of type navigator:browser.
   */
  get browserWindow()
  {
    return Services.wm.getMostRecentWindow("navigator:browser");
  },

  /**
   * Get the gBrowser object of the most recent browser window.
   */
  get gBrowser()
  {
    let recentWin = this.browserWindow;
    return recentWin ? recentWin.gBrowser : null;
  },

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
   * Replaces context of an editor with provided value (a string).
   * Note: this method is simply a shortcut to editor.setText.
   */
  setText: function SP_setText(value)
  {
    return this.editor.setText(value);
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
  evaluate: function SP_evaluate(aString)
  {
    let connection;
    if (this.target) {
      connection = ScratchpadTarget.consoleFor(this.target);
    }
    else if (this.executionContext == SCRATCHPAD_CONTEXT_CONTENT) {
      connection = ScratchpadTab.consoleFor(this.gBrowser.selectedTab);
    }
    else {
      connection = ScratchpadWindow.consoleFor(this.browserWindow);
    }

    let evalOptions = { url: this.uniqueName };

    return connection.then(({ debuggerClient, webConsoleClient }) => {
      let deferred = promise.defer();

      webConsoleClient.evaluateJS(aString, aResponse => {
        this.debuggerClient = debuggerClient;
        this.webConsoleClient = webConsoleClient;
        if (aResponse.error) {
          deferred.reject(aResponse);
        }
        else if (aResponse.exception !== null) {
          deferred.resolve([aString, aResponse]);
        }
        else {
          deferred.resolve([aString, undefined, aResponse.result]);
        }
      }, evalOptions);

      return deferred.promise;
    });
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
    let selection = this.editor.getSelection() || this.getText();
    return this.evaluate(selection);
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
    let deferred = promise.defer();
    let reject = aReason => deferred.reject(aReason);

    this.execute().then(([aString, aError, aResult]) => {
      let resolve = () => deferred.resolve([aString, aError, aResult]);

      if (aError) {
        this.writeAsErrorComment(aError.exception).then(resolve, reject);
      }
      else {
        this.editor.dropSelection();
        resolve();
      }
    }, reject);

    return deferred.promise;
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
    let deferred = promise.defer();
    let reject = aReason => deferred.reject(aReason);

    this.execute().then(([aString, aError, aResult]) => {
      let resolve = () => deferred.resolve([aString, aError, aResult]);

      if (aError) {
        this.writeAsErrorComment(aError.exception).then(resolve, reject);
      }
      else if (VariablesView.isPrimitive({ value: aResult })) {
        this._writePrimitiveAsComment(aResult).then(resolve, reject);
      }
      else {
        this.editor.dropSelection();
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
    let deferred = promise.defer();

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
    let deferred = promise.defer();
    let reject = aReason => deferred.reject(aReason);

    this.execute().then(([aString, aError, aResult]) => {
      let resolve = () => deferred.resolve([aString, aError, aResult]);

      if (aError) {
        this.writeAsErrorComment(aError.exception).then(resolve, reject);
      }
      else if (VariablesView.isPrimitive({ value: aResult })) {
        this._writePrimitiveAsComment(aResult).then(resolve, reject);
      }
      else {
        let objectClient = new ObjectClient(this.debuggerClient, aResult);
        objectClient.getDisplayString(aResponse => {
          if (aResponse.error) {
            reportError("display", aResponse);
            reject(aResponse);
          }
          else {
            this.writeAsComment(aResponse.displayString);
            resolve();
          }
        });
      }
    }, reject);

    return deferred.promise;
  },

  _prettyPrintWorker: null,

  /**
   * Get or create the worker that handles pretty printing.
   */
  get prettyPrintWorker() {
    if (!this._prettyPrintWorker) {
      this._prettyPrintWorker = new ChromeWorker(
        "resource://gre/modules/devtools/server/actors/pretty-print-worker.js");

      this._prettyPrintWorker.addEventListener("error", ({ message, filename, lineno }) => {
        DevToolsUtils.reportException(message + " @ " + filename + ":" + lineno);
      }, false);
    }
    return this._prettyPrintWorker;
  },

  /**
   * Pretty print the source text inside the scratchpad.
   *
   * @return Promise
   *         A promise resolved with the pretty printed code, or rejected with
   *         an error.
   */
  prettyPrint: function SP_prettyPrint() {
    const uglyText = this.getText();
    const tabsize = Services.prefs.getIntPref("devtools.editor.tabsize");
    const id = Math.random();
    const deferred = promise.defer();

    const onReply = ({ data }) => {
      if (data.id !== id) {
        return;
      }
      this.prettyPrintWorker.removeEventListener("message", onReply, false);

      if (data.error) {
        let errorString = DevToolsUtils.safeErrorString(data.error);
        this.writeAsErrorComment(errorString);
        deferred.reject(errorString);
      } else {
        this.editor.setText(data.code);
        deferred.resolve(data.code);
      }
    };

    this.prettyPrintWorker.addEventListener("message", onReply, false);
    this.prettyPrintWorker.postMessage({
      id: id,
      url: "(scratchpad)",
      indent: tabsize,
      source: uglyText
    });

    return deferred.promise;
  },

  /**
   * Parse the text and return an AST. If we can't parse it, write an error
   * comment and return false.
   */
  _parseText: function SP__parseText(aText) {
    try {
      return Reflect.parse(aText);
    } catch (e) {
      this.writeAsErrorComment(DevToolsUtils.safeErrorString(e));
      return false;
    }
  },

  /**
   * Determine if the given AST node location contains the given cursor
   * position.
   *
   * @returns Boolean
   */
  _containsCursor: function (aLoc, aCursorPos) {
    // Our line numbers are 1-based, while CodeMirror's are 0-based.
    const lineNumber = aCursorPos.line + 1;
    const columnNumber = aCursorPos.ch;

    if (aLoc.start.line <= lineNumber && aLoc.end.line >= lineNumber) {
      if (aLoc.start.line === aLoc.end.line) {
        return aLoc.start.column <= columnNumber
          && aLoc.end.column >= columnNumber;
      }

      if (aLoc.start.line == lineNumber) {
        return columnNumber >= aLoc.start.column;
      }

      if (aLoc.end.line == lineNumber) {
        return columnNumber <= aLoc.end.column;
      }

      return true;
    }

    return false;
  },

  /**
   * Find the top level function AST node that the cursor is within.
   *
   * @returns Object|null
   */
  _findTopLevelFunction: function SP__findTopLevelFunction(aAst, aCursorPos) {
    for (let statement of aAst.body) {
      switch (statement.type) {
      case "FunctionDeclaration":
        if (this._containsCursor(statement.loc, aCursorPos)) {
          return statement;
        }
        break;

      case "VariableDeclaration":
        for (let decl of statement.declarations) {
          if (!decl.init) {
            continue;
          }
          if ((decl.init.type == "FunctionExpression"
               || decl.init.type == "ArrowExpression")
              && this._containsCursor(decl.loc, aCursorPos)) {
            return decl;
          }
        }
        break;
      }
    }

    return null;
  },

  /**
   * Get the source text associated with the given function statement.
   *
   * @param Object aFunction
   * @param String aFullText
   * @returns String
   */
  _getFunctionText: function SP__getFunctionText(aFunction, aFullText) {
    let functionText = "";
    // Initially set to 0, but incremented first thing in the loop below because
    // line numbers are 1 based, not 0 based.
    let lineNumber = 0;
    const { start, end } = aFunction.loc;
    const singleLine = start.line === end.line;

    for (let line of aFullText.split(/\n/g)) {
      lineNumber++;

      if (singleLine && start.line === lineNumber) {
        functionText = line.slice(start.column, end.column);
        break;
      }

      if (start.line === lineNumber) {
        functionText += line.slice(start.column) + "\n";
        continue;
      }

      if (end.line === lineNumber) {
        functionText += line.slice(0, end.column);
        break;
      }

      if (start.line < lineNumber && end.line > lineNumber) {
        functionText += line + "\n";
      }
    }

    return functionText;
  },

  /**
   * Evaluate the top level function that the cursor is resting in.
   *
   * @returns Promise [text, error, result]
   */
  evalTopLevelFunction: function SP_evalTopLevelFunction() {
    const text = this.getText();
    const ast = this._parseText(text);
    if (!ast) {
      return promise.resolve([text, undefined, undefined]);
    }

    const cursorPos = this.editor.getCursor();
    const funcStatement = this._findTopLevelFunction(ast, cursorPos);
    if (!funcStatement) {
      return promise.resolve([text, undefined, undefined]);
    }

    let functionText = this._getFunctionText(funcStatement, text);

    // TODO: This is a work around for bug 940086. It should be removed when
    // that is fixed.
    if (funcStatement.type == "FunctionDeclaration"
        && !functionText.startsWith("function ")) {
      functionText = "function " + functionText;
      funcStatement.loc.start.column -= 9;
    }

    // The decrement by one is because our line numbers are 1-based, while
    // CodeMirror's are 0-based.
    const from = {
      line: funcStatement.loc.start.line - 1,
      ch: funcStatement.loc.start.column
    };
    const to = {
      line: funcStatement.loc.end.line - 1,
      ch: funcStatement.loc.end.column
    };

    const marker = this.editor.markText(from, to, "eval-text");
    setTimeout(() => marker.clear(), EVAL_FUNCTION_TIMEOUT);

    return this.evaluate(functionText);
  },

  /**
   * Writes out a primitive value as a comment. This handles values which are
   * to be printed directly (number, string) as well as grips to values
   * (null, undefined, longString).
   *
   * @param any aValue
   *        The value to print.
   * @return Promise
   *         The promise that resolves after the value has been printed.
   */
  _writePrimitiveAsComment: function SP__writePrimitiveAsComment(aValue)
  {
    let deferred = promise.defer();

    if (aValue.type == "longString") {
      let client = this.webConsoleClient;
      client.longString(aValue).substring(0, aValue.length, aResponse => {
        if (aResponse.error) {
          reportError("display", aResponse);
          deferred.reject(aResponse);
        }
        else {
          deferred.resolve(aResponse.substring);
        }
      });
    }
    else {
      deferred.resolve(aValue.type || aValue);
    }

    return deferred.promise.then(aComment => {
      this.writeAsComment(aComment);
    });
  },

  /**
   * Write out a value at the next line from the current insertion point.
   * The comment block will always be preceded by a newline character.
   * @param object aValue
   *        The Object to write out as a string
   */
  writeAsComment: function SP_writeAsComment(aValue)
  {
    let value = "\n/*\n" + aValue + "\n*/";

    if (this.editor.somethingSelected()) {
      let from = this.editor.getCursor("end");
      this.editor.replaceSelection(this.editor.getSelection() + value);
      let to = this.editor.getPosition(this.editor.getOffset(from) + value.length);
      this.editor.setSelection(from, to);
      return;
    }

    let text = this.editor.getText();
    this.editor.setText(text + value);

    let [ from, to ] = this.editor.getPosition(text.length, (text + value).length);
    this.editor.setSelection(from, to);
  },

  /**
   * Write out an error at the current insertion point as a block comment
   * @param object aValue
   *        The Error object to write out the message and stack trace
   * @return Promise
   *         The promise that indicates when writing the comment completes.
   */
  writeAsErrorComment: function SP_writeAsErrorComment(aError)
  {
    let deferred = promise.defer();

    if (VariablesView.isPrimitive({ value: aError })) {
      let type = aError.type;
      if (type == "undefined" ||
          type == "null" ||
          type == "Infinity" ||
          type == "-Infinity" ||
          type == "NaN" ||
          type == "-0") {
        deferred.resolve(type);
      }
      else if (type == "longString") {
        deferred.resolve(aError.initial + "\u2026");
      }
      else {
        deferred.resolve(aError);
      }
    }
    else {
      let objectClient = new ObjectClient(this.debuggerClient, aError);
      objectClient.getPrototypeAndProperties(aResponse => {
        if (aResponse.error) {
          deferred.reject(aResponse);
          return;
        }

        let { ownProperties, safeGetterValues } = aResponse;
        let error = Object.create(null);

        // Combine all the property descriptor/getter values into one object.
        for (let key of Object.keys(safeGetterValues)) {
          error[key] = safeGetterValues[key].getterValue;
        }

        for (let key of Object.keys(ownProperties)) {
          error[key] = ownProperties[key].value;
        }

        // Assemble the best possible stack we can given the properties we have.
        let stack;
        if (typeof error.stack == "string") {
          stack = error.stack;
        }
        else if (typeof error.fileName == "number") {
          stack = "@" + error.fileName;
          if (typeof error.lineNumber == "number") {
            stack += ":" + error.lineNumber;
          }
        }
        else if (typeof error.lineNumber == "number") {
          stack = "@" + error.lineNumber;
        }

        stack = stack ? "\n" + stack.replace(/\n$/, "") : "";

        if (typeof error.message == "string") {
          deferred.resolve(error.message + stack);
        }
        else {
          objectClient.getDisplayString(aResponse => {
            if (aResponse.error) {
              deferred.reject(aResponse);
            }
            else if (typeof aResponse.displayString == "string") {
              deferred.resolve(aResponse.displayString + stack);
            }
            else {
              deferred.resolve(stack);
            }
          });
        }
      });
    }

    return deferred.promise.then(aMessage => {
      console.error(aMessage);
      this.writeAsComment("Exception: " + aMessage);
    });
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
    let writePromise = OS.File.writeAtomic(aFile.path, buffer,{tmpPath: aFile.path + ".tmp"});
    writePromise.then(value => {
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

        this.editor.setText(content);
        this.editor.clearHistory();
        document.getElementById("sp-cmd-revert").setAttribute("disabled", true);
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
      fp.appendFilter("JavaScript Files", "*.js; *.jsm; *.json");
      fp.appendFilter("All Files", "*.*");
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
        this.dirty = false;
        document.getElementById("sp-cmd-revert").setAttribute("disabled", true);
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
            this.dirty = false;
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
    fp.appendFilter("JavaScript Files", "*.js; *.jsm; *.json");
    fp.appendFilter("All Files", "*.*");
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
    this.browserWindow.HUDService.toggleBrowserConsole();
  },

  /**
   * Open the Web Console.
   */
  openWebConsole: function SP_openWebConsole()
  {
    let target = TargetFactory.forTab(this.gBrowser.selectedTab);
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
      [ShortcutUtils.prettifyShortcut(document.getElementById("sp-key-run"), true),
       ShortcutUtils.prettifyShortcut(document.getElementById("sp-key-inspect"), true),
       ShortcutUtils.prettifyShortcut(document.getElementById("sp-key-display"), true)],
      3);

    let args = window.arguments;
    let state = null;

    if (args && args[0] instanceof Ci.nsIDialogParamBlock) {
      args = args[0];
      this._instanceId = args.GetString(0);

      state = args.GetString(1) || null;
      if (state) {
        state = JSON.parse(state);
        this.setState(state);
        initialText = state.text;
      }
    } else {
      this._instanceId = ScratchpadManager.createUid();
    }

    this.editor = new Editor({
      mode: Editor.modes.js,
      value: initialText,
      lineNumbers: true,
      contextMenu: "scratchpad-text-popup"
    });

    this.editor.appendTo(document.querySelector("#scratchpad-editor")).then(() => {
      var lines = initialText.split("\n");

      this.editor.on("change", this._onChanged);
      this.editor.focus();
      this.editor.setCursor({ line: lines.length, ch: lines.pop().length });

      if (state)
        this.dirty = !state.saved;

      this.initialized = true;
      this._triggerObservers("Ready");
      this.populateRecentFilesMenu();
      PreferenceObserver.init();
      CloseObserver.init();
    }).then(null, (err) => console.log(err.message));
  },

  /**
   * The Source Editor "change" event handler. This function updates the
   * Scratchpad window title to show an asterisk when there are unsaved changes.
   *
   * @private
   */
  _onChanged: function SP__onChanged()
  {
    Scratchpad._updateTitle();

    if (Scratchpad.filename) {
      if (Scratchpad.dirty)
        document.getElementById("sp-cmd-revert").removeAttribute("disabled");
      else
        document.getElementById("sp-cmd-revert").setAttribute("disabled", true);
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

    // This event is created only after user uses 'reload and run' feature.
    if (this._reloadAndRunEvent && this.gBrowser) {
      this.gBrowser.selectedBrowser.removeEventListener("load",
          this._reloadAndRunEvent, true);
    }

    PreferenceObserver.uninit();
    CloseObserver.uninit();

    this.editor.off("change", this._onChanged);
    this.editor.destroy();
    this.editor = null;

    if (this._sidebar) {
      this._sidebar.destroy();
      this._sidebar = null;
    }

    if (this._prettyPrintWorker) {
      this._prettyPrintWorker.terminate();
      this._prettyPrintWorker = null;
    }

    scratchpadTargets = null;
    this.webConsoleClient = null;
    this.debuggerClient = null;
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
    if (this.dirty) {
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
    let shouldClose;

    this.promptSave((aShouldClose, aSaved, aStatus) => {
       shouldClose = aShouldClose;
      if (aSaved && !Components.isSuccessCode(aStatus)) {
        shouldClose = false;
      }

      if (shouldClose) {
        telemetry.toolClosed("scratchpad");
        window.close();
      }

      if (aCallback) {
        aCallback(shouldClose);
      }
    });

    return shouldClose;
  },

  _observers: [],

  /**
   * Add an observer for Scratchpad events.
   *
   * The observer implements IScratchpadObserver := {
   *   onReady:      Called when the Scratchpad and its Editor are ready.
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

  /**
   * Opens the MDN documentation page for Scratchpad.
   */
  openDocumentationPage: function SP_openDocumentationPage()
  {
    let url = this.strings.GetStringFromName("help.openDocumentationPage");
    let newTab = this.gBrowser.addTab(url);
    this.browserWindow.focus();
    this.gBrowser.selectedTab = newTab;
  },
};


/**
 * Represents the DebuggerClient connection to a specific tab as used by the
 * Scratchpad.
 *
 * @param object aTab
 *              The tab to connect to.
 */
function ScratchpadTab(aTab)
{
  this._tab = aTab;
}

let scratchpadTargets = new WeakMap();

/**
 * Returns the object containing the DebuggerClient and WebConsoleClient for a
 * given tab or window.
 *
 * @param object aSubject
 *        The tab or window to obtain the connection for.
 * @return Promise
 *         The promise for the connection information.
 */
ScratchpadTab.consoleFor = function consoleFor(aSubject)
{
  if (!scratchpadTargets.has(aSubject)) {
    scratchpadTargets.set(aSubject, new this(aSubject));
  }
  return scratchpadTargets.get(aSubject).connect();
};


ScratchpadTab.prototype = {
  /**
   * The promise for the connection.
   */
  _connector: null,

  /**
   * Initialize a debugger client and connect it to the debugger server.
   *
   * @return Promise
   *         The promise for the result of connecting to this tab or window.
   */
  connect: function ST_connect()
  {
    if (this._connector) {
      return this._connector;
    }

    let deferred = promise.defer();
    this._connector = deferred.promise;

    let connectTimer = setTimeout(() => {
      deferred.reject({
        error: "timeout",
        message: Scratchpad.strings.GetStringFromName("connectionTimeout"),
      });
    }, REMOTE_TIMEOUT);

    deferred.promise.then(() => clearTimeout(connectTimer));

    this._attach().then(aTarget => {
      let consoleActor = aTarget.form.consoleActor;
      let client = aTarget.client;
      client.attachConsole(consoleActor, [], (aResponse, aWebConsoleClient) => {
        if (aResponse.error) {
          reportError("attachConsole", aResponse);
          deferred.reject(aResponse);
        }
        else {
          deferred.resolve({
            webConsoleClient: aWebConsoleClient,
            debuggerClient: client
          });
        }
      });
    });

    return deferred.promise;
  },

  /**
   * Attach to this tab.
   *
   * @return Promise
   *         The promise for the TabTarget for this tab.
   */
  _attach: function ST__attach()
  {
    let target = TargetFactory.forTab(this._tab);
    return target.makeRemote().then(() => target);
  },
};


/**
 * Represents the DebuggerClient connection to a specific window as used by the
 * Scratchpad.
 */
function ScratchpadWindow() {}

ScratchpadWindow.consoleFor = ScratchpadTab.consoleFor;

ScratchpadWindow.prototype = Heritage.extend(ScratchpadTab.prototype, {
  /**
   * Attach to this window.
   *
   * @return Promise
   *         The promise for the target for this window.
   */
  _attach: function SW__attach()
  {
    let deferred = promise.defer();

    if (!DebuggerServer.initialized) {
      DebuggerServer.init();
      DebuggerServer.addBrowserActors();
    }

    let client = new DebuggerClient(DebuggerServer.connectPipe());
    client.connect(() => {
      client.listTabs(aResponse => {
        if (aResponse.error) {
          reportError("listTabs", aResponse);
          deferred.reject(aResponse);
        }
        else {
          deferred.resolve({ form: aResponse, client: client });
        }
      });
    });

    return deferred.promise;
  }
});


function ScratchpadTarget(aTarget)
{
  this._target = aTarget;
}

ScratchpadTarget.consoleFor = ScratchpadTab.consoleFor;

ScratchpadTarget.prototype = Heritage.extend(ScratchpadTab.prototype, {
  _attach: function ST__attach()
  {
    if (this._target.isRemote) {
      return promise.resolve(this._target);
    }
    return this._target.makeRemote().then(() => this._target);
  }
});


/**
 * Encapsulates management of the sidebar containing the VariablesView for
 * object inspection.
 */
function ScratchpadSidebar(aScratchpad)
{
  let ToolSidebar = require("devtools/framework/sidebar").ToolSidebar;
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

    let deferred = promise.defer();

    let onTabReady = () => {
      if (this.variablesView) {
        this.variablesView.controller.releaseActors();
      }
      else {
        let window = this._sidebar.getWindowForTab("variablesview");
        let container = window.document.querySelector("#variables");

        this.variablesView = new VariablesView(container, {
          searchEnabled: true,
          searchPlaceholder: this._scratchpad.strings
                             .GetStringFromName("propertiesFilterPlaceholder")
        });

        VariablesViewController.attach(this.variablesView, {
          getEnvironmentClient: aGrip => {
            return new EnvironmentClient(this._scratchpad.debuggerClient, aGrip);
          },
          getObjectClient: aGrip => {
            return new ObjectClient(this._scratchpad.debuggerClient, aGrip);
          },
          getLongStringClient: aActor => {
            return this._scratchpad.webConsoleClient.longString(aActor);
          },
          releaseActor: aActor => {
            this._scratchpad.debuggerClient.release(aActor);
          }
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
   * Destroy the sidebar.
   *
   * @return Promise
   *         The promise that resolves when the sidebar is destroyed.
   */
  destroy: function SS_destroy()
  {
    if (this.variablesView) {
      this.variablesView.controller.releaseActors();
      this.variablesView = null;
    }
    return this._sidebar.destroy();
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
    let options = { objectActor: aObject };
    let view = this.variablesView;
    view.empty();
    return view.controller.setSingleVariable(options).expanded;
  }
};


/**
 * Report an error coming over the remote debugger protocol.
 *
 * @param string aAction
 *        The name of the action or method that failed.
 * @param object aResponse
 *        The response packet that contains the error.
 */
function reportError(aAction, aResponse)
{
  Cu.reportError(aAction + " failed: " + aResponse.error + " " +
                 aResponse.message);
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


/**
 * The CloseObserver listens for the last browser window closing and attempts to
 * close the Scratchpad.
 */
var CloseObserver = {
  init: function CO_init()
  {
    Services.obs.addObserver(this, "browser-lastwindow-close-requested", false);
  },

  observe: function CO_observe(aSubject)
  {
    if (Scratchpad.close()) {
      this.uninit();
    }
    else {
      aSubject.QueryInterface(Ci.nsISupportsPRBool);
      aSubject.data = true;
    }
  },

  uninit: function CO_uninit()
  {
    Services.obs.removeObserver(this, "browser-lastwindow-close-requested",
                                false);
  },
};

XPCOMUtils.defineLazyGetter(Scratchpad, "strings", function () {
  return Services.strings.createBundle(SCRATCHPAD_L10N);
});

addEventListener("load", Scratchpad.onLoad.bind(Scratchpad), false);
addEventListener("unload", Scratchpad.onUnload.bind(Scratchpad), false);
addEventListener("close", Scratchpad.onClose.bind(Scratchpad), false);
