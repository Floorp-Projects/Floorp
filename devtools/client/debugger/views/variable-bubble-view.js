/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* import-globals-from ../debugger-controller.js */
/* import-globals-from ../debugger-view.js */
/* import-globals-from ../utils.js */
/* globals document, window */
"use strict";

/**
 * Functions handling the variables bubble UI.
 */
function VariableBubbleView(DebuggerController, DebuggerView) {
  dumpn("VariableBubbleView was instantiated");

  this.StackFrames = DebuggerController.StackFrames;
  this.Parser = DebuggerController.Parser;
  this.DebuggerView = DebuggerView;

  this._onMouseMove = this._onMouseMove.bind(this);
  this._onMouseOut = this._onMouseOut.bind(this);
  this._onPopupHiding = this._onPopupHiding.bind(this);
}

VariableBubbleView.prototype = {
  /**
   * Delay before showing the variables bubble tooltip when hovering a valid
   * target.
   */
  TOOLTIP_SHOW_DELAY: 750,

  /**
   * Tooltip position for the variables bubble tooltip.
   */
  TOOLTIP_POSITION: "topcenter bottomleft",

  /**
   * Initialization function, called when the debugger is started.
   */
  initialize: function() {
    dumpn("Initializing the VariableBubbleView");

    this._toolbox = DebuggerController._toolbox;
    this._editorContainer = document.getElementById("editor");
    this._editorContainer.addEventListener("mousemove", this._onMouseMove, false);
    this._editorContainer.addEventListener("mouseout", this._onMouseOut, false);

    this._tooltip = new Tooltip(document, {
      closeOnEvents: [{
        emitter: this._toolbox,
        event: "select"
      }, {
        emitter: this._editorContainer,
        event: "scroll",
        useCapture: true
      }, {
        emitter: document,
        event: "keydown"
      }]
    });
    this._tooltip.defaultPosition = this.TOOLTIP_POSITION;
    this._tooltip.panel.addEventListener("popuphiding", this._onPopupHiding);
  },

  /**
   * Destruction function, called when the debugger is closed.
   */
  destroy: function() {
    dumpn("Destroying the VariableBubbleView");

    this._tooltip.panel.removeEventListener("popuphiding", this._onPopupHiding);
    this._editorContainer.removeEventListener("mousemove", this._onMouseMove, false);
    this._editorContainer.removeEventListener("mouseout", this._onMouseOut, false);
  },

  /**
   * Specifies whether literals can be (redundantly) inspected in a popup.
   * This behavior is deprecated, but still tested in a few places.
   */
  _ignoreLiterals: true,

  /**
   * Searches for an identifier underneath the specified position in the
   * source editor, and if found, opens a VariablesView inspection popup.
   *
   * @param number x, y
   *        The left/top coordinates where to look for an identifier.
   */
  _findIdentifier: function(x, y) {
    let editor = this.DebuggerView.editor;

    // Calculate the editor's line and column at the current x and y coords.
    let hoveredPos = editor.getPositionFromCoords({ left: x, top: y });
    let hoveredOffset = editor.getOffset(hoveredPos);
    let hoveredLine = hoveredPos.line;
    let hoveredColumn = hoveredPos.ch;

    // A source contains multiple scripts. Find the start index of the script
    // containing the specified offset relative to its parent source.
    let contents = editor.getText();
    let location = this.DebuggerView.Sources.selectedValue;
    let parsedSource = this.Parser.get(contents, location);
    let scriptInfo = parsedSource.getScriptInfo(hoveredOffset);

    // If the script length is negative, we're not hovering JS source code.
    if (scriptInfo.length == -1) {
      return;
    }

    // Using the script offset, determine the actual line and column inside the
    // script, to use when finding identifiers.
    let scriptStart = editor.getPosition(scriptInfo.start);
    let scriptLineOffset = scriptStart.line;
    let scriptColumnOffset = (hoveredLine == scriptStart.line ? scriptStart.ch : 0);

    let scriptLine = hoveredLine - scriptLineOffset;
    let scriptColumn = hoveredColumn - scriptColumnOffset;
    let identifierInfo = parsedSource.getIdentifierAt({
      line: scriptLine + 1,
      column: scriptColumn,
      scriptIndex: scriptInfo.index,
      ignoreLiterals: this._ignoreLiterals
    });

    // If the info is null, we're not hovering any identifier.
    if (!identifierInfo) {
      return;
    }

    // Transform the line and column relative to the parsed script back
    // to the context of the parent source.
    let { start: identifierStart, end: identifierEnd } = identifierInfo.location;
    let identifierCoords = {
      line: identifierStart.line + scriptLineOffset,
      column: identifierStart.column + scriptColumnOffset,
      length: identifierEnd.column - identifierStart.column
    };

    // Evaluate the identifier in the current stack frame and show the
    // results in a VariablesView inspection popup.
    this.StackFrames.evaluate(identifierInfo.evalString)
      .then(frameFinished => {
        if ("return" in frameFinished) {
          this.showContents({
            coords: identifierCoords,
            evalPrefix: identifierInfo.evalString,
            objectActor: frameFinished.return
          });
        } else {
          let msg = "Evaluation has thrown for: " + identifierInfo.evalString;
          console.warn(msg);
          dumpn(msg);
        }
      })
      .then(null, err => {
        let msg = "Couldn't evaluate: " + err.message;
        console.error(msg);
        dumpn(msg);
      });
  },

  /**
   * Shows an inspection popup for a specified object actor grip.
   *
   * @param string object
   *        An object containing the following properties:
   *          - coords: the inspected identifier coordinates in the editor,
   *                    containing the { line, column, length } properties.
   *          - evalPrefix: a prefix for the variables view evaluation macros.
   *          - objectActor: the value grip for the object actor.
   */
  showContents: function({ coords, evalPrefix, objectActor }) {
    let editor = this.DebuggerView.editor;
    let { line, column, length } = coords;

    // Highlight the function found at the mouse position.
    this._markedText = editor.markText(
      { line: line - 1, ch: column },
      { line: line - 1, ch: column + length });

    // If the grip represents a primitive value, use a more lightweight
    // machinery to display it.
    if (VariablesView.isPrimitive({ value: objectActor })) {
      let className = VariablesView.getClass(objectActor);
      let textContent = VariablesView.getString(objectActor);
      this._tooltip.setTextContent({
        messages: [textContent],
        messagesClass: className,
        containerClass: "plain"
      }, [{
        label: L10N.getStr('addWatchExpressionButton'),
        className: "dbg-expression-button",
        command: () => {
          this.DebuggerView.VariableBubble.hideContents();
          this.DebuggerView.WatchExpressions.addExpression(evalPrefix, true);
        }
      }]);
    } else {
      this._tooltip.setVariableContent(objectActor, {
        searchPlaceholder: L10N.getStr("emptyPropertiesFilterText"),
        searchEnabled: Prefs.variablesSearchboxVisible,
        eval: (variable, value) => {
          let string = variable.evaluationMacro(variable, value);
          this.StackFrames.evaluate(string);
          this.DebuggerView.VariableBubble.hideContents();
        }
      }, {
        getEnvironmentClient: aObject => gThreadClient.environment(aObject),
        getObjectClient: aObject => gThreadClient.pauseGrip(aObject),
        simpleValueEvalMacro: this._getSimpleValueEvalMacro(evalPrefix),
        getterOrSetterEvalMacro: this._getGetterOrSetterEvalMacro(evalPrefix),
        overrideValueEvalMacro: this._getOverrideValueEvalMacro(evalPrefix)
      }, {
        fetched: (aEvent, aType) => {
          if (aType == "properties") {
            window.emit(EVENTS.FETCHED_BUBBLE_PROPERTIES);
          }
        }
      }, [{
        label: L10N.getStr("addWatchExpressionButton"),
        className: "dbg-expression-button",
        command: () => {
          this.DebuggerView.VariableBubble.hideContents();
          this.DebuggerView.WatchExpressions.addExpression(evalPrefix, true);
        }
      }], this._toolbox);
    }

    this._tooltip.show(this._markedText.anchor);
  },

  /**
   * Hides the inspection popup.
   */
  hideContents: function() {
    clearNamedTimeout("editor-mouse-move");
    this._tooltip.hide();
  },

  /**
   * Checks whether the inspection popup is shown.
   *
   * @return boolean
   *         True if the panel is shown or showing, false otherwise.
   */
  contentsShown: function() {
    return this._tooltip.isShown();
  },

  /**
   * Functions for getting customized variables view evaluation macros.
   *
   * @param string aPrefix
   *        See the corresponding VariablesView.* functions.
   */
  _getSimpleValueEvalMacro: function(aPrefix) {
    return (item, string) =>
      VariablesView.simpleValueEvalMacro(item, string, aPrefix);
  },
  _getGetterOrSetterEvalMacro: function(aPrefix) {
    return (item, string) =>
      VariablesView.getterOrSetterEvalMacro(item, string, aPrefix);
  },
  _getOverrideValueEvalMacro: function(aPrefix) {
    return (item, string) =>
      VariablesView.overrideValueEvalMacro(item, string, aPrefix);
  },

  /**
   * The mousemove listener for the source editor.
   */
  _onMouseMove: function(e) {
    // Prevent the variable inspection popup from showing when the thread client
    // is not paused, or while a popup is already visible, or when the user tries
    // to select text in the editor.
    let isResumed = gThreadClient && gThreadClient.state != "paused";
    let isSelecting = this.DebuggerView.editor.somethingSelected() && e.buttons > 0;
    let isPopupVisible = !this._tooltip.isHidden();
    if (isResumed || isSelecting || isPopupVisible) {
      clearNamedTimeout("editor-mouse-move");
      return;
    }
    // Allow events to settle down first. If the mouse hovers over
    // a certain point in the editor long enough, try showing a variable bubble.
    setNamedTimeout("editor-mouse-move",
      this.TOOLTIP_SHOW_DELAY, () => this._findIdentifier(e.clientX, e.clientY));
  },

  /**
   * The mouseout listener for the source editor container node.
   */
  _onMouseOut: function() {
    clearNamedTimeout("editor-mouse-move");
  },

  /**
   * Listener handling the popup hiding event.
   */
  _onPopupHiding: function({ target }) {
    if (this._tooltip.panel != target) {
      return;
    }
    if (this._markedText) {
      this._markedText.clear();
      this._markedText = null;
    }
    if (!this._tooltip.isEmpty()) {
      this._tooltip.empty();
    }
  },

  _editorContainer: null,
  _markedText: null,
  _tooltip: null
};

DebuggerView.VariableBubble = new VariableBubbleView(DebuggerController, DebuggerView);
