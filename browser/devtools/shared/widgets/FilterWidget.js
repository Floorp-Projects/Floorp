/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
  * This is a CSS Filter Editor widget used
  * for Rule View's filter swatches
  */

const EventEmitter = require("devtools/toolkit/event-emitter");
const { Cu } = require("chrome");
const { ViewHelpers } = Cu.import("resource:///modules/devtools/ViewHelpers.jsm", {});
const STRINGS_URI = "chrome://browser/locale/devtools/filterwidget.properties";
const L10N = new ViewHelpers.L10N(STRINGS_URI);

const DEFAULT_FILTER_TYPE = "length";
const UNIT_MAPPING = {
  percentage: "%",
  length: "px",
  angle: "deg",
  string: ""
};

const FAST_VALUE_MULTIPLIER = 10;
const SLOW_VALUE_MULTIPLIER = 0.1;
const DEFAULT_VALUE_MULTIPLIER = 1;

const LIST_PADDING = 7;
const LIST_ITEM_HEIGHT = 32;

const filterList = [
  {
    "name": "blur",
    "range": [0, Infinity],
    "type": "length"
  },
  {
    "name": "brightness",
    "range": [0, Infinity],
    "type": "percentage"
  },
  {
    "name": "contrast",
    "range": [0, Infinity],
    "type": "percentage"
  },
  {
    "name": "drop-shadow",
    "placeholder": L10N.getStr("dropShadowPlaceholder"),
    "type": "string"
  },
  {
    "name": "grayscale",
    "range": [0, 100],
    "type": "percentage"
  },
  {
    "name": "hue-rotate",
    "range": [0, 360],
    "type": "angle"
  },
  {
    "name": "invert",
    "range": [0, 100],
    "type": "percentage"
  },
  {
    "name": "opacity",
    "range": [0, 100],
    "type": "percentage"
  },
  {
    "name": "saturate",
    "range": [0, Infinity],
    "type": "percentage"
  },
  {
    "name": "sepia",
    "range": [0, 100],
    "type": "percentage"
  },
  {
    "name": "url",
    "placeholder": "example.svg#c1",
    "type": "string"
  }
];

/**
 * A CSS Filter editor widget used to add/remove/modify
 * filters.
 *
 * Normally, it takes a CSS filter value as input, parses it
 * and creates the required elements / bindings.
 *
 * You can, however, use add/remove/update methods manually.
 * See each method's comments for more details
 *
 * @param {nsIDOMNode} el
 *        The widget container.
 * @param {String} value
 *        CSS filter value
 */
function CSSFilterEditorWidget(el, value = "") {
  this.doc = el.ownerDocument;
  this.win = this.doc.ownerGlobal;
  this.el = el;

  this._addButtonClick = this._addButtonClick.bind(this);
  this._removeButtonClick = this._removeButtonClick.bind(this);
  this._mouseMove = this._mouseMove.bind(this);
  this._mouseUp = this._mouseUp.bind(this);
  this._mouseDown = this._mouseDown.bind(this);
  this._input = this._input.bind(this);

  this._initMarkup();
  this._buildFilterItemMarkup();
  this._addEventListeners();

  EventEmitter.decorate(this);

  this.filters = [];
  this.setCssValue(value);
}

exports.CSSFilterEditorWidget = CSSFilterEditorWidget;

CSSFilterEditorWidget.prototype = {
  _initMarkup: function() {
    const list = this.el.querySelector(".filters");
    this.el.appendChild(list);
    this.el.insertBefore(list, this.el.firstChild);
    this.container = this.el;
    this.list = list;

    this.filterSelect = this.el.querySelector("select");
    this._populateFilterSelect();
  },

  /**
    * Creates <option> elements for each filter definition
    * in filterList
    */
  _populateFilterSelect: function() {
    let select = this.filterSelect;
    filterList.forEach(filter => {
      let option = this.doc.createElement("option");
      option.innerHTML = option.value = filter.name;
      select.appendChild(option);
    });
  },

  /**
    * Creates a template for filter elements which is cloned and used in render
    */
  _buildFilterItemMarkup: function() {
    let base = this.doc.createElement("div");
    base.className = "filter";

    let name = this.doc.createElement("div");
    name.className = "filter-name";

    let value = this.doc.createElement("div");
    value.className = "filter-value";

    let drag = this.doc.createElement("i");
    drag.title = L10N.getStr("dragHandleTooltipText");

    let label = this.doc.createElement("label");

    name.appendChild(drag);
    name.appendChild(label);

    let unitPreview = this.doc.createElement("span");
    let input = this.doc.createElement("input");
    input.classList.add("devtools-textinput");

    value.appendChild(input);
    value.appendChild(unitPreview);

    let removeButton = this.doc.createElement("button");
    removeButton.className = "remove-button";
    value.appendChild(removeButton);

    base.appendChild(name);
    base.appendChild(value);

    this._filterItemMarkup = base;
  },

  _addEventListeners: function() {
    this.addButton = this.el.querySelector("#add-filter");
    this.addButton.addEventListener("click", this._addButtonClick);
    this.list.addEventListener("click", this._removeButtonClick);
    this.list.addEventListener("mousedown", this._mouseDown);

    // These events are event delegators for
    // drag-drop re-ordering and label-dragging
    this.win.addEventListener("mousemove", this._mouseMove);
    this.win.addEventListener("mouseup", this._mouseUp);

    // Used to workaround float-precision problems
    this.list.addEventListener("input", this._input);
  },

  _input: function(e) {
    let filterEl = e.target.closest(".filter"),
        index = [...this.list.children].indexOf(filterEl),
        filter = this.filters[index],
        def = this._definition(filter.name);

    if (def.type !== "string") {
      e.target.value = fixFloat(e.target.value);
    }
    this.updateValueAt(index, e.target.value);
  },

  _mouseDown: function(e) {
    let filterEl = e.target.closest(".filter");

    // re-ordering drag handle
    if (e.target.tagName.toLowerCase() === "i") {
      this.isReorderingFilter = true;
      filterEl.startingY = e.pageY;
      filterEl.classList.add("dragging");

      this.container.classList.add("dragging");
    // label-dragging
    } else if (e.target.classList.contains("devtools-draglabel")) {
      let label = e.target,
          input = filterEl.querySelector("input"),
          index = [...this.list.children].indexOf(filterEl);

      this._dragging = {
        index, label, input,
        startX: e.pageX
      };

      this.isDraggingLabel = true;
    }
  },

  _addButtonClick: function() {
    const select = this.filterSelect;
    if (!select.value) {
      return;
    }

    const key = select.value;
    const def = this._definition(key);
    // UNIT_MAPPING[string] is an empty string (falsy), so
    // using || doesn't work here
    const unitLabel = typeof UNIT_MAPPING[def.type] === "undefined" ?
                             UNIT_MAPPING[DEFAULT_FILTER_TYPE] :
                             UNIT_MAPPING[def.type];

    // string-type filters have no default value but a placeholder instead
    if (!unitLabel) {
      this.add(key);
    } else {
      this.add(key, def.range[0] + unitLabel);
    }

    this.render();
  },

  _removeButtonClick: function(e) {
    const isRemoveButton = e.target.classList.contains("remove-button");
    if (!isRemoveButton) {
      return;
    }

    let filterEl = e.target.closest(".filter");
    let index = [...this.list.children].indexOf(filterEl);
    this.removeAt(index);
  },

  _mouseMove: function(e) {
    if (this.isReorderingFilter) {
      this._dragFilterElement(e);
    } else if (this.isDraggingLabel) {
      this._dragLabel(e);
    }
  },

  _dragFilterElement: function(e) {
    const rect = this.list.getBoundingClientRect(),
          top = e.pageY - LIST_PADDING,
          bottom = e.pageY + LIST_PADDING;
    // don't allow dragging over top/bottom of list
    if (top < rect.top || bottom > rect.bottom) {
      return;
    }

    const filterEl = this.list.querySelector(".dragging");

    const delta = e.pageY - filterEl.startingY;
    filterEl.style.top = delta + "px";

    // change is the number of _steps_ taken from initial position
    // i.e. how many elements we have passed
    let change = delta / LIST_ITEM_HEIGHT;
    change = change > 0 ? Math.floor(change) :
             change < 0 ? Math.ceil(change) : change;

    const children = this.list.children;
    const index = [...children].indexOf(filterEl);
    const destination = index + change;

    // If we're moving out, or there's no change at all, stop and return
    if (destination >= children.length || destination < 0 || change === 0) {
      return;
    }

    // Re-order filter objects
    swapArrayIndices(this.filters, index, destination);

    // Re-order the dragging element in markup
    const target = change > 0 ? children[destination + 1]
                              : children[destination];
    if (target) {
      this.list.insertBefore(filterEl, target);
    } else {
      this.list.appendChild(filterEl);
    }

    filterEl.removeAttribute("style");

    const currentPosition = change * LIST_ITEM_HEIGHT;
    filterEl.startingY = e.pageY + currentPosition - delta;
  },

  _dragLabel: function(e) {
    let dragging = this._dragging;

    let input = dragging.input;

    let multiplier = DEFAULT_VALUE_MULTIPLIER;

    if (e.altKey) {
      multiplier = SLOW_VALUE_MULTIPLIER;
    } else if (e.shiftKey) {
      multiplier = FAST_VALUE_MULTIPLIER;
    }

    dragging.lastX = e.pageX;
    const delta = e.pageX - dragging.startX;
    const startValue = parseFloat(input.value);
    let value = startValue + delta * multiplier;

    const filter = this.filters[dragging.index];
    const [min, max] = this._definition(filter.name).range;
    value = value < min ? min :
            value > max ? max : value;

    input.value = fixFloat(value);

    dragging.startX = e.pageX;

    this.updateValueAt(dragging.index, value);
  },

  _mouseUp: function() {
    // Label-dragging is disabled on mouseup
    this._dragging = null;
    this.isDraggingLabel = false;

    // Filter drag/drop needs more cleaning
    if (!this.isReorderingFilter) {
      return;
    }
    let filterEl = this.list.querySelector(".dragging");

    this.isReorderingFilter = false;
    filterEl.classList.remove("dragging");
    this.container.classList.remove("dragging");
    filterEl.removeAttribute("style");

    this.emit("updated", this.getCssValue());
    this.render();
  },

  /**
   * Clears the list and renders filters, binding required events.
   * There are some delegated events bound in _addEventListeners method
   */
  render: function() {
    if (!this.filters.length) {
      this.list.innerHTML = `<p> ${L10N.getStr("emptyFilterList")} <br />
                                 ${L10N.getStr("addUsingList")} </p>`;
      this.emit("render");
      return;
    }

    this.list.innerHTML = "";

    let base = this._filterItemMarkup;

    for (let filter of this.filters) {
      const def = this._definition(filter.name);

      let el = base.cloneNode(true);

      let [name, value] = el.children,
          label = name.children[1],
          [input, unitPreview] = value.children;

      let min, max;
      if (def.range) {
        [min, max] = def.range;
      }

      label.textContent = filter.name;
      input.value = filter.value;

      switch (def.type) {
        case "percentage":
        case "angle":
        case "length":
          input.type = "number";
          input.min = min;
          if (max !== Infinity) {
            input.max = max;
          }
          input.step = "0.1";
        break;
        case "string":
          input.type = "text";
          input.placeholder = def.placeholder;
        break;
      }

      // use photoshop-style label-dragging
      // and show filters' unit next to their <input>
      if (def.type !== "string") {
        unitPreview.textContent = filter.unit;

        label.classList.add("devtools-draglabel");
        label.title = L10N.getStr("labelDragTooltipText");
      } else {
        // string-type filters have no unit
        unitPreview.remove();
      }

      this.list.appendChild(el);
    }

    let el = this.list.querySelector(`.filter:last-of-type input`);
    if (el) {
      el.focus();
      // move cursor to end of input
      el.setSelectionRange(el.value.length, el.value.length);
    }

    this.emit("render");
  },

  /**
    * returns definition of a filter as defined in filterList
    *
    * @param {String} name
    *        filter name (e.g. blur)
    * @return {Object}
    *        filter's definition
    */
  _definition: function(name) {
    return filterList.find(a => a.name === name);
  },

  /**
    * Parses the CSS value specified, updating widget's filters
    *
    * @param {String} cssValue
    *        css value to be parsed
    */
  setCssValue: function(cssValue) {
    if (!cssValue) {
      throw new Error("Missing CSS filter value in setCssValue");
    }

    this.filters = [];

    if (cssValue === "none") {
      this.emit("updated", this.getCssValue());
      this.render();
      return;
    }

    // Apply filter to a temporary element
    // and get the computed value to make parsing
    // easier
    let tmp = this.doc.createElement("i");
    tmp.style.filter = cssValue;
    const computedValue = this.win.getComputedStyle(tmp).filter;

    for (let {name, value} of tokenizeComputedFilter(computedValue)) {
      this.add(name, value);
    }

    this.emit("updated", this.getCssValue());
    this.render();
  },

  /**
    * Creates a new [name] filter record with value
    *
    * @param {String} name
    *        filter name (e.g. blur)
    * @param {String} value
    *        value of the filter (e.g. 30px, 20%)
    * @return {Number}
    *        The index of the new filter in the current list of filters
    */
  add: function(name, value = "") {
    const def = this._definition(name);
    if (!def) {
      return false;
    }

    let unit = def.type === "string"
               ? ""
               : (/[a-zA-Z%]+/.exec(value) || [])[0];

    if (def.type !== "string") {
      value = parseFloat(value);

      // You can omit percentage values' and use a value between 0..1
      if (def.type === "percentage" && !unit) {
        value = value * 100;
        unit = "%";
      }

      const [min, max] = def.range;
      if (value < min) {
        value = min;
      } else if (value > max) {
        value = max;
      }
    }

    const index = this.filters.push({value, unit, name: def.name}) - 1;
    this.emit("updated", this.getCssValue());

    return index;
  },

  /**
    * returns value + unit of the specified filter
    *
    * @param {Number} index
    *        filter index
    * @return {String}
    *        css value of filter
    */
  getValueAt: function(index) {
    let filter = this.filters[index];
    if (!filter) {
      return null;
    }

    const {value, unit} = filter;

    return value + unit;
  },

  removeAt: function(index) {
    if (!this.filters[index]) {
      return null;
    }

    this.filters.splice(index, 1);
    this.emit("updated", this.getCssValue());
    this.render();
  },

  /**
    * Generates CSS filter value for filters of the widget
    *
    * @return {String}
    *        css value of filters
    */
  getCssValue: function() {
    return this.filters.map((filter, i) => {
      return `${filter.name}(${this.getValueAt(i)})`;
    }).join(" ") || "none";
  },

  /**
    * Updates specified filter's value
    *
    * @param {Number} index
    *        The index of the filter in the current list of filters
    * @param {number/string} value
    *        value to set, string for string-typed filters
    *        number for the rest (unit automatically determined)
    */
  updateValueAt: function(index, value) {
    let filter = this.filters[index];
    if (!filter) {
      return;
    }

    const def = this._definition(filter.name);

    if (def.type !== "string") {
      const [min, max] = def.range;
      if (value < min) {
        value = min;
      } else if (value > max) {
        value = max;
      }
    }

    filter.value = filter.unit ? fixFloat(value, true) : value;

    this.emit("updated", this.getCssValue());
  },

  _removeEventListeners: function() {
    this.addButton.removeEventListener("click", this._addButtonClick);
    this.list.removeEventListener("click", this._removeButtonClick);
    this.list.removeEventListener("mousedown", this._mouseDown);

    // These events are used for drag drop re-ordering
    this.win.removeEventListener("mousemove", this._mouseMove);
    this.win.removeEventListener("mouseup", this._mouseUp);

    // Used to workaround float-precision problems
    this.list.removeEventListener("input", this._input);
  },

  _destroyMarkup: function() {
    this._filterItemMarkup.remove();
    this.el.remove();
    this.el = this.list = this.container = this._filterItemMarkup = null;
  },

  destroy: function() {
    this._removeEventListeners();
    this._destroyMarkup();
  }
};

// Fixes JavaScript's float precision
function fixFloat(a, number) {
  let fixed = parseFloat(a).toFixed(1);
  return number ? parseFloat(fixed) : fixed;
}

/**
 * Used to swap two filters' indexes
 * after drag/drop re-ordering
 *
 * @param {Array} array
 *        the array to swap elements of
 * @param {Number} a
 *        index of first element
 * @param {Number} b
 *        index of second element
 */
function swapArrayIndices(array, a, b) {
  array[a] = array.splice(b, 1, array[a])[0];
}

/**
  * Tokenizes CSS Filter value and returns an array of {name, value} pairs
  *
  * This is only a very simple tokenizer that only works its way through
  * parenthesis in the string to detect function names and values.
  * It assumes that the string actually is a well-formed filter value
  * (e.g. "blur(2px) hue-rotate(100deg)").
  *
  * @param {String} css
  *        CSS Filter value to be parsed
  * @return {Array}
  *        An array of {name, value} pairs
  */
function tokenizeComputedFilter(css) {
  let filters = [];
  let current = "";
  let depth = 0;

  if (css === "none") {
    return filters;
  }

  while (css.length) {
    const char = css[0];

    switch (char) {
      case "(":
        depth++;
        if (depth === 1) {
          filters.push({name: current.trim()});
          current = "";
        } else {
          current += char;
        }
      break;
      case ")":
        depth--;
        if (depth === 0) {
          filters[filters.length - 1].value = current.trim();
          current = "";
        } else {
          current += char;
        }
      break;
      default:
        current += char;
      break;
    }

    css = css.slice(1);
  }

  return filters;
}
