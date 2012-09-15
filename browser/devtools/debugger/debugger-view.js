/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const BREAKPOINT_LINE_TOOLTIP_MAX_SIZE = 1000; // chars
const PROPERTY_VIEW_FLASH_DURATION = 400; // ms
const GLOBAL_SEARCH_MATCH_FLASH_DURATION = 100; // ms
const GLOBAL_SEARCH_URL_MAX_SIZE = 100; // chars
const GLOBAL_SEARCH_LINE_MAX_SIZE = 300; // chars
const GLOBAL_SEARCH_ACTION_DELAY = 150; // ms

const SEARCH_GLOBAL_FLAG = "!";
const SEARCH_LINE_FLAG = ":";
const SEARCH_TOKEN_FLAG = "#";

/**
 * Object mediating visual changes and event listeners between the debugger and
 * the html view.
 */
let DebuggerView = {

  /**
   * An instance of SourceEditor.
   */
  editor: null,

  /**
   * Caches frequently used global view elements.
   */
  cacheView: function DV_cacheView() {
    this._onTogglePanesButtonPressed = this._onTogglePanesButtonPressed.bind(this);

    // Panes and view containers
    this._togglePanesButton = document.getElementById("toggle-panes");
    this._stackframesAndBreakpoints = document.getElementById("stackframes+breakpoints");
    this._stackframes = document.getElementById("stackframes");
    this._breakpoints = document.getElementById("breakpoints");
    this._variables = document.getElementById("variables");
    this._scripts = document.getElementById("scripts");
    this._globalSearch = document.getElementById("globalsearch");
    this._globalSearchSplitter = document.getElementById("globalsearch-splitter");

    // Keys
    this._fileSearchKey = document.getElementById("fileSearchKey");
    this._lineSearchKey = document.getElementById("lineSearchKey");
    this._tokenSearchKey = document.getElementById("tokenSearchKey");
    this._globalSearchKey = document.getElementById("globalSearchKey");
    this._resumeKey = document.getElementById("resumeKey");
    this._stepOverKey = document.getElementById("stepOverKey");
    this._stepInKey = document.getElementById("stepInKey");
    this._stepOutKey = document.getElementById("stepOutKey");

    // Buttons, textboxes etc.
    this._resumeButton = document.getElementById("resume");
    this._stepOverButton = document.getElementById("step-over");
    this._stepInButton = document.getElementById("step-in");
    this._stepOutButton = document.getElementById("step-out");
    this._scriptsSearchbox = document.getElementById("scripts-search");
    this._globalOperatorLabel = document.getElementById("global-operator-label");
    this._globalOperatorButton = document.getElementById("global-operator-button");
    this._tokenOperatorLabel = document.getElementById("token-operator-label");
    this._tokenOperatorButton = document.getElementById("token-operator-button");
    this._lineOperatorLabel = document.getElementById("line-operator-label");
    this._lineOperatorButton = document.getElementById("line-operator-button");
  },

  /**
   * Applies the correct key labels and tooltips across global view elements.
   */
  initializeKeys: function DV_initializeKeys() {
    this._resumeButton.setAttribute("tooltiptext",
      L10N.getFormatStr("pauseButtonTooltip", [LayoutHelpers.prettyKey(this._resumeKey)]));
    this._stepOverButton.setAttribute("tooltiptext",
      L10N.getFormatStr("stepOverTooltip", [LayoutHelpers.prettyKey(this._stepOverKey)]));
    this._stepInButton.setAttribute("tooltiptext",
      L10N.getFormatStr("stepInTooltip", [LayoutHelpers.prettyKey(this._stepInKey)]));
    this._stepOutButton.setAttribute("tooltiptext",
      L10N.getFormatStr("stepOutTooltip", [LayoutHelpers.prettyKey(this._stepOutKey)]));

    this._scriptsSearchbox.setAttribute("placeholder",
      L10N.getFormatStr("emptyFilterText", [LayoutHelpers.prettyKey(this._fileSearchKey)]));
    this._globalOperatorLabel.setAttribute("value",
      L10N.getFormatStr("searchPanelGlobal", [LayoutHelpers.prettyKey(this._globalSearchKey)]));
    this._tokenOperatorLabel.setAttribute("value",
      L10N.getFormatStr("searchPanelToken", [LayoutHelpers.prettyKey(this._tokenSearchKey)]));
    this._lineOperatorLabel.setAttribute("value",
      L10N.getFormatStr("searchPanelLine", [LayoutHelpers.prettyKey(this._lineSearchKey)]));

    this._globalOperatorButton.setAttribute("label", SEARCH_GLOBAL_FLAG);
    this._tokenOperatorButton.setAttribute("label", SEARCH_TOKEN_FLAG);
    this._lineOperatorButton.setAttribute("label", SEARCH_LINE_FLAG);
  },

  /**
   * Initializes UI properties for all the displayed panes.
   */
  initializePanes: function DV_initializePanes() {
    this._togglePanesButton.addEventListener("click", this._onTogglePanesButtonPressed);

    this._stackframesAndBreakpoints.setAttribute("width", Prefs.stackframesWidth);
    this._variables.setAttribute("width", Prefs.variablesWidth);

    this.showStackframesAndBreakpointsPane(Prefs.stackframesPaneVisible);
    this.showVariablesPane(Prefs.variablesPaneVisible);
  },

  /**
   * Initializes the SourceEditor instance.
   *
   * @param function aCallback
   *        Called after the editor finishes initializing.
   */
  initializeEditor: function DV_initializeEditor(aCallback) {
    let placeholder = document.getElementById("editor");

    let config = {
      mode: SourceEditor.MODES.JAVASCRIPT,
      showLineNumbers: true,
      readOnly: true,
      showAnnotationRuler: true,
      showOverviewRuler: true,
    };

    this.editor = new SourceEditor();
    this.editor.init(placeholder, config, function() {
      this._onEditorLoad();
      aCallback();
    }.bind(this));
  },

  /**
   * Removes the displayed panes and saves any necessary state.
   */
  destroyPanes: function DV_destroyPanes() {
    this._togglePanesButton.removeEventListener("click", this._onTogglePanesButtonPressed);

    Prefs.stackframesWidth = this._stackframesAndBreakpoints.getAttribute("width");
    Prefs.variablesWidth = this._variables.getAttribute("width");

    this._breakpoints.parentNode.removeChild(this._breakpoints);
    this._stackframes.parentNode.removeChild(this._stackframes);
    this._stackframesAndBreakpoints.parentNode.removeChild(this._stackframesAndBreakpoints);
    this._variables.parentNode.removeChild(this._variables);
    this._globalSearch.parentNode.removeChild(this._globalSearch);

    // Delete all the cached global view elements.
    for (let i in this) {
      if (!(this[i] instanceof Function)) delete this[i];
    }
  },

  /**
   * Removes the SourceEditor instance and added breakpoints.
   */
  destroyEditor: function DV_destroyEditor() {
    DebuggerController.Breakpoints.destroy();
    this.editor = null;
  },

  /**
   * The load event handler for the source editor. This method does post-load
   * editor initialization.
   */
  _onEditorLoad: function DV__onEditorLoad() {
    DebuggerController.Breakpoints.initialize();
    this.editor.focus();
  },

  /**
   * Called when the panes toggle button is clicked.
   */
  _onTogglePanesButtonPressed: function DV__onTogglePanesButtonPressed() {
    this.showStackframesAndBreakpointsPane(
      this._togglePanesButton.getAttribute("stackframesAndBreakpointsHidden"), true);

    this.showVariablesPane(
      this._togglePanesButton.getAttribute("variablesHidden"), true);
  },

  /**
   * Sets the close button hidden or visible. It's hidden by default.
   * @param boolean aVisibleFlag
   */
  showCloseButton: function DV_showCloseButton(aVisibleFlag) {
    document.getElementById("close").setAttribute("hidden", !aVisibleFlag);
  },

  /**
   * Sets the stackframes and breakpoints pane hidden or visible.
   * @param boolean aVisibleFlag
   * @param boolean aAnimatedFlag
   */
  showStackframesAndBreakpointsPane:
  function DV_showStackframesAndBreakpointsPane(aVisibleFlag, aAnimatedFlag) {
    if (aAnimatedFlag) {
      this._stackframesAndBreakpoints.setAttribute("animated", "");
    } else {
      this._stackframesAndBreakpoints.removeAttribute("animated");
    }
    if (aVisibleFlag) {
      this._stackframesAndBreakpoints.style.marginLeft = "0";
      this._togglePanesButton.removeAttribute("stackframesAndBreakpointsHidden");
      this._togglePanesButton.setAttribute("tooltiptext", L10N.getStr("collapsePanes"));
    } else {
      let margin = parseInt(this._stackframesAndBreakpoints.getAttribute("width")) + 1;
      this._stackframesAndBreakpoints.style.marginLeft = -margin + "px";
      this._togglePanesButton.setAttribute("stackframesAndBreakpointsHidden", "true");
      this._togglePanesButton.setAttribute("tooltiptext", L10N.getStr("expandPanes"));
    }
    Prefs.stackframesPaneVisible = aVisibleFlag;
  },

  /**
   * Sets the variable spane hidden or visible.
   * @param boolean aVisibleFlag
   * @param boolean aAnimatedFlag
   */
  showVariablesPane:
  function DV_showVariablesPane(aVisibleFlag, aAnimatedFlag) {
    if (aAnimatedFlag) {
      this._variables.setAttribute("animated", "");
    } else {
      this._variables.removeAttribute("animated");
    }
    if (aVisibleFlag) {
      this._variables.style.marginRight = "0";
      this._togglePanesButton.removeAttribute("variablesHidden");
      this._togglePanesButton.setAttribute("tooltiptext", L10N.getStr("collapsePanes"));
    } else {
      let margin = parseInt(this._variables.getAttribute("width")) + 1;
      this._variables.style.marginRight = -margin + "px";
      this._togglePanesButton.setAttribute("variablesHidden", "true");
      this._togglePanesButton.setAttribute("tooltiptext", L10N.getStr("expandPanes"));
    }
    Prefs.variablesPaneVisible = aVisibleFlag;
  },

  /**
   * The cached global view elements.
   */
  _togglePanesButton: null,
  _stackframesAndBreakpoints: null,
  _stackframes: null,
  _breakpoints: null,
  _variables: null,
  _scripts: null,
  _globalSearch: null,
  _globalSearchSplitter: null,
  _fileSearchKey: null,
  _lineSearchKey: null,
  _tokenSearchKey: null,
  _globalSearchKey: null,
  _resumeKey: null,
  _stepOverKey: null,
  _stepInKey: null,
  _stepOutKey: null,
  _resumeButton: null,
  _stepOverButton: null,
  _stepInButton: null,
  _stepOutButton: null,
  _scriptsSearchbox: null,
  _globalOperatorLabel: null,
  _globalOperatorButton: null,
  _tokenOperatorLabel: null,
  _tokenOperatorButton: null,
  _lineOperatorLabel: null,
  _lineOperatorButton: null
};

/**
 * A simple way of displaying a "Connect to..." prompt.
 */
function RemoteDebuggerPrompt() {

  /**
   * The remote host and port the user wants to connect to.
   */
  this.remote = {};
}

RemoteDebuggerPrompt.prototype = {

  /**
   * Shows the prompt and sets the uri using the user input.
   *
   * @param boolean aIsReconnectingFlag
   *                True to show the reconnect message instead.
   */
  show: function RDP_show(aIsReconnectingFlag) {
    let check = { value: Prefs.remoteAutoConnect };
    let input = { value: Prefs.remoteHost + ":" + Prefs.remotePort };
    let parts;

    while (true) {
      let result = Services.prompt.prompt(null,
        L10N.getStr("remoteDebuggerPromptTitle"),
        L10N.getStr(aIsReconnectingFlag
          ? "remoteDebuggerReconnectMessage"
          : "remoteDebuggerPromptMessage"), input,
        L10N.getStr("remoteDebuggerPromptCheck"), check);

      Prefs.remoteAutoConnect = check.value;

      if (!result) {
        return false;
      }
      if ((parts = input.value.split(":")).length === 2) {
        let [host, port] = parts;

        if (host.length && port.length) {
          this.remote = { host: host, port: port };
          return true;
        }
      }
    }
  }
};

/**
 * Functions handling the global search UI.
 */
function GlobalSearchView() {
  this._onFetchScriptFinished = this._onFetchScriptFinished.bind(this);
  this._onFetchScriptsFinished = this._onFetchScriptsFinished.bind(this);
  this._onLineClick = this._onLineClick.bind(this);
  this._onMatchClick = this._onMatchClick.bind(this);
  this._onResultsScroll = this._onResultsScroll.bind(this);
  this._onFocusLost = this._onFocusLost.bind(this);
  this._startSearch = this._startSearch.bind(this);
}

GlobalSearchView.prototype = {

  /**
   * Hides or shows the search results container.
   * @param boolean value
   */
  set hidden(value) {
    this._pane.hidden = value;
    this._splitter.hidden = value;
  },

  /**
   * True if the search results container is hidden.
   * @return boolean
   */
  get hidden() this._pane.hidden,

  /**
   * Removes all elements from the search results container, leaving it empty.
   */
  empty: function DVGS_empty() {
    while (this._pane.firstChild) {
      this._pane.removeChild(this._pane.firstChild);
    }
    this._pane.scrollTop = 0;
    this._pane.scrollLeft = 0;
    this._currentlyFocusedMatch = -1;
  },

  /**
   * Hides and empties the search results container.
   */
  hideAndEmpty: function DVGS_hideAndEmpty() {
    this.hidden = true;
    this.empty();
    DebuggerController.dispatchEvent("Debugger:GlobalSearch:ViewCleared");
  },

  /**
   * Clears all the fetched scripts from the cache.
   */
  clearCache: function DVGS_clearCache() {
    this._scriptSources = new Map();
    DebuggerController.dispatchEvent("Debugger:GlobalSearch:CacheCleared");
  },

  /**
   * Starts fetching all the script sources, silently.
   *
   * @param function aFetchCallback [optional]
   *        Called after each script is fetched.
   * @param function aFetchedCallback [optional]
   *        Called if all the scripts were already fetched.
   * @param array aUrls [optional]
   *        The urls for the scripts to fetch. If undefined, it defaults to
   *        all the currently known scripts.
   */
  fetchScripts:
  function DVGS_fetchScripts(aFetchCallback = null,
                             aFetchedCallback = null,
                             aUrls = DebuggerView.Scripts.scriptLocations) {

    // If all the scripts sources were already fetched, then don't do anything.
    if (this._scriptSources.size() === aUrls.length) {
      aFetchedCallback && aFetchedCallback();
      return;
    }

    // Fetch each new script's source.
    for (let url of aUrls) {
      if (this._scriptSources.has(url)) {
        continue;
      }
      DebuggerController.dispatchEvent("Debugger:LoadSource", {
        url: url,
        options: {
          silent: true,
          callback: aFetchCallback
        }
      });
    }
  },

  /**
   * Schedules searching for a token in all the scripts.
   */
  scheduleSearch: function DVGS_scheduleSearch() {
    window.clearTimeout(this._searchTimeout);
    this._searchTimeout = window.setTimeout(this._startSearch, GLOBAL_SEARCH_ACTION_DELAY);
  },

  /**
   * Starts searching for a token in all the scripts.
   */
  _startSearch: function DVGS__startSearch() {
    let scriptLocations = DebuggerView.Scripts.scriptLocations;
    this._scriptCount = scriptLocations.length;

    this.fetchScripts(
      this._onFetchScriptFinished, this._onFetchScriptsFinished, scriptLocations);
  },

  /**
   * Called when a script's source has been fetched.
   *
   * @param string aScriptUrl
   *        The URL of the source script.
   * @param string aSourceText
   *        The text of the source script.
   */
  _onFetchScriptFinished: function DVGS__onFetchScriptFinished(aScriptUrl, aSourceText) {
    this._scriptSources.set(aScriptUrl, aSourceText);

    if (this._scriptSources.size() === this._scriptCount) {
      this._onFetchScriptsFinished();
    }
  },

  /**
   * Called when all the script's sources have been fetched.
   */
  _onFetchScriptsFinished: function DVGS__onFetchScriptsFinished() {
    this.empty();

    let token = DebuggerView.Scripts.searchToken;
    let lowerCaseToken = token.toLowerCase();

    // Make sure we're actually searching for something.
    if (!token) {
      DebuggerController.dispatchEvent("Debugger:GlobalSearch:TokenEmpty");
      this.hidden = true;
      return;
    }

    // Prepare the results map, containing search details for each script/line.
    let globalResults = new Map();

    for (let [url, text] of this._scriptSources) {
      // Check if the search token is not found anywhere in the script source.
      if (!text.toLowerCase().contains(lowerCaseToken)) {
        continue;
      }
      let lines = text.split("\n");
      let scriptResults = {
        lineResults: [],
        matchCount: 0
      };

      for (let i = 0, len = lines.length; i < len; i++) {
        let line = lines[i];
        let lowerCaseLine = line.toLowerCase();

        // Search is not case sensitive, and is tied to each line in the source.
        if (!lowerCaseLine.contains(lowerCaseToken)) {
          continue;
        }

        let lineNumber = i;
        let lineContents = [];

        lowerCaseLine.split(lowerCaseToken).reduce(function(prev, curr, index, {length}) {
          let unmatched = line.substr(prev.length, curr.length);
          lineContents.push({ string: unmatched });

          if (index !== length - 1) {
            let matched = line.substr(prev.length + curr.length, token.length);
            let range = {
              start: prev.length + curr.length,
              length: matched.length
            };
            lineContents.push({
              string: matched,
              range: range,
              match: true
            });
            scriptResults.matchCount++;
          }
          return prev + token + curr;
        }, "");

        scriptResults.lineResults.push({
          lineNumber: lineNumber,
          lineContents: lineContents
        });
      }
      if (scriptResults.matchCount) {
        globalResults.set(url, scriptResults);
      }
    }

    if (globalResults.size()) {
      this._createGlobalResultsUI(globalResults);
      this.hidden = false;
      DebuggerController.dispatchEvent("Debugger:GlobalSearch:MatchFound");
    } else {
      this.hidden = true;
      DebuggerController.dispatchEvent("Debugger:GlobalSearch:MatchNotFound");
    }
  },

  /**
   * Creates global search results elements and adds them to the results container.
   *
   * @param Map aGlobalResults
   *        A map containing the search results, grouped by script url.
   */
  _createGlobalResultsUI:
  function DVGS__createGlobalResultsUI(aGlobalResults) {
    let i = 0;

    for (let [scriptUrl, scriptResults] of aGlobalResults) {
      if (i++ === 0) {
        this._createScriptResultsUI(scriptUrl, scriptResults, true);
      } else {
        // Dispatch subsequent document manipulation operations, to avoid
        // blocking the main thread when a large number of search results
        // is found, thus giving the impression of faster searching.
        Services.tm.currentThread.dispatch({ run:
          this._createScriptResultsUI.bind(this, scriptUrl, scriptResults) }, 0);
      }
    }
  },

  /**
   * Creates script search results elements and adds them to the results container.
   *
   * @param string aScriptUrl
   *        The URL of the source script.
   * @param array aScriptResults
   *        An array containing the search results for a single script url.
   * @param boolean aExpandFlag
   *        True to expand the script results container.
   */
  _createScriptResultsUI:
  function DVGS__createScriptResultsUI(aScriptUrl, aScriptResults, aExpandFlag) {
    let { lineResults, matchCount } = aScriptResults;
    let element;

    for (let lineResult of lineResults) {
      element = this._createLineSearchResultsUI({
        scriptUrl: aScriptUrl,
        matchCount: matchCount,
        lineNumber: lineResult.lineNumber + 1,
        lineContents: lineResult.lineContents
      });
    }
    if (aExpandFlag) {
      element.expand(true);
    }
  },

  /**
   * Creates per-line search results elements and adds them to the results container.
   *
   * @param object aLineResults
   *        An object containing the search results for each line in a script.
   * @return object
   *         The newly created html node representing the added search results.
   */
  _createLineSearchResultsUI:
  function DVGS__createLineSearchresultsUI(aLineResults) {
    let scriptResultsId = "search-results-" + aLineResults.scriptUrl;
    let scriptResults = document.getElementById(scriptResultsId);

    // Create the script results container if not available yet.
    if (!scriptResults) {
      let trimFunc = DebuggerController.SourceScripts.trimUrlLength;
      let urlLabel = trimFunc(aLineResults.scriptUrl, GLOBAL_SEARCH_URL_MAX_SIZE);

      let resultsUrl = document.createElement("label");
      resultsUrl.className = "plain script-url";
      resultsUrl.setAttribute("value", urlLabel);

      let resultsCount = document.createElement("label");
      resultsCount.className = "plain match-count";
      resultsCount.setAttribute("value", "(" + aLineResults.matchCount + ")");

      let arrow = document.createElement("box");
      arrow.className = "arrow";

      let resultsHeader = document.createElement("hbox");
      resultsHeader.className = "dbg-results-header";
      resultsHeader.setAttribute("align", "center")
      resultsHeader.appendChild(arrow);
      resultsHeader.appendChild(resultsUrl);
      resultsHeader.appendChild(resultsCount);

      let resultsContainer = document.createElement("vbox");
      resultsContainer.className = "dbg-results-container";

      scriptResults = document.createElement("vbox");
      scriptResults.id = scriptResultsId;
      scriptResults.className = "dbg-script-results";
      scriptResults.header = resultsHeader;
      scriptResults.container = resultsContainer;
      scriptResults.appendChild(resultsHeader);
      scriptResults.appendChild(resultsContainer);
      this._pane.appendChild(scriptResults);

      /**
       * Expands the element, showing all the added details.
       *
       * @param boolean aSkipAnimationFlag
       *        Pass true to not show an opening animation.
       * @return object
       *         The same element.
       */
      scriptResults.expand = function DVGS_element_expand(aSkipAnimationFlag) {
        resultsContainer.setAttribute("open", "");
        arrow.setAttribute("open", "");

        if (!aSkipAnimationFlag) {
          resultsContainer.setAttribute("animated", "");
        }
        return scriptResults;
      };

      /**
       * Collapses the element, hiding all the added details.
       * @return object
       *         The same element.
       */
      scriptResults.collapse = function DVGS_element_collapse() {
        resultsContainer.removeAttribute("animated");
        resultsContainer.removeAttribute("open");
        arrow.removeAttribute("open");
        return scriptResults;
      };

      /**
       * Toggles between the element collapse/expand state.
       * @return object
       *         The same element.
       */
      scriptResults.toggle = function DVGS_element_toggle(e) {
        if (e instanceof Event) {
          scriptResults._userToggle = true;
        }
        scriptResults.expanded = !scriptResults.expanded;
        return scriptResults;
      };

      /**
       * Returns if the element is expanded.
       * @return boolean
       *         True if the element is expanded.
       */
      Object.defineProperty(scriptResults, "expanded", {
        get: function DVP_element_getExpanded() {
          return arrow.hasAttribute("open");
        },
        set: function DVP_element_setExpanded(value) {
          if (value) {
            scriptResults.expand();
          } else {
            scriptResults.collapse();
          }
        }
      });

      /**
       * Called when a header in the search results container is clicked.
       */
      resultsHeader.addEventListener("click", scriptResults.toggle, false);
    }

    let lineNumber = document.createElement("label");
    lineNumber.className = "plain line-number";
    lineNumber.setAttribute("value", aLineResults.lineNumber);

    let lineContents = document.createElement("hbox");
    lineContents.setAttribute("flex", "1");
    lineContents.className = "line-contents";
    lineContents.addEventListener("click", this._onLineClick, false);

    let lineContent;
    let totalLength = 0;
    let ellipsis = Services.prefs.getComplexValue("intl.ellipsis", Ci.nsIPrefLocalizedString);

    for (lineContent of aLineResults.lineContents) {
      let string = lineContent.string;
      let match = lineContent.match;

      string = string.substr(0, GLOBAL_SEARCH_LINE_MAX_SIZE - totalLength);
      totalLength += string.length;

      let label = document.createElement("label");
      label.className = "plain string";
      label.setAttribute("value", string);
      label.setAttribute("match", match || false);
      lineContents.appendChild(label);

      if (match) {
        label.addEventListener("click", this._onMatchClick, false);
        label.setUserData("lineResults", aLineResults, null);
        label.setUserData("lineContentRange", lineContent.range, null);
        label.container = scriptResults;
      }
      if (totalLength >= GLOBAL_SEARCH_LINE_MAX_SIZE) {
        label = document.createElement("label");
        label.className = "plain string";
        label.setAttribute("value", ellipsis.data);
        lineContents.appendChild(label);
        break;
      }
    }

    let searchResult = document.createElement("hbox");
    searchResult.className = "dbg-search-result";
    searchResult.appendChild(lineNumber);
    searchResult.appendChild(lineContents);

    let resultsContainer = scriptResults.container;
    resultsContainer.appendChild(searchResult);

    // Return the element for later use if necessary.
    return scriptResults;
  },

  /**
   * Focuses the next found match in the source editor.
   */
  focusNextMatch: function DVGS_focusNextMatch() {
    let matches = this._pane.querySelectorAll(".string[match=true]");
    if (!matches.length) {
      return;
    }
    if (++this._currentlyFocusedMatch >= matches.length) {
      this._currentlyFocusedMatch = 0;
    }
    this._onMatchClick({ target: matches[this._currentlyFocusedMatch] });
  },

  /**
   * Focuses the previously found match in the source editor.
   */
  focusPrevMatch: function DVGS_focusPrevMatch() {
    let matches = this._pane.querySelectorAll(".string[match=true]");
    if (!matches.length) {
      return;
    }
    if (--this._currentlyFocusedMatch < 0) {
      this._currentlyFocusedMatch = matches.length - 1;
    }
    this._onMatchClick({ target: matches[this._currentlyFocusedMatch] });
  },

  /**
   * Called when a line in the search results container is clicked.
   */
  _onLineClick: function DVGS__onLineClick(e) {
    let firstMatch = e.target.parentNode.querySelector(".string[match=true]");
    this._onMatchClick({ target: firstMatch });
  },

  /**
   * Called when a match in the search results container is clicked.
   */
  _onMatchClick: function DVGLS__onMatchClick(e) {
    if (e instanceof Event) {
      e.preventDefault();
      e.stopPropagation();
    }
    let match = e.target;

    match.container.expand(true);
    this._scrollMatchIntoViewIfNeeded(match);
    this._animateMatchBounce(match);

    let results = match.getUserData("lineResults");
    let range = match.getUserData("lineContentRange");

    let stackframes = DebuggerController.StackFrames;
    stackframes.updateEditorToLocation(results.scriptUrl, results.lineNumber, 0, 0, 1);

    let editor = DebuggerView.editor;
    let offset = editor.getCaretOffset();
    editor.setSelection(offset + range.start, offset + range.start + range.length);
  },

  /**
   * Listener handling the searchbox blur event.
   */
  _onFocusLost: function DVGS__onFocusLost(e) {
    this.hideAndEmpty();
  },

  /**
   * Listener handling the global search container scroll event.
   */
  _onResultsScroll: function DVGS__onResultsScroll(e) {
    this._expandAllVisibleResults();
  },

  /**
   * Expands all the script results that are currently visible.
   */
  _expandAllVisibleResults: function DVGS__expandAllVisibleResults() {
    let collapsed = this._pane.querySelectorAll(".dbg-results-container:not([open])");

    for (let i = 0, l = collapsed.length; i < l; i++) {
      this._expandResultsIfNeeded(collapsed[i].parentNode);
    }
  },

  /**
   * Expands the script results it they are currently visible.
   * @param nsIDOMElement aTarget
   */
  _expandResultsIfNeeded: function DVGS__expandResultsIfNeeded(aTarget) {
    if (aTarget.expanded || aTarget._userToggle) {
      return;
    }
    let { clientHeight } = this._pane;
    let { top, height } = aTarget.getBoundingClientRect();

    if (top - height <= clientHeight || this._forceExpandResults) {
      aTarget.expand(true);
    }
  },

  /**
   * Scrolls a match into view.
   * @param nsIDOMElement aTarget
   */
  _scrollMatchIntoViewIfNeeded: function DVGS__scrollMatchIntoViewIfNeeded(aTarget) {
    let { clientHeight } = this._pane;
    let { top, height } = aTarget.getBoundingClientRect();

    let style = window.getComputedStyle(aTarget);
    let topBorderSize = window.parseInt(style.getPropertyValue("border-top-width"));
    let bottomBorderSize = window.parseInt(style.getPropertyValue("border-bottom-width"));

    let marginY = top - (height + topBorderSize + bottomBorderSize) * 2;
    if (marginY <= 0) {
      this._pane.scrollTop += marginY;
    }
    if (marginY + height > clientHeight) {
      this._pane.scrollTop += height - (clientHeight - marginY);
    }
  },

  /**
   * Starts a bounce animation for a match.
   * @param nsIDOMElement aTarget
   */
  _animateMatchBounce: function DVGS__animateMatchBounce(aTarget) {
    aTarget.setAttribute("focused", "");

    window.setTimeout(function() {
     aTarget.removeAttribute("focused");
    }, GLOBAL_SEARCH_MATCH_FLASH_DURATION);
  },

  /**
   * Map containing the sources for all the currently known scripts.
   */
  _scriptSources: new Map(),

  /**
   * The currently focused match from the search results container.
   */
  _currentlyFocusedMatch: -1,

  /**
   * The cached global search results container.
   */
  _pane: null,
  _splitter: null,
  _searchbox: null,

  /**
   * Initialization function, called when the debugger is initialized.
   */
  initialize: function DVGS_initialize() {
    this._pane = DebuggerView._globalSearch;
    this._splitter = DebuggerView._globalSearchSplitter;
    this._searchbox = DebuggerView._scriptsSearchbox;

    this._pane.addEventListener("scroll", this._onResultsScroll, false);
    this._searchbox.addEventListener("blur", this._onFocusLost, false);
  },

  /**
   * Destruction function, called when the debugger is shut down.
   */
  destroy: function DVS_destroy() {
    this._pane.removeEventListener("scroll", this._onResultsScroll, false);
    this._searchbox.removeEventListener("blur", this._onFocusLost, false);

    this.hideAndEmpty();
    this._pane = null;
    this._splitter = null;
    this._searchbox = null;
    this._scriptSources = null;
  }
};

/**
 * Functions handling the scripts UI.
 */
function ScriptsView() {
  this._onScriptsChange = this._onScriptsChange.bind(this);
  this._onScriptsSearchClick = this._onScriptsSearchClick.bind(this);
  this._onScriptsSearchBlur = this._onScriptsSearchBlur.bind(this);
  this._onScriptsSearch = this._onScriptsSearch.bind(this);
  this._onScriptsKeyPress = this._onScriptsKeyPress.bind(this);
}

ScriptsView.prototype = {

  /**
   * Removes all elements from the scripts container, leaving it empty.
   */
  empty: function DVS_empty() {
    this._scripts.selectedIndex = -1;
    this._scripts.setAttribute("label", L10N.getStr("noScriptsText"));
    this._scripts.removeAttribute("tooltiptext");

    while (this._scripts.firstChild) {
      this._scripts.removeChild(this._scripts.firstChild);
    }
  },

  /**
   * Removes the input in the searchbox and unhides all the scripts.
   */
  clearSearch: function DVS_clearSearch() {
    this._searchbox.value = "";
    this._onScriptsSearch({});
  },

  /**
   * Checks whether the script with the specified URL is among the scripts
   * known to the debugger (ignoring the query & reference).
   *
   * @param string aUrl
   *        The script URL.
   * @return boolean
   */
  containsIgnoringQuery: function DVS_containsIgnoringQuery(aUrl) {
    let sourceScripts = DebuggerController.SourceScripts;
    aUrl = sourceScripts.trimUrlQuery(aUrl);

    if (this._tmpScripts.some(function(element) {
      return sourceScripts.trimUrlQuery(element.script.url) == aUrl;
    })) {
      return true;
    }
    if (this.scriptLocations.some(function(url) {
      return sourceScripts.trimUrlQuery(url) == aUrl;
    })) {
      return true;
    }
    return false;
  },

  /**
   * Checks whether the script with the specified URL is among the scripts
   * known to the debugger and shown in the list.
   *
   * @param string aUrl
   *        The script URL.
   * @return boolean
   */
  contains: function DVS_contains(aUrl) {
    if (this._tmpScripts.some(function(element) {
      return element.script.url == aUrl;
    })) {
      return true;
    }
    if (this._scripts.getElementsByAttribute("value", aUrl).length > 0) {
      return true;
    }
    return false;
  },

  /**
   * Checks whether the script with the specified label is among the scripts
   * known to the debugger and shown in the list.
   *
   * @param string aLabel
   *        The script label.
   * @return boolean
   */
  containsLabel: function DVS_containsLabel(aLabel) {
    if (this._tmpScripts.some(function(element) {
      return element.label == aLabel;
    })) {
      return true;
    }
    if (this._scripts.getElementsByAttribute("label", aLabel).length > 0) {
      return true;
    }
    return false;
  },

  /**
   * Selects the script with the specified index from the list.
   *
   * @param number aIndex
   *        The script index.
   */
  selectIndex: function DVS_selectIndex(aIndex) {
    this._scripts.selectedIndex = aIndex;
  },

  /**
   * Selects the script with the specified URL from the list.
   *
   * @param string aUrl
   *        The script URL.
   */
  selectScript: function DVS_selectScript(aUrl) {
    for (let i = 0, l = this._scripts.itemCount; i < l; i++) {
      if (this._scripts.getItemAtIndex(i).value == aUrl) {
        this._scripts.selectedIndex = i;
        return;
      }
    }
  },

  /**
   * Checks whether the script with the specified URL is selected in the list.
   *
   * @param string aUrl
   *        The script URL.
   */
  isSelected: function DVS_isSelected(aUrl) {
    if (this._scripts.selectedItem &&
        this._scripts.selectedItem.value == aUrl) {
      return true;
    }
    return false;
  },

  /**
   * Retrieve the URL of the selected script.
   * @return string | null
   */
  get selected() {
    return this._scripts.selectedItem ?
           this._scripts.selectedItem.value : null;
  },

  /**
   * Gets the most recently selected script url.
   * @return string | null
   */
  get preferredScriptUrl()
    this._preferredScriptUrl ? this._preferredScriptUrl : null,

  /**
   * Sets the most recently selected script url.
   * @param string
   */
  set preferredScriptUrl(value) this._preferredScriptUrl = value,

  /**
   * Gets the script in the container having the specified label.
   *
   * @param string aLabel
   *        The label used to identify the script.
   * @return element | null
   *         The matched element, or null if nothing is found.
   */
  getScriptByLabel: function DVS_getScriptByLabel(aLabel) {
    return this._scripts.getElementsByAttribute("label", aLabel)[0];
  },

  /**
   * Returns the list of labels in the scripts container.
   * @return array
   */
  get scriptLabels() {
    let labels = [];
    for (let i = 0, l = this._scripts.itemCount; i < l; i++) {
      labels.push(this._scripts.getItemAtIndex(i).label);
    }
    return labels;
  },

  /**
   * Gets the script in the container having the specified label.
   *
   * @param string aUrl
   *        The url used to identify the script.
   * @return element | null
   *         The matched element, or null if nothing is found.
   */
  getScriptByLocation: function DVS_getScriptByLocation(aUrl) {
    return this._scripts.getElementsByAttribute("value", aUrl)[0];
  },

  /**
   * Returns the list of URIs for scripts in the page.
   * @return array
   */
  get scriptLocations() {
    let locations = [];
    for (let i = 0, l = this._scripts.itemCount; i < l; i++) {
      locations.push(this._scripts.getItemAtIndex(i).value);
    }
    return locations;
  },

  /**
   * Gets the number of visible (hidden=false) scripts in the container.
   * @return number
   */
  get visibleItemsCount() {
    let count = 0;
    for (let i = 0, l = this._scripts.itemCount; i < l; i++) {
      count += this._scripts.getItemAtIndex(i).hidden ? 0 : 1;
    }
    return count;
  },

  /**
   * Prepares a script to be added to the scripts container. This allows
   * for a large number of scripts to be batched up before being
   * alphabetically sorted and added in the container.
   * @see ScriptsView.commitScripts
   *
   * If aForceFlag is true, the script will be immediately inserted at the
   * necessary position in the container so that all the scripts remain sorted.
   * This can be much slower than batching up multiple scripts.
   *
   * @param string aLabel
   *        The simplified script location to be shown.
   * @param string aScript
   *        The source script.
   * @param boolean aForceFlag
   *        True to force the script to be immediately added.
   */
  addScript: function DVS_addScript(aLabel, aScript, aForceFlag) {
    // Batch the script to be added later.
    if (!aForceFlag) {
      this._tmpScripts.push({ label: aLabel, script: aScript });
      return;
    }

    // Find the target position in the menulist and insert the script there.
    for (let i = 0, l = this._scripts.itemCount; i < l; i++) {
      if (this._scripts.getItemAtIndex(i).label > aLabel) {
        this._createScriptElement(aLabel, aScript, i);
        return;
      }
    }
    // The script is alphabetically the last one.
    this._createScriptElement(aLabel, aScript, -1);
  },

  /**
   * Adds all the prepared scripts to the scripts container.
   * If a script already exists (was previously added), nothing happens.
   */
  commitScripts: function DVS_commitScripts() {
    let newScripts = this._tmpScripts;
    this._tmpScripts = [];

    if (!newScripts || !newScripts.length) {
      return;
    }
    newScripts.sort(function(a, b) {
      return a.label.toLowerCase() > b.label.toLowerCase();
    });

    for (let i = 0, l = newScripts.length; i < l; i++) {
      let item = newScripts[i];
      this._createScriptElement(item.label, item.script, -1);
    }
  },

  /**
   * Creates a custom script element and adds it to the scripts container.
   * If the script with the specified label already exists, nothing happens.
   *
   * @param string aLabel
   *        The simplified script location to be shown.
   * @param string aScript
   *        The source script.
   * @param number aIndex
   *        The index where to insert to new script in the container.
   *        Pass -1 to append the script at the end.
   */
  _createScriptElement: function DVS__createScriptElement(aLabel, aScript, aIndex)
  {
    // Make sure we don't duplicate anything.
    if (aLabel == "null" || this.containsLabel(aLabel) || this.contains(aScript.url)) {
      return;
    }

    let scriptItem =
      aIndex == -1 ? this._scripts.appendItem(aLabel, aScript.url)
                   : this._scripts.insertItemAt(aIndex, aLabel, aScript.url);

    scriptItem.setAttribute("tooltiptext", aScript.url);
    scriptItem.setUserData("sourceScript", aScript, null);
  },

  /**
   * Gets the entered file, line and token entered in the searchbox.
   *
   * @return array
   *         A [file, line, token] array.
   */
  get searchboxInfo() {
    let file, line, token, isGlobal;

    let rawValue = this._searchbox.value;
    let rawLength = rawValue.length;
    let lineFlagIndex = rawValue.lastIndexOf(SEARCH_LINE_FLAG);
    let tokenFlagIndex = rawValue.lastIndexOf(SEARCH_TOKEN_FLAG);
    let globalFlagIndex = rawValue.lastIndexOf(SEARCH_GLOBAL_FLAG);

    if (globalFlagIndex !== 0) {
      let fileEnd = lineFlagIndex !== -1 ? lineFlagIndex : tokenFlagIndex !== -1 ? tokenFlagIndex : rawLength;
      let lineEnd = tokenFlagIndex !== -1 ? tokenFlagIndex : rawLength;

      file = rawValue.slice(0, fileEnd);
      line = window.parseInt(rawValue.slice(fileEnd + 1, lineEnd)) || -1;
      token = rawValue.slice(lineEnd + 1);
      isGlobal = false;
    } else {
      file = "";
      line = -1;
      token = rawValue.slice(1);
      isGlobal = true;
    }

    return [file, line, token, isGlobal];
  },

  /**
   * Returns the current search token.
   */
  get searchToken() this.searchboxInfo[2],

  /**
   * The click listener for the scripts container.
   */
  _onScriptsChange: function DVS__onScriptsChange() {
    let selectedItem = this._scripts.selectedItem;
    if (!selectedItem) {
      return;
    }

    this._preferredScript = selectedItem;
    this._preferredScriptUrl = selectedItem.value;
    this._scripts.setAttribute("tooltiptext", selectedItem.value);
    DebuggerController.SourceScripts.showScript(selectedItem.getUserData("sourceScript"));
  },

  _prevSearchedFile: "",
  _prevSearchedLine: 0,
  _prevSearchedToken: "",

  /**
   * Performs a file search if necessary.
   *
   * @param string aFile
   *        The script filename to search for.
   */
  _performFileSearch: function DVS__performFileSearch(aFile) {
    let scripts = this._scripts;

    // Presume we won't find anything.
    scripts.selectedItem = this._preferredScript;
    scripts.setAttribute("label", this._preferredScript.label);
    scripts.setAttribute("tooltiptext", this._preferredScript.value);

    // If we're not searching for a file anymore, unhide all the scripts.
    if (!aFile && this._someScriptsHidden) {
      this._someScriptsHidden = false;

      for (let i = 0, l = scripts.itemCount; i < l; i++) {
        scripts.getItemAtIndex(i).hidden = false;
      }
    } else if (this._prevSearchedFile !== aFile) {
      let lowerCaseFile = aFile.toLowerCase();
      let found = false;

      for (let i = 0, l = scripts.itemCount; i < l; i++) {
        let item = scripts.getItemAtIndex(i);
        let lowerCaseLabel = item.label.toLowerCase();

        // Search is not case sensitive, and is tied to the label not the url.
        if (lowerCaseLabel.match(aFile)) {
          item.hidden = false;

          if (!found) {
            found = true;
            scripts.selectedItem = item;
            scripts.setAttribute("label", item.label);
            scripts.setAttribute("tooltiptext", item.value);
          }
        }
        // Hide what doesn't match our search.
        else {
          item.hidden = true;
          this._someScriptsHidden = true;
        }
      }
      if (!found) {
        scripts.setAttribute("label", L10N.getStr("noMatchingScriptsText"));
        scripts.removeAttribute("tooltiptext");
      }
    }
    this._prevSearchedFile = aFile;
  },

  /**
   * Performs a line search if necessary.
   *
   * @param number aLine
   *        The script line number to jump to.
   */
  _performLineSearch: function DVS__performLineSearch(aLine) {
    // Jump to lines in the currently visible source.
    if (this._prevSearchedLine !== aLine && aLine > -1) {
      DebuggerView.editor.setCaretPosition(aLine - 1);
    }
    this._prevSearchedLine = aLine;
  },

  /**
   * Performs a token search if necessary.
   *
   * @param string aToken
   *        The script token to find.
   */
  _performTokenSearch: function DVS__performTokenSearch(aToken) {
    // Search for tokens in the currently visible source.
    if (this._prevSearchedToken !== aToken && aToken.length > 0) {
      let editor = DebuggerView.editor;
      let offset = editor.find(aToken, { ignoreCase: true });
      if (offset > -1) {
        editor.setSelection(offset, offset + aToken.length)
      }
    }
    this._prevSearchedToken = aToken;
  },

  /**
   * The focus listener for the scripts search box.
   */
  _onScriptsSearchClick: function DVS__onScriptsSearchClick() {
    this._searchboxPanel.openPopup(this._searchbox);
  },

  /**
   * The blur listener for the scripts search box.
   */
  _onScriptsSearchBlur: function DVS__onScriptsSearchBlur() {
    this._searchboxPanel.hidePopup();
  },

  /**
   * The search listener for the scripts search box.
   */
  _onScriptsSearch: function DVS__onScriptsSearch() {
    // If the webpage has no scripts, searching is redundant.
    if (!this._scripts.itemCount) {
      return;
    }
    this._searchboxPanel.hidePopup();

    let [file, line, token, isGlobal] = this.searchboxInfo;

    // If this is a global script search, schedule a search in all the sources,
    // or hide the pane otherwise.
    if (isGlobal) {
      DebuggerView.GlobalSearch.scheduleSearch();
    } else {
      DebuggerView.GlobalSearch.hideAndEmpty();
      this._performFileSearch(file);
      this._performLineSearch(line);
      this._performTokenSearch(token);
    }
  },

  /**
   * The keypress listener for the scripts search box.
   */
  _onScriptsKeyPress: function DVS__onScriptsKeyPress(e) {
    if (e.keyCode === e.DOM_VK_ESCAPE) {
      DebuggerView.editor.focus();
      return;
    }
    var action;

    if (e.keyCode === e.DOM_VK_DOWN ||
        e.keyCode === e.DOM_VK_RETURN ||
        e.keyCode === e.DOM_VK_ENTER) {
      action = 1;
    } else if (e.keyCode === e.DOM_VK_UP) {
      action = 2;
    }

    if (action) {
      let [file, line, token, isGlobal] = this.searchboxInfo;

      if (token.length) {
        e.preventDefault();
        e.stopPropagation();
      } else {
        return;
      }
      if (isGlobal) {
        if (DebuggerView.GlobalSearch.hidden) {
          DebuggerView.GlobalSearch.scheduleSearch();
        } else {
          DebuggerView.GlobalSearch[action === 1
            ? "focusNextMatch"
            : "focusPrevMatch"]();
        }
        return;
      }

      let editor = DebuggerView.editor;
      let offset = editor[action === 1 ? "findNext" : "findPrevious"](true);
      if (offset > -1) {
        editor.setSelection(offset, offset + token.length)
      }
    }
  },

  /**
   * Called when the scripts filter key sequence was pressed.
   */
  _onSearch: function DVS__onSearch(aValue = "") {
    this._searchbox.focus();
    this._searchbox.value = aValue;
    DebuggerView.GlobalSearch.hideAndEmpty();
  },

  /**
   * Called when the scripts path filter key sequence was pressed.
   */
  _onFileSearch: function DVS__onFileSearch() {
    this._onSearch();
    this._searchboxPanel.openPopup(this._searchbox);
  },

  /**
   * Called when the scripts token filter key sequence was pressed.
   */
  _onLineSearch: function DVS__onLineSearch() {
    this._onSearch(SEARCH_LINE_FLAG);
    this._searchboxPanel.hidePopup();
  },

  /**
   * Called when the scripts token filter key sequence was pressed.
   */
  _onTokenSearch: function DVS__onTokenSearch() {
    this._onSearch(SEARCH_TOKEN_FLAG);
    this._searchboxPanel.hidePopup();
  },

  /**
   * Called when the scripts token filter key sequence was pressed.
   */
  _onGlobalSearch: function DVS__onGlobalSearch() {
    this._onSearch(SEARCH_GLOBAL_FLAG);
    this._searchboxPanel.hidePopup();
  },

  /**
   * The cached scripts container and search box.
   */
  _scripts: null,
  _searchbox: null,
  _searchboxPanel: null,

  /**
   * Initialization function, called when the debugger is initialized.
   */
  initialize: function DVS_initialize() {
    this._scripts = DebuggerView._scripts;
    this._searchbox = document.getElementById("scripts-search");
    this._searchboxPanel = document.getElementById("scripts-search-panel");

    this._scripts.addEventListener("select", this._onScriptsChange, false);
    this._searchbox.addEventListener("click", this._onScriptsSearchClick, false);
    this._searchbox.addEventListener("blur", this._onScriptsSearchBlur, false);
    this._searchbox.addEventListener("select", this._onScriptsSearch, false);
    this._searchbox.addEventListener("input", this._onScriptsSearch, false);
    this._searchbox.addEventListener("keypress", this._onScriptsKeyPress, false);
    this.commitScripts();
  },

  /**
   * Destruction function, called when the debugger is shut down.
   */
  destroy: function DVS_destroy() {
    this._scripts.removeEventListener("select", this._onScriptsChange, false);
    this._searchbox.removeEventListener("click", this._onScriptsSearchClick, false);
    this._searchbox.removeEventListener("blur", this._onScriptsSearchBlur, false);
    this._searchbox.removeEventListener("select", this._onScriptsSearch, false);
    this._searchbox.removeEventListener("input", this._onScriptsSearch, false);
    this._searchbox.removeEventListener("keypress", this._onScriptsKeyPress, false);

    this.empty();
    this._scripts = null;
    this._searchbox = null;
    this._searchboxPanel = null;
  }
};

/**
 * Functions handling the html stackframes UI.
 */
function StackFramesView() {
  this._onFramesScroll = this._onFramesScroll.bind(this);
  this._onPauseExceptionsClick = this._onPauseExceptionsClick.bind(this);
  this._onCloseButtonClick = this._onCloseButtonClick.bind(this);
  this._onResume = this._onResume.bind(this);
  this._onStepOver = this._onStepOver.bind(this);
  this._onStepIn = this._onStepIn.bind(this);
  this._onStepOut = this._onStepOut.bind(this);
}

StackFramesView.prototype = {

 /**
  * Sets the current frames state based on the debugger active thread state.
  *
  * @param string aState
  *        Either "paused" or "attached".
  */
  updateState: function DVF_updateState(aState) {
    let resume = DebuggerView._resumeButton;
    let resumeKey = LayoutHelpers.prettyKey(DebuggerView._resumeKey);

    // If we're paused, show a pause label and a resume label on the button.
    if (aState == "paused") {
      resume.setAttribute("tooltiptext", L10N.getFormatStr("resumeButtonTooltip", [resumeKey]));
      resume.setAttribute("checked", true);
    }
    // If we're attached, do the opposite.
    else if (aState == "attached") {
      resume.setAttribute("tooltiptext", L10N.getFormatStr("pauseButtonTooltip", [resumeKey]));
      resume.removeAttribute("checked");
    }
  },

  /**
   * Removes all elements from the stackframes container, leaving it empty.
   */
  empty: function DVF_empty() {
    while (this._frames.firstChild) {
      this._frames.removeChild(this._frames.firstChild);
    }
  },

  /**
   * Removes all elements from the stackframes container, and adds a child node
   * with an empty text note attached.
   */
  emptyText: function DVF_emptyText() {
    // Make sure the container is empty first.
    this.empty();

    let item = document.createElement("label");

    // The empty node should look grayed out to avoid confusion.
    item.className = "list-item empty";
    item.setAttribute("value", L10N.getStr("emptyStackText"));

    this._frames.appendChild(item);
  },

  /**
   * Adds a frame to the stackframes container.
   * If the frame already exists (was previously added), null is returned.
   * Otherwise, the newly created element is returned.
   *
   * @param number aDepth
   *        The frame depth specified by the debugger.
   * @param string aFrameNameText
   *        The name to be displayed in the list.
   * @param string aFrameDetailsText
   *        The details to be displayed in the list.
   * @return object
   *         The newly created html node representing the added frame.
   */
  addFrame: function DVF_addFrame(aDepth, aFrameNameText, aFrameDetailsText) {
    // Make sure we don't duplicate anything.
    if (document.getElementById("stackframe-" + aDepth)) {
      return null;
    }

    let frame = document.createElement("box");
    let frameName = document.createElement("label");
    let frameDetails = document.createElement("label");

    // Create a list item to be added to the stackframes container.
    frame.id = "stackframe-" + aDepth;
    frame.className = "dbg-stackframe list-item";

    // This list should display the name and details for the frame.
    frameName.className = "dbg-stackframe-name plain";
    frameDetails.className = "dbg-stackframe-details plain";
    frameName.setAttribute("value", aFrameNameText);
    frameDetails.setAttribute("value", aFrameDetailsText);

    let spacer = document.createElement("spacer");
    spacer.setAttribute("flex", "1");

    frame.appendChild(frameName);
    frame.appendChild(spacer);
    frame.appendChild(frameDetails);

    this._frames.appendChild(frame);

    // Return the element for later use if necessary.
    return frame;
  },

  /**
   * Highlights a frame from the stackframe container as selected/deselected.
   *
   * @param number aDepth
   *        The frame depth specified by the debugger.
   * @param boolean aFlag
   *        True if the frame should be deselected, false otherwise.
   */
  highlightFrame: function DVF_highlightFrame(aDepth, aFlag) {
    let frame = document.getElementById("stackframe-" + aDepth);

    // The list item wasn't found in the stackframe container.
    if (!frame) {
      return;
    }

    // Add the 'selected' css class if the frame isn't already selected.
    if (!aFlag && !frame.classList.contains("selected")) {
      frame.classList.add("selected");
    }
    // Remove the 'selected' css class if the frame is already selected.
    else if (aFlag && frame.classList.contains("selected")) {
      frame.classList.remove("selected");
    }
  },

  /**
   * Deselects a frame from the stackframe container.
   *
   * @param number aDepth
   *        The frame depth specified by the debugger.
   */
  unhighlightFrame: function DVF_unhighlightFrame(aDepth) {
    this.highlightFrame(aDepth, true);
  },

  /**
   * Gets the current dirty state.
   *
   * @return boolean value
   *         True if should load more frames.
   */
  get dirty() {
    return this._dirty;
  },

  /**
   * Sets if the active thread has more frames that need to be loaded.
   *
   * @param boolean aValue
   *        True if should load more frames.
   */
  set dirty(aValue) {
    this._dirty = aValue;
  },

  /**
   * Listener handling the stackframes container click event.
   */
  _onFramesClick: function DVF__onFramesClick(aEvent) {
    let target = aEvent.target;

    while (target) {
      if (target.debuggerFrame) {
        DebuggerController.StackFrames.selectFrame(target.debuggerFrame.depth);
        return;
      }
      target = target.parentNode;
    }
  },

  /**
   * Listener handling the stackframes container scroll event.
   */
  _onFramesScroll: function DVF__onFramesScroll(aEvent) {
    // Update the stackframes container only if we have to.
    if (this._dirty) {
      let clientHeight = this._frames.clientHeight;
      let scrollTop = this._frames.scrollTop;
      let scrollHeight = this._frames.scrollHeight;

      // If the stackframes container was scrolled past 95% of the height,
      // load more content.
      if (scrollTop >= (scrollHeight - clientHeight) * 0.95) {
        this._dirty = false;

        DebuggerController.StackFrames.addMoreFrames();
      }
    }
  },

  /**
   * Listener handling the close button click event.
   */
  _onCloseButtonClick: function DVF__onCloseButtonClick() {
    DebuggerController.dispatchEvent("Debugger:Close");
  },

  /**
   * Listener handling the pause-on-exceptions click event.
   */
  _onPauseExceptionsClick: function DVF__onPauseExceptionsClick() {
    let option = document.getElementById("pause-exceptions");
    DebuggerController.StackFrames.updatePauseOnExceptions(option.checked);
  },

  /**
   * Listener handling the pause/resume button click event.
   */
  _onResume: function DVF__onResume(e) {
    if (DebuggerController.activeThread.paused) {
      DebuggerController.activeThread.resume();
    } else {
      DebuggerController.activeThread.interrupt();
    }
  },

  /**
   * Listener handling the step over button click event.
   */
  _onStepOver: function DVF__onStepOver(e) {
    if (DebuggerController.activeThread.paused) {
      DebuggerController.activeThread.stepOver();
    }
  },

  /**
   * Listener handling the step in button click event.
   */
  _onStepIn: function DVF__onStepIn(e) {
    if (DebuggerController.activeThread.paused) {
      DebuggerController.activeThread.stepIn();
    }
  },

  /**
   * Listener handling the step out button click event.
   */
  _onStepOut: function DVF__onStepOut(e) {
    if (DebuggerController.activeThread.paused) {
      DebuggerController.activeThread.stepOut();
    }
  },

  /**
   * Specifies if the active thread has more frames which need to be loaded.
   */
  _dirty: false,

  /**
   * The cached stackframes container.
   */
  _frames: null,

  /**
   * Initialization function, called when the debugger is initialized.
   */
  initialize: function DVF_initialize() {
    let close = document.getElementById("close");
    let pauseOnExceptions = document.getElementById("pause-exceptions");
    let resume = DebuggerView._resumeButton;
    let stepOver = DebuggerView._stepOverButton;
    let stepIn = DebuggerView._stepInButton;
    let stepOut = DebuggerView._stepOutButton;
    let frames = DebuggerView._stackframes;

    close.addEventListener("click", this._onCloseButtonClick, false);
    pauseOnExceptions.checked = DebuggerController.StackFrames.pauseOnExceptions;
    pauseOnExceptions.addEventListener("click", this._onPauseExceptionsClick, false);
    resume.addEventListener("click", this._onResume, false);
    stepOver.addEventListener("click", this._onStepOver, false);
    stepIn.addEventListener("click", this._onStepIn, false);
    stepOut.addEventListener("click", this._onStepOut, false);
    frames.addEventListener("click", this._onFramesClick, false);
    frames.addEventListener("scroll", this._onFramesScroll, false);
    window.addEventListener("resize", this._onFramesScroll, false);

    this._frames = frames;
    this.emptyText();
  },

  /**
   * Destruction function, called when the debugger is shut down.
   */
  destroy: function DVF_destroy() {
    let close = document.getElementById("close");
    let pauseOnExceptions = document.getElementById("pause-exceptions");
    let resume = DebuggerView._resumeButton;
    let stepOver = DebuggerView._stepOverButton;
    let stepIn = DebuggerView._stepInButton;
    let stepOut = DebuggerView._stepOutButton;
    let frames = DebuggerView._stackframes;

    close.removeEventListener("click", this._onCloseButtonClick, false);
    pauseOnExceptions.removeEventListener("click", this._onPauseExceptionsClick, false);
    resume.removeEventListener("click", this._onResume, false);
    stepOver.removeEventListener("click", this._onStepOver, false);
    stepIn.removeEventListener("click", this._onStepIn, false);
    stepOut.removeEventListener("click", this._onStepOut, false);
    frames.removeEventListener("click", this._onFramesClick, false);
    frames.removeEventListener("scroll", this._onFramesScroll, false);
    window.removeEventListener("resize", this._onFramesScroll, false);

    this.empty();
    this._frames = null;
  }
};

/**
 * Functions handling the breakpoints view.
 */
function BreakpointsView() {
  this._onBreakpointClick = this._onBreakpointClick.bind(this);
  this._onBreakpointCheckboxChange = this._onBreakpointCheckboxChange.bind(this);
}

BreakpointsView.prototype = {

  /**
   * Removes all elements from the breakpoints container, leaving it empty.
   */
  empty: function DVB_empty() {
    let firstChild;

    while (firstChild = this._breakpoints.firstChild) {
      this._destroyContextMenu(firstChild);
      this._breakpoints.removeChild(firstChild);
    }
  },

  /**
   * Removes all elements from the breakpoints container, and adds a child node
   * with an empty text note attached.
   */
  emptyText: function DVB_emptyText() {
    // Make sure the container is empty first.
    this.empty();

    let item = document.createElement("label");

    // The empty node should look grayed out to avoid confusion.
    item.className = "list-item empty";
    item.setAttribute("value", L10N.getStr("emptyBreakpointsText"));

    this._breakpoints.appendChild(item);
  },

  /**
   * Checks whether the breakpoint with the specified script URL and line is
   * among the breakpoints known to the debugger and shown in the list, and
   * returns the matched result or null if nothing is found.
   *
   * @param string aUrl
   *        The original breakpoint script url.
   * @param number aLine
   *        The original breakpoint script line.
   * @return object | null
   *         The queried breakpoint
   */
  getBreakpoint: function DVB_getBreakpoint(aUrl, aLine) {
    return this._breakpoints.getElementsByAttribute("location", aUrl + ":" + aLine)[0];
  },

  /**
   * Removes a breakpoint only from the breakpoints container.
   * This doesn't remove the breakpoint from the DebuggerController!
   *
   * @param string aId
   *        A breakpoint identifier specified by the debugger.
   */
  removeBreakpoint: function DVB_removeBreakpoint(aId) {
    let breakpoint = document.getElementById("breakpoint-" + aId);

    // Make sure we have something to remove.
    if (!breakpoint) {
      return;
    }
    this._destroyContextMenu(breakpoint);
    this._breakpoints.removeChild(breakpoint);

    if (!this.count) {
      this.emptyText();
    }
  },

  /**
   * Adds a breakpoint to the breakpoints container.
   * If the breakpoint already exists (was previously added), null is returned.
   * If it's already added but disabled, it will be enabled and null is returned.
   * Otherwise, the newly created element is returned.
   *
   * @param string aId
   *        A breakpoint identifier specified by the debugger.
   * @param string aLineInfo
   *        The script line information to be displayed in the list.
   * @param string aLineText
   *        The script line text to be displayed in the list.
   * @param string aUrl
   *        The original breakpoint script url.
   * @param number aLine
   *        The original breakpoint script line.
   * @return object
   *         The newly created html node representing the added breakpoint.
   */
  addBreakpoint: function DVB_addBreakpoint(aId, aLineInfo, aLineText, aUrl, aLine) {
    // Make sure we don't duplicate anything.
    if (document.getElementById("breakpoint-" + aId)) {
      return null;
    }
    // Remove the empty list text if it was there.
    if (!this.count) {
      this.empty();
    }

    // If the breakpoint was already added but disabled, enable it now.
    let breakpoint = this.getBreakpoint(aUrl, aLine);
    if (breakpoint) {
      breakpoint.id = "breakpoint-" + aId;
      breakpoint.breakpointActor = aId;
      breakpoint.getElementsByTagName("checkbox")[0].setAttribute("checked", "true");
      return;
    }

    breakpoint = document.createElement("box");
    let bkpCheckbox = document.createElement("checkbox");
    let bkpLineInfo = document.createElement("label");
    let bkpLineText = document.createElement("label");

    // Create a list item to be added to the stackframes container.
    breakpoint.id = "breakpoint-" + aId;
    breakpoint.className = "dbg-breakpoint list-item";
    breakpoint.setAttribute("location", aUrl + ":" + aLine);
    breakpoint.breakpointUrl = aUrl;
    breakpoint.breakpointLine = aLine;
    breakpoint.breakpointActor = aId;

    aLineInfo = aLineInfo.trim();
    aLineText = aLineText.trim();

    // A checkbox specifies if the breakpoint is enabled or not.
    bkpCheckbox.setAttribute("checked", "true");
    bkpCheckbox.addEventListener("click", this._onBreakpointCheckboxChange, false);

    // This list should display the line info and text for the breakpoint.
    bkpLineInfo.className = "dbg-breakpoint-info plain";
    bkpLineText.className = "dbg-breakpoint-text plain";
    bkpLineInfo.setAttribute("value", aLineInfo);
    bkpLineText.setAttribute("value", aLineText);
    bkpLineInfo.setAttribute("crop", "end");
    bkpLineText.setAttribute("crop", "end");
    bkpLineText.setAttribute("tooltiptext", aLineText.substr(0, BREAKPOINT_LINE_TOOLTIP_MAX_SIZE));

    // Create a context menu for the breakpoint.
    let menupopupId = this._createContextMenu(breakpoint);
    breakpoint.setAttribute("contextmenu", menupopupId);

    let state = document.createElement("vbox");
    state.className = "state";
    state.appendChild(bkpCheckbox);

    let content = document.createElement("vbox");
    content.className = "content";
    content.setAttribute("flex", "1");
    content.appendChild(bkpLineInfo);
    content.appendChild(bkpLineText);

    breakpoint.appendChild(state);
    breakpoint.appendChild(content);

    this._breakpoints.appendChild(breakpoint);

    // Return the element for later use if necessary.
    return breakpoint;
  },

  /**
   * Enables a breakpoint.
   *
   * @param object aBreakpoint
   *        An element representing a breakpoint.
   * @param function aCallback
   *        Optional function to invoke once the breakpoint is enabled.
   * @param boolean aNoCheckboxUpdate
   *        Pass true to not update the checkbox checked state.
   *        This is usually necessary when the checked state will be updated
   *        automatically (e.g: on a checkbox click).
   */
  enableBreakpoint:
  function DVB_enableBreakpoint(aTarget, aCallback, aNoCheckboxUpdate) {
    let { breakpointUrl: url, breakpointLine: line } = aTarget;
    let breakpoint = DebuggerController.Breakpoints.getBreakpoint(url, line)

    if (!breakpoint) {
      if (!aNoCheckboxUpdate) {
        aTarget.getElementsByTagName("checkbox")[0].setAttribute("checked", "true");
      }
      DebuggerController.Breakpoints.
        addBreakpoint({ url: url, line: line }, aCallback);

      return true;
    }
    return false;
  },

  /**
   * Disables a breakpoint.
   *
   * @param object aTarget
   *        An element representing a breakpoint.
   * @param function aCallback
   *        Optional function to invoke once the breakpoint is disabled.
   * @param boolean aNoCheckboxUpdate
   *        Pass true to not update the checkbox checked state.
   *        This is usually necessary when the checked state will be updated
   *        automatically (e.g: on a checkbox click).
   */
  disableBreakpoint:
  function DVB_disableBreakpoint(aTarget, aCallback, aNoCheckboxUpdate) {
    let { breakpointUrl: url, breakpointLine: line } = aTarget;
    let breakpoint = DebuggerController.Breakpoints.getBreakpoint(url, line)

    if (breakpoint) {
      if (!aNoCheckboxUpdate) {
        aTarget.getElementsByTagName("checkbox")[0].removeAttribute("checked");
      }
      DebuggerController.Breakpoints.
        removeBreakpoint(breakpoint, aCallback, false, true);

      return true;
    }
    return false;
  },

  /**
   * Gets the current number of added breakpoints.
   */
  get count() {
    return this._breakpoints.getElementsByClassName("dbg-breakpoint").length;
  },

  /**
   * Iterates through all the added breakpoints.
   *
   * @param function aCallback
   *        Function called for each element.
   */
  _iterate: function DVB_iterate(aCallback) {
    Array.forEach(Array.slice(this._breakpoints.childNodes), aCallback);
  },

  /**
   * Gets the real breakpoint target when an event is handled.
   * @return object
   */
  _getBreakpointTarget: function DVB__getBreakpointTarget(aEvent) {
    let target = aEvent.target;

    while (target) {
      if (target.breakpointActor) {
        return target;
      }
      target = target.parentNode;
    }
  },

  /**
   * Listener handling the breakpoint click event.
   */
  _onBreakpointClick: function DVB__onBreakpointClick(aEvent) {
    let target = this._getBreakpointTarget(aEvent);
    let { breakpointUrl: url, breakpointLine: line } = target;

    DebuggerController.StackFrames.updateEditorToLocation(url, line, 0, 0, 1);
  },

  /**
   * Listener handling the breakpoint checkbox change event.
   */
  _onBreakpointCheckboxChange: function DVB__onBreakpointCheckboxChange(aEvent) {
    aEvent.stopPropagation();

    let target = this._getBreakpointTarget(aEvent);
    let { breakpointUrl: url, breakpointLine: line } = target;

    if (aEvent.target.getAttribute("checked") === "true") {
      this.disableBreakpoint(target, null, true);
    } else {
      this.enableBreakpoint(target, null, true);
    }
  },

  /**
   * Listener handling the "enableSelf" menuitem command.
   *
   * @param object aTarget
   *        The corresponding breakpoint element.
   */
  _onEnableSelf: function DVB__onEnableSelf(aTarget) {
    if (!aTarget) {
      return;
    }
    if (this.enableBreakpoint(aTarget)) {
      aTarget.enableSelf.menuitem.setAttribute("hidden", "true");
      aTarget.disableSelf.menuitem.removeAttribute("hidden");
    }
  },

  /**
   * Listener handling the "disableSelf" menuitem command.
   *
   * @param object aTarget
   *        The corresponding breakpoint element.
   */
  _onDisableSelf: function DVB__onDisableSelf(aTarget) {
    if (!aTarget) {
      return;
    }
    if (this.disableBreakpoint(aTarget)) {
      aTarget.enableSelf.menuitem.removeAttribute("hidden");
      aTarget.disableSelf.menuitem.setAttribute("hidden", "true");
    }
  },

  /**
   * Listener handling the "deleteSelf" menuitem command.
   *
   * @param object aTarget
   *        The corresponding breakpoint element.
   */
  _onDeleteSelf: function DVB__onDeleteSelf(aTarget) {
    let { breakpointUrl: url, breakpointLine: line } = aTarget;
    let breakpoint = DebuggerController.Breakpoints.getBreakpoint(url, line)

    if (aTarget) {
      this.removeBreakpoint(aTarget.breakpointActor);
    }
    if (breakpoint) {
      DebuggerController.Breakpoints.removeBreakpoint(breakpoint);
    }
  },

  /**
   * Listener handling the "enableOthers" menuitem command.
   *
   * @param object aTarget
   *        The corresponding breakpoint element.
   */
  _onEnableOthers: function DVB__onEnableOthers(aTarget) {
    this._iterate(function(element) {
      if (element !== aTarget) {
        this._onEnableSelf(element);
      }
    }.bind(this));
  },

  /**
   * Listener handling the "disableOthers" menuitem command.
   *
   * @param object aTarget
   *        The corresponding breakpoint element.
   */
  _onDisableOthers: function DVB__onDisableOthers(aTarget) {
    this._iterate(function(element) {
      if (element !== aTarget) {
        this._onDisableSelf(element);
      }
    }.bind(this));
  },

  /**
   * Listener handling the "deleteOthers" menuitem command.
   *
   * @param object aTarget
   *        The corresponding breakpoint element.
   */
  _onDeleteOthers: function DVB__onDeleteOthers(aTarget) {
    this._iterate(function(element) {
      if (element !== aTarget) {
        this._onDeleteSelf(element);
      }
    }.bind(this));
  },

  /**
   * Listener handling the "disableAll" menuitem command.
   *
   * @param object aTarget
   *        The corresponding breakpoint element.
   */
  _onEnableAll: function DVB__onEnableAll(aTarget) {
    this._onEnableOthers(aTarget);
    this._onEnableSelf(aTarget);
  },

  /**
   * Listener handling the "disableAll" menuitem command.
   *
   * @param object aTarget
   *        The corresponding breakpoint element.
   */
  _onDisableAll: function DVB__onDisableAll(aTarget) {
    this._onDisableOthers(aTarget);
    this._onDisableSelf(aTarget);
  },

  /**
   * Listener handling the "deleteAll" menuitem command.
   *
   * @param object aTarget
   *        The corresponding breakpoint element.
   */
  _onDeleteAll: function DVB__onDeleteAll(aTarget) {
    this._onDeleteOthers(aTarget);
    this._onDeleteSelf(aTarget);
  },

  /**
   * The cached breakpoints container.
   */
  _breakpoints: null,

  /**
   * Creates a breakpoint context menu.
   *
   * @param object aBreakpoint
   *        An element representing a breakpoint.
   * @return string
   *         The popup id.
   */
  _createContextMenu: function DVB_createContextMenu(aBreakpoint) {
    let commandsetId = "breakpointMenuCommands-" + aBreakpoint.id;
    let menupopupId = "breakpointContextMenu-" + aBreakpoint.id;

    let commandset = document.createElement("commandset");
    commandset.setAttribute("id", commandsetId);

    let menupopup = document.createElement("menupopup");
    menupopup.setAttribute("id", menupopupId);

    /**
     * Creates a menu item specified by a name with the appropriate attributes
     * (label and command handler).
     *
     * @param string aName
     *        A global identifier for the menu item.
     * @param boolean aHiddenFlag
     *        True if this menuitem should be hidden.
     */
    function createMenuItem(aName, aHiddenFlag) {
      let menuitem = document.createElement("menuitem");
      let command = document.createElement("command");

      let func = this["_on" + aName.charAt(0).toUpperCase() + aName.slice(1)];
      let label = L10N.getStr("breakpointMenuItem." + aName);

      let prefix = "bp-cMenu-";
      let commandId = prefix + aName + "-" + aBreakpoint.id + "-command";
      let menuitemId = prefix + aName + "-" + aBreakpoint.id + "-menuitem";

      command.setAttribute("id", commandId);
      command.setAttribute("label", label);
      command.addEventListener("command", func.bind(this, aBreakpoint), true);

      menuitem.setAttribute("id", menuitemId);
      menuitem.setAttribute("command", commandId);
      menuitem.setAttribute("hidden", aHiddenFlag);

      commandset.appendChild(command);
      menupopup.appendChild(menuitem);

      aBreakpoint[aName] = {
        menuitem: menuitem,
        command: command
      };
    }

    /**
     * Creates a simple menu separator element and appends it to the current
     * menupopup hierarchy.
     */
    function createMenuSeparator() {
      let menuseparator = document.createElement("menuseparator");
      menupopup.appendChild(menuseparator);
    }

    createMenuItem.call(this, "enableSelf", true);
    createMenuItem.call(this, "disableSelf");
    createMenuItem.call(this, "deleteSelf");
    createMenuSeparator();
    createMenuItem.call(this, "enableOthers");
    createMenuItem.call(this, "disableOthers");
    createMenuItem.call(this, "deleteOthers");
    createMenuSeparator();
    createMenuItem.call(this, "enableAll");
    createMenuItem.call(this, "disableAll");
    createMenuSeparator();
    createMenuItem.call(this, "deleteAll");

    let popupset = document.getElementById("debugger-popups");
    popupset.appendChild(menupopup);
    document.documentElement.appendChild(commandset);

    aBreakpoint.commandsetId = commandsetId;
    aBreakpoint.menupopupId = menupopupId;

    return menupopupId;
  },

  /**
   * Destroys a breakpoint context menu.
   *
   * @param object aBreakpoint
   *        An element representing a breakpoint.
   */
  _destroyContextMenu: function DVB__destroyContextMenu(aBreakpoint) {
    if (!aBreakpoint.commandsetId || !aBreakpoint.menupopupId) {
      return;
    }

    let commandset = document.getElementById(aBreakpoint.commandsetId);
    let menupopup = document.getElementById(aBreakpoint.menupopupId);

    commandset.parentNode.removeChild(commandset);
    menupopup.parentNode.removeChild(menupopup);
  },

  /**
   * Initialization function, called when the debugger is initialized.
   */
  initialize: function DVB_initialize() {
    let breakpoints = DebuggerView._breakpoints;
    breakpoints.addEventListener("click", this._onBreakpointClick, false);

    this._breakpoints = breakpoints;
    this.emptyText();
  },

  /**
   * Destruction function, called when the debugger is shut down.
   */
  destroy: function DVB_destroy() {
    let breakpoints = this._breakpoints;
    breakpoints.removeEventListener("click", this._onBreakpointClick, false);

    this.empty();
    this._breakpoints = null;
  }
};

/**
 * Functions handling the properties view.
 */
function PropertiesView() {
  this.addScope = this._addScope.bind(this);
  this._addVar = this._addVar.bind(this);
  this._addProperties = this._addProperties.bind(this);
}

PropertiesView.prototype = {

  /**
   * A monotonically-increasing counter, that guarantees the uniqueness of scope
   * IDs.
   */
  _idCount: 1,

  /**
   * Adds a scope to contain any inspected variables.
   * If the optional id is not specified, the scope html node will have a
   * default id set as aName-scope.
   *
   * @param string aName
   *        The scope name (e.g. "Local", "Global" or "With block").
   * @param string aId
   *        Optional, an id for the scope html node.
   * @return object
   *         The newly created html node representing the added scope or null
   *         if a node was not created.
   */
  _addScope: function DVP__addScope(aName, aId) {
    // Make sure the parent container exists.
    if (!this._vars) {
      return null;
    }

    // Generate a unique id for the element, if not specified.
    aId = aId || aName.toLowerCase().trim().replace(/\s+/g, "-") + this._idCount++;

    // Contains generic nodes and functionality.
    let element = this._createPropertyElement(aName, aId, "scope", this._vars);

    // Make sure the element was created successfully.
    if (!element) {
      return null;
    }
    element._identifier = aName;

    /**
     * @see DebuggerView.Properties._addVar
     */
    element.addVar = this._addVar.bind(this, element);

    /**
     * @see DebuggerView.Properties.addScopeToHierarchy
     */
    element.addToHierarchy = this.addScopeToHierarchy.bind(this, element);

    // Setup the additional elements specific for a scope node.
    element.refresh(function() {
      let title = element.getElementsByClassName("title")[0];
      title.classList.add("devtools-toolbar");
    }.bind(this));

    // Return the element for later use if necessary.
    return element;
  },

  /**
   * Removes all added scopes in the property container tree.
   */
  empty: function DVP_empty() {
    while (this._vars.firstChild) {
      this._vars.removeChild(this._vars.firstChild);
    }
  },

  /**
   * Removes all elements from the variables container, and adds a child node
   * with an empty text note attached.
   */
  emptyText: function DVP_emptyText() {
    // Make sure the container is empty first.
    this.empty();

    let item = document.createElement("label");

    // The empty node should look grayed out to avoid confusion.
    item.className = "list-item empty";
    item.setAttribute("value", L10N.getStr("emptyVariablesText"));

    this._vars.appendChild(item);
  },

  /**
   * Adds a variable to a specified scope.
   * If the optional id is not specified, the variable html node will have a
   * default id set as aScope.id->aName-variable.
   *
   * @param object aScope
   *        The parent scope element.
   * @param string aName
   *        The variable name.
   * @param object aFlags
   *        Optional, contains configurable, enumerable or writable flags.
   * @param string aId
   *        Optional, an id for the variable html node.
   * @return object
   *         The newly created html node representing the added var.
   */
  _addVar: function DVP__addVar(aScope, aName, aFlags, aId) {
    // Make sure the scope container exists.
    if (!aScope) {
      return null;
    }

    // Compute the id of the element if not specified.
    aId = aId || (aScope.id + "->" + aName + "-variable");

    // Contains generic nodes and functionality.
    let element = this._createPropertyElement(aName, aId, "variable",
                                              aScope.getElementsByClassName("details")[0]);

    // Make sure the element was created successfully.
    if (!element) {
      return null;
    }
    element._identifier = aName;

    /**
     * @see DebuggerView.Properties._setGrip
     */
    element.setGrip = this._setGrip.bind(this, element);

    /**
     * @see DebuggerView.Properties._addProperties
     */
    element.addProperties = this._addProperties.bind(this, element);

    // Setup the additional elements specific for a variable node.
    element.refresh(function() {
      let separatorLabel = document.createElement("label");
      let valueLabel = document.createElement("label");
      let title = element.getElementsByClassName("title")[0];

      // Use attribute flags to specify the element type and tooltip text.
      this._setAttributes(element, aName, aFlags);

      // Separator between the variable name and its value.
      separatorLabel.className = "plain";
      separatorLabel.setAttribute("value", ":");

      // The variable information (type, class and/or value).
      valueLabel.className = "value plain";

      // Handle the click event when pressing the element value label.
      valueLabel.addEventListener("click", this._activateElementInputMode.bind({
        scope: this,
        element: element,
        valueLabel: valueLabel
      }));

      // Maintain the symbolic name of the variable.
      Object.defineProperty(element, "token", {
        value: aName,
        writable: false,
        enumerable: true,
        configurable: true
      });

      title.appendChild(separatorLabel);
      title.appendChild(valueLabel);

      // Remember a simple hierarchy between the parent and the element.
      this._saveHierarchy({
        parent: aScope,
        element: element,
        valueLabel: valueLabel
      });
    }.bind(this));

    // Return the element for later use if necessary.
    return element;
  },

  /**
   * Sets a variable's configurable, enumerable or writable attributes.
   *
   * @param object aVar
   *        The object to set the attributes on.
   * @param object aName
   *        The varialbe name.
   * @param object aFlags
   *        Contains configurable, enumerable or writable flags.
   */
  _setAttributes: function DVP_setAttributes(aVar, aName, aFlags) {
    if (aFlags) {
      if (!aFlags.configurable) {
        aVar.setAttribute("non-configurable", "");
      }
      if (!aFlags.enumerable) {
        aVar.setAttribute("non-enumerable", "");
      }
      if (!aFlags.writable) {
        aVar.setAttribute("non-writable", "");
      }
    }
    if (aName === "this") {
      aVar.setAttribute("self", "");
    }
    if (aName === "__proto__ ") {
      aVar.setAttribute("proto", "");
    }
  },

  /**
   * Sets the specific grip for a variable.
   * The grip should contain the value or the type & class, as defined in the
   * remote debugger protocol. For convenience, undefined and null are
   * both considered types.
   *
   * @param object aVar
   *        The parent variable element.
   * @param object aGrip
   *        The primitive or object defining the grip, specifying
   *        the value and/or type & class of the variable (if the type
   *        is not specified, it will be inferred from the value).
   *        e.g. 42
   *             true
   *             "nasu"
   *             { type: "undefined" }
   *             { type: "null" }
   *             { type: "object", class: "Object" }
   * @return object
   *         The same variable.
   */
  _setGrip: function DVP__setGrip(aVar, aGrip) {
    // Make sure the variable container exists.
    if (!aVar) {
      return null;
    }
    if (aGrip === undefined) {
      aGrip = { type: "undefined" };
    }
    if (aGrip === null) {
      aGrip = { type: "null" };
    }

    let valueLabel = aVar.getElementsByClassName("value")[0];

    // Make sure the value node exists.
    if (!valueLabel) {
      return null;
    }

    this._applyGrip(valueLabel, aGrip);
    return aVar;
  },

  /**
   * Applies the necessary text content and class name to a value node based
   * on a grip.
   *
   * @param object aValueLabel
   *        The value node to apply the changes to.
   * @param object aGrip
   *        @see DebuggerView.Properties._setGrip
   */
  _applyGrip: function DVP__applyGrip(aValueLabel, aGrip) {
    let prevGrip = aValueLabel.currentGrip;
    if (prevGrip) {
      aValueLabel.classList.remove(this._propertyColor(prevGrip));
    }

    aValueLabel.setAttribute("value", this._propertyString(aGrip));
    aValueLabel.classList.add(this._propertyColor(aGrip));
    aValueLabel.currentGrip = aGrip;
  },

  /**
   * Adds multiple properties to a specified variable.
   * This function handles two types of properties: data properties and
   * accessor properties, as defined in the remote debugger protocol spec.
   *
   * @param object aVar
   *        The parent variable element.
   * @param object aProperties
   *        An object containing the key: descriptor data properties,
   *        specifying the value and/or type & class of the variable,
   *        or 'get' & 'set' accessor properties.
   *        e.g. { "someProp0": { value: 42 },
   *               "someProp1": { value: true },
   *               "someProp2": { value: "nasu" },
   *               "someProp3": { value: { type: "undefined" } },
   *               "someProp4": { value: { type: "null" } },
   *               "someProp5": { value: { type: "object", class: "Object" } },
   *               "someProp6": { get: { type: "object", class: "Function" },
   *                              set: { type: "undefined" } }
   * @return object
   *         The same variable.
   */
  _addProperties: function DVP__addProperties(aVar, aProperties) {
    // For each property, add it using the passed object key/grip.
    for (let i in aProperties) {
      // Can't use aProperties.hasOwnProperty(i), because it may be overridden.
      if (Object.getOwnPropertyDescriptor(aProperties, i)) {

        // Get the specified descriptor for current property.
        let desc = aProperties[i];

        // As described in the remote debugger protocol, the value grip must be
        // contained in a 'value' property.
        let value = desc["value"];

        // For accessor property descriptors, the two grips need to be
        // contained in 'get' and 'set' properties.
        let getter = desc["get"];
        let setter = desc["set"];

        // Handle data property and accessor property descriptors.
        if (value !== undefined) {
          this._addProperty(aVar, [i, value], desc);
        }
        if (getter !== undefined || setter !== undefined) {
          let prop = this._addProperty(aVar, [i]).expand();
          prop.getter = this._addProperty(prop, ["get", getter], desc);
          prop.setter = this._addProperty(prop, ["set", setter], desc);
        }
      }
    }
    return aVar;
  },

  /**
   * Adds a property to a specified variable.
   * If the optional id is not specified, the property html node will have a
   * default id set as aVar.id->aKey-property.
   *
   * @param object aVar
   *        The parent variable element.
   * @param array aProperty
   *        An array containing the key and grip properties, specifying
   *        the value and/or type & class of the variable (if the type
   *        is not specified, it will be inferred from the value).
   *        e.g. ["someProp0", 42]
   *             ["someProp1", true]
   *             ["someProp2", "nasu"]
   *             ["someProp3", { type: "undefined" }]
   *             ["someProp4", { type: "null" }]
   *             ["someProp5", { type: "object", class: "Object" }]
   * @param object aFlags
   *        Contains configurable, enumerable or writable flags.
   * @param string aName
   *        Optional, the property name.
   * @paarm string aId
   *        Optional, an id for the property html node.
   * @return object
   *         The newly created html node representing the added prop.
   */
  _addProperty: function DVP__addProperty(aVar, aProperty, aFlags, aName, aId) {
    // Make sure the variable container exists.
    if (!aVar) {
      return null;
    }

    // Compute the id of the element if not specified.
    aId = aId || (aVar.id + "->" + aProperty[0] + "-property");

    // Contains generic nodes and functionality.
    let element = this._createPropertyElement(aName, aId, "property",
                                              aVar.getElementsByClassName("details")[0]);

    // Make sure the element was created successfully.
    if (!element) {
      return null;
    }
    element._identifier = aName;

    /**
     * @see DebuggerView.Properties._setGrip
     */
    element.setGrip = this._setGrip.bind(this, element);

    /**
     * @see DebuggerView.Properties._addProperties
     */
    element.addProperties = this._addProperties.bind(this, element);

    // Setup the additional elements specific for a variable node.
    element.refresh(function(pKey, pGrip) {
      let title = element.getElementsByClassName("title")[0];
      let nameLabel = title.getElementsByClassName("name")[0];
      let separatorLabel = document.createElement("label");
      let valueLabel = document.createElement("label");

      // Use attribute flags to specify the element type and tooltip text.
      this._setAttributes(element, pKey, aFlags);

      if ("undefined" !== typeof pKey) {
        // Use a key element to specify the property name.
        nameLabel.className = "key plain";
        nameLabel.setAttribute("value", pKey.trim());
        title.appendChild(nameLabel);
      }
      if ("undefined" !== typeof pGrip) {
        // Separator between the variable name and its value.
        separatorLabel.className = "plain";
        separatorLabel.setAttribute("value", ":");

        // Use a value element to specify the property value.
        valueLabel.className = "value plain";
        this._applyGrip(valueLabel, pGrip);

        title.appendChild(separatorLabel);
        title.appendChild(valueLabel);
      }

      // Handle the click event when pressing the element value label.
      valueLabel.addEventListener("click", this._activateElementInputMode.bind({
        scope: this,
        element: element,
        valueLabel: valueLabel
      }));

      // Maintain the symbolic name of the property.
      Object.defineProperty(element, "token", {
        value: aVar.token + "['" + pKey + "']",
        writable: false,
        enumerable: true,
        configurable: true
      });

      // Remember a simple hierarchy between the parent and the element.
      this._saveHierarchy({
        parent: aVar,
        element: element,
        valueLabel: valueLabel
      });

      // Save the property to the variable for easier access.
      Object.defineProperty(aVar, pKey, { value: element,
                                          writable: false,
                                          enumerable: true,
                                          configurable: true });
    }.bind(this), aProperty);

    // Return the element for later use if necessary.
    return element;
  },

  /**
   * Makes an element's (variable or priperty) value editable.
   * Make sure 'this' is bound to an object containing the properties:
   * {
   *   "scope": the original scope to be used, probably DebuggerView.Properties,
   *   "element": the element whose value should be made editable,
   *   "valueLabel": the label displaying the value
   * }
   *
   * @param event aEvent [optional]
   *        The event requesting this action.
   */
  _activateElementInputMode: function DVP__activateElementInputMode(aEvent) {
    if (aEvent) {
      aEvent.stopPropagation();
    }

    let self = this.scope;
    let element = this.element;
    let valueLabel = this.valueLabel;
    let titleNode = valueLabel.parentNode;
    let initialValue = valueLabel.getAttribute("value");

    // When editing an object we need to collapse it first, in order to avoid
    // displaying an inconsistent state while the user is editing.
    element._previouslyExpanded = element.expanded;
    element._preventExpand = true;
    element.collapse();
    element.forceHideArrow();

    // Create a texbox input element which will be shown in the current
    // element's value location.
    let textbox = document.createElement("textbox");
    textbox.setAttribute("value", initialValue);
    textbox.className = "element-input";
    textbox.width = valueLabel.clientWidth + 1;

    // Save the new value when the texbox looses focus or ENTER is pressed.
    function DVP_element_textbox_blur(aTextboxEvent) {
      DVP_element_textbox_save();
    }

    function DVP_element_textbox_keyup(aTextboxEvent) {
      if (aTextboxEvent.keyCode === aTextboxEvent.DOM_VK_LEFT ||
          aTextboxEvent.keyCode === aTextboxEvent.DOM_VK_RIGHT ||
          aTextboxEvent.keyCode === aTextboxEvent.DOM_VK_UP ||
          aTextboxEvent.keyCode === aTextboxEvent.DOM_VK_DOWN) {
        return;
      }
      if (aTextboxEvent.keyCode === aTextboxEvent.DOM_VK_RETURN ||
          aTextboxEvent.keyCode === aTextboxEvent.DOM_VK_ENTER) {
        DVP_element_textbox_save();
        return;
      }
      if (aTextboxEvent.keyCode === aTextboxEvent.DOM_VK_ESCAPE) {
        valueLabel.setAttribute("value", initialValue);
        DVP_element_textbox_clear();
        return;
      }
    }

    // The actual save mechanism for the new variable/property value.
    function DVP_element_textbox_save() {
      if (textbox.value !== valueLabel.getAttribute("value")) {
        valueLabel.setAttribute("value", textbox.value);

        let expr = "(" + element.token + "=" + textbox.value + ")";
        DebuggerController.StackFrames.evaluate(expr);
      }
      DVP_element_textbox_clear();
    }

    // Removes the event listeners and appends the value node again.
    function DVP_element_textbox_clear() {
      element._preventExpand = false;
      if (element._previouslyExpanded) {
        element._previouslyExpanded = false;
        element.expand();
      }
      element.showArrow();

      textbox.removeEventListener("blur", DVP_element_textbox_blur, false);
      textbox.removeEventListener("keyup", DVP_element_textbox_keyup, false);
      titleNode.removeChild(textbox);
      titleNode.appendChild(valueLabel);
    }

    textbox.addEventListener("blur", DVP_element_textbox_blur, false);
    textbox.addEventListener("keyup", DVP_element_textbox_keyup, false);
    titleNode.removeChild(valueLabel);
    titleNode.appendChild(textbox);

    textbox.select();

    // When the value is a string (displayed as "value"), then we probably want
    // to change it to another string in the textbox, so to avoid typing the ""
    // again, tackle with the selection bounds just a bit.
    if (valueLabel.getAttribute("value").match(/^"[^"]*"$/)) {
      textbox.selectionEnd--;
      textbox.selectionStart++;
    }
  },

  /**
   * Returns a custom formatted property string for a type and a value.
   *
   * @param string | object aGrip
   *        The variable grip.
   * @return string
   *         The formatted property string.
   */
  _propertyString: function DVP__propertyString(aGrip) {
    if (aGrip && "object" === typeof aGrip) {
      switch (aGrip.type) {
        case "undefined":
          return "undefined";
        case "null":
          return "null";
        default:
          return "[" + aGrip.type + " " + aGrip.class + "]";
      }
    } else {
      switch (typeof aGrip) {
        case "string":
          return "\"" + aGrip + "\"";
        case "boolean":
          return aGrip ? "true" : "false";
        default:
          return aGrip + "";
      }
    }
    return aGrip + "";
  },

  /**
   * Returns a custom class style for a type and a value.
   *
   * @param string | object aGrip
   *        The variable grip.
   *
   * @return string
   *         The css class style.
   */
  _propertyColor: function DVP__propertyColor(aGrip) {
    if (aGrip && "object" === typeof aGrip) {
      switch (aGrip.type) {
        case "undefined":
          return "token-undefined";
        case "null":
          return "token-null";
      }
    } else {
      switch (typeof aGrip) {
        case "string":
          return "token-string";
        case "boolean":
          return "token-boolean";
        case "number":
          return "token-number";
      }
    }
    return "token-other";
  },

  /**
   * Creates an element which contains generic nodes and functionality used by
   * any scope, variable or property added to the tree.
   * If the variable or property already exists, null is returned.
   * Otherwise, the newly created element is returned.
   *
   * @param string aName
   *        A generic name used in a title strip.
   * @param string aId
   *        id used by the created element node.
   * @param string aClass
   *        Recommended style class used by the created element node.
   * @param object aParent
   *        The parent node which will contain the element.
   * @return object
   *         The newly created html node representing the generic elem.
   */
  _createPropertyElement: function DVP__createPropertyElement(aName, aId, aClass, aParent) {
    // Make sure we don't duplicate anything and the parent exists.
    if (document.getElementById(aId)) {
      return null;
    }
    if (!aParent) {
      return null;
    }

    let element = document.createElement("vbox");
    let arrow = document.createElement("box");
    let name = document.createElement("label");

    let title = document.createElement("box");
    let details = document.createElement("vbox");

    // Create a scope node to contain all the elements.
    element.id = aId;
    element.className = aClass;

    // The expand/collapse arrow.
    arrow.className = "arrow";
    arrow.style.visibility = "hidden";

    // The name element.
    name.className = "name plain";
    name.setAttribute("value", aName || "");

    // The title element, containing the arrow and the name.
    title.className = "title";
    title.setAttribute("align", "center")

    // The node element which will contain any added scope variables.
    details.className = "details";

    // Add the click event handler for the title, or arrow and name.
    if (aClass === "scope") {
      title.addEventListener("click", function() element.toggle(), false);
    } else {
      arrow.addEventListener("click", function() element.toggle(), false);
      name.addEventListener("click", function() element.toggle(), false);
      name.addEventListener("mouseover", function() element.updateTooltip(name), false);
    }

    title.appendChild(arrow);
    title.appendChild(name);

    element.appendChild(title);
    element.appendChild(details);

    aParent.appendChild(element);

    /**
     * Shows the element, setting the display style to "block".
     * @return object
     *         The same element.
     */
    element.show = function DVP_element_show() {
      element.style.display = "-moz-box";

      if ("function" === typeof element.onshow) {
        element.onshow(element);
      }
      return element;
    };

    /**
     * Hides the element, setting the display style to "none".
     * @return object
     *         The same element.
     */
    element.hide = function DVP_element_hide() {
      element.style.display = "none";

      if ("function" === typeof element.onhide) {
        element.onhide(element);
      }
      return element;
    };

    /**
     * Expands the element, showing all the added details.
     *
     * @param boolean aSkipAnimationFlag
     *        Pass true to not show an opening animation.
     * @return object
     *         The same element.
     */
    element.expand = function DVP_element_expand(aSkipAnimationFlag) {
      if (element._preventExpand) {
        return;
      }
      arrow.setAttribute("open", "");
      details.setAttribute("open", "");

      if (!aSkipAnimationFlag) {
        details.setAttribute("animated", "");
      }
      if ("function" === typeof element.onexpand) {
        element.onexpand(element);
      }
      return element;
    };

    /**
     * Collapses the element, hiding all the added details.
     * @return object
     *         The same element.
     */
    element.collapse = function DVP_element_collapse() {
      if (element._preventCollapse) {
        return;
      }
      arrow.removeAttribute("open");
      details.removeAttribute("open");
      details.removeAttribute("animated");

      if ("function" === typeof element.oncollapse) {
        element.oncollapse(element);
      }
      return element;
    };

    /**
     * Toggles between the element collapse/expand state.
     * @return object
     *         The same element.
     */
    element.toggle = function DVP_element_toggle() {
      element.expanded = !element.expanded;

      if ("function" === typeof element.ontoggle) {
        element.ontoggle(element);
      }
      return element;
    };

    /**
     * Shows the element expand/collapse arrow (only if necessary!).
     * @return object
     *         The same element.
     */
    element.showArrow = function DVP_element_showArrow() {
      if (element._forceShowArrow || details.childNodes.length) {
        arrow.style.visibility = "visible";
      }
      return element;
    };

    /**
     * Hides the element expand/collapse arrow.
     * @return object
     *         The same element.
     */
    element.hideArrow = function DVP_element_hideArrow() {
      if (!element._forceShowArrow) {
        arrow.style.visibility = "hidden";
      }
      return element;
    };

    /**
     * Forces the element expand/collapse arrow to be visible, even if there
     * are no child elements.
     *
     * @return object
     *         The same element.
     */
    element.forceShowArrow = function DVP_element_forceShowArrow() {
      element._forceShowArrow = true;
      arrow.style.visibility = "visible";
      return element;
    };

    /**
     * Forces the element expand/collapse arrow to be hidden, even if there
     * are some child elements.
     *
     * @return object
     *         The same element.
     */
    element.forceHideArrow = function DVP_element_forceHideArrow() {
      arrow.style.visibility = "hidden";
      return element;
    };

    /**
     * Returns if the element is visible.
     * @return boolean
     *         True if the element is visible.
     */
    Object.defineProperty(element, "visible", {
      get: function DVP_element_getVisible() {
        return element.style.display !== "none";
      },
      set: function DVP_element_setVisible(value) {
        if (value) {
          element.show();
        } else {
          element.hide();
        }
      }
    });

    /**
     * Returns if the element is expanded.
     * @return boolean
     *         True if the element is expanded.
     */
    Object.defineProperty(element, "expanded", {
      get: function DVP_element_getExpanded() {
        return arrow.hasAttribute("open");
      },
      set: function DVP_element_setExpanded(value) {
        if (value) {
          element.expand();
        } else {
          element.collapse();
        }
      }
    });

    /**
     * Removes all added children in the details container tree.
     * @return object
     *         The same element.
     */
    element.empty = function DVP_element_empty() {
      // This details node won't have any elements, so hide the arrow.
      arrow.style.visibility = "hidden";
      while (details.firstChild) {
        details.removeChild(details.firstChild);
      }

      if ("function" === typeof element.onempty) {
        element.onempty(element);
      }
      return element;
    };

    /**
     * Removes the element from the parent node details container tree.
     * @return object
     *         The same element.
     */
    element.remove = function DVP_element_remove() {
      element.parentNode.removeChild(element);

      if ("function" === typeof element.onremove) {
        element.onremove(element);
      }
      return element;
    };

    /**
     * Returns if the element expander (arrow) is visible.
     * @return boolean
     *         True if the arrow is visible.
     */
    Object.defineProperty(element, "arrowVisible", {
      get: function DVP_element_getArrowVisible() {
        return arrow.style.visibility !== "hidden";
      },
      set: function DVP_element_setExpanded(value) {
        if (value) {
          element.showArrow();
        } else {
          element.hideArrow();
        }
      }
    });

    /**
     * Creates a tooltip for the element displaying certain attributes.
     *
     * @param object aAnchor
     *        The element which will anchor the tooltip.
     */
    element.updateTooltip = function DVP_element_updateTooltip(aAnchor) {
      let tooltip = document.getElementById("element-tooltip");
      if (tooltip) {
        document.documentElement.removeChild(tooltip);
      }

      tooltip = document.createElement("tooltip");
      tooltip.id = "element-tooltip";

      let configurableLabel = document.createElement("label");
      configurableLabel.id = "configurableLabel";
      configurableLabel.setAttribute("value", "configurable");

      let enumerableLabel = document.createElement("label");
      enumerableLabel.id = "enumerableLabel";
      enumerableLabel.setAttribute("value", "enumerable");

      let writableLabel = document.createElement("label");
      writableLabel.id = "writableLabel";
      writableLabel.setAttribute("value", "writable");

      tooltip.setAttribute("orient", "horizontal")
      tooltip.appendChild(configurableLabel);
      tooltip.appendChild(enumerableLabel);
      tooltip.appendChild(writableLabel);

      if (element.hasAttribute("non-configurable")) {
        configurableLabel.setAttribute("non-configurable", "");
      }
      if (element.hasAttribute("non-enumerable")) {
        enumerableLabel.setAttribute("non-enumerable", "");
      }
      if (element.hasAttribute("non-writable")) {
        writableLabel.setAttribute("non-writable", "");
      }

      document.documentElement.appendChild(tooltip);
      aAnchor.setAttribute("tooltip", tooltip.id);
    };

    /**
     * Generic function refreshing the internal state of the element when
     * it's modified (e.g. a child detail, variable, property is added).
     *
     * @param function aFunction
     *        The function logic used to modify the internal state.
     * @param array aArguments
     *        Optional arguments array to be applied to aFunction.
     */
    element.refresh = function DVP_element_refresh(aFunction, aArguments) {
      if ("function" === typeof aFunction) {
        aFunction.apply(this, aArguments);
      }

      let node = aParent.parentNode;
      let arrow = node.getElementsByClassName("arrow")[0];
      let children = node.getElementsByClassName("details")[0].childNodes.length;

      // If the parent details node has at least one element, set the
      // expand/collapse arrow visible.
      if (children) {
        arrow.style.visibility = "visible";
      } else {
        arrow.style.visibility = "hidden";
      }
    }.bind(this);

    // Return the element for later use and customization.
    return element;
  },

  /**
   * Remember a simple hierarchy of parent->element->children.
   *
   * @param object aProperties
   *        Container for the parent, element and the associated value node.
   */
  _saveHierarchy: function DVP__saveHierarchy(aProperties) {
    let parent = aProperties.parent;
    let element = aProperties.element;
    let valueLabel = aProperties.valueLabel;
    let store = aProperties.store || parent._children;

    // Make sure we have a valid element and a children storage object.
    if (!element || !store) {
      return;
    }

    let relation = {
      root: parent ? (parent._root || parent) : null,
      parent: parent || null,
      element: element,
      valueLabel: valueLabel,
      children: {}
    };

    store[element._identifier] = relation;
    element._root = relation.root;
    element._children = relation.children;
  },

  /**
   * Creates an object to store a hierarchy of scopes, variables and properties
   * and saves the previous store.
   */
  createHierarchyStore: function DVP_createHierarchyStore() {
    this._prevHierarchy = this._currHierarchy;
    this._currHierarchy = {};
  },

  /**
   * Creates a hierarchy holder for a scope.
   *
   * @param object aScope
   *        The designated scope to track.
   */
  addScopeToHierarchy: function DVP_addScopeToHierarchy(aScope) {
    this._saveHierarchy({ element: aScope, store: this._currHierarchy });
  },

  /**
   * Briefly flash the variables that changed between pauses.
   */
  commitHierarchy: function DVS_commitHierarchy() {
    for (let i in this._currHierarchy) {
      let currScope = this._currHierarchy[i];
      let prevScope = this._prevHierarchy[i];

      if (!prevScope) {
        continue;
      }

      for (let v in currScope.children) {
        let currVar = currScope.children[v];
        let prevVar = prevScope.children[v];

        let action = "";

        if (prevVar) {
          let prevValue = prevVar.valueLabel.getAttribute("value");
          let currValue = currVar.valueLabel.getAttribute("value");

          if (currValue != prevValue) {
            action = "changed";
          } else {
            action = "unchanged";
          }
        } else {
          action = "added";
        }

        if (action) {
          currVar.element.setAttribute(action, "");

          window.setTimeout(function() {
           currVar.element.removeAttribute(action);
          }, PROPERTY_VIEW_FLASH_DURATION);
        }
      }
    }
  },

  /**
   * A simple model representation of all the scopes, variables and properties,
   * with parent-child relations.
   */
  _currHierarchy: null,
  _prevHierarchy: null,

  /**
   * The cached variable properties container.
   */
  _vars: null,

  /**
   * Initialization function, called when the debugger is initialized.
   */
  initialize: function DVP_initialize() {
    this._vars = DebuggerView._variables;

    this.emptyText();
    this.createHierarchyStore();
  },

  /**
   * Destruction function, called when the debugger is shut down.
   */
  destroy: function DVP_destroy() {
    this.empty();

    this._currHierarchy = null;
    this._prevHierarchy = null;
    this._vars = null;
  }
};

/**
 * Preliminary setup for the DebuggerView object.
 */
DebuggerView.GlobalSearch = new GlobalSearchView();
DebuggerView.Scripts = new ScriptsView();
DebuggerView.StackFrames = new StackFramesView();
DebuggerView.Breakpoints = new BreakpointsView();
DebuggerView.Properties = new PropertiesView();

/**
 * Export the source editor to the global scope for easier access in tests.
 */
Object.defineProperty(window, "editor", {
  get: function() { return DebuggerView.editor; }
});
