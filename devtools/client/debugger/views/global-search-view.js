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
 * Functions handling the global search UI.
 */
function GlobalSearchView(DebuggerController, DebuggerView) {
  dumpn("GlobalSearchView was instantiated");

  this.SourceScripts = DebuggerController.SourceScripts;
  this.DebuggerView = DebuggerView;

  this._onHeaderClick = this._onHeaderClick.bind(this);
  this._onLineClick = this._onLineClick.bind(this);
  this._onMatchClick = this._onMatchClick.bind(this);
}

GlobalSearchView.prototype = Heritage.extend(WidgetMethods, {
  /**
   * Initialization function, called when the debugger is started.
   */
  initialize: function () {
    dumpn("Initializing the GlobalSearchView");

    this.widget = new SimpleListWidget(document.getElementById("globalsearch"));
    this._splitter = document.querySelector("#globalsearch + .devtools-horizontal-splitter");

    this.emptyText = L10N.getStr("noMatchingStringsText");
  },

  /**
   * Destruction function, called when the debugger is closed.
   */
  destroy: function () {
    dumpn("Destroying the GlobalSearchView");
  },

  /**
   * Sets the results container hidden or visible. It's hidden by default.
   * @param boolean aFlag
   */
  set hidden(aFlag) {
    this.widget.setAttribute("hidden", aFlag);
    this._splitter.setAttribute("hidden", aFlag);
  },

  /**
   * Gets the visibility state of the global search container.
   * @return boolean
   */
  get hidden() {
    return this.widget.getAttribute("hidden") == "true" ||
           this._splitter.getAttribute("hidden") == "true";
  },

  /**
   * Hides and removes all items from this search container.
   */
  clearView: function () {
    this.hidden = true;
    this.empty();
  },

  /**
   * Selects the next found item in this container.
   * Does not change the currently focused node.
   */
  selectNext: function () {
    let totalLineResults = LineResults.size();
    if (!totalLineResults) {
      return;
    }
    if (++this._currentlyFocusedMatch >= totalLineResults) {
      this._currentlyFocusedMatch = 0;
    }
    this._onMatchClick({
      target: LineResults.getElementAtIndex(this._currentlyFocusedMatch)
    });
  },

  /**
   * Selects the previously found item in this container.
   * Does not change the currently focused node.
   */
  selectPrev: function () {
    let totalLineResults = LineResults.size();
    if (!totalLineResults) {
      return;
    }
    if (--this._currentlyFocusedMatch < 0) {
      this._currentlyFocusedMatch = totalLineResults - 1;
    }
    this._onMatchClick({
      target: LineResults.getElementAtIndex(this._currentlyFocusedMatch)
    });
  },

  /**
   * Schedules searching for a string in all of the sources.
   *
   * @param string aToken
   *        The string to search for.
   * @param number aWait
   *        The amount of milliseconds to wait until draining.
   */
  scheduleSearch: function (aToken, aWait) {
    // The amount of time to wait for the requests to settle.
    let maxDelay = GLOBAL_SEARCH_ACTION_MAX_DELAY;
    let delay = aWait === undefined ? maxDelay / aToken.length : aWait;

    // Allow requests to settle down first.
    setNamedTimeout("global-search", delay, () => {
      // Start fetching as many sources as possible, then perform the search.
      let actors = this.DebuggerView.Sources.values;
      let sourcesFetched = DebuggerController.dispatch(actions.getTextForSources(actors));
      sourcesFetched.then(aSources => this._doSearch(aToken, aSources));
    });
  },

  /**
   * Finds string matches in all the sources stored in the controller's cache,
   * and groups them by url and line number.
   *
   * @param string aToken
   *        The string to search for.
   * @param array aSources
   *        An array of [url, text] tuples for each source.
   */
  _doSearch: function (aToken, aSources) {
    // Don't continue filtering if the searched token is an empty string.
    if (!aToken) {
      this.clearView();
      return;
    }

    // Search is not case sensitive, prepare the actual searched token.
    let lowerCaseToken = aToken.toLowerCase();
    let tokenLength = aToken.length;

    // Create a Map containing search details for each source.
    let globalResults = new GlobalResults();

    // Search for the specified token in each source's text.
    for (let [actor, text] of aSources) {
      let item = this.DebuggerView.Sources.getItemByValue(actor);
      let url = item.attachment.source.url;
      if (!url) {
        continue;
      }

      // Verify that the search token is found anywhere in the source.
      if (!text.toLowerCase().includes(lowerCaseToken)) {
        continue;
      }
      // ...and if so, create a Map containing search details for each line.
      let sourceResults = new SourceResults(actor,
                                            globalResults,
                                            this.DebuggerView.Sources);

      // Search for the specified token in each line's text.
      text.split("\n").forEach((aString, aLine) => {
        // Search is not case sensitive, prepare the actual searched line.
        let lowerCaseLine = aString.toLowerCase();

        // Verify that the search token is found anywhere in this line.
        if (!lowerCaseLine.includes(lowerCaseToken)) {
          return;
        }
        // ...and if so, create a Map containing search details for each word.
        let lineResults = new LineResults(aLine, sourceResults);

        // Search for the specified token this line's text.
        lowerCaseLine.split(lowerCaseToken).reduce((aPrev, aCurr, aIndex, aArray) => {
          let prevLength = aPrev.length;
          let currLength = aCurr.length;

          // Everything before the token is unmatched.
          let unmatched = aString.substr(prevLength, currLength);
          lineResults.add(unmatched);

          // The lowered-case line was split by the lowered-case token. So,
          // get the actual matched text from the original line's text.
          if (aIndex != aArray.length - 1) {
            let matched = aString.substr(prevLength + currLength, tokenLength);
            let range = { start: prevLength + currLength, length: matched.length };
            lineResults.add(matched, range, true);
          }

          // Continue with the next sub-region in this line's text.
          return aPrev + aToken + aCurr;
        }, "");

        if (lineResults.matchCount) {
          sourceResults.add(lineResults);
        }
      });

      if (sourceResults.matchCount) {
        globalResults.add(sourceResults);
      }
    }

    // Rebuild the results, then signal if there are any matches.
    if (globalResults.matchCount) {
      this.hidden = false;
      this._currentlyFocusedMatch = -1;
      this._createGlobalResultsUI(globalResults);
      window.emit(EVENTS.GLOBAL_SEARCH_MATCH_FOUND);
    } else {
      window.emit(EVENTS.GLOBAL_SEARCH_MATCH_NOT_FOUND);
    }
  },

  /**
   * Creates global search results entries and adds them to this container.
   *
   * @param GlobalResults aGlobalResults
   *        An object containing all source results, grouped by source location.
   */
  _createGlobalResultsUI: function (aGlobalResults) {
    let i = 0;

    for (let sourceResults of aGlobalResults) {
      if (i++ == 0) {
        this._createSourceResultsUI(sourceResults);
      } else {
        // Dispatch subsequent document manipulation operations, to avoid
        // blocking the main thread when a large number of search results
        // is found, thus giving the impression of faster searching.
        Services.tm.currentThread.dispatch({ run:
          this._createSourceResultsUI.bind(this, sourceResults)
        }, 0);
      }
    }
  },

  /**
   * Creates source search results entries and adds them to this container.
   *
   * @param SourceResults aSourceResults
   *        An object containing all the matched lines for a specific source.
   */
  _createSourceResultsUI: function (aSourceResults) {
    // Create the element node for the source results item.
    let container = document.createElement("hbox");
    aSourceResults.createView(container, {
      onHeaderClick: this._onHeaderClick,
      onLineClick: this._onLineClick,
      onMatchClick: this._onMatchClick
    });

    // Append a source results item to this container.
    let item = this.push([container], {
      index: -1, /* specifies on which position should the item be appended */
      attachment: {
        sourceResults: aSourceResults
      }
    });
  },

  /**
   * The click listener for a results header.
   */
  _onHeaderClick: function (e) {
    let sourceResultsItem = SourceResults.getItemForElement(e.target);
    sourceResultsItem.instance.toggle(e);
  },

  /**
   * The click listener for a results line.
   */
  _onLineClick: function (e) {
    let lineResultsItem = LineResults.getItemForElement(e.target);
    this._onMatchClick({ target: lineResultsItem.firstMatch });
  },

  /**
   * The click listener for a result match.
   */
  _onMatchClick: function (e) {
    if (e instanceof Event) {
      e.preventDefault();
      e.stopPropagation();
    }

    let target = e.target;
    let sourceResultsItem = SourceResults.getItemForElement(target);
    let lineResultsItem = LineResults.getItemForElement(target);

    sourceResultsItem.instance.expand();
    this._currentlyFocusedMatch = LineResults.indexOfElement(target);
    this._scrollMatchIntoViewIfNeeded(target);
    this._bounceMatch(target);

    let actor = sourceResultsItem.instance.actor;
    let line = lineResultsItem.instance.line;

    this.DebuggerView.setEditorLocation(actor, line + 1, { noDebug: true });

    let range = lineResultsItem.lineData.range;
    let cursor = this.DebuggerView.editor.getOffset({ line: line, ch: 0 });
    let [ anchor, head ] = this.DebuggerView.editor.getPosition(
      cursor + range.start,
      cursor + range.start + range.length
    );

    this.DebuggerView.editor.setSelection(anchor, head);
  },

  /**
   * Scrolls a match into view if not already visible.
   *
   * @param nsIDOMNode aMatch
   *        The match to scroll into view.
   */
  _scrollMatchIntoViewIfNeeded: function (aMatch) {
    this.widget.ensureElementIsVisible(aMatch);
  },

  /**
   * Starts a bounce animation for a match.
   *
   * @param nsIDOMNode aMatch
   *        The match to start a bounce animation for.
   */
  _bounceMatch: function (aMatch) {
    Services.tm.currentThread.dispatch({ run: () => {
      aMatch.addEventListener("transitionend", function onEvent() {
        aMatch.removeEventListener("transitionend", onEvent);
        aMatch.removeAttribute("focused");
      });
      aMatch.setAttribute("focused", "");
    }}, 0);
    aMatch.setAttribute("focusing", "");
  },

  _splitter: null,
  _currentlyFocusedMatch: -1,
  _forceExpandResults: false
});

DebuggerView.GlobalSearch = new GlobalSearchView(DebuggerController, DebuggerView);

/**
 * An object containing all source results, grouped by source location.
 * Iterable via "for (let [location, sourceResults] of globalResults) { }".
 */
function GlobalResults() {
  this._store = [];
  SourceResults._itemsByElement = new Map();
  LineResults._itemsByElement = new Map();
}

GlobalResults.prototype = {
  /**
   * Adds source results to this store.
   *
   * @param SourceResults aSourceResults
   *        An object containing search results for a specific source.
   */
  add: function (aSourceResults) {
    this._store.push(aSourceResults);
  },

  /**
   * Gets the number of source results in this store.
   */
  get matchCount() {
    return this._store.length;
  }
};

/**
 * An object containing all the matched lines for a specific source.
 * Iterable via "for (let [lineNumber, lineResults] of sourceResults) { }".
 *
 * @param string aActor
 *        The target source actor id.
 * @param GlobalResults aGlobalResults
 *        An object containing all source results, grouped by source location.
 */
function SourceResults(aActor, aGlobalResults, sourcesView) {
  let item = sourcesView.getItemByValue(aActor);
  this.actor = aActor;
  this.label = item.attachment.source.url;
  this._globalResults = aGlobalResults;
  this._store = [];
}

SourceResults.prototype = {
  /**
   * Adds line results to this store.
   *
   * @param LineResults aLineResults
   *        An object containing search results for a specific line.
   */
  add: function (aLineResults) {
    this._store.push(aLineResults);
  },

  /**
   * Gets the number of line results in this store.
   */
  get matchCount() {
    return this._store.length;
  },

  /**
   * Expands the element, showing all the added details.
   */
  expand: function () {
    this._resultsContainer.removeAttribute("hidden");
    this._arrow.setAttribute("open", "");
  },

  /**
   * Collapses the element, hiding all the added details.
   */
  collapse: function () {
    this._resultsContainer.setAttribute("hidden", "true");
    this._arrow.removeAttribute("open");
  },

  /**
   * Toggles between the element collapse/expand state.
   */
  toggle: function (e) {
    this.expanded ^= 1;
  },

  /**
   * Gets this element's expanded state.
   * @return boolean
   */
  get expanded() {
    return this._resultsContainer.getAttribute("hidden") != "true" &&
           this._arrow.hasAttribute("open");
  },

  /**
   * Sets this element's expanded state.
   * @param boolean aFlag
   */
  set expanded(aFlag) {
    this[aFlag ? "expand" : "collapse"]();
  },

  /**
   * Gets the element associated with this item.
   * @return nsIDOMNode
   */
  get target() {
    return this._target;
  },

  /**
   * Customization function for creating this item's UI.
   *
   * @param nsIDOMNode aElementNode
   *        The element associated with the displayed item.
   * @param object aCallbacks
   *        An object containing all the necessary callback functions:
   *          - onHeaderClick
   *          - onMatchClick
   */
  createView: function (aElementNode, aCallbacks) {
    this._target = aElementNode;

    let arrow = this._arrow = document.createElement("box");
    arrow.className = "arrow";

    let locationNode = document.createElement("label");
    locationNode.className = "plain dbg-results-header-location";
    locationNode.setAttribute("value", this.label);

    let matchCountNode = document.createElement("label");
    matchCountNode.className = "plain dbg-results-header-match-count";
    matchCountNode.setAttribute("value", "(" + this.matchCount + ")");

    let resultsHeader = this._resultsHeader = document.createElement("hbox");
    resultsHeader.className = "dbg-results-header";
    resultsHeader.setAttribute("align", "center");
    resultsHeader.appendChild(arrow);
    resultsHeader.appendChild(locationNode);
    resultsHeader.appendChild(matchCountNode);
    resultsHeader.addEventListener("click", aCallbacks.onHeaderClick, false);

    let resultsContainer = this._resultsContainer = document.createElement("vbox");
    resultsContainer.className = "dbg-results-container";
    resultsContainer.setAttribute("hidden", "true");

    // Create lines search results entries and add them to this container.
    // Afterwards, if the number of matches is reasonable, expand this
    // container automatically.
    for (let lineResults of this._store) {
      lineResults.createView(resultsContainer, aCallbacks);
    }
    if (this.matchCount < GLOBAL_SEARCH_EXPAND_MAX_RESULTS) {
      this.expand();
    }

    let resultsBox = document.createElement("vbox");
    resultsBox.setAttribute("flex", "1");
    resultsBox.appendChild(resultsHeader);
    resultsBox.appendChild(resultsContainer);

    aElementNode.id = "source-results-" + this.actor;
    aElementNode.className = "dbg-source-results";
    aElementNode.appendChild(resultsBox);

    SourceResults._itemsByElement.set(aElementNode, { instance: this });
  },

  actor: "",
  _globalResults: null,
  _store: null,
  _target: null,
  _arrow: null,
  _resultsHeader: null,
  _resultsContainer: null
};

/**
 * An object containing all the matches for a specific line.
 * Iterable via "for (let chunk of lineResults) { }".
 *
 * @param number aLine
 *        The target line in the source.
 * @param SourceResults aSourceResults
 *        An object containing all the matched lines for a specific source.
 */
function LineResults(aLine, aSourceResults) {
  this.line = aLine;
  this._sourceResults = aSourceResults;
  this._store = [];
  this._matchCount = 0;
}

LineResults.prototype = {
  /**
   * Adds string details to this store.
   *
   * @param string aString
   *        The text contents chunk in the line.
   * @param object aRange
   *        An object containing the { start, length } of the chunk.
   * @param boolean aMatchFlag
   *        True if the chunk is a matched string, false if just text content.
   */
  add: function (aString, aRange, aMatchFlag) {
    this._store.push({ string: aString, range: aRange, match: !!aMatchFlag });
    this._matchCount += aMatchFlag ? 1 : 0;
  },

  /**
   * Gets the number of word results in this store.
   */
  get matchCount() {
    return this._matchCount;
  },

  /**
   * Gets the element associated with this item.
   * @return nsIDOMNode
   */
  get target() {
    return this._target;
  },

  /**
   * Customization function for creating this item's UI.
   *
   * @param nsIDOMNode aElementNode
   *        The element associated with the displayed item.
   * @param object aCallbacks
   *        An object containing all the necessary callback functions:
   *          - onMatchClick
   *          - onLineClick
   */
  createView: function (aElementNode, aCallbacks) {
    this._target = aElementNode;

    let lineNumberNode = document.createElement("label");
    lineNumberNode.className = "plain dbg-results-line-number";
    lineNumberNode.classList.add("devtools-monospace");
    lineNumberNode.setAttribute("value", this.line + 1);

    let lineContentsNode = document.createElement("hbox");
    lineContentsNode.className = "dbg-results-line-contents";
    lineContentsNode.classList.add("devtools-monospace");
    lineContentsNode.setAttribute("flex", "1");

    let lineString = "";
    let lineLength = 0;
    let firstMatch = null;

    for (let lineChunk of this._store) {
      let { string, range, match } = lineChunk;
      lineString = string.substr(0, GLOBAL_SEARCH_LINE_MAX_LENGTH - lineLength);
      lineLength += string.length;

      let lineChunkNode = document.createElement("label");
      lineChunkNode.className = "plain dbg-results-line-contents-string";
      lineChunkNode.setAttribute("value", lineString);
      lineChunkNode.setAttribute("match", match);
      lineContentsNode.appendChild(lineChunkNode);

      if (match) {
        this._entangleMatch(lineChunkNode, lineChunk);
        lineChunkNode.addEventListener("click", aCallbacks.onMatchClick, false);
        firstMatch = firstMatch || lineChunkNode;
      }
      if (lineLength >= GLOBAL_SEARCH_LINE_MAX_LENGTH) {
        lineContentsNode.appendChild(this._ellipsis.cloneNode(true));
        break;
      }
    }

    this._entangleLine(lineContentsNode, firstMatch);
    lineContentsNode.addEventListener("click", aCallbacks.onLineClick, false);

    let searchResult = document.createElement("hbox");
    searchResult.className = "dbg-search-result";
    searchResult.appendChild(lineNumberNode);
    searchResult.appendChild(lineContentsNode);

    aElementNode.appendChild(searchResult);
  },

  /**
   * Handles a match while creating the view.
   * @param nsIDOMNode aNode
   * @param object aMatchChunk
   */
  _entangleMatch: function (aNode, aMatchChunk) {
    LineResults._itemsByElement.set(aNode, {
      instance: this,
      lineData: aMatchChunk
    });
  },

  /**
   * Handles a line while creating the view.
   * @param nsIDOMNode aNode
   * @param nsIDOMNode aFirstMatch
   */
  _entangleLine: function (aNode, aFirstMatch) {
    LineResults._itemsByElement.set(aNode, {
      instance: this,
      firstMatch: aFirstMatch,
      ignored: true
    });
  },

  /**
   * An nsIDOMNode label with an ellipsis value.
   */
  _ellipsis: (function () {
    let label = document.createElement("label");
    label.className = "plain dbg-results-line-contents-string";
    label.setAttribute("value", ELLIPSIS);
    return label;
  })(),

  line: 0,
  _sourceResults: null,
  _store: null,
  _target: null
};

/**
 * A generator-iterator over the global, source or line results.
 */
GlobalResults.prototype[Symbol.iterator] =
SourceResults.prototype[Symbol.iterator] =
LineResults.prototype[Symbol.iterator] = function* () {
  yield* this._store;
};

/**
 * Gets the item associated with the specified element.
 *
 * @param nsIDOMNode aElement
 *        The element used to identify the item.
 * @return object
 *         The matched item, or null if nothing is found.
 */
SourceResults.getItemForElement =
LineResults.getItemForElement = function (aElement) {
  return WidgetMethods.getItemForElement.call(this, aElement, { noSiblings: true });
};

/**
 * Gets the element associated with a particular item at a specified index.
 *
 * @param number aIndex
 *        The index used to identify the item.
 * @return nsIDOMNode
 *         The matched element, or null if nothing is found.
 */
SourceResults.getElementAtIndex =
LineResults.getElementAtIndex = function (aIndex) {
  for (let [element, item] of this._itemsByElement) {
    if (!item.ignored && !aIndex--) {
      return element;
    }
  }
  return null;
};

/**
 * Gets the index of an item associated with the specified element.
 *
 * @param nsIDOMNode aElement
 *        The element to get the index for.
 * @return number
 *         The index of the matched element, or -1 if nothing is found.
 */
SourceResults.indexOfElement =
LineResults.indexOfElement = function (aElement) {
  let count = 0;
  for (let [element, item] of this._itemsByElement) {
    if (element == aElement) {
      return count;
    }
    if (!item.ignored) {
      count++;
    }
  }
  return -1;
};

/**
 * Gets the number of cached items associated with a specified element.
 *
 * @return number
 *         The number of key/value pairs in the corresponding map.
 */
SourceResults.size =
LineResults.size = function () {
  let count = 0;
  for (let [, item] of this._itemsByElement) {
    if (!item.ignored) {
      count++;
    }
  }
  return count;
};
