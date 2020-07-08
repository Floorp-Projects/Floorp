/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
const { Ci } = require("chrome");
const Services = require("Services");

loader.lazyRequireGetter(
  this,
  "isWindowIncluded",
  "devtools/shared/layout/utils",
  true
);
loader.lazyRequireGetter(
  this,
  "isRemoteFrame",
  "devtools/shared/layout/utils",
  true
);

const IS_OSX = Services.appinfo.OS === "Darwin";

class NodePicker {
  constructor(walker, targetActor) {
    this._walker = walker;
    this._targetActor = targetActor;

    this._isPicking = false;
    this._hoveredNode = null;
    this._currentNode = null;

    this._onHovered = this._onHovered.bind(this);
    this._onKey = this._onKey.bind(this);
    this._onPick = this._onPick.bind(this);
    this._onSuppressedEvent = this._onSuppressedEvent.bind(this);
  }

  _findAndAttachElement(event) {
    // originalTarget allows access to the "real" element before any retargeting
    // is applied, such as in the case of XBL anonymous elements.  See also
    // https://developer.mozilla.org/docs/XBL/XBL_1.0_Reference/Anonymous_Content#Event_Flow_and_Targeting
    const node = event.originalTarget || event.target;
    return this._walker.attachElement(node);
  }

  /**
   * Returns `true` if the event was dispatched from a window included in
   * the current highlighter environment; or if the highlighter environment has
   * chrome privileges
   *
   * @param {Event} event
   *          The event to allow
   * @return {Boolean}
   */
  _isEventAllowed({ view }) {
    const { window } = this._targetActor;

    return (
      window instanceof Ci.nsIDOMChromeWindow || isWindowIncluded(window, view)
    );
  }

  /**
   * Pick a node on click.
   *
   * This method doesn't respond anything interesting, however, it starts
   * mousemove, and click listeners on the content document to fire
   * events and let connected clients know when nodes are hovered over or
   * clicked.
   *
   * Once a node is picked, events will cease, and listeners will be removed.
   */
  _onPick(event) {
    // If the picked node is a remote frame, then we need to let the event through
    // since there's a highlighter actor in that sub-frame also picking.
    if (isRemoteFrame(event.target)) {
      return;
    }

    this._preventContentEvent(event);
    if (!this._isEventAllowed(event)) {
      return;
    }

    // If Shift is pressed, this is only a preview click.
    // Send the event to the client, but don't stop picking.
    if (event.shiftKey) {
      this._walker.emit(
        "picker-node-previewed",
        this._findAndAttachElement(event)
      );
      return;
    }

    this._stopPickerListeners();
    this._isPicking = false;
    if (!this._currentNode) {
      this._currentNode = this._findAndAttachElement(event);
    }

    this._walker.emit("picker-node-picked", this._currentNode);
  }

  _onHovered(event) {
    // If the hovered node is a remote frame, then we need to let the event through
    // since there's a highlighter actor in that sub-frame also picking.
    if (isRemoteFrame(event.target)) {
      return;
    }

    this._preventContentEvent(event);
    if (!this._isEventAllowed(event)) {
      return;
    }

    this._currentNode = this._findAndAttachElement(event);
    if (this._hoveredNode !== this._currentNode.node) {
      this._walker.emit("picker-node-hovered", this._currentNode);
      this._hoveredNode = this._currentNode.node;
    }
  }

  _onKey(event) {
    if (!this._currentNode || !this._isPicking) {
      return;
    }

    this._preventContentEvent(event);
    if (!this._isEventAllowed(event)) {
      return;
    }

    let currentNode = this._currentNode.node.rawNode;

    /**
     * KEY: Action/scope
     * LEFT_KEY: wider or parent
     * RIGHT_KEY: narrower or child
     * ENTER/CARRIAGE_RETURN: Picks currentNode
     * ESC/CTRL+SHIFT+C: Cancels picker, picks currentNode
     */
    switch (event.keyCode) {
      // Wider.
      case event.DOM_VK_LEFT:
        if (!currentNode.parentElement) {
          return;
        }
        currentNode = currentNode.parentElement;
        break;

      // Narrower.
      case event.DOM_VK_RIGHT:
        if (!currentNode.children.length) {
          return;
        }

        // Set firstElementChild by default
        let child = currentNode.firstElementChild;
        // If currentNode is parent of hoveredNode, then
        // previously selected childNode is set
        const hoveredNode = this._hoveredNode.rawNode;
        for (const sibling of currentNode.children) {
          if (sibling.contains(hoveredNode) || sibling === hoveredNode) {
            child = sibling;
          }
        }

        currentNode = child;
        break;

      // Select the element.
      case event.DOM_VK_RETURN:
        this._onPick(event);
        return;

      // Cancel pick mode.
      case event.DOM_VK_ESCAPE:
        this.cancelPick();
        this._walker.emit("picker-node-canceled");
        return;
      case event.DOM_VK_C:
        const { altKey, ctrlKey, metaKey, shiftKey } = event;

        if (
          (IS_OSX && metaKey && altKey | shiftKey) ||
          (!IS_OSX && ctrlKey && shiftKey)
        ) {
          this.cancelPick();
          this._walker.emit("picker-node-canceled");
        }
        return;
      default:
        return;
    }

    // Store currently attached element
    this._currentNode = this._walker.attachElement(currentNode);
    this._walker.emit("picker-node-hovered", this._currentNode);
  }

  _onSuppressedEvent(event) {
    if (event.type == "mousemove") {
      this._onHovered(event);
    } else if (event.type == "mouseup") {
      // Suppressed mousedown/mouseup events will be sent to us before they have
      // been converted into click events. Just treat any mouseup as a click.
      this._onPick(event);
    }
  }

  // In most cases, we need to prevent content events from reaching the content. This is
  // needed to avoid triggering actions such as submitting forms or following links.
  // In the case where the event happens on a remote frame however, we do want to let it
  // through. That is because otherwise the pickers started in nested remote frames will
  // never have a chance of picking their own elements.
  _preventContentEvent(event) {
    if (isRemoteFrame(event.target)) {
      return;
    }
    event.stopPropagation();
    event.preventDefault();
  }

  /**
   * When the debugger pauses execution in a page, events will not be delivered
   * to any handlers added to elements on that page. This method uses the
   * document's setSuppressedEventListener interface to bypass this restriction:
   * events will be delivered to the callback at times when they would
   * otherwise be suppressed. The set of events delivered this way is currently
   * limited to mouse events.
   *
   * @param callback The function to call with suppressed events, or null.
   */
  _setSuppressedEventListener(callback) {
    const { document } = this._targetActor.window;

    // Pass the callback to setSuppressedEventListener as an EventListener.
    document.setSuppressedEventListener(
      callback ? { handleEvent: callback } : null
    );
  }

  _startPickerListeners() {
    const target = this._targetActor.chromeEventHandler;
    target.addEventListener("mousemove", this._onHovered, true);
    target.addEventListener("click", this._onPick, true);
    target.addEventListener("mousedown", this._preventContentEvent, true);
    target.addEventListener("mouseup", this._preventContentEvent, true);
    target.addEventListener("dblclick", this._preventContentEvent, true);
    target.addEventListener("keydown", this._onKey, true);
    target.addEventListener("keyup", this._preventContentEvent, true);

    this._setSuppressedEventListener(this._onSuppressedEvent);
  }

  _stopPickerListeners() {
    const target = this._targetActor.chromeEventHandler;
    if (!target) {
      return;
    }

    target.removeEventListener("mousemove", this._onHovered, true);
    target.removeEventListener("click", this._onPick, true);
    target.removeEventListener("mousedown", this._preventContentEvent, true);
    target.removeEventListener("mouseup", this._preventContentEvent, true);
    target.removeEventListener("dblclick", this._preventContentEvent, true);
    target.removeEventListener("keydown", this._onKey, true);
    target.removeEventListener("keyup", this._preventContentEvent, true);

    this._setSuppressedEventListener(null);
  }

  cancelPick() {
    if (this._targetActor.threadActor) {
      this._targetActor.threadActor.showOverlay();
    }

    if (this._isPicking) {
      this._stopPickerListeners();
      this._isPicking = false;
      this._hoveredNode = null;
    }
  }

  pick(doFocus = false) {
    if (this._targetActor.threadActor) {
      this._targetActor.threadActor.hideOverlay();
    }

    if (this._isPicking) {
      return;
    }

    this._startPickerListeners();
    this._isPicking = true;

    if (doFocus) {
      this._targetActor.window.focus();
    }
  }

  destroy() {
    this.cancelPick();

    this._targetActor = null;
    this._walker = null;
  }
}

exports.NodePicker = NodePicker;
