/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cc, Cu, Ci} = require("chrome");
const promise = require("sdk/core/promise");
const IOService = Cc["@mozilla.org/network/io-service;1"]
  .getService(Ci.nsIIOService);

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource:///modules/devtools/ViewHelpers.jsm");

const GRADIENT_RE = /\b(repeating-)?(linear|radial)-gradient\(((rgb|hsl)a?\(.+?\)|[^\)])+\)/gi;
const BORDERCOLOR_RE = /^border-[-a-z]*color$/ig;
const BORDER_RE = /^border(-(top|bottom|left|right))?$/ig;
const BACKGROUND_IMAGE_RE = /url\([\'\"]?(.*?)[\'\"]?\)/;

/**
 * Tooltip widget.
 *
 * This widget is intended at any tool that may need to show rich content in the
 * form of floating panels.
 * A common use case is image previewing in the CSS rule view, but more complex
 * use cases may include color pickers, object inspection, etc...
 *
 * Tooltips are based on XUL (namely XUL arrow-type <panel>s), and therefore
 * need a XUL Document to live in.
 * This is pretty much the only requirement they have on their environment.
 *
 * The way to use a tooltip is simply by instantiating a tooltip yourself and
 * attaching some content in it, or using one of the ready-made content types.
 *
 * A convenient `startTogglingOnHover` method may avoid having to register event
 * handlers yourself if the tooltip has to be shown when hovering over a
 * specific element or group of elements (which is usually the most common case)
 */

/**
 * The low level structure of a tooltip is a XUL element (a <panel>, although
 * <tooltip> is supported too, it won't have the nice arrow shape).
 */
let PanelFactory = {
  get: function(doc, xulTag="panel") {
    // Create the tooltip
    let panel = doc.createElement(xulTag);
    panel.setAttribute("hidden", true);

    if (xulTag === "panel") {
      // Prevent the click used to close the panel from being consumed
      panel.setAttribute("consumeoutsideclicks", false);
      panel.setAttribute("type", "arrow");
      panel.setAttribute("level", "top");
    }

    panel.setAttribute("class", "devtools-tooltip devtools-tooltip-" + xulTag);
    doc.querySelector("window").appendChild(panel);

    return panel;
  }
};

/**
 * Tooltip class.
 *
 * Basic usage:
 *   let t = new Tooltip(xulDoc);
 *   t.content = someXulContent;
 *   t.show();
 *   t.hide();
 *   t.destroy();
 *
 * Better usage:
 *   let t = new Tooltip(xulDoc);
 *   t.startTogglingOnHover(container, target => {
 *     if (<condition based on target>) {
 *       t.setImageContent("http://image.png");
 *       return true;
 *     }
 *   });
 *   t.destroy();
 *
 * @param XULDocument doc
 *        The XUL document hosting this tooltip
 */
function Tooltip(doc) {
  this.doc = doc;
  this.panel = PanelFactory.get(doc);

  // Used for namedTimeouts in the mouseover handling
  this.uid = "tooltip-" + Date.now();
}

module.exports.Tooltip = Tooltip;

Tooltip.prototype = {
  /**
   * Show the tooltip. It might be wise to append some content first if you
   * don't want the tooltip to be empty. You may access the content of the
   * tooltip by setting a XUL node to t.tooltip.content.
   * @param {node} anchor
   *        Which node should the tooltip be shown on
   * @param {string} position
   *        https://developer.mozilla.org/en-US/docs/XUL/PopupGuide/Positioning
   *        Defaults to before_start
   */
  show: function(anchor, position="before_start") {
    this.panel.hidden = false;
    this.panel.openPopup(anchor, position);
  },

  /**
   * Hide the tooltip
   */
  hide: function() {
    this.panel.hidden = true;
    this.panel.hidePopup();
  },

  /**
   * Empty the tooltip's content
   */
  empty: function() {
    while (this.panel.hasChildNodes()) {
      this.panel.removeChild(this.panel.firstChild);
    }
  },

  /**
   * Get rid of references and event listeners
   */
  destroy: function () {
    this.hide();
    this.content = null;

    this.doc = null;

    this.panel.parentNode.removeChild(this.panel);
    this.panel = null;

    if (this._basedNode) {
      this.stopTogglingOnHover();
    }
  },

  /**
   * Show/hide the tooltip when the mouse hovers over particular nodes.
   *
   * 2 Ways to make this work:
   * - Provide a single node to attach the tooltip to, as the baseNode, and
   *   omit the second targetNodeCb argument
   * - Provide a baseNode that is the container of possibly numerous children
   *   elements that may receive a tooltip. In this case, provide the second
   *   targetNodeCb argument to decide wether or not a child should receive
   *   a tooltip.
   *
   * This works by tracking mouse movements on a base container node (baseNode)
   * and showing the tooltip when the mouse stops moving. The targetNodeCb
   * callback is used to know whether or not the particular element being
   * hovered over should indeed receive the tooltip. If you don't provide it
   * it's equivalent to a function that always returns true.
   *
   * Note that if you call this function a second time, it will itself call
   * stopTogglingOnHover before adding mouse tracking listeners again.
   *
   * @param {node} baseNode
   *        The container for all target nodes
   * @param {Function} targetNodeCb
   *        A function that accepts a node argument and returns true or false
   *        to signify if the tooltip should be shown on that node or not.
   *        Additionally, the function receives a second argument which is the
   *        tooltip instance itself, to be used to add/modify the content of the
   *        tooltip if needed. If omitted, the tooltip will be shown everytime.
   * @param {Number} showDelay
   *        An optional delay that will be observed before showing the tooltip.
   *        Defaults to 750ms
   */
  startTogglingOnHover: function(baseNode, targetNodeCb, showDelay = 750) {
    if (this._basedNode) {
      this.stopTogglingOnHover();
    }

    this._basedNode = baseNode;
    this._showDelay = showDelay;
    this._targetNodeCb = targetNodeCb || (() => true);

    this._onBaseNodeMouseMove = this._onBaseNodeMouseMove.bind(this);
    this._onBaseNodeMouseLeave = this._onBaseNodeMouseLeave.bind(this);

    baseNode.addEventListener("mousemove", this._onBaseNodeMouseMove, false);
    baseNode.addEventListener("mouseleave", this._onBaseNodeMouseLeave, false);
  },

  /**
   * If the startTogglingOnHover function has been used previously, and you want
   * to get rid of this behavior, then call this function to remove the mouse
   * movement tracking
   */
  stopTogglingOnHover: function() {
    clearNamedTimeout(this.uid);

    this._basedNode.removeEventListener("mousemove",
      this._onBaseNodeMouseMove, false);
    this._basedNode.removeEventListener("mouseleave",
      this._onBaseNodeMouseLeave, false);

    this._basedNode = null;
    this._targetNodeCb = null;
    this._lastHovered = null;
  },

  _onBaseNodeMouseMove: function(event) {
    if (event.target !== this._lastHovered) {
      this.hide();
      this._lastHovered = null;
      setNamedTimeout(this.uid, this._showDelay, () => {
        this._showOnHover(event.target);
      });
    }
  },

  _showOnHover: function(target) {
    if (this._targetNodeCb && this._targetNodeCb(target, this)) {
      this.show(target);
      this._lastHovered = target;
    }
  },

  _onBaseNodeMouseLeave: function() {
    clearNamedTimeout(this.uid);
    this._lastHovered = null;
  },

  /**
   * Set the content of this tooltip. Will first empty the tooltip and then
   * append the new content element.
   * Consider using one of the set<type>Content() functions instead.
   * @param {node} content
   *        A node that can be appended in the tooltip XUL element
   */
  set content(content) {
    this.empty();
    if (content) {
      this.panel.appendChild(content);
    }
  },

  get content() {
    return this.panel.firstChild;
  },

  /**
   * Fill the tooltip with an image, displayed over a tiled background useful
   * for transparent images.
   * Also adds the image dimension as a label at the bottom.
   */
  setImageContent: function(imageUrl, maxDim=400) {
    // Main container
    let vbox = this.doc.createElement("vbox");
    vbox.setAttribute("align", "center")

    // Transparency tiles (image will go in there)
    let tiles = createTransparencyTiles(this.doc, vbox);

    // Temporary label during image load
    let label = this.doc.createElement("label");
    label.classList.add("devtools-tooltip-caption");
    label.textContent = l10n.strings.GetStringFromName("previewTooltip.image.brokenImage");
    vbox.appendChild(label);

    // Display the image
    let image = this.doc.createElement("image");
    image.setAttribute("src", imageUrl);
    if (maxDim) {
      image.style.maxWidth = maxDim + "px";
      image.style.maxHeight = maxDim + "px";
    }
    tiles.appendChild(image);

    this.content = vbox;

    // Load the image to get dimensions and display it when done
    let imgObj = new this.doc.defaultView.Image();
    imgObj.src = imageUrl;
    imgObj.onload = () => {
      imgObj.onload = null;

      // Display dimensions
      label.textContent = imgObj.naturalWidth + " x " + imgObj.naturalHeight;
      if (imgObj.naturalWidth > maxDim ||
        imgObj.naturalHeight > maxDim) {
        label.textContent += " *";
      }
    }
  },

  /**
   * Exactly the same as the `image` function but takes a css background image
   * value instead : url(....)
   */
  setCssBackgroundImageContent: function(cssBackground, sheetHref, maxDim=400) {
    let uri = getBackgroundImageUri(cssBackground, sheetHref);
    if (uri) {
      this.setImageContent(uri, maxDim);
    }
  },

  setCssGradientContent: function(cssGradient) {
    let tiles = createTransparencyTiles(this.doc);

    let gradientBox = this.doc.createElement("box");
    gradientBox.width = "100";
    gradientBox.height = "100";
    gradientBox.style.background = this.cssGradient;
    gradientBox.style.borderRadius = "2px";
    gradientBox.style.boxShadow = "inset 0 0 4px #333";

    tiles.appendChild(gradientBox)

    this.content = tiles;
  },

  _setSimpleCssPropertiesContent: function(properties, width, height) {
    let tiles = createTransparencyTiles(this.doc);

    let box = this.doc.createElement("box");
    box.width = width + "";
    box.height = height + "";
    properties.forEach(({name, value}) => {
      box.style[name] = value;
    });
    tiles.appendChild(box);

    this.content = tiles;
  },

  setCssColorContent: function(cssColor) {
    this._setSimpleCssPropertiesContent([
      {name: "background", value: cssColor},
      {name: "borderRadius", value: "2px"},
      {name: "boxShadow", value: "inset 0 0 4px #333"},
    ], 50, 50);
  },

  setCssBoxShadowContent: function(cssBoxShadow) {
    this._setSimpleCssPropertiesContent([
      {name: "background", value: "white"},
      {name: "boxShadow", value: cssBoxShadow}
    ], 80, 80);
  },

  setCssBorderContent: function(cssBorder) {
    this._setSimpleCssPropertiesContent([
      {name: "background", value: "white"},
      {name: "border", value: cssBorder}
    ], 80, 80);
  }
};

/**
 * Internal utility function that creates a tiled background useful for
 * displaying semi-transparent images
 */
function createTransparencyTiles(doc, parentEl) {
  let tiles = doc.createElement("box");
  tiles.classList.add("devtools-tooltip-tiles");
  if (parentEl) {
    parentEl.appendChild(tiles);
  }
  return tiles;
}

/**
 * Internal util, checks whether a css declaration is a gradient
 */
function isGradientRule(property, value) {
  return (property === "background" || property === "background-image") &&
    value.match(GRADIENT_RE);
}

/**
 * Internal util, checks whether a css declaration is a color
 */
function isColorOnly(property, value) {
  return property === "background-color" ||
         property === "color" ||
         property.match(BORDERCOLOR_RE);
}

/**
 * Internal util, returns the background image uri if any
 */
function getBackgroundImageUri(value, sheetHref) {
  let uriMatch = BACKGROUND_IMAGE_RE.exec(value);
  let uri = null;

  if (uriMatch && uriMatch[1]) {
    uri = uriMatch[1];
    if (sheetHref) {
      let sheetUri = IOService.newURI(sheetHref, null, null);
      uri = sheetUri.resolve(uri);
    }
  }

  return uri;
}

/**
 * L10N utility class
 */
function L10N() {}
L10N.prototype = {};

let l10n = new L10N();

loader.lazyGetter(L10N.prototype, "strings", () => {
  return Services.strings.createBundle(
    "chrome://browser/locale/devtools/inspector.properties");
});
