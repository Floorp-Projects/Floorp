/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["UrlbarView"];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetters(this, {
  Services: "resource://gre/modules/Services.jsm",
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

    // For the horizontal fade-out effect, set the overflow attribute on result
    // rows when they overflow.
    this._rows.addEventListener("overflow", this);
    this._rows.addEventListener("underflow", this);

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
      row = this._selected.previousElementSibling ||
            this._rows.lastElementChild;
    } else {
      row = this._selected.nextElementSibling ||
            this._rows.firstElementChild;
    }

    this._selected.toggleAttribute("selected", false);
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
    this._rows.textContent = "";
  }

  onQueryCancelled(queryContext) {
    // Nothing.
  }

  onQueryFinished(queryContext) {
    // Nothing.
  }

  onQueryResults(queryContext) {
    // XXX For now, clear the results for each set received. We should really
    // be updating the existing list.
    this._rows.textContent = "";
    this._queryContext = queryContext;
    for (let resultIndex in queryContext.results) {
      this._addRow(resultIndex);
    }

    if (queryContext.preselected) {
      this._selected = this._rows.firstElementChild;
      this._selected.toggleAttribute("selected", true);
    }

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

    this._alignPanel();

    // TODO: Search one off buttons are a stub right now.
    //       We'll need to set them up properly.
    this.oneOffSearchButtons;

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

  _addRow(resultIndex) {
    let result = this._queryContext.results[resultIndex];
    let item = this._createElement("div");
    item.className = "urlbarView-row";
    item.setAttribute("resultIndex", resultIndex);

    if (result.source == UrlbarUtils.MATCH_SOURCE.TABS) {
      item.setAttribute("type", "tab");
    } else if (result.source == UrlbarUtils.MATCH_SOURCE.BOOKMARKS) {
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
      favicon.src = "chrome://browser/skin/search-glass.svg";
    } else {
      favicon.src = result.payload.icon || "chrome://mozapps/skin/places/defaultFavicon.svg";
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

    let secondary = this._createElement("span");
    secondary.className = "urlbarView-secondary";
    switch (result.type) {
      case UrlbarUtils.RESULT_TYPE.TAB_SWITCH:
        secondary.classList.add("urlbarView-action");
        secondary.textContent = bundle.GetStringFromName("switchToTab2");
        break;
      case UrlbarUtils.RESULT_TYPE.SEARCH:
        secondary.classList.add("urlbarView-action");
        secondary.textContent =
          bundle.formatStringFromName("searchWithEngine",
                                      [result.payload.engine], 1);
        break;
      default:
        secondary.classList.add("urlbarView-url");
        this._addTextContentWithHighlights(secondary, result.payload.url || "",
                                           result.payloadHighlights.url || []);
        break;
    }
    content.appendChild(secondary);

    this._rows.appendChild(item);
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

  _on_popuphiding(event) {
    this.controller.cancelQuery();
  }
}
