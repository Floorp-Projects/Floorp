/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Utils: WebConsoleUtils} = require("devtools/client/webconsole/utils");
const Services = require("Services");

loader.lazyServiceGetter(this, "clipboardHelper",
                         "@mozilla.org/widget/clipboardhelper;1",
                         "nsIClipboardHelper");
loader.lazyRequireGetter(this, "Debugger", "Debugger");
loader.lazyRequireGetter(this, "EventEmitter", "devtools/shared/event-emitter");
loader.lazyRequireGetter(this, "AutocompletePopup", "devtools/client/shared/autocomplete-popup");
loader.lazyRequireGetter(this, "PropTypes", "devtools/client/shared/vendor/react-prop-types");
loader.lazyRequireGetter(this, "gDevTools", "devtools/client/framework/devtools", true);
loader.lazyRequireGetter(this, "KeyCodes", "devtools/client/shared/keycodes", true);
loader.lazyRequireGetter(this, "Editor", "devtools/client/sourceeditor/editor");
loader.lazyRequireGetter(this, "Telemetry", "devtools/client/shared/telemetry");
loader.lazyRequireGetter(this, "saveScreenshot", "devtools/shared/screenshot/save");

const l10n = require("devtools/client/webconsole/webconsole-l10n");

const HELP_URL = "https://developer.mozilla.org/docs/Tools/Web_Console/Helpers";

function gSequenceId() {
  return gSequenceId.n++;
}
gSequenceId.n = 0;

// React & Redux
const { Component } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { connect } = require("devtools/client/shared/vendor/react-redux");

// History Modules
const {
  getHistory,
  getHistoryValue
} = require("devtools/client/webconsole/selectors/history");
const historyActions = require("devtools/client/webconsole/actions/history");

// Constants used for defining the direction of JSTerm input history navigation.
const {
  HISTORY_BACK,
  HISTORY_FORWARD
} = require("devtools/client/webconsole/constants");

/**
 * Create a JSTerminal (a JavaScript command line). This is attached to an
 * existing HeadsUpDisplay (a Web Console instance). This code is responsible
 * with handling command line input and code evaluation.
 *
 * @constructor
 * @param object webConsoleFrame
 *        The WebConsoleFrame object that owns this JSTerm instance.
 */
class JSTerm extends Component {
  static get propTypes() {
    return {
      // Append new executed expression into history list (action).
      appendToHistory: PropTypes.func.isRequired,
      // Remove all entries from the history list (action).
      clearHistory: PropTypes.func.isRequired,
      // Returns previous or next value from the history
      // (depending on direction argument).
      getValueFromHistory: PropTypes.func.isRequired,
      // History of executed expression (state).
      history: PropTypes.object.isRequired,
      // Console object.
      hud: PropTypes.object.isRequired,
      // Needed for opening context menu
      serviceContainer: PropTypes.object.isRequired,
      // Handler for clipboard 'paste' event (also used for 'drop' event, callback).
      onPaste: PropTypes.func,
      codeMirrorEnabled: PropTypes.bool,
      // Update position in the history after executing an expression (action).
      updateHistoryPosition: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);

    const {
      hud,
    } = props;

    this.hud = hud;
    this.hudId = this.hud.hudId;

    this._keyPress = this._keyPress.bind(this);
    this._inputEventHandler = this._inputEventHandler.bind(this);
    this._blurEventHandler = this._blurEventHandler.bind(this);
    this.onContextMenu = this.onContextMenu.bind(this);

    this.SELECTED_FRAME = -1;

    /**
     * Array that caches the user input suggestions received from the server.
     * @private
     * @type array
     */
    this._autocompleteCache = null;

    /**
     * The input that caused the last request to the server, whose response is
     * cached in the _autocompleteCache array.
     * @private
     * @type string
     */
    this._autocompleteQuery = null;

    /**
     * The frameActorId used in the last autocomplete query. Whenever this changes
     * the autocomplete cache must be invalidated.
     * @private
     * @type string
     */
    this._lastFrameActorId = null;

    /**
     * Last input value.
     * @type string
     */
    this.lastInputValue = "";

    this.currentAutoCompletionRequestId = null;

    this.autocompletePopup = null;
    this.inputNode = null;
    this.completeNode = null;

    this._telemetry = new Telemetry();

    EventEmitter.decorate(this);
    hud.jsterm = this;
  }

  componentDidMount() {
    const autocompleteOptions = {
      onSelect: this.onAutocompleteSelect.bind(this),
      onClick: this.acceptProposedCompletion.bind(this),
      listId: "webConsole_autocompletePopupListBox",
      position: "bottom",
      autoSelect: true
    };

    const doc = this.hud.document;
    const toolbox = gDevTools.getToolbox(this.hud.owner.target);
    const tooltipDoc = toolbox ? toolbox.doc : doc;
    // The popup will be attached to the toolbox document or HUD document in the case
    // such as the browser console which doesn't have a toolbox.
    this.autocompletePopup = new AutocompletePopup(tooltipDoc, autocompleteOptions);

    if (this.props.codeMirrorEnabled) {
      if (this.node) {
        const onArrowUp = () => {
          let inputUpdated;
          if (this.autocompletePopup.isOpen) {
            this.autocompletePopup.selectPreviousItem();
            return null;
          }

          if (this.canCaretGoPrevious()) {
            inputUpdated = this.historyPeruse(HISTORY_BACK);
          }

          if (!inputUpdated) {
            return "CodeMirror.Pass";
          }
          return null;
        };

        const onArrowDown = () => {
          let inputUpdated;
          if (this.autocompletePopup.isOpen) {
            this.autocompletePopup.selectNextItem();
            return null;
          }

          if (this.canCaretGoNext()) {
            inputUpdated = this.historyPeruse(HISTORY_FORWARD);
          }

          if (!inputUpdated) {
            return "CodeMirror.Pass";
          }
          return null;
        };

        this.editor = new Editor({
          autofocus: true,
          enableCodeFolding: false,
          gutters: [],
          lineWrapping: true,
          mode: Editor.modes.js,
          styleActiveLine: false,
          tabIndex: "0",
          viewportMargin: Infinity,
          extraKeys: {
            "Enter": () => {
              // No need to handle shift + Enter as it's natively handled by CodeMirror.

              const hasSuggestion = this.hasAutocompletionSuggestion();
              if (!hasSuggestion && !Debugger.isCompilableUnit(this.getInputValue())) {
                // incomplete statement
                return "CodeMirror.Pass";
              }

              if (hasSuggestion) {
                return this.acceptProposedCompletion();
              }

              this.execute();
              return null;
            },

            "Tab": () => {
              if (this.hasEmptyInput()) {
                this.editor.codeMirror.getInputField().blur();
                return false;
              }

              const isSomethingSelected = this.editor.somethingSelected();
              const hasSuggestion = this.hasAutocompletionSuggestion();

              if (hasSuggestion && !isSomethingSelected) {
                this.acceptProposedCompletion();
                return false;
              }

              if (!isSomethingSelected) {
                this.insertStringAtCursor("\t");
                return false;
              }

              // Something is selected, let the editor handle the indent.
              return true;
            },

            "Up": onArrowUp,
            "Cmd-Up": onArrowUp,

            "Down": onArrowDown,
            "Cmd-Down": onArrowDown,

            "Left": () => {
              if (this.autocompletePopup.isOpen || this.getAutoCompletionText()) {
                this.clearCompletion();
              }
              return "CodeMirror.Pass";
            },

            "Right": () => {
              // We only want to complete on Right arrow if the completion text is
              // displayed.
              if (this.getAutoCompletionText()) {
                this.acceptProposedCompletion();
                return null;
              }

              this.clearCompletion();
              return "CodeMirror.Pass";
            },

            "Ctrl-N": () => {
              // Control-N differs from down arrow: it ignores autocomplete state.
              // Note that we preserve the default 'down' navigation within
              // multiline text.
              if (
                Services.appinfo.OS === "Darwin"
                && this.canCaretGoNext()
                && this.historyPeruse(HISTORY_FORWARD)
              ) {
                return null;
              }

              this.clearCompletion();
              return "CodeMirror.Pass";
            },

            "Ctrl-P": () => {
              // Control-P differs from up arrow: it ignores autocomplete state.
              // Note that we preserve the default 'up' navigation within
              // multiline text.
              if (
                Services.appinfo.OS === "Darwin"
                && this.canCaretGoPrevious()
                && this.historyPeruse(HISTORY_BACK)
              ) {
                return null;
              }

              this.clearCompletion();
              return "CodeMirror.Pass";
            },

            "PageUp": () => {
              if (this.autocompletePopup.isOpen) {
                this.autocompletePopup.selectPreviousPageItem();
              } else {
                this.hud.outputScroller.scrollTop = Math.max(
                  0,
                  this.hud.outputScroller.scrollTop - this.hud.outputScroller.clientHeight
                );
              }

              return null;
            },

            "PageDown": () => {
              if (this.autocompletePopup.isOpen) {
                this.autocompletePopup.selectNextPageItem();
              } else {
                this.hud.outputScroller.scrollTop = Math.min(
                  this.hud.outputScroller.scrollHeight,
                  this.hud.outputScroller.scrollTop + this.hud.outputScroller.clientHeight
                );
              }

              return null;
            },

            "Home": () => {
              if (this.autocompletePopup.isOpen) {
                this.autocompletePopup.selectItemAtIndex(0);
                return null;
              }

              if (!this.getInputValue()) {
                this.hud.outputScroller.scrollTop = 0;
                return null;
              }

              return "CodeMirror.Pass";
            },

            "End": () => {
              if (this.autocompletePopup.isOpen) {
                this.autocompletePopup.selectItemAtIndex(
                  this.autocompletePopup.itemCount - 1);
                return null;
              }

              if (!this.getInputValue()) {
                this.hud.outputScroller.scrollTop = this.hud.outputScroller.scrollHeight;
                return null;
              }

              return "CodeMirror.Pass";
            },

            "Esc": false,
            "Cmd-F": false,
            "Ctrl-F": false,
          }
        });

        this.editor.on("changes", this._inputEventHandler);
        this.editor.appendToLocalElement(this.node);
        const cm = this.editor.codeMirror;
        cm.on("paste", (_, event) => this.props.onPaste(event));
        cm.on("drop", (_, event) => this.props.onPaste(event));

        this.node.addEventListener("keydown", event => {
          if (event.keyCode === KeyCodes.DOM_VK_ESCAPE && this.autocompletePopup.isOpen) {
            this.clearCompletion();
            event.preventDefault();
            event.stopPropagation();
          }
        });
      }
    } else if (this.inputNode) {
      this.inputNode.addEventListener("keypress", this._keyPress);
      this.inputNode.addEventListener("input", this._inputEventHandler);
      this.inputNode.addEventListener("keyup", this._inputEventHandler);
      this.focus();
    }

    this.inputBorderSize = this.inputNode
      ? this.inputNode.getBoundingClientRect().height - this.inputNode.clientHeight
      : 0;

    // Update the character and chevron width needed for the popup offset calculations.
    this._inputCharWidth = this._getInputCharWidth();
    this._paddingInlineStart = this.editor ? null : this._getInputPaddingInlineStart();

    this.hud.window.addEventListener("blur", this._blurEventHandler);
    this.lastInputValue && this.setInputValue(this.lastInputValue);
  }

  shouldComponentUpdate(nextProps, nextState) {
    // XXX: For now, everything is handled in an imperative way and we
    // only want React to do the initial rendering of the component.
    // This should be modified when the actual refactoring will take place.
    return false;
  }

  /**
   * Getter for the element that holds the messages we display.
   * @type Element
   */
  get outputNode() {
    return this.hud.outputNode;
  }

  /**
   * Getter for the debugger WebConsoleClient.
   * @type object
   */
  get webConsoleClient() {
    return this.hud.webConsoleClient;
  }

  focus() {
    if (this.editor) {
      this.editor.focus();
    } else if (this.inputNode && !this.inputNode.getAttribute("focused")) {
      this.inputNode.focus();
    }
  }

  /**
   * The JavaScript evaluation response handler.
   *
   * @private
   * @param object response
   *        The message received from the server.
   */
  async _executeResultCallback(response) {
    if (!this.hud) {
      return null;
    }
    if (response.error) {
      console.error("Evaluation error " + response.error + ": " + response.message);
      return null;
    }
    let errorMessage = response.exceptionMessage;

    // Wrap thrown strings in Error objects, so `throw "foo"` outputs "Error: foo"
    if (typeof response.exception === "string") {
      errorMessage = new Error(errorMessage).toString();
    }
    const result = response.result;
    const helperResult = response.helperResult;
    const helperHasRawOutput = !!(helperResult || {}).rawOutput;

    if (helperResult && helperResult.type) {
      switch (helperResult.type) {
        case "clearOutput":
          this.hud.clearOutput();
          break;
        case "clearHistory":
          this.props.clearHistory();
          break;
        case "inspectObject":
          this.inspectObjectActor(helperResult.object);
          break;
        case "error":
          try {
            errorMessage = l10n.getStr(helperResult.message);
          } catch (ex) {
            errorMessage = helperResult.message;
          }
          break;
        case "help":
          this.hud.owner.openLink(HELP_URL);
          break;
        case "copyValueToClipboard":
          clipboardHelper.copyString(helperResult.value);
          break;
        case "screenshotOutput":
          const { args, value } = helperResult;
          const results = await saveScreenshot(this.hud.window, args, value);
          this.screenshotNotify(results);
          // early return as screenshot notify has dispatched all necessary messages
          return null;
      }
    }

    // Hide undefined results coming from JSTerm helper functions.
    if (!errorMessage && result && typeof result == "object" &&
      result.type == "undefined" &&
      helperResult && !helperHasRawOutput) {
      return null;
    }

    if (this.hud.consoleOutput) {
      return this.hud.consoleOutput.dispatchMessageAdd(response, true);
    }

    return null;
  }

  inspectObjectActor(objectActor) {
    this.hud.consoleOutput.dispatchMessageAdd({
      helperResult: {
        type: "inspectObject",
        object: objectActor
      }
    }, true);
    return this.hud.consoleOutput;
  }

  screenshotNotify(results) {
    const wrappedResults = results.map(message => ({ message, type: "logMessage" }));
    this.hud.consoleOutput.dispatchMessagesAdd(wrappedResults);
  }

  /**
   * Execute a string. Execution happens asynchronously in the content process.
   *
   * @param {String} executeString
   *        The string you want to execute. If this is not provided, the current
   *        user input is used - taken from |this.getInputValue()|.
   * @returns {Promise}
   *          Resolves with the message once the result is displayed.
   */
  async execute(executeString) {
    // attempt to execute the content of the inputNode
    executeString = executeString || this.getInputValue();
    if (!executeString) {
      return null;
    }

    // Append executed expression into the history list.
    this.props.appendToHistory(executeString);

    WebConsoleUtils.usageCount++;
    this.setInputValue("");
    this.clearCompletion();

    let selectedNodeActor = null;
    const inspectorSelection = this.hud.owner.getInspectorSelection();
    if (inspectorSelection && inspectorSelection.nodeFront) {
      selectedNodeActor = inspectorSelection.nodeFront.actorID;
    }

    const { ConsoleCommand } = require("devtools/client/webconsole/types");
    const cmdMessage = new ConsoleCommand({
      messageText: executeString,
    });
    this.hud.proxy.dispatchMessageAdd(cmdMessage);

    const options = {
      frame: this.SELECTED_FRAME,
      selectedNodeActor,
    };

    const mappedString = await this.hud.owner.getMappedExpression(executeString);
    // Even if requestEvaluation rejects (because of webConsoleClient.evaluateJSAsync),
    // we still need to pass the error response to executeResultCallback.
    const onEvaluated = this.requestEvaluation(mappedString, options)
      .then(res => res, res => res);
    const response = await onEvaluated;
    return this._executeResultCallback(response);
  }

  /**
   * Request a JavaScript string evaluation from the server.
   *
   * @param string str
   *        String to execute.
   * @param object [options]
   *        Options for evaluation:
   *        - bindObjectActor: tells the ObjectActor ID for which you want to do
   *        the evaluation. The Debugger.Object of the OA will be bound to
   *        |_self| during evaluation, such that it's usable in the string you
   *        execute.
   *        - frame: tells the stackframe depth to evaluate the string in. If
   *        the jsdebugger is paused, you can pick the stackframe to be used for
   *        evaluation. Use |this.SELECTED_FRAME| to always pick th;
   *        user-selected stackframe.
   *        If you do not provide a |frame| the string will be evaluated in the
   *        global content window.
   *        - selectedNodeActor: tells the NodeActor ID of the current selection
   *        in the Inspector, if such a selection exists. This is used by
   *        helper functions that can evaluate on the current selection.
   * @return object
   *         A promise object that is resolved when the server response is
   *         received.
   */
  requestEvaluation(str, options = {}) {
    // Send telemetry event. If we are in the browser toolbox we send -1 as the
    // toolbox session id.
    this.props.serviceContainer.recordTelemetryEvent("execute_js", {
      "lines": str.split(/\n/).length
    });

    let frameActor = null;
    if ("frame" in options) {
      frameActor = this.getFrameActor(options.frame);
    }

    const evalOptions = {
      bindObjectActor: options.bindObjectActor,
      frameActor,
      selectedNodeActor: options.selectedNodeActor,
      selectedObjectActor: options.selectedObjectActor,
    };

    return this.webConsoleClient.evaluateJSAsync(str, null, evalOptions);
  }

  /**
   * Copy the object/variable by invoking the server
   * which invokes the `copy(variable)` command and makes it
   * available in the clipboard
   * @param evalString - string which has the evaluation string to be copied
   * @param options - object - Options for evaluation
   * @return object
   *         A promise object that is resolved when the server response is
   *         received.
   */
  copyObject(evalString, evalOptions) {
    return this.webConsoleClient.evaluateJSAsync(`copy(${evalString})`,
      null, evalOptions);
  }

  /**
   * Retrieve the FrameActor ID given a frame depth.
   *
   * @param number frame
   *        Frame depth.
   * @return string|null
   *         The FrameActor ID for the given frame depth.
   */
  getFrameActor(frame) {
    const state = this.hud.owner.getDebuggerFrames();
    if (!state) {
      return null;
    }

    let grip;
    if (frame == this.SELECTED_FRAME) {
      grip = state.frames[state.selected];
    } else {
      grip = state.frames[frame];
    }

    return grip ? grip.actor : null;
  }

  /**
   * Updates the size of the input field (command line) to fit its contents.
   *
   * @returns void
   */
  resizeInput() {
    if (this.props.codeMirrorEnabled || !this.inputNode) {
      return;
    }

    const {inputNode, completeNode} = this;

    // Reset the height so that scrollHeight will reflect the natural height of
    // the contents of the input field.
    inputNode.style.height = "auto";
    const minHeightBackup = inputNode.style.minHeight;
    inputNode.style.minHeight = "unset";
    completeNode.style.height = "auto";

    // Now resize the input field to fit its contents.
    const scrollHeight = inputNode.scrollHeight;

    if (scrollHeight > 0) {
      const pxHeight = (scrollHeight + this.inputBorderSize) + "px";
      inputNode.style.height = pxHeight;
      inputNode.style.minHeight = minHeightBackup;
      completeNode.style.height = pxHeight;
    }
  }

  /**
   * Sets the value of the input field (command line), and resizes the field to
   * fit its contents. This method is preferred over setting "inputNode.value"
   * directly, because it correctly resizes the field.
   *
   * @param string newValue
   *        The new value to set.
   * @returns void
   */
  setInputValue(newValue) {
    newValue = newValue || "";
    this.lastInputValue = newValue;

    if (this.props.codeMirrorEnabled) {
      if (this.editor) {
        // In order to get the autocomplete popup to work properly, we need to set the
        // editor text and the cursor in the same operation. If we don't, the text change
        // is done before the cursor is moved, and the autocompletion call to the server
        // sends an erroneous query.
        this.editor.codeMirror.operation(() => {
          this.editor.setText(newValue);

          // Set the cursor at the end of the input.
          const lines = newValue.split("\n");
          this.editor.setCursor({
            line: lines.length - 1,
            ch: lines[lines.length - 1].length
          });
          this.editor.setAutoCompletionText();
        });
      }
    } else {
      if (!this.inputNode) {
        return;
      }

      this.inputNode.value = newValue;
      this.completeNode.value = "";
    }

    this.resizeInput();
    this.emit("set-input-value");
  }

  /**
   * Gets the value from the input field
   * @returns string
   */
  getInputValue() {
    if (this.props.codeMirrorEnabled) {
      return this.editor ? this.editor.getText() || "" : "";
    }

    return this.inputNode ? this.inputNode.value || "" : "";
  }

  getSelectionStart() {
    if (this.props.codeMirrorEnabled) {
      return this.getInputValueBeforeCursor().length;
    }

    return this.inputNode ? this.inputNode.selectionStart : null;
  }

  /**
   * The inputNode "input" and "keyup" event handler.
   * @private
   */
  _inputEventHandler() {
    const value = this.getInputValue();
    if (this.lastInputValue !== value) {
      this.resizeInput();
      this.updateAutocompletion();
      this.lastInputValue = value;
    }
  }

  /**
   * The window "blur" event handler.
   * @private
   */
  _blurEventHandler() {
    if (this.autocompletePopup) {
      this.clearCompletion();
    }
  }

  /* eslint-disable complexity */
  /**
   * The inputNode "keypress" event handler.
   *
   * @private
   * @param Event event
   */
  _keyPress(event) {
    const inputNode = this.inputNode;
    const inputValue = this.getInputValue();
    let inputUpdated = false;
    if (event.ctrlKey) {
      switch (event.charCode) {
        case 101:
          // control-e
          if (Services.appinfo.OS == "WINNT") {
            break;
          }
          let lineEndPos = inputValue.length;
          if (this.hasMultilineInput()) {
            // find index of closest newline >= cursor
            for (let i = inputNode.selectionEnd; i < lineEndPos; i++) {
              if (inputValue.charAt(i) == "\r" ||
                  inputValue.charAt(i) == "\n") {
                lineEndPos = i;
                break;
              }
            }
          }
          inputNode.setSelectionRange(lineEndPos, lineEndPos);
          event.preventDefault();
          this.clearCompletion();
          break;

        case 110:
          // Control-N differs from down arrow: it ignores autocomplete state.
          // Note that we preserve the default 'down' navigation within
          // multiline text.
          if (Services.appinfo.OS == "Darwin" &&
              this.canCaretGoNext() &&
              this.historyPeruse(HISTORY_FORWARD)) {
            event.preventDefault();
            // Ctrl-N is also used to focus the Network category button on
            // MacOSX. The preventDefault() call doesn't prevent the focus
            // from moving away from the input.
            this.focus();
          }
          this.clearCompletion();
          break;

        case 112:
          // Control-P differs from up arrow: it ignores autocomplete state.
          // Note that we preserve the default 'up' navigation within
          // multiline text.
          if (Services.appinfo.OS == "Darwin" &&
              this.canCaretGoPrevious() &&
              this.historyPeruse(HISTORY_BACK)) {
            event.preventDefault();
            // Ctrl-P may also be used to focus some category button on MacOSX.
            // The preventDefault() call doesn't prevent the focus from moving
            // away from the input.
            this.focus();
          }
          this.clearCompletion();
          break;
        default:
          break;
      }
      return;
    } else if (event.keyCode == KeyCodes.DOM_VK_RETURN) {
      if (!this.autocompletePopup.isOpen && (
        event.shiftKey || !Debugger.isCompilableUnit(this.getInputValue())
      )) {
        // shift return or incomplete statement
        return;
      }
    }

    switch (event.keyCode) {
      case KeyCodes.DOM_VK_ESCAPE:
        if (this.autocompletePopup.isOpen) {
          this.clearCompletion();
          event.preventDefault();
          event.stopPropagation();
        }
        break;

      case KeyCodes.DOM_VK_RETURN:
        if (this.hasAutocompletionSuggestion()) {
          this.acceptProposedCompletion();
        } else {
          this.execute();
        }
        event.preventDefault();
        break;

      case KeyCodes.DOM_VK_UP:
        if (this.autocompletePopup.isOpen) {
          this.autocompletePopup.selectPreviousItem();
          event.preventDefault();
        } else if (this.canCaretGoPrevious()) {
          inputUpdated = this.historyPeruse(HISTORY_BACK);
        }
        if (inputUpdated) {
          event.preventDefault();
        }
        break;

      case KeyCodes.DOM_VK_DOWN:
        if (this.autocompletePopup.isOpen) {
          this.autocompletePopup.selectNextItem();
          event.preventDefault();
        } else if (this.canCaretGoNext()) {
          inputUpdated = this.historyPeruse(HISTORY_FORWARD);
        }
        if (inputUpdated) {
          event.preventDefault();
        }
        break;

      case KeyCodes.DOM_VK_PAGE_UP:
        if (this.autocompletePopup.isOpen) {
          this.autocompletePopup.selectPreviousPageItem();
        } else {
          this.hud.outputScroller.scrollTop =
            Math.max(0,
              this.hud.outputScroller.scrollTop -
              this.hud.outputScroller.clientHeight
            );
        }
        event.preventDefault();
        break;

      case KeyCodes.DOM_VK_PAGE_DOWN:
        if (this.autocompletePopup.isOpen) {
          this.autocompletePopup.selectNextPageItem();
        } else {
          this.hud.outputScroller.scrollTop =
            Math.min(this.hud.outputScroller.scrollHeight,
              this.hud.outputScroller.scrollTop +
              this.hud.outputScroller.clientHeight
            );
        }
        event.preventDefault();
        break;

      case KeyCodes.DOM_VK_HOME:
        if (this.autocompletePopup.isOpen) {
          this.autocompletePopup.selectItemAtIndex(0);
          event.preventDefault();
        } else if (inputValue.length <= 0) {
          this.hud.outputScroller.scrollTop = 0;
          event.preventDefault();
        }
        break;

      case KeyCodes.DOM_VK_END:
        if (this.autocompletePopup.isOpen) {
          this.autocompletePopup.selectItemAtIndex(this.autocompletePopup.itemCount - 1);
          event.preventDefault();
        } else if (inputValue.length <= 0) {
          this.hud.outputScroller.scrollTop = this.hud.outputScroller.scrollHeight;
          event.preventDefault();
        }
        break;

      case KeyCodes.DOM_VK_LEFT:
        if (this.autocompletePopup.isOpen || this.getAutoCompletionText()) {
          this.clearCompletion();
        }
        break;

      case KeyCodes.DOM_VK_RIGHT:
        // We only want to complete on Right arrow if the completion text is
        // displayed.
        if (this.getAutoCompletionText()) {
          this.acceptProposedCompletion();
          event.preventDefault();
        }
        this.clearCompletion();
        break;

      case KeyCodes.DOM_VK_TAB:
        if (this.hasAutocompletionSuggestion()) {
          this.acceptProposedCompletion();
          event.preventDefault();
        } else if (!this.hasEmptyInput()) {
          if (!event.shiftKey) {
            this.insertStringAtCursor("\t");
          }
          event.preventDefault();
        }
        break;
      default:
        break;
    }
  }
  /* eslint-enable complexity */

  /**
   * Go up/down the history stack of input values.
   *
   * @param number direction
   *        History navigation direction: HISTORY_BACK or HISTORY_FORWARD.
   *
   * @returns boolean
   *          True if the input value changed, false otherwise.
   */
  historyPeruse(direction) {
    const {
      history,
      updateHistoryPosition,
      getValueFromHistory,
    } = this.props;

    if (!history.entries.length) {
      return false;
    }

    const newInputValue = getValueFromHistory(direction);
    const expression = this.getInputValue();
    updateHistoryPosition(direction, expression);

    if (newInputValue != null) {
      this.setInputValue(newInputValue);
      return true;
    }

    return false;
  }

  /**
   * Test for empty input.
   *
   * @return boolean
   */
  hasEmptyInput() {
    return this.getInputValue() === "";
  }

  /**
   * Test for multiline input.
   *
   * @return boolean
   *         True if CR or LF found in node value; else false.
   */
  hasMultilineInput() {
    return /[\r\n]/.test(this.getInputValue());
  }

  /**
   * Check if the caret is at a location that allows selecting the previous item
   * in history when the user presses the Up arrow key.
   *
   * @return boolean
   *         True if the caret is at a location that allows selecting the
   *         previous item in history when the user presses the Up arrow key,
   *         otherwise false.
   */
  canCaretGoPrevious() {
    const inputValue = this.getInputValue();

    if (this.editor) {
      const {line, ch} = this.editor.getCursor();
      return (line === 0 && ch === 0) || (line === 0 && ch === inputValue.length);
    }

    const node = this.inputNode;
    if (node.selectionStart != node.selectionEnd) {
      return false;
    }

    const multiline = /[\r\n]/.test(inputValue);
    return node.selectionStart == 0 ? true :
           node.selectionStart == inputValue.length && !multiline;
  }

  /**
   * Check if the caret is at a location that allows selecting the next item in
   * history when the user presses the Down arrow key.
   *
   * @return boolean
   *         True if the caret is at a location that allows selecting the next
   *         item in history when the user presses the Down arrow key, otherwise
   *         false.
   */
  canCaretGoNext() {
    const inputValue = this.getInputValue();
    const multiline = /[\r\n]/.test(inputValue);

    if (this.editor) {
      const {line, ch} = this.editor.getCursor();
      return (!multiline && ch === 0) ||
        this.editor.getDoc()
          .getRange({line: 0, ch: 0}, {line, ch})
          .length === inputValue.length;
    }

    const node = this.inputNode;
    if (node.selectionStart != node.selectionEnd) {
      return false;
    }

    return node.selectionStart == node.value.length ? true :
           node.selectionStart == 0 && !multiline;
  }

  async updateAutocompletion() {
    const inputValue = this.getInputValue();
    const {editor, inputNode} = this;
    const frameActor = this.getFrameActor(this.SELECTED_FRAME);

    // Only complete if the selection is empty and the input value is not.
    if (
      !inputValue ||
      (inputNode && inputNode.selectionStart != inputNode.selectionEnd) ||
      (editor && editor.getSelection()) ||
      (this.lastInputValue === inputValue && frameActor === this._lastFrameActorId)
    ) {
      this.clearCompletion();
      this.emit("autocomplete-updated");
      return;
    }

    const cursor = this.getSelectionStart();
    const input = inputValue.substring(0, cursor);

    // If the current input starts with the previous input, then we already
    // have a list of suggestions and we just need to filter the cached
    // suggestions. When the current input ends with a non-alphanumeric character we ask
    // the server again for suggestions.

    // Check if last character is non-alphanumeric
    if (!/[a-zA-Z0-9]$/.test(input) || frameActor != this._lastFrameActorId) {
      this._autocompleteQuery = null;
      this._autocompleteCache = null;
    }

    if (this._autocompleteQuery && input.startsWith(this._autocompleteQuery)) {
      let filterBy = input;
      // Find the last non-alphanumeric other than "_", ":", or "$" if it exists.
      const lastNonAlpha = input.match(/[^a-zA-Z0-9_$:][a-zA-Z0-9_$:]*$/);
      // If input contains non-alphanumerics, use the part after the last one
      // to filter the cache.
      if (lastNonAlpha) {
        filterBy = input.substring(input.lastIndexOf(lastNonAlpha) + 1);
      }

      const filterByLc = filterBy.toLocaleLowerCase();
      const looseMatching = !filterBy || filterBy[0].toLocaleLowerCase() === filterBy[0];
      const newList = this._autocompleteCache.filter(l => {
        if (looseMatching) {
          return l.toLocaleLowerCase().startsWith(filterByLc);
        }

        return l.startsWith(filterBy);
      });

      this._receiveAutocompleteProperties(null, {
        matches: newList,
        matchProp: filterBy
      });
      return;
    }
    const requestId = gSequenceId();
    this._lastFrameActorId = frameActor;
    this.currentAutoCompletionRequestId = requestId;

    const message = await this.webConsoleClient.autocomplete(input, cursor, frameActor);
    this._receiveAutocompleteProperties(requestId, message);
  }

  /**
   * Handler for the autocompletion results. This method takes
   * the completion result received from the server and updates the UI
   * accordingly.
   *
   * @param number requestId
   *        Request ID.
   * @param object message
   *        The JSON message which holds the completion results received from
   *        the content process.
   */
  _receiveAutocompleteProperties(requestId, message) {
    if (this.currentAutoCompletionRequestId !== requestId) {
      return;
    }
    this.currentAutoCompletionRequestId = null;

    // Cache whatever came from the server if the last char is
    // alphanumeric or '.'
    const inputUntilCursor = this.getInputValueBeforeCursor();

    if (requestId != null && /[a-zA-Z0-9.]$/.test(inputUntilCursor)) {
      this._autocompleteCache = message.matches;
      this._autocompleteQuery = inputUntilCursor;
    }

    const {matches, matchProp} = message;
    if (!matches.length) {
      this.clearCompletion();
      this.emit("autocomplete-updated");
      return;
    }

    const items = matches.map(match => ({
      preLabel: match.substring(0, matchProp.length),
      label: match
    }));

    if (items.length > 0) {
      const suffix = items[0].label.substring(matchProp.length);
      this.setAutoCompletionText(suffix);
    }

    const popup = this.autocompletePopup;
    // We don't want to trigger the onSelect callback since we already set the completion
    // text a few lines above.
    popup.setItems(items, 0, {
      preventSelectCallback: true,
    });

    const minimumAutoCompleteLength = 2;

    // We want to show the autocomplete popup if:
    // - there are at least 2 matching results
    // - OR, if there's 1 result, but whose label does not start like the input (this can
    //   happen with insensitive search: `num` will match `Number`).
    // - OR, if there's 1 result, but we can't show the completionText (because there's
    // some text after the cursor), unless the text in the popup is the same as the input.
    if (items.length >= minimumAutoCompleteLength
      || (items.length === 1 && items[0].preLabel !== matchProp)
      || (
        items.length === 1
        && !this.canDisplayAutoCompletionText()
        && items[0].label !== matchProp
      )
    ) {
      let popupAlignElement;
      let xOffset;
      let yOffset;

      if (this.editor) {
        popupAlignElement = this.node.querySelector(".CodeMirror-cursor");
        // We need to show the popup at the ".".
        xOffset = -1 * matchProp.length * this._inputCharWidth;
        yOffset = 5;
      } else if (this.inputNode) {
        const offset = inputUntilCursor.length -
          (inputUntilCursor.lastIndexOf("\n") + 1) -
          matchProp.length;
        xOffset = (offset * this._inputCharWidth) + this._paddingInlineStart;
        // We use completeNode as the popup anchor as its height never exceeds the
        // content size, whereas it can be the case for inputNode (when there's no message
        // in the output, it takes the whole height).
        popupAlignElement = this.completeNode;
      }

      if (popupAlignElement) {
        popup.openPopup(popupAlignElement, xOffset, yOffset, 0, {
          preventSelectCallback: true,
        });
      }
    } else if (items.length < minimumAutoCompleteLength && popup.isOpen) {
      popup.hidePopup();
    }

    this.emit("autocomplete-updated");
  }

  onAutocompleteSelect() {
    const {selectedItem} = this.autocompletePopup;
    if (selectedItem) {
      const suffix = selectedItem.label.substring(selectedItem.preLabel.length);
      this.setAutoCompletionText(suffix);
    } else {
      this.setAutoCompletionText("");
    }
  }

  /**
   * Clear the current completion information and close the autocomplete popup,
   * if needed.
   */
  clearCompletion() {
    this.setAutoCompletionText("");
    if (this.autocompletePopup) {
      this.autocompletePopup.clearItems();

      if (this.autocompletePopup.isOpen) {
        // Trigger a blur/focus of the JSTerm input to force screen readers to read the
        // value again.
        if (this.inputNode) {
          this.inputNode.blur();
        }
        this.autocompletePopup.once("popup-closed", () => {
          this.focus();
        });
        this.autocompletePopup.hidePopup();
      }
    }
  }

  /**
   * Accept the proposed input completion.
   *
   * @return boolean
   *         True if there was a selected completion item and the input value
   *         was updated, false otherwise.
   */
  acceptProposedCompletion() {
    let completionText = this.getAutoCompletionText();
    let numberOfCharsToReplaceCharsBeforeCursor;

    // If the autocompletion popup is open, we always get the selected element from there,
    // since the autocompletion text might not be enough (e.g. `dOcUmEn` should
    // autocomplete to `document`, but the autocompletion text only shows `t`).
    if (this.autocompletePopup.isOpen && this.autocompletePopup.selectedItem) {
      const {selectedItem} = this.autocompletePopup;
      completionText = selectedItem.label;
      numberOfCharsToReplaceCharsBeforeCursor = selectedItem.preLabel.length;
    }

    this.clearCompletion();

    if (completionText) {
      this.insertStringAtCursor(completionText, numberOfCharsToReplaceCharsBeforeCursor);
    }
  }

  getInputValueBeforeCursor() {
    if (this.editor) {
      return this.editor.getDoc().getRange({line: 0, ch: 0}, this.editor.getCursor());
    }

    if (this.inputNode) {
      const cursor = this.inputNode.selectionStart;
      return this.getInputValue().substring(0, cursor);
    }

    return null;
  }

  /**
   * Insert a string into the console at the cursor location,
   * moving the cursor to the end of the string.
   *
   * @param {string} str
   * @param {int} numberOfCharsToReplaceCharsBeforeCursor - defaults to 0
   */
  insertStringAtCursor(str, numberOfCharsToReplaceCharsBeforeCursor = 0) {
    const value = this.getInputValue();
    let prefix = this.getInputValueBeforeCursor();
    const suffix = value.replace(prefix, "");

    if (numberOfCharsToReplaceCharsBeforeCursor) {
      prefix =
        prefix.substring(0, prefix.length - numberOfCharsToReplaceCharsBeforeCursor);
    }

    // We need to retrieve the cursor before setting the new value.
    const editorCursor = this.editor && this.editor.getCursor();

    const scrollPosition = this.inputNode ? this.inputNode.parentElement.scrollTop : null;

    this.setInputValue(prefix + str + suffix);

    if (this.inputNode) {
      const newCursor = prefix.length + str.length;
      this.inputNode.selectionStart = this.inputNode.selectionEnd = newCursor;
      this.inputNode.parentElement.scrollTop = scrollPosition;
    } else if (this.editor) {
      // Set the cursor on the same line it was already at, after the autocompleted text
      this.editor.setCursor({
        line: editorCursor.line,
        ch: editorCursor.ch + str.length - numberOfCharsToReplaceCharsBeforeCursor
      });
    }
  }

  /**
   * Set the autocompletion text of the input.
   *
   * @param string suffix
   *        The proposed suffix for the inputNode value.
   */
  setAutoCompletionText(suffix) {
    if (suffix && !this.canDisplayAutoCompletionText()) {
      suffix = "";
    }

    if (this.completeNode) {
      const lines = this.getInputValueBeforeCursor().split("\n");
      const lastLine = lines[lines.length - 1];
      const prefix = ("\n".repeat(lines.length - 1)) + lastLine.replace(/[\S]/g, " ");
      this.completeNode.value = suffix ? prefix + suffix : "";
    }

    if (this.editor) {
      this.editor.setAutoCompletionText(suffix);
    }
  }

  getAutoCompletionText() {
    if (this.completeNode) {
      // Remove the spaces we set to align with the input value.
      return this.completeNode.value.replace(/^\s+/gm, "");
    }

    if (this.editor) {
      return this.editor.getAutoCompletionText();
    }

    return null;
  }

  /**
   * Indicate if the input has an autocompletion suggestion, i.e. that there is either
   * something in the autocompletion text or that there's a selected item in the
   * autocomplete popup.
   */
  hasAutocompletionSuggestion() {
    // We can have cases where the popup is opened but we can't display the autocompletion
    // text.
    return this.getAutoCompletionText() || (
      this.autocompletePopup.isOpen &&
      Number.isInteger(this.autocompletePopup.selectedIndex) &&
      this.autocompletePopup.selectedIndex > -1
    );
  }

  /**
   * Returns a boolean indicating if we can display an autocompletion text in the input,
   * i.e. if there is no characters displayed on the same line of the cursor and after it.
   */
  canDisplayAutoCompletionText() {
    if (this.editor) {
      const { ch, line } = this.editor.getCursor();
      const lineContent = this.editor.getLine(line);
      const textAfterCursor = lineContent.substring(ch);
      return textAfterCursor === "";
    }

    if (this.inputNode) {
      const value = this.getInputValue();
      const textAfterCursor = value.substring(this.inputNode.selectionStart);
      return textAfterCursor.split("\n")[0] === "";
    }

    return false;
  }

  /**
   * Calculates and returns the width of a single character of the input box.
   * This will be used in opening the popup at the correct offset.
   *
   * @returns {Number|null}: Width off the "x" char, or null if the input does not exist.
   */
  _getInputCharWidth() {
    if (!this.inputNode && !this.node) {
      return null;
    }

    if (this.editor) {
      return this.editor.defaultCharWidth();
    }

    const doc = this.hud.document;
    const tempLabel = doc.createElement("span");
    const style = tempLabel.style;
    style.position = "fixed";
    style.padding = "0";
    style.margin = "0";
    style.width = "auto";
    style.color = "transparent";
    WebConsoleUtils.copyTextStyles(this.inputNode, tempLabel);
    tempLabel.textContent = "x";
    doc.documentElement.appendChild(tempLabel);
    const width = tempLabel.getBoundingClientRect().width;
    tempLabel.remove();
    return width;
  }

  /**
   * Calculates and returns the width of the chevron icon.
   * This will be used in opening the popup at the correct offset.
   *
   * @returns {Number|null}: Width of the icon, or null if the input does not exist.
   */
  _getInputPaddingInlineStart() {
    if (!this.inputNode) {
      return null;
    }
   // Calculate the width of the chevron placed at the beginning of the input box.
    const doc = this.hud.document;

    return new Number(doc.defaultView
      .getComputedStyle(this.inputNode)
      .paddingInlineStart.replace(/[^0-9.]/g, ""));
  }

  onContextMenu(e) {
    // The toolbox does it's own edit menu handling with
    // toolbox-textbox-context-popup and friends. For now, fall
    // back to use that if running inside the toolbox, but use our
    // own menu when running in the Browser Console (see Bug 1476097).
    if (this.props.hud.isBrowserConsole) {
      this.props.serviceContainer.openEditContextMenu(e);
    }
  }

  destroy() {
    this.webConsoleClient.clearNetworkRequests();
    if (this.hud.outputNode) {
      // We do this because it's much faster than letting React handle the ConsoleOutput
      // unmounting.
      this.hud.outputNode.innerHTML = "";
    }

    if (this.autocompletePopup) {
      this.autocompletePopup.destroy();
      this.autocompletePopup = null;
    }

    if (this.inputNode) {
      this.inputNode.removeEventListener("keypress", this._keyPress);
      this.inputNode.removeEventListener("input", this._inputEventHandler);
      this.inputNode.removeEventListener("keyup", this._inputEventHandler);
      this.hud.window.removeEventListener("blur", this._blurEventHandler);
    }

    if (this.editor) {
      this.editor.destroy();
      this.editor = null;
    }

    this.hud = null;
  }

  render() {
    if (this.props.hud.isBrowserConsole &&
        !Services.prefs.getBoolPref("devtools.chrome.enabled")) {
      return null;
    }

    if (this.props.codeMirrorEnabled) {
      return dom.div({
        className: "jsterm-input-container devtools-monospace",
        key: "jsterm-container",
        style: {direction: "ltr"},
        "aria-live": "off",
        onContextMenu: this.onContextMenu,
        ref: node => {
          this.node = node;
        },
      });
    }

    const {
      onPaste
    } = this.props;

    return (
      dom.div({
        className: "jsterm-input-container",
        key: "jsterm-container",
        style: {direction: "ltr"},
        "aria-live": "off",
      },
        dom.textarea({
          className: "jsterm-complete-node devtools-monospace",
          key: "complete",
          tabIndex: "-1",
          ref: node => {
            this.completeNode = node;
          },
        }),
        dom.textarea({
          className: "jsterm-input-node devtools-monospace",
          key: "input",
          tabIndex: "0",
          rows: "1",
          "aria-autocomplete": "list",
          ref: node => {
            this.inputNode = node;
          },
          onPaste: onPaste,
          onDrop: onPaste,
          onContextMenu: this.onContextMenu,
        })
      )
    );
  }
}

// Redux connect

function mapStateToProps(state) {
  return {
    history: getHistory(state),
    getValueFromHistory: (direction) => getHistoryValue(state, direction),
  };
}

function mapDispatchToProps(dispatch) {
  return {
    appendToHistory: (expr) => dispatch(historyActions.appendToHistory(expr)),
    clearHistory: () => dispatch(historyActions.clearHistory()),
    updateHistoryPosition: (direction, expression) =>
      dispatch(historyActions.updateHistoryPosition(direction, expression)),
  };
}

module.exports = connect(mapStateToProps, mapDispatchToProps)(JSTerm);
