/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Utils: WebConsoleUtils} = require("devtools/client/webconsole/utils");
const Services = require("Services");
const { debounce } = require("devtools/shared/debounce");

loader.lazyServiceGetter(this, "clipboardHelper",
                         "@mozilla.org/widget/clipboardhelper;1",
                         "nsIClipboardHelper");
loader.lazyRequireGetter(this, "Debugger", "Debugger");
loader.lazyRequireGetter(this, "EventEmitter", "devtools/shared/event-emitter");
loader.lazyRequireGetter(this, "AutocompletePopup", "devtools/client/shared/autocomplete-popup");
loader.lazyRequireGetter(this, "PropTypes", "devtools/client/shared/vendor/react-prop-types");
loader.lazyRequireGetter(this, "KeyCodes", "devtools/client/shared/keycodes", true);
loader.lazyRequireGetter(this, "Editor", "devtools/client/shared/sourceeditor/editor");
loader.lazyRequireGetter(this, "Telemetry", "devtools/client/shared/telemetry");
loader.lazyRequireGetter(this, "saveScreenshot", "devtools/shared/screenshot/save");
loader.lazyRequireGetter(this, "focusableSelector", "devtools/client/shared/focus", true);

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
  getHistoryValue,
} = require("devtools/client/webconsole/selectors/history");
const {getAutocompleteState} = require("devtools/client/webconsole/selectors/autocomplete");
const historyActions = require("devtools/client/webconsole/actions/history");
const autocompleteActions = require("devtools/client/webconsole/actions/autocomplete");

// Constants used for defining the direction of JSTerm input history navigation.
const {
  HISTORY_BACK,
  HISTORY_FORWARD,
} = require("devtools/client/webconsole/constants");

/**
 * Create a JSTerminal (a JavaScript command line). This is attached to an
 * existing HeadsUpDisplay (a Web Console instance). This code is responsible
 * with handling command line input and code evaluation.
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
      webConsoleUI: PropTypes.object.isRequired,
      // Needed for opening context menu
      serviceContainer: PropTypes.object.isRequired,
      // Handler for clipboard 'paste' event (also used for 'drop' event, callback).
      onPaste: PropTypes.func,
      codeMirrorEnabled: PropTypes.bool,
      // Update position in the history after executing an expression (action).
      updateHistoryPosition: PropTypes.func.isRequired,
      // Update autocomplete popup state.
      autocompleteUpdate: PropTypes.func.isRequired,
      autocompleteClear: PropTypes.func.isRequired,
      // Data to be displayed in the autocomplete popup.
      autocompleteData: PropTypes.object.isRequired,
      // Is the input in editor mode.
      editorMode: PropTypes.bool,
      autocomplete: PropTypes.bool,
    };
  }

  constructor(props) {
    super(props);

    const {
      webConsoleUI,
    } = props;

    this.webConsoleUI = webConsoleUI;
    this.hudId = this.webConsoleUI.hudId;

    this._keyPress = this._keyPress.bind(this);
    this._inputEventHandler = this._inputEventHandler.bind(this);
    this._blurEventHandler = this._blurEventHandler.bind(this);
    this.onContextMenu = this.onContextMenu.bind(this);
    this.imperativeUpdate = this.imperativeUpdate.bind(this);

    // We debounce the autocompleteUpdate so we don't send too many requests to the server
    // as the user is typing.
    // The delay should be small enough to be unnoticed by the user.
    this.autocompleteUpdate = debounce(this.props.autocompleteUpdate, 75, this);

    /**
     * Last input value.
     * @type string
     */
    this.lastInputValue = "";

    this.autocompletePopup = null;
    this.inputNode = null;
    this.completeNode = null;

    this._telemetry = new Telemetry();

    EventEmitter.decorate(this);
    webConsoleUI.jsterm = this;
  }

  componentDidMount() {
    const autocompleteOptions = {
      onSelect: this.onAutocompleteSelect.bind(this),
      onClick: this.acceptProposedCompletion.bind(this),
      listId: "webConsole_autocompletePopupListBox",
      position: "bottom",
      autoSelect: true,
    };

    const doc = this.webConsoleUI.document;
    const toolbox = this.webConsoleUI.wrapper.toolbox;
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

        const onArrowLeft = () => {
          if (this.autocompletePopup.isOpen || this.getAutoCompletionText()) {
            this.clearCompletion();
          }
          return "CodeMirror.Pass";
        };

        const onArrowRight = () => {
          // We only want to complete on Right arrow if the completion text is
          // displayed.
          if (this.getAutoCompletionText()) {
            this.acceptProposedCompletion();
            return null;
          }

          this.clearCompletion();
          return "CodeMirror.Pass";
        };

        this.editor = new Editor({
          autofocus: true,
          enableCodeFolding: false,
          autoCloseBrackets: false,
          gutters: [],
          lineWrapping: true,
          mode: Editor.modes.js,
          styleActiveLine: false,
          tabIndex: "0",
          viewportMargin: Infinity,
          disableSearchAddon: true,
          extraKeys: {
            "Enter": () => {
              // No need to handle shift + Enter as it's natively handled by CodeMirror.

              const hasSuggestion = this.hasAutocompletionSuggestion();
              if (!hasSuggestion && !Debugger.isCompilableUnit(this._getValue())) {
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

            "Shift-Tab": () => {
              if (this.hasEmptyInput()) {
                this.focusPreviousElement();
                return false;
              }

              const hasSuggestion = this.hasAutocompletionSuggestion();

              if (hasSuggestion) {
                return false;
              }

              return "CodeMirror.Pass";
            },

            "Up": onArrowUp,
            "Cmd-Up": onArrowUp,

            "Down": onArrowDown,
            "Cmd-Down": onArrowDown,

            "Left": onArrowLeft,
            "Ctrl-Left": onArrowLeft,
            "Cmd-Left": onArrowLeft,
            "Alt-Left": onArrowLeft,

            "Right": onArrowRight,
            "Ctrl-Right": onArrowRight,
            "Cmd-Right": onArrowRight,
            "Alt-Right": onArrowRight,

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
                const {outputScroller} = this.webConsoleUI;
                const {scrollTop, clientHeight} = outputScroller;
                outputScroller.scrollTop = Math.max(0, scrollTop - clientHeight);
              }

              return null;
            },

            "PageDown": () => {
              if (this.autocompletePopup.isOpen) {
                this.autocompletePopup.selectNextPageItem();
              } else {
                const {outputScroller} = this.webConsoleUI;
                const {scrollTop, scrollHeight, clientHeight} = outputScroller;
                outputScroller.scrollTop =
                  Math.min(scrollHeight, scrollTop + clientHeight);
              }

              return null;
            },

            "Home": () => {
              if (this.autocompletePopup.isOpen) {
                this.autocompletePopup.selectItemAtIndex(0);
                return null;
              }

              if (!this._getValue()) {
                this.webConsoleUI.outputScroller.scrollTop = 0;
                return null;
              }

              if (this.getAutoCompletionText()) {
                this.clearCompletion();
              }

              return "CodeMirror.Pass";
            },

            "End": () => {
              if (this.autocompletePopup.isOpen) {
                this.autocompletePopup.selectItemAtIndex(
                  this.autocompletePopup.itemCount - 1);
                return null;
              }

              if (!this._getValue()) {
                const {outputScroller} = this.webConsoleUI;
                outputScroller.scrollTop = outputScroller.scrollHeight;
                return null;
              }

              if (this.getAutoCompletionText()) {
                this.clearCompletion();
              }

              return "CodeMirror.Pass";
            },

            "Ctrl-Space": () => {
              if (!this.autocompletePopup.isOpen) {
                this.props.autocompleteUpdate(true);
                return null;
              }

              return "CodeMirror.Pass";
            },

            "Esc": false,
            "Cmd-F": false,
            "Ctrl-F": false,
          },
        });

        this.editor.on("changes", this._inputEventHandler);
        this.editor.on("beforeChange", this._onBeforeChange);
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

    this.webConsoleUI.window.addEventListener("blur", this._blurEventHandler);
    this.lastInputValue && this._setValue(this.lastInputValue);
  }

  componentWillReceiveProps(nextProps) {
    this.imperativeUpdate(nextProps);
  }

  shouldComponentUpdate(nextProps) {
    // XXX: For now, everything is handled in an imperative way and we
    // only want React to do the initial rendering of the component.
    // This should be modified when the actual refactoring will take place.
    return false;
  }

  /**
   * Do all the imperative work needed after a Redux store update.
   *
   * @param {Object} nextProps: props passed from shouldComponentUpdate.
   */
  imperativeUpdate(nextProps) {
    if (
      nextProps &&
      nextProps.autocompleteData !== this.props.autocompleteData &&
      nextProps.autocompleteData.pendingRequestId === null
    ) {
      this.updateAutocompletionPopup(nextProps.autocompleteData);
    }
  }

  /**
   * Getter for the element that holds the messages we display.
   * @type Element
   */
  get outputNode() {
    return this.webConsoleUI.outputNode;
  }

  /**
   * Getter for the debugger WebConsoleClient.
   * @type object
   */
  get webConsoleClient() {
    return this.webConsoleUI.webConsoleClient;
  }

  focus() {
    if (this.editor) {
      this.editor.focus();
    } else if (this.inputNode && !this.inputNode.getAttribute("focused")) {
      this.inputNode.focus();
    }
  }

  focusPreviousElement() {
    const inputField = this.editor.codeMirror.getInputField();

    const findPreviousFocusableElement = el => {
      if (!el || !el.querySelectorAll) {
        return null;
      }

      const items = Array.from(el.querySelectorAll(focusableSelector));
      const inputIndex = items.indexOf(inputField);

      if (items.length === 0 || (inputIndex > -1 && items.length === 1)) {
        return findPreviousFocusableElement(el.parentNode);
      }

      const index = inputIndex > 0
        ? inputIndex - 1
        : items.length - 1;
      return items[index];
    };

    const focusableEl = findPreviousFocusableElement(this.node.parentNode);
    if (focusableEl) {
      focusableEl.focus();
    }
  }

  /**
   * The JavaScript evaluation response handler.
   *
   * @private
   * @param {Object} response
   *        The message received from the server.
   */
  async _executeResultCallback(response) {
    if (!this.webConsoleUI) {
      return null;
    }

    if (response.error) {
      console.error("Evaluation error " + response.error + ": " + response.message);
      return null;
    }

    // If the evaluation was a top-level await expression that was rejected, there will
    // be an uncaught exception reported, so we don't want need to print anything here.
    if (response.topLevelAwaitRejected === true) {
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
          this.webConsoleUI.clearOutput();
          break;
        case "clearHistory":
          this.props.clearHistory();
          break;
        case "inspectObject":
          this.webConsoleUI.inspectObjectActor(helperResult.object);
          break;
        case "error":
          try {
            errorMessage = l10n.getStr(helperResult.message);
          } catch (ex) {
            errorMessage = helperResult.message;
          }
          break;
        case "help":
          this.webConsoleUI.hud.openLink(HELP_URL);
          break;
        case "copyValueToClipboard":
          clipboardHelper.copyString(helperResult.value);
          break;
        case "screenshotOutput":
          const { args, value } = helperResult;
          const results = await saveScreenshot(this.webConsoleUI.window, args, value);
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

    if (this.webConsoleUI.wrapper) {
      return this.webConsoleUI.wrapper.dispatchMessageAdd(response, true);
    }

    return null;
  }

  screenshotNotify(results) {
    const wrappedResults = results.map(message => ({ message, type: "logMessage" }));
    this.webConsoleUI.wrapper.dispatchMessagesAdd(wrappedResults);
  }

  /**
   * Execute a string. Execution happens asynchronously in the content process.
   *
   * @param {String} executeString
   *        The string you want to execute. If this is not provided, the current
   *        user input is used - taken from |this._getValue()|.
   * @returns {Promise}
   *          Resolves with the message once the result is displayed.
   */
  async execute(executeString) {
    // attempt to execute the content of the inputNode
    executeString = executeString || this._getValue();
    if (!executeString) {
      return null;
    }

    // Append executed expression into the history list.
    this.props.appendToHistory(executeString);

    WebConsoleUtils.usageCount++;

    if (!this.props.editorMode) {
      this._setValue("");
    }

    this.clearCompletion();

    let selectedNodeActor = null;
    const inspectorSelection = this.webConsoleUI.hud.getInspectorSelection();
    if (inspectorSelection && inspectorSelection.nodeFront) {
      selectedNodeActor = inspectorSelection.nodeFront.actorID;
    }

    const { ConsoleCommand } = require("devtools/client/webconsole/types");
    const cmdMessage = new ConsoleCommand({
      messageText: executeString,
      timeStamp: Date.now(),
    });
    this.webConsoleUI.proxy.dispatchMessageAdd(cmdMessage);

    let mappedExpressionRes = null;
    try {
      mappedExpressionRes =
        await this.webConsoleUI.hud.getMappedExpression(executeString);
    } catch (e) {
      console.warn("Error when calling getMappedExpression", e);
    }

    executeString = mappedExpressionRes ? mappedExpressionRes.expression : executeString;

    const options = {
      selectedNodeActor,
      mapped: mappedExpressionRes ? mappedExpressionRes.mapped : null,
    };

    // Even if requestEvaluation rejects (because of webConsoleClient.evaluateJSAsync),
    // we still need to pass the error response to executeResultCallback.
    const onEvaluated = this.requestEvaluation(executeString, options)
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
   *        - selectedNodeActor: tells the NodeActor ID of the current selection
   *        in the Inspector, if such a selection exists. This is used by
   *        helper functions that can evaluate on the current selection.
   *        - mapped: basically getMappedExpression().mapped. An object that indicates
   *        which modifications were done to the input entered by the user.
   * @return object
   *         A promise object that is resolved when the server response is
   *         received.
   */
  requestEvaluation(str, options = {}) {
    // Send telemetry event. If we are in the browser toolbox we send -1 as the
    // toolbox session id.
    this.props.serviceContainer.recordTelemetryEvent("execute_js", {
      "lines": str.split(/\n/).length,
    });

    const { frameActor, client } =
      this.props.serviceContainer.getFrameActor(options.frame);

    return client.evaluateJSAsync(str, {
      frameActor,
      ...options,
    });
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
    return this.webConsoleClient.evaluateJSAsync(`copy(${evalString})`, evalOptions);
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
  _setValue(newValue) {
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
            ch: lines[lines.length - 1].length,
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
  _getValue() {
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

  getSelectedText() {
    if (this.inputNode) {
      return this.inputNode.value.substring(
        this.inputNode.selectionStart, this.inputNode.selectionEnd);
    }
    return this.editor.getSelection();
  }

  /**
   * Even handler for the "beforeChange" event fired by codeMirror. This event is fired
   * when codeMirror is about to make a change to its DOM representation.
   */
  _onBeforeChange() {
    // clear the completionText before the change is done to prevent a visual glitch.
    // See Bug 1491776.
    this.setAutoCompletionText("");
  }

  /**
   * The inputNode "input" and "keyup" event handler.
   * @private
   */
  _inputEventHandler() {
    const value = this._getValue();
    if (this.lastInputValue !== value) {
      this.resizeInput();
      if (this.props.autocomplete) {
        this.autocompleteUpdate();
      }
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
    const inputValue = this._getValue();
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

      if (event.key === " " && !this.autocompletePopup.isOpen) {
        // Open the autocompletion popup on Ctrl-Space (if it wasn't displayed).
        this.props.autocompleteUpdate(true);
        event.preventDefault();
      }

      if (event.keyCode === KeyCodes.DOM_VK_LEFT &&
        (this.autocompletePopup.isOpen || this.getAutoCompletionText())
      ) {
        this.clearCompletion();
      }

      // We only want to complete on Right arrow if the completion text is displayed.
      if (event.keyCode === KeyCodes.DOM_VK_RIGHT) {
        if (this.getAutoCompletionText()) {
          this.acceptProposedCompletion();
          event.preventDefault();
        }
        this.clearCompletion();
      }

      return;
    } else if (event.keyCode == KeyCodes.DOM_VK_RETURN) {
      if (!this.autocompletePopup.isOpen && (
        event.shiftKey || !Debugger.isCompilableUnit(this._getValue())
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
          this.webConsoleUI.outputScroller.scrollTop =
            Math.max(0,
              this.webConsoleUI.outputScroller.scrollTop -
              this.webConsoleUI.outputScroller.clientHeight
            );
        }
        event.preventDefault();
        break;

      case KeyCodes.DOM_VK_PAGE_DOWN:
        if (this.autocompletePopup.isOpen) {
          this.autocompletePopup.selectNextPageItem();
        } else {
          this.webConsoleUI.outputScroller.scrollTop =
            Math.min(this.webConsoleUI.outputScroller.scrollHeight,
              this.webConsoleUI.outputScroller.scrollTop +
              this.webConsoleUI.outputScroller.clientHeight
            );
        }
        event.preventDefault();
        break;

      case KeyCodes.DOM_VK_HOME:
        if (this.autocompletePopup.isOpen) {
          this.autocompletePopup.selectItemAtIndex(0);
          event.preventDefault();
        } else if (this.getAutoCompletionText()) {
          this.clearCompletion();
        } else if (inputValue.length <= 0) {
          this.webConsoleUI.outputScroller.scrollTop = 0;
          event.preventDefault();
        }
        break;

      case KeyCodes.DOM_VK_END:
        if (this.autocompletePopup.isOpen) {
          this.autocompletePopup.selectItemAtIndex(this.autocompletePopup.itemCount - 1);
          event.preventDefault();
        } else if (this.getAutoCompletionText()) {
          this.clearCompletion();
        } else if (inputValue.length <= 0) {
          const {outputScroller} = this.webConsoleUI;
          outputScroller.scrollTop = outputScroller.scrollHeight;
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
    const expression = this._getValue();
    updateHistoryPosition(direction, expression);

    if (newInputValue != null) {
      this._setValue(newInputValue);
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
    return this._getValue() === "";
  }

  /**
   * Test for multiline input.
   *
   * @return boolean
   *         True if CR or LF found in node value; else false.
   */
  hasMultilineInput() {
    return /[\r\n]/.test(this._getValue());
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
    const inputValue = this._getValue();

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
    const inputValue = this._getValue();
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

  /**
   * Takes the data returned by the server and update the autocomplete popup state (i.e.
   * its visibility and items).
   *
   * @param {Object} data
   *        The autocompletion data as returned by the webconsole actor's autocomplete
   *        service. Should be of the following shape:
   *        {
   *          matches: {Array} array of the properties matching the input,
   *          matchProp: {String} The string used to filter the properties,
   *          isElementAccess: {Boolean} True when the input is an element access,
   *                           i.e. `document["addEve`.
   *        }
   * @fires autocomplete-updated
   */
  updateAutocompletionPopup(data) {
    const {matches, matchProp, isElementAccess} = data;
    if (!matches.length) {
      this.clearCompletion();
      this.emit("autocomplete-updated");
      return;
    }

    const inputUntilCursor = this.getInputValueBeforeCursor();

    const items = matches.map(label => {
      let preLabel = label.substring(0, matchProp.length);
      // If the user is performing an element access, and if they did not typed a quote,
      // then we need to adjust the preLabel to match the quote from the label + what
      // the user entered.
      if (isElementAccess && /^['"`]/.test(matchProp) === false) {
        preLabel = label.substring(0, matchProp.length + 1);
      }
      return {preLabel, label, isElementAccess};
    });

    if (items.length > 0) {
      const {preLabel, label} = items[0];
      let suffix = label.substring(preLabel.length);
      if (isElementAccess) {
        if (!matchProp) {
          suffix = label;
        }
        const inputAfterCursor = this._getValue().substring(inputUntilCursor.length);
        // If there's not a bracket after the cursor, add it to the completionText.
        if (!inputAfterCursor.trimLeft().startsWith("]")) {
          suffix = suffix + "]";
        }
      }
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
      const popupAlignElement = this.props.serviceContainer.getJsTermTooltipAnchor();
      let xOffset;
      let yOffset;

      if (this.editor) {
        // We need to show the popup at the "." or "[".
        xOffset = -1 * matchProp.length * this._inputCharWidth;
        yOffset = 5;
      } else if (this.inputNode) {
        const offset = inputUntilCursor.length -
          (inputUntilCursor.lastIndexOf("\n") + 1) -
          matchProp.length;
        xOffset = (offset * this._inputCharWidth) + this._paddingInlineStart;
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
      const {preLabel, label, isElementAccess} = selectedItem;
      let suffix = label.substring(preLabel.length);

      // If the user is performing an element access, we need to check if we should add
      // starting and ending quotes, as well as a closing bracket.
      if (isElementAccess) {
        const inputBeforeCursor = this.getInputValueBeforeCursor();
        if (inputBeforeCursor.trim().endsWith("[")) {
          suffix = label;
        }

        const inputAfterCursor = this._getValue().substring(inputBeforeCursor.length);
        // If there's no closing bracket after the cursor, add it to the completionText.
        if (!inputAfterCursor.trimLeft().startsWith("]")) {
          suffix = suffix + "]";
        }
      }
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
    this.emit("autocomplete-updated");
  }

  /**
   * Accept the proposed input completion.
   */
  acceptProposedCompletion() {
    let completionText = this.getAutoCompletionText();
    let numberOfCharsToReplaceCharsBeforeCursor;

    // If the autocompletion popup is open, we always get the selected element from there,
    // since the autocompletion text might not be enough (e.g. `dOcUmEn` should
    // autocomplete to `document`, but the autocompletion text only shows `t`).
    if (this.autocompletePopup.isOpen && this.autocompletePopup.selectedItem) {
      const {selectedItem} = this.autocompletePopup;
      const {label, preLabel, isElementAccess} = selectedItem;

      completionText = label;
      numberOfCharsToReplaceCharsBeforeCursor = preLabel.length;

      // If the user is performing an element access, we need to check if we should add
      // starting and ending quotes, as well as a closing bracket.
      if (isElementAccess) {
        const inputBeforeCursor = this.getInputValueBeforeCursor();
        const lastOpeningBracketIndex = inputBeforeCursor.lastIndexOf("[");
        if (lastOpeningBracketIndex > -1) {
          numberOfCharsToReplaceCharsBeforeCursor =
            inputBeforeCursor.substring(lastOpeningBracketIndex + 1).length;
        }

        const inputAfterCursor = this._getValue().substring(inputBeforeCursor.length);
        // If there's not a bracket after the cursor, add it.
        if (!inputAfterCursor.trimLeft().startsWith("]")) {
          completionText = completionText + "]";
        }
      }
    }

    this.props.autocompleteClear();

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
      return this._getValue().substring(0, cursor);
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
    const value = this._getValue();
    let prefix = this.getInputValueBeforeCursor();
    const suffix = value.replace(prefix, "");

    if (numberOfCharsToReplaceCharsBeforeCursor) {
      prefix =
        prefix.substring(0, prefix.length - numberOfCharsToReplaceCharsBeforeCursor);
    }

    // We need to retrieve the cursor before setting the new value.
    const editorCursor = this.editor && this.editor.getCursor();

    const scrollPosition = this.inputNode ? this.inputNode.parentElement.scrollTop : null;

    this._setValue(prefix + str + suffix);

    if (this.inputNode) {
      const newCursor = prefix.length + str.length;
      this.inputNode.selectionStart = this.inputNode.selectionEnd = newCursor;
      this.inputNode.parentElement.scrollTop = scrollPosition;
    } else if (this.editor) {
      // Set the cursor on the same line it was already at, after the autocompleted text
      this.editor.setCursor({
        line: editorCursor.line,
        ch: editorCursor.ch + str.length - numberOfCharsToReplaceCharsBeforeCursor,
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
      const value = this._getValue();
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

    const doc = this.webConsoleUI.document;
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
    const doc = this.webConsoleUI.document;

    return new Number(doc.defaultView
      .getComputedStyle(this.inputNode)
      .paddingInlineStart.replace(/[^0-9.]/g, ""));
  }

  onContextMenu(e) {
    this.props.serviceContainer.openEditContextMenu(e);
  }

  destroy() {
    this.webConsoleClient.clearNetworkRequests();
    if (this.webConsoleUI.outputNode) {
      // We do this because it's much faster than letting React handle the ConsoleOutput
      // unmounting.
      this.webConsoleUI.outputNode.innerHTML = "";
    }

    if (this.autocompletePopup) {
      this.autocompletePopup.destroy();
      this.autocompletePopup = null;
    }

    if (this.inputNode) {
      this.inputNode.removeEventListener("keypress", this._keyPress);
      this.inputNode.removeEventListener("input", this._inputEventHandler);
      this.inputNode.removeEventListener("keyup", this._inputEventHandler);
      this.webConsoleUI.window.removeEventListener("blur", this._blurEventHandler);
    }

    if (this.editor) {
      this.editor.destroy();
      this.editor = null;
    }

    this.webConsoleUI = null;
  }

  render() {
    if (this.props.webConsoleUI.isBrowserConsole &&
        !Services.prefs.getBoolPref("devtools.chrome.enabled")) {
      return null;
    }

    if (this.props.codeMirrorEnabled) {
      return dom.div({
        className: "jsterm-input-container devtools-input devtools-monospace",
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
      onPaste,
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
    autocompleteData: getAutocompleteState(state),
  };
}

function mapDispatchToProps(dispatch) {
  return {
    appendToHistory: (expr) => dispatch(historyActions.appendToHistory(expr)),
    clearHistory: () => dispatch(historyActions.clearHistory()),
    updateHistoryPosition: (direction, expression) =>
      dispatch(historyActions.updateHistoryPosition(direction, expression)),
    autocompleteUpdate: force => dispatch(autocompleteActions.autocompleteUpdate(force)),
    autocompleteClear: () => dispatch(autocompleteActions.autocompleteClear()),
  };
}

module.exports = connect(mapStateToProps, mapDispatchToProps)(JSTerm);
