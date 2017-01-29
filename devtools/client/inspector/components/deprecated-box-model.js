/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * THIS MODULE IS DEPRECATED.
 *
 * To continue any work related to the box model view, see the new react/redux
 * implementation in devtools/client/inspector/layout/.
 */

"use strict";

const {Task} = require("devtools/shared/task");
const {InplaceEditor, editableItem} =
      require("devtools/client/shared/inplace-editor");
const {ReflowFront} = require("devtools/shared/fronts/reflow");
const {LocalizationHelper} = require("devtools/shared/l10n");
const {getCssProperties} = require("devtools/shared/fronts/css-properties");
const {KeyCodes} = require("devtools/client/shared/keycodes");
const EditingSession = require("devtools/client/inspector/layout/utils/editing-session");

const STRINGS_URI = "devtools/client/locales/shared.properties";
const STRINGS_INSPECTOR = "devtools/shared/locales/styleinspector.properties";
const SHARED_L10N = new LocalizationHelper(STRINGS_URI);
const INSPECTOR_L10N = new LocalizationHelper(STRINGS_INSPECTOR);
const NUMERIC = /^-?[\d\.]+$/;
const LONG_TEXT_ROTATE_LIMIT = 3;

/**
 * The box model view
 * @param {InspectorPanel} inspector
 *        An instance of the inspector-panel currently loaded in the toolbox
 * @param {Document} document
 *        The document that will contain the box model view.
 */
function BoxModelView(inspector, document) {
  this.inspector = inspector;
  this.doc = document;
  this.wrapper = this.doc.getElementById("old-boxmodel-wrapper");
  this.container = this.doc.getElementById("old-boxmodel-container");
  this.expander = this.doc.getElementById("old-boxmodel-expander");
  this.sizeLabel = this.doc.querySelector(".old-boxmodel-size > span");
  this.sizeHeadingLabel = this.doc.getElementById("old-boxmodel-element-size");
  this._geometryEditorHighlighter = null;
  this._cssProperties = getCssProperties(inspector.toolbox);

  this.init();
}

BoxModelView.prototype = {
  init: function () {
    this.update = this.update.bind(this);

    this.onNewSelection = this.onNewSelection.bind(this);
    this.inspector.selection.on("new-node-front", this.onNewSelection);

    this.onNewNode = this.onNewNode.bind(this);
    this.inspector.sidebar.on("computedview-selected", this.onNewNode);

    this.onSidebarSelect = this.onSidebarSelect.bind(this);
    this.inspector.sidebar.on("select", this.onSidebarSelect);

    this.onToggleExpander = this.onToggleExpander.bind(this);
    this.expander.addEventListener("click", this.onToggleExpander);
    let header = this.doc.getElementById("old-boxmodel-header");
    header.addEventListener("dblclick", this.onToggleExpander);

    this.onFilterComputedView = this.onFilterComputedView.bind(this);
    this.inspector.on("computed-view-filtered",
      this.onFilterComputedView);

    this.onPickerStarted = this.onPickerStarted.bind(this);
    this.onMarkupViewLeave = this.onMarkupViewLeave.bind(this);
    this.onMarkupViewNodeHover = this.onMarkupViewNodeHover.bind(this);
    this.onWillNavigate = this.onWillNavigate.bind(this);
    this.onKeyDown = this.onKeyDown.bind(this);
    this.onLevelClick = this.onLevelClick.bind(this);
    this.setAriaActive = this.setAriaActive.bind(this);
    this.getEditBoxes = this.getEditBoxes.bind(this);
    this.makeFocusable = this.makeFocusable.bind(this);
    this.makeUnfocasable = this.makeUnfocasable.bind(this);
    this.moveFocus = this.moveFocus.bind(this);
    this.onFocus = this.onFocus.bind(this);

    this.borderLayout = this.doc.getElementById("old-boxmodel-borders");
    this.boxModel = this.doc.getElementById("old-boxmodel-wrapper");
    this.marginLayout = this.doc.getElementById("old-boxmodel-margins");
    this.paddingLayout = this.doc.getElementById("old-boxmodel-padding");

    this.layouts = {
      "margin": new Map([
        [KeyCodes.DOM_VK_ESCAPE, this.marginLayout],
        [KeyCodes.DOM_VK_DOWN, this.borderLayout],
        [KeyCodes.DOM_VK_UP, null],
        ["click", this.marginLayout]
      ]),
      "border": new Map([
        [KeyCodes.DOM_VK_ESCAPE, this.borderLayout],
        [KeyCodes.DOM_VK_DOWN, this.paddingLayout],
        [KeyCodes.DOM_VK_UP, this.marginLayout],
        ["click", this.borderLayout]
      ]),
      "padding": new Map([
        [KeyCodes.DOM_VK_ESCAPE, this.paddingLayout],
        [KeyCodes.DOM_VK_DOWN, null],
        [KeyCodes.DOM_VK_UP, this.borderLayout],
        ["click", this.paddingLayout]
      ])
    };

    this.boxModel.addEventListener("click", this.onLevelClick, true);
    this.boxModel.addEventListener("focus", this.onFocus, true);
    this.boxModel.addEventListener("keydown", this.onKeyDown, true);

    this.initBoxModelHighlighter();

    // Store for the different dimensions of the node.
    // 'selector' refers to the element that holds the value;
    // 'property' is what we are measuring;
    // 'value' is the computed dimension, computed in update().
    this.map = {
      position: {
        selector: "#old-boxmodel-element-position",
        property: "position",
        value: undefined
      },
      marginTop: {
        selector: ".old-boxmodel-margin.old-boxmodel-top > span",
        property: "margin-top",
        value: undefined
      },
      marginBottom: {
        selector: ".old-boxmodel-margin.old-boxmodel-bottom > span",
        property: "margin-bottom",
        value: undefined
      },
      marginLeft: {
        selector: ".old-boxmodel-margin.old-boxmodel-left > span",
        property: "margin-left",
        value: undefined
      },
      marginRight: {
        selector: ".old-boxmodel-margin.old-boxmodel-right > span",
        property: "margin-right",
        value: undefined
      },
      paddingTop: {
        selector: ".old-boxmodel-padding.old-boxmodel-top > span",
        property: "padding-top",
        value: undefined
      },
      paddingBottom: {
        selector: ".old-boxmodel-padding.old-boxmodel-bottom > span",
        property: "padding-bottom",
        value: undefined
      },
      paddingLeft: {
        selector: ".old-boxmodel-padding.old-boxmodel-left > span",
        property: "padding-left",
        value: undefined
      },
      paddingRight: {
        selector: ".old-boxmodel-padding.old-boxmodel-right > span",
        property: "padding-right",
        value: undefined
      },
      borderTop: {
        selector: ".old-boxmodel-border.old-boxmodel-top > span",
        property: "border-top-width",
        value: undefined
      },
      borderBottom: {
        selector: ".old-boxmodel-border.old-boxmodel-bottom > span",
        property: "border-bottom-width",
        value: undefined
      },
      borderLeft: {
        selector: ".old-boxmodel-border.old-boxmodel-left > span",
        property: "border-left-width",
        value: undefined
      },
      borderRight: {
        selector: ".old-boxmodel-border.old-boxmodel-right > span",
        property: "border-right-width",
        value: undefined
      }
    };

    // Make each element the dimensions editable
    for (let i in this.map) {
      if (i == "position") {
        continue;
      }

      let dimension = this.map[i];
      editableItem({
        element: this.doc.querySelector(dimension.selector)
      }, (element, event) => {
        this.initEditor(element, event, dimension);
      });
    }

    this.onNewNode();

    let nodeGeometry = this.doc.getElementById("old-layout-geometry-editor");
    this.onGeometryButtonClick = this.onGeometryButtonClick.bind(this);
    nodeGeometry.addEventListener("click", this.onGeometryButtonClick);
  },

  initBoxModelHighlighter: function () {
    let highlightElts = this.doc.querySelectorAll("#old-boxmodel-container *[title]");
    this.onHighlightMouseOver = this.onHighlightMouseOver.bind(this);
    this.onHighlightMouseOut = this.onHighlightMouseOut.bind(this);

    for (let element of highlightElts) {
      element.addEventListener("mouseover", this.onHighlightMouseOver, true);
      element.addEventListener("mouseout", this.onHighlightMouseOut, true);
    }
  },

  /**
   * Start listening to reflows in the current tab.
   */
  trackReflows: function () {
    if (!this.reflowFront) {
      let { target } = this.inspector;
      if (target.form.reflowActor) {
        this.reflowFront = ReflowFront(target.client,
                                       target.form);
      } else {
        return;
      }
    }

    this.reflowFront.on("reflows", this.update);
    this.reflowFront.start();
  },

  /**
   * Stop listening to reflows in the current tab.
   */
  untrackReflows: function () {
    if (!this.reflowFront) {
      return;
    }

    this.reflowFront.off("reflows", this.update);
    this.reflowFront.stop();
  },

  /**
   * Called when the user clicks on one of the editable values in the box model view
   */
  initEditor: function (element, event, dimension) {
    let { property } = dimension;
    let session = new EditingSession(this);
    let initialValue = session.getProperty(property);

    let editor = new InplaceEditor({
      element: element,
      initial: initialValue,
      contentType: InplaceEditor.CONTENT_TYPES.CSS_VALUE,
      property: {
        name: dimension.property
      },
      start: self => {
        self.elt.parentNode.classList.add("old-boxmodel-editing");
      },
      change: value => {
        if (NUMERIC.test(value)) {
          value += "px";
        }

        let properties = [
          { name: property, value: value }
        ];

        if (property.substring(0, 7) == "border-") {
          let bprop = property.substring(0, property.length - 5) + "style";
          let style = session.getProperty(bprop);
          if (!style || style == "none" || style == "hidden") {
            properties.push({ name: bprop, value: "solid" });
          }
        }

        session.setProperties(properties).catch(e => console.error(e));
      },
      done: (value, commit) => {
        editor.elt.parentNode.classList.remove("old-boxmodel-editing");
        if (!commit) {
          session.revert().then(() => {
            session.destroy();
          }, e => console.error(e));
        }
      },
      contextMenu: this.inspector.onTextBoxContextMenu,
      cssProperties: this._cssProperties
    }, event);
  },

  /**
   * Is the BoxModelView visible in the sidebar.
   * @return {Boolean}
   */
  isViewVisible: function () {
    return this.inspector &&
           this.inspector.sidebar.getCurrentTabID() == "computedview";
  },

  /**
   * Is the BoxModelView visible in the sidebar and is the current node valid to
   * be displayed in the view.
   * @return {Boolean}
   */
  isViewVisibleAndNodeValid: function () {
    return this.isViewVisible() &&
           this.inspector.selection.isConnected() &&
           this.inspector.selection.isElementNode();
  },

  /**
   * Destroy the nodes. Remove listeners.
   */
  destroy: function () {
    let highlightElts = this.doc.querySelectorAll("#old-boxmodel-container *[title]");

    for (let element of highlightElts) {
      element.removeEventListener("mouseover", this.onHighlightMouseOver, true);
      element.removeEventListener("mouseout", this.onHighlightMouseOut, true);
    }

    this.expander.removeEventListener("click", this.onToggleExpander);
    let header = this.doc.getElementById("old-boxmodel-header");
    header.removeEventListener("dblclick", this.onToggleExpander);

    let nodeGeometry = this.doc.getElementById("old-layout-geometry-editor");
    nodeGeometry.removeEventListener("click", this.onGeometryButtonClick);

    this.boxModel.removeEventListener("click", this.onLevelClick, true);
    this.boxModel.removeEventListener("focus", this.onFocus, true);
    this.boxModel.removeEventListener("keydown", this.onKeyDown, true);

    this.inspector.off("picker-started", this.onPickerStarted);

    // Inspector Panel will destroy `markup` object on "will-navigate" event,
    // therefore we have to check if it's still available in case BoxModelView
    // is destroyed immediately after.
    if (this.inspector.markup) {
      this.inspector.markup.off("leave", this.onMarkupViewLeave);
      this.inspector.markup.off("node-hover", this.onMarkupViewNodeHover);
    }

    this.inspector.sidebar.off("computedview-selected", this.onNewNode);
    this.inspector.selection.off("new-node-front", this.onNewSelection);
    this.inspector.sidebar.off("select", this.onSidebarSelect);
    this.inspector.target.off("will-navigate", this.onWillNavigate);
    this.inspector.off("computed-view-filtered", this.onFilterComputedView);

    this.inspector = null;
    this.doc = null;
    this.wrapper = null;
    this.container = null;
    this.expander = null;
    this.sizeLabel = null;
    this.sizeHeadingLabel = null;

    this.marginLayout = null;
    this.borderLayout = null;
    this.paddingLayout = null;
    this.boxModel = null;
    this.layouts = null;

    if (this.reflowFront) {
      this.untrackReflows();
      this.reflowFront.destroy();
      this.reflowFront = null;
    }
  },

  /**
   * Set initial box model focus to the margin layout.
   */
  onFocus: function () {
    let activeDescendant = this.boxModel.getAttribute("aria-activedescendant");

    if (!activeDescendant) {
      let nextLayout = this.marginLayout;
      this.setAriaActive(nextLayout);
    }
  },

  /**
   * Active aria-level set to current layout.
   *
   * @param {Element} nextLayout
   *        Element of next layout that user has navigated to
   * @param {Node} target
   *        Node to be observed
   */
  setAriaActive: function (nextLayout, target) {
    this.boxModel.setAttribute("aria-activedescendant", nextLayout.id);
    if (target && target._editable) {
      target.blur();
    }

    // Clear all
    this.marginLayout.classList.remove("layout-active-elm");
    this.borderLayout.classList.remove("layout-active-elm");
    this.paddingLayout.classList.remove("layout-active-elm");

    // Set the next level's border outline
    nextLayout.classList.add("layout-active-elm");
  },

  /**
   * Update aria-active on mouse click.
   *
   * @param {Event} event
   *         The event triggered by a mouse click on the box model
   */
  onLevelClick: function (event) {
    let {target} = event;
    let nextLayout = this.layouts[target.getAttribute("data-box")].get("click");

    this.setAriaActive(nextLayout, target);
  },

  /**
   * Handle keyboard navigation and focus for box model layouts.
   *
   * Updates active layout on arrow key navigation
   * Focuses next layout's editboxes on enter key
   * Unfocuses current layout's editboxes when active layout changes
   * Controls tabbing between editBoxes
   *
   * @param {Event} event
   *         The event triggered by a keypress on the box model
   */
  onKeyDown: function (event) {
    let {target, keyCode} = event;
    // If focused on editable value or in editing mode
    let isEditable = target._editable || target.editor;
    let level = this.boxModel.getAttribute("aria-activedescendant");
    let editingMode = target.tagName === "input";
    let nextLayout;

    switch (keyCode) {
      case KeyCodes.DOM_VK_RETURN:
        if (!isEditable) {
          this.makeFocusable(level);
        }
        break;
      case KeyCodes.DOM_VK_DOWN:
      case KeyCodes.DOM_VK_UP:
        if (!editingMode) {
          event.preventDefault();
          this.makeUnfocasable(level);
          let datalevel = this.doc.getElementById(level).getAttribute("data-box");
          nextLayout = this.layouts[datalevel].get(keyCode);
          this.boxModel.focus();
        }
        break;
      case KeyCodes.DOM_VK_TAB:
        if (isEditable) {
          event.preventDefault();
          this.moveFocus(event, level);
        }
        break;
      case KeyCodes.DOM_VK_ESCAPE:
        if (isEditable && target._editable) {
          event.preventDefault();
          event.stopPropagation();
          this.makeUnfocasable(level);
          this.boxModel.focus();
        }
        break;
      default:
        break;
    }

    if (nextLayout) {
      this.setAriaActive(nextLayout, target);
    }
  },

  /**
   * Make previous layout's elements unfocusable.
   *
   * @param {String} editLevel
   *        The previous layout
   */
  makeUnfocasable: function (editLevel) {
    let editBoxes = this.getEditBoxes(editLevel);
    editBoxes.forEach(editBox => editBox.setAttribute("tabindex", "-1"));
  },

  /**
   * Make current layout's elements focusable.
   *
   * @param {String} editLevel
   *        The current layout
   */
  makeFocusable: function (editLevel) {
    let editBoxes = this.getEditBoxes(editLevel);
    editBoxes.forEach(editBox => editBox.setAttribute("tabindex", "0"));
    editBoxes[0].focus();
  },

  /**
   * Keyboard navigation of edit boxes wraps around on edge
   * elements ([layout]-top, [layout]-left).
   *
   * @param {Node} target
   *        Node to be observed
   * @param {Boolean} shiftKey
   *        Determines if shiftKey was pressed
   * @param {String} level
   *        Current active layout
   */
  moveFocus: function ({target, shiftKey}, level) {
    let editBoxes = this.getEditBoxes(level);
    let editingMode = target.tagName === "input";
    // target.nextSibling is input field
    let position = editingMode ? editBoxes.indexOf(target.nextSibling)
                               : editBoxes.indexOf(target);

    if (position === editBoxes.length - 1 && !shiftKey) {
      position = 0;
    } else if (position === 0 && shiftKey) {
      position = editBoxes.length - 1;
    } else {
      shiftKey ? position-- : position++;
    }

    let editBox = editBoxes[position];
    editBox.focus();

    if (editingMode) {
      editBox.click();
    }
  },

  /**
   * Retrieve edit boxes for current layout.
   *
   * @param {String} editLevel
   *        Current active layout
   * @return Layout's edit boxes
   */
  getEditBoxes: function (editLevel) {
    let dataLevel = this.doc.getElementById(editLevel).getAttribute("data-box");
    return [...this.doc.querySelectorAll(
      `[data-box="${dataLevel}"].old-boxmodel-editable`)];
  },

  onSidebarSelect: function (e, sidebar) {
    this.setActive(sidebar === "computedview");
  },

  /**
   * Selection 'new-node-front' event handler.
   */
  onNewSelection: function () {
    let done = this.inspector.updating("computed-view");
    this.onNewNode()
      .then(() => this.hideGeometryEditor())
      .then(done, (err) => {
        console.error(err);
        done();
      }).catch(console.error);
  },

  /**
   * @return a promise that resolves when the view has been updated
   */
  onNewNode: function () {
    this.setActive(this.isViewVisibleAndNodeValid());
    return this.update();
  },

  onHighlightMouseOver: function (e) {
    let region = e.target.getAttribute("data-box");
    if (!region) {
      return;
    }

    this.showBoxModel({
      region,
      showOnly: region,
      onlyRegionArea: true
    });
  },

  onHighlightMouseOut: function () {
    this.hideBoxModel();
  },

  onGeometryButtonClick: function ({target}) {
    if (target.hasAttribute("checked")) {
      target.removeAttribute("checked");
      this.hideGeometryEditor();
    } else {
      target.setAttribute("checked", "true");
      this.showGeometryEditor();
    }
  },

  onPickerStarted: function () {
    this.hideGeometryEditor();
  },

  onToggleExpander: function () {
    let isOpen = this.expander.hasAttribute("open");

    if (isOpen) {
      this.container.hidden = true;
      this.expander.removeAttribute("open");
    } else {
      this.container.hidden = false;
      this.expander.setAttribute("open", "");
    }
  },

  onMarkupViewLeave: function () {
    this.showGeometryEditor(true);
  },

  onMarkupViewNodeHover: function () {
    this.hideGeometryEditor(false);
  },

  onWillNavigate: function () {
    this._geometryEditorHighlighter.release().catch(console.error);
    this._geometryEditorHighlighter = null;
  },

  /**
   * Event handler that responds to the computed view being filtered
   * @param {String} reason
   * @param {Boolean} hidden
   *        Whether or not to hide the box model wrapper
   */
  onFilterComputedView: function (reason, hidden) {
    this.wrapper.hidden = hidden;
  },

  /**
   * Stop tracking reflows and hide all values when no node is selected or the
   * box model view is hidden, otherwise track reflows and show values.
   * @param {Boolean} isActive
   */
  setActive: function (isActive) {
    if (isActive === this.isActive) {
      return;
    }
    this.isActive = isActive;

    if (isActive) {
      this.trackReflows();
    } else {
      this.untrackReflows();
    }
  },

  /**
   * Compute the dimensions of the node and update the values in
   * the inspector.xul document.
   * @return a promise that will be resolved when complete.
   */
  update: function () {
    let lastRequest = Task.spawn((function* () {
      if (!this.isViewVisibleAndNodeValid()) {
        this.wrapper.hidden = true;
        this.inspector.emit("boxmodel-view-updated");
        return null;
      }

      let node = this.inspector.selection.nodeFront;
      let layout = yield this.inspector.pageStyle.getLayout(node, {
        autoMargins: this.isActive
      });
      let styleEntries = yield this.inspector.pageStyle.getApplied(node, {});

      yield this.updateGeometryButton();

      // If a subsequent request has been made, wait for that one instead.
      if (this._lastRequest != lastRequest) {
        return this._lastRequest;
      }

      this._lastRequest = null;
      let width = layout.width;
      let height = layout.height;
      let newLabel = SHARED_L10N.getFormatStr("dimensions", width, height);

      if (this.sizeHeadingLabel.textContent != newLabel) {
        this.sizeHeadingLabel.textContent = newLabel;
      }

      for (let i in this.map) {
        let property = this.map[i].property;
        if (!(property in layout)) {
          // Depending on the actor version, some properties
          // might be missing.
          continue;
        }
        let parsedValue = parseFloat(layout[property]);
        if (Number.isNaN(parsedValue)) {
          // Not a number. We use the raw string.
          // Useful for "position" for example.
          this.map[i].value = layout[property];
        } else {
          this.map[i].value = parsedValue;
        }
      }

      let margins = layout.autoMargins;
      if ("top" in margins) {
        this.map.marginTop.value = "auto";
      }
      if ("right" in margins) {
        this.map.marginRight.value = "auto";
      }
      if ("bottom" in margins) {
        this.map.marginBottom.value = "auto";
      }
      if ("left" in margins) {
        this.map.marginLeft.value = "auto";
      }

      for (let i in this.map) {
        let selector = this.map[i].selector;
        let span = this.doc.querySelector(selector);
        this.updateSourceRuleTooltip(span, this.map[i].property, styleEntries);
        if (span.textContent.length > 0 &&
            span.textContent == this.map[i].value) {
          continue;
        }
        span.textContent = this.map[i].value;
        this.manageOverflowingText(span);
      }

      width -= this.map.borderLeft.value + this.map.borderRight.value +
               this.map.paddingLeft.value + this.map.paddingRight.value;
      width = parseFloat(width.toPrecision(6));
      height -= this.map.borderTop.value + this.map.borderBottom.value +
                this.map.paddingTop.value + this.map.paddingBottom.value;
      height = parseFloat(height.toPrecision(6));

      let newValue = width + "\u00D7" + height;
      if (this.sizeLabel.textContent != newValue) {
        this.sizeLabel.textContent = newValue;
      }

      this.elementRules = styleEntries.map(e => e.rule);

      this.wrapper.hidden = false;

      this.inspector.emit("boxmodel-view-updated");
      return null;
    }).bind(this)).catch(console.error);

    this._lastRequest = lastRequest;
    return this._lastRequest;
  },

  /**
   * Update the text in the tooltip shown when hovering over a value to provide
   * information about the source CSS rule that sets this value.
   * @param {DOMNode} el The element that will receive the tooltip.
   * @param {String} property The name of the CSS property for the tooltip.
   * @param {Array} rules An array of applied rules retrieved by
   * styleActor.getApplied.
   */
  updateSourceRuleTooltip: function (el, property, rules) {
    // Dummy element used to parse the cssText of applied rules.
    let dummyEl = this.doc.createElement("div");

    // Rules are in order of priority so iterate until we find the first that
    // defines a value for the property.
    let sourceRule, value;
    for (let {rule} of rules) {
      dummyEl.style.cssText = rule.cssText;
      value = dummyEl.style.getPropertyValue(property);
      if (value !== "") {
        sourceRule = rule;
        break;
      }
    }

    let title = property;
    if (sourceRule && sourceRule.selectors) {
      title += "\n" + sourceRule.selectors.join(", ");
    }
    if (sourceRule && sourceRule.parentStyleSheet) {
      if (sourceRule.parentStyleSheet.href) {
        title += "\n" + sourceRule.parentStyleSheet.href + ":" + sourceRule.line;
      } else {
        title += "\n" + INSPECTOR_L10N.getStr("rule.sourceInline") +
          ":" + sourceRule.line;
      }
    }

    el.setAttribute("title", title);
  },

  /**
   * Show the box-model highlighter on the currently selected element
   * @param {Object} options Options passed to the highlighter actor
   */
  showBoxModel: function (options = {}) {
    let toolbox = this.inspector.toolbox;
    let nodeFront = this.inspector.selection.nodeFront;

    toolbox.highlighterUtils.highlightNodeFront(nodeFront, options);
  },

  /**
   * Hide the box-model highlighter on the currently selected element
   */
  hideBoxModel: function () {
    let toolbox = this.inspector.toolbox;

    toolbox.highlighterUtils.unhighlight();
  },

  /**
   * Show the geometry editor highlighter on the currently selected element
   * @param {Boolean} [showOnlyIfActive=false]
   *   Indicates if the Geometry Editor should be shown only if it's active but
   *   hidden.
   */
  showGeometryEditor: function (showOnlyIfActive = false) {
    let toolbox = this.inspector.toolbox;
    let nodeFront = this.inspector.selection.nodeFront;
    let nodeGeometry = this.doc.getElementById("old-layout-geometry-editor");
    let isActive = nodeGeometry.hasAttribute("checked");

    if (showOnlyIfActive && !isActive) {
      return;
    }

    if (this._geometryEditorHighlighter) {
      this._geometryEditorHighlighter.show(nodeFront).catch(console.error);
      return;
    }

    // instantiate Geometry Editor highlighter
    toolbox.highlighterUtils
      .getHighlighterByType("GeometryEditorHighlighter").then(highlighter => {
        highlighter.show(nodeFront).catch(console.error);
        this._geometryEditorHighlighter = highlighter;

        // Hide completely the geometry editor if the picker is clicked
        toolbox.on("picker-started", this.onPickerStarted);

        // Temporary hide the geometry editor
        this.inspector.markup.on("leave", this.onMarkupViewLeave);
        this.inspector.markup.on("node-hover", this.onMarkupViewNodeHover);

        // Release the actor on will-navigate event
        this.inspector.target.once("will-navigate", this.onWillNavigate);
      });
  },

  /**
   * Hide the geometry editor highlighter on the currently selected element
   * @param {Boolean} [updateButton=true]
   *   Indicates if the Geometry Editor's button needs to be unchecked too
   */
  hideGeometryEditor: function (updateButton = true) {
    if (this._geometryEditorHighlighter) {
      this._geometryEditorHighlighter.hide().catch(console.error);
    }

    if (updateButton) {
      let nodeGeometry = this.doc.getElementById("old-layout-geometry-editor");
      nodeGeometry.removeAttribute("checked");
    }
  },

  /**
   * Update the visibility and the state of the geometry editor button,
   * based on the selected node.
   */
  updateGeometryButton: Task.async(function* () {
    let node = this.inspector.selection.nodeFront;
    let isEditable = false;

    if (node) {
      isEditable = yield this.inspector.pageStyle.isPositionEditable(node);
    }

    let nodeGeometry = this.doc.getElementById("old-layout-geometry-editor");
    nodeGeometry.style.visibility = isEditable ? "visible" : "hidden";
  }),

  manageOverflowingText: function (span) {
    let classList = span.parentNode.classList;

    if (classList.contains("old-boxmodel-left") ||
        classList.contains("old-boxmodel-right")) {
      let force = span.textContent.length > LONG_TEXT_ROTATE_LIMIT;
      classList.toggle("old-boxmodel-rotate", force);
    }
  }
};

module.exports = BoxModelView;
