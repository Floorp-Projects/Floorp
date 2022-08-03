/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cu, Cr } = require("chrome");
const {
  getCurrentZoom,
  getWindowDimensions,
  getViewportDimensions,
  loadSheet,
  removeSheet,
} = require("devtools/shared/layout/utils");
const EventEmitter = require("devtools/shared/event-emitter");
const InspectorUtils = require("InspectorUtils");

const lazyContainer = {};

loader.lazyRequireGetter(
  lazyContainer,
  "CssLogic",
  "devtools/server/actors/inspector/css-logic",
  true
);
loader.lazyRequireGetter(
  this,
  "isDocumentReady",
  "devtools/server/actors/inspector/utils",
  true
);

exports.getComputedStyle = node =>
  lazyContainer.CssLogic.getComputedStyle(node);

exports.getBindingElementAndPseudo = node =>
  lazyContainer.CssLogic.getBindingElementAndPseudo(node);

exports.hasPseudoClassLock = (...args) =>
  InspectorUtils.hasPseudoClassLock(...args);

exports.addPseudoClassLock = (...args) =>
  InspectorUtils.addPseudoClassLock(...args);

exports.removePseudoClassLock = (...args) =>
  InspectorUtils.removePseudoClassLock(...args);

const SVG_NS = "http://www.w3.org/2000/svg";
const XHTML_NS = "http://www.w3.org/1999/xhtml";
const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
// Highlighter in parent process will create an iframe relative to its target
// window. We need to make sure that the iframe is styled correctly. Note:
// this styles are taken from browser/base/content/browser.css
// iframe.devtools-highlighter-renderer rules.
const XUL_HIGHLIGHTER_STYLES_SHEET = `data:text/css;charset=utf-8,
:root > iframe.devtools-highlighter-renderer {
  border: none;
  pointer-events: none;
  position: fixed;
  top: 0;
  left: 0;
  width: 100%;
  height: 100%;
  z-index: 2;
  color-scheme: light;
}`;

const STYLESHEET_URI =
  "resource://devtools/server/actors/" + "highlighters.css";

const _tokens = Symbol("classList/tokens");

/**
 * Shims the element's `classList` for anonymous content elements; used
 * internally by `CanvasFrameAnonymousContentHelper.getElement()` method.
 */
function ClassList(className) {
  const trimmed = (className || "").trim();
  this[_tokens] = trimmed ? trimmed.split(/\s+/) : [];
}

ClassList.prototype = {
  item(index) {
    return this[_tokens][index];
  },
  contains(token) {
    return this[_tokens].includes(token);
  },
  add(token) {
    if (!this.contains(token)) {
      this[_tokens].push(token);
    }
    EventEmitter.emit(this, "update");
  },
  remove(token) {
    const index = this[_tokens].indexOf(token);

    if (index > -1) {
      this[_tokens].splice(index, 1);
    }
    EventEmitter.emit(this, "update");
  },
  toggle(token, force) {
    // If force parameter undefined retain the toggle behavior
    if (force === undefined) {
      if (this.contains(token)) {
        this.remove(token);
      } else {
        this.add(token);
      }
    } else if (force) {
      // If force is true, enforce token addition
      this.add(token);
    } else {
      // If force is falsy value, enforce token removal
      this.remove(token);
    }
  },
  get length() {
    return this[_tokens].length;
  },
  *[Symbol.iterator]() {
    for (let i = 0; i < this.tokens.length; i++) {
      yield this[_tokens][i];
    }
  },
  toString() {
    return this[_tokens].join(" ");
  },
};

/**
 * Is this content window a XUL window?
 * @param {Window} window
 * @return {Boolean}
 */
function isXUL(window) {
  return window.document.documentElement.namespaceURI === XUL_NS;
}
exports.isXUL = isXUL;

/**
 * Returns true if a DOM node is "valid", where "valid" means that the node isn't a dead
 * object wrapper, is still attached to a document, and is of a given type.
 * @param {DOMNode} node
 * @param {Number} nodeType Optional, defaults to ELEMENT_NODE
 * @return {Boolean}
 */
function isNodeValid(node, nodeType = Node.ELEMENT_NODE) {
  // Is it still alive?
  if (!node || Cu.isDeadWrapper(node)) {
    return false;
  }

  // Is it of the right type?
  if (node.nodeType !== nodeType) {
    return false;
  }

  // Is its document accessible?
  const doc = node.nodeType === Node.DOCUMENT_NODE ? node : node.ownerDocument;
  if (!doc || !doc.defaultView) {
    return false;
  }

  // Is the node connected to the document?
  if (!node.isConnected) {
    return false;
  }

  return true;
}
exports.isNodeValid = isNodeValid;

/**
 * Every highlighters should insert their markup content into the document's
 * canvasFrame anonymous content container (see dom/webidl/Document.webidl).
 *
 * Since this container gets cleared when the document navigates, highlighters
 * should use this helper to have their markup content automatically re-inserted
 * in the new document.
 *
 * Since the markup content is inserted in the canvasFrame using
 * insertAnonymousContent, this means that it can be modified using the API
 * described in AnonymousContent.webidl.
 * To retrieve the AnonymousContent instance, use the content getter.
 *
 * @param {HighlighterEnv} highlighterEnv
 *        The environemnt which windows will be used to insert the node.
 * @param {Function} nodeBuilder
 *        A function that, when executed, returns a DOM node to be inserted into
 *        the canvasFrame.
 * @param {Object} options
 * @param {Boolean} options.waitForDocumentToLoad
 *        Set to false to try to insert the anonymous content even if the document
 *        isn't loaded yet. Defaults to true.
 */
function CanvasFrameAnonymousContentHelper(
  highlighterEnv,
  nodeBuilder,
  { waitForDocumentToLoad = true } = {}
) {
  this.highlighterEnv = highlighterEnv;
  this.nodeBuilder = nodeBuilder;
  this.waitForDocumentToLoad = !!waitForDocumentToLoad;

  this._onWindowReady = this._onWindowReady.bind(this);
  this.highlighterEnv.on("window-ready", this._onWindowReady);

  this.listeners = new Map();
  this.elements = new Map();
}

CanvasFrameAnonymousContentHelper.prototype = {
  initialize() {
    // _insert will resolve this promise once the markup is displayed
    const onInitialized = new Promise(resolve => {
      this._initialized = resolve;
    });
    // Only try to create the highlighter when the document is loaded,
    // otherwise, wait for the window-ready event to fire.
    const doc = this.highlighterEnv.document;
    if (
      doc.documentElement &&
      (!this.waitForDocumentToLoad ||
        isDocumentReady(doc) ||
        doc.readyState !== "uninitialized")
    ) {
      this._insert();
    }

    return onInitialized;
  },

  destroy() {
    this._remove();
    if (this._iframe) {
      // If iframe is used, remove one ref count from its numberOfHighlighters
      // data attribute.
      const numberOfHighlighters =
        parseInt(this._iframe.dataset.numberOfHighlighters, 10) - 1;
      this._iframe.dataset.numberOfHighlighters = numberOfHighlighters;
      // If we reached 0, we can now remove the iframe and its styling from
      // target window.
      if (numberOfHighlighters === 0) {
        this._iframe.remove();
        removeSheet(this.highlighterEnv.window, XUL_HIGHLIGHTER_STYLES_SHEET);
      }
      this._iframe = null;
    }

    this.highlighterEnv.off("window-ready", this._onWindowReady);
    this.highlighterEnv = this.nodeBuilder = this._content = null;
    this.anonymousContentDocument = null;
    this.anonymousContentWindow = null;
    this.pageListenerTarget = null;

    this._removeAllListeners();
    this.elements.clear();
  },

  async _insert() {
    if (this.waitForDocumentToLoad) {
      await waitForContentLoaded(this.highlighterEnv.window);
    }
    if (!this.highlighterEnv) {
      // CanvasFrameAnonymousContentHelper was already destroyed.
      return;
    }

    const window = this.highlighterEnv.window;
    const isXULWindow = isXUL(window);
    const isChromeWindow = window.isChromeWindow;

    if (isXULWindow || isChromeWindow) {
      // In order to use anonymous content, we need to create and use an IFRAME
      // inside a XUL document first and use its window/document the same way we
      // would normally use highlighter environment's window/document.
      // See Bug 1594587 for more details.
      // For Chrome Windows, we also need to do it as the first call to insertAnonymousContent
      // closes XUL popups even if ui.popup.disable_autohide is true (See Bug 1768896).

      const { documentElement } = window.document;
      if (!this._iframe) {
        this._iframe = documentElement.querySelector(
          ":scope > .devtools-highlighter-renderer"
        );
        if (this._iframe) {
          // If iframe is used and already exists, add one ref count to its
          // numberOfHighlighters data attribute.
          const numberOfHighlighters =
            parseInt(this._iframe.dataset.numberOfHighlighters, 10) + 1;
          this._iframe.dataset.numberOfHighlighters = numberOfHighlighters;
        }
      }

      if (!this._iframe) {
        this._iframe = window.document.createElement("iframe");
        // We need the color-scheme shenanigans to ensure that the iframe is
        // transparent, see bug 1773155, bug 1738380, and
        // https://github.com/mozilla/wg-decisions/issues/774.
        this._iframe.srcdoc =
          "<!doctype html><meta name=color-scheme content=light>";
        this._iframe.classList.add("devtools-highlighter-renderer");
        // If iframe is used for the first time, add ref count of one to its
        // numberOfHighlighters data attribute.
        this._iframe.dataset.numberOfHighlighters = 1;
        documentElement.append(this._iframe);
        loadSheet(window, XUL_HIGHLIGHTER_STYLES_SHEET);
      }

      if (this.waitForDocumentToLoad) {
        await waitForContentLoaded(this._iframe);
      }
      if (!this.highlighterEnv) {
        // CanvasFrameAnonymousContentHelper was already destroyed.
        return;
      }

      // If it's a XUL window anonymous content will be inserted inside a newly
      // created IFRAME in the chrome window.
      this.anonymousContentDocument = this._iframe.contentDocument;
      this.anonymousContentWindow = this._iframe.contentWindow;
      this.pageListenerTarget = this._iframe.contentWindow;
    } else {
      // Regular highlighters are drawn inside the anonymous content of the
      // highlighter environment document.
      this.anonymousContentDocument = this.highlighterEnv.document;
      this.anonymousContentWindow = this.highlighterEnv.window;
      this.pageListenerTarget = this.highlighterEnv.pageListenerTarget;
    }

    // For now highlighters.css is injected in content as a ua sheet because
    // we no longer support scoped style sheets (see bug 1345702).
    // If it did, highlighters.css would be injected as an anonymous content
    // node using CanvasFrameAnonymousContentHelper instead.
    loadSheet(this.anonymousContentWindow, STYLESHEET_URI);

    const node = this.nodeBuilder();

    // It was stated that hidden documents don't accept
    // `insertAnonymousContent` calls yet. That doesn't seems the case anymore,
    // at least on desktop. Therefore, removing the code that was dealing with
    // that scenario, fixes when we're adding anonymous content in a tab that
    // is not the active one (see bug 1260043 and bug 1260044)
    try {
      // If we didn't wait for the document to load, we want to force a layout update
      // to ensure the anonymous content will be rendered (see Bug 1580394).
      const forceSynchronousLayoutUpdate = !this.waitForDocumentToLoad;
      this._content = this.anonymousContentDocument.insertAnonymousContent(
        node,
        forceSynchronousLayoutUpdate
      );
    } catch (e) {
      // If the `insertAnonymousContent` fails throwing a `NS_ERROR_UNEXPECTED`, it means
      // we don't have access to a `CustomContentContainer` yet (see bug 1365075).
      // At this point, it could only happen on document's interactive state, and we
      // need to wait until the `complete` state before inserting the anonymous content
      // again.
      if (
        e.result === Cr.NS_ERROR_UNEXPECTED &&
        this.anonymousContentDocument.readyState === "interactive"
      ) {
        // The next state change will be "complete" since the current is "interactive"
        await new Promise(resolve => {
          this.anonymousContentDocument.addEventListener(
            "readystatechange",
            resolve,
            { once: true }
          );
        });
        this._content = this.anonymousContentDocument.insertAnonymousContent(
          node
        );
      } else {
        throw e;
      }
    }

    this._initialized();
  },

  _remove() {
    try {
      this.anonymousContentDocument.removeAnonymousContent(this._content);
    } catch (e) {
      // If the current window isn't the one the content was inserted into, this
      // will fail, but that's fine.
    }
  },

  /**
   * The "window-ready" event can be triggered when:
   *   - a new window is created
   *   - a window is unfrozen from bfcache
   *   - when first attaching to a page
   *   - when swapping frame loaders (moving tabs, toggling RDM)
   */
  _onWindowReady({ isTopLevel }) {
    if (isTopLevel) {
      this._removeAllListeners();
      this.elements.clear();
      if (this._iframe) {
        // When we are switching top level targets, we can remove the iframe and
        // its styling as well, since it will be re-created for the new top
        // level target document.
        this._iframe.remove();
        removeSheet(this.highlighterEnv.window, XUL_HIGHLIGHTER_STYLES_SHEET);
        this._iframe = null;
      }

      this._insert();
    }
  },

  getComputedStylePropertyValue(id, property) {
    return (
      this.content && this.content.getComputedStylePropertyValue(id, property)
    );
  },

  getTextContentForElement(id) {
    return this.content && this.content.getTextContentForElement(id);
  },

  setTextContentForElement(id, text) {
    if (this.content) {
      this.content.setTextContentForElement(id, text);
    }
  },

  setAttributeForElement(id, name, value) {
    if (this.content) {
      this.content.setAttributeForElement(id, name, value);
    }
  },

  getAttributeForElement(id, name) {
    return this.content && this.content.getAttributeForElement(id, name);
  },

  removeAttributeForElement(id, name) {
    if (this.content) {
      this.content.removeAttributeForElement(id, name);
    }
  },

  hasAttributeForElement(id, name) {
    return typeof this.getAttributeForElement(id, name) === "string";
  },

  getCanvasContext(id, type = "2d") {
    return this.content && this.content.getCanvasContext(id, type);
  },

  /**
   * Add an event listener to one of the elements inserted in the canvasFrame
   * native anonymous container.
   * Like other methods in this helper, this requires the ID of the element to
   * be passed in.
   *
   * Note that if the content page navigates, the event listeners won't be
   * added again.
   *
   * Also note that unlike traditional DOM events, the events handled by
   * listeners added here will propagate through the document only through
   * bubbling phase, so the useCapture parameter isn't supported.
   * It is possible however to call e.stopPropagation() to stop the bubbling.
   *
   * IMPORTANT: the chrome-only canvasFrame insertion API takes great care of
   * not leaking references to inserted elements to chrome JS code. That's
   * because otherwise, chrome JS code could freely modify native anon elements
   * inside the canvasFrame and probably change things that are assumed not to
   * change by the C++ code managing this frame.
   * See https://wiki.mozilla.org/DevTools/Highlighter#The_AnonymousContent_API
   * Unfortunately, the inserted nodes are still available via
   * event.originalTarget, and that's what the event handler here uses to check
   * that the event actually occured on the right element, but that also means
   * consumers of this code would be able to access the inserted elements.
   * Therefore, the originalTarget property will be nullified before the event
   * is passed to your handler.
   *
   * IMPL DETAIL: A single event listener is added per event types only, at
   * browser level and if the event originalTarget is found to have the provided
   * ID, the callback is executed (and then IDs of parent nodes of the
   * originalTarget are checked too).
   *
   * @param {String} id
   * @param {String} type
   * @param {Function} handler
   */
  addEventListenerForElement(id, type, handler) {
    if (typeof id !== "string") {
      throw new Error(
        "Expected a string ID in addEventListenerForElement but" + " got: " + id
      );
    }

    // If no one is listening for this type of event yet, add one listener.
    if (!this.listeners.has(type)) {
      const target = this.pageListenerTarget;
      target.addEventListener(type, this, true);
      // Each type entry in the map is a map of ids:handlers.
      this.listeners.set(type, new Map());
    }

    const listeners = this.listeners.get(type);
    listeners.set(id, handler);
  },

  /**
   * Remove an event listener from one of the elements inserted in the
   * canvasFrame native anonymous container.
   * @param {String} id
   * @param {String} type
   */
  removeEventListenerForElement(id, type) {
    const listeners = this.listeners.get(type);
    if (!listeners) {
      return;
    }
    listeners.delete(id);

    // If no one is listening for event type anymore, remove the listener.
    if (!this.listeners.has(type)) {
      const target = this.pageListenerTarget;
      target.removeEventListener(type, this, true);
    }
  },

  handleEvent(event) {
    const listeners = this.listeners.get(event.type);
    if (!listeners) {
      return;
    }

    // Hide the originalTarget property to avoid exposing references to native
    // anonymous elements. See addEventListenerForElement's comment.
    let isPropagationStopped = false;
    const eventProxy = new Proxy(event, {
      get: (obj, name) => {
        if (name === "originalTarget") {
          return null;
        } else if (name === "stopPropagation") {
          return () => {
            isPropagationStopped = true;
          };
        }
        return obj[name];
      },
    });

    // Start at originalTarget, bubble through ancestors and call handlers when
    // needed.
    let node = event.originalTarget;
    while (node) {
      const handler = listeners.get(node.id);
      if (handler) {
        handler(eventProxy, node.id);
        if (isPropagationStopped) {
          break;
        }
      }
      node = node.parentNode;
    }
  },

  _removeAllListeners() {
    if (this.pageListenerTarget) {
      const target = this.pageListenerTarget;
      for (const [type] of this.listeners) {
        target.removeEventListener(type, this, true);
      }
    }
    this.listeners.clear();
  },

  getElement(id) {
    if (this.elements.has(id)) {
      return this.elements.get(id);
    }

    const classList = new ClassList(this.getAttributeForElement(id, "class"));

    EventEmitter.on(classList, "update", () => {
      this.setAttributeForElement(id, "class", classList.toString());
    });

    const element = {
      getTextContent: () => this.getTextContentForElement(id),
      setTextContent: text => this.setTextContentForElement(id, text),
      setAttribute: (name, val) => this.setAttributeForElement(id, name, val),
      getAttribute: name => this.getAttributeForElement(id, name),
      removeAttribute: name => this.removeAttributeForElement(id, name),
      hasAttribute: name => this.hasAttributeForElement(id, name),
      getCanvasContext: type => this.getCanvasContext(id, type),
      addEventListener: (type, handler) => {
        return this.addEventListenerForElement(id, type, handler);
      },
      removeEventListener: (type, handler) => {
        return this.removeEventListenerForElement(id, type, handler);
      },
      computedStyle: {
        getPropertyValue: property =>
          this.getComputedStylePropertyValue(id, property),
      },
      classList,
    };

    this.elements.set(id, element);

    return element;
  },

  get content() {
    if (!this._content || Cu.isDeadWrapper(this._content)) {
      return null;
    }
    return this._content;
  },

  /**
   * The canvasFrame anonymous content container gets zoomed in/out with the
   * page. If this is unwanted, i.e. if you want the inserted element to remain
   * unzoomed, then this method can be used.
   *
   * Consumers of the CanvasFrameAnonymousContentHelper should call this method,
   * it isn't executed automatically. Typically, AutoRefreshHighlighter can call
   * it when _update is executed.
   *
   * The matching element will be scaled down or up by 1/zoomLevel (using css
   * transform) to cancel the current zoom. The element's width and height
   * styles will also be set according to the scale. Finally, the element's
   * position will be set as absolute.
   *
   * Note that if the matching element already has an inline style attribute, it
   * *won't* be preserved.
   *
   * @param {DOMNode} node This node is used to determine which container window
   * should be used to read the current zoom value.
   * @param {String} id The ID of the root element inserted with this API.
   */
  scaleRootElement(node, id) {
    const boundaryWindow = this.highlighterEnv.window;
    const zoom = getCurrentZoom(node);
    // Hide the root element and force the reflow in order to get the proper window's
    // dimensions without increasing them.
    this.setAttributeForElement(id, "style", "display: none");
    node.offsetWidth;

    let { width, height } = getWindowDimensions(boundaryWindow);
    let value = "";

    if (zoom !== 1) {
      value = `transform-origin:top left; transform:scale(${1 / zoom}); `;
      width *= zoom;
      height *= zoom;
    }

    value += `position:absolute; width:${width}px;height:${height}px; overflow:hidden`;

    this.setAttributeForElement(id, "style", value);
  },

  /**
   * Helper function that creates SVG DOM nodes.
   * @param {Object} Options for the node include:
   * - nodeType: the type of node, defaults to "box".
   * - attributes: a {name:value} object to be used as attributes for the node.
   * - prefix: a string that will be used to prefix the values of the id and class
   *   attributes.
   * - parent: if provided, the newly created element will be appended to this
   *   node.
   */
  createSVGNode(options) {
    if (!options.nodeType) {
      options.nodeType = "box";
    }

    options.namespace = SVG_NS;

    return this.createNode(options);
  },

  /**
   * Helper function that creates DOM nodes.
   * @param {Object} Options for the node include:
   * - nodeType: the type of node, defaults to "div".
   * - namespace: the namespace to use to create the node, defaults to XHTML namespace.
   * - attributes: a {name:value} object to be used as attributes for the node.
   * - prefix: a string that will be used to prefix the values of the id and class
   *   attributes.
   * - parent: if provided, the newly created element will be appended to this
   *   node.
   * - text: if provided, set the text content of the element.
   */
  createNode(options) {
    const type = options.nodeType || "div";
    const namespace = options.namespace || XHTML_NS;
    const doc = this.anonymousContentDocument;

    const node = doc.createElementNS(namespace, type);

    for (const name in options.attributes || {}) {
      let value = options.attributes[name];
      if (options.prefix && (name === "class" || name === "id")) {
        value = options.prefix + value;
      }
      node.setAttribute(name, value);
    }

    if (options.parent) {
      options.parent.appendChild(node);
    }

    if (options.text) {
      node.appendChild(doc.createTextNode(options.text));
    }

    return node;
  },
};
exports.CanvasFrameAnonymousContentHelper = CanvasFrameAnonymousContentHelper;

/**
 * Wait for document readyness.
 * @param {Object} iframeOrWindow
 *        IFrame or Window for which the content should be loaded.
 */
function waitForContentLoaded(iframeOrWindow) {
  let loadEvent = "DOMContentLoaded";
  // If we are waiting for an iframe to load and it is for a XUL window
  // highlighter that is not browser toolbox, we must wait for IFRAME's "load".
  if (
    iframeOrWindow.contentWindow &&
    iframeOrWindow.ownerGlobal !==
      iframeOrWindow.contentWindow.browsingContext.topChromeWindow
  ) {
    loadEvent = "load";
  }

  const doc = iframeOrWindow.contentDocument || iframeOrWindow.document;
  if (isDocumentReady(doc)) {
    return Promise.resolve();
  }

  return new Promise(resolve => {
    iframeOrWindow.addEventListener(loadEvent, resolve, { once: true });
  });
}

/**
 * Move the infobar to the right place in the highlighter. This helper method is utilized
 * in both css-grid.js and box-model.js to help position the infobar in an appropriate
 * space over the highlighted node element or grid area. The infobar is used to display
 * relevant information about the highlighted item (ex, node or grid name and dimensions).
 *
 * This method will first try to position the infobar to top or bottom of the container
 * such that it has enough space for the height of the infobar. Afterwards, it will try
 * to horizontally center align with the container element if possible.
 *
 * @param  {DOMNode} container
 *         The container element which will be used to position the infobar.
 * @param  {Object} bounds
 *         The content bounds of the container element.
 * @param  {Window} win
 *         The window object.
 * @param  {Object} [options={}]
 *         Advanced options for the infobar.
 * @param  {String} options.position
 *         Force the infobar to be displayed either on "top" or "bottom". Any other value
 *         will be ingnored.
 */
function moveInfobar(container, bounds, win, options = {}) {
  const zoom = getCurrentZoom(win);
  const viewport = getViewportDimensions(win);

  const { computedStyle } = container;

  const margin = 2;
  const arrowSize = parseFloat(
    computedStyle.getPropertyValue("--highlighter-bubble-arrow-size")
  );
  const containerHeight = parseFloat(computedStyle.getPropertyValue("height"));
  const containerWidth = parseFloat(computedStyle.getPropertyValue("width"));
  const containerHalfWidth = containerWidth / 2;

  const viewportWidth = viewport.width * zoom;
  const viewportHeight = viewport.height * zoom;
  let { pageXOffset, pageYOffset } = win;

  pageYOffset *= zoom;
  pageXOffset *= zoom;

  // Defines the boundaries for the infobar.
  const topBoundary = margin;
  const bottomBoundary = viewportHeight - containerHeight - margin - 1;
  const leftBoundary = containerHalfWidth + margin;
  const rightBoundary = viewportWidth - containerHalfWidth - margin;

  // Set the default values.
  let top = bounds.y - containerHeight - arrowSize;
  const bottom = bounds.bottom + margin + arrowSize;
  let left = bounds.x + bounds.width / 2;
  let isOverlapTheNode = false;
  let positionAttribute = "top";
  let position = "absolute";

  // Here we start the math.
  // We basically want to position absolutely the infobar, except when is pointing to a
  // node that is offscreen or partially offscreen, in a way that the infobar can't
  // be placed neither on top nor on bottom.
  // In such cases, the infobar will overlap the node, and to limit the latency given
  // by APZ (See Bug 1312103) it will be positioned as "fixed".
  // It's a sort of "position: sticky" (but positioned as absolute instead of relative).
  const canBePlacedOnTop = top >= pageYOffset;
  const canBePlacedOnBottom = bottomBoundary + pageYOffset - bottom > 0;
  const forcedOnTop = options.position === "top";
  const forcedOnBottom = options.position === "bottom";

  if (
    (!canBePlacedOnTop && canBePlacedOnBottom && !forcedOnTop) ||
    forcedOnBottom
  ) {
    top = bottom;
    positionAttribute = "bottom";
  }

  const isOffscreenOnTop = top < topBoundary + pageYOffset;
  const isOffscreenOnBottom = top > bottomBoundary + pageYOffset;
  const isOffscreenOnLeft = left < leftBoundary + pageXOffset;
  const isOffscreenOnRight = left > rightBoundary + pageXOffset;

  if (isOffscreenOnTop) {
    top = topBoundary;
    isOverlapTheNode = true;
  } else if (isOffscreenOnBottom) {
    top = bottomBoundary;
    isOverlapTheNode = true;
  } else if (isOffscreenOnLeft || isOffscreenOnRight) {
    isOverlapTheNode = true;
    top -= pageYOffset;
  }

  if (isOverlapTheNode) {
    left = Math.min(Math.max(leftBoundary, left - pageXOffset), rightBoundary);

    position = "fixed";
    container.setAttribute("hide-arrow", "true");
  } else {
    position = "absolute";
    container.removeAttribute("hide-arrow");
  }

  // We need to scale the infobar Independently from the highlighter's container;
  // otherwise the `position: fixed` won't work, since "any value other than `none` for
  // the transform, results in the creation of both a stacking context and a containing
  // block. The object acts as a containing block for fixed positioned descendants."
  // (See https://www.w3.org/TR/css-transforms-1/#transform-rendering)
  // We also need to shift the infobar 50% to the left in order for it to appear centered
  // on the element it points to.
  container.setAttribute(
    "style",
    `
    position:${position};
    transform-origin: 0 0;
    transform: scale(${1 / zoom}) translate(calc(${left}px - 50%), ${top}px)`
  );

  container.setAttribute("position", positionAttribute);
}
exports.moveInfobar = moveInfobar;
