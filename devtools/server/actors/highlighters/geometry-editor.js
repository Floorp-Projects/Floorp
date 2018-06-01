/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { AutoRefreshHighlighter } = require("./auto-refresh");
const { CanvasFrameAnonymousContentHelper, getComputedStyle,
        createSVGNode, createNode } = require("./utils/markup");
const { setIgnoreLayoutChanges, getAdjustedQuads } = require("devtools/shared/layout/utils");
const { getCSSStyleRules } = require("devtools/shared/inspector/css-logic");

const GEOMETRY_LABEL_SIZE = 6;

// List of all DOM Events subscribed directly to the document from the
// Geometry Editor highlighter
const DOM_EVENTS = ["mousemove", "mouseup", "pagehide"];

const _dragging = Symbol("geometry/dragging");

/**
 * Element geometry properties helper that gives names of position and size
 * properties.
 */
var GeoProp = {
  SIDES: ["top", "right", "bottom", "left"],
  SIZES: ["width", "height"],

  allProps: function() {
    return [...this.SIDES, ...this.SIZES];
  },

  isSide: function(name) {
    return this.SIDES.includes(name);
  },

  isSize: function(name) {
    return this.SIZES.includes(name);
  },

  containsSide: function(names) {
    return names.some(name => this.SIDES.includes(name));
  },

  containsSize: function(names) {
    return names.some(name => this.SIZES.includes(name));
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
  const win = node.ownerGlobal;

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
 * Get the list of geometry properties that are actually set on the provided
 * node.
 *
 * @param {Node} node The node to analyze.
 * @return {Map} A map indexed by property name and where the value is an
 * object having the cssRule property.
 */
function getDefinedGeometryProperties(node) {
  const props = new Map();
  if (!node) {
    return props;
  }

  // Get the list of css rules applying to the current node.
  const cssRules = getCSSStyleRules(node);
  for (let i = 0; i < cssRules.length; i++) {
    const rule = cssRules[i];
    for (const name of GeoProp.allProps()) {
      const value = rule.style.getPropertyValue(name);
      if (value && value !== "auto") {
        // getCSSStyleRules returns rules ordered from least to most specific
        // so just override any previous properties we have set.
        props.set(name, {
          cssRule: rule
        });
      }
    }
  }

  // Go through the inline styles last, only if the node supports inline style
  // (e.g. pseudo elements don't have a style property)
  if (node.style) {
    for (const name of GeoProp.allProps()) {
      const value = node.style.getPropertyValue(name);
      if (value && value !== "auto") {
        props.set(name, {
          // There's no cssRule to store here, so store the node instead since
          // node.style exists.
          cssRule: node
        });
      }
    }
  }

  // Post-process the list for invalid properties. This is done after the fact
  // because of cases like relative positioning with both top and bottom where
  // only top will actually be used, but both exists in css rules and computed
  // styles.
  const { position } = getComputedStyle(node);
  for (const [name] of props) {
    // Top/left/bottom/right on static positioned elements have no effect.
    if (position === "static" && GeoProp.SIDES.includes(name)) {
      props.delete(name);
    }

    // Bottom/right on relative positioned elements are only used if top/left
    // are not defined.
    const hasRightAndLeft = name === "right" && props.has("left");
    const hasBottomAndTop = name === "bottom" && props.has("top");
    if (position === "relative" && (hasRightAndLeft || hasBottomAndTop)) {
      props.delete(name);
    }
  }

  return props;
}
exports.getDefinedGeometryProperties = getDefinedGeometryProperties;

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
class GeometryEditorHighlighter extends AutoRefreshHighlighter {
  constructor(highlighterEnv) {
    super(highlighterEnv);

    this.ID_CLASS_PREFIX = "geometry-editor-";

    // The list of element geometry properties that can be set.
    this.definedProperties = new Map();

    this.markup = new CanvasFrameAnonymousContentHelper(highlighterEnv,
      this._buildMarkup.bind(this));

    const { pageListenerTarget } = this.highlighterEnv;

    // Register the geometry editor instance to all events we're interested in.
    DOM_EVENTS.forEach(type => pageListenerTarget.addEventListener(type, this));

    // Register the mousedown event for each Geometry Editor's handler.
    // Those events are automatically removed when the markup is destroyed.
    const onMouseDown = this.handleEvent.bind(this);

    for (const side of GeoProp.SIDES) {
      this.getElement("handler-" + side)
        .addEventListener("mousedown", onMouseDown);
    }

    this.onWillNavigate = this.onWillNavigate.bind(this);

    this.highlighterEnv.on("will-navigate", this.onWillNavigate);
  }

  _buildMarkup() {
    const container = createNode(this.win, {
      attributes: {"class": "highlighter-container"}
    });

    const root = createNode(this.win, {
      parent: container,
      attributes: {
        "id": "root",
        "class": "root",
        "hidden": "true"
      },
      prefix: this.ID_CLASS_PREFIX
    });

    const svg = createSVGNode(this.win, {
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

    // Build the 4 side arrows, handlers and labels.
    for (const name of GeoProp.SIDES) {
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

      createSVGNode(this.win, {
        nodeType: "circle",
        parent: svg,
        attributes: {
          "class": "handler-" + name,
          "id": "handler-" + name,
          "r": "4",
          "data-side": name,
          "hidden": "true"
        },
        prefix: this.ID_CLASS_PREFIX
      });

      // Labels are positioned by using a translated <g>. This group contains
      // a path and text that are themselves positioned using another translated
      // <g>. This is so that the label arrow points at the 0,0 coordinates of
      // parent <g>.
      const labelG = createSVGNode(this.win, {
        nodeType: "g",
        parent: svg,
        attributes: {
          "id": "label-" + name,
          "hidden": "true"
        },
        prefix: this.ID_CLASS_PREFIX
      });

      const subG = createSVGNode(this.win, {
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

    return container;
  }

  destroy() {
    // Avoiding exceptions if `destroy` is called multiple times; and / or the
    // highlighter environment was already destroyed.
    if (!this.highlighterEnv) {
      return;
    }

    const { pageListenerTarget } = this.highlighterEnv;

    if (pageListenerTarget) {
      DOM_EVENTS.forEach(type =>
        pageListenerTarget.removeEventListener(type, this));
    }

    AutoRefreshHighlighter.prototype.destroy.call(this);

    this.markup.destroy();
    this.definedProperties.clear();
    this.definedProperties = null;
    this.offsetParent = null;
  }

  handleEvent(event, id) {
    // No event handling if the highlighter is hidden
    if (this.getElement("root").hasAttribute("hidden")) {
      return;
    }

    const { target, type, pageX, pageY } = event;

    switch (type) {
      case "pagehide":
        // If a page hide event is triggered for current window's highlighter, hide the
        // highlighter.
        if (target.defaultView === this.win) {
          this.destroy();
        }

        break;
      case "mousedown":
        // The mousedown event is intended only for the handler
        if (!id) {
          return;
        }

        const handlerSide = this.markup.getElement(id).getAttribute("data-side");

        if (handlerSide) {
          const side = handlerSide;
          const sideProp = this.definedProperties.get(side);

          if (!sideProp) {
            return;
          }

          let value = sideProp.cssRule.style.getPropertyValue(side);
          const computedValue = this.computedStyle.getPropertyValue(side);

          const [unit] = value.match(/[^\d]+$/) || [""];

          value = parseFloat(value);

          const ratio = (value / parseFloat(computedValue)) || 1;
          const dir = GeoProp.isInverted(side) ? -1 : 1;

          // Store all the initial values needed for drag & drop
          this[_dragging] = {
            side,
            value,
            unit,
            x: pageX,
            y: pageY,
            inc: ratio * dir
          };

          this.getElement("handler-" + side).classList.add("dragging");
        }

        this.getElement("root").setAttribute("dragging", "true");
        break;
      case "mouseup":
        // If we're dragging, drop it.
        if (this[_dragging]) {
          const { side } = this[_dragging];
          this.getElement("root").removeAttribute("dragging");
          this.getElement("handler-" + side).classList.remove("dragging");
          this[_dragging] = null;
        }
        break;
      case "mousemove":
        if (!this[_dragging]) {
          return;
        }

        const { side, x, y, value, unit, inc } = this[_dragging];
        const sideProps = this.definedProperties.get(side);

        if (!sideProps) {
          return;
        }

        const delta = (GeoProp.isHorizontal(side) ? pageX - x : pageY - y) * inc;

        // The inline style has usually the priority over any other CSS rule
        // set in stylesheets. However, if a rule has `!important` keyword,
        // it will override the inline style too. To ensure Geometry Editor
        // will always update the element, we have to add `!important` as
        // well.
        this.currentNode.style.setProperty(
          side, (value + delta) + unit, "important");

        break;
    }
  }

  getElement(id) {
    return this.markup.getElement(this.ID_CLASS_PREFIX + id);
  }

  _show() {
    this.computedStyle = getComputedStyle(this.currentNode);
    const pos = this.computedStyle.position;
    // XXX: sticky positioning is ignored for now. To be implemented next.
    if (pos === "sticky") {
      this.hide();
      return false;
    }

    const hasUpdated = this._update();
    if (!hasUpdated) {
      this.hide();
      return false;
    }

    this.getElement("root").removeAttribute("hidden");

    return true;
  }

  _update() {
    // At each update, the position or/and size may have changed, so get the
    // list of defined properties, and re-position the arrows and highlighters.
    this.definedProperties = getDefinedGeometryProperties(this.currentNode);

    if (!this.definedProperties.size) {
      console.warn("The element does not have editable geometry properties");
      return false;
    }

    setIgnoreLayoutChanges(true);

    // Update the highlighters and arrows.
    this.updateOffsetParent();
    this.updateCurrentNode();
    this.updateArrows();

    // Avoid zooming the arrows when content is zoomed.
    const node = this.currentNode;
    this.markup.scaleRootElement(node, this.ID_CLASS_PREFIX + "root");

    setIgnoreLayoutChanges(false, this.highlighterEnv.document.documentElement);
    return true;
  }

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
  updateOffsetParent() {
    // Get the offsetParent, if any.
    this.offsetParent = getOffsetParent(this.currentNode);
    // And the offsetParent quads.
    this.parentQuads = getAdjustedQuads(
        this.win, this.offsetParent.element, "padding");

    const el = this.getElement("offset-parent");

    const isPositioned = this.computedStyle.position === "absolute" ||
                       this.computedStyle.position === "fixed";
    const isRelative = this.computedStyle.position === "relative";
    let isHighlighted = false;

    if (this.offsetParent.element && isPositioned) {
      const {p1, p2, p3, p4} = this.parentQuads[0];
      const points = p1.x + "," + p1.y + " " +
                   p2.x + "," + p2.y + " " +
                   p3.x + "," + p3.y + " " +
                   p4.x + "," + p4.y;
      el.setAttribute("points", points);
      isHighlighted = true;
    } else if (isRelative) {
      const xDelta = parseFloat(this.computedStyle.left);
      const yDelta = parseFloat(this.computedStyle.top);
      if (xDelta || yDelta) {
        const {p1, p2, p3, p4} = this.currentQuads.margin[0];
        const points = (p1.x - xDelta) + "," + (p1.y - yDelta) + " " +
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
  }

  updateCurrentNode() {
    const box = this.getElement("current-node");
    const {p1, p2, p3, p4} = this.currentQuads.margin[0];
    const attr = p1.x + "," + p1.y + " " +
               p2.x + "," + p2.y + " " +
               p3.x + "," + p3.y + " " +
               p4.x + "," + p4.y;
    box.setAttribute("points", attr);
    box.removeAttribute("hidden");
  }

  _hide() {
    setIgnoreLayoutChanges(true);

    this.getElement("root").setAttribute("hidden", "true");
    this.getElement("current-node").setAttribute("hidden", "true");
    this.getElement("offset-parent").setAttribute("hidden", "true");
    this.hideArrows();

    this.definedProperties.clear();

    setIgnoreLayoutChanges(false, this.highlighterEnv.document.documentElement);
  }

  hideArrows() {
    for (const side of GeoProp.SIDES) {
      this.getElement("arrow-" + side).setAttribute("hidden", "true");
      this.getElement("label-" + side).setAttribute("hidden", "true");
      this.getElement("handler-" + side).setAttribute("hidden", "true");
    }
  }

  updateArrows() {
    this.hideArrows();

    // Position arrows always end at the node's margin box.
    const marginBox = this.currentQuads.margin[0].bounds;

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
    const getSideArrowStartPos = side => {
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
      return -1 * this.currentNode.ownerGlobal["scroll" +
                                              GeoProp.axis(side).toUpperCase()];
    };

    for (const side of GeoProp.SIDES) {
      const sideProp = this.definedProperties.get(side);
      if (!sideProp) {
        continue;
      }

      const mainAxisStartPos = getSideArrowStartPos(side);
      const mainAxisEndPos = marginBox[side];
      const crossAxisPos = marginBox[GeoProp.crossAxisStart(side)] +
                         marginBox[GeoProp.crossAxisSize(side)] / 2;

      this.updateArrow(side, mainAxisStartPos, mainAxisEndPos, crossAxisPos,
                       sideProp.cssRule.style.getPropertyValue(side));
    }
  }

  updateArrow(side, mainStart, mainEnd, crossPos, labelValue) {
    const arrowEl = this.getElement("arrow-" + side);
    const labelEl = this.getElement("label-" + side);
    const labelTextEl = this.getElement("label-text-" + side);
    const handlerEl = this.getElement("handler-" + side);

    // Position the arrow <line>.
    arrowEl.setAttribute(GeoProp.axis(side) + "1", mainStart);
    arrowEl.setAttribute(GeoProp.crossAxis(side) + "1", crossPos);
    arrowEl.setAttribute(GeoProp.axis(side) + "2", mainEnd);
    arrowEl.setAttribute(GeoProp.crossAxis(side) + "2", crossPos);
    arrowEl.removeAttribute("hidden");

    handlerEl.setAttribute("c" + GeoProp.axis(side), mainEnd);
    handlerEl.setAttribute("c" + GeoProp.crossAxis(side), crossPos);
    handlerEl.removeAttribute("hidden");

    // Position the label <text> in the middle of the arrow (making sure it's
    // not hidden below the fold).
    const capitalize = str => str[0].toUpperCase() + str.substring(1);
    const winMain = this.win["inner" + capitalize(GeoProp.mainAxisSize(side))];
    let labelMain = mainStart + (mainEnd - mainStart) / 2;
    if ((mainStart > 0 && mainStart < winMain) ||
        (mainEnd > 0 && mainEnd < winMain)) {
      if (labelMain < GEOMETRY_LABEL_SIZE) {
        labelMain = GEOMETRY_LABEL_SIZE;
      } else if (labelMain > winMain - GEOMETRY_LABEL_SIZE) {
        labelMain = winMain - GEOMETRY_LABEL_SIZE;
      }
    }
    const labelCross = crossPos;
    labelEl.setAttribute("transform", GeoProp.isHorizontal(side)
                         ? "translate(" + labelMain + " " + labelCross + ")"
                         : "translate(" + labelCross + " " + labelMain + ")");
    labelEl.removeAttribute("hidden");
    labelTextEl.setTextContent(labelValue);
  }

  onWillNavigate({ isTopLevel }) {
    if (isTopLevel) {
      this.hide();
    }
  }
}

exports.GeometryEditorHighlighter = GeometryEditorHighlighter;
