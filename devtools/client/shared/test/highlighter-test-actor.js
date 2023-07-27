/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

/* exported HighlighterTestActor, HighlighterTestFront */

"use strict";

// A helper actor for testing highlighters.
// ⚠️ This should only be used for getting data for objects using CanvasFrameAnonymousContentHelper,
// that we can't get directly from tests.
const {
  getRect,
  getAdjustedQuads,
} = require("resource://devtools/shared/layout/utils.js");

// Set up a dummy environment so that EventUtils works. We need to be careful to
// pass a window object into each EventUtils method we call rather than having
// it rely on the |window| global.
const EventUtils = {};
EventUtils.window = {};
EventUtils.parent = {};
/* eslint-disable camelcase */
EventUtils._EU_Ci = Ci;
EventUtils._EU_Cc = Cc;
/* eslint-disable camelcase */
Services.scriptloader.loadSubScript(
  "chrome://mochikit/content/tests/SimpleTest/EventUtils.js",
  EventUtils
);

// We're an actor so we don't run in the browser test environment, so
// we need to import TestUtils manually despite what the linter thinks.
// eslint-disable-next-line mozilla/no-redeclare-with-import-autofix
const { TestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TestUtils.sys.mjs"
);

const protocol = require("resource://devtools/shared/protocol.js");
const { Arg, RetVal } = protocol;

const dumpn = msg => {
  dump(msg + "\n");
};

/**
 * Get the instance of CanvasFrameAnonymousContentHelper used by a given
 * highlighter actor.
 * The instance provides methods to get/set attributes/text/style on nodes of
 * the highlighter, inserted into the nsCanvasFrame.
 * @see /devtools/server/actors/highlighters.js
 * @param {String} actorID
 */
function getHighlighterCanvasFrameHelper(conn, actorID) {
  // Retrieve the CustomHighlighterActor by its actorID:
  const actor = conn.getActor(actorID);
  if (!actor) {
    return null;
  }

  // Retrieve the sub class instance specific to each highlighter type:
  let highlighter = actor.instance;

  // SelectorHighlighter and TabbingOrderHighlighter can hold multiple highlighters.
  // For now, only retrieve the first highlighter.
  if (
    highlighter._highlighters &&
    Array.isArray(highlighter._highlighters) &&
    highlighter._highlighters.length
  ) {
    highlighter = highlighter._highlighters[0];
  }

  // Now, `highlighter` should be a final highlighter class, exposing
  // `CanvasFrameAnonymousContentHelper` via a `markup` attribute.
  if (highlighter.markup) {
    return highlighter.markup;
  }

  // Here we didn't find any highlighter; it can happen if the actor is a
  // FontsHighlighter (which does not use a CanvasFrameAnonymousContentHelper).
  return null;
}

var highlighterTestSpec = protocol.generateActorSpec({
  typeName: "highlighterTest",

  events: {
    "highlighter-updated": {},
  },

  methods: {
    getHighlighterAttribute: {
      request: {
        nodeID: Arg(0, "string"),
        name: Arg(1, "string"),
        actorID: Arg(2, "string"),
      },
      response: {
        value: RetVal("string"),
      },
    },
    getHighlighterBoundingClientRect: {
      request: {
        nodeID: Arg(0, "string"),
        actorID: Arg(1, "string"),
      },
      response: {
        value: RetVal("json"),
      },
    },
    getHighlighterComputedStyle: {
      request: {
        nodeID: Arg(0, "string"),
        property: Arg(1, "string"),
        actorID: Arg(2, "string"),
      },
      response: {
        value: RetVal("string"),
      },
    },
    getHighlighterNodeTextContent: {
      request: {
        nodeID: Arg(0, "string"),
        actorID: Arg(1, "string"),
      },
      response: {
        value: RetVal("string"),
      },
    },
    getSelectorHighlighterBoxNb: {
      request: {
        highlighter: Arg(0, "string"),
      },
      response: {
        value: RetVal("number"),
      },
    },
    changeHighlightedNodeWaitForUpdate: {
      request: {
        name: Arg(0, "string"),
        value: Arg(1, "string"),
        actorID: Arg(2, "string"),
      },
      response: {},
    },
    registerOneTimeHighlighterUpdate: {
      request: {
        actorID: Arg(0, "string"),
      },
      response: {},
    },
    getNodeRect: {
      request: {
        selector: Arg(0, "string"),
      },
      response: {
        value: RetVal("json"),
      },
    },
    getTextNodeRect: {
      request: {
        parentSelector: Arg(0, "string"),
        childNodeIndex: Arg(1, "number"),
      },
      response: {
        value: RetVal("json"),
      },
    },
    isPausedDebuggerOverlayVisible: {
      request: {},
      response: {
        value: RetVal("boolean"),
      },
    },
    clickPausedDebuggerOverlayButton: {
      request: {
        id: Arg(0, "string"),
      },
      response: {},
    },
    isEyeDropperVisible: {
      request: {},
      response: {
        value: RetVal("boolean"),
      },
    },
    getEyeDropperElementAttribute: {
      request: {
        elementId: Arg(0, "string"),
        attributeName: Arg(1, "string"),
      },
      response: {
        value: RetVal("string"),
      },
    },
    getEyeDropperColorValue: {
      request: {},
      response: {
        value: RetVal("string"),
      },
    },
    getTabbingOrderHighlighterData: {
      request: {},
      response: {
        value: RetVal("json"),
      },
    },
  },
});

class HighlighterTestActor extends protocol.Actor {
  constructor(conn, targetActor, options) {
    super(conn, highlighterTestSpec);

    this.targetActor = targetActor;
  }

  get content() {
    return this.targetActor.window;
  }

  /**
   * Helper to retrieve a DOM element.
   * @param {string | array} selector Either a regular selector string
   *   or a selector array. If an array, each item, except the last one
   *   are considered matching an iframe, so that we can query element
   *   within deep iframes.
   */
  _querySelector(selector) {
    let document = this.content.document;
    if (Array.isArray(selector)) {
      const fullSelector = selector.join(" >> ");
      while (selector.length > 1) {
        const str = selector.shift();
        const iframe = document.querySelector(str);
        if (!iframe) {
          throw new Error(
            'Unable to find element with selector "' +
              str +
              '"' +
              " (full selector:" +
              fullSelector +
              ")"
          );
        }
        if (!iframe.contentWindow) {
          throw new Error(
            "Iframe selector doesn't target an iframe \"" +
              str +
              '"' +
              " (full selector:" +
              fullSelector +
              ")"
          );
        }
        document = iframe.contentWindow.document;
      }
      selector = selector.shift();
    }
    const node = document.querySelector(selector);
    if (!node) {
      throw new Error(
        'Unable to find element with selector "' + selector + '"'
      );
    }
    return node;
  }

  /**
   * Get a value for a given attribute name, on one of the elements of the box
   * model highlighter, given its ID.
   * @param {String} nodeID The full ID of the element to get the attribute for
   * @param {String} name The name of the attribute to get
   * @param {String} actorID The highlighter actor ID
   * @return {String} The value, if found, null otherwise
   */
  getHighlighterAttribute(nodeID, name, actorID) {
    const helper = getHighlighterCanvasFrameHelper(this.conn, actorID);

    if (!helper) {
      throw new Error(`Highlighter not found`);
    }

    return helper.getAttributeForElement(nodeID, name);
  }

  /**
   * Get the bounding client rect for an highlighter element, given its ID.
   *
   * @param {String} nodeID The full ID of the element to get the DOMRect for
   * @param {String} actorID The highlighter actor ID
   * @return {DOMRect} The value, if found, null otherwise
   */
  getHighlighterBoundingClientRect(nodeID, actorID) {
    const helper = getHighlighterCanvasFrameHelper(this.conn, actorID);

    if (!helper) {
      throw new Error(`Highlighter not found`);
    }

    return helper.getBoundingClientRect(nodeID);
  }

  /**
   * Get the computed style for a given property, on one of the elements of the
   * box model highlighter, given its ID.
   * @param {String} nodeID The full ID of the element to get the attribute for
   * @param {String} property The name of the property
   * @param {String} actorID The highlighter actor ID
   * @return {String} The computed style of the property
   */
  getHighlighterComputedStyle(nodeID, property, actorID) {
    const helper = getHighlighterCanvasFrameHelper(this.conn, actorID);

    if (!helper) {
      throw new Error(`Highlighter not found`);
    }

    return helper.getElement(nodeID).computedStyle.getPropertyValue(property);
  }

  /**
   * Get the textcontent of one of the elements of the box model highlighter,
   * given its ID.
   * @param {String} nodeID The full ID of the element to get the attribute for
   * @param {String} actorID The highlighter actor ID
   * @return {String} The textcontent value
   */
  getHighlighterNodeTextContent(nodeID, actorID) {
    let value;
    const helper = getHighlighterCanvasFrameHelper(this.conn, actorID);
    if (helper) {
      value = helper.getTextContentForElement(nodeID);
    }
    return value;
  }

  /**
   * Get the number of box-model highlighters created by the SelectorHighlighter
   * @param {String} actorID The highlighter actor ID
   * @return {Number} The number of box-model highlighters created, or null if the
   * SelectorHighlighter was not found.
   */
  getSelectorHighlighterBoxNb(actorID) {
    const highlighter = this.conn.getActor(actorID);
    const { _highlighter: h } = highlighter;
    if (!h || !h._highlighters) {
      return null;
    }
    return h._highlighters.length;
  }

  /**
   * Subscribe to the box-model highlighter's update event, modify an attribute of
   * the currently highlighted node and send a message when the highlighter has
   * updated.
   * @param {String} the name of the attribute to be changed
   * @param {String} the new value for the attribute
   * @param {String} actorID The highlighter actor ID
   */
  changeHighlightedNodeWaitForUpdate(name, value, actorID) {
    return new Promise(resolve => {
      const highlighter = this.conn.getActor(actorID);
      const { _highlighter: h } = highlighter;

      h.once("updated", resolve);

      h.currentNode.setAttribute(name, value);
    });
  }

  /**
   * Register a one-time "updated" event listener.
   * The method does not wait for the "updated" event itself so the response can be sent
   * back and the client would know the event listener is properly set.
   * A separate event, "highlighter-updated", will be emitted when the highlighter updates.
   *
   * @param {String} actorID The highlighter actor ID
   */
  registerOneTimeHighlighterUpdate(actorID) {
    const { _highlighter } = this.conn.getActor(actorID);
    _highlighter.once("updated").then(() => this.emit("highlighter-updated"));

    // Return directly so the client knows the event listener is set
  }

  async getNodeRect(selector) {
    const node = this._querySelector(selector);
    return getRect(this.content, node, this.content);
  }

  async getTextNodeRect(parentSelector, childNodeIndex) {
    const parentNode = this._querySelector(parentSelector);
    const node = parentNode.childNodes[childNodeIndex];
    return getAdjustedQuads(this.content, node)[0].bounds;
  }

  /**
   * @returns {PausedDebuggerOverlay} The paused overlay instance
   */
  _getPausedDebuggerOverlay() {
    // We use `_pauseOverlay` since it's the cached value; `pauseOverlay` is a getter that
    // will create the overlay when called (if it does not exist yet).
    return this.targetActor?.threadActor?._pauseOverlay;
  }

  isPausedDebuggerOverlayVisible() {
    const pauseOverlay = this._getPausedDebuggerOverlay();
    if (!pauseOverlay) {
      return false;
    }

    const root = pauseOverlay.getElement("root");
    const toolbar = pauseOverlay.getElement("toolbar");

    return (
      root.getAttribute("hidden") !== "true" &&
      root.getAttribute("overlay") == "true" &&
      toolbar.getAttribute("hidden") !== "true" &&
      !!toolbar.getTextContent()
    );
  }

  /**
   * Simulates a click on a button of the debugger pause overlay.
   *
   * @param {String} id: The id of the element (e.g. "paused-dbg-resume-button").
   */
  async clickPausedDebuggerOverlayButton(id) {
    const pauseOverlay = this._getPausedDebuggerOverlay();
    if (!pauseOverlay) {
      return;
    }

    // Because the highlighter markup elements live inside an anonymous content frame which
    // does not expose an API to dispatch events to them, we can't directly dispatch
    // events to the nodes themselves.
    // We're directly calling `handleEvent` on the pause overlay, which is the mouse events
    // listener callback on the overlay.
    pauseOverlay.handleEvent({ type: "mousedown", target: { id } });
  }

  /**
   * @returns {EyeDropper}
   */
  _getEyeDropper() {
    const form = this.targetActor.form();
    const inspectorActor = this.conn._getOrCreateActor(form.inspectorActor);
    return inspectorActor?._eyeDropper;
  }

  isEyeDropperVisible() {
    const eyeDropper = this._getEyeDropper();
    if (!eyeDropper) {
      return false;
    }

    return eyeDropper.getElement("root").getAttribute("hidden") !== "true";
  }

  getEyeDropperElementAttribute(elementId, attributeName) {
    const eyeDropper = this._getEyeDropper();
    if (!eyeDropper) {
      return null;
    }

    return eyeDropper.getElement(elementId).getAttribute(attributeName);
  }

  async getEyeDropperColorValue() {
    const eyeDropper = this._getEyeDropper();
    if (!eyeDropper) {
      return null;
    }

    // It might happen that while the eyedropper isn't hidden anymore, the color-value
    // is not set yet.
    const color = await TestUtils.waitForCondition(() => {
      const colorValueElement = eyeDropper.getElement("color-value");
      const textContent = colorValueElement.getTextContent();
      return textContent;
    }, "Couldn't get a non-empty text content for the color-value element");

    return color;
  }

  /**
   * Get the TabbingOrderHighlighter for the associated targetActor
   *
   * @returns {TabbingOrderHighlighter}
   */
  _getTabbingOrderHighlighter() {
    const form = this.targetActor.form();
    const accessibilityActor = this.conn._getOrCreateActor(
      form.accessibilityActor
    );

    if (!accessibilityActor) {
      return null;
    }
    // We use `_tabbingOrderHighlighter` since it's the cached value; `tabbingOrderHighlighter`
    // is a getter that will create the highlighter when called (if it does not exist yet).
    return accessibilityActor.walker?._tabbingOrderHighlighter;
  }

  /**
   * Get a representation of the NodeTabbingOrderHighlighters created by the
   * TabbingOrderHighlighter of a given targetActor.
   *
   * @returns {Array<String>} An array which will contain as many entry as they are
   *          NodeTabbingOrderHighlighters displayed.
   *          Each item will be of the form `nodename[#id]: index`.
   *          For example:
   *          [
   *            `button#top-btn-1 : 1`,
   *            `html : 2`,
   *            `button#iframe-btn-1 : 3`,
   *            `button#iframe-btn-2 : 4`,
   *            `button#top-btn-2 : 5`,
   *          ]
   */
  getTabbingOrderHighlighterData() {
    const highlighter = this._getTabbingOrderHighlighter();
    if (!highlighter) {
      return [];
    }

    const nodeTabbingOrderHighlighters = [
      ...highlighter._highlighter._highlighters.values(),
    ].filter(h => h.getElement("root").getAttribute("hidden") !== "true");

    return nodeTabbingOrderHighlighters.map(h => {
      let nodeStr = h.currentNode.nodeName.toLowerCase();
      if (h.currentNode.id) {
        nodeStr = `${nodeStr}#${h.currentNode.id}`;
      }
      return `${nodeStr} : ${h.getElement("root").getTextContent()}`;
    });
  }
}
exports.HighlighterTestActor = HighlighterTestActor;

class HighlighterTestFront extends protocol.FrontClassWithSpec(
  highlighterTestSpec
) {
  constructor(client, targetFront, parentFront) {
    super(client, targetFront, parentFront);
    this.formAttributeName = "highlighterTestActor";
    // The currently active highlighter is obtained by calling a custom getter
    // provided manually after requesting TestFront. See `getHighlighterTestFront(toolbox)`
    this._highlighter = null;
  }

  /**
   * Override the highlighter getter with a custom method that returns
   * the currently active highlighter instance.
   *
   * @param {Function|Highlighter} _customHighlighterGetter
   */
  set highlighter(_customHighlighterGetter) {
    this._highlighter = _customHighlighterGetter;
  }

  /**
   * The currently active highlighter instance.
   * If there is a custom getter for the highlighter, return its result.
   *
   * @return {Highlighter|null}
   */
  get highlighter() {
    return typeof this._highlighter === "function"
      ? this._highlighter()
      : this._highlighter;
  }

  /* eslint-disable max-len */
  changeHighlightedNodeWaitForUpdate(name, value, highlighter) {
    /* eslint-enable max-len */
    return super.changeHighlightedNodeWaitForUpdate(
      name,
      value,
      (highlighter || this.highlighter).actorID
    );
  }

  /**
   * Get the value of an attribute on one of the highlighter's node.
   * @param {String} nodeID The Id of the node in the highlighter.
   * @param {String} name The name of the attribute.
   * @param {Object} highlighter Optional custom highlighter to target
   * @return {String} value
   */
  getHighlighterNodeAttribute(nodeID, name, highlighter) {
    return this.getHighlighterAttribute(
      nodeID,
      name,
      (highlighter || this.highlighter).actorID
    );
  }

  getHighlighterNodeTextContent(nodeID, highlighter) {
    return super.getHighlighterNodeTextContent(
      nodeID,
      (highlighter || this.highlighter).actorID
    );
  }

  /**
   * Get the computed style of a property on one of the highlighter's node.
   * @param {String} nodeID The Id of the node in the highlighter.
   * @param {String} property The name of the property.
   * @param {Object} highlighter Optional custom highlighter to target
   * @return {String} value
   */
  getHighlighterComputedStyle(nodeID, property, highlighter) {
    return super.getHighlighterComputedStyle(
      nodeID,
      property,
      (highlighter || this.highlighter).actorID
    );
  }

  /**
   * Is the highlighter currently visible on the page?
   */
  async isHighlighting() {
    // Once the highlighter is hidden, the reference to it is lost.
    // Assume it is not highlighting.
    if (!this.highlighter) {
      return false;
    }

    try {
      const hidden = await this.getHighlighterNodeAttribute(
        "box-model-elements",
        "hidden"
      );
      return hidden === null;
    } catch (e) {
      if (e.message.match(/Highlighter not found/)) {
        return false;
      }
      throw e;
    }
  }

  /**
   * Get the current rect of the border region of the box-model highlighter
   */
  async getSimpleBorderRect() {
    const { border } = await this.getBoxModelStatus();
    const { p1, p2, p4 } = border.points;

    return {
      top: p1.y,
      left: p1.x,
      width: p2.x - p1.x,
      height: p4.y - p1.y,
    };
  }

  /**
   * Get the current positions and visibility of the various box-model highlighter
   * elements.
   */
  async getBoxModelStatus() {
    const isVisible = await this.isHighlighting();

    const ret = {
      visible: isVisible,
    };

    for (const region of ["margin", "border", "padding", "content"]) {
      const points = await this._getPointsForRegion(region);
      const visible = await this._isRegionHidden(region);
      ret[region] = { points, visible };
    }

    ret.guides = {};
    for (const guide of ["top", "right", "bottom", "left"]) {
      ret.guides[guide] = await this._getGuideStatus(guide);
    }

    return ret;
  }

  /**
   * Check that the box-model highlighter is currently highlighting the node matching the
   * given selector.
   * @param {String} selector
   * @return {Boolean}
   */
  async assertHighlightedNode(selector) {
    const rect = await this.getNodeRect(selector);
    return this.isNodeRectHighlighted(rect);
  }

  /**
   * Check that the box-model highlighter is currently highlighting the text node that can
   * be found at a given index within the list of childNodes of a parent element matching
   * the given selector.
   * @param {String} parentSelector
   * @param {Number} childNodeIndex
   * @return {Boolean}
   */
  async assertHighlightedTextNode(parentSelector, childNodeIndex) {
    const rect = await this.getTextNodeRect(parentSelector, childNodeIndex);
    return this.isNodeRectHighlighted(rect);
  }

  /**
   * Check that the box-model highlighter is currently highlighting the given rect.
   * @param {Object} rect
   * @return {Boolean}
   */
  async isNodeRectHighlighted({ left, top, width, height }) {
    const { visible, border } = await this.getBoxModelStatus();
    let points = border.points;
    if (!visible) {
      return false;
    }

    // Check that the node is within the box model
    const right = left + width;
    const bottom = top + height;

    // Converts points dictionnary into an array
    const list = [];
    for (let i = 1; i <= 4; i++) {
      const p = points["p" + i];
      list.push([p.x, p.y]);
    }
    points = list;

    // Check that each point of the node is within the box model
    return (
      isInside([left, top], points) &&
      isInside([right, top], points) &&
      isInside([right, bottom], points) &&
      isInside([left, bottom], points)
    );
  }

  /**
   * Get the coordinate (points attribute) from one of the polygon elements in the
   * box model highlighter.
   */
  async _getPointsForRegion(region) {
    const d = await this.getHighlighterNodeAttribute(
      "box-model-" + region,
      "d"
    );

    if (!d) {
      return null;
    }

    const polygons = d.match(/M[^M]+/g);
    if (!polygons) {
      return null;
    }

    const points = polygons[0]
      .trim()
      .split(" ")
      .map(i => {
        return i.replace(/M|L/, "").split(",");
      });

    return {
      p1: {
        x: parseFloat(points[0][0]),
        y: parseFloat(points[0][1]),
      },
      p2: {
        x: parseFloat(points[1][0]),
        y: parseFloat(points[1][1]),
      },
      p3: {
        x: parseFloat(points[2][0]),
        y: parseFloat(points[2][1]),
      },
      p4: {
        x: parseFloat(points[3][0]),
        y: parseFloat(points[3][1]),
      },
    };
  }

  /**
   * Is a given region polygon element of the box-model highlighter currently
   * hidden?
   */
  async _isRegionHidden(region) {
    const value = await this.getHighlighterNodeAttribute(
      "box-model-" + region,
      "hidden"
    );
    return value !== null;
  }

  async _getGuideStatus(location) {
    const id = "box-model-guide-" + location;

    const hidden = await this.getHighlighterNodeAttribute(id, "hidden");
    const x1 = await this.getHighlighterNodeAttribute(id, "x1");
    const y1 = await this.getHighlighterNodeAttribute(id, "y1");
    const x2 = await this.getHighlighterNodeAttribute(id, "x2");
    const y2 = await this.getHighlighterNodeAttribute(id, "y2");

    return {
      visible: !hidden,
      x1,
      y1,
      x2,
      y2,
    };
  }

  /**
   * Get the coordinates of the rectangle that is defined by the 4 guides displayed
   * in the toolbox box-model highlighter.
   * @return {Object} Null if at least one guide is hidden. Otherwise an object
   * with p1, p2, p3, p4 properties being {x, y} objects.
   */
  async getGuidesRectangle() {
    const tGuide = await this._getGuideStatus("top");
    const rGuide = await this._getGuideStatus("right");
    const bGuide = await this._getGuideStatus("bottom");
    const lGuide = await this._getGuideStatus("left");

    if (
      !tGuide.visible ||
      !rGuide.visible ||
      !bGuide.visible ||
      !lGuide.visible
    ) {
      return null;
    }

    return {
      p1: { x: lGuide.x1, y: tGuide.y1 },
      p2: { x: +rGuide.x1 + 1, y: tGuide.y1 },
      p3: { x: +rGuide.x1 + 1, y: +bGuide.y1 + 1 },
      p4: { x: lGuide.x1, y: +bGuide.y1 + 1 },
    };
  }

  /**
   * Get the "d" attribute value for one of the box-model highlighter's region
   * <path> elements, and parse it to a list of points.
   * @param {String} region The box model region name.
   * @param {Front} highlighter The front of the highlighter.
   * @return {Object} The object returned has the following form:
   * - d {String} the d attribute value
   * - points {Array} an array of all the polygons defined by the path. Each box
   *   is itself an Array of points, themselves being [x,y] coordinates arrays.
   */
  async getHighlighterRegionPath(region, highlighter) {
    const d = await this.getHighlighterNodeAttribute(
      `box-model-${region}`,
      "d",
      highlighter
    );
    if (!d) {
      return { d: null };
    }

    const polygons = d.match(/M[^M]+/g);
    if (!polygons) {
      return { d };
    }

    const points = [];
    for (const polygon of polygons) {
      points.push(
        polygon
          .trim()
          .split(" ")
          .map(i => {
            return i.replace(/M|L/, "").split(",");
          })
      );
    }

    return { d, points };
  }
}
protocol.registerFront(HighlighterTestFront);
/**
 * Check whether a point is included in a polygon.
 * Taken and tweaked from:
 * https://github.com/iominh/point-in-polygon-extended/blob/master/src/index.js#L30-L85
 * @param {Array} point [x,y] coordinates
 * @param {Array} polygon An array of [x,y] points
 * @return {Boolean}
 */
function isInside(point, polygon) {
  if (polygon.length === 0) {
    return false;
  }

  // Reduce the length of the fractional part because this is likely to cause errors when
  // the point is on the edge of the polygon.
  point = point.map(n => n.toFixed(2));
  polygon = polygon.map(p => p.map(n => n.toFixed(2)));

  const n = polygon.length;
  const newPoints = polygon.slice(0);
  newPoints.push(polygon[0]);
  let wn = 0;

  // loop through all edges of the polygon
  for (let i = 0; i < n; i++) {
    // Accept points on the edges
    const r = isLeft(newPoints[i], newPoints[i + 1], point);
    if (r === 0) {
      return true;
    }
    if (newPoints[i][1] <= point[1]) {
      if (newPoints[i + 1][1] > point[1] && r > 0) {
        wn++;
      }
    } else if (newPoints[i + 1][1] <= point[1] && r < 0) {
      wn--;
    }
  }
  if (wn === 0) {
    dumpn(JSON.stringify(point) + " is outside of " + JSON.stringify(polygon));
  }
  // the point is outside only when this winding number wn===0, otherwise it's inside
  return wn !== 0;
}

function isLeft(p0, p1, p2) {
  const l =
    (p1[0] - p0[0]) * (p2[1] - p0[1]) - (p2[0] - p0[0]) * (p1[1] - p0[1]);
  return l;
}
