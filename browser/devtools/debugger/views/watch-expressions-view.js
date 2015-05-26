/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * Functions handling the watch expressions UI.
 */
function WatchExpressionsView(DebuggerController, DebuggerView) {
  dumpn("WatchExpressionsView was instantiated");

  this.StackFrames = DebuggerController.StackFrames;
  this.DebuggerView = DebuggerView;

  this.switchExpression = this.switchExpression.bind(this);
  this.deleteExpression = this.deleteExpression.bind(this);
  this._createItemView = this._createItemView.bind(this);
  this._onClick = this._onClick.bind(this);
  this._onClose = this._onClose.bind(this);
  this._onBlur = this._onBlur.bind(this);
  this._onKeyPress = this._onKeyPress.bind(this);
}

WatchExpressionsView.prototype = Heritage.extend(WidgetMethods, {
  /**
   * Initialization function, called when the debugger is started.
   */
  initialize: function() {
    dumpn("Initializing the WatchExpressionsView");

    this.widget = new SimpleListWidget(document.getElementById("expressions"));
    this.widget.setAttribute("context", "debuggerWatchExpressionsContextMenu");
    this.widget.addEventListener("click", this._onClick, false);

    this.headerText = L10N.getStr("addWatchExpressionText");
    this._addCommands();
  },

  /**
   * Destruction function, called when the debugger is closed.
   */
  destroy: function() {
    dumpn("Destroying the WatchExpressionsView");

    this.widget.removeEventListener("click", this._onClick, false);
  },

  /**
   * Add commands that XUL can fire.
   */
  _addCommands: function() {
    XULUtils.addCommands(document.getElementById('debuggerCommands'), {
      addWatchExpressionCommand: () => this._onCmdAddExpression(),
      removeAllWatchExpressionsCommand: () => this._onCmdRemoveAllExpressions()
    });
  },

  /**
   * Adds a watch expression in this container.
   *
   * @param string aExpression [optional]
   *        An optional initial watch expression text.
   * @param boolean aSkipUserInput [optional]
   *        Pass true to avoid waiting for additional user input
   *        on the watch expression.
   */
  addExpression: function(aExpression = "", aSkipUserInput = false) {
    // Watch expressions are UI elements which benefit from visible panes.
    this.DebuggerView.showInstrumentsPane();

    // Create the element node for the watch expression item.
    let itemView = this._createItemView(aExpression);

    // Append a watch expression item to this container.
    let expressionItem = this.push([itemView.container], {
      index: 0, /* specifies on which position should the item be appended */
      attachment: {
        view: itemView,
        initialExpression: aExpression,
        currentExpression: "",
      }
    });

    // Automatically focus the new watch expression input
    // if additional user input is desired.
    if (!aSkipUserInput) {
      expressionItem.attachment.view.inputNode.select();
      expressionItem.attachment.view.inputNode.focus();
      this.DebuggerView.Variables.parentNode.scrollTop = 0;
    }
    // Otherwise, add and evaluate the new watch expression immediately.
    else {
      this.toggleContents(false);
      this._onBlur({ target: expressionItem.attachment.view.inputNode });
    }
  },

  /**
   * Changes the watch expression corresponding to the specified variable item.
   * This function is called whenever a watch expression's code is edited in
   * the variables view container.
   *
   * @param Variable aVar
   *        The variable representing the watch expression evaluation.
   * @param string aExpression
   *        The new watch expression text.
   */
  switchExpression: function(aVar, aExpression) {
    let expressionItem =
      [...this].filter(i => i.attachment.currentExpression == aVar.name)[0];

    // Remove the watch expression if it's going to be empty or a duplicate.
    if (!aExpression || this.getAllStrings().indexOf(aExpression) != -1) {
      this.deleteExpression(aVar);
      return;
    }

    // Save the watch expression code string.
    expressionItem.attachment.currentExpression = aExpression;
    expressionItem.attachment.view.inputNode.value = aExpression;

    // Synchronize with the controller's watch expressions store.
    this.StackFrames.syncWatchExpressions();
  },

  /**
   * Removes the watch expression corresponding to the specified variable item.
   * This function is called whenever a watch expression's value is edited in
   * the variables view container.
   *
   * @param Variable aVar
   *        The variable representing the watch expression evaluation.
   */
  deleteExpression: function(aVar) {
    let expressionItem =
      [...this].filter(i => i.attachment.currentExpression == aVar.name)[0];

    // Remove the watch expression.
    this.remove(expressionItem);

    // Synchronize with the controller's watch expressions store.
    this.StackFrames.syncWatchExpressions();
  },

  /**
   * Gets the watch expression code string for an item in this container.
   *
   * @param number aIndex
   *        The index used to identify the watch expression.
   * @return string
   *         The watch expression code string.
   */
  getString: function(aIndex) {
    return this.getItemAtIndex(aIndex).attachment.currentExpression;
  },

  /**
   * Gets the watch expressions code strings for all items in this container.
   *
   * @return array
   *         The watch expressions code strings.
   */
  getAllStrings: function() {
    return this.items.map(e => e.attachment.currentExpression);
  },

  /**
   * Customization function for creating an item's UI.
   *
   * @param string aExpression
   *        The watch expression string.
   */
  _createItemView: function(aExpression) {
    let container = document.createElement("hbox");
    container.className = "list-widget-item dbg-expression";
    container.setAttribute("align", "center");

    let arrowNode = document.createElement("hbox");
    arrowNode.className = "dbg-expression-arrow";

    let inputNode = document.createElement("textbox");
    inputNode.className = "plain dbg-expression-input devtools-monospace";
    inputNode.setAttribute("value", aExpression);
    inputNode.setAttribute("flex", "1");

    let closeNode = document.createElement("toolbarbutton");
    closeNode.className = "plain variables-view-delete";

    closeNode.addEventListener("click", this._onClose, false);
    inputNode.addEventListener("blur", this._onBlur, false);
    inputNode.addEventListener("keypress", this._onKeyPress, false);

    container.appendChild(arrowNode);
    container.appendChild(inputNode);
    container.appendChild(closeNode);

    return {
      container: container,
      arrowNode: arrowNode,
      inputNode: inputNode,
      closeNode: closeNode
    };
  },

  /**
   * Called when the add watch expression key sequence was pressed.
   */
  _onCmdAddExpression: function(aText) {
    // Only add a new expression if there's no pending input.
    if (this.getAllStrings().indexOf("") == -1) {
      this.addExpression(aText || this.DebuggerView.editor.getSelection());
    }
  },

  /**
   * Called when the remove all watch expressions key sequence was pressed.
   */
  _onCmdRemoveAllExpressions: function() {
    // Empty the view of all the watch expressions and clear the cache.
    this.empty();

    // Synchronize with the controller's watch expressions store.
    this.StackFrames.syncWatchExpressions();
  },

  /**
   * The click listener for this container.
   */
  _onClick: function(e) {
    if (e.button != 0) {
      // Only allow left-click to trigger this event.
      return;
    }
    let expressionItem = this.getItemForElement(e.target);
    if (!expressionItem) {
      // The container is empty or we didn't click on an actual item.
      this.addExpression();
    }
  },

  /**
   * The click listener for a watch expression's close button.
   */
  _onClose: function(e) {
    // Remove the watch expression.
    this.remove(this.getItemForElement(e.target));

    // Synchronize with the controller's watch expressions store.
    this.StackFrames.syncWatchExpressions();

    // Prevent clicking the expression element itself.
    e.preventDefault();
    e.stopPropagation();
  },

  /**
   * The blur listener for a watch expression's textbox.
   */
  _onBlur: function({ target: textbox }) {
    let expressionItem = this.getItemForElement(textbox);
    let oldExpression = expressionItem.attachment.currentExpression;
    let newExpression = textbox.value.trim();

    // Remove the watch expression if it's empty.
    if (!newExpression) {
      this.remove(expressionItem);
    }
    // Remove the watch expression if it's a duplicate.
    else if (!oldExpression && this.getAllStrings().indexOf(newExpression) != -1) {
      this.remove(expressionItem);
    }
    // Expression is eligible.
    else {
      expressionItem.attachment.currentExpression = newExpression;
    }

    // Synchronize with the controller's watch expressions store.
    this.StackFrames.syncWatchExpressions();
  },

  /**
   * The keypress listener for a watch expression's textbox.
   */
  _onKeyPress: function(e) {
    switch (e.keyCode) {
      case e.DOM_VK_RETURN:
      case e.DOM_VK_ESCAPE:
        e.stopPropagation();
        this.DebuggerView.editor.focus();
    }
  }
});

DebuggerView.WatchExpressions = new WatchExpressionsView(DebuggerController,
                                                         DebuggerView);
