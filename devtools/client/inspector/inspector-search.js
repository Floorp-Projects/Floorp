/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cu, Ci} = require("chrome");

const promise = require("promise");
loader.lazyGetter(this, "EventEmitter", () => require("devtools/shared/event-emitter"));
loader.lazyGetter(this, "AutocompletePopup", () => require("devtools/client/shared/autocomplete-popup").AutocompletePopup);

// Maximum number of selector suggestions shown in the panel.
const MAX_SUGGESTIONS = 15;


/**
 * Converts any input field into a document search box.
 *
 * @param {InspectorPanel} inspector The InspectorPanel whose `walker` attribute
 * should be used for document traversal.
 * @param {DOMNode} input The input element to which the panel will be attached
 * and from where search input will be taken.
 *
 * Emits the following events:
 * - search-cleared: when the search box is emptied
 * - search-result: when a search is made and a result is selected
 */
function InspectorSearch(inspector, input) {
  this.inspector = inspector;
  this.searchBox = input;
  this._lastSearched = null;

  this._onKeyDown = this._onKeyDown.bind(this);
  this._onCommand = this._onCommand.bind(this);
  this.searchBox.addEventListener("keydown", this._onKeyDown, true);
  this.searchBox.addEventListener("command", this._onCommand, true);

  // For testing, we need to be able to wait for the most recent node request
  // to finish.  Tests can watch this promise for that.
  this._lastQuery = promise.resolve(null);

  this.autocompleter = new SelectorAutocompleter(inspector, input);
  EventEmitter.decorate(this);
}

exports.InspectorSearch = InspectorSearch;

InspectorSearch.prototype = {
  get walker() {
    return this.inspector.walker;
  },

  destroy: function() {
    this.searchBox.removeEventListener("keydown", this._onKeyDown, true);
    this.searchBox.removeEventListener("command", this._onCommand, true);
    this.searchBox = null;
    this.autocompleter.destroy();
  },

  _onSearch: function(reverse = false) {
    this.doFullTextSearch(this.searchBox.value, reverse)
        .catch(e => console.error(e));
  },

  doFullTextSearch: Task.async(function*(query, reverse) {
    let lastSearched = this._lastSearched;
    this._lastSearched = query;

    if (query.length === 0) {
      this.searchBox.classList.remove("devtools-no-search-result");
      if (!lastSearched || lastSearched.length > 0) {
        this.emit("search-cleared");
      }
      return;
    }

    let res = yield this.walker.search(query, { reverse });

    // Value has changed since we started this request, we're done.
    if (query != this.searchBox.value) {
      return;
    }

    if (res) {
      this.inspector.selection.setNodeFront(res.node, "inspectorsearch");
      this.searchBox.classList.remove("devtools-no-search-result");

      res.query = query;
      this.emit("search-result", res);
    } else {
      this.searchBox.classList.add("devtools-no-search-result");
      this.emit("search-result");
    }
  }),

  _onCommand: function() {
    if (this.searchBox.value.length === 0) {
      this._onSearch();
    }
  },

  _onKeyDown: function(event) {
    if (this.searchBox.value.length === 0) {
      this.searchBox.removeAttribute("filled");
    } else {
      this.searchBox.setAttribute("filled", true);
    }
    if (event.keyCode === event.DOM_VK_RETURN) {
      this._onSearch();
    } if (event.keyCode === Ci.nsIDOMKeyEvent.DOM_VK_G && event.metaKey) {
      this._onSearch(event.shiftKey);
      event.preventDefault();
    }
  }
};

/**
 * Converts any input box on a page to a CSS selector search and suggestion box.
 *
 * Emits 'processing-done' event when it is done processing the current
 * keypress, search request or selection from the list, whether that led to a
 * search or not.
 *
 * @constructor
 * @param InspectorPanel inspector
 *        The InspectorPanel whose `walker` attribute should be used for
 *        document traversal.
 * @param nsiInputElement inputNode
 *        The input element to which the panel will be attached and from where
 *        search input will be taken.
 */
function SelectorAutocompleter(inspector, inputNode) {
  this.inspector = inspector;
  this.searchBox = inputNode;
  this.panelDoc = this.searchBox.ownerDocument;

  this.showSuggestions = this.showSuggestions.bind(this);
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

  this.searchBox.addEventListener("input", this.showSuggestions, true);
  this.searchBox.addEventListener("keypress", this._onSearchKeypress, true);
  this.inspector.on("markupmutation", this._onMarkupMutation);

  // For testing, we need to be able to wait for the most recent node request
  // to finish.  Tests can watch this promise for that.
  this._lastQuery = promise.resolve(null);
  EventEmitter.decorate(this);
}

exports.SelectorAutocompleter = SelectorAutocompleter;

SelectorAutocompleter.prototype = {
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
    this.searchBox.removeEventListener("input", this.showSuggestions, true);
    this.searchBox.removeEventListener("keypress", this._onSearchKeypress, true);
    this.inspector.off("markupmutation", this._onMarkupMutation);
    this.searchPopup.destroy();
    this.searchPopup = null;
    this.searchBox = null;
    this.panelDoc = null;
  },

  /**
   * Handles keypresses inside the input box.
   */
  _onSearchKeypress: function(event) {
    let query = this.searchBox.value;
    switch(event.keyCode) {
      case event.DOM_VK_RETURN:
      case event.DOM_VK_TAB:
        if (this.searchPopup.isOpen &&
            this.searchPopup.getItemAtIndex(this.searchPopup.itemCount - 1)
                .preLabel == query) {
          this.searchPopup.selectedIndex = this.searchPopup.itemCount - 1;
          this.searchBox.value = this.searchPopup.selectedItem.label;
          this.hidePopup();
        }
        break;

      case event.DOM_VK_UP:
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
        break;

      case event.DOM_VK_DOWN:
        if (this.searchPopup.isOpen && this.searchPopup.itemCount > 0) {
          this.searchPopup.focus();
          this.searchPopup.selectedIndex = 0;
          this.searchBox.value = this.searchPopup.selectedItem.label;
        }
        break;

      default:
        return;
    }

    event.preventDefault();
    event.stopPropagation();
    this.emit("processing-done");
  },

  /**
   * Handles keypress and mouse click on the suggestions richlistbox.
   */
  _onListBoxKeypress: function(event) {
    switch(event.keyCode || event.button) {
      case event.DOM_VK_RETURN:
      case event.DOM_VK_TAB:
      case 0: // left mouse button
        event.stopPropagation();
        event.preventDefault();
        this.searchBox.value = this.searchPopup.selectedItem.label;
        this.searchBox.focus();
        this.hidePopup();
        break;

      case event.DOM_VK_UP:
        if (this.searchPopup.selectedIndex == 0) {
          this.searchPopup.selectedIndex = -1;
          event.stopPropagation();
          event.preventDefault();
          this.searchBox.focus();
        }
        else {
          let index = this.searchPopup.selectedIndex;
          this.searchBox.value = this.searchPopup.getItemAtIndex(index - 1).label;
        }
        break;

      case event.DOM_VK_DOWN:
        if (this.searchPopup.selectedIndex == this.searchPopup.itemCount - 1) {
          this.searchPopup.selectedIndex = -1;
          event.stopPropagation();
          event.preventDefault();
          this.searchBox.focus();
        }
        else {
          let index = this.searchPopup.selectedIndex;
          this.searchBox.value = this.searchPopup.getItemAtIndex(index + 1).label;
        }
        break;

      case event.DOM_VK_BACK_SPACE:
        event.stopPropagation();
        event.preventDefault();
        this.searchBox.focus();
        if (this.searchBox.selectionStart > 0) {
          this.searchBox.value =
            this.searchBox.value.substring(0, this.searchBox.selectionStart - 1);
        }
        this.hidePopup();
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
  _showPopup: function(list, firstPart, aState) {
    let total = 0;
    let query = this.searchBox.value;
    let items = [];

    for (let [value, /*count*/, state] of list) {
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
        label: value
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
      this.hidePopup();
    }
  },


  /**
   * Hide the suggestion popup if necessary.
   */
  hidePopup: function() {
    if (this.searchPopup.isOpen) {
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

    if (state === this.States.TAG) {
      // gets the tag that is being completed. For ex. 'div.foo > s' returns 's',
      // 'di' returns 'di' and likewise.
      firstPart = (query.match(/[\s>+]?([a-zA-Z]*)$/) || ["", query])[1];
      query = query.slice(0, query.length - firstPart.length);
    }
    else if (state === this.States.CLASS) {
      // gets the class that is being completed. For ex. '.foo.b' returns 'b'
      firstPart = query.match(/\.([^\.]*)$/)[1];
      query = query.slice(0, query.length - firstPart.length - 1);
    }
    else if (state === this.States.ID) {
      // gets the id that is being completed. For ex. '.foo#b' returns 'b'
      firstPart = query.match(/#([^#]*)$/)[1];
      query = query.slice(0, query.length - firstPart.length - 1);
    }
    // TODO: implement some caching so that over the wire request is not made
    // everytime.
    if (/[\s+>~]$/.test(query)) {
      query += "*";
    }

    this._lastQuery = this.walker.getSuggestionsForQuery(query, firstPart, state).then(result => {
      this.emit("processing-done");
      if (result.query !== query) {
        // This means that this response is for a previous request and the user
        // as since typed something extra leading to a new request.
        return;
      }

      if (state === this.States.CLASS) {
        firstPart = "." + firstPart;
      } else if (state === this.States.ID) {
        firstPart = "#" + firstPart;
      }

      // If there is a single tag match and it's what the user typed, then
      // don't need to show a popup.
      if (result.suggestions.length === 1 &&
          result.suggestions[0][0] === firstPart) {
        result.suggestions = [];
      }


      this._showPopup(result.suggestions, firstPart, state);
    });

    return this._lastQuery;
  }
};
