/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.ContentSearchUIController = (function() {

const MAX_DISPLAYED_SUGGESTIONS = 6;
const SUGGESTION_ID_PREFIX = "searchSuggestion";
const ONE_OFF_ID_PREFIX = "oneOff";

const HTML_NS = "http://www.w3.org/1999/xhtml";

/**
 * Creates a new object that manages search suggestions and their UI for a text
 * box.
 *
 * The UI consists of an html:table that's inserted into the DOM after the given
 * text box and styled so that it appears as a dropdown below the text box.
 *
 * @param inputElement
 *        Search suggestions will be based on the text in this text box.
 *        Assumed to be an html:input.  xul:textbox is untested but might work.
 * @param tableParent
 *        The suggestion table is appended as a child to this element.  Since
 *        the table is absolutely positioned and its top and left values are set
 *        to be relative to the top and left of the page, either the parent and
 *        all its ancestors should not be positioned elements (i.e., their
 *        positions should be "static"), or the parent's position should be the
 *        top left of the page.
 * @param healthReportKey
 *        This will be sent with the search data for FHR to record the search.
 * @param searchPurpose
 *        Sent with search data, see nsISearchEngine.getSubmission.
 * @param idPrefix
 *        The IDs of elements created by the object will be prefixed with this
 *        string.
 */
function ContentSearchUIController(inputElement, tableParent, healthReportKey,
                                   searchPurpose, idPrefix = "") {
  this.input = inputElement;
  this._idPrefix = idPrefix;
  this._healthReportKey = healthReportKey;
  this._searchPurpose = searchPurpose;

  let tableID = idPrefix + "searchSuggestionTable";
  this.input.autocomplete = "off";
  this.input.setAttribute("aria-autocomplete", "true");
  this.input.setAttribute("aria-controls", tableID);
  tableParent.appendChild(this._makeTable(tableID));

  this.input.addEventListener("keypress", this);
  this.input.addEventListener("input", this);
  this.input.addEventListener("focus", this);
  this.input.addEventListener("blur", this);
  window.addEventListener("ContentSearchService", this);

  this._stickyInputValue = "";
  this._hideSuggestions();

  this._getSearchEngines();
  this._getStrings();
}

ContentSearchUIController.prototype = {

  _oneOffButtons: [],
  // Setting up the one off buttons causes an uninterruptible reflow. If we
  // receive the list of engines while the newtab page is loading, this reflow
  // may regress performance - so we set this flag and only set up the buttons
  // if it's set when the suggestions table is actually opened.
  _pendingOneOffRefresh: undefined,

  get defaultEngine() {
    return this._defaultEngine;
  },

  set defaultEngine(engine) {
    if (this._defaultEngine && this._defaultEngine.icon) {
      URL.revokeObjectURL(this._defaultEngine.icon);
    }
    let icon;
    if (engine.iconBuffer) {
      icon = this._getFaviconURIFromBuffer(engine.iconBuffer);
    } else {
      icon = "chrome://mozapps/skin/places/defaultFavicon.svg";
    }
    this._defaultEngine = {
      name: engine.name,
      icon,
    };
    this._updateDefaultEngineHeader();

    if (engine && document.activeElement == this.input) {
      this._speculativeConnect();
    }
  },

  get engines() {
    return this._engines;
  },

  set engines(val) {
    this._engines = val;
    this._pendingOneOffRefresh = true;
  },

  // The selectedIndex is the index of the element with the "selected" class in
  // the list obtained by concatenating the suggestion rows, one-off buttons, and
  // search settings button.
  get selectedIndex() {
    let allElts = [...this._suggestionsList.children,
                   ...this._oneOffButtons,
                   document.getElementById("contentSearchSettingsButton")];
    for (let i = 0; i < allElts.length; ++i) {
      let elt = allElts[i];
      if (elt.classList.contains("selected")) {
        return i;
      }
    }
    return -1;
  },

  set selectedIndex(idx) {
    // Update the table's rows, and the input when there is a selection.
    this._table.removeAttribute("aria-activedescendant");
    this.input.removeAttribute("aria-activedescendant");

    let allElts = [...this._suggestionsList.children,
                   ...this._oneOffButtons,
                   document.getElementById("contentSearchSettingsButton")];
    // If we are selecting a suggestion and a one-off is selected, don't deselect it.
    let excludeIndex = idx < this.numSuggestions && this.selectedButtonIndex > -1 ?
                       this.numSuggestions + this.selectedButtonIndex : -1;
    for (let i = 0; i < allElts.length; ++i) {
      let elt = allElts[i];
      let ariaSelectedElt = i < this.numSuggestions ? elt.firstChild : elt;
      if (i == idx) {
        elt.classList.add("selected");
        ariaSelectedElt.setAttribute("aria-selected", "true");
        this.input.setAttribute("aria-activedescendant", ariaSelectedElt.id);
      } else if (i != excludeIndex) {
        elt.classList.remove("selected");
        ariaSelectedElt.setAttribute("aria-selected", "false");
      }
    }
  },

  get selectedButtonIndex() {
    let elts = [...this._oneOffButtons,
                document.getElementById("contentSearchSettingsButton")];
    for (let i = 0; i < elts.length; ++i) {
      if (elts[i].classList.contains("selected")) {
        return i;
      }
    }
    return -1;
  },

  set selectedButtonIndex(idx) {
    let elts = [...this._oneOffButtons,
                document.getElementById("contentSearchSettingsButton")];
    for (let i = 0; i < elts.length; ++i) {
      let elt = elts[i];
      if (i == idx) {
        elt.classList.add("selected");
        elt.setAttribute("aria-selected", "true");
      } else {
        elt.classList.remove("selected");
        elt.setAttribute("aria-selected", "false");
      }
    }
  },

  get selectedEngineName() {
    let selectedElt = this._oneOffsTable.querySelector(".selected");
    if (selectedElt) {
      return selectedElt.engineName;
    }
    return this.defaultEngine.name;
  },

  get numSuggestions() {
    return this._suggestionsList.children.length;
  },

  selectAndUpdateInput(idx) {
    this.selectedIndex = idx;
    let newValue = this.suggestionAtIndex(idx) || this._stickyInputValue;
    // Setting the input value when the value has not changed commits the current
    // IME composition, which we don't want to do.
    if (this.input.value != newValue) {
      this.input.value = newValue;
    }
    this._updateSearchWithHeader();
  },

  suggestionAtIndex(idx) {
    let row = this._suggestionsList.children[idx];
    return row ? row.textContent : null;
  },

  deleteSuggestionAtIndex(idx) {
    // Only form history suggestions can be deleted.
    if (this.isFormHistorySuggestionAtIndex(idx)) {
      let suggestionStr = this.suggestionAtIndex(idx);
      this._sendMsg("RemoveFormHistoryEntry", suggestionStr);
      this._suggestionsList.children[idx].remove();
      this.selectAndUpdateInput(-1);
    }
  },

  isFormHistorySuggestionAtIndex(idx) {
    let row = this._suggestionsList.children[idx];
    return row && row.classList.contains("formHistory");
  },

  addInputValueToFormHistory() {
    this._sendMsg("AddFormHistoryEntry", this.input.value);
  },

  handleEvent(event) {
    this["_on" + event.type[0].toUpperCase() + event.type.substr(1)](event);
  },

  _onCommand(aEvent) {
    if (this.selectedButtonIndex == this._oneOffButtons.length) {
      // Settings button was selected.
      this._sendMsg("ManageEngines");
      return;
    }

    this.search(aEvent);

    if (aEvent) {
      aEvent.preventDefault();
    }
  },

  search(aEvent) {
    if (!this.defaultEngine) {
      return; // Not initialized yet.
    }

    let searchText = this.input;
    let searchTerms;
    if (this._table.hidden ||
        (aEvent.originalTarget &&
          aEvent.originalTarget.id == "contentSearchDefaultEngineHeader") ||
        aEvent instanceof KeyboardEvent) {
      searchTerms = searchText.value;
    } else {
      searchTerms = this.suggestionAtIndex(this.selectedIndex) || searchText.value;
    }
    // Send an event that will perform a search and Firefox Health Report will
    // record that a search from the healthReportKey passed to the constructor.
    let eventData = {
      engineName: this.selectedEngineName,
      searchString: searchTerms,
      healthReportKey: this._healthReportKey,
      searchPurpose: this._searchPurpose,
      originalEvent: {
        shiftKey: aEvent.shiftKey,
        ctrlKey: aEvent.ctrlKey,
        metaKey: aEvent.metaKey,
        altKey: aEvent.altKey,
      },
    };
    if ("button" in aEvent) {
      eventData.originalEvent.button = aEvent.button;
    }

    if (this.suggestionAtIndex(this.selectedIndex)) {
      eventData.selection = {
        index: this.selectedIndex,
        kind: undefined,
      };
      if (aEvent instanceof MouseEvent) {
        eventData.selection.kind = "mouse";
      } else if (aEvent instanceof KeyboardEvent) {
        eventData.selection.kind = "key";
      }
    }

    this._sendMsg("Search", eventData);
    this.addInputValueToFormHistory();
  },

  _onInput() {
    if (!this.input.value) {
      this._stickyInputValue = "";
      this._hideSuggestions();
    } else if (this.input.value != this._stickyInputValue) {
      // Only fetch new suggestions if the input value has changed.
      this._getSuggestions();
      this.selectAndUpdateInput(-1);
    }
    this._updateSearchWithHeader();
  },

  _onKeypress(event) {
    let selectedIndexDelta = 0;
    let selectedSuggestionDelta = 0;
    let selectedOneOffDelta = 0;

    switch (event.keyCode) {
    case event.DOM_VK_UP:
      if (this._table.hidden) {
        return;
      }
      if (event.getModifierState("Accel")) {
        if (event.shiftKey) {
          selectedSuggestionDelta = -1;
          break;
        }
        this._cycleCurrentEngine(true);
        break;
      }
      if (event.altKey) {
        selectedOneOffDelta = -1;
        break;
      }
      selectedIndexDelta = -1;
      break;
    case event.DOM_VK_DOWN:
      if (this._table.hidden) {
        this._getSuggestions();
        return;
      }
      if (event.getModifierState("Accel")) {
        if (event.shiftKey) {
          selectedSuggestionDelta = 1;
          break;
        }
        this._cycleCurrentEngine(false);
        break;
      }
      if (event.altKey) {
        selectedOneOffDelta = 1;
        break;
      }
      selectedIndexDelta = 1;
      break;
    case event.DOM_VK_TAB:
      if (this._table.hidden) {
        return;
      }
      // Shift+tab when either the first or no one-off is selected, as well as
      // tab when the settings button is selected, should change focus as normal.
      if ((this.selectedButtonIndex <= 0 && event.shiftKey) ||
          this.selectedButtonIndex == this._oneOffButtons.length && !event.shiftKey) {
        return;
      }
      selectedOneOffDelta = event.shiftKey ? -1 : 1;
      break;
    case event.DOM_VK_RIGHT:
      // Allow normal caret movement until the caret is at the end of the input.
      if (this.input.selectionStart != this.input.selectionEnd ||
          this.input.selectionEnd != this.input.value.length) {
        return;
      }
      if (this.numSuggestions && this.selectedIndex >= 0 &&
          this.selectedIndex < this.numSuggestions) {
        this.input.value = this.suggestionAtIndex(this.selectedIndex);
        this.input.setAttribute("selection-index", this.selectedIndex);
        this.input.setAttribute("selection-kind", "key");
      } else {
        // If we didn't select anything, make sure to remove the attributes
        // in case they were populated last time.
        this.input.removeAttribute("selection-index");
        this.input.removeAttribute("selection-kind");
      }
      this._stickyInputValue = this.input.value;
      this._hideSuggestions();
      return;
    case event.DOM_VK_RETURN:
      this._onCommand(event);
      return;
    case event.DOM_VK_DELETE:
      if (this.selectedIndex >= 0) {
        this.deleteSuggestionAtIndex(this.selectedIndex);
      }
      return;
    case event.DOM_VK_ESCAPE:
      if (!this._table.hidden) {
        this._hideSuggestions();
      }
      return;
    default:
      return;
    }

    let currentIndex = this.selectedIndex;
    if (selectedIndexDelta) {
      let newSelectedIndex = currentIndex + selectedIndexDelta;
      if (newSelectedIndex < -1) {
        newSelectedIndex = this.numSuggestions + this._oneOffButtons.length;
      }
      // If are moving up from the first one off, we have to deselect the one off
      // manually because the selectedIndex setter tries to exclude the selected
      // one-off (which is desirable for accel+shift+up/down).
      if (currentIndex == this.numSuggestions && selectedIndexDelta == -1) {
        this.selectedButtonIndex = -1;
      }
      this.selectAndUpdateInput(newSelectedIndex);
    } else if (selectedSuggestionDelta) {
      let newSelectedIndex;
      if (currentIndex >= this.numSuggestions || currentIndex == -1) {
        // No suggestion already selected, select the first/last one appropriately.
        newSelectedIndex = selectedSuggestionDelta == 1 ?
                           0 : this.numSuggestions - 1;
      } else {
        newSelectedIndex = currentIndex + selectedSuggestionDelta;
      }
      if (newSelectedIndex >= this.numSuggestions) {
        newSelectedIndex = -1;
      }
      this.selectAndUpdateInput(newSelectedIndex);
    } else if (selectedOneOffDelta) {
      let newSelectedIndex;
      let currentButton = this.selectedButtonIndex;
      if (currentButton == -1 || currentButton == this._oneOffButtons.length) {
        // No one-off already selected, select the first/last one appropriately.
        newSelectedIndex = selectedOneOffDelta == 1 ?
                           0 : this._oneOffButtons.length - 1;
      } else {
        newSelectedIndex = currentButton + selectedOneOffDelta;
      }
      // Allow selection of the settings button via the tab key.
      if (newSelectedIndex == this._oneOffButtons.length &&
          event.keyCode != event.DOM_VK_TAB) {
        newSelectedIndex = -1;
      }
      this.selectedButtonIndex = newSelectedIndex;
    }

    // Prevent the input's caret from moving.
    event.preventDefault();
  },

  _currentEngineIndex: -1,
  _cycleCurrentEngine(aReverse) {
    if ((this._currentEngineIndex == this._engines.length - 1 && !aReverse) ||
        (this._currentEngineIndex == 0 && aReverse)) {
      return;
    }
    this._currentEngineIndex += aReverse ? -1 : 1;
    let engineName = this._engines[this._currentEngineIndex].name;
    this._sendMsg("SetCurrentEngine", engineName);
  },

  _onFocus() {
    if (this._mousedown) {
      return;
    }
    // When the input box loses focus to something in our table, we refocus it
    // immediately. This causes the focus highlight to flicker, so we set a
    // custom attribute which consumers should use for focus highlighting. This
    // attribute is removed only when we do not immediately refocus the input
    // box, thus eliminating flicker.
    this.input.setAttribute("keepfocus", "true");
    this._speculativeConnect();
  },

  _onBlur() {
    if (this._mousedown) {
      // At this point, this.input has lost focus, but a new element has not yet
      // received it. If we re-focus this.input directly, the new element will
      // steal focus immediately, so we queue it instead.
      setTimeout(() => this.input.focus(), 0);
      return;
    }
    this.input.removeAttribute("keepfocus");
    this._hideSuggestions();
  },

  _onMousemove(event) {
    let idx = this._indexOfTableItem(event.target);
    if (idx >= this.numSuggestions) {
      // Deselect any search suggestion that has been selected.
      this.selectedIndex = -1;
      this.selectedButtonIndex = idx - this.numSuggestions;
      return;
    }
    this.selectedIndex = idx;
  },

  _onMouseup(event) {
    if (event.button == 2) {
      return;
    }
    this._onCommand(event);
  },

  _onMouseout(event) {
    // We only deselect one-off buttons and the settings button when they are
    // moused out.
    let idx = this._indexOfTableItem(event.originalTarget);
    if (idx >= this.numSuggestions) {
      this.selectedButtonIndex = -1;
    }
  },

  _onClick(event) {
    this._onMouseup(event);
  },

  _onContentSearchService(event) {
    let methodName = "_onMsg" + event.detail.type;
    if (methodName in this) {
      this[methodName](event.detail.data);
    }
  },

  _onMsgFocusInput(event) {
    this.input.focus();
  },

  _onMsgBlur(event) {
    this.input.blur();
    this._hideSuggestions();
  },

  _onMsgSuggestions(suggestions) {
    // Ignore the suggestions if their search string or engine doesn't match
    // ours.  Due to the async nature of message passing, this can easily happen
    // when the user types quickly.
    if (this._stickyInputValue != suggestions.searchString ||
        this.defaultEngine.name != suggestions.engineName) {
      return;
    }

    this._clearSuggestionRows();

    // Position and size the table.
    let { left } = this.input.getBoundingClientRect();
    this._table.style.top = this.input.offsetHeight + "px";
    this._table.style.minWidth = this.input.offsetWidth + "px";
    this._table.style.maxWidth = (window.innerWidth - left - 40) + "px";

    // Add the suggestions to the table.
    let searchWords =
      new Set(suggestions.searchString.trim().toLowerCase().split(/\s+/));
    for (let i = 0; i < MAX_DISPLAYED_SUGGESTIONS; i++) {
      let type, idx;
      if (i < suggestions.formHistory.length) {
        [type, idx] = ["formHistory", i];
      } else {
        let j = i - suggestions.formHistory.length;
        if (j < suggestions.remote.length) {
          [type, idx] = ["remote", j];
        } else {
          break;
        }
      }
      this._suggestionsList.appendChild(
        this._makeTableRow(type, suggestions[type][idx], i, searchWords));
    }

    if (this._table.hidden) {
      this.selectedIndex = -1;
      if (this._pendingOneOffRefresh) {
        this._setUpOneOffButtons();
        delete this._pendingOneOffRefresh;
      }
      this._currentEngineIndex =
        this._engines.findIndex(aEngine => aEngine.name == this.defaultEngine.name);
      this._table.hidden = false;
      this.input.setAttribute("aria-expanded", "true");
      this._originalDefaultEngine = {
        name: this.defaultEngine.name,
        icon: this.defaultEngine.icon,
      };
    }
  },

  _onMsgSuggestionsCancelled() {
    if (!this._table.hidden) {
      this._hideSuggestions();
    }
  },

  _onMsgState(state) {
    this.engines = state.engines;
    // No point updating the default engine (and the header) if there's no change.
    if (this.defaultEngine &&
        this.defaultEngine.name == state.currentEngine.name &&
        this.defaultEngine.icon == state.currentEngine.icon) {
      return;
    }
    this.defaultEngine = state.currentEngine;
  },

  _onMsgCurrentState(state) {
    this._onMsgState(state);
  },

  _onMsgCurrentEngine(engine) {
    this.defaultEngine = engine;
    this._pendingOneOffRefresh = true;
  },

  _onMsgStrings(strings) {
    this._strings = strings;
    this._updateDefaultEngineHeader();
    this._updateSearchWithHeader();
    document.getElementById("contentSearchSettingsButton").textContent =
      this._strings.searchSettings;
  },

  _updateDefaultEngineHeader() {
    let header = document.getElementById("contentSearchDefaultEngineHeader");
    header.firstChild.setAttribute("src", this.defaultEngine.icon);
    if (!this._strings) {
      return;
    }
    while (header.firstChild.nextSibling) {
      header.firstChild.nextSibling.remove();
    }
    header.appendChild(document.createTextNode(
      this._strings.searchHeader.replace("%S", this.defaultEngine.name)));
  },

  _updateSearchWithHeader() {
    if (!this._strings) {
      return;
    }
    let searchWithHeader = document.getElementById("contentSearchSearchWithHeader");
    if (this.input.value) {
      // eslint-disable-next-line no-unsanitized/property
      searchWithHeader.innerHTML = this._strings.searchForSomethingWith;
      searchWithHeader.querySelector(".contentSearchSearchWithHeaderSearchText").textContent = this.input.value;
    } else {
      searchWithHeader.textContent = this._strings.searchWithHeader;
    }
  },

  _speculativeConnect() {
    if (this.defaultEngine) {
      this._sendMsg("SpeculativeConnect", this.defaultEngine.name);
    }
  },

  _makeTableRow(type, suggestionStr, currentRow, searchWords) {
    let row = document.createElementNS(HTML_NS, "tr");
    row.dir = "auto";
    row.classList.add("contentSearchSuggestionRow");
    row.classList.add(type);
    row.setAttribute("role", "presentation");
    row.addEventListener("mousemove", this);
    row.addEventListener("mouseup", this);

    let entry = document.createElementNS(HTML_NS, "td");
    let img = document.createElementNS(HTML_NS, "div");
    img.setAttribute("class", "historyIcon");
    entry.appendChild(img);
    entry.classList.add("contentSearchSuggestionEntry");
    entry.setAttribute("role", "option");
    entry.id = this._idPrefix + SUGGESTION_ID_PREFIX + currentRow;
    entry.setAttribute("aria-selected", "false");

    let suggestionWords = suggestionStr.trim().toLowerCase().split(/\s+/);
    for (let i = 0; i < suggestionWords.length; i++) {
      let word = suggestionWords[i];
      let wordSpan = document.createElementNS(HTML_NS, "span");
      if (searchWords.has(word)) {
        wordSpan.classList.add("typed");
      }
      wordSpan.textContent = word;
      entry.appendChild(wordSpan);
      if (i < suggestionWords.length - 1) {
        entry.appendChild(document.createTextNode(" "));
      }
    }

    row.appendChild(entry);
    return row;
  },

  // Converts favicon array buffer into a data URI.
  _getFaviconURIFromBuffer(buffer) {
    let blob = new Blob([buffer]);
    return URL.createObjectURL(blob);
  },

  // Adds "@2x" to the name of the given PNG url for "retina" screens.
  _getImageURIForCurrentResolution(uri) {
    if (window.devicePixelRatio > 1) {
      return uri.replace(/\.png$/, "@2x.png");
    }
    return uri;
  },

  _getSearchEngines() {
    this._sendMsg("GetState");
  },

  _getStrings() {
    this._sendMsg("GetStrings");
  },

  _getSuggestions() {
    this._stickyInputValue = this.input.value;
    if (this.defaultEngine) {
      this._sendMsg("GetSuggestions", {
        engineName: this.defaultEngine.name,
        searchString: this.input.value,
      });
    }
  },

  _clearSuggestionRows() {
    while (this._suggestionsList.firstElementChild) {
      this._suggestionsList.firstElementChild.remove();
    }
  },

  _hideSuggestions() {
    this.input.setAttribute("aria-expanded", "false");
    this.selectedIndex = -1;
    this.selectedButtonIndex = -1;
    this._currentEngineIndex = -1;
    this._table.hidden = true;
  },

  _indexOfTableItem(elt) {
    if (elt.classList.contains("contentSearchOneOffItem")) {
      return this.numSuggestions + this._oneOffButtons.indexOf(elt);
    }
    if (elt.classList.contains("contentSearchSettingsButton")) {
      return this.numSuggestions + this._oneOffButtons.length;
    }
    while (elt && elt.localName != "tr") {
      elt = elt.parentNode;
    }
    if (!elt) {
      throw new Error("Element is not a row");
    }
    return elt.rowIndex;
  },

  _makeTable(id) {
    this._table = document.createElementNS(HTML_NS, "table");
    this._table.id = id;
    this._table.hidden = true;
    this._table.classList.add("contentSearchSuggestionTable");
    this._table.setAttribute("role", "presentation");

    // When the search input box loses focus, we want to immediately give focus
    // back to it if the blur was because the user clicked somewhere in the table.
    // onBlur uses the _mousedown flag to detect this.
    this._table.addEventListener("mousedown", () => { this._mousedown = true; });
    document.addEventListener("mouseup", () => { delete this._mousedown; });

    // Deselect the selected element on mouseout if it wasn't a suggestion.
    this._table.addEventListener("mouseout", this);

    let headerRow = document.createElementNS(HTML_NS, "tr");
    let header = document.createElementNS(HTML_NS, "td");
    headerRow.setAttribute("class", "contentSearchHeaderRow");
    header.setAttribute("class", "contentSearchHeader");
    let iconImg = document.createElementNS(HTML_NS, "img");
    header.appendChild(iconImg);
    header.id = "contentSearchDefaultEngineHeader";
    headerRow.appendChild(header);
    headerRow.addEventListener("click", this);
    this._table.appendChild(headerRow);

    let row = document.createElementNS(HTML_NS, "tr");
    row.setAttribute("class", "contentSearchSuggestionsContainer");
    let cell = document.createElementNS(HTML_NS, "td");
    cell.setAttribute("class", "contentSearchSuggestionsContainer");
    this._suggestionsList = document.createElementNS(HTML_NS, "table");
    this._suggestionsList.setAttribute("class", "contentSearchSuggestionsList");
    cell.appendChild(this._suggestionsList);
    row.appendChild(cell);
    this._table.appendChild(row);
    this._suggestionsList.setAttribute("role", "listbox");

    this._oneOffsTable = document.createElementNS(HTML_NS, "table");
    this._oneOffsTable.setAttribute("class", "contentSearchOneOffsTable");
    this._oneOffsTable.classList.add("contentSearchSuggestionsContainer");
    this._oneOffsTable.setAttribute("role", "group");
    this._table.appendChild(this._oneOffsTable);

    headerRow = document.createElementNS(HTML_NS, "tr");
    header = document.createElementNS(HTML_NS, "td");
    headerRow.setAttribute("class", "contentSearchHeaderRow");
    header.setAttribute("class", "contentSearchHeader");
    headerRow.appendChild(header);
    header.id = "contentSearchSearchWithHeader";
    this._oneOffsTable.appendChild(headerRow);

    let button = document.createElementNS(HTML_NS, "button");
    button.setAttribute("class", "contentSearchSettingsButton");
    button.classList.add("contentSearchHeaderRow");
    button.classList.add("contentSearchHeader");
    button.id = "contentSearchSettingsButton";
    button.addEventListener("click", this);
    button.addEventListener("mousemove", this);
    this._table.appendChild(button);

    return this._table;
  },

  _setUpOneOffButtons() {
    // Sometimes we receive a CurrentEngine message from the ContentSearch service
    // before we've received a State message - i.e. before we have our engines.
    if (!this._engines) {
      return;
    }

    while (this._oneOffsTable.firstChild.nextSibling) {
      this._oneOffsTable.firstChild.nextSibling.remove();
    }

    this._oneOffButtons = [];

    let engines = this._engines.filter(aEngine => aEngine.name != this.defaultEngine.name)
                               .filter(aEngine => !aEngine.hidden);
    if (!engines.length) {
      this._oneOffsTable.hidden = true;
      return;
    }

    const kDefaultButtonWidth = 49; // 48px + 1px border.
    let rowWidth = this.input.offsetWidth - 2; // 2px border.
    let enginesPerRow = Math.floor(rowWidth / kDefaultButtonWidth);
    let buttonWidth = Math.floor(rowWidth / enginesPerRow);

    let row = document.createElementNS(HTML_NS, "tr");
    let cell = document.createElementNS(HTML_NS, "td");
    row.setAttribute("class", "contentSearchSuggestionsContainer");
    cell.setAttribute("class", "contentSearchSuggestionsContainer");

    for (let i = 0; i < engines.length; ++i) {
      let engine = engines[i];
      if (i > 0 && i % enginesPerRow == 0) {
        row.appendChild(cell);
        this._oneOffsTable.appendChild(row);
        row = document.createElementNS(HTML_NS, "tr");
        cell = document.createElementNS(HTML_NS, "td");
        row.setAttribute("class", "contentSearchSuggestionsContainer");
        cell.setAttribute("class", "contentSearchSuggestionsContainer");
      }
      let button = document.createElementNS(HTML_NS, "button");
      button.setAttribute("class", "contentSearchOneOffItem");
      let img = document.createElementNS(HTML_NS, "img");
      let uri;
      if (engine.iconBuffer) {
        uri = this._getFaviconURIFromBuffer(engine.iconBuffer);
      } else {
        uri = this._getImageURIForCurrentResolution(
          "chrome://browser/skin/search-engine-placeholder.png");
      }
      img.setAttribute("src", uri);
      img.addEventListener("load", function() {
        URL.revokeObjectURL(uri);
      }, {once: true});
      button.appendChild(img);
      button.style.width = buttonWidth + "px";
      button.setAttribute("title", engine.name);

      button.engineName = engine.name;
      button.addEventListener("click", this);
      button.addEventListener("mousemove", this);

      if (engines.length - i <= enginesPerRow - (i % enginesPerRow)) {
        button.classList.add("last-row");
      }

      if ((i + 1) % enginesPerRow == 0) {
        button.classList.add("end-of-row");
      }

      button.id = ONE_OFF_ID_PREFIX + i;
      cell.appendChild(button);
      this._oneOffButtons.push(button);
    }
    row.appendChild(cell);
    this._oneOffsTable.appendChild(row);
    this._oneOffsTable.hidden = false;
  },

  _sendMsg(type, data = null) {
    dispatchEvent(new CustomEvent("ContentSearchClient", {
      detail: {
        type,
        data,
      },
    }));
  },
};

return ContentSearchUIController;
})();
