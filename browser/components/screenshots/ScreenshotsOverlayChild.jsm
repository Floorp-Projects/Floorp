/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * The Screenshots overlay is inserted into the document's
 * canvasFrame anonymous content container (see dom/webidl/Document.webidl).
 *
 * This container gets cleared automatically when the document navigates.
 *
 * Since the overlay markup is inserted in the canvasFrame using
 * insertAnonymousContent, this means that it can be modified using the API
 * described in AnonymousContent.webidl.
 *
 * Any mutation of this content must be via the AnonymousContent API.
 * This is similar in design to [devtools' highlighters](https://firefox-source-docs.mozilla.org/devtools/tools/highlighters.html#inserting-content-in-the-page),
 * though as Screenshots doesnt need to work on XUL documents, or allow multiple kinds of
 * highlight/overlay our case is a little simpler.
 *
 * To retrieve the AnonymousContent instance, use the `content` getter.
 */

/* States:

  "crosshairs":
    Nothing has happened, and the crosshairs will follow the movement of the mouse
  "draggingReady":
    The user has pressed the mouse button, but hasn't moved enough to create a selection
  "dragging":
    The user has pressed down a mouse button, and is dragging out an area far enough to show a selection
  "selected":
    The user has selected an area
  "resizing":
    The user is resizing the selection

  A pointerdown goes from crosshairs to dragging.
  A pointerup goes from dragging to selected
  A click outside of the selection goes from selected to crosshairs
  A pointerdown on one of the draggers goes from selected to resizing

  */

var EXPORTED_SYMBOLS = ["ScreenshotsOverlayChild"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const lazy = {};

XPCOMUtils.defineLazyGetter(lazy, "overlayLocalization", () => {
  return new Localization(["browser/screenshotsOverlay.ftl"], true);
});

const STYLESHEET_URL =
  "chrome://browser/content/screenshots/overlay/overlay.css";

class AnonymousContentOverlay {
  constructor(contentDocument, screenshotsChild) {
    this.listeners = new Map();
    this.elements = new Map();

    this.screenshotsChild = screenshotsChild;

    this.contentDocument = contentDocument;
    // aliased for easier diffs/maintenance of the event management code borrowed from devtools highlighters
    this.pageListenerTarget = contentDocument.ownerGlobal;

    this.overlayFragment = null;

    this.overlayId = "screenshots-overlay-container";
    this.previewId = "preview-container";
    this.selectionId = "selection-container";

    this._initialized = false;

    this.moverIds = [
      "mover-left",
      "mover-top",
      "mover-right",
      "mover-bottom",
      "mover-topLeft",
      "mover-topRight",
      "mover-bottomLeft",
      "mover-bottomRight",
    ];
  }
  get content() {
    if (!this._content || Cu.isDeadWrapper(this._content)) {
      return null;
    }
    return this._content;
  }
  async initialize() {
    if (this._initialized) {
      return;
    }

    let document = this.contentDocument;
    let window = document.ownerGlobal;

    // Inject stylesheet
    if (!this.overlayFragment) {
      try {
        window.windowUtils.loadSheetUsingURIString(
          STYLESHEET_URL,
          window.windowUtils.AGENT_SHEET
        );
      } catch {
        // The method fails if the url is already loaded.
      }
      // Inject markup for the overlay UI
      this.overlayFragment = this.buildOverlay();
    }

    this._content = document.insertAnonymousContent(
      this.overlayFragment.children[0]
    );

    this.addEventListeners();

    this.previewLayer = new PreviewLayer(this.previewId, this.content);
    this.selectionLayer = new SelectionLayer(this.selectionId, this.content);

    this.screenshotsContainer = new ScreenshotsContainerLayer(
      this.overlayId,
      this.content,
      this.previewLayer,
      this.selectionLayer
    );

    this.stateHandler = new StateHandler(this.screenshotsContainer);

    this.screenshotsContainer.updateSize(window);

    this.stateHandler.setState("crosshairs");

    this._initialized = true;
  }

  /**
   * The Anonymous Content doesn't shrink when the window is resized so we need
   * to find the largest element that isn't the Anonymous Content and we will
   * use that width and height.
   * Otherwise we will fallback to the documentElement scroll width and height
   * @param eventType If "resize", we called this from a resize event so we will
   *  try shifting the SelectionBox.
   *  If "scroll", we called this from a scroll event so we will redraw the buttons
   */
  updateScreenshotsSize(eventType) {
    this.stateHandler.updateScreenshotsContainerSize(
      this.contentDocument.ownerGlobal,
      eventType
    );
  }

  /**
   * Add required event listeners to the overlay
   */
  addEventListeners() {
    this.addEventListenerForElement(
      "screenshots-cancel-button",
      "click",
      (event, targetId) => {
        this.screenshotsChild.requestCancelScreenshot();
      }
    );
    this.addEventListenerForElement("cancel", "click", (event, targetId) => {
      this.screenshotsChild.requestCancelScreenshot();
    });
    this.addEventListenerForElement("copy", "click", (event, targetId) => {
      this.screenshotsChild.requestCopyScreenshot(
        this.screenshotsContainer.getSelectionLayerBoxDimensions()
      );
    });
    this.addEventListenerForElement("download", "click", (event, targetId) => {
      this.screenshotsChild.requestDownloadScreenshot(
        this.screenshotsContainer.getSelectionLayerBoxDimensions()
      );
    });

    // The pointerdown event is added to the selection buttons to prevent the
    // pointerdown event from occurring on the "screenshots-overlay-container"
    this.addEventListenerForElement(
      "cancel",
      "pointerdown",
      (event, targetId) => {
        event.stopPropagation();
      }
    );
    this.addEventListenerForElement(
      "copy",
      "pointerdown",
      (event, targetId) => {
        event.stopPropagation();
      }
    );
    this.addEventListenerForElement(
      "download",
      "pointerdown",
      (event, targetId) => {
        event.stopPropagation();
      }
    );

    this.addEventListenerForElement(
      this.overlayId,
      "pointerdown",
      (event, targetId) => {
        this.dragStart(event, targetId);
      }
    );
    this.addEventListenerForElement(
      this.overlayId,
      "pointerup",
      (event, targetId) => {
        this.dragEnd(event, targetId);
      }
    );
    this.addEventListenerForElement(
      this.overlayId,
      "pointermove",
      (event, targetId) => {
        this.drag(event, targetId);
      }
    );

    for (let id of this.moverIds.concat(["highlight"])) {
      this.addEventListenerForElement(id, "pointerdown", (event, targetId) => {
        this.dragStart(event, targetId);
      });
      this.addEventListenerForElement(id, "pointerup", (event, targetId) => {
        this.dragEnd(event, targetId);
      });
      this.addEventListenerForElement(id, "pointermove", (event, targetId) => {
        this.drag(event, targetId);
      });
    }
  }

  /**
   * Removes all event listeners and removes the overlay from the Anonymous Content
   */
  tearDown() {
    if (this._content) {
      this._removeAllListeners();
      try {
        this.contentDocument.removeAnonymousContent(this._content);
      } catch (e) {
        // If the current window isn't the one the content was inserted into, this
        // will fail, but that's fine.
      }
    }
    this._initialized = false;
  }

  /**
   * Creates the document fragment that will be added to the Anonymous Content
   * @returns document fragment that can be injected into the Anonymous Content
   */
  buildOverlay() {
    let [
      cancel,
      instrustions,
      download,
      copy,
    ] = lazy.overlayLocalization.formatMessagesSync([
      { id: "screenshots-overlay-cancel-button" },
      { id: "screenshots-overlay-instructions" },
      { id: "screenshots-overlay-download-button" },
      { id: "screenshots-overlay-copy-button" },
    ]);

    const htmlString = `
    <div id="screenshots-component">
      <div id="${this.overlayId}">
        <div id="${this.previewId}">
          <div class="fixed-container">
            <div class="face-container">
              <div class="eye left"><div id="left-eye" class="eyeball"></div></div>
              <div class="eye right"><div id="right-eye" class="eyeball"></div></div>
              <div class="face"></div>
            </div>
            <div class="preview-instructions" data-l10n-id="screenshots-instructions">${instrustions.value}</div>
            <div class="cancel-shot" id="screenshots-cancel-button" data-l10n-id="screenshots-overlay-cancel-button">${cancel.value}</div>
          </div>
        </div>
        <div id="${this.selectionId}" style="display:none;">
          <div id="bgTop" class="bghighlight" style="display:none;"></div>
          <div id="bgBottom" class="bghighlight" style="display:none;"></div>
          <div id="bgLeft" class="bghighlight" style="display:none;"></div>
          <div id="bgRight" class="bghighlight" style="display:none;"></div>
          <div id="highlight" class="highlight" style="display:none;">
            <div id="mover-topLeft" class="mover-target direction-topLeft">
              <div class="mover"></div>
            </div>
            <div id="mover-top" class="mover-target direction-top">
              <div class="mover"></div>
            </div>
            <div id="mover-topRight" class="mover-target direction-topRight">
              <div class="mover"></div>
            </div>
            <div id="mover-left" class="mover-target direction-left">
              <div class="mover"></div>
            </div>
            <div id="mover-right" class="mover-target direction-right">
              <div class="mover"></div>
            </div>
            <div id="mover-bottomLeft" class="mover-target direction-bottomLeft">
              <div class="mover"></div>
            </div>
            <div id="mover-bottom" class="mover-target direction-bottom">
              <div class="mover"></div>
            </div>
            <div id="mover-bottomRight" class="mover-target direction-bottomRight">
              <div class="mover"></div>
            </div>
          </div>
          <div id="buttons" style="display:none;">
            <button id="cancel" class="screenshots-button" title="${cancel.value}" aria-label="${cancel.value}"><img/></button>
            <button id="copy" class="screenshots-button" title="${copy.value}" aria-label="${copy.value}"><img/>${copy.value}</button>
            <button id="download" class="screenshots-button" title="${download.value}" aria-label="${download.value}"><img/>${download.value}</button>
          </div>
        </div>
      </div>
    </div>`;

    const parser = new this.contentDocument.ownerGlobal.DOMParser();
    const tmpDoc = parser.parseFromString(htmlString, "text/html");
    const fragment = this.contentDocument.createDocumentFragment();

    fragment.appendChild(tmpDoc.body.children[0]);
    return fragment;
  }

  // The event tooling is borrowed directly from devtools' highlighters (CanvasFrameAnonymousContentHelper)
  /**
   * Add an event listener to one of the elements inserted in the canvasFrame
   * native anonymous container.
   * Like other methods in this helper, this requires the ID of the element to
   * be passed in.
   *
   * Note that if the content page navigates, the event listeners won't be
   * added again.
   *
   * Also note that unlike traditional DOM events, the events handled by
   * listeners added here will propagate through the document only through
   * bubbling phase, so the useCapture parameter isn't supported.
   * It is possible however to call e.stopPropagation() to stop the bubbling.
   *
   * IMPORTANT: the chrome-only canvasFrame insertion API takes great care of
   * not leaking references to inserted elements to chrome JS code. That's
   * because otherwise, chrome JS code could freely modify native anon elements
   * inside the canvasFrame and probably change things that are assumed not to
   * change by the C++ code managing this frame.
   * See https://wiki.mozilla.org/DevTools/Highlighter#The_AnonymousContent_API
   * Unfortunately, the inserted nodes are still available via
   * event.originalTarget, and that's what the event handler here uses to check
   * that the event actually occured on the right element, but that also means
   * consumers of this code would be able to access the inserted elements.
   * Therefore, the originalTarget property will be nullified before the event
   * is passed to your handler.
   *
   * IMPL DETAIL: A single event listener is added per event types only, at
   * browser level and if the event originalTarget is found to have the provided
   * ID, the callback is executed (and then IDs of parent nodes of the
   * originalTarget are checked too).
   *
   * @param {String} id
   * @param {String} type
   * @param {Function} handler
   */
  addEventListenerForElement(id, type, handler) {
    if (typeof id !== "string") {
      throw new Error(
        "Expected a string ID in addEventListenerForElement but got: " + id
      );
    }

    // If no one is listening for this type of event yet, add one listener.
    if (!this.listeners.has(type)) {
      const target = this.pageListenerTarget;
      target.addEventListener(type, this, true);
      // Each type entry in the map is a map of ids:handlers.
      this.listeners.set(type, new Map());
    }

    const listeners = this.listeners.get(type);
    listeners.set(id, handler);
  }

  /**
   * Remove an event listener from one of the elements inserted in the
   * canvasFrame native anonymous container.
   * @param {String} id
   * @param {String} type
   */
  removeEventListenerForElement(id, type) {
    const listeners = this.listeners.get(type);
    if (!listeners) {
      return;
    }
    listeners.delete(id);

    // If no one is listening for event type anymore, remove the listener.
    if (!listeners.size) {
      const target = this.pageListenerTarget;
      target.removeEventListener(type, this, true);
    }
  }

  handleEvent(event) {
    const listeners = this.listeners.get(event.type);
    if (!listeners) {
      return;
    }

    // Hide the originalTarget property to avoid exposing references to native
    // anonymous elements. See addEventListenerForElement's comment.
    let isPropagationStopped = false;
    const eventProxy = new Proxy(event, {
      get: (obj, name) => {
        if (name === "originalTarget") {
          return null;
        } else if (name === "stopPropagation") {
          return () => {
            isPropagationStopped = true;
          };
        }
        return obj[name];
      },
    });

    // Start at originalTarget, bubble through ancestors and call handlers when
    // needed.
    let node = event.originalTarget;
    while (node) {
      let nodeId = node.id;
      if (nodeId) {
        const handler = listeners.get(node.id);
        if (handler) {
          handler(eventProxy, nodeId);
          if (isPropagationStopped) {
            break;
          }
        }
        if (nodeId == this.overlayId) {
          break;
        }
      }
      node = node.parentNode;
    }
  }

  _removeAllListeners() {
    if (this.pageListenerTarget) {
      const target = this.pageListenerTarget;
      for (const [type] of this.listeners) {
        target.removeEventListener(type, this, true);
      }
    }
    this.listeners.clear();
  }

  /**
   * Pass the pointer down event to the state handler
   * @param event The pointer down event
   * @param targetId The target element id
   */
  dragStart(event, targetId) {
    this.stateHandler.dragStart(event, targetId);
  }

  /**
   * Pass the pointer move event to the state handler
   * @param event The pointer move event
   * @param targetId The target element id
   */
  drag(event, targetId) {
    this.stateHandler.drag(event);
  }

  /**
   * Pass the pointer up event to the state handler
   * @param event The pointer up event
   * @param targetId The target element id
   */
  dragEnd(event, targetId) {
    this.stateHandler.dragEnd(event);
  }
}

var ScreenshotsOverlayChild = {
  AnonymousContentOverlay,
};

/**
 * The StateHandler class handles the state of the overlay
 */
class StateHandler {
  #state;
  #lastBox;
  #moverId;
  #lastX;
  #lastY;
  #screenshotsContainer;

  constructor(screenshotsContainer) {
    this.#state = "crosshairs";
    this.#lastBox = {};

    this.#screenshotsContainer = screenshotsContainer;
  }

  setState(newState) {
    this.#state = newState;
    this.start();
  }

  getState() {
    return this.#state;
  }

  /**
   * At the start of the some states we need to perform some actions
   */
  start() {
    switch (this.#state) {
      case "crosshairs": {
        this.crosshairsStart();
        break;
      }
      case "dragging": {
        this.draggingStart();
        break;
      }
      case "selected": {
        this.selectedStart();
        break;
      }
      case "resizing": {
        this.resizingStart();
        break;
      }
    }
  }

  /**
   * Returns the x and y coordinates of the event
   * @param event The mouse or touch event
   * @returns object containing the x and y coordinates of the mouse
   */
  getCoordinates(event) {
    let clientX = event.pageX;
    let clientY = event.pageY;

    return { clientX, clientY };
  }

  /**
   * Handles the mousedown/touchstart event depending on the state
   * @param event The mousedown or touchstart event
   * @param targetId The id of the event target
   */
  dragStart(event, targetId) {
    let { clientX, clientY } = this.getCoordinates(event);

    switch (this.#state) {
      case "crosshairs": {
        this.crosshairsDragStart(clientX, clientY);
        break;
      }
      case "selected": {
        this.selectedDragStart(clientX, clientY, targetId);
        break;
      }
    }
  }

  /**
   * Handles the move event depending on the state
   * @param event The mousemove or touchmove event
   * @param targetId The id of the event target
   */
  drag(event, targetId) {
    let { clientX, clientY } = this.getCoordinates(event);

    switch (this.#state) {
      case "crosshairs": {
        this.crosshairsMove(clientX, clientY);
        break;
      }
      case "draggingReady": {
        this.draggingReadyDrag(clientX, clientY);
        break;
      }
      case "dragging": {
        this.draggingDrag(clientX, clientY);
        break;
      }
      case "resizing": {
        this.resizingDrag(clientX, clientY);
        break;
      }
    }
  }

  /**
   * Handles the move event depending on the state
   * @param event The mouseup/tou
   * @param targetId The id of the event target
   */
  dragEnd(event, targetId) {
    let { clientX, clientY } = this.getCoordinates(event);

    switch (this.#state) {
      case "draggingReady": {
        this.draggingReadyDragEnd();
        break;
      }
      case "dragging": {
        this.draggingDragEnd(clientX, clientY, targetId);
        break;
      }
      case "resizing": {
        this.resizingDragEnd(clientX, clientY, targetId);
        break;
      }
    }
  }

  /**
   * Hide the box and highlighter and show the overlay at the start of crosshairs state
   */
  crosshairsStart() {
    this.#screenshotsContainer.hideSelectionLayer();
    this.#screenshotsContainer.showPreviewLayer();
  }

  /**
   * Hide the overlay and draw the box at the start of dragging state
   */
  draggingStart() {
    this.#screenshotsContainer.hidePreviewLayer();
    this.#screenshotsContainer.hideButtonsLayer();
    this.#screenshotsContainer.drawSelectionBox();
  }

  /**
   * Show the buttons at the start of the selected state
   */
  selectedStart() {
    this.#screenshotsContainer.drawButtonsLayer();
  }

  /**
   * Hide the buttons and store width and height of box at the start of the resizing state
   */
  resizingStart() {
    this.#screenshotsContainer.hideButtonsLayer();
    let {
      width,
      height,
    } = this.#screenshotsContainer.getSelectionLayerBoxDimensions();
    this.#lastBox = {
      width,
      height,
    };
  }

  /**
   * Set the initial box coordinates and set the state to "draggingReady"
   * @param clientX x coordinate
   * @param clientY y coordinate
   */
  crosshairsDragStart(clientX, clientY) {
    this.#screenshotsContainer.setSelectionBoxDimensions({
      left: clientX,
      top: clientY,
      right: clientX,
      bottom: clientY,
    });

    this.setState("draggingReady");
  }

  /**
   * If the background is clicked we set the state to crosshairs
   * otherwise set the state to resizing
   * @param clientX x coordinate
   * @param clientY y coordinate
   * @param targetId The id of the event target
   */
  selectedDragStart(clientX, clientY, targetId) {
    if (targetId === this.#screenshotsContainer.id) {
      this.setState("crosshairs");
      return;
    }
    this.#moverId = targetId;
    this.#lastX = clientX;
    this.#lastY = clientY;

    this.setState("resizing");
  }

  /**
   * Handles the pointer move for the crosshairs state
   * @param clientX x pointer position
   * @param clientY y pointer position
   */
  crosshairsMove(clientX, clientY) {
    this.#screenshotsContainer.drawPreviewEyes(clientX, clientY);
  }

  /**
   * Set the bottom and right coordinates of the box and draw the box
   * @param clientX x coordinate
   * @param clientY y coordinate
   */
  draggingDrag(clientX, clientY) {
    this.#screenshotsContainer.setSelectionBoxDimensions({
      right: clientX,
      bottom: clientY,
    });

    this.#screenshotsContainer.drawSelectionBox();
  }

  /**
   * If the mouse has moved at least 40 pixels then set the state to "dragging"
   * @param clientX x coordinate
   * @param clientY y coordinate
   */
  draggingReadyDrag(clientX, clientY) {
    this.#screenshotsContainer.setSelectionBoxDimensions({
      right: clientX,
      bottom: clientY,
    });

    if (this.#screenshotsContainer.selectionBoxDistance() > 40) {
      this.setState("dragging");
    }
  }

  /**
   * Depending on what mover was selected we will resize the box accordingly
   * @param clientX x coordinate
   * @param clientY y coordinate
   */
  resizingDrag(clientX, clientY) {
    switch (this.#moverId) {
      case "mover-topLeft": {
        this.#screenshotsContainer.setSelectionBoxDimensions({
          left: clientX,
          top: clientY,
        });
        break;
      }
      case "mover-top": {
        this.#screenshotsContainer.setSelectionBoxDimensions({ top: clientY });
        break;
      }
      case "mover-topRight": {
        this.#screenshotsContainer.setSelectionBoxDimensions({
          top: clientY,
          right: clientX,
        });
        break;
      }
      case "mover-right": {
        this.#screenshotsContainer.setSelectionBoxDimensions({
          right: clientX,
        });
        break;
      }
      case "mover-bottomRight": {
        this.#screenshotsContainer.setSelectionBoxDimensions({
          right: clientX,
          bottom: clientY,
        });
        break;
      }
      case "mover-bottom": {
        this.#screenshotsContainer.setSelectionBoxDimensions({
          bottom: clientY,
        });
        break;
      }
      case "mover-bottomLeft": {
        this.#screenshotsContainer.setSelectionBoxDimensions({
          left: clientX,
          bottom: clientY,
        });
        break;
      }
      case "mover-left": {
        this.#screenshotsContainer.setSelectionBoxDimensions({ left: clientX });
        break;
      }
      case "highlight": {
        let lastBox = this.#lastBox;
        let diffX = this.#lastX - clientX;
        let diffY = this.#lastY - clientY;

        let newLeft;
        let newRight;
        let newTop;
        let newBottom;

        // Unpack SelectionBox dimensions to use here
        let {
          boxLeft,
          boxTop,
          boxRight,
          boxBottom,
          boxWidth,
          boxHeight,
          scrollWidth,
          scrollHeight,
        } = this.#screenshotsContainer.getSelectionLayerDimensions();

        // wait until all 4 if elses have completed before setting box dimensions
        if (boxWidth <= lastBox.width && boxLeft === 0) {
          newLeft = boxRight - lastBox.width;
        } else {
          newLeft = boxLeft;
        }

        if (boxWidth <= lastBox.width && boxRight === scrollWidth) {
          newRight = boxLeft + lastBox.width;
        } else {
          newRight = boxRight;
        }

        if (boxHeight <= lastBox.height && boxTop === 0) {
          newTop = boxBottom - lastBox.height;
        } else {
          newTop = boxTop;
        }

        if (boxHeight <= lastBox.height && boxBottom === scrollHeight) {
          newBottom = boxTop + lastBox.height;
        } else {
          newBottom = boxBottom;
        }

        this.#screenshotsContainer.setSelectionBoxDimensions({
          left: newLeft - diffX,
          top: newTop - diffY,
          right: newRight - diffX,
          bottom: newBottom - diffY,
        });

        this.#lastX = clientX;
        this.#lastY = clientY;
        break;
      }
    }
    this.#screenshotsContainer.drawSelectionBox();
  }

  /**
   * Set the state to "crosshairs"
   */
  draggingReadyDragEnd() {
    this.setState("crosshairs");
  }

  /**
   * Draw the box one last time and set the state to "selected"
   * @param clientX x coordinate
   * @param clientY y coordinate
   */
  draggingDragEnd(clientX, clientY) {
    this.#screenshotsContainer.setSelectionBoxDimensions({
      right: clientX,
      bottom: clientY,
    });
    this.#screenshotsContainer.sortSelectionLayerBoxCoords();
    this.setState("selected");
  }

  /**
   * Draw the box one last time and set the state to "selected"
   * @param clientX x coordinate
   * @param clientY y coordinate
   */
  resizingDragEnd(clientX, clientY, targetId) {
    this.resizingDrag(clientX, clientY, targetId);
    this.#screenshotsContainer.sortSelectionLayerBoxCoords();
    this.setState("selected");
  }

  /**
   * The page was resized or scrolled. We need to update the
   * ScreenshotsContainer size so we don't draw outside the window bounds
   * If the current state is "selected" and this was called from a resize event
   * then we need to maybe shift the SelectionBox
   * @param win The window object of the page
   * @param eventType If this was called from a resize event
   */
  updateScreenshotsContainerSize(win, eventType) {
    this.#screenshotsContainer.updateSize(win);

    if (this.#state === "selected" && eventType === "resize") {
      this.#screenshotsContainer.shiftSelectionLayerBox();
    } else if (this.#state && eventType === "scroll") {
      this.#screenshotsContainer.drawButtonsLayer();
    }
  }
}

class AnonLayer {
  id;
  content;

  constructor(id, content) {
    this.id = id;
    this.content = content;
  }

  /**
   * Show element with id this.id
   */
  show() {
    this.content.removeAttributeForElement(this.id, "style");
  }

  /**
   * Hide element with id this.id
   */
  hide() {
    this.content.setAttributeForElement(this.id, "style", "display:none;");
  }
}

class SelectionLayer extends AnonLayer {
  #selectionBox;
  #buttons;
  #hidden;
  /**
   * the documentDimensions follows the below structure
   * {
   *    scrollWidth: the total document width
   *    scrollHeight: the total document height
   *    scrollX: the x scrolled offset
   *    scrollY: the y scrolled offset
   *    innerWidth: the viewport width
   *    innerHeight: the viewport height
   * }
   */
  #documentDimensions;

  constructor(id, content) {
    super(id, content);
    this.#selectionBox = new SelectionBox(content, this);
    this.#buttons = new ButtonsLayer("buttons", content, this);

    this.#hidden = true;
    this.#documentDimensions = {};
  }

  /**
   * Hide the buttons layer
   */
  hideButtons() {
    this.#buttons.hide();
  }

  /**
   * Call
   */
  drawButtonsLayer() {
    this.#buttons.show();
  }

  /**
   * Hide the selection-container element
   */
  hide() {
    super.hide();
    this.#hidden = true;
  }

  /**
   * Draw the SelectionBox
   */
  drawSelectionBox() {
    if (this.#hidden) {
      this.show();
      this.#hidden = false;
    }
    this.#selectionBox.show();
  }

  /**
   * Sort the SelectionBox coordinates
   */
  sortSelectionBoxCoords() {
    this.#selectionBox.sortCoords();
  }

  /**
   * Sets the SelectionBox dimensions
   * @param {Object} dims The new box dimensions
   *  {
   *    left: new left dimension value or undefined
   *    top: new top dimension value or undefined
   *    right: new right dimension value or undefined
   *    bottom: new bottom dimension value or undefined
   *   }
   */
  setSelectionBoxDimensions(dims) {
    if (dims.left) {
      this.#selectionBox.left = dims.left;
    }
    if (dims.top) {
      this.#selectionBox.top = dims.top;
    }
    if (dims.right) {
      this.#selectionBox.right = dims.right;
    }
    if (dims.bottom) {
      this.#selectionBox.bottom = dims.bottom;
    }
  }

  /**
   * Gets the selections box dimensions
   * @returns {Object}
   *  {
   *    x1: the left dimension value
   *    y1: the top dimension value
   *    width: the width of the selected region
   *    height: the height of the selected region
   *  }
   */
  getSelectionBoxDimensions() {
    return this.#selectionBox.getDimensions();
  }

  /**
   * Returns the box dimensions and the page dimensions
   * @returns {Object}
   *  {
   *    boxLeft: the left position of the box
   *    boxTop: the top position of the box
   *    boxRight: the right position of the box
   *    boxBottom: the bottom position of the box
   *    scrollWidth: the total document width
   *    scrollHeight: the total document height
   *    scrollX: the x scrolled offset
   *    scrollY: the y scrolled offset
   *    innerWidth: the viewport width
   *    innerHeight: the viewport height
   *  }
   */
  getDimensions() {
    return {
      boxLeft: this.#selectionBox.left,
      boxTop: this.#selectionBox.top,
      boxRight: this.#selectionBox.right,
      boxBottom: this.#selectionBox.bottom,
      boxWidth: this.#selectionBox.width,
      boxHeight: this.#selectionBox.height,
      ...this.#documentDimensions,
    };
  }

  /**
   * Gets the diagonal distance of the SelectionBox
   * @returns The diagonal distance of the SelectionBox
   */
  getSelectionBoxDistance() {
    return this.#selectionBox.distance;
  }

  /**
   * Shift the SelectionBox so that it is always within the document
   */
  shiftSelectionBox() {
    this.#selectionBox.shiftBox();
  }

  get scrollWidth() {
    return this.#documentDimensions.scrollWidth;
  }
  set scrollWidth(val) {
    this.#documentDimensions.scrollWidth = val;
  }

  get scrollHeight() {
    return this.#documentDimensions.scrollHeight;
  }
  set scrollHeight(val) {
    this.#documentDimensions.scrollHeight = val;
  }

  get scrollX() {
    return this.#documentDimensions.scrollX;
  }
  set scrollX(val) {
    this.#documentDimensions.scrollX = val;
  }

  get scrollY() {
    return this.#documentDimensions.scrollY;
  }
  set scrollY(val) {
    this.#documentDimensions.scrollY = val;
  }

  get innerWidth() {
    return this.#documentDimensions.innerWidth;
  }
  set innerWidth(val) {
    this.#documentDimensions.innerWidth = val;
  }

  get innerHeight() {
    return this.#documentDimensions.innerHeight;
  }
  set innerHeight(val) {
    this.#documentDimensions.innerHeight = val;
  }
}

/**
 * The SelectionBox class handles drawing the highlight and background
 */
class SelectionBox extends AnonLayer {
  #x1;
  #x2;
  #y1;
  #y2;
  #xOffset;
  #yOffset;
  #selectionLayer;

  constructor(content, selectionLayer) {
    super("", content);

    this.#selectionLayer = selectionLayer;

    this.#x1 = 0;
    this.#x2 = 0;
    this.#y1 = 0;
    this.#y2 = 0;
    this.#xOffset = 0;
    this.#yOffset = 0;
  }

  /**
   * Draw the selected region for screenshotting
   */
  show() {
    this.content.setAttributeForElement(
      "highlight",
      "style",
      `top:${this.top}px;left:${this.left}px;height:${this.height}px;width:${this.width}px;`
    );

    this.content.setAttributeForElement(
      "bgTop",
      "style",
      `top:0px;height:${this.top}px;left:0px;width:100%;`
    );

    this.content.setAttributeForElement(
      "bgBottom",
      "style",
      `top:${this.bottom}px;height:calc(100% - ${this.bottom}px);left:0px;width:100%;`
    );

    this.content.setAttributeForElement(
      "bgLeft",
      "style",
      `top:${this.top}px;height:${this.height}px;left:0px;width:${this.left}px;`
    );

    this.content.setAttributeForElement(
      "bgRight",
      "style",
      `top:${this.top}px;height:${this.height}px;left:${this.right}px;width:calc(100% - ${this.right}px);`
    );
  }

  /**
   * Hide the selected region
   */
  hide() {
    this.content.setAttributeForElement("highlight", "style", "display:none;");
    this.content.setAttributeForElement("bgTop", "style", "display:none;");
    this.content.setAttributeForElement("bgBottom", "style", "display:none;");
    this.content.setAttributeForElement("bgLeft", "style", "display:none;");
    this.content.setAttributeForElement("bgRight", "style", "display:none;");
  }

  /**
   * The box should never appear outside the document so the SelectionBox will
   * be shifted if the bounds of the box are outside the documents width or height
   */
  shiftBox() {
    let didShift = false;
    let xDiff = this.right - this.#selectionLayer.scrollWidth;
    if (xDiff > 0) {
      this.right -= xDiff;
      this.left -= xDiff;

      didShift = true;
    }

    let yDiff = this.bottom - this.#selectionLayer.scrollHeight;
    if (yDiff > 0) {
      let curWidth = this.width;

      this.bottom -= yDiff;
      this.top = this.bottom - curWidth;

      didShift = true;
    }

    if (didShift) {
      this.show();
      this.#selectionLayer.drawButtonsLayer();
    }
  }

  /**
   * Sort the coordinates so x1 < x2 and y1 < y2
   */
  sortCoords() {
    if (this.#x1 > this.#x2) {
      [this.#x1, this.#x2] = [this.#x2, this.#x1];
    }
    if (this.#y1 > this.#y2) {
      [this.#y1, this.#y2] = [this.#y2, this.#y1];
    }
  }

  /**
   * Gets the dimensions of the currently selected region
   * @returns {Object}
   *  {
   *    x1: the left dimension value
   *    y1: the top dimension value
   *    width: the width of the selected region
   *    height: the height of the selected region
   *  }
   */
  getDimensions() {
    return {
      x1: this.left,
      y1: this.top,
      width: this.width,
      height: this.height,
    };
  }

  get distance() {
    return Math.sqrt(Math.pow(this.width, 2) + Math.pow(this.height, 2));
  }

  get xOffset() {
    return this.#xOffset;
  }
  set xOffset(val) {
    this.#xOffset = val;
  }

  get yOffset() {
    return this.#yOffset;
  }
  set yOffset(val) {
    this.#yOffset = val;
  }

  get top() {
    return Math.min(this.#y1, this.#y2);
  }
  set top(val) {
    this.#y1 = val > 0 ? val : 0;
  }

  get left() {
    return Math.min(this.#x1, this.#x2);
  }
  set left(val) {
    this.#x1 = val > 0 ? val : 0;
  }

  get right() {
    return Math.max(this.#x1, this.#x2);
  }
  set right(val) {
    this.#x2 =
      val > this.#selectionLayer.scrollWidth
        ? this.#selectionLayer.scrollWidth
        : val;
  }

  get bottom() {
    return Math.max(this.#y1, this.#y2);
  }
  set bottom(val) {
    this.#y2 =
      val > this.#selectionLayer.scrollHeight
        ? this.#selectionLayer.scrollHeight
        : val;
  }

  get width() {
    return Math.abs(this.#x2 - this.#x1);
  }
  get height() {
    return Math.abs(this.#y2 - this.#y1);
  }
}

class ButtonsLayer extends AnonLayer {
  #selectionLayer;

  constructor(id, content, selectionLayer) {
    super(id, content);

    this.#selectionLayer = selectionLayer;
  }

  /**
   * Draw the buttons. Check if the box is too near the bottom or left of the
   * viewport and adjust the buttons accordingly
   */
  show() {
    let {
      boxLeft,
      boxTop,
      boxRight,
      boxBottom,
      scrollX,
      scrollY,
      innerWidth,
      innerHeight,
    } = this.#selectionLayer.getDimensions();

    if (
      boxTop > scrollY + innerHeight ||
      boxBottom < scrollY ||
      boxLeft > scrollX + innerWidth ||
      boxRight < scrollX
    ) {
      // The box is offscreen so need to draw the buttons
      return;
    }

    let top = boxBottom;
    let leftOrRight = `right:calc(100% - ${boxRight}px);`;

    if (scrollY + innerHeight - boxBottom < 70) {
      if (boxBottom < scrollY + innerHeight) {
        top = boxBottom - 60;
      } else if (scrollY + innerHeight - boxTop < 70) {
        top = boxTop - 60;
      } else {
        top = scrollY + innerHeight - 60;
      }
    }
    if (boxRight < 265) {
      leftOrRight = `left:${boxLeft}px;`;
    }

    this.content.setAttributeForElement(
      "buttons",
      "style",
      `top:${top}px;${leftOrRight}`
    );
  }
}

class PreviewLayer extends AnonLayer {
  constructor(id, content) {
    super(id, content);
  }

  /**
   * Draw the eyeballs facing the mouse
   * @param clientX x pointer position
   * @param clientY y pointer position
   * @param width width of the viewport
   * @param height height of the viewport
   */
  drawEyes(clientX, clientY, width, height) {
    const xpos = Math.floor((10 * (clientX - width / 2)) / width);
    const ypos = Math.floor((10 * (clientY - height / 2)) / height);
    const move = `transform:translate(${xpos}px, ${ypos}px);`;
    this.content.setAttributeForElement("left-eye", "style", move);
    this.content.setAttributeForElement("right-eye", "style", move);
  }
}

class ScreenshotsContainerLayer extends AnonLayer {
  #width;
  #height;
  #previewLayer;
  #selectionLayer;

  constructor(id, content, previewLayer, selectionLayer) {
    super(id, content);

    this.#previewLayer = previewLayer;
    this.#selectionLayer = selectionLayer;
  }

  /**
   * Hide the SelectionLayer
   */
  hideSelectionLayer() {
    this.#selectionLayer.hide();
  }

  /**
   * Show the PreviewLayer
   */
  showPreviewLayer() {
    this.#previewLayer.show();
  }

  /**
   * Hide the PreviewLayer
   */
  hidePreviewLayer() {
    this.#previewLayer.hide();
  }

  /**
   * Show the ButtonsLayer
   */
  drawButtonsLayer() {
    this.#selectionLayer.drawButtonsLayer();
  }

  /**
   * Hide the ButtonsLayer
   */
  hideButtonsLayer() {
    this.#selectionLayer.hideButtons();
  }

  /**
   * Show the SelectionBox
   */
  drawSelectionBox() {
    this.#selectionLayer.drawSelectionBox();
  }

  /**
   * Draw the eyes in the PreviewLayer
   * @param clientX The x mouse position
   * @param clientY The y mouse position
   */
  drawPreviewEyes(clientX, clientY) {
    this.#previewLayer.drawEyes(
      clientX - this.#selectionLayer.scrollX,
      clientY - this.#selectionLayer.scrollY,
      this.#selectionLayer.innerWidth,
      this.#selectionLayer.innerHeight
    );
  }

  /**
   * Get the diagonal distance of the SelectionBox
   * @returns The diagonal distance of the currently selected region
   */
  selectionBoxDistance() {
    return this.#selectionLayer.getSelectionBoxDistance();
  }

  /**
   * Sort the coordinates of the SelectionBox
   */
  sortSelectionLayerBoxCoords() {
    this.#selectionLayer.sortSelectionBoxCoords();
  }

  /**
   * Get the SelectionLayer dimensions
   * @returns {Object}
   *  {
   *    x1: the left dimension value
   *    y1: the top dimension value
   *    width: the width of the selected region
   *    height: the height of the selected region
   *  }
   */
  getSelectionLayerBoxDimensions() {
    return this.#selectionLayer.getSelectionBoxDimensions();
  }

  /**
   * Gets the SelectionBox and page dimensions
   * @returns {Object}
   *  {
   *    boxLeft: the left position of the box
   *    boxTop: the top position of the box
   *    boxRight: the right position of the box
   *    boxBottom: the bottom position of the box
   *    scrollWidth: the total document width
   *    scrollHeight: the total document height
   *    scrollX: the x scrolled offset
   *    scrollY: the y scrolled offset
   *    innerWidth: the viewport width
   *    innerHeight: the viewport height
   *  }
   */
  getSelectionLayerDimensions() {
    return this.#selectionLayer.getDimensions();
  }

  /**
   * Shift the SelectionBox
   */
  shiftSelectionLayerBox() {
    this.#selectionLayer.shiftSelectionBox();
  }

  /**
   * Set the respective dimensions of the SelectionBox
   * @param {Object} boxDimensionObject The new box dimensions
   *  {
   *    left: new left dimension value or undefined
   *    top: new top dimension value or undefined
   *    right: new right dimension value or undefined
   *    bottom: new bottom dimension value or undefined
   *   }
   */
  setSelectionBoxDimensions(boxDimensionObject) {
    this.#selectionLayer.setSelectionBoxDimensions(boxDimensionObject);
  }

  /**
   * The screenshots-overlay-container doesn't shrink with the window when the
   * window is resized so we have to manually find the width and height of the
   * window by looping throught the documentElement's children
   * If the children mysteriously have a height or width of 0 then we will
   * fallback to the scrollWidth and scrollHeight which can cause the container
   * to be larger than the window dimensions
   * @param win The window object
   */
  updateSize(win) {
    let { innerWidth, innerHeight, scrollX, scrollY } = win;
    this.#selectionLayer.innerWidth = innerWidth;
    this.#selectionLayer.innerHeight = innerHeight;
    this.#selectionLayer.scrollX = scrollX;
    this.#selectionLayer.scrollY = scrollY;

    const doc = win.document.documentElement;
    let width = Math.max.apply(
      null,
      Array.from(doc.children, x => x.scrollWidth)
    );
    let height = Math.max.apply(
      null,
      Array.from(doc.children, x => x.scrollHeight)
    );

    if (width < 1) {
      width = doc.scrollWidth;
    }
    if (height < 1) {
      height = doc.scrollHeight;
    }

    this.#selectionLayer.scrollWidth = width;
    this.#selectionLayer.scrollHeight = height;

    this.#width = width;
    this.#height = height;

    this.drawScreenshotsContainer();
  }

  /**
   * Return the dimensions of the screenshots container
   * @returns {Object}
   *  width: the container width
   *  height: the container height
   */
  getDimension() {
    return { width: this.#width, height: this.#height };
  }

  /**
   * Draw the screenshots container
   */
  drawScreenshotsContainer() {
    this.content.setAttributeForElement(
      this.id,
      "style",
      `top:0;left:0;width:${this.#width}px;height:${this.#height}px;`
    );
  }
}
