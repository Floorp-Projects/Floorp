/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

/* exported TestActor, TestFront */

"use strict";

// A helper actor for inspector and markupview tests.
const { Ci, Cu, Cc } = require("chrome");
const Services = require("Services");
const {
  getRect,
  getAdjustedQuads,
  getWindowDimensions,
} = require("devtools/shared/layout/utils");
const {
  isAgentStylesheet,
  getCSSStyleRules,
} = require("devtools/shared/inspector/css-logic");
const InspectorUtils = require("InspectorUtils");

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

var ChromeUtils = require("ChromeUtils");
const { TestUtils } = ChromeUtils.import(
  "resource://testing-common/TestUtils.jsm"
);

const protocol = require("devtools/shared/protocol");
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
  const actor = conn.getActor(actorID);
  if (actor && actor._highlighter) {
    return actor._highlighter.markup;
  }
  return null;
}

var testSpec = protocol.generateActorSpec({
  typeName: "test",

  methods: {
    getNumberOfElementMatches: {
      request: {
        selector: Arg(0, "string"),
      },
      response: {
        value: RetVal("number"),
      },
    },
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
    waitForHighlighterEvent: {
      request: {
        event: Arg(0, "string"),
        actorID: Arg(1, "string"),
      },
      response: {},
    },
    waitForEventOnNode: {
      request: {
        eventName: Arg(0, "string"),
        selector: Arg(1, "nullable:string"),
      },
      response: {},
    },
    changeZoomLevel: {
      request: {
        level: Arg(0, "string"),
        actorID: Arg(1, "string"),
      },
      response: {},
    },
    getAllAdjustedQuads: {
      request: {
        selector: Arg(0, "string"),
      },
      response: {
        value: RetVal("json"),
      },
    },
    synthesizeMouse: {
      request: {
        object: Arg(0, "json"),
      },
      response: {},
    },
    synthesizeKey: {
      request: {
        args: Arg(0, "json"),
      },
      response: {},
    },
    scrollIntoView: {
      request: {
        args: Arg(0, "string"),
      },
      response: {},
    },
    hasPseudoClassLock: {
      request: {
        selector: Arg(0, "string"),
        pseudo: Arg(1, "string"),
      },
      response: {
        value: RetVal("boolean"),
      },
    },
    loadAndWaitForCustomEvent: {
      request: {
        url: Arg(0, "string"),
      },
      response: {},
    },
    hasNode: {
      request: {
        selector: Arg(0, "string"),
      },
      response: {
        value: RetVal("boolean"),
      },
    },
    getBoundingClientRect: {
      request: {
        selector: Arg(0, "string"),
      },
      response: {
        value: RetVal("json"),
      },
    },
    setProperty: {
      request: {
        selector: Arg(0, "string"),
        property: Arg(1, "string"),
        value: Arg(2, "string"),
      },
      response: {},
    },
    getProperty: {
      request: {
        selector: Arg(0, "string"),
        property: Arg(1, "string"),
      },
      response: {
        value: RetVal("string"),
      },
    },
    reload: {
      request: {},
      response: {},
    },
    reloadFrame: {
      request: {
        selector: Arg(0, "string"),
      },
      response: {},
    },
    eval: {
      request: {
        js: Arg(0, "string"),
      },
      response: {
        value: RetVal("nullable:json"),
      },
    },
    scrollWindow: {
      request: {
        x: Arg(0, "number"),
        y: Arg(1, "number"),
        relative: Arg(2, "nullable:boolean"),
      },
      response: {
        value: RetVal("json"),
      },
    },
    reflow: {},
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
    getNodeInfo: {
      request: {
        selector: Arg(0, "string"),
      },
      response: {
        value: RetVal("json"),
      },
    },
    getStyleSheetsInfoForNode: {
      request: {
        selector: Arg(0, "string"),
      },
      response: {
        value: RetVal("json"),
      },
    },
    getWindowDimensions: {
      request: {},
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
      request: {
        inspectorActorID: Arg(0, "string"),
      },
      response: {
        value: RetVal("boolean"),
      },
    },
    getEyeDropperElementAttribute: {
      request: {
        inspectorActorID: Arg(0, "string"),
        elementId: Arg(1, "string"),
        attributeName: Arg(2, "string"),
      },
      response: {
        value: RetVal("string"),
      },
    },
    getEyeDropperColorValue: {
      request: {
        inspectorActorID: Arg(0, "string"),
      },
      response: {
        value: RetVal("string"),
      },
    },
  },
});

var TestActor = protocol.ActorClassWithSpec(testSpec, {
  initialize: function(conn, targetActor, options) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this.conn = conn;
    this.targetActor = targetActor;
  },

  get content() {
    return this.targetActor.window;
  },

  /**
   * Helper to retrieve a DOM element.
   * @param {string | array} selector Either a regular selector string
   *   or a selector array. If an array, each item, except the last one
   *   are considered matching an iframe, so that we can query element
   *   within deep iframes.
   */
  _querySelector: function(selector) {
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
  },
  /**
   * Helper to get the number of elements matching a selector
   * @param {string} CSS selector.
   */
  getNumberOfElementMatches: function(selector, root = this.content.document) {
    return root.querySelectorAll(selector).length;
  },

  /**
   * Get a value for a given attribute name, on one of the elements of the box
   * model highlighter, given its ID.
   * @param {Object} msg The msg.data part expects the following properties
   * - {String} nodeID The full ID of the element to get the attribute for
   * - {String} name The name of the attribute to get
   * - {String} actorID The highlighter actor ID
   * @return {String} The value, if found, null otherwise
   */
  getHighlighterAttribute: function(nodeID, name, actorID) {
    const helper = getHighlighterCanvasFrameHelper(this.conn, actorID);
    if (helper) {
      return helper.getAttributeForElement(nodeID, name);
    }
    return null;
  },

  /**
   * Get the textcontent of one of the elements of the box model highlighter,
   * given its ID.
   * @param {String} nodeID The full ID of the element to get the attribute for
   * @param {String} actorID The highlighter actor ID
   * @return {String} The textcontent value
   */
  getHighlighterNodeTextContent: function(nodeID, actorID) {
    let value;
    const helper = getHighlighterCanvasFrameHelper(this.conn, actorID);
    if (helper) {
      value = helper.getTextContentForElement(nodeID);
    }
    return value;
  },

  /**
   * Get the number of box-model highlighters created by the SelectorHighlighter
   * @param {String} actorID The highlighter actor ID
   * @return {Number} The number of box-model highlighters created, or null if the
   * SelectorHighlighter was not found.
   */
  getSelectorHighlighterBoxNb: function(actorID) {
    const highlighter = this.conn.getActor(actorID);
    const { _highlighter: h } = highlighter;
    if (!h || !h._highlighters) {
      return null;
    }
    return h._highlighters.length;
  },

  /**
   * Subscribe to the box-model highlighter's update event, modify an attribute of
   * the currently highlighted node and send a message when the highlighter has
   * updated.
   * @param {String} the name of the attribute to be changed
   * @param {String} the new value for the attribute
   * @param {String} actorID The highlighter actor ID
   */
  changeHighlightedNodeWaitForUpdate: function(name, value, actorID) {
    return new Promise(resolve => {
      const highlighter = this.conn.getActor(actorID);
      const { _highlighter: h } = highlighter;

      h.once("updated", resolve);

      h.currentNode.setAttribute(name, value);
    });
  },

  /**
   * Subscribe to a given highlighter event and respond when the event is received.
   * @param {String} event The name of the highlighter event to listen to
   * @param {String} actorID The highlighter actor ID
   */
  waitForHighlighterEvent: function(event, actorID) {
    const highlighter = this.conn.getActor(actorID);
    const { _highlighter: h } = highlighter;

    return h.once(event);
  },

  /**
   * Wait for a specific event on a node matching the provided selector.
   * @param {String} eventName The name of the event to listen to
   * @param {String} selector Optional:  css selector of the node which should
   *        trigger the event. If ommitted, target will be the content window
   */
  waitForEventOnNode: function(eventName, selector) {
    return new Promise(resolve => {
      const node = selector ? this._querySelector(selector) : this.content;
      node.addEventListener(
        eventName,
        function() {
          resolve();
        },
        { once: true }
      );
    });
  },

  /**
   * Change the zoom level of the page.
   * Optionally subscribe to the box-model highlighter's update event and waiting
   * for it to refresh before responding.
   * @param {Number} level The new zoom level
   * @param {String} actorID Optional. The highlighter actor ID
   */
  changeZoomLevel: function(level, actorID) {
    dumpn("Zooming page to " + level);
    return new Promise(resolve => {
      if (actorID) {
        const actor = this.conn.getActor(actorID);
        const { _highlighter: h } = actor;
        h.once("updated", resolve);
      } else {
        resolve();
      }

      const bc = this.content.docShell.browsingContext;
      bc.fullZoom = level;
    });
  },

  /**
   * Get all box-model regions' adjusted boxquads for the given element
   * @param {String} selector The node selector to target a given element
   * @return {Object} An object with each property being a box-model region, each
   * of them being an object with the p1/p2/p3/p4 properties
   */
  getAllAdjustedQuads: function(selector) {
    const regions = {};
    const node = this._querySelector(selector);
    for (const boxType of ["content", "padding", "border", "margin"]) {
      regions[boxType] = getAdjustedQuads(this.content, node, boxType);
    }

    return regions;
  },

  /**
   * Get the window which mouse events on node should be delivered to.
   */
  windowForMouseEvent: function(node) {
    return node.ownerDocument.defaultView;
  },

  /**
   * Synthesize a mouse event on an element, after ensuring that it is visible
   * in the viewport. This handler doesn't send a message back. Consumers
   * should listen to specific events on the inspector/highlighter to know when
   * the event got synthesized.
   * @param {String} selector The node selector to get the node target for the event
   * @param {Number} x
   * @param {Number} y
   * @param {Boolean} center If set to true, x/y will be ignored and
   *                  synthesizeMouseAtCenter will be used instead
   * @param {Object} options Other event options
   */
  synthesizeMouse: function({ selector, x, y, center, options }) {
    const node = this._querySelector(selector);
    node.scrollIntoView();
    if (center) {
      EventUtils.synthesizeMouseAtCenter(
        node,
        options,
        this.windowForMouseEvent(node)
      );
    } else {
      EventUtils.synthesizeMouse(
        node,
        x,
        y,
        options,
        this.windowForMouseEvent(node)
      );
    }
  },

  /**
   * Synthesize a key event for an element. This handler doesn't send a message
   * back. Consumers should listen to specific events on the inspector/highlighter
   * to know when the event got synthesized.
   */
  synthesizeKey: function({ key, options, content }) {
    EventUtils.synthesizeKey(key, options, this.content);
  },

  /**
   * Scroll an element into view.
   * @param {String} selector The selector for the node to scroll into view.
   */
  scrollIntoView: function(selector) {
    const node = this._querySelector(selector);
    node.scrollIntoView();
  },

  /**
   * Check that an element currently has a pseudo-class lock.
   * @param {String} selector The node selector to get the pseudo-class from
   * @param {String} pseudo The pseudoclass to check for
   * @return {Boolean}
   */
  hasPseudoClassLock: function(selector, pseudo) {
    const node = this._querySelector(selector);
    return InspectorUtils.hasPseudoClassLock(node, pseudo);
  },

  loadAndWaitForCustomEvent: function(url) {
    return new Promise(resolve => {
      // Wait for DOMWindowCreated first, as listening on the current outerwindow
      // doesn't allow receiving test-page-processing-done.
      this.targetActor.chromeEventHandler.addEventListener(
        "DOMWindowCreated",
        () => {
          this.content.addEventListener("test-page-processing-done", resolve, {
            once: true,
          });
        },
        { once: true }
      );

      this.content.location = url;
    });
  },

  hasNode: function(selector) {
    try {
      // _querySelector throws if the node doesn't exists
      this._querySelector(selector);
      return true;
    } catch (e) {
      return false;
    }
  },

  /**
   * Get the bounding rect for a given DOM node once.
   * @param {String} selector selector identifier to select the DOM node
   * @return {json} the bounding rect info
   */
  getBoundingClientRect: function(selector) {
    const node = this._querySelector(selector);
    const rect = node.getBoundingClientRect();
    // DOMRect can't be stringified directly, so return a simple object instead.
    return {
      x: rect.x,
      y: rect.y,
      width: rect.width,
      height: rect.height,
      top: rect.top,
      right: rect.right,
      bottom: rect.bottom,
      left: rect.left,
    };
  },

  /**
   * Set a JS property on a DOM Node.
   * @param {String} selector The node selector
   * @param {String} property The property name
   * @param {String} value The attribute value
   */
  setProperty: function(selector, property, value) {
    const node = this._querySelector(selector);
    node[property] = value;
  },

  /**
   * Get a JS property on a DOM Node.
   * @param {String} selector The node selector
   * @param {String} property The property name
   * @return {String} value The attribute value
   */
  getProperty: function(selector, property) {
    const node = this._querySelector(selector);
    return node[property];
  },

  /**
   * Reload the content window.
   */
  reload: function() {
    this.content.location.reload();
  },

  /**
   * Reload an iframe and wait for its load event.
   * @param {String} selector The node selector
   */
  reloadFrame: function(selector) {
    return new Promise(resolve => {
      const node = this._querySelector(selector);

      const onLoad = function() {
        node.removeEventListener("load", onLoad);
        resolve();
      };
      node.addEventListener("load", onLoad);

      node.contentWindow.location.reload();
    });
  },

  /**
   * Evaluate a JS string in the context of the content document.
   * @param {String} js JS string to evaluate
   * @return {json} The evaluation result
   */
  eval: function(js) {
    // We have to use a sandbox, as CSP prevent us from using eval on apps...
    const sb = Cu.Sandbox(this.content, { sandboxPrototype: this.content });
    const result = Cu.evalInSandbox(js, sb);

    // Ensure passing only serializable data to RDP
    if (typeof result == "function") {
      return null;
    } else if (typeof result == "object") {
      return JSON.parse(JSON.stringify(result));
    }
    return result;
  },

  /**
   * Scrolls the window to a particular set of coordinates in the document, or
   * by the given amount if `relative` is set to `true`.
   *
   * @param {Number} x
   * @param {Number} y
   * @param {Boolean} relative
   *
   * @return {Object} An object with x / y properties, representing the number
   * of pixels that the document has been scrolled horizontally and vertically.
   */
  scrollWindow: function(x, y, relative) {
    if (isNaN(x) || isNaN(y)) {
      return {};
    }

    return new Promise(resolve => {
      this.content.addEventListener(
        "scroll",
        function(event) {
          const data = { x: this.content.scrollX, y: this.content.scrollY };
          resolve(data);
        },
        { once: true }
      );

      this.content[relative ? "scrollBy" : "scrollTo"](x, y);
    });
  },

  /**
   * Forces the reflow and waits for the next repaint.
   */
  reflow: function() {
    return new Promise(resolve => {
      this.content.document.documentElement.offsetWidth;
      this.content.requestAnimationFrame(resolve);
    });
  },

  async getNodeRect(selector) {
    const node = this._querySelector(selector);
    return getRect(this.content, node, this.content);
  },

  async getTextNodeRect(parentSelector, childNodeIndex) {
    const parentNode = this._querySelector(parentSelector);
    const node = parentNode.childNodes[childNodeIndex];
    return getAdjustedQuads(this.content, node)[0].bounds;
  },

  /**
   * Get information about a DOM element, identified by a selector.
   * @param {String} selector The CSS selector to get the node (can be an array
   * of selectors to get elements in an iframe).
   * @return {Object} data Null if selector didn't match any node, otherwise:
   * - {String} tagName.
   * - {String} namespaceURI.
   * - {Number} numChildren The number of children in the element.
   * - {Array} attributes An array of {name, value, namespaceURI} objects.
   * - {String} outerHTML.
   * - {String} innerHTML.
   * - {String} textContent.
   */
  getNodeInfo: function(selector) {
    const node = this._querySelector(selector);
    let info = null;

    if (node) {
      info = {
        tagName: node.tagName,
        namespaceURI: node.namespaceURI,
        numChildren: node.children.length,
        numNodes: node.childNodes.length,
        attributes: [...node.attributes].map(
          ({ name, value, namespaceURI }) => {
            return { name, value, namespaceURI };
          }
        ),
        outerHTML: node.outerHTML,
        innerHTML: node.innerHTML,
        textContent: node.textContent,
      };
    }

    return info;
  },

  /**
   * Get information about the stylesheets which have CSS rules that apply to a given DOM
   * element, identified by a selector.
   * @param {String} selector The CSS selector to get the node (can be an array
   * of selectors to get elements in an iframe).
   * @return {Array} A list of stylesheet objects, each having the following properties:
   * - {String} href.
   * - {Boolean} isContentSheet.
   */
  getStyleSheetsInfoForNode: function(selector) {
    const node = this._querySelector(selector);
    const domRules = getCSSStyleRules(node);

    const sheets = [];

    for (let i = 0, n = domRules.length; i < n; i++) {
      const sheet = domRules[i].parentStyleSheet;
      sheets.push({
        href: sheet.href,
        isContentSheet: !isAgentStylesheet(sheet),
      });
    }

    return sheets;
  },

  /**
   * Returns the window's dimensions for the `window` given.
   *
   * @return {Object} An object with `width` and `height` properties, representing the
   * number of pixels for the document's size.
   */
  getWindowDimensions: function() {
    return getWindowDimensions(this.content);
  },

  /**
   * @returns {PausedDebuggerOverlay} The paused overlay instance
   */
  _getPausedDebuggerOverlay() {
    // We use `_pauseOverlay` since it's the cached value; `pauseOverlay` is a getter that
    // will create the overlay when called (if it does not exist yet).
    return this.targetActor?.threadActor?._pauseOverlay;
  },

  isPausedDebuggerOverlayVisible() {
    const pauseOverlay = this._getPausedDebuggerOverlay();
    if (!pauseOverlay) {
      return false;
    }

    const root = pauseOverlay.getElement("root");
    return root.getAttribute("hidden") !== "true";
  },

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
  },

  /**
   * @returns {EyeDropper}
   */
  _getEyeDropper(inspectorActorID) {
    const inspectorActor = this.conn.getActor(inspectorActorID);
    return inspectorActor?._eyeDropper;
  },

  isEyeDropperVisible(inspectorActorID) {
    const eyeDropper = this._getEyeDropper(inspectorActorID);
    if (!eyeDropper) {
      return false;
    }

    return eyeDropper.getElement("root").getAttribute("hidden") !== "true";
  },

  getEyeDropperElementAttribute(inspectorActorID, elementId, attributeName) {
    const eyeDropper = this._getEyeDropper(inspectorActorID);
    if (!eyeDropper) {
      return null;
    }

    return eyeDropper.getElement(elementId).getAttribute(attributeName);
  },

  async getEyeDropperColorValue(inspectorActorID) {
    const eyeDropper = this._getEyeDropper(inspectorActorID);
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
  },
});
exports.TestActor = TestActor;

class TestFront extends protocol.FrontClassWithSpec(testSpec) {
  constructor(client, targetFront, parentFront) {
    super(client, targetFront, parentFront);
    this.formAttributeName = "testActor";
    // The currently active highlighter is obtained by calling a custom getter
    // provided manually after requesting TestFront. See `getTestActor(toolbox)`
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

  /**
   * Zoom the current page to a given level.
   * @param {Number} level The new zoom level.
   * @param {String} actorID Optional. The highlighter actor ID.
   * @return {Promise} The returned promise will only resolve when the
   * highlighter has updated to the new zoom level.
   */
  zoomPageTo(level, actorID = this.highlighter.actorID) {
    return this.changeZoomLevel(level, actorID);
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
   * Is the highlighter currently visible on the page?
   */
  isHighlighting() {
    // Once the highlighter is hidden, the reference to it is lost.
    // Assume it is not highlighting.
    if (!this.highlighter) {
      return false;
    }

    return this.getHighlighterNodeAttribute(
      "box-model-elements",
      "hidden"
    ).then(value => value === null);
  }

  /**
   * Assert that the box-model highlighter's current position corresponds to the
   * given node boxquads.
   * @param {String} selector The node selector to get the boxQuads from
   * @param {Function} is assertion function to call for equality checks
   * @param {String} prefix An optional prefix for logging information to the
   * console.
   */
  async isNodeCorrectlyHighlighted(selector, is, prefix = "") {
    prefix += (prefix ? " " : "") + selector + " ";

    const boxModel = await this._getBoxModelStatus();
    const regions = await this.getAllAdjustedQuads(selector);

    for (const boxType of ["content", "padding", "border", "margin"]) {
      const [quad] = regions[boxType];
      for (const point in boxModel[boxType].points) {
        is(
          boxModel[boxType].points[point].x,
          quad[point].x,
          prefix + boxType + " point " + point + " x coordinate is correct"
        );
        is(
          boxModel[boxType].points[point].y,
          quad[point].y,
          prefix + boxType + " point " + point + " y coordinate is correct"
        );
      }
    }
  }

  /**
   * Get the current rect of the border region of the box-model highlighter
   */
  async getSimpleBorderRect() {
    const { border } = await this._getBoxModelStatus();
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
  async _getBoxModelStatus() {
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
    const { visible, border } = await this._getBoxModelStatus();
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
      x1: x1,
      y1: y1,
      x2: x2,
      y2: y2,
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

  waitForHighlighterEvent(event) {
    return super.waitForHighlighterEvent(event, this.highlighter.actorID);
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
protocol.registerFront(TestFront);
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
