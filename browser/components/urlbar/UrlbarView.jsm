/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["UrlbarView"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
XPCOMUtils.defineLazyModuleGetters(this, {
  UrlbarContextualTip: "resource:///modules/UrlbarContextualTip.jsm",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.jsm",
  UrlbarTokenizer: "resource:///modules/UrlbarTokenizer.jsm",
  UrlbarUtils: "resource:///modules/UrlbarUtils.jsm",
});

// Stale rows are removed on a timer with this timeout.  Tests can override this
// by setting UrlbarView.removeStaleRowsTimeout.
const DEFAULT_REMOVE_STALE_ROWS_TIMEOUT = 400;

/**
 * Receives and displays address bar autocomplete results.
 */
class UrlbarView {
  /**
   * @param {UrlbarInput} input
   *   The UrlbarInput instance belonging to this UrlbarView instance.
   */
  constructor(input) {
    this.input = input;
    this.panel = input.panel;
    this.controller = input.controller;
    this.document = this.panel.ownerDocument;
    this.window = this.document.defaultView;

    if (this.input.megabar) {
      this.panel.classList.add("megabar");
    }

    this._mainContainer = this.panel.querySelector(".urlbarView-body-inner");
    this._rows = this.panel.querySelector(".urlbarView-results");

    this._rows.addEventListener("mousedown", this);
    this._rows.addEventListener("mouseup", this);

    // For the horizontal fade-out effect, set the overflow attribute on result
    // rows when they overflow.
    this._rows.addEventListener("overflow", this);
    this._rows.addEventListener("underflow", this);

    this.controller.setView(this);
    this.controller.addQueryListener(this);
  }

  /**
   * Sets the icon, title, button's title, and link's title
   * for the contextual tip. If a contextual tip has not
   * been created, then it will be created.
   *
   * @param {object} details
   * @param {string} details.title
   *   Main title displayed by the contextual tip.
   * @param {string} [details.buttonTitle]
   *   Title of the button on the contextual tip.
   *   If omitted then the button will be hidden.
   * @param {string} [details.linkTitle]
   *   Title of the link on the contextual tip.
   *   If omitted then the link will be hidden.
   * @param {string} [details.iconStyle]
   *   A non-empty string of styles to add to the icon's style attribute.
   *   These styles set CSS variables to URLs of images;
   *   the CSS variables responsible for the icon's background image are
   *   the variable names containing `--webextension-contextual-tip-icon`
   *   in `browser/base/content/browser.css`.
   *   If ommited, no changes are made to the icon.
   */
  setContextualTip(details) {
    if (!this.contextualTip) {
      this.contextualTip = new UrlbarContextualTip(this);
    }
    this.contextualTip.set(details);

    // Disable one off search buttons from appearing if
    // the contextual tip is the only item in the urlbar view.
    if (this.visibleItemCount == 0) {
      this._enableOrDisableOneOffSearches(false);
    }

    this._openPanel();
  }

  /**
   * Hides the contextual tip.
   */
  hideContextualTip() {
    if (this.contextualTip) {
      this.contextualTip.hide();

      // When the pending query has finished and there's 0 results then
      // close the urlbar view.
      this.input.lastQueryContextPromise.then(() => {
        if (this.visibleItemCount == 0) {
          this.close();
        }
      });
    }
  }

  /**
   * Removes the contextual tip from the DOM.
   */
  removeContextualTip() {
    if (!this.contextualTip) {
      return;
    }
    this.contextualTip.remove();
    this.contextualTip = null;
  }

  get oneOffSearchButtons() {
    if (!this._oneOffSearchButtons) {
      this._oneOffSearchButtons = new this.window.SearchOneOffs(
        this.panel.querySelector(".search-one-offs")
      );
      this._oneOffSearchButtons.addEventListener(
        "SelectedOneOffButtonChanged",
        this
      );
    }
    return this._oneOffSearchButtons;
  }

  /**
   * @returns {boolean}
   *   Whether the panel is open.
   */
  get isOpen() {
    return !this.panel.hasAttribute("hidden");
  }

  get allowEmptySelection() {
    return !(
      this._queryContext &&
      this._queryContext.results[0] &&
      this._queryContext.results[0].heuristic
    );
  }

  get selectedIndex() {
    if (!this.isOpen || !this._selected) {
      return -1;
    }
    return this._selected.result.uiIndex;
  }

  set selectedIndex(val) {
    if (!this.isOpen) {
      throw new Error(
        "UrlbarView: Cannot select an item if the view isn't open."
      );
    }

    if (val < 0) {
      this._selectItem(null);
      return val;
    }

    let items = Array.from(this._rows.children).filter(r =>
      this._isRowVisible(r)
    );
    if (val >= items.length) {
      throw new Error(`UrlbarView: Index ${val} is out of bounds.`);
    }
    this._selectItem(items[val]);
    return val;
  }

  /**
   * @returns {UrlbarResult}
   *   The currently selected result.
   */
  get selectedResult() {
    if (!this.isOpen || !this._selected) {
      return null;
    }
    return this._selected.result;
  }

  /**
   * @returns {number}
   *   The number of visible results in the view.  Note that this may be larger
   *   than the number of results in the current query context since the view
   *   may be showing stale results.
   */
  get visibleItemCount() {
    let sum = 0;
    for (let row of this._rows.children) {
      sum += Number(this._isRowVisible(row));
    }
    return sum;
  }

  /**
   * Moves the view selection forward or backward.
   *
   * @param {number} amount
   *   The number of steps to move.
   * @param {boolean} options.reverse
   *   Set to true to select the previous item. By default the next item
   *   will be selected.
   */
  selectBy(amount, { reverse = false } = {}) {
    if (!this.isOpen) {
      throw new Error(
        "UrlbarView: Cannot select an item if the view isn't open."
      );
    }

    // Freeze results as the user is interacting with them.
    this.controller.cancelQuery();

    let row = this._selected;

    // Results over maxResults may be hidden and should not be selectable.
    let lastElementChild = this._rows.lastElementChild;
    while (lastElementChild && !this._isRowVisible(lastElementChild)) {
      lastElementChild = lastElementChild.previousElementSibling;
    }

    if (!row) {
      this._selectItem(
        reverse ? lastElementChild : this._rows.firstElementChild
      );
      return;
    }

    let endReached = reverse
      ? row == this._rows.firstElementChild
      : row == lastElementChild;
    if (endReached) {
      if (this.allowEmptySelection) {
        row = null;
      } else {
        row = reverse ? lastElementChild : this._rows.firstElementChild;
      }
      this._selectItem(row);
      return;
    }

    while (amount-- > 0) {
      let next = reverse ? row.previousElementSibling : row.nextElementSibling;
      if (!next) {
        break;
      }
      if (!this._isRowVisible(next)) {
        continue;
      }
      row = next;
    }
    this._selectItem(row);
  }

  removeAccessibleFocus() {
    this._setAccessibleFocus(null);
  }

  /**
   * Closes the view, cancelling the query if necessary.
   */
  close() {
    this.controller.cancelQuery();

    if (!this.isOpen) {
      return;
    }

    this.panel.setAttribute("hidden", "true");
    this.removeAccessibleFocus();
    this.input.inputField.setAttribute("aria-expanded", "false");
    this.input.dropmarker.removeAttribute("open");

    this.input.removeAttribute("open");
    this.input.endLayoutBreakout();

    this._rows.textContent = "";

    this.window.removeEventListener("resize", this);

    this.window.removeEventListener("mousedown", this);
    this.panel.removeEventListener("mousedown", this);
    this.input.textbox.removeEventListener("mousedown", this);

    this.controller.notify(this.controller.NOTIFICATIONS.VIEW_CLOSE);
    if (this.contextualTip) {
      this.contextualTip.hide();
    }
  }

  // UrlbarController listener methods.
  onQueryStarted(queryContext) {
    this._queryWasCancelled = false;
    this._startRemoveStaleRowsTimer();
  }

  onQueryCancelled(queryContext) {
    this._queryWasCancelled = true;
    this._cancelRemoveStaleRowsTimer();
  }

  onQueryFinished(queryContext) {
    this._cancelRemoveStaleRowsTimer();
    // If the query has not been canceled, remove stale rows immediately.
    if (!this._queryWasCancelled) {
      this._removeStaleRows();
    }
  }

  onQueryResults(queryContext) {
    this._queryContext = queryContext;

    this._updateResults(queryContext);

    let isFirstPreselectedResult = false;
    if (queryContext.lastResultCount == 0) {
      if (queryContext.preselected) {
        isFirstPreselectedResult = true;
        this._selectItem(this._rows.firstElementChild, {
          updateInput: false,
          setAccessibleFocus: this.controller._userSelectionBehavior == "arrow",
        });
      } else {
        // Clear the selection when we get a new set of results.
        this._selectItem(null, {
          updateInput: false,
        });
      }
      // Hide the one-off search buttons if the search string is empty, or
      // starts with a potential @ search alias or the search restriction
      // character.
      let trimmedValue = queryContext.searchString.trim();
      this._enableOrDisableOneOffSearches(
        trimmedValue &&
          trimmedValue[0] != "@" &&
          (trimmedValue[0] != UrlbarTokenizer.RESTRICT.SEARCH ||
            trimmedValue.length != 1)
      );
    }

    this._openPanel();

    if (isFirstPreselectedResult) {
      // The first, preselected result may be a search alias result, so apply
      // formatting if necessary.  Conversely, the first result of the previous
      // query may have been an alias, so remove formatting if necessary.
      this.input.formatValue();
    }
  }

  /**
   * Handles removing a result from the view when it is removed from the query,
   * and attempts to select the new result on the same row.
   *
   * This assumes that the result rows are in index order.
   *
   * @param {number} index The index of the result that has been removed.
   */
  onQueryResultRemoved(index) {
    let rowToRemove = this._rows.children[index];
    rowToRemove.remove();

    this._updateIndices();

    if (rowToRemove != this._selected) {
      return;
    }

    // Select the row at the same index, if possible.
    let newSelectionIndex = index;
    if (index >= this._queryContext.results.length) {
      newSelectionIndex = this._queryContext.results.length - 1;
    }
    if (newSelectionIndex >= 0) {
      this.selectedIndex = newSelectionIndex;
    }
  }

  /**
   * Passes DOM events for the view to the _on_<event type> methods.
   * @param {Event} event
   *   DOM event from the <view>.
   */
  handleEvent(event) {
    let methodName = "_on_" + event.type;
    if (methodName in this) {
      this[methodName](event);
    } else {
      throw new Error("Unrecognized UrlbarView event: " + event.type);
    }
  }

  /**
   * This is called when a one-off is clicked and when "search in new tab"
   * is selected from a one-off context menu.
   * @param {Event} event
   * @param {nsISearchEngine} engine
   * @param {string} where
   * @param {object} params
   */
  handleOneOffSearch(event, engine, where, params) {
    this.input.handleCommand(event, where, params);
  }

  // Private methods below.

  _createElement(name) {
    return this.document.createElementNS("http://www.w3.org/1999/xhtml", name);
  }

  _openPanel() {
    if (this.isOpen) {
      return;
    }
    this.controller.userSelectionBehavior = "none";

    this.panel.removeAttribute("actionoverride");

    if (!this.input.megabar) {
      let getBoundsWithoutFlushing = element =>
        this.window.windowUtils.getBoundsWithoutFlushing(element);
      let px = number => number.toFixed(2) + "px";
      let inputRect = getBoundsWithoutFlushing(this.input.textbox);

      // Make the panel span the width of the window.
      let documentRect = getBoundsWithoutFlushing(
        this.document.documentElement
      );
      let width = documentRect.right - documentRect.left;

      // Keep the popup items' site icons aligned with the input's identity
      // icon if it's not too far from the edge of the window.  We define
      // "too far" as "more than 30% of the window's width AND more than
      // 250px".
      let boundToCheck = this.window.RTL_UI ? "right" : "left";
      let startOffset = Math.abs(
        inputRect[boundToCheck] - documentRect[boundToCheck]
      );
      let alignSiteIcons = startOffset / width <= 0.3 || startOffset <= 250;

      if (alignSiteIcons) {
        // Calculate the end margin if we have a start margin.
        let boundToCheckEnd = this.window.RTL_UI ? "left" : "right";
        let endOffset = Math.abs(
          inputRect[boundToCheckEnd] - documentRect[boundToCheckEnd]
        );
        if (endOffset > startOffset * 2) {
          // Provide more space when aligning would result in an unbalanced
          // margin. This allows the location bar to be moved to the start
          // of the navigation toolbar to reclaim space for results.
          endOffset = startOffset;
        }

        // Align the view's icons with the tracking protection or identity icon,
        // whichever is visible.
        let alignRect;
        for (let id of ["tracking-protection-icon-box", "identity-icon"]) {
          alignRect = getBoundsWithoutFlushing(
            this.document.getElementById(id)
          );
          if (alignRect.width > 0) {
            break;
          }
        }
        let start = this.window.RTL_UI
          ? documentRect.right - alignRect.right
          : alignRect.left;

        this.panel.style.setProperty("--item-padding-start", px(start));
        this.panel.style.setProperty("--item-padding-end", px(endOffset));
      } else {
        this.panel.style.removeProperty("--item-padding-start");
        this.panel.style.removeProperty("--item-padding-end");
      }

      // Align the panel with the parent toolbar.
      this.panel.style.top = px(
        getBoundsWithoutFlushing(this.input.textbox.closest("toolbar")).bottom
      );

      this._mainContainer.style.maxWidth = px(width);
    }

    this.panel.removeAttribute("hidden");
    this.input.inputField.setAttribute("aria-expanded", "true");
    this.input.dropmarker.setAttribute("open", "true");

    this.input.setAttribute("open", "true");
    this.input.startLayoutBreakout();

    this.window.addEventListener("mousedown", this);
    this.panel.addEventListener("mousedown", this);
    this.input.textbox.addEventListener("mousedown", this);

    this.window.addEventListener("resize", this);
    this._windowOuterWidth = this.window.outerWidth;

    this.controller.notify(this.controller.NOTIFICATIONS.VIEW_OPEN);
  }

  /**
   * Whether a result is a search suggestion.
   * @param {UrlbarResult} result The result to examine.
   * @returns {boolean} Whether the result is a search suggestion.
   */
  _resultIsSearchSuggestion(result) {
    return Boolean(
      result &&
        result.type == UrlbarUtils.RESULT_TYPE.SEARCH &&
        result.payload.suggestion
    );
  }

  /**
   * Checks whether the given row index can be update to the result we want
   * to apply. This is used in _updateResults to avoid flickering of results, by
   * reusing existing rows.
   * @param {number} rowIndex Index of the row to examine.
   * @param {UrlbarResult} result The result we'd like to apply.
   * @param {number} firstSearchSuggestionIndex Index of the first search suggestion.
   * @param {number} lastSearchSuggestionIndex Index of the last search suggestion.
   * @returns {boolean} Whether the row can be updated to this result.
   */
  _rowCanUpdateToResult(
    rowIndex,
    result,
    firstSearchSuggestionIndex,
    lastSearchSuggestionIndex
  ) {
    // The heuristic result must always be current, thus it's always compatible.
    if (result.heuristic) {
      return true;
    }
    let row = this._rows.children[rowIndex];
    let resultIsSearchSuggestion = this._resultIsSearchSuggestion(result);
    // If the row is same type, just update it.
    if (
      resultIsSearchSuggestion == this._resultIsSearchSuggestion(row.result)
    ) {
      return true;
    }
    // If the row has a different type, update it if we are in a compatible
    // index range.
    // In practice we don't want to overwrite a search suggestion with a non
    // search suggestion, but we allow the opposite.
    return resultIsSearchSuggestion && rowIndex >= firstSearchSuggestionIndex;
  }

  _updateResults(queryContext) {
    // TODO: For now this just compares search suggestions to the rest, in the
    // future we should make it support any type of result. Or, even better,
    // results should be grouped, thus we can directly update groups.

    // Find where are existing search suggestions.
    let firstSearchSuggestionIndex = -1;
    let lastSearchSuggestionIndex = -1;
    for (let i = 0; i < this._rows.children.length; ++i) {
      let row = this._rows.children[i];
      // Mark every row as stale, _updateRow will unmark them later.
      row.setAttribute("stale", "true");
      // Skip any row that isn't a search suggestion, or is non-visible because
      // over maxResults.
      if (
        row.result.heuristic ||
        i >= queryContext.maxResults ||
        !this._resultIsSearchSuggestion(row.result)
      ) {
        continue;
      }
      if (firstSearchSuggestionIndex == -1) {
        firstSearchSuggestionIndex = i;
      }
      lastSearchSuggestionIndex = i;
    }

    // Walk rows and find an insertion index for results. To avoid flicker, we
    // skip rows until we find one compatible with the result we want to apply.
    // If we couldn't find a compatible range, we'll just update.
    let results = queryContext.results;
    let resultIndex = 0;
    // We can have more rows than the visible ones.
    for (
      let rowIndex = 0;
      rowIndex < this._rows.children.length && resultIndex < results.length;
      ++rowIndex
    ) {
      let row = this._rows.children[rowIndex];
      let result = results[resultIndex];
      if (
        this._rowCanUpdateToResult(
          rowIndex,
          result,
          firstSearchSuggestionIndex,
          lastSearchSuggestionIndex
        )
      ) {
        this._updateRow(row, result);
        resultIndex++;
      }
    }
    // Add remaining results, if we have fewer rows than results.
    for (; resultIndex < results.length; ++resultIndex) {
      let row = this._createRow(results[resultIndex].type);
      this._updateRow(row, results[resultIndex]);
      // Due to stale rows, we may have more rows than maxResults, thus we must
      // hide them, and we'll revert this when stale rows are removed.
      if (this._rows.children.length >= queryContext.maxResults) {
        this._setRowVisibility(row, false);
      }
      this._rows.appendChild(row);
    }

    this._updateIndices();
  }

  _createRow(type) {
    let item = this._createElement("div");
    item.className = "urlbarView-row";
    item.setAttribute("role", "option");
    item._elements = new Map();

    let content = this._createElement("span");
    content.className = "urlbarView-row-inner";
    item.appendChild(content);

    let typeIcon = this._createElement("span");
    typeIcon.className = "urlbarView-type-icon";
    content.appendChild(typeIcon);

    let favicon = this._createElement("img");
    favicon.className = "urlbarView-favicon";
    content.appendChild(favicon);
    item._elements.set("favicon", favicon);

    let title = this._createElement("span");
    title.className = "urlbarView-title";
    content.appendChild(title);
    item._elements.set("title", title);

    if (type == UrlbarUtils.RESULT_TYPE.TIP) {
      let buttonSpacer = this._createElement("span");
      buttonSpacer.className = "urlbarView-tip-button-spacer";
      content.appendChild(buttonSpacer);

      let tipButton = this._createElement("button");
      tipButton.className = "urlbarView-tip-button";
      content.appendChild(tipButton);
      item._elements.set("tipButton", tipButton);

      let helpIcon = this._createElement("span");
      helpIcon.className = "urlbarView-tip-help";
      content.appendChild(helpIcon);
    } else {
      let tagsContainer = this._createElement("span");
      tagsContainer.className = "urlbarView-tags";
      content.appendChild(tagsContainer);
      item._elements.set("tagsContainer", tagsContainer);

      let titleSeparator = this._createElement("span");
      titleSeparator.className = "urlbarView-title-separator";
      content.appendChild(titleSeparator);
      item._elements.set("titleSeparator", titleSeparator);

      let action = this._createElement("span");
      action.className = "urlbarView-secondary urlbarView-action";
      content.appendChild(action);
      item._elements.set("action", action);

      let url = this._createElement("span");
      url.className = "urlbarView-secondary urlbarView-url";
      content.appendChild(url);
      item._elements.set("url", url);
    }
    return item;
  }

  _updateRow(item, result) {
    item.result = result;
    item.removeAttribute("stale");

    if (
      result.type == UrlbarUtils.RESULT_TYPE.SEARCH &&
      !result.payload.keywordOffer
    ) {
      item.setAttribute("type", "search");
    } else if (result.type == UrlbarUtils.RESULT_TYPE.REMOTE_TAB) {
      item.setAttribute("type", "remotetab");
    } else if (result.type == UrlbarUtils.RESULT_TYPE.TAB_SWITCH) {
      item.setAttribute("type", "switchtab");
    } else if (result.type == UrlbarUtils.RESULT_TYPE.TIP) {
      item.setAttribute("type", "tip");
    } else if (result.source == UrlbarUtils.RESULT_SOURCE.BOOKMARKS) {
      item.setAttribute("type", "bookmark");
    } else {
      item.removeAttribute("type");
    }

    let favicon = item._elements.get("favicon");
    if (
      result.type == UrlbarUtils.RESULT_TYPE.SEARCH ||
      result.type == UrlbarUtils.RESULT_TYPE.KEYWORD
    ) {
      favicon.src = result.payload.icon || UrlbarUtils.ICON.SEARCH_GLASS;
    } else if (result.type == UrlbarUtils.RESULT_TYPE.TIP) {
      favicon.src = result.payload.icon || UrlbarUtils.ICON.TIP;
    } else {
      favicon.src = result.payload.icon || UrlbarUtils.ICON.DEFAULT;
    }

    let title = item._elements.get("title");

    if (result.type == UrlbarUtils.RESULT_TYPE.TIP) {
      this._addTextContentWithHighlights(title, result.payload.text, []);
      let tipButton = item._elements.get("tipButton");
      this._addTextContentWithHighlights(
        tipButton,
        result.payload.buttonText,
        []
      );
      // Tips are dissimilar to other types of results and don't need the rest
      // of this markup. We return early.
      return;
    }

    this._addTextContentWithHighlights(
      title,
      result.title,
      result.titleHighlights
    );
    title._tooltip = result.title;
    if (title.hasAttribute("overflow")) {
      title.setAttribute("title", title._tooltip);
    }

    let tagsContainer = item._elements.get("tagsContainer");
    tagsContainer.textContent = "";
    if (result.payload.tags && result.payload.tags.length > 0) {
      tagsContainer.append(
        ...result.payload.tags.map((tag, i) => {
          const element = this._createElement("span");
          element.className = "urlbarView-tag";
          this._addTextContentWithHighlights(
            element,
            tag,
            result.payloadHighlights.tags[i]
          );
          return element;
        })
      );
    }

    let action = "";
    let isVisitAction = false;
    let setURL = false;
    switch (result.type) {
      case UrlbarUtils.RESULT_TYPE.TAB_SWITCH:
        action = UrlbarUtils.strings.GetStringFromName("switchToTab2");
        setURL = true;
        break;
      case UrlbarUtils.RESULT_TYPE.REMOTE_TAB:
        action = result.payload.device;
        setURL = true;
        break;
      case UrlbarUtils.RESULT_TYPE.SEARCH:
        action = UrlbarUtils.strings.formatStringFromName("searchWithEngine", [
          result.payload.engine,
        ]);
        break;
      case UrlbarUtils.RESULT_TYPE.KEYWORD:
        isVisitAction = result.payload.input.trim() == result.payload.keyword;
        break;
      case UrlbarUtils.RESULT_TYPE.OMNIBOX:
        action = result.payload.content;
        break;
      default:
        if (result.heuristic) {
          isVisitAction = true;
        } else {
          setURL = true;
        }
        break;
    }

    let url = item._elements.get("url");
    if (setURL) {
      this._addTextContentWithHighlights(
        url,
        result.payload.displayUrl,
        result.payloadHighlights.displayUrl || []
      );
      url._tooltip = result.payload.displayUrl;
    } else {
      url.textContent = "";
      url._tooltip = "";
    }
    if (url.hasAttribute("overflow")) {
      url.setAttribute("title", url._tooltip);
    }

    if (isVisitAction) {
      action = UrlbarUtils.strings.GetStringFromName("visit");
      title.setAttribute("isurl", "true");
    } else {
      title.removeAttribute("isurl");
    }
    item._elements.get("action").textContent = action;

    item._elements.get("titleSeparator").hidden = !action && !setURL;
  }

  _updateIndices() {
    for (let i = 0; i < this._rows.children.length; i++) {
      let item = this._rows.children[i];
      item.result.uiIndex = i;
      item.id = "urlbarView-row-" + i;
    }
  }

  _setRowVisibility(row, visible) {
    row.style.display = visible ? "" : "none";
    if (!visible) {
      // Reset the overflow state of elements that can overflow in case their
      // content changes while they're hidden. When making the row visible
      // again, we'll get new overflow events if needed.
      this._setElementOverflowing(row._elements.get("title"), false);
      this._setElementOverflowing(row._elements.get("url"), false);
    }
  }

  _isRowVisible(row) {
    return row.style.display != "none";
  }

  _removeStaleRows() {
    let row = this._rows.lastElementChild;
    while (row) {
      let next = row.previousElementSibling;
      if (row.hasAttribute("stale")) {
        row.remove();
      } else {
        this._setRowVisibility(row, true);
      }
      row = next;
    }
    this._updateIndices();
  }

  _startRemoveStaleRowsTimer() {
    this._removeStaleRowsTimer = this.window.setTimeout(() => {
      this._removeStaleRowsTimer = null;
      this._removeStaleRows();
    }, UrlbarView.removeStaleRowsTimeout);
  }

  _cancelRemoveStaleRowsTimer() {
    if (this._removeStaleRowsTimer) {
      this.window.clearTimeout(this._removeStaleRowsTimer);
      this._removeStaleRowsTimer = null;
    }
  }

  _selectItem(item, { updateInput = true, setAccessibleFocus = true } = {}) {
    if (this._selected) {
      this._selected.toggleAttribute("selected", false);
      this._selected.removeAttribute("aria-selected");
    }
    if (item) {
      item.toggleAttribute("selected", true);
      item.setAttribute("aria-selected", "true");
    }
    this._setAccessibleFocus(setAccessibleFocus && item);
    this._selected = item;

    if (updateInput) {
      this.input.setValueFromResult(item && item.result);
    }
  }

  _setAccessibleFocus(item) {
    if (item) {
      this.input.inputField.setAttribute("aria-activedescendant", item.id);
    } else {
      this.input.inputField.removeAttribute("aria-activedescendant");
    }
  }

  /**
   * Adds text content to a node, placing substrings that should be highlighted
   * inside <em> nodes.
   *
   * @param {Node} parentNode
   *   The text content will be added to this node.
   * @param {string} textContent
   *   The text content to give the node.
   * @param {array} highlights
   *   The matches to highlight in the text.
   */
  _addTextContentWithHighlights(parentNode, textContent, highlights) {
    parentNode.textContent = "";
    if (!textContent) {
      return;
    }
    highlights = (highlights || []).concat([[textContent.length, 0]]);
    let index = 0;
    for (let [highlightIndex, highlightLength] of highlights) {
      if (highlightIndex - index > 0) {
        parentNode.appendChild(
          this.document.createTextNode(
            textContent.substring(index, highlightIndex)
          )
        );
      }
      if (highlightLength > 0) {
        let strong = this._createElement("strong");
        strong.textContent = textContent.substring(
          highlightIndex,
          highlightIndex + highlightLength
        );
        parentNode.appendChild(strong);
      }
      index = highlightIndex + highlightLength;
    }
  }

  _enableOrDisableOneOffSearches(enable = true) {
    if (enable && UrlbarPrefs.get("oneOffSearches")) {
      this.oneOffSearchButtons.telemetryOrigin = "urlbar";
      this.oneOffSearchButtons.style.display = "";
      this.oneOffSearchButtons.textbox = this.input.inputField;
      this.oneOffSearchButtons.view = this;
    } else {
      this.oneOffSearchButtons.telemetryOrigin = null;
      this.oneOffSearchButtons.style.display = "none";
      this.oneOffSearchButtons.textbox = null;
      this.oneOffSearchButtons.view = null;
    }
  }

  _setElementOverflowing(element, overflowing) {
    element.toggleAttribute("overflow", overflowing);
    if (overflowing) {
      element.setAttribute("title", element._tooltip);
    } else {
      element.removeAttribute("title");
    }
  }

  // Event handlers below.

  _on_SelectedOneOffButtonChanged() {
    if (!this.isOpen || !this._queryContext) {
      return;
    }

    // Update all search suggestion results to use the newly selected engine, or
    // if no engine is selected, revert to their original engines.
    let engine =
      this.oneOffSearchButtons.selectedButton &&
      this.oneOffSearchButtons.selectedButton.engine;
    for (let i = 0; i < this._queryContext.results.length; i++) {
      let result = this._queryContext.results[i];
      if (
        result.type != UrlbarUtils.RESULT_TYPE.SEARCH ||
        (!result.heuristic && !result.payload.suggestion)
      ) {
        continue;
      }
      if (engine) {
        if (!result.payload.originalEngine) {
          result.payload.originalEngine = result.payload.engine;
        }
        result.payload.engine = engine.name;
      } else if (result.payload.originalEngine) {
        result.payload.engine = result.payload.originalEngine;
        delete result.payload.originalEngine;
      }
      let item = this._rows.children[i];
      let action = item.querySelector(".urlbarView-action");
      action.textContent = UrlbarUtils.strings.formatStringFromName(
        "searchWithEngine",
        [(engine && engine.name) || result.payload.engine]
      );
      // If we just changed the engine from the original engine and it had an
      // icon, then make sure the result now uses the new engine's icon or
      // failing that the default icon.  If we changed it back to the original
      // engine, go back to the original or default icon.
      let favicon = item.querySelector(".urlbarView-favicon");
      if (engine && result.payload.icon) {
        favicon.src =
          (engine.iconURI && engine.iconURI.spec) ||
          UrlbarUtils.ICON.SEARCH_GLASS;
      } else if (!engine) {
        favicon.src = result.payload.icon || UrlbarUtils.ICON.SEARCH_GLASS;
      }
    }
  }

  _on_mousedown(event) {
    switch (event.currentTarget) {
      case this.panel:
      case this.input.textbox:
        this._mousedownOnViewOrInput = true;
        break;
      case this.window:
        // Close the view when clicking on toolbars and other UI pieces that might
        // not automatically remove focus from the input.
        if (this._mousedownOnViewOrInput) {
          this._mousedownOnViewOrInput = false;
          break;
        }
        // Respect the autohide preference for easier inspecting/debugging via
        // the browser toolbox.
        if (!UrlbarPrefs.get("ui.popup.disable_autohide")) {
          this.close();
        }
        break;
      case this._rows:
        if (event.button == 2) {
          // Ignore right clicks.
          break;
        }
        let row = event.target;
        while (!row.classList.contains("urlbarView-row")) {
          row = row.parentNode;
        }
        this._selectItem(row, { updateInput: false });
        this.controller.speculativeConnect(
          this.selectedResult,
          this._queryContext,
          "mousedown"
        );
        break;
    }
  }

  _on_mouseup(event) {
    if (event.button == 2) {
      // Ignore right clicks.
      return;
    }

    let row = event.target;
    while (!row.classList.contains("urlbarView-row")) {
      row = row.parentNode;
    }
    this.input.pickResult(row.result, event);
  }

  _on_overflow(event) {
    if (
      event.detail == 1 &&
      (event.target.classList.contains("urlbarView-url") ||
        event.target.classList.contains("urlbarView-title"))
    ) {
      this._setElementOverflowing(event.target, true);
    }
  }

  _on_underflow(event) {
    if (
      event.detail == 1 &&
      (event.target.classList.contains("urlbarView-url") ||
        event.target.classList.contains("urlbarView-title"))
    ) {
      this._setElementOverflowing(event.target, false);
    }
  }

  _on_resize() {
    if (this.megabar) {
      return;
    }

    if (this._windowOuterWidth == this.window.outerWidth) {
      // Sometimes a resize event is fired when the window's size doesn't
      // actually change; at least, browser_tabMatchesInAwesomebar.js triggers
      // it intermittently, which causes that test to hang or fail.  Ignore
      // those events.
      return;
    }

    // Close the popup as it would be wrongly sized. This can
    // happen when using special OS resize functions like Win+Arrow.
    this.close();
  }
}

UrlbarView.removeStaleRowsTimeout = DEFAULT_REMOVE_STALE_ROWS_TIMEOUT;
