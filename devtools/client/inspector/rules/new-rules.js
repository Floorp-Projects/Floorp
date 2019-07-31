/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const ElementStyle = require("devtools/client/inspector/rules/models/element-style");
const {
  createFactory,
  createElement,
} = require("devtools/client/shared/vendor/react");
const { Provider } = require("devtools/client/shared/vendor/react-redux");
const EventEmitter = require("devtools/shared/event-emitter");

const {
  updateClasses,
  updateClassPanelExpanded,
} = require("./actions/class-list");
const {
  disableAllPseudoClasses,
  setPseudoClassLocks,
  togglePseudoClass,
} = require("./actions/pseudo-classes");
const {
  updateAddRuleEnabled,
  updateHighlightedSelector,
  updatePrintSimulationHidden,
  updateRules,
  updateSourceLinkEnabled,
} = require("./actions/rules");

const RulesApp = createFactory(require("./components/RulesApp"));

const { LocalizationHelper } = require("devtools/shared/l10n");
const INSPECTOR_L10N = new LocalizationHelper(
  "devtools/client/locales/inspector.properties"
);

loader.lazyRequireGetter(this, "Tools", "devtools/client/definitions", true);
loader.lazyRequireGetter(
  this,
  "gDevTools",
  "devtools/client/framework/devtools",
  true
);
loader.lazyRequireGetter(
  this,
  "ClassList",
  "devtools/client/inspector/rules/models/class-list"
);
loader.lazyRequireGetter(
  this,
  "advanceValidate",
  "devtools/client/inspector/shared/utils",
  true
);
loader.lazyRequireGetter(
  this,
  "AutocompletePopup",
  "devtools/client/shared/autocomplete-popup"
);
loader.lazyRequireGetter(
  this,
  "InplaceEditor",
  "devtools/client/shared/inplace-editor",
  true
);

const PREF_UA_STYLES = "devtools.inspector.showUserAgentStyles";

class RulesView {
  constructor(inspector, window) {
    this.cssProperties = inspector.cssProperties;
    this.doc = window.document;
    this.inspector = inspector;
    this.pageStyle = inspector.pageStyle;
    this.selection = inspector.selection;
    this.store = inspector.store;
    this.telemetry = inspector.telemetry;
    this.toolbox = inspector.toolbox;
    this.isNewRulesView = true;

    this.showUserAgentStyles = Services.prefs.getBoolPref(PREF_UA_STYLES);

    this.onAddClass = this.onAddClass.bind(this);
    this.onAddRule = this.onAddRule.bind(this);
    this.onOpenSourceLink = this.onOpenSourceLink.bind(this);
    this.onSelection = this.onSelection.bind(this);
    this.onSetClassState = this.onSetClassState.bind(this);
    this.onToggleClassPanelExpanded = this.onToggleClassPanelExpanded.bind(
      this
    );
    this.onToggleDeclaration = this.onToggleDeclaration.bind(this);
    this.onTogglePrintSimulation = this.onTogglePrintSimulation.bind(this);
    this.onTogglePseudoClass = this.onTogglePseudoClass.bind(this);
    this.onToolChanged = this.onToolChanged.bind(this);
    this.onToggleSelectorHighlighter = this.onToggleSelectorHighlighter.bind(
      this
    );
    this.showDeclarationNameEditor = this.showDeclarationNameEditor.bind(this);
    this.showDeclarationValueEditor = this.showDeclarationValueEditor.bind(
      this
    );
    this.showNewDeclarationEditor = this.showNewDeclarationEditor.bind(this);
    this.showSelectorEditor = this.showSelectorEditor.bind(this);
    this.updateClassList = this.updateClassList.bind(this);
    this.updateRules = this.updateRules.bind(this);

    this.inspector.sidebar.on("select", this.onSelection);
    this.selection.on("detached-front", this.onSelection);
    this.selection.on("new-node-front", this.onSelection);
    this.toolbox.on("tool-registered", this.onToolChanged);
    this.toolbox.on("tool-unregistered", this.onToolChanged);

    this.init();

    EventEmitter.decorate(this);
  }

  init() {
    if (!this.inspector) {
      return;
    }

    const rulesApp = RulesApp({
      onAddClass: this.onAddClass,
      onAddRule: this.onAddRule,
      onOpenSourceLink: this.onOpenSourceLink,
      onSetClassState: this.onSetClassState,
      onToggleClassPanelExpanded: this.onToggleClassPanelExpanded,
      onToggleDeclaration: this.onToggleDeclaration,
      onTogglePrintSimulation: this.onTogglePrintSimulation,
      onTogglePseudoClass: this.onTogglePseudoClass,
      onToggleSelectorHighlighter: this.onToggleSelectorHighlighter,
      showDeclarationNameEditor: this.showDeclarationNameEditor,
      showDeclarationValueEditor: this.showDeclarationValueEditor,
      showNewDeclarationEditor: this.showNewDeclarationEditor,
      showSelectorEditor: this.showSelectorEditor,
    });

    this.initPrintSimulation();

    const provider = createElement(
      Provider,
      {
        id: "ruleview",
        key: "ruleview",
        store: this.store,
        title: INSPECTOR_L10N.getStr("inspector.sidebar.ruleViewTitle"),
      },
      rulesApp
    );

    // Exposes the provider to let inspector.js use it in setupSidebar.
    this.provider = provider;
  }

  async initPrintSimulation() {
    const target = this.inspector.target;

    // In order to query if the emulation actor's print simulation methods are supported,
    // we have to call the emulation front so that the actor is lazily loaded. This allows
    // us to use `actorHasMethod`. Please see `getActorDescription` for more information.
    this.emulationFront = await target.getFront("emulation");

    // Show the toggle button if:
    // - Print simulation is supported for the current target.
    // - Not debugging content document.
    if (
      (await target.actorHasMethod(
        "emulation",
        "getIsPrintSimulationEnabled"
      )) &&
      !target.chrome
    ) {
      this.store.dispatch(updatePrintSimulationHidden(false));
    } else {
      this.store.dispatch(updatePrintSimulationHidden(true));
    }
  }

  destroy() {
    this.inspector.sidebar.off("select", this.onSelection);
    this.selection.off("detached-front", this.onSelection);
    this.selection.off("new-node-front", this.onSelection);
    this.toolbox.off("tool-registered", this.onToolChanged);
    this.toolbox.off("tool-unregistered", this.onToolChanged);

    if (this._autocompletePopup) {
      this._autocompletePopup.destroy();
      this._autocompletePopup = null;
    }

    if (this._classList) {
      this._classList.off("current-node-class-changed", this.refreshClassList);
      this._classList.destroy();
      this._classList = null;
    }

    if (this._selectHighlighter) {
      this._selectorHighlighter.finalize();
      this._selectorHighlighter = null;
    }

    if (this.elementStyle) {
      this.elementStyle.destroy();
      this.elementStyle = null;
    }

    if (this.emulationFront) {
      this.emulationFront.destroy();
      this.emulationFront = null;
    }

    this._dummyElement = null;
    this.cssProperties = null;
    this.doc = null;
    this.inspector = null;
    this.pageStyle = null;
    this.selection = null;
    this.showUserAgentStyles = null;
    this.store = null;
    this.telemetry = null;
    this.toolbox = null;
  }

  /**
   * Get an instance of the AutocompletePopup.
   *
   * @return {AutocompletePopup}
   */
  get autocompletePopup() {
    if (!this._autocompletePopup) {
      this._autocompletePopup = new AutocompletePopup(this.doc, {
        autoSelect: true,
        theme: "auto",
      });
    }

    return this._autocompletePopup;
  }

  /**
   * Get an instance of the ClassList model used to manage the list of CSS classes
   * applied to the element.
   *
   * @return {ClassList} used to manage the list of CSS classes applied to the element.
   */
  get classList() {
    if (!this._classList) {
      this._classList = new ClassList(this.inspector);
    }

    return this._classList;
  }
  /**
   * Creates a dummy element in the document that helps get the computed style in
   * TextProperty.
   *
   * @return {Element} used to get the computed style for text properties.
   */
  get dummyElement() {
    // To figure out how shorthand properties are interpreted by the
    // engine, we will set properties on a dummy element and observe
    // how their .style attribute reflects them as computed values.
    if (!this._dummyElement) {
      this._dummyElement = this.doc.createElement("div");
    }

    return this._dummyElement;
  }

  /**
   * Get the highlighters overlay from the Inspector.
   *
   * @return {HighlighterOverlay}.
   */
  get highlighters() {
    return this.inspector.highlighters;
  }

  /**
   * Returns the grid line names of the grid that the currently selected element is
   * contained in.
   *
   * @return {Object} that contains the names of the cols and rows as arrays
   * { cols: [], rows: [] }.
   */
  async getGridlineNames() {
    const gridLineNames = { cols: [], rows: [] };
    const layoutInspector = await this.inspector.walker.getLayoutInspector();
    const gridFront = await layoutInspector.getCurrentGrid(
      this.selection.nodeFront
    );

    if (gridFront) {
      const gridFragments = gridFront.gridFragments;

      for (const gridFragment of gridFragments) {
        for (const rowLine of gridFragment.rows.lines) {
          gridLineNames.rows = gridLineNames.rows.concat(rowLine.names);
        }
        for (const colLine of gridFragment.cols.lines) {
          gridLineNames.cols = gridLineNames.cols.concat(colLine.names);
        }
      }
    }

    // This event is emitted for testing purposes.
    this.inspector.emit("grid-line-names-updated");
    return gridLineNames;
  }

  /**
   * Get an instance of SelectorHighlighter (used to highlight nodes that match
   * selectors in the rule-view).
   *
   * @return {Promise} resolves to the instance of the highlighter.
   */
  async getSelectorHighlighter() {
    if (!this.inspector) {
      return null;
    }

    if (this._selectorHighlighter) {
      return this._selectorHighlighter;
    }

    try {
      const front = this.inspector.inspectorFront;
      this._selectorHighlighter = await front.getHighlighterByType(
        "SelectorHighlighter"
      );
      return this._selectorHighlighter;
    } catch (e) {
      // The SelectorHighlighter type could not be created in the
      // current target. It could be an older server, or a XUL page.
      return null;
    }
  }

  /**
   * Returns true if the rules panel is visible, and false otherwise.
   */
  isPanelVisible() {
    return (
      this.inspector &&
      this.inspector.toolbox &&
      this.inspector.sidebar &&
      this.inspector.toolbox.currentToolId === "inspector" &&
      this.inspector.sidebar.getCurrentTabID() === "newruleview"
    );
  }

  /**
   * Handler for adding the given CSS class value to the current element's class list.
   *
   * @param  {String} value
   *         The string that contains all classes.
   */
  async onAddClass(value) {
    await this.classList.addClassName(value);
    this.updateClassList();
  }

  /**
   * Handler for adding a new CSS rule.
   */
  async onAddRule() {
    await this.elementStyle.addNewRule();
  }

  /**
   * Handler for opening the source link of the given rule in the Style Editor.
   *
   * @param  {String} ruleId
   *         The id of the Rule for opening the source link.
   */
  async onOpenSourceLink(ruleId) {
    const rule = this.elementStyle.getRule(ruleId);
    if (!rule || !Tools.styleEditor.isTargetSupported(this.inspector.target)) {
      return;
    }

    const toolbox = await gDevTools.showToolbox(
      this.inspector.target,
      "styleeditor"
    );
    const styleEditor = toolbox.getCurrentPanel();
    if (!styleEditor) {
      return;
    }

    const { url, line, column } = rule.sourceLocation;
    styleEditor.selectStyleSheet(url, line, column);
  }

  /**
   * Handler for selection events "detached-front" and "new-node-front" and inspector
   * sidbar "select" event. Updates the rules view with the selected node if the panel
   * is visible.
   */
  onSelection() {
    if (!this.isPanelVisible()) {
      return;
    }

    if (!this.selection.isConnected() || !this.selection.isElementNode()) {
      this.update();
      return;
    }

    this.update(this.selection.nodeFront);
  }

  /**
   * Handler for toggling a CSS class from the current element's class list. Sets the
   * state of given CSS class name to the given checked value.
   *
   * @param  {String} name
   *         The CSS class name.
   * @param  {Boolean} checked
   *         Whether or not the given CSS class is checked.
   */
  async onSetClassState(name, checked) {
    await this.classList.setClassState(name, checked);
    this.updateClassList();
  }

  /**
   * Handler for toggling the expanded property of the class list panel.
   *
   * @param  {Boolean} isClassPanelExpanded
   *         Whether or not the class list panel is expanded.
   */
  onToggleClassPanelExpanded(isClassPanelExpanded) {
    if (isClassPanelExpanded) {
      this.classList.on("current-node-class-changed", this.updateClassList);
    } else {
      this.classList.off("current-node-class-changed", this.updateClassList);
    }

    this.store.dispatch(updateClassPanelExpanded(isClassPanelExpanded));
  }

  /**
   * Handler for toggling the enabled property for a given CSS declaration.
   *
   * @param  {String} ruleId
   *         The Rule id of the given CSS declaration.
   * @param  {String} declarationId
   *         The TextProperty id for the CSS declaration.
   */
  onToggleDeclaration(ruleId, declarationId) {
    this.elementStyle.toggleDeclaration(ruleId, declarationId);
    this.telemetry.recordEvent("edit_rule", "ruleview", null, {
      session_id: this.toolbox.sessionId,
    });
  }

  /**
   * Handler for toggling print media simulation.
   */
  async onTogglePrintSimulation() {
    const enabled = await this.emulationFront.getIsPrintSimulationEnabled();

    if (!enabled) {
      await this.emulationFront.startPrintMediaSimulation();
    } else {
      await this.emulationFront.stopPrintMediaSimulation(false);
    }

    await this.updateElementStyle();
  }

  /**
   * Handler for toggling a pseudo class in the pseudo class panel. Toggles on and off
   * a given pseudo class value.
   *
   * @param  {String} value
   *         The pseudo class to toggle on or off.
   */
  onTogglePseudoClass(value) {
    this.store.dispatch(togglePseudoClass(value));
    this.inspector.togglePseudoClass(value);
  }

  /**
   * Handler for toggling the selector highlighter for the given selector.
   * Highlight/unhighlight all the nodes that match a given set of selectors inside the
   * document of the current selected node. Only one selector can be highlighted at a
   * time, so calling the method a second time with a different selector will first
   * unhighlight the previously highlighted nodes. Calling the method a second time with
   * the same select will unhighlight the highlighted nodes.
   *
   * @param  {String} selector
   *         The selector used to find nodes in the page.
   */
  async onToggleSelectorHighlighter(selector) {
    const highlighter = await this.getSelectorHighlighter();
    if (!highlighter) {
      return;
    }

    await highlighter.hide();

    if (selector !== this.highlighters.selectorHighlighterShown) {
      this.store.dispatch(updateHighlightedSelector(selector));

      await highlighter.show(this.selection.nodeFront, {
        hideInfoBar: true,
        hideGuides: true,
        selector,
      });

      this.highlighters.selectorHighlighterShown = selector;
      // This event is emitted for testing purposes.
      this.emit("ruleview-selectorhighlighter-toggled", true);
    } else {
      this.highlighters.selectorHighlighterShown = null;
      this.store.dispatch(updateHighlightedSelector(""));
      // This event is emitted for testing purposes.
      this.emit("ruleview-selectorhighlighter-toggled", false);
    }
  }

  /**
   * Handler for when the toolbox's tools are registered or unregistered.
   * The source links in the rules view should be enabled only while the
   * Style Editor is registered because that's where source links point to.
   */
  onToolChanged() {
    const prevIsSourceLinkEnabled = this.store.getState().rules
      .isSourceLinkEnabled;
    const isSourceLinkEnabled = this.toolbox.isToolRegistered("styleeditor");

    if (prevIsSourceLinkEnabled !== isSourceLinkEnabled) {
      this.store.dispatch(updateSourceLinkEnabled(isSourceLinkEnabled));
    }
  }

  /**
   * Handler for showing the inplace editor when an editable property name is clicked in
   * the rules view.
   *
   * @param  {DOMNode} element
   *         The declaration name span element to be edited.
   * @param  {String} ruleId
   *         The id of the Rule object to be edited.
   * @param  {String} declarationId
   *         The id of the TextProperty object to be edited.
   */
  showDeclarationNameEditor(element, ruleId, declarationId) {
    new InplaceEditor({
      advanceChars: ":",
      contentType: InplaceEditor.CONTENT_TYPES.CSS_PROPERTY,
      cssProperties: this.cssProperties,
      done: async (name, commit) => {
        if (!commit) {
          return;
        }

        await this.elementStyle.modifyDeclarationName(
          ruleId,
          declarationId,
          name
        );
        this.telemetry.recordEvent("edit_rule", "ruleview", null, {
          session_id: this.toolbox.sessionId,
        });
      },
      element,
      popup: this.autocompletePopup,
    });
  }

  /**
   * Handler for showing the inplace editor when an editable property value is clicked
   * in the rules view.
   *
   * @param  {DOMNode} element
   *         The declaration value span element to be edited.
   * @param  {String} ruleId
   *         The id of the Rule object to be edited.
   * @param  {String} declarationId
   *         The id of the TextProperty object to be edited.
   */
  showDeclarationValueEditor(element, ruleId, declarationId) {
    const rule = this.elementStyle.getRule(ruleId);
    if (!rule) {
      return;
    }

    const declaration = rule.getDeclaration(declarationId);
    if (!declaration) {
      return;
    }

    new InplaceEditor({
      advanceChars: advanceValidate,
      contentType: InplaceEditor.CONTENT_TYPES.CSS_VALUE,
      cssProperties: this.cssProperties,
      cssVariables:
        this.elementStyle.variablesMap.get(rule.pseudoElement) || [],
      defaultIncrement: declaration.name === "opacity" ? 0.1 : 1,
      done: async (value, commit) => {
        if (!commit || !value || !value.trim()) {
          return;
        }

        await this.elementStyle.modifyDeclarationValue(
          ruleId,
          declarationId,
          value
        );
        this.telemetry.recordEvent("edit_rule", "ruleview", null, {
          session_id: this.toolbox.sessionId,
        });
      },
      element,
      getGridLineNames: this.getGridlineNames,
      maxWidth: () => {
        // Return the width of the closest declaration container element.
        const containerElement = element.closest(".ruleview-propertycontainer");
        return containerElement.getBoundingClientRect().width;
      },
      multiline: true,
      popup: this.autocompletePopup,
      property: declaration,
    });
  }

  /**
   * Shows the new inplace editor for a new declaration.
   *
   * @param  {DOMNode} element
   *         A new declaration span element to be edited.
   * @param  {String} ruleId
   *         The id of the Rule object to be edited.
   * @param  {Function} callback
   *         A callback function that is called when the inplace editor is destroyed.
   */
  showNewDeclarationEditor(element, ruleId, callback) {
    new InplaceEditor({
      advanceChars: ":",
      contentType: InplaceEditor.CONTENT_TYPES.CSS_PROPERTY,
      cssProperties: this.cssProperties,
      destroy: () => {
        callback();
      },
      done: (value, commit) => {
        if (!commit || !value || !value.trim()) {
          return;
        }

        this.elementStyle.addNewDeclaration(ruleId, value);
        this.telemetry.recordEvent("edit_rule", "ruleview", null, {
          session_id: this.toolbox.sessionId,
        });
      },
      element,
      popup: this.autocompletePopup,
    });
  }

  /**
   * Shows the inplace editor for the a selector.
   *
   * @param  {DOMNode} element
   *         The selector's span element to show the inplace editor.
   * @param  {String} ruleId
   *         The id of the Rule to be modified.
   */
  showSelectorEditor(element, ruleId) {
    new InplaceEditor({
      element,
      done: async (value, commit) => {
        if (!value || !commit) {
          return;
        }

        // Hide the selector highlighter if it matches the selector being edited.
        if (this.highlighters.selectorHighlighterShown) {
          const selector = await this.elementStyle
            .getRule(ruleId)
            .getUniqueSelector();
          if (this.highlighters.selectorHighlighterShown === selector) {
            this.onToggleSelectorHighlighter(
              this.highlighters.selectorHighlighterShown
            );
          }
        }

        await this.elementStyle.modifySelector(ruleId, value);
      },
    });
  }

  /**
   * Updates the rules view by dispatching the new rules data of the newly selected
   * element. This is called when the rules view becomes visible or upon new node
   * selection.
   *
   * @param  {NodeFront|null} element
   *         The NodeFront of the current selected element.
   */
  async update(element) {
    if (this.elementStyle) {
      this.elementStyle.destroy();
    }

    if (!element) {
      this.store.dispatch(disableAllPseudoClasses());
      this.store.dispatch(updateAddRuleEnabled(false));
      this.store.dispatch(updateClasses([]));
      this.store.dispatch(updateRules([]));
      return;
    }

    this.elementStyle = new ElementStyle(
      element,
      this,
      {},
      this.pageStyle,
      this.showUserAgentStyles
    );
    this.elementStyle.onChanged = this.updateRules;

    await this.updateElementStyle();
  }

  /**
   * Updates the class list panel with the current list of CSS classes.
   */
  updateClassList() {
    this.store.dispatch(updateClasses(this.classList.currentClasses));
  }

  /**
   * Updates the list of rules for the selected element. This should be called after
   * ElementStyle is initialized or if the list of rules for the selected element needs
   * to be refresh (e.g. when print media simulation is toggled).
   */
  async updateElementStyle() {
    await this.elementStyle.populate();

    const isAddRuleEnabled =
      this.selection.isElementNode() && !this.selection.isAnonymousNode();
    this.store.dispatch(updateAddRuleEnabled(isAddRuleEnabled));
    this.store.dispatch(
      setPseudoClassLocks(this.elementStyle.element.pseudoClassLocks)
    );
    this.updateClassList();
    this.updateRules();
  }

  /**
   * Updates the rules view by dispatching the current rules state. This is called from
   * the update() function, and from the ElementStyle's onChange() handler.
   */
  updateRules() {
    this.store.dispatch(updateRules(this.elementStyle.rules));
  }
}

module.exports = RulesView;
