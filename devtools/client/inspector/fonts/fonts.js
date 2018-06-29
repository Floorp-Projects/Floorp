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

class FontInspector {
  constructor(inspector, window) {
    this.cssProperties = getCssProperties(inspector.toolbox);
    this.document = window.document;
    this.inspector = inspector;
    // Set of unique keyword values supported by designated font properties.
    this.keywordValues = new Set(this.getFontPropertyValueKeywords());
    this.nodeComputedStyle = {};
    this.pageStyle = this.inspector.pageStyle;
    this.ruleView = this.inspector.getPanel("ruleview").view;
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
    this.onPreviewFonts = this.onPreviewFonts.bind(this);
    this.onPropertyChange = this.onPropertyChange.bind(this);
    this.onRuleUpdated = this.onRuleUpdated.bind(this);
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
      onPreviewFonts: this.onPreviewFonts,
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
   * Given all fonts on the page, and given the fonts used in given node, return all fonts
   * not from the page not used in this node.
   *
   * @param  {Array} allFonts
   *         All fonts used on the entire page
   * @param  {Array} nodeFonts
   *         Fonts used only in one of the nodes
   * @return {Array}
   *         All fonts, except the ones used in the current node
   */
  excludeNodeFonts(allFonts, nodeFonts) {
    return allFonts.filter(font => {
      return !nodeFonts.some(nodeFont => nodeFont.name === font.name);
    });
  }

  /**
   * Destruction function called when the inspector is destroyed. Removes event listeners
   * and cleans up references.
   */
  destroy() {
    this.inspector.selection.off("new-node-front", this.onNewNode);
    this.inspector.sidebar.off("fontinspector-selected", this.onNewNode);
    this.ruleView.off("property-value-updated", this.onRuleUpdated);
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
   * Get a subset of fonts used on a node whose font family names are found in the
   * node's CSS font-family property value. The fonts will be sorted in the order their
   * family names are declared in CSS font-family.
   *
   * Fonts returned by this.getFontsForNode() contain, among others, these attributes:
   * - CSSFamilyName: a string of the font's family name (ex: "Times");
   * - CSSGeneric: a string of the generic font family (ex: "serif", "sans-serif") if
   * the font was resolved from a generic font family keyword, like serif, instead of
   * an explicit font famly, like "Times". If the font is resolved from an
   * explicit font family, CSSGeneric is null.
   *
   * For example:
   * font-family: "Avenir", serif;
   *
   * If fonts from both families are used, it will yield:
   * { CSSFamilyName: "Avenir", CSSGeneric: null, ... },
   * { CSSFamilyName: "Times", CSSGeneric: "serif", ... },
   *
   * @param {Array} fonts
   *        Fonts used on a node got from a call to this.getFontsForNode().
   * @param {Array} fontFamilies
   *        Strings of font families from a node's CSS font-family property value.
   * @return {Array}
   *         Subset of `fonts` whose font family names appear in `fontFamilies`.
   */
  filterFontsUsed(fonts = [], fontFamilies = []) {
    return fontFamilies.reduce((acc, family) => {
      const match = fonts.find(font => {
        const generic = typeof font.CSSGeneric === "string"
          ? font.CSSGeneric.toLowerCase()
          : font.CSSGeneric;

        return generic === family.toLowerCase()
          || font.CSSFamilyName.toLowerCase() === family.toLowerCase();
      });

      if (match) {
        acc.push(match);
      }

      return acc;
    }, []);
  }

  /**
   * Get all expected CSS font properties and values from the node's matching rules and
   * fallback to computed style. Skip CSS Custom Properties, `calc()` and keyword values.
   *
   * @return {Object}
   */
  getFontProperties() {
    const properties = {};

    // First, get all expected font properties from computed styles, if available.
    for (const prop of FONT_PROPERTIES) {
      properties[prop] =
        (this.nodeComputedStyle[prop] && this.nodeComputedStyle[prop].value)
          ? this.nodeComputedStyle[prop].value
          : "";
    }

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
   *
   * This list is used to filter out values when reading CSS font properties from rules.
   * Computed styles will be used instead of any of these values.
   *
   * @return {Array}
   */
  getFontPropertyValueKeywords() {
    return ["font-size", "font-weight", "font-stretch"].reduce((acc, property) => {
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

  async getFontsNotInNode(nodeFonts, options) {
    // In case we've been destroyed in the meantime
    if (!this.document) {
      return [];
    }

    let allFonts = await this.pageStyle.getAllUsedFontFaces(options).catch(console.error);
    if (!allFonts) {
      allFonts = [];
    }

    return this.excludeNodeFonts(allFonts, nodeFonts);
  }

  /**
   * Get a reference to a TextProperty instance from the current selected rule for a
   * given property name. If one doesn't exist, create one with the given value.
   *
   * @param {String} name
   *        CSS property name
   * @param {String} value
   *        CSS property value
   * @return {TextProperty}
   */
  getTextProperty(name, value) {
    let textProperty =
      this.selectedRule.textProps.find(prop => prop.name === name);
    if (!textProperty) {
      textProperty = this.selectedRule.editor.addProperty(name, value, "", true);
    }

    return textProperty;
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
   * Given a list of font families, return an object that groups them into sets of used
   * and not used if they match families of fonts from the given list of fonts used on a
   * node.
   *
   * @See this.filterFontsUsed() for an explanation of CSSFamilyName and CSSGeneric.
   *
   * @param {Array} fontsUsed
   *        Fonts used on a node.
   * @param {Array} fontFamilies
   *        Strings of font families
   * @return {Object}
   */
  groupFontFamilies(fontsUsed = [], fontFamilies = []) {
    const families = {};
    // Font family names declared and used.
    families.used = fontsUsed.map(font =>
      font.CSSGeneric ? font.CSSGeneric : font.CSSFamilyName
    );
    const familiesUsedLowercase = families.used.map(family => family.toLowerCase());
    // Font family names declared but not used.
    families.notUsed = fontFamilies
      .filter(family => !familiesUsedLowercase.includes(family.toLowerCase()));

    return families;
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
           this.inspector.selection.isElementNode();
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
      textProperty.setValue(value);
    }

    this.ruleView.on("property-value-updated", this.onRuleUpdated);
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
    if (this.isPanelVisible()) {
      this.update();
      this.refreshFontEditor();
    }
  }

  /**
   * Handler for change in preview input.
   */
  onPreviewFonts(value) {
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
   * @param  {String} value
   *         CSS font property numeric value or axis value
   * @param  {String|null} unit
   *         CSS unit or null
   */
  onPropertyChange(property, value, unit) {
    if (FONT_PROPERTIES.includes(property)) {
      this.onFontPropertyUpdate(property, value, unit);
    } else {
      this.onAxisUpdate(property, value);
    }
  }

  /**
   * Handler for "property-value-updated" event emitted from the rule view whenever a
   * property value changes.
   */
  async onRuleUpdated() {
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

    if (!this.inspector || !this.store || !this.isSelectedNodeValid()) {
      this.store.dispatch(resetFontEditor());
      return;
    }

    const options = {};
    if (this.pageStyle.supportsFontVariations) {
      options.includeVariations = true;
    }

    const node = this.inspector.selection.nodeFront;
    const fonts = await this.getFontsForNode(node, options);

    // Get computed styles for the selected node, but filter by CSS font properties.
    this.nodeComputedStyle = await this.pageStyle.getComputed(node, {
      filterProperties: FONT_PROPERTIES
    });

    if (!this.nodeComputedStyle) {
      this.store.dispatch(resetFontEditor());
      return;
    }

    // Clear any references to writer methods and CSS declarations because the node's
    // styles may have changed since the last font editor refresh.
    this.writers.clear();
    // Select the node's inline style as the rule where to write property value changes.
    this.selectedRule =
      this.ruleView.rules.find(rule => rule.domRule.type === ELEMENT_STYLE);

    const properties = this.getFontProperties();
    const familiesDeclared =
      properties["font-family"].split(",")
      .map(font => font.replace(/["']+/g, "").trim());
    // Subset of fonts used on the node whose family names exist in CSS font-family.
    let fontsUsed = this.filterFontsUsed(fonts, familiesDeclared);
    // Object with font families groupped by used and not used.
    const families = this.groupFontFamilies(fontsUsed, familiesDeclared);
    // Assign writer methods to each axis defined in font-variation-settings.
    const axes = parseFontVariationAxes(properties["font-variation-settings"]);
    Object.keys(axes).map(axis => {
      this.writers.set(axis, this.getWriterForAxis(axis));
    });

    // Pick fonts from descendants if no declared fonts were used on this node.
    if (!fontsUsed.length && fonts.length) {
      const otherVarFonts = fonts.filter(font => {
        return (font.variationAxes && font.variationAxes.length);
      });

      // Prefer picking variable fonts if any were found on descendants of this node.
      // The FontEditor component will render UI for the first font in the list.
      fontsUsed = otherVarFonts.length ? otherVarFonts : fonts;
    }

    this.store.dispatch(updateFontEditor(fontsUsed, families, properties));
    this.inspector.emit("fonteditor-updated");
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

    let fonts = [];
    let otherFonts = [];

    if (!this.isSelectedNodeValid()) {
      this.store.dispatch(updateFonts(fonts, otherFonts));
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
    otherFonts = await this.getFontsNotInNode(fonts, options);

    if (!fonts.length && !otherFonts.length) {
      // No fonts to display. Clear the previously shown fonts.
      if (this.store) {
        this.store.dispatch(updateFonts(fonts, otherFonts));
      }
      return;
    }

    for (const font of [...fonts, ...otherFonts]) {
      font.previewUrl = await font.preview.data.string();
    }

    // in case we've been destroyed in the meantime
    if (!this.document) {
      return;
    }

    this.store.dispatch(updateFonts(fonts, otherFonts));

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
    const textProperty = this.getTextProperty(name, value);
    if (!textProperty || textProperty.value === value) {
      return;
    }

    // Prevent reacting to changes we caused.
    this.ruleView.off("property-value-updated", this.onRuleUpdated);
    // Live preview font property changes on the page.
    textProperty.rule.previewPropertyValue(textProperty, value, "");
    // Sync Rule view with changes reflected on the page (debounced).
    this.syncChanges(name, value);
  }
}

module.exports = FontInspector;
