/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { require } = ChromeUtils.import(
  "resource://devtools/shared/loader/Loader.jsm"
);
const EventEmitter = require("devtools/shared/event-emitter");
const { KeyCodes } = require("devtools/client/shared/keycodes");

const EXPORTED_SYMBOLS = ["SplitView"];

var bindings = new WeakMap();

class SplitView extends EventEmitter {
  #filter;

  static FILTERED_CLASSNAME = "splitview-filtered";
  static ALL_FILTERED_CLASSNAME = "splitview-all-filtered";

  /**
   * Initialize the split view UI on an existing DOM element.
   *
   * A split view contains items, each of those having one summary and one details
   * elements.
   * It is adaptive as it behaves similarly to a richlistbox when there the aspect
   * ratio is narrow or as a pair listbox-box otherwise.
   *
   * @param DOMElement root
   * @see appendItem
   */
  constructor(root) {
    super();
    this._root = root;
    this._controller = root.querySelector(".splitview-controller");
    this._nav = root.querySelector(".splitview-nav");
    this._side = root.querySelector(".splitview-side-details");
    this._tplSummary = root.querySelector("#splitview-tpl-summary-stylesheet");
    this._tplDetails = root.querySelector("#splitview-tpl-details-stylesheet");
    this._activeSummary = null;

    // items list focus and search-on-type handling
    this._nav.addEventListener("keydown", event => {
      function getFocusedItemWithin(nav) {
        let node = nav.ownerDocument.activeElement;
        while (node && node.parentNode != nav) {
          node = node.parentNode;
        }
        return node;
      }

      // do not steal focus from inside iframes or textboxes
      if (
        event.target.ownerDocument != this._nav.ownerDocument ||
        event.target.tagName == "input" ||
        event.target.tagName == "textarea" ||
        event.target.classList.contains("textbox")
      ) {
        return false;
      }

      // handle keyboard navigation within the items list
      const visibleElements = Array.from(
        this._nav.querySelectorAll(`li:not(.${SplitView.FILTERED_CLASSNAME})`)
      );
      // Elements have a different visual order (due to the use of MozBoxOrdinalGroup), so
      // we need to sort them by their data-ordinal attribute
      visibleElements.sort(
        (a, b) =>
          a.getAttribute("data-ordinal") - b.getAttribute("data-ordinal")
      );

      let elementToFocus;
      if (
        event.keyCode == KeyCodes.DOM_VK_PAGE_UP ||
        event.keyCode == KeyCodes.DOM_VK_HOME
      ) {
        elementToFocus = visibleElements[0];
      } else if (
        event.keyCode == KeyCodes.DOM_VK_PAGE_DOWN ||
        event.keyCode == KeyCodes.DOM_VK_END
      ) {
        elementToFocus = visibleElements.at(-1);
      } else if (event.keyCode == KeyCodes.DOM_VK_UP) {
        const focusedIndex = visibleElements.indexOf(
          getFocusedItemWithin(this._nav)
        );
        elementToFocus = visibleElements[focusedIndex - 1];
      } else if (event.keyCode == KeyCodes.DOM_VK_DOWN) {
        const focusedIndex = visibleElements.indexOf(
          getFocusedItemWithin(this._nav)
        );
        elementToFocus = visibleElements[focusedIndex + 1];
      }

      if (elementToFocus !== undefined) {
        event.stopPropagation();
        event.preventDefault();
        elementToFocus.focus();
        return false;
      }

      return true;
    });
  }

  /**
   * Retrieve the root element.
   *
   * @return DOMElement
   */
  get rootElement() {
    return this._root;
  }

  /**
   * Set the active item's summary element.
   *
   * @param DOMElement summary
   * @param {Object} options
   * @param {String=} options.reason: Indicates why the summary was selected. It's set to
   *                  "filter-auto" when the summary was automatically selected as the result
   *                  of the previous active summary being filtered out.
   */
  setActiveSummary(summary, options = {}) {
    if (summary == this._activeSummary) {
      return;
    }

    if (this._activeSummary) {
      const binding = bindings.get(this._activeSummary);

      this._activeSummary.classList.remove("splitview-active");
      binding._details.classList.remove("splitview-active");
    }

    this._activeSummary = summary;
    if (!summary) {
      this.emit("active-summary-cleared");
      return;
    }

    const binding = bindings.get(summary);
    summary.classList.add("splitview-active");
    binding._details.classList.add("splitview-active");

    if (binding.onShow) {
      binding.onShow(binding._details, options);
    }
  }

  /**
   * Append an item to the split view.
   *
   * @param {Object} options
   *        Optional object that defines custom behavior and data for the item.
   *        All properties are optional.
   * @param {function} [options.onShow]
   *         Called with the detailed element when the item is shown/active, and a second
   *         parameter, an object, that may hold a "reason" string property, indicating
   *         why the editor was shown.
   * @param {Number} [options.ordinal]
   *         Items with a lower ordinal are displayed before those with a higher ordinal.
   */
  appendItem(options) {
    // Create the detail and summary nodes from the templates node (declared in index.xhtml)
    const details = this._tplDetails.cloneNode(true);
    details.id = "";
    const summary = this._tplSummary.cloneNode(true);
    summary.id = "";

    if (options?.ordinal !== undefined) {
      // can be zero
      summary.style.MozBoxOrdinalGroup = options.ordinal;
      summary.setAttribute("data-ordinal", options.ordinal);
    }
    summary.addEventListener("click", event => {
      event.stopPropagation();
      this.setActiveSummary(summary);
    });

    this._nav.appendChild(summary);
    this._side.appendChild(details);

    bindings.set(summary, {
      _summary: summary,
      _details: details,
      onShow: options?.onShow,
    });

    return { summary, details };
  }

  /**
   * Remove an item from the split view.
   *
   * @param DOMElement summary
   *        Summary element of the item to remove.
   */
  removeItem(summary) {
    if (summary == this._activeSummary) {
      this.setActiveSummary(null);
    }

    const binding = bindings.get(summary);
    if (!binding) {
      return;
    }

    summary.remove();
    binding._details.remove();
  }

  /**
   * Remove all items from the split view.
   */
  removeAll() {
    while (this._nav.hasChildNodes()) {
      this.removeItem(this._nav.firstChild);
    }
  }

  /**
   * Filter the list
   *
   * @param String str
   */
  setFilter(str) {
    this.#filter = str;
    for (const summary of this._nav.childNodes) {
      // Don't update nav class for every element, we do it after the loop.
      this.handleSummaryVisibility(summary, {
        triggerOnFilterStateChange: false,
      });
    }

    this.#onFilterStateChange();

    if (this._activeSummary == null) {
      const firstVisibleSummary = Array.from(this._nav.childNodes).find(
        node => !node.classList.contains(SplitView.FILTERED_CLASSNAME)
      );

      if (firstVisibleSummary) {
        this.setActiveSummary(firstVisibleSummary, { reason: "filter-auto" });
      }
    }
  }

  /**
   * @emits "filter-state-change"
   */
  #onFilterStateChange() {
    const summaries = Array.from(this._nav.childNodes);
    const hasVisibleSummary = summaries.some(
      node => !node.classList.contains(SplitView.FILTERED_CLASSNAME)
    );
    const allFiltered = summaries.length > 0 && !hasVisibleSummary;

    this._nav.classList.toggle(SplitView.ALL_FILTERED_CLASSNAME, allFiltered);
    this.emit("filter-state-change", { allFiltered });
  }

  /**
   * Make the passed element visible or not, depending if it matches the current filter
   *
   * @param {Element} summary
   * @param {Object} options
   * @param {Boolean} options.triggerOnFilterStateChange: Set to false to avoid calling
   *                  #onFilterStateChange directly here. This can be useful when this
   *                  function is called for every item of the list, like in `setFilter`.
   */
  handleSummaryVisibility(summary, { triggerOnFilterStateChange = true } = {}) {
    if (!this.#filter) {
      summary.classList.remove(SplitView.FILTERED_CLASSNAME);
      return;
    }

    const label = summary.querySelector(".stylesheet-name label");
    const itemText = label.value.toLowerCase();
    const matchesSearch = itemText.includes(this.#filter.toLowerCase());
    summary.classList.toggle(SplitView.FILTERED_CLASSNAME, !matchesSearch);

    if (this._activeSummary == summary && !matchesSearch) {
      this.setActiveSummary(null);
    }

    if (triggerOnFilterStateChange) {
      this.#onFilterStateChange();
    }
  }
}
