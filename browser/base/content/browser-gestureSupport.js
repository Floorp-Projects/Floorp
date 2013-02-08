# -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

// History Swipe Animation Support (bug 678392)
let gHistorySwipeAnimation = {

  active: false,
  isLTR: false,

  /**
   * Initializes the support for history swipe animations, if it is supported
   * by the platform/configuration.
   */
  init: function HSA_init() {
    if (!this._isSupported())
      return;
  
    gBrowser.addEventListener("pagehide", this, false);
    gBrowser.addEventListener("pageshow", this, false);
    gBrowser.addEventListener("popstate", this, false);
    gBrowser.tabContainer.addEventListener("TabClose", this, false);

    this.active = true;
    this.isLTR = document.documentElement.mozMatchesSelector(
                                            ":-moz-locale-dir(ltr)");
    this._trackedSnapshots = [];
    this._historyIndex = -1;
    this._boxWidth = -1;
    this._maxSnapshots = this._getMaxSnapshots();
    this._lastSwipeDir = "";
  },

  /**
   * Uninitializes the support for history swipe animations.
   */
  uninit: function HSA_uninit() {
    gBrowser.removeEventListener("pagehide", this, false);
    gBrowser.removeEventListener("pageshow", this, false);
    gBrowser.removeEventListener("popstate", this, false);
    gBrowser.tabContainer.removeEventListener("TabClose", this, false);

    this.active = false;
    this.isLTR = false;
  },

  /**
   * Starts the swipe animation and handles fast swiping (i.e. a swipe animation
   * is already in progress when a new one is initiated).
   */
  startAnimation: function HSA_startAnimation() {
    if (this.isAnimationRunning()) {
      gBrowser.stop();
      this._lastSwipeDir = "RELOAD"; // just ensure that != ""
      this._canGoBack = this.canGoBack();
      this._canGoForward = this.canGoForward();
      this._handleFastSwiping();
    }
    else {
      this._historyIndex = gBrowser.webNavigation.sessionHistory.index;
      this._canGoBack = this.canGoBack();
      this._canGoForward = this.canGoForward();
      this._takeSnapshot();
      this._installPrevAndNextSnapshots();
      this._addBoxes();
      this._lastSwipeDir = "";
    }
    this.updateAnimation(0);
  },

  /**
   * Stops the swipe animation.
   */
  stopAnimation: function HSA_stopAnimation() {
    gHistorySwipeAnimation._removeBoxes();
  },

  /**
   * Updates the animation between two pages in history.
   *
   * @param aVal
   *        A floating point value that represents the progress of the
   *        swipe gesture.
   */
  updateAnimation: function HSA_updateAnimation(aVal) {
    if (!this.isAnimationRunning())
      return;

    if ((aVal >= 0 && this.isLTR) ||
        (aVal <= 0 && !this.isLTR)) {
      if (aVal > 1)
        aVal = 1; // Cap value to avoid sliding the page further than allowed.

      if (this._canGoBack)
        this._prevBox.collapsed = false;
      else
        this._prevBox.collapsed = true;

      // The current page is pushed to the right (LTR) or left (RTL),
      // the intention is to go back.
      // If there is a page to go back to, it should show in the background.
      this._positionBox(this._curBox, aVal);

      // The forward page should be pushed offscreen all the way to the right.
      this._positionBox(this._nextBox, 1);
    }
    else {
      if (aVal < -1)
        aVal = -1; // Cap value to avoid sliding the page further than allowed.

      // The intention is to go forward. If there is a page to go forward to,
      // it should slide in from the right (LTR) or left (RTL).
      // Otherwise, the current page should slide to the left (LTR) or
      // right (RTL) and the backdrop should appear in the background.
      // For the backdrop to be visible in that case, the previous page needs
      // to be hidden (if it exists).
      if (this._canGoForward) {
        let offset = this.isLTR ? 1 : -1;
        this._positionBox(this._curBox, 0);
        this._positionBox(this._nextBox, offset + aVal); // aVal is negative
      }
      else {
        this._prevBox.collapsed = true;
        this._positionBox(this._curBox, aVal);
      }
    }
  },

  /**
   * Event handler for events relevant to the history swipe animation.
   *
   * @param aEvent
   *        An event to process.
   */
  handleEvent: function HSA_handleEvent(aEvent) {
    switch (aEvent.type) {
      case "TabClose":
        let browser = gBrowser.getBrowserForTab(aEvent.target);
        this._removeTrackedSnapshot(-1, browser);
        break;
      case "pageshow":
      case "popstate":
        if (this.isAnimationRunning()) {
          if (aEvent.target != gBrowser.selectedBrowser.contentDocument)
            break;
          this.stopAnimation();
        }
        this._historyIndex = gBrowser.webNavigation.sessionHistory.index;
        break;
      case "pagehide":
        if (aEvent.target == gBrowser.selectedBrowser.contentDocument) {
          // Take a snapshot of a page whenever it's about to be navigated away
          // from.
          this._takeSnapshot();
        }
        break;
    }
  },

  /**
   * Checks whether the history swipe animation is currently running or not.
   *
   * @return true if the animation is currently running, false otherwise.
   */
  isAnimationRunning: function HSA_isAnimationRunning() {
    return !!this._container;
  },

  /**
   * Process a swipe event based on the given direction.
   *
   * @param aEvent
   *        The swipe event to handle
   * @param aDir
   *        The direction for the swipe event
   */
  processSwipeEvent: function HSA_processSwipeEvent(aEvent, aDir) {
    if (aDir == "RIGHT")
      this._historyIndex += this.isLTR ? 1 : -1;
    else if (aDir == "LEFT")
      this._historyIndex += this.isLTR ? -1 : 1;
    else
      return;
    this._lastSwipeDir = aDir;
  },

  /**
   * Checks if there is a page in the browser history to go back to.
   *
   * @return true if there is a previous page in history, false otherwise.
   */
  canGoBack: function HSA_canGoBack() {
    if (this.isAnimationRunning())
      return this._doesIndexExistInHistory(this._historyIndex - 1);
    return gBrowser.webNavigation.canGoBack;
  },

  /**
   * Checks if there is a page in the browser history to go forward to.
   *
   * @return true if there is a next page in history, false otherwise.
   */
  canGoForward: function HSA_canGoForward() {
    if (this.isAnimationRunning())
      return this._doesIndexExistInHistory(this._historyIndex + 1);
    return gBrowser.webNavigation.canGoForward;
  },

  /**
   * Used to notify the history swipe animation that the OS sent a swipe end
   * event and that we should navigate to the page that the user swiped to, if
   * any. This will also result in the animation overlay to be torn down.
   */
  swipeEndEventReceived: function HSA_swipeEndEventReceived() {
    if (this._lastSwipeDir != "")
      this._navigateToHistoryIndex();
    else
      this.stopAnimation();
  },

  /**
   * Checks whether a particular index exists in the browser history or not.
   *
   * @param aIndex
   *        The index to check for availability for in the history.
   * @return true if the index exists in the browser history, false otherwise.
   */
  _doesIndexExistInHistory: function HSA__doesIndexExistInHistory(aIndex) {
    try {
      gBrowser.webNavigation.sessionHistory.getEntryAtIndex(aIndex, false);
    }
    catch(ex) {
      return false;
    }
    return true;
  },

  /**
   * Navigates to the index in history that is currently being tracked by
   * |this|.
   */
  _navigateToHistoryIndex: function HSA__navigateToHistoryIndex() {
    if (this._doesIndexExistInHistory(this._historyIndex)) {
      gBrowser.webNavigation.gotoIndex(this._historyIndex);
    }
  },

  /**
   * Checks to see if history swipe animations are supported by this
   * platform/configuration.
   *
   * return true if supported, false otherwise.
   */
  _isSupported: function HSA__isSupported() {
    // Only activate on Lion.
    // TODO: Only if [NSEvent isSwipeTrackingFromScrollEventsEnabled]
    return window.matchMedia("(-moz-mac-lion-theme)").matches;
  },

  /**
   * Handle fast swiping (i.e. a swipe animation is already in
   * progress when a new one is initiated). This will swap out the snapshots
   * used in the previous animation with the appropriate new ones.
   */
  _handleFastSwiping: function HSA__handleFastSwiping() {
    this._installCurrentPageSnapshot(null);
    this._installPrevAndNextSnapshots();
  },

  /**
   * Adds the boxes that contain the snapshots used during the swipe animation.
   */
  _addBoxes: function HSA__addBoxes() {
    let browserStack =
      document.getAnonymousElementByAttribute(gBrowser.getNotificationBox(),
                                              "class", "browserStack");
    this._container = this._createElement("historySwipeAnimationContainer",
                                          "stack");
    browserStack.appendChild(this._container);

    this._prevBox = this._createElement("historySwipeAnimationPreviousPage",
                                        "box");
    this._container.appendChild(this._prevBox);

    this._curBox = this._createElement("historySwipeAnimationCurrentPage",
                                       "box");
    this._container.appendChild(this._curBox);

    this._nextBox = this._createElement("historySwipeAnimationNextPage",
                                        "box");
    this._container.appendChild(this._nextBox);

    this._boxWidth = this._curBox.getBoundingClientRect().width; // cache width
  },

  /**
   * Removes the boxes.
   */
  _removeBoxes: function HSA__removeBoxes() {
    this._curBox = null;
    this._prevBox = null;
    this._nextBox = null;
    if (this._container)
      this._container.parentNode.removeChild(this._container);
    this._container = null;
    this._boxWidth = -1;
  },

  /**
   * Creates an element with a given identifier and tag name.
   *
   * @param aID
   *        An identifier to create the element with.
   * @param aTagName
   *        The name of the tag to create the element for.
   * @return the newly created element.
   */
  _createElement: function HSA__createElement(aID, aTagName) {
    let XULNS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
    let element = document.createElementNS(XULNS, aTagName);
    element.id = aID;
    return element;
  },

  /**
   * Moves a given box to a given X coordinate position.
   *
   * @param aBox
   *        The box element to position.
   * @param aPosition
   *        The position (in X coordinates) to move the box element to.
   */
  _positionBox: function HSA__positionBox(aBox, aPosition) {
    aBox.style.transform = "translateX(" + this._boxWidth * aPosition + "px)";
  },

  /**
   * Takes a snapshot of the page the browser is currently on.
   */
  _takeSnapshot: function HSA__takeSnapshot() {
    if ((this._maxSnapshots < 1) ||
        (gBrowser.webNavigation.sessionHistory.index < 0))
      return;

    let browser = gBrowser.selectedBrowser;
    let r = browser.getBoundingClientRect();
    let canvas = document.createElementNS("http://www.w3.org/1999/xhtml",
                                          "canvas");
    canvas.mozOpaque = true;
    canvas.width = r.width;
    canvas.height = r.height;
    let ctx = canvas.getContext("2d");
    let zoom = browser.markupDocumentViewer.fullZoom;
    ctx.scale(zoom, zoom);
    ctx.drawWindow(browser.contentWindow, 0, 0, r.width, r.height, "white",
                   ctx.DRAWWINDOW_DO_NOT_FLUSH | ctx.DRAWWINDOW_DRAW_VIEW |
                   ctx.DRAWWINDOW_ASYNC_DECODE_IMAGES |
                   ctx.DRAWWINDOW_USE_WIDGET_LAYERS);

    this._installCurrentPageSnapshot(canvas);
    this._assignSnapshotToCurrentBrowser(canvas);
  },

  /**
   * Retrieves the maximum number of snapshots that should be kept in memory.
   * This limit is a global limit and is valid across all open tabs.
   */
  _getMaxSnapshots: function HSA__getMaxSnapshots() {
    return gPrefService.getIntPref("browser.snapshots.limit");
  },

  /**
   * Adds a snapshot to the list and initiates the compression of said snapshot.
   * Once the compression is completed, it will replace the uncompressed
   * snapshot in the list.
   *
   * @param aCanvas
   *        The snapshot to add to the list and compress.
   */
  _assignSnapshotToCurrentBrowser:
  function HSA__assignSnapshotToCurrentBrowser(aCanvas) {
    let browser = gBrowser.selectedBrowser;
    let currIndex = browser.webNavigation.sessionHistory.index;

    this._removeTrackedSnapshot(currIndex, browser);
    this._addSnapshotRefToArray(currIndex, browser);

    if (!("snapshots" in browser))
      browser.snapshots = [];
    let snapshots = browser.snapshots;
    // Temporarily store the canvas as the compressed snapshot.
    // This avoids a blank page if the user swipes quickly
    // between pages before the compression could complete.
    snapshots[currIndex] = aCanvas;

    // Kick off snapshot compression.
    aCanvas.toBlob(function(aBlob) {
        snapshots[currIndex] = aBlob;
      }, "image/png"
    );
  },

  /**
   * Removes a snapshot identified by the browser and index in the array of
   * snapshots for that browser, if present. If no snapshot could be identified
   * the method simply returns without taking any action. If aIndex is negative,
   * all snapshots for a particular browser will be removed.
   *
   * @param aIndex
   *        The index in history of the new snapshot, or negative value if all
   *        snapshots for a browser should be removed.
   * @param aBrowser
   *        The browser the new snapshot was taken in.
   */
  _removeTrackedSnapshot: function HSA__removeTrackedSnapshot(aIndex, aBrowser) {
    let arr = this._trackedSnapshots;
    let requiresExactIndexMatch = aIndex >= 0;
    for (let i = 0; i < arr.length; i++) {
      if ((arr[i].browser == aBrowser) &&
          (aIndex < 0 || aIndex == arr[i].index)) {
        delete aBrowser.snapshots[arr[i].index];
        arr.splice(i, 1);
        if (requiresExactIndexMatch)
          return; // Found and removed the only element.
        i--; // Make sure to revisit the index that we just removed an
             // element at.
      }
    }
  },

  /**
   * Adds a new snapshot reference for a given index and browser to the array
   * of references to tracked snapshots.
   *
   * @param aIndex
   *        The index in history of the new snapshot.
   * @param aBrowser
   *        The browser the new snapshot was taken in.
   */
  _addSnapshotRefToArray:
  function HSA__addSnapshotRefToArray(aIndex, aBrowser) {
    let id = { index: aIndex,
               browser: aBrowser };
    let arr = this._trackedSnapshots;
    arr.unshift(id);

    while (arr.length > this._maxSnapshots) {
      let lastElem = arr[arr.length - 1];
      delete lastElem.browser.snapshots[lastElem.index];
      arr.splice(-1, 1);
    }
  },

  /**
   * Converts a compressed blob to an Image object. In some situations
   * (especially during fast swiping) aBlob may still be a canvas, not a
   * compressed blob. In this case, we simply return the canvas.
   *
   * @param aBlob
   *        The compressed blob to convert, or a canvas if a blob compression
   *        couldn't complete before this method was called.
   * @return A new Image object representing the converted blob.
   */
  _convertToImg: function HSA__convertToImg(aBlob) {
    if (!aBlob)
      return null;

    // Return aBlob if it's still a canvas and not a compressed blob yet.
    if (aBlob instanceof HTMLCanvasElement)
      return aBlob;

    let img = new Image();
    let url = URL.createObjectURL(aBlob);
    img.onload = function() {
      URL.revokeObjectURL(url);
    };
    img.src = url;
    return img;
  },

  /**
   * Sets the snapshot of the current page to the snapshot passed as parameter,
   * or to the one previously stored for the current index in history if the
   * parameter is null.
   *
   * @param aCanvas
   *        The snapshot to set the current page to. If this parameter is null,
   *        the previously stored snapshot for this index (if any) will be used.
   */
  _installCurrentPageSnapshot:
  function HSA__installCurrentPageSnapshot(aCanvas) {
    let currSnapshot = aCanvas;
    if (!currSnapshot) {
      let snapshots = gBrowser.selectedBrowser.snapshots || {};
      let currIndex = this._historyIndex;
      if (currIndex in snapshots)
        currSnapshot = this._convertToImg(snapshots[currIndex]);
    }
    document.mozSetImageElement("historySwipeAnimationCurrentPageSnapshot",
                                  currSnapshot);
  },

  /**
   * Sets the snapshots of the previous and next pages to the snapshots
   * previously stored for their respective indeces.
   */
  _installPrevAndNextSnapshots:
  function HSA__installPrevAndNextSnapshots() {
    let snapshots = gBrowser.selectedBrowser.snapshots || [];
    let currIndex = this._historyIndex;
    let prevIndex = currIndex - 1;
    let prevSnapshot = null;
    if (prevIndex in snapshots)
      prevSnapshot = this._convertToImg(snapshots[prevIndex]);
    document.mozSetImageElement("historySwipeAnimationPreviousPageSnapshot",
                                prevSnapshot);

    let nextIndex = currIndex + 1;
    let nextSnapshot = null;
    if (nextIndex in snapshots)
      nextSnapshot = this._convertToImg(snapshots[nextIndex]);
    document.mozSetImageElement("historySwipeAnimationNextPageSnapshot",
                                nextSnapshot);
  },
};
