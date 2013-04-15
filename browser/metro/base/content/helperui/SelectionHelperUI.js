/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * selection management
 */

/*
 * Current monocle image:
 *  dimensions: 32 x 24
 *  circle center: 16 x 14
 *  padding top: 6
 */

// Y axis scroll distance that will disable this module and cancel selection
const kDisableOnScrollDistance = 25;

/*
 * Markers
 */

function MarkerDragger(aMarker) {
  this._marker = aMarker;
}

MarkerDragger.prototype = {
  _selectionHelperUI: null,
  _marker: null,
  _shutdown: false,
  _dragging: false,

  get marker() {
    return this._marker;
  },

  set shutdown(aVal) {
    this._shutdown = aVal;
  },

  get shutdown() {
    return this._shutdown;
  },

  get dragging() {
    return this._dragging;
  },

  freeDrag: function freeDrag() {
    return true;
  },

  isDraggable: function isDraggable(aTarget, aContent) {
    return { x: true, y: true };
  },

  dragStart: function dragStart(aX, aY, aTarget, aScroller) {
    if (this._shutdown)
      return false;
    this._dragging = true;
    this.marker.dragStart(aX, aY);
    return true;
  },

  dragStop: function dragStop(aDx, aDy, aScroller) {
    if (this._shutdown)
      return false;
    this._dragging = false;
    this.marker.dragStop(aDx, aDy);
    return true;
  },

  dragMove: function dragMove(aDx, aDy, aScroller, aIsKenetic, aClientX, aClientY) {
    // Note if aIsKenetic is true this is synthetic movement,
    // we don't want that so return false.
    if (this._shutdown || aIsKenetic)
      return false;
    this.marker.moveBy(aDx, aDy, aClientX, aClientY);
    // return true if we moved, false otherwise. The result
    // is used in deciding if we should repaint between drags.
    return true;
  }
}

function Marker(aParent, aTag, aElementId, xPos, yPos) {
  this._xPos = xPos;
  this._yPos = yPos;
  this._selectionHelperUI = aParent;
  this._element = document.getElementById(aElementId);
  this._elementId = aElementId;
  // These get picked in input.js and receives drag input
  this._element.customDragger = new MarkerDragger(this);
  this.tag = aTag;
}

Marker.prototype = {
  _element: null,
  _elementId: "",
  _selectionHelperUI: null,
  _xPos: 0,
  _yPos: 0,
  _tag: "",
  _hPlane: 0,
  _vPlane: 0,

  // Tweak me if the monocle graphics change in any way
  _monocleRadius: 8,
  _monocleXHitTextAdjust: -2, 
  _monocleYHitTextAdjust: -10, 

  get xPos() {
    return this._xPos;
  },

  get yPos() {
    return this._yPos;
  },

  get tag() {
    return this._tag;
  },

  set tag(aVal) {
    this._tag = aVal;
  },

  get dragging() {
    return this._element.customDragger.dragging;
  },

  shutdown: function shutdown() {
    this._element.hidden = true;
    this._element.customDragger.shutdown = true;
    delete this._element.customDragger;
    this._selectionHelperUI = null;
    this._element = null;
  },

  setTrackBounds: function setTrackBounds(aVerticalPlane, aHorizontalPlane) {
    // monocle boundaries
    this._hPlane = aHorizontalPlane;
    this._vPlane = aVerticalPlane;
  },

  show: function show() {
    this._element.hidden = false;
  },

  hide: function hide() {
    this._element.hidden = true;
  },

  get visible() {
    return this._element.hidden == false;
  },

  position: function position(aX, aY) {
    if (aX < 0) {
      Util.dumpLn("Marker: aX is negative");
      aX = 0;
    }
    if (aY < 0) {
      Util.dumpLn("Marker: aY is negative");
      aY = 0;
    }
    this._xPos = aX;
    this._yPos = aY;
    this._setPosition();
  },

  _setPosition: function _setPosition() {
    this._element.left = this._xPos + "px";
    this._element.top = this._yPos + "px";
  },

  dragStart: function dragStart(aX, aY) {
    this._selectionHelperUI.markerDragStart(this);
  },

  dragStop: function dragStop(aDx, aDy) {
    this._selectionHelperUI.markerDragStop(this);
  },

  moveBy: function moveBy(aDx, aDy, aClientX, aClientY) {
    this._xPos -= aDx;
    this._yPos -= aDy;
    let direction = (aDx >= 0 && aDy >= 0 ? "start" : "end");
    // We may swap markers in markerDragMove. If markerDragMove
    // returns true keep processing, otherwise get out of here.
    if (this._selectionHelperUI.markerDragMove(this, direction)) {
      this._setPosition();
    }
  },

  hitTest: function hitTest(aX, aY) {
    // Gets the pointer of the arrow right in the middle of the
    // monocle.
    aY += this._monocleYHitTextAdjust;
    aX += this._monocleXHitTextAdjust;
    if (aX >= (this._xPos - this._monocleRadius) &&
        aX <= (this._xPos + this._monocleRadius) &&
        aY >= (this._yPos - this._monocleRadius) &&
        aY <= (this._yPos + this._monocleRadius))
      return true;
    return false;
  },

  swapMonocle: function swapMonocle(aCaret) {
    let targetElement = aCaret._element;
    let targetElementId = aCaret._elementId;

    aCaret._element = this._element;
    aCaret._element.customDragger._marker = aCaret;
    aCaret._elementId = this._elementId;

    this._xPos = aCaret._xPos;
    this._yPos = aCaret._yPos;
    this._element = targetElement;
    this._element.customDragger._marker = this;
    this._elementId = targetElementId;
    this._element.visible = true;
  },

};

/*
 * SelectionHelperUI
 */

var SelectionHelperUI = {
  _debugEvents: false,
  _msgTarget: null,
  _startMark: null,
  _endMark: null,
  _caretMark: null,
  _target: null,
  _movement: { active: false, x:0, y: 0 },
  _activeSelectionRect: null,
  _selectionHandlerActive: false,
  _selectionMarkIds: [],
  _targetIsEditable: false,

  /*
   * Properties
   */

  get startMark() {
    if (this._startMark == null) {
      this._startMark = new Marker(this, "start", this._selectionMarkIds.pop(), 0, 0);
    }
    return this._startMark;
  },

  get endMark() {
    if (this._endMark == null) {
      this._endMark = new Marker(this, "end", this._selectionMarkIds.pop(), 0, 0);
    }
    return this._endMark;
  },

  get caretMark() {
    if (this._caretMark == null) {
      this._caretMark = new Marker(this, "caret", this._selectionMarkIds.pop(), 0, 0);
    }
    return this._caretMark;
  },

  get overlay() {
    return document.getElementById("selection-overlay");
  },

  /*
   * isActive (prop)
   *
   * Determines if an edit session is currently active.
   */
  get isActive() {
    return (this._msgTarget &&
            this._selectionHandlerActive);
  },

  /*
   * Public apis
   */

  /*
   * openEditSession
   * 
   * Attempts to select underlying text at a point and begins editing
   * the section.
   *
   * @param aContent - Browser object
   * @param aX, aY - Browser relative client coordinates.
   */
  openEditSession: function openEditSession(aBrowser, aX, aY) {
    if (!aBrowser || this.isActive)
      return;
    this._init(aBrowser);
    this._setupDebugOptions();

    // Send this over to SelectionHandler in content, they'll message us
    // back with information on the current selection. SelectionStart
    // takes client coordinates.
    this._selectionHandlerActive = false;
    this._sendAsyncMessage("Browser:SelectionStart", {
      xPos: aX,
      yPos: aY
    });
  },

  /*
   * attachEditSession
   * 
   * Attaches to existing selection and begins editing.
   *
   * @param aBrowser - Browser object
   * @param aX, aY - Browser relative client coordinates.
   */
  attachEditSession: function attachEditSession(aBrowser, aX, aY) {
    if (!aBrowser || this.isActive)
      return;
    this._init(aBrowser);
    this._setupDebugOptions();

    // Send this over to SelectionHandler in content, they'll message us
    // back with information on the current selection. SelectionAttach
    // takes client coordinates.
    this._selectionHandlerActive = false;
    this._sendAsyncMessage("Browser:SelectionAttach", {
      xPos: aX,
      yPos: aY
    });
  },

  /*
   * attachToCaret
   * 
   * Initiates a touch caret selection session for a text input.
   * Can be called multiple times to move the caret marker around.
   *
   * Note the caret marker is pretty limited in functionality. The
   * only thing is can do is be displayed at the caret position.
   * Once the user starts a drag, the caret marker is hidden, and
   * the start and end markers take over.
   *
   * @param aBrowser - Browser object
   * @param aX, aY - Browser relative client coordinates of the tap
   * that initiated the session.
   */
  attachToCaret: function attachToCaret(aBrowser, aX, aY) {
    if (!this.isActive) {
      this._init(aBrowser);
      this._setupDebugOptions();
    } else {
      this._hideMonocles();
    }

    this._lastPoint = { xPos: aX, yPos: aY };

    this._selectionHandlerActive = false;
    this._sendAsyncMessage("Browser:CaretAttach", {
      xPos: aX,
      yPos: aY
    });
  },

  /*
   * canHandleContextMenuMsg
   *
   * Determines if we can handle a ContextMenuHandler message.
   */
  canHandleContextMenuMsg: function canHandleContextMenuMsg(aMessage) {
    if (aMessage.json.types.indexOf("content-text") != -1)
      return true;
    return false;
  },

  /*
   * closeEditSession(aClearSelection)
   *
   * Closes an active edit session and shuts down. Does not clear existing
   * selection regions if they exist.
   *
   * @param aClearSelection bool indicating if the selection handler should also
   * clear any selection. optional, the default is false.
   */
  closeEditSession: function closeEditSession(aClearSelection) {
    let clearSelection = aClearSelection || false;
    this._sendAsyncMessage("Browser:SelectionClose", {
      clearSelection: clearSelection
    });
    this._shutdown();
  },

  /*
   * Init and shutdown
   */

  _init: function _init(aMsgTarget) {
    // store the target message manager
    this._msgTarget = aMsgTarget;

    // Init our list of available monocle ids
    this._setupMonocleIdArray();

    // Init selection rect info
    this._activeSelectionRect = Util.getCleanRect();
    this._targetElementRect = Util.getCleanRect();

    // SelectionHandler messages
    messageManager.addMessageListener("Content:SelectionRange", this);
    messageManager.addMessageListener("Content:SelectionCopied", this);
    messageManager.addMessageListener("Content:SelectionFail", this);
    messageManager.addMessageListener("Content:SelectionDebugRect", this);

    window.addEventListener("keypress", this, true);
    window.addEventListener("click", this, false);
    window.addEventListener("touchstart", this, true);
    window.addEventListener("touchend", this, true);
    window.addEventListener("touchmove", this, true);
    window.addEventListener("MozContextUIShow", this, true);
    window.addEventListener("MozContextUIDismiss", this, true);
    window.addEventListener("MozPrecisePointer", this, true);
    window.addEventListener("MozDeckOffsetChanging", this, true);
    window.addEventListener("MozDeckOffsetChanged", this, true);

    Elements.browsers.addEventListener("URLChanged", this, true);
    Elements.browsers.addEventListener("SizeChanged", this, true);
    Elements.browsers.addEventListener("ZoomChanged", this, true);

    this.overlay.enabled = true;
  },

  _shutdown: function _shutdown() {
    messageManager.removeMessageListener("Content:SelectionRange", this);
    messageManager.removeMessageListener("Content:SelectionCopied", this);
    messageManager.removeMessageListener("Content:SelectionFail", this);
    messageManager.removeMessageListener("Content:SelectionDebugRect", this);

    window.removeEventListener("keypress", this, true);
    window.removeEventListener("click", this, false);
    window.removeEventListener("touchstart", this, true);
    window.removeEventListener("touchend", this, true);
    window.removeEventListener("touchmove", this, true);
    window.removeEventListener("MozContextUIShow", this, true);
    window.removeEventListener("MozContextUIDismiss", this, true);
    window.removeEventListener("MozPrecisePointer", this, true);
    window.removeEventListener("MozDeckOffsetChanging", this, true);
    window.removeEventListener("MozDeckOffsetChanged", this, true);

    Elements.browsers.removeEventListener("URLChanged", this, true);
    Elements.browsers.removeEventListener("SizeChanged", this, true);
    Elements.browsers.removeEventListener("ZoomChanged", this, true);

    this._shutdownAllMarkers();

    this._selectionMarkIds = [];
    this._msgTarget = null;
    this._activeSelectionRect = null;
    this._selectionHandlerActive = false;

    this.overlay.displayDebugLayer = false;
    this.overlay.enabled = false;
  },

  /*
   * Utilities
   */

  /*
   * _swapCaretMarker
   *
   * Swap two drag markers - used when transitioning from caret mode
   * to selection mode. We take the current caret marker (which is in a
   * drag state) and swap it out with one of the selection markers.
   */
  _swapCaretMarker: function _swapCaretMarker(aDirection) {
    let targetMark = null;
    if (aDirection == "start")
      targetMark = this.startMark;
    else
      targetMark = this.endMark;
    let caret = this.caretMark;
    targetMark.swapMonocle(caret);
    let id = caret._elementId;
    caret.shutdown();
    this._caretMark = null;
    this._selectionMarkIds.push(id);
  },

  /*
   * _transitionFromCaretToSelection
   *
   * Transitions from caret mode to text selection mode.
   */
  _transitionFromCaretToSelection: function _transitionFromCaretToSelection(aDirection) {
    // Get selection markers initialized if they aren't already
    { let mark = this.startMark; mark = this.endMark; }

    // Swap the caret marker out for the start or end marker depending
    // on direction.
    this._swapCaretMarker(aDirection);

    let targetMark = null;
    if (aDirection == "start")
      targetMark = this.startMark;
    else
      targetMark = this.endMark;

    // Position both in the same starting location.
    this.startMark.position(targetMark.xPos, targetMark.yPos);
    this.endMark.position(targetMark.xPos, targetMark.yPos);

    // Start the selection monocle drag. SelectionHandler relies on this
    // for getting initialized. This will also trigger a message back for
    // monocle positioning. Note, markerDragMove is still on the stack in
    // this call!
    this._sendAsyncMessage("Browser:SelectionSwitchMode", {
      newMode: "selection",
      change: targetMark.tag,
      xPos: this._msgTarget.ctobx(targetMark.xPos, true),
      yPos: this._msgTarget.ctoby(targetMark.yPos, true),
    });
  },

  /*
   * _transitionFromSelectionToCaret
   *
   * Transitions from text selection mode to caret mode.
   *
   * @param aClientX, aClientY - client coordinates of the
   * tap that initiates the change.
   */
  _transitionFromSelectionToCaret: function _transitionFromSelectionToCaret(aClientX, aClientY) {
    // clear existing selection and shutdown SelectionHandler
    this.closeEditSession(true);

    // Reset some of our state
    this._selectionHandlerActive = false;
    this._activeSelectionRect = null;

    // Reset the monocles
    this._shutdownAllMarkers();
    this._setupMonocleIdArray();

    // Translate to browser relative client coordinates
    let coords =
      this._msgTarget.ptClientToBrowser(aClientX, aClientY, true);

    // Init SelectionHandler and turn on caret selection. Note the focus caret
    // will have been removed from the target element due to the shutdown call.
    // This won't set the caret position on its own.
    this._sendAsyncMessage("Browser:CaretAttach", {
      xPos: coords.x,
      yPos: coords.y
    });

    // Set the caret position
    this._setCaretPositionAtPoint(coords.x, coords.y);
  },

  /*
   * _setupDebugOptions
   *
   * Sends a message over to content instructing it to
   * turn on various debug features.
   */
  _setupDebugOptions: function _setupDebugOptions() {
    // Debug options for selection
    let debugOpts = { dumpRanges: false, displayRanges: false, dumpEvents: false };
    try {
      if (Services.prefs.getBoolPref(kDebugSelectionDumpPref))
        debugOpts.displayRanges = true;
    } catch (ex) {}
    try {
      if (Services.prefs.getBoolPref(kDebugSelectionDisplayPref))
        debugOpts.displayRanges = true;
    } catch (ex) {}
    try {
      if (Services.prefs.getBoolPref(kDebugSelectionDumpEvents)) {
        debugOpts.dumpEvents = true;
        this._debugEvents = true;
      }
    } catch (ex) {}

    if (debugOpts.displayRanges || debugOpts.dumpRanges || debugOpts.dumpEvents) {
      // Turn on the debug layer
      this.overlay.displayDebugLayer = true;
      // Tell SelectionHandler what to do
      this._sendAsyncMessage("Browser:SelectionDebug", debugOpts);
    }
  },

  /*
   * _sendAsyncMessage
   *
   * Helper for sending a message to SelectionHandler.
   */
  _sendAsyncMessage: function _sendAsyncMessage(aMsg, aJson) {
    if (!this._msgTarget) {
      if (this._debugEvents)
        Util.dumpLn("SelectionHelperUI sendAsyncMessage could not send", aMsg);
      return;
    }
    this._msgTarget.messageManager.sendAsyncMessage(aMsg, aJson);
  },

  _checkForActiveDrag: function _checkForActiveDrag() {
    return (this.startMark.dragging || this.endMark.dragging ||
            this.caretMark.dragging);
  },

  _hitTestSelection: function _hitTestSelection(aEvent) {
    // Ignore if the double tap isn't on our active selection rect.
    if (this._activeSelectionRect &&
        Util.pointWithinRect(aEvent.clientX, aEvent.clientY, this._activeSelectionRect)) {
      return true;
    }
    return false;
  },

  /*
   * _setCaretPositionAtPoint - sets the current caret position.
   *
   * @param aX, aY - browser relative client coordinates
   */
  _setCaretPositionAtPoint: function _setCaretPositionAtPoint(aX, aY) {
    let json = this._getMarkerBaseMessage();
    json.change = "caret";
    json.caret.xPos = aX;
    json.caret.yPos = aY;
    this._sendAsyncMessage("Browser:CaretUpdate", json);
  },

  /*
   * _shutdownAllMarkers
   *
   * Helper for shutting down all markers and
   * freeing the objects associated with them.
   */
  _shutdownAllMarkers: function _shutdownAllMarkers() {
    if (this._startMark)
      this._startMark.shutdown();
    if (this._endMark)
      this._endMark.shutdown();
    if (this._caretMark)
      this._caretMark.shutdown();

    this._startMark = null;
    this._endMark = null;
    this._caretMark = null;
  },

  /*
   * _setupMonocleIdArray
   *
   * Helper for initing the array of monocle ids.
   */
  _setupMonocleIdArray: function _setupMonocleIdArray() {
    this._selectionMarkIds = ["selectionhandle-mark1",
                              "selectionhandle-mark2",
                              "selectionhandle-mark3"];
  },

  _hideMonocles: function _hideMonocles() {
    if (this._startMark) {
      this.startMark.hide();
    }
    if (this._endMark) {
      this.endMark.hide();
    }
    if (this._caretMark) {
      this.caretMark.hide();
    }
  },

  /*
   * Event handlers for document events
   */

  /*
   * _onTap
   *
   * Handles taps that move the current caret around in text edits,
   * clear active selection and focus when neccessary, or change
   * modes.
   * Future: changing selection modes by tapping on a monocle.
   */
  _onTap: function _onTap(aEvent) {
    let clientCoords =
      this._msgTarget.ptBrowserToClient(aEvent.clientX, aEvent.clientY, true);

    // Check for a tap on a monocle
    if (this.startMark.hitTest(clientCoords.x, clientCoords.y) ||
        this.endMark.hitTest(clientCoords.x, clientCoords.y)) {
      aEvent.stopPropagation();
      aEvent.preventDefault();
      return;
    }

    // Is the tap point within the bound of the target element? This
    // is useful if we are dealing with some sort of input control.
    // Not so much if the target is a page or div.
    let pointInTargetElement =
      Util.pointWithinRect(clientCoords.x, clientCoords.y,
                           this._targetElementRect);

    // If the tap is within an editable element and the caret monocle is
    // active, update the caret.
    if (this.caretMark.visible && pointInTargetElement) {
      // setCaretPositionAtPoint takes browser relative coords.
      this._setCaretPositionAtPoint(aEvent.clientX, aEvent.clientY);
      return;
    }

    // if the target is editable, we have selection or a caret, and the
    // user clicks off the target clear selection and remove focus from
    // the input.
    if (this._targetIsEditable && !pointInTargetElement) {
      // shutdown but leave focus alone. the event will fall through
      // and the dom will update focus for us. If the user tapped on
      // another input, we'll get a attachToCaret call soonish on the
      // new input.
      this.closeEditSession(false);
      return;
    }

    let selectionTap = this._hitTestSelection(aEvent);

    // If the tap is in the selection, just ignore it. We disallow this
    // since we always get a single tap before a double, and double tap
    // copies selected text.
    if (selectionTap) {
      aEvent.stopPropagation();
      aEvent.preventDefault();
      return;
    }

    // A tap within an editable but outside active selection, clear the
    // selection and flip back to caret mode.
    if (this.startMark.visible && pointInTargetElement &&
        this._targetIsEditable) {
      this._transitionFromSelectionToCaret(clientCoords.x, clientCoords.y);
    }

    // If we have active selection in anything else don't let the event get
    // to content. Prevents random taps from killing active selection.
    aEvent.stopPropagation();
    aEvent.preventDefault();
  },

  _onKeypress: function _onKeypress() {
    this.closeEditSession();
  },

  _onResize: function _onResize() {
    this._sendAsyncMessage("Browser:SelectionUpdate", {});
  },

  _onContextUIVisibilityEvent: function _onContextUIVisibilityEvent(aType) {
    // Manage display of monocles when the context ui is displayed.
    if (!this.isActive)
      return;
    this.overlay.hidden = (aType == "MozContextUIShow");
  },

  /*
   * _onDeckOffsetChanging - fired by ContentAreaObserver before the browser
   * deck is shifted for form input access in response to a soft keyboard
   * display.
   */
  _onDeckOffsetChanging: function _onDeckOffsetChanging(aEvent) {
    // Hide the monocles temporarily
    this._hideMonocles();
  },

  /*
   * _onDeckOffsetChanged - fired by ContentAreaObserver after the browser
   * deck is shifted for form input access in response to a soft keyboard
   * display.
   */
  _onDeckOffsetChanged: function _onDeckOffsetChanged(aEvent) {
    // Update the monocle position and display
    this.attachToCaret(null, this._lastPoint.xPos, this._lastPoint.yPos);
  },

  /*
   * Event handlers for message manager
   */

  _onDebugRectRequest: function _onDebugRectRequest(aMsg) {
    this.overlay.addDebugRect(aMsg.left, aMsg.top, aMsg.right, aMsg.bottom,
                              aMsg.color, aMsg.fill, aMsg.id);
  },

  _onSelectionCopied: function _onSelectionCopied(json) {
    this.closeEditSession(true);
  },

  _onSelectionRangeChange: function _onSelectionRangeChange(json) {
    let haveSelectionRect = true;

    // start and end contain client coordinates.
    if (json.updateStart) {
      this.startMark.position(this._msgTarget.btocx(json.start.xPos, true),
                              this._msgTarget.btocy(json.start.yPos, true));
      this.startMark.show();
    }
    if (json.updateEnd) {
      this.endMark.position(this._msgTarget.btocx(json.end.xPos, true),
                            this._msgTarget.btocy(json.end.yPos, true));
      this.endMark.show();
    }
    if (json.updateCaret) {
      // If selectionRangeFound is set SelectionHelper found a range we can
      // attach to. If not, there's no text in the control, and hence no caret
      // position information we can use.
      haveSelectionRect = json.selectionRangeFound;
      if (json.selectionRangeFound) {
        this.caretMark.position(this._msgTarget.btocx(json.caret.xPos, true),
                                this._msgTarget.btocy(json.caret.yPos, true));
        this.caretMark.show();
      }
    }

    this._targetIsEditable = json.targetIsEditable;
    this._activeSelectionRect = haveSelectionRect ?
      this._msgTarget.rectBrowserToClient(json.selection, true) :
      this._activeSelectionRect = Util.getCleanRect();
    this._targetElementRect =
      this._msgTarget.rectBrowserToClient(json.element, true);
  },

  _onSelectionFail: function _onSelectionFail() {
    Util.dumpLn("failed to get a selection.");
    this.closeEditSession();
  },

  /*
   * Events
   */

  handleEvent: function handleEvent(aEvent) {
    if (this._debugEvents && aEvent.type != "touchmove") {
      Util.dumpLn("SelectionHelperUI:", aEvent.type);
    }
    switch (aEvent.type) {
      case "click":
        this._onTap(aEvent);
        break;

      case "touchstart": {
        if (aEvent.touches.length != 1)
          break;
        let touch = aEvent.touches[0];
        this._movement.x = this._movement.y = 0;
        this._movement.x = touch.clientX;
        this._movement.y = touch.clientY;
        this._movement.active = true;
        break;
      }

      case "touchend":
        if (aEvent.touches.length == 0)
          this._movement.active = false;
        break;

      case "touchmove": {
        if (aEvent.touches.length != 1)
          break;
        let touch = aEvent.touches[0];
        // Clear our selection overlay when the user starts to pan the page
        if (!this._checkForActiveDrag() && this._movement.active) {
          let distanceY = touch.clientY - this._movement.y;
          if (Math.abs(distanceY) > kDisableOnScrollDistance) {
            this.closeEditSession(true);
          }
        }
        break;
      }

      case "keypress":
        this._onKeypress(aEvent);
        break;

      case "SizeChanged":
        this._onResize(aEvent);
        break;

      case "ZoomChanged":
      case "URLChanged":
      case "MozPrecisePointer":
        this.closeEditSession(true);
        break;

      case "MozContextUIShow":
      case "MozContextUIDismiss":
        this._onContextUIVisibilityEvent(aEvent.type);
        break;

      case "MozDeckOffsetChanging":
        this._onDeckOffsetChanging(aEvent);
        break;

      case "MozDeckOffsetChanged":
        this._onDeckOffsetChanged(aEvent);
        break;
    }
  },

  receiveMessage: function sh_receiveMessage(aMessage) {
    if (this._debugEvents) Util.dumpLn("SelectionHelperUI:", aMessage.name);
    let json = aMessage.json;
    switch (aMessage.name) {
      case "Content:SelectionFail":
        this._selectionHandlerActive = false;
        this._onSelectionFail();
        break;
      case "Content:SelectionRange":
        this._selectionHandlerActive = true;
        this._onSelectionRangeChange(json);
        break;
      case "Content:SelectionCopied":
        this._selectionHandlerActive = true;
        this._onSelectionCopied(json);
        break;
      case "Content:SelectionDebugRect":
        this._onDebugRectRequest(json);
        break;
    }
  },

  /*
   * Callbacks from markers
   */

  _getMarkerBaseMessage: function _getMarkerBaseMessage() {
    return {
      start: {
        xPos: this._msgTarget.ctobx(this.startMark.xPos, true),
        yPos: this._msgTarget.ctoby(this.startMark.yPos, true)
      },
      end: {
        xPos: this._msgTarget.ctobx(this.endMark.xPos, true),
        yPos: this._msgTarget.ctoby(this.endMark.yPos, true)
      },
      caret: {
        xPos: this._msgTarget.ctobx(this.caretMark.xPos, true),
        yPos: this._msgTarget.ctoby(this.caretMark.yPos, true)
      },
    };
  },

  markerDragStart: function markerDragStart(aMarker) {
    let json = this._getMarkerBaseMessage();
    json.change = aMarker.tag;
    if (aMarker.tag == "caret") {
      this._sendAsyncMessage("Browser:CaretMove", json);
      return;
    }
    this._sendAsyncMessage("Browser:SelectionMoveStart", json);
  },

  markerDragStop: function markerDragStop(aMarker) {
    let json = this._getMarkerBaseMessage();
    json.change = aMarker.tag;
    if (aMarker.tag == "caret") {
      this._sendAsyncMessage("Browser:CaretUpdate", json);
      return;
    }
    this._sendAsyncMessage("Browser:SelectionMoveEnd", json);
  },

  markerDragMove: function markerDragMove(aMarker, aDirection) {
    if (aMarker.tag == "caret") {
      // We are going to transition from caret browsing mode to selection
      // mode on drag. So swap the caret monocle for a start or end monocle
      // depending on the direction of the drag, and start selecting text.
      this._transitionFromCaretToSelection(aDirection);
      return false;
    }
    let json = this._getMarkerBaseMessage();
    json.change = aMarker.tag;
    this._sendAsyncMessage("Browser:SelectionMove", json);
    return true;
  },
};
