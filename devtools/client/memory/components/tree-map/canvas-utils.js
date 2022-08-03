/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */

"use strict";

/**
 * Create 2 canvases and contexts for drawing onto, 1 main canvas, and 1 zoom
 * canvas. The main canvas dimensions match the parent div, but the CSS can be
 * transformed to be zoomed and dragged around (potentially creating a blurry
 * canvas once zoomed in). The zoom canvas is a zoomed in section that matches
 * the parent div's dimensions and is kept in place through CSS. A zoomed in
 * view of the visualization is drawn onto this canvas, providing a crisp zoomed
 * in view of the tree map.
 */
const { debounce } = require("devtools/shared/debounce");
const EventEmitter = require("devtools/shared/event-emitter");

const HTML_NS = "http://www.w3.org/1999/xhtml";
const FULLSCREEN_STYLE = {
  width: "100%",
  height: "100%",
  position: "absolute",
};

/**
 * Create the canvases, resize handlers, and return references to them all
 *
 * @param  {HTMLDivElement} parentEl
 * @param  {Number} debounceRate
 * @return {Object}
 */
function Canvases(parentEl, debounceRate) {
  EventEmitter.decorate(this);
  this.container = createContainingDiv(parentEl);

  // This canvas contains all of the treemap
  this.main = createCanvas(this.container, "main");
  // This canvas contains only the zoomed in portion, overlaying the main canvas
  this.zoom = createCanvas(this.container, "zoom");

  this.removeHandlers = handleResizes(this, debounceRate);
}

Canvases.prototype = {
  /**
   * Remove the handlers and elements
   *
   * @return {type}  description
   */
  destroy() {
    this.removeHandlers();
    this.container.removeChild(this.main.canvas);
    this.container.removeChild(this.zoom.canvas);
  },
};

module.exports = Canvases;

/**
 * Create the containing div
 *
 * @param  {HTMLDivElement} parentEl
 * @return {HTMLDivElement}
 */
function createContainingDiv(parentEl) {
  const div = parentEl.ownerDocument.createElementNS(HTML_NS, "div");
  Object.assign(div.style, FULLSCREEN_STYLE);
  parentEl.appendChild(div);
  return div;
}

/**
 * Create a canvas and context
 *
 * @param  {HTMLDivElement} container
 * @param  {String} className
 * @return {Object} { canvas, ctx }
 */
function createCanvas(container, className) {
  const window = container.ownerDocument.defaultView;
  const canvas = container.ownerDocument.createElementNS(HTML_NS, "canvas");
  container.appendChild(canvas);
  canvas.width = container.offsetWidth * window.devicePixelRatio;
  canvas.height = container.offsetHeight * window.devicePixelRatio;
  canvas.className = className;

  Object.assign(canvas.style, FULLSCREEN_STYLE, {
    pointerEvents: "none",
  });

  const ctx = canvas.getContext("2d");

  return { canvas, ctx };
}

/**
 * Resize the canvases' resolutions, and fires out the onResize callback
 *
 * @param  {HTMLDivElement} container
 * @param  {Object} canvases
 * @param  {Number} debounceRate
 */
function handleResizes(canvases, debounceRate) {
  const { container, main, zoom } = canvases;
  const window = container.ownerDocument.defaultView;

  function resize() {
    const width = container.offsetWidth * window.devicePixelRatio;
    const height = container.offsetHeight * window.devicePixelRatio;

    main.canvas.width = width;
    main.canvas.height = height;
    zoom.canvas.width = width;
    zoom.canvas.height = height;

    canvases.emit("resize");
  }

  // Tests may not need debouncing
  const debouncedResize =
    debounceRate > 0 ? debounce(resize, debounceRate) : resize;

  window.addEventListener("resize", debouncedResize);
  resize();

  return function removeResizeHandlers() {
    window.removeEventListener("resize", debouncedResize);
  };
}
