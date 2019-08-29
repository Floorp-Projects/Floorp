/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const { debounce } = require("devtools/shared/debounce");
const isMacOS = Services.appinfo.OS === "Darwin";

loader.lazyRequireGetter(this, "Debugger", "Debugger");
loader.lazyRequireGetter(this, "EventEmitter", "devtools/shared/event-emitter");
loader.lazyRequireGetter(
  this,
  "AutocompletePopup",
  "devtools/client/shared/autocomplete-popup"
);
loader.lazyRequireGetter(
  this,
  "PropTypes",
  "devtools/client/shared/vendor/react-prop-types"
);
loader.lazyRequireGetter(
  this,
  "KeyCodes",
  "devtools/client/shared/keycodes",
  true
);
loader.lazyRequireGetter(
  this,
  "Editor",
  "devtools/client/shared/sourceeditor/editor"
);
loader.lazyRequireGetter(
  this,
  "focusableSelector",
  "devtools/client/shared/focus",
  true
);
loader.lazyRequireGetter(
  this,
  "l10n",
  "devtools/client/webconsole/utils/messages",
  true
);

// React & Redux
const { Component } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { connect } = require("devtools/client/shared/vendor/react-redux");

// History Modules
const {
  getHistory,
  getHistoryValue,
} = require("devtools/client/webconsole/selectors/history");
const {
  getAutocompleteState,
} = require("devtools/client/webconsole/selectors/autocomplete");
const actions = require("devtools/client/webconsole/actions/index");

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
      // Evaluate provided expression.
      evaluateExpression: PropTypes.func.isRequired,
      // Update position in the history after executing an expression (action).
      updateHistoryPosition: PropTypes.func.isRequired,
      // Update autocomplete popup state.
      autocompleteUpdate: PropTypes.func.isRequired,
      autocompleteClear: PropTypes.func.isRequired,
      // Data to be displayed in the autocomplete popup.
      autocompleteData: PropTypes.object.isRequired,
      // Toggle the editor mode.
      editorToggle: PropTypes.func.isRequired,
      // Dismiss the editor onboarding UI.
      editorOnboardingDismiss: PropTypes.func.isRequired,
      // Is the editor feature enabled
      editorFeatureEnabled: PropTypes.bool,
      // Is the input in editor mode.
      editorMode: PropTypes.bool,
      editorWidth: PropTypes.number,
      showEditorOnboarding: PropTypes.bool,
      autocomplete: PropTypes.bool,
    };
  }

  constructor(props) {
    super(props);

    const { webConsoleUI } = props;

    this.webConsoleUI = webConsoleUI;
    this.hudId = this.webConsoleUI.hudId;

    this._onEditorChanges = this._onEditorChanges.bind(this);
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

    EventEmitter.decorate(this);
    webConsoleUI.jsterm = this;
  }

  componentDidMount() {
    if (this.props.editorMode) {
      this.setEditorWidth(this.props.editorWidth);
    }

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
    this.autocompletePopup = new AutocompletePopup(
      tooltipDoc,
      autocompleteOptions
    );

    if (this.node) {
      const onArrowUp = () => {
        let inputUpdated;
        if (this.autocompletePopup.isOpen) {
          this.autocompletePopup.selectPreviousItem();
          return null;
        }

        if (this.props.editorMode === false && this.canCaretGoPrevious()) {
          inputUpdated = this.historyPeruse(HISTORY_BACK);
        }

        return inputUpdated ? null : "CodeMirror.Pass";
      };

      const onArrowDown = () => {
        let inputUpdated;
        if (this.autocompletePopup.isOpen) {
          this.autocompletePopup.selectNextItem();
          return null;
        }

        if (this.props.editorMode === false && this.canCaretGoNext()) {
          inputUpdated = this.historyPeruse(HISTORY_FORWARD);
        }

        return inputUpdated ? null : "CodeMirror.Pass";
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

      const onCtrlCmdEnter = () => {
        if (this.hasAutocompletionSuggestion()) {
          return this.acceptProposedCompletion();
        }

        this._execute();
        return null;
      };

      this.editor = new Editor({
        autofocus: true,
        enableCodeFolding: false,
        autoCloseBrackets: false,
        lineNumbers: this.props.editorMode,
        lineWrapping: true,
        mode: Editor.modes.js,
        styleActiveLine: false,
        tabIndex: "0",
        viewportMargin: Infinity,
        disableSearchAddon: true,
        extraKeys: {
          Enter: () => {
            // No need to handle shift + Enter as it's natively handled by CodeMirror.

            const hasSuggestion = this.hasAutocompletionSuggestion();
            if (
              !hasSuggestion &&
              !Debugger.isCompilableUnit(this._getValue())
            ) {
              // incomplete statement
              return "CodeMirror.Pass";
            }

            if (hasSuggestion) {
              return this.acceptProposedCompletion();
            }

            if (!this.props.editorMode) {
              this._execute();
              return null;
            }
            return "CodeMirror.Pass";
          },

          "Cmd-Enter": onCtrlCmdEnter,
          "Ctrl-Enter": onCtrlCmdEnter,

          Tab: () => {
            if (this.hasEmptyInput()) {
              this.editor.codeMirror.getInputField().blur();
              return false;
            }

            if (
              this.props.autocompleteData &&
              this.props.autocompleteData.getterPath
            ) {
              this.props.autocompleteUpdate(
                true,
                this.props.autocompleteData.getterPath
              );
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

          Up: onArrowUp,
          "Cmd-Up": onArrowUp,

          Down: onArrowDown,
          "Cmd-Down": onArrowDown,

          Left: onArrowLeft,
          "Ctrl-Left": onArrowLeft,
          "Cmd-Left": onArrowLeft,
          "Alt-Left": onArrowLeft,
          // On OSX, Ctrl-A navigates to the beginning of the line.
          "Ctrl-A": isMacOS ? onArrowLeft : undefined,

          Right: onArrowRight,
          "Ctrl-Right": onArrowRight,
          "Cmd-Right": onArrowRight,
          "Alt-Right": onArrowRight,

          "Ctrl-N": () => {
            // Control-N differs from down arrow: it ignores autocomplete state.
            // Note that we preserve the default 'down' navigation within
            // multiline text.
            if (
              Services.appinfo.OS === "Darwin" &&
              this.props.editorMode === false &&
              this.canCaretGoNext() &&
              this.historyPeruse(HISTORY_FORWARD)
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
              Services.appinfo.OS === "Darwin" &&
              this.props.editorMode === false &&
              this.canCaretGoPrevious() &&
              this.historyPeruse(HISTORY_BACK)
            ) {
              return null;
            }

            this.clearCompletion();
            return "CodeMirror.Pass";
          },

          PageUp: () => {
            if (this.autocompletePopup.isOpen) {
              this.autocompletePopup.selectPreviousPageItem();
            } else {
              const { outputScroller } = this.webConsoleUI;
              const { scrollTop, clientHeight } = outputScroller;
              outputScroller.scrollTop = Math.max(0, scrollTop - clientHeight);
            }

            return null;
          },

          PageDown: () => {
            if (this.autocompletePopup.isOpen) {
              this.autocompletePopup.selectNextPageItem();
            } else {
              const { outputScroller } = this.webConsoleUI;
              const { scrollTop, scrollHeight, clientHeight } = outputScroller;
              outputScroller.scrollTop = Math.min(
                scrollHeight,
                scrollTop + clientHeight
              );
            }

            return null;
          },

          Home: () => {
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

          End: () => {
            if (this.autocompletePopup.isOpen) {
              this.autocompletePopup.selectItemAtIndex(
                this.autocompletePopup.itemCount - 1
              );
              return null;
            }

            if (!this._getValue()) {
              const { outputScroller } = this.webConsoleUI;
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

          Esc: false,
          "Cmd-F": false,
          "Ctrl-F": false,
        },
      });

      this.editor.on("changes", this._onEditorChanges);
      this.editor.on("beforeChange", this._onEditorBeforeChange);
      this.editor.appendToLocalElement(this.node);
      const cm = this.editor.codeMirror;
      cm.on("paste", (_, event) => this.props.onPaste(event));
      cm.on("drop", (_, event) => this.props.onPaste(event));

      this.node.addEventListener("keydown", event => {
        if (event.keyCode === KeyCodes.DOM_VK_ESCAPE) {
          if (this.autocompletePopup.isOpen) {
            this.clearCompletion();
            event.preventDefault();
            event.stopPropagation();
          }

          if (
            this.props.autocompleteData &&
            this.props.autocompleteData.getterPath
          ) {
            this.props.autocompleteClear();
            event.preventDefault();
            event.stopPropagation();
          }
        }
      });

      // Update the character width needed for the popup offset calculations.
      this._inputCharWidth = this._getInputCharWidth();
      this.lastInputValue && this._setValue(this.lastInputValue);
    }
  }

  componentWillReceiveProps(nextProps) {
    this.imperativeUpdate(nextProps);
  }

  shouldComponentUpdate(nextProps) {
    return (
      this.props.showEditorOnboarding !== nextProps.showEditorOnboarding ||
      this.props.editorMode !== nextProps.editorMode
    );
  }

  /**
   * Do all the imperative work needed after a Redux store update.
   *
   * @param {Object} nextProps: props passed from shouldComponentUpdate.
   */
  imperativeUpdate(nextProps) {
    if (!nextProps) {
      return;
    }

    if (
      nextProps.autocompleteData !== this.props.autocompleteData &&
      nextProps.autocompleteData.pendingRequestId === null
    ) {
      this.updateAutocompletionPopup(nextProps.autocompleteData);
    }

    if (nextProps.editorMode !== this.props.editorMode) {
      if (this.editor) {
        this.editor.setOption("lineNumbers", nextProps.editorMode);
      }

      if (nextProps.editorMode && nextProps.editorWidth) {
        this.setEditorWidth(nextProps.editorWidth);
      } else {
        this.setEditorWidth(null);
      }
    }
  }

  /**
   *
   * @param {Number|null} editorWidth: The width to set the node to. If null, removes any
   *                                   `width` property on node style.
   */
  setEditorWidth(editorWidth) {
    if (!this.node) {
      return;
    }

    if (editorWidth) {
      this.node.style.width = `${editorWidth}px`;
    } else {
      this.node.style.removeProperty("width");
    }
  }

  focus() {
    if (this.editor) {
      this.editor.focus();
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

      const index = inputIndex > 0 ? inputIndex - 1 : items.length - 1;
      return items[index];
    };

    const focusableEl = findPreviousFocusableElement(this.node.parentNode);
    if (focusableEl) {
      focusableEl.focus();
    }
  }

  /**
   * Execute a string. Execution happens asynchronously in the content process.
   */
  _execute() {
    const executeString = this.getSelectedText() || this._getValue();
    if (!executeString) {
      return;
    }

    if (!this.props.editorMode) {
      this._setValue("");
    }
    this.clearCompletion();
    this.props.evaluateExpression(executeString);
  }

  /**
   * Sets the value of the input field.
   *
   * @param string newValue
   *        The new value to set.
   * @returns void
   */
  _setValue(newValue = "") {
    this.lastInputValue = newValue;

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

    this.emit("set-input-value");
  }

  /**
   * Gets the value from the input field
   * @returns string
   */
  _getValue() {
    return this.editor ? this.editor.getText() || "" : "";
  }

  getSelectionStart() {
    return this.getInputValueBeforeCursor().length;
  }

  getSelectedText() {
    return this.editor.getSelection();
  }

  /**
   * Even handler for the "beforeChange" event fired by codeMirror. This event is fired
   * when codeMirror is about to make a change to its DOM representation.
   */
  _onEditorBeforeChange(cm, change) {
    // If the user did not type a character that matches the completion text, then we
    // clear it before the change is done to prevent a visual glitch.
    // See Bugs 1491776 & 1558248.
    const { from, to, origin, text } = change;
    const completionText = this.getAutoCompletionText();

    const addedCharacterMatchCompletion =
      from.line === to.line &&
      from.ch === to.ch &&
      origin === "+input" &&
      completionText.startsWith(text.join(""));

    if (!completionText || change.canceled || !addedCharacterMatchCompletion) {
      this.setAutoCompletionText("");
    }
  }

  /**
   * The editor "changes" event handler.
   */
  _onEditorChanges() {
    const value = this._getValue();
    if (this.lastInputValue !== value) {
      if (this.props.autocomplete || this.autocompletePopup.isOpen) {
        this.autocompleteUpdate();
      }
      this.lastInputValue = value;
    }
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
    const { history, updateHistoryPosition, getValueFromHistory } = this.props;

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
   * Check if the caret is at a location that allows selecting the previous item
   * in history when the user presses the Up arrow key.
   *
   * @return boolean
   *         True if the caret is at a location that allows selecting the
   *         previous item in history when the user presses the Up arrow key,
   *         otherwise false.
   */
  canCaretGoPrevious() {
    if (!this.editor) {
      return false;
    }

    const inputValue = this._getValue();
    const { line, ch } = this.editor.getCursor();
    return (line === 0 && ch === 0) || (line === 0 && ch === inputValue.length);
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
    if (!this.editor) {
      return false;
    }

    const inputValue = this._getValue();
    const multiline = /[\r\n]/.test(inputValue);

    const { line, ch } = this.editor.getCursor();
    return (
      (!multiline && ch === 0) ||
      this.editor.getDoc().getRange({ line: 0, ch: 0 }, { line, ch }).length ===
        inputValue.length
    );
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
    if (!this.editor) {
      return;
    }

    const { matches, matchProp, isElementAccess } = data;
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
      return { preLabel, label, isElementAccess };
    });

    if (items.length > 0) {
      const { preLabel, label } = items[0];
      let suffix = label.substring(preLabel.length);
      if (isElementAccess) {
        if (!matchProp) {
          suffix = label;
        }
        const inputAfterCursor = this._getValue().substring(
          inputUntilCursor.length
        );
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
    if (
      items.length >= minimumAutoCompleteLength ||
      (items.length === 1 && items[0].preLabel !== matchProp) ||
      (items.length === 1 &&
        !this.canDisplayAutoCompletionText() &&
        items[0].label !== matchProp)
    ) {
      // We need to show the popup at the "." or "[".
      const xOffset = -1 * matchProp.length * this._inputCharWidth;
      const yOffset = 5;
      const popupAlignElement = this.props.serviceContainer.getJsTermTooltipAnchor();
      popup.openPopup(popupAlignElement, xOffset, yOffset, 0, {
        preventSelectCallback: true,
      });
    } else if (items.length < minimumAutoCompleteLength && popup.isOpen) {
      popup.hidePopup();
    }
    this.emit("autocomplete-updated");
  }

  onAutocompleteSelect() {
    const { selectedItem } = this.autocompletePopup;
    if (selectedItem) {
      const { preLabel, label, isElementAccess } = selectedItem;
      let suffix = label.substring(preLabel.length);

      // If the user is performing an element access, we need to check if we should add
      // starting and ending quotes, as well as a closing bracket.
      if (isElementAccess) {
        const inputBeforeCursor = this.getInputValueBeforeCursor();
        if (inputBeforeCursor.trim().endsWith("[")) {
          suffix = label;
        }

        const inputAfterCursor = this._getValue().substring(
          inputBeforeCursor.length
        );
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
        this.autocompletePopup.once("popup-closed", () => {
          this.focus();
        });
        this.autocompletePopup.hidePopup();
      }
      this.emit("autocomplete-updated");
    }
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
      const { selectedItem } = this.autocompletePopup;
      const { label, preLabel, isElementAccess } = selectedItem;

      completionText = label;
      numberOfCharsToReplaceCharsBeforeCursor = preLabel.length;

      // If the user is performing an element access, we need to check if we should add
      // starting and ending quotes, as well as a closing bracket.
      if (isElementAccess) {
        const inputBeforeCursor = this.getInputValueBeforeCursor();
        const lastOpeningBracketIndex = inputBeforeCursor.lastIndexOf("[");
        if (lastOpeningBracketIndex > -1) {
          numberOfCharsToReplaceCharsBeforeCursor = inputBeforeCursor.substring(
            lastOpeningBracketIndex + 1
          ).length;
        }

        const inputAfterCursor = this._getValue().substring(
          inputBeforeCursor.length
        );
        // If there's not a bracket after the cursor, add it.
        if (!inputAfterCursor.trimLeft().startsWith("]")) {
          completionText = completionText + "]";
        }
      }
    }

    this.props.autocompleteClear();

    if (completionText) {
      this.insertStringAtCursor(
        completionText,
        numberOfCharsToReplaceCharsBeforeCursor
      );
    }
  }

  getInputValueBeforeCursor() {
    return this.editor
      ? this.editor
          .getDoc()
          .getRange({ line: 0, ch: 0 }, this.editor.getCursor())
      : null;
  }

  /**
   * Insert a string into the console at the cursor location,
   * moving the cursor to the end of the string.
   *
   * @param {string} str
   * @param {int} numberOfCharsToReplaceCharsBeforeCursor - defaults to 0
   */
  insertStringAtCursor(str, numberOfCharsToReplaceCharsBeforeCursor = 0) {
    if (!this.editor) {
      return;
    }

    const value = this._getValue();
    let prefix = this.getInputValueBeforeCursor();
    const suffix = value.replace(prefix, "");

    if (numberOfCharsToReplaceCharsBeforeCursor) {
      prefix = prefix.substring(
        0,
        prefix.length - numberOfCharsToReplaceCharsBeforeCursor
      );
    }

    // We need to retrieve the cursor before setting the new value.
    const { line, ch } = this.editor.getCursor();

    this._setValue(prefix + str + suffix);

    // Set the cursor on the same line it was already at, after the autocompleted text
    this.editor.setCursor({
      line,
      ch: ch + str.length - numberOfCharsToReplaceCharsBeforeCursor,
    });
  }

  /**
   * Set the autocompletion text of the input.
   *
   * @param string suffix
   *        The proposed suffix for the input value.
   */
  setAutoCompletionText(suffix) {
    if (!this.editor) {
      return;
    }

    if (suffix && !this.canDisplayAutoCompletionText()) {
      suffix = "";
    }

    this.editor.setAutoCompletionText(suffix);
  }

  getAutoCompletionText() {
    return this.editor ? this.editor.getAutoCompletionText() : null;
  }

  /**
   * Indicate if the input has an autocompletion suggestion, i.e. that there is either
   * something in the autocompletion text or that there's a selected item in the
   * autocomplete popup.
   */
  hasAutocompletionSuggestion() {
    // We can have cases where the popup is opened but we can't display the autocompletion
    // text.
    return (
      this.getAutoCompletionText() ||
      (this.autocompletePopup.isOpen &&
        Number.isInteger(this.autocompletePopup.selectedIndex) &&
        this.autocompletePopup.selectedIndex > -1)
    );
  }

  /**
   * Returns a boolean indicating if we can display an autocompletion text in the input,
   * i.e. if there is no characters displayed on the same line of the cursor and after it.
   */
  canDisplayAutoCompletionText() {
    if (!this.editor) {
      return false;
    }

    const { ch, line } = this.editor.getCursor();
    const lineContent = this.editor.getLine(line);
    const textAfterCursor = lineContent.substring(ch);
    return textAfterCursor === "";
  }

  /**
   * Calculates and returns the width of a single character of the input box.
   * This will be used in opening the popup at the correct offset.
   *
   * @returns {Number|null}: Width off the "x" char, or null if the input does not exist.
   */
  _getInputCharWidth() {
    return this.editor ? this.editor.defaultCharWidth() : null;
  }

  onContextMenu(e) {
    this.props.serviceContainer.openEditContextMenu(e);
  }

  destroy() {
    if (this.autocompletePopup) {
      this.autocompletePopup.destroy();
      this.autocompletePopup = null;
    }

    if (this.editor) {
      this.editor.destroy();
      this.editor = null;
    }

    this.webConsoleUI = null;
  }

  renderOpenEditorButton() {
    if (!this.props.editorFeatureEnabled || this.props.editorMode) {
      return null;
    }

    return dom.button({
      className: "devtools-button webconsole-input-openEditorButton",
      title: l10n.getFormatStr("webconsole.input.openEditorButton.tooltip2", [
        isMacOS ? "Cmd + B" : "Ctrl + B",
      ]),
      onClick: this.props.editorToggle,
    });
  }

  renderEditorOnboarding() {
    if (!this.props.editorFeatureEnabled || !this.props.showEditorOnboarding) {
      return null;
    }

    // We deliberately use getStr, and not getFormatStr, because we want keyboard
    // shortcuts to be wrapped in their own span.
    const label = l10n.getStr("webconsole.input.editor.onboarding.label");
    let [prefix, suffix] = label.split("%1$S");
    suffix = suffix.split("%2$S");

    const enterString = l10n.getStr("webconsole.enterKey");

    return dom.header(
      { className: "editor-onboarding" },
      dom.img({
        className: "editor-onboarding-fox",
        src: "chrome://devtools/skin/images/fox-smiling.svg",
      }),
      dom.p(
        {},
        prefix,
        dom.span({ className: "editor-onboarding-shortcut" }, enterString),
        suffix[0],
        dom.span({ className: "editor-onboarding-shortcut" }, [
          isMacOS ? `Cmd+${enterString}` : `Ctrl+${enterString}`,
        ]),
        suffix[1]
      ),
      dom.button(
        {
          className: "editor-onboarding-dismiss-button",
          onClick: () => this.props.editorOnboardingDismiss(),
        },
        l10n.getStr("webconsole.input.editor.onboarding.dissmis.label")
      )
    );
  }

  render() {
    if (
      this.props.webConsoleUI.isBrowserConsole &&
      !Services.prefs.getBoolPref("devtools.chrome.enabled")
    ) {
      return null;
    }

    return dom.div(
      {
        className: "jsterm-input-container devtools-input devtools-monospace",
        key: "jsterm-container",
        style: { direction: "ltr" },
        "aria-live": "off",
        onContextMenu: this.onContextMenu,
        ref: node => {
          this.node = node;
        },
      },
      this.renderOpenEditorButton(),
      this.renderEditorOnboarding()
    );
  }
}

// Redux connect

function mapStateToProps(state) {
  return {
    history: getHistory(state),
    getValueFromHistory: direction => getHistoryValue(state, direction),
    autocompleteData: getAutocompleteState(state),
    showEditorOnboarding: state.ui.showEditorOnboarding,
  };
}

function mapDispatchToProps(dispatch) {
  return {
    updateHistoryPosition: (direction, expression) =>
      dispatch(actions.updateHistoryPosition(direction, expression)),
    autocompleteUpdate: (force, getterPath) =>
      dispatch(actions.autocompleteUpdate(force, getterPath)),
    autocompleteClear: () => dispatch(actions.autocompleteClear()),
    evaluateExpression: expression =>
      dispatch(actions.evaluateExpression(expression)),
    editorToggle: () => dispatch(actions.editorToggle()),
    editorOnboardingDismiss: () => dispatch(actions.editorOnboardingDismiss()),
  };
}

module.exports = connect(
  mapStateToProps,
  mapDispatchToProps
)(JSTerm);
