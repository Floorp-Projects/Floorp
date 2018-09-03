/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const { gDevTools } = require("devtools/client/framework/devtools");
const { getColor } = require("devtools/client/shared/theme");
const { createFactory, createElement } = require("devtools/client/shared/vendor/react");
const { getCssProperties } = require("devtools/shared/fronts/css-properties");
const { Provider } = require("devtools/client/shared/vendor/react-redux");
const { debounce } = require("devtools/shared/debounce");
const { ELEMENT_STYLE } = require("devtools/shared/specs/styles");

const FontsApp = createFactory(require("./components/FontsApp"));

const { LocalizationHelper } = require("devtools/shared/l10n");
const INSPECTOR_L10N =
  new LocalizationHelper("devtools/client/locales/inspector.properties");

const { getStr } = require("./utils/l10n");
const { parseFontVariationAxes } = require("./utils/font-utils");
const { updateFonts } = require("./actions/fonts");
const {
  applyInstance,
  resetFontEditor,
  updateAxis,
  updateCustomInstance,
  updateFontEditor,
  updateFontProperty,
  updateWarningMessage,
} = require("./actions/font-editor");
const { updatePreviewText } = require("./actions/font-options");

const CUSTOM_INSTANCE_NAME = getStr("fontinspector.customInstanceName");
const FONT_PROPERTIES = [
  "font-family",
  "font-optical-sizing",
  "font-size",
  "font-stretch",
  "font-style",
  "font-variation-settings",
  "font-weight",
  "line-height",
];
const PREF_FONT_EDITOR = "devtools.inspector.fonteditor.enabled";
const REGISTERED_AXES_TO_FONT_PROPERTIES = {
  "ital": "font-style",
  "opsz": "font-optical-sizing",
  "slnt": "font-style",
  "wdth": "font-stretch",
  "wght": "font-weight",
};
const REGISTERED_AXES = Object.keys(REGISTERED_AXES_TO_FONT_PROPERTIES);

const HISTOGRAM_N_FONT_AXES = "DEVTOOLS_FONTEDITOR_N_FONT_AXES";
const HISTOGRAM_N_FONTS_RENDERED = "DEVTOOLS_FONTEDITOR_N_FONTS_RENDERED";
const HISTOGRAM_FONT_TYPE_DISPLAYED = "DEVTOOLS_FONTEDITOR_FONT_TYPE_DISPLAYED";

class FontInspector {
  constructor(inspector, window) {
    this.cssProperties = getCssProperties(inspector.toolbox);
    this.document = window.document;
    this.inspector = inspector;
    // Set of unique keyword values supported by designated font properties.
    this.keywordValues = new Set(this.getFontPropertyValueKeywords());
    this.nodeComputedStyle = {};
    this.pageStyle = this.inspector.pageStyle;
    this.ruleViewTool = this.inspector.getPanel("ruleview");
    this.ruleView = this.ruleViewTool.view;
    this.selectedRule = null;
    this.store = this.inspector.store;
    // Map CSS property names and variable font axis names to methods that write their
    // corresponding values to the appropriate TextProperty from the Rule view.
    // Values of variable font registered axes may be written to CSS font properties under
    // certain cascade circumstances and platform support. @see `getWriterForAxis(axis)`
    this.writers = new Map();

    this.snapshotChanges = debounce(this.snapshotChanges, 100, this);
    this.syncChanges = debounce(this.syncChanges, 100, this);
    this.onInstanceChange = this.onInstanceChange.bind(this);
    this.onNewNode = this.onNewNode.bind(this);
    this.onPreviewTextChange = debounce(this.onPreviewTextChange, 100, this);
    this.onPropertyChange = this.onPropertyChange.bind(this);
    this.onRulePropertyUpdated = debounce(this.onRulePropertyUpdated, 100, this);
    this.onToggleFontHighlight = this.onToggleFontHighlight.bind(this);
    this.onThemeChanged = this.onThemeChanged.bind(this);
    this.update = this.update.bind(this);
    this.updateFontVariationSettings = this.updateFontVariationSettings.bind(this);

    this.init();
  }

  init() {
    if (!this.inspector) {
      return;
    }

    const fontsApp = FontsApp({
      fontEditorEnabled: Services.prefs.getBoolPref(PREF_FONT_EDITOR),
      onInstanceChange: this.onInstanceChange,
      onToggleFontHighlight: this.onToggleFontHighlight,
      onPreviewTextChange: this.onPreviewTextChange,
      onPropertyChange: this.onPropertyChange,
    });

    const provider = createElement(Provider, {
      id: "fontinspector",
      key: "fontinspector",
      store: this.store,
      title: INSPECTOR_L10N.getStr("inspector.sidebar.fontInspectorTitle"),
    }, fontsApp);

    // Expose the provider to let inspector.js use it in setupSidebar.
    this.provider = provider;

    this.inspector.selection.on("new-node-front", this.onNewNode);
    // @see ToolSidebar.onSidebarTabSelected()
    this.inspector.sidebar.on("fontinspector-selected", this.onNewNode);

    // Listen for theme changes as the color of the previews depend on the theme
    gDevTools.on("theme-switched", this.onThemeChanged);
  }

  /**
   * Convert a value for font-size between two CSS unit types.
   * Conversion is done via pixels. If neither of the two given unit types is "px",
   * recursively get the value in pixels, then convert that result to the desired unit.
   *
   * @param  {String} property
   *         Property name for the converted value.
   *         Assumed to be "font-size", but special case for "line-height".
   * @param  {Number} value
   *         Numeric value to convert.
   * @param  {String} fromUnit
   *         CSS unit to convert from.
   * @param  {String} toUnit
   *         CSS unit to convert to.
   * @return {Number}
   *         Converted numeric value.
   */
  async convertUnits(property, value, fromUnit, toUnit) {
    if (value !== parseFloat(value)) {
      throw TypeError(`Invalid value for conversion. Expected Number, got ${value}`);
    }

    if (fromUnit === toUnit || value === 0) {
      return value;
    }

    // Special case for line-height. Consider em and untiless to be equivalent.
    if (property === "line-height" &&
       (fromUnit === "" && toUnit === "em") || (fromUnit === "em" && toUnit === "")) {
      return value;
    }

    // If neither unit is in pixels, first convert the value to pixels.
    // Reassign input value and source CSS unit.
    if (toUnit !== "px" && fromUnit !== "px") {
      value = await this.convertUnits(property, value, fromUnit, "px");
      fromUnit = "px";
    }

    // Whether the conversion is done from pixels.
    const fromPx = fromUnit === "px";
    // Determine the target CSS unit for conversion.
    const unit = toUnit === "px" ? fromUnit : toUnit;
    // NodeFront instance of selected element.
    const node = this.inspector.selection.nodeFront;
    // Reference node based on which to convert relative sizes like "em" and "%".
    const referenceNode = (property === "line-height") ? node : node.parentNode();
    // Default output value to input value for a 1-to-1 conversion as a guard against
    // unrecognized CSS units. It will not be correct, but it will also not break.
    let out = value;
    // Computed style for reference node used for conversion of "em", "rem", "%".
    let computedStyle;

    if (unit === "in") {
      out = fromPx
        ? value / 96
        : value * 96;
    }

    if (unit === "cm") {
      out = fromPx
        ? value * 0.02645833333
        : value / 0.02645833333;
    }

    if (unit === "mm") {
      out = fromPx
        ? value * 0.26458333333
        : value / 0.26458333333;
    }

    if (unit === "pt") {
      out = fromPx
        ? value * 0.75
        : value / 0.75;
    }

    if (unit === "pc") {
      out = fromPx
        ? value * 0.0625
        : value / 0.0625;
    }

    if (unit === "%") {
      computedStyle =
        await this.pageStyle.getComputed(referenceNode).catch(console.error);

      if (!computedStyle) {
        return value;
      }

      out = fromPx
        ? value * 100 / parseFloat(computedStyle["font-size"].value)
        : value / 100 * parseFloat(computedStyle["font-size"].value);
    }

    // Special handling for unitless line-height.
    if (unit === "em" || (unit === "" && property === "line-height")) {
      computedStyle =
        await this.pageStyle.getComputed(referenceNode).catch(console.error);

      if (!computedStyle) {
        return value;
      }

      out = fromPx
        ? value / parseFloat(computedStyle["font-size"].value)
        : value * parseFloat(computedStyle["font-size"].value);
    }

    if (unit === "rem") {
      const document = await this.inspector.walker.documentElement();
      computedStyle = await this.pageStyle.getComputed(document).catch(console.error);

      if (!computedStyle) {
        return value;
      }

      out = fromPx
        ? value / parseFloat(computedStyle["font-size"].value)
        : value * parseFloat(computedStyle["font-size"].value);
    }

    if (unit === "vh" || unit === "vw" || unit === "vmin" || unit === "vmax") {
      const dim = await node.getOwnerGlobalDimensions();

      // The getOwnerGlobalDimensions() method does not exist on the NodeFront API spec
      // prior to Firefox 63. In that case, return a 1-to-1 conversion which isn't a
      // correct conversion, but doesn't break the font editor either.
      if (!dim || !dim.innerWidth || !dim.innerHeight) {
        out = value;
      } else if (unit === "vh") {
        out = fromPx
          ? value * 100 / dim.innerHeight
          : value / 100 * dim.innerHeight;
      } else if (unit === "vw") {
        out = fromPx
          ? value * 100 / dim.innerWidth
          : value / 100 * dim.innerWidth;
      } else if (unit === "vmin") {
        out = fromPx
          ? value * 100 / Math.min(dim.innerWidth, dim.innerHeight)
          : value / 100 * Math.min(dim.innerWidth, dim.innerHeight);
      } else if (unit === "vmax") {
        out = fromPx
          ? value * 100 / Math.max(dim.innerWidth, dim.innerHeight)
          : value / 100 * Math.max(dim.innerWidth, dim.innerHeight);
      }
    }

    // Catch any NaN or Infinity as result of dividing by zero in any
    // of the relative unit conversions which rely on external values.
    if (isNaN(out) || Math.abs(out) === Infinity) {
      out = 0;
    }

    // Return rounded pixel values. Limit other values to 3 decimals.
    if (fromPx) {
      // Round values like 1.000 to 1
      return out === Math.round(out) ? Math.round(out) : out.toFixed(3);
    }

    return Math.round(out);
  }

  /**
   * Destruction function called when the inspector is destroyed. Removes event listeners
   * and cleans up references.
   */
  destroy() {
    this.inspector.selection.off("new-node-front", this.onNewNode);
    this.inspector.sidebar.off("fontinspector-selected", this.onNewNode);
    this.ruleView.off("property-value-updated", this.onRulePropertyUpdated);
    gDevTools.off("theme-switched", this.onThemeChanged);

    this.document = null;
    this.inspector = null;
    this.nodeComputedStyle = {};
    this.pageStyle = null;
    this.ruleView = null;
    this.selectedRule = null;
    this.store = null;
    this.writers.clear();
    this.writers = null;
  }

  /**
   * Get all expected CSS font properties and values from the node's matching rules and
   * fallback to computed style. Skip CSS Custom Properties, `calc()` and keyword values.
   *
   * @return {Object}
   */
  async getFontProperties() {
    const properties = {};

    // First, get all expected font properties from computed styles, if available.
    for (const prop of FONT_PROPERTIES) {
      properties[prop] =
        (this.nodeComputedStyle[prop] && this.nodeComputedStyle[prop].value)
          ? this.nodeComputedStyle[prop].value
          : "";
    }

    // Convert computed value for line-height from pixels to unitless.
    // If it is not overwritten by an explicit line-height CSS declaration,
    // this will be the implicit value shown in the editor.

    properties["line-height"] =
      await this.convertUnits("line-height", parseFloat(properties["line-height"]),
                              "px", "");

    // Then, replace with enabled font properties found on any of the rules that apply.
    for (const rule of this.ruleView.rules) {
      if (rule.inherited) {
        continue;
      }

      for (const textProp of rule.textProps) {
        if (FONT_PROPERTIES.includes(textProp.name) &&
            !this.keywordValues.has(textProp.value) &&
            !textProp.value.includes("calc(") &&
            !textProp.value.includes("var(") &&
            !textProp.overridden &&
            textProp.enabled) {
          properties[textProp.name] = textProp.value;
        }
      }
    }

    return properties;
  }

  /**
   * Get an array of keyword values supported by the following CSS properties:
   * - font-size
   * - font-weight
   * - font-stretch
   * - line-height
   *
   * This list is used to filter out values when reading CSS font properties from rules.
   * Computed styles will be used instead of any of these values.
   *
   * @return {Array}
   */
  getFontPropertyValueKeywords() {
    return [
      "font-size",
      "font-weight",
      "font-stretch",
      "line-height"
    ].reduce((acc, property) => {
      return acc.concat(this.cssProperties.getValues(property));
    }, []);
  }

  async getFontsForNode(node, options) {
    // In case we've been destroyed in the meantime
    if (!this.document) {
      return [];
    }

    const fonts =
      await this.pageStyle.getUsedFontFaces(node, options).catch(console.error);
    if (!fonts) {
      return [];
    }

    return fonts;
  }

  async getAllFonts(options) {
    // In case we've been destroyed in the meantime
    if (!this.document) {
      return [];
    }

    let allFonts = await this.pageStyle.getAllUsedFontFaces(options).catch(console.error);
    if (!allFonts) {
      allFonts = [];
    }

    return allFonts;
  }

  /**
   * Get a reference to a TextProperty instance from the current selected rule for a
   * given property name.
   *
   * @param {String} name
   *        CSS property name
   * @return {TextProperty|null}
   */
  getTextProperty(name) {
    if (!this.selectedRule) {
      return null;
    }

    return this.selectedRule.textProps.find(prop =>
      prop.name === name && prop.enabled && !prop.overridden
    );
  }

  /**
   * Given the axis name of a registered axis, return a method which updates the
   * corresponding CSS font property when called with a value.
   *
   * All variable font axes can be written in the value of the "font-variation-settings"
   * CSS font property. In CSS Fonts Level 4, registered axes values can be used as
   * values of font properties, like "font-weight", "font-stretch" and "font-style".
   *
   * Axes declared in "font-variation-settings", either on the rule or inherited,
   * overwrite any corresponding font properties. Updates to these axes must be written
   * to "font-variation-settings" to preserve the cascade. Authors are discouraged from
   * using this practice. Whenever possible, registered axes values should be written to
   * their corresponding font properties.
   *
   * Registered axis name to font property mapping:
   *  - wdth -> font-stretch
   *  - wght -> font-weight
   *  - opsz -> font-optical-sizing
   *  - slnt -> font-style
   *  - ital -> font-style
   *
   * @param {String} axis
   *        Name of registered axis.
   * @return {Function}
   *         Method to call which updates the corresponding CSS font property.
   */
  getWriterForAxis(axis) {
    // Find any declaration of "font-variation-setttings".
    const FVSComputedStyle = this.nodeComputedStyle["font-variation-settings"];

    // If "font-variation-settings" CSS property is defined (on the rule or inherited)
    // and contains a declaration for the given registered axis, write to it.
    if (FVSComputedStyle && FVSComputedStyle.value.includes(axis)) {
      return this.updateFontVariationSettings;
    }

    // Get corresponding CSS font property value for registered axis.
    const property = REGISTERED_AXES_TO_FONT_PROPERTIES[axis];

    return (value) => {
      let condition = false;

      switch (axis) {
        case "wght":
          // Whether the page supports values of font-weight from CSS Fonts Level 4.
          condition = this.pageStyle.supportsFontWeightLevel4;
          break;

        case "wdth":
          // font-stretch in CSS Fonts Level 4 accepts percentage units.
          value = `${value}%`;
          // Whether the page supports values of font-stretch from CSS Fonts Level 4.
          condition = this.pageStyle.supportsFontStretchLevel4;
          break;

        case "slnt":
          // font-style in CSS Fonts Level 4 accepts an angle value.
          value = `oblique ${value}deg`;
          // Whether the page supports values of font-style from CSS Fonts Level 4.
          condition = this.pageStyle.supportsFontStyleLevel4;
          break;
      }

      if (condition) {
        this.updatePropertyValue(property, value);
      } else {
        // Replace the writer method for this axis so it won't get called next time.
        this.writers.set(axis, this.updateFontVariationSettings);
        // Fall back to writing to font-variation-settings together with all other axes.
        this.updateFontVariationSettings();
      }
    };
  }

  /**
   * Given a CSS property name or axis name of a variable font, return a method which
   * updates the corresponding CSS font property when called with a value.
   *
   * This is used to distinguish between CSS font properties, registered axes and
   * custom axes. Registered axes, like "wght" and "wdth", should be written to
   * corresponding CSS properties, like "font-weight" and "font-stretch".
   *
   * Unrecognized names (which aren't font property names or registered axes names) are
   * considered to be custom axes names and will be written to the
   * "font-variation-settings" CSS property.
   *
   * @param {String} name
   *        CSS property name or axis name.
   * @return {Function}
   *         Method which updates the rule view and page style.
   */
  getWriterForProperty(name) {
    if (this.writers.has(name)) {
      return this.writers.get(name);
    }

    if (REGISTERED_AXES.includes(name)) {
      this.writers.set(name, this.getWriterForAxis(name));
    } else if (FONT_PROPERTIES.includes(name)) {
      this.writers.set(name, (value) => {
        this.updatePropertyValue(name, value);
      });
    } else {
      this.writers.set(name, this.updateFontVariationSettings);
    }

    return this.writers.get(name);
  }

  /**
   * Check if the font inspector panel is visible.
   *
   * @return {Boolean}
   */
  isPanelVisible() {
    return this.inspector &&
           this.inspector.sidebar &&
           this.inspector.sidebar.getCurrentTabID() === "fontinspector";
  }
  /**
   * Check if a selected node exists and fonts can apply to it.
   *
   * @return {Boolean}
   */
  isSelectedNodeValid() {
    return this.inspector &&
           this.inspector.selection.nodeFront &&
           this.inspector.selection.isConnected() &&
           this.inspector.selection.isElementNode() &&
           !this.inspector.selection.isPseudoElementNode();
  }

  /**
   * Upon a new node selection, log some interesting telemetry probes.
   */
  logTelemetryProbesOnNewNode() {
    const { fontData, fontEditor } = this.store.getState();
    const { telemetry } = this.inspector;

    // Log the number of font faces used to render content of the element.
    const nbOfFontsRendered = fontData.fonts.length;
    if (nbOfFontsRendered) {
      telemetry.getHistogramById(HISTOGRAM_N_FONTS_RENDERED).add(nbOfFontsRendered);
    }

    // Log data about the currently edited font (if any).
    // Note that the edited font is always the first one from the fontEditor.fonts array.
    const editedFont = fontEditor.fonts[0];
    if (!editedFont) {
      return;
    }

    const nbOfAxes = editedFont.variationAxes ? editedFont.variationAxes.length : 0;
    telemetry.getHistogramById(HISTOGRAM_FONT_TYPE_DISPLAYED).add(
      !nbOfAxes ? "nonvariable" : "variable");
    if (nbOfAxes) {
      telemetry.getHistogramById(HISTOGRAM_N_FONT_AXES).add(nbOfAxes);
    }
  }

  /**
   * Sync the Rule view with the latest styles from the page. Called in a debounced way
   * (see constructor) after property changes are applied directly to the CSS style rule
   * on the page circumventing direct TextProperty.setValue() which triggers expensive DOM
   * operations in TextPropertyEditor.update().
   *
   * @param  {String} name
   *         CSS property name
   * @param  {String} value
   *         CSS property value
   */
  syncChanges(name, value) {
    const textProperty = this.getTextProperty(name, value);
    if (textProperty) {
      // This method may be called after the connection to the page style actor is closed.
      // For example, during teardown of automated tests. Here, we catch any failure that
      // may occur because of that. We're not interested in handling the error.
      textProperty.setValue(value).catch(error => {
        if (!this.document) {
          return;
        }

        throw error;
      });
    }

    this.ruleView.on("property-value-updated", this.onRulePropertyUpdated);
  }

  /**
   * Handler for changes of a font axis value coming from the FontEditor.
   *
   * @param  {String} tag
   *         Tag name of the font axis.
   * @param  {String} value
   *         Value of the font axis.
   */
  onAxisUpdate(tag, value) {
    this.store.dispatch(updateAxis(tag, value));
    this.store.dispatch(applyInstance(CUSTOM_INSTANCE_NAME, null));
    this.snapshotChanges();

    const writer = this.getWriterForProperty(tag);
    writer(value);
  }

  /**
   * Handler for changes of a CSS font property value coming from the FontEditor.
   *
   * @param  {String} property
   *         CSS font property name.
   * @param  {String} value
   *         CSS font property numeric value.
   * @param  {String|null} unit
   *         CSS unit or null
   */
  onFontPropertyUpdate(property, value, unit) {
    value = (unit !== null) ? value + unit : value;
    this.store.dispatch(updateFontProperty(property, value));
    const writer = this.getWriterForProperty(property);
    writer(value);
  }

  /**
   * Handler for selecting a font variation instance. Dispatches an action which updates
   * the axes and their values as defined by that variation instance.
   *
   * @param {String} name
   *        Name of variation instance. (ex: Light, Regular, Ultrabold, etc.)
   * @param {Array} values
   *        Array of objects with axes and values defined by the variation instance.
   */
  onInstanceChange(name, values) {
    this.store.dispatch(applyInstance(name, values));
    let writer;
    values.map(obj => {
      writer = this.getWriterForProperty(obj.axis);
      writer(obj.value.toString());
    });
  }

  /**
   * Selection 'new-node' event handler.
   */
  onNewNode() {
    this.ruleView.off("property-value-updated", this.onRulePropertyUpdated);
    if (this.isPanelVisible()) {
      Promise.all([this.update(), this.refreshFontEditor()]).then(() => {
        this.logTelemetryProbesOnNewNode();
      }).catch(e => console.error(e));
    }
  }

  /**
   * Handler for change in preview input.
   */
  onPreviewTextChange(value) {
    this.store.dispatch(updatePreviewText(value));
    this.update();
  }

  /**
   * Handler for changes to any CSS font property value or variable font axis value coming
   * from the Font Editor. This handler calls the appropriate method to preview the
   * changes on the page and update the store.
   *
   * If the property parameter is not a recognized CSS font property name, assume it's a
   * variable font axis name.
   *
   * @param  {String} property
   *         CSS font property name or axis name
   * @param  {String|Number} value
   *         CSS font property value or axis value
   * @param  {String|undefined} fromUnit
   *         Optional CSS unit to convert from
   * @param  {String|undefined} toUnit
   *         Optional CSS unit to convert to
   */
  async onPropertyChange(property, value, fromUnit, toUnit) {
    if (FONT_PROPERTIES.includes(property)) {
      let unit = fromUnit;

      // Strict checks because "line-height" value may be unitless (empty string).
      if (toUnit !== undefined && fromUnit !== undefined) {
        value = await this.convertUnits(property, value, fromUnit, toUnit);
        unit = toUnit;
      }

      // Cast value to string.
      this.onFontPropertyUpdate(property, value + "", unit);
    } else {
      // Cast axis value to string.
      this.onAxisUpdate(property, value + "");
    }
  }

  /**
   * Handler for "property-value-updated" event emitted from the rule view whenever a
   * property value changes. Ignore changes to properties unrelated to the font editor.
   *
   * @param {Object} eventData
   *        Object with the property name and value and origin rule.
   *        Example: { name: "font-size", value: "1em", rule: Object }
   */
  async onRulePropertyUpdated(eventData) {
    if (!this.selectedRule || !FONT_PROPERTIES.includes(eventData.property)) {
      return;
    }

    if (this.isPanelVisible()) {
      await this.refreshFontEditor();
    }
  }

  /**
   * Reveal a font's usage in the page.
   *
   * @param  {String} font
   *         The name of the font to be revealed in the page.
   * @param  {Boolean} show
   *         Whether or not to reveal the font.
   * @param  {Boolean} isForCurrentElement
   *         Optional. Default `true`. Whether or not to restrict revealing the font
   *         just to the current element selection.
   */
  async onToggleFontHighlight(font, show, isForCurrentElement = true) {
    if (!this.fontsHighlighter) {
      try {
        this.fontsHighlighter = await this.inspector.toolbox.highlighterUtils
                                          .getHighlighterByType("FontsHighlighter");
      } catch (e) {
        // When connecting to an older server or when debugging a XUL document, the
        // FontsHighlighter won't be available. Silently fail here and prevent any future
        // calls to the function.
        this.onToggleFontHighlight = () => {};
        return;
      }
    }

    try {
      if (show) {
        const node = isForCurrentElement
                   ? this.inspector.selection.nodeFront
                   : this.inspector.walker.rootNode;

        await this.fontsHighlighter.show(node, {
          CSSFamilyName: font.CSSFamilyName,
          name: font.name,
        });
      } else {
        await this.fontsHighlighter.hide();
      }
    } catch (e) {
      // Silently handle protocol errors here, because these might be called during
      // shutdown of the browser or devtools, and we don't care if they fail.
    }
  }

  /**
   * Handler for the "theme-switched" event.
   */
  onThemeChanged(frame) {
    if (frame === this.document.defaultView) {
      this.update();
    }
  }

  /**
   * Update the state of the font editor with:
   * - the fonts which apply to the current node;
   * - the computed style CSS font properties of the current node.
   *
   * This method is called:
   * - during initial setup;
   * - when a new node is selected;
   * - when any property is changed in the Rule view.
   * For the latter case, we compare between the latest computed style font properties
   * and the ones already in the store to decide if to update the font editor state.
   */
  async refreshFontEditor() {
    // Early return if pref for font editor is not enabled.
    if (!Services.prefs.getBoolPref(PREF_FONT_EDITOR)) {
      return;
    }

    if (!this.store || !this.isSelectedNodeValid()) {
      if (this.inspector.selection.isPseudoElementNode()) {
        const noPseudoWarning = getStr("fontinspector.noPseduoWarning");
        this.store.dispatch(resetFontEditor());
        this.store.dispatch(updateWarningMessage(noPseudoWarning));
        return;
      }

      // If the selection is a TextNode, switch selection to be its parent node.
      if (this.inspector.selection.isTextNode()) {
        const selection = this.inspector.selection;
        selection.setNodeFront(selection.nodeFront.parentNode());
        return;
      }

      this.store.dispatch(resetFontEditor());
      return;
    }

    const options = {};
    if (this.pageStyle.supportsFontVariations) {
      options.includeVariations = true;
    }

    const node = this.inspector.selection.nodeFront;
    const fonts = await this.getFontsForNode(node, options);

    try {
      // Get computed styles for the selected node, but filter by CSS font properties.
      this.nodeComputedStyle = await this.pageStyle.getComputed(node, {
        filterProperties: FONT_PROPERTIES
      });
    } catch (e) {
      // Because getComputed is async, there is a chance the font editor was
      // destroyed while we were waiting. If that happened, just bail out
      // silently.
      if (!this.document) {
        return;
      }

      throw e;
    }

    if (!this.nodeComputedStyle || !fonts.length) {
      this.store.dispatch(resetFontEditor());
      this.inspector.emit("fonteditor-updated");
      return;
    }

    // Clear any references to writer methods and CSS declarations because the node's
    // styles may have changed since the last font editor refresh.
    this.writers.clear();

    // If the Rule panel is not visible, the selected element's rule models may not have
    // been created yet. For example, in 2-pane mode when Fonts is opened as the default
    // panel. Select the current node to force the Rule view to create the rule models.
    if (!this.ruleViewTool.isSidebarActive()) {
      await this.ruleView.selectElement(node, false);
    }

    // Select the node's inline style as the rule where to write property value changes.
    this.selectedRule =
      this.ruleView.rules.find(rule => rule.domRule.type === ELEMENT_STYLE);

    const properties = await this.getFontProperties();
    // Assign writer methods to each axis defined in font-variation-settings.
    const axes = parseFontVariationAxes(properties["font-variation-settings"]);
    Object.keys(axes).map(axis => {
      this.writers.set(axis, this.getWriterForAxis(axis));
    });

    this.store.dispatch(updateFontEditor(fonts, properties, node.actorID));
    this.inspector.emit("fonteditor-updated");
    // Listen to manual changes in the Rule view that could update the Font Editor state
    this.ruleView.on("property-value-updated", this.onRulePropertyUpdated);
  }

  /**
   * Capture the state of all variation axes. Allows the user to return to this state with
   * the "Custom" instance after they've selected a font-defined named variation instance.
   * This method is debounced. See constructor.
   */
  snapshotChanges() {
    this.store.dispatch(updateCustomInstance());
  }

  async update() {
    // Stop refreshing if the inspector or store is already destroyed.
    if (!this.inspector || !this.store) {
      return;
    }

    let allFonts = [];
    let fonts = [];
    let otherFonts = [];

    if (!this.isSelectedNodeValid()) {
      this.store.dispatch(updateFonts(fonts, otherFonts, allFonts));
      return;
    }

    const { fontOptions } = this.store.getState();
    const { previewText } = fontOptions;

    const options = {
      includePreviews: true,
      previewText,
      previewFillStyle: getColor("body-color")
    };

    // Add the includeVariations argument into the option to get font variation data.
    if (this.pageStyle.supportsFontVariations) {
      options.includeVariations = true;
    }

    const node = this.inspector.selection.nodeFront;
    fonts = await this.getFontsForNode(node, options);
    allFonts = await this.getAllFonts(options);
    // Get the subset of fonts from the page which are not used on the selected node.
    otherFonts = allFonts.filter(font => {
      return !fonts.some(nodeFont => nodeFont.name === font.name);
    });

    if (!fonts.length && !otherFonts.length) {
      // No fonts to display. Clear the previously shown fonts.
      if (this.store) {
        this.store.dispatch(updateFonts(fonts, otherFonts, allFonts));
      }
      return;
    }

    for (const font of [...allFonts]) {
      font.previewUrl = await font.preview.data.string();
    }

    // in case we've been destroyed in the meantime
    if (!this.document) {
      return;
    }

    this.store.dispatch(updateFonts(fonts, otherFonts, allFonts));

    this.inspector.emit("fontinspector-updated");
  }

  /**
   * Update the "font-variation-settings" CSS property with the state of all touched
   * font variation axes which shouldn't be written to other CSS font properties.
   */
  updateFontVariationSettings() {
    const fontEditor = this.store.getState().fontEditor;
    const name = "font-variation-settings";
    const value = Object.keys(fontEditor.axes)
      // Pick only axes which are supposed to be written to font-variation-settings.
      // Skip registered axes which should be written to a different CSS property.
      .filter(tag => this.writers.get(tag) === this.updateFontVariationSettings)
      // Build a string value for the "font-variation-settings" CSS property
      .map(tag => `"${tag}" ${fontEditor.axes[tag]}`)
      .join(", ");

    this.updatePropertyValue(name, value);
  }

  /**
   * Preview a property value (live) then sync the changes (debounced) to the Rule view.
   *
   * NOTE: Until Bug 1462591 is addressed, all changes are written to the element's inline
   * style attribute. In this current scenario, Rule.previewPropertyValue()
   * causes the whole inline style representation in the Rule view to update instead of
   * just previewing the change on the element.
   * We keep the debounced call to syncChanges() because it explicitly calls
   * TextProperty.setValue() which performs other actions, including marking the property
   * as "changed" in the Rule view with a green indicator.
   *
   * @param {String} name
   *        CSS property name
   * @param {String}value
   *        CSS property value
   */
  updatePropertyValue(name, value) {
    const textProperty = this.getTextProperty(name);

    if (!textProperty) {
      this.selectedRule.createProperty(name, value, "", true);
      return;
    }

    if (textProperty.value === value) {
      return;
    }

    // Prevent reacting to changes we caused.
    this.ruleView.off("property-value-updated", this.onRulePropertyUpdated);
    // Live preview font property changes on the page.
    textProperty.rule.previewPropertyValue(textProperty, value, "").catch(console.error);

    // Sync Rule view with changes reflected on the page (debounced).
    this.syncChanges(name, value);
  }
}

module.exports = FontInspector;
