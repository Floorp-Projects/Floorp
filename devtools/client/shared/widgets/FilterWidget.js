/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
  * This is a CSS Filter Editor widget used
  * for Rule View's filter swatches
  */

const EventEmitter = require("devtools/shared/event-emitter");
const { Cu, Cc, Ci } = require("chrome");
const { ViewHelpers } =
      Cu.import("resource://devtools/client/shared/widgets/ViewHelpers.jsm",
                {});
const STRINGS_URI = "chrome://devtools/locale/filterwidget.properties";
const L10N = new ViewHelpers.L10N(STRINGS_URI);
const {cssTokenizer} = require("devtools/client/shared/css-parsing-utils");

loader.lazyGetter(this, "asyncStorage",
                  () => require("devtools/shared/async-storage"));

loader.lazyGetter(this, "DOMUtils", () => {
  return Cc["@mozilla.org/inspector/dom-utils;1"].getService(Ci.inIDOMUtils);
});

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
    "range": [0, Infinity],
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

// Valid values that shouldn't be parsed for filters.
const SPECIAL_VALUES = new Set(["none", "unset", "initial", "inherit"]);

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
  this._keyDown = this._keyDown.bind(this);
  this._input = this._input.bind(this);
  this._presetClick = this._presetClick.bind(this);
  this._savePreset = this._savePreset.bind(this);
  this._togglePresets = this._togglePresets.bind(this);

  // Passed to asyncStorage, requires binding
  this.renderPresets = this.renderPresets.bind(this);

  this._initMarkup();
  this._buildFilterItemMarkup();
  this._buildPresetItemMarkup();
  this._addEventListeners();

  EventEmitter.decorate(this);

  this.filters = [];
  this.setCssValue(value);
  this.renderPresets();
}

exports.CSSFilterEditorWidget = CSSFilterEditorWidget;

CSSFilterEditorWidget.prototype = {
  _initMarkup: function() {
    this.filtersList = this.el.querySelector("#filters");
    this.presetsList = this.el.querySelector("#presets");
    this.togglePresets = this.el.querySelector("#toggle-presets");
    this.filterSelect = this.el.querySelector("select");
    this.addPresetButton = this.el.querySelector(".presets-list .add");
    this.addPresetInput = this.el.querySelector(".presets-list .footer input");

    this.el.querySelector(".presets-list input").value = "";

    this._populateFilterSelect();
  },

  _destroyMarkup: function() {
    this._filterItemMarkup.remove();
    this.el.remove();
    this.el = this.filtersList = this._filterItemMarkup = null;
    this.presetsList = this.togglePresets = this.filterSelect = null;
    this.addPresetButton = null;
  },

  destroy: function() {
    this._removeEventListeners();
    this._destroyMarkup();
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

    base.appendChild(name);
    base.appendChild(value);
    base.appendChild(removeButton);

    this._filterItemMarkup = base;
  },

  _buildPresetItemMarkup: function() {
    let base = this.doc.createElement("div");
    base.classList.add("preset");

    let name = this.doc.createElement("label");
    base.appendChild(name);

    let value = this.doc.createElement("span");
    base.appendChild(value);

    let removeButton = this.doc.createElement("button");
    removeButton.classList.add("remove-button");

    base.appendChild(removeButton);

    this._presetItemMarkup = base;
  },

  _addEventListeners: function() {
    this.addButton = this.el.querySelector("#add-filter");
    this.addButton.addEventListener("click", this._addButtonClick);
    this.filtersList.addEventListener("click", this._removeButtonClick);
    this.filtersList.addEventListener("mousedown", this._mouseDown);
    this.filtersList.addEventListener("keydown", this._keyDown);

    this.presetsList.addEventListener("click", this._presetClick);
    this.togglePresets.addEventListener("click", this._togglePresets);
    this.addPresetButton.addEventListener("click", this._savePreset);

    // These events are event delegators for
    // drag-drop re-ordering and label-dragging
    this.win.addEventListener("mousemove", this._mouseMove);
    this.win.addEventListener("mouseup", this._mouseUp);

    // Used to workaround float-precision problems
    this.filtersList.addEventListener("input", this._input);
  },

  _removeEventListeners: function() {
    this.addButton.removeEventListener("click", this._addButtonClick);
    this.filtersList.removeEventListener("click", this._removeButtonClick);
    this.filtersList.removeEventListener("mousedown", this._mouseDown);
    this.filtersList.removeEventListener("keydown", this._keyDown);

    this.presetsList.removeEventListener("click", this._presetClick);
    this.togglePresets.removeEventListener("click", this._togglePresets);
    this.addPresetButton.removeEventListener("click", this._savePreset);

    // These events are used for drag drop re-ordering
    this.win.removeEventListener("mousemove", this._mouseMove);
    this.win.removeEventListener("mouseup", this._mouseUp);

    // Used to workaround float-precision problems
    this.filtersList.removeEventListener("input", this._input);
  },

  _getFilterElementIndex: function(el) {
    return [...this.filtersList.children].indexOf(el);
  },

  _keyDown: function(e) {
    if (e.target.tagName.toLowerCase() !== "input" ||
       (e.keyCode !== 40 && e.keyCode !== 38)) {
      return;
    }
    let input = e.target;

    const direction = e.keyCode === 40 ? -1 : 1;

    let multiplier = DEFAULT_VALUE_MULTIPLIER;
    if (e.altKey) {
      multiplier = SLOW_VALUE_MULTIPLIER;
    } else if (e.shiftKey) {
      multiplier = FAST_VALUE_MULTIPLIER;
    }

    const filterEl = e.target.closest(".filter");
    const index = this._getFilterElementIndex(filterEl);
    const filter = this.filters[index];

    // Filters that have units are number-type filters. For them,
    // the value can be incremented/decremented simply.
    // For other types of filters (e.g. drop-shadow) we need to check
    // if the keypress happened close to a number first.
    if (filter.unit) {
      let startValue = parseFloat(e.target.value);
      let value = startValue + direction * multiplier;

      const [min, max] = this._definition(filter.name).range;
      if (value < min) {
        value = min;
      } else if (value > max) {
        value = max;
      }

      input.value = fixFloat(value);

      this.updateValueAt(index, value);
    } else {
      let selectionStart = input.selectionStart;
      let num = getNeighbourNumber(input.value, selectionStart);
      if (!num) {
        return;
      }

      let {start, end, value} = num;

      let split = input.value.split("");
      let computed = fixFloat(value + direction * multiplier);
      let dotIndex = computed.indexOf(".0");
      if (dotIndex > -1) {
        computed = computed.slice(0, -2);

        selectionStart = selectionStart > start + dotIndex ?
                                          start + dotIndex :
                                          selectionStart;
      }
      split.splice(start, end - start, computed);

      value = split.join("");
      input.value = value;
      this.updateValueAt(index, value);
      input.setSelectionRange(selectionStart, selectionStart);
    }
    e.preventDefault();
  },

  _input: function(e) {
    let filterEl = e.target.closest(".filter");
    let index = this._getFilterElementIndex(filterEl);
    let filter = this.filters[index];
    let def = this._definition(filter.name);

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

      this.el.classList.add("dragging");
    // label-dragging
    } else if (e.target.classList.contains("devtools-draglabel")) {
      let label = e.target;
      let input = filterEl.querySelector("input");
      let index = this._getFilterElementIndex(filterEl);

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
    this.add(key, null);

    this.render();
  },

  _removeButtonClick: function(e) {
    const isRemoveButton = e.target.classList.contains("remove-button");
    if (!isRemoveButton) {
      return;
    }

    let filterEl = e.target.closest(".filter");
    let index = this._getFilterElementIndex(filterEl);
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
    const rect = this.filtersList.getBoundingClientRect();
    let top = e.pageY - LIST_PADDING;
    let bottom = e.pageY + LIST_PADDING;
    // don't allow dragging over top/bottom of list
    if (top < rect.top || bottom > rect.bottom) {
      return;
    }

    const filterEl = this.filtersList.querySelector(".dragging");

    const delta = e.pageY - filterEl.startingY;
    filterEl.style.top = delta + "px";

    // change is the number of _steps_ taken from initial position
    // i.e. how many elements we have passed
    let change = delta / LIST_ITEM_HEIGHT;
    if (change > 0) {
      change = Math.floor(change);
    } else if (change < 0) {
      change = Math.ceil(change);
    }

    const children = this.filtersList.children;
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
      this.filtersList.insertBefore(filterEl, target);
    } else {
      this.filtersList.appendChild(filterEl);
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
    if (value < min) {
      value = min;
    } else if (value > max) {
      value = max;
    }

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
    let filterEl = this.filtersList.querySelector(".dragging");

    this.isReorderingFilter = false;
    filterEl.classList.remove("dragging");
    this.el.classList.remove("dragging");
    filterEl.removeAttribute("style");

    this.emit("updated", this.getCssValue());
    this.render();
  },

  _presetClick: function(e) {
    let el = e.target;
    let preset = el.closest(".preset");
    if (!preset) {
      return;
    }

    let id = +preset.dataset.id;

    this.getPresets().then(presets => {
      if (el.classList.contains("remove-button")) {
        // If the click happened on the remove button.
        presets.splice(id, 1);
        this.setPresets(presets).then(this.renderPresets, Cu.reportError);
      } else {
        // Or if the click happened on a preset.
        let p = presets[id];

        this.setCssValue(p.value);
        this.addPresetInput.value = p.name;
      }
    }, Cu.reportError);
  },

  _togglePresets: function() {
    this.el.classList.toggle("show-presets");
    this.emit("render");
  },

  _savePreset: function(e) {
    e.preventDefault();

    let name = this.addPresetInput.value;
    let value = this.getCssValue();

    if (!name || !value || SPECIAL_VALUES.has(value)) {
      this.emit("preset-save-error");
      return;
    }

    this.getPresets().then(presets => {
      let index = presets.findIndex(preset => preset.name === name);

      if (index > -1) {
        presets[index].value = value;
      } else {
        presets.push({name, value});
      }

      this.setPresets(presets).then(this.renderPresets, Cu.reportError);
    }, Cu.reportError);
  },

  /**
   * Clears the list and renders filters, binding required events.
   * There are some delegated events bound in _addEventListeners method
   */
  render: function() {
    if (!this.filters.length) {
      this.filtersList.innerHTML = `<p> ${L10N.getStr("emptyFilterList")} <br />
                                 ${L10N.getStr("addUsingList")} </p>`;
      this.emit("render");
      return;
    }

    this.filtersList.innerHTML = "";

    let base = this._filterItemMarkup;

    for (let filter of this.filters) {
      const def = this._definition(filter.name);

      let el = base.cloneNode(true);

      let [name, value] = el.children;
      let label = name.children[1];
      let [input, unitPreview] = value.children;

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

      this.filtersList.appendChild(el);
    }

    let lastInput =
        this.filtersList.querySelector(`.filter:last-of-type input`);
    if (lastInput) {
      lastInput.focus();
      // move cursor to end of input
      const end = lastInput.value.length;
      lastInput.setSelectionRange(end, end);
    }

    this.emit("render");
  },

  renderPresets: function() {
    this.getPresets().then(presets => {
      if (!presets || !presets.length) {
        this.presetsList.innerHTML = `<p>${L10N.getStr("emptyPresetList")}</p>`;
        this.emit("render");
        return;
      }
      let base = this._presetItemMarkup;

      this.presetsList.innerHTML = "";

      for (let [index, preset] of presets.entries()) {
        let el = base.cloneNode(true);

        let [label, span] = el.children;

        el.dataset.id = index;

        label.textContent = preset.name;
        span.textContent = preset.value;

        this.presetsList.appendChild(el);
      }

      this.emit("render");
    });
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
    name = name.toLowerCase();
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

    if (SPECIAL_VALUES.has(cssValue)) {
      this._specialValue = cssValue;
      this.emit("updated", this.getCssValue());
      this.render();
      return;
    }

    for (let {name, value, quote} of tokenizeFilterValue(cssValue)) {
      // If the specified value is invalid, replace it with the
      // default.
      if (name !== "url") {
        if (!DOMUtils.cssPropertyIsValid("filter", name + "(" + value + ")")) {
          value = null;
        }
      }

      this.add(name, value, quote);
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
    *        If this is |null|, then a default value may be supplied.
    * @param {String} quote
    *        For a url filter, the quoting style.  This can be a
    *        single quote, a double quote, or empty.
    * @return {Number}
    *        The index of the new filter in the current list of filters
    */
  add: function(name, value, quote) {
    const def = this._definition(name);
    if (!def) {
      return false;
    }

    if (value === null) {
      // UNIT_MAPPING[string] is an empty string (falsy), so
      // using || doesn't work here
      const unitLabel = typeof UNIT_MAPPING[def.type] === "undefined" ?
                               UNIT_MAPPING[DEFAULT_FILTER_TYPE] :
                               UNIT_MAPPING[def.type];

      // string-type filters have no default value but a placeholder instead
      if (!unitLabel) {
        value = "";
      } else {
        value = def.range[0] + unitLabel;
      }

      if (name === "url") {
        // Default quote.
        quote = "\"";
      }
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

    const index = this.filters.push({value, unit, name, quote}) - 1;
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

    // Just return the value+unit for non-url functions.
    if (filter.name !== "url") {
      return filter.value + filter.unit;
    }

    // url values need to be quoted and escaped.
    if (filter.quote === "'") {
      return "'" + filter.value.replace(/\'/g, "\\'") + "'";
    } else if (filter.quote === "\"") {
      return "\"" + filter.value.replace(/\"/g, "\\\"") + "\"";
    }

    // Unquoted.  This approach might change the original input -- for
    // example the original might be over-quoted.  But, this is
    // correct and probably good enough.
    return filter.value.replace(/[ \t(){};]/g, "\\$&");
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
    }).join(" ") || this._specialValue || "none";
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

  getPresets: function() {
    return asyncStorage.getItem("cssFilterPresets").then(presets => {
      if (!presets) {
        return [];
      }

      return presets;
    }, Cu.reportError);
  },

  setPresets: function(presets) {
    return asyncStorage.setItem("cssFilterPresets", presets)
                       .catch(Cu.reportError);
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
 * Tokenizes a CSS Filter value and returns an array of {name, value} pairs.
 *
 * @param {String} css CSS Filter value to be parsed
 * @return {Array} An array of {name, value} pairs
 */
function tokenizeFilterValue(css) {
  let filters = [];
  let depth = 0;

  if (SPECIAL_VALUES.has(css)) {
    return filters;
  }

  let state = "initial";
  let name;
  let contents;
  for (let token of cssTokenizer(css)) {
    switch (state) {
      case "initial":
        if (token.tokenType === "function") {
          name = token.text;
          contents = "";
          state = "function";
          depth = 1;
        } else if (token.tokenType === "url" || token.tokenType === "bad_url") {
          // Extract the quoting style from the url.
          let originalText = css.substring(token.startOffset, token.endOffset);
          let [, quote] = /^url\([ \t\r\n\f]*(["']?)/i.exec(originalText);

          filters.push({name: "url", value: token.text.trim(), quote: quote});
          // Leave state as "initial" because the URL token includes
          // the trailing close paren.
        }
        break;

      case "function":
        if (token.tokenType === "symbol" && token.text === ")") {
          --depth;
          if (depth === 0) {
            filters.push({name: name, value: contents.trim()});
            state = "initial";
            break;
          }
        }
        contents += css.substring(token.startOffset, token.endOffset);
        if (token.tokenType === "function" ||
            (token.tokenType === "symbol" && token.text === "(")) {
          ++depth;
        }
        break;
    }
  }

  return filters;
}

/**
  * Finds neighbour number characters of an index in a string
  * the numbers may be floats (containing dots)
  * It's assumed that the value given to this function is a valid number
  *
  * @param {String} string
  *        The string containing numbers
  * @param {Number} index
  *        The index to look for neighbours for
  * @return {Object}
  *         returns null if no number is found
  *         value: The number found
  *         start: The number's starting index
  *         end: The number's ending index
  */
function getNeighbourNumber(string, index) {
  if (!/\d/.test(string)) {
    return null;
  }

  let left = /-?[0-9.]*$/.exec(string.slice(0, index));
  let right = /-?[0-9.]*/.exec(string.slice(index));

  left = left ? left[0] : "";
  right = right ? right[0] : "";

  if (!right && !left) {
    return null;
  }

  return {
    value: fixFloat(left + right, true),
    start: index - left.length,
    end: index + right.length
  };
}
