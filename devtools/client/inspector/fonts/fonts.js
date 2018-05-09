/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { gDevTools } = require("devtools/client/framework/devtools");
const { getColor } = require("devtools/client/shared/theme");
const { createFactory, createElement } = require("devtools/client/shared/vendor/react");
const { Provider } = require("devtools/client/shared/vendor/react-redux");
const { throttle } = require("devtools/shared/throttle");
const { debounce } = require("devtools/shared/debounce");

const FontsApp = createFactory(require("./components/FontsApp"));

const { LocalizationHelper } = require("devtools/shared/l10n");
const INSPECTOR_L10N =
  new LocalizationHelper("devtools/client/locales/inspector.properties");

const { getStr } = require("./utils/l10n");
const { updateFonts } = require("./actions/fonts");
const {
  applyInstance,
  resetFontEditor,
  toggleFontEditor,
  updateAxis,
  updateCustomInstance,
  updateFontEditor,
} = require("./actions/font-editor");
const { updatePreviewText } = require("./actions/font-options");

const CUSTOM_INSTANCE_NAME = getStr("fontinspector.customInstanceName");
const FONT_EDITOR_ID = "fonteditor";
const FONT_PROPERTIES = [
  "font-optical-sizing",
  "font-size",
  "font-stretch",
  "font-style",
  "font-variation-settings",
  "font-weight",
];
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
    this.document = window.document;
    this.inspector = inspector;
    this.nodeComputedStyle = {};
    this.pageStyle = this.inspector.pageStyle;
    this.ruleView = this.inspector.getPanel("ruleview").view;
    this.selectedRule = null;
    this.store = this.inspector.store;
    // Map CSS property names to corresponding TextProperty instances from the Rule view.
    this.textProperties = new Map();
    // Map CSS property names and variable font axis names to methods that write their
    // corresponding values to the appropriate TextProperty from the Rule view.
    // Values of variable font registered axes may be written to CSS font properties under
    // certain cascade circumstances and platform support. @see `getWriterForAxis(axis)`
    this.writers = new Map();

    this.snapshotChanges = debounce(this.snapshotChanges, 100, this);
    this.syncChanges = throttle(this.syncChanges, 100, this);
    this.onAxisUpdate = this.onAxisUpdate.bind(this);
    this.onInstanceChange = this.onInstanceChange.bind(this);
    this.onNewNode = this.onNewNode.bind(this);
    this.onPreviewFonts = this.onPreviewFonts.bind(this);
    this.onRuleSelected = this.onRuleSelected.bind(this);
    this.onToggleFontHighlight = this.onToggleFontHighlight.bind(this);
    this.onRuleUnselected = this.onRuleUnselected.bind(this);
    this.onRuleUpdated = this.onRuleUpdated.bind(this);
    this.onThemeChanged = this.onThemeChanged.bind(this);
    this.update = this.update.bind(this);
    this.updateFontVariationSettings = this.updateFontVariationSettings.bind(this);

    this.init();
  }

  init() {
    if (!this.inspector) {
      return;
    }

    let fontsApp = FontsApp({
      onAxisUpdate: this.onAxisUpdate,
      onInstanceChange: this.onInstanceChange,
      onToggleFontHighlight: this.onToggleFontHighlight,
      onPreviewFonts: this.onPreviewFonts,
    });

    let provider = createElement(Provider, {
      id: "fontinspector",
      key: "fontinspector",
      store: this.store,
      title: INSPECTOR_L10N.getStr("inspector.sidebar.fontInspectorTitle"),
    }, fontsApp);

    // Expose the provider to let inspector.js use it in setupSidebar.
    this.provider = provider;

    this.inspector.selection.on("new-node-front", this.onNewNode);
    this.inspector.sidebar.on("fontinspector-selected", this.onNewNode);
    this.ruleView.on("ruleview-rule-selected", this.onRuleSelected);
    this.ruleView.on("ruleview-rule-unselected", this.onRuleUnselected);

    // Listen for theme changes as the color of the previews depend on the theme
    gDevTools.on("theme-switched", this.onThemeChanged);

    // The FontInspector is lazy-loaded. If it's not yet loaded, the event handler for
    // "ruleview-rule-selected" won't be attached to catch the first font editor toggle.
    // Here, we check if the rule was already marked as selected for the font editor
    // before the FontInspector was instantiated and call the event handler manually.
    const selectedRule = this.ruleView.getSelectedRules(FONT_EDITOR_ID)[0];
    if (selectedRule) {
      this.onRuleSelected({ editorId: FONT_EDITOR_ID, rule: selectedRule });
    } else {
      this.store.dispatch(updatePreviewText(""));
      this.update();
    }
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
    this.ruleView.off("ruleview-rule-selected", this.onRuleSelected);
    this.ruleView.off("ruleview-rule-unselected", this.onRuleUnselected);
    gDevTools.off("theme-switched", this.onThemeChanged);

    this.document = null;
    this.inspector = null;
    this.nodeComputedStyle = {};
    this.pageStyle = null;
    this.ruleView = null;
    this.selectedRule = null;
    this.store = null;
    this.textProperties.clear();
    this.textProperties = null;
    this.writers.clear();
    this.writers = null;
  }

  /**
   * Collect all the expected CSS font properties and their values for the selected node
   * from the node's computed style and from all the rules that apply to it.
   *
   * @return {Object}
   */
  getFontProperties() {
    let properties = {};

    // First, get all expected font properties from computed styles.
    for (let prop of FONT_PROPERTIES) {
      properties[prop] = this.nodeComputedStyle[prop].value;
    }

    // Then, replace with enabled font properties found on any of the rules that apply.
    for (let rule of this.ruleView.rules) {
      for (let textProp of rule.textProps) {
        if (FONT_PROPERTIES.includes(textProp.name) && textProp.enabled &&
            !textProp.overridden) {
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

    let fonts = await this.pageStyle.getUsedFontFaces(node, options).catch(console.error);
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
   * given property name. If one doesn't exist, create and return a new one.
   *
   * @param {String} name
   *        CSS property name
   * @return {TextProperty}
   */
  getTextProperty(name) {
    if (!this.textProperties.has(name)) {
      let textProperty =
        this.selectedRule.textProps.find(prop => prop.name === name);
      if (!textProperty) {
        textProperty = this.selectedRule.editor.addProperty(name, "initial", "", true);
      }

      this.textProperties.set(name, textProperty);
    }

    return this.textProperties.get(name);
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
        this.updateFontProperty(name, value);
      });
    } else {
      this.writers.set(name, this.updateFontVariationSettings);
    }

    return this.writers.get(name);
  }

  /**
   * Returns true if the font inspector panel is visible, and false otherwise.
   */
  isPanelVisible() {
    return this.inspector.sidebar &&
           this.inspector.sidebar.getCurrentTabID() === "fontinspector";
  }

    /**
   * Sync the Rule view with the styles from the page. Called in a throttled way
   * (see constructor) after property changes are applied directly to the CSS style rule
   * on the page circumventing TextProperty.setValue() which triggers expensive DOM
   * operations in TextPropertyEditor.update().
   *
   * @param  {TextProperty} textProperty
   *         Model of CSS declaration for a property in used in the rule view.
   * @param  {String} value
   *         Value of the CSS property that should be reflected in the rule view.
   */
  syncChanges(textProperty, value) {
    textProperty.updateValue(value);
    this.ruleView.on("property-value-updated", this.onRuleUpdated);
  }

  /**
   * Handler for changes of font axis value. Updates the value in the store and previews
   * the change on the page.
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
   * Handler for "ruleview-rule-selected" event emitted from the rule view when a rule is
   * marked as selected for an editor.
   * If selected for the font editor, hold a reference to the rule so we know where to
   * put property changes coming from the font editor and show the font editor panel.
   *
   * @param  {Object} eventData
   *         Data payload for the event. Contains:
   *         - {String} editorId - id of the editor for which the rule was selected
   *         - {Rule} rule - reference to rule that was selected
   */
  async onRuleSelected(eventData) {
    const { editorId, rule } = eventData;
    if (editorId === FONT_EDITOR_ID) {
      const selector = rule.matchedSelectors[0];
      this.selectedRule = rule;

      await this.refreshFontEditor();
      this.store.dispatch(toggleFontEditor(true, selector));
      this.ruleView.on("property-value-updated", this.onRuleUpdated);
    }
  }

  /**
   * Handler for "property-value-updated" event emitted from the rule view whenever a
   * property value changes.
   */
  async onRuleUpdated() {
    await this.refreshFontEditor();
  }

  /**
   * Handler for "ruleview-rule-unselected" event emitted from the rule view when a rule
   * was released from being selected for an editor.
   * If previously selected for the font editor, release the reference to the rule and
   * hide the font editor panel.
   *
   * @param {Object} eventData
   *        Data payload for the event. Contains:
   *        - {String} editorId - id of the editor for which the rule was released
   *        - {Rule} rule - reference to rule that was released
   */
  onRuleUnselected(eventData) {
    const { editorId, rule } = eventData;
    if (editorId === FONT_EDITOR_ID && rule == this.selectedRule) {
      this.nodeComputedStyle = {};
      this.selectedRule = null;
      this.textProperties.clear();
      this.writers.clear();

      this.store.dispatch(toggleFontEditor(false));
      this.store.dispatch(resetFontEditor());

      this.ruleView.off("property-value-updated", this.onRuleUpdated);
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
   *         Whether or not to reveal the font for the current element selection.
   */
  async onToggleFontHighlight(font, show, isForCurrentElement) {
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
        let node = isForCurrentElement
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
   * - the CSS font properties declared on the selected rule.
   *
   * This method is called during initial setup and as a result of any property
   * values change in the Rule view. For the latter case, we do a deep compare between the
   * font properties on the selected rule and the ones already store to decide if to
   * update the font edtior to reflect a new external state.
   */
  async refreshFontEditor() {
    if (!this.selectedRule || !this.inspector || !this.store) {
      return;
    }

    const options = {};
    if (this.pageStyle.supportsFontVariations) {
      options.includeVariations = true;
    }

    const node = this.inspector.selection.nodeFront;
    const fonts = await this.getFontsForNode(node, options);
    // Get all computed styles for selected node; used for identifying inherited values.
    this.nodeComputedStyle = await this.pageStyle.getComputed(node, {
      filterProperties: FONT_PROPERTIES
    });
    const fontEditor = this.store.getState().fontEditor;
    const properties = this.getFontProperties();

    // Update the font editor state only if property values in rule differ from store.
    // This can happen when a user makes manual edits to the values in the rule view.
    if (JSON.stringify(properties) !== JSON.stringify(fontEditor.properties)) {
      this.store.dispatch(updateFontEditor(fonts, properties));
    }
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

    let node = this.inspector.selection.nodeFront;

    let fonts = [];
    let otherFonts = [];

    let { fontOptions } = this.store.getState();
    let { previewText } = fontOptions;

    // Clear the list of fonts if the currently selected node is not connected or a text
    // or element node unless all fonts are supposed to be shown.
    let isElementOrTextNode = this.inspector.selection.isElementNode() ||
                              this.inspector.selection.isTextNode();
    if (!node ||
        !this.isPanelVisible() ||
        !this.inspector.selection.isConnected() ||
        !isElementOrTextNode) {
      this.store.dispatch(updateFonts(fonts, otherFonts));
      return;
    }

    let options = {
      includePreviews: true,
      previewText,
      previewFillStyle: getColor("body-color")
    };

    // Add the includeVariations argument into the option to get font variation data.
    if (this.pageStyle.supportsFontVariations) {
      options.includeVariations = true;
    }

    fonts = await this.getFontsForNode(node, options);
    otherFonts = await this.getFontsNotInNode(fonts, options);

    if (!fonts.length && !otherFonts.length) {
      // No fonts to display. Clear the previously shown fonts.
      if (this.store) {
        this.store.dispatch(updateFonts(fonts, otherFonts));
      }
      return;
    }

    for (let font of [...fonts, ...otherFonts]) {
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
   * Preview a property value (live) then sync the changes (throttled) to the Rule view.
   *
   * @param {String} name
   *        CSS property name
   * @param {String}value
   *        CSS property value
   */
  updatePropertyValue(name, value) {
    const textProperty = this.getTextProperty(name);
    if (!textProperty) {
      return;
    }

    // Prevent reacting to changes we caused.
    this.ruleView.off("property-value-updated", this.onRuleUpdated);
    // Live preview font property changes on the page.
    textProperty.rule.previewPropertyValue(textProperty, value, "");
    // Sync Rule view with changes reflected on the page (throttled).
    this.syncChanges(textProperty, value);
  }
}

module.exports = FontInspector;
