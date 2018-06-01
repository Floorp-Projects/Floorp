/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { debounce } = require("devtools/shared/debounce");
const { lerp } = require("devtools/client/memory/utils");
const EventEmitter = require("devtools/shared/event-emitter");

const LERP_SPEED = 0.5;
const ZOOM_SPEED = 0.01;
const TRANSLATE_EPSILON = 1;
const ZOOM_EPSILON = 0.001;
const LINE_SCROLL_MODE = 1;
const SCROLL_LINE_SIZE = 15;

/**
 * DragZoom is a constructor that contains the state of the current dragging and
 * zooming behavior. It sets the scrolling and zooming behaviors.
 *
 * @param  {HTMLElement} container description
 *         The container for the canvases
 */
function DragZoom(container, debounceRate, requestAnimationFrame) {
  EventEmitter.decorate(this);

  this.isDragging = false;

  // The current mouse position
  this.mouseX = container.offsetWidth / 2;
  this.mouseY = container.offsetHeight / 2;

  // The total size of the visualization after being zoomed, in pixels
  this.zoomedWidth = container.offsetWidth;
  this.zoomedHeight = container.offsetHeight;

  // How much the visualization has been zoomed in
  this.zoom = 0;

  // The offset of visualization from the container. This is applied after
  // the zoom, and the visualization by default is centered
  this.translateX = 0;
  this.translateY = 0;

  // The size of the offset between the top/left of the container, and the
  // top/left of the containing element. This value takes into account
  // the device pixel ratio for canvas draws.
  this.offsetX = 0;
  this.offsetY = 0;

  // The smoothed values that are animated and eventually match the target
  // values. The values are updated by the update loop
  this.smoothZoom = 0;
  this.smoothTranslateX = 0;
  this.smoothTranslateY = 0;

  // Add the constant values for testing purposes
  this.ZOOM_SPEED = ZOOM_SPEED;
  this.ZOOM_EPSILON = ZOOM_EPSILON;

  const update = createUpdateLoop(container, this, requestAnimationFrame);

  this.destroy = setHandlers(this, container, update, debounceRate);
}

module.exports = DragZoom;

/**
 * Returns an update loop. This loop smoothly updates the visualization when
 * actions are performed. Once the animations have reached their target values
 * the animation loop is stopped.
 *
 * Any value in the `dragZoom` object that starts with "smooth" is the
 * smoothed version of a value that is interpolating toward the target value.
 * For instance `dragZoom.smoothZoom` approaches `dragZoom.zoom` on each
 * iteration of the update loop until it's sufficiently close as defined by
 * the epsilon values.
 *
 * Only these smoothed values and the container CSS are updated by the loop.
 *
 * @param {HTMLDivElement} container
 * @param {Object} dragZoom
 *        The values that represent the current dragZoom state
 * @param {Function} requestAnimationFrame
 */
function createUpdateLoop(container, dragZoom, requestAnimationFrame) {
  let isLooping = false;

  function update() {
    const isScrollChanging = (
      Math.abs(dragZoom.smoothZoom - dragZoom.zoom) > ZOOM_EPSILON
    );
    const isTranslateChanging = (
      Math.abs(dragZoom.smoothTranslateX - dragZoom.translateX)
      > TRANSLATE_EPSILON ||
      Math.abs(dragZoom.smoothTranslateY - dragZoom.translateY)
      > TRANSLATE_EPSILON
    );

    isLooping = isScrollChanging || isTranslateChanging;

    if (isScrollChanging) {
      dragZoom.smoothZoom = lerp(dragZoom.smoothZoom, dragZoom.zoom,
                                 LERP_SPEED);
    } else {
      dragZoom.smoothZoom = dragZoom.zoom;
    }

    if (isTranslateChanging) {
      dragZoom.smoothTranslateX = lerp(dragZoom.smoothTranslateX,
                                       dragZoom.translateX, LERP_SPEED);
      dragZoom.smoothTranslateY = lerp(dragZoom.smoothTranslateY,
                                       dragZoom.translateY, LERP_SPEED);
    } else {
      dragZoom.smoothTranslateX = dragZoom.translateX;
      dragZoom.smoothTranslateY = dragZoom.translateY;
    }

    const zoom = 1 + dragZoom.smoothZoom;
    const x = dragZoom.smoothTranslateX;
    const y = dragZoom.smoothTranslateY;
    container.style.transform = `translate(${x}px, ${y}px) scale(${zoom})`;

    if (isLooping) {
      requestAnimationFrame(update);
    }
  }

  // Go ahead and start the update loop
  update();

  return function restartLoopingIfStopped() {
    if (!isLooping) {
      update();
    }
  };
}

/**
 * Set the various event listeners and return a function to remove them
 *
 * @param  {Object} dragZoom
 * @param  {HTMLElement} container
 * @param  {Function} update
 * @return {Function}  The function to remove the handlers
 */
function setHandlers(dragZoom, container, update, debounceRate) {
  const emitChanged = debounce(() => dragZoom.emit("change"), debounceRate);

  const removeDragHandlers =
    setDragHandlers(container, dragZoom, emitChanged, update);
  const removeScrollHandlers =
    setScrollHandlers(container, dragZoom, emitChanged, update);

  return function removeHandlers() {
    removeDragHandlers();
    removeScrollHandlers();
  };
}

/**
 * Sets handlers for when the user drags on the canvas. It will update dragZoom
 * object with new translate and offset values.
 *
 * @param  {HTMLElement} container
 * @param  {Object} dragZoom
 * @param  {Function} changed
 * @param  {Function} update
 */
function setDragHandlers(container, dragZoom, emitChanged, update) {
  const parentEl = container.parentElement;

  function startDrag() {
    dragZoom.isDragging = true;
    container.style.cursor = "grabbing";
  }

  function stopDrag() {
    dragZoom.isDragging = false;
    container.style.cursor = "grab";
  }

  function drag(event) {
    const prevMouseX = dragZoom.mouseX;
    const prevMouseY = dragZoom.mouseY;

    dragZoom.mouseX = event.clientX - parentEl.offsetLeft;
    dragZoom.mouseY = event.clientY - parentEl.offsetTop;

    if (!dragZoom.isDragging) {
      return;
    }

    dragZoom.translateX += dragZoom.mouseX - prevMouseX;
    dragZoom.translateY += dragZoom.mouseY - prevMouseY;

    keepInView(container, dragZoom);

    emitChanged();
    update();
  }

  parentEl.addEventListener("mousedown", startDrag);
  parentEl.addEventListener("mouseup", stopDrag);
  parentEl.addEventListener("mouseout", stopDrag);
  parentEl.addEventListener("mousemove", drag);

  return function removeListeners() {
    parentEl.removeEventListener("mousedown", startDrag);
    parentEl.removeEventListener("mouseup", stopDrag);
    parentEl.removeEventListener("mouseout", stopDrag);
    parentEl.removeEventListener("mousemove", drag);
  };
}

/**
 * Sets the handlers for when the user scrolls. It updates the dragZoom object
 * and keeps the canvases all within the view. After changing values update
 * loop is called, and the changed event is emitted.
 *
 * @param  {HTMLDivElement} container
 * @param  {Object} dragZoom
 * @param  {Function} changed
 * @param  {Function} update
 */
function setScrollHandlers(container, dragZoom, emitChanged, update) {
  const window = container.ownerDocument.defaultView;

  function handleWheel(event) {
    event.preventDefault();

    if (dragZoom.isDragging) {
      return;
    }

    // Update the zoom level
    const scrollDelta = getScrollDelta(event, window);
    const prevZoom = dragZoom.zoom;
    dragZoom.zoom = Math.max(0, dragZoom.zoom - scrollDelta * ZOOM_SPEED);

    // Calculate the updated width and height
    const prevZoomedWidth = container.offsetWidth * (1 + prevZoom);
    const prevZoomedHeight = container.offsetHeight * (1 + prevZoom);
    dragZoom.zoomedWidth = container.offsetWidth * (1 + dragZoom.zoom);
    dragZoom.zoomedHeight = container.offsetHeight * (1 + dragZoom.zoom);
    const deltaWidth = dragZoom.zoomedWidth - prevZoomedWidth;
    const deltaHeight = dragZoom.zoomedHeight - prevZoomedHeight;

    const mouseOffsetX = dragZoom.mouseX - container.offsetWidth / 2;
    const mouseOffsetY = dragZoom.mouseY - container.offsetHeight / 2;

    // The ratio of where the center of the mouse is in regards to the total
    // zoomed width/height
    const ratioZoomX = (prevZoomedWidth / 2 + mouseOffsetX - dragZoom.translateX)
      / prevZoomedWidth;
    const ratioZoomY = (prevZoomedHeight / 2 + mouseOffsetY - dragZoom.translateY)
      / prevZoomedHeight;

    // Distribute the change in width and height based on the above ratio
    dragZoom.translateX -= lerp(-deltaWidth / 2, deltaWidth / 2, ratioZoomX);
    dragZoom.translateY -= lerp(-deltaHeight / 2, deltaHeight / 2, ratioZoomY);

    // Keep the canvas in range of the container
    keepInView(container, dragZoom);
    emitChanged();
    update();
  }

  container.addEventListener("wheel", handleWheel);

  return function removeListener() {
    container.removeEventListener("wheel", handleWheel);
  };
}

/**
 * Account for the various mouse wheel event types, per pixel or per line
 *
 * @param  {WheelEvent} event
 * @param  {Window} window
 * @return {Number} The scroll size in pixels
 */
function getScrollDelta(event, window) {
  if (event.deltaMode === LINE_SCROLL_MODE) {
    // Update by a fixed arbitrary value to normalize scroll types
    return event.deltaY * SCROLL_LINE_SIZE;
  }
  return event.deltaY;
}

/**
 * Keep the dragging and zooming within the view by updating the values in the
 * `dragZoom` object.
 *
 * @param  {HTMLDivElement} container
 * @param  {Object} dragZoom
 */
function keepInView(container, dragZoom) {
  const { devicePixelRatio } = container.ownerDocument.defaultView;
  const overdrawX = (dragZoom.zoomedWidth - container.offsetWidth) / 2;
  const overdrawY = (dragZoom.zoomedHeight - container.offsetHeight) / 2;

  dragZoom.translateX = Math.max(-overdrawX,
                                 Math.min(overdrawX, dragZoom.translateX));
  dragZoom.translateY = Math.max(-overdrawY,
                                 Math.min(overdrawY, dragZoom.translateY));

  dragZoom.offsetX = devicePixelRatio * (
    (dragZoom.zoomedWidth - container.offsetWidth) / 2 - dragZoom.translateX
  );
  dragZoom.offsetY = devicePixelRatio * (
    (dragZoom.zoomedHeight - container.offsetHeight) / 2 - dragZoom.translateY
  );
}
