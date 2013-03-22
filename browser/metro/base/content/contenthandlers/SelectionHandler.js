/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

dump("### SelectionHandler.js loaded\n");

/*
  http://mxr.mozilla.org/mozilla-central/source/docshell/base/nsIDocShell.idl
  http://mxr.mozilla.org/mozilla-central/source/content/base/public/nsISelectionDisplay.idl
  http://mxr.mozilla.org/mozilla-central/source/content/base/public/nsISelectionListener.idl
  http://mxr.mozilla.org/mozilla-central/source/content/base/public/nsISelectionPrivate.idl
  http://mxr.mozilla.org/mozilla-central/source/content/base/public/nsISelectionController.idl
  http://mxr.mozilla.org/mozilla-central/source/content/base/public/nsISelection.idl
  http://mxr.mozilla.org/mozilla-central/source/dom/interfaces/core/nsIDOMDocument.idl#372
    rangeCount
    getRangeAt
    containsNode
  http://www.w3.org/TR/DOM-Level-2-Traversal-Range/ranges.html
  http://mxr.mozilla.org/mozilla-central/source/dom/interfaces/range/nsIDOMRange.idl
  http://mxr.mozilla.org/mozilla-central/source/dom/interfaces/core/nsIDOMDocument.idl#80
    content.document.createRange()
    getBoundingClientRect
    isPointInRange
  http://mxr.mozilla.org/mozilla-central/source/dom/interfaces/core/nsIDOMNode.idl
  http://mxr.mozilla.org/mozilla-central/source/dom/interfaces/base/nsIDOMWindowUtils.idl
    setSelectionAtPoint
  http://mxr.mozilla.org/mozilla-central/source/dom/interfaces/core/nsIDOMElement.idl
    getClientRect
  http://mxr.mozilla.org/mozilla-central/source/layout/generic/nsFrameSelection.h
  http://mxr.mozilla.org/mozilla-central/source/editor/idl/nsIEditor.idl
  http://mxr.mozilla.org/mozilla-central/source/dom/interfaces/base/nsIFocusManager.idl

*/

// selection node parameters for various apis
const kSelectionNodeAnchor = 1;
const kSelectionNodeFocus = 2;

var SelectionHandler = {
  _debugEvents: false,
  _cache: {},
  _targetElement: null,
  _targetIsEditable: false,
  _contentWindow: null,
  _contentOffset: { x:0, y:0 },
  _domWinUtils: null,
  _selectionMoveActive: false,
  _lastMarker: "",
  _debugOptions: { dumpRanges: false, displayRanges: false },
  _snap: true,

  init: function init() {
    addMessageListener("Browser:SelectionStart", this);
    addMessageListener("Browser:SelectionAttach", this);
    addMessageListener("Browser:SelectionEnd", this);
    addMessageListener("Browser:SelectionMoveStart", this);
    addMessageListener("Browser:SelectionMove", this);
    addMessageListener("Browser:SelectionMoveEnd", this);
    addMessageListener("Browser:SelectionUpdate", this);
    addMessageListener("Browser:SelectionClose", this);
    addMessageListener("Browser:SelectionClear", this);
    addMessageListener("Browser:SelectionCopy", this);
    addMessageListener("Browser:SelectionDebug", this);
    addMessageListener("Browser:CaretAttach", this);
    addMessageListener("Browser:CaretMove", this);
    addMessageListener("Browser:CaretUpdate", this);
    addMessageListener("Browser:SelectionSwitchMode", this);
  },

  shutdown: function shutdown() {
    removeMessageListener("Browser:SelectionStart", this);
    removeMessageListener("Browser:SelectionAttach", this);
    removeMessageListener("Browser:SelectionEnd", this);
    removeMessageListener("Browser:SelectionMoveStart", this);
    removeMessageListener("Browser:SelectionMove", this);
    removeMessageListener("Browser:SelectionMoveEnd", this);
    removeMessageListener("Browser:SelectionUpdate", this);
    removeMessageListener("Browser:SelectionClose", this);
    removeMessageListener("Browser:SelectionClear", this);
    removeMessageListener("Browser:SelectionCopy", this);
    removeMessageListener("Browser:SelectionDebug", this);
    removeMessageListener("Browser:CaretAttach", this);
    removeMessageListener("Browser:CaretMove", this);
    removeMessageListener("Browser:CaretUpdate", this);
    removeMessageListener("Browser:SelectionSwitchMode", this);
  },

  /*************************************************
   * Properties
   */

  /*
   * snap - enable or disable word snap for the active marker when a
   * SelectionMoveEnd event is received. Typically you would disable
   * snap when zoom is < 1.0 for precision selection.
   */
  get snap() {
    return this._snap;
  },

  /*************************************************
   * Browser event handlers
   */

  /*
   * Selection start event handler
   */
  _onSelectionStart: function _onSelectionStart(aX, aY) {
    // Init content window information
    if (!this._initTargetInfo(aX, aY)) {
      this._onFail("failed to get target information");
      return;
    }

    // Clear any existing selection from the document
    let selection = this._contentWindow.getSelection();
    selection.removeAllRanges();

    // Set our initial selection, aX and aY should be in client coordinates.
    let framePoint = this._clientPointToFramePoint({ xPos: aX, yPos: aY });
    if (!this._domWinUtils.selectAtPoint(framePoint.xPos, framePoint.yPos,
                                         Ci.nsIDOMWindowUtils.SELECT_WORDNOSPACE)) {
      this._onFail("failed to set selection at point");
      return;
    }

    // Update the position of our selection monocles
    this._updateSelectionUI(true, true);
  },

  _onSelectionAttach: function _onSelectionAttach(aX, aY) {
    // Init content window information
    if (!this._initTargetInfo(aX, aY)) {
      this._onFail("failed to get frame offset");
      return;
    }

    // Update the position of our selection monocles
    this._updateSelectionUI(true, true);
  },

  /*
   * Switch selection modes. Currently we only support switching
   * from "caret" to "selection".
   */
  _onSwitchMode: function _onSwitchMode(aMode, aMarker, aX, aY) {
    if (aMode != "selection") {
      this._onFail("unsupported mode switch");
      return;
    }
    
    // Sanity check to be sure we are initialized
    if (!this._targetElement) {
      this._onFail("not initialized");
      return;
    }

    // Similar to _onSelectionStart - we need to create initial selection
    // but without the initialization bits.
    let framePoint = this._clientPointToFramePoint({ xPos: aX, yPos: aY });
    if (!this._domWinUtils.selectAtPoint(framePoint.xPos, framePoint.yPos,
                                         Ci.nsIDOMWindowUtils.SELECT_CHARACTER)) {
      this._onFail("failed to set selection at point");
      return;
    }

    // We bail if things get out of sync here implying we missed a message.
    this._selectionMoveActive = true;

    // Update the position of the selection marker that is *not*
    // being dragged.
    this._updateSelectionUI(aMarker == "end", aMarker == "start");
  },

  /*
   * Selection monocle start move event handler
   */
  _onSelectionMoveStart: function _onSelectionMoveStart(aMsg) {
    if (!this._contentWindow) {
      this._onFail("_onSelectionMoveStart was called without proper view set up");
      return;
    }

    if (this._selectionMoveActive) {
      this._onFail("mouse is already down on drag start?");
      return;
    }

    // We bail if things get out of sync here implying we missed a message.
    this._selectionMoveActive = true;

    // Update the position of our selection monocles
    this._updateSelectionUI(true, true);
  },
  
  /*
   * Selection monocle move event handler
   */
  _onSelectionMove: function _onSelectionMove(aMsg) {
    if (!this._contentWindow) {
      this._onFail("_onSelectionMove was called without proper view set up");
      return;
    }

    if (!this._selectionMoveActive) {
      this._onFail("mouse isn't down for drag move?");
      return;
    }

    // Update selection in the doc
    let pos = null;
    if (aMsg.change == "start") {
      pos = aMsg.start;
    } else {
      pos = aMsg.end;
    }
    this._handleSelectionPoint(aMsg.change, pos, false);
  },

  /*
   * Selection monocle move finished event handler
   */
  _onSelectionMoveEnd: function _onSelectionMoveComplete(aMsg) {
    if (!this._contentWindow) {
      this._onFail("_onSelectionMove was called without proper view set up");
      return;
    }

    if (!this._selectionMoveActive) {
      this._onFail("mouse isn't down for drag move?");
      return;
    }

    // Update selection in the doc
    let pos = null;
    if (aMsg.change == "start") {
      pos = aMsg.start;
    } else {
      pos = aMsg.end;
    }

    this._handleSelectionPoint(aMsg.change, pos, true);
    this._selectionMoveActive = false;
    
    // _handleSelectionPoint may set a scroll timer, so this must
    // be reset after the last call.
    this._clearTimers();

    // Update the position of our selection monocles
    this._updateSelectionUI(true, true);
  },

   /*
    * _onCaretAttach - called by SelectionHelperUI when the user taps in a
    * form input. Initializes SelectionHandler, updates the location of the
    * caret, and messages back with current monocle position information.
    *
    * @param aX, aY tap location in client coordinates.
    */
  _onCaretAttach: function _onCaretAttach(aX, aY) {
    // Init content window information
    if (!this._initTargetInfo(aX, aY)) {
      this._onFail("failed to get target information");
      return;
    }

    // This should never happen, but we check to make sure
    if (!this._targetIsEditable || !Util.isTextInput(this._targetElement)) {
      this._onFail("Unexpected, coordiates didn't find a text input element.");
      return;
    }

    // Locate and sanity check the caret position
    let selection = this._getSelection();
    if (!selection || !selection.isCollapsed) {
      this._onFail("Unexpected, No selection or selection is not collapsed.");
      return;
    }

    // Update the position of our selection monocles
    this._updateSelectionUI(false, false, true);
  },

   /*
    * _onCaretPositionUpdate - sets the current caret location based on
    * a client coordinates. Messages back with updated monocle position
    * information.
    *
    * @param aX, aY drag location in client coordinates.
    */
  _onCaretPositionUpdate: function _onCaretPositionUpdate(aX, aY) {
    this._onCaretMove(aX, aY);

    // Update the position of our selection monocles
    this._updateSelectionUI(false, false, true);
  },

   /*
    * _onCaretMove - updates the current caret location based on a client
    * coordinates.
    *
    * @param aX, aY drag location in client coordinates.
    */
  _onCaretMove: function _onCaretMove(aX, aY) {
    if (!this._targetIsEditable) {
      this._onFail("Unexpected, caret position isn't supported with non-inputs.");
      return;
    }
    let cp = this._contentWindow.document.caretPositionFromPoint(aX, aY);
    let input = cp.offsetNode;
    let offset = cp.offset;
    input.selectionStart = input.selectionEnd = offset;
  },

  /*
   * Selection copy event handler
   *
   * Check to see if the incoming click was on our selection rect.
   * if it was, copy to the clipboard. Incoming coordinates are
   * content values.
   */
  _onSelectionCopy: function _onSelectionCopy(aMsg) {
    let tap = {
      xPos: aMsg.xPos,
      yPos: aMsg.yPos,
    };

    let tapInSelection = (tap.xPos > this._cache.selection.left &&
                          tap.xPos < this._cache.selection.right) &&
                         (tap.yPos > this._cache.selection.top &&
                          tap.yPos < this._cache.selection.bottom);
    // Util.dumpLn(tapInSelection,
    //             tap.xPos, tap.yPos, "|", this._cache.selection.left,
    //             this._cache.selection.right, this._cache.selection.top,
    //             this._cache.selection.bottom);
    let success = false;
    let selectedText = this._getSelectedText();
    if (tapInSelection && selectedText.length) {
      let clipboard = Cc["@mozilla.org/widget/clipboardhelper;1"]
                        .getService(Ci.nsIClipboardHelper);
      clipboard.copyString(selectedText, this._contentWindow.document);
      success = true;
    }
    sendSyncMessage("Content:SelectionCopied", { succeeded: success });
  },

  /*
   * Selection close event handler
   */
  _onSelectionClose: function _onSelectionClose() {
    this._closeSelection();
  },

  /*
   * Selection clear message handler
   *
   * @param aClearFocus requests that the focus also be cleared.
   */
  _onSelectionClear: function _onSelectionClear(aClearFocus) {
    this._clearSelection();
    if (aClearFocus && this._targetElement) {
      this._targetElement.blur();
    }
  },

  /*
   * Called any time SelectionHelperUI would like us to
   * recalculate the selection bounds.
   */
  _onSelectionUpdate: function _onSelectionUpdate() {
    if (!this._contentWindow) {
      this._onFail("_onSelectionUpdate was called without proper view set up");
      return;
    }

    // Update the position of our selection monocles
    this._updateSelectionUI(true, true);
  },

  /*
   * Called if for any reason we fail during the selection
   * process. Cancels the selection.
   */
  _onFail: function _onFail(aDbgMessage) {
    if (aDbgMessage && aDbgMessage.length > 0)
      Util.dumpLn(aDbgMessage);
    sendAsyncMessage("Content:SelectionFail");
    this._clearSelection();
    this._closeSelection();
  },

  /*
   * Turning on or off various debug featues.
   */
  _onSelectionDebug: function _onSelectionDebug(aMsg) {
    this._debugOptions = aMsg;
    this._debugEvents = aMsg.dumpEvents;
  },

  /*************************************************
   * Selection helpers
   */

  /*
   * _clearSelection
   *
   * Clear existing selection if it exists and reset our internla state.
   */
  _clearSelection: function _clearSelection() {
    this._clearTimers();
    if (this._contentWindow) {
      let selection = this._getSelection();
      if (selection)
        selection.removeAllRanges();
    } else {
      let selection = content.getSelection();
      if (selection)
        selection.removeAllRanges();
    }
    this.selectedText = "";
  },

  /*
   * _closeSelection
   *
   * Shuts SelectionHandler down.
   */
  _closeSelection: function _closeSelection() {
    this._clearTimers();
    this._cache = null;
    this._contentWindow = null;
    this.selectedText = "";
    this._selectionMoveActive = false;
  },

  /*
   * _updateSelectionUI
   *
   * Informs SelectionHelperUI about selection marker position
   * so that our selection monocles can be positioned properly.
   *
   * @param aUpdateStart bool update start marker position
   * @param aUpdateEnd bool update end marker position
   * @param aUpdateCaret bool update caret marker position, can be
   * undefined, defaults to false.
   */
  _updateSelectionUI: function _updateSelectionUI(aUpdateStart, aUpdateEnd,
                                                  aUpdateCaret) {
    let selection = this._getSelection();

    // If the range didn't have any text, let's bail
    if (!selection) {
      this._onFail("no selection was present");
      return;
    }

    // Updates this._cache content selection position data which we send over
    // to SelectionHelperUI. Note updateUIMarkerRects will fail if there isn't
    // any selection in the page. This can happen when we start a monocle drag
    // but haven't dragged enough to create selection. Just return. 
    try {
      this._updateUIMarkerRects(selection);
    } catch (ex) {
      Util.dumpLn(ex.message);
      return;
    }

    this._cache.updateStart = aUpdateStart;
    this._cache.updateEnd = aUpdateEnd;
    this._cache.updateCaret = aUpdateCaret || false;
    this._cache.targetIsEditable = this._targetIsEditable;

    // Get monocles positioned correctly
    sendAsyncMessage("Content:SelectionRange", this._cache);
  },

  /*
   * Find content within frames - cache the target nsIDOMWindow,
   * client coordinate offset, target element, and dom utils interface.
   */
  _initTargetInfo: function _initTargetInfo(aX, aY) {
    // getCurrentWindowAndOffset takes client coordinates
    let { element: element,
          contentWindow: contentWindow,
          offset: offset,
          utils: utils } =
      this.getCurrentWindowAndOffset(aX, aY);
    if (!contentWindow) {
      return false;
    }
    this._targetElement = element;
    this._contentWindow = contentWindow;
    this._contentOffset = offset;
    this._domWinUtils = utils;
    this._targetIsEditable = false;
    if (this._isTextInput(this._targetElement)) {
      this._targetIsEditable = true;
      // Since we have an overlay, focus will not get set, so set it. There
      // are ways around this if this causes trouble - we have the selection
      // controller, so we can turn selection display on manually. (Selection
      // display is setup on edits when focus changes.) I think web pages will
      // prefer that focus be set when we are interacting with selection in
      // the element.
      this._targetElement.focus();
    }
    return true;
  },

  /*
   * _updateUIMarkerRects(aSelection)
   *
   * Extracts the rects of the current selection, clips them to any text
   * input bounds, and stores them in the cache table we send over to
   * SelectionHelperUI.
   */
  _updateUIMarkerRects: function _updateUIMarkerRects(aSelection) {
    this._cache = this._extractClientRectFromRange(aSelection.getRangeAt(0));
    if (this. _debugOptions.dumpRanges)  {
       Util.dumpLn("start:", "(" + this._cache.start.xPos + "," +
                   this._cache.start.yPos + ")");
       Util.dumpLn("end:", "(" + this._cache.end.xPos + "," +
                   this._cache.end.yPos + ")");
       Util.dumpLn("caret:", "(" + this._cache.caret.xPos + "," +
                   this._cache.caret.yPos + ")");
    }
    this._restrictSelectionRectToEditBounds();
  },

  /*
   * Selection bounds will fall outside the bound of a control if the control
   * can scroll. Clip UI cache data to the bounds of the target so monocles
   * don't draw outside the control.
   */
  _restrictSelectionRectToEditBounds: function _restrictSelectionRectToEditBounds() {
    if (!this._targetIsEditable)
      return;
    let bounds = this._getTargetClientRect();
    if (this._cache.start.xPos < bounds.left)
      this._cache.start.xPos = bounds.left;
    if (this._cache.end.xPos < bounds.left)
      this._cache.end.xPos = bounds.left;
    if (this._cache.caret.xPos < bounds.left)
      this._cache.caret.xPos = bounds.left;
    if (this._cache.start.xPos > bounds.right)
      this._cache.start.xPos = bounds.right;
    if (this._cache.end.xPos > bounds.right)
      this._cache.end.xPos = bounds.right;
    if (this._cache.caret.xPos > bounds.right)
      this._cache.caret.xPos = bounds.right;

    if (this._cache.start.yPos < bounds.top)
      this._cache.start.yPos = bounds.top;
    if (this._cache.end.yPos < bounds.top)
      this._cache.end.yPos = bounds.top;
    if (this._cache.caret.yPos < bounds.top)
      this._cache.caret.yPos = bounds.top;
    if (this._cache.start.yPos > bounds.bottom)
      this._cache.start.yPos = bounds.bottom;
    if (this._cache.end.yPos > bounds.bottom)
      this._cache.end.yPos = bounds.bottom;
    if (this._cache.caret.yPos > bounds.bottom)
      this._cache.caret.yPos = bounds.bottom;
  },

  /*
   * _handleSelectionPoint(aMarker, aPoint, aEndOfSelection) 
   *
   * After a monocle moves to a new point in the document, determines
   * what the target is and acts on its selection accordingly. If the
   * monocle is within the bounds of the target, adds or subtracts selection
   * at the monocle coordinates appropriately and then merges selection ranges
   * into a single continuous selection. If the monocle is outside the bounds
   * of the target and the underlying target is editable, uses the selection
   * controller to advance selection and visibility within the control.
   */
  _handleSelectionPoint: function _handleSelectionPoint(aMarker, aClientPoint,
                                                        aEndOfSelection) {
    let selection = this._getSelection();

    let clientPoint = { xPos: aClientPoint.xPos, yPos: aClientPoint.yPos };

    if (selection.rangeCount == 0) {
      this._onFail("selection.rangeCount == 0");
      return;
    }

    // We expect continuous selection ranges.
    if (selection.rangeCount > 1) {
      this._setContinuousSelection();
    }

    // Adjust our y position up such that we are sending coordinates on
    // the text line vs. below it where the monocle is positioned. This
    // applies to free floating text areas. For text inputs we'll constrain
    // coordinates further below.
    let halfLineHeight = this._queryHalfLineHeight(aMarker, selection);
    clientPoint.yPos -= halfLineHeight;

    if (this._targetIsEditable) {
      // Check to see if we are beyond the bounds of selection in a input
      // control. If we are we want to add selection and scroll the added
      // selection into view.
      let result = this.updateTextEditSelection(clientPoint);

      // If we're targeting a text input of any kind, make sure clientPoint
      // is contained within the bounds of the text control. For example, if
      // a user drags up too close to an upper bounds, selectAtPoint might
      // select the content above the control. This looks crappy and breaks
      // our selection rect management.
      clientPoint =
       this._constrainPointWithinControl(clientPoint, halfLineHeight);

      // If result.trigger is true, the monocle is outside the bounds of the
      // control. If it's false, fall through to our additive text selection
      // below.
      if (result.trigger) {
        // _handleSelectionPoint is triggered by input movement, so if we've
        // tested positive for out-of-bounds scrolling here, we need to set a
        // recurring timer to keep the expected selection behavior going as
        // long as the user keeps the monocle out of bounds.
        if (!this._scrollTimer)
          this._scrollTimer = new Util.Timeout();
        this._setTextEditUpdateInterval(result.speed);

        // Smooth the selection
        this._setContinuousSelection();

        // Update the other monocle's position if we've dragged off to one side
        this._updateSelectionUI(result.start, result.end);

        return;
      }
    }

    this._lastMarker = aMarker;

    // If we aren't out-of-bounds, clear the scroll timer if it exists.
    this._clearTimers();

    // Adjusts the selection based on monocle movement
    this._adjustSelection(aMarker, clientPoint, aEndOfSelection);

    // Update the other monocle's position. We do this because the dragging
    // monocle may reset the static monocle to a new position if the dragging
    // monocle drags ahead or behind the other.
    if (aMarker == "start") {
      this._updateSelectionUI(false, true);
    } else {
      this._updateSelectionUI(true, false);
    }
  },

  /*
   * _handleSelectionPoint helper methods
   */

  /*
   * Based on a monocle marker and position, adds or subtracts from the
   * existing selection.
   *
   * @param the marker currently being manipulated
   * @param aClientPoint the point designating the new start or end
   * position for the selection.
   * @param aEndOfSelection indicates if this is the end of a selection
   * move, in which case we may want to snap to the end of a word or
   * sentence.
   */
  _adjustSelection: function _adjustSelection(aMarker, aClientPoint,
                                              aEndOfSelection) {
    // Make a copy of the existing range, we may need to reset it.
    this._backupRangeList();

    // shrinkSelectionFromPoint takes sub-frame relative coordinates.
    let framePoint = this._clientPointToFramePoint(aClientPoint);

    // Tests to see if the user is trying to shrink the selection, and if so
    // collapses it down to the appropriate side such that our calls below
    // will reset the selection to the proper range.
    let shrunk = this._shrinkSelectionFromPoint(aMarker, framePoint);

    let selectResult = false;
    try {
      // If we're at the end of a selection (touchend) snap to the word.
      let type = ((aEndOfSelection && this._snap) ?
        Ci.nsIDOMWindowUtils.SELECT_WORD :
        Ci.nsIDOMWindowUtils.SELECT_CHARACTER);

      // Select a character at the point.
      selectResult = 
        this._domWinUtils.selectAtPoint(framePoint.xPos,
                                        framePoint.yPos,
                                        type);
    } catch (ex) {
    }

    // If selectAtPoint failed (which can happen if there's nothing to select)
    // reset our range back before we shrunk it.
    if (!selectResult) {
      this._restoreRangeList();
    }

    this._freeRangeList();

    // Smooth over the selection between all existing ranges.
    this._setContinuousSelection();
  },

  /*
   * _backupRangeList, _restoreRangeList, and _freeRangeList
   *
   * Utilities that manage a cloned copy of the existing selection.
   */

  _backupRangeList: function _backupRangeList() {
    this._rangeBackup = new Array();
    for (let idx = 0; idx < this._getSelection().rangeCount; idx++) {
      this._rangeBackup.push(this._getSelection().getRangeAt(idx).cloneRange());
    }
  },

  _restoreRangeList: function _restoreRangeList() {
    if (this._rangeBackup == null)
      return;
    for (let idx = 0; idx < this._rangeBackup.length; idx++) {
      this._getSelection().addRange(this._rangeBackup[idx]);
    }
    this._freeRangeList();
  },

  _freeRangeList: function _restoreRangeList() {
    this._rangeBackup = null;
  },

  /*
   * Constrains a selection point within a text input control bounds.
   *
   * @param aPoint - client coordinate point
   * @param aHalfLineHeight - half the line height at the point
   * @return new constrained point struct
   */
  _constrainPointWithinControl: function _cpwc(aPoint, aHalfLineHeight) {
    let bounds = this._getTargetClientRect();
    let point = { xPos: aPoint.xPos, yPos: aPoint.yPos };
    if (point.xPos <= bounds.left)
      point.xPos = bounds.left + 2;
    if (point.xPos >= bounds.right)
      point.xPos = bounds.right - 2;
    if (point.yPos <= (bounds.top + aHalfLineHeight))
      point.yPos = (bounds.top + aHalfLineHeight);
    if (point.yPos >= (bounds.bottom - aHalfLineHeight))
      point.yPos = (bounds.bottom - aHalfLineHeight);
    return point;
  },

  /*
   * _pointOrientationToRect(aPoint, aRect)
   *
   * Returns a table representing which sides of target aPoint is offset
   * from: { left: offset, top: offset, right: offset, bottom: offset }
   * Works on client coordinates.
   */
  _pointOrientationToRect: function _pointOrientationToRect(aPoint) {
    let bounds = this._targetElement.getBoundingClientRect();
    let result = { left: 0, right: 0, top: 0, bottom: 0 };
    if (aPoint.xPos <= bounds.left)
      result.left = bounds.left - aPoint.xPos;
    if (aPoint.xPos >= bounds.right)
      result.right = aPoint.xPos - bounds.right;
    if (aPoint.yPos <= bounds.top)
      result.top = bounds.top - aPoint.yPos;
    if (aPoint.yPos >= bounds.bottom)
      result.bottom = aPoint.yPos - bounds.bottom;
    return result;
  },

  /*
   * updateTextEditSelection(aPoint, aClientPoint)
   *
   * Checks to see if the monocle point is outside the bounds of the
   * target edit. If so, use the selection controller to select and
   * scroll the edit appropriately.
   *
   * @param aClientPoint raw pointer position
   * @return { speed: 0.0 -> 1.0,
   *           trigger: true/false if out of bounds,
   *           start: true/false if updated position,
   *           end: true/false if updated position }
   */
  updateTextEditSelection: function updateTextEditSelection(aClientPoint) {
    if (aClientPoint == undefined) {
      aClientPoint = this._rawSelectionPoint;
    }
    this._rawSelectionPoint = aClientPoint;

    let orientation = this._pointOrientationToRect(aClientPoint);
    let result = { speed: 1, trigger: false, start: false, end: false };

    if (orientation.left || orientation.top) {
      this._addEditSelection(kSelectionNodeAnchor);
      result.speed = orientation.left + orientation.top;
      result.trigger = true;
      result.end = true;
    } else if (orientation.right || orientation.bottom) {
      this._addEditSelection(kSelectionNodeFocus);
      result.speed = orientation.right + orientation.bottom;
      result.trigger = true;
      result.start = true;
    }

    // 'speed' is just total pixels offset, so clamp it to something
    // reasonable callers can work with.
    if (result.speed > 100)
      result.speed = 100;
    if (result.speed < 1)
      result.speed = 1;
    result.speed /= 100;
    return result;
  },

  _setTextEditUpdateInterval: function _setTextEditUpdateInterval(aSpeedValue) {
    let timeout = (75 - (aSpeedValue * 75));
    this._scrollTimer.interval(timeout, this.scrollTimerCallback);
  },

  _clearTimers: function _clearTimers() {
    if (this._scrollTimer) {
      this._scrollTimer.clear();
    }
  },

  /*
   * _addEditSelection - selection control call wrapper for text inputs.
   * Adds selection on the anchor or focus side of selection in a text
   * input. Scrolls the location into view as well.
   * (TBD: anchor side scrolling is currently broken, see bug 848594)
   *
   * @param const selection node identifier
   */
  _addEditSelection: function _addEditSelection(aLocation) {
    let selCtrl = this._getSelectController();
    try {
      if (aLocation == kSelectionNodeAnchor) {
        this._targetElement.selectionStart = this._targetElement.selectionStart - 1;
        selCtrl.scrollSelectionIntoView(Ci.nsISelectionController.SELECTION_NORMAL,
                                        Ci.nsISelectionController.SELECTION_ANCHOR_REGION,
                                        Ci.nsISelectionController.SCROLL_SYNCHRONOUS);
      } else {
        this._targetElement.selectionEnd = this._targetElement.selectionEnd + 1;
        selCtrl.scrollSelectionIntoView(Ci.nsISelectionController.SELECTION_NORMAL,
                                        Ci.nsISelectionController.SELECTION_FOCUS_REGION,
                                        Ci.nsISelectionController.SCROLL_SYNCHRONOUS);
      }
    } catch (ex) {}
  },

  /*
   * _queryHalfLineHeight(aMarker, aSelection)
   *
   * Y offset applied to the coordinates of the selection position we send
   * to dom utils. The selection marker sits below text, but we want the
   * selection position to be on the text above the monocle. Since text
   * height can vary across the entire selection range, we need the correct
   * height based on the line the marker in question is moving on.
   */
  _queryHalfLineHeight: function _queryHalfLineHeight(aMarker, aSelection) {
    let rects = aSelection.getRangeAt(0).getClientRects();
    if (!rects.length) {
      return 0;
    }

    // We are assuming here that these rects are ordered correctly.
    // From looking at the range code it appears they will be.
    let height = 0;
    if (aMarker == "start") {
      // height of the first rect corresponding to the start marker:
      height = rects[0].bottom - rects[0].top;
    } else {
      // height of the last rect corresponding to the end marker:
      let len = rects.length - 1;
      height = rects[len].bottom - rects[len].top;
    }
    return height / 2;
  },

  /*
   * _setContinuousSelection()
   *
   * Smooths a selection with multiple ranges into a single
   * continuous range.
   */
  _setContinuousSelection: function _setContinuousSelection() {
    let selection = this._getSelection();
    try {
      if (selection.rangeCount > 1) {
        let startRange = selection.getRangeAt(0);
        if (this. _debugOptions.displayRanges) {
          let clientRect = startRange.getBoundingClientRect();
          this._setDebugRect(clientRect, "red", false);
        }
        let newStartNode = null;
        let newStartOffset = 0;
        let newEndNode = null;
        let newEndOffset = 0;
        for (let idx = 1; idx < selection.rangeCount; idx++) {
          let range = selection.getRangeAt(idx);
          switch (startRange.compareBoundaryPoints(Ci.nsIDOMRange.START_TO_START, range)) {
            case -1: // startRange is before
              newStartNode = startRange.startContainer;
              newStartOffset = startRange.startOffset;
              break;
            case 0: // startRange is equal
              newStartNode = startRange.startContainer;
              newStartOffset = startRange.startOffset;
              break;
            case 1: // startRange is after
              newStartNode = range.startContainer;
              newStartOffset = range.startOffset;
              break;
          }
          switch (startRange.compareBoundaryPoints(Ci.nsIDOMRange.END_TO_END, range)) {
            case -1: // startRange is before
              newEndNode = range.endContainer;
              newEndOffset = range.endOffset;
              break;
            case 0: // startRange is equal
              newEndNode = range.endContainer;
              newEndOffset = range.endOffset;
              break;
            case 1: // startRange is after
              newEndNode = startRange.endContainer;
              newEndOffset = startRange.endOffset;
              break;
          }
          if (this. _debugOptions.displayRanges) {
            let clientRect = range.getBoundingClientRect();
            this._setDebugRect(clientRect, "orange", false);
          }
        }
        let range = content.document.createRange();
        range.setStart(newStartNode, newStartOffset);
        range.setEnd(newEndNode, newEndOffset);
        selection.addRange(range);
      }
    } catch (ex) {
      Util.dumpLn("exception while modifying selection:", ex.message);
      this._onFail("_handleSelectionPoint failed.");
      return false;
    }
    return true;
  },

  /*
   * _shrinkSelectionFromPoint(aMarker, aFramePoint)
   *
   * Tests to see if aFramePoint intersects the current selection and if so,
   * collapses selection down to the opposite start or end point leaving a
   * character of selection at the collapse point.
   *
   * @param aMarker the marker that is being relocated. ("start" or "end")
   * @param aFramePoint position of the marker.
   */
  _shrinkSelectionFromPoint: function _shrinkSelectionFromPoint(aMarker, aFramePoint) {
    let result = false;
    try {
      let selection = this._getSelection();
      for (let range = 0; range < selection.rangeCount; range++) {
        // relative to frame
        let rects = selection.getRangeAt(range).getClientRects();
        for (let idx = 0; idx < rects.length; idx++) {
          // Util.dumpLn("[" + idx + "]", aFramePoint.xPos, aFramePoint.yPos, rects[idx].left,
          // rects[idx].top, rects[idx].right, rects[idx].bottom);
          if (Util.pointWithinDOMRect(aFramePoint.xPos, aFramePoint.yPos, rects[idx])) {
            result = true;
            if (aMarker == "start") {
              selection.collapseToEnd();
            } else {
              selection.collapseToStart();
            }
            // collapseToStart and collapseToEnd leave an empty range in the
            // selection at the collapse point. Therefore we need to add some
            // selection such that the selection added by selectAtPoint and
            // the current empty range will get merged properly when we smooth
            // the selection ranges out.
            let selCtrl = this._getSelectController();
            // Expand the collapsed range such that it occupies a little space.
            if (aMarker == "start") {
              // State: focus = anchor (collapseToEnd does this)
              selCtrl.characterMove(false, true);
              // State: focus = (anchor - 1)
              selection.collapseToStart();
              // State: focus = anchor and both are -1 from the original offset
              selCtrl.characterMove(true, true);
              // State: focus = anchor + 1, both have been moved back one char
            } else {
              selCtrl.characterMove(true, true);
            }
            break;
          }
        }
      }
    } catch (ex) {
      Util.dumpLn("error shrinking selection:", ex.message);
    }
    return result;
  },

  /*
   * Events
   */

  /*
   * Scroll + selection advancement timer when the monocle is
   * outside the bounds of an input control.
   */
  scrollTimerCallback: function scrollTimerCallback() {
    let result = SelectionHandler.updateTextEditSelection();
    // Update monocle position and speed if we've dragged off to one side
    if (result.trigger) {
      if (result.start)
        SelectionHandler._updateSelectionUI(true, false);
      if (result.end)
        SelectionHandler._updateSelectionUI(false, true);
    }
  },

  receiveMessage: function sh_receiveMessage(aMessage) {
    if (this._debugEvents && aMessage.name != "Browser:SelectionMove") {
      Util.dumpLn("SelectionHandler:", aMessage.name);
    }
    let json = aMessage.json;
    switch (aMessage.name) {
      case "Browser:SelectionStart":
        this._onSelectionStart(json.xPos, json.yPos);
        break;

      case "Browser:SelectionAttach":
        this._onSelectionAttach(json.xPos, json.yPos);
        break;

      case "Browser:CaretAttach":
        this._onCaretAttach(json.xPos, json.yPos);
        break;

      case "Browser:CaretMove":
        this._onCaretMove(json.caret.xPos, json.caret.yPos);
        break;

      case "Browser:CaretUpdate":
        this._onCaretPositionUpdate(json.caret.xPos, json.caret.yPos);
        break;

      case "Browser:SelectionSwitchMode":
        this._onSwitchMode(json.newMode, json.change, json.xPos, json.yPos);
        break;

      case "Browser:SelectionClose":
        this._onSelectionClose();
        break;

      case "Browser:SelectionMoveStart":
        this._onSelectionMoveStart(json);
        break;

      case "Browser:SelectionMove":
        this._onSelectionMove(json);
        break;

      case "Browser:SelectionMoveEnd":
        this._onSelectionMoveEnd(json);
        break;

      case "Browser:SelectionCopy":
        this._onSelectionCopy(json);
        break;

      case "Browser:SelectionClear":
        this._onSelectionClear(json.clearFocus);
        break;

      case "Browser:SelectionDebug":
        this._onSelectionDebug(json);
        break;

      case "Browser:SelectionUpdate":
        this._onSelectionUpdate();
        break;
    }
  },

  /*
   * Utilities
   */

  /*
   * Returns data on the position of a selection using the relative
   * coordinates in a range extracted from any sub frames. If aRange
   * is in the root frame offset should be zero. 
   */
  _extractClientRectFromRange: function _extractClientRectFromRange(aRange) {
    let cache = {
      start: {}, end: {}, caret: {},
      selection: { left: 0, top: 0, right: 0, bottom: 0 },
      element: { left: 0, top: 0, right: 0, bottom: 0 }
    };

    // When in an iframe, aRange coordinates are relative to the frame origin.
    let rects = aRange.getClientRects();

    let startSet = false;
    for (let idx = 0; idx < rects.length; idx++) {
      if (this. _debugOptions.dumpRanges) Util.dumpDOMRect(idx, rects[idx]);
      if (!startSet && !Util.isEmptyDOMRect(rects[idx])) {
        cache.start.xPos = rects[idx].left + this._contentOffset.x;
        cache.start.yPos = rects[idx].bottom + this._contentOffset.y;
        cache.caret = cache.start;
        startSet = true;
        if (this. _debugOptions.dumpRanges) Util.dumpLn("start set");
      }
      if (!Util.isEmptyDOMRect(rects[idx])) {
        cache.end.xPos = rects[idx].right + this._contentOffset.x;
        cache.end.yPos = rects[idx].bottom + this._contentOffset.y;
        if (this. _debugOptions.dumpRanges) Util.dumpLn("end set");
      }
    }

    // Store the client rect of selection
    let r = aRange.getBoundingClientRect();
    cache.selection.left = r.left + this._contentOffset.x;
    cache.selection.top = r.top + this._contentOffset.y;
    cache.selection.right = r.right + this._contentOffset.x;
    cache.selection.bottom = r.bottom + this._contentOffset.y;

    // Store the client rect of target element
    r = this._getTargetClientRect();
    cache.element.left = r.left + this._contentOffset.x;
    cache.element.top = r.top + this._contentOffset.y;
    cache.element.right = r.right + this._contentOffset.x;
    cache.element.bottom = r.bottom + this._contentOffset.y;

    if (!rects.length) {
      Util.dumpLn("no rects in selection range. unexpected.");
    }

    return cache;
  },

  _getTargetClientRect: function _getTargetClientRect() {
    return this._targetElement.getBoundingClientRect();
  },

   /*
    * Translate a top level client point to frame relative client point.
    */
  _clientPointToFramePoint: function _clientPointToFramePoint(aClientPoint) {
    let point = {
      xPos: aClientPoint.xPos - this._contentOffset.x,
      yPos: aClientPoint.yPos - this._contentOffset.y
    };
    return point;
  },

  /*
   * Retrieve the total offset from the window's origin to the sub frame
   * element including frame and scroll offsets. The resulting offset is
   * such that:
   * sub frame coords + offset = root frame position
   */
  getCurrentWindowAndOffset: function(x, y) {
    // If the element at the given point belongs to another document (such
    // as an iframe's subdocument), the element in the calling document's
    // DOM (e.g. the iframe) is returned.
    let utils = Util.getWindowUtils(content);
    let element = utils.elementFromPoint(x, y, true, false);
    let offset = { x:0, y:0 };

    while (element && (element instanceof HTMLIFrameElement ||
                       element instanceof HTMLFrameElement)) {
      // get the child frame position in client coordinates
      let rect = element.getBoundingClientRect();

      // calculate offsets for digging down into sub frames
      // using elementFromPoint:

      // Get the content scroll offset in the child frame
      scrollOffset = ContentScroll.getScrollOffset(element.contentDocument.defaultView);
      // subtract frame and scroll offset from our elementFromPoint coordinates
      x -= rect.left + scrollOffset.x;
      y -= rect.top + scrollOffset.y;

      // calculate offsets we'll use to translate to client coords:

      // add frame client offset to our total offset result
      offset.x += rect.left;
      offset.y += rect.top;

      // get the frame's nsIDOMWindowUtils
      utils = element.contentDocument
                     .defaultView
                     .QueryInterface(Ci.nsIInterfaceRequestor)
                     .getInterface(Ci.nsIDOMWindowUtils);

      // retrieve the target element in the sub frame at x, y
      element = utils.elementFromPoint(x, y, true, false);
    }

    if (!element)
      return {};

    return {
      element: element,
      contentWindow: element.ownerDocument.defaultView,
      offset: offset,
      utils: utils
    };
  },

  _isTextInput: function _isTextInput(aElement) {
    return ((aElement instanceof Ci.nsIDOMHTMLInputElement &&
             aElement.mozIsTextField(false)) ||
            aElement instanceof Ci.nsIDOMHTMLTextAreaElement);
  },

  _getDocShell: function _getDocShell(aWindow) {
    if (aWindow == null)
      return null;
    return aWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                  .getInterface(Ci.nsIWebNavigation)
                  .QueryInterface(Ci.nsIDocShell);
  },

  _getSelectedText: function _getSelectedText() {
    let selection = this._getSelection();
    if (selection)
      return selection.toString();
    return "";
  },

  _getSelection: function _getSelection() {
    if (this._targetElement instanceof Ci.nsIDOMNSEditableElement) {
      return this._targetElement
                 .QueryInterface(Ci.nsIDOMNSEditableElement)
                 .editor.selection;
    } else if (this._contentWindow)
      return this._contentWindow.getSelection();
    return null;
  },

  _getSelectController: function _getSelectController() {
    if (this._targetElement instanceof Ci.nsIDOMNSEditableElement) {
      return this._targetElement
                 .QueryInterface(Ci.nsIDOMNSEditableElement)
                 .editor.selectionController;
    } else {
      let docShell = this._getDocShell(this._contentWindow);
      if (docShell == null)
        return null;
      return docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                     .getInterface(Ci.nsISelectionDisplay)
                     .QueryInterface(Ci.nsISelectionController);
    }
  },

  /*
   * Debug routines
   */

  _debugDumpSelection: function _debugDumpSelection(aNote, aSel) {
    Util.dumpLn("--" + aNote + "--");
    Util.dumpLn("anchor:", aSel.anchorNode, aSel.anchorOffset);
    Util.dumpLn("focus:", aSel.focusNode, aSel.focusOffset);
  },

  _debugDumpChildNodes: function _dumpChildNodes(aNode, aSpacing) {
    for (let idx = 0; idx < aNode.childNodes.length; idx++) {
      let node = aNode.childNodes.item(idx);
      for (let spaceIdx = 0; spaceIdx < aSpacing; spaceIdx++) dump(" ");
      Util.dumpLn("[" + idx + "]", node);
      this._debugDumpChildNodes(node, aSpacing + 1);
    }
  },

  _setDebugElementRect: function _setDebugElementRect(e, aScrollOffset, aColor) {
    try {
      if (e == null) {
        Util.dumpLn("SelectionHandler _setDebugElementRect(): passed in null element");
        return;
      }
      if (e.offsetWidth == 0 || e.offsetHeight== 0) {
        Util.dumpLn("SelectionHandler _setDebugElementRect(): passed in flat rect");
        return;
      }
      // e.offset values are positioned relative to the view.
      sendAsyncMessage("Content:SelectionDebugRect",
        { left:e.offsetLeft - aScrollOffset.x,
          top:e.offsetTop - aScrollOffset.y,
          right:e.offsetLeft + e.offsetWidth - aScrollOffset.x,
          bottom:e.offsetTop + e.offsetHeight - aScrollOffset.y,
          color:aColor, id: e.id });
    } catch(ex) {
      Util.dumpLn("SelectionHandler _setDebugElementRect():", ex.message);
    }
  },

  /*
   * Adds a debug rect to the selection overlay, useful in identifying
   * locations for points and rects. Params are in client coordinates.
   *
   * Example:
   * let rect = { left: aPoint.xPos - 1, top: aPoint.yPos - 1,
   *              right: aPoint.xPos + 1, bottom: aPoint.yPos + 1 };
   * this._setDebugRect(rect, "red");
   *
   * In SelectionHelperUI, you'll need to turn on displayDebugLayer
   * in init().
   */
  _setDebugRect: function _setDebugRect(aRect, aColor, aFill, aId) {
    sendAsyncMessage("Content:SelectionDebugRect",
      { left:aRect.left, top:aRect.top,
        right:aRect.right, bottom:aRect.bottom,
        color:aColor, fill: aFill, id: aId });
  },

  /*
   * Adds a small debug rect at the point specified. Params are in
   * client coordinates.
   *
   * In SelectionHelperUI, you'll need to turn on displayDebugLayer
   * in init().
   */
  _setDebugPoint: function _setDebugPoint(aX, aY, aColor) {
    let rect = { left: aX - 2, top: aY - 2,
                 right: aX + 2, bottom: aY + 2 };
    this._setDebugRect(rect, aColor, true);
  },
};

SelectionHandler.init();