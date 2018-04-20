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

class FontInspector {
  constructor(inspector, window) {
    this.document = window.document;
    this.inspector = inspector;
    this.pageStyle = this.inspector.pageStyle;
    this.ruleView = this.inspector.getPanel("ruleview").view;
    this.selectedRule = null;
    this.store = this.inspector.store;

    this.snapshotChanges = debounce(this.snapshotChanges, 100, this);
    this.syncChanges = throttle(this.syncChanges, 100, this);
    this.onAxisUpdate = this.onAxisUpdate.bind(this);
    this.onInstanceChange = this.onInstanceChange.bind(this);
    this.onNewNode = this.onNewNode.bind(this);
    this.onPreviewFonts = this.onPreviewFonts.bind(this);
    this.onRuleSelected = this.onRuleSelected.bind(this);
    this.onRuleUnselected = this.onRuleUnselected.bind(this);
    this.onRuleUpdated = this.onRuleUpdated.bind(this);
    this.onThemeChanged = this.onThemeChanged.bind(this);
    this.update = this.update.bind(this);

    this.init();
  }

  init() {
    if (!this.inspector) {
      return;
    }

    let fontsApp = FontsApp({
      onAxisUpdate: this.onAxisUpdate,
      onInstanceChange: this.onInstanceChange,
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

    this.store.dispatch(updatePreviewText(""));
    this.update(false, "");
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
    this.ruleView.off("ruleview-rule-selected", this.onRuleSelected);
    this.ruleView.off("ruleview-rule-unselected", this.onRuleUnselected);
    gDevTools.off("theme-switched", this.onThemeChanged);

    this.document = null;
    this.inspector = null;
    this.pageStyle = null;
    this.ruleView = null;
    this.selectedRule = null;
    this.store = null;
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
   * Returns true if the font inspector panel is visible, and false otherwise.
   */
  isPanelVisible() {
    return this.inspector.sidebar &&
           this.inspector.sidebar.getCurrentTabID() === "fontinspector";
  }

  /**
   * Live preview all CSS font property values from the fontEditor store on the page
   * and sync the changes to the Rule view.
   */
  applyChanges() {
    const fontEditor = this.store.getState().fontEditor;
    // Until registered axis values are supported as font property values,
    // write all axes and their values to font-variation-settings.
    // Bug 1449891: https://bugzilla.mozilla.org/show_bug.cgi?id=1449891
    const name = "font-variation-settings";
    const value = Object.keys(fontEditor.axes)
      .map(tag => `"${tag}" ${fontEditor.axes[tag]}`)
      .join(", ");

    let textProperty = this.selectedRule.textProps.filter(prop => prop.name === name)[0];
    if (!textProperty) {
      textProperty = this.selectedRule.editor.addProperty(name, value, "", true);
    }

    // Prevent reacting to changes we caused.
    this.ruleView.off("property-value-updated", this.onRuleUpdated);
    // Live preview font property changes on the page.
    this.selectedRule.previewPropertyValue(textProperty, value, "");
    // Sync Rule view with changes reflected on the page (throttled).
    this.syncChanges(textProperty, value);
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
    this.applyChanges();
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
    this.applyChanges();
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
      this.selectedRule = null;
      this.store.dispatch(toggleFontEditor(false));
      this.store.dispatch(resetFontEditor());
      this.ruleView.off("property-value-updated", this.onRuleUpdated);
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
    // Collect any expected font properties and their values from the selected rule.
    const properties = this.selectedRule.textProps.reduce((acc, prop) => {
      if (FONT_PROPERTIES.includes(prop.name)) {
        acc[prop.name] = prop.value;
      }

      return acc;
    }, {});

    const fontEditor = this.store.getState().fontEditor;

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
}

module.exports = FontInspector;
