/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

/* exported TestActor, TestActorFront */

"use strict";

// A helper actor for inspector and markupview tests.

const { Cc, Ci, Cu } = require("chrome");
const {
  getRect, getElementFromPoint, getAdjustedQuads, getWindowDimensions
} = require("devtools/shared/layout/utils");
const defer = require("devtools/shared/defer");
const {Task} = require("devtools/shared/task");
const {isContentStylesheet} = require("devtools/shared/inspector/css-logic");
const DOMUtils = Cc["@mozilla.org/inspector/dom-utils;1"].getService(Ci.inIDOMUtils);
const loader = Cc["@mozilla.org/moz/jssubscript-loader;1"]
                 .getService(Ci.mozIJSSubScriptLoader);

// Set up a dummy environment so that EventUtils works. We need to be careful to
// pass a window object into each EventUtils method we call rather than having
// it rely on the |window| global.
let EventUtils = {};
EventUtils.window = {};
EventUtils.parent = {};
/* eslint-disable camelcase */
EventUtils._EU_Ci = Components.interfaces;
EventUtils._EU_Cc = Components.classes;
/* eslint-disable camelcase */
loader.loadSubScript("chrome://mochikit/content/tests/SimpleTest/EventUtils.js", EventUtils);

const protocol = require("devtools/shared/protocol");
const {Arg, RetVal} = protocol;

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
  let actor = conn.getActor(actorID);
  if (actor && actor._highlighter) {
    return actor._highlighter.markup;
  }
  return null;
}

var testSpec = protocol.generateActorSpec({
  typeName: "testActor",

  methods: {
    getNumberOfElementMatches: {
      request: {
        selector: Arg(0, "string"),
      },
      response: {
        value: RetVal("number")
      }
    },
    getHighlighterAttribute: {
      request: {
        nodeID: Arg(0, "string"),
        name: Arg(1, "string"),
        actorID: Arg(2, "string")
      },
      response: {
        value: RetVal("string")
      }
    },
    getHighlighterNodeTextContent: {
      request: {
        nodeID: Arg(0, "string"),
        actorID: Arg(1, "string")
      },
      response: {
        value: RetVal("string")
      }
    },
    getSelectorHighlighterBoxNb: {
      request: {
        highlighter: Arg(0, "string"),
      },
      response: {
        value: RetVal("number")
      }
    },
    changeHighlightedNodeWaitForUpdate: {
      request: {
        name: Arg(0, "string"),
        value: Arg(1, "string"),
        actorID: Arg(2, "string")
      },
      response: {}
    },
    waitForHighlighterEvent: {
      request: {
        event: Arg(0, "string"),
        actorID: Arg(1, "string")
      },
      response: {}
    },
    waitForEventOnNode: {
      request: {
        eventName: Arg(0, "string"),
        selector: Arg(1, "nullable:string")
      },
      response: {}
    },
    changeZoomLevel: {
      request: {
        level: Arg(0, "string"),
        actorID: Arg(1, "string"),
      },
      response: {}
    },
    assertElementAtPoint: {
      request: {
        x: Arg(0, "number"),
        y: Arg(1, "number"),
        selector: Arg(2, "string")
      },
      response: {
        value: RetVal("boolean")
      }
    },
    getAllAdjustedQuads: {
      request: {
        selector: Arg(0, "string")
      },
      response: {
        value: RetVal("json")
      }
    },
    synthesizeMouse: {
      request: {
        object: Arg(0, "json")
      },
      response: {}
    },
    synthesizeKey: {
      request: {
        args: Arg(0, "json")
      },
      response: {}
    },
    scrollIntoView: {
      request: {
        args: Arg(0, "string")
      },
      response: {}
    },
    hasPseudoClassLock: {
      request: {
        selector: Arg(0, "string"),
        pseudo: Arg(1, "string")
      },
      response: {
        value: RetVal("boolean")
      }
    },
    loadAndWaitForCustomEvent: {
      request: {
        url: Arg(0, "string")
      },
      response: {}
    },
    hasNode: {
      request: {
        selector: Arg(0, "string")
      },
      response: {
        value: RetVal("boolean")
      }
    },
    getBoundingClientRect: {
      request: {
        selector: Arg(0, "string"),
      },
      response: {
        value: RetVal("json")
      }
    },
    setProperty: {
      request: {
        selector: Arg(0, "string"),
        property: Arg(1, "string"),
        value: Arg(2, "string")
      },
      response: {}
    },
    getProperty: {
      request: {
        selector: Arg(0, "string"),
        property: Arg(1, "string")
      },
      response: {
        value: RetVal("string")
      }
    },
    getAttribute: {
      request: {
        selector: Arg(0, "string"),
        property: Arg(1, "string")
      },
      response: {
        value: RetVal("string")
      }
    },
    setAttribute: {
      request: {
        selector: Arg(0, "string"),
        property: Arg(1, "string"),
        value: Arg(2, "string")
      },
      response: {}
    },
    removeAttribute: {
      request: {
        selector: Arg(0, "string"),
        property: Arg(1, "string")
      },
      response: {}
    },
    reload: {
      request: {},
      response: {}
    },
    reloadFrame: {
      request: {
        selector: Arg(0, "string"),
      },
      response: {}
    },
    eval: {
      request: {
        js: Arg(0, "string")
      },
      response: {
        value: RetVal("nullable:json")
      }
    },
    scrollWindow: {
      request: {
        x: Arg(0, "number"),
        y: Arg(1, "number"),
        relative: Arg(2, "nullable:boolean"),
      },
      response: {
        value: RetVal("json")
      }
    },
    reflow: {},
    getNodeRect: {
      request: {
        selector: Arg(0, "string")
      },
      response: {
        value: RetVal("json")
      }
    },
    getTextNodeRect: {
      request: {
        parentSelector: Arg(0, "string"),
        childNodeIndex: Arg(1, "number")
      },
      response: {
        value: RetVal("json")
      }
    },
    getNodeInfo: {
      request: {
        selector: Arg(0, "string")
      },
      response: {
        value: RetVal("json")
      }
    },
    getStyleSheetsInfoForNode: {
      request: {
        selector: Arg(0, "string")
      },
      response: {
        value: RetVal("json")
      }
    },
    getWindowDimensions: {
      request: {},
      response: {
        value: RetVal("json")
      }
    }
  }
});

var TestActor = exports.TestActor = protocol.ActorClassWithSpec(testSpec, {
  initialize: function (conn, tabActor, options) {
    this.conn = conn;
    this.tabActor = tabActor;
  },

  get content() {
    return this.tabActor.window;
  },

  /**
   * Helper to retrieve a DOM element.
   * @param {string | array} selector Either a regular selector string
   *   or a selector array. If an array, each item, except the last one
   *   are considered matching an iframe, so that we can query element
   *   within deep iframes.
   */
  _querySelector: function (selector) {
    let document = this.content.document;
    if (Array.isArray(selector)) {
      let fullSelector = selector.join(" >> ");
      while (selector.length > 1) {
        let str = selector.shift();
        let iframe = document.querySelector(str);
        if (!iframe) {
          throw new Error("Unable to find element with selector \"" + str + "\"" +
                          " (full selector:" + fullSelector + ")");
        }
        if (!iframe.contentWindow) {
          throw new Error("Iframe selector doesn't target an iframe \"" + str + "\"" +
                          " (full selector:" + fullSelector + ")");
        }
        document = iframe.contentWindow.document;
      }
      selector = selector.shift();
    }
    let node = document.querySelector(selector);
    if (!node) {
      throw new Error("Unable to find element with selector \"" + selector + "\"");
    }
    return node;
  },
  /**
   * Helper to get the number of elements matching a selector
   * @param {string} CSS selector.
   */
  getNumberOfElementMatches: function (selector, root = this.content.document) {
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
  getHighlighterAttribute: function (nodeID, name, actorID) {
    let helper = getHighlighterCanvasFrameHelper(this.conn, actorID);
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
  getHighlighterNodeTextContent: function (nodeID, actorID) {
    let value;
    let helper = getHighlighterCanvasFrameHelper(this.conn, actorID);
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
  getSelectorHighlighterBoxNb: function (actorID) {
    let highlighter = this.conn.getActor(actorID);
    let {_highlighter: h} = highlighter;
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
  changeHighlightedNodeWaitForUpdate: function (name, value, actorID) {
    return new Promise(resolve => {
      let highlighter = this.conn.getActor(actorID);
      let {_highlighter: h} = highlighter;

      h.once("updated", resolve);

      h.currentNode.setAttribute(name, value);
    });
  },

  /**
   * Subscribe to a given highlighter event and respond when the event is received.
   * @param {String} event The name of the highlighter event to listen to
   * @param {String} actorID The highlighter actor ID
   */
  waitForHighlighterEvent: function (event, actorID) {
    let highlighter = this.conn.getActor(actorID);
    let {_highlighter: h} = highlighter;

    return h.once(event);
  },

  /**
   * Wait for a specific event on a node matching the provided selector.
   * @param {String} eventName The name of the event to listen to
   * @param {String} selector Optional:  css selector of the node which should
   *        trigger the event. If ommitted, target will be the content window
   */
  waitForEventOnNode: function (eventName, selector) {
    return new Promise(resolve => {
      let node = selector ? this._querySelector(selector) : this.content;
      node.addEventListener(eventName, function () {
        resolve();
      }, {once: true});
    });
  },

  /**
   * Change the zoom level of the page.
   * Optionally subscribe to the box-model highlighter's update event and waiting
   * for it to refresh before responding.
   * @param {Number} level The new zoom level
   * @param {String} actorID Optional. The highlighter actor ID
   */
  changeZoomLevel: function (level, actorID) {
    dumpn("Zooming page to " + level);
    return new Promise(resolve => {
      if (actorID) {
        let actor = this.conn.getActor(actorID);
        let {_highlighter: h} = actor;
        h.once("updated", resolve);
      } else {
        resolve();
      }

      let docShell = this.content.QueryInterface(Ci.nsIInterfaceRequestor)
                                 .getInterface(Ci.nsIWebNavigation)
                                 .QueryInterface(Ci.nsIDocShell);
      docShell.contentViewer.fullZoom = level;
    });
  },

  assertElementAtPoint: function (x, y, selector) {
    let elementAtPoint = getElementFromPoint(this.content.document, x, y);
    if (!elementAtPoint) {
      throw new Error("Unable to find element at (" + x + ", " + y + ")");
    }
    let node = this._querySelector(selector);
    return node == elementAtPoint;
  },

  /**
   * Get all box-model regions' adjusted boxquads for the given element
   * @param {String} selector The node selector to target a given element
   * @return {Object} An object with each property being a box-model region, each
   * of them being an object with the p1/p2/p3/p4 properties
   */
  getAllAdjustedQuads: function (selector) {
    let regions = {};
    let node = this._querySelector(selector);
    for (let boxType of ["content", "padding", "border", "margin"]) {
      regions[boxType] = getAdjustedQuads(this.content, node, boxType);
    }

    return regions;
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
  synthesizeMouse: function ({ selector, x, y, center, options }) {
    let node = this._querySelector(selector);
    node.scrollIntoView();
    if (center) {
      EventUtils.synthesizeMouseAtCenter(node, options, node.ownerDocument.defaultView);
    } else {
      EventUtils.synthesizeMouse(node, x, y, options, node.ownerDocument.defaultView);
    }
  },

  /**
  * Synthesize a key event for an element. This handler doesn't send a message
  * back. Consumers should listen to specific events on the inspector/highlighter
  * to know when the event got synthesized.
  */
  synthesizeKey: function ({key, options, content}) {
    EventUtils.synthesizeKey(key, options, this.content);
  },

  /**
   * Scroll an element into view.
   * @param {String} selector The selector for the node to scroll into view.
   */
  scrollIntoView: function (selector) {
    let node = this._querySelector(selector);
    node.scrollIntoView();
  },

  /**
   * Check that an element currently has a pseudo-class lock.
   * @param {String} selector The node selector to get the pseudo-class from
   * @param {String} pseudo The pseudoclass to check for
   * @return {Boolean}
   */
  hasPseudoClassLock: function (selector, pseudo) {
    let node = this._querySelector(selector);
    return DOMUtils.hasPseudoClassLock(node, pseudo);
  },

  loadAndWaitForCustomEvent: function (url) {
    return new Promise(resolve => {
      // Wait for DOMWindowCreated first, as listening on the current outerwindow
      // doesn't allow receiving test-page-processing-done.
      this.tabActor.chromeEventHandler.addEventListener("DOMWindowCreated", () => {
        this.content.addEventListener(
          "test-page-processing-done", resolve, { once: true }
        );
      }, { once: true });

      this.content.location = url;
    });
  },

  hasNode: function (selector) {
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
  getBoundingClientRect: function (selector) {
    let node = this._querySelector(selector);
    let rect = node.getBoundingClientRect();
    // DOMRect can't be stringified directly, so return a simple object instead.
    return {
      x: rect.x,
      y: rect.y,
      width: rect.width,
      height: rect.height,
      top: rect.top,
      right: rect.right,
      bottom: rect.bottom,
      left: rect.left
    };
  },

  /**
   * Set a JS property on a DOM Node.
   * @param {String} selector The node selector
   * @param {String} property The property name
   * @param {String} value The attribute value
   */
  setProperty: function (selector, property, value) {
    let node = this._querySelector(selector);
    node[property] = value;
  },

  /**
   * Get a JS property on a DOM Node.
   * @param {String} selector The node selector
   * @param {String} property The property name
   * @return {String} value The attribute value
   */
  getProperty: function (selector, property) {
    let node = this._querySelector(selector);
    return node[property];
  },

  /**
   * Get an attribute on a DOM Node.
   * @param {String} selector The node selector
   * @param {String} attribute The attribute name
   * @return {String} value The attribute value
   */
  getAttribute: function (selector, attribute) {
    let node = this._querySelector(selector);
    return node.getAttribute(attribute);
  },

  /**
   * Set an attribute on a DOM Node.
   * @param {String} selector The node selector
   * @param {String} attribute The attribute name
   * @param {String} value The attribute value
   */
  setAttribute: function (selector, attribute, value) {
    let node = this._querySelector(selector);
    node.setAttribute(attribute, value);
  },

  /**
   * Remove an attribute from a DOM Node.
   * @param {String} selector The node selector
   * @param {String} attribute The attribute name
   */
  removeAttribute: function (selector, attribute) {
    let node = this._querySelector(selector);
    node.removeAttribute(attribute);
  },

  /**
   * Reload the content window.
   */
  reload: function () {
    this.content.location.reload();
  },

  /**
   * Reload an iframe and wait for its load event.
   * @param {String} selector The node selector
   */
  reloadFrame: function (selector) {
    let node = this._querySelector(selector);

    let deferred = defer();

    let onLoad = function () {
      node.removeEventListener("load", onLoad);
      deferred.resolve();
    };
    node.addEventListener("load", onLoad);

    node.contentWindow.location.reload();
    return deferred.promise;
  },

  /**
   * Evaluate a JS string in the context of the content document.
   * @param {String} js JS string to evaluate
   * @return {json} The evaluation result
   */
  eval: function (js) {
    // We have to use a sandbox, as CSP prevent us from using eval on apps...
    let sb = Cu.Sandbox(this.content, { sandboxPrototype: this.content });
    return Cu.evalInSandbox(js, sb);
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
  scrollWindow: function (x, y, relative) {
    if (isNaN(x) || isNaN(y)) {
      return {};
    }

    let deferred = defer();
    this.content.addEventListener("scroll", function (event) {
      let data = {x: this.content.scrollX, y: this.content.scrollY};
      deferred.resolve(data);
    }, {once: true});

    this.content[relative ? "scrollBy" : "scrollTo"](x, y);

    return deferred.promise;
  },

  /**
   * Forces the reflow and waits for the next repaint.
   */
  reflow: function () {
    let deferred = defer();
    this.content.document.documentElement.offsetWidth;
    this.content.requestAnimationFrame(deferred.resolve);

    return deferred.promise;
  },

  getNodeRect: Task.async(function* (selector) {
    let node = this._querySelector(selector);
    return getRect(this.content, node, this.content);
  }),

  getTextNodeRect: Task.async(function* (parentSelector, childNodeIndex) {
    let parentNode = this._querySelector(parentSelector);
    let node = parentNode.childNodes[childNodeIndex];
    return getAdjustedQuads(this.content, node)[0].bounds;
  }),

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
  getNodeInfo: function (selector) {
    let node = this._querySelector(selector);
    let info = null;

    if (node) {
      info = {
        tagName: node.tagName,
        namespaceURI: node.namespaceURI,
        numChildren: node.children.length,
        numNodes: node.childNodes.length,
        attributes: [...node.attributes].map(({name, value, namespaceURI}) => {
          return {name, value, namespaceURI};
        }),
        outerHTML: node.outerHTML,
        innerHTML: node.innerHTML,
        textContent: node.textContent
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
  getStyleSheetsInfoForNode: function (selector) {
    let node = this._querySelector(selector);
    let domRules = DOMUtils.getCSSStyleRules(node);

    let sheets = [];

    for (let i = 0, n = domRules.Count(); i < n; i++) {
      let sheet = domRules.GetElementAt(i).parentStyleSheet;
      sheets.push({
        href: sheet.href,
        isContentSheet: isContentStylesheet(sheet)
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
  getWindowDimensions: function () {
    return getWindowDimensions(this.content);
  }
});

var TestActorFront = exports.TestActorFront = protocol.FrontClassWithSpec(testSpec, {
  initialize: function (client, { testActor }, toolbox) {
    protocol.Front.prototype.initialize.call(this, client, { actor: testActor });
    this.manage(this);
    this.toolbox = toolbox;
  },

  /**
   * Zoom the current page to a given level.
   * @param {Number} level The new zoom level.
   * @param {String} actorID Optional. The highlighter actor ID.
   * @return {Promise} The returned promise will only resolve when the
   * highlighter has updated to the new zoom level.
   */
  zoomPageTo: function (level, actorID = this.toolbox.highlighter.actorID) {
    return this.changeZoomLevel(level, actorID);
  },

  /* eslint-disable max-len */
  changeHighlightedNodeWaitForUpdate: protocol.custom(function (name, value, highlighter) {
    /* eslint-enable max-len */
    return this._changeHighlightedNodeWaitForUpdate(
      name, value, (highlighter || this.toolbox.highlighter).actorID
    );
  }, {
    impl: "_changeHighlightedNodeWaitForUpdate"
  }),

  /**
   * Get the value of an attribute on one of the highlighter's node.
   * @param {String} nodeID The Id of the node in the highlighter.
   * @param {String} name The name of the attribute.
   * @param {Object} highlighter Optional custom highlither to target
   * @return {String} value
   */
  getHighlighterNodeAttribute: function (nodeID, name, highlighter) {
    return this.getHighlighterAttribute(
      nodeID, name, (highlighter || this.toolbox.highlighter).actorID
    );
  },

  getHighlighterNodeTextContent: protocol.custom(function (nodeID, highlighter) {
    return this._getHighlighterNodeTextContent(
      nodeID, (highlighter || this.toolbox.highlighter).actorID
    );
  }, {
    impl: "_getHighlighterNodeTextContent"
  }),

  /**
   * Is the highlighter currently visible on the page?
   */
  isHighlighting: function () {
    return this.getHighlighterNodeAttribute("box-model-elements", "hidden")
      .then(value => value === null);
  },

  /**
   * Assert that the box-model highlighter's current position corresponds to the
   * given node boxquads.
   * @param {String} selector The node selector to get the boxQuads from
   * @param {Function} is assertion function to call for equality checks
   * @param {String} prefix An optional prefix for logging information to the
   * console.
   */
  isNodeCorrectlyHighlighted: Task.async(function* (selector, is, prefix = "") {
    prefix += (prefix ? " " : "") + selector + " ";

    let boxModel = yield this._getBoxModelStatus();
    let regions = yield this.getAllAdjustedQuads(selector);

    for (let boxType of ["content", "padding", "border", "margin"]) {
      let [quad] = regions[boxType];
      for (let point in boxModel[boxType].points) {
        is(boxModel[boxType].points[point].x, quad[point].x,
          prefix + boxType + " point " + point + " x coordinate is correct");
        is(boxModel[boxType].points[point].y, quad[point].y,
          prefix + boxType + " point " + point + " y coordinate is correct");
      }
    }
  }),

  /**
   * Get the current rect of the border region of the box-model highlighter
   */
  getSimpleBorderRect: Task.async(function* (toolbox) {
    let {border} = yield this._getBoxModelStatus(toolbox);
    let {p1, p2, p4} = border.points;

    return {
      top: p1.y,
      left: p1.x,
      width: p2.x - p1.x,
      height: p4.y - p1.y
    };
  }),

  /**
   * Get the current positions and visibility of the various box-model highlighter
   * elements.
   */
  _getBoxModelStatus: Task.async(function* () {
    let isVisible = yield this.isHighlighting();

    let ret = {
      visible: isVisible
    };

    for (let region of ["margin", "border", "padding", "content"]) {
      let points = yield this._getPointsForRegion(region);
      let visible = yield this._isRegionHidden(region);
      ret[region] = {points, visible};
    }

    ret.guides = {};
    for (let guide of ["top", "right", "bottom", "left"]) {
      ret.guides[guide] = yield this._getGuideStatus(guide);
    }

    return ret;
  }),

  /**
   * Check that the box-model highlighter is currently highlighting the node matching the
   * given selector.
   * @param {String} selector
   * @return {Boolean}
   */
  assertHighlightedNode: Task.async(function* (selector) {
    let rect = yield this.getNodeRect(selector);
    return yield this.isNodeRectHighlighted(rect);
  }),

  /**
   * Check that the box-model highlighter is currently highlighting the text node that can
   * be found at a given index within the list of childNodes of a parent element matching
   * the given selector.
   * @param {String} parentSelector
   * @param {Number} childNodeIndex
   * @return {Boolean}
   */
  assertHighlightedTextNode: Task.async(function* (parentSelector, childNodeIndex) {
    let rect = yield this.getTextNodeRect(parentSelector, childNodeIndex);
    return yield this.isNodeRectHighlighted(rect);
  }),

  /**
   * Check that the box-model highlighter is currently highlighting the given rect.
   * @param {Object} rect
   * @return {Boolean}
   */
  isNodeRectHighlighted: Task.async(function* ({ left, top, width, height }) {
    let {visible, border} = yield this._getBoxModelStatus();
    let points = border.points;
    if (!visible) {
      return false;
    }

    // Check that the node is within the box model
    let right = left + width;
    let bottom = top + height;

    // Converts points dictionnary into an array
    let list = [];
    for (let i = 1; i <= 4; i++) {
      let p = points["p" + i];
      list.push([p.x, p.y]);
    }
    points = list;

    // Check that each point of the node is within the box model
    return isInside([left, top], points) &&
           isInside([right, top], points) &&
           isInside([right, bottom], points) &&
           isInside([left, bottom], points);
  }),

  /**
   * Get the coordinate (points attribute) from one of the polygon elements in the
   * box model highlighter.
   */
  _getPointsForRegion: Task.async(function* (region) {
    let d = yield this.getHighlighterNodeAttribute("box-model-" + region, "d");

    let polygons = d.match(/M[^M]+/g);
    if (!polygons) {
      return null;
    }

    let points = polygons[0].trim().split(" ").map(i => {
      return i.replace(/M|L/, "").split(",");
    });

    return {
      p1: {
        x: parseFloat(points[0][0]),
        y: parseFloat(points[0][1])
      },
      p2: {
        x: parseFloat(points[1][0]),
        y: parseFloat(points[1][1])
      },
      p3: {
        x: parseFloat(points[2][0]),
        y: parseFloat(points[2][1])
      },
      p4: {
        x: parseFloat(points[3][0]),
        y: parseFloat(points[3][1])
      }
    };
  }),

  /**
   * Is a given region polygon element of the box-model highlighter currently
   * hidden?
   */
  _isRegionHidden: Task.async(function* (region) {
    let value = yield this.getHighlighterNodeAttribute("box-model-" + region, "hidden");
    return value !== null;
  }),

  _getGuideStatus: Task.async(function* (location) {
    let id = "box-model-guide-" + location;

    let hidden = yield this.getHighlighterNodeAttribute(id, "hidden");
    let x1 = yield this.getHighlighterNodeAttribute(id, "x1");
    let y1 = yield this.getHighlighterNodeAttribute(id, "y1");
    let x2 = yield this.getHighlighterNodeAttribute(id, "x2");
    let y2 = yield this.getHighlighterNodeAttribute(id, "y2");

    return {
      visible: !hidden,
      x1: x1,
      y1: y1,
      x2: x2,
      y2: y2
    };
  }),

  /**
   * Get the coordinates of the rectangle that is defined by the 4 guides displayed
   * in the toolbox box-model highlighter.
   * @return {Object} Null if at least one guide is hidden. Otherwise an object
   * with p1, p2, p3, p4 properties being {x, y} objects.
   */
  getGuidesRectangle: Task.async(function* () {
    let tGuide = yield this._getGuideStatus("top");
    let rGuide = yield this._getGuideStatus("right");
    let bGuide = yield this._getGuideStatus("bottom");
    let lGuide = yield this._getGuideStatus("left");

    if (!tGuide.visible || !rGuide.visible || !bGuide.visible || !lGuide.visible) {
      return null;
    }

    return {
      p1: {x: lGuide.x1, y: tGuide.y1},
      p2: {x: +rGuide.x1 + 1, y: tGuide.y1},
      p3: {x: +rGuide.x1 + 1, y: +bGuide.y1 + 1},
      p4: {x: lGuide.x1, y: +bGuide.y1 + 1}
    };
  }),

  waitForHighlighterEvent: protocol.custom(function (event) {
    return this._waitForHighlighterEvent(event, this.toolbox.highlighter.actorID);
  }, {
    impl: "_waitForHighlighterEvent"
  }),

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
  getHighlighterRegionPath: Task.async(function* (region, highlighter) {
    let d = yield this.getHighlighterNodeAttribute(
      `box-model-${region}`, "d", highlighter
    );
    if (!d) {
      return {d: null};
    }

    let polygons = d.match(/M[^M]+/g);
    if (!polygons) {
      return {d};
    }

    let points = [];
    for (let polygon of polygons) {
      points.push(polygon.trim().split(" ").map(i => {
        return i.replace(/M|L/, "").split(",");
      }));
    }

    return {d, points};
  })
});

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

  const n = polygon.length;
  const newPoints = polygon.slice(0);
  newPoints.push(polygon[0]);
  let wn = 0;

  // loop through all edges of the polygon
  for (let i = 0; i < n; i++) {
    // Accept points on the edges
    let r = isLeft(newPoints[i], newPoints[i + 1], point);
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
  let l = ((p1[0] - p0[0]) * (p2[1] - p0[1])) -
          ((p2[0] - p0[0]) * (p1[1] - p0[1]));
  return l;
}
