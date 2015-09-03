/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * Functions handling the toolbar view: close button, expand/collapse button,
 * pause/resume and stepping buttons etc.
 */
function ToolbarView(DebuggerController, DebuggerView) {
  dumpn("ToolbarView was instantiated");

  this.StackFrames = DebuggerController.StackFrames;
  this.ThreadState = DebuggerController.ThreadState;
  this.DebuggerController = DebuggerController;
  this.DebuggerView = DebuggerView;

  this._onTogglePanesPressed = this._onTogglePanesPressed.bind(this);
  this._onResumePressed = this._onResumePressed.bind(this);
  this._onStepOverPressed = this._onStepOverPressed.bind(this);
  this._onStepInPressed = this._onStepInPressed.bind(this);
  this._onStepOutPressed = this._onStepOutPressed.bind(this);
}

ToolbarView.prototype = {
  get activeThread() {
    return this.DebuggerController.activeThread;
  },

  get resumptionWarnFunc() {
    return this.DebuggerController._ensureResumptionOrder;
  },

  /**
   * Initialization function, called when the debugger is started.
   */
  initialize: function() {
    dumpn("Initializing the ToolbarView");

    this._instrumentsPaneToggleButton = document.getElementById("instruments-pane-toggle");
    this._resumeButton = document.getElementById("resume");
    this._stepOverButton = document.getElementById("step-over");
    this._stepInButton = document.getElementById("step-in");
    this._stepOutButton = document.getElementById("step-out");
    this._resumeOrderTooltip = new Tooltip(document);
    this._resumeOrderTooltip.defaultPosition = TOOLBAR_ORDER_POPUP_POSITION;

    let resumeKey = ShortcutUtils.prettifyShortcut(document.getElementById("resumeKey"));
    let stepOverKey = ShortcutUtils.prettifyShortcut(document.getElementById("stepOverKey"));
    let stepInKey = ShortcutUtils.prettifyShortcut(document.getElementById("stepInKey"));
    let stepOutKey = ShortcutUtils.prettifyShortcut(document.getElementById("stepOutKey"));
    this._resumeTooltip = L10N.getFormatStr("resumeButtonTooltip", resumeKey);
    this._pauseTooltip = L10N.getFormatStr("pauseButtonTooltip", resumeKey);
    this._pausePendingTooltip = L10N.getStr("pausePendingButtonTooltip");
    this._stepOverTooltip = L10N.getFormatStr("stepOverTooltip", stepOverKey);
    this._stepInTooltip = L10N.getFormatStr("stepInTooltip", stepInKey);
    this._stepOutTooltip = L10N.getFormatStr("stepOutTooltip", stepOutKey);

    this._instrumentsPaneToggleButton.addEventListener("mousedown", this._onTogglePanesPressed, false);
    this._resumeButton.addEventListener("mousedown", this._onResumePressed, false);
    this._stepOverButton.addEventListener("mousedown", this._onStepOverPressed, false);
    this._stepInButton.addEventListener("mousedown", this._onStepInPressed, false);
    this._stepOutButton.addEventListener("mousedown", this._onStepOutPressed, false);

    this._stepOverButton.setAttribute("tooltiptext", this._stepOverTooltip);
    this._stepInButton.setAttribute("tooltiptext", this._stepInTooltip);
    this._stepOutButton.setAttribute("tooltiptext", this._stepOutTooltip);
    this._toggleButtonsState({ enabled: false });

    this._addCommands();
  },

  /**
   * Destruction function, called when the debugger is closed.
   */
  destroy: function() {
    dumpn("Destroying the ToolbarView");

    this._instrumentsPaneToggleButton.removeEventListener("mousedown", this._onTogglePanesPressed, false);
    this._resumeButton.removeEventListener("mousedown", this._onResumePressed, false);
    this._stepOverButton.removeEventListener("mousedown", this._onStepOverPressed, false);
    this._stepInButton.removeEventListener("mousedown", this._onStepInPressed, false);
    this._stepOutButton.removeEventListener("mousedown", this._onStepOutPressed, false);
  },

  /**
   * Add commands that XUL can fire.
   */
  _addCommands: function() {
    XULUtils.addCommands(document.getElementById('debuggerCommands'), {
      resumeCommand: () => this._onResumePressed(),
      stepOverCommand: () => this._onStepOverPressed(),
      stepInCommand: () => this._onStepInPressed(),
      stepOutCommand: () => this._onStepOutPressed()
    });
  },

  /**
   * Display a warning when trying to resume a debuggee while another is paused.
   * Debuggees must be unpaused in a Last-In-First-Out order.
   *
   * @param string aPausedUrl
   *        The URL of the last paused debuggee.
   */
  showResumeWarning: function(aPausedUrl) {
    let label = L10N.getFormatStr("resumptionOrderPanelTitle", aPausedUrl);
    let defaultStyle = "default-tooltip-simple-text-colors";
    this._resumeOrderTooltip.setTextContent({ messages: [label], isAlertTooltip: true });
    this._resumeOrderTooltip.show(this._resumeButton);
  },

  /**
   * Sets the resume button state based on the debugger active thread.
   *
   * @param string aState
   *        Either "paused", "attached", or "breakOnNext".
   * @param boolean hasLocation
   *        True if we are paused at a specific JS location
   */
  toggleResumeButtonState: function(aState, hasLocation) {
    // Intermidiate state after pressing the pause button and waiting
    // for the next script execution to happen.
    if (aState == "breakOnNext") {
      this._resumeButton.setAttribute("break-on-next", "true");
      this._resumeButton.disabled = true;
      this._resumeButton.setAttribute("tooltiptext", this._pausePendingTooltip);
      return;
    }

    this._resumeButton.removeAttribute("break-on-next");
    this._resumeButton.disabled = false;

    // If we're paused, check and show a resume label on the button.
    if (aState == "paused") {
      this._resumeButton.setAttribute("checked", "true");
      this._resumeButton.setAttribute("tooltiptext", this._resumeTooltip);

      // Only enable the stepping buttons if we are paused at a
      // specific location. After bug 789430, we'll always be paused
      // at a location, but currently you can pause the entire engine
      // at any point without knowing the location.
      if (hasLocation) {
        this._toggleButtonsState({ enabled: true });
      }
    }
    // If we're attached, do the opposite.
    else if (aState == "attached") {
      this._resumeButton.removeAttribute("checked");
      this._resumeButton.setAttribute("tooltiptext", this._pauseTooltip);
      this._toggleButtonsState({ enabled: false });
    }
  },

  _toggleButtonsState: function({ enabled }) {
    const buttons = [
      this._stepOutButton,
      this._stepInButton,
      this._stepOverButton
    ];
    for (let button of buttons) {
      button.disabled = !enabled;
    }
  },

  /**
   * Listener handling the toggle button click event.
   */
  _onTogglePanesPressed: function() {
    DebuggerView.toggleInstrumentsPane({
      visible: DebuggerView.instrumentsPaneHidden,
      animated: true,
      delayed: true
    });
  },

  /**
   * Listener handling the pause/resume button click event.
   */
  _onResumePressed: function() {
    if (this.StackFrames._currentFrameDescription != FRAME_TYPE.NORMAL ||
        this._resumeButton.disabled) {
      return;
    }

    if (this.activeThread.paused) {
      this.StackFrames.currentFrameDepth = -1;
      this.activeThread.resume(this.resumptionWarnFunc);
    } else {
      this.ThreadState.interruptedByResumeButton = true;
      this.toggleResumeButtonState("breakOnNext");
      this.activeThread.breakOnNext();
    }
  },

  /**
   * Listener handling the step over button click event.
   */
  _onStepOverPressed: function() {
    if (this.activeThread.paused && !this._stepOverButton.disabled) {
      this.StackFrames.currentFrameDepth = -1;
      this.activeThread.stepOver(this.resumptionWarnFunc);
    }
  },

  /**
   * Listener handling the step in button click event.
   */
  _onStepInPressed: function() {
    if (this.StackFrames._currentFrameDescription != FRAME_TYPE.NORMAL ||
       this._stepInButton.disabled) {
      return;
    }

    if (this.activeThread.paused) {
      this.StackFrames.currentFrameDepth = -1;
      this.activeThread.stepIn(this.resumptionWarnFunc);
    }
  },

  /**
   * Listener handling the step out button click event.
   */
  _onStepOutPressed: function() {
    if (this.activeThread.paused && !this._stepOutButton.disabled) {
      this.StackFrames.currentFrameDepth = -1;
      this.activeThread.stepOut(this.resumptionWarnFunc);
    }
  },

  _instrumentsPaneToggleButton: null,
  _resumeButton: null,
  _stepOverButton: null,
  _stepInButton: null,
  _stepOutButton: null,
  _resumeOrderTooltip: null,
  _resumeTooltip: "",
  _pauseTooltip: "",
  _stepOverTooltip: "",
  _stepInTooltip: "",
  _stepOutTooltip: ""
};

DebuggerView.Toolbar = new ToolbarView(DebuggerController, DebuggerView);
