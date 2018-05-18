/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Utils: WebConsoleUtils} = require("devtools/client/webconsole/utils");
const Services = require("Services");

loader.lazyServiceGetter(this, "clipboardHelper",
                         "@mozilla.org/widget/clipboardhelper;1",
                         "nsIClipboardHelper");
loader.lazyRequireGetter(this, "defer", "devtools/shared/defer");
loader.lazyRequireGetter(this, "Debugger", "Debugger");
loader.lazyRequireGetter(this, "EventEmitter", "devtools/shared/event-emitter");
loader.lazyRequireGetter(this, "AutocompletePopup", "devtools/client/shared/autocomplete-popup");
loader.lazyRequireGetter(this, "asyncStorage", "devtools/shared/async-storage");
loader.lazyRequireGetter(this, "PropTypes", "devtools/client/shared/vendor/react-prop-types");
loader.lazyRequireGetter(this, "gDevTools", "devtools/client/framework/devtools", true);
loader.lazyRequireGetter(this, "KeyCodes", "devtools/client/shared/keycodes", true);

const l10n = require("devtools/client/webconsole/webconsole-l10n");

// Constants used for defining the direction of JSTerm input history navigation.
const HISTORY_BACK = -1;
const HISTORY_FORWARD = 1;

const HELP_URL = "https://developer.mozilla.org/docs/Tools/Web_Console/Helpers";

const PREF_INPUT_HISTORY_COUNT = "devtools.webconsole.inputHistoryCount";
const PREF_AUTO_MULTILINE = "devtools.webconsole.autoMultiline";

function gSequenceId() {
  return gSequenceId.n++;
}
gSequenceId.n = 0;

const { Component } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");

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
      hud: PropTypes.object.isRequired,
      // Handler for clipboard 'paste' event (also used for 'drop' event).
      onPaste: PropTypes.func,
    };
  }

  constructor(props) {
    super(props);

    const {
      hud,
    } = props;

    this.hud = hud;
    this.hudId = this.hud.hudId;
    this.inputHistoryCount = Services.prefs.getIntPref(PREF_INPUT_HISTORY_COUNT);
    this._loadHistory();

    /**
     * Stores the data for the last completion.
     * @type object
     */
    this.lastCompletion = { value: null };

    this._keyPress = this._keyPress.bind(this);
    this._inputEventHandler = this._inputEventHandler.bind(this);
    this._focusEventHandler = this._focusEventHandler.bind(this);
    this._blurEventHandler = this._blurEventHandler.bind(this);

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

    /**
     * Tells if the input node changed since the last focus.
     *
     * @private
     * @type boolean
     */
    this._inputChanged = false;

    /**
     * Tells if the autocomplete popup was navigated since the last open.
     *
     * @private
     * @type boolean
     */
    this._autocompletePopupNavigated = false;

    /**
     * History of code that was executed.
     * @type array
     */
    this.history = [];
    this.autocompletePopup = null;
    this.inputNode = null;
    this.completeNode = null;

    this.COMPLETE_FORWARD = 0;
    this.COMPLETE_BACKWARD = 1;
    this.COMPLETE_HINT_ONLY = 2;
    this.COMPLETE_PAGEUP = 3;
    this.COMPLETE_PAGEDOWN = 4;

    EventEmitter.decorate(this);
    hud.jsterm = this;
  }

  componentDidMount() {
    if (!this.inputNode) {
      return;
    }

    let autocompleteOptions = {
      onSelect: this.onAutocompleteSelect.bind(this),
      onClick: this.acceptProposedCompletion.bind(this),
      listId: "webConsole_autocompletePopupListBox",
      position: "top",
      theme: "auto",
      autoSelect: true
    };

    let doc = this.hud.document;
    let toolbox = gDevTools.getToolbox(this.hud.owner.target);
    let tooltipDoc = toolbox ? toolbox.doc : doc;
    // The popup will be attached to the toolbox document or HUD document in the case
    // such as the browser console which doesn't have a toolbox.
    this.autocompletePopup = new AutocompletePopup(tooltipDoc, autocompleteOptions);

    this.inputBorderSize = this.inputNode.getBoundingClientRect().height -
                           this.inputNode.clientHeight;

    // Update the character width and height needed for the popup offset
    // calculations.
    this._updateCharSize();

    this.inputNode.addEventListener("keypress", this._keyPress);
    this.inputNode.addEventListener("input", this._inputEventHandler);
    this.inputNode.addEventListener("keyup", this._inputEventHandler);
    this.inputNode.addEventListener("focus", this._focusEventHandler);
    this.hud.window.addEventListener("blur", this._blurEventHandler);
    this.lastInputValue && this.setInputValue(this.lastInputValue);

    this.focus();
  }

  shouldComponentUpdate() {
    // XXX: For now, everything is handled in an imperative way and we only want React
    // to do the initial rendering of the component.
    // This should be modified when the actual refactoring will take place.
    return false;
  }

  /**
   * Load the console history from previous sessions.
   * @private
   */
  _loadHistory() {
    this.history = [];
    this.historyIndex = this.historyPlaceHolder = 0;

    this.historyLoaded = asyncStorage.getItem("webConsoleHistory")
      .then(value => {
        if (Array.isArray(value)) {
          // Since it was gotten asynchronously, there could be items already in
          // the history.  It's not likely but stick them onto the end anyway.
          this.history = value.concat(this.history);

          // Holds the number of entries in history. This value is incremented
          // in this.execute().
          this.historyIndex = this.history.length;

          // Holds the index of the history entry that the user is currently
          // viewing. This is reset to this.history.length when this.execute()
          // is invoked.
          this.historyPlaceHolder = this.history.length;
        }
      }, console.error);
  }

  /**
   * Clear the console history altogether.  Note that this will not affect
   * other consoles that are already opened (since they have their own copy),
   * but it will reset the array for all newly-opened consoles.
   * @returns Promise
   *          Resolves once the changes have been persisted.
   */
  clearHistory() {
    this.history = [];
    this.historyIndex = this.historyPlaceHolder = 0;
    return this.storeHistory();
  }

  /**
   * Stores the console history for future console instances.
   * @returns Promise
   *          Resolves once the changes have been persisted.
   */
  storeHistory() {
    return asyncStorage.setItem("webConsoleHistory", this.history);
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
    if (this.inputNode && !this.inputNode.getAttribute("focused")) {
      this.inputNode.focus();
    }
  }

  /**
   * The JavaScript evaluation response handler.
   *
   * @private
   * @param function [callback]
   *        Optional function to invoke when the evaluation result is added to
   *        the output.
   * @param object response
   *        The message received from the server.
   */
  _executeResultCallback(callback, response) {
    if (!this.hud) {
      return;
    }
    if (response.error) {
      console.error("Evaluation error " + response.error + ": " + response.message);
      return;
    }
    let errorMessage = response.exceptionMessage;

    // Wrap thrown strings in Error objects, so `throw "foo"` outputs "Error: foo"
    if (typeof response.exception === "string") {
      errorMessage = new Error(errorMessage).toString();
    }
    let result = response.result;
    let helperResult = response.helperResult;
    let helperHasRawOutput = !!(helperResult || {}).rawOutput;

    if (helperResult && helperResult.type) {
      switch (helperResult.type) {
        case "clearOutput":
          this.clearOutput();
          break;
        case "clearHistory":
          this.clearHistory();
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
      }
    }

    // Hide undefined results coming from JSTerm helper functions.
    if (!errorMessage && result && typeof result == "object" &&
      result.type == "undefined" &&
      helperResult && !helperHasRawOutput) {
      callback && callback();
      return;
    }

    if (this.hud.newConsoleOutput) {
      this.hud.newConsoleOutput.dispatchMessageAdd(response, true).then(callback);
    }
  }

  inspectObjectActor(objectActor) {
    this.hud.newConsoleOutput.dispatchMessageAdd({
      helperResult: {
        type: "inspectObject",
        object: objectActor
      }
    }, true);
    return this.hud.newConsoleOutput;
  }

  /**
   * Execute a string. Execution happens asynchronously in the content process.
   *
   * @param string [executeString]
   *        The string you want to execute. If this is not provided, the current
   *        user input is used - taken from |this.getInputValue()|.
   * @param function [callback]
   *        Optional function to invoke when the result is displayed.
   *        This is deprecated - please use the promise return value instead.
   * @returns Promise
   *          Resolves with the message once the result is displayed.
   */
  async execute(executeString, callback) {
    let deferred = defer();
    let resultCallback = msg => deferred.resolve(msg);

    // attempt to execute the content of the inputNode
    executeString = executeString || this.getInputValue();
    if (!executeString) {
      return null;
    }

    // Append a new value in the history of executed code, or overwrite the most
    // recent entry. The most recent entry may contain the last edited input
    // value that was not evaluated yet.
    this.history[this.historyIndex++] = executeString;
    this.historyPlaceHolder = this.history.length;

    if (this.history.length > this.inputHistoryCount) {
      this.history.splice(0, this.history.length - this.inputHistoryCount);
      this.historyIndex = this.historyPlaceHolder = this.history.length;
    }
    this.storeHistory();
    WebConsoleUtils.usageCount++;
    this.setInputValue("");
    this.clearCompletion();

    let selectedNodeActor = null;
    let inspectorSelection = this.hud.owner.getInspectorSelection();
    if (inspectorSelection && inspectorSelection.nodeFront) {
      selectedNodeActor = inspectorSelection.nodeFront.actorID;
    }

    const { ConsoleCommand } = require("devtools/client/webconsole/types");
    let message = new ConsoleCommand({
      messageText: executeString,
    });
    this.hud.proxy.dispatchMessageAdd(message);

    let onResult = this._executeResultCallback.bind(this, resultCallback);

    let options = {
      frame: this.SELECTED_FRAME,
      selectedNodeActor: selectedNodeActor,
    };

    const mappedString = await this.hud.owner.getMappedExpression(executeString);
    this.requestEvaluation(mappedString, options).then(onResult, onResult);

    return deferred.promise;
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
    let deferred = defer();

    function onResult(response) {
      if (!response.error) {
        deferred.resolve(response);
      } else {
        deferred.reject(response);
      }
    }

    let frameActor = null;
    if ("frame" in options) {
      frameActor = this.getFrameActor(options.frame);
    }

    let evalOptions = {
      bindObjectActor: options.bindObjectActor,
      frameActor: frameActor,
      selectedNodeActor: options.selectedNodeActor,
      selectedObjectActor: options.selectedObjectActor,
    };

    this.webConsoleClient.evaluateJSAsync(str, onResult, evalOptions);
    return deferred.promise;
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
    let state = this.hud.owner.getDebuggerFrames();
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
   * Clear the Web Console output.
   *
   * This method emits the "messages-cleared" notification.
   *
   * @param boolean clearStorage
   *        True if you want to clear the console messages storage associated to
   *        this Web Console.
   */
  clearOutput(clearStorage) {
    if (this.hud && this.hud.newConsoleOutput) {
      this.hud.newConsoleOutput.dispatchMessagesClear();
    }

    this.webConsoleClient.clearNetworkRequests();
    if (clearStorage) {
      this.webConsoleClient.clearMessagesCache();
    }
    this.focus();
    this.emit("messages-cleared");
  }

  /**
   * Remove all of the private messages from the Web Console output.
   *
   * This method emits the "private-messages-cleared" notification.
   */
  clearPrivateMessages() {
    if (this.hud && this.hud.newConsoleOutput) {
      this.hud.newConsoleOutput.dispatchPrivateMessagesClear();
      this.emit("private-messages-cleared");
    }
  }

  /**
   * Updates the size of the input field (command line) to fit its contents.
   *
   * @returns void
   */
  resizeInput() {
    if (!this.inputNode) {
      return;
    }

    let inputNode = this.inputNode;

    // Reset the height so that scrollHeight will reflect the natural height of
    // the contents of the input field.
    inputNode.style.height = "auto";

    // Now resize the input field to fit its contents.
    // TODO: remove `inputNode.inputField.scrollHeight` when the old
    // console UI is removed. See bug 1381834
    let scrollHeight = inputNode.inputField ?
      inputNode.inputField.scrollHeight : inputNode.scrollHeight;

    if (scrollHeight > 0) {
      inputNode.style.height = (scrollHeight + this.inputBorderSize) + "px";
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
    if (!this.inputNode) {
      return;
    }

    this.inputNode.value = newValue;
    this.lastInputValue = newValue;
    this.completeNode.value = "";
    this.resizeInput();
    this._inputChanged = true;
    this.emit("set-input-value");
  }

  /**
   * Gets the value from the input field
   * @returns string
   */
  getInputValue() {
    return this.inputNode ? this.inputNode.value || "" : "";
  }

  /**
   * The inputNode "input" and "keyup" event handler.
   * @private
   */
  _inputEventHandler() {
    if (this.lastInputValue != this.getInputValue()) {
      this.resizeInput();
      this.complete(this.COMPLETE_HINT_ONLY);
      this.lastInputValue = this.getInputValue();
      this._inputChanged = true;
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
    let inputNode = this.inputNode;
    let inputValue = this.getInputValue();
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
      let autoMultiline = Services.prefs.getBoolPref(PREF_AUTO_MULTILINE);
      if (event.shiftKey ||
          (!Debugger.isCompilableUnit(inputNode.value) && autoMultiline)) {
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
        if (this._autocompletePopupNavigated &&
            this.autocompletePopup.isOpen &&
            this.autocompletePopup.selectedIndex > -1) {
          this.acceptProposedCompletion();
        } else {
          this.execute();
          this._inputChanged = false;
        }
        event.preventDefault();
        break;

      case KeyCodes.DOM_VK_UP:
        if (this.autocompletePopup.isOpen) {
          inputUpdated = this.complete(this.COMPLETE_BACKWARD);
          if (inputUpdated) {
            this._autocompletePopupNavigated = true;
          }
        } else if (this.canCaretGoPrevious()) {
          inputUpdated = this.historyPeruse(HISTORY_BACK);
        }
        if (inputUpdated) {
          event.preventDefault();
        }
        break;

      case KeyCodes.DOM_VK_DOWN:
        if (this.autocompletePopup.isOpen) {
          inputUpdated = this.complete(this.COMPLETE_FORWARD);
          if (inputUpdated) {
            this._autocompletePopupNavigated = true;
          }
        } else if (this.canCaretGoNext()) {
          inputUpdated = this.historyPeruse(HISTORY_FORWARD);
        }
        if (inputUpdated) {
          event.preventDefault();
        }
        break;

      case KeyCodes.DOM_VK_PAGE_UP:
        if (this.autocompletePopup.isOpen) {
          inputUpdated = this.complete(this.COMPLETE_PAGEUP);
          if (inputUpdated) {
            this._autocompletePopupNavigated = true;
          }
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
          inputUpdated = this.complete(this.COMPLETE_PAGEDOWN);
          if (inputUpdated) {
            this._autocompletePopupNavigated = true;
          }
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
          this.autocompletePopup.selectedIndex = 0;
          event.preventDefault();
        } else if (inputValue.length <= 0) {
          this.hud.outputScroller.scrollTop = 0;
          event.preventDefault();
        }
        break;

      case KeyCodes.DOM_VK_END:
        if (this.autocompletePopup.isOpen) {
          this.autocompletePopup.selectedIndex =
            this.autocompletePopup.itemCount - 1;
          event.preventDefault();
        } else if (inputValue.length <= 0) {
          this.hud.outputScroller.scrollTop =
            this.hud.outputScroller.scrollHeight;
          event.preventDefault();
        }
        break;

      case KeyCodes.DOM_VK_LEFT:
        if (this.autocompletePopup.isOpen || this.lastCompletion.value) {
          this.clearCompletion();
        }
        break;

      case KeyCodes.DOM_VK_RIGHT:
        let cursorAtTheEnd = this.inputNode.selectionStart ==
                             this.inputNode.selectionEnd &&
                             this.inputNode.selectionStart ==
                             inputValue.length;
        let haveSuggestion = this.autocompletePopup.isOpen ||
                             this.lastCompletion.value;
        let useCompletion = cursorAtTheEnd || this._autocompletePopupNavigated;
        if (haveSuggestion && useCompletion &&
            this.complete(this.COMPLETE_HINT_ONLY) &&
            this.lastCompletion.value &&
            this.acceptProposedCompletion()) {
          event.preventDefault();
        }
        if (this.autocompletePopup.isOpen) {
          this.clearCompletion();
        }
        break;

      case KeyCodes.DOM_VK_TAB:
        // Generate a completion and accept the first proposed value.
        if (this.complete(this.COMPLETE_HINT_ONLY) &&
            this.lastCompletion &&
            this.acceptProposedCompletion()) {
          event.preventDefault();
        } else if (this._inputChanged) {
          this.updateCompleteNode(l10n.getStr("Autocomplete.blank"));
          event.preventDefault();
        }
        break;
      default:
        break;
    }
  }
  /* eslint-enable complexity */

  /**
   * The inputNode "focus" event handler.
   * @private
   */
  _focusEventHandler() {
    this._inputChanged = false;
  }

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
    if (!this.history.length) {
      return false;
    }

    // Up Arrow key
    if (direction == HISTORY_BACK) {
      if (this.historyPlaceHolder <= 0) {
        return false;
      }
      let inputVal = this.history[--this.historyPlaceHolder];

      // Save the current input value as the latest entry in history, only if
      // the user is already at the last entry.
      // Note: this code does not store changes to items that are already in
      // history.
      if (this.historyPlaceHolder + 1 == this.historyIndex) {
        this.history[this.historyIndex] = this.getInputValue() || "";
      }

      this.setInputValue(inputVal);
    } else if (direction == HISTORY_FORWARD) {
      // Down Arrow key
      if (this.historyPlaceHolder >= (this.history.length - 1)) {
        return false;
      }

      let inputVal = this.history[++this.historyPlaceHolder];
      this.setInputValue(inputVal);
    } else {
      throw new Error("Invalid argument 0");
    }

    return true;
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
    let node = this.inputNode;
    if (node.selectionStart != node.selectionEnd) {
      return false;
    }

    let multiline = /[\r\n]/.test(node.value);
    return node.selectionStart == 0 ? true :
           node.selectionStart == node.value.length && !multiline;
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
    let node = this.inputNode;
    if (node.selectionStart != node.selectionEnd) {
      return false;
    }

    let multiline = /[\r\n]/.test(node.value);
    return node.selectionStart == node.value.length ? true :
           node.selectionStart == 0 && !multiline;
  }

  /**
   * Completes the current typed text in the inputNode. Completion is performed
   * only if the selection/cursor is at the end of the string. If no completion
   * is found, the current inputNode value and cursor/selection stay.
   *
   * @param int type possible values are
   *    - this.COMPLETE_FORWARD: If there is more than one possible completion
   *          and the input value stayed the same compared to the last time this
   *          function was called, then the next completion of all possible
   *          completions is used. If the value changed, then the first possible
   *          completion is used and the selection is set from the current
   *          cursor position to the end of the completed text.
   *          If there is only one possible completion, then this completion
   *          value is used and the cursor is put at the end of the completion.
   *    - this.COMPLETE_BACKWARD: Same as this.COMPLETE_FORWARD but if the
   *          value stayed the same as the last time the function was called,
   *          then the previous completion of all possible completions is used.
   *    - this.COMPLETE_PAGEUP: Scroll up one page if available or select the
   *          first item.
   *    - this.COMPLETE_PAGEDOWN: Scroll down one page if available or select
   *          the last item.
   *    - this.COMPLETE_HINT_ONLY: If there is more than one possible
   *          completion and the input value stayed the same compared to the
   *          last time this function was called, then the same completion is
   *          used again. If there is only one possible completion, then
   *          the this.getInputValue() is set to this value and the selection
   *          is set from the current cursor position to the end of the
   *          completed text.
   * @param function callback
   *        Optional function invoked when the autocomplete properties are
   *        updated.
   * @returns boolean true if there existed a completion for the current input,
   *          or false otherwise.
   */
  complete(type, callback) {
    let inputNode = this.inputNode;
    let inputValue = this.getInputValue();
    let frameActor = this.getFrameActor(this.SELECTED_FRAME);

    // If the inputNode has no value, then don't try to complete on it.
    if (!inputValue) {
      this.clearCompletion();
      callback && callback(this);
      this.emit("autocomplete-updated");
      return false;
    }

    // Only complete if the selection is empty.
    if (inputNode.selectionStart != inputNode.selectionEnd) {
      this.clearCompletion();
      this.callback && callback(this);
      this.emit("autocomplete-updated");
      return false;
    }

    // Update the completion results.
    if (this.lastCompletion.value != inputValue ||
        frameActor != this._lastFrameActorId) {
      this._updateCompletionResult(type, callback);
      return false;
    }

    let popup = this.autocompletePopup;
    let accepted = false;

    if (type != this.COMPLETE_HINT_ONLY && popup.itemCount == 1) {
      this.acceptProposedCompletion();
      accepted = true;
    } else if (type == this.COMPLETE_BACKWARD) {
      popup.selectPreviousItem();
    } else if (type == this.COMPLETE_FORWARD) {
      popup.selectNextItem();
    } else if (type == this.COMPLETE_PAGEUP) {
      popup.selectPreviousPageItem();
    } else if (type == this.COMPLETE_PAGEDOWN) {
      popup.selectNextPageItem();
    }

    callback && callback(this);
    this.emit("autocomplete-updated");
    return accepted || popup.itemCount > 0;
  }

  /**
   * Update the completion result. This operation is performed asynchronously by
   * fetching updated results from the content process.
   *
   * @private
   * @param int type
   *        Completion type. See this.complete() for details.
   * @param function [callback]
   *        Optional, function to invoke when completion results are received.
   */
  _updateCompletionResult(type, callback) {
    let frameActor = this.getFrameActor(this.SELECTED_FRAME);
    if (this.lastCompletion.value == this.getInputValue() &&
        frameActor == this._lastFrameActorId) {
      return;
    }

    let requestId = gSequenceId();
    let cursor = this.inputNode.selectionStart;
    let input = this.getInputValue().substring(0, cursor);
    let cache = this._autocompleteCache;

    // If the current input starts with the previous input, then we already
    // have a list of suggestions and we just need to filter the cached
    // suggestions. When the current input ends with a non-alphanumeri;
    // character we ask the server again for suggestions.

    // Check if last character is non-alphanumeric
    if (!/[a-zA-Z0-9]$/.test(input) || frameActor != this._lastFrameActorId) {
      this._autocompleteQuery = null;
      this._autocompleteCache = null;
    }

    if (this._autocompleteQuery && input.startsWith(this._autocompleteQuery)) {
      let filterBy = input;
      // Find the last non-alphanumeric other than _ or $ if it exists.
      let lastNonAlpha = input.match(/[^a-zA-Z0-9_$][a-zA-Z0-9_$]*$/);
      // If input contains non-alphanumerics, use the part after the last one
      // to filter the cache
      if (lastNonAlpha) {
        filterBy = input.substring(input.lastIndexOf(lastNonAlpha) + 1);
      }

      let newList = cache.sort().filter(function(l) {
        return l.startsWith(filterBy);
      });

      this.lastCompletion = {
        requestId: null,
        completionType: type,
        value: null,
      };

      let response = { matches: newList, matchProp: filterBy };
      this._receiveAutocompleteProperties(null, callback, response);
      return;
    }

    this._lastFrameActorId = frameActor;

    this.lastCompletion = {
      requestId: requestId,
      completionType: type,
      value: null,
    };

    let autocompleteCallback =
      this._receiveAutocompleteProperties.bind(this, requestId, callback);

    this.webConsoleClient.autocomplete(
      input, cursor, autocompleteCallback, frameActor);
  }

  /**
   * Handler for the autocompletion results. This method takes
   * the completion result received from the server and updates the UI
   * accordingly.
   *
   * @param number requestId
   *        Request ID.
   * @param function [callback=null]
   *        Optional, function to invoke when the completion result is received.
   * @param object message
   *        The JSON message which holds the completion results received from
   *        the content process.
   */
  _receiveAutocompleteProperties(requestId, callback, message) {
    let inputNode = this.inputNode;
    let inputValue = this.getInputValue();
    if (this.lastCompletion.value == inputValue ||
        requestId != this.lastCompletion.requestId) {
      return;
    }
    // Cache whatever came from the server if the last char is
    // alphanumeric or '.'
    let cursor = inputNode.selectionStart;
    let inputUntilCursor = inputValue.substring(0, cursor);

    if (requestId != null && /[a-zA-Z0-9.]$/.test(inputUntilCursor)) {
      this._autocompleteCache = message.matches;
      this._autocompleteQuery = inputUntilCursor;
    }

    let matches = message.matches;
    let lastPart = message.matchProp;
    if (!matches.length) {
      this.clearCompletion();
      callback && callback(this);
      this.emit("autocomplete-updated");
      return;
    }

    let items = matches.reverse().map(function(match) {
      return { preLabel: lastPart, label: match };
    });

    let popup = this.autocompletePopup;
    popup.setItems(items);

    let completionType = this.lastCompletion.completionType;
    this.lastCompletion = {
      value: inputValue,
      matchProp: lastPart,
    };
    if (items.length > 1 && !popup.isOpen) {
      let str = this.getInputValue().substr(0, this.inputNode.selectionStart);
      let offset = str.length - (str.lastIndexOf("\n") + 1) - lastPart.length;
      let x = offset * this._inputCharWidth;
      popup.openPopup(inputNode, x + this._chevronWidth);
      this._autocompletePopupNavigated = false;
    } else if (items.length < 2 && popup.isOpen) {
      popup.hidePopup();
      this._autocompletePopupNavigated = false;
    }
    if (items.length == 1) {
      popup.selectedIndex = 0;
    }

    this.onAutocompleteSelect();

    if (completionType != this.COMPLETE_HINT_ONLY && popup.itemCount == 1) {
      this.acceptProposedCompletion();
    } else if (completionType == this.COMPLETE_BACKWARD) {
      popup.selectPreviousItem();
    } else if (completionType == this.COMPLETE_FORWARD) {
      popup.selectNextItem();
    }

    callback && callback(this);
    this.emit("autocomplete-updated");
  }

  onAutocompleteSelect() {
    // Render the suggestion only if the cursor is at the end of the input.
    if (this.inputNode.selectionStart != this.getInputValue().length) {
      return;
    }

    let currentItem = this.autocompletePopup.selectedItem;
    if (currentItem && this.lastCompletion.value) {
      let suffix =
        currentItem.label.substring(this.lastCompletion.matchProp.length);
      this.updateCompleteNode(suffix);
    } else {
      this.updateCompleteNode("");
    }
  }

  /**
   * Clear the current completion information and close the autocomplete popup,
   * if needed.
   */
  clearCompletion() {
    this.lastCompletion = { value: null };
    this.updateCompleteNode("");
    if (this.autocompletePopup) {
      this.autocompletePopup.clearItems();

      if (this.autocompletePopup.isOpen) {
        // Trigger a blur/focus of the JSTerm input to force screen readers to read the
        // value again.
        this.inputNode.blur();
        this.autocompletePopup.once("popup-closed", () => {
          this.inputNode.focus();
        });
        this.autocompletePopup.hidePopup();
        this._autocompletePopupNavigated = false;
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
    let updated = false;

    let currentItem = this.autocompletePopup.selectedItem;
    if (currentItem && this.lastCompletion.value) {
      let suffix =
        currentItem.label.substring(this.lastCompletion.matchProp.length);
      let cursor = this.inputNode.selectionStart;
      let value = this.getInputValue();
      this.setInputValue(value.substr(0, cursor) +
        suffix + value.substr(cursor));
      let newCursor = cursor + suffix.length;
      this.inputNode.selectionStart = this.inputNode.selectionEnd = newCursor;
      updated = true;
    }

    this.clearCompletion();

    return updated;
  }

  /**
   * Update the node that displays the currently selected autocomplete proposal.
   *
   * @param string suffix
   *        The proposed suffix for the inputNode value.
   */
  updateCompleteNode(suffix) {
    if (!this.completeNode) {
      return;
    }

    // completion prefix = input, with non-control chars replaced by spaces
    let prefix = suffix ? this.getInputValue().replace(/[\S]/g, " ") : "";
    this.completeNode.value = prefix + suffix;
  }
  /**
   * Calculates the width and height of a single character of the input box.
   * This will be used in opening the popup at the correct offset.
   *
   * @private
   */
  _updateCharSize() {
    let doc = this.hud.document;
    let tempLabel = doc.createElement("span");
    let style = tempLabel.style;
    style.position = "fixed";
    style.padding = "0";
    style.margin = "0";
    style.width = "auto";
    style.color = "transparent";
    WebConsoleUtils.copyTextStyles(this.inputNode, tempLabel);
    tempLabel.textContent = "x";
    doc.documentElement.appendChild(tempLabel);
    this._inputCharWidth = tempLabel.offsetWidth;
    tempLabel.remove();
    // Calculate the width of the chevron placed at the beginning of the input
    // box. Remove 4 more pixels to accommodate the padding of the popup.
    this._chevronWidth = +doc.defaultView.getComputedStyle(this.inputNode)
                             .paddingLeft.replace(/[^0-9.]/g, "") - 4;
  }

  destroy() {
    this.clearCompletion();

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
      this.inputNode.removeEventListener("focus", this._focusEventHandler);
      this.hud.window.removeEventListener("blur", this._blurEventHandler);
    }

    this.hud = null;
  }

  render() {
    if (this.props.hud.isBrowserConsole &&
        !Services.prefs.getBoolPref("devtools.chrome.enabled")) {
      return null;
    }

    let {
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
        })
      )
    );
  }
}

module.exports = JSTerm;
