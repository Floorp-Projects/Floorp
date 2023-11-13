/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * The Screenshots overlay is inserted into the document's
 * anonymous content container (see dom/webidl/Document.webidl).
 *
 * This container gets cleared automatically when the document navigates.
 *
 * To retrieve the AnonymousContent instance, use the `content` getter.
 */

/*
 * Below are the states of the screenshots overlay
 * States:
 *  "crosshairs":
 *    Nothing has happened, and the crosshairs will follow the movement of the mouse
 *  "draggingReady":
 *    The user has pressed the mouse button, but hasn't moved enough to create a selection
 *  "dragging":
 *    The user has pressed down a mouse button, and is dragging out an area far enough to show a selection
 *  "selected":
 *    The user has selected an area
 *  "resizing":
 *    The user is resizing the selection
 */

import {
  setMaxDetectHeight,
  setMaxDetectWidth,
  getBestRectForElement,
  Region,
  WindowDimensions,
} from "chrome://browser/content/screenshots/overlayHelpers.mjs";

const lazy = {};

ChromeUtils.defineLazyGetter(lazy, "overlayLocalization", () => {
  return new Localization(["browser/screenshotsOverlay.ftl"], true);
});

const REGION_CHANGE_THRESHOLD = 5;
const SCROLL_BY_EDGE = 20;

export class ScreenshotsOverlay {
  #content;
  #initialized = false;
  #state = "";
  #moverId;
  #cachedEle;
  #lastPageX;
  #lastPageY;
  #lastClientX;
  #lastClientY;
  #previousDimensions;
  #methodsUsed;

  get markup() {
    let [cancel, instructions, download, copy] =
      lazy.overlayLocalization.formatMessagesSync([
        { id: "screenshots-overlay-cancel-button" },
        { id: "screenshots-overlay-instructions" },
        { id: "screenshots-overlay-download-button" },
        { id: "screenshots-overlay-copy-button" },
      ]);

    return `
      <template>
        <link rel="stylesheet" href="chrome://browser/content/screenshots/overlay/overlay.css" />
        <div id="screenshots-component">
          <div id="preview-container" hidden>
            <div class="face-container">
              <div class="eye left"><div id="left-eye" class="eyeball"></div></div>
              <div class="eye right"><div id="right-eye" class="eyeball"></div></div>
              <div class="face"></div>
            </div>
            <div class="preview-instructions">${instructions.value}</div>
            <button class="screenshots-button" id="screenshots-cancel-button">${cancel.value}</button>
          </div>
          <div id="hover-highlight" hidden></div>
          <div id="selection-container" hidden>
            <div id="top-background" class="bghighlight"></div>
            <div id="bottom-background" class="bghighlight"></div>
            <div id="left-background" class="bghighlight"></div>
            <div id="right-background" class="bghighlight"></div>
            <div id="highlight" class="highlight">
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
              <div id="selection-size-container">
                <span id="selection-size"></span>
              </div>
            </div>
          </div>
          <div id="buttons-container" hidden>
            <div class="buttons-wrapper">
              <button id="cancel" class="screenshots-button" title="${cancel.value}" aria-label="${cancel.value}"><img/></button>
              <button id="copy" class="screenshots-button" title="${copy.value}" aria-label="${copy.value}"><img/>${copy.value}</button>
              <button id="download" class="screenshots-button primary" title="${download.value}" aria-label="${download.value}"><img/>${download.value}</button>
            </div>
          </div>
        </div>
      </template>`;
  }

  get fragment() {
    if (!this.overlayTemplate) {
      let parser = new DOMParser();
      let doc = parser.parseFromString(this.markup, "text/html");
      this.overlayTemplate = this.document.importNode(
        doc.querySelector("template"),
        true
      );
    }
    let fragment = this.overlayTemplate.content.cloneNode(true);
    return fragment;
  }

  get initialized() {
    return this.#initialized;
  }

  get state() {
    return this.#state;
  }

  get methodsUsed() {
    return this.#methodsUsed;
  }

  constructor(contentDocument) {
    this.document = contentDocument;
    this.window = contentDocument.ownerGlobal;

    this.windowDimensions = new WindowDimensions();
    this.selectionRegion = new Region(this.windowDimensions);
    this.hoverElementRegion = new Region(this.windowDimensions);
    this.resetMethodsUsed();
  }

  get content() {
    if (!this.#content || Cu.isDeadWrapper(this.#content)) {
      return null;
    }
    return this.#content;
  }

  getElementById(id) {
    return this.content.root.getElementById(id);
  }

  async initialize() {
    if (this.initialized) {
      return;
    }

    this.windowDimensions.reset();

    this.#content = this.document.insertAnonymousContent();
    this.#content.root.appendChild(this.fragment);

    this.initializeElements();
    this.updateWindowDimensions();

    this.#setState("crosshairs");

    this.#initialized = true;
  }

  /**
   * Get all the elements that will be used.
   */
  initializeElements() {
    this.previewCancelButton = this.getElementById("screenshots-cancel-button");
    this.cancelButton = this.getElementById("cancel");
    this.copyButton = this.getElementById("copy");
    this.downloadButton = this.getElementById("download");

    this.previewContainer = this.getElementById("preview-container");
    this.hoverElementContainer = this.getElementById("hover-highlight");
    this.selectionContainer = this.getElementById("selection-container");
    this.buttonsContainer = this.getElementById("buttons-container");
    this.screenshotsContainer = this.getElementById("screenshots-component");

    this.leftEye = this.getElementById("left-eye");
    this.rightEye = this.getElementById("right-eye");

    this.leftBackgroundEl = this.getElementById("left-background");
    this.topBackgroundEl = this.getElementById("top-background");
    this.rightBackgroundEl = this.getElementById("right-background");
    this.bottomBackgroundEl = this.getElementById("bottom-background");
    this.highlightEl = this.getElementById("highlight");

    this.selectionSize = this.getElementById("selection-size");

    this.addEventListeners();
  }

  /**
   * Removes all event listeners and removes the overlay from the Anonymous Content
   */
  tearDown(options = {}) {
    if (this.#content) {
      this.removeEventListeners();
      if (!(options.doNotResetMethods === true)) {
        this.resetMethodsUsed();
      }
      try {
        this.document.removeAnonymousContent(this.#content);
      } catch (e) {
        // If the current window isn't the one the content was inserted into, this
        // will fail, but that's fine.
      }
    }
    this.#initialized = false;
    this.#setState("");
  }

  /**
   * Add required event listeners to the overlay
   */
  addEventListeners() {
    this.previewCancelButton.addEventListener("click", this);
    this.previewCancelButton.addEventListener("pointerdown", this);

    this.cancelButton.addEventListener("click", this);
    this.cancelButton.addEventListener("pointerdown", this);

    this.copyButton.addEventListener("click", this);
    this.copyButton.addEventListener("pointerdown", this);

    this.downloadButton.addEventListener("click", this);
    this.downloadButton.addEventListener("pointerdown", this);

    this.screenshotsContainer.addEventListener("pointerdown", this);
    this.screenshotsContainer.addEventListener("pointermove", this);
    this.screenshotsContainer.addEventListener("pointerup", this);

    let moverIds = [
      "mover-left",
      "mover-top",
      "mover-right",
      "mover-bottom",
      "mover-topLeft",
      "mover-topRight",
      "mover-bottomLeft",
      "mover-bottomRight",
      "highlight",
    ];

    for (let id of moverIds) {
      let mover = this.getElementById(id);
      mover.addEventListener("pointerdown", this);
      mover.addEventListener("pointermove", this);
      mover.addEventListener("pointerup", this);
    }
  }

  /**
   * Remove the events listeners from the overlay
   */
  removeEventListeners() {
    this.previewCancelButton.removeEventListener("click", this);
    this.cancelButton.removeEventListener("click", this);
    this.copyButton.removeEventListener("click", this);
    this.downloadButton.removeEventListener("click", this);

    this.screenshotsContainer.removeEventListener("pointerdown", this);
    this.screenshotsContainer.removeEventListener("pointermove", this);
    this.screenshotsContainer.removeEventListener("pointerup", this);
  }

  resetMethodsUsed() {
    this.#methodsUsed = {
      element: 0,
      region: 0,
      move: 0,
      resize: 0,
    };
  }

  /**
   * Returns the x and y coordinates of the event relative to both the
   * viewport and the page.
   * @param {Event} event The event
   * @returns
   *  {
   *    clientX: The x position relative to the viewport
   *    clientY: The y position relative to the viewport
   *    pageX: The x position relative to the entire page
   *    pageY: The y position relative to the entire page
   *  }
   */
  getCoordinatesFromEvent(event) {
    const { clientX, clientY, pageX, pageY } = event;

    return { clientX, clientY, pageX, pageY };
  }

  handleEvent(event) {
    switch (event.type) {
      case "click":
        this.handleClick(event);
        break;
      case "pointerdown":
        this.handlePointerDown(event);
        break;
      case "pointermove":
        this.handlePointerMove(event);
        break;
      case "pointerup":
        this.handlePointerUp(event);
        break;
    }
  }

  handleClick(event) {
    switch (event.target.id) {
      case "screenshots-cancel-button":
      case "cancel":
        this.#dispatchEvent("Screenshots:Close", { reason: "overlay_cancel" });
        break;
      case "copy":
        this.#dispatchEvent("Screenshots:Copy", {
          region: this.selectionRegion.dimensions,
        });
        break;
      case "download":
        this.#dispatchEvent("Screenshots:Download", {
          region: this.selectionRegion.dimensions,
        });
        break;
    }
  }

  /**
   * Handles the pointerdown event depending on the state.
   * Early return when a pointer down happens on a button.
   * @param {Event} event The pointerown event
   */
  handlePointerDown(event) {
    if (
      event.target.id === "screenshots-cancel-button" ||
      event.target.closest("#buttons-container") === this.buttonsContainer
    ) {
      event.stopPropagation();
      return;
    }

    const { pageX, pageY } = this.getCoordinatesFromEvent(event);

    switch (this.state) {
      case "crosshairs": {
        this.crosshairsDragStart(pageX, pageY);
        break;
      }
      case "selected": {
        this.selectedDragStart(pageX, pageY, event.target.id);
        break;
      }
    }
  }

  /**
   * Handles the pointermove event depending on the state
   * @param {Event} event The pointermove event
   */
  handlePointerMove(event) {
    const { pageX, pageY, clientX, clientY } =
      this.getCoordinatesFromEvent(event);

    switch (this.#state) {
      case "crosshairs": {
        this.crosshairsMove(clientX, clientY);
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
   * Handles the pointerup event depending on the state
   * @param {Event} event The pointerup event
   */
  handlePointerUp(event) {
    const { pageX, pageY, clientX, clientY } =
      this.getCoordinatesFromEvent(event);

    switch (this.#state) {
      case "draggingReady": {
        this.draggingReadyDragEnd(pageX - clientX, pageY - clientY);
        break;
      }
      case "dragging": {
        this.draggingDragEnd(pageX, pageY, event.target.id);
        break;
      }
      case "resizing": {
        this.resizingDragEnd(pageX, pageY, event.target.id);
        break;
      }
    }
  }

  /**
   * Dispatch a custom event to the ScreenshotsComponentChild actor
   * @param {String} eventType The name of the event
   * @param {object} detail Extra details to send to the child actor
   */
  #dispatchEvent(eventType, detail) {
    this.window.dispatchEvent(
      new CustomEvent(eventType, {
        bubbles: true,
        detail,
      })
    );
  }

  /**
   * Set a new state for the overlay
   * @param {String} newState
   */
  #setState(newState) {
    if (this.#state === "selected" && newState === "crosshairs") {
      this.#dispatchEvent("Screenshots:RecordEvent", {
        eventName: "started",
        reason: "overlay_retry",
      });
    }
    if (newState !== this.#state) {
      this.#dispatchEvent("Screenshots:OverlaySelection", {
        hasSelection: newState == "selected",
      });
    }
    this.#state = newState;

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
   * Hide hover element, selection and buttons containers.
   * Show the preview container and the panel.
   * This is the initial state of the overlay.
   */
  crosshairsStart() {
    this.hideHoverElementContainer();
    this.hideSelectionContainer();
    this.hideButtonsContainer();
    this.showPreviewContainer();
    this.#dispatchEvent("Screenshots:ShowPanel");
    this.#previousDimensions = null;
  }

  /**
   * Hide the panel because we have started dragging.
   */
  draggingReadyStart() {
    this.#dispatchEvent("Screenshots:HidePanel");
  }

  /**
   * Hide the preview, hover element and buttons containers.
   * Show the selection container.
   */
  draggingStart() {
    this.hidePreviewContainer();
    this.hideButtonsContainer();
    this.hideHoverElementContainer();
    this.drawSelectionContainer();
  }

  /**
   * Hide the preview and hover element containers.
   * Draw the selection and buttons containers.
   */
  selectedStart() {
    this.hidePreviewContainer();
    this.hideHoverElementContainer();
    this.drawSelectionContainer();
    this.drawButtonsContainer();
  }

  /**
   * Hide the buttons container.
   * Store the width and height of the current selected region.
   * The dimensions will be used when moving the region along the edge of the
   * page and for recording telemetry.
   */
  resizingStart() {
    this.hideButtonsContainer();
    let { width, height } = this.selectionRegion.dimensions;
    this.#previousDimensions = { width, height };
  }

  /**
   * Dragging has started so we set the initial selection region and set the
   * state to draggingReady.
   * @param {Number} pageX The x position relative to the page
   * @param {Number} pageY The y position relative to the page
   */
  crosshairsDragStart(pageX, pageY) {
    this.selectionRegion.dimensions = {
      left: pageX,
      top: pageY,
      right: pageX,
      bottom: pageY,
    };

    this.#setState("draggingReady");
  }

  /**
   * If the background is clicked we set the state to crosshairs
   * otherwise set the state to resizing
   * @param {Number} pageX The x position relative to the page
   * @param {Number} pageY The y position relative to the page
   * @param {String} targetId The id of the event target
   */
  selectedDragStart(pageX, pageY, targetId) {
    if (targetId === this.screenshotsContainer.id) {
      this.#setState("crosshairs");
      return;
    }
    this.#moverId = targetId;
    this.#lastPageX = pageX;
    this.#lastPageY = pageY;

    this.#setState("resizing");
  }

  /**
   * Draw the eyes in the preview container and find the element currently
   * being hovered.
   * @param {Number} clientX The x position relative to the viewport
   * @param {Number} clientY The y position relative to the viewport
   */
  crosshairsMove(clientX, clientY) {
    this.drawPreviewEyes(clientX, clientY);

    this.handleElementHover(clientX, clientY);
  }

  /**
   * Set the selection region dimensions and if the region is at least 40
   * pixels diagnally in distance, set the state to dragging.
   * @param {Number} pageX The x position relative to the page
   * @param {Number} pageY The y position relative to the page
   */
  draggingReadyDrag(pageX, pageY) {
    this.selectionRegion.dimensions = {
      right: pageX,
      bottom: pageY,
    };

    if (this.selectionRegion.distance > 40) {
      this.#setState("dragging");
    }
  }

  /**
   * Scroll if along the edge of the viewport, update the selection region
   * dimensions and draw the selection container.
   * @param {Number} pageX The x position relative to the page
   * @param {Number} pageY The y position relative to the page
   */
  draggingDrag(pageX, pageY) {
    this.scrollIfByEdge(pageX, pageY);
    this.selectionRegion.dimensions = {
      right: pageX,
      bottom: pageY,
    };

    this.drawSelectionContainer();
  }

  /**
   * Resize the selection region depending on the mover that started the resize.
   * @param {Number} pageX The x position relative to the page
   * @param {Number} pageY The y position relative to the page
   */
  resizingDrag(pageX, pageY) {
    this.scrollIfByEdge(pageX, pageY);
    switch (this.#moverId) {
      case "mover-topLeft": {
        this.selectionRegion.dimensions = {
          left: pageX,
          top: pageY,
        };
        break;
      }
      case "mover-top": {
        this.selectionRegion.dimensions = { top: pageY };
        break;
      }
      case "mover-topRight": {
        this.selectionRegion.dimensions = {
          top: pageY,
          right: pageX,
        };
        break;
      }
      case "mover-right": {
        this.selectionRegion.dimensions = {
          right: pageX,
        };
        break;
      }
      case "mover-bottomRight": {
        this.selectionRegion.dimensions = {
          right: pageX,
          bottom: pageY,
        };
        break;
      }
      case "mover-bottom": {
        this.selectionRegion.dimensions = {
          bottom: pageY,
        };
        break;
      }
      case "mover-bottomLeft": {
        this.selectionRegion.dimensions = {
          left: pageX,
          bottom: pageY,
        };
        break;
      }
      case "mover-left": {
        this.selectionRegion.dimensions = { left: pageX };
        break;
      }
      case "highlight": {
        let diffX = this.#lastPageX - pageX;
        let diffY = this.#lastPageY - pageY;

        let newLeft;
        let newRight;
        let newTop;
        let newBottom;

        // Unpack dimensions to use here
        let {
          left: boxLeft,
          top: boxTop,
          right: boxRight,
          bottom: boxBottom,
          width: boxWidth,
          height: boxHeight,
        } = this.selectionRegion.dimensions;
        let { scrollWidth, scrollHeight } = this.windowDimensions.dimensions;

        // wait until all 4 if elses have completed before setting box dimensions
        if (boxWidth <= this.#previousDimensions.width && boxLeft === 0) {
          newLeft = boxRight - this.#previousDimensions.width;
        } else {
          newLeft = boxLeft;
        }

        if (
          boxWidth <= this.#previousDimensions.width &&
          boxRight === scrollWidth
        ) {
          newRight = boxLeft + this.#previousDimensions.width;
        } else {
          newRight = boxRight;
        }

        if (boxHeight <= this.#previousDimensions.height && boxTop === 0) {
          newTop = boxBottom - this.#previousDimensions.height;
        } else {
          newTop = boxTop;
        }

        if (
          boxHeight <= this.#previousDimensions.height &&
          boxBottom === scrollHeight
        ) {
          newBottom = boxTop + this.#previousDimensions.height;
        } else {
          newBottom = boxBottom;
        }

        this.selectionRegion.dimensions = {
          left: newLeft - diffX,
          top: newTop - diffY,
          right: newRight - diffX,
          bottom: newBottom - diffY,
        };

        this.#lastPageX = pageX;
        this.#lastPageY = pageY;
        break;
      }
    }
    this.drawSelectionContainer();
  }

  /**
   * If there is a valid element region, update and draw the selection
   * container and set the state to selected.
   * Otherwise set the state to crosshairs.
   */
  draggingReadyDragEnd() {
    if (this.hoverElementRegion.isRegionValid) {
      this.selectionRegion.dimensions = this.hoverElementRegion.dimensions;
      this.#setState("selected");
      this.#dispatchEvent("Screenshots:RecordEvent", {
        eventName: "selected",
        reason: "element",
      });
      this.#methodsUsed.element += 1;
    } else {
      this.#setState("crosshairs");
    }
  }

  /**
   * Update the selection region dimensions and set the state to selected.
   * @param {Number} pageX The x position relative to the page
   * @param {Number} pageY The y position relative to the page
   */
  draggingDragEnd(pageX, pageY) {
    this.selectionRegion.dimensions = {
      right: pageX,
      bottom: pageY,
    };
    this.selectionRegion.sortCoords();
    this.#setState("selected");
    this.maybeRecordRegionSelected();
    this.#methodsUsed.region += 1;
  }

  /**
   * Update the selection region dimensions by calling `resizingDrag` and set
   * the state to selected.
   * @param {Number} pageX The x position relative to the page
   * @param {Number} pageY The y position relative to the page
   */
  resizingDragEnd(pageX, pageY, targetId) {
    this.resizingDrag(pageX, pageY, targetId);
    this.selectionRegion.sortCoords();
    this.#setState("selected");
    this.maybeRecordRegionSelected();
    if (targetId === "highlight") {
      this.#methodsUsed.move += 1;
    } else {
      this.#methodsUsed.resize += 1;
    }
  }

  maybeRecordRegionSelected() {
    let { width, height } = this.selectionRegion.dimensions;

    if (
      !this.#previousDimensions ||
      (Math.abs(this.#previousDimensions.width - width) >
        REGION_CHANGE_THRESHOLD &&
        Math.abs(this.#previousDimensions.height - height) >
          REGION_CHANGE_THRESHOLD)
    ) {
      this.#dispatchEvent("Screenshots:RecordEvent", {
        eventName: "selected",
        reason: "region_selection",
      });
    }
    this.#previousDimensions = { width, height };
  }

  /**
   * Draw the preview eyes pointer towards the mouse.
   * @param {Number} clientX The x position relative to the viewport
   * @param {Number} clientY The y position relative to the viewport
   */
  drawPreviewEyes(clientX, clientY) {
    let { clientWidth, clientHeight } = this.windowDimensions.dimensions;
    const xpos = Math.floor((10 * (clientX - clientWidth / 2)) / clientWidth);
    const ypos = Math.floor((10 * (clientY - clientHeight / 2)) / clientHeight);
    const move = `transform:translate(${xpos}px, ${ypos}px);`;
    this.leftEye.style = move;
    this.rightEye.style = move;
  }

  showPreviewContainer() {
    this.previewContainer.hidden = false;
  }

  hidePreviewContainer() {
    this.previewContainer.hidden = true;
  }

  updatePreviewContainer() {
    let { clientWidth, clientHeight } = this.windowDimensions.dimensions;
    this.previewContainer.style.width = `${clientWidth}px`;
    this.previewContainer.style.height = `${clientHeight}px`;
  }

  /**
   * Update the screenshots overlay container based on the window dimensions.
   */
  updateScreenshotsOverlayContainer() {
    let { scrollWidth, scrollHeight } = this.windowDimensions.dimensions;
    this.screenshotsContainer.style = `width:${scrollWidth}px;height:${scrollHeight}px;`;
  }

  showScreenshotsOverlayContainer() {
    this.screenshotsContainer.hidden = false;
  }

  hideScreenshotsOverlayContainer() {
    this.screenshotsContainer.hidden = true;
  }

  /**
   * Draw the hover element container based on the hover element region.
   */
  drawHoverElementRegion() {
    this.showHoverElementContainer();

    let { top, left, width, height } = this.hoverElementRegion.dimensions;

    this.hoverElementContainer.style = `top:${top}px;left:${left}px;width:${width}px;height:${height}px;`;
  }

  showHoverElementContainer() {
    this.hoverElementContainer.hidden = false;
  }

  hideHoverElementContainer() {
    this.hoverElementContainer.hidden = true;
  }

  /**
   * Draw each background element and the highlight element base on the
   * selection region.
   */
  drawSelectionContainer() {
    this.showSelectionContainer();

    let { top, left, right, bottom, width, height } =
      this.selectionRegion.dimensions;

    this.highlightEl.style = `top:${top}px;left:${left}px;width:${width}px;height:${height}px;`;

    this.leftBackgroundEl.style = `top:${top}px;width:${left}px;height:${height}px;`;
    this.topBackgroundEl.style.height = `${top}px`;
    this.rightBackgroundEl.style = `top:${top}px;left:${right}px;width:calc(100% - ${right}px);height:${height}px;`;
    this.bottomBackgroundEl.style = `top:${bottom}px;height:calc(100% - ${bottom}px);`;

    this.updateSelectionSizeText();
  }

  updateSelectionSizeText() {
    let dpr = this.windowDimensions.devicePixelRatio;
    let { width, height } = this.selectionRegion.dimensions;

    let [selectionSizeTranslation] =
      lazy.overlayLocalization.formatMessagesSync([
        {
          id: "screenshots-overlay-selection-region-size",
          args: {
            width: Math.floor(width * dpr),
            height: Math.floor(height * dpr),
          },
        },
      ]);
    this.selectionSize.textContent = selectionSizeTranslation.value;
  }

  showSelectionContainer() {
    this.selectionContainer.hidden = false;
  }

  hideSelectionContainer() {
    this.selectionContainer.hidden = true;
  }

  /**
   * Draw the buttons container in the bottom right corner of the selection
   * container if possible.
   * The buttons will be visible in the viewport if the selection container
   * is within the viewport, otherwise skip drawing the buttons.
   */
  drawButtonsContainer() {
    this.showButtonsContainer();

    let {
      left: boxLeft,
      top: boxTop,
      right: boxRight,
      bottom: boxBottom,
    } = this.selectionRegion.dimensions;
    let { clientWidth, clientHeight, scrollX, scrollY } =
      this.windowDimensions.dimensions;

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
      this.buttonsContainer.style.left = `${boxLeft}px`;
      this.buttonsContainer.style.right = "";
    } else {
      this.buttonsContainer.style.right = `calc(100% - ${boxRight}px)`;
      this.buttonsContainer.style.left = "";
    }

    this.buttonsContainer.style.top = `${top}px`;
  }

  showButtonsContainer() {
    this.buttonsContainer.hidden = false;
  }

  hideButtonsContainer() {
    this.buttonsContainer.hidden = true;
  }

  /**
   * Set the pointer events to none on the screenshots elements so
   * elementFromPoint can find the real element at the given point.
   */
  setPointerEventsNone() {
    this.screenshotsContainer.style.pointerEvents = "none";
  }

  resetPointerEvents() {
    this.screenshotsContainer.style.pointerEvents = "";
  }

  /**
   * Try to find a reasonable element for a given point.
   * If a reasonable element is found, draw the hover element container for
   * that element region.
   * @param {Number} clientX The x position relative to the viewport
   * @param {Number} clientY The y position relative to the viewport
   */
  handleElementHover(clientX, clientY) {
    this.setPointerEventsNone();
    let ele = this.document.elementFromPoint(clientX, clientY);
    this.resetPointerEvents();

    if (this.#cachedEle && this.#cachedEle === ele) {
      // Still hovering over the same element
      return;
    }
    this.#cachedEle = ele;

    let rect = getBestRectForElement(ele, this.document);
    if (rect) {
      let { scrollX, scrollY } = this.windowDimensions.dimensions;
      let { left, top, right, bottom } = rect;
      let newRect = {
        left: left + scrollX,
        top: top + scrollY,
        right: right + scrollX,
        bottom: bottom + scrollY,
      };
      this.hoverElementRegion.dimensions = newRect;
      this.drawHoverElementRegion();
    } else {
      this.hoverElementRegion.resetDimensions();
      this.hideHoverElementContainer();
    }
  }

  /**
   * Scroll the viewport if near one or both of the edges.
   * @param {Number} pageX The x position relative to the page
   * @param {Number} pageY The y position relative to the page
   */
  scrollIfByEdge(pageX, pageY) {
    let { scrollX, scrollY, clientWidth, clientHeight } =
      this.windowDimensions.dimensions;

    if (pageY - scrollY <= SCROLL_BY_EDGE) {
      // Scroll up
      this.scrollWindow(0, -SCROLL_BY_EDGE);
    } else if (scrollY + clientHeight - pageY <= SCROLL_BY_EDGE) {
      // Scroll down
      this.scrollWindow(0, SCROLL_BY_EDGE);
    }

    if (pageX - scrollX <= SCROLL_BY_EDGE) {
      // Scroll left
      this.scrollWindow(-SCROLL_BY_EDGE, 0);
    } else if (scrollX + clientWidth - pageX <= SCROLL_BY_EDGE) {
      // Scroll right
      this.scrollWindow(SCROLL_BY_EDGE, 0);
    }
  }

  /**
   * Scroll the window by the given amount.
   * @param {Number} x The x amount to scroll
   * @param {Number} y The y amount to scroll
   */
  scrollWindow(x, y) {
    this.window.scrollBy(x, y);
    this.updateScreenshotsOverlayDimensions("scroll");
  }

  /**
   * The page was resized or scrolled. We need to update the screenshots
   * container size so we don't draw outside the page bounds.
   * @param {String} eventType will be "scroll" or "resize"
   */
  updateScreenshotsOverlayDimensions(eventType) {
    this.updateWindowDimensions();

    if (this.state === "crosshairs") {
      if (eventType === "resize") {
        this.hideHoverElementContainer();
        this.updatePreviewContainer();
      } else if (eventType === "scroll") {
        if (this.#lastClientX && this.#lastClientY) {
          this.#cachedEle = null;
          this.handleElementHover(this.#lastClientX, this.#lastClientY);
        }
      }
    } else if (this.state === "selected") {
      this.drawButtonsContainer();
      this.updateSelectionSizeText();
    }
  }

  /**
   * Returns the window's dimensions for the current window.
   *
   * @return {Object} An object containing window dimensions
   *   {
   *     clientWidth: The width of the viewport
   *     clientHeight: The height of the viewport
   *     scrollWidth: The width of the enitre page
   *     scrollHeight: The height of the entire page
   *     scrollX: The X scroll offset of the viewport
   *     scrollY: The Y scroll offest of the viewport
   *     scrollMinX: The X mininmun the viewport can scroll to
   *     scrollMinY: The Y mininmun the viewport can scroll to
   *   }
   */
  getDimensionsFromWindow() {
    let {
      innerHeight,
      innerWidth,
      scrollMaxY,
      scrollMaxX,
      scrollMinY,
      scrollMinX,
      scrollY,
      scrollX,
    } = this.window;

    let scrollWidth = innerWidth + scrollMaxX - scrollMinX;
    let scrollHeight = innerHeight + scrollMaxY - scrollMinY;
    let clientHeight = innerHeight;
    let clientWidth = innerWidth;

    const scrollbarHeight = {};
    const scrollbarWidth = {};
    this.window.windowUtils.getScrollbarSize(
      false,
      scrollbarWidth,
      scrollbarHeight
    );
    scrollWidth -= scrollbarWidth.value;
    scrollHeight -= scrollbarHeight.value;
    clientWidth -= scrollbarWidth.value;
    clientHeight -= scrollbarHeight.value;

    return {
      clientWidth,
      clientHeight,
      scrollWidth,
      scrollHeight,
      scrollX,
      scrollY,
      scrollMinX,
      scrollMinY,
    };
  }

  /**
   * Update the screenshots container because the window has changed size of
   * scrolled. The screenshots-overlay-container doesn't shrink with the page
   * when the window is resized so we have to manually find the width and
   * height of the page and make sure we aren't drawing outside the actual page
   * dimensions.
   */
  updateWindowDimensions() {
    let {
      clientWidth,
      clientHeight,
      scrollWidth,
      scrollHeight,
      scrollX,
      scrollY,
      scrollMinX,
      scrollMinY,
    } = this.getDimensionsFromWindow();

    let shouldUpdate = true;

    if (
      clientHeight < this.windowDimensions.clientHeight ||
      clientWidth < this.windowDimensions.clientWidth
    ) {
      let widthDiff = this.windowDimensions.clientWidth - clientWidth;
      let heightDiff = this.windowDimensions.clientHeight - clientHeight;

      this.windowDimensions.dimensions = {
        scrollWidth: scrollWidth - Math.max(widthDiff, 0),
        scrollHeight: scrollHeight - Math.max(heightDiff, 0),
        clientWidth,
        clientHeight,
      };

      if (this.state === "selected") {
        let didShift = this.selectionRegion.shift();
        if (didShift) {
          this.drawSelectionContainer();
          this.drawButtonsContainer();
        }
      } else if (this.state === "crosshairs") {
        this.updatePreviewContainer();
      }
      this.updateScreenshotsOverlayContainer();
      // We just updated the screenshots container so we check if the window
      // dimensions are still accurate
      let { scrollWidth: updatedWidth, scrollHeight: updatedHeight } =
        this.getDimensionsFromWindow();

      // If the width and height are the same then we don't need to draw the overlay again
      if (updatedWidth === scrollWidth && updatedHeight === scrollHeight) {
        shouldUpdate = false;
      }

      scrollWidth = updatedWidth;
      scrollHeight = updatedHeight;
    }

    setMaxDetectHeight(Math.max(clientHeight + 100, 700));
    setMaxDetectWidth(Math.max(clientWidth + 100, 1000));

    this.windowDimensions.dimensions = {
      clientWidth,
      clientHeight,
      scrollWidth,
      scrollHeight,
      scrollX,
      scrollY,
      scrollMinX,
      scrollMinY,
      devicePixelRatio: this.window.devicePixelRatio,
    };

    if (shouldUpdate) {
      this.updatePreviewContainer();
      this.updateScreenshotsOverlayContainer();
    }
  }
}
