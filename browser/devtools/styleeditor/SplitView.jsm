/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Style Editor code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Cedric Vivier <cedricv@neonux.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

"use strict";

const EXPORTED_SYMBOLS = ["SplitView"];

/* this must be kept in sync with CSS (ie. splitview.css) */
const LANDSCAPE_MEDIA_QUERY = "(min-aspect-ratio: 5/3)";

const BINDING_USERDATA = "splitview-binding";


/**
 * SplitView constructor
 *
 * Initialize the split view UI on an existing DOM element.
 *
 * A split view contains items, each of those having one summary and one details
 * elements.
 * It is adaptive as it behaves similarly to a richlistbox when there the aspect
 * ratio is narrow or as a pair listbox-box otherwise.
 *
 * @param DOMElement aRoot
 * @see appendItem
 */
function SplitView(aRoot)
{
  this._root = aRoot;
  this._controller = aRoot.querySelector(".splitview-controller");
  this._nav = aRoot.querySelector(".splitview-nav");
  this._side = aRoot.querySelector(".splitview-side-details");
  this._activeSummary = null

  this._mql = aRoot.ownerDocument.defaultView.matchMedia(LANDSCAPE_MEDIA_QUERY);

  this._filter = aRoot.querySelector(".splitview-filter");
  if (this._filter) {
    this._setupFilterBox();
  }

  // items list focus and search-on-type handling
  this._nav.addEventListener("keydown", function onKeyCatchAll(aEvent) {
    function getFocusedItemWithin(nav) {
      let node = nav.ownerDocument.activeElement;
      while (node && node.parentNode != nav) {
        node = node.parentNode;
      }
      return node;
    }

    // do not steal focus from inside iframes or textboxes
    if (aEvent.target.ownerDocument != this._nav.ownerDocument ||
        aEvent.target.tagName == "input" ||
        aEvent.target.tagName == "textbox" ||
        aEvent.target.tagName == "textarea" ||
        aEvent.target.classList.contains("textbox")) {
      return false;
    }

    // handle keyboard navigation within the items list
    let newFocusOrdinal;
    if (aEvent.keyCode == aEvent.DOM_VK_PAGE_UP ||
        aEvent.keyCode == aEvent.DOM_VK_HOME) {
      newFocusOrdinal = 0;
    } else if (aEvent.keyCode == aEvent.DOM_VK_PAGE_DOWN ||
               aEvent.keyCode == aEvent.DOM_VK_END) {
      newFocusOrdinal = this._nav.childNodes.length - 1;
    } else if (aEvent.keyCode == aEvent.DOM_VK_UP) {
      newFocusOrdinal = getFocusedItemWithin(this._nav).getAttribute("data-ordinal");
      newFocusOrdinal--;
    } else if (aEvent.keyCode == aEvent.DOM_VK_DOWN) {
      newFocusOrdinal = getFocusedItemWithin(this._nav).getAttribute("data-ordinal");
      newFocusOrdinal++;
    }
    if (newFocusOrdinal !== undefined) {
      aEvent.stopPropagation();
      let el = this.getSummaryElementByOrdinal(newFocusOrdinal);
      if (el) {
        el.focus();
      }
      return false;
    }

    // search-on-type when any non-whitespace character is pressed while list
    // has the focus
    if (this._filter &&
        !/\s/.test(String.fromCharCode(aEvent.which))) {
      this._filter.focus();
    }
  }.bind(this), false);
}

SplitView.prototype = {
  /**
    * Retrieve whether the UI currently has a landscape orientation.
    *
    * @return boolean
    */
  get isLandscape() this._mql.matches,

  /**
    * Retrieve the root element.
    *
    * @return DOMElement
    */
  get rootElement() this._root,

  /**
    * Retrieve the active item's summary element or null if there is none.
    *
    * @return DOMElement
    */
  get activeSummary() this._activeSummary,

  /**
    * Set the active item's summary element.
    *
    * @param DOMElement aSummary
    */
  set activeSummary(aSummary)
  {
    if (aSummary == this._activeSummary) {
      return;
    }

    if (this._activeSummary) {
      let binding = this._activeSummary.getUserData(BINDING_USERDATA);

      if (binding.onHide) {
        binding.onHide(this._activeSummary, binding._details, binding.data);
      }

      this._activeSummary.classList.remove("splitview-active");
      binding._details.classList.remove("splitview-active");
    }

    if (!aSummary) {
      return;
    }

    let binding = aSummary.getUserData(BINDING_USERDATA);
    aSummary.classList.add("splitview-active");
    binding._details.classList.add("splitview-active");

    this._activeSummary = aSummary;

    if (binding.onShow) {
      binding.onShow(aSummary, binding._details, binding.data);
    }
    aSummary.scrollIntoView();
  },

  /**
    * Retrieve the active item's details element or null if there is none.
    * @return DOMElement
    */
  get activeDetails()
  {
    let summary = this.activeSummary;
    return summary ? summary.getUserData(BINDING_USERDATA)._details : null;
  },

  /**
   * Retrieve the summary element for a given ordinal.
   *
   * @param number aOrdinal
   * @return DOMElement
   *         Summary element with given ordinal or null if not found.
   * @see appendItem
   */
  getSummaryElementByOrdinal: function SEC_getSummaryElementByOrdinal(aOrdinal)
  {
    return this._nav.querySelector("* > li[data-ordinal='" + aOrdinal + "']");
  },

  /**
   * Append an item to the split view.
   *
   * @param DOMElement aSummary
   *        The summary element for the item.
   * @param DOMElement aDetails
   *        The details element for the item.
   * @param object aOptions
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
   *     - function(summary, details, data, query) onFilterBy
   *         Called when the user performs a filtering search.
   *         If the function returns false, the item does not match query
   *         string and will be hidden.
   *     - object data
   *         Object to pass to the callbacks above.
   *     - boolean disableAnimations
   *         If true there is no animation or scrolling when this item is
   *         appended. Set this when batch appending (eg. initial population).
   *     - number ordinal
   *         Items with a lower ordinal are displayed before those with a
   *         higher ordinal.
   */
  appendItem: function ASV_appendItem(aSummary, aDetails, aOptions)
  {
    let binding = aOptions || {};

    binding._summary = aSummary;
    binding._details = aDetails;
    aSummary.setUserData(BINDING_USERDATA, binding, null);

    if (!binding.disableAnimations) {
      aSummary.classList.add("splitview-slide");
      aSummary.classList.add("splitview-flash");
    }
    this._nav.appendChild(aSummary);

    aSummary.addEventListener("click", function onSummaryClick(aEvent) {
      aEvent.stopPropagation();
      this.activeSummary = aSummary;
    }.bind(this), false);

    this._side.appendChild(aDetails);

    if (binding.onCreate) {
      // queue onCreate handler
      this._root.ownerDocument.defaultView.setTimeout(function () {
        binding.onCreate(aSummary, aDetails, binding.data);
      }, 0);
    }

    if (!binding.disableAnimations) {
      scheduleAnimation(aSummary, "splitview-slide", "splitview-flash");
      aSummary.scrollIntoView();
    }
  },

  /**
   * Append an item to the split view according to two template elements
   * (one for the item's summary and the other for the item's details).
   *
   * @param string aName
   *        Name of the template elements to instantiate.
   *        Requires two (hidden) DOM elements with id "splitview-tpl-summary-"
   *        and "splitview-tpl-details-" suffixed with aName.
   * @param object aOptions
   *        Optional object that defines custom behavior and data for the item.
   *        See appendItem for full description.
   * @return object{summary:,details:}
   *         Object with the new DOM elements created for summary and details.
   * @see appendItem
   */
  appendTemplatedItem: function ASV_appendTemplatedItem(aName, aOptions)
  {
    aOptions = aOptions || {};
    let summary = this._root.querySelector("#splitview-tpl-summary-" + aName);
    let details = this._root.querySelector("#splitview-tpl-details-" + aName);

    summary = summary.cloneNode(true);
    summary.id = "";
    if (aOptions.ordinal !== undefined) { // can be zero
      summary.style.MozBoxOrdinalGroup = aOptions.ordinal;
      summary.setAttribute("data-ordinal", aOptions.ordinal);
    }
    details = details.cloneNode(true);
    details.id = "";

    this.appendItem(summary, details, aOptions);
    return {summary: summary, details: details};
  },

  /**
    * Remove an item from the split view.
    *
    * @param DOMElement aSummary
    *        Summary element of the item to remove.
    */
  removeItem: function ASV_removeItem(aSummary)
  {
    if (aSummary == this._activeSummary) {
      this.activeSummary = null;
    }

    let binding = aSummary.getUserData(BINDING_USERDATA);
    aSummary.parentNode.removeChild(aSummary);
    binding._details.parentNode.removeChild(binding._details);

    if (binding.onDestroy) {
      binding.onDestroy(aSummary, binding._details, binding.data);
    }
  },

  /**
   * Remove all items from the split view.
   */
  removeAll: function ASV_removeAll()
  {
    while (this._nav.hasChildNodes()) {
      this.removeItem(this._nav.firstChild);
    }
  },

  /**
    * Filter items by given string.
    * Matching is performed on every item by calling onFilterBy when defined
    * and then by searching aQuery in the summary element's text item.
    * Non-matching item is hidden.
    *
    * If no item matches, 'splitview-all-filtered' class is set on the filter
    * input element and the splitview-nav element.
    *
    * @param string aQuery
    *        The query string. Use null to reset (no filter).
    * @return number
    *         The number of filtered (non-matching) item.
    */
  filterItemsBy: function ASV_filterItemsBy(aQuery)
  {
    if (!this._nav.hasChildNodes()) {
      return 0;
    }
    if (aQuery) {
      aQuery = aQuery.trim();
    }
    if (!aQuery) {
      for (let i = 0; i < this._nav.childNodes.length; ++i) {
        this._nav.childNodes[i].classList.remove("splitview-filtered");
      }
      this._filter.classList.remove("splitview-all-filtered");
      this._nav.classList.remove("splitview-all-filtered");
      return 0;
    }

    let count = 0;
    let filteredCount = 0;
    for (let i = 0; i < this._nav.childNodes.length; ++i) {
      let summary = this._nav.childNodes[i];

      let matches = false;
      let binding = summary.getUserData(BINDING_USERDATA);
      if (binding.onFilterBy) {
        matches = binding.onFilterBy(summary, binding._details, binding.data, aQuery);
      }
      if (!matches) { // try text content
        let content = summary.textContent.toUpperCase();
        matches = (content.indexOf(aQuery.toUpperCase()) > -1);
      }

      count++;
      if (!matches) {
        summary.classList.add("splitview-filtered");
        filteredCount++;
      } else {
        summary.classList.remove("splitview-filtered");
      }
    }

    if (count > 0 && filteredCount == count) {
      this._filter.classList.add("splitview-all-filtered");
      this._nav.classList.add("splitview-all-filtered");
    } else {
      this._filter.classList.remove("splitview-all-filtered");
      this._nav.classList.remove("splitview-all-filtered");
    }
    return filteredCount;
  },

  /**
   * Set the item's CSS class name.
   * This sets the class on both the summary and details elements, retaining
   * any SplitView-specific classes.
   *
   * @param DOMElement aSummary
   *        Summary element of the item to set.
   * @param string aClassName
   *        One or more space-separated CSS classes.
   */
  setItemClassName: function ASV_setItemClassName(aSummary, aClassName)
  {
    let binding = aSummary.getUserData(BINDING_USERDATA);
    let viewSpecific;

    viewSpecific = aSummary.className.match(/(splitview\-[\w-]+)/g);
    viewSpecific = viewSpecific ? viewSpecific.join(" ") : "";
    aSummary.className = viewSpecific + " " + aClassName;

    viewSpecific = binding._details.className.match(/(splitview\-[\w-]+)/g);
    viewSpecific = viewSpecific ? viewSpecific.join(" ") : "";
    binding._details.className = viewSpecific + " " + aClassName;
  },

  /**
   * Set up filter search box.
   */
  _setupFilterBox: function ASV__setupFilterBox()
  {
    let clearFilter = function clearFilter(aEvent) {
      this._filter.value = "";
      this.filterItemsBy("");
      return false;
    }.bind(this);

    this._filter.addEventListener("command", function onFilterInput(aEvent) {
      this.filterItemsBy(this._filter.value);
    }.bind(this), false);

    this._filter.addEventListener("keyup", function onFilterKeyUp(aEvent) {
      if (aEvent.keyCode == aEvent.DOM_VK_ESCAPE) {
        clearFilter();
      }
      if (aEvent.keyCode == aEvent.DOM_VK_ENTER ||
          aEvent.keyCode == aEvent.DOM_VK_RETURN) {
        // autofocus matching item if there is only one
        let matches = this._nav.querySelectorAll("* > li:not(.splitview-filtered)");
        if (matches.length == 1) {
          this.activeSummary = matches[0];
        }
      }
    }.bind(this), false);

    let clearButtons = this._root.querySelectorAll(".splitview-filter-clearButton");
    for (let i = 0; i < clearButtons.length; ++i) {
      clearButtons[i].addEventListener("click", clearFilter, false);
    }
  }
};

//
// private helpers

/**
 * Schedule one or multiple CSS animation(s) on an element.
 *
 * @param DOMElement aElement
 * @param string ...
 *        One or multiple animation class name(s).
 */
function scheduleAnimation(aElement)
{
  let classes = Array.prototype.slice.call(arguments, 1);
  for each (let klass in classes) {
    aElement.classList.add(klass);
  }

  let window = aElement.ownerDocument.defaultView;
  window.mozRequestAnimationFrame(function triggerAnimation() {
    for each (let klass in classes) {
      aElement.classList.remove(klass);
    }
  });
}
