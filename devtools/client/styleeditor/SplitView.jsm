/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { require } = ChromeUtils.import(
  "resource://devtools/shared/loader/Loader.jsm"
);
const { KeyCodes } = require("devtools/client/shared/keycodes");

const EXPORTED_SYMBOLS = ["SplitView"];

var bindings = new WeakMap();

class SplitView {
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
    this._root = root;
    this._controller = root.querySelector(".splitview-controller");
    this._nav = root.querySelector(".splitview-nav");
    this._side = root.querySelector(".splitview-side-details");
    this._tplSummary = root.querySelector("#splitview-tpl-summary-stylesheet");
    this._tplDetails = root.querySelector("#splitview-tpl-details-stylesheet");
    this._activeSummary = null;
    this._filter = null;

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
      let newFocusOrdinal;
      if (
        event.keyCode == KeyCodes.DOM_VK_PAGE_UP ||
        event.keyCode == KeyCodes.DOM_VK_HOME
      ) {
        newFocusOrdinal = 0;
      } else if (
        event.keyCode == KeyCodes.DOM_VK_PAGE_DOWN ||
        event.keyCode == KeyCodes.DOM_VK_END
      ) {
        newFocusOrdinal = this._nav.childNodes.length - 1;
      } else if (event.keyCode == KeyCodes.DOM_VK_UP) {
        newFocusOrdinal = getFocusedItemWithin(this._nav).getAttribute(
          "data-ordinal"
        );
        newFocusOrdinal--;
      } else if (event.keyCode == KeyCodes.DOM_VK_DOWN) {
        newFocusOrdinal = getFocusedItemWithin(this._nav).getAttribute(
          "data-ordinal"
        );
        newFocusOrdinal++;
      }
      if (newFocusOrdinal !== undefined) {
        event.stopPropagation();
        const el = this.#getSummaryElementByOrdinal(newFocusOrdinal);
        if (el) {
          el.focus();
        }
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
   */
  setActiveSummary(summary) {
    if (summary == this._activeSummary) {
      return;
    }

    if (this._activeSummary) {
      const binding = bindings.get(this._activeSummary);

      this._activeSummary.classList.remove("splitview-active");
      binding._details.classList.remove("splitview-active");
    }

    if (!summary) {
      return;
    }

    const binding = bindings.get(summary);
    summary.classList.add("splitview-active");
    binding._details.classList.add("splitview-active");

    this._activeSummary = summary;

    if (binding.onShow) {
      binding.onShow(binding._details);
    }
  }

  /**
   * Retrieve the summary element for a given ordinal.
   *
   * @param number ordinal
   * @return DOMElement
   *         Summary element with given ordinal or null if not found.
   * @see appendItem
   */
  #getSummaryElementByOrdinal(ordinal) {
    return this._nav.querySelector("* > li[data-ordinal='" + ordinal + "']");
  }

  /**
   * Append an item to the split view.
   *
   * @param {Object} options
   *        Optional object that defines custom behavior and data for the item.
   *        All properties are optional.
   * @param {function} [options.onShow]
   *         Called with the detailed element when the item is shown/active.
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
}
