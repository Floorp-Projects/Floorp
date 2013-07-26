/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Handles nav overlay button positioning.
 */

// minimum amount of movement using the mouse after which we cancel the button click handlers
const kOnClickMargin = 3;

const kNavButtonPref = "browser.display.overlaynavbuttons";

var NavButtonSlider = {
  _back: document.getElementById("overlay-back"),
  _plus: document.getElementById("overlay-plus"),
  _mouseMoveStarted: false,
  _mouseDown: false,
  _yPos: -1,

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
    return true;
  },

  dragStop: function dragStop(aDx, aDy, aScroller) {
    return true;
  },

  dragMove: function dragMove(aDx, aDy, aScroller, aIsKenetic, aClientX, aClientY) {
    // Note if aIsKenetic is true this is synthetic movement,
    // we don't want that so return false.
    if (aIsKenetic) {
      return false;
    }
    
    this._updatePosition(aClientY);

    // return true if we moved, false otherwise. The result
    // is used in deciding if we should repaint between drags.
    return true;
  },

  /*
   * logic
   */

  init: function init() {
    // Touch drag support provided by input.js
    this._back.customDragger = this;
    this._plus.customDragger = this;
    Elements.browsers.addEventListener("ContentSizeChanged", this, true);
    let events = ["mousedown", "mouseup", "mousemove", "click", "touchstart", "touchmove", "touchend"];
    events.forEach(function (value) {
      this._back.addEventListener(value, this, true);
      this._plus.addEventListener(value, this, true);
    }, this);

    this._updateStops();
    this._updateVisibility();
    Services.prefs.addObserver(kNavButtonPref, this, false);
  },

  observe: function BrowserUI_observe(aSubject, aTopic, aData) {
    if (aTopic == "nsPref:changed" && aData == kNavButtonPref) {
      this._updateVisibility();
    }
  },

  _updateVisibility: function () {
    if (Services.prefs.getBoolPref(kNavButtonPref)) {
      this._back.removeAttribute("hidden");
      this._plus.removeAttribute("hidden");
    } else {
      this._back.setAttribute("hidden", true);
      this._plus.setAttribute("hidden", true);
    }
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

  _getPosition: function _getPosition() {
    this._yPos = parseInt(getComputedStyle(this._back).top);
  },

  _setPosition: function _setPosition() {
    this._back.style.top = this._yPos + "px";
    this._plus.style.top = this._yPos + "px";
  },

  _updatePosition: function (aClientY) {
    if (this._topStop > aClientY || this._bottomStop < aClientY)
      return;
    this._yPos = aClientY;
    this._setPosition();
  },

  _updateOffset: function (aOffset) {
    let newPos = this._yPos + aOffset;
    if (this._topStop > newPos || this._bottomStop < newPos)
      return;
    this._yPos = newPos;
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

      case "touchstart":
        if (aEvent.touches.length != 1)
          break;
        aEvent.preventDefault();
        aEvent = aEvent.touches[0];
      case "mousedown":
        this._getPosition();
        this._mouseDown = true;
        this._mouseMoveStarted = false;
        this._mouseY = aEvent.clientY;
        if (aEvent.originalTarget)
          aEvent.originalTarget.setCapture();
        this._back.setAttribute("mousedrag", true);
        this._plus.setAttribute("mousedrag", true);
        break;

      case "touchend":
        if (aEvent.touches.length != 0)
          break;
        this._mouseDown = false;
        this._back.removeAttribute("mousedrag");
        this._plus.removeAttribute("mousedrag");
        if (!this._mouseMoveStarted) {
          if (aEvent.originalTarget == this._back) {
            CommandUpdater.doCommand('cmd_back');
          } else {
            CommandUpdater.doCommand('cmd_newTab');
          }
        }
        break;
      case "mouseup":
        this._mouseDown = false;
        this._back.removeAttribute("mousedrag");
        this._plus.removeAttribute("mousedrag");
        break;

      case "touchmove":
        if (aEvent.touches.length != 1)
          break;
        aEvent = aEvent.touches[0];
      case "mousemove":
        // Check to be sure this is a drag operation
        if (!this._mouseDown) {
          return;
        }
        // Don't start a drag until we've passed a threshold
        let dy = aEvent.clientY - this._mouseY;
        if (!this._mouseMoveStarted && Math.abs(dy) < kOnClickMargin) {
          return;
        }
        // Start dragging via the mouse
        this._mouseMoveStarted = true;
        this._mouseY = aEvent.clientY;
        this._updateOffset(dy);
        break;
      case "click":
        // Don't invoke the click action if we've moved the buttons via the mouse.
        if (this._mouseMoveStarted) {
          return;
        }
        if (aEvent.originalTarget == this._back) {
           CommandUpdater.doCommand('cmd_back');
        } else {
           CommandUpdater.doCommand('cmd_newTab');
        }
        break;
    }
  },
};


