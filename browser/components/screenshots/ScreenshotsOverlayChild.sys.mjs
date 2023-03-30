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

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

XPCOMUtils.defineLazyGetter(lazy, "overlayLocalization", () => {
  return new Localization(["browser/screenshotsOverlay.ftl"], true);
});

const STYLESHEET_URL =
  "chrome://browser/content/screenshots/overlay/overlay.css";

// An autoselection smaller than these will be ignored entirely:
const MIN_DETECT_ABSOLUTE_HEIGHT = 10;
const MIN_DETECT_ABSOLUTE_WIDTH = 30;
// An autoselection smaller than these will not be preferred:
const MIN_DETECT_HEIGHT = 30;
const MIN_DETECT_WIDTH = 100;
// An autoselection bigger than either of these will be ignored:
let MAX_DETECT_HEIGHT = 700;
let MAX_DETECT_WIDTH = 1000;

const REGION_CHANGE_THRESHOLD = 5;
const SCROLL_BY_EDGE = 20;

const doNotAutoselectTags = {
  H1: true,
  H2: true,
  H3: true,
  H4: true,
  H5: true,
  H6: true,
};

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
    this.hoverBoxId = "hover-highlight";

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

    const hoverElementBox = new HoverElementBox(
      this.hoverBoxId,
      this.content,
      document
    );

    const previewLayer = new PreviewLayer(this.previewId, this.content);
    const selectionLayer = new SelectionLayer(
      this.selectionId,
      this.content,
      hoverElementBox
    );

    this.screenshotsContainer = new ScreenshotsContainerLayer(
      this.overlayId,
      this.content,
      previewLayer,
      selectionLayer
    );

    this.stateHandler = new StateHandler(
      this.screenshotsContainer,
      this.screenshotsChild
    );

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
    let cancelScreenshotsFunciton = () => {
      this.screenshotsChild.requestCancelScreenshot("overlay_cancel");
    };
    this.addEventListenerForElement(
      "screenshots-cancel-button",
      "click",
      cancelScreenshotsFunciton
    );
    this.addEventListenerForElement(
      "cancel",
      "click",
      cancelScreenshotsFunciton
    );
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
      instructions,
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
            <div class="preview-instructions">${instructions.value}</div>
            <button class="screenshots-button" id="screenshots-cancel-button">${cancel.value}</button>
          </div>
        </div>
        <div id="${this.hoverBoxId}"></div>
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
            <button id="download" class="screenshots-button primary" title="${download.value}" aria-label="${download.value}"><img/>${download.value}</button>
          </div>
        </div>
      </div>
    </div>`;

    const parser = new this.contentDocument.ownerGlobal.DOMParser();
    const tmpDoc = parser.parseFromSafeString(htmlString, "text/html");
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
    this.stateHandler.drag(event, targetId);
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

export var ScreenshotsOverlayChild = {
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
  #screenshotsChild;
  #previousDimensions;

  constructor(screenshotsContainer, screenshotsChild) {
    this.#state = "crosshairs";
    this.#lastBox = {};

    this.#screenshotsContainer = screenshotsContainer;
    this.#screenshotsChild = screenshotsChild;
  }

  setState(newState) {
    if (this.#state === "selected" && newState === "crosshairs") {
      this.#screenshotsChild.recordTelemetryEvent(
        "started",
        "overlay_retry",
        {}
      );
    }
    this.#state = newState;
    this.start();
  }

  getState() {
    return this.#state;
  }

  getHoverElementBoxRect() {
    return this.#screenshotsContainer.hoverElementBoxRect;
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
      case "draggingReady": {
        this.draggingReadyStart();
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
    const { clientX, clientY, pageX, pageY } = event;

    MAX_DETECT_HEIGHT = Math.max(event.target.clientHeight + 100, 700);
    MAX_DETECT_WIDTH = Math.max(event.target.clientWidth + 100, 1000);

    return { clientX, clientY, pageX, pageY };
  }

  /**
   * Handles the mousedown/touchstart event depending on the state
   * @param event The mousedown or touchstart event
   * @param targetId The id of the event target
   */
  dragStart(event, targetId) {
    const { pageX, pageY } = this.getCoordinates(event);

    switch (this.#state) {
      case "crosshairs": {
        this.crosshairsDragStart(pageX, pageY);
        break;
      }
      case "selected": {
        this.selectedDragStart(pageX, pageY, targetId);
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
    const { pageX, pageY, clientX, clientY } = this.getCoordinates(event);

    switch (this.#state) {
      case "crosshairs": {
        this.crosshairsMove(clientX, clientY, targetId);
        break;
      }
      case "draggingReady": {
        this.draggingReadyDrag(pageX, pageY);
        break;
      }
      case "dragging": {
        this.draggingDrag(pageX, pageY);
        break;
      }
      case "resizing": {
        this.resizingDrag(pageX, pageY);
        break;
      }
    }
  }

  /**
   * Handles the move event depending on the state
   * @param event The mouseup event
   * @param targetId The id of the event target
   */
  dragEnd(event, targetId) {
    const { pageX, pageY, clientX, clientY } = this.getCoordinates(event);

    switch (this.#state) {
      case "draggingReady": {
        this.draggingReadyDragEnd(pageX - clientX, pageY - clientY);
        break;
      }
      case "dragging": {
        this.draggingDragEnd(pageX, pageY, targetId);
        break;
      }
      case "resizing": {
        this.resizingDragEnd(pageX, pageY, targetId);
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
    this.#screenshotsChild.showPanel();
    this.#previousDimensions = null;
  }

  /**
   *
   */
  draggingReadyStart() {
    this.#screenshotsChild.hidePanel();
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
   * @param pageX x coordinate
   * @param pageY y coordinate
   */
  crosshairsDragStart(pageX, pageY) {
    this.#screenshotsContainer.setSelectionBoxDimensions({
      left: pageX,
      top: pageY,
      right: pageX,
      bottom: pageY,
    });

    this.setState("draggingReady");
  }

  /**
   * If the background is clicked we set the state to crosshairs
   * otherwise set the state to resizing
   * @param pageX x coordinate
   * @param pageY y coordinate
   * @param targetId The id of the event target
   */
  selectedDragStart(pageX, pageY, targetId) {
    if (targetId === this.#screenshotsContainer.id) {
      this.setState("crosshairs");
      return;
    }
    this.#moverId = targetId;
    this.#lastX = pageX;
    this.#lastY = pageY;

    this.setState("resizing");
  }

  /**
   * Handles the pointer move for the crosshairs state
   * @param clientX x pointer position in the visible window
   * @param clientY y pointer position in the visible window
   * @param targetId The id of the target element
   */
  crosshairsMove(clientX, clientY, targetId) {
    this.#screenshotsContainer.drawPreviewEyes(clientX, clientY);

    this.#screenshotsContainer.handleElementHover(clientX, clientY, targetId);
  }

  /**
   * Set the bottom and right coordinates of the box and draw the box
   * @param pageX x coordinate
   * @param pageY y coordinate
   */
  draggingDrag(pageX, pageY) {
    this.scrollIfByEdge(pageX, pageY);
    this.#screenshotsContainer.setSelectionBoxDimensions({
      right: pageX,
      bottom: pageY,
    });

    this.#screenshotsContainer.drawSelectionBox();
  }

  /**
   * If the mouse has moved at least 40 pixels then set the state to "dragging"
   * @param pageX x coordinate
   * @param pageY y coordinate
   */
  draggingReadyDrag(pageX, pageY) {
    this.#screenshotsContainer.setSelectionBoxDimensions({
      right: pageX,
      bottom: pageY,
    });

    if (this.#screenshotsContainer.selectionBoxDistance() > 40) {
      this.setState("dragging");
    }
  }

  /**
   * Depending on what mover was selected we will resize the box accordingly
   * @param pageX x coordinate
   * @param pageY y coordinate
   */
  resizingDrag(pageX, pageY) {
    this.scrollIfByEdge(pageX, pageY);
    switch (this.#moverId) {
      case "mover-topLeft": {
        this.#screenshotsContainer.setSelectionBoxDimensions({
          left: pageX,
          top: pageY,
        });
        break;
      }
      case "mover-top": {
        this.#screenshotsContainer.setSelectionBoxDimensions({ top: pageY });
        break;
      }
      case "mover-topRight": {
        this.#screenshotsContainer.setSelectionBoxDimensions({
          top: pageY,
          right: pageX,
        });
        break;
      }
      case "mover-right": {
        this.#screenshotsContainer.setSelectionBoxDimensions({
          right: pageX,
        });
        break;
      }
      case "mover-bottomRight": {
        this.#screenshotsContainer.setSelectionBoxDimensions({
          right: pageX,
          bottom: pageY,
        });
        break;
      }
      case "mover-bottom": {
        this.#screenshotsContainer.setSelectionBoxDimensions({
          bottom: pageY,
        });
        break;
      }
      case "mover-bottomLeft": {
        this.#screenshotsContainer.setSelectionBoxDimensions({
          left: pageX,
          bottom: pageY,
        });
        break;
      }
      case "mover-left": {
        this.#screenshotsContainer.setSelectionBoxDimensions({ left: pageX });
        break;
      }
      case "highlight": {
        let lastBox = this.#lastBox;
        let diffX = this.#lastX - pageX;
        let diffY = this.#lastY - pageY;

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

        this.#lastX = pageX;
        this.#lastY = pageY;
        break;
      }
    }
    this.#screenshotsContainer.drawSelectionBox();
  }

  /**
   * Draw the selection box from the hover element box if it exists
   * Else set the state to "crosshairs"
   */
  draggingReadyDragEnd(scrollX, scrollY) {
    if (this.#screenshotsContainer.hoverElementBoxRect) {
      this.#screenshotsContainer.hidePreviewLayer();
      this.#screenshotsContainer.updateSelectionBoxFromRect(scrollX, scrollY);
      this.#screenshotsContainer.drawSelectionBox();
      this.setState("selected");
      this.#screenshotsChild.recordTelemetryEvent("selected", "element", {});
    } else {
      this.setState("crosshairs");
    }
  }

  /**
   * Draw the box one last time and set the state to "selected"
   * @param pageX x coordinate
   * @param pageY y coordinate
   */
  draggingDragEnd(pageX, pageY) {
    this.#screenshotsContainer.setSelectionBoxDimensions({
      right: pageX,
      bottom: pageY,
    });
    this.#screenshotsContainer.sortSelectionLayerBoxCoords();
    this.setState("selected");

    let {
      width,
      height,
    } = this.#screenshotsContainer.getSelectionLayerBoxDimensions();

    if (
      !this.#previousDimensions ||
      (Math.abs(this.#previousDimensions.width - width) >
        REGION_CHANGE_THRESHOLD &&
        Math.abs(this.#previousDimensions.height - height) >
          REGION_CHANGE_THRESHOLD)
    ) {
      this.#screenshotsChild.recordTelemetryEvent(
        "selected",
        "region_selection",
        {}
      );
    }
    this.#previousDimensions = { width, height };
  }

  /**
   * Draw the box one last time and set the state to "selected"
   * @param pageX x coordinate
   * @param pageY y coordinate
   */
  resizingDragEnd(pageX, pageY, targetId) {
    this.resizingDrag(pageX, pageY, targetId);
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
    if (this.#state === "crosshairs" && eventType === "resize") {
      this.#screenshotsContainer.hideHoverElementBox();
    }

    this.#screenshotsContainer.updateSize(win);

    if (this.#state === "selected" && eventType === "resize") {
      this.#screenshotsContainer.shiftSelectionLayerBox();
    } else if (
      this.#state !== "resizing" &&
      this.#state !== "dragging" &&
      eventType === "scroll"
    ) {
      this.#screenshotsContainer.drawButtonsLayer();
      if (this.#state === "crosshairs") {
        this.#screenshotsContainer.handleElementScroll();
      }
    }
  }

  scrollIfByEdge(pageX, pageY) {
    let dimensions = this.#screenshotsContainer.getSelectionLayerDimensions();

    if (pageY - dimensions.scrollY <= SCROLL_BY_EDGE) {
      // Scroll up
      this.#screenshotsChild.scrollWindow(0, -SCROLL_BY_EDGE);
    } else if (
      dimensions.scrollY + dimensions.clientHeight - pageY <=
      SCROLL_BY_EDGE
    ) {
      // Scroll down
      this.#screenshotsChild.scrollWindow(0, SCROLL_BY_EDGE);
    }

    if (pageX - dimensions.scrollX <= SCROLL_BY_EDGE) {
      // Scroll left
      this.#screenshotsChild.scrollWindow(-SCROLL_BY_EDGE, 0);
    } else if (
      dimensions.scrollX + dimensions.clientWidth - pageX <=
      SCROLL_BY_EDGE
    ) {
      // Scroll right
      this.#screenshotsChild.scrollWindow(SCROLL_BY_EDGE, 0);
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

class HoverElementBox extends AnonLayer {
  #document;
  #rect;
  #lastX;
  #lastY;

  constructor(id, content, document) {
    super(id, content);

    this.#document = document;
  }

  get rect() {
    return this.#rect;
  }

  /**
   * Draws the hover box over an element from the given rect
   * @param rect The rect to draw the hover element box
   */
  drawHoverBox(rect) {
    if (!rect) {
      this.hide();
    } else {
      let maxHeight = this.selectionLayer.scrollHeight;
      let maxWidth = this.selectionLayer.scrollWidth;
      let top = this.#document.documentElement.scrollTop + rect.top;
      top = top > 0 ? top : 0;
      let left = this.#document.documentElement.scrollLeft + rect.left;
      left = left > 0 ? left : 0;
      let height =
        rect.top + rect.height > maxHeight ? maxHeight - rect.top : rect.height;
      let width =
        rect.left + rect.width > maxWidth ? maxWidth - rect.left : rect.width;

      this.content.setAttributeForElement(
        this.id,
        "style",
        `top:${top}px;left:${left}px;height:${height}px;width:${width}px;`
      );
    }
  }

  /**
   * Handles when the user moves the mouse over an element
   * @param clientX The x coordinate in the visible window
   * @param clientY The y coordinate in the visible window
   * @param targetId The target element id
   */
  handleElementHover(clientX, clientY, targetId) {
    if (targetId === "screenshots-overlay-container") {
      let ele = this.getElementFromPoint(clientX, clientY);

      if (this.cachedEle && this.cachedEle === ele) {
        // Still hovering over the same element
        return;
      }
      this.cachedEle = ele;

      this.getBestRectForElement(ele);

      this.#lastX = clientX;
      this.#lastY = clientY;
    }
  }

  /**
   * Handles moving the rect when the user has scrolled but not moved the mouse
   * It uses the last x and y coordinates to find the new element at the mouse position
   */
  handleElementScroll() {
    if (this.#lastX && this.#lastY) {
      this.cachedEle = null;
      this.handleElementHover(
        this.#lastX,
        this.#lastY,
        "screenshots-overlay-container"
      );
    }
  }

  /**
   * Finds an element for the given coordinates within the viewport
   * @param x The x coordinate in the visible window
   * @param y The y coordinate in the visible window
   * @returns An element location at the given coordinates
   */
  getElementFromPoint(x, y) {
    this.setPointerEventsNone();
    let ele;
    try {
      ele = this.#document.elementFromPoint(x, y);
    } finally {
      this.resetPointerEvents();
    }

    return ele;
  }

  /**
   * Gets the rect for an element if getBoundingClientRect exists
   * @param ele The element to get the rect from
   * @returns The bounding client rect of the element or null
   */
  getBoundingClientRect(ele) {
    if (!ele.getBoundingClientRect) {
      return null;
    }

    return ele.getBoundingClientRect();
  }

  /**
   * This function takes an element and finds a suitable rect to draw the hover box on
   * @param ele The element to find a suitale rect of
   */
  getBestRectForElement(ele) {
    let lastRect;
    let lastNode;
    let rect;
    let attemptExtend = false;
    let node = ele;
    while (node) {
      rect = this.getBoundingClientRect(node);
      if (!rect) {
        rect = lastRect;
        break;
      }
      if (rect.width < MIN_DETECT_WIDTH || rect.height < MIN_DETECT_HEIGHT) {
        // Avoid infinite loop for elements with zero or nearly zero height,
        // like non-clearfixed float parents with or without borders.
        break;
      }
      if (rect.width > MAX_DETECT_WIDTH || rect.height > MAX_DETECT_HEIGHT) {
        // Then the last rectangle is better
        rect = lastRect;
        attemptExtend = true;
        break;
      }
      if (rect.width >= MIN_DETECT_WIDTH && rect.height >= MIN_DETECT_HEIGHT) {
        if (!doNotAutoselectTags[node.tagName]) {
          break;
        }
      }
      lastRect = rect;
      lastNode = node;
      node = node.parentNode;
    }
    if (rect && node) {
      const evenBetter = this.evenBetterElement(node);
      if (evenBetter) {
        node = lastNode = evenBetter;
        rect = this.getBoundingClientRect(evenBetter);
        attemptExtend = false;
      }
    }
    if (rect && attemptExtend) {
      let extendNode = lastNode.nextSibling;
      while (extendNode) {
        if (extendNode.nodeType === this.#document.ELEMENT_NODE) {
          break;
        }
        extendNode = extendNode.nextSibling;
        if (!extendNode) {
          const parent = lastNode.parentNode;
          for (let i = 0; i < parent.childNodes.length; i++) {
            if (parent.childNodes[i] === lastNode) {
              extendNode = parent.childNodes[i + 1];
            }
          }
        }
      }
      if (extendNode) {
        const extendRect = this.getBoundingClientRect(extendNode);
        let x = Math.min(rect.x, extendRect.x);
        let y = Math.min(rect.y, extendRect.y);
        let width = Math.max(rect.right, extendRect.right) - x;
        let height = Math.max(rect.bottom, extendRect.bottom) - y;
        const combinedRect = new DOMRect(x, y, width, height);
        if (
          combinedRect.width <= MAX_DETECT_WIDTH &&
          combinedRect.height <= MAX_DETECT_HEIGHT
        ) {
          rect = combinedRect;
        }
      }
    }

    if (
      rect &&
      (rect.width < MIN_DETECT_ABSOLUTE_WIDTH ||
        rect.height < MIN_DETECT_ABSOLUTE_HEIGHT)
    ) {
      rect = null;
    }

    if (!rect) {
      this.hide();
    } else {
      this.drawHoverBox(rect);
    }

    this.#rect = rect;
  }

  /**
   * This finds a better element by looking for elements with role article
   * @param node The currently hovered node
   * @returns A better node or null
   */
  evenBetterElement(node) {
    let el = node.parentNode;
    const ELEMENT_NODE = this.#document.ELEMENT_NODE;
    while (el && el.nodeType === ELEMENT_NODE) {
      if (!el.getAttribute) {
        return null;
      }
      if (el.getAttribute("role") === "article") {
        const rect = this.getBoundingClientRect(el);
        if (!rect) {
          return null;
        }
        if (
          rect.width <= MAX_DETECT_WIDTH &&
          rect.height <= MAX_DETECT_HEIGHT
        ) {
          return el;
        }
        return null;
      }
      el = el.parentNode;
    }
    return null;
  }

  /**
   * The pointer events need to be removed temporarily so we can find the
   * correct element from document.elementFromPoint()
   * If the pointer events are on for the screenshots elements, then we will always
   * get the screenshots elements as the elements from a given point
   */
  setPointerEventsNone() {
    this.content.setAttributeForElement(
      "screenshots-component",
      "style",
      "pointer-events:none;"
    );

    let temp = this.content.getAttributeForElement(
      "screenshots-overlay-container",
      "style"
    );
    this.content.setAttributeForElement(
      "screenshots-overlay-container",
      "style",
      temp + "pointer-events:none;"
    );
  }

  /**
   * Return the pointer events to the original state because we found the element
   */
  resetPointerEvents() {
    this.content.setAttributeForElement("screenshots-component", "style", "");

    let temp = this.content.getAttributeForElement(
      "screenshots-overlay-container",
      "style"
    );
    this.content.setAttributeForElement(
      "screenshots-overlay-container",
      "style",
      temp.replace("pointer-events:none;", "")
    );
  }
}

class SelectionLayer extends AnonLayer {
  #selectionBox;
  #hoverElementBox;
  #buttons;
  #hidden;
  /**
   * the documentDimensions follows the below structure
   * {
   *    scrollWidth: the total document width
   *    scrollHeight: the total document height
   *    scrollX: the x scrolled offset
   *    scrollY: the y scrolled offset
   *    clientWidth: the viewport width
   *    clientHeight: the viewport height
   * }
   */
  #documentDimensions;

  constructor(id, content, hoverElementBox) {
    super(id, content);
    this.#selectionBox = new SelectionBox(content, this);
    this.#buttons = new ButtonsLayer("buttons", content, this);
    this.#hoverElementBox = hoverElementBox;
    this.#hoverElementBox.selectionLayer = this;

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
   *    clientWidth: the viewport width
   *    clientHeight: the viewport height
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

  /**
   * Update the box coordinates from the hover element rect
   */
  updateSelectionBoxFromRect(scrollX, scrollY) {
    this.#selectionBox.updateBoxFromRect(
      this.#hoverElementBox.rect,
      scrollX,
      scrollY
    );
  }

  /**
   * Handles when the user moves the mouse over an element
   * @param clientX The x coordinate in the visible window
   * @param clientY The y coordinate in the visible window
   * @param targetId The target element id
   */
  handleElementHover(clientX, clientY, targetId) {
    this.#hoverElementBox.handleElementHover(clientX, clientY, targetId);
  }

  /**
   * Handles moving the rect when the user has scrolled but not moved the mouse
   * It uses the last x and y coordinates to find the new element at the mouse position
   */
  handleElementScroll() {
    this.#hoverElementBox.handleElementScroll();
  }

  hideHoverElementSelection() {
    this.#hoverElementBox.hide();
  }

  get hoverElementBoxRect() {
    return this.#hoverElementBox.rect;
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

  get clientWidth() {
    return this.#documentDimensions.clientWidth;
  }
  set clientWidth(val) {
    this.#documentDimensions.clientWidth = val;
  }

  get clientHeight() {
    return this.#documentDimensions.clientHeight;
  }
  set clientHeight(val) {
    this.#documentDimensions.clientHeight = val;
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
   * Update the box coordinates from the rect
   * @param rect The hover element box
   * @param scrollX The x offset the page is scrolled
   * @param scrollY The y offset the page is scrolled
   */
  updateBoxFromRect(rect, scrollX, scrollY) {
    this.top = rect.top + scrollY;
    this.left = rect.left + scrollX;
    this.right = rect.right + scrollX;
    this.bottom = rect.bottom + scrollY;
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
      let curHeight = this.height;

      this.bottom -= yDiff;
      this.top = this.bottom - curHeight;

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
      clientWidth,
      clientHeight,
    } = this.#selectionLayer.getDimensions();

    if (
      boxTop > scrollY + clientHeight ||
      boxBottom < scrollY ||
      boxLeft > scrollX + clientWidth ||
      boxRight < scrollX
    ) {
      // The box is offscreen so need to draw the buttons
      return;
    }

    let top = boxBottom;
    let leftOrRight = `right:calc(100% - ${boxRight}px);`;

    if (scrollY + clientHeight - boxBottom < 70) {
      if (boxBottom < scrollY + clientHeight) {
        top = boxBottom - 60;
      } else if (scrollY + clientHeight - boxTop < 70) {
        top = boxTop - 60;
      } else {
        top = scrollY + clientHeight - 60;
      }
    }
    if (boxRight < 300) {
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
    this.#selectionLayer.hideHoverElementSelection();
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

  hideHoverElementBox() {
    this.#selectionLayer.hideHoverElementSelection();
  }

  /**
   * Update the box coordinates from the hover element rect
   */
  updateSelectionBoxFromRect(scrollX, scrollY) {
    this.#selectionLayer.updateSelectionBoxFromRect(scrollX, scrollY);
  }

  /**
   * Handles when the user moves the mouse over an element
   * @param clientX The x coordinate in the visible window
   * @param clientY The y coordinate in the visible window
   * @param targetId The target element id
   */
  handleElementHover(clientX, clientY, targetId) {
    this.#selectionLayer.handleElementHover(clientX, clientY, targetId);
  }

  /**
   * Handles moving the rect when the user has scrolled but not moved the mouse
   * It uses the last x and y coordinates to find the new element at the mouse position
   */
  handleElementScroll() {
    this.#selectionLayer.handleElementScroll();
  }

  /**
   * Draw the eyes in the PreviewLayer
   * @param clientX The x mouse position
   * @param clientY The y mouse position
   */
  drawPreviewEyes(clientX, clientY) {
    this.#previewLayer.drawEyes(
      clientX,
      clientY,
      this.#selectionLayer.clientWidth,
      this.#selectionLayer.clientHeight
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
   *    clientWidth: the viewport width
   *    clientHeight: the viewport height
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
   * Returns the window's dimensions for the `window` given.
   *
   * @return {Object} An object containing window dimensions
   *   {
   *     clientWidth: The width of the viewport
   *     clientHeight: The height of the viewport
   *     width: The width of the enitre page
   *     height: The height of the entire page
   *     scrollX: The X scroll offset of the viewport
   *     scrollY: The Y scroll offest of the viewport
   *   }
   */
  getDimensionsFromWindow(window) {
    let {
      innerHeight,
      innerWidth,
      scrollMaxY,
      scrollMaxX,
      scrollMinY,
      scrollMinX,
      scrollY,
      scrollX,
    } = window;

    let width = innerWidth + scrollMaxX - scrollMinX;
    let height = innerHeight + scrollMaxY - scrollMinY;
    let clientHeight = innerHeight;
    let clientWidth = innerWidth;

    const scrollbarHeight = {};
    const scrollbarWidth = {};
    window.windowUtils.getScrollbarSize(false, scrollbarWidth, scrollbarHeight);
    width -= scrollbarWidth.value;
    height -= scrollbarHeight.value;
    clientWidth -= scrollbarWidth.value;
    clientHeight -= scrollbarHeight.value;

    return { clientWidth, clientHeight, width, height, scrollX, scrollY };
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
    let {
      clientWidth,
      clientHeight,
      width,
      height,
      scrollX,
      scrollY,
    } = this.getDimensionsFromWindow(win);

    let shouldDraw = true;

    if (
      clientHeight < this.#selectionLayer.clientHeight ||
      clientWidth < this.#selectionLayer.clientWidth
    ) {
      let widthDiff = this.#selectionLayer.clientWidth - clientWidth;
      let heightDiff = this.#selectionLayer.clientHeight - clientHeight;

      this.#width -= widthDiff;
      this.#height -= heightDiff;

      this.drawScreenshotsContainer();
      // We just updated the screenshots container so we check if the window
      // dimensions are still accurate
      let {
        width: updatedWidth,
        height: updatedHeight,
      } = this.getDimensionsFromWindow(win);

      // If the width and height are the same then we don't need to draw the overlay again
      if (updatedWidth === width && updatedHeight === height) {
        shouldDraw = false;
      }

      width = updatedWidth;
      height = updatedHeight;
    }

    this.#selectionLayer.clientWidth = clientWidth;
    this.#selectionLayer.clientHeight = clientHeight;
    this.#selectionLayer.scrollX = scrollX;
    this.#selectionLayer.scrollY = scrollY;

    this.#selectionLayer.scrollWidth = width;
    this.#selectionLayer.scrollHeight = height;

    this.#width = width;
    this.#height = height;

    if (shouldDraw) {
      this.drawScreenshotsContainer();
    }
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

  get hoverElementBoxRect() {
    return this.#selectionLayer.hoverElementBoxRect;
  }
}
