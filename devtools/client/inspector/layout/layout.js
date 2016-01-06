/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var {utils: Cu, interfaces: Ci, classes: Cc} = Components;

Cu.import("resource://gre/modules/Task.jsm");
const {require} = Cu.import("resource://devtools/shared/Loader.jsm", {});
Cu.import("resource://gre/modules/Console.jsm");
Cu.import("resource://devtools/client/shared/widgets/ViewHelpers.jsm");

const {InplaceEditor, editableItem} = require("devtools/client/shared/inplace-editor");
const {ReflowFront} = require("devtools/server/actors/layout");

const STRINGS_URI = "chrome://devtools/locale/shared.properties";
const SHARED_L10N = new ViewHelpers.L10N(STRINGS_URI);
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
  getPropertyFromRule: function(rule, property) {
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
  getProperty: function(property) {
    // Create a hidden element for getPropertyFromRule to use
    let div = this._doc.createElement("div");
    div.setAttribute("style", "display: none");
    this._doc.body.appendChild(div);
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
  setProperties: function(properties) {
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
  revert: function() {
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

  destroy: function() {
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
  this.sizeLabel = this.doc.querySelector(".size > span");
  this.sizeHeadingLabel = this.doc.getElementById("element-size");

  this.init();
}

LayoutView.prototype = {
  init: function() {
    this.update = this.update.bind(this);

    this.onNewSelection = this.onNewSelection.bind(this);
    this.inspector.selection.on("new-node-front", this.onNewSelection);

    this.onNewNode = this.onNewNode.bind(this);
    this.inspector.sidebar.on("layoutview-selected", this.onNewNode);

    this.onSidebarSelect = this.onSidebarSelect.bind(this);
    this.inspector.sidebar.on("select", this.onSidebarSelect);

    // Store for the different dimensions of the node.
    // 'selector' refers to the element that holds the value in view.xhtml;
    // 'property' is what we are measuring;
    // 'value' is the computed dimension, computed in update().
    this.map = {
      position: {selector: "#element-position",
                 property: "position",
                 value: undefined},
      marginTop: {selector: ".margin.top > span",
                  property: "margin-top",
                  value: undefined},
      marginBottom: {selector: ".margin.bottom > span",
                  property: "margin-bottom",
                  value: undefined},
      marginLeft: {selector: ".margin.left > span",
                  property: "margin-left",
                  value: undefined},
      marginRight: {selector: ".margin.right > span",
                  property: "margin-right",
                  value: undefined},
      paddingTop: {selector: ".padding.top > span",
                  property: "padding-top",
                  value: undefined},
      paddingBottom: {selector: ".padding.bottom > span",
                  property: "padding-bottom",
                  value: undefined},
      paddingLeft: {selector: ".padding.left > span",
                  property: "padding-left",
                  value: undefined},
      paddingRight: {selector: ".padding.right > span",
                  property: "padding-right",
                  value: undefined},
      borderTop: {selector: ".border.top > span",
                  property: "border-top-width",
                  value: undefined},
      borderBottom: {selector: ".border.bottom > span",
                  property: "border-bottom-width",
                  value: undefined},
      borderLeft: {selector: ".border.left > span",
                  property: "border-left-width",
                  value: undefined},
      borderRight: {selector: ".border.right > span",
                  property: "border-right-width",
                  value: undefined}
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
  },

  /**
   * Start listening to reflows in the current tab.
   */
  trackReflows: function() {
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
  untrackReflows: function() {
    if (!this.reflowFront) {
      return;
    }

    this.reflowFront.off("reflows", this.update);
    this.reflowFront.stop();
  },

  /**
   * Called when the user clicks on one of the editable values in the layoutview
   */
  initEditor: function(element, event, dimension) {
    let { property } = dimension;
    let session = new EditingSession(document, this.elementRules);
    let initialValue = session.getProperty(property);

    let editor = new InplaceEditor({
      element: element,
      initial: initialValue,

      start: editor => {
        editor.elt.parentNode.classList.add("editing");
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
        editor.elt.parentNode.classList.remove("editing");
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
  isViewVisible: function() {
    return this.inspector &&
           this.inspector.sidebar.getCurrentTabID() == "layoutview";
  },

  /**
   * Is the layoutview visible in the sidebar and is the current node valid to
   * be displayed in the view.
   * @return {Boolean}
   */
  isViewVisibleAndNodeValid: function() {
    return this.isViewVisible() &&
           this.inspector.selection.isConnected() &&
           this.inspector.selection.isElementNode();
  },

  /**
   * Destroy the nodes. Remove listeners.
   */
  destroy: function() {
    this.inspector.sidebar.off("layoutview-selected", this.onNewNode);
    this.inspector.selection.off("new-node-front", this.onNewSelection);
    this.inspector.sidebar.off("select", this.onSidebarSelect);

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

  onSidebarSelect: function(e, sidebar) {
    this.setActive(sidebar === "layoutview");
  },

  /**
   * Selection 'new-node-front' event handler.
   */
  onNewSelection: function() {
    let done = this.inspector.updating("layoutview");
    this.onNewNode().then(done, err => {
      console.error(err);
      done();
    });
  },

  /**
   * @return a promise that resolves when the view has been updated
   */
  onNewNode: function() {
    this.setActive(this.isViewVisibleAndNodeValid());
    return this.update();
  },

  /**
   * Stop tracking reflows and hide all values when no node is selected or the
   * layout-view is hidden, otherwise track reflows and show values.
   * @param {Boolean} isActive
   */
  setActive: function(isActive) {
    if (isActive === this.isActive) {
      return;
    }
    this.isActive = isActive;

    this.doc.body.classList.toggle("inactive", !isActive);
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
  update: function() {
    let lastRequest = Task.spawn((function*() {
      if (!this.isViewVisibleAndNodeValid()) {
        return;
      }

      let node = this.inspector.selection.nodeFront;
      let layout = yield this.inspector.pageStyle.getLayout(node, {
        autoMargins: this.isActive
      });
      let styleEntries = yield this.inspector.pageStyle.getApplied(node, {});

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
    }).bind(this)).then(null, console.error);

    return this._lastRequest = lastRequest;
  },

  /**
   * Update the text in the tooltip shown when hovering over a value to provide
   * information about the source CSS rule that sets this value.
   * @param {DOMNode} el The element that will receive the tooltip.
   * @param {String} property The name of the CSS property for the tooltip.
   * @param {Array} rules An array of applied rules retrieved by
   * styleActor.getApplied.
   */
  updateSourceRuleTooltip: function(el, property, rules) {
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
  showBoxModel: function(options={}) {
    let toolbox = this.inspector.toolbox;
    let nodeFront = this.inspector.selection.nodeFront;

    toolbox.highlighterUtils.highlightNodeFront(nodeFront, options);
  },

  /**
   * Hide the box-model highlighter on the currently selected element
   */
  hideBoxModel: function() {
    let toolbox = this.inspector.toolbox;

    toolbox.highlighterUtils.unhighlight();
  },

  manageOverflowingText: function(span) {
    let classList = span.parentNode.classList;

    if (classList.contains("left") || classList.contains("right")) {
      let force = span.textContent.length > LONG_TEXT_ROTATE_LIMIT;
      classList.toggle("rotate", force);
    }
  }
};

var elts;

var onmouseover = function(e) {
  let region = e.target.getAttribute("data-box");
  if (!region) {
    return false;
  }

  this.layoutview.showBoxModel({
    region,
    showOnly: region,
    onlyRegionArea: true
  });

  return false;
}.bind(window);

var onmouseout = function() {
  this.layoutview.hideBoxModel();
  return false;
}.bind(window);

window.setPanel = function(panel) {
  this.layoutview = new LayoutView(panel, window);

  // Box model highlighter mechanism
  elts = document.querySelectorAll("*[title]");
  for (let i = 0; i < elts.length; i++) {
    let elt = elts[i];
    elt.addEventListener("mouseover", onmouseover, true);
    elt.addEventListener("mouseout", onmouseout, true);
  }

  // Mark document as RTL or LTR:
  let chromeReg = Cc["@mozilla.org/chrome/chrome-registry;1"]
                  .getService(Ci.nsIXULChromeRegistry);
  let dir = chromeReg.isLocaleRTL("global");
  document.body.setAttribute("dir", dir ? "rtl" : "ltr");

  window.parent.postMessage("layoutview-ready", "*");
};

window.onunload = function() {
  if (this.layoutview) {
    this.layoutview.destroy();
  }
  if (elts) {
    for (let i = 0; i < elts.length; i++) {
      let elt = elts[i];
      elt.removeEventListener("mouseover", onmouseover, true);
      elt.removeEventListener("mouseout", onmouseout, true);
    }
  }
};
