/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * Functions handling the toolbar view: close button, expand/collapse button,
 * pause/resume and stepping buttons etc.
 */
function ToolbarView() {
  dumpn("ToolbarView was instantiated");
  this._onCloseClick = this._onCloseClick.bind(this);
  this._onTogglePanesPressed = this._onTogglePanesPressed.bind(this);
  this._onResumePressed = this._onResumePressed.bind(this);
  this._onStepOverPressed = this._onStepOverPressed.bind(this);
  this._onStepInPressed = this._onStepInPressed.bind(this);
  this._onStepOutPressed = this._onStepOutPressed.bind(this);
}

ToolbarView.prototype = {
  /**
   * Initialization function, called when the debugger is started.
   */
  initialize: function DVT_initialize() {
    dumpn("Initializing the ToolbarView");
    this._closeButton = document.getElementById("close");
    this._togglePanesButton = document.getElementById("toggle-panes");
    this._resumeButton = document.getElementById("resume");
    this._stepOverButton = document.getElementById("step-over");
    this._stepInButton = document.getElementById("step-in");
    this._stepOutButton = document.getElementById("step-out");
    this._chromeGlobals = document.getElementById("chrome-globals");
    this._scripts = document.getElementById("sources");

    let resumeKey = LayoutHelpers.prettyKey(document.getElementById("resumeKey"));
    let stepOverKey = LayoutHelpers.prettyKey(document.getElementById("stepOverKey"));
    let stepInKey = LayoutHelpers.prettyKey(document.getElementById("stepInKey"));
    let stepOutKey = LayoutHelpers.prettyKey(document.getElementById("stepOutKey"));
    this._resumeTooltip = L10N.getFormatStr("resumeButtonTooltip", [resumeKey]);
    this._pauseTooltip = L10N.getFormatStr("pauseButtonTooltip", [resumeKey]);
    this._stepOverTooltip = L10N.getFormatStr("stepOverTooltip", [stepOverKey]);
    this._stepInTooltip = L10N.getFormatStr("stepInTooltip", [stepInKey]);
    this._stepOutTooltip = L10N.getFormatStr("stepOutTooltip", [stepOutKey]);

    this._closeButton.addEventListener("click", this._onCloseClick, false);
    this._togglePanesButton.addEventListener("mousedown", this._onTogglePanesPressed, false);
    this._resumeButton.addEventListener("mousedown", this._onResumePressed, false);
    this._stepOverButton.addEventListener("mousedown", this._onStepOverPressed, false);
    this._stepInButton.addEventListener("mousedown", this._onStepInPressed, false);
    this._stepOutButton.addEventListener("mousedown", this._onStepOutPressed, false);

    this._stepOverButton.setAttribute("tooltiptext", this._stepOverTooltip);
    this._stepInButton.setAttribute("tooltiptext", this._stepInTooltip);
    this._stepOutButton.setAttribute("tooltiptext", this._stepOutTooltip);

    this.toggleCloseButton(!window._isRemoteDebugger && !window._isChromeDebugger);
    this.toggleChromeGlobalsContainer(window._isChromeDebugger);
  },

  /**
   * Destruction function, called when the debugger is closed.
   */
  destroy: function DVT_destroy() {
    dumpn("Destroying the ToolbarView");
    this._closeButton.removeEventListener("click", this._onCloseClick, false);
    this._togglePanesButton.removeEventListener("mousedown", this._onTogglePanesPressed, false);
    this._resumeButton.removeEventListener("mousedown", this._onResumePressed, false);
    this._stepOverButton.removeEventListener("mousedown", this._onStepOverPressed, false);
    this._stepInButton.removeEventListener("mousedown", this._onStepInPressed, false);
    this._stepOutButton.removeEventListener("mousedown", this._onStepOutPressed, false);
  },

  /**
   * Sets the close button hidden or visible. It's hidden by default.
   *
   * @param boolean aVisibleFlag
   *        Specifies the intended visibility.
   */
  toggleCloseButton: function DVT_toggleCloseButton(aVisibleFlag) {
    this._closeButton.setAttribute("hidden", !aVisibleFlag);
  },

  /**
   * Sets the resume button state based on the debugger active thread.
   *
   * @param string aState
   *        Either "paused" or "attached".
   */
  toggleResumeButtonState: function DVT_toggleResumeButtonState(aState) {
    // If we're paused, check and show a resume label on the button.
    if (aState == "paused") {
      this._resumeButton.setAttribute("checked", "true");
      this._resumeButton.setAttribute("tooltiptext", this._resumeTooltip);
    }
    // If we're attached, do the opposite.
    else if (aState == "attached") {
      this._resumeButton.removeAttribute("checked");
      this._resumeButton.setAttribute("tooltiptext", this._pauseTooltip);
    }
  },

  /**
   * Sets the chrome globals container hidden or visible. It's hidden by default.
   *
   * @param boolean aVisibleFlag
   *        Specifies the intended visibility.
   */
  toggleChromeGlobalsContainer: function DVT_toggleChromeGlobalsContainer(aVisibleFlag) {
    this._chromeGlobals.setAttribute("hidden", !aVisibleFlag);
  },

  /**
   * Sets the sources container hidden or visible. It's visible by default.
   *
   * @param boolean aVisibleFlag
   *        Specifies the intended visibility.
   */
  toggleSourcesContainer: function DVT_toggleSourcesContainer(aVisibleFlag) {
    this._sources.setAttribute("hidden", !aVisibleFlag);
  },

  /**
   * Listener handling the close button click event.
   */
  _onCloseClick: function DVT__onCloseClick() {
    DebuggerController._shutdownDebugger();
  },

  /**
   * Listener handling the toggle button click event.
   */
  _onTogglePanesPressed: function DVT__onTogglePanesPressed() {
    DebuggerView.togglePanes({
      visible: DebuggerView.panesHidden,
      animated: true,
      silent: false
    });
  },

  /**
   * Listener handling the pause/resume button click event.
   */
  _onResumePressed: function DVT__onResumePressed() {
    if (DebuggerController.activeThread.paused) {
      DebuggerController.activeThread.resume();
    } else {
      DebuggerController.activeThread.interrupt();
    }
  },

  /**
   * Listener handling the step over button click event.
   */
  _onStepOverPressed: function DVT__onStepOverPressed() {
    if (DebuggerController.activeThread.paused) {
      DebuggerController.activeThread.stepOver();
    }
  },

  /**
   * Listener handling the step in button click event.
   */
  _onStepInPressed: function DVT__onStepInPressed() {
    if (DebuggerController.activeThread.paused) {
      DebuggerController.activeThread.stepIn();
    }
  },

  /**
   * Listener handling the step out button click event.
   */
  _onStepOutPressed: function DVT__onStepOutPressed() {
    if (DebuggerController.activeThread.paused) {
      DebuggerController.activeThread.stepOut();
    }
  },

  _closeButton: null,
  _togglePanesButton: null,
  _resumeButton: null,
  _stepOverButton: null,
  _stepInButton: null,
  _stepOutButton: null,
  _chromeGlobals: null,
  _sources: null,
  _resumeTooltip: "",
  _pauseTooltip: "",
  _stepOverTooltip: "",
  _stepInTooltip: "",
  _stepOutTooltip: ""
};

/**
 * Functions handling the options UI.
 */
function OptionsView() {
  dumpn("OptionsView was instantiated");
  this._onPoeClick = this._onPoeClick.bind(this);
  this._onShowNonenumClick = this._onShowNonenumClick.bind(this);
}

OptionsView.prototype = {
  /**
   * Initialization function, called when the debugger is started.
   */
  initialize: function DVO_initialize() {
    dumpn("Initializing the OptionsView");
    this._poeCheckbox = document.getElementById("pause-on-exceptions");
    this._showNonenumCheckbox = document.getElementById("show-nonenum");

    this._poeCheckbox.addEventListener("click", this._onPoeClick, false);
    this._showNonenumCheckbox.addEventListener("click", this._onShowNonenumClick, false);

    this._poeCheckbox.checked = false; // Never pause on exceptions by default.
    this._showNonenumCheckbox.checked = Prefs.nonEnumVisible;
  },

  /**
   * Destruction function, called when the debugger is closed.
   */
  destroy: function DVO_destroy() {
    dumpn("Destroying the OptionsView");
    this._poeCheckbox.removeEventListener("click", this._onPoeClick, false);
    this._showNonenumCheckbox.removeEventListener("click", this._onShowNonenumClick, false);
  },

  /**
   * Listener handling the 'pause on exceptions' checkbox click event.
   */
  _onPoeClick: function DVO__onPOEClick() {
    DebuggerController.activeThread.pauseOnExceptions(this._poeCheckbox.checked);
  },

  /**
   * Listener handling the 'show non-enumerables' checkbox click event.
   */
  _onShowNonenumClick: function DVO__onShowNonenumClick() {
    DebuggerView.Variables.nonEnumVisible =
      Prefs.nonEnumVisible = this._showNonenumCheckbox.checked;
  },

  _poeCheckbox: null,
  _showNonenumCheckbox: null
};

/**
 * Functions handling the chrome globals UI.
 */
function ChromeGlobalsView() {
  dumpn("ChromeGlobalsView was instantiated");
  MenuContainer.call(this);
  this._onSelect = this._onSelect.bind(this);
  this._onClick = this._onClick.bind(this);
}

create({ constructor: ChromeGlobalsView, proto: MenuContainer.prototype }, {
  /**
   * Initialization function, called when the debugger is started.
   */
  initialize: function DVCG_initialize() {
    dumpn("Initializing the ChromeGlobalsView");
    this._container = document.getElementById("chrome-globals");
    this._emptyLabel = L10N.getStr("noGlobalsText");
    this._unavailableLabel = L10N.getStr("noMatchingGlobalsText");

    this._container.addEventListener("select", this._onSelect, false);
    this._container.addEventListener("click", this._onClick, false);

    this.empty();
  },

  /**
   * Destruction function, called when the debugger is closed.
   */
  destroy: function DVT_destroy() {
    dumpn("Destroying the ChromeGlobalsView");
    this._container.removeEventListener("select", this._onSelect, false);
    this._container.removeEventListener("click", this._onClick, false);
  },

  /**
   * The select listener for the chrome globals container.
   */
  _onSelect: function DVCG__onSelect() {
    if (!this.refresh()) {
      return;
    }
    // TODO: Do something useful for chrome debugging.
  },

  /**
   * The click listener for the chrome globals container.
   */
  _onClick: function DVCG__onClick() {
    DebuggerView.Filtering.target = this;
  }
});

/**
 * Functions handling the sources UI.
 */
function SourcesView() {
  dumpn("SourcesView was instantiated");
  MenuContainer.call(this);
  this._onSelect = this._onSelect.bind(this);
  this._onClick = this._onClick.bind(this);
}

create({ constructor: SourcesView, proto: MenuContainer.prototype }, {
  /**
   * Initialization function, called when the debugger is started.
   */
  initialize: function DVS_initialize() {
    dumpn("Initializing the SourcesView");
    this._container = document.getElementById("sources");
    this._emptyLabel = L10N.getStr("noScriptsText");
    this._unavailableLabel = L10N.getStr("noMatchingScriptsText");

    this._container.addEventListener("select", this._onSelect, false);
    this._container.addEventListener("click", this._onClick, false);

    this.empty();
  },

  /**
   * Destruction function, called when the debugger is closed.
   */
  destroy: function DVS_destroy() {
    dumpn("Destroying the SourcesView");
    this._container.removeEventListener("select", this._onSelect, false);
    this._container.removeEventListener("click", this._onClick, false);
  },

  /**
   * The select listener for the sources container.
   */
  _onSelect: function DVS__onSelect() {
    if (!this.refresh()) {
      return;
    }
    DebuggerView.setEditorSource(this.selectedItem.attachment);
  },

  /**
   * The click listener for the sources container.
   */
  _onClick: function DVS__onClick() {
    DebuggerView.Filtering.target = this;
  }
});

/**
 * Utility functions for handling sources.
 */
let SourceUtils = {
  _labelsCache: new Map(),

  /**
   * Gets a unique, simplified label from a source url.
   *
   * @param string aUrl
   *        The source url.
   * @return string
   *         The simplified label.
   */
  getSourceLabel: function SU_getSourceLabel(aUrl) {
    if (!this._labelsCache.has(aUrl)) {
      this._labelsCache.set(aUrl, this.trimUrlLength(this.trimUrl(aUrl)));
    }
    return this._labelsCache.get(aUrl);
  },

  /**
   * Clears the labels cache, populated by SourceUtils.getSourceLabel.
   * This should be done every time the content location changes.
   */
  clearLabelsCache: function SU_clearLabelsCache() {
    this._labelsCache = new Map();
  },

  /**
   * Trims the url by shortening it if it exceeds a certain length, adding an
   * ellipsis at the end.
   *
   * @param string aUrl
   *        The source url.
   * @param number aMaxLength [optional]
   *        The max source url length.
   * @return string
   *         The shortened url.
   */
  trimUrlLength: function SU_trimUrlLength(aUrl, aMaxLength = SOURCE_URL_MAX_LENGTH) {
    if (aUrl.length > aMaxLength) {
      return aUrl.substring(0, aMaxLength) + L10N.ellipsis;
    }
    return aUrl;
  },

  /**
   * Trims the query part or reference identifier of a url string, if necessary.
   *
   * @param string aUrl
   *        The source url.
   * @return string
   *         The shortened url.
   */
  trimUrlQuery: function SU_trimUrlQuery(aUrl) {
    let length = aUrl.length;
    let q1 = aUrl.indexOf('?');
    let q2 = aUrl.indexOf('&');
    let q3 = aUrl.indexOf('#');
    let q = Math.min(q1 != -1 ? q1 : length,
                     q2 != -1 ? q2 : length,
                     q3 != -1 ? q3 : length);

    return aUrl.slice(0, q);
  },

  /**
   * Trims as much as possible from a url, while keeping the result unique
   * in the sources container.
   *
   * @param string | nsIURL aUrl
   *        The script url.
   * @param string aLabel [optional]
   *        The resulting label at each step.
   * @param number aSeq [optional]
   *        The current iteration step.
   * @return string
   *         The resulting label at the final step.
   */
  trimUrl: function SU_trimUrl(aUrl, aLabel, aSeq) {
    if (!(aUrl instanceof Ci.nsIURL)) {
      try {
        // Use an nsIURL to parse all the url path parts.
        aUrl = Services.io.newURI(aUrl, null, null).QueryInterface(Ci.nsIURL);
      } catch (e) {
        // This doesn't look like a url, or nsIURL can't handle it.
        return aUrl;
      }
    }
    if (!aSeq) {
      let name = aUrl.fileName;
      if (name) {
        // This is a regular file url, get only the file name (contains the
        // base name and extension if available).

        // If this url contains an invalid query, unfortunately nsIURL thinks
        // it's part of the file extension. It must be removed.
        aLabel = aUrl.fileName.replace(/\&.*/, "");
      } else {
        // This is not a file url, hence there is no base name, nor extension.
        // Proceed using other available information.
        aLabel = "";
      }
      aSeq = 1;
    }

    // If we have a label and it doesn't start with a query...
    if (aLabel && aLabel.indexOf("?") != 0) {
      if (DebuggerView.Sources.containsTrimmedValue(aUrl.spec)) {
        // A page may contain multiple requests to the same url but with different
        // queries. It would be redundant to show each one.
        return aLabel;
      }
      if (!DebuggerView.Sources.containsLabel(aLabel)) {
        // We found the shortest unique label for the url.
        return aLabel;
      }
    }

    // Append the url query.
    if (aSeq == 1) {
      let query = aUrl.query;
      if (query) {
        return this.trimUrl(aUrl, aLabel + "?" + query, aSeq + 1);
      }
      aSeq++;
    }
    // Append the url reference.
    if (aSeq == 2) {
      let ref = aUrl.ref;
      if (ref) {
        return this.trimUrl(aUrl, aLabel + "#" + aUrl.ref, aSeq + 1);
      }
      aSeq++;
    }
    // Prepend the url directory.
    if (aSeq == 3) {
      let dir = aUrl.directory;
      if (dir) {
        return this.trimUrl(aUrl, dir.replace(/^\//, "") + aLabel, aSeq + 1);
      }
      aSeq++;
    }
    // Prepend the hostname and port number.
    if (aSeq == 4) {
      let host = aUrl.hostPort;
      if (host) {
        return this.trimUrl(aUrl, host + "/" + aLabel, aSeq + 1);
      }
      aSeq++;
    }
    // Use the whole url spec but ignoring the reference.
    if (aSeq == 5) {
      return this.trimUrl(aUrl, aUrl.specIgnoringRef, aSeq + 1);
    }
    // Give up.
    return aUrl.spec;
  }
};

/**
 * Functions handling the filtering UI.
 */
function FilterView() {
  dumpn("FilterView was instantiated");
  this._onClick = this._onClick.bind(this);
  this._onSearch = this._onSearch.bind(this);
  this._onKeyPress = this._onKeyPress.bind(this);
  this._onBlur = this._onBlur.bind(this);
}

FilterView.prototype = {
  /**
   * Initialization function, called when the debugger is started.
   */
  initialize: function DVF_initialize() {
    dumpn("Initializing the FilterView");
    this._searchbox = document.getElementById("searchbox");
    this._searchboxPanel = document.getElementById("searchbox-panel");
    this._globalOperatorButton = document.getElementById("global-operator-button");
    this._globalOperatorLabel = document.getElementById("global-operator-label");
    this._tokenOperatorButton = document.getElementById("token-operator-button");
    this._tokenOperatorLabel = document.getElementById("token-operator-label");
    this._lineOperatorButton = document.getElementById("line-operator-button");
    this._lineOperatorLabel = document.getElementById("line-operator-label");

    this._globalSearchKey = LayoutHelpers.prettyKey(document.getElementById("globalSearchKey"));
    this._fileSearchKey = LayoutHelpers.prettyKey(document.getElementById("fileSearchKey"));
    this._lineSearchKey = LayoutHelpers.prettyKey(document.getElementById("lineSearchKey"));
    this._tokenSearchKey = LayoutHelpers.prettyKey(document.getElementById("tokenSearchKey"));

    this._searchbox.addEventListener("click", this._onClick, false);
    this._searchbox.addEventListener("select", this._onSearch, false);
    this._searchbox.addEventListener("input", this._onSearch, false);
    this._searchbox.addEventListener("keypress", this._onKeyPress, false);
    this._searchbox.addEventListener("blur", this._onBlur, false);

    this._globalOperatorButton.setAttribute("label", SEARCH_GLOBAL_FLAG);
    this._tokenOperatorButton.setAttribute("label", SEARCH_TOKEN_FLAG);
    this._lineOperatorButton.setAttribute("label", SEARCH_LINE_FLAG);

    this._globalOperatorLabel.setAttribute("value",
      L10N.getFormatStr("searchPanelGlobal", [this._globalSearchKey]));
    this._tokenOperatorLabel.setAttribute("value",
      L10N.getFormatStr("searchPanelToken", [this._tokenSearchKey]));
    this._lineOperatorLabel.setAttribute("value",
      L10N.getFormatStr("searchPanelLine", [this._lineSearchKey]));

    if (window._isChromeDebugger) {
      this.target = DebuggerView.ChromeGlobals;
    } else {
      this.target = DebuggerView.Sources;
    }
  },

  /**
   * Destruction function, called when the debugger is closed.
   */
  destroy: function DVF_destroy() {
    dumpn("Destroying the FilterView");
    this._searchbox.removeEventListener("click", this._onClick, false);
    this._searchbox.removeEventListener("select", this._onSearch, false);
    this._searchbox.removeEventListener("input", this._onSearch, false);
    this._searchbox.removeEventListener("keypress", this._onKeyPress, false);
    this._searchbox.removeEventListener("blur", this._onBlur, false);
  },

  /**
   * Sets the target container to be currently filtered.
   * @param object aView
   */
  set target(aView) {
    var placeholder = "";
    switch (aView) {
      case DebuggerView.ChromeGlobals:
        placeholder = L10N.getFormatStr("emptyChromeGlobalsFilterText", [this._fileSearchKey]);
        break;
      case DebuggerView.Sources:
        placeholder = L10N.getFormatStr("emptyFilterText", [this._fileSearchKey]);
        break;
    }
    this._searchbox.setAttribute("placeholder", placeholder);
    this._target = aView;
  },

  /**
   * Gets the entered file, line and token entered in the searchbox.
   * @return array
   */
  get searchboxInfo() {
    let file, line, token, global;

    let rawValue = this._searchbox.value;
    let rawLength = rawValue.length;
    let globalFlagIndex = rawValue.indexOf(SEARCH_GLOBAL_FLAG);
    let lineFlagIndex = rawValue.lastIndexOf(SEARCH_LINE_FLAG);
    let tokenFlagIndex = rawValue.lastIndexOf(SEARCH_TOKEN_FLAG);

    // This is not a global search, allow file or line flags.
    if (globalFlagIndex != 0) {
      let fileEnd = lineFlagIndex != -1
        ? lineFlagIndex
        : tokenFlagIndex != -1 ? tokenFlagIndex : rawLength;

      let lineEnd = tokenFlagIndex != -1
        ? tokenFlagIndex
        : rawLength;

      file = rawValue.slice(0, fileEnd);
      line = ~~(rawValue.slice(fileEnd + 1, lineEnd)) || -1;
      token = rawValue.slice(lineEnd + 1);
      global = false;
    }
    // Global searches dissalow the use of file or line flags.
    else {
      file = "";
      line = -1;
      token = rawValue.slice(1);
      global = true;
    }

    return [file, line, token, global];
  },

  /**
   * Returns the currently searched file.
   * @return string
   */
  get searchedFile() this.searchboxInfo[0],

  /**
   * Returns the currently searched line.
   * @return number
   */
  get searchedLine() this.searchboxInfo[1],

  /**
   * Returns the currently searched token.
   * @return string
   */
  get searchedToken() this.searchboxInfo[2],

  /**
   * Clears the text from the searchbox and resets any changed view.
   */
  clearSearch: function DVF_clearSearch() {
    this._searchbox.value = "";
    this._onSearch();
  },

  /**
   * Performs a file search if necessary.
   *
   * @param string aFile
   *        The source location to search for.
   */
  _performFileSearch: function DVF__performFileSearch(aFile) {
    // Don't search for files if the input hasn't changed.
    if (this._prevSearchedFile == aFile) {
      return;
    }

    let view = this._target;

    // If we're not searching for a file anymore, unhide all the items.
    if (!aFile) {
      for (let item in view) {
        item.target.hidden = false;
      }
      view.refresh();
    }
    // If the searched file string is valid, hide non-matched items.
    else {
      let found = false;
      let lowerCaseFile = aFile.toLowerCase();

      for (let item in view) {
        let element = item.target;
        let lowerCaseLabel = item.label.toLowerCase();

        // Search is not case sensitive, and is tied to the label not the value.
        if (lowerCaseLabel.match(lowerCaseFile)) {
          element.hidden = false;

          // Automatically select the first match.
          if (!found) {
            found = true;
            view.selectedItem = item;
          }
        }
        // Item not matched, hide the corresponding node.
        else {
          element.hidden = true;
        }
      }
      // If no matches were found, display the appropriate info.
      if (!found) {
        view.setUnavailable();
      }
    }
    this._prevSearchedFile = aFile;
  },

  /**
   * Performs a line search if necessary.
   * (Jump to lines in the currently visible source).
   *
   * @param number aLine
   *        The source line number to jump to.
   */
  _performLineSearch: function DVF__performLineSearch(aLine) {
    // Don't search for lines if the input hasn't changed.
    if (this._prevSearchedLine != aLine && aLine > -1) {
      DebuggerView.editor.setCaretPosition(aLine - 1);
    }
    this._prevSearchedLine = aLine;
  },

  /**
   * Performs a token search if necessary.
   * (Search for tokens in the currently visible source).
   *
   * @param string aToken
   *        The source token to find.
   */
  _performTokenSearch: function DVF__performTokenSearch(aToken) {
    // Don't search for tokens if the input hasn't changed.
    if (this._prevSearchedToken != aToken && aToken.length > 0) {
      let editor = DebuggerView.editor;
      let offset = editor.find(aToken, { ignoreCase: true });
      if (offset > -1) {
        editor.setSelection(offset, offset + aToken.length)
      }
    }
    this._prevSearchedToken = aToken;
  },

  /**
   * The click listener for the search container.
   */
  _onClick: function DVF__onClick() {
    this._searchboxPanel.openPopup(this._searchbox);
  },

  /**
   * The search listener for the search container.
   */
  _onSearch: function DVF__onScriptsSearch() {
    this._searchboxPanel.hidePopup();
    let [file, line, token, global] = this.searchboxInfo;

    // If this is a global search, schedule it for when the user stops typing,
    // or hide the corresponding pane otherwise.
    if (global) {
      DebuggerView.GlobalSearch.scheduleSearch();
    } else {
      DebuggerView.GlobalSearch.clearView();
      this._performFileSearch(file);
      this._performLineSearch(line);
      this._performTokenSearch(token);
    }
  },

  /**
   * The key press listener for the search container.
   */
  _onKeyPress: function DVF__onScriptsKeyPress(e) {
    let [file, line, token, global] = this.searchboxInfo;
    let action;

    switch (e.keyCode) {
      case e.DOM_VK_DOWN:
      case e.DOM_VK_RETURN:
      case e.DOM_VK_ENTER:
        action = 0;
        break;
      case e.DOM_VK_UP:
        action = 1;
        break;
      case e.DOM_VK_ESCAPE:
        action = 2;
        break;
      default:
        action = -1;
    }

    if (action == 2) {
      DebuggerView.editor.focus();
      return;
    }
    if (action == -1 || !token) {
      return;
    }

    e.preventDefault();
    e.stopPropagation();

    if (global) {
      if (DebuggerView.GlobalSearch.hidden) {
        DebuggerView.GlobalSearch.scheduleSearch();
      } else {
        DebuggerView.GlobalSearch[["focusNextMatch", "focusPrevMatch"][action]]();
      }
    } else {
      let editor = DebuggerView.editor;
      let offset = editor[["findNext", "findPrevious"][action]](true);
      if (offset > -1) {
        editor.setSelection(offset, offset + token.length)
      }
    }
  },

  /**
   * The blur listener for the search container.
   */
  _onBlur: function DVF__onBlur() {
    DebuggerView.GlobalSearch.clearView();
    this._searchboxPanel.hidePopup();
  },

  /**
   * Called when a filtering key sequence was pressed.
   *
   * @param string aOperator
   *        The operator to use for filtering.
   */
  _doSearch: function DVF__doSearch(aOperator = "") {
    this._searchbox.focus();
    this._searchbox.value = aOperator;
    DebuggerView.GlobalSearch.clearView();
  },

  /**
   * Called when the source location filter key sequence was pressed.
   */
  _doFileSearch: function DVF__doFileSearch() {
    this._doSearch();
    this._searchboxPanel.openPopup(this._searchbox);
  },

  /**
   * Called when the source line filter key sequence was pressed.
   */
  _doLineSearch: function DVF__doLineSearch() {
    this._doSearch(SEARCH_LINE_FLAG);
    this._searchboxPanel.hidePopup();
  },

  /**
   * Called when the source token filter key sequence was pressed.
   */
  _doTokenSearch: function DVF__doTokenSearch() {
    this._doSearch(SEARCH_TOKEN_FLAG);
    this._searchboxPanel.hidePopup();
  },

  /**
   * Called when the global search filter key sequence was pressed.
   */
  _doGlobalSearch: function DVF__doGlobalSearch() {
    this._doSearch(SEARCH_GLOBAL_FLAG);
    this._searchboxPanel.hidePopup();
  },

  _searchbox: null,
  _searchboxPanel: null,
  _globalOperatorButton: null,
  _globalOperatorLabel: null,
  _tokenOperatorButton: null,
  _tokenOperatorLabel: null,
  _lineOperatorButton: null,
  _lineOperatorLabel: null,
  _globalSearchKey: "",
  _fileSearchKey: "",
  _lineSearchKey: "",
  _tokenSearchKey: "",
  _target: null,
  _prevSearchedFile: "",
  _prevSearchedLine: -1,
  _prevSearchedToken: ""
};

/**
 * Preliminary setup for the DebuggerView object.
 */
DebuggerView.Toolbar = new ToolbarView();
DebuggerView.Options = new OptionsView();
DebuggerView.ChromeGlobals = new ChromeGlobalsView();
DebuggerView.Sources = new SourcesView();
DebuggerView.Filtering = new FilterView();
