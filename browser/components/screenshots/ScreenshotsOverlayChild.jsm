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
    }

    // Inject markup for the overlay UI
    this.overlayFragment = this.overlayFragment
      ? this.overlayFragment
      : this.buildOverlay();

    this._content = document.insertAnonymousContent(
      this.overlayFragment.children[0]
    );

    let height = this.contentDocument.documentElement.clientHeight;
    let width = this.contentDocument.documentElement.clientWidth;
    this.Box = new Box(this._content, width, height);

    this.addEventListeners();

    this.stateHandler = new StateHandler(this.Box, this);
    this.stateHandler.setState("crosshairs");

    this._initialized = true;
  }

  /**
   * Returns an object with the required information to take a screenshot
   * @returns object of the required coordinates to take a screenshot
   */
  getCoordinatesFromBox() {
    return {
      x1: this.Box.left + this.Box.xOffset,
      y1: this.Box.top + this.Box.yOffset,
      width: this.Box.width,
      height: this.Box.height,
    };
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
      this.Box.xOffset = event.pageX - event.clientX;
      this.Box.yOffset = event.pageY - event.clientY;
      this.screenshotsChild.requestCopyScreenshot(this.getCoordinatesFromBox());
    });
    this.addEventListenerForElement("download", "click", (event, targetId) => {
      this.Box.xOffset = event.pageX - event.clientX;
      this.Box.yOffset = event.pageY - event.clientY;
      this.screenshotsChild.requestDownloadScreenshot(
        this.getCoordinatesFromBox()
      );
    });

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
   * Shows the screenshots overlay crosshairs state
   */
  showOverylay() {
    this._content.setAttributeForElement(`${this.overlayId}`, "style", "");
  }

  /**
   * Hides the screenshots overlay crosshairs state
   */
  hideOverylay() {
    this._content.setAttributeForElement(
      `${this.overlayId}`,
      "style",
      `opacity:0;`
    );
  }

  /**
   * Draw the eyeballs facing the mouse
   * @param clientX x pointer position
   * @param clientY y pointer position
   * @param width width of the window
   * @param height height of the window
   */
  drawEyes(clientX, clientY, width, height) {
    const xpos = Math.floor((10 * (clientX - width / 2)) / width);
    const ypos = Math.floor((10 * (clientY - height / 2)) / height);
    const move = `transform:translate(${xpos}px, ${ypos}px);`;
    this._content.setAttributeForElement("left-eye", "style", move);
    this._content.setAttributeForElement("right-eye", "style", move);
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

  // Event handler that call the state handler to handle the events
  dragStart(event, targetId) {
    this.stateHandler.dragStart(event, targetId);
  }

  drag(event, targetId) {
    this.stateHandler.drag(event);
  }

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
  #box;
  #anonymousContentOverlay;
  #lastBox;
  #moverId;
  #lastX;
  #lastY;

  constructor(box, anonContent) {
    this.#state = "crosshairs";
    this.#box = box;
    this.#anonymousContentOverlay = anonContent;
    this.#lastBox = {};
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
    this.#box.windowHeight = event.explicitOriginalTarget.clientHeight;
    this.#box.windowWidth = event.explicitOriginalTarget.clientWidth;

    let clientX = event.clientX;
    let clientY = event.clientY;

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
        this.crosshairsMove(
          clientX,
          clientY,
          event.explicitOriginalTarget.clientWidth,
          event.explicitOriginalTarget.clientHeight
        );
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
    this.#box.hideAll();
    this.#anonymousContentOverlay.showOverylay();
  }

  /**
   * Hide the overlay and draw the box at the start of dragging state
   */
  draggingStart() {
    this.#anonymousContentOverlay.hideOverylay();
    this.#box.drawBox();
  }

  /**
   * Show the buttons at the start of the selected state
   */
  selectedStart() {
    this.#box.showButtons();
  }

  /**
   * Hide the buttons and store width and height of box at the start of the resizing state
   */
  resizingStart() {
    this.#box.hideButtons();
    this.#lastBox = {
      width: this.#box.width,
      height: this.#box.height,
    };
  }

  /**
   * Set the initial box coordinates and set the state to "draggingReady"
   * @param clientX x coordinate
   * @param clientY y coordinate
   */
  crosshairsDragStart(clientX, clientY) {
    this.#box.top = clientY;
    this.#box.left = clientX;
    this.#box.bottom = clientY;
    this.#box.right = clientX;

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
    if (targetId === this.#anonymousContentOverlay.overlayId) {
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
   * @param width width of the window
   * @param height height of the window
   */
  crosshairsMove(clientX, clientY, width, height) {
    this.#anonymousContentOverlay.drawEyes(clientX, clientY, width, height);
  }

  /**
   * Set the bottom and right coordinates of the box and draw the box
   * @param clientX x coordinate
   * @param clientY y coordinate
   */
  draggingDrag(clientX, clientY) {
    this.#box.bottom = clientY;
    this.#box.right = clientX;

    this.#box.drawBox();
  }

  /**
   * If the mouse has moved at least 40 pixels then set the state to "dragging"
   * @param clientX x coordinate
   * @param clientY y coordinate
   */
  draggingReadyDrag(clientX, clientY) {
    this.#box.bottom = clientY;
    this.#box.right = clientX;

    if (
      Math.sqrt(Math.pow(this.#box.width, 2) + Math.pow(this.#box.height, 2)) >
      40
    ) {
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
        this.#box.top = clientY;
        this.#box.left = clientX;
        break;
      }
      case "mover-top": {
        this.#box.top = clientY;
        break;
      }
      case "mover-topRight": {
        this.#box.top = clientY;
        this.#box.right = clientX;
        break;
      }
      case "mover-right": {
        this.#box.right = clientX;
        break;
      }
      case "mover-bottomRight": {
        this.#box.bottom = clientY;
        this.#box.right = clientX;
        break;
      }
      case "mover-bottom": {
        this.#box.bottom = clientY;
        break;
      }
      case "mover-bottomLeft": {
        this.#box.bottom = clientY;
        this.#box.left = clientX;
        break;
      }
      case "mover-left": {
        this.#box.left = clientX;
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

        // wait until all 4 if elses have completed before setting box dimensions

        if (this.#box.width <= lastBox.width && this.#box.left === 0) {
          newLeft = this.#box.right - lastBox.width;
        } else {
          newLeft = this.#box.left;
        }

        if (
          this.#box.width <= lastBox.width &&
          this.#box.right === this.#box.windowWidth
        ) {
          newRight = this.#box.left + lastBox.width;
        } else {
          newRight = this.#box.right;
        }

        if (this.#box.height <= lastBox.height && this.#box.top === 0) {
          newTop = this.#box.bottom - lastBox.height;
        } else {
          newTop = this.#box.top;
        }

        if (
          this.#box.height <= lastBox.height &&
          this.#box.bottom === this.#box.windowHeight
        ) {
          newBottom = this.#box.top + lastBox.height;
        } else {
          newBottom = this.#box.bottom;
        }

        this.#box.top = newTop - diffY;
        this.#box.bottom = newBottom - diffY;
        this.#box.left = newLeft - diffX;
        this.#box.right = newRight - diffX;

        this.#lastX = clientX;
        this.#lastY = clientY;
        break;
      }
    }
    this.#box.drawBox();
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
    this.#box.bottom = clientY;
    this.#box.right = clientX;
    this.#box.sortCoords();
    this.setState("selected");
  }

  /**
   * Draw the box one last time and set the state to "selected"
   * @param clientX x coordinate
   * @param clientY y coordinate
   */
  resizingDragEnd(clientX, clientY, targetId) {
    this.resizingDrag(clientX, clientY, targetId);
    this.#box.sortCoords();
    this.setState("selected");
  }
}

/**
 * The Box class handles drawing the highlight and background
 * Also handles drawing the buttons
 */
class Box {
  #content;
  #x1;
  #x2;
  #y1;
  #y2;
  #windowWidth;
  #windowHeight;
  #xOffset;
  #yOffset;

  constructor(content, width, height) {
    this.#content = content;
    this.#windowWidth = width;
    this.#windowHeight = height;

    this.#x1 = 0;
    this.#x2 = 0;
    this.#y1 = 0;
    this.#y2 = 0;
    this.#xOffset = 0;
    this.#yOffset = 0;
  }

  /**
   * Draw the highlight and background
   */
  drawBox() {
    this.#content.setAttributeForElement(
      "highlight",
      "style",
      `top:${this.top}px;left:${this.left}px;height:${this.height}px;width:${this.width}px;`
    );

    this.#content.setAttributeForElement(
      "bgTop",
      "style",
      `top:0px;height:${this.top}px;left:0px;width:100%;`
    );

    this.#content.setAttributeForElement(
      "bgBottom",
      "style",
      `top:${this.bottom}px;bottom:0px;left:0px;width:100%;`
    );

    this.#content.setAttributeForElement(
      "bgLeft",
      "style",
      `top:${this.top}px;height:${this.height}px;left:0px;width:${this.left}px;`
    );

    this.#content.setAttributeForElement(
      "bgRight",
      "style",
      `top:${this.top}px;height:${this.height}px;left:${this.right}px;width:calc(100% - ${this.right}px);`
    );
  }

  /**
   * Draw the buttons. Check if the box is too close the bottom or left and
   * adjust the buttons accordingly
   */
  showButtons() {
    let top = this.bottom;
    let leftOrRight = `right:calc(100% - ${this.right}px);`;

    if (this.#windowHeight - this.bottom < 70) {
      top = this.bottom - 60;
    }
    if (this.right < 265) {
      leftOrRight = `left:${this.left}px;`;
    }

    this.#content.setAttributeForElement(
      "buttons",
      "style",
      `top:${top}px;${leftOrRight}`
    );
  }

  /**
   * Hide the buttons
   */
  hideButtons() {
    this.#content.setAttributeForElement("buttons", "style", "display:none;");
  }

  /**
   * Hide the highlight, background, and buttons
   */
  hideAll() {
    this.hideButtons();
    this.#content.setAttributeForElement("highlight", "style", "display:none;");
    this.#content.setAttributeForElement("bgTop", "style", "display:none;");
    this.#content.setAttributeForElement("bgBottom", "style", "display:none;");
    this.#content.setAttributeForElement("bgLeft", "style", "display:none;");
    this.#content.setAttributeForElement("bgRight", "style", "display:none;");
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

  get windowWidth() {
    return this.#windowWidth;
  }
  set windowWidth(val) {
    this.#windowWidth = val;
  }

  get windowHeight() {
    return this.#windowHeight;
  }
  set windowHeight(val) {
    this.#windowHeight = val;
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
    this.#x2 = val > this.#windowWidth ? this.#windowWidth : val;
  }

  get bottom() {
    return Math.max(this.#y1, this.#y2);
  }
  set bottom(val) {
    this.#y2 = val > this.#windowHeight ? this.#windowHeight : val;
  }

  get width() {
    return Math.abs(this.#x2 - this.#x1);
  }
  get height() {
    return Math.abs(this.#y2 - this.#y1);
  }
}
