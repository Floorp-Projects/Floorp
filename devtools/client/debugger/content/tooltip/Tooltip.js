/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const defer = require("devtools/shared/defer");
const EventEmitter = require("devtools/shared/event-emitter");
const {KeyCodes} = require("devtools/client/shared/keycodes");
const {TooltipToggle} = require("devtools/client/shared/widgets/tooltip/TooltipToggle");

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
 * - keydown : when any key gets pressed, with keyCode
 */

class Tooltip {
  constructor(doc, {
  consumeOutsideClick = false,
  closeOnKeys = [ESCAPE_KEYCODE],
  noAutoFocus = true,
  closeOnEvents = [],
  } = {}) {
    EventEmitter.decorate(this);

    this.defaultPosition = "before_start";
    // px
    this.defaultOffsetX = 0;
    // px
    this.defaultOffsetY = 0;
    // px

    this.doc = doc;
    this.consumeOutsideClick = consumeOutsideClick;
    this.closeOnKeys = closeOnKeys;
    this.noAutoFocus = noAutoFocus;
    this.closeOnEvents = closeOnEvents;

    this.panel = this._createPanel();

    // Create tooltip toggle helper and decorate the Tooltip instance with
    // shortcut methods.
    this._toggle = new TooltipToggle(this);
    this.startTogglingOnHover = this._toggle.start.bind(this._toggle);
    this.stopTogglingOnHover = this._toggle.stop.bind(this._toggle);

  // Emit show/hide events when the panel does.
    for (const eventName of POPUP_EVENTS) {
      this["_onPopup" + eventName] = (name => {
        return e => {
          if (e.target === this.panel) {
            this.emit(name);
          }
        };
      })(eventName);
      this.panel.addEventListener("popup" + eventName,
        this["_onPopup" + eventName]);
    }

  // Listen to keydown events to close the tooltip if configured to do so
    const win = this.doc.querySelector("window");
    this._onKeyDown = event => {
      if (this.panel.hidden) {
        return;
      }

      this.emit("keydown", event.keyCode);
      if (this.closeOnKeys.includes(event.keyCode) &&
          this.isShown()) {
        event.stopPropagation();
        this.hide();
      }
    };
    win.addEventListener("keydown", this._onKeyDown);

  // Listen to custom emitters' events to close the tooltip
    this.hide = this.hide.bind(this);
    for (const {emitter, event, useCapture} of this.closeOnEvents) {
      for (const add of ["addEventListener", "on"]) {
        if (add in emitter) {
          emitter[add](event, this.hide, useCapture);
          break;
        }
      }
    }
  }

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
  show(anchor,
    position = this.defaultPosition,
    x = this.defaultOffsetX,
    y = this.defaultOffsetY) {
    this.panel.hidden = false;
    this.panel.openPopup(anchor, position, x, y);
  }

  /**
   * Hide the tooltip
   */
  hide() {
    this.panel.hidden = true;
    this.panel.hidePopup();
  }

  isShown() {
    return this.panel &&
           this.panel.state !== "closed" &&
           this.panel.state !== "hiding";
  }

  setSize(width, height) {
    this.panel.sizeTo(width, height);
  }

  /**
   * Empty the tooltip's content
   */
  empty() {
    while (this.panel.hasChildNodes()) {
      this.panel.firstChild.remove();
    }
  }

  /**
   * Gets this panel's visibility state.
   * @return boolean
   */
  isHidden() {
    return this.panel.state == "closed" || this.panel.state == "hiding";
  }

  /**
   * Gets if this panel has any child nodes.
   * @return boolean
   */
  isEmpty() {
    return !this.panel.hasChildNodes();
  }

  /**
   * Get rid of references and event listeners
   */
  destroy() {
    this.hide();

    for (const eventName of POPUP_EVENTS) {
      this.panel.removeEventListener("popup" + eventName,
        this["_onPopup" + eventName]);
    }

    const win = this.doc.querySelector("window");
    win.removeEventListener("keydown", this._onKeyDown);

    for (const {emitter, event, useCapture} of this.closeOnEvents) {
      for (const remove of ["removeEventListener", "off"]) {
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
  }

  /**
   * Returns the outer container node (that includes the arrow etc.). Happens
   * to be identical to this.panel here, can be different element in other
   * Tooltip implementations.
   */
  get container() {
    return this.panel;
  }

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
  }

  get content() {
    return this.panel.firstChild;
  }

  /**
   * Sets some text as the content of this tooltip.
   *
   * @param {array} messages
   *        A list of text messages.
   * @param {string} messagesClass [optional]
   *        A style class for the text messages.
   * @param {string} containerClass [optional]
   *        A style class for the text messages container.
   */
  setTextContent(
    {
      messages,
      messagesClass,
      containerClass
    },
    extraButtons = []) {
    messagesClass = messagesClass || "default-tooltip-simple-text-colors";
    containerClass = containerClass || "default-tooltip-simple-text-colors";

    const vbox = this.doc.createElement("vbox");
    vbox.className = "devtools-tooltip-simple-text-container " + containerClass;
    vbox.setAttribute("flex", "1");

    for (const text of messages) {
      const description = this.doc.createElement("description");
      description.setAttribute("flex", "1");
      description.className = "devtools-tooltip-simple-text " + messagesClass;
      description.textContent = text;
      vbox.appendChild(description);
    }

    for (const { label, className, command } of extraButtons) {
      const button = this.doc.createElement("button");
      button.className = className;
      button.setAttribute("label", label);
      button.addEventListener("command", command);
      vbox.appendChild(button);
    }

    this.content = vbox;
  }

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
  setIFrameContent({width, height}, url) {
    const def = defer();

    // Create an iframe
    const iframe = this.doc.createElementNS(XHTML_NS, "iframe");
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

  /**
   * Create the tooltip panel
   */
  _createPanel() {
    const panel = this.doc.createElement("panel");
    panel.setAttribute("hidden", true);
    panel.setAttribute("ignorekeys", true);
    panel.setAttribute("animate", false);

    panel.setAttribute("consumeoutsideclicks",
                       this.consumeOutsideClick);
    panel.setAttribute("noautofocus", this.noAutoFocus);
    panel.setAttribute("type", "arrow");
    panel.setAttribute("level", "top");

    panel.setAttribute("class", "devtools-tooltip theme-tooltip-panel");
    this.doc.querySelector("window").appendChild(panel);

    return panel;
  }
}

module.exports = Tooltip;
