/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["UrlbarView"];

const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetters(this, {
  Services: "resource://gre/modules/Services.jsm",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.jsm",
  UrlbarUtils: "resource:///modules/UrlbarUtils.jsm",
});

XPCOMUtils.defineLazyGetter(this, "bundle", function() {
  return Services.strings.createBundle("chrome://global/locale/autocomplete.properties");
});

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

    this._mainContainer = this.panel.querySelector(".urlbarView-body-inner");
    this._rows = this.panel.querySelector(".urlbarView-results");

    this._rows.addEventListener("mouseup", this);
    this._rows.addEventListener("mousedown", this);

    // For the horizontal fade-out effect, set the overflow attribute on result
    // rows when they overflow.
    this._rows.addEventListener("overflow", this);
    this._rows.addEventListener("underflow", this);

    this.panel.addEventListener("popupshowing", this);
    this.panel.addEventListener("popuphiding", this);

    this.controller.setView(this);
    this.controller.addQueryListener(this);
  }

  get oneOffSearchButtons() {
    return this._oneOffSearchButtons ||
      (this._oneOffSearchButtons =
         new this.window.SearchOneOffs(this.panel.querySelector(".search-one-offs")));
  }

  /**
   * @returns {boolean}
   *   Whether the panel is open.
   */
  get isOpen() {
    return this.panel.state == "open" || this.panel.state == "showing";
  }

  /**
   * @returns {UrlbarResult}
   *   The currently selected result.
   */
  get selectedResult() {
    if (!this.isOpen) {
      return null;
    }

    let resultIndex = this._selected.getAttribute("resultIndex");
    return this._queryContext.results[resultIndex];
  }

  /**
   * Selects the next or previous view item. An item could be an autocomplete
   * result or a one-off search button.
   *
   * @param {boolean} options.reverse
   *   Set to true to select the previous item. By default the next item
   *   will be selected.
   */
  selectNextItem({reverse = false} = {}) {
    if (!this.isOpen) {
      throw new Error("UrlbarView: Cannot select an item if the view isn't open.");
    }

    // TODO: handle one-off search buttons

    let row;
    if (reverse) {
      row = (this._selected && this._selected.previousElementSibling) ||
            this._rows.lastElementChild;
    } else {
      row = (this._selected && this._selected.nextElementSibling) ||
            this._rows.firstElementChild;
    }

    if (this._selected) {
      this._selected.toggleAttribute("selected", false);
    }
    this._selected = row;
    row.toggleAttribute("selected", true);

    let resultIndex = row.getAttribute("resultIndex");
    let result = this._queryContext.results[resultIndex];
    if (result) {
      this.input.setValueFromResult(result);
    }
  }

  /**
   * Closes the autocomplete results popup.
   */
  close() {
    this.panel.hidePopup();
  }

  // UrlbarController listener methods.
  onQueryStarted(queryContext) {
    this._rows.style.minHeight = this._getBoundsWithoutFlushing(this._rows).height + "px";
  }

  onQueryCancelled(queryContext) {
    // Nothing.
  }

  onQueryFinished(queryContext) {
    this._rows.style.minHeight = "";
  }

  onQueryResults(queryContext) {
    this._queryContext = queryContext;

    let fragment = this.document.createDocumentFragment();
    for (let resultIndex in queryContext.results) {
      fragment.appendChild(this._createRow(resultIndex));
    }

    if (queryContext.preselected) {
      this._selected = fragment.firstElementChild;
      this._selected.toggleAttribute("selected", true);
    } else if (queryContext.lastResultCount == 0) {
      // Clear the selection when we get a new set of results.
      this._selected = null;
    }

    // TODO bug 1523602: For now, clear the results for each set received.
    // We should be updating the existing list instead.
    this._rows.textContent = "";
    this._rows.appendChild(fragment);

    this._openPanel();
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
    // Change the index for any rows above the removed index.
    for (let i = index + 1; i < this._rows.children.length; i++) {
      let child = this._rows.children[i];
      child.setAttribute("resultIndex", child.getAttribute("resultIndex") - 1);
    }

    let rowToRemove = this._rows.children[index];
    rowToRemove.remove();

    if (rowToRemove != this._selected) {
      return;
    }

    // Select the row at the same index, if possible.
    let newSelectionIndex = index;
    if (index >= this._queryContext.results.length) {
      newSelectionIndex = this._queryContext.results.length - 1;
    }
    if (newSelectionIndex >= 0) {
      this._selected = this._rows.children[newSelectionIndex];
      this._selected.setAttribute("selected", true);
    }

    this.input.setValueFromResult(this._queryContext.results[newSelectionIndex]);
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

  _getBoundsWithoutFlushing(element) {
    return this.window.windowUtils.getBoundsWithoutFlushing(element);
  }

  _createElement(name) {
    return this.document.createElementNS("http://www.w3.org/1999/xhtml", name);
  }

  _openPanel() {
    if (this.isOpen) {
      return;
    }

    this.panel.removeAttribute("hidden");
    this.panel.removeAttribute("actionoverride");

    this._alignPanel();

    this.panel.openPopup(this.input.textbox.closest("toolbar"), "after_end", 0, -1);
  }

  _alignPanel() {
    // Make the panel span the width of the window.
    let documentRect =
      this._getBoundsWithoutFlushing(this.document.documentElement);
    let width = documentRect.right - documentRect.left;
    this.panel.setAttribute("width", width);

    // Subtract two pixels for left and right borders on the panel.
    this._mainContainer.style.maxWidth = (width - 2) + "px";

    // Keep the popup items' site icons aligned with the input's identity
    // icon if it's not too far from the edge of the window.  We define
    // "too far" as "more than 30% of the window's width AND more than
    // 250px".
    let boundToCheck = this.window.RTL_UI ? "right" : "left";
    let inputRect = this._getBoundsWithoutFlushing(this.input.textbox);
    let startOffset = Math.abs(inputRect[boundToCheck] - documentRect[boundToCheck]);
    let alignSiteIcons = startOffset / width <= 0.3 || startOffset <= 250;
    if (alignSiteIcons) {
      // Calculate the end margin if we have a start margin.
      let boundToCheckEnd = this.window.RTL_UI ? "left" : "right";
      let endOffset = Math.abs(inputRect[boundToCheckEnd] -
                               documentRect[boundToCheckEnd]);
      if (endOffset > startOffset * 2) {
        // Provide more space when aligning would result in an unbalanced
        // margin. This allows the location bar to be moved to the start
        // of the navigation toolbar to reclaim space for results.
        endOffset = startOffset;
      }
      let identityIcon = this.document.getElementById("identity-icon");
      let identityRect = this._getBoundsWithoutFlushing(identityIcon);
      let start = this.window.RTL_UI ?
                    documentRect.right - identityRect.right :
                    identityRect.left;

      this.panel.style.setProperty("--item-padding-start", Math.round(start) + "px");
      this.panel.style.setProperty("--item-padding-end", Math.round(endOffset) + "px");
    } else {
      this.panel.style.removeProperty("--item-padding-start");
      this.panel.style.removeProperty("--item-padding-end");
    }
  }

  _createRow(resultIndex) {
    let result = this._queryContext.results[resultIndex];
    let item = this._createElement("div");
    item.className = "urlbarView-row";
    item.setAttribute("resultIndex", resultIndex);

    if (result.type == UrlbarUtils.RESULT_TYPE.SEARCH) {
      item.setAttribute("type", "search");
    } else if (result.type == UrlbarUtils.RESULT_TYPE.REMOTE_TAB) {
      item.setAttribute("type", "remotetab");
    } else if (result.type == UrlbarUtils.RESULT_TYPE.TAB_SWITCH) {
      item.setAttribute("type", "switchtab");
    } else if (result.source == UrlbarUtils.RESULT_SOURCE.BOOKMARKS) {
      item.setAttribute("type", "bookmark");
    }

    let content = this._createElement("span");
    content.className = "urlbarView-row-inner";
    item.appendChild(content);

    let typeIcon = this._createElement("span");
    typeIcon.className = "urlbarView-type-icon";
    content.appendChild(typeIcon);

    let favicon = this._createElement("img");
    favicon.className = "urlbarView-favicon";
    if (result.type == UrlbarUtils.RESULT_TYPE.SEARCH ||
        result.type == UrlbarUtils.RESULT_TYPE.KEYWORD) {
      favicon.src = UrlbarUtils.ICON.SEARCH_GLASS;
    } else {
      favicon.src = result.payload.icon || UrlbarUtils.ICON.DEFAULT;
    }
    content.appendChild(favicon);

    let title = this._createElement("span");
    title.className = "urlbarView-title";
    this._addTextContentWithHighlights(
      title, result.title, result.titleHighlights);
    content.appendChild(title);

    if (result.payload.tags && result.payload.tags.length > 0) {
      const tagsContainer = this._createElement("div");
      tagsContainer.className = "urlbarView-tags";
      tagsContainer.append(...result.payload.tags.map((tag, i) => {
        const element = this._createElement("span");
        element.className = "urlbarView-tag";
        this._addTextContentWithHighlights(
          element, tag, result.payloadHighlights.tags[i]);
        return element;
      }));
      content.appendChild(tagsContainer);
    }

    let action;
    let url;
    let setAction = text => {
      action = this._createElement("span");
      action.className = "urlbarView-secondary urlbarView-action";
      action.textContent = text;
    };
    let setURL = () => {
      url = this._createElement("span");
      url.className = "urlbarView-secondary urlbarView-url";
      this._addTextContentWithHighlights(url, result.payload.url || "",
                                         result.payloadHighlights.url || []);
    };
    switch (result.type) {
      case UrlbarUtils.RESULT_TYPE.TAB_SWITCH:
        setAction(bundle.GetStringFromName("switchToTab2"));
        setURL();
        break;
      case UrlbarUtils.RESULT_TYPE.REMOTE_TAB:
        setAction(result.payload.device);
        setURL();
        break;
      case UrlbarUtils.RESULT_TYPE.SEARCH:
        setAction(bundle.formatStringFromName("searchWithEngine",
                                              [result.payload.engine], 1));
        break;
      default:
        if (resultIndex == 0) {
          setAction(bundle.GetStringFromName("visit"));
        } else {
          setURL();
        }
        break;
    }
    if (action) {
      content.appendChild(action);
    }
    if (url) {
      content.appendChild(url);
    }

    return item;
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

  _enableOrDisableOneOffSearches() {
    if (UrlbarPrefs.get("oneOffSearches")) {
      this.oneOffSearchButtons.telemetryOrigin = "urlbar";
      this.oneOffSearchButtons.style.display = "";
      // Set .textbox first, since the popup setter will cause
      // a _rebuild call that uses it.
      this.oneOffSearchButtons.textbox = this.input.textbox;
      this.oneOffSearchButtons.view = this;
    } else if (this._oneOffSearchButtons) {
      this.oneOffSearchButtons.telemetryOrigin = null;
      this.oneOffSearchButtons.style.display = "none";
      this.oneOffSearchButtons.textbox = null;
      this.oneOffSearchButtons.view = null;
    }
  }

  _on_mousedown(event) {
    if (event.button == 2) {
      // Ignore right clicks.
      return;
    }

    let row = event.target;
    while (!row.classList.contains("urlbarView-row")) {
      row = row.parentNode;
    }
    let resultIndex = row.getAttribute("resultIndex");
    if (this._selected) {
      this._selected.toggleAttribute("selected", false);
    }
    this._selected = this._rows.children[resultIndex];
    this._selected.toggleAttribute("selected", true);
    this.controller.speculativeConnect(this._queryContext, resultIndex, "mousedown");
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
    let resultIndex = row.getAttribute("resultIndex");
    let result = this._queryContext.results[resultIndex];
    if (result) {
      this.input.pickResult(event, result);
    }
  }

  _on_overflow(event) {
    if (event.target.classList.contains("urlbarView-row-inner")) {
      event.target.toggleAttribute("overflow", true);
    }
  }

  _on_underflow(event) {
    if (event.target.classList.contains("urlbarView-row-inner")) {
      event.target.toggleAttribute("overflow", false);
    }
  }

  _on_popupshowing() {
    this._enableOrDisableOneOffSearches();
  }

  _on_popuphiding() {
    this.controller.cancelQuery();
  }
}
