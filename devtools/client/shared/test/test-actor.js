/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// A helper actor for brower/devtools/inspector tests.

let { Cc, Ci, Cu, Cr } = require("chrome");
const {getElementFromPoint, getAdjustedQuads} = require("devtools/shared/layout/utils");
const promise = require("promise");
const {Task} = Cu.import("resource://gre/modules/Task.jsm", {});
var DOMUtils = Cc["@mozilla.org/inspector/dom-utils;1"].getService(Ci.inIDOMUtils);
var loader = Cc["@mozilla.org/moz/jssubscript-loader;1"]
            .getService(Ci.mozIJSSubScriptLoader);
var EventUtils = {};
loader.loadSubScript("chrome://marionette/content/EventUtils.js", EventUtils);

const protocol = require("devtools/server/protocol");
const {Arg, Option, method, RetVal, types} = protocol;

var dumpn = msg => {
  dump(msg + "\n");
}

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
}

const TestActor = exports.TestActor = protocol.ActorClass({
  typeName: "testActor",

  initialize: function(conn, tabActor, options) {
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
      while(selector.length > 1) {
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
   * Get a value for a given attribute name, on one of the elements of the box
   * model highlighter, given its ID.
   * @param {Object} msg The msg.data part expects the following properties
   * - {String} nodeID The full ID of the element to get the attribute for
   * - {String} name The name of the attribute to get
   * - {String} actorID The highlighter actor ID
   * @return {String} The value, if found, null otherwise
   */
  getHighlighterAttribute: protocol.method(function (nodeID, name, actorID) {
    let helper = getHighlighterCanvasFrameHelper(this.conn, actorID);
    if (helper) {
      return helper.getAttributeForElement(nodeID, name);
    }
  }, {
    request: {
      nodeID: Arg(0, "string"),
      name: Arg(1, "string"),
      actorID: Arg(2, "string")
    },
    response: {
      value: RetVal("string")
    }
  }),

  /**
   * Get the textcontent of one of the elements of the box model highlighter,
   * given its ID.
   * @param {String} nodeID The full ID of the element to get the attribute for
   * @param {String} actorID The highlighter actor ID
   * @return {String} The textcontent value
   */
  getHighlighterNodeTextContent: protocol.method(function (nodeID, actorID) {
    let value;
    let helper = getHighlighterCanvasFrameHelper(this.conn, actorID);
    if (helper) {
      value = helper.getTextContentForElement(nodeID);
    }
    return value;
  }, {
    request: {
      nodeID: Arg(0, "string"),
      actorID: Arg(1, "string")
    },
    response: {
      value: RetVal("string")
    }
  }),

  /**
   * Get the number of box-model highlighters created by the SelectorHighlighter
   * @param {String} actorID The highlighter actor ID
   * @return {Number} The number of box-model highlighters created, or null if the
   * SelectorHighlighter was not found.
   */
  getSelectorHighlighterBoxNb: protocol.method(function (actorID) {
    let highlighter = this.conn.getActor(actorID);
    let {_highlighter: h} = highlighter;
    if (!h || !h._highlighters) {
      return null;
    } else {
      return h._highlighters.length;
    }
  }, {
    request: {
      highlighter: Arg(0, "string"),
    },
    response: {
      value: RetVal("number")
    }
  }),

  /**
   * Subscribe to the box-model highlighter's update event, modify an attribute of
   * the currently highlighted node and send a message when the highlighter has
   * updated.
   * @param {String} the name of the attribute to be changed
   * @param {String} the new value for the attribute
   * @param {String} actorID The highlighter actor ID
   */
  changeHighlightedNodeWaitForUpdate: protocol.method(function (name, value, actorID) {
    let deferred = promise.defer();

    let highlighter = this.conn.getActor(actorID);
    let {_highlighter: h} = highlighter;

    h.once("updated", () => {
      deferred.resolve();
    });

    h.currentNode.setAttribute(name, value);

    return deferred.promise;
  }, {
    request: {
      name: Arg(0, "string"),
      value: Arg(1, "string"),
      actorID: Arg(2, "string")
    },
    response: {}
  }),

  /**
   * Subscribe to a given highlighter event and respond when the event is received.
   * @param {String} event The name of the highlighter event to listen to
   * @param {String} actorID The highlighter actor ID
   */
  waitForHighlighterEvent: protocol.method(function (event, actorID) {
    let highlighter = this.conn.getActor(actorID);
    let {_highlighter: h} = highlighter;

    return h.once(event);
  }, {
    request: {
      event: Arg(0, "string"),
      actorID: Arg(1, "string")
    },
    response: {}
  }),

  /**
   * Change the zoom level of the page.
   * Optionally subscribe to the box-model highlighter's update event and waiting
   * for it to refresh before responding.
   * @param {Number} level The new zoom level
   * @param {String} actorID Optional. The highlighter actor ID
   */
  changeZoomLevel: protocol.method(function (level, actorID) {
    dumpn("Zooming page to " + level);
    let deferred = promise.defer();

    if (actorID) {
      let actor = this.conn.getActor(actorID);
      let {_highlighter: h} = actor;
      h.once("updated", () => {
        deferred.resolve();
      });
    } else {
      deferred.resolve();
    }

    let docShell = this.content.QueryInterface(Ci.nsIInterfaceRequestor)
                               .getInterface(Ci.nsIWebNavigation)
                               .QueryInterface(Ci.nsIDocShell);
    docShell.contentViewer.fullZoom = level;

    return deferred.promise;
  }, {
    request: {
      level: Arg(0, "string"),
      actorID: Arg(1, "string"),
    },
    response: {}
  }),

  assertElementAtPoint: protocol.method(function (x, y, selector) {
    let elementAtPoint = getElementFromPoint(this.content.document, x, y);
    if (!elementAtPoint) {
      throw new Error("Unable to find element at (" + x + ", " + y + ")");
    }
    let node = this._querySelector(selector);
    return node == elementAtPoint;
  }, {
    request: {
      x: Arg(0, "number"),
      y: Arg(1, "number"),
      selector: Arg(2, "string")
    },
    response: {
      value: RetVal("boolean")
    }
  }),


  /**
   * Get all box-model regions' adjusted boxquads for the given element
   * @param {String} selector The node selector to target a given element
   * @return {Object} An object with each property being a box-model region, each
   * of them being an object with the p1/p2/p3/p4 properties
   */
  getAllAdjustedQuads: protocol.method(function(selector) {
    let regions = {};
    let node = this._querySelector(selector);
    for (let boxType of ["content", "padding", "border", "margin"]) {
      regions[boxType] = getAdjustedQuads(this.content, node, boxType);
    }

    return regions;
  }, {
    request: {
      selector: Arg(0, "string")
    },
    response: {
      value: RetVal("json")
    }
  }),

  /**
   * Synthesize a mouse event on an element. This handler doesn't send a message
   * back. Consumers should listen to specific events on the inspector/highlighter
   * to know when the event got synthesized.
   * @param {String} selector The node selector to get the node target for the event
   * @param {Number} x
   * @param {Number} y
   * @param {Boolean} center If set to true, x/y will be ignored and
   *                  synthesizeMouseAtCenter will be used instead
   * @param {Object} options Other event options
   */
  synthesizeMouse: protocol.method(function({ selector, x, y, center, options }) {
    let node = this._querySelector(selector);

    if (center) {
      EventUtils.synthesizeMouseAtCenter(node, options, node.ownerDocument.defaultView);
    } else {
      EventUtils.synthesizeMouse(node, x, y, options, node.ownerDocument.defaultView);
    }
  }, {
    request: {
      object: Arg(0, "json")
    },
    response: {}
  }),

  /**
  * Synthesize a key event for an element. This handler doesn't send a message
  * back. Consumers should listen to specific events on the inspector/highlighter
  * to know when the event got synthesized.
  */
  synthesizeKey: protocol.method(function ({key, options, content}) {
    EventUtils.synthesizeKey(key, options, this.content);
  }, {
    request: {
      args: Arg(0, "json")
    },
    response: {}
  }),

  /**
   * Check that an element currently has a pseudo-class lock.
   * @param {String} selector The node selector to get the pseudo-class from
   * @param {String} pseudo The pseudoclass to check for
   * @return {Boolean}
   */
  hasPseudoClassLock: protocol.method(function (selector, pseudo) {
    let node = this._querySelector(selector);
    return DOMUtils.hasPseudoClassLock(node, pseudo);
  }, {
    request: {
      selector: Arg(0, "string"),
      pseudo: Arg(1, "string")
    },
    response: {
      value: RetVal("boolean")
    }
  }),

  loadAndWaitForCustomEvent: protocol.method(function (url) {
    let deferred = promise.defer();
    let self = this;
    // Wait for DOMWindowCreated first, as listening on the current outerwindow
    // doesn't allow receiving test-page-processing-done.
    this.tabActor.chromeEventHandler.addEventListener("DOMWindowCreated", function onWindowCreated() {
      self.tabActor.chromeEventHandler.removeEventListener("DOMWindowCreated", onWindowCreated);
      self.content.addEventListener("test-page-processing-done", function onEvent() {
        self.content.removeEventListener("test-page-processing-done", onEvent);
        deferred.resolve();
      });
    });

    this.content.location = url;
    return deferred.promise;
  }, {
    request: {
      url: Arg(0, "string")
    },
    response: {}
  }),

  hasNode: protocol.method(function (selector) {
    try {
      // _querySelector throws if the node doesn't exists
      this._querySelector(selector);
      return true;
    } catch(e) {
      return false;
    }
  }, {
    request: {
      selector: Arg(0, "string")
    },
    response: {
      value: RetVal("boolean")
    }
  }),

  /**
   * Get the bounding rect for a given DOM node once.
   * @param {String} selector selector identifier to select the DOM node
   * @return {json} the bounding rect info
   */
  getBoundingClientRect: protocol.method(function (selector) {
    let node = this._querySelector(selector);
    return node.getBoundingClientRect();
  }, {
    request: {
      selector: Arg(0, "string"),
    },
    response: {
      value: RetVal("json")
    }
  }),

  /**
   * Set a JS property on a DOM Node.
   * @param {String} selector The node selector
   * @param {String} attribute The attribute name
   * @param {String} value The attribute value
   */
  setProperty: protocol.method(function (selector, property, value) {
    let node = this._querySelector(selector);
    node[property] = value;
  }, {
    request: {
      selector: Arg(0, "string"),
      property: Arg(1, "string"),
      value: Arg(2, "string")
    },
    response: {}
  }),

  /**
   * Get a JS property on a DOM Node.
   * @param {String} selector The node selector
   * @param {String} attribute The attribute name
   * @return {String} value The attribute value
   */
  getProperty: protocol.method(function (selector, property) {
    let node = this._querySelector(selector);
    return node[property];
  }, {
    request: {
      selector: Arg(0, "string"),
      property: Arg(1, "string")
    },
    response: {
      value: RetVal("string")
    }
  }),

  /**
   * Reload an iframe and wait for its load event.
   * @param {String} selector The node selector
   */
  reloadFrame: protocol.method(function (selector) {
    let node = this._querySelector(selector);

    let deferred = promise.defer();

    let onLoad = function () {
      node.removeEventListener("load", onLoad);
      deferred.resolve();
    };
    node.addEventListener("load", onLoad);

    node.contentWindow.location.reload();
    return deferred.promise;
  }, {
    request: {
      selector: Arg(0, "string"),
    },
    response: {}
  }),

  /**
   * Evaluate a JS string in the context of the content document.
   * @param {String} js JS string to evaluate
   * @return {json} The evaluation result
   */
  eval: protocol.method(function (js) {
    // We have to use a sandbox, as CSP prevent us from using eval on apps...
    let sb = Cu.Sandbox(this.content, { sandboxPrototype: this.content });
    return Cu.evalInSandbox(js, sb);
  }, {
    request: {
      js: Arg(0, "string")
    },
    response: {
      value: RetVal("nullable:json")
    }
  }),

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
  scrollWindow: protocol.method(function (x, y, relative) {
    if (isNaN(x) || isNaN(y)) {
      return {};
    }

    let deferred = promise.defer();
    this.content.addEventListener("scroll", function onScroll(event) {
      this.removeEventListener("scroll", onScroll);

      let data = {x: this.content.scrollX, y: this.content.scrollY};
      deferred.resolve(data);
    });

    this.content[relative ? "scrollBy" : "scrollTo"](x, y);

    return deferred.promise;
  }, {
    request: {
      x: Arg(0, "number"),
      y: Arg(1, "number"),
      relative: Arg(2, "nullable:boolean"),
    },
    response: {
      value: RetVal("json")
    }
  }),
});

const TestActorFront = exports.TestActorFront = protocol.FrontClass(TestActor, {
  initialize: function(client, { testActor }, toolbox) {
    protocol.Front.prototype.initialize.call(this, client, { actor: testActor });
    this.manage(this);
    this.toolbox = toolbox;
  },

  /**
   * Zoom the current page to a given level.
   * @param {Number} level The new zoom level.
   * @return {Promise} The returned promise will only resolve when the
   * highlighter has updated to the new zoom level.
   */
  zoomPageTo: function(level) {
    return this.changeZoomLevel(level, this.toolbox.highlighter.actorID);
  },

  changeHighlightedNodeWaitForUpdate: protocol.custom(function(name, value, highlighter) {
    return this._changeHighlightedNodeWaitForUpdate(name, value, (highlighter || this.toolbox.highlighter).actorID);
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
  getHighlighterNodeAttribute: function(nodeID, name, highlighter) {
    return this.getHighlighterAttribute(nodeID, name, (highlighter || this.toolbox.highlighter).actorID);
  },

  getHighlighterNodeTextContent: protocol.custom(function(nodeID, highlighter) {
    return this._getHighlighterNodeTextContent(nodeID, (highlighter || this.toolbox.highlighter).actorID);
  }, {
    impl: "_getHighlighterNodeTextContent"
  }),

  /**
   * Is the highlighter currently visible on the page?
   */
  isHighlighting: function() {
    return this.getHighlighterNodeAttribute("box-model-elements", "hidden")
      .then(value => value === null);
  },

  assertHighlightedNode: Task.async(function* (selector) {
    let {visible, content} = yield this._getBoxModelStatus();
    let points = content.points;
    if (visible) {
      let x = (points.p1.x + points.p2.x + points.p3.x + points.p4.x) / 4;
      let y = (points.p1.y + points.p2.y + points.p3.y + points.p4.y) / 4;

      return this.assertElementAtPoint(x, y, selector);
    } else {
      return false;
    }
  }),

  /**
   * Assert that the box-model highlighter's current position corresponds to the
   * given node boxquads.
   * @param {String} selector The node selector to get the boxQuads from
   * @param {Function} is assertion function to call for equality checks
   * @param {String} prefix An optional prefix for logging information to the
   * console.
   */
  isNodeCorrectlyHighlighted: Task.async(function*(selector, is, prefix="") {
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
  getSimpleBorderRect: Task.async(function*(toolbox) {
    let {border} = yield this._getBoxModelStatus(toolbox);
    let {p1, p2, p3, p4} = border.points;

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
  _getBoxModelStatus: Task.async(function*() {
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
   * Get the coordinate (points attribute) from one of the polygon elements in the
   * box model highlighter.
   */
  _getPointsForRegion: Task.async(function*(region) {
    let d = yield this.getHighlighterNodeAttribute("box-model-" + region, "d");

    let polygons = d.match(/M[^M]+/g);
    if (!polygons) {
      return null;
    }

    let points = polygons[0].trim().split(" ").map(i => {
      return i.replace(/M|L/, "").split(",")
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
  _isRegionHidden: Task.async(function*(region) {
    let value = yield this.getHighlighterNodeAttribute("box-model-" + region, "hidden");
    return value !== null;
  }),

  _getGuideStatus: Task.async(function*(location) {
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
  getGuidesRectangle: Task.async(function*() {
    let tGuide = yield this._getGuideStatus("top");
    let rGuide = yield this._getGuideStatus("right");
    let bGuide = yield this._getGuideStatus("bottom");
    let lGuide = yield this._getGuideStatus("left");

    if (!tGuide.visible || !rGuide.visible || !bGuide.visible || !lGuide.visible) {
      return null;
    }

    return {
      p1: {x: lGuide.x1, y: tGuide.y1},
      p2: {x: rGuide.x1, y: tGuide. y1},
      p3: {x: rGuide.x1, y: bGuide.y1},
      p4: {x: lGuide.x1, y: bGuide.y1}
    };
  }),

  waitForHighlighterEvent: protocol.custom(function(event) {
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
  getHighlighterRegionPath: Task.async(function*(region, highlighter) {
    let d = yield this.getHighlighterNodeAttribute("box-model-" + region, "d", highlighter);
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
        return i.replace(/M|L/, "").split(",")
      }));
    }

    return {d, points};
  })
});
