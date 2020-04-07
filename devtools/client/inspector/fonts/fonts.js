/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { gDevTools } = require("devtools/client/framework/devtools");
const { getColor } = require("devtools/client/shared/theme");
const {
  createFactory,
  createElement,
} = require("devtools/client/shared/vendor/react");
const { Provider } = require("devtools/client/shared/vendor/react-redux");
const { debounce } = require("devtools/shared/debounce");
const { ELEMENT_STYLE } = require("devtools/shared/specs/styles");

const FontsApp = createFactory(
  require("devtools/client/inspector/fonts/components/FontsApp")
);

const { LocalizationHelper } = require("devtools/shared/l10n");
const INSPECTOR_L10N = new LocalizationHelper(
  "devtools/client/locales/inspector.properties"
);

const {
  parseFontVariationAxes,
} = require("devtools/client/inspector/fonts/utils/font-utils");

const fontDataReducer = require("devtools/client/inspector/fonts/reducers/fonts");
const fontEditorReducer = require("devtools/client/inspector/fonts/reducers/font-editor");
const fontOptionsReducer = require("devtools/client/inspector/fonts/reducers/font-options");
const {
  updateFonts,
} = require("devtools/client/inspector/fonts/actions/fonts");
const {
  applyInstance,
  resetFontEditor,
  setEditorDisabled,
  updateAxis,
  updateFontEditor,
  updateFontProperty,
} = require("devtools/client/inspector/fonts/actions/font-editor");
const {
  updatePreviewText,
} = require("devtools/client/inspector/fonts/actions/font-options");

const FONT_PROPERTIES = [
  "font-family",
  "font-optical-sizing",
  "font-size",
  "font-stretch",
  "font-style",
  "font-variation-settings",
  "font-weight",
  "letter-spacing",
  "line-height",
];
const REGISTERED_AXES_TO_FONT_PROPERTIES = {
  ital: "font-style",
  opsz: "font-optical-sizing",
  slnt: "font-style",
  wdth: "font-stretch",
  wght: "font-weight",
};
const REGISTERED_AXES = Object.keys(REGISTERED_AXES_TO_FONT_PROPERTIES);

const HISTOGRAM_FONT_TYPE_DISPLAYED = "DEVTOOLS_FONTEDITOR_FONT_TYPE_DISPLAYED";

class FontInspector {
  constructor(inspector, window) {
    this.cssProperties = inspector.cssProperties;
    this.document = window.document;
    this.inspector = inspector;
    // Selected node in the markup view. For text nodes, this points to their parent node
    // element. Font faces and font properties for this node will be shown in the editor.
    this.node = null;
    this.nodeComputedStyle = {};
    // The page style actor that will be providing the style information.
    this.pageStyle = null;
    this.ruleViewTool = this.inspector.getPanel("ruleview");
    this.ruleView = this.ruleViewTool.view;
    this.selectedRule = null;
    this.store = this.inspector.store;
    // Map CSS property names and variable font axis names to methods that write their
    // corresponding values to the appropriate TextProperty from the Rule view.
    // Values of variable font registered axes may be written to CSS font properties under
    // certain cascade circumstances and platform support. @see `getWriterForAxis(axis)`
    this.writers = new Map();

    this.store.injectReducer("fontOptions", fontOptionsReducer);
    this.store.injectReducer("fontData", fontDataReducer);
    this.store.injectReducer("fontEditor", fontEditorReducer);

    this.syncChanges = debounce(this.syncChanges, 100, this);
    this.onInstanceChange = this.onInstanceChange.bind(this);
    this.onNewNode = this.onNewNode.bind(this);
    this.onPreviewTextChange = debounce(this.onPreviewTextChange, 100, this);
    this.onPropertyChange = this.onPropertyChange.bind(this);
    this.onRulePropertyUpdated = debounce(
      this.onRulePropertyUpdated,
      300,
      this
    );
    this.onToggleFontHighlight = this.onToggleFontHighlight.bind(this);
    this.onThemeChanged = this.onThemeChanged.bind(this);
    this.update = this.update.bind(this);
    this.updateFontVariationSettings = this.updateFontVariationSettings.bind(
      this
    );

    this.init();
  }

  /**
   * Map CSS font property names to a list of values that should be skipped when consuming
   * font properties from CSS rules. The skipped values are mostly keyword values like
   * `bold`, `initial`, `unset`. Computed values will be used instead of such keywords.
   *
   * @return {Map}
   */
  get skipValuesMap() {
    if (!this._skipValuesMap) {
      this._skipValuesMap = new Map();

      for (const property of FONT_PROPERTIES) {
        const values = this.cssProperties.getValues(property);

        switch (property) {
          case "line-height":
          case "letter-spacing":
            // There's special handling for "normal" so remove it from the skip list.
            this.skipValuesMap.set(
              property,
              values.filter(value => value !== "normal")
            );
            break;
          default:
            this.skipValuesMap.set(property, values);
        }
      }
    }

    return this._skipValuesMap;
  }

  init() {
    if (!this.inspector) {
      return;
    }

    const fontsApp = FontsApp({
      onInstanceChange: this.onInstanceChange,
      onToggleFontHighlight: this.onToggleFontHighlight,
      onPreviewTextChange: this.onPreviewTextChange,
      onPropertyChange: this.onPropertyChange,
    });

    const provider = createElement(
      Provider,
      {
        id: "fontinspector",
        key: "fontinspector",
        store: this.store,
        title: INSPECTOR_L10N.getStr("inspector.sidebar.fontInspectorTitle"),
      },
      fontsApp
    );

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
      throw TypeError(
        `Invalid value for conversion. Expected Number, got ${value}`
      );
    }

    const shouldReturn = () => {
      // Early return if:
      // - conversion is not required
      // - property is `line-height`
      // - `fromUnit` is `em` and `toUnit` is unitless
      const conversionNotRequired = fromUnit === toUnit || value === 0;
      const forLineHeight =
        property === "line-height" && fromUnit === "" && toUnit === "em";
      const isEmToUnitlessConversion = fromUnit === "em" && toUnit === "";
      return conversionNotRequired || forLineHeight || isEmToUnitlessConversion;
    };

    if (shouldReturn()) {
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
    // Default output value to input value for a 1-to-1 conversion as a guard against
    // unrecognized CSS units. It will not be correct, but it will also not break.
    let out = value;

    const converters = {
      in: () => (fromPx ? value / 96 : value * 96),
      cm: () => (fromPx ? value * 0.02645833333 : value / 0.02645833333),
      mm: () => (fromPx ? value * 0.26458333333 : value / 0.26458333333),
      pt: () => (fromPx ? value * 0.75 : value / 0.75),
      pc: () => (fromPx ? value * 0.0625 : value / 0.0625),
      "%": async () => {
        const fontSize = await this.getReferenceFontSize(property, unit);
        return fromPx
          ? (value * 100) / parseFloat(fontSize)
          : (value / 100) * parseFloat(fontSize);
      },
      rem: async () => {
        const fontSize = await this.getReferenceFontSize(property, unit);
        return fromPx
          ? value / parseFloat(fontSize)
          : value * parseFloat(fontSize);
      },
      vh: async () => {
        const { height } = await this.getReferenceBox(property, unit);
        return fromPx ? (value * 100) / height : (value / 100) * height;
      },
      vw: async () => {
        const { width } = await this.getReferenceBox(property, unit);
        return fromPx ? (value * 100) / width : (value / 100) * width;
      },
      vmin: async () => {
        const { width, height } = await this.getReferenceBox(property, unit);
        return fromPx
          ? (value * 100) / Math.min(width, height)
          : (value / 100) * Math.min(width, height);
      },
      vmax: async () => {
        const { width, height } = await this.getReferenceBox(property, unit);
        return fromPx
          ? (value * 100) / Math.max(width, height)
          : (value / 100) * Math.max(width, height);
      },
    };

    if (converters.hasOwnProperty(unit)) {
      const converter = converters[unit];
      out = await converter();
    }

    // Special handling for unitless line-height.
    if (unit === "em" || (unit === "" && property === "line-height")) {
      const fontSize = await this.getReferenceFontSize(property, unit);
      out = fromPx
        ? value / parseFloat(fontSize)
        : value * parseFloat(fontSize);
    }

    // Catch any NaN or Infinity as result of dividing by zero in any
    // of the relative unit conversions which rely on external values.
    if (isNaN(out) || Math.abs(out) === Infinity) {
      out = 0;
    }

    // Return values limited to 3 decimals when:
    // - the unit is converted from pixels to something else
    // - the value is for letter spacing, regardless of unit (allow sub-pixel precision)
    if (fromPx || property === "letter-spacing") {
      // Round values like 1.000 to 1
      return out === Math.round(out) ? Math.round(out) : out.toFixed(3);
    }

    // Round pixel values.
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
    this.node = null;
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
        this.nodeComputedStyle[prop] && this.nodeComputedStyle[prop].value
          ? this.nodeComputedStyle[prop].value
          : "";
    }

    // Then, replace with enabled font properties found on any of the rules that apply.
    for (const rule of this.ruleView.rules) {
      if (rule.inherited) {
        continue;
      }

      for (const textProp of rule.textProps) {
        if (
          FONT_PROPERTIES.includes(textProp.name) &&
          !this.skipValuesMap.get(textProp.name).includes(textProp.value) &&
          !textProp.value.includes("calc(") &&
          !textProp.value.includes("var(") &&
          !textProp.overridden &&
          textProp.enabled
        ) {
          properties[textProp.name] = textProp.value;
        }
      }
    }

    return properties;
  }

  async getFontsForNode(node, options) {
    // In case we've been destroyed in the meantime
    if (!this.document) {
      return [];
    }

    const fonts = await this.pageStyle
      .getUsedFontFaces(node, options)
      .catch(console.error);
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

    const inspectorFronts = await this.inspector.getAllInspectorFronts();

    let allFonts = [];
    for (const { pageStyle } of inspectorFronts) {
      allFonts = allFonts.concat(await pageStyle.getAllUsedFontFaces(options));
    }

    return allFonts;
  }

  /**
   * Get the box dimensions used for unit conversion according to the CSS property and
   * target CSS unit.
   *
   * @param  {String} property
   *         CSS property
   * @param  {String} unit
   *         Target CSS unit
   * @return {Promise}
   *         Promise that resolves with an object with box dimensions in pixels.
   */
  async getReferenceBox(property, unit) {
    const box = { width: 0, height: 0 };
    const node = await this.getReferenceNode(property, unit).catch(
      console.error
    );

    if (!node) {
      return box;
    }

    switch (unit) {
      case "vh":
      case "vw":
      case "vmin":
      case "vmax":
        const dim = await node.getOwnerGlobalDimensions().catch(console.error);
        if (dim) {
          box.width = dim.innerWidth;
          box.height = dim.innerHeight;
        }
        break;

      case "%":
        const style = await this.pageStyle
          .getComputed(node)
          .catch(console.error);
        if (style) {
          box.width = style.width.value;
          box.height = style.height.value;
        }
        break;
    }

    return box;
  }

  /**
   * Get the refernece font size value used for unit conversion according to the
   * CSS property and target CSS unit.
   *
   * @param {String} property
   *        CSS property
   * @param {String} unit
   *        Target CSS unit
   * @return {Promise}
   *         Promise that resolves with the reference font size value or null if there
   *         was an error getting that value.
   */
  async getReferenceFontSize(property, unit) {
    const node = await this.getReferenceNode(property, unit).catch(
      console.error
    );
    if (!node) {
      return null;
    }

    const style = await this.pageStyle.getComputed(node).catch(console.error);
    if (!style) {
      return null;
    }

    return style["font-size"].value;
  }

  /**
   * Get the reference node used in measurements for unit conversion according to the
   * the CSS property and target CSS unit type.
   *
   * @param  {String} property
   *         CSS property
   * @param  {String} unit
   *         Target CSS unit
   * @return {Promise}
   *          Promise that resolves with the reference node used in measurements for unit
   *          conversion.
   */
  async getReferenceNode(property, unit) {
    let node;

    switch (property) {
      case "line-height":
      case "letter-spacing":
        node = this.node;
        break;
      default:
        node = this.node.parentNode();
    }

    switch (unit) {
      case "rem":
        // Regardless of CSS property, always use the root document element for "rem".
        node = await this.node.walkerFront.documentElement();
        break;
    }

    return node;
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

    return this.selectedRule.textProps.find(
      prop => prop.name === name && prop.enabled && !prop.overridden
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

    return value => {
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
          // We have to invert the sign of the angle because CSS and OpenType measure
          // in opposite directions.
          value = -value;
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
      this.writers.set(name, value => {
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
    return (
      this.inspector &&
      this.inspector.sidebar &&
      this.inspector.sidebar.getCurrentTabID() === "fontinspector"
    );
  }

  /**
   * Upon a new node selection, log some interesting telemetry probes.
   */
  logTelemetryProbesOnNewNode() {
    const { fontEditor } = this.store.getState();
    const { telemetry } = this.inspector;

    // Log data about the currently edited font (if any).
    // Note that the edited font is always the first one from the fontEditor.fonts array.
    const editedFont = fontEditor.fonts[0];
    if (!editedFont) {
      return;
    }

    const nbOfAxes = editedFont.variationAxes
      ? editedFont.variationAxes.length
      : 0;
    telemetry
      .getHistogramById(HISTOGRAM_FONT_TYPE_DISPLAYED)
      .add(!nbOfAxes ? "nonvariable" : "variable");
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
  async syncChanges(name, value) {
    const textProperty = this.getTextProperty(name, value);
    if (textProperty) {
      try {
        await textProperty.setValue(value, "", true);
        this.ruleView.on("property-value-updated", this.onRulePropertyUpdated);
      } catch (error) {
        // Because setValue() does an asynchronous call to the server, there is a chance
        // the font editor was destroyed while we were waiting. If that happened, just
        // bail out silently.
        if (!this.document) {
          return;
        }

        throw error;
      }
    }
  }

  /**
   * Handler for changes of a font axis value coming from the FontEditor.
   *
   * @param  {String} tag
   *         Tag name of the font axis.
   * @param  {Number} value
   *         Value of the font axis.
   */
  onAxisUpdate(tag, value) {
    this.store.dispatch(updateAxis(tag, value));
    const writer = this.getWriterForProperty(tag);
    writer(value.toString());
  }

  /**
   * Handler for changes of a CSS font property value coming from the FontEditor.
   *
   * @param  {String} property
   *         CSS font property name.
   * @param  {Number} value
   *         CSS font property numeric value.
   * @param  {String|null} unit
   *         CSS unit or null
   */
  onFontPropertyUpdate(property, value, unit) {
    value = unit !== null ? value + unit : value;
    this.store.dispatch(updateFontProperty(property, value));
    const writer = this.getWriterForProperty(property);
    writer(value.toString());
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
   * Event handler for "new-node-front" event fired when a new node is selected in the
   * markup view.
   *
   * Sets the selected node for which font faces and font properties will be
   * shown in the font editor. If the selection is a text node, use its parent element.
   *
   * Triggers a refresh of the font editor and font overview if the panel is visible.
   */
  onNewNode() {
    this.ruleView.off("property-value-updated", this.onRulePropertyUpdated);

    // First, reset the selected node and page style front.
    this.node = null;
    this.pageStyle = null;

    // Then attempt to assign a selected node according to its type.
    const selection = this.inspector && this.inspector.selection;
    if (selection && selection.isConnected()) {
      if (selection.isElementNode()) {
        this.node = selection.nodeFront;
      } else if (selection.isTextNode()) {
        this.node = selection.nodeFront.parentNode();
      }

      this.pageStyle = this.node.inspectorFront.pageStyle;
    }

    if (this.isPanelVisible()) {
      Promise.all([this.update(), this.refreshFontEditor()])
        .then(() => {
          this.logTelemetryProbesOnNewNode();
        })
        .catch(e => console.error(e));
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
   * @param  {Number} value
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

      this.onFontPropertyUpdate(property, value, unit);
    } else {
      this.onAxisUpdate(property, value);
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
        this.fontsHighlighter = await this.inspector.inspectorFront.getHighlighterByType(
          "FontsHighlighter"
        );
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
          ? this.node
          : this.node.walkerFront.rootNode;

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
   * - when a new node is selected;
   * - when any property is changed in the Rule view.
   * For the latter case, we compare between the latest computed style font properties
   * and the ones already in the store to decide if to update the font editor state.
   */
  async refreshFontEditor() {
    if (!this.node) {
      this.store.dispatch(resetFontEditor());
      return;
    }

    const options = {};
    if (this.pageStyle.supportsFontVariations) {
      options.includeVariations = true;
    }

    const fonts = await this.getFontsForNode(this.node, options);

    try {
      // Get computed styles for the selected node, but filter by CSS font properties.
      this.nodeComputedStyle = await this.pageStyle.getComputed(this.node, {
        filterProperties: FONT_PROPERTIES,
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
    if (!this.ruleViewTool.isPanelVisible()) {
      await this.ruleView.selectElement(this.node, false);
    }

    // Select the node's inline style as the rule where to write property value changes.
    this.selectedRule = this.ruleView.rules.find(
      rule => rule.domRule.type === ELEMENT_STYLE
    );

    const properties = await this.getFontProperties();
    // Assign writer methods to each axis defined in font-variation-settings.
    const axes = parseFontVariationAxes(properties["font-variation-settings"]);
    Object.keys(axes).map(axis => {
      this.writers.set(axis, this.getWriterForAxis(axis));
    });

    this.store.dispatch(updateFontEditor(fonts, properties, this.node.actorID));
    this.store.dispatch(setEditorDisabled(this.node.isPseudoElement));

    this.inspector.emit("fonteditor-updated");
    // Listen to manual changes in the Rule view that could update the Font Editor state
    this.ruleView.on("property-value-updated", this.onRulePropertyUpdated);
  }

  async update() {
    // Stop refreshing if the inspector or store is already destroyed.
    if (!this.inspector || !this.store) {
      return;
    }

    let allFonts = [];

    if (!this.node) {
      this.store.dispatch(updateFonts(allFonts));
      return;
    }

    const { fontOptions } = this.store.getState();
    const { previewText } = fontOptions;

    const options = {
      includePreviews: true,
      // Coerce the type of `supportsFontVariations` to a boolean.
      includeVariations: !!this.pageStyle.supportsFontVariations,
      previewText,
      previewFillStyle: getColor("body-color"),
    };

    // If there are no fonts used on the page, the result is an empty array.
    allFonts = await this.getAllFonts(options);

    // Augment each font object with a dataURI for an image with a sample of the font.
    for (const font of [...allFonts]) {
      font.previewUrl = await font.preview.data.string();
    }

    // Dispatch to the store if it hasn't been destroyed in the meantime.
    this.store && this.store.dispatch(updateFonts(allFonts));
    // Emit on the inspector if it hasn't been destroyed in the meantime.
    this.inspector && this.inspector.emit("fontinspector-updated");
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
    textProperty.rule
      .previewPropertyValue(textProperty, value, "")
      .catch(console.error);

    // Sync Rule view with changes reflected on the page (debounced).
    this.syncChanges(name, value);
  }
}

module.exports = FontInspector;
