/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["EncryptedMediaChild"];

/**
 * GlobalCaptureListener is a class that listens for changes to the global
 * capture state of windows and screens. It uses this information to notify
 * observers if it's possible that media is being shared by these captures.
 * You probably only want one instance of this class per content process.
 */
class GlobalCaptureListener {
  constructor() {
    Services.cpmm.sharedData.addEventListener("change", this);
    // Tracks if screen capture is taking place based on shared data. Default
    // to true for safety.
    this._isScreenCaptured = true;
    // Tracks if any windows are being captured. Default to true for safety.
    this._isAnyWindowCaptured = true;
  }

  /**
   * Updates the capture state and forces that the state is notified to
   * observers even if it hasn't changed since the last update.
   */
  requestUpdateAndNotify() {
    this._updateCaptureState({ forceNotify: true });
  }

  /**
   * Handle changes in shared data that may alter the capture state.
   * @param event a notification that sharedData has changed. If this includes
   * changes to screen or window sharing state then we'll update the capture
   * state.
   */
  handleEvent(event) {
    if (
      event.changedKeys.includes("webrtcUI:isSharingScreen") ||
      event.changedKeys.includes("webrtcUI:sharedTopInnerWindowIds")
    ) {
      this._updateCaptureState();
    }
  }

  /**
   * Updates the capture state and notifies the state to observers if the
   * state has changed since last update, or if forced.
   * @param forceNotify if true then the capture state will be sent to
   * observers even if it didn't change since the last update.
   */
  _updateCaptureState({ forceNotify = false } = {}) {
    const previousCaptureState =
      this._isScreenCaptured || this._isAnyWindowCaptured;

    this._isScreenCaptured = Boolean(
      Services.cpmm.sharedData.get("webrtcUI:isSharingScreen")
    );

    const capturedTopInnerWindowIds = Services.cpmm.sharedData.get(
      "webrtcUI:sharedTopInnerWindowIds"
    );
    if (capturedTopInnerWindowIds && capturedTopInnerWindowIds.size > 0) {
      this._isAnyWindowCaptured = true;
    } else {
      this._isAnyWindowCaptured = false;
    }
    const newCaptureState = this._isScreenCaptured || this._isAnyWindowCaptured;

    const captureStateChanged = previousCaptureState != newCaptureState;

    if (forceNotify || captureStateChanged) {
      // Notify the state if the caller forces it, or if the state changed.
      this._notifyCaptureState();
    }
  }

  /**
   * Notifies observers of the current capture state. Notifies observers
   * with a null subject, "mediakeys-response" topic, and data that is either
   * "capture-possible" or "capture-not-possible", depending on if capture is
   * possible or not.
   */
  _notifyCaptureState() {
    const isCapturePossible =
      this._isScreenCaptured || this._isAnyWindowCaptured;
    const isCapturePossibleString = isCapturePossible
      ? "capture-possible"
      : "capture-not-possible";
    Services.obs.notifyObservers(
      null,
      "mediakeys-response",
      isCapturePossibleString
    );
  }
}

const gGlobalCaptureListener = new GlobalCaptureListener();

class EncryptedMediaChild extends JSWindowActorChild {
  // Expected to observe 'mediakeys-request' as notified from MediaKeySystemAccess.
  // @param aSubject the nsPIDOMWindowInner associated with the notifying MediaKeySystemAccess.
  // @param aTopic should be "mediakeys-request".
  // @param aData json containing a `status` and a `keysystem`.
  observe(aSubject, aTopic, aData) {
    let parsedData;
    try {
      parsedData = JSON.parse(aData);
    } catch (ex) {
      Cu.reportError("Malformed EME video message with data: " + aData);
      return;
    }
    const { status } = parsedData;
    if (status == "is-capture-possible") {
      // We handle this status in process -- don't send a message to the parent.
      gGlobalCaptureListener.requestUpdateAndNotify();
      return;
    }

    this.sendAsyncMessage("EMEVideo:ContentMediaKeysRequest", aData);
  }
}
