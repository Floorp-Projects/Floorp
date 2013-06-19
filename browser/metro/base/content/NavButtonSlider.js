/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Handles nav overlay button positioning.
 */

var NavButtonSlider = {
  _back: document.getElementById("overlay-back"),
  _plus: document.getElementById("overlay-plus"),
  _dragging: false,
  _yPos: -1,

  get dragging() {
    return this._dragging;
  },

  /*
   * custom dragger, see input.js
   */

  freeDrag: function freeDrag() {
    return true;
  },

  isDraggable: function isDraggable(aTarget, aContent) {
    return { x: false, y: true };
  },

  dragStart: function dragStart(aX, aY, aTarget, aScroller) {
    this._dragging = true;
    return true;
  },

  dragStop: function dragStop(aDx, aDy, aScroller) {
    this._dragging = false;
    return true;
  },

  dragMove: function dragMove(aDx, aDy, aScroller, aIsKenetic, aClientX, aClientY) {
    // Note if aIsKenetic is true this is synthetic movement,
    // we don't want that so return false.
    if (aIsKenetic) {
      return false;
    }
    
    this._update(aClientY);

    // return true if we moved, false otherwise. The result
    // is used in deciding if we should repaint between drags.
    return true;
  },

  /*
   * logic
   */

  init: function init() {
    // touch dragger
    this._back.customDragger = this;
    this._plus.customDragger = this;
    Elements.browsers.addEventListener("ContentSizeChanged", this, true);
    this._updateStops();
  },

  _updateStops: function () {
    this._contentHeight = ContentAreaObserver.contentHeight;
    this._imageHeight = 118;
    this._topStop = this._imageHeight * .7;
    this._bottomStop = this._contentHeight - (this._imageHeight * .7);

    // Check to see if we need to move the slider into view
    if (this._yPos != -1 &&
        (this._topStop > this._yPos || this._bottomStop < this._yPos)) {
      this._back.style.top = "50%";
      this._plus.style.top = "50%";
    }
  },

  _setPosition: function _setPosition() {
    this._back.style.top = this._yPos + "px";
    this._plus.style.top = this._yPos + "px";
  },

  _update: function (aClientY) {
    if (this._topStop > aClientY || this._bottomStop < aClientY)
      return;
    this._yPos = aClientY;
    this._setPosition();
  },

  /*
   * Events
   */

  handleEvent: function handleEvent(aEvent) {
    switch (aEvent.type) {
      case "ContentSizeChanged":
        this._updateStops();
        break;
    }
  },
};


