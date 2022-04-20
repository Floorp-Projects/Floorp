/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { require } = ChromeUtils.import(
  "resource://devtools/shared/loader/Loader.jsm"
);
const { KeyCodes } = require("devtools/client/shared/keycodes");

const EXPORTED_SYMBOLS = ["SplitView"];

/* this must be kept in sync with CSS (ie. splitview.css) */
const LANDSCAPE_MEDIA_QUERY = "(min-width: 701px)";

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
    this._activeSummary = null;
    this._filter = null;

    this._mql = root.ownerDocument.defaultView.matchMedia(
      LANDSCAPE_MEDIA_QUERY
    );

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
        const el = this.getSummaryElementByOrdinal(newFocusOrdinal);
        if (el) {
          el.focus();
        }
        return false;
      }

      return true;
    });
  }

  /**
   * Retrieve whether the UI currently has a landscape orientation.
   *
   * @return boolean
   */
  get isLandscape() {
    return this._mql.matches;
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
   * Retrieve the active item's summary element or null if there is none.
   *
   * @return DOMElement
   */
  get activeSummary() {
    return this._activeSummary;
  }

  /**
   * Set the active item's summary element.
   *
   * @param DOMElement summary
   */
  set activeSummary(summary) {
    if (summary == this._activeSummary) {
      return;
    }

    if (this._activeSummary) {
      const binding = bindings.get(this._activeSummary);

      if (binding.onHide) {
        binding.onHide(this._activeSummary, binding._details, binding.data);
      }

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
      binding.onShow(summary, binding._details, binding.data);
    }
  }

  /**
   * Retrieve the active item's details element or null if there is none.
   * @return DOMElement
   */
  get activeDetails() {
    const summary = this.activeSummary;
    return summary ? bindings.get(summary)._details : null;
  }

  /**
   * Retrieve the summary element for a given ordinal.
   *
   * @param number ordinal
   * @return DOMElement
   *         Summary element with given ordinal or null if not found.
   * @see appendItem
   */
  getSummaryElementByOrdinal(ordinal) {
    return this._nav.querySelector("* > li[data-ordinal='" + ordinal + "']");
  }

  /**
   * Append an item to the split view.
   *
   * @param DOMElement summary
   *        The summary element for the item.
   * @param DOMElement details
   *        The details element for the item.
   * @param object options
   *     Optional object that defines custom behavior and data for the item.
   *     All properties are optional :
   *     - function(DOMElement summary, DOMElement details, object data) onCreate
   *         Called when the item has been added.
   *     - function(summary, details, data) onShow
   *         Called when the item is shown/active.
   *     - function(summary, details, data) onHide
   *         Called when the item is hidden/inactive.
   *     - function(summary, details, data) onDestroy
   *         Called when the item has been removed.
   *     - object data
   *         Object to pass to the callbacks above.
   *     - number ordinal
   *         Items with a lower ordinal are displayed before those with a
   *         higher ordinal.
   */
  appendItem(summary, details, options) {
    const binding = options || {};

    binding._summary = summary;
    binding._details = details;
    bindings.set(summary, binding);

    this._nav.appendChild(summary);

    summary.addEventListener("click", event => {
      event.stopPropagation();
      this.activeSummary = summary;
    });

    this._side.appendChild(details);

    if (binding.onCreate) {
      binding.onCreate(summary, details, binding.data);
    }
  }

  /**
   * Append an item to the split view according to two template elements
   * (one for the item's summary and the other for the item's details).
   *
   * @param string name
   *        Name of the template elements to instantiate.
   *        Requires two (hidden) DOM elements with id "splitview-tpl-summary-"
   *        and "splitview-tpl-details-" suffixed with name.
   * @param object options
   *        Optional object that defines custom behavior and data for the item.
   *        See appendItem for full description.
   * @return object{summary:,details:}
   *         Object with the new DOM elements created for summary and details.
   * @see appendItem
   */
  appendTemplatedItem(name, options) {
    options = options || {};
    let summary = this._root.querySelector("#splitview-tpl-summary-" + name);
    let details = this._root.querySelector("#splitview-tpl-details-" + name);

    summary = summary.cloneNode(true);
    summary.id = "";
    if (options.ordinal !== undefined) {
      // can be zero
      summary.style.MozBoxOrdinalGroup = options.ordinal;
      summary.setAttribute("data-ordinal", options.ordinal);
    }
    details = details.cloneNode(true);
    details.id = "";

    this.appendItem(summary, details, options);
    return { summary: summary, details: details };
  }

  /**
   * Remove an item from the split view.
   *
   * @param DOMElement summary
   *        Summary element of the item to remove.
   */
  removeItem(summary) {
    if (summary == this._activeSummary) {
      this.activeSummary = null;
    }

    const binding = bindings.get(summary);
    summary.remove();
    binding._details.remove();

    if (binding.onDestroy) {
      binding.onDestroy(summary, binding._details, binding.data);
    }
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
   * Set the item's CSS class name.
   * This sets the class on both the summary and details elements, retaining
   * any SplitView-specific classes.
   *
   * @param DOMElement summary
   *        Summary element of the item to set.
   * @param string className
   *        One or more space-separated CSS classes.
   */
  setItemClassName(summary, className) {
    const binding = bindings.get(summary);
    let viewSpecific;

    viewSpecific = summary.className.match(/(splitview\-[\w-]+)/g);
    viewSpecific = viewSpecific ? viewSpecific.join(" ") : "";
    summary.className = viewSpecific + " " + className;

    viewSpecific = binding._details.className.match(/(splitview\-[\w-]+)/g);
    viewSpecific = viewSpecific ? viewSpecific.join(" ") : "";
    binding._details.className = viewSpecific + " " + className;
  }
}
