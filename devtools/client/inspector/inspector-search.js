/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const promise = require("promise");
const {Task} = require("devtools/shared/task");
const {KeyCodes} = require("devtools/client/shared/keycodes");

const EventEmitter = require("devtools/shared/event-emitter");
const AutocompletePopup = require("devtools/client/shared/autocomplete-popup");
const Services = require("Services");

// Maximum number of selector suggestions shown in the panel.
const MAX_SUGGESTIONS = 15;

/**
 * Converts any input field into a document search box.
 *
 * @param {InspectorPanel} inspector
 *        The InspectorPanel whose `walker` attribute should be used for
 *        document traversal.
 * @param {DOMNode} input
 *        The input element to which the panel will be attached and from where
 *        search input will be taken.
 * @param {DOMNode} clearBtn
 *        The clear button in the input field that will clear the input value.
 *
 * Emits the following events:
 * - search-cleared: when the search box is emptied
 * - search-result: when a search is made and a result is selected
 */
function InspectorSearch(inspector, input, clearBtn) {
  this.inspector = inspector;
  this.searchBox = input;
  this.searchClearButton = clearBtn;
  this._lastSearched = null;

  this.searchClearButton.hidden = true;

  this._onKeyDown = this._onKeyDown.bind(this);
  this._onInput = this._onInput.bind(this);
  this._onClearSearch = this._onClearSearch.bind(this);
  this.searchBox.addEventListener("keydown", this._onKeyDown, true);
  this.searchBox.addEventListener("input", this._onInput, true);
  this.searchBox.addEventListener("contextmenu", this.inspector.onTextBoxContextMenu);
  this.searchClearButton.addEventListener("click", this._onClearSearch);

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

  destroy: function () {
    this.searchBox.removeEventListener("keydown", this._onKeyDown, true);
    this.searchBox.removeEventListener("input", this._onInput, true);
    this.searchBox.removeEventListener("contextmenu",
      this.inspector.onTextBoxContextMenu);
    this.searchClearButton.removeEventListener("click", this._onClearSearch);
    this.searchBox = null;
    this.searchClearButton = null;
    this.autocompleter.destroy();
  },

  _onSearch: function (reverse = false) {
    this.doFullTextSearch(this.searchBox.value, reverse)
        .catch(e => console.error(e));
  },

  doFullTextSearch: Task.async(function* (query, reverse) {
    let lastSearched = this._lastSearched;
    this._lastSearched = query;

    if (query.length === 0) {
      this.searchBox.classList.remove("devtools-style-searchbox-no-match");
      if (!lastSearched || lastSearched.length > 0) {
        this.emit("search-cleared");
      }
      return;
    }

    let res = yield this.walker.search(query, { reverse });

    // Value has changed since we started this request, we're done.
    if (query !== this.searchBox.value) {
      return;
    }

    if (res) {
      this.inspector.selection.setNodeFront(res.node, "inspectorsearch");
      this.searchBox.classList.remove("devtools-style-searchbox-no-match");

      res.query = query;
      this.emit("search-result", res);
    } else {
      this.searchBox.classList.add("devtools-style-searchbox-no-match");
      this.emit("search-result");
    }
  }),

  _onInput: function () {
    if (this.searchBox.value.length === 0) {
      this.searchClearButton.hidden = true;
      this._onSearch();
    } else {
      this.searchClearButton.hidden = false;
    }
  },

  _onKeyDown: function (event) {
    if (event.keyCode === KeyCodes.DOM_VK_RETURN) {
      this._onSearch(event.shiftKey);
    }

    const modifierKey = Services.appinfo.OS === "Darwin"
                        ? event.metaKey : event.ctrlKey;
    if (event.keyCode === KeyCodes.DOM_VK_G && modifierKey) {
      this._onSearch(event.shiftKey);
      event.preventDefault();
    }
  },

  _onClearSearch: function () {
    this.searchBox.classList.remove("devtools-style-searchbox-no-match");
    this.searchBox.value = "";
    this.searchClearButton.hidden = true;
    this.emit("search-cleared");
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
  this._onSearchPopupClick = this._onSearchPopupClick.bind(this);
  this._onMarkupMutation = this._onMarkupMutation.bind(this);

  // Options for the AutocompletePopup.
  let options = {
    listId: "searchbox-panel-listbox",
    autoSelect: true,
    position: "top",
    theme: "auto",
    onClick: this._onSearchPopupClick,
  };

  // The popup will be attached to the toolbox document.
  this.searchPopup = new AutocompletePopup(inspector._toolbox.doc, options);

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
    ATTRIBUTE: "attribute",
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
    // Now we iterate over the query and decide the state character by
    // character.
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

        case this.States.TAG: // eslint-disable-line
          if (lastChar === ".") {
            this._state = this.States.CLASS;
          } else if (lastChar === "#") {
            this._state = this.States.ID;
          } else if (lastChar === "[") {
            this._state = this.States.ATTRIBUTE;
          } else {
            this._state = this.States.TAG;
          }
          break;

        case this.States.CLASS:
          if (subQuery.match(/[\.]+[^\.]*$/)[0].length > 2) {
            // Checks whether the subQuery has atleast one [a-zA-Z] after the
            // '.'.
            if (lastChar === " " || lastChar === ">") {
              this._state = this.States.TAG;
            } else if (lastChar === "#") {
              this._state = this.States.ID;
            } else if (lastChar === "[") {
              this._state = this.States.ATTRIBUTE;
            } else {
              this._state = this.States.CLASS;
            }
          }
          break;

        case this.States.ID:
          if (subQuery.match(/[#]+[^#]*$/)[0].length > 2) {
            // Checks whether the subQuery has atleast one [a-zA-Z] after the
            // '#'.
            if (lastChar === " " || lastChar === ">") {
              this._state = this.States.TAG;
            } else if (lastChar === ".") {
              this._state = this.States.CLASS;
            } else if (lastChar === "[") {
              this._state = this.States.ATTRIBUTE;
            } else {
              this._state = this.States.ID;
            }
          }
          break;

        case this.States.ATTRIBUTE:
          if (subQuery.match(/[\[][^\]]+[\]]/) !== null) {
            // Checks whether the subQuery has at least one ']' after the '['.
            if (lastChar === " " || lastChar === ">") {
              this._state = this.States.TAG;
            } else if (lastChar === ".") {
              this._state = this.States.CLASS;
            } else if (lastChar === "#") {
              this._state = this.States.ID;
            } else {
              this._state = this.States.ATTRIBUTE;
            }
          }
          break;
      }
    }
    return this._state;
  },

  /**
   * Removes event listeners and cleans up references.
   */
  destroy: function () {
    this.searchBox.removeEventListener("input", this.showSuggestions, true);
    this.searchBox.removeEventListener("keypress",
      this._onSearchKeypress, true);
    this.inspector.off("markupmutation", this._onMarkupMutation);
    this.searchPopup.destroy();
    this.searchPopup = null;
    this.searchBox = null;
    this.panelDoc = null;
  },

  /**
   * Handles keypresses inside the input box.
   */
  _onSearchKeypress: function (event) {
    let popup = this.searchPopup;

    switch (event.keyCode) {
      case KeyCodes.DOM_VK_RETURN:
      case KeyCodes.DOM_VK_TAB:
        if (popup.isOpen) {
          if (popup.selectedItem) {
            this.searchBox.value = popup.selectedItem.label;
          }
          this.hidePopup();
        } else if (!popup.isOpen) {
          // When tab is pressed with focus on searchbox and closed popup,
          // do not prevent the default to avoid a keyboard trap and move focus
          // to next/previous element.
          this.emit("processing-done");
          return;
        }
        break;

      case KeyCodes.DOM_VK_UP:
        if (popup.isOpen && popup.itemCount > 0) {
          if (popup.selectedIndex === 0) {
            popup.selectedIndex = popup.itemCount - 1;
          } else {
            popup.selectedIndex--;
          }
          this.searchBox.value = popup.selectedItem.label;
        }
        break;

      case KeyCodes.DOM_VK_DOWN:
        if (popup.isOpen && popup.itemCount > 0) {
          if (popup.selectedIndex === popup.itemCount - 1) {
            popup.selectedIndex = 0;
          } else {
            popup.selectedIndex++;
          }
          this.searchBox.value = popup.selectedItem.label;
        }
        break;

      case KeyCodes.DOM_VK_ESCAPE:
        if (popup.isOpen) {
          this.hidePopup();
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
   * Handles click events from the autocomplete popup.
   */
  _onSearchPopupClick: function (event) {
    let selectedItem = this.searchPopup.selectedItem;
    if (selectedItem) {
      this.searchBox.value = selectedItem.label;
    }
    this.hidePopup();

    event.preventDefault();
    event.stopPropagation();
  },

  /**
   * Reset previous search results on markup-mutations to make sure we search
   * again after nodes have been added/removed/changed.
   */
  _onMarkupMutation: function () {
    this._searchResults = null;
    this._lastSearched = null;
  },

  /**
   * Populates the suggestions list and show the suggestion popup.
   *
   * @return {Promise} promise that will resolve when the autocomplete popup is fully
   * displayed or hidden.
   */
  _showPopup: function (list, firstPart, popupState) {
    let total = 0;
    let query = this.searchBox.value;
    let items = [];

    for (let [value, , state] of list) {
      if (query.match(/[\s>+]$/)) {
        // for cases like 'div ' or 'div >' or 'div+'
        value = query + value;
      } else if (query.match(/[\s>+][\.#a-zA-Z][^\s>+\.#\[]*$/)) {
        // for cases like 'div #a' or 'div .a' or 'div > d' and likewise
        let lastPart = query.match(/[\s>+][\.#a-zA-Z][^\s>+\.#\[]*$/)[0];
        value = query.slice(0, -1 * lastPart.length + 1) + value;
      } else if (query.match(/[a-zA-Z][#\.][^#\.\s+>]*$/)) {
        // for cases like 'div.class' or '#foo.bar' and likewise
        let lastPart = query.match(/[a-zA-Z][#\.][^#\.\s+>]*$/)[0];
        value = query.slice(0, -1 * lastPart.length + 1) + value;
      } else if (query.match(/[a-zA-Z]*\[[^\]]*\][^\]]*/)) {
        // for cases like '[foo].bar' and likewise
        let attrPart = query.substring(0, query.lastIndexOf("]") + 1);
        value = attrPart + value;
      }

      let item = {
        preLabel: query,
        label: value
      };

      // In case the query's state is tag and the item's state is id or class
      // adjust the preLabel
      if (popupState === this.States.TAG && state === this.States.CLASS) {
        item.preLabel = "." + item.preLabel;
      }
      if (popupState === this.States.TAG && state === this.States.ID) {
        item.preLabel = "#" + item.preLabel;
      }

      items.unshift(item);
      if (++total > MAX_SUGGESTIONS - 1) {
        break;
      }
    }

    if (total > 0) {
      let onPopupOpened = this.searchPopup.once("popup-opened");
      this.searchPopup.once("popup-closed", () => {
        this.searchPopup.setItems(items);
        this.searchPopup.openPopup(this.searchBox);
      });
      this.searchPopup.hidePopup();
      return onPopupOpened;
    }

    return this.hidePopup();
  },

  /**
   * Hide the suggestion popup if necessary.
   */
  hidePopup: function () {
    let onPopupClosed = this.searchPopup.once("popup-closed");
    this.searchPopup.hidePopup();
    return onPopupClosed;
  },

  /**
   * Suggests classes,ids and tags based on the user input as user types in the
   * searchbox.
   */
  showSuggestions: function () {
    let query = this.searchBox.value;
    let state = this.state;
    let firstPart = "";

    if (query.endsWith("*") || state === this.States.ATTRIBUTE) {
      // Hide the popup if the query ends with * (because we don't want to
      // suggest all nodes) or if it is an attribute selector (because
      // it would give a lot of useless results).
      this.hidePopup();
      return;
    }

    if (state === this.States.TAG) {
      // gets the tag that is being completed. For ex. 'div.foo > s' returns
      // 's', 'di' returns 'di' and likewise.
      firstPart = (query.match(/[\s>+]?([a-zA-Z]*)$/) || ["", query])[1];
      query = query.slice(0, query.length - firstPart.length);
    } else if (state === this.States.CLASS) {
      // gets the class that is being completed. For ex. '.foo.b' returns 'b'
      firstPart = query.match(/\.([^\.]*)$/)[1];
      query = query.slice(0, query.length - firstPart.length - 1);
    } else if (state === this.States.ID) {
      // gets the id that is being completed. For ex. '.foo#b' returns 'b'
      firstPart = query.match(/#([^#]*)$/)[1];
      query = query.slice(0, query.length - firstPart.length - 1);
    }
    // TODO: implement some caching so that over the wire request is not made
    // everytime.
    if (/[\s+>~]$/.test(query)) {
      query += "*";
    }

    let suggestionsPromise = this.walker.getSuggestionsForQuery(
      query, firstPart, state);
    this._lastQuery = suggestionsPromise.then(result => {
      this.emit("processing-done");
      if (result.query !== query) {
        // This means that this response is for a previous request and the user
        // as since typed something extra leading to a new request.
        return promise.resolve(null);
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

      // Wait for the autocomplete-popup to fire its popup-opened event, to make sure
      // the autoSelect item has been selected.
      return this._showPopup(result.suggestions, firstPart, state);
    });
  }
};
