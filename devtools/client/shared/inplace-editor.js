/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Basic use:
 * let spanToEdit = document.getElementById("somespan");
 *
 * editableField({
 *   element: spanToEdit,
 *   done: function(value, commit, direction, key) {
 *     if (commit) {
 *       spanToEdit.textContent = value;
 *     }
 *   },
 *   trigger: "dblclick"
 * });
 *
 * See editableField() for more options.
 */

"use strict";

const focusManager = Services.focus;
const isOSX = Services.appinfo.OS === "Darwin";
const { KeyCodes } = require("resource://devtools/client/shared/keycodes.js");
const EventEmitter = require("resource://devtools/shared/event-emitter.js");
const {
  findMostRelevantCssPropertyIndex,
} = require("resource://devtools/client/shared/suggestion-picker.js");

const HTML_NS = "http://www.w3.org/1999/xhtml";
const CONTENT_TYPES = {
  PLAIN_TEXT: 0,
  CSS_VALUE: 1,
  CSS_MIXED: 2,
  CSS_PROPERTY: 3,
};

// The limit of 500 autocomplete suggestions should not be reached but is kept
// for safety.
const MAX_POPUP_ENTRIES = 500;

const FOCUS_FORWARD = focusManager.MOVEFOCUS_FORWARD;
const FOCUS_BACKWARD = focusManager.MOVEFOCUS_BACKWARD;

const WORD_REGEXP = /\w/;
const isWordChar = function (str) {
  return str && WORD_REGEXP.test(str);
};

const GRID_PROPERTY_NAMES = [
  "grid-area",
  "grid-row",
  "grid-row-start",
  "grid-row-end",
  "grid-column",
  "grid-column-start",
  "grid-column-end",
];
const GRID_ROW_PROPERTY_NAMES = [
  "grid-area",
  "grid-row",
  "grid-row-start",
  "grid-row-end",
];
const GRID_COL_PROPERTY_NAMES = [
  "grid-area",
  "grid-column",
  "grid-column-start",
  "grid-column-end",
];

/**
 * Helper to check if the provided key matches one of the expected keys.
 * Keys will be prefixed with DOM_VK_ and should match a key in KeyCodes.
 *
 * @param {String} key
 *        the key to check (can be a keyCode).
 * @param {...String} keys
 *        list of possible keys allowed.
 * @return {Boolean} true if the key matches one of the keys.
 */
function isKeyIn(key, ...keys) {
  return keys.some(expectedKey => {
    return key === KeyCodes["DOM_VK_" + expectedKey];
  });
}

/**
 * Mark a span editable.  |editableField| will listen for the span to
 * be focused and create an InlineEditor to handle text input.
 * Changes will be committed when the InlineEditor's input is blurred
 * or dropped when the user presses escape.
 *
 * @param {Object} options: Options for the editable field
 * @param {Element} options.element:
 *        (required) The span to be edited on focus.
 * @param {Function} options.canEdit:
 *        Will be called before creating the inplace editor.  Editor
 *        won't be created if canEdit returns false.
 * @param {Function} options.start:
 *        Will be called when the inplace editor is initialized.
 * @param {Function} options.change:
 *        Will be called when the text input changes.  Will be called
 *        with the current value of the text input.
 * @param {Function} options.done:
 *        Called when input is committed or blurred.  Called with
 *        current value, a boolean telling the caller whether to
 *        commit the change, the direction of the next element to be
 *        selected and the event keybode. Direction may be one of Services.focus.MOVEFOCUS_FORWARD,
 *        Services.focus.MOVEFOCUS_BACKWARD, or null (no movement).
 *        This function is called before the editor has been torn down.
 * @param {Function} options.destroy:
 *        Called when the editor is destroyed and has been torn down.
 * @param {Function} options.contextMenu:
 *        Called when the user triggers a contextmenu event on the input.
 * @param {Object} options.advanceChars:
 *        This can be either a string or a function.
 *        If it is a string, then if any characters in it are typed,
 *        focus will advance to the next element.
 *        Otherwise, if it is a function, then the function will
 *        be called with three arguments: a key code, the current text,
 *        and the insertion point.  If the function returns true,
 *        then the focus advance takes place.  If it returns false,
 *        then the character is inserted instead.
 * @param {Boolean} options.stopOnReturn:
 *        If true, the return key will not advance the editor to the next
 *        focusable element. Note that Ctrl/Cmd+Enter will still advance the editor
 * @param {Boolean} options.stopOnTab:
 *        If true, the tab key will not advance the editor to the next
 *        focusable element.
 * @param {Boolean} options.stopOnShiftTab:
 *        If true, shift tab will not advance the editor to the previous
 *        focusable element.
 * @param {String} options.trigger: The DOM event that should trigger editing,
 *        defaults to "click"
 * @param {Boolean} options.multiline: Should the editor be a multiline textarea?
 *        defaults to false
 * @param {Function or options.Number} maxWidth:
 *        Should the editor wrap to remain below the provided max width. Only
 *        available if multiline is true. If a function is provided, it will be
 *        called when replacing the element by the inplace input.
 * @param {Boolean} options.trimOutput: Should the returned string be trimmed?
 *        defaults to true
 * @param {Boolean} options.preserveTextStyles: If true, do not copy text-related styles
 *        from `element` to the new input.
 *        defaults to false
 * @param {Object} options.cssProperties: An instance of CSSProperties.
 * @param {Object} options.cssVariables: A Map object containing all CSS variables.
 * @param {Number} options.defaultIncrement: The value by which the input is incremented
 *        or decremented by default (0.1 for properties like opacity and 1 by default)
 * @param {Function} options.getGridLineNames:
 *        Will be called before offering autocomplete sugestions, if the property is
 *        a member of GRID_PROPERTY_NAMES.
 * @param {Boolean} options.showSuggestCompletionOnEmpty:
 *        If true, show the suggestions in case that the current text becomes empty.
 *        Defaults to false.
 * @param {Boolean} options.focusEditableFieldAfterApply
 *        If true, try to focus the next editable field after the input value is commited.
 *        When set to true, focusEditableFieldContainerSelector is mandatory.
 *        If no editable field can be found within the element retrieved with
 *        focusEditableFieldContainerSelector, the focus will be moved to the next focusable
 *        element (which won't be an editable field)
 * @param {String} options.focusEditableFieldContainerSelector
 *        A CSS selector that will be used to retrieve the container element into which
 *        the next focused element should be in, when focusEditableFieldAfterApply
 *        is set to true. This allows to bail out if we can't find a suitable
 *        focusable field.
 * @param {String} options.inputAriaLabel
 *        Optional aria-label attribute value that will be added to the input.
 * @param {String} options.inputAriaLabelledBy
 *        Optional aria-labelled-by attribute value that will be added to the input.
 */
function editableField(options) {
  return editableItem(options, function (element, event) {
    if (!options.element.inplaceEditor) {
      new InplaceEditor(options, event);
    }
  });
}

exports.editableField = editableField;

/**
 * Handle events for an element that should respond to
 * clicks and sit in the editing tab order, and call
 * a callback when it is activated.
 *
 * @param {Object} options
 *    The options for this editor, including:
 *    {Element} element: The DOM element.
 *    {String} trigger: The DOM event that should trigger editing,
 *      defaults to "click"
 * @param {Function} callback
 *        Called when the editor is activated.
 * @return {Function} function which calls callback
 */
function editableItem(options, callback) {
  const trigger = options.trigger || "click";
  const element = options.element;
  element.addEventListener(trigger, function (evt) {
    if (evt.target.nodeName !== "a") {
      const win = this.ownerDocument.defaultView;
      const selection = win.getSelection();
      if (trigger != "click" || selection.isCollapsed) {
        callback(element, evt);
      }
      evt.stopPropagation();
    }
  });

  // If focused by means other than a click, start editing by
  // pressing enter or space.
  element.addEventListener(
    "keypress",
    function (evt) {
      if (evt.target.nodeName === "button") {
        return;
      }

      if (isKeyIn(evt.keyCode, "RETURN") || isKeyIn(evt.charCode, "SPACE")) {
        callback(element);
      }
    },
    true
  );

  // Ugly workaround - the element is focused on mousedown but
  // the editor is activated on click/mouseup.  This leads
  // to an ugly flash of the focus ring before showing the editor.
  // So hide the focus ring while the mouse is down.
  element.addEventListener("mousedown", function (evt) {
    if (evt.target.nodeName !== "a") {
      const cleanup = function () {
        element.style.removeProperty("outline-style");
        element.removeEventListener("mouseup", cleanup);
        element.removeEventListener("mouseout", cleanup);
      };
      element.style.setProperty("outline-style", "none");
      element.addEventListener("mouseup", cleanup);
      element.addEventListener("mouseout", cleanup);
    }
  });

  // Mark the element editable field for tab
  // navigation while editing.
  element._editable = true;

  // Save the trigger type so we can dispatch this later
  element._trigger = trigger;

  // Add button semantics to the element, to indicate that it can be activated.
  element.setAttribute("role", "button");

  return function turnOnEditMode() {
    callback(element);
  };
}

exports.editableItem = editableItem;

/*
 * Various API consumers (especially tests) sometimes want to grab the
 * inplaceEditor expando off span elements. However, when each global has its
 * own compartment, those expandos live on Xray wrappers that are only visible
 * within this JSM. So we provide a little workaround here.
 */

function getInplaceEditorForSpan(span) {
  return span.inplaceEditor;
}

exports.getInplaceEditorForSpan = getInplaceEditorForSpan;

class InplaceEditor extends EventEmitter {
  constructor(options, event) {
    super();

    this.elt = options.element;
    const doc = this.elt.ownerDocument;
    this.doc = doc;
    this.elt.inplaceEditor = this;
    this.cssProperties = options.cssProperties;
    this.cssVariables = options.cssVariables || new Map();
    this.change = options.change;
    this.done = options.done;
    this.contextMenu = options.contextMenu;
    this.defaultIncrement = options.defaultIncrement || 1;
    this.destroy = options.destroy;
    this.initial = options.initial ? options.initial : this.elt.textContent;
    this.multiline = options.multiline || false;
    this.maxWidth = options.maxWidth;
    if (typeof this.maxWidth == "function") {
      this.maxWidth = this.maxWidth();
    }

    this.trimOutput =
      options.trimOutput === undefined ? true : !!options.trimOutput;
    this.stopOnShiftTab = !!options.stopOnShiftTab;
    this.stopOnTab = !!options.stopOnTab;
    this.stopOnReturn = !!options.stopOnReturn;
    this.contentType = options.contentType || CONTENT_TYPES.PLAIN_TEXT;
    this.property = options.property;
    this.popup = options.popup;
    this.preserveTextStyles =
      options.preserveTextStyles === undefined
        ? false
        : !!options.preserveTextStyles;
    this.showSuggestCompletionOnEmpty = !!options.showSuggestCompletionOnEmpty;
    this.focusEditableFieldAfterApply =
      options.focusEditableFieldAfterApply === true;
    this.focusEditableFieldContainerSelector =
      options.focusEditableFieldContainerSelector;

    if (
      this.focusEditableFieldAfterApply &&
      !this.focusEditableFieldContainerSelector
    ) {
      throw new Error(
        "focusEditableFieldContainerSelector is mandatory when focusEditableFieldAfterApply is true"
      );
    }

    this.#createInput(options);

    // Hide the provided element and add our editor.
    this.originalDisplay = this.elt.style.display;
    this.elt.style.display = "none";
    this.elt.parentNode.insertBefore(this.input, this.elt);

    // After inserting the input to have all CSS styles applied, start autosizing.
    this.#autosize();

    this.inputCharDimensions = this.#getInputCharDimensions();
    // Pull out character codes for advanceChars, listing the
    // characters that should trigger a blur.
    if (typeof options.advanceChars === "function") {
      this.#advanceChars = options.advanceChars;
    } else {
      const advanceCharcodes = {};
      const advanceChars = options.advanceChars || "";
      for (let i = 0; i < advanceChars.length; i++) {
        advanceCharcodes[advanceChars.charCodeAt(i)] = true;
      }
      this.#advanceChars = charCode => charCode in advanceCharcodes;
    }

    this.input.focus();

    if (typeof options.selectAll == "undefined" || options.selectAll) {
      this.input.select();
    }

    const win = doc.defaultView;
    this.#abortController = new win.AbortController();
    const eventListenerConfig = { signal: this.#abortController.signal };

    this.input.addEventListener("blur", this.#onBlur, eventListenerConfig);
    this.input.addEventListener(
      "keypress",
      this.#onKeyPress,
      eventListenerConfig
    );
    this.input.addEventListener("input", this.#onInput, eventListenerConfig);
    this.input.addEventListener(
      "dblclick",
      this.#stopEventPropagation,
      eventListenerConfig
    );
    this.input.addEventListener(
      "click",
      this.#stopEventPropagation,
      eventListenerConfig
    );
    this.input.addEventListener(
      "mousedown",
      this.#stopEventPropagation,
      eventListenerConfig
    );
    this.input.addEventListener(
      "contextmenu",
      this.#onContextMenu,
      eventListenerConfig
    );
    win.addEventListener("blur", this.#onWindowBlur, eventListenerConfig);

    this.validate = options.validate;

    if (this.validate) {
      this.input.addEventListener("keyup", this.#onKeyup, eventListenerConfig);
    }

    this.#updateSize();

    if (options.start) {
      options.start(this, event);
    }

    this.#getGridNamesBeforeCompletion(options.getGridLineNames);
  }
  static CONTENT_TYPES = CONTENT_TYPES;

  #abortController;
  #advanceChars;
  #applied;
  #measurement;
  #openPopupTimeout;
  #pressedKey;
  #preventSuggestions;
  #selectedIndex;

  get currentInputValue() {
    const val = this.trimOutput ? this.input.value.trim() : this.input.value;
    return val;
  }

  /**
   * Create the input element.
   *
   * @param {Object} options
   * @param {String} options.inputAriaLabel
   *        Optional aria-label attribute value that will be added to the input.
   * @param {String} options.inputAriaLabelledBy
   *        Optional aria-labelledby attribute value that will be added to the input.
   */
  #createInput(options = {}) {
    this.input = this.doc.createElementNS(
      HTML_NS,
      this.multiline ? "textarea" : "input"
    );
    this.input.inplaceEditor = this;

    if (this.multiline) {
      // Hide the textarea resize handle.
      this.input.style.resize = "none";
      this.input.style.overflow = "hidden";
      // Also reset padding.
      this.input.style.padding = "0";
    }

    this.input.classList.add("styleinspector-propertyeditor");
    this.input.value = this.initial;
    if (options.inputAriaLabel) {
      this.input.setAttribute("aria-label", options.inputAriaLabel);
    } else if (options.inputAriaLabelledBy) {
      this.input.setAttribute("aria-labelledby", options.inputAriaLabelledBy);
    }

    if (!this.preserveTextStyles) {
      copyTextStyles(this.elt, this.input);
    }
  }

  /**
   * Get rid of the editor.
   */
  #clear() {
    if (!this.input) {
      // Already cleared.
      return;
    }

    this.#abortController.abort();
    this.#stopAutosize();

    this.elt.style.display = this.originalDisplay;

    if (this.doc.activeElement == this.input) {
      this.elt.focus();
    }

    this.input.remove();
    this.input = null;

    delete this.elt.inplaceEditor;
    delete this.elt;

    if (this.destroy) {
      this.destroy();
    }
  }

  /**
   * Keeps the editor close to the size of its input string.  This is pretty
   * crappy, suggestions for improvement welcome.
   */
  #autosize() {
    // Create a hidden, absolutely-positioned span to measure the text
    // in the input.  Boo.

    // We can't just measure the original element because a) we don't
    // change the underlying element's text ourselves (we leave that
    // up to the client), and b) without tweaking the style of the
    // original element, it might wrap differently or something.
    this.#measurement = this.doc.createElementNS(
      HTML_NS,
      this.multiline ? "pre" : "span"
    );
    this.#measurement.className = "autosizer";
    this.elt.parentNode.appendChild(this.#measurement);
    const style = this.#measurement.style;
    style.visibility = "hidden";
    style.position = "absolute";
    style.top = "0";
    style.left = "0";

    if (this.multiline) {
      style.whiteSpace = "pre-wrap";
      style.wordWrap = "break-word";
      if (this.maxWidth) {
        style.maxWidth = this.maxWidth + "px";
        // Use position fixed to measure dimensions without any influence from
        // the container of the editor.
        style.position = "fixed";
      }
    }

    copyAllStyles(this.input, this.#measurement);
    this.#updateSize();
  }

  /**
   * Clean up the mess created by _autosize().
   */
  #stopAutosize() {
    if (!this.#measurement) {
      return;
    }
    this.#measurement.remove();
    this.#measurement = null;
  }

  /**
   * Size the editor to fit its current contents.
   */
  #updateSize() {
    // Replace spaces with non-breaking spaces.  Otherwise setting
    // the span's textContent will collapse spaces and the measurement
    // will be wrong.
    let content = this.input.value;
    const unbreakableSpace = "\u00a0";

    // Make sure the content is not empty.
    if (content === "") {
      content = unbreakableSpace;
    }

    // If content ends with a new line, add a blank space to force the autosize
    // element to adapt its height.
    if (content.lastIndexOf("\n") === content.length - 1) {
      content = content + unbreakableSpace;
    }

    if (!this.multiline) {
      content = content.replace(/ /g, unbreakableSpace);
    }

    this.#measurement.textContent = content;

    // Do not use offsetWidth: it will round floating width values.
    let width = this.#measurement.getBoundingClientRect().width;
    if (this.multiline) {
      if (this.maxWidth) {
        width = Math.min(this.maxWidth, width);
      }
      const height = this.#measurement.getBoundingClientRect().height;
      this.input.style.height = height + "px";
    }
    this.input.style.width = width + "px";
  }

  /**
   * Get the width and height of a single character in the input to properly
   * position the autocompletion popup.
   */
  #getInputCharDimensions() {
    // Just make the text content to be 'x' to get the width and height of any
    // character in a monospace font.
    this.#measurement.textContent = "x";
    const width = this.#measurement.clientWidth;
    const height = this.#measurement.clientHeight;
    return { width, height };
  }

  /**
   * Increment property values in rule view.
   *
   * @param {Number} increment
   *        The amount to increase/decrease the property value.
   * @return {Boolean} true if value has been incremented.
   */
  #incrementValue(increment) {
    const value = this.input.value;
    const selectionStart = this.input.selectionStart;
    const selectionEnd = this.input.selectionEnd;

    const newValue = this.#incrementCSSValue(
      value,
      increment,
      selectionStart,
      selectionEnd
    );

    if (!newValue) {
      return false;
    }

    this.input.value = newValue.value;
    this.input.setSelectionRange(newValue.start, newValue.end);
    this.#doValidation();

    // Call the user's change handler if available.
    if (this.change) {
      this.change(this.currentInputValue);
    }

    return true;
  }

  /**
   * Increment the property value based on the property type.
   *
   * @param {String} value
   *        Property value.
   * @param {Number} increment
   *        Amount to increase/decrease the property value.
   * @param {Number} selStart
   *        Starting index of the value.
   * @param {Number} selEnd
   *        Ending index of the value.
   * @return {Object} object with properties 'value', 'start', and 'end'.
   */
  #incrementCSSValue(value, increment, selStart, selEnd) {
    const range = this.#parseCSSValue(value, selStart);
    const type = range?.type || "";
    const rawValue = range ? value.substring(range.start, range.end) : "";
    const preRawValue = range ? value.substr(0, range.start) : "";
    const postRawValue = range ? value.substr(range.end) : "";
    let info;

    let incrementedValue = null,
      selection;
    if (type === "num") {
      if (rawValue == "0") {
        info = {};
        info.units = this.#findCompatibleUnit(preRawValue, postRawValue);
      }

      const newValue = this.#incrementRawValue(rawValue, increment, info);
      if (newValue !== null) {
        incrementedValue = newValue;
        selection = [0, incrementedValue.length];
      }
    } else if (type === "hex") {
      const exprOffset = selStart - range.start;
      const exprOffsetEnd = selEnd - range.start;
      const newValue = this.#incHexColor(
        rawValue,
        increment,
        exprOffset,
        exprOffsetEnd
      );
      if (newValue) {
        incrementedValue = newValue.value;
        selection = newValue.selection;
      }
    } else {
      if (type === "rgb" || type === "hsl" || type === "hwb") {
        info = {};
        const isCSS4Color = !value.includes(",");
        // In case the value uses the new syntax of the CSS Color 4 specification,
        // it is split by the spaces and the slash separating the alpha value
        // between the different color components.
        // Example: rgb(255 0 0 / 0.5)
        // Otherwise, the value is represented using the old color syntax and is
        // split by the commas between the color components.
        // Example: rgba(255, 0, 0, 0.5)
        const part =
          value
            .substring(range.start, selStart)
            .split(isCSS4Color ? / ?\/ ?| / : ",").length - 1;
        if (part === 3) {
          // alpha
          info.minValue = 0;
          info.maxValue = 1;
        } else if (type === "rgb") {
          info.minValue = 0;
          info.maxValue = 255;
        } else if (part !== 0) {
          // hsl or hwb percentage
          info.minValue = 0;
          info.maxValue = 100;

          // select the previous number if the selection is at the end of a
          // percentage sign.
          if (value.charAt(selStart - 1) === "%") {
            --selStart;
          }
        }
      }
      return this.#incrementGenericValue(
        value,
        increment,
        selStart,
        selEnd,
        info
      );
    }

    if (incrementedValue === null) {
      return null;
    }

    return {
      value: preRawValue + incrementedValue + postRawValue,
      start: range.start + selection[0],
      end: range.start + selection[1],
    };
  }

  /**
   * Find a compatible unit to use for a CSS number value inserted between the
   * provided beforeValue and afterValue. The compatible unit will be picked
   * from a selection of default units corresponding to supported CSS value
   * dimensions (distance, angle, duration).
   *
   * @param {String} beforeValue
   *        The string preceeding the number value in the current property
   *        value.
   * @param {String} afterValue
   *        The string following the number value in the current property value.
   * @return {String} a valid unit that can be used for this number value or
   *         empty string if no match could be found.
   */
  #findCompatibleUnit(beforeValue, afterValue) {
    if (!this.property || !this.property.name) {
      return "";
    }

    // A DOM element is used to test the validity of various units. This is to
    // avoid having to do an async call to the server to get this information.
    const el = this.doc.createElement("div");

    // Cycle through unitless (""), pixels, degrees and seconds.
    const units = ["", "px", "deg", "s"];
    for (const unit of units) {
      const value = beforeValue + "1" + unit + afterValue;
      el.style.setProperty(this.property.name, "");
      el.style.setProperty(this.property.name, value);
      // The property was set to `""` first, so if the value is no longer `""`,
      // it means that the second `setProperty` call set a valid property and we
      // can use this unit.
      if (el.style.getPropertyValue(this.property.name) !== "") {
        return unit;
      }
    }
    return "";
  }

  /**
   * Parses the property value and type.
   *
   * @param {String} value
   *        Property value.
   * @param {Number} offset
   *        Starting index of value.
   * @return {Object} object with properties 'value', 'start', 'end', and
   *         'type'.
   */
  #parseCSSValue(value, offset) {
    /* eslint-disable max-len */
    const reSplitCSS =
      /(?<url>url\("?[^"\)]+"?\)?)|(?<rgb>rgba?\([^)]*\)?)|(?<hsl>hsla?\([^)]*\)?)|(?<hwb>hwb\([^)]*\)?)|(?<hex>#[\dA-Fa-f]+)|(?<number>-?\d*\.?\d+(%|[a-z]{1,4})?)|"([^"]*)"?|'([^']*)'?|([^,\s\/!\(\)]+)|(!(.*)?)/;
    /* eslint-enable */
    let start = 0;
    let m;

    // retreive values from left to right until we find the one at our offset
    while ((m = reSplitCSS.exec(value)) && m.index + m[0].length < offset) {
      value = value.substring(m.index + m[0].length);
      start += m.index + m[0].length;
      offset -= m.index + m[0].length;
    }

    if (!m) {
      return null;
    }

    let type;
    if (m.groups.url) {
      type = "url";
    } else if (m.groups.rgb) {
      type = "rgb";
    } else if (m.groups.hsl) {
      type = "hsl";
    } else if (m.groups.hwb) {
      type = "hwb";
    } else if (m.groups.hex) {
      type = "hex";
    } else if (m.groups.number) {
      type = "num";
    }

    return {
      value: m[0],
      start: start + m.index,
      end: start + m.index + m[0].length,
      type,
    };
  }

  /**
   * Increment the property value for types other than
   * number or hex, such as rgb, hsl, hwb, and file names.
   *
   * @param {String} value
   *        Property value.
   * @param {Number} increment
   *        Amount to increment/decrement.
   * @param {Number} offset
   *        Starting index of the property value.
   * @param {Number} offsetEnd
   *        Ending index of the property value.
   * @param {Object} info
   *        Object with details about the property value.
   * @return {Object} object with properties 'value', 'start', and 'end'.
   */
  #incrementGenericValue(value, increment, offset, offsetEnd, info) {
    // Try to find a number around the cursor to increment.
    let start, end;
    // Check if we are incrementing in a non-number context (such as a URL)
    if (
      /^-?[0-9.]/.test(value.substring(offset, offsetEnd)) &&
      !/\d/.test(value.charAt(offset - 1) + value.charAt(offsetEnd))
    ) {
      // We have a number selected, possibly with a suffix, and we are not in
      // the disallowed case of just part of a known number being selected.
      // Use that number.
      start = offset;
      end = offsetEnd;
    } else {
      // Parse periods as belonging to the number only if we are in a known
      // number context. (This makes incrementing the 1 in 'image1.gif' work.)
      const pattern = "[" + (info ? "0-9." : "0-9") + "]*";
      const before = new RegExp(pattern + "$").exec(value.substr(0, offset))[0]
        .length;
      const after = new RegExp("^" + pattern).exec(value.substr(offset))[0]
        .length;

      start = offset - before;
      end = offset + after;

      // Expand the number to contain an initial minus sign if it seems
      // free-standing.
      if (
        value.charAt(start - 1) === "-" &&
        (start - 1 === 0 || /[ (:,='"]/.test(value.charAt(start - 2)))
      ) {
        --start;
      }
    }

    if (start !== end) {
      // Include percentages as part of the incremented number (they are
      // common enough).
      if (value.charAt(end) === "%") {
        ++end;
      }

      const first = value.substr(0, start);
      let mid = value.substring(start, end);
      const last = value.substr(end);

      mid = this.#incrementRawValue(mid, increment, info);

      if (mid !== null) {
        return {
          value: first + mid + last,
          start,
          end: start + mid.length,
        };
      }
    }

    return null;
  }

  /**
   * Increment the property value for numbers.
   *
   * @param {String} rawValue
   *        Raw value to increment.
   * @param {Number} increment
   *        Amount to increase/decrease the raw value.
   * @param {Object} info
   *        Object with info about the property value.
   * @return {String} the incremented value.
   */
  #incrementRawValue(rawValue, increment, info) {
    const num = parseFloat(rawValue);

    if (isNaN(num)) {
      return null;
    }

    const number = /\d+(\.\d+)?/.exec(rawValue);

    let units = rawValue.substr(number.index + number[0].length);
    if (info && "units" in info) {
      units = info.units;
    }

    // avoid rounding errors
    let newValue = Math.round((num + increment) * 1000) / 1000;

    if (info && "minValue" in info) {
      newValue = Math.max(newValue, info.minValue);
    }
    if (info && "maxValue" in info) {
      newValue = Math.min(newValue, info.maxValue);
    }

    newValue = newValue.toString();

    return newValue + units;
  }

  /**
   * Increment the property value for hex.
   *
   * @param {String} value
   *        Property value.
   * @param {Number} increment
   *        Amount to increase/decrease the property value.
   * @param {Number} offset
   *        Starting index of the property value.
   * @param {Number} offsetEnd
   *        Ending index of the property value.
   * @return {Object} object with properties 'value' and 'selection'.
   */
  #incHexColor(rawValue, increment, offset, offsetEnd) {
    // Return early if no part of the rawValue is selected.
    if (offsetEnd > rawValue.length && offset >= rawValue.length) {
      return null;
    }
    if (offset < 1 && offsetEnd <= 1) {
      return null;
    }
    // Ignore the leading #.
    rawValue = rawValue.substr(1);
    --offset;
    --offsetEnd;

    // Clamp the selection to within the actual value.
    offset = Math.max(offset, 0);
    offsetEnd = Math.min(offsetEnd, rawValue.length);
    offsetEnd = Math.max(offsetEnd, offset);

    // Normalize #ABC -> #AABBCC.
    if (rawValue.length === 3) {
      rawValue =
        rawValue.charAt(0) +
        rawValue.charAt(0) +
        rawValue.charAt(1) +
        rawValue.charAt(1) +
        rawValue.charAt(2) +
        rawValue.charAt(2);
      offset *= 2;
      offsetEnd *= 2;
    }

    // Normalize #ABCD -> #AABBCCDD.
    if (rawValue.length === 4) {
      rawValue =
        rawValue.charAt(0) +
        rawValue.charAt(0) +
        rawValue.charAt(1) +
        rawValue.charAt(1) +
        rawValue.charAt(2) +
        rawValue.charAt(2) +
        rawValue.charAt(3) +
        rawValue.charAt(3);
      offset *= 2;
      offsetEnd *= 2;
    }

    if (rawValue.length !== 6 && rawValue.length !== 8) {
      return null;
    }

    // If no selection, increment an adjacent color, preferably one to the left.
    if (offset === offsetEnd) {
      if (offset === 0) {
        offsetEnd = 1;
      } else {
        offset = offsetEnd - 1;
      }
    }

    // Make the selection cover entire parts.
    offset -= offset % 2;
    offsetEnd += offsetEnd % 2;

    // Remap the increments from [0.1, 1, 10] to [1, 1, 16].
    if (increment > -1 && increment < 1) {
      increment = increment < 0 ? -1 : 1;
    }
    if (Math.abs(increment) === 10) {
      increment = increment < 0 ? -16 : 16;
    }

    const isUpper = rawValue.toUpperCase() === rawValue;

    for (let pos = offset; pos < offsetEnd; pos += 2) {
      // Increment the part in [pos, pos+2).
      let mid = rawValue.substr(pos, 2);
      const value = parseInt(mid, 16);

      if (isNaN(value)) {
        return null;
      }

      mid = Math.min(Math.max(value + increment, 0), 255).toString(16);

      while (mid.length < 2) {
        mid = "0" + mid;
      }
      if (isUpper) {
        mid = mid.toUpperCase();
      }

      rawValue = rawValue.substr(0, pos) + mid + rawValue.substr(pos + 2);
    }

    return {
      value: "#" + rawValue,
      selection: [offset + 1, offsetEnd + 1],
    };
  }

  /**
   * Cycle through the autocompletion suggestions in the popup.
   *
   * @param {Boolean} reverse
   *        true to select previous item from the popup.
   * @param {Boolean} noSelect
   *        true to not select the text after selecting the newly selectedItem
   *        from the popup.
   */
  #cycleCSSSuggestion(reverse, noSelect) {
    // selectedItem can be null when nothing is selected in an empty editor.
    const { label, preLabel } = this.popup.selectedItem || {
      label: "",
      preLabel: "",
    };
    if (reverse) {
      this.popup.selectPreviousItem();
    } else {
      this.popup.selectNextItem();
    }

    this.#selectedIndex = this.popup.selectedIndex;
    const input = this.input;
    let pre = "";

    if (input.selectionStart < input.selectionEnd) {
      pre = input.value.slice(0, input.selectionStart);
    } else {
      pre = input.value.slice(
        0,
        input.selectionStart - label.length + preLabel.length
      );
    }

    const post = input.value.slice(input.selectionEnd, input.value.length);
    const item = this.popup.selectedItem;
    const toComplete = item.label.slice(item.preLabel.length);
    input.value = pre + toComplete + post;

    if (!noSelect) {
      input.setSelectionRange(pre.length, pre.length + toComplete.length);
    } else {
      input.setSelectionRange(
        pre.length + toComplete.length,
        pre.length + toComplete.length
      );
    }

    this.#updateSize();
    // This emit is mainly for the purpose of making the test flow simpler.
    this.emit("after-suggest");
  }

  /**
   * Call the client's done handler and clear out.
   */
  #apply(direction, key) {
    if (this.#applied) {
      return null;
    }

    this.#applied = true;

    if (this.done) {
      const val = this.cancelled ? this.initial : this.currentInputValue;
      return this.done(val, !this.cancelled, direction, key);
    }

    return null;
  }

  /**
   * Hide the popup and cancel any pending popup opening.
   */
  #onWindowBlur = () => {
    if (this.popup && this.popup.isOpen) {
      this.popup.hidePopup();
    }

    if (this.#openPopupTimeout) {
      this.doc.defaultView.clearTimeout(this.#openPopupTimeout);
    }
  };

  /**
   * Event handler called when the inplace-editor's input loses focus.
   */
  #onBlur = event => {
    if (
      event &&
      this.popup &&
      this.popup.isOpen &&
      this.popup.selectedIndex >= 0
    ) {
      this.#acceptPopupSuggestion();
    } else {
      this.#apply();
      this.#clear();
    }
  };

  /**
   * Before offering autocomplete, set this.gridLineNames as the line names
   * of the current grid, if they exist.
   *
   * @param {Function} getGridLineNames
   *        A function which gets the line names of the current grid.
   */
  async #getGridNamesBeforeCompletion(getGridLineNames) {
    if (
      getGridLineNames &&
      this.property &&
      GRID_PROPERTY_NAMES.includes(this.property.name)
    ) {
      this.gridLineNames = await getGridLineNames();
    }

    if (
      this.contentType == CONTENT_TYPES.CSS_VALUE &&
      this.input &&
      this.input.value == ""
    ) {
      this.#maybeSuggestCompletion(false);
    }
  }

  /**
   * Event handler called by the autocomplete popup when receiving a click
   * event.
   */
  #onAutocompletePopupClick = () => {
    this.#acceptPopupSuggestion();
  };

  #acceptPopupSuggestion() {
    let label, preLabel;

    if (this.#selectedIndex === undefined) {
      ({ label, preLabel } = this.popup.getItemAtIndex(
        this.popup.selectedIndex
      ));
    } else {
      ({ label, preLabel } = this.popup.getItemAtIndex(this.#selectedIndex));
    }

    const input = this.input;

    let pre = "";

    // CSS_MIXED needs special treatment here to make it so that
    // multiple presses of tab will cycle through completions, but
    // without selecting the completed text.  However, this same
    // special treatment will do the wrong thing for other editing
    // styles.
    if (
      input.selectionStart < input.selectionEnd ||
      this.contentType !== CONTENT_TYPES.CSS_MIXED
    ) {
      pre = input.value.slice(0, input.selectionStart);
    } else {
      pre = input.value.slice(
        0,
        input.selectionStart - label.length + preLabel.length
      );
    }
    const post = input.value.slice(input.selectionEnd, input.value.length);
    const item = this.popup.selectedItem;
    this.#selectedIndex = this.popup.selectedIndex;
    const toComplete = item.label.slice(item.preLabel.length);
    input.value = pre + toComplete + post;
    input.setSelectionRange(
      pre.length + toComplete.length,
      pre.length + toComplete.length
    );
    this.#updateSize();
    // Wait for the popup to hide and then focus input async otherwise it does
    // not work.
    const onPopupHidden = () => {
      this.popup.off("popup-closed", onPopupHidden);
      this.doc.defaultView.setTimeout(() => {
        input.focus();
        this.emit("after-suggest");
      }, 0);
    };
    this.popup.on("popup-closed", onPopupHidden);
    this.#hideAutocompletePopup();
  }

  /**
   * Handle the input field's keypress event.
   */
  // eslint-disable-next-line complexity
  #onKeyPress = event => {
    let prevent = false;

    const key = event.keyCode;
    const input = this.input;

    // We want to autoclose some characters, remember the pressed key in order to process
    // it later on in maybeSuggestionCompletion().
    this.#pressedKey = event.key;

    const multilineNavigation =
      !this.#isSingleLine() && isKeyIn(key, "UP", "DOWN", "LEFT", "RIGHT");
    const isPlainText = this.contentType == CONTENT_TYPES.PLAIN_TEXT;
    const isPopupOpen = this.popup && this.popup.isOpen;

    let increment = 0;
    if (!isPlainText && !multilineNavigation) {
      increment = this.#getIncrement(event);
    }

    if (isKeyIn(key, "PAGE_UP", "PAGE_DOWN")) {
      this.#preventSuggestions = true;
    }

    let cycling = false;
    if (increment && this.#incrementValue(increment)) {
      this.#updateSize();
      prevent = true;
      cycling = true;
    }

    if (isPopupOpen && isKeyIn(key, "UP", "DOWN", "PAGE_UP", "PAGE_DOWN")) {
      prevent = true;
      cycling = true;
      this.#cycleCSSSuggestion(isKeyIn(key, "UP", "PAGE_UP"));
      this.#doValidation();
    }

    if (isKeyIn(key, "BACK_SPACE", "DELETE", "LEFT", "RIGHT", "HOME", "END")) {
      if (isPopupOpen && this.currentInputValue !== "") {
        this.#hideAutocompletePopup();
      }
    } else if (
      // We may show the suggestion completion if Ctrl+space is pressed, or if an
      // otherwise unhandled key is pressed and the user is not cycling through the
      // options in the pop-up menu, it is not an expanded shorthand property, and no
      // modifier key is pressed.
      (event.key === " " && event.ctrlKey) ||
      (!cycling &&
        !multilineNavigation &&
        !event.metaKey &&
        !event.altKey &&
        !event.ctrlKey)
    ) {
      this.#maybeSuggestCompletion(true);
    }

    if (this.multiline && event.shiftKey && isKeyIn(key, "RETURN")) {
      prevent = false;
    } else if (
      this.#advanceChars(event.charCode, input.value, input.selectionStart) ||
      isKeyIn(key, "RETURN", "TAB")
    ) {
      prevent = true;

      const ctrlOrCmd = isOSX ? event.metaKey : event.ctrlKey;

      let direction;
      if (
        (this.stopOnReturn && isKeyIn(key, "RETURN") && !ctrlOrCmd) ||
        (this.stopOnTab && !event.shiftKey && isKeyIn(key, "TAB")) ||
        (this.stopOnShiftTab && event.shiftKey && isKeyIn(key, "TAB"))
      ) {
        direction = null;
      } else if (event.shiftKey && isKeyIn(key, "TAB")) {
        direction = FOCUS_BACKWARD;
      } else {
        direction = FOCUS_FORWARD;
      }

      // Now we don't want to suggest anything as we are moving out.
      this.#preventSuggestions = true;
      // But we still want to show suggestions for css values. i.e. moving out
      // of css property input box in forward direction
      if (
        this.contentType == CONTENT_TYPES.CSS_PROPERTY &&
        direction == FOCUS_FORWARD
      ) {
        this.#preventSuggestions = false;
      }

      if (isKeyIn(key, "TAB") && this.contentType == CONTENT_TYPES.CSS_MIXED) {
        if (this.popup && input.selectionStart < input.selectionEnd) {
          event.preventDefault();
          input.setSelectionRange(input.selectionEnd, input.selectionEnd);
          this.emit("after-suggest");
          return;
        } else if (this.popup && this.popup.isOpen) {
          event.preventDefault();
          this.#cycleCSSSuggestion(event.shiftKey, true);
          return;
        }
      }

      this.#apply(direction, key);

      // Close the popup if open
      if (this.popup && this.popup.isOpen) {
        this.#hideAutocompletePopup();
      }

      if (direction !== null && focusManager.focusedElement === input) {
        // If the focused element wasn't changed by the done callback,
        // move the focus as requested.
        const next = moveFocus(
          this.doc.defaultView,
          direction,
          this.focusEditableFieldAfterApply,
          this.focusEditableFieldContainerSelector
        );

        // If the next node to be focused has been tagged as an editable
        // node, trigger editing using the configured event
        if (next && next.ownerDocument === this.doc && next._editable) {
          const e = this.doc.createEvent("Event");
          e.initEvent(next._trigger, true, true);
          next.dispatchEvent(e);
        }
      }

      this.#clear();
    } else if (isKeyIn(key, "ESCAPE")) {
      // Cancel and blur ourselves.
      // Now we don't want to suggest anything as we are moving out.
      this.#preventSuggestions = true;
      // Close the popup if open
      if (this.popup && this.popup.isOpen) {
        this.#hideAutocompletePopup();
      }
      prevent = true;
      this.cancelled = true;
      this.#apply();
      this.#clear();
      event.stopPropagation();
    } else if (isKeyIn(key, "SPACE")) {
      // No need for leading spaces here.  This is particularly
      // noticable when adding a property: it's very natural to type
      // <name>: (which advances to the next property) then spacebar.
      prevent = !input.value;
    }

    if (prevent) {
      event.preventDefault();
    }
  };

  #onContextMenu = event => {
    if (this.contextMenu) {
      // Call stopPropagation() and preventDefault() here so that avoid to show default
      // context menu in about:devtools-toolbox. See Bug 1515265.
      event.stopPropagation();
      event.preventDefault();
      this.contextMenu(event);
    }
  };

  /**
   * Open the autocomplete popup, adding a custom click handler and classname.
   *
   * @param {Number} offset
   *        X-offset relative to the input starting edge.
   * @param {Number} selectedIndex
   *        The index of the item that should be selected. Use -1 to have no
   *        item selected.
   */
  #openAutocompletePopup(offset, selectedIndex) {
    this.popup.on("popup-click", this.#onAutocompletePopupClick);
    this.popup.openPopup(this.input, offset, 0, selectedIndex);
  }

  /**
   * Remove the custom classname and click handler and close the autocomplete
   * popup.
   */
  #hideAutocompletePopup() {
    this.popup.off("popup-click", this.#onAutocompletePopupClick);
    this.popup.hidePopup();
  }

  /**
   * Get the increment/decrement step to use for the provided key event.
   */
  #getIncrement(event) {
    const getSmallIncrementKey = evt => {
      if (isOSX) {
        return evt.altKey;
      }
      return evt.ctrlKey;
    };

    const largeIncrement = 100;
    const mediumIncrement = 10;
    const smallIncrement = 0.1;

    let increment = 0;
    const key = event.keyCode;

    if (isKeyIn(key, "UP", "PAGE_UP")) {
      increment = 1 * this.defaultIncrement;
    } else if (isKeyIn(key, "DOWN", "PAGE_DOWN")) {
      increment = -1 * this.defaultIncrement;
    }

    if (event.shiftKey && !getSmallIncrementKey(event)) {
      if (isKeyIn(key, "PAGE_UP", "PAGE_DOWN")) {
        increment *= largeIncrement;
      } else {
        increment *= mediumIncrement;
      }
    } else if (getSmallIncrementKey(event) && !event.shiftKey) {
      increment *= smallIncrement;
    }

    return increment;
  }

  /**
   * Handle the input field's keyup event.
   */
  #onKeyup = () => {
    this.#applied = false;
  };

  /**
   * Handle changes to the input text.
   */
  #onInput = () => {
    // Validate the entered value.
    this.#doValidation();

    // Update size if we're autosizing.
    if (this.#measurement) {
      this.#updateSize();
    }

    // Call the user's change handler if available.
    if (this.change) {
      this.change(this.currentInputValue);
    }

    // In case that the current value becomes empty, show the suggestions if needed.
    if (this.currentInputValue === "" && this.showSuggestCompletionOnEmpty) {
      this.#maybeSuggestCompletion(false);
    }
  };

  /**
   * Stop propagation on the provided event
   */
  #stopEventPropagation(e) {
    e.stopPropagation();
  }

  /**
   * Fire validation callback with current input
   */
  #doValidation() {
    if (this.validate && this.input) {
      this.validate(this.input.value);
    }
  }

  /**
   * Handles displaying suggestions based on the current input.
   *
   * @param {Boolean} autoInsert
   *        Pass true to automatically insert the most relevant suggestion.
   */
  #maybeSuggestCompletion(autoInsert) {
    // Input can be null in cases when you intantaneously switch out of it.
    if (!this.input) {
      return;
    }

    const preTimeoutQuery = this.input.value;

    // Since we are calling this method from a keypress event handler, the
    // |input.value| does not include currently typed character. Thus we perform
    // this method async.
    // eslint-disable-next-line complexity
    this.#openPopupTimeout = this.doc.defaultView.setTimeout(() => {
      if (this.#preventSuggestions) {
        this.#preventSuggestions = false;
        return;
      }
      if (this.contentType == CONTENT_TYPES.PLAIN_TEXT) {
        return;
      }
      if (!this.input) {
        return;
      }
      const input = this.input;
      // The length of input.value should be increased by 1
      if (input.value.length - preTimeoutQuery.length > 1) {
        return;
      }
      const query = input.value.slice(0, input.selectionStart);
      let startCheckQuery = query;
      if (query == null) {
        return;
      }
      // If nothing is selected and there is a word (\w) character after the cursor, do
      // not autocomplete.
      if (
        input.selectionStart == input.selectionEnd &&
        input.selectionStart < input.value.length
      ) {
        const nextChar = input.value.slice(input.selectionStart)[0];
        // Check if the next character is a valid word character, no suggestion should be
        // provided when preceeding a word.
        if (/[\w-]/.test(nextChar)) {
          // This emit is mainly to make the test flow simpler.
          this.emit("after-suggest", "nothing to autocomplete");
          return;
        }
      }

      let list = [];
      let postLabelValues = [];

      if (this.contentType == CONTENT_TYPES.CSS_PROPERTY) {
        list = this.#getCSSPropertyList();
      } else if (this.contentType == CONTENT_TYPES.CSS_VALUE) {
        // Get the last query to be completed before the caret.
        const match = /([^\s,.\/]+$)/.exec(query);
        if (match) {
          startCheckQuery = match[0];
        } else {
          startCheckQuery = "";
        }

        // Check if the query to be completed is a CSS variable.
        const varMatch = /^var\(([^\s]+$)/.exec(startCheckQuery);

        if (varMatch && varMatch.length == 2) {
          startCheckQuery = varMatch[1];
          list = this.#getCSSVariableNames();
          postLabelValues = list.map(varName =>
            this.#getCSSVariableValue(varName)
          );
        } else {
          list = [
            "!important",
            ...this.#getCSSValuesForPropertyName(this.property.name),
          ];
        }

        if (query == "") {
          // Do not suggest '!important' without any manually typed character.
          list.splice(0, 1);
        }
      } else if (
        this.contentType == CONTENT_TYPES.CSS_MIXED &&
        /^\s*style\s*=/.test(query)
      ) {
        // Check if the style attribute is closed before the selection.
        const styleValue = query.replace(/^\s*style\s*=\s*/, "");
        // Look for a quote matching the opening quote (single or double).
        if (/^("[^"]*"|'[^']*')/.test(styleValue)) {
          // This emit is mainly to make the test flow simpler.
          this.emit("after-suggest", "nothing to autocomplete");
          return;
        }

        // Detecting if cursor is at property or value;
        const match = query.match(/([:;"'=]?)\s*([^"';:=]+)?$/);
        if (match && match.length >= 2) {
          if (match[1] == ":") {
            // We are in CSS value completion
            const propertyName = query.match(
              /[;"'=]\s*([^"';:= ]+)\s*:\s*[^"';:=]*$/
            )[1];
            list = [
              "!important;",
              ...this.#getCSSValuesForPropertyName(propertyName),
            ];
            const matchLastQuery = /([^\s,.\/]+$)/.exec(match[2] || "");
            if (matchLastQuery) {
              startCheckQuery = matchLastQuery[0];
            } else {
              startCheckQuery = "";
            }
            if (!match[2]) {
              // Don't suggest '!important' without any manually typed character
              list.splice(0, 1);
            }
          } else if (match[1]) {
            // We are in CSS property name completion
            list = this.#getCSSPropertyList();
            startCheckQuery = match[2];
          }
          if (startCheckQuery == null) {
            // This emit is mainly to make the test flow simpler.
            this.emit("after-suggest", "nothing to autocomplete");
            return;
          }
        }
      }

      if (!this.popup) {
        // This emit is mainly to make the test flow simpler.
        this.emit("after-suggest", "no popup");
        return;
      }

      const finalList = [];
      const length = list.length;
      for (let i = 0, count = 0; i < length && count < MAX_POPUP_ENTRIES; i++) {
        if (startCheckQuery != null && list[i].startsWith(startCheckQuery)) {
          count++;
          finalList.push({
            preLabel: startCheckQuery,
            label: list[i],
            postLabel: postLabelValues[i] ? postLabelValues[i] : "",
          });
        } else if (count > 0) {
          // Since count was incremented, we had already crossed the entries
          // which would have started with query, assuming that list is sorted.
          break;
        } else if (startCheckQuery != null && list[i][0] > startCheckQuery[0]) {
          // We have crossed all possible matches alphabetically.
          break;
        }
      }

      // Sort items starting with [a-z0-9] first, to make sure vendor-prefixed
      // values and "!important" are suggested only after standard values.
      finalList.sort((item1, item2) => {
        // Get the expected alphabetical comparison between the items.
        let comparison = item1.label.localeCompare(item2.label);
        if (/^\w/.test(item1.label) != /^\w/.test(item2.label)) {
          // One starts with [a-z0-9], one does not: flip the comparison.
          comparison = -1 * comparison;
        }
        return comparison;
      });

      let index = 0;
      if (startCheckQuery) {
        // Only select a "best" suggestion when the user started a query.
        const cssValues = finalList.map(item => item.label);
        index = findMostRelevantCssPropertyIndex(cssValues);
      }

      // Insert the most relevant item from the final list as the input value.
      if (autoInsert && finalList[index]) {
        const item = finalList[index].label;
        input.value =
          query +
          item.slice(startCheckQuery.length) +
          input.value.slice(query.length);
        input.setSelectionRange(
          query.length,
          query.length + item.length - startCheckQuery.length
        );
        this.#updateSize();
      }

      // Display the list of suggestions if there are more than one.
      if (finalList.length > 1) {
        // Calculate the popup horizontal offset.
        const indent = this.input.selectionStart - startCheckQuery.length;
        let offset = indent * this.inputCharDimensions.width;
        offset = this.#isSingleLine() ? offset : 0;

        // Select the most relevantItem if autoInsert is allowed
        const selectedIndex = autoInsert ? index : -1;

        // Open the suggestions popup.
        this.popup.setItems(finalList, selectedIndex);
        this.#openAutocompletePopup(offset, selectedIndex);
      } else {
        this.#hideAutocompletePopup();
      }

      this.#autocloseParenthesis();

      // This emit is mainly for the purpose of making the test flow simpler.
      this.emit("after-suggest");
      this.#doValidation();
    }, 0);
  }

  /**
   * Automatically add closing parenthesis and skip closing parenthesis when needed.
   */
  #autocloseParenthesis() {
    // Split the current value at the cursor index to rebuild the string.
    const parts = this.#splitStringAt(
      this.input.value,
      this.input.selectionStart
    );

    // Lookup the character following the caret to know if the string should be modified.
    const nextChar = parts[1][0];

    // Autocomplete closing parenthesis if the last key pressed was "(" and the next
    // character is not a "word" character.
    if (this.#pressedKey == "(" && !isWordChar(nextChar)) {
      this.#updateValue(parts[0] + ")" + parts[1]);
    }

    // Skip inserting ")" if the next character is already a ")" (note that we actually
    // insert and remove the extra ")" here, as the input has already been modified).
    if (this.#pressedKey == ")" && nextChar == ")") {
      this.#updateValue(parts[0] + parts[1].substring(1));
    }

    this.#pressedKey = null;
  }

  /**
   * Update the current value of the input while preserving the caret position.
   */
  #updateValue(str) {
    const start = this.input.selectionStart;
    this.input.value = str;
    this.input.setSelectionRange(start, start);
    this.#updateSize();
  }

  /**
   * Split the provided string at the provided index. Returns an array of two strings.
   * _splitStringAt("1234567", 3) will return ["123", "4567"]
   */
  #splitStringAt(str, index) {
    return [str.substring(0, index), str.substring(index, str.length)];
  }

  /**
   * Check if the current input is displaying more than one line of text.
   *
   * @return {Boolean} true if the input has a single line of text
   */
  #isSingleLine() {
    if (!this.multiline) {
      // Checking the inputCharDimensions.height only makes sense with multiline
      // editors, because the textarea is directly sized using
      // inputCharDimensions (see _updateSize()).
      // By definition if !this.multiline, then we are in single line mode.
      return true;
    }
    const inputRect = this.input.getBoundingClientRect();
    return inputRect.height < 2 * this.inputCharDimensions.height;
  }

  /**
   * Returns the list of CSS properties to use for the autocompletion. This
   * method is overridden by tests in order to use mocked suggestion lists.
   *
   * @return {Array} array of CSS property names (Strings)
   */
  #getCSSPropertyList() {
    return this.cssProperties.getNames().sort();
  }

  /**
   * Returns a list of CSS values valid for a provided property name to use for
   * the autocompletion. This method is overridden by tests in order to use
   * mocked suggestion lists.
   *
   * @param {String} propertyName
   * @return {Array} array of CSS property values (Strings)
   */
  #getCSSValuesForPropertyName(propertyName) {
    const gridLineList = [];
    if (this.gridLineNames) {
      if (GRID_ROW_PROPERTY_NAMES.includes(this.property.name)) {
        gridLineList.push(...this.gridLineNames.rows);
      }
      if (GRID_COL_PROPERTY_NAMES.includes(this.property.name)) {
        gridLineList.push(...this.gridLineNames.cols);
      }
    }
    // Must be alphabetically sorted before comparing the results with
    // the user input, otherwise we will lose some results.
    return gridLineList
      .concat(this.cssProperties.getValues(propertyName))
      .sort();
  }

  /**
   * Returns the list of all CSS variables to use for the autocompletion.
   *
   * @return {Array} array of CSS variable names (Strings)
   */
  #getCSSVariableNames() {
    return Array.from(this.cssVariables.keys()).sort();
  }

  /**
   * Returns the variable's value for the given CSS variable name.
   *
   * @param {String} varName
   *        The variable name to retrieve the value of
   * @return {String} the variable value to the given CSS variable name
   */
  #getCSSVariableValue(varName) {
    return this.cssVariables.get(varName);
  }
}

exports.InplaceEditor = InplaceEditor;

/**
 * Copy text-related styles from one element to another.
 */
function copyTextStyles(from, to) {
  const win = from.ownerDocument.defaultView;
  const style = win.getComputedStyle(from);

  to.style.fontFamily = style.fontFamily;
  to.style.fontSize = style.fontSize;
  to.style.fontWeight = style.fontWeight;
  to.style.fontStyle = style.fontStyle;
}

/**
 * Copy all styles which could have an impact on the element size.
 */
function copyAllStyles(from, to) {
  const win = from.ownerDocument.defaultView;
  const style = win.getComputedStyle(from);

  copyTextStyles(from, to);
  to.style.lineHeight = style.lineHeight;

  // If box-sizing is set to border-box, box model styles also need to be
  // copied.
  const boxSizing = style.boxSizing;
  if (boxSizing === "border-box") {
    to.style.boxSizing = boxSizing;
    copyBoxModelStyles(from, to);
  }
}

/**
 * Copy box model styles that can impact width and height measurements when box-
 * sizing is set to "border-box" instead of "content-box".
 *
 * @param {DOMNode} from
 *        the element from which styles are copied
 * @param {DOMNode} to
 *        the element on which copied styles are applied
 */
function copyBoxModelStyles(from, to) {
  const properties = [
    // Copy all paddings.
    "paddingTop",
    "paddingRight",
    "paddingBottom",
    "paddingLeft",
    // Copy border styles.
    "borderTopStyle",
    "borderRightStyle",
    "borderBottomStyle",
    "borderLeftStyle",
    // Copy border widths.
    "borderTopWidth",
    "borderRightWidth",
    "borderBottomWidth",
    "borderLeftWidth",
  ];

  const win = from.ownerDocument.defaultView;
  const style = win.getComputedStyle(from);
  for (const property of properties) {
    to.style[property] = style[property];
  }
}

/**
 * Trigger a focus change similar to pressing tab/shift-tab.
 *
 * @param {Window} win: The window into which the focus should be moved
 * @param {Number} direction: See Services.focus.MOVEFOCUS_*
 * @param {Boolean} focusEditableField: Set to true to move the focus to the previous/next
 *        editable field. If not set, the focus will be set on the next focusable element.
 *        The function might still put the focus on a non-editable field, if none is found
 *        within the element matching focusEditableFieldContainerSelector
 * @param {String} focusEditableFieldContainerSelector: A CSS selector the editabled element
 *        we want to focus should be in. This is only used when focusEditableField is set
 *        to true.
 *        It's important to pass a boundary otherwise we might hit an infinite loop
 * @returns {Element} The element that received the focus
 */
function moveFocus(
  win,
  direction,
  focusEditableField,
  focusEditableFieldContainerSelector
) {
  if (!focusEditableField) {
    return focusManager.moveFocus(win, null, direction, 0);
  }

  if (!win.document.querySelector(focusEditableFieldContainerSelector)) {
    console.error(
      focusEditableFieldContainerSelector,
      "can't be found in document.",
      `focusEditableFieldContainerSelector should match an existing element`
    );
    return focusManager.moveFocus(win, null, direction, 0);
  }

  // Let's look for the next/previous editable element to focus
  while (true) {
    const focusedElement = focusManager.moveFocus(win, null, direction, 0);
    // The _editable property is set by the InplaceEditor on the target element
    if (focusedElement._editable) {
      return focusedElement;
    }

    // If the focus was moved outside of the container, simply return the focused element
    if (!focusedElement.closest(focusEditableFieldContainerSelector)) {
      return focusedElement;
    }
  }
}
