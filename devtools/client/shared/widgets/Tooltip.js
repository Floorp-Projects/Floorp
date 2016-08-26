/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const defer = require("devtools/shared/defer");
const {Spectrum} = require("devtools/client/shared/widgets/Spectrum");
const {CubicBezierWidget} =
      require("devtools/client/shared/widgets/CubicBezierWidget");
const {CSSFilterEditorWidget} = require("devtools/client/shared/widgets/FilterWidget");
const {TooltipToggle} = require("devtools/client/shared/widgets/tooltip/TooltipToggle");
const EventEmitter = require("devtools/shared/event-emitter");
const {colorUtils} = require("devtools/shared/css-color");
const Heritage = require("sdk/core/heritage");
const {HTMLTooltip} = require("devtools/client/shared/widgets/HTMLTooltip");
const {KeyShortcuts} = require("devtools/client/shared/key-shortcuts");
const {Task} = require("devtools/shared/task");
const {KeyCodes} = require("devtools/client/shared/keycodes");

const XHTML_NS = "http://www.w3.org/1999/xhtml";
const ESCAPE_KEYCODE = KeyCodes.DOM_VK_ESCAPE;
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
 *       t.content = el;
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
   * Returns the outer container node (that includes the arrow etc.). Happens
   * to be identical to this.panel here, can be different element in other
   * Tooltip implementations.
   */
  get container() {
    return this.panel;
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
    let def = defer();

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
  }
};

/**
 * Base class for all (color, gradient, ...)-swatch based value editors inside
 * tooltips
 *
 * @param {Toolbox} toolbox
 *        The devtools toolbox, needed to get the devtools main window.
 */
function SwatchBasedEditorTooltip(toolbox, stylesheet) {
  EventEmitter.decorate(this);
  // Creating a tooltip instance
  // This one will consume outside clicks as it makes more sense to let the user
  // close the tooltip by clicking out
  // It will also close on <escape> and <enter>
  this.tooltip = new HTMLTooltip(toolbox, {
    type: "arrow",
    consumeOutsideClicks: true,
    useXulWrapper: true,
    stylesheet
  });

  // By default, swatch-based editor tooltips revert value change on <esc> and
  // commit value change on <enter>
  this.shortcuts = new KeyShortcuts({
    window: this.tooltip.topWindow
  });
  this.shortcuts.on("Escape", (name, event) => {
    if (!this.tooltip.isVisible()) {
      return;
    }
    this.revert();
    this.hide();
    event.stopPropagation();
    event.preventDefault();
  });
  this.shortcuts.on("Return", (name, event) => {
    if (!this.tooltip.isVisible()) {
      return;
    }
    this.commit();
    this.hide();
    event.stopPropagation();
    event.preventDefault();
  });

  // All target swatches are kept in a map, indexed by swatch DOM elements
  this.swatches = new Map();

  // When a swatch is clicked, and for as long as the tooltip is shown, the
  // activeSwatch property will hold the reference to the swatch DOM element
  // that was clicked
  this.activeSwatch = null;

  this._onSwatchClick = this._onSwatchClick.bind(this);
}

SwatchBasedEditorTooltip.prototype = {
  /**
   * Show the editor tooltip for the currently active swatch.
   *
   * @return {Promise} a promise that resolves once the editor tooltip is displayed, or
   *         immediately if there is no currently active swatch.
   */
  show: function () {
    if (this.activeSwatch) {
      let onShown = this.tooltip.once("shown");
      this.tooltip.show(this.activeSwatch, "topcenter bottomleft");

      // When the tooltip is closed by clicking outside the panel we want to
      // commit any changes.
      this.tooltip.once("hidden", () => {
        if (!this._reverted && !this.eyedropperOpen) {
          this.commit();
        }
        this._reverted = false;

        // Once the tooltip is hidden we need to clean up any remaining objects.
        if (!this.eyedropperOpen) {
          this.activeSwatch = null;
        }
      });

      return onShown;
    }

    return Promise.resolve();
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
      this.tooltip.once("hidden", () => {
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
    this.shortcuts.destroy();
  }
};

/**
 * The swatch color picker tooltip class is a specific class meant to be used
 * along with output-parser's generated color swatches.
 * It extends the parent SwatchBasedEditorTooltip class.
 * It just wraps a standard Tooltip and sets its content with an instance of a
 * color picker.
 *
 * @param {Toolbox} toolbox
 *        The devtools toolbox, needed to get the devtools main window.
 * @param {InspectorPanel} inspector
 *        The inspector panel, needed for the eyedropper.
 */
function SwatchColorPickerTooltip(toolbox, inspector) {
  let stylesheet = "chrome://devtools/content/shared/widgets/spectrum.css";
  SwatchBasedEditorTooltip.call(this, toolbox, stylesheet);

  this.inspector = inspector;

  // Creating a spectrum instance. this.spectrum will always be a promise that
  // resolves to the spectrum instance
  this.spectrum = this.setColorPickerContent([0, 0, 0, 1]);
  this._onSpectrumColorChange = this._onSpectrumColorChange.bind(this);
  this._openEyeDropper = this._openEyeDropper.bind(this);
}

module.exports.SwatchColorPickerTooltip = SwatchColorPickerTooltip;

SwatchColorPickerTooltip.prototype =
Heritage.extend(SwatchBasedEditorTooltip.prototype, {
  /**
   * Fill the tooltip with a new instance of the spectrum color picker widget
   * initialized with the given color, and return the instance of spectrum
   */
  setColorPickerContent: function (color) {
    let { doc } = this.tooltip;

    let container = doc.createElementNS(XHTML_NS, "div");
    container.id = "spectrum-tooltip";
    let spectrumNode = doc.createElementNS(XHTML_NS, "div");
    spectrumNode.id = "spectrum";
    container.appendChild(spectrumNode);
    let eyedropper = doc.createElementNS(XHTML_NS, "button");
    eyedropper.id = "eyedropper-button";
    eyedropper.className = "devtools-button";
    container.appendChild(eyedropper);

    this.tooltip.setContent(container, { width: 218, height: 224 });

    let spectrum = new Spectrum(spectrumNode, color);

    // Wait for the tooltip to be shown before calling spectrum.show
    // as it expect to be visible in order to compute DOM element sizes.
    this.tooltip.once("shown", () => {
      spectrum.show();
    });

    return spectrum;
  },

  /**
   * Overriding the SwatchBasedEditorTooltip.show function to set spectrum's
   * color.
   */
  show: Task.async(function* () {
    // Call then parent class' show function
    yield SwatchBasedEditorTooltip.prototype.show.call(this);
    // Then set spectrum's color and listen to color changes to preview them
    if (this.activeSwatch) {
      this.currentSwatchColor = this.activeSwatch.nextSibling;
      this._originalColor = this.currentSwatchColor.textContent;
      let color = this.activeSwatch.style.backgroundColor;
      this.spectrum.off("changed", this._onSpectrumColorChange);
      this.spectrum.rgb = this._colorToRgba(color);
      this.spectrum.on("changed", this._onSpectrumColorChange);
      this.spectrum.updateUI();
    }

    let {target} = this.inspector.toolbox;
    target.actorHasMethod("inspector", "pickColorFromPage").then(value => {
      let tooltipDoc = this.tooltip.doc;
      let eyeButton = tooltipDoc.querySelector("#eyedropper-button");
      if (value && this.inspector.selection.nodeFront.isInHTMLDocument) {
        eyeButton.addEventListener("click", this._openEyeDropper);
      } else {
        eyeButton.style.display = "none";
      }
      this.emit("ready");
    }, e => console.error(e));
  }),

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
    let {inspector, toolbox, telemetry} = this.inspector;
    telemetry.toolOpened("pickereyedropper");
    inspector.pickColorFromPage(toolbox, {copyOnSelect: false}).then(() => {
      this.eyedropperOpen = true;

      // close the colorpicker tooltip so that only the eyedropper is open.
      this.hide();

      this.tooltip.emit("eyedropper-opened");
    }, e => console.error(e));

    inspector.once("color-picked", color => {
      toolbox.win.focus();
      this._selectColor(color);
      this._onEyeDropperDone();
    });

    inspector.once("color-pick-canceled", () => {
      this._onEyeDropperDone();
    });
  },

  _onEyeDropperDone: function () {
    this.eyedropperOpen = false;
    this.activeSwatch = null;
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
    this.inspector = null;
    this.currentSwatchColor = null;
    this.spectrum.off("changed", this._onSpectrumColorChange);
    this.spectrum.destroy();
  }
});

/**
 * The swatch cubic-bezier tooltip class is a specific class meant to be used
 * along with rule-view's generated cubic-bezier swatches.
 * It extends the parent SwatchBasedEditorTooltip class.
 * It just wraps a standard Tooltip and sets its content with an instance of a
 * CubicBezierWidget.
 *
 * @param {Toolbox} toolbox
 *        The devtools toolbox, needed to get the devtools main window.
 */
function SwatchCubicBezierTooltip(toolbox) {
  let stylesheet = "chrome://devtools/content/shared/widgets/cubic-bezier.css";
  SwatchBasedEditorTooltip.call(this, toolbox, stylesheet);

  // Creating a cubic-bezier instance.
  // this.widget will always be a promise that resolves to the widget instance
  this.widget = this.setCubicBezierContent([0, 0, 1, 1]);
  this._onUpdate = this._onUpdate.bind(this);
}

module.exports.SwatchCubicBezierTooltip = SwatchCubicBezierTooltip;

SwatchCubicBezierTooltip.prototype =
Heritage.extend(SwatchBasedEditorTooltip.prototype, {
  /**
   * Fill the tooltip with a new instance of the cubic-bezier widget
   * initialized with the given value, and return a promise that resolves to
   * the instance of the widget
   */
  setCubicBezierContent: function (bezier) {
    let { doc } = this.tooltip;

    let container = doc.createElementNS(XHTML_NS, "div");
    container.className = "cubic-bezier-container";

    this.tooltip.setContent(container, { width: 510, height: 370 });

    let def = defer();

    // Wait for the tooltip to be shown before calling instanciating the widget
    // as it expect its DOM elements to be visible.
    this.tooltip.once("shown", () => {
      let widget = new CubicBezierWidget(container, bezier);
      def.resolve(widget);
    });

    return def.promise;
  },

  /**
   * Overriding the SwatchBasedEditorTooltip.show function to set the cubic
   * bezier curve in the widget
   */
  show: Task.async(function* () {
    // Call the parent class' show function
    yield SwatchBasedEditorTooltip.prototype.show.call(this);
    // Then set the curve and listen to changes to preview them
    if (this.activeSwatch) {
      this.currentBezierValue = this.activeSwatch.nextSibling;
      this.widget.then(widget => {
        widget.off("updated", this._onUpdate);
        widget.cssCubicBezierValue = this.currentBezierValue.textContent;
        widget.on("updated", this._onUpdate);
        this.emit("ready");
      });
    }
  }),

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
 * The swatch-based css filter tooltip class is a specific class meant to be
 * used along with rule-view's generated css filter swatches.
 * It extends the parent SwatchBasedEditorTooltip class.
 * It just wraps a standard Tooltip and sets its content with an instance of a
 * CSSFilterEditorWidget.
 *
 * @param {Toolbox} toolbox
 *        The devtools toolbox, needed to get the devtools main window.
 */
function SwatchFilterTooltip(toolbox) {
  let stylesheet = "chrome://devtools/content/shared/widgets/filter-widget.css";
  SwatchBasedEditorTooltip.call(this, toolbox, stylesheet);

  // Creating a filter editor instance.
  this.widget = this.setFilterContent("none");
  this._onUpdate = this._onUpdate.bind(this);
}

exports.SwatchFilterTooltip = SwatchFilterTooltip;

SwatchFilterTooltip.prototype =
Heritage.extend(SwatchBasedEditorTooltip.prototype, {
  /**
   * Fill the tooltip with a new instance of the CSSFilterEditorWidget
   * widget initialized with the given filter value, and return a promise
   * that resolves to the instance of the widget when ready.
   */
  setFilterContent: function (filter) {
    let { doc } = this.tooltip;

    let container = doc.createElementNS(XHTML_NS, "div");
    container.id = "filter-container";

    this.tooltip.setContent(container, { width: 510, height: 200 });

    return new CSSFilterEditorWidget(container, filter);
  },

  show: Task.async(function* () {
    // Call the parent class' show function
    yield SwatchBasedEditorTooltip.prototype.show.call(this);
    // Then set the filter value and listen to changes to preview them
    if (this.activeSwatch) {
      this.currentFilterValue = this.activeSwatch.nextSibling;
      this.widget.off("updated", this._onUpdate);
      this.widget.on("updated", this._onUpdate);
      this.widget.setCssValue(this.currentFilterValue.textContent);
      this.widget.render();
      this.emit("ready");
    }
  }),

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
    this.widget.off("updated", this._onUpdate);
    this.widget.destroy();
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
