/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* import-globals-from ../debugger-controller.js */
/* import-globals-from ../debugger-view.js */
/* import-globals-from ../utils.js */
/* globals document, window */
"use strict";


/**
 * Functions handling the filtering UI.
 */
function FilterView(DebuggerController, DebuggerView) {
  dumpn("FilterView was instantiated");

  this.Parser = DebuggerController.Parser;

  this.DebuggerView = DebuggerView;
  this.FilteredSources = new FilteredSourcesView(DebuggerView);
  this.FilteredFunctions = new FilteredFunctionsView(DebuggerController.SourceScripts,
                                                     DebuggerController.Parser,
                                                     DebuggerView);

  this._onClick = this._onClick.bind(this);
  this._onInput = this._onInput.bind(this);
  this._onKeyPress = this._onKeyPress.bind(this);
  this._onBlur = this._onBlur.bind(this);
}

FilterView.prototype = {
  /**
   * Initialization function, called when the debugger is started.
   */
  initialize: function() {
    dumpn("Initializing the FilterView");

    this._searchbox = document.getElementById("searchbox");
    this._searchboxHelpPanel = document.getElementById("searchbox-help-panel");
    this._filterLabel = document.getElementById("filter-label");
    this._globalOperatorButton = document.getElementById("global-operator-button");
    this._globalOperatorLabel = document.getElementById("global-operator-label");
    this._functionOperatorButton = document.getElementById("function-operator-button");
    this._functionOperatorLabel = document.getElementById("function-operator-label");
    this._tokenOperatorButton = document.getElementById("token-operator-button");
    this._tokenOperatorLabel = document.getElementById("token-operator-label");
    this._lineOperatorButton = document.getElementById("line-operator-button");
    this._lineOperatorLabel = document.getElementById("line-operator-label");
    this._variableOperatorButton = document.getElementById("variable-operator-button");
    this._variableOperatorLabel = document.getElementById("variable-operator-label");

    this._fileSearchKey = ShortcutUtils.prettifyShortcut(document.getElementById("fileSearchKey"));
    this._globalSearchKey = ShortcutUtils.prettifyShortcut(document.getElementById("globalSearchKey"));
    this._filteredFunctionsKey = ShortcutUtils.prettifyShortcut(document.getElementById("functionSearchKey"));
    this._tokenSearchKey = ShortcutUtils.prettifyShortcut(document.getElementById("tokenSearchKey"));
    this._lineSearchKey = ShortcutUtils.prettifyShortcut(document.getElementById("lineSearchKey"));
    this._variableSearchKey = ShortcutUtils.prettifyShortcut(document.getElementById("variableSearchKey"));

    this._searchbox.addEventListener("click", this._onClick, false);
    this._searchbox.addEventListener("select", this._onInput, false);
    this._searchbox.addEventListener("input", this._onInput, false);
    this._searchbox.addEventListener("keypress", this._onKeyPress, false);
    this._searchbox.addEventListener("blur", this._onBlur, false);

    let placeholder = L10N.getFormatStr("emptySearchText", this._fileSearchKey);
    this._searchbox.setAttribute("placeholder", placeholder);

    this._globalOperatorButton.setAttribute("label", SEARCH_GLOBAL_FLAG);
    this._functionOperatorButton.setAttribute("label", SEARCH_FUNCTION_FLAG);
    this._tokenOperatorButton.setAttribute("label", SEARCH_TOKEN_FLAG);
    this._lineOperatorButton.setAttribute("label", SEARCH_LINE_FLAG);
    this._variableOperatorButton.setAttribute("label", SEARCH_VARIABLE_FLAG);

    this._filterLabel.setAttribute("value",
      L10N.getFormatStr("searchPanelFilter", this._fileSearchKey));
    this._globalOperatorLabel.setAttribute("value",
      L10N.getFormatStr("searchPanelGlobal", this._globalSearchKey));
    this._functionOperatorLabel.setAttribute("value",
      L10N.getFormatStr("searchPanelFunction", this._filteredFunctionsKey));
    this._tokenOperatorLabel.setAttribute("value",
      L10N.getFormatStr("searchPanelToken", this._tokenSearchKey));
    this._lineOperatorLabel.setAttribute("value",
      L10N.getFormatStr("searchPanelGoToLine", this._lineSearchKey));
    this._variableOperatorLabel.setAttribute("value",
      L10N.getFormatStr("searchPanelVariable", this._variableSearchKey));

    this.FilteredSources.initialize();
    this.FilteredFunctions.initialize();

    this._addCommands();
  },

  /**
   * Destruction function, called when the debugger is closed.
   */
  destroy: function() {
    dumpn("Destroying the FilterView");

    this._searchbox.removeEventListener("click", this._onClick, false);
    this._searchbox.removeEventListener("select", this._onInput, false);
    this._searchbox.removeEventListener("input", this._onInput, false);
    this._searchbox.removeEventListener("keypress", this._onKeyPress, false);
    this._searchbox.removeEventListener("blur", this._onBlur, false);

    this.FilteredSources.destroy();
    this.FilteredFunctions.destroy();
  },

  /**
   * Add commands that XUL can fire.
   */
  _addCommands: function() {
    XULUtils.addCommands(document.getElementById('debuggerCommands'), {
      fileSearchCommand: () => this._doFileSearch(),
      globalSearchCommand: () => this._doGlobalSearch(),
      functionSearchCommand: () => this._doFunctionSearch(),
      tokenSearchCommand: () => this._doTokenSearch(),
      lineSearchCommand: () => this._doLineSearch(),
      variableSearchCommand: () => this._doVariableSearch(),
      variablesFocusCommand: () => this._doVariablesFocus()
    });
  },

  /**
   * Gets the entered operator and arguments in the searchbox.
   * @return array
   */
  get searchData() {
    let operator = "", args = [];

    let rawValue = this._searchbox.value;
    let rawLength = rawValue.length;
    let globalFlagIndex = rawValue.indexOf(SEARCH_GLOBAL_FLAG);
    let functionFlagIndex = rawValue.indexOf(SEARCH_FUNCTION_FLAG);
    let variableFlagIndex = rawValue.indexOf(SEARCH_VARIABLE_FLAG);
    let tokenFlagIndex = rawValue.lastIndexOf(SEARCH_TOKEN_FLAG);
    let lineFlagIndex = rawValue.lastIndexOf(SEARCH_LINE_FLAG);

    // This is not a global, function or variable search, allow file/line flags.
    if (globalFlagIndex != 0 && functionFlagIndex != 0 && variableFlagIndex != 0) {
      // Token search has precedence over line search.
      if (tokenFlagIndex != -1) {
        operator = SEARCH_TOKEN_FLAG;
        args.push(rawValue.slice(0, tokenFlagIndex)); // file
        args.push(rawValue.substr(tokenFlagIndex + 1, rawLength)); // token
      } else if (lineFlagIndex != -1) {
        operator = SEARCH_LINE_FLAG;
        args.push(rawValue.slice(0, lineFlagIndex)); // file
        args.push(+rawValue.substr(lineFlagIndex + 1, rawLength) || 0); // line
      } else {
        args.push(rawValue);
      }
    }
    // Global searches dissalow the use of file or line flags.
    else if (globalFlagIndex == 0) {
      operator = SEARCH_GLOBAL_FLAG;
      args.push(rawValue.slice(1));
    }
    // Function searches dissalow the use of file or line flags.
    else if (functionFlagIndex == 0) {
      operator = SEARCH_FUNCTION_FLAG;
      args.push(rawValue.slice(1));
    }
    // Variable searches dissalow the use of file or line flags.
    else if (variableFlagIndex == 0) {
      operator = SEARCH_VARIABLE_FLAG;
      args.push(rawValue.slice(1));
    }

    return [operator, args];
  },

  /**
   * Returns the current search operator.
   * @return string
   */
  get searchOperator() {
    return this.searchData[0];
  },

  /**
   * Returns the current search arguments.
   * @return array
   */
  get searchArguments() {
    return this.searchData[1];
  },

  /**
   * Clears the text from the searchbox and any changed views.
   */
  clearSearch: function() {
    this._searchbox.value = "";
    this.clearViews();

    this.FilteredSources.clearView();
    this.FilteredFunctions.clearView();
  },

  /**
   * Clears all the views that may pop up when searching.
   */
  clearViews: function() {
    this.DebuggerView.GlobalSearch.clearView();
    this.FilteredSources.clearView();
    this.FilteredFunctions.clearView();
    this._searchboxHelpPanel.hidePopup();
  },

  /**
   * Performs a line search if necessary.
   * (Jump to lines in the currently visible source).
   *
   * @param number aLine
   *        The source line number to jump to.
   */
  _performLineSearch: function(aLine) {
    // Make sure we're actually searching for a valid line.
    if (aLine) {
      this.DebuggerView.editor.setCursor({ line: aLine - 1, ch: 0 }, "center");
    }
  },

  /**
   * Performs a token search if necessary.
   * (Search for tokens in the currently visible source).
   *
   * @param string aToken
   *        The source token to find.
   */
  _performTokenSearch: function(aToken) {
    // Make sure we're actually searching for a valid token.
    if (!aToken) {
      return;
    }
    this.DebuggerView.editor.find(aToken);
  },

  /**
   * The click listener for the search container.
   */
  _onClick: function() {
    // If there's some text in the searchbox, displaying a panel would
    // interfere with double/triple click default behaviors.
    if (!this._searchbox.value) {
      this._searchboxHelpPanel.openPopup(this._searchbox);
    }
  },

  /**
   * The input listener for the search container.
   */
  _onInput: function() {
    this.clearViews();

    // Make sure we're actually searching for something.
    if (!this._searchbox.value) {
      return;
    }

    // Perform the required search based on the specified operator.
    switch (this.searchOperator) {
      case SEARCH_GLOBAL_FLAG:
        // Schedule a global search for when the user stops typing.
        this.DebuggerView.GlobalSearch.scheduleSearch(this.searchArguments[0]);
        break;
      case SEARCH_FUNCTION_FLAG:
      // Schedule a function search for when the user stops typing.
        this.FilteredFunctions.scheduleSearch(this.searchArguments[0]);
        break;
      case SEARCH_VARIABLE_FLAG:
        // Schedule a variable search for when the user stops typing.
        this.DebuggerView.Variables.scheduleSearch(this.searchArguments[0]);
        break;
      case SEARCH_TOKEN_FLAG:
        // Schedule a file+token search for when the user stops typing.
        this.FilteredSources.scheduleSearch(this.searchArguments[0]);
        this._performTokenSearch(this.searchArguments[1]);
        break;
      case SEARCH_LINE_FLAG:
        // Schedule a file+line search for when the user stops typing.
        this.FilteredSources.scheduleSearch(this.searchArguments[0]);
        this._performLineSearch(this.searchArguments[1]);
        break;
      default:
        // Schedule a file only search for when the user stops typing.
        this.FilteredSources.scheduleSearch(this.searchArguments[0]);
        break;
    }
  },

  /**
   * The key press listener for the search container.
   */
  _onKeyPress: function(e) {
    // This attribute is not implemented in Gecko at this time, see bug 680830.
    e.char = String.fromCharCode(e.charCode);

    // Perform the required action based on the specified operator.
    let [operator, args] = this.searchData;
    let isGlobalSearch = operator == SEARCH_GLOBAL_FLAG;
    let isFunctionSearch = operator == SEARCH_FUNCTION_FLAG;
    let isVariableSearch = operator == SEARCH_VARIABLE_FLAG;
    let isTokenSearch = operator == SEARCH_TOKEN_FLAG;
    let isLineSearch = operator == SEARCH_LINE_FLAG;
    let isFileOnlySearch = !operator && args.length == 1;

    // Depending on the pressed keys, determine to correct action to perform.
    let actionToPerform;

    // Meta+G and Ctrl+N focus next matches.
    if ((e.char == "g" && e.metaKey) || e.char == "n" && e.ctrlKey) {
      actionToPerform = "selectNext";
    }
    // Meta+Shift+G and Ctrl+P focus previous matches.
    else if ((e.char == "G" && e.metaKey) || e.char == "p" && e.ctrlKey) {
      actionToPerform = "selectPrev";
    }
    // Return, enter, down and up keys focus next or previous matches, while
    // the escape key switches focus from the search container.
    else switch (e.keyCode) {
      case e.DOM_VK_RETURN:
        var isReturnKey = true;
        // If the shift key is pressed, focus on the previous result
        actionToPerform = e.shiftKey ? "selectPrev" : "selectNext";
        break;
      case e.DOM_VK_DOWN:
        actionToPerform = "selectNext";
        break;
      case e.DOM_VK_UP:
        actionToPerform = "selectPrev";
        break;
    }

    // If there's no action to perform, or no operator, file line or token
    // were specified, then this is either a broken or empty search.
    if (!actionToPerform || (!operator && !args.length)) {
      this.DebuggerView.editor.dropSelection();
      return;
    }

    e.preventDefault();
    e.stopPropagation();

    // Jump to the next/previous entry in the global search, or perform
    // a new global search immediately
    if (isGlobalSearch) {
      let targetView = this.DebuggerView.GlobalSearch;
      if (!isReturnKey) {
        targetView[actionToPerform]();
      } else if (targetView.hidden) {
        targetView.scheduleSearch(args[0], 0);
      }
      return;
    }

    // Jump to the next/previous entry in the function search, perform
    // a new function search immediately, or clear it.
    if (isFunctionSearch) {
      let targetView = this.FilteredFunctions;
      if (!isReturnKey) {
        targetView[actionToPerform]();
      } else if (targetView.hidden) {
        targetView.scheduleSearch(args[0], 0);
      } else {
        if (!targetView.selectedItem) {
          targetView.selectedIndex = 0;
        }
        this.clearSearch();
      }
      return;
    }

    // Perform a new variable search immediately.
    if (isVariableSearch) {
      let targetView = this.DebuggerView.Variables;
      if (isReturnKey) {
        targetView.scheduleSearch(args[0], 0);
      }
      return;
    }

    // Jump to the next/previous entry in the file search, perform
    // a new file search immediately, or clear it.
    if (isFileOnlySearch) {
      let targetView = this.FilteredSources;
      if (!isReturnKey) {
        targetView[actionToPerform]();
      } else if (targetView.hidden) {
        targetView.scheduleSearch(args[0], 0);
      } else {
        if (!targetView.selectedItem) {
          targetView.selectedIndex = 0;
        }
        this.clearSearch();
      }
      return;
    }

    // Jump to the next/previous instance of the currently searched token.
    if (isTokenSearch) {
      let methods = { selectNext: "findNext", selectPrev: "findPrev" };
      this.DebuggerView.editor[methods[actionToPerform]]();
      return;
    }

    // Increment/decrement the currently searched caret line.
    if (isLineSearch) {
      let [, line] = args;
      let amounts = { selectNext: 1, selectPrev: -1 };

      // Modify the line number and jump to it.
      line += !isReturnKey ? amounts[actionToPerform] : 0;
      let lineCount = this.DebuggerView.editor.lineCount();
      let lineTarget = line < 1 ? 1 : line > lineCount ? lineCount : line;
      this._doSearch(SEARCH_LINE_FLAG, lineTarget);
      return;
    }
  },

  /**
   * The blur listener for the search container.
   */
  _onBlur: function() {
    this.clearViews();
  },

  /**
   * Called when a filtering key sequence was pressed.
   *
   * @param string aOperator
   *        The operator to use for filtering.
   */
  _doSearch: function(aOperator = "", aText = "") {
    this._searchbox.focus();
    this._searchbox.value = ""; // Need to clear value beforehand. Bug 779738.

    if (aText) {
      this._searchbox.value = aOperator + aText;
      return;
    }
    if (this.DebuggerView.editor.somethingSelected()) {
      this._searchbox.value = aOperator + this.DebuggerView.editor.getSelection();
      return;
    }
    if (SEARCH_AUTOFILL.indexOf(aOperator) != -1) {
      let cursor = this.DebuggerView.editor.getCursor();
      let content = this.DebuggerView.editor.getText();
      let location = this.DebuggerView.Sources.selectedItem.attachment.source.url;
      let source = this.Parser.get(content, location);
      let identifier = source.getIdentifierAt({ line: cursor.line+1, column: cursor.ch });

      if (identifier && identifier.name) {
        this._searchbox.value = aOperator + identifier.name;
        this._searchbox.select();
        this._searchbox.selectionStart += aOperator.length;
        return;
      }
    }
    this._searchbox.value = aOperator;
  },

  /**
   * Called when the source location filter key sequence was pressed.
   */
  _doFileSearch: function() {
    this._doSearch();
    this._searchboxHelpPanel.openPopup(this._searchbox);
  },

  /**
   * Called when the global search filter key sequence was pressed.
   */
  _doGlobalSearch: function() {
    this._doSearch(SEARCH_GLOBAL_FLAG);
    this._searchboxHelpPanel.hidePopup();
  },

  /**
   * Called when the source function filter key sequence was pressed.
   */
  _doFunctionSearch: function() {
    this._doSearch(SEARCH_FUNCTION_FLAG);
    this._searchboxHelpPanel.hidePopup();
  },

  /**
   * Called when the source token filter key sequence was pressed.
   */
  _doTokenSearch: function() {
    this._doSearch(SEARCH_TOKEN_FLAG);
    this._searchboxHelpPanel.hidePopup();
  },

  /**
   * Called when the source line filter key sequence was pressed.
   */
  _doLineSearch: function() {
    this._doSearch(SEARCH_LINE_FLAG);
    this._searchboxHelpPanel.hidePopup();
  },

  /**
   * Called when the variable search filter key sequence was pressed.
   */
  _doVariableSearch: function() {
    this._doSearch(SEARCH_VARIABLE_FLAG);
    this._searchboxHelpPanel.hidePopup();
  },

  /**
   * Called when the variables focus key sequence was pressed.
   */
  _doVariablesFocus: function() {
    this.DebuggerView.showInstrumentsPane();
    this.DebuggerView.Variables.focusFirstVisibleItem();
  },

  _searchbox: null,
  _searchboxHelpPanel: null,
  _globalOperatorButton: null,
  _globalOperatorLabel: null,
  _functionOperatorButton: null,
  _functionOperatorLabel: null,
  _tokenOperatorButton: null,
  _tokenOperatorLabel: null,
  _lineOperatorButton: null,
  _lineOperatorLabel: null,
  _variableOperatorButton: null,
  _variableOperatorLabel: null,
  _fileSearchKey: "",
  _globalSearchKey: "",
  _filteredFunctionsKey: "",
  _tokenSearchKey: "",
  _lineSearchKey: "",
  _variableSearchKey: "",
};

/**
 * Functions handling the filtered sources UI.
 */
function FilteredSourcesView(DebuggerView) {
  dumpn("FilteredSourcesView was instantiated");

  this.DebuggerView = DebuggerView;

  this._onClick = this._onClick.bind(this);
  this._onSelect = this._onSelect.bind(this);
}

FilteredSourcesView.prototype = Heritage.extend(ResultsPanelContainer.prototype, {
  /**
   * Initialization function, called when the debugger is started.
   */
  initialize: function() {
    dumpn("Initializing the FilteredSourcesView");

    this.anchor = document.getElementById("searchbox");
    this.widget.addEventListener("select", this._onSelect, false);
    this.widget.addEventListener("click", this._onClick, false);
  },

  /**
   * Destruction function, called when the debugger is closed.
   */
  destroy: function() {
    dumpn("Destroying the FilteredSourcesView");

    this.widget.removeEventListener("select", this._onSelect, false);
    this.widget.removeEventListener("click", this._onClick, false);
    this.anchor = null;
  },

  /**
   * Schedules searching for a source.
   *
   * @param string aToken
   *        The function to search for.
   * @param number aWait
   *        The amount of milliseconds to wait until draining.
   */
  scheduleSearch: function(aToken, aWait) {
    // The amount of time to wait for the requests to settle.
    let maxDelay = FILE_SEARCH_ACTION_MAX_DELAY;
    let delay = aWait === undefined ? maxDelay / aToken.length : aWait;

    // Allow requests to settle down first.
    setNamedTimeout("sources-search", delay, () => this._doSearch(aToken));
  },

  /**
   * Finds file matches in all the displayed sources.
   *
   * @param string aToken
   *        The string to search for.
   */
  _doSearch: function(aToken, aStore = []) {
    // Don't continue filtering if the searched token is an empty string.
    // In contrast with function searching, in this case we don't want to
    // show a list of all the files when no search token was supplied.
    if (!aToken) {
      return;
    }

    for (let item of this.DebuggerView.Sources.items) {
      let lowerCaseLabel = item.attachment.label.toLowerCase();
      let lowerCaseToken = aToken.toLowerCase();
      if (lowerCaseLabel.match(lowerCaseToken)) {
        aStore.push(item);
      }

      // Once the maximum allowed number of results is reached, proceed
      // with building the UI immediately.
      if (aStore.length >= RESULTS_PANEL_MAX_RESULTS) {
        this._syncView(aStore);
        return;
      }
    }

    // Couldn't reach the maximum allowed number of results, but that's ok,
    // continue building the UI.
    this._syncView(aStore);
  },

  /**
   * Updates the list of sources displayed in this container.
   *
   * @param array aSearchResults
   *        The results array, containing search details for each source.
   */
  _syncView: function(aSearchResults) {
    // If there are no matches found, keep the popup hidden and avoid
    // creating the view.
    if (!aSearchResults.length) {
      window.emit(EVENTS.FILE_SEARCH_MATCH_NOT_FOUND);
      return;
    }

    for (let item of aSearchResults) {
      let url = item.attachment.source.url;

      if (url) {
        // Create the element node for the location item.
        let itemView = this._createItemView(
          SourceUtils.trimUrlLength(item.attachment.label),
          SourceUtils.trimUrlLength(url, 0, "start")
        );

        // Append a location item to this container for each match.
        this.push([itemView], {
          index: -1, /* specifies on which position should the item be appended */
          attachment: {
            url: url
          }
        });
      }
    }

    // There's at least one item displayed in this container. Don't select it
    // automatically if not forced (by tests) or in tandem with an
    // operator.
    if (this._autoSelectFirstItem || this.DebuggerView.Filtering.searchOperator) {
      this.selectedIndex = 0;
    }
    this.hidden = false;

    // Signal that file search matches were found and displayed.
    window.emit(EVENTS.FILE_SEARCH_MATCH_FOUND);
  },

  /**
   * The click listener for this container.
   */
  _onClick: function(e) {
    let locationItem = this.getItemForElement(e.target);
    if (locationItem) {
      this.selectedItem = locationItem;
      this.DebuggerView.Filtering.clearSearch();
    }
  },

  /**
   * The select listener for this container.
   *
   * @param object aItem
   *        The item associated with the element to select.
   */
  _onSelect: function({ detail: locationItem }) {
    if (locationItem) {
      let source = queries.getSourceByURL(DebuggerController.getState(),
                                          locationItem.attachment.url);
      this.DebuggerView.setEditorLocation(source.actor, undefined, {
        noCaret: true,
        noDebug: true
      });
    }
  }
});

/**
 * Functions handling the function search UI.
 */
function FilteredFunctionsView(SourceScripts, Parser, DebuggerView) {
  dumpn("FilteredFunctionsView was instantiated");

  this.SourceScripts = SourceScripts;
  this.Parser = Parser;
  this.DebuggerView = DebuggerView;

  this._onClick = this._onClick.bind(this);
  this._onSelect = this._onSelect.bind(this);
}

FilteredFunctionsView.prototype = Heritage.extend(ResultsPanelContainer.prototype, {
  /**
   * Initialization function, called when the debugger is started.
   */
  initialize: function() {
    dumpn("Initializing the FilteredFunctionsView");

    this.anchor = document.getElementById("searchbox");
    this.widget.addEventListener("select", this._onSelect, false);
    this.widget.addEventListener("click", this._onClick, false);
  },

  /**
   * Destruction function, called when the debugger is closed.
   */
  destroy: function() {
    dumpn("Destroying the FilteredFunctionsView");

    this.widget.removeEventListener("select", this._onSelect, false);
    this.widget.removeEventListener("click", this._onClick, false);
    this.anchor = null;
  },

  /**
   * Schedules searching for a function in all of the sources.
   *
   * @param string aToken
   *        The function to search for.
   * @param number aWait
   *        The amount of milliseconds to wait until draining.
   */
  scheduleSearch: function(aToken, aWait) {
    // The amount of time to wait for the requests to settle.
    let maxDelay = FUNCTION_SEARCH_ACTION_MAX_DELAY;
    let delay = aWait === undefined ? maxDelay / aToken.length : aWait;

    // Allow requests to settle down first.
    setNamedTimeout("function-search", delay, () => {
      // Start fetching as many sources as possible, then perform the search.
      let actors = this.DebuggerView.Sources.values;
      let sourcesFetched = DebuggerController.dispatch(actions.getTextForSources(actors));
      sourcesFetched.then(aSources => this._doSearch(aToken, aSources));
    });
  },

  /**
   * Finds function matches in all the sources stored in the cache, and groups
   * them by location and line number.
   *
   * @param string aToken
   *        The string to search for.
   * @param array aSources
   *        An array of [url, text] tuples for each source.
   */
  _doSearch: function(aToken, aSources, aStore = []) {
    // Continue parsing even if the searched token is an empty string, to
    // cache the syntax tree nodes generated by the reflection API.

    // Make sure the currently displayed source is parsed first. Once the
    // maximum allowed number of results are found, parsing will be halted.
    let currentActor = this.DebuggerView.Sources.selectedValue;
    let currentSource = aSources.filter(([actor]) => actor == currentActor)[0];
    aSources.splice(aSources.indexOf(currentSource), 1);
    aSources.unshift(currentSource);

    // If not searching for a specific function, only parse the displayed source,
    // which is now the first item in the sources array.
    if (!aToken) {
      aSources.splice(1);
    }

    for (let [actor, contents] of aSources) {
      let item = this.DebuggerView.Sources.getItemByValue(actor);
      let url = item.attachment.source.url;
      if (!url) {
        continue;
      }

      let parsedSource = this.Parser.get(contents, url);
      let sourceResults = parsedSource.getNamedFunctionDefinitions(aToken);

      for (let scriptResult of sourceResults) {
        for (let parseResult of scriptResult) {
          aStore.push({
            sourceUrl: scriptResult.sourceUrl,
            scriptOffset: scriptResult.scriptOffset,
            functionName: parseResult.functionName,
            functionLocation: parseResult.functionLocation,
            inferredName: parseResult.inferredName,
            inferredChain: parseResult.inferredChain,
            inferredLocation: parseResult.inferredLocation
          });

          // Once the maximum allowed number of results is reached, proceed
          // with building the UI immediately.
          if (aStore.length >= RESULTS_PANEL_MAX_RESULTS) {
            this._syncView(aStore);
            return;
          }
        }
      }
    }

    // Couldn't reach the maximum allowed number of results, but that's ok,
    // continue building the UI.
    this._syncView(aStore);
  },

  /**
   * Updates the list of functions displayed in this container.
   *
   * @param array aSearchResults
   *        The results array, containing search details for each source.
   */
  _syncView: function(aSearchResults) {
    // If there are no matches found, keep the popup hidden and avoid
    // creating the view.
    if (!aSearchResults.length) {
      window.emit(EVENTS.FUNCTION_SEARCH_MATCH_NOT_FOUND);
      return;
    }

    for (let item of aSearchResults) {
      // Some function expressions don't necessarily have a name, but the
      // parser provides us with an inferred name from an enclosing
      // VariableDeclarator, AssignmentExpression, ObjectExpression node.
      if (item.functionName && item.inferredName &&
          item.functionName != item.inferredName) {
        let s = " " + L10N.getStr("functionSearchSeparatorLabel") + " ";
        item.displayedName = item.inferredName + s + item.functionName;
      }
      // The function doesn't have an explicit name, but it could be inferred.
      else if (item.inferredName) {
        item.displayedName = item.inferredName;
      }
      // The function only has an explicit name.
      else {
        item.displayedName = item.functionName;
      }

      // Some function expressions have unexpected bounds, since they may not
      // necessarily have an associated name defining them.
      if (item.inferredLocation) {
        item.actualLocation = item.inferredLocation;
      } else {
        item.actualLocation = item.functionLocation;
      }

      // Create the element node for the function item.
      let itemView = this._createItemView(
        SourceUtils.trimUrlLength(item.displayedName + "()"),
        SourceUtils.trimUrlLength(item.sourceUrl, 0, "start"),
        (item.inferredChain || []).join(".")
      );

      // Append a function item to this container for each match.
      this.push([itemView], {
        index: -1, /* specifies on which position should the item be appended */
        attachment: item
      });
    }

    // There's at least one item displayed in this container. Don't select it
    // automatically if not forced (by tests).
    if (this._autoSelectFirstItem) {
      this.selectedIndex = 0;
    }
    this.hidden = false;

    // Signal that function search matches were found and displayed.
    window.emit(EVENTS.FUNCTION_SEARCH_MATCH_FOUND);
  },

  /**
   * The click listener for this container.
   */
  _onClick: function(e) {
    let functionItem = this.getItemForElement(e.target);
    if (functionItem) {
      this.selectedItem = functionItem;
      this.DebuggerView.Filtering.clearSearch();
    }
  },

  /**
   * The select listener for this container.
   */
  _onSelect: function({ detail: functionItem }) {
    if (functionItem) {
      let sourceUrl = functionItem.attachment.sourceUrl;
      let actor = queries.getSourceByURL(DebuggerController.getState(), sourceUrl).actor;
      let scriptOffset = functionItem.attachment.scriptOffset;
      let actualLocation = functionItem.attachment.actualLocation;

      this.DebuggerView.setEditorLocation(actor, actualLocation.start.line, {
        charOffset: scriptOffset,
        columnOffset: actualLocation.start.column,
        align: "center",
        noDebug: true
      });
    }
  },

  _searchTimeout: null,
  _searchFunction: null,
  _searchedToken: ""
});

DebuggerView.Filtering = new FilterView(DebuggerController, DebuggerView);
