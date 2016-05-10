/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cc, Ci, Cu} = require("chrome");
const {Task} = require("resource://gre/modules/Task.jsm");
const {InplaceEditor, editableItem} =
      require("devtools/client/shared/inplace-editor");
const {ReflowFront} = require("devtools/server/actors/layout");
const {LocalizationHelper} = require("devtools/client/shared/l10n");

Cu.import("resource://devtools/client/shared/widgets/ViewHelpers.jsm");

const STRINGS_URI = "chrome://devtools/locale/shared.properties";
const SHARED_L10N = new LocalizationHelper(STRINGS_URI);
const NUMERIC = /^-?[\d\.]+$/;
const LONG_TEXT_ROTATE_LIMIT = 3;

/**
 * An instance of EditingSession tracks changes that have been made during the
 * modification of box model values. All of these changes can be reverted by
 * calling revert.
 *
 * @param doc    A DOM document that can be used to test style rules.
 * @param rules  An array of the style rules defined for the node being edited.
 *               These should be in order of priority, least important first.
 */
function EditingSession(doc, rules) {
  this._doc = doc;
  this._rules = rules;
  this._modifications = new Map();
}

EditingSession.prototype = {
  /**
   * Gets the value of a single property from the CSS rule.
   *
   * @param rule      The CSS rule
   * @param property  The name of the property
   */
  getPropertyFromRule: function (rule, property) {
    let dummyStyle = this._element.style;

    dummyStyle.cssText = rule.cssText;
    return dummyStyle.getPropertyValue(property);
  },

  /**
   * Returns the current value for a property as a string or the empty string if
   * no style rules affect the property.
   *
   * @param property  The name of the property as a string
   */
  getProperty: function (property) {
    // Create a hidden element for getPropertyFromRule to use
    let div = this._doc.createElement("div");
    div.setAttribute("style", "display: none");
    this._doc.getElementById("sidebar-panel-layoutview").appendChild(div);
    this._element = this._doc.createElement("p");
    div.appendChild(this._element);

    // As the rules are in order of priority we can just iterate until we find
    // the first that defines a value for the property and return that.
    for (let rule of this._rules) {
      let value = this.getPropertyFromRule(rule, property);
      if (value !== "") {
        div.remove();
        return value;
      }
    }
    div.remove();
    return "";
  },

  /**
   * Sets a number of properties on the node. Returns a promise that will be
   * resolved when the modifications are complete.
   *
   * @param properties  An array of properties, each is an object with name and
   *                    value properties. If the value is "" then the property
   *                    is removed.
   */
  setProperties: function (properties) {
    let modifications = this._rules[0].startModifyingProperties();

    for (let property of properties) {
      if (!this._modifications.has(property.name)) {
        this._modifications.set(property.name,
          this.getPropertyFromRule(this._rules[0], property.name));
      }

      if (property.value == "") {
        modifications.removeProperty(-1, property.name);
      } else {
        modifications.setProperty(-1, property.name, property.value, "");
      }
    }

    return modifications.apply().then(null, console.error);
  },

  /**
   * Reverts all of the property changes made by this instance. Returns a
   * promise that will be resolved when complete.
   */
  revert: function () {
    let modifications = this._rules[0].startModifyingProperties();

    for (let [property, value] of this._modifications) {
      if (value != "") {
        modifications.setProperty(-1, property, value, "");
      } else {
        modifications.removeProperty(-1, property);
      }
    }

    return modifications.apply().then(null, console.error);
  },

  destroy: function () {
    this._doc = null;
    this._rules = null;
    this._modifications.clear();
  }
};

/**
 * The layout-view panel
 * @param {InspectorPanel} inspector An instance of the inspector-panel
 * currently loaded in the toolbox
 * @param {Window} win The window containing the panel
 */
function LayoutView(inspector, win) {
  this.inspector = inspector;
  this.doc = win.document;
  this.sizeLabel = this.doc.querySelector(".layout-size > span");
  this.sizeHeadingLabel = this.doc.getElementById("layout-element-size");
  this._geometryEditorHighlighter = null;

  this.init();
}

LayoutView.prototype = {
  init: function () {
    this.update = this.update.bind(this);

    this.onNewSelection = this.onNewSelection.bind(this);
    this.inspector.selection.on("new-node-front", this.onNewSelection);

    this.onNewNode = this.onNewNode.bind(this);
    this.inspector.sidebar.on("layoutview-selected", this.onNewNode);

    this.onSidebarSelect = this.onSidebarSelect.bind(this);
    this.inspector.sidebar.on("select", this.onSidebarSelect);

    this.onPickerStarted = this.onPickerStarted.bind(this);
    this.onMarkupViewLeave = this.onMarkupViewLeave.bind(this);
    this.onMarkupViewNodeHover = this.onMarkupViewNodeHover.bind(this);
    this.onWillNavigate = this.onWillNavigate.bind(this);

    this.initBoxModelHighlighter();

    // Store for the different dimensions of the node.
    // 'selector' refers to the element that holds the value in view.xhtml;
    // 'property' is what we are measuring;
    // 'value' is the computed dimension, computed in update().
    this.map = {
      position: {
        selector: "#layout-element-position",
        property: "position",
        value: undefined
      },
      marginTop: {
        selector: ".layout-margin.layout-top > span",
        property: "margin-top",
        value: undefined
      },
      marginBottom: {
        selector: ".layout-margin.layout-bottom > span",
        property: "margin-bottom",
        value: undefined
      },
      marginLeft: {
        selector: ".layout-margin.layout-left > span",
        property: "margin-left",
        value: undefined
      },
      marginRight: {
        selector: ".layout-margin.layout-right > span",
        property: "margin-right",
        value: undefined
      },
      paddingTop: {
        selector: ".layout-padding.layout-top > span",
        property: "padding-top",
        value: undefined
      },
      paddingBottom: {
        selector: ".layout-padding.layout-bottom > span",
        property: "padding-bottom",
        value: undefined
      },
      paddingLeft: {
        selector: ".layout-padding.layout-left > span",
        property: "padding-left",
        value: undefined
      },
      paddingRight: {
        selector: ".layout-padding.layout-right > span",
        property: "padding-right",
        value: undefined
      },
      borderTop: {
        selector: ".layout-border.layout-top > span",
        property: "border-top-width",
        value: undefined
      },
      borderBottom: {
        selector: ".layout-border.layout-bottom > span",
        property: "border-bottom-width",
        value: undefined
      },
      borderLeft: {
        selector: ".layout-border.layout-left > span",
        property: "border-left-width",
        value: undefined
      },
      borderRight: {
        selector: ".layout-border.layout-right > span",
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

    // Mark document as RTL or LTR:
    let chromeReg = Cc["@mozilla.org/chrome/chrome-registry;1"]
                    .getService(Ci.nsIXULChromeRegistry);
    let dir = chromeReg.isLocaleRTL("global");
    let container = this.doc.getElementById("layout-container");
    container.setAttribute("dir", dir ? "rtl" : "ltr");

    let nodeGeometry = this.doc.getElementById("layout-geometry-editor");

    this.onGeometryButtonClick = this.onGeometryButtonClick.bind(this);
    nodeGeometry.addEventListener("click", this.onGeometryButtonClick);
  },

  initBoxModelHighlighter: function () {
    let highlightElts = this.doc.querySelectorAll("#layout-container *[title]");
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
      let toolbox = this.inspector.toolbox;
      if (toolbox.target.form.reflowActor) {
        this.reflowFront = ReflowFront(toolbox.target.client,
                                       toolbox.target.form);
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
   * Called when the user clicks on one of the editable values in the layoutview
   */
  initEditor: function (element, event, dimension) {
    let { property } = dimension;
    let session = new EditingSession(this.doc, this.elementRules);
    let initialValue = session.getProperty(property);

    let editor = new InplaceEditor({
      element: element,
      initial: initialValue,
      contentType: InplaceEditor.CONTENT_TYPES.CSS_VALUE,
      property: {
        name: dimension.property
      },
      start: self => {
        self.elt.parentNode.classList.add("layout-editing");
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

        session.setProperties(properties);
      },

      done: (value, commit) => {
        editor.elt.parentNode.classList.remove("layout-editing");
        if (!commit) {
          session.revert();
          session.destroy();
        }
      }
    }, event);
  },

  /**
   * Is the layoutview visible in the sidebar.
   * @return {Boolean}
   */
  isViewVisible: function () {
    return this.inspector &&
           this.inspector.sidebar.getCurrentTabID() == "layoutview";
  },

  /**
   * Is the layoutview visible in the sidebar and is the current node valid to
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
    let highlightElts = this.doc.querySelectorAll("#layout-container *[title]");

    for (let element of highlightElts) {
      element.removeEventListener("mouseover", this.onHighlightMouseOver, true);
      element.removeEventListener("mouseout", this.onHighlightMouseOut, true);
    }

    let nodeGeometry = this.doc.getElementById("layout-geometry-editor");
    nodeGeometry.removeEventListener("click", this.onGeometryButtonClick);

    this.inspector.off("picker-started", this.onPickerStarted);

    // Inspector Panel will destroy `markup` object on "will-navigate" event,
    // therefore we have to check if it's still available in case LayoutView
    // is destroyed immediately after.
    if (this.inspector.markup) {
      this.inspector.markup.off("leave", this.onMarkupViewLeave);
      this.inspector.markup.off("node-hover", this.onMarkupViewNodeHover);
    }

    this.inspector.sidebar.off("layoutview-selected", this.onNewNode);
    this.inspector.selection.off("new-node-front", this.onNewSelection);
    this.inspector.sidebar.off("select", this.onSidebarSelect);
    this.inspector._target.off("will-navigate", this.onWillNavigate);

    this.sizeHeadingLabel = null;
    this.sizeLabel = null;
    this.inspector = null;
    this.doc = null;

    if (this.reflowFront) {
      this.untrackReflows();
      this.reflowFront.destroy();
      this.reflowFront = null;
    }
  },

  onSidebarSelect: function (e, sidebar) {
    this.setActive(sidebar === "layoutview");
  },

  /**
   * Selection 'new-node-front' event handler.
   */
  onNewSelection: function () {
    let done = this.inspector.updating("layoutview");
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
   * Stop tracking reflows and hide all values when no node is selected or the
   * layout-view is hidden, otherwise track reflows and show values.
   * @param {Boolean} isActive
   */
  setActive: function (isActive) {
    if (isActive === this.isActive) {
      return;
    }
    this.isActive = isActive;

    let panel = this.doc.getElementById("sidebar-panel-layoutview");
    panel.classList.toggle("inactive", !isActive);

    if (isActive) {
      this.trackReflows();
    } else {
      this.untrackReflows();
    }
  },

  /**
   * Compute the dimensions of the node and update the values in
   * the layoutview/view.xhtml document.
   * @return a promise that will be resolved when complete.
   */
  update: function () {
    let lastRequest = Task.spawn((function* () {
      if (!this.isViewVisibleAndNodeValid()) {
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

      // If the view isn't active, no need to do anything more.
      if (!this.isActive) {
        this.inspector.emit("layoutview-updated");
        return null;
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

      this.inspector.emit("layoutview-updated");
      return undefined;
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
      title += "\n" + sourceRule.parentStyleSheet.href + ":" + sourceRule.line;
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
    let nodeGeometry = this.doc.getElementById("layout-geometry-editor");
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
        this.inspector._target.once("will-navigate", this.onWillNavigate);
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
      let nodeGeometry = this.doc.getElementById("layout-geometry-editor");
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

    let nodeGeometry = this.doc.getElementById("layout-geometry-editor");
    nodeGeometry.style.visibility = isEditable ? "visible" : "hidden";
  }),

  manageOverflowingText: function (span) {
    let classList = span.parentNode.classList;

    if (classList.contains("layout-left") ||
        classList.contains("layout-right")) {
      let force = span.textContent.length > LONG_TEXT_ROTATE_LIMIT;
      classList.toggle("layout-rotate", force);
    }
  }
};

exports.LayoutView = LayoutView;
