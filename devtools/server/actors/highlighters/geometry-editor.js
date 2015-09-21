/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { extend } = require("sdk/core/heritage");
const { AutoRefreshHighlighter } = require("./auto-refresh");
const {
  CanvasFrameAnonymousContentHelper, getCSSStyleRules, getComputedStyle,
  createSVGNode, createNode } = require("./utils/markup");

const { setIgnoreLayoutChanges,
  getAdjustedQuads } = require("devtools/shared/layout/utils");

const GEOMETRY_LABEL_SIZE = 6;

/**
 * Element geometry properties helper that gives names of position and size
 * properties.
 */
let GeoProp = {
  SIDES: ["top", "right", "bottom", "left"],
  SIZES: ["width", "height"],

  allProps: function() {
    return [...this.SIDES, ...this.SIZES];
  },

  isSide: function(name) {
    return this.SIDES.indexOf(name) !== -1;
  },

  isSize: function(name) {
    return this.SIZES.indexOf(name) !== -1;
  },

  containsSide: function(names) {
    return names.some(name => this.SIDES.indexOf(name) !== -1);
  },

  containsSize: function(names) {
    return names.some(name => this.SIZES.indexOf(name) !== -1);
  },

  isHorizontal: function(name) {
    return name === "left" || name === "right" || name === "width";
  },

  isInverted: function(name) {
    return name === "right" || name === "bottom";
  },

  mainAxisStart: function(name) {
    return this.isHorizontal(name) ? "left" : "top";
  },

  crossAxisStart: function(name) {
    return this.isHorizontal(name) ? "top" : "left";
  },

  mainAxisSize: function(name) {
    return this.isHorizontal(name) ? "width" : "height";
  },

  crossAxisSize: function(name) {
    return this.isHorizontal(name) ? "height" : "width";
  },

  axis: function(name) {
    return this.isHorizontal(name) ? "x" : "y";
  },

  crossAxis: function(name) {
    return this.isHorizontal(name) ? "y" : "x";
  }
};

/**
 * Get the provided node's offsetParent dimensions.
 * Returns an object with the {parent, dimension} properties.
 * Note that the returned parent will be null if the offsetParent is the
 * default, non-positioned, body or html node.
 *
 * node.offsetParent returns the nearest positioned ancestor but if it is
 * non-positioned itself, we just return null to let consumers know the node is
 * actually positioned relative to the viewport.
 *
 * @return {Object}
 */
function getOffsetParent(node) {
  let win = node.ownerDocument.defaultView;

  let offsetParent = node.offsetParent;
  if (offsetParent &&
      getComputedStyle(offsetParent).position === "static") {
    offsetParent = null;
  }

  let width, height;
  if (!offsetParent) {
    height = win.innerHeight;
    width = win.innerWidth;
  } else {
    height = offsetParent.offsetHeight;
    width = offsetParent.offsetWidth;
  }

  return {
    element: offsetParent,
    dimension: {width, height}
  };
}

/**
 * The GeometryEditor highlights an elements's top, left, bottom, right, width
 * and height dimensions, when they are set.
 *
 * To determine if an element has a set size and position, the highlighter lists
 * the CSS rules that apply to the element and checks for the top, left, bottom,
 * right, width and height properties.
 * The highlighter won't be shown if the element doesn't have any of these
 * properties set, but will be shown when at least 1 property is defined.
 *
 * The highlighter displays lines and labels for each of the defined properties
 * in and around the element (relative to the offset parent when one exists).
 * The highlighter also highlights the element itself and its offset parent if
 * there is one.
 *
 * Note that the class name contains the word Editor because the aim is for the
 * handles to be draggable in content to make the geometry editable.
 */
function GeometryEditorHighlighter(highlighterEnv) {
  AutoRefreshHighlighter.call(this, highlighterEnv);

  // The list of element geometry properties that can be set.
  this.definedProperties = new Map();

  this.markup = new CanvasFrameAnonymousContentHelper(highlighterEnv,
    this._buildMarkup.bind(this));
}

GeometryEditorHighlighter.prototype = extend(AutoRefreshHighlighter.prototype, {
  typeName: "GeometryEditorHighlighter",

  ID_CLASS_PREFIX: "geometry-editor-",

  _buildMarkup: function() {
    let container = createNode(this.win, {
      attributes: {"class": "highlighter-container"}
    });

    let root = createNode(this.win, {
      parent: container,
      attributes: {
        "id": "root",
        "class": "root"
      },
      prefix: this.ID_CLASS_PREFIX
    });

    let svg = createSVGNode(this.win, {
      nodeType: "svg",
      parent: root,
      attributes: {
        "id": "elements",
        "width": "100%",
        "height": "100%"
      },
      prefix: this.ID_CLASS_PREFIX
    });

    // Offset parent node highlighter.
    createSVGNode(this.win, {
      nodeType: "polygon",
      parent: svg,
      attributes: {
        "class": "offset-parent",
        "id": "offset-parent",
        "hidden": "true"
      },
      prefix: this.ID_CLASS_PREFIX
    });

    // Current node highlighter (margin box).
    createSVGNode(this.win, {
      nodeType: "polygon",
      parent: svg,
      attributes: {
        "class": "current-node",
        "id": "current-node",
        "hidden": "true"
      },
      prefix: this.ID_CLASS_PREFIX
    });

    // Build the 4 side arrows and labels.
    for (let name of GeoProp.SIDES) {
      createSVGNode(this.win, {
        nodeType: "line",
        parent: svg,
        attributes: {
          "class": "arrow " + name,
          "id": "arrow-" + name,
          "hidden": "true"
        },
        prefix: this.ID_CLASS_PREFIX
      });

      // Labels are positioned by using a translated <g>. This group contains
      // a path and text that are themselves positioned using another translated
      // <g>. This is so that the label arrow points at the 0,0 coordinates of
      // parent <g>.
      let labelG = createSVGNode(this.win, {
        nodeType: "g",
        parent: svg,
        attributes: {
          "id": "label-" + name,
          "hidden": "true"
        },
        prefix: this.ID_CLASS_PREFIX
      });

      let subG = createSVGNode(this.win, {
        nodeType: "g",
        parent: labelG,
        attributes: {
          "transform": GeoProp.isHorizontal(name)
                       ? "translate(-30 -30)"
                       : "translate(5 -10)"
        }
      });

      createSVGNode(this.win, {
        nodeType: "path",
        parent: subG,
        attributes: {
          "class": "label-bubble",
          "d": GeoProp.isHorizontal(name)
               ? "M0 0 L60 0 L60 20 L35 20 L30 25 L25 20 L0 20z"
               : "M5 0 L65 0 L65 20 L5 20 L5 15 L0 10 L5 5z"
        },
        prefix: this.ID_CLASS_PREFIX
      });

      createSVGNode(this.win, {
        nodeType: "text",
        parent: subG,
        attributes: {
          "class": "label-text",
          "id": "label-text-" + name,
          "x": GeoProp.isHorizontal(name) ? "30" : "35",
          "y": "10"
        },
        prefix: this.ID_CLASS_PREFIX
      });
    }

    // Build the width/height label and resize handle.
    let labelSizeG = createSVGNode(this.win, {
      nodeType: "g",
      parent: svg,
      attributes: {
        "id": "label-size",
        "hidden": "true"
      },
      prefix: this.ID_CLASS_PREFIX
    });

    let subSizeG = createSVGNode(this.win, {
      nodeType: "g",
      parent: labelSizeG,
      attributes: {
        "transform": "translate(-50 -10)"
      }
    });

    createSVGNode(this.win, {
      nodeType: "path",
      parent: subSizeG,
      attributes: {
        "class": "label-bubble",
        "d": "M0 0 L100 0 L100 20 L0 20z"
      },
      prefix: this.ID_CLASS_PREFIX
    });

    createSVGNode(this.win, {
      nodeType: "text",
      parent: subSizeG,
      attributes: {
        "class": "label-text",
        "id": "label-text-size",
        "x": "50",
        "y": "10"
      },
      prefix: this.ID_CLASS_PREFIX
    });

    return container;
  },

  destroy: function() {
    AutoRefreshHighlighter.prototype.destroy.call(this);

    this.markup.destroy();
    this.definedProperties.clear();
    this.definedProperties = null;
    this.offsetParent = null;
  },

  getElement: function(id) {
    return this.markup.getElement(this.ID_CLASS_PREFIX + id);
  },

  /**
   * Get the list of geometry properties that are actually set on the current
   * node.
   * @return {Map} A map indexed by property name and where the value is an
   * object having the cssRule property.
   */
  getDefinedGeometryProperties: function() {
    let props = new Map();
    if (!this.currentNode) {
      return props;
    }

    // Get the list of css rules applying to the current node.
    let cssRules = getCSSStyleRules(this.currentNode);
    for (let i = 0; i < cssRules.Count(); i++) {
      let rule = cssRules.GetElementAt(i);
      for (let name of GeoProp.allProps()) {
        let value = rule.style.getPropertyValue(name);
        if (value && value !== "auto") {
          // getCSSStyleRules returns rules ordered from least to most specific
          // so just override any previous properties we have set.
          props.set(name, {
            cssRule: rule
          });
        }
      }
    }

    // Go through the inline styles last.
    for (let name of GeoProp.allProps()) {
      let value = this.currentNode.style.getPropertyValue(name);
      if (value && value !== "auto") {
        props.set(name, {
          // There's no cssRule to store here, so store the node instead since
          // node.style exists.
          cssRule: this.currentNode
        });
      }
    }

    // Post-process the list for invalid properties. This is done after the fact
    // because of cases like relative positioning with both top and bottom where
    // only top will actually be used, but both exists in css rules and computed
    // styles.
    for (let [name] of props) {
      let pos = this.computedStyle.position;

      // Top/left/bottom/right on static positioned elements have no effect.
      if (pos === "static" && GeoProp.SIDES.indexOf(name) !== -1) {
        props.delete(name);
      }

      // Bottom/right on relative positioned elements are only used if top/left
      // are not defined.
      let hasRightAndLeft = name === "right" && props.has("left");
      let hasBottomAndTop = name === "bottom" && props.has("top");
      if (pos === "relative" && (hasRightAndLeft || hasBottomAndTop)) {
        props.delete(name);
      }
    }

    return props;
  },

  _show: function() {
    this.computedStyle = getComputedStyle(this.currentNode);
    let pos = this.computedStyle.position;
    // XXX: sticky positioning is ignored for now. To be implemented next.
    if (pos === "sticky") {
      this.hide();
      return false;
    }

    let hasUpdated = this._update();
    if (!hasUpdated) {
      this.hide();
      return false;
    }
    return true;
  },

  _update: function() {
    // At each update, the position or/and size may have changed, so get the
    // list of defined properties, and re-position the arrows and highlighters.
    this.definedProperties = this.getDefinedGeometryProperties();

    if (!this.definedProperties.size) {
      console.warn("The element does not have editable geometry properties");
      return false;
    }

    setIgnoreLayoutChanges(true);

    // Update the highlighters and arrows.
    this.updateOffsetParent();
    this.updateCurrentNode();
    this.updateArrows();
    this.updateSize();

    // Avoid zooming the arrows when content is zoomed.
    this.markup.scaleRootElement(this.currentNode, this.ID_CLASS_PREFIX + "root");

    setIgnoreLayoutChanges(false, this.currentNode.ownerDocument.documentElement);
    return true;
  },

  /**
   * Update the offset parent rectangle.
   * There are 3 different cases covered here:
   * - the node is absolutely/fixed positioned, and an offsetParent is defined
   *   (i.e. it's not just positioned in the viewport): the offsetParent node
   *   is highlighted (i.e. the rectangle is shown),
   * - the node is relatively positioned: the rectangle is shown where the node
   *   would originally have been (because that's where the relative positioning
   *   is calculated from),
   * - the node has no offset parent at all: the offsetParent rectangle is
   *   hidden.
   */
  updateOffsetParent: function() {
    // Get the offsetParent, if any.
    this.offsetParent = getOffsetParent(this.currentNode);
    // And the offsetParent quads.
    this.parentQuads = getAdjustedQuads(
        this.win, this.offsetParent.element, "padding");

    let el = this.getElement("offset-parent");

    let isPositioned = this.computedStyle.position === "absolute" ||
                       this.computedStyle.position === "fixed";
    let isRelative = this.computedStyle.position === "relative";
    let isHighlighted = false;

    if (this.offsetParent.element && isPositioned) {
      let {p1, p2, p3, p4} = this.parentQuads[0];
      let points = p1.x + "," + p1.y + " " +
                   p2.x + "," + p2.y + " " +
                   p3.x + "," + p3.y + " " +
                   p4.x + "," + p4.y;
      el.setAttribute("points", points);
      isHighlighted = true;
    } else if (isRelative) {
      let xDelta = parseFloat(this.computedStyle.left);
      let yDelta = parseFloat(this.computedStyle.top);
      if (xDelta || yDelta) {
        let {p1, p2, p3, p4} = this.currentQuads.margin[0];
        let points = (p1.x - xDelta) + "," + (p1.y - yDelta) + " " +
                     (p2.x - xDelta) + "," + (p2.y - yDelta) + " " +
                     (p3.x - xDelta) + "," + (p3.y - yDelta) + " " +
                     (p4.x - xDelta) + "," + (p4.y - yDelta);
        el.setAttribute("points", points);
        isHighlighted = true;
      }
    }

    if (isHighlighted) {
      el.removeAttribute("hidden");
    } else {
      el.setAttribute("hidden", "true");
    }
  },

  updateCurrentNode: function() {
    let box = this.getElement("current-node");
    let {p1, p2, p3, p4} = this.currentQuads.margin[0];
    let attr = p1.x + "," + p1.y + " " +
               p2.x + "," + p2.y + " " +
               p3.x + "," + p3.y + " " +
               p4.x + "," + p4.y;
    box.setAttribute("points", attr);
    box.removeAttribute("hidden");
  },

  _hide: function() {
    setIgnoreLayoutChanges(true);

    this.getElement("current-node").setAttribute("hidden", "true");
    this.getElement("offset-parent").setAttribute("hidden", "true");
    this.hideArrows();
    this.hideSize();

    this.definedProperties.clear();

    setIgnoreLayoutChanges(false, this.currentNode.ownerDocument.documentElement);
  },

  hideArrows: function() {
    for (let side of GeoProp.SIDES) {
      this.getElement("arrow-" + side).setAttribute("hidden", "true");
      this.getElement("label-" + side).setAttribute("hidden", "true");
    }
  },

  hideSize: function() {
    this.getElement("label-size").setAttribute("hidden", "true");
  },

  updateSize: function() {
    this.hideSize();

    let labels = [];
    let width = this.definedProperties.get("width");
    let height = this.definedProperties.get("height");

    if (width) {
      labels.push("↔ " + width.cssRule.style.getPropertyValue("width"));
    }
    if (height) {
      labels.push("↕ " + height.cssRule.style.getPropertyValue("height"));
    }

    if (labels.length) {
      let labelEl = this.getElement("label-size");
      let labelTextEl = this.getElement("label-text-size");

      let {bounds} = this.currentQuads.margin[0];

      labelEl.setAttribute("transform", "translate(" +
        (bounds.left + bounds.width / 2) + " " +
        (bounds.top + bounds.height / 2) + ")");
      labelEl.removeAttribute("hidden");
      labelTextEl.setTextContent(labels.join(" "));
    }
  },

  updateArrows: function() {
    this.hideArrows();

    // Position arrows always end at the node's margin box.
    let marginBox = this.currentQuads.margin[0].bounds;

    // Position the side arrows which need to be visible.
    // Arrows always start at the offsetParent edge, and end at the middle
    // position of the node's margin edge.
    // Note that for relative positioning, the offsetParent is considered to be
    // the node itself, where it would have been originally.
    // +------------------+----------------+
    // | offsetparent     | top            |
    // | or viewport      |                |
    // |         +--------+--------+       |
    // |         | node            |       |
    // +---------+                 +-------+
    // | left    |                 | right |
    // |         +--------+--------+       |
    // |                  | bottom         |
    // +------------------+----------------+
    let getSideArrowStartPos = side => {
      // In case an offsetParent exists and is highlighted.
      if (this.parentQuads && this.parentQuads.length) {
        return this.parentQuads[0].bounds[side];
      }

      // In case of relative positioning.
      if (this.computedStyle.position === "relative") {
        if (GeoProp.isInverted(side)) {
          return marginBox[side] + parseFloat(this.computedStyle[side]);
        }
        return marginBox[side] - parseFloat(this.computedStyle[side]);
      }

      // In case the element is positioned in the viewport.
      if (GeoProp.isInverted(side)) {
        return this.offsetParent.dimension[GeoProp.mainAxisSize(side)];
      }
      return -1 * this.currentNode.ownerDocument.defaultView["scroll" +
                                              GeoProp.axis(side).toUpperCase()];
    };

    for (let side of GeoProp.SIDES) {
      let sideProp = this.definedProperties.get(side);
      if (!sideProp) {
        continue;
      }

      let mainAxisStartPos = getSideArrowStartPos(side);
      let mainAxisEndPos = marginBox[side];
      let crossAxisPos = marginBox[GeoProp.crossAxisStart(side)] +
                         marginBox[GeoProp.crossAxisSize(side)] / 2;

      this.updateArrow(side, mainAxisStartPos, mainAxisEndPos, crossAxisPos,
                       sideProp.cssRule.style.getPropertyValue(side));
    }
  },

  updateArrow: function(side, mainStart, mainEnd, crossPos, labelValue) {
    let arrowEl = this.getElement("arrow-" + side);
    let labelEl = this.getElement("label-" + side);
    let labelTextEl = this.getElement("label-text-" + side);

    // Position the arrow <line>.
    arrowEl.setAttribute(GeoProp.axis(side) + "1", mainStart);
    arrowEl.setAttribute(GeoProp.crossAxis(side) + "1", crossPos);
    arrowEl.setAttribute(GeoProp.axis(side) + "2", mainEnd);
    arrowEl.setAttribute(GeoProp.crossAxis(side) + "2", crossPos);
    arrowEl.removeAttribute("hidden");

    // Position the label <text> in the middle of the arrow (making sure it's
    // not hidden below the fold).
    let capitalize = str => str.substring(0, 1).toUpperCase() + str.substring(1);
    let winMain = this.win["inner" + capitalize(GeoProp.mainAxisSize(side))];
    let labelMain = mainStart + (mainEnd - mainStart) / 2;
    if ((mainStart > 0 && mainStart < winMain) ||
        (mainEnd > 0 && mainEnd < winMain)) {
      if (labelMain < GEOMETRY_LABEL_SIZE) {
        labelMain = GEOMETRY_LABEL_SIZE;
      } else if (labelMain > winMain - GEOMETRY_LABEL_SIZE) {
        labelMain = winMain - GEOMETRY_LABEL_SIZE;
      }
    }
    let labelCross = crossPos;
    labelEl.setAttribute("transform", GeoProp.isHorizontal(side)
                         ? "translate(" + labelMain + " " + labelCross + ")"
                         : "translate(" + labelCross + " " + labelMain + ")");
    labelEl.removeAttribute("hidden");
    labelTextEl.setTextContent(labelValue);
  }
});
exports.GeometryEditorHighlighter = GeometryEditorHighlighter;
