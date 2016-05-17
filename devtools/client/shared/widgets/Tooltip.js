/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cu, Ci} = require("chrome");
const promise = require("promise");
const {Spectrum} = require("devtools/client/shared/widgets/Spectrum");
const {CubicBezierWidget} =
      require("devtools/client/shared/widgets/CubicBezierWidget");
const {MdnDocsWidget} = require("devtools/client/shared/widgets/MdnDocsWidget");
const {CSSFilterEditorWidget} = require("devtools/client/shared/widgets/FilterWidget");
const {TooltipToggle} = require("devtools/client/shared/widgets/tooltip/TooltipToggle");
const EventEmitter = require("devtools/shared/event-emitter");
const {colorUtils} = require("devtools/client/shared/css-color");
const Heritage = require("sdk/core/heritage");
const {Eyedropper} = require("devtools/client/eyedropper/eyedropper");
const Editor = require("devtools/client/sourceeditor/editor");
const Services = require("Services");

loader.lazyRequireGetter(this, "beautify", "devtools/shared/jsbeautify/beautify");
loader.lazyRequireGetter(this, "setNamedTimeout", "devtools/client/shared/widgets/view-helpers", true);
loader.lazyRequireGetter(this, "clearNamedTimeout", "devtools/client/shared/widgets/view-helpers", true);
loader.lazyRequireGetter(this, "setNamedTimeout", "devtools/client/shared/widgets/view-helpers", true);

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "VariablesView",
  "resource://devtools/client/shared/widgets/VariablesView.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "VariablesViewController",
  "resource://devtools/client/shared/widgets/VariablesViewController.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
  "resource://gre/modules/Task.jsm");

const XHTML_NS = "http://www.w3.org/1999/xhtml";
const SPECTRUM_FRAME = "chrome://devtools/content/shared/widgets/spectrum-frame.xhtml";
const CUBIC_BEZIER_FRAME =
      "chrome://devtools/content/shared/widgets/cubic-bezier-frame.xhtml";
const MDN_DOCS_FRAME = "chrome://devtools/content/shared/widgets/mdn-docs-frame.xhtml";
const FILTER_FRAME = "chrome://devtools/content/shared/widgets/filter-frame.xhtml";
const ESCAPE_KEYCODE = Ci.nsIDOMKeyEvent.DOM_VK_ESCAPE;
const RETURN_KEYCODE = Ci.nsIDOMKeyEvent.DOM_VK_RETURN;
const POPUP_EVENTS = ["shown", "hidden", "showing", "hiding"];

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
 * Container used for dealing with optional parameters.
 *
 * @param {Object} defaults
 *        An object with all default options {p1: v1, p2: v2, ...}
 * @param {Object} options
 *        The actual values.
 */
function OptionsStore(defaults, options) {
  this.defaults = defaults || {};
  this.options = options || {};
}

OptionsStore.prototype = {
  /**
   * Get the value for a given option name.
   * @return {Object} Returns the value for that option, coming either for the
   *         actual values that have been set in the constructor, or from the
   *         defaults if that options was not specified.
   */
  get: function (name) {
    if (typeof this.options[name] !== "undefined") {
      return this.options[name];
    }
    return this.defaults[name];
  }
};

/**
 * The low level structure of a tooltip is a XUL element (a <panel>).
 */
var PanelFactory = {
  /**
   * Get a new XUL panel instance.
   * @param {XULDocument} doc
   *        The XUL document to put that panel into
   * @param {OptionsStore} options
   *        An options store to get some configuration from
   */
  get: function (doc, options) {
    // Create the tooltip
    let panel = doc.createElement("panel");
    panel.setAttribute("hidden", true);
    panel.setAttribute("ignorekeys", true);
    panel.setAttribute("animate", false);

    panel.setAttribute("consumeoutsideclicks",
                       options.get("consumeOutsideClick"));
    panel.setAttribute("noautofocus", options.get("noAutoFocus"));
    panel.setAttribute("type", "arrow");
    panel.setAttribute("level", "top");

    panel.setAttribute("class", "devtools-tooltip theme-tooltip-panel");
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
 * @param {XULDocument} doc
 *        The XUL document hosting this tooltip
 * @param {Object} options
 *        Optional options that give options to consumers:
 *        - consumeOutsideClick {Boolean} Wether the first click outside of the
 *        tooltip should close the tooltip and be consumed or not.
 *        Defaults to false.
 *        - closeOnKeys {Array} An array of key codes that should close the
 *        tooltip. Defaults to [27] (escape key).
 *        - closeOnEvents [{emitter: {Object}, event: {String},
 *                          useCapture: {Boolean}}]
 *        Provide an optional list of emitter objects and event names here to
 *        trigger the closing of the tooltip when these events are fired by the
 *        emitters. The emitter objects should either implement
 *        on/off(event, cb) or addEventListener/removeEventListener(event, cb).
 *        Defaults to [].
 *        For instance, the following would close the tooltip whenever the
 *        toolbox selects a new tool and when a DOM node gets scrolled:
 *        new Tooltip(doc, {
 *          closeOnEvents: [
 *            {emitter: toolbox, event: "select"},
 *            {emitter: myContainer, event: "scroll", useCapture: true}
 *          ]
 *        });
 *        - noAutoFocus {Boolean} Should the focus automatically go to the panel
 *        when it opens. Defaults to true.
 *
 * Fires these events:
 * - showing : just before the tooltip shows
 * - shown : when the tooltip is shown
 * - hiding : just before the tooltip closes
 * - hidden : when the tooltip gets hidden
 * - keypress : when any key gets pressed, with keyCode
 */
function Tooltip(doc, options) {
  EventEmitter.decorate(this);

  this.doc = doc;
  this.options = new OptionsStore({
    consumeOutsideClick: false,
    closeOnKeys: [ESCAPE_KEYCODE],
    noAutoFocus: true,
    closeOnEvents: []
  }, options);
  this.panel = PanelFactory.get(doc, this.options);

  // Create tooltip toggle helper and decorate the Tooltip instance with
  // shortcut methods.
  this._toggle = new TooltipToggle(this);
  this.startTogglingOnHover = this._toggle.start.bind(this._toggle);
  this.stopTogglingOnHover = this._toggle.stop.bind(this._toggle);

  // Emit show/hide events when the panel does.
  for (let eventName of POPUP_EVENTS) {
    this["_onPopup" + eventName] = (name => {
      return e => {
        if (e.target === this.panel) {
          this.emit(name);
        }
      };
    })(eventName);
    this.panel.addEventListener("popup" + eventName,
      this["_onPopup" + eventName], false);
  }

  // Listen to keypress events to close the tooltip if configured to do so
  let win = this.doc.querySelector("window");
  this._onKeyPress = event => {
    if (this.panel.hidden) {
      return;
    }

    this.emit("keypress", event.keyCode);
    if (this.options.get("closeOnKeys").indexOf(event.keyCode) !== -1 &&
        this.isShown()) {
      event.stopPropagation();
      this.hide();
    }
  };
  win.addEventListener("keypress", this._onKeyPress, false);

  // Listen to custom emitters' events to close the tooltip
  this.hide = this.hide.bind(this);
  let closeOnEvents = this.options.get("closeOnEvents");
  for (let {emitter, event, useCapture} of closeOnEvents) {
    for (let add of ["addEventListener", "on"]) {
      if (add in emitter) {
        emitter[add](event, this.hide, useCapture);
        break;
      }
    }
  }
}

module.exports.Tooltip = Tooltip;

Tooltip.prototype = {
  defaultPosition: "before_start",
  // px
  defaultOffsetX: 0,
  // px
  defaultOffsetY: 0,
  // px

  /**
   * Show the tooltip. It might be wise to append some content first if you
   * don't want the tooltip to be empty. You may access the content of the
   * tooltip by setting a XUL node to t.content.
   * @param {node} anchor
   *        Which node should the tooltip be shown on
   * @param {string} position [optional]
   *        Optional tooltip position. Defaults to before_start
   *        https://developer.mozilla.org/en-US/docs/XUL/PopupGuide/Positioning
   * @param {number} x, y [optional]
   *        The left and top offset coordinates, in pixels.
   */
  show: function (anchor,
    position = this.defaultPosition,
    x = this.defaultOffsetX,
    y = this.defaultOffsetY) {
    this.panel.hidden = false;
    this.panel.openPopup(anchor, position, x, y);
  },

  /**
   * Hide the tooltip
   */
  hide: function () {
    this.panel.hidden = true;
    this.panel.hidePopup();
  },

  isShown: function () {
    return this.panel &&
           this.panel.state !== "closed" &&
           this.panel.state !== "hiding";
  },

  setSize: function (width, height) {
    this.panel.sizeTo(width, height);
  },

  /**
   * Empty the tooltip's content
   */
  empty: function () {
    while (this.panel.hasChildNodes()) {
      this.panel.removeChild(this.panel.firstChild);
    }
  },

  /**
   * Gets this panel's visibility state.
   * @return boolean
   */
  isHidden: function () {
    return this.panel.state == "closed" || this.panel.state == "hiding";
  },

  /**
   * Gets if this panel has any child nodes.
   * @return boolean
   */
  isEmpty: function () {
    return !this.panel.hasChildNodes();
  },

  /**
   * Get rid of references and event listeners
   */
  destroy: function () {
    this.hide();

    for (let eventName of POPUP_EVENTS) {
      this.panel.removeEventListener("popup" + eventName,
        this["_onPopup" + eventName], false);
    }

    let win = this.doc.querySelector("window");
    win.removeEventListener("keypress", this._onKeyPress, false);

    let closeOnEvents = this.options.get("closeOnEvents");
    for (let {emitter, event, useCapture} of closeOnEvents) {
      for (let remove of ["removeEventListener", "off"]) {
        if (remove in emitter) {
          emitter[remove](event, this.hide, useCapture);
          break;
        }
      }
    }

    this.content = null;

    this._toggle.destroy();

    this.doc = null;

    this.panel.remove();
    this.panel = null;
  },

  /**
   * Set the content of this tooltip. Will first empty the tooltip and then
   * append the new content element.
   * Consider using one of the set<type>Content() functions instead.
   * @param {node} content
   *        A node that can be appended in the tooltip XUL element
   */
  set content(content) {
    if (this.content == content) {
      return;
    }

    this.empty();
    this.panel.removeAttribute("clamped-dimensions");
    this.panel.removeAttribute("clamped-dimensions-no-min-height");
    this.panel.removeAttribute("clamped-dimensions-no-max-or-min-height");
    this.panel.removeAttribute("wide");

    if (content) {
      this.panel.appendChild(content);
    }
  },

  get content() {
    return this.panel.firstChild;
  },

  /**
   * Sets some text as the content of this tooltip.
   *
   * @param {array} messages
   *        A list of text messages.
   * @param {string} messagesClass [optional]
   *        A style class for the text messages.
   * @param {string} containerClass [optional]
   *        A style class for the text messages container.
   * @param {boolean} isAlertTooltip [optional]
   *        Pass true to add an alert image for your tooltip.
   */
  setTextContent: function (
    {
      messages,
      messagesClass,
      containerClass,
      isAlertTooltip
    },
    extraButtons = []) {
    messagesClass = messagesClass || "default-tooltip-simple-text-colors";
    containerClass = containerClass || "default-tooltip-simple-text-colors";

    let vbox = this.doc.createElement("vbox");
    vbox.className = "devtools-tooltip-simple-text-container " + containerClass;
    vbox.setAttribute("flex", "1");

    for (let text of messages) {
      let description = this.doc.createElement("description");
      description.setAttribute("flex", "1");
      description.className = "devtools-tooltip-simple-text " + messagesClass;
      description.textContent = text;
      vbox.appendChild(description);
    }

    for (let { label, className, command } of extraButtons) {
      let button = this.doc.createElement("button");
      button.className = className;
      button.setAttribute("label", label);
      button.addEventListener("command", command);
      vbox.appendChild(button);
    }

    if (isAlertTooltip) {
      let hbox = this.doc.createElement("hbox");
      hbox.setAttribute("align", "start");

      let alertImg = this.doc.createElement("image");
      alertImg.className = "devtools-tooltip-alert-icon";
      hbox.appendChild(alertImg);
      hbox.appendChild(vbox);
      this.content = hbox;
    } else {
      this.content = vbox;
    }
  },

  /**
   * Sets some event listener info as the content of this tooltip.
   *
   * @param {Object} (destructuring assignment)
   *          @0 {array} eventListenerInfos
   *             A list of event listeners.
   *          @1 {toolbox} toolbox
   *             Toolbox used to select debugger panel.
   */
  setEventContent: function ({ eventListenerInfos, toolbox }) {
    new EventTooltip(this, eventListenerInfos, toolbox);
  },

  /**
   * Fill the tooltip with a variables view, inspecting an object via its
   * corresponding object actor, as specified in the remote debugging protocol.
   *
   * @param {object} objectActor
   *        The value grip for the object actor.
   * @param {object} viewOptions [optional]
   *        Options for the variables view visualization.
   * @param {object} controllerOptions [optional]
   *        Options for the variables view controller.
   * @param {object} relayEvents [optional]
   *        A collection of events to listen on the variables view widget.
   *        For example, { fetched: () => ... }
   * @param {boolean} reuseCachedWidget [optional]
   *        Pass false to instantiate a brand new widget for this variable.
   *        Otherwise, if a variable was previously inspected, its widget
   *        will be reused.
   * @param {Toolbox} toolbox [optional]
   *        Pass the instance of the current toolbox if you want the variables
   *        view widget to allow highlighting and selection of DOM nodes
   */
  setVariableContent: function (objectActor,
                               viewOptions = {},
                               controllerOptions = {},
                               relayEvents = {},
                               extraButtons = [],
                               toolbox = null) {
    let vbox = this.doc.createElement("vbox");
    vbox.className = "devtools-tooltip-variables-view-box";
    vbox.setAttribute("flex", "1");

    let innerbox = this.doc.createElement("vbox");
    innerbox.className = "devtools-tooltip-variables-view-innerbox";
    innerbox.setAttribute("flex", "1");
    vbox.appendChild(innerbox);

    for (let { label, className, command } of extraButtons) {
      let button = this.doc.createElement("button");
      button.className = className;
      button.setAttribute("label", label);
      button.addEventListener("command", command);
      vbox.appendChild(button);
    }

    let widget = new VariablesView(innerbox, viewOptions);

    // If a toolbox was provided, link it to the vview
    if (toolbox) {
      widget.toolbox = toolbox;
    }

    // Analyzing state history isn't useful with transient object inspectors.
    widget.commitHierarchy = () => {};

    for (let e in relayEvents) widget.on(e, relayEvents[e]);
    VariablesViewController.attach(widget, controllerOptions);

    // Some of the view options are allowed to change between uses.
    widget.searchPlaceholder = viewOptions.searchPlaceholder;
    widget.searchEnabled = viewOptions.searchEnabled;

    // Use the object actor's grip to display it as a variable in the widget.
    // The controller options are allowed to change between uses.
    widget.controller.setSingleVariable(
      { objectActor: objectActor }, controllerOptions);

    this.content = vbox;
    this.panel.setAttribute("clamped-dimensions", "");
  },

  /**
   * Uses the provided inspectorFront's getImageDataFromURL method to resolve
   * the relative URL on the server-side, in the page context, and then sets the
   * tooltip content with the resulting image just like |setImageContent| does.
   * @return a promise that resolves when the image is shown in the tooltip or
   * resolves when the broken image tooltip content is ready, but never rejects.
   */
  setRelativeImageContent: Task.async(function* (imageUrl, inspectorFront,
                                                maxDim) {
    if (imageUrl.startsWith("data:")) {
      // If the imageUrl already is a data-url, save ourselves a round-trip
      this.setImageContent(imageUrl, {maxDim: maxDim});
    } else if (inspectorFront) {
      try {
        let {data, size} = yield inspectorFront.getImageDataFromURL(imageUrl,
                                                                    maxDim);
        size.maxDim = maxDim;
        let str = yield data.string();
        this.setImageContent(str, size);
      } catch (e) {
        this.setBrokenImageContent();
      }
    }
  }),

  /**
   * Fill the tooltip with a message explaining the the image is missing
   */
  setBrokenImageContent: function () {
    this.setTextContent({
      messages: [l10n.strings.GetStringFromName("previewTooltip.image.brokenImage")]
    });
  },

  /**
   * Fill the tooltip with an image and add the image dimension at the bottom.
   *
   * Only use this for absolute URLs that can be queried from the devtools
   * client-side. For relative URLs, use |setRelativeImageContent|.
   *
   * @param {string} imageUrl
   *        The url to load the image from
   * @param {Object} options
   *        The following options are supported:
   *        - resized : whether or not the image identified by imageUrl has been
   *        resized before this function was called.
   *        - naturalWidth/naturalHeight : the original size of the image before
   *        it was resized, if if was resized before this function was called.
   *        If not provided, will be measured on the loaded image.
   *        - maxDim : if the image should be resized before being shown, pass
   *        a number here.
   *        - hideDimensionLabel : if the dimension label should be appended
   *        after the image.
   */
  setImageContent: function (imageUrl, options = {}) {
    if (!imageUrl) {
      return;
    }

    // Main container
    let vbox = this.doc.createElement("vbox");
    vbox.setAttribute("align", "center");

    // Display the image
    let image = this.doc.createElement("image");
    image.setAttribute("src", imageUrl);
    if (options.maxDim) {
      image.style.maxWidth = options.maxDim + "px";
      image.style.maxHeight = options.maxDim + "px";
    }
    vbox.appendChild(image);

    if (!options.hideDimensionLabel) {
      let label = this.doc.createElement("label");
      label.classList.add("devtools-tooltip-caption");
      label.classList.add("theme-comment");

      if (options.naturalWidth && options.naturalHeight) {
        label.textContent = this._getImageDimensionLabel(options.naturalWidth,
          options.naturalHeight);
      } else {
        // If no dimensions were provided, load the image to get them
        label.textContent =
          l10n.strings.GetStringFromName("previewTooltip.image.brokenImage");
        let imgObj = new this.doc.defaultView.Image();
        imgObj.src = imageUrl;
        imgObj.onload = () => {
          imgObj.onload = null;
          label.textContent = this._getImageDimensionLabel(imgObj.naturalWidth,
              imgObj.naturalHeight);
        };
      }

      vbox.appendChild(label);
    }

    this.content = vbox;
  },

  _getImageDimensionLabel: (w, h) => w + " \u00D7 " + h,

  /**
   * Load a document into an iframe, and set the iframe
   * to be the tooltip's content.
   *
   * Used by tooltips that want to load their interface
   * into an iframe from a URL.
   *
   * @param {string} width
   *        Width of the iframe.
   * @param {string} height
   *        Height of the iframe.
   * @param {string} url
   *        URL of the document to load into the iframe.
   *
   * @return {promise} A promise which is resolved with
   * the iframe.
   *
   * This function creates an iframe, loads the specified document
   * into it, sets the tooltip's content to the iframe, and returns
   * a promise.
   *
   * When the document is loaded, the function gets the content window
   * and resolves the promise with the content window.
   */
  setIFrameContent: function ({width, height}, url) {
    let def = promise.defer();

    // Create an iframe
    let iframe = this.doc.createElementNS(XHTML_NS, "iframe");
    iframe.setAttribute("transparent", true);
    iframe.setAttribute("width", width);
    iframe.setAttribute("height", height);
    iframe.setAttribute("flex", "1");
    iframe.setAttribute("tooltip", "aHTMLTooltip");
    iframe.setAttribute("class", "devtools-tooltip-iframe");

    // Wait for the load to initialize the widget
    function onLoad() {
      iframe.removeEventListener("load", onLoad, true);
      def.resolve(iframe);
    }
    iframe.addEventListener("load", onLoad, true);

    // load the document from url into the iframe
    iframe.setAttribute("src", url);

    // Put the iframe in the tooltip
    this.content = iframe;

    return def.promise;
  },

  /**
   * Fill the tooltip with a new instance of the spectrum color picker widget
   * initialized with the given color, and return a promise that resolves to
   * the instance of spectrum
   */
  setColorPickerContent: function (color) {
    let dimensions = {width: "210", height: "216"};
    let panel = this.panel;
    return this.setIFrameContent(dimensions, SPECTRUM_FRAME).then(onLoaded);

    function onLoaded(iframe) {
      let win = iframe.contentWindow.wrappedJSObject;
      let def = promise.defer();
      let container = win.document.getElementById("spectrum");
      let spectrum = new Spectrum(container, color);

      function finalizeSpectrum() {
        spectrum.show();
        def.resolve(spectrum);
      }

      // Finalize spectrum's init when the tooltip becomes visible
      if (panel.state == "open") {
        finalizeSpectrum();
      } else {
        panel.addEventListener("popupshown", function shown() {
          panel.removeEventListener("popupshown", shown, true);
          finalizeSpectrum();
        }, true);
      }
      return def.promise;
    }
  },

  /**
   * Fill the tooltip with a new instance of the cubic-bezier widget
   * initialized with the given value, and return a promise that resolves to
   * the instance of the widget
   */
  setCubicBezierContent: function (bezier) {
    let dimensions = {width: "500", height: "360"};
    let panel = this.panel;
    return this.setIFrameContent(dimensions, CUBIC_BEZIER_FRAME).then(onLoaded);

    function onLoaded(iframe) {
      let win = iframe.contentWindow.wrappedJSObject;
      let def = promise.defer();
      let container = win.document.getElementById("container");
      let widget = new CubicBezierWidget(container, bezier);

      // Resolve to the widget instance whenever the popup becomes visible
      if (panel.state == "open") {
        def.resolve(widget);
      } else {
        panel.addEventListener("popupshown", function shown() {
          panel.removeEventListener("popupshown", shown, true);
          def.resolve(widget);
        }, true);
      }
      return def.promise;
    }
  },

  /**
   * Fill the tooltip with a new instance of the CSSFilterEditorWidget
   * widget initialized with the given filter value, and return a promise
   * that resolves to the instance of the widget when ready.
   */
  setFilterContent: function (filter) {
    let dimensions = {width: "500", height: "200"};
    let panel = this.panel;

    return this.setIFrameContent(dimensions, FILTER_FRAME).then(onLoaded);

    function onLoaded(iframe) {
      let win = iframe.contentWindow.wrappedJSObject;
      let doc = win.document.documentElement;
      let def = promise.defer();
      let container = win.document.getElementById("container");
      let widget = new CSSFilterEditorWidget(container, filter);

      // Resolve to the widget instance whenever the popup becomes visible
      if (panel.state === "open") {
        def.resolve(widget);
      } else {
        panel.addEventListener("popupshown", function shown() {
          panel.removeEventListener("popupshown", shown, true);
          def.resolve(widget);
        }, true);
      }
      return def.promise;
    }
  },

  /**
   * Set the content of the tooltip to display a font family preview.
   * This is based on Lea Verou's Dablet.
   * See https://github.com/LeaVerou/dabblet
   * for more info.
   * @param {String} font The font family value.
   * @param {object} nodeFront
   *        The NodeActor that will used to retrieve the dataURL for the font
   *        family tooltip contents.
   * @return A promise that resolves when the font tooltip content is ready, or
   *         rejects if no font is provided
   */
  setFontFamilyContent: Task.async(function* (font, nodeFront) {
    if (!font || !nodeFront) {
      throw new Error("Missing font");
    }

    if (typeof nodeFront.getFontFamilyDataURL === "function") {
      font = font.replace(/"/g, "'");
      font = font.replace("!important", "");
      font = font.trim();

      let fillStyle =
          (Services.prefs.getCharPref("devtools.theme") === "light") ?
          "black" : "white";

      let {data, size} = yield nodeFront.getFontFamilyDataURL(font, fillStyle);
      let str = yield data.string();
      this.setImageContent(str, { hideDimensionLabel: true, maxDim: size });
    }
  }),

  /**
   * Set the content of this tooltip to the MDN docs widget.
   *
   * This is called when the tooltip is first constructed.
   *
   * @return {promise} A promise which is resolved with an MdnDocsWidget.
   *
   * It loads the tooltip's structure from a separate XHTML file
   * into an iframe. When the iframe is loaded it constructs
   * an MdnDocsWidget and passes that into resolve.
   *
   * The caller can use the MdnDocsWidget to update the tooltip's
   * UI with new content each time the tooltip is shown.
   */
  setMdnDocsContent: function () {
    let dimensions = {width: "410", height: "300"};
    return this.setIFrameContent(dimensions, MDN_DOCS_FRAME).then(onLoaded);

    function onLoaded(iframe) {
      let win = iframe.contentWindow.wrappedJSObject;
      // create an MdnDocsWidget, initializing it with the content document
      let widget = new MdnDocsWidget(win.document);
      return widget;
    }
  }
};

/**
 * Base class for all (color, gradient, ...)-swatch based value editors inside
 * tooltips
 *
 * @param {XULDocument} doc
 */
function SwatchBasedEditorTooltip(doc) {
  // Creating a tooltip instance
  // This one will consume outside clicks as it makes more sense to let the user
  // close the tooltip by clicking out
  // It will also close on <escape> and <enter>
  this.tooltip = new Tooltip(doc, {
    consumeOutsideClick: true,
    closeOnKeys: [ESCAPE_KEYCODE, RETURN_KEYCODE],
    noAutoFocus: false
  });

  // By default, swatch-based editor tooltips revert value change on <esc> and
  // commit value change on <enter>
  this._onTooltipKeypress = (event, code) => {
    if (code === ESCAPE_KEYCODE) {
      this.revert();
    } else if (code === RETURN_KEYCODE) {
      this.commit();
    }
  };
  this.tooltip.on("keypress", this._onTooltipKeypress);

  // All target swatches are kept in a map, indexed by swatch DOM elements
  this.swatches = new Map();

  // When a swatch is clicked, and for as long as the tooltip is shown, the
  // activeSwatch property will hold the reference to the swatch DOM element
  // that was clicked
  this.activeSwatch = null;

  this._onSwatchClick = this._onSwatchClick.bind(this);
}

SwatchBasedEditorTooltip.prototype = {
  show: function () {
    if (this.activeSwatch) {
      this.tooltip.show(this.activeSwatch, "topcenter bottomleft");

      // When the tooltip is closed by clicking outside the panel we want to
      // commit any changes. Because the "hidden" event destroys the tooltip we
      // need to do this before the tooltip is destroyed (in the "hiding"
      // event).
      this.tooltip.once("hiding", () => {
        if (!this._reverted && !this.eyedropperOpen) {
          this.commit();
        }
        this._reverted = false;
      });

      // Once the tooltip is hidden we need to clean up any remaining objects.
      this.tooltip.once("hidden", () => {
        if (!this.eyedropperOpen) {
          this.activeSwatch = null;
        }
      });
    }
  },

  hide: function () {
    this.tooltip.hide();
  },

  /**
   * Add a new swatch DOM element to the list of swatch elements this editor
   * tooltip knows about. That means from now on, clicking on that swatch will
   * toggle the editor.
   *
   * @param {node} swatchEl
   *        The element to add
   * @param {object} callbacks
   *        Callbacks that will be executed when the editor wants to preview a
   *        value change, or revert a change, or commit a change.
   *        - onShow: will be called when one of the swatch tooltip is shown
   *        - onPreview: will be called when one of the sub-classes calls
   *        preview
   *        - onRevert: will be called when the user ESCapes out of the tooltip
   *        - onCommit: will be called when the user presses ENTER or clicks
   *        outside the tooltip.
   */
  addSwatch: function (swatchEl, callbacks = {}) {
    if (!callbacks.onShow) {
      callbacks.onShow = function () {};
    }
    if (!callbacks.onPreview) {
      callbacks.onPreview = function () {};
    }
    if (!callbacks.onRevert) {
      callbacks.onRevert = function () {};
    }
    if (!callbacks.onCommit) {
      callbacks.onCommit = function () {};
    }

    this.swatches.set(swatchEl, {
      callbacks: callbacks
    });
    swatchEl.addEventListener("click", this._onSwatchClick, false);
  },

  removeSwatch: function (swatchEl) {
    if (this.swatches.has(swatchEl)) {
      if (this.activeSwatch === swatchEl) {
        this.hide();
        this.activeSwatch = null;
      }
      swatchEl.removeEventListener("click", this._onSwatchClick, false);
      this.swatches.delete(swatchEl);
    }
  },

  _onSwatchClick: function (event) {
    let swatch = this.swatches.get(event.target);

    if (event.shiftKey) {
      event.stopPropagation();
      return;
    }
    if (swatch) {
      this.activeSwatch = event.target;
      this.show();
      swatch.callbacks.onShow();
      event.stopPropagation();
    }
  },

  /**
   * Not called by this parent class, needs to be taken care of by sub-classes
   */
  preview: function (value) {
    if (this.activeSwatch) {
      let swatch = this.swatches.get(this.activeSwatch);
      swatch.callbacks.onPreview(value);
    }
  },

  /**
   * This parent class only calls this on <esc> keypress
   */
  revert: function () {
    if (this.activeSwatch) {
      this._reverted = true;
      let swatch = this.swatches.get(this.activeSwatch);
      this.tooltip.once("hiding", () => {
        swatch.callbacks.onRevert();
      });
    }
  },

  /**
   * This parent class only calls this on <enter> keypress
   */
  commit: function () {
    if (this.activeSwatch) {
      let swatch = this.swatches.get(this.activeSwatch);
      swatch.callbacks.onCommit();
    }
  },

  destroy: function () {
    this.swatches.clear();
    this.activeSwatch = null;
    this.tooltip.off("keypress", this._onTooltipKeypress);
    this.tooltip.destroy();
  }
};

/**
 * The swatch color picker tooltip class is a specific class meant to be used
 * along with output-parser's generated color swatches.
 * It extends the parent SwatchBasedEditorTooltip class.
 * It just wraps a standard Tooltip and sets its content with an instance of a
 * color picker.
 *
 * @param {XULDocument} doc
 */
function SwatchColorPickerTooltip(doc) {
  SwatchBasedEditorTooltip.call(this, doc);

  // Creating a spectrum instance. this.spectrum will always be a promise that
  // resolves to the spectrum instance
  this.spectrum = this.tooltip.setColorPickerContent([0, 0, 0, 1]);
  this._onSpectrumColorChange = this._onSpectrumColorChange.bind(this);
  this._openEyeDropper = this._openEyeDropper.bind(this);
}

module.exports.SwatchColorPickerTooltip = SwatchColorPickerTooltip;

SwatchColorPickerTooltip.prototype = Heritage.extend(SwatchBasedEditorTooltip.prototype, {
  /**
   * Overriding the SwatchBasedEditorTooltip.show function to set spectrum's
   * color.
   */
  show: function () {
    // Call then parent class' show function
    SwatchBasedEditorTooltip.prototype.show.call(this);
    // Then set spectrum's color and listen to color changes to preview them
    if (this.activeSwatch) {
      this.currentSwatchColor = this.activeSwatch.nextSibling;
      this._originalColor = this.currentSwatchColor.textContent;
      let color = this.activeSwatch.style.backgroundColor;
      this.spectrum.then(spectrum => {
        spectrum.off("changed", this._onSpectrumColorChange);
        spectrum.rgb = this._colorToRgba(color);
        spectrum.on("changed", this._onSpectrumColorChange);
        spectrum.updateUI();
      });
    }

    let tooltipDoc = this.tooltip.content.contentDocument;
    let eyeButton = tooltipDoc.querySelector("#eyedropper-button");
    eyeButton.addEventListener("click", this._openEyeDropper);
  },

  _onSpectrumColorChange: function (event, rgba, cssColor) {
    this._selectColor(cssColor);
  },

  _selectColor: function (color) {
    if (this.activeSwatch) {
      this.activeSwatch.style.backgroundColor = color;
      this.activeSwatch.parentNode.dataset.color = color;

      color = this._toDefaultType(color);
      this.currentSwatchColor.textContent = color;
      this.preview(color);

      if (this.eyedropperOpen) {
        this.commit();
      }
    }
  },

  _openEyeDropper: function () {
    let chromeWindow = this.tooltip.doc.defaultView.top;
    let windowType = chromeWindow.document.documentElement
                     .getAttribute("windowtype");
    let toolboxWindow;
    if (windowType != "navigator:browser") {
      // this means the toolbox is in a seperate window. We need to make
      // sure we'll be inspecting the browser window instead
      toolboxWindow = chromeWindow;
      chromeWindow = Services.wm.getMostRecentWindow("navigator:browser");
      chromeWindow.focus();
    }
    let dropper = new Eyedropper(chromeWindow, { copyOnSelect: false,
                                                 context: "picker" });

    dropper.once("select", (event, color) => {
      if (toolboxWindow) {
        toolboxWindow.focus();
      }
      this._selectColor(color);
    });

    dropper.once("destroy", () => {
      this.eyedropperOpen = false;
      this.activeSwatch = null;
    });

    dropper.open();
    this.eyedropperOpen = true;

    // close the colorpicker tooltip so that only the eyedropper is open.
    this.hide();

    this.tooltip.emit("eyedropper-opened", dropper);
  },

  _colorToRgba: function (color) {
    color = new colorUtils.CssColor(color);
    let rgba = color._getRGBATuple();
    return [rgba.r, rgba.g, rgba.b, rgba.a];
  },

  _toDefaultType: function (color) {
    let colorObj = new colorUtils.CssColor(color);
    colorObj.setAuthoredUnitFromColor(this._originalColor);
    return colorObj.toString();
  },

  destroy: function () {
    SwatchBasedEditorTooltip.prototype.destroy.call(this);
    this.currentSwatchColor = null;
    this.spectrum.then(spectrum => {
      spectrum.off("changed", this._onSpectrumColorChange);
      spectrum.destroy();
    });
  }
});

function EventTooltip(tooltip, eventListenerInfos, toolbox) {
  this._tooltip = tooltip;
  this._eventListenerInfos = eventListenerInfos;
  this._toolbox = toolbox;
  this._tooltip.eventEditors = new WeakMap();

  this._headerClicked = this._headerClicked.bind(this);
  this._debugClicked = this._debugClicked.bind(this);
  this.destroy = this.destroy.bind(this);

  this._init();
}

EventTooltip.prototype = {
  _init: function () {
    let config = {
      mode: Editor.modes.js,
      lineNumbers: false,
      lineWrapping: false,
      readOnly: true,
      styleActiveLine: true,
      extraKeys: {},
      theme: "mozilla markup-view"
    };

    let doc = this._tooltip.doc;
    let container = doc.createElement("vbox");
    container.setAttribute("id", "devtools-tooltip-events-container");

    for (let listener of this._eventListenerInfos) {
      let phase = listener.capturing ? "Capturing" : "Bubbling";
      let level = listener.DOM0 ? "DOM0" : "DOM2";

      // Header
      let header = doc.createElement("hbox");
      header.className = "event-header devtools-toolbar";
      container.appendChild(header);

      if (!listener.hide.debugger) {
        let debuggerIcon = doc.createElement("image");
        debuggerIcon.className = "event-tooltip-debugger-icon";
        debuggerIcon.setAttribute("src", "chrome://devtools/skin/images/tool-debugger.svg");
        let openInDebugger =
            l10n.strings.GetStringFromName("eventsTooltip.openInDebugger");
        debuggerIcon.setAttribute("tooltiptext", openInDebugger);
        header.appendChild(debuggerIcon);
      }

      if (!listener.hide.type) {
        let eventTypeLabel = doc.createElement("label");
        eventTypeLabel.className = "event-tooltip-event-type";
        eventTypeLabel.setAttribute("value", listener.type);
        eventTypeLabel.setAttribute("tooltiptext", listener.type);
        header.appendChild(eventTypeLabel);
      }

      if (!listener.hide.filename) {
        let filename = doc.createElement("label");
        filename.className = "event-tooltip-filename devtools-monospace";
        filename.setAttribute("value", listener.origin);
        filename.setAttribute("tooltiptext", listener.origin);
        filename.setAttribute("crop", "left");
        header.appendChild(filename);
      }

      let attributesContainer = doc.createElement("hbox");
      attributesContainer.setAttribute("class",
                                       "event-tooltip-attributes-container");
      header.appendChild(attributesContainer);

      if (!listener.hide.capturing) {
        let attributesBox = doc.createElement("box");
        attributesBox.setAttribute("class", "event-tooltip-attributes-box");
        attributesContainer.appendChild(attributesBox);

        let capturing = doc.createElement("label");
        capturing.className = "event-tooltip-attributes";
        capturing.setAttribute("value", phase);
        capturing.setAttribute("tooltiptext", phase);
        attributesBox.appendChild(capturing);
      }

      if (listener.tags) {
        for (let tag of listener.tags.split(",")) {
          let attributesBox = doc.createElement("box");
          attributesBox.setAttribute("class", "event-tooltip-attributes-box");
          attributesContainer.appendChild(attributesBox);

          let tagBox = doc.createElement("label");
          tagBox.className = "event-tooltip-attributes";
          tagBox.setAttribute("value", tag);
          tagBox.setAttribute("tooltiptext", tag);
          attributesBox.appendChild(tagBox);
        }
      }

      if (!listener.hide.dom0) {
        let attributesBox = doc.createElement("box");
        attributesBox.setAttribute("class", "event-tooltip-attributes-box");
        attributesContainer.appendChild(attributesBox);

        let dom0 = doc.createElement("label");
        dom0.className = "event-tooltip-attributes";
        dom0.setAttribute("value", level);
        dom0.setAttribute("tooltiptext", level);
        attributesBox.appendChild(dom0);
      }

      // Content
      let content = doc.createElement("box");
      let editor = new Editor(config);
      this._tooltip.eventEditors.set(content, {
        editor: editor,
        handler: listener.handler,
        searchString: listener.searchString,
        uri: listener.origin,
        dom0: listener.DOM0,
        appended: false
      });

      content.className = "event-tooltip-content-box";
      container.appendChild(content);

      this._addContentListeners(header);
    }

    this._tooltip.content = container;
    this._tooltip.panel.setAttribute("clamped-dimensions-no-max-or-min-height",
                                     "");
    this._tooltip.panel.setAttribute("wide", "");

    this._tooltip.panel.addEventListener("popuphiding", () => {
      this.destroy(container);
    }, false);
  },

  _addContentListeners: function (header) {
    header.addEventListener("click", this._headerClicked);
  },

  _headerClicked: function (event) {
    if (event.target.classList.contains("event-tooltip-debugger-icon")) {
      this._debugClicked(event);
      event.stopPropagation();
      return;
    }

    let doc = this._tooltip.doc;
    let header = event.currentTarget;
    let content = header.nextElementSibling;

    if (content.hasAttribute("open")) {
      content.removeAttribute("open");
    } else {
      let contentNodes = doc.querySelectorAll(".event-tooltip-content-box");

      for (let node of contentNodes) {
        if (node !== content) {
          node.removeAttribute("open");
        }
      }

      content.setAttribute("open", "");

      let eventEditors = this._tooltip.eventEditors.get(content);

      if (eventEditors.appended) {
        return;
      }

      let {editor, handler} = eventEditors;

      let iframe = doc.createElement("iframe");
      iframe.setAttribute("style", "width:100%;");

      editor.appendTo(content, iframe).then(() => {
        let tidied = beautify.js(handler, { indent_size: 2 });

        editor.setText(tidied);

        eventEditors.appended = true;

        let container = header.parentElement.getBoundingClientRect();
        if (header.getBoundingClientRect().top < container.top) {
          header.scrollIntoView(true);
        } else if (content.getBoundingClientRect().bottom > container.bottom) {
          content.scrollIntoView(false);
        }

        this._tooltip.emit("event-tooltip-ready");
      });
    }
  },

  _debugClicked: function (event) {
    let header = event.currentTarget;
    let content = header.nextElementSibling;

    let {uri, searchString, dom0} =
      this._tooltip.eventEditors.get(content);

    if (uri && uri !== "?") {
      // Save a copy of toolbox as it will be set to null when we hide the
      // tooltip.
      let toolbox = this._toolbox;

      this._tooltip.hide();

      uri = uri.replace(/"/g, "");

      let showSource = ({ DebuggerView }) => {
        let matches = uri.match(/(.*):(\d+$)/);
        let line = 1;

        if (matches) {
          uri = matches[1];
          line = matches[2];
        }

        let item = DebuggerView.Sources.getItemForAttachment(
          a => a.source.url === uri
        );
        if (item) {
          let actor = item.attachment.source.actor;
          DebuggerView.setEditorLocation(actor, line, {noDebug: true}).then(() => {
            if (dom0) {
              let text = DebuggerView.editor.getText();
              let index = text.indexOf(searchString);
              let lastIndex = text.lastIndexOf(searchString);

              // To avoid confusion we only search for DOM0 event handlers when
              // there is only one possible match in the file.
              if (index !== -1 && index === lastIndex) {
                text = text.substr(0, index);
                let newlineMatches = text.match(/\n/g);

                if (newlineMatches) {
                  DebuggerView.editor.setCursor({
                    line: newlineMatches.length
                  });
                }
              }
            }
          });
        }
      };

      let debuggerAlreadyOpen = toolbox.getPanel("jsdebugger");
      toolbox.selectTool("jsdebugger").then(({ panelWin: dbg }) => {
        if (debuggerAlreadyOpen) {
          showSource(dbg);
        } else {
          dbg.once(dbg.EVENTS.SOURCES_ADDED, () => showSource(dbg));
        }
      });
    }
  },

  destroy: function (container) {
    if (this._tooltip) {
      this._tooltip.panel.removeEventListener("popuphiding", this.destroy,
                                              false);

      let boxes = container.querySelectorAll(".event-tooltip-content-box");

      for (let box of boxes) {
        let {editor} = this._tooltip.eventEditors.get(box);
        editor.destroy();
      }

      this._tooltip.eventEditors = null;
    }

    let headerNodes = container.querySelectorAll(".event-header");

    for (let node of headerNodes) {
      node.removeEventListener("click", this._headerClicked);
    }

    let sourceNodes =
        container.querySelectorAll(".event-tooltip-debugger-icon");
    for (let node of sourceNodes) {
      node.removeEventListener("click", this._debugClicked);
    }

    this._eventListenerInfos = this._toolbox = this._tooltip = null;
  }
};

/**
 * The swatch cubic-bezier tooltip class is a specific class meant to be used
 * along with rule-view's generated cubic-bezier swatches.
 * It extends the parent SwatchBasedEditorTooltip class.
 * It just wraps a standard Tooltip and sets its content with an instance of a
 * CubicBezierWidget.
 *
 * @param {XULDocument} doc
 */
function SwatchCubicBezierTooltip(doc) {
  SwatchBasedEditorTooltip.call(this, doc);

  // Creating a cubic-bezier instance.
  // this.widget will always be a promise that resolves to the widget instance
  this.widget = this.tooltip.setCubicBezierContent([0, 0, 1, 1]);
  this._onUpdate = this._onUpdate.bind(this);
}

module.exports.SwatchCubicBezierTooltip = SwatchCubicBezierTooltip;

SwatchCubicBezierTooltip.prototype = Heritage.extend(SwatchBasedEditorTooltip.prototype, {
  /**
   * Overriding the SwatchBasedEditorTooltip.show function to set the cubic
   * bezier curve in the widget
   */
  show: function () {
    // Call the parent class' show function
    SwatchBasedEditorTooltip.prototype.show.call(this);
    // Then set the curve and listen to changes to preview them
    if (this.activeSwatch) {
      this.currentBezierValue = this.activeSwatch.nextSibling;
      this.widget.then(widget => {
        widget.off("updated", this._onUpdate);
        widget.cssCubicBezierValue = this.currentBezierValue.textContent;
        widget.on("updated", this._onUpdate);
      });
    }
  },

  _onUpdate: function (event, bezier) {
    if (!this.activeSwatch) {
      return;
    }

    this.currentBezierValue.textContent = bezier + "";
    this.preview(bezier + "");
  },

  destroy: function () {
    SwatchBasedEditorTooltip.prototype.destroy.call(this);
    this.currentBezierValue = null;
    this.widget.then(widget => {
      widget.off("updated", this._onUpdate);
      widget.destroy();
    });
  }
});

/**
 * Tooltip for displaying docs for CSS properties from MDN.
 *
 * @param {XULDocument} doc
 */
function CssDocsTooltip(doc) {
  this.tooltip = new Tooltip(doc, {
    consumeOutsideClick: true,
    closeOnKeys: [ESCAPE_KEYCODE, RETURN_KEYCODE],
    noAutoFocus: false
  });
  this.widget = this.tooltip.setMdnDocsContent();
}

module.exports.CssDocsTooltip = CssDocsTooltip;

CssDocsTooltip.prototype = {
  /**
   * Load CSS docs for the given property,
   * then display the tooltip.
   */
  show: function (anchor, propertyName) {
    function loadCssDocs(widget) {
      return widget.loadCssDocs(propertyName);
    }

    this.widget.then(loadCssDocs);
    this.tooltip.show(anchor, "topcenter bottomleft");
  },

  hide: function () {
    this.tooltip.hide();
  },

  destroy: function () {
    this.tooltip.destroy();
  }
};

/**
 * The swatch-based css filter tooltip class is a specific class meant to be
 * used along with rule-view's generated css filter swatches.
 * It extends the parent SwatchBasedEditorTooltip class.
 * It just wraps a standard Tooltip and sets its content with an instance of a
 * CSSFilterEditorWidget.
 *
 * @param {XULDocument} doc
 */
function SwatchFilterTooltip(doc) {
  SwatchBasedEditorTooltip.call(this, doc);

  // Creating a filter editor instance.
  // this.widget will always be a promise that resolves to the widget instance
  this.widget = this.tooltip.setFilterContent("none");
  this._onUpdate = this._onUpdate.bind(this);
}

exports.SwatchFilterTooltip = SwatchFilterTooltip;

SwatchFilterTooltip.prototype = Heritage.extend(SwatchBasedEditorTooltip.prototype, {
  show: function () {
    // Call the parent class' show function
    SwatchBasedEditorTooltip.prototype.show.call(this);
    // Then set the filter value and listen to changes to preview them
    if (this.activeSwatch) {
      this.currentFilterValue = this.activeSwatch.nextSibling;
      this.widget.then(widget => {
        widget.off("updated", this._onUpdate);
        widget.on("updated", this._onUpdate);
        widget.setCssValue(this.currentFilterValue.textContent);
        widget.render();
      });
    }
  },

  _onUpdate: function (event, filters) {
    if (!this.activeSwatch) {
      return;
    }

    // Remove the old children and reparse the property value to
    // recompute them.
    while (this.currentFilterValue.firstChild) {
      this.currentFilterValue.firstChild.remove();
    }
    let node = this._parser.parseCssProperty("filter", filters, this._options);
    this.currentFilterValue.appendChild(node);

    this.preview();
  },

  destroy: function () {
    SwatchBasedEditorTooltip.prototype.destroy.call(this);
    this.currentFilterValue = null;
    this.widget.then(widget => {
      widget.off("updated", this._onUpdate);
      widget.destroy();
    });
  },

  /**
   * Like SwatchBasedEditorTooltip.addSwatch, but accepts a parser object
   * to use when previewing the updated property value.
   *
   * @param {node} swatchEl
   *        @see SwatchBasedEditorTooltip.addSwatch
   * @param {object} callbacks
   *        @see SwatchBasedEditorTooltip.addSwatch
   * @param {object} parser
   *        A parser object; @see OutputParser object
   * @param {object} options
   *        options to pass to the output parser, with
   *          the option |filterSwatch| set.
   */
  addSwatch: function (swatchEl, callbacks, parser, options) {
    SwatchBasedEditorTooltip.prototype.addSwatch.call(this, swatchEl,
                                                      callbacks);
    this._parser = parser;
    this._options = options;
  }
});

/**
 * L10N utility class
 */
function L10N() {}
L10N.prototype = {};

var l10n = new L10N();

loader.lazyGetter(L10N.prototype, "strings", () => {
  return Services.strings.createBundle(
    "chrome://devtools/locale/inspector.properties");
});
