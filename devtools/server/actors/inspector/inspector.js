/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Here's the server side of the remote inspector.
 *
 * The WalkerActor is the client's view of the debuggee's DOM.  It's gives
 * the client a tree of NodeActor objects.
 *
 * The walker presents the DOM tree mostly unmodified from the source DOM
 * tree, but with a few key differences:
 *
 *  - Empty text nodes are ignored.  This is pretty typical of developer
 *    tools, but maybe we should reconsider that on the server side.
 *  - iframes with documents loaded have the loaded document as the child,
 *    the walker provides one big tree for the whole document tree.
 *
 * There are a few ways to get references to NodeActors:
 *
 *   - When you first get a WalkerActor reference, it comes with a free
 *     reference to the root document's node.
 *   - Given a node, you can ask for children, siblings, and parents.
 *   - You can issue querySelector and querySelectorAll requests to find
 *     other elements.
 *   - Requests that return arbitrary nodes from the tree (like querySelector
 *     and querySelectorAll) will also return any nodes the client hasn't
 *     seen in order to have a complete set of parents.
 *
 * Once you have a NodeFront, you should be able to answer a few questions
 * without further round trips, like the node's name, namespace/tagName,
 * attributes, etc.  Other questions (like a text node's full nodeValue)
 * might require another round trip.
 *
 * The protocol guarantees that the client will always know the parent of
 * any node that is returned by the server.  This means that some requests
 * (like querySelector) will include the extra nodes needed to satisfy this
 * requirement.  The client keeps track of this parent relationship, so the
 * node fronts form a tree that is a subset of the actual DOM tree.
 *
 *
 * We maintain this guarantee to support the ability to release subtrees on
 * the client - when a node is disconnected from the DOM tree we want to be
 * able to free the client objects for all the children nodes.
 *
 * So to be able to answer "all the children of a given node that we have
 * seen on the client side", we guarantee that every time we've seen a node,
 * we connect it up through its parents.
 */

const Services = require("Services");
const protocol = require("devtools/shared/protocol");
const {LongStringActor} = require("devtools/server/actors/string");
const defer = require("devtools/shared/defer");
const ReplayInspector = require("devtools/server/actors/replay/inspector");

const {inspectorSpec} = require("devtools/shared/specs/inspector");

loader.lazyRequireGetter(this, "InspectorActorUtils", "devtools/server/actors/inspector/utils");
loader.lazyRequireGetter(this, "WalkerActor", "devtools/server/actors/inspector/walker", true);
loader.lazyRequireGetter(this, "EyeDropper", "devtools/server/actors/highlighters/eye-dropper", true);
loader.lazyRequireGetter(this, "PageStyleActor", "devtools/server/actors/styles", true);
loader.lazyRequireGetter(this, "HighlighterActor", "devtools/server/actors/highlighters", true);
loader.lazyRequireGetter(this, "CustomHighlighterActor", "devtools/server/actors/highlighters", true);
loader.lazyRequireGetter(this, "isTypeRegistered", "devtools/server/actors/highlighters", true);
loader.lazyRequireGetter(this, "HighlighterEnvironment", "devtools/server/actors/highlighters", true);

const SVG_NS = "http://www.w3.org/2000/svg";
const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

/**
 * Server side of the inspector actor, which is used to create
 * inspector-related actors, including the walker.
 */
exports.InspectorActor = protocol.ActorClassWithSpec(inspectorSpec, {
  initialize: function(conn, targetActor) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this.targetActor = targetActor;

    this._onColorPicked = this._onColorPicked.bind(this);
    this._onColorPickCanceled = this._onColorPickCanceled.bind(this);
    this.destroyEyeDropper = this.destroyEyeDropper.bind(this);
  },

  destroy: function() {
    protocol.Actor.prototype.destroy.call(this);
    this.destroyEyeDropper();

    this._highlighterPromise = null;
    this._pageStylePromise = null;
    this._walkerPromise = null;
    this.walker = null;
    this.targetActor = null;
  },

  get window() {
    return isReplaying ? ReplayInspector.window : this.targetActor.window;
  },

  getWalker: function(options = {}) {
    if (this._walkerPromise) {
      return this._walkerPromise;
    }

    const deferred = defer();
    this._walkerPromise = deferred.promise;

    const isXULDocument =
      this.targetActor.window.document.documentElement.namespaceURI === XUL_NS;
    const loadEvent = isXULDocument ? "load" : "DOMContentLoaded";

    const window = this.window;
    const domReady = () => {
      const targetActor = this.targetActor;
      window.removeEventListener(loadEvent, domReady, true);
      this.walker = WalkerActor(this.conn, targetActor, options);
      this.manage(this.walker);
      this.walker.once("destroyed", () => {
        this._walkerPromise = null;
        this._pageStylePromise = null;
      });
      deferred.resolve(this.walker);
    };

    if (window.document.readyState === "loading") {
      window.addEventListener(loadEvent, domReady, true);
    } else {
      domReady();
    }

    return this._walkerPromise;
  },

  getPageStyle: function() {
    if (this._pageStylePromise) {
      return this._pageStylePromise;
    }

    this._pageStylePromise = this.getWalker().then(walker => {
      const pageStyle = PageStyleActor(this);
      this.manage(pageStyle);
      return pageStyle;
    });
    return this._pageStylePromise;
  },

  /**
   * The most used highlighter actor is the HighlighterActor which can be
   * conveniently retrieved by this method.
   * The same instance will always be returned by this method when called
   * several times.
   * The highlighter actor returned here is used to highlighter elements's
   * box-models from the markup-view, box model, console, debugger, ... as
   * well as select elements with the pointer (pick).
   *
   * @param {Boolean} autohide Optionally autohide the highlighter after an
   * element has been picked
   * @return {HighlighterActor}
   */
  getHighlighter: function(autohide) {
    if (this._highlighterPromise) {
      return this._highlighterPromise;
    }

    this._highlighterPromise = this.getWalker().then(walker => {
      const highlighter = HighlighterActor(this, autohide);
      this.manage(highlighter);
      return highlighter;
    });
    return this._highlighterPromise;
  },

  /**
   * If consumers need to display several highlighters at the same time or
   * different types of highlighters, then this method should be used, passing
   * the type name of the highlighter needed as argument.
   * A new instance will be created everytime the method is called, so it's up
   * to the consumer to release it when it is not needed anymore
   *
   * @param {String} type The type of highlighter to create
   * @return {Highlighter} The highlighter actor instance or null if the
   * typeName passed doesn't match any available highlighter
   */
  getHighlighterByType: function(typeName) {
    if (isTypeRegistered(typeName)) {
      return CustomHighlighterActor(this, typeName);
    }
    return null;
  },

  /**
   * Get the node's image data if any (for canvas and img nodes).
   * Returns an imageData object with the actual data being a LongStringActor
   * and a size json object.
   * The image data is transmitted as a base64 encoded png data-uri.
   * The method rejects if the node isn't an image or if the image is missing
   *
   * Accepts a maxDim request parameter to resize images that are larger. This
   * is important as the resizing occurs server-side so that image-data being
   * transfered in the longstring back to the client will be that much smaller
   */
  getImageDataFromURL: function(url, maxDim) {
    const img = new this.window.Image();
    img.src = url;

    // imageToImageData waits for the image to load.
    return InspectorActorUtils.imageToImageData(img, maxDim).then(imageData => {
      return {
        data: LongStringActor(this.conn, imageData.data),
        size: imageData.size,
      };
    });
  },

  /**
   * Resolve a URL to its absolute form, in the scope of a given content window.
   * @param {String} url.
   * @param {NodeActor} node If provided, the owner window of this node will be
   * used to resolve the URL. Otherwise, the top-level content window will be
   * used instead.
   * @return {String} url.
   */
  resolveRelativeURL: function(url, node) {
    const document = InspectorActorUtils.isNodeDead(node)
                   ? this.window.document
                   : InspectorActorUtils.nodeDocument(node.rawNode);

    if (!document) {
      return url;
    }

    const baseURI = Services.io.newURI(document.location.href);
    return Services.io.newURI(url, null, baseURI).spec;
  },

  /**
   * Create an instance of the eye-dropper highlighter and store it on this._eyeDropper.
   * Note that for now, a new instance is created every time to deal with page navigation.
   */
  createEyeDropper: function() {
    this.destroyEyeDropper();
    this._highlighterEnv = new HighlighterEnvironment();
    this._highlighterEnv.initFromTargetActor(this.targetActor);
    this._eyeDropper = new EyeDropper(this._highlighterEnv);
  },

  /**
   * Destroy the current eye-dropper highlighter instance.
   */
  destroyEyeDropper: function() {
    if (this._eyeDropper) {
      this.cancelPickColorFromPage();
      this._eyeDropper.destroy();
      this._eyeDropper = null;
      this._highlighterEnv.destroy();
      this._highlighterEnv = null;
    }
  },

  /**
   * Pick a color from the page using the eye-dropper. This method doesn't return anything
   * but will cause events to be sent to the front when a color is picked or when the user
   * cancels the picker.
   * @param {Object} options
   */
  pickColorFromPage: function(options) {
    this.createEyeDropper();
    this._eyeDropper.show(this.window.document.documentElement, options);
    this._eyeDropper.once("selected", this._onColorPicked);
    this._eyeDropper.once("canceled", this._onColorPickCanceled);
    this.targetActor.once("will-navigate", this.destroyEyeDropper);
  },

  /**
   * After the pickColorFromPage method is called, the only way to dismiss the eye-dropper
   * highlighter is for the user to click in the page and select a color. If you need to
   * dismiss the eye-dropper programatically instead, use this method.
   */
  cancelPickColorFromPage: function() {
    if (this._eyeDropper) {
      this._eyeDropper.hide();
      this._eyeDropper.off("selected", this._onColorPicked);
      this._eyeDropper.off("canceled", this._onColorPickCanceled);
      this.targetActor.off("will-navigate", this.destroyEyeDropper);
    }
  },

  /**
   * Check if the current document supports highlighters using a canvasFrame anonymous
   * content container (ie all highlighters except the SimpleOutlineHighlighter).
   * It is impossible to detect the feature programmatically as some document types simply
   * don't render the canvasFrame without throwing any error.
   */
  supportsHighlighters: function() {
    const doc = this.targetActor.window.document;
    const ns = doc.documentElement.namespaceURI;

    // XUL documents do not support insertAnonymousContent().
    if (ns === XUL_NS) {
      return false;
    }

    // SVG documents do not render the canvasFrame (see Bug 1157592).
    if (ns === SVG_NS) {
      return false;
    }

    return true;
  },

  _onColorPicked: function(color) {
    this.emit("color-picked", color);
  },

  _onColorPickCanceled: function() {
    this.emit("color-pick-canceled");
  },
});
