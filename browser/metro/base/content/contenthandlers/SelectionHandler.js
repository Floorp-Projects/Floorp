/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

dump("### SelectionHandler.js loaded\n");

var SelectionHandler = {
  init: function init() {
    this.type = kContentSelector;
    this.snap = true;
    this.lastYPos = this.lastXPos = null;
    addMessageListener("Browser:SelectionStart", this);
    addMessageListener("Browser:SelectionAttach", this);
    addMessageListener("Browser:SelectionEnd", this);
    addMessageListener("Browser:SelectionMoveStart", this);
    addMessageListener("Browser:SelectionMove", this);
    addMessageListener("Browser:SelectionMoveEnd", this);
    addMessageListener("Browser:SelectionUpdate", this);
    addMessageListener("Browser:SelectionClose", this);
    addMessageListener("Browser:SelectionCopy", this);
    addMessageListener("Browser:SelectionDebug", this);
    addMessageListener("Browser:CaretAttach", this);
    addMessageListener("Browser:CaretMove", this);
    addMessageListener("Browser:CaretUpdate", this);
    addMessageListener("Browser:SelectionSwitchMode", this);
    addMessageListener("Browser:RepositionInfoRequest", this);
    addMessageListener("Browser:SelectionHandlerPing", this);
    addMessageListener("Browser:ResetLastPos", this);
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
    removeMessageListener("Browser:SelectionCopy", this);
    removeMessageListener("Browser:SelectionDebug", this);
    removeMessageListener("Browser:CaretAttach", this);
    removeMessageListener("Browser:CaretMove", this);
    removeMessageListener("Browser:CaretUpdate", this);
    removeMessageListener("Browser:SelectionSwitchMode", this);
    removeMessageListener("Browser:RepositionInfoRequest", this);
    removeMessageListener("Browser:SelectionHandlerPing", this);
    removeMessageListener("Browser:ResetLastPos", this);
  },

  sendAsync: function sendAsync(aMsg, aJson) {
    sendAsyncMessage(aMsg, aJson);
  },

  /*************************************************
   * Browser event handlers
   */

  /*
   * Selection start event handler
   */
  _onSelectionStart: function _onSelectionStart(aJson) {
    // Init content window information
    if (!this._initTargetInfo(aJson.xPos, aJson.yPos)) {
      this._onFail("failed to get target information");
      return;
    }

    // for context menu select command, which doesn't trigger
    // form input focus changes.
    if (aJson.setFocus && this._targetIsEditable) {
      this._targetElement.focus();
    }

    // Clear any existing selection from the document
    let selection = this._contentWindow.getSelection();
    selection.removeAllRanges();

    // Set our initial selection, aX and aY should be in client coordinates.
    let framePoint = this._clientPointToFramePoint({ xPos: aJson.xPos, yPos: aJson.yPos });
    if (!this._domWinUtils.selectAtPoint(framePoint.xPos, framePoint.yPos,
                                         Ci.nsIDOMWindowUtils.SELECT_WORDNOSPACE)) {
      this._onFail("failed to set selection at point");
      return;
    }

    // Update the position of our selection monocles
    this._updateSelectionUI("start", true, true);
  },

  _onSelectionAttach: function _onSelectionAttach(aX, aY) {
    // Init content window information
    if (!this._initTargetInfo(aX, aY)) {
      this._onFail("failed to get frame offset");
      return;
    }

    // Update the position of our selection monocles
    this._updateSelectionUI("start", true, true);
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
    this._updateSelectionUI("update", aMarker == "end", aMarker == "start");
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

    if (this._targetIsEditable) {
      // If we're coming out of an out-of-bounds scroll, the node the user is
      // trying to drag may be hidden (the monocle will be pegged to the edge
      // of the edit). Make sure the node the user wants to move is visible
      // and has focus.
      this._updateInputFocus(aMsg.change);
    }

    // Update the position of our selection monocles
    this._updateSelectionUI("update", true, true);
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
    this._updateSelectionUI("end", true, true);
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
    if (!this._targetIsEditable) {
      this._onFail("Coordiates didn't find a text input element.");
      return;
    }

    // Locate and sanity check the caret position
    let selection = this._getSelection();
    if (!selection || !selection.isCollapsed) {
      this._onFail("No selection or selection is not collapsed.");
      return;
    }

    // Update the position of our selection monocles
    this._updateSelectionUI("caret", false, false, true);
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
   *
   * @param aClearSelection requests that selection be cleared.
   */
  _onSelectionClose: function _onSelectionClose(aClearSelection) {
    if (aClearSelection) {
      this._clearSelection();
    }
    this.closeSelection();
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
    this._updateSelectionUI("update", true, true);
  },

  /*
   * Called if for any reason we fail during the selection
   * process. Cancels the selection.
   */
  _onFail: function _onFail(aDbgMessage) {
    if (aDbgMessage && aDbgMessage.length > 0)
      Util.dumpLn(aDbgMessage);
    this.sendAsync("Content:SelectionFail");
    this._clearSelection();
    this.closeSelection();
  },

  /*
   * _repositionInfoRequest - fired at us by ContentAreaObserver when the
   * soft keyboard is being displayed. CAO wants to make a decision about
   * whether the browser deck needs repositioning.
   */
  _repositionInfoRequest: function _repositionInfoRequest(aJsonMsg) {
    let result = this._calcNewContentPosition(aJsonMsg.viewHeight);

    // no repositioning needed
    if (result == 0) {
      this.sendAsync("Content:RepositionInfoResponse", { reposition: false });
      return;
    }

    this.sendAsync("Content:RepositionInfoResponse", {
      reposition: true,
      raiseContent: result,
    });
  },

  _onPing: function _onPing(aId) {
    this.sendAsync("Content:SelectionHandlerPong", { id: aId });
  },

  onClickCoords: function (xPos, yPos) {
    this.lastXPos = xPos;
    this.lastYPos = yPos;
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
  },

  /*
   * closeSelection
   *
   * Shuts SelectionHandler down.
   */
  closeSelection: function closeSelection() {
    this._clearTimers();
    this._cache = null;
    this._contentWindow = null;
    this._targetElement = null;
    this._selectionMoveActive = false;
    this._contentOffset = null;
    this._domWinUtils = null;
    this._targetIsEditable = false;
    sendSyncMessage("Content:HandlerShutdown", {});
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
      Content.getCurrentWindowAndOffset(aX, aY);
    if (!contentWindow) {
      return false;
    }
    this._targetElement = element;
    this._contentWindow = contentWindow;
    this._contentOffset = offset;
    this._domWinUtils = utils;
    this._targetIsEditable = Util.isEditable(this._targetElement);
    return true;
  },

  /*
   * _calcNewContentPosition - calculates the distance the browser should be
   * raised to move the focused form input out of the way of the soft
   * keyboard.
   *
   * @param aNewViewHeight the new content view height
   * @return 0 if no positioning is required or a positive val equal to the
   * distance content should be raised to center the target element.
   */
  _calcNewContentPosition: function _calcNewContentPosition(aNewViewHeight) {
    // We have no target element but the keyboard is up
    // so lets not cover content that is below the keyboard
    if (!this._cache || !this._cache.element) {
      if (this.lastYPos != null && this.lastYPos > aNewViewHeight) {
        return Services.metro.keyboardHeight;
      }
      return 0;
    }

    let position = Util.centerElementInView(aNewViewHeight, this._cache.element);
    if (position !== undefined) {
      return position;
    }

    // Special case: we are dealing with an input that is taller than the
    // desired height of content. We need to center on the caret location.
    let rect =
      this._domWinUtils.sendQueryContentEvent(this._domWinUtils.QUERY_CARET_RECT,
                                              this._targetElement.selectionEnd,
                                              0, 0, 0);
    if (!rect || !rect.succeeded) {
      Util.dumpLn("no caret was present, unexpected.");
      return 0;
    }

    // Note sendQueryContentEvent with QUERY_CARET_RECT is really buggy. If it
    // can't find the exact location of the caret position it will "guess".
    // Sometimes this can put the result in unexpected locations.
    let caretLocation = Math.max(Math.min(Math.round(rect.top + (rect.height * .5)),
                                          viewBottom), 0);

    // Caret is above the bottom of the new view bounds, no need to shift.
    if (caretLocation <= aNewViewHeight) {
      return 0;
    }

    // distance from the top of the keyboard down to the caret location
    return caretLocation - aNewViewHeight;
  },

  /*************************************************
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
      SelectionHandler._updateSelectionUI("update", result.start, result.end);
    }
  },

  receiveMessage: function sh_receiveMessage(aMessage) {
    if (this._debugEvents && aMessage.name != "Browser:SelectionMove") {
      Util.dumpLn("SelectionHandler:", aMessage.name);
    }
    let json = aMessage.json;
    switch (aMessage.name) {
      case "Browser:SelectionStart":
        this._onSelectionStart(json);
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
        this._onSelectionClose(json.clearSelection);
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

      case "Browser:SelectionDebug":
        this._onSelectionDebug(json);
        break;

      case "Browser:SelectionUpdate":
        this._onSelectionUpdate();
        break;

      case "Browser:RepositionInfoRequest":
        // This message is sent simultaneously with a tap event.
        // Wait a bit to make sure we have the most up-to-date tap co-ordinates
        // before a call to _calcNewContentPosition() which accesses them.
        content.setTimeout (function () {
          SelectionHandler._repositionInfoRequest(json);
        }, 50);
        break;

      case "Browser:SelectionHandlerPing":
        this._onPing(json.id);
        break;

      case "Browser:ResetLastPos":
        this.onClickCoords(json.xPos, json.yPos);
        break;
    }
  },

  /*************************************************
   * Utilities
   */

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
};

SelectionHandler.__proto__ = new SelectionPrototype();
SelectionHandler.init();

