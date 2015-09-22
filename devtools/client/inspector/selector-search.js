/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cu } = require("chrome");

const promise = require("promise");
loader.lazyGetter(this, "EventEmitter", () => require("devtools/shared/event-emitter"));
loader.lazyGetter(this, "AutocompletePopup", () => require("devtools/client/shared/autocomplete-popup").AutocompletePopup);

// Maximum number of selector suggestions shown in the panel.
const MAX_SUGGESTIONS = 15;

/**
 * Converts any input box on a page to a CSS selector search and suggestion box.
 *
 * Emits 'processing-done' event when it is done processing the current
 * keypress, search request or selection from the list, whether that led to a
 * search or not.
 *
 * @constructor
 * @param InspectorPanel aInspector
 *        The InspectorPanel whose `walker` attribute should be used for
 *        document traversal.
 * @param nsiInputElement aInputNode
 *        The input element to which the panel will be attached and from where
 *        search input will be taken.
 */
function SelectorSearch(aInspector, aInputNode) {
  this.inspector = aInspector;
  this.searchBox = aInputNode;
  this.panelDoc = this.searchBox.ownerDocument;

  // initialize variables.
  this._lastSearched = null;
  this._lastValidSearch = "";
  this._lastToLastValidSearch = null;
  this._searchResults = null;
  this._searchSuggestions = {};
  this._searchIndex = 0;

  // bind!
  this._showPopup = this._showPopup.bind(this);
  this._onHTMLSearch = this._onHTMLSearch.bind(this);
  this._onSearchKeypress = this._onSearchKeypress.bind(this);
  this._onListBoxKeypress = this._onListBoxKeypress.bind(this);
  this._onMarkupMutation = this._onMarkupMutation.bind(this);

  // Options for the AutocompletePopup.
  let options = {
    panelId: "inspector-searchbox-panel",
    listBoxId: "searchbox-panel-listbox",
    autoSelect: true,
    position: "before_start",
    direction: "ltr",
    theme: "auto",
    onClick: this._onListBoxKeypress,
    onKeypress: this._onListBoxKeypress
  };
  this.searchPopup = new AutocompletePopup(this.panelDoc, options);

  // event listeners.
  this.searchBox.addEventListener("command", this._onHTMLSearch, true);
  this.searchBox.addEventListener("keypress", this._onSearchKeypress, true);
  this.inspector.on("markupmutation", this._onMarkupMutation);

  // For testing, we need to be able to wait for the most recent node request
  // to finish.  Tests can watch this promise for that.
  this._lastQuery = promise.resolve(null);
  EventEmitter.decorate(this);
}

exports.SelectorSearch = SelectorSearch;

SelectorSearch.prototype = {
  get walker() {
    return this.inspector.walker;
  },

  // The possible states of the query.
  States: {
    CLASS: "class",
    ID: "id",
    TAG: "tag",
  },

  // The current state of the query.
  _state: null,

  // The query corresponding to last state computation.
  _lastStateCheckAt: null,

  /**
   * Computes the state of the query. State refers to whether the query
   * currently requires a class suggestion, or a tag, or an Id suggestion.
   * This getter will effectively compute the state by traversing the query
   * character by character each time the query changes.
   *
   * @example
   *        '#f' requires an Id suggestion, so the state is States.ID
   *        'div > .foo' requires class suggestion, so state is States.CLASS
   */
  get state() {
    if (!this.searchBox || !this.searchBox.value) {
      return null;
    }

    let query = this.searchBox.value;
    if (this._lastStateCheckAt == query) {
      // If query is the same, return early.
      return this._state;
    }
    this._lastStateCheckAt = query;

    this._state = null;
    let subQuery = "";
    // Now we iterate over the query and decide the state character by character.
    // The logic here is that while iterating, the state can go from one to
    // another with some restrictions. Like, if the state is Class, then it can
    // never go to Tag state without a space or '>' character; Or like, a Class
    // state with only '.' cannot go to an Id state without any [a-zA-Z] after
    // the '.' which means that '.#' is a selector matching a class name '#'.
    // Similarily for '#.' which means a selctor matching an id '.'.
    for (let i = 1; i <= query.length; i++) {
      // Calculate the state.
      subQuery = query.slice(0, i);
      let [secondLastChar, lastChar] = subQuery.slice(-2);
      switch (this._state) {
        case null:
          // This will happen only in the first iteration of the for loop.
          lastChar = secondLastChar;
        case this.States.TAG:
          this._state = lastChar == "."
            ? this.States.CLASS
            : lastChar == "#"
              ? this.States.ID
              : this.States.TAG;
          break;

        case this.States.CLASS:
          if (subQuery.match(/[\.]+[^\.]*$/)[0].length > 2) {
            // Checks whether the subQuery has atleast one [a-zA-Z] after the '.'.
            this._state = (lastChar == " " || lastChar == ">")
            ? this.States.TAG
            : lastChar == "#"
              ? this.States.ID
              : this.States.CLASS;
          }
          break;

        case this.States.ID:
          if (subQuery.match(/[#]+[^#]*$/)[0].length > 2) {
            // Checks whether the subQuery has atleast one [a-zA-Z] after the '#'.
            this._state = (lastChar == " " || lastChar == ">")
            ? this.States.TAG
            : lastChar == "."
              ? this.States.CLASS
              : this.States.ID;
          }
          break;
      }
    }
    return this._state;
  },

  /**
   * Removes event listeners and cleans up references.
   */
  destroy: function() {
    // event listeners.
    this.searchBox.removeEventListener("command", this._onHTMLSearch, true);
    this.searchBox.removeEventListener("keypress", this._onSearchKeypress, true);
    this.inspector.off("markupmutation", this._onMarkupMutation);
    this.searchPopup.destroy();
    this.searchPopup = null;
    this.searchBox = null;
    this.panelDoc = null;
    this._searchResults = null;
    this._searchSuggestions = null;
  },

  _selectResult: function(index) {
    return this._searchResults.item(index).then(node => {
      this.inspector.selection.setNodeFront(node, "selectorsearch");
    });
  },

  _queryNodes: Task.async(function*(query) {
    if (typeof this.hasMultiFrameSearch === "undefined") {
      let target = this.inspector.toolbox.target;
      this.hasMultiFrameSearch = yield target.actorHasMethod("domwalker",
        "multiFrameQuerySelectorAll");
    }

    if (this.hasMultiFrameSearch) {
      return yield this.walker.multiFrameQuerySelectorAll(query);
    } else {
      return yield this.walker.querySelectorAll(this.walker.rootNode, query);
    }
  }),

  /**
   * The command callback for the input box. This function is automatically
   * invoked as the user is typing if the input box type is search.
   */
  _onHTMLSearch: function() {
    let query = this.searchBox.value;
    if (query == this._lastSearched) {
      this.emit("processing-done");
      return;
    }
    this._lastSearched = query;
    this._searchResults = [];
    this._searchIndex = 0;

    if (query.length == 0) {
      this._lastValidSearch = "";
      this.searchBox.removeAttribute("filled");
      this.searchBox.classList.remove("devtools-no-search-result");
      if (this.searchPopup.isOpen) {
        this.searchPopup.hidePopup();
      }
      this.emit("processing-done");
      return;
    }

    this.searchBox.setAttribute("filled", true);
    let queryList = null;

    this._lastQuery = this._queryNodes(query).then(list => {
      return list;
    }, (err) => {
      // Failures are ok here, just use a null item list;
      return null;
    }).then(queryList => {
      // Value has changed since we started this request, we're done.
      if (query != this.searchBox.value) {
        if (queryList) {
          queryList.release();
        }
        return promise.reject(null);
      }

      this._searchResults = queryList || [];
      if (this._searchResults && this._searchResults.length > 0) {
        this._lastValidSearch = query;
        // Even though the selector matched atleast one node, there is still
        // possibility of suggestions.
        if (query.match(/[\s>+]$/)) {
          // If the query has a space or '>' at the end, create a selector to match
          // the children of the selector inside the search box by adding a '*'.
          this._lastValidSearch += "*";
        }
        else if (query.match(/[\s>+][\.#a-zA-Z][\.#>\s+]*$/)) {
          // If the query is a partial descendant selector which does not matches
          // any node, remove the last incomplete part and add a '*' to match
          // everything. For ex, convert 'foo > b' to 'foo > *' .
          let lastPart = query.match(/[\s>+][\.#a-zA-Z][^>\s+]*$/)[0];
          this._lastValidSearch = query.slice(0, -1 * lastPart.length + 1) + "*";
        }

        if (!query.slice(-1).match(/[\.#\s>+]/)) {
          // Hide the popup if we have some matching nodes and the query is not
          // ending with [.# >] which means that the selector is not at the
          // beginning of a new class, tag or id.
          if (this.searchPopup.isOpen) {
            this.searchPopup.hidePopup();
          }
          this.searchBox.classList.remove("devtools-no-search-result");

          return this._selectResult(0);
        }
        return this._selectResult(0).then(() => {
          this.searchBox.classList.remove("devtools-no-search-result");
        }).then(() => this.showSuggestions());
      }
      if (query.match(/[\s>+]$/)) {
        this._lastValidSearch = query + "*";
      }
      else if (query.match(/[\s>+][\.#a-zA-Z][\.#>\s+]*$/)) {
        let lastPart = query.match(/[\s+>][\.#a-zA-Z][^>\s+]*$/)[0];
        this._lastValidSearch = query.slice(0, -1 * lastPart.length + 1) + "*";
      }
      this.searchBox.classList.add("devtools-no-search-result");
      return this.showSuggestions();
    }).then(() => this.emit("processing-done"), Cu.reportError);
  },

  /**
   * Handles keypresses inside the input box.
   */
  _onSearchKeypress: function(aEvent) {
    let query = this.searchBox.value;
    switch(aEvent.keyCode) {
      case aEvent.DOM_VK_RETURN:
        if (query == this._lastSearched && this._searchResults) {
          this._searchIndex = (this._searchIndex + 1) % this._searchResults.length;
        }
        else {
          this._onHTMLSearch();
          return;
        }
        break;

      case aEvent.DOM_VK_UP:
        if (this.searchPopup.isOpen && this.searchPopup.itemCount > 0) {
          this.searchPopup.focus();
          if (this.searchPopup.selectedIndex == this.searchPopup.itemCount - 1) {
            this.searchPopup.selectedIndex =
              Math.max(0, this.searchPopup.itemCount - 2);
          }
          else {
            this.searchPopup.selectedIndex = this.searchPopup.itemCount - 1;
          }
          this.searchBox.value = this.searchPopup.selectedItem.label;
        }
        else if (--this._searchIndex < 0) {
          this._searchIndex = this._searchResults.length - 1;
        }
        break;

      case aEvent.DOM_VK_DOWN:
        if (this.searchPopup.isOpen && this.searchPopup.itemCount > 0) {
          this.searchPopup.focus();
          this.searchPopup.selectedIndex = 0;
          this.searchBox.value = this.searchPopup.selectedItem.label;
        }
        this._searchIndex = (this._searchIndex + 1) % this._searchResults.length;
        break;

      case aEvent.DOM_VK_TAB:
        if (this.searchPopup.isOpen &&
            this.searchPopup.getItemAtIndex(this.searchPopup.itemCount - 1)
                .preLabel == query) {
          this.searchPopup.selectedIndex = this.searchPopup.itemCount - 1;
          this.searchBox.value = this.searchPopup.selectedItem.label;
          this._onHTMLSearch();
        }
        break;

      case aEvent.DOM_VK_BACK_SPACE:
      case aEvent.DOM_VK_DELETE:
        // need to throw away the lastValidSearch.
        this._lastToLastValidSearch = null;
        // This gets the most complete selector from the query. For ex.
        // '.foo.ba' returns '.foo' , '#foo > .bar.baz' returns '#foo > .bar'
        // '.foo +bar' returns '.foo +' and likewise.
        this._lastValidSearch = (query.match(/(.*)[\.#][^\.# ]{0,}$/) ||
                                 query.match(/(.*[\s>+])[a-zA-Z][^\.# ]{0,}$/) ||
                                 ["",""])[1];
        return;

      default:
        return;
    }

    aEvent.preventDefault();
    aEvent.stopPropagation();
    if (this._searchResults && this._searchResults.length > 0) {
      this._lastQuery = this._selectResult(this._searchIndex).then(() => this.emit("processing-done"));
    }
    else {
      this.emit("processing-done");
    }
  },

  /**
   * Handles keypress and mouse click on the suggestions richlistbox.
   */
  _onListBoxKeypress: function(aEvent) {
    switch(aEvent.keyCode || aEvent.button) {
      case aEvent.DOM_VK_RETURN:
      case aEvent.DOM_VK_TAB:
      case 0: // left mouse button
        aEvent.stopPropagation();
        aEvent.preventDefault();
        this.searchBox.value = this.searchPopup.selectedItem.label;
        this.searchBox.focus();
        this._onHTMLSearch();
        break;

      case aEvent.DOM_VK_UP:
        if (this.searchPopup.selectedIndex == 0) {
          this.searchPopup.selectedIndex = -1;
          aEvent.stopPropagation();
          aEvent.preventDefault();
          this.searchBox.focus();
        }
        else {
          let index = this.searchPopup.selectedIndex;
          this.searchBox.value = this.searchPopup.getItemAtIndex(index - 1).label;
        }
        break;

      case aEvent.DOM_VK_DOWN:
        if (this.searchPopup.selectedIndex == this.searchPopup.itemCount - 1) {
          this.searchPopup.selectedIndex = -1;
          aEvent.stopPropagation();
          aEvent.preventDefault();
          this.searchBox.focus();
        }
        else {
          let index = this.searchPopup.selectedIndex;
          this.searchBox.value = this.searchPopup.getItemAtIndex(index + 1).label;
        }
        break;

      case aEvent.DOM_VK_BACK_SPACE:
        aEvent.stopPropagation();
        aEvent.preventDefault();
        this.searchBox.focus();
        if (this.searchBox.selectionStart > 0) {
          this.searchBox.value =
            this.searchBox.value.substring(0, this.searchBox.selectionStart - 1);
        }
        this._lastToLastValidSearch = null;
        let query = this.searchBox.value;
        this._lastValidSearch = (query.match(/(.*)[\.#][^\.# ]{0,}$/) ||
                                 query.match(/(.*[\s>+])[a-zA-Z][^\.# ]{0,}$/) ||
                                 ["",""])[1];
        this._onHTMLSearch();
        break;
    }
    this.emit("processing-done");
  },

  /**
   * Reset previous search results on markup-mutations to make sure we search
   * again after nodes have been added/removed/changed.
   */
  _onMarkupMutation: function() {
    this._searchResults = null;
    this._lastSearched = null;
  },

  /**
   * Populates the suggestions list and show the suggestion popup.
   */
  _showPopup: function(aList, aFirstPart, aState) {
    let total = 0;
    let query = this.searchBox.value;
    let items = [];

    for (let [value, count, state] of aList) {
      // for cases like 'div ' or 'div >' or 'div+'
      if (query.match(/[\s>+]$/)) {
        value = query + value;
      }
      // for cases like 'div #a' or 'div .a' or 'div > d' and likewise
      else if (query.match(/[\s>+][\.#a-zA-Z][^\s>+\.#]*$/)) {
        let lastPart = query.match(/[\s>+][\.#a-zA-Z][^>\s+\.#]*$/)[0];
        value = query.slice(0, -1 * lastPart.length + 1) + value;
      }
      // for cases like 'div.class' or '#foo.bar' and likewise
      else if (query.match(/[a-zA-Z][#\.][^#\.\s+>]*$/)) {
        let lastPart = query.match(/[a-zA-Z][#\.][^#\.\s>+]*$/)[0];
        value = query.slice(0, -1 * lastPart.length + 1) + value;
      }

      let item = {
        preLabel: query,
        label: value,
        count: count
      };

      // In case of tagNames, change the case to small
      if (value.match(/.*[\.#][^\.#]{0,}$/) == null) {
        item.label = value.toLowerCase();
      }

      // In case the query's state is tag and the item's state is id or class
      // adjust the preLabel
      if (aState === this.States.TAG && state === this.States.CLASS) {
        item.preLabel = "." + item.preLabel;
      }
      if (aState === this.States.TAG && state === this.States.ID) {
        item.preLabel = "#" + item.preLabel;
      }

      items.unshift(item);
      if (++total > MAX_SUGGESTIONS - 1) {
        break;
      }
    }
    if (total > 0) {
      this.searchPopup.setItems(items);
      this.searchPopup.openPopup(this.searchBox);
    }
    else {
      this.searchPopup.hidePopup();
    }
  },

  /**
   * Suggests classes,ids and tags based on the user input as user types in the
   * searchbox.
   */
  showSuggestions: function() {
    let query = this.searchBox.value;
    let state = this.state;
    let firstPart = "";

    if (state == this.States.TAG) {
      // gets the tag that is being completed. For ex. 'div.foo > s' returns 's',
      // 'di' returns 'di' and likewise.
      firstPart = (query.match(/[\s>+]?([a-zA-Z]*)$/) || ["", query])[1];
      query = query.slice(0, query.length - firstPart.length);
    }
    else if (state == this.States.CLASS) {
      // gets the class that is being completed. For ex. '.foo.b' returns 'b'
      firstPart = query.match(/\.([^\.]*)$/)[1];
      query = query.slice(0, query.length - firstPart.length - 1);
    }
    else if (state == this.States.ID) {
      // gets the id that is being completed. For ex. '.foo#b' returns 'b'
      firstPart = query.match(/#([^#]*)$/)[1];
      query = query.slice(0, query.length - firstPart.length - 1);
    }
    // TODO: implement some caching so that over the wire request is not made
    // everytime.
    if (/[\s+>~]$/.test(query)) {
      query += "*";
    }

    this._currentSuggesting = query;
    return this.walker.getSuggestionsForQuery(query, firstPart, state).then(result => {
      if (this._currentSuggesting != result.query) {
        // This means that this response is for a previous request and the user
        // as since typed something extra leading to a new request.
        return;
      }
      this._lastToLastValidSearch = this._lastValidSearch;

      if (state == this.States.CLASS) {
        firstPart = "." + firstPart;
      }
      else if (state == this.States.ID) {
        firstPart = "#" + firstPart;
      }

      this._showPopup(result.suggestions, firstPart, state);
    });
  }
};
