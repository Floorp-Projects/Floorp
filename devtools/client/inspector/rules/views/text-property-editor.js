/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {l10n} = require("devtools/shared/inspector/css-logic");
const {getCssProperties} = require("devtools/shared/fronts/css-properties");
const {InplaceEditor, editableField} =
      require("devtools/client/shared/inplace-editor");
const {
  createChild,
  appendText,
  advanceValidate,
  blurOnMultipleProperties
} = require("devtools/client/inspector/shared/utils");
const {
  parseDeclarations,
  parseSingleValue,
} = require("devtools/shared/css/parsing-utils");
const Services = require("Services");

const HTML_NS = "http://www.w3.org/1999/xhtml";

const SHARED_SWATCH_CLASS = "ruleview-swatch";
const COLOR_SWATCH_CLASS = "ruleview-colorswatch";
const BEZIER_SWATCH_CLASS = "ruleview-bezierswatch";
const FILTER_SWATCH_CLASS = "ruleview-filterswatch";
const ANGLE_SWATCH_CLASS = "ruleview-angleswatch";
const FONT_FAMILY_CLASS = "ruleview-font-family";
const SHAPE_SWATCH_CLASS = "ruleview-shapeswatch";

/*
 * An actionable element is an element which on click triggers a specific action
 * (e.g. shows a color tooltip, opens a link, …).
 */
const ACTIONABLE_ELEMENTS_SELECTORS = [
  `.${COLOR_SWATCH_CLASS}`,
  `.${BEZIER_SWATCH_CLASS}`,
  `.${FILTER_SWATCH_CLASS}`,
  `.${ANGLE_SWATCH_CLASS}`,
  "a"
];

// In order to highlight the used fonts in font-family properties, we
// retrieve the list of used fonts from the server. That always
// returns the actually used font family name(s). If the property's
// authored value is sans-serif for instance, the used font might be
// arial instead.  So we need the list of all generic font family
// names to underline those when we find them.
const GENERIC_FONT_FAMILIES = [
  "serif",
  "sans-serif",
  "cursive",
  "fantasy",
  "monospace",
  "system-ui"
];

/**
 * TextPropertyEditor is responsible for the following:
 *   Owns a TextProperty object.
 *   Manages changes to the TextProperty.
 *   Can be expanded to display computed properties.
 *   Can mark a property disabled or enabled.
 *
 * @param {RuleEditor} ruleEditor
 *        The rule editor that owns this TextPropertyEditor.
 * @param {TextProperty} property
 *        The text property to edit.
 */
function TextPropertyEditor(ruleEditor, property) {
  this.ruleEditor = ruleEditor;
  this.ruleView = this.ruleEditor.ruleView;
  this.doc = this.ruleEditor.doc;
  this.popup = this.ruleView.popup;
  this.prop = property;
  this.prop.editor = this;
  this.browserWindow = this.doc.defaultView.top;
  this._populatedComputed = false;
  this._hasPendingClick = false;
  this._clickedElementOptions = null;

  const toolbox = this.ruleView.inspector.toolbox;
  this.cssProperties = getCssProperties(toolbox);

  this.getGridlineNames = this.getGridlineNames.bind(this);
  this.update = this.update.bind(this);
  this.updatePropertyState = this.updatePropertyState.bind(this);
  this._onEnableClicked = this._onEnableClicked.bind(this);
  this._onExpandClicked = this._onExpandClicked.bind(this);
  this._onNameDone = this._onNameDone.bind(this);
  this._onStartEditing = this._onStartEditing.bind(this);
  this._onSwatchCommit = this._onSwatchCommit.bind(this);
  this._onSwatchPreview = this._onSwatchPreview.bind(this);
  this._onSwatchRevert = this._onSwatchRevert.bind(this);
  this._onValidate = this.ruleView.debounce(this._previewValue, 10, this);
  this._onValueDone = this._onValueDone.bind(this);

  this._create();
  this.update();
}

TextPropertyEditor.prototype = {
  /**
   * Boolean indicating if the name or value is being currently edited.
   */
  get editing() {
    return !!(this.nameSpan.inplaceEditor || this.valueSpan.inplaceEditor ||
      this.ruleView.tooltips.isEditing) || this.popup.isOpen;
  },

  /**
   * Get the rule to the current text property
   */
  get rule() {
    return this.prop.rule;
  },

  /**
   * Create the property editor's DOM.
   */
  _create: function() {
    this.element = this.doc.createElementNS(HTML_NS, "li");
    this.element.classList.add("ruleview-property");
    this.element._textPropertyEditor = this;

    this.container = createChild(this.element, "div", {
      class: "ruleview-propertycontainer inline-tooltip-container"
    });

    // The enable checkbox will disable or enable the rule.
    this.enable = createChild(this.container, "div", {
      class: "ruleview-enableproperty theme-checkbox",
      tabindex: "-1"
    });

    this.nameContainer = createChild(this.container, "span", {
      class: "ruleview-namecontainer"
    });

    // Property name, editable when focused.  Property name
    // is committed when the editor is unfocused.
    this.nameSpan = createChild(this.nameContainer, "span", {
      class: "ruleview-propertyname theme-fg-color5",
      tabindex: this.ruleEditor.isEditable ? "0" : "-1",
    });

    appendText(this.nameContainer, ": ");

    // Click to expand the computed properties of the text property.
    this.expander = createChild(this.container, "span", {
      class: "ruleview-expander theme-twisty"
    });
    this.expander.addEventListener("click", this._onExpandClicked, true);

    // Create a span that will hold the property and semicolon.
    // Use this span to create a slightly larger click target
    // for the value.
    this.valueContainer = createChild(this.container, "span", {
      class: "ruleview-propertyvaluecontainer"
    });

    // Property value, editable when focused.  Changes to the
    // property value are applied as they are typed, and reverted
    // if the user presses escape.
    this.valueSpan = createChild(this.valueContainer, "span", {
      class: "ruleview-propertyvalue theme-fg-color1",
      tabindex: this.ruleEditor.isEditable ? "0" : "-1",
    });

    // Storing the TextProperty on the elements for easy access
    // (for instance by the tooltip)
    this.valueSpan.textProperty = this.prop;
    this.nameSpan.textProperty = this.prop;

    // If the value is a color property we need to put it through the parser
    // so that colors can be coerced into the default color type. This prevents
    // us from thinking that when colors are coerced they have been changed by
    // the user.
    let outputParser = this.ruleView._outputParser;
    let frag = outputParser.parseCssProperty(this.prop.name, this.prop.value);
    let parsedValue = frag.textContent;

    // Save the initial value as the last committed value,
    // for restoring after pressing escape.
    this.committed = { name: this.prop.name,
                       value: parsedValue,
                       priority: this.prop.priority };

    appendText(this.valueContainer, ";");

    this.warning = createChild(this.container, "div", {
      class: "ruleview-warning",
      hidden: "",
      title: l10n("rule.warning.title"),
    });

    // Filter button that filters for the current property name and is
    // displayed when the property is overridden by another rule.
    this.filterProperty = createChild(this.container, "div", {
      class: "ruleview-overridden-rule-filter",
      hidden: "",
      title: l10n("rule.filterProperty.title"),
    });

    this.filterProperty.addEventListener("click", event => {
      this.ruleEditor.ruleView.setFilterStyles("`" + this.prop.name + "`");
      event.stopPropagation();
    });

    // Holds the viewers for the computed properties.
    // will be populated in |_updateComputed|.
    this.computed = createChild(this.element, "ul", {
      class: "ruleview-computedlist",
    });

    // Holds the viewers for the overridden shorthand properties.
    // will be populated in |_updateShorthandOverridden|.
    this.shorthandOverridden = createChild(this.element, "ul", {
      class: "ruleview-overridden-items",
    });

    // Only bind event handlers if the rule is editable.
    if (this.ruleEditor.isEditable) {
      this.enable.addEventListener("click", this._onEnableClicked, true);

      this.nameContainer.addEventListener("click", (event) => {
        // Clicks within the name shouldn't propagate any further.
        event.stopPropagation();

        // Forward clicks on nameContainer to the editable nameSpan
        if (event.target === this.nameContainer) {
          this.nameSpan.click();
        }
      });

      editableField({
        start: this._onStartEditing,
        element: this.nameSpan,
        done: this._onNameDone,
        destroy: this.updatePropertyState,
        advanceChars: ":",
        contentType: InplaceEditor.CONTENT_TYPES.CSS_PROPERTY,
        popup: this.popup,
        cssProperties: this.cssProperties,
      });

      // Auto blur name field on multiple CSS rules get pasted in.
      this.nameContainer.addEventListener("paste",
        blurOnMultipleProperties(this.cssProperties));

      this.valueContainer.addEventListener("click", (event) => {
        // Clicks within the value shouldn't propagate any further.
        event.stopPropagation();

        // Forward clicks on valueContainer to the editable valueSpan
        if (event.target === this.valueContainer) {
          this.valueSpan.click();
        }
      });

      // The mousedown event could trigger a blur event on nameContainer, which
      // will trigger a call to the update function. The update function clears
      // valueSpan's markup. Thus the regular click event does not bubble up, and
      // listener's callbacks are not called.
      // So we need to remember where the user clicks in order to re-trigger the click
      // after the valueSpan's markup is re-populated. We only need to track this for
      // valueSpan's child elements, because direct click on valueSpan will always
      // trigger a click event.
      this.valueSpan.addEventListener("mousedown", (event) => {
        let clickedEl = event.target;
        if (clickedEl === this.valueSpan) {
          return;
        }
        this._hasPendingClick = true;

        let matchedSelector = ACTIONABLE_ELEMENTS_SELECTORS.find(
          (selector) => clickedEl.matches(selector));
        if (matchedSelector) {
          let similarElements = [...this.valueSpan.querySelectorAll(matchedSelector)];
          this._clickedElementOptions = {
            selector: matchedSelector,
            index: similarElements.indexOf(clickedEl)
          };
        }
      });

      this.valueSpan.addEventListener("mouseup", (event) => {
        this._clickedElementOptions = null;
        this._hasPendingClick = false;
      });

      this.valueSpan.addEventListener("click", (event) => {
        let target = event.target;

        if (target.nodeName === "a") {
          event.stopPropagation();
          event.preventDefault();
          let browserWin = this.ruleView.inspector.target.tab.ownerDocument.defaultView;
          browserWin.openTrustedLinkIn(target.href, "tab");
        }
      });

      editableField({
        start: this._onStartEditing,
        element: this.valueSpan,
        done: this._onValueDone,
        destroy: this.update,
        validate: this._onValidate,
        advanceChars: advanceValidate,
        contentType: InplaceEditor.CONTENT_TYPES.CSS_VALUE,
        property: this.prop,
        defaultIncrement: this.prop.name === "opacity" ? 0.1 : 1,
        popup: this.popup,
        multiline: true,
        maxWidth: () => this.container.getBoundingClientRect().width,
        cssProperties: this.cssProperties,
        cssVariables: this.rule.elementStyle.variables,
        getGridLineNames: this.getGridlineNames,
      });
    }
  },

  /**
   * Get the grid line names of the grid that the currently selected element is
   * contained in.
   *
   * @return {Object} Contains the names of the cols and rows as arrays
   * {cols: [], rows: []}.
   */
  getGridlineNames: async function() {
    let gridLineNames = {cols: [], rows: []};
    let layoutInspector = await this.ruleView.inspector.walker.getLayoutInspector();
    let gridFront = await layoutInspector.getCurrentGrid(
      this.ruleView.inspector.selection.nodeFront);

    if (gridFront) {
      let gridFragments = gridFront.gridFragments;

      for (let gridFragment of gridFragments) {
        for (let rowLine of gridFragment.rows.lines) {
          gridLineNames.rows = gridLineNames.rows.concat(rowLine.names);
        }
        for (let colLine of gridFragment.cols.lines) {
          gridLineNames.cols = gridLineNames.cols.concat(colLine.names);
        }
      }
    }

    // Emit message for test files
    this.ruleView.inspector.emit("grid-line-names-updated");
    return gridLineNames;
  },

  /**
   * Get the path from which to resolve requests for this
   * rule's stylesheet.
   *
   * @return {String} the stylesheet's href.
   */
  get sheetHref() {
    let domRule = this.rule.domRule;
    if (domRule) {
      return domRule.href || domRule.nodeHref;
    }
    return undefined;
  },

  /**
   * Populate the span based on changes to the TextProperty.
   */
  update: function() {
    if (this.ruleView.isDestroyed) {
      return;
    }

    this.updatePropertyState();

    let name = this.prop.name;
    this.nameSpan.textContent = name;

    // Combine the property's value and priority into one string for
    // the value.
    let store = this.rule.elementStyle.store;
    let val = store.userProperties.getProperty(this.rule.style, name,
                                               this.prop.value);
    if (this.prop.priority) {
      val += " !" + this.prop.priority;
    }

    let propDirty = store.userProperties.contains(this.rule.style, name);

    if (propDirty) {
      this.element.setAttribute("dirty", "");
    } else {
      this.element.removeAttribute("dirty");
    }

    let outputParser = this.ruleView._outputParser;
    let parserOptions = {
      angleClass: "ruleview-angle",
      angleSwatchClass: SHARED_SWATCH_CLASS + " " + ANGLE_SWATCH_CLASS,
      bezierClass: "ruleview-bezier",
      bezierSwatchClass: SHARED_SWATCH_CLASS + " " + BEZIER_SWATCH_CLASS,
      colorClass: "ruleview-color",
      colorSwatchClass: SHARED_SWATCH_CLASS + " " + COLOR_SWATCH_CLASS,
      filterClass: "ruleview-filter",
      filterSwatchClass: SHARED_SWATCH_CLASS + " " + FILTER_SWATCH_CLASS,
      flexClass: "ruleview-flex",
      gridClass: "ruleview-grid",
      shapeClass: "ruleview-shape",
      shapeSwatchClass: SHARED_SWATCH_CLASS + " " + SHAPE_SWATCH_CLASS,
      defaultColorType: !propDirty,
      urlClass: "theme-link",
      fontFamilyClass: FONT_FAMILY_CLASS,
      baseURI: this.sheetHref,
      unmatchedVariableClass: "ruleview-unmatched-variable",
      matchedVariableClass: "ruleview-variable",
      isVariableInUse: varName => this.rule.elementStyle.getVariable(varName),
    };
    let frag = outputParser.parseCssProperty(name, val, parserOptions);
    this.valueSpan.innerHTML = "";
    this.valueSpan.appendChild(frag);

    this.ruleView.emit("property-value-updated", this.valueSpan);

    // Highlight the currently used font in font-family properties.
    // If we cannot find a match, highlight the first generic family instead.
    let fontFamilySpans = this.valueSpan.querySelectorAll("." + FONT_FAMILY_CLASS);
    if (fontFamilySpans.length && this.prop.enabled && !this.prop.overridden) {
      this.rule.elementStyle.getUsedFontFamilies().then(families => {
        const usedFontFamilies = families.map(font => font.toLowerCase());
        let foundMatchingFamily = false;
        let firstGenericSpan = null;

        for (let span of fontFamilySpans) {
          const authoredFont = span.textContent.toLowerCase();

          if (!firstGenericSpan && GENERIC_FONT_FAMILIES.includes(authoredFont)) {
            firstGenericSpan = span;
          }

          if (usedFontFamilies.includes(authoredFont)) {
            span.classList.add("used-font");
            foundMatchingFamily = true;
          }
        }

        if (!foundMatchingFamily && firstGenericSpan) {
          firstGenericSpan.classList.add("used-font");
        }

        this.ruleView.emit("font-highlighted", this.valueSpan);
      }).catch(e => console.error("Could not get the list of font families", e));
    }

    // Attach the color picker tooltip to the color swatches
    this._colorSwatchSpans =
      this.valueSpan.querySelectorAll("." + COLOR_SWATCH_CLASS);
    if (this.ruleEditor.isEditable) {
      for (let span of this._colorSwatchSpans) {
        // Adding this swatch to the list of swatches our colorpicker
        // knows about
        this.ruleView.tooltips.getTooltip("colorPicker").addSwatch(span, {
          onShow: this._onStartEditing,
          onPreview: this._onSwatchPreview,
          onCommit: this._onSwatchCommit,
          onRevert: this._onSwatchRevert
        });
        span.on("unit-change", this._onSwatchCommit);
        let title = l10n("rule.colorSwatch.tooltip");
        span.setAttribute("title", title);
        span.dataset.propertyName = this.nameSpan.textContent;
      }
    }

    // Attach the cubic-bezier tooltip to the bezier swatches
    this._bezierSwatchSpans =
      this.valueSpan.querySelectorAll("." + BEZIER_SWATCH_CLASS);
    if (this.ruleEditor.isEditable) {
      for (let span of this._bezierSwatchSpans) {
        // Adding this swatch to the list of swatches our colorpicker
        // knows about
        this.ruleView.tooltips.getTooltip("cubicBezier").addSwatch(span, {
          onShow: this._onStartEditing,
          onPreview: this._onSwatchPreview,
          onCommit: this._onSwatchCommit,
          onRevert: this._onSwatchRevert
        });
        let title = l10n("rule.bezierSwatch.tooltip");
        span.setAttribute("title", title);
      }
    }

    // Attach the filter editor tooltip to the filter swatch
    let span = this.valueSpan.querySelector("." + FILTER_SWATCH_CLASS);
    if (this.ruleEditor.isEditable) {
      if (span) {
        parserOptions.filterSwatch = true;

        this.ruleView.tooltips.getTooltip("filterEditor").addSwatch(span, {
          onShow: this._onStartEditing,
          onPreview: this._onSwatchPreview,
          onCommit: this._onSwatchCommit,
          onRevert: this._onSwatchRevert
        }, outputParser, parserOptions);
        let title = l10n("rule.filterSwatch.tooltip");
        span.setAttribute("title", title);
      }
    }

    this.angleSwatchSpans =
      this.valueSpan.querySelectorAll("." + ANGLE_SWATCH_CLASS);
    if (this.ruleEditor.isEditable) {
      for (let angleSpan of this.angleSwatchSpans) {
        angleSpan.on("unit-change", this._onSwatchCommit);
        let title = l10n("rule.angleSwatch.tooltip");
        angleSpan.setAttribute("title", title);
      }
    }

    let flexToggle = this.valueSpan.querySelector(".ruleview-flex");
    if (flexToggle) {
      flexToggle.setAttribute("title", l10n("rule.flexToggle.tooltip"));
      if (this.ruleView.highlighters.flexboxHighlighterShown ===
          this.ruleView.inspector.selection.nodeFront) {
        flexToggle.classList.add("active");
      }
    }

    let gridToggle = this.valueSpan.querySelector(".ruleview-grid");
    if (gridToggle) {
      gridToggle.setAttribute("title", l10n("rule.gridToggle.tooltip"));
      if (this.ruleView.highlighters.gridHighlighterShown ===
          this.ruleView.inspector.selection.nodeFront) {
        gridToggle.classList.add("active");
      }
    }

    let shapeToggle = this.valueSpan.querySelector(".ruleview-shapeswatch");
    if (shapeToggle) {
      let mode = "css" + name.split("-").map(s => {
        return s[0].toUpperCase() + s.slice(1);
      }).join("");
      shapeToggle.setAttribute("data-mode", mode);
    }

    // Now that we have updated the property's value, we might have a pending
    // click on the value container. If we do, we have to trigger a click event
    // on the right element.
    if (this._hasPendingClick) {
      this._hasPendingClick = false;
      let elToClick;

      if (this._clickedElementOptions !== null) {
        let {selector, index} = this._clickedElementOptions;
        elToClick = this.valueSpan.querySelectorAll(selector)[index];

        this._clickedElementOptions = null;
      }

      if (!elToClick) {
        elToClick = this.valueSpan;
      }
      elToClick.click();
    }

    // Populate the computed styles and shorthand overridden styles.
    this._updateComputed();
    this._updateShorthandOverridden();

    // Update the rule property highlight.
    this.ruleView._updatePropertyHighlight(this);
  },

  _onStartEditing: function() {
    this.element.classList.remove("ruleview-overridden");
    this.filterProperty.hidden = true;
    this.enable.style.visibility = "hidden";
    this.expander.style.display = "none";
  },

  /**
   * Update the visibility of the enable checkbox, the warning indicator and
   * the filter property, as well as the overridden state of the property.
   */
  updatePropertyState: function() {
    if (this.prop.enabled) {
      this.enable.style.removeProperty("visibility");
      this.enable.setAttribute("checked", "");
    } else {
      this.enable.style.visibility = "visible";
      this.enable.removeAttribute("checked");
    }

    this.warning.title = !this.isNameValid()
      ? l10n("rule.warningName.title")
      : l10n("rule.warning.title");

    this.warning.hidden = this.editing || this.isValid();
    this.filterProperty.hidden = this.editing ||
                                 !this.isValid() ||
                                 !this.prop.overridden ||
                                 this.ruleEditor.rule.isUnmatched;

    let showExpander = this.prop.computed.some(c => c.name !== this.prop.name);
    this.expander.style.display = showExpander ? "inline-block" : "none";

    if (!this.editing &&
        (this.prop.overridden || !this.prop.enabled ||
         !this.prop.isKnownProperty())) {
      this.element.classList.add("ruleview-overridden");
    } else {
      this.element.classList.remove("ruleview-overridden");
    }
  },

  /**
   * Update the indicator for computed styles. The computed styles themselves
   * are populated on demand, when they become visible.
   */
  _updateComputed: function() {
    this.computed.innerHTML = "";

    let showExpander = this.prop.computed.some(c => c.name !== this.prop.name);
    this.expander.style.display = !this.editing && showExpander ? "inline-block" : "none";

    this._populatedComputed = false;
    if (this.expander.hasAttribute("open")) {
      this._populateComputed();
    }
  },

  /**
   * Populate the list of computed styles.
   */
  _populateComputed: function() {
    if (this._populatedComputed) {
      return;
    }
    this._populatedComputed = true;

    for (let computed of this.prop.computed) {
      // Don't bother to duplicate information already
      // shown in the text property.
      if (computed.name === this.prop.name) {
        continue;
      }

      // Store the computed style element for easy access when highlighting
      // styles
      computed.element = this._createComputedListItem(this.computed, computed,
        "ruleview-computed");
    }
  },

  /**
   * Update the indicator for overridden shorthand styles. The shorthand
   * overridden styles themselves are populated on demand, when they
   * become visible.
   */
  _updateShorthandOverridden: function() {
    this.shorthandOverridden.innerHTML = "";

    this._populatedShorthandOverridden = false;
    this._populateShorthandOverridden();
  },

  /**
   * Populate the list of overridden shorthand styles.
   */
  _populateShorthandOverridden: function() {
    if (this._populatedShorthandOverridden || this.prop.overridden) {
      return;
    }
    this._populatedShorthandOverridden = true;

    for (let computed of this.prop.computed) {
      // Don't display duplicate information or show properties
      // that are completely overridden.
      if (computed.name === this.prop.name || !computed.overridden) {
        continue;
      }

      this._createComputedListItem(this.shorthandOverridden, computed,
        "ruleview-overridden-item");
    }
  },

  /**
   * Creates and populates a list item with the computed CSS property.
   */
  _createComputedListItem: function(parentEl, computed, className) {
    let li = createChild(parentEl, "li", {
      class: className
    });

    if (computed.overridden) {
      li.classList.add("ruleview-overridden");
    }

    createChild(li, "span", {
      class: "ruleview-propertyname theme-fg-color5",
      textContent: computed.name
    });
    appendText(li, ": ");

    let outputParser = this.ruleView._outputParser;
    let frag = outputParser.parseCssProperty(
      computed.name, computed.value, {
        colorSwatchClass: "ruleview-swatch ruleview-colorswatch",
        urlClass: "theme-link",
        baseURI: this.sheetHref,
        fontFamilyClass: "ruleview-font-family"
      }
    );

    // Store the computed property value that was parsed for output
    computed.parsedValue = frag.textContent;

    createChild(li, "span", {
      class: "ruleview-propertyvalue theme-fg-color1",
      child: frag
    });

    appendText(li, ";");

    return li;
  },

  /**
   * Handles clicks on the disabled property.
   */
  _onEnableClicked: function(event) {
    let checked = this.enable.hasAttribute("checked");
    if (checked) {
      this.enable.removeAttribute("checked");
    } else {
      this.enable.setAttribute("checked", "");
    }
    this.prop.setEnabled(!checked);
    event.stopPropagation();
  },

  /**
   * Handles clicks on the computed property expander. If the computed list is
   * open due to user expanding or style filtering, collapse the computed list
   * and close the expander. Otherwise, add user-open attribute which is used to
   * expand the computed list and tracks whether or not the computed list is
   * expanded by manually by the user.
   */
  _onExpandClicked: function(event) {
    if (this.computed.hasAttribute("filter-open") ||
        this.computed.hasAttribute("user-open")) {
      this.expander.removeAttribute("open");
      this.computed.removeAttribute("filter-open");
      this.computed.removeAttribute("user-open");
      this.shorthandOverridden.removeAttribute("hidden");
      this._populateShorthandOverridden();
    } else {
      this.expander.setAttribute("open", "true");
      this.computed.setAttribute("user-open", "");
      this.shorthandOverridden.setAttribute("hidden", "true");
      this._populateComputed();
    }

    event.stopPropagation();
  },

  /**
   * Expands the computed list when a computed property is matched by the style
   * filtering. The filter-open attribute is used to track whether or not the
   * computed list was toggled opened by the filter.
   */
  expandForFilter: function() {
    if (!this.computed.hasAttribute("user-open")) {
      this.expander.setAttribute("open", "true");
      this.computed.setAttribute("filter-open", "");
      this._populateComputed();
    }
  },

  /**
   * Collapses the computed list that was expanded by style filtering.
   */
  collapseForFilter: function() {
    this.computed.removeAttribute("filter-open");

    if (!this.computed.hasAttribute("user-open")) {
      this.expander.removeAttribute("open");
    }
  },

  /**
   * Called when the property name's inplace editor is closed.
   * Ignores the change if the user pressed escape, otherwise
   * commits it.
   *
   * @param {String} value
   *        The value contained in the editor.
   * @param {Boolean} commit
   *        True if the change should be applied.
   * @param {Number} direction
   *        The move focus direction number.
   */
  _onNameDone: function(value, commit, direction) {
    let isNameUnchanged = (!commit && !this.ruleEditor.isEditing) ||
                          this.committed.name === value;
    if (this.prop.value && isNameUnchanged) {
      return;
    }

    // Remove a property if the name is empty
    if (!value.trim()) {
      this.remove(direction);
      return;
    }

    // Remove a property if the property value is empty and the property
    // value is not about to be focused
    if (!this.prop.value &&
        direction !== Services.focus.MOVEFOCUS_FORWARD) {
      this.remove(direction);
      return;
    }

    // Adding multiple rules inside of name field overwrites the current
    // property with the first, then adds any more onto the property list.
    let properties = parseDeclarations(this.cssProperties.isKnown, value);

    if (properties.length) {
      this.prop.setName(properties[0].name);
      this.committed.name = this.prop.name;

      if (!this.prop.enabled) {
        this.prop.setEnabled(true);
      }

      if (properties.length > 1) {
        this.prop.setValue(properties[0].value, properties[0].priority);
        this.ruleEditor.addProperties(properties.slice(1), this.prop);
      }
    }
  },

  /**
   * Remove property from style and the editors from DOM.
   * Begin editing next or previous available property given the focus
   * direction.
   *
   * @param {Number} direction
   *        The move focus direction number.
   */
  remove: function(direction) {
    if (this._colorSwatchSpans && this._colorSwatchSpans.length) {
      for (let span of this._colorSwatchSpans) {
        this.ruleView.tooltips.getTooltip("colorPicker").removeSwatch(span);
        span.off("unit-change", this._onSwatchCommit);
      }
    }

    if (this.angleSwatchSpans && this.angleSwatchSpans.length) {
      for (let span of this.angleSwatchSpans) {
        span.off("unit-change", this._onSwatchCommit);
      }
    }

    this.element.remove();
    this.ruleEditor.rule.editClosestTextProperty(this.prop, direction);
    this.nameSpan.textProperty = null;
    this.valueSpan.textProperty = null;
    this.prop.remove();
  },

  /**
   * Called when a value editor closes.  If the user pressed escape,
   * revert to the value this property had before editing.
   *
   * @param {String} value
   *        The value contained in the editor.
   * @param {Boolean} commit
   *        True if the change should be applied.
   * @param {Number} direction
   *        The move focus direction number.
   */
  _onValueDone: function(value = "", commit, direction) {
    let parsedProperties = this._getValueAndExtraProperties(value);
    let val = parseSingleValue(this.cssProperties.isKnown,
                               parsedProperties.firstValue);
    let isValueUnchanged = (!commit && !this.ruleEditor.isEditing) ||
                           !parsedProperties.propertiesToAdd.length &&
                           this.committed.value === val.value &&
                           this.committed.priority === val.priority;

    // If the value is not empty and unchanged, revert the property back to
    // its original value and enabled or disabled state
    if (value.trim() && isValueUnchanged) {
      this.ruleEditor.rule.previewPropertyValue(this.prop, val.value,
                                                val.priority);
      this.rule.setPropertyEnabled(this.prop, this.prop.enabled);
      return;
    }

    // Since the value was changed, check if the original propertywas a flex or grid
    // display declaration and hide their respective highlighters.
    if (this.isDisplayFlex()) {
      this.ruleView.highlighters.hideFlexboxHighlighter();
    }

    if (this.isDisplayGrid()) {
      this.ruleView.highlighters.hideGridHighlighter();
    }

    // First, set this property value (common case, only modified a property)
    this.prop.setValue(val.value, val.priority);

    if (!this.prop.enabled) {
      this.prop.setEnabled(true);
    }

    this.committed.value = this.prop.value;
    this.committed.priority = this.prop.priority;

    // If needed, add any new properties after this.prop.
    this.ruleEditor.addProperties(parsedProperties.propertiesToAdd, this.prop);

    // If the input value is empty and the focus is moving forward to the next
    // editable field, then remove the whole property.
    // A timeout is used here to accurately check the state, since the inplace
    // editor `done` and `destroy` events fire before the next editor
    // is focused.
    if (!value.trim() && direction !== Services.focus.MOVEFOCUS_BACKWARD) {
      setTimeout(() => {
        if (!this.editing) {
          this.remove(direction);
        }
      }, 0);
    }
  },

  /**
   * Called when the swatch editor wants to commit a value change.
   */
  _onSwatchCommit: function() {
    this._onValueDone(this.valueSpan.textContent, true);
    this.update();
  },

  /**
   * Called when the swatch editor wants to preview a value change.
   */
  _onSwatchPreview: function() {
    this._previewValue(this.valueSpan.textContent);
  },

  /**
   * Called when the swatch editor closes from an ESC. Revert to the original
   * value of this property before editing.
   */
  _onSwatchRevert: function() {
    this._previewValue(this.prop.value, true);
    this.update();
  },

  /**
   * Parse a value string and break it into pieces, starting with the
   * first value, and into an array of additional properties (if any).
   *
   * Example: Calling with "red; width: 100px" would return
   * { firstValue: "red", propertiesToAdd: [{ name: "width", value: "100px" }] }
   *
   * @param {String} value
   *        The string to parse
   * @return {Object} An object with the following properties:
   *        firstValue: A string containing a simple value, like
   *                    "red" or "100px!important"
   *        propertiesToAdd: An array with additional properties, following the
   *                         parseDeclarations format of {name,value,priority}
   */
  _getValueAndExtraProperties: function(value) {
    // The inplace editor will prevent manual typing of multiple properties,
    // but we need to deal with the case during a paste event.
    // Adding multiple properties inside of value editor sets value with the
    // first, then adds any more onto the property list (below this property).
    let firstValue = value;
    let propertiesToAdd = [];

    let properties = parseDeclarations(this.cssProperties.isKnown, value);

    // Check to see if the input string can be parsed as multiple properties
    if (properties.length) {
      // Get the first property value (if any), and any remaining
      // properties (if any)
      if (!properties[0].name && properties[0].value) {
        firstValue = properties[0].value;
        propertiesToAdd = properties.slice(1);
      } else if (properties[0].name && properties[0].value) {
        // In some cases, the value could be a property:value pair
        // itself.  Join them as one value string and append
        // potentially following properties
        firstValue = properties[0].name + ": " + properties[0].value;
        propertiesToAdd = properties.slice(1);
      }
    }

    return {
      propertiesToAdd: propertiesToAdd,
      firstValue: firstValue
    };
  },

  /**
   * Live preview this property, without committing changes.
   *
   * @param {String} value
   *        The value to set the current property to.
   * @param {Boolean} reverting
   *        True if we're reverting the previously previewed value
   */
  _previewValue: function(value, reverting = false) {
    // Since function call is debounced, we need to make sure we are still
    // editing, and any selector modifications have been completed
    if (!reverting && (!this.editing || this.ruleEditor.isEditing)) {
      return;
    }

    let val = parseSingleValue(this.cssProperties.isKnown, value);
    this.ruleEditor.rule.previewPropertyValue(this.prop, val.value,
                                              val.priority);
  },

  /**
   * Validate this property. Does it make sense for this value to be assigned
   * to this property name? This does not apply the property value
   *
   * @return {Boolean} true if the property name + value pair is valid, false otherwise.
   */
  isValid: function() {
    return this.prop.isValid();
  },

  /**
   * Validate the name of this property.
   * @return {Boolean} true if the property name is valid, false otherwise.
   */
  isNameValid: function() {
    return this.prop.isNameValid();
  },

  /**
   * Returns true if the property is a `display: [inline-]flex` declaration.
   *
   * @return {Boolean} true if the property is a `display: [inline-]flex` declaration.
   */
  isDisplayFlex: function() {
    return this.prop.name === "display" &&
      (this.prop.value === "flex" || this.prop.value === "inline-flex");
  },

  /**
   * Returns true if the property is a `display: [inline-]grid` declaration.
   *
   * @return {Boolean} true if the property is a `display: [inline-]grid` declaration.
   */
  isDisplayGrid: function() {
    return this.prop.name === "display" &&
      (this.prop.value === "grid" || this.prop.value === "inline-grid");
  },
};

module.exports = TextPropertyEditor;
