/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This is a chart-like editor for linear() easing function, used in the Rules View.
 */

const EventEmitter = require("devtools/shared/event-emitter");
const { getCSSLexer } = require("devtools/shared/css/lexer");
const { throttle } = require("devtools/shared/throttle");
const XHTML_NS = "http://www.w3.org/1999/xhtml";
const SVG_NS = "http://www.w3.org/2000/svg";

const numberFormatter = new Intl.NumberFormat("en", {
  maximumFractionDigits: 3,
});
const percentFormatter = new Intl.NumberFormat("en", {
  maximumFractionDigits: 2,
  style: "percent",
});

/**
 * Easing function widget. Draw the lines and control points in an svg.
 *
 * XXX: The spec allows input and output values in the [-Infinity,Infinity] range,
 * but this will be hard to have proper visual representation to handle those cases, so we
 * only handle points inside [0,0] [1,1] to represent most common use cases (even though
 * the line will properly link points outside of this range)
 *
 *
 * @emits "updated" events whenever the line is changed, with the updated property value.
 */
class LinearEasingFunctionWidget extends EventEmitter {
  /**
   * @param {DOMNode} parent The container where the widget should be created
   */
  constructor(parent) {
    super();

    this.parent = parent;
    this.#initMarkup();

    this.#svgEl.addEventListener("mousedown", this.#onMouseDown.bind(this), {
      signal: this.#abortController.signal,
    });
    this.#svgEl.addEventListener("dblclick", this.#onDoubleClick.bind(this), {
      signal: this.#abortController.signal,
    });

    // Add the timing function previewer
    // if prefers-reduced-motion is not set
    this.#reducedMotion = parent.ownerGlobal.matchMedia(
      "(prefers-reduced-motion)"
    );
    if (!this.#reducedMotion.matches) {
      this.#timingPreview = new TimingFunctionPreviewWidget(this.#wrapperEl);
    }

    // add event listener to change prefers-reduced-motion
    // of the timing function preview during runtime
    this.#reducedMotion.addEventListener(
      "change",
      event => {
        // if prefers-reduced-motion is enabled destroy timing function preview
        // else create it if it does not exist
        if (event.matches) {
          if (this.#timingPreview) {
            this.#timingPreview.destroy();
          }
          this.#timingPreview = undefined;
        } else if (!this.#timingPreview) {
          this.#timingPreview = new TimingFunctionPreviewWidget(
            this.#wrapperEl
          );
        }
      },
      { signal: this.#abortController.signal }
    );
  }

  static CONTROL_POINTS_CLASSNAME = "control-point";

  // Handles event listener that are enabled for the whole widget lifetime
  #abortController = new AbortController();

  // Array<Object>: Object has `input` (plotted on x axis) and `output` (plotted on y axis) properties
  #functionPoints;

  // MediaQueryList
  #reducedMotion;

  // TimingFunctionPreviewWidget
  #timingPreview;

  // current dragged element. null if there's no dragging happening
  #draggedEl = null;

  // handles event listeners added when user starts dragging an element
  #dragAbortController;

  // element references
  #wrapperEl;
  #svgEl;
  #linearLineEl;
  #controlPointGroupEl;

  /**
   * Creates the markup of the widget
   */
  #initMarkup() {
    const doc = this.parent.ownerDocument;

    const wrap = doc.createElementNS(XHTML_NS, "div");
    wrap.className = "display-wrap";
    this.#wrapperEl = wrap;

    const svg = doc.createElementNS(SVG_NS, "svg");
    svg.classList.add("chart");

    // Add some "padding" to the viewBox so circles near the edges are not clipped.
    const padding = 0.1;
    const length = 1 + padding * 2;
    // XXX: The spec allows input and output values in the [-Infinity,Infinity] range,
    // but this will be hard to have proper visual representation for all cases, so we
    // set the viewBox is basically starting at 0,0 and has a size of 1 (if we don't take the
    // padding into account), to represent most common use cases.
    svg.setAttribute(
      "viewBox",
      `${0 - padding} ${0 - padding} ${length} ${length}`
    );

    // Create a background grid
    const chartGrid = doc.createElementNS(SVG_NS, "g");
    chartGrid.setAttribute("stroke-width", "0.005");
    chartGrid.classList.add("chart-grid");
    for (let i = 0; i <= 10; i++) {
      const value = i / 10;
      const hLine = doc.createElementNS(SVG_NS, "line");
      hLine.setAttribute("x1", 0);
      hLine.setAttribute("y1", value);
      hLine.setAttribute("x2", 1);
      hLine.setAttribute("y2", value);
      const vLine = doc.createElementNS(SVG_NS, "line");
      vLine.setAttribute("x1", value);
      vLine.setAttribute("y1", 0);
      vLine.setAttribute("x2", value);
      vLine.setAttribute("y2", 1);
      chartGrid.append(hLine, vLine);
    }

    // Create the actual graph line
    const linearLine = doc.createElementNS(SVG_NS, "polyline");
    linearLine.classList.add("chart-linear");
    linearLine.setAttribute("fill", "none");
    linearLine.setAttribute("stroke", "context-stroke black");
    linearLine.setAttribute("stroke-width", "0.01");

    // And a group for all the control points
    const controlPointGroup = doc.createElementNS(SVG_NS, "g");
    controlPointGroup.classList.add("control-points-group");

    this.#linearLineEl = linearLine;
    this.#svgEl = svg;
    this.#controlPointGroupEl = controlPointGroup;

    svg.append(chartGrid, linearLine, controlPointGroup);
    wrap.append(svg);
    this.parent.append(wrap);
  }

  /**
   * Remove widget markup, called on destroy
   */
  #removeMarkup() {
    this.#wrapperEl.remove();
  }

  /**
   * Handle mousedown event on the svg
   *
   * @param {MouseEvent} event
   */
  #onMouseDown(event) {
    if (
      !event.target.classList.contains(
        LinearEasingFunctionWidget.CONTROL_POINTS_CLASSNAME
      )
    ) {
      return;
    }

    this.#draggedEl = event.target;
    this.#draggedEl.setPointerCapture(event.pointerId);

    this.#dragAbortController = new AbortController();
    this.#draggedEl.addEventListener(
      "mousemove",
      this.#onMouseMove.bind(this),
      { signal: this.#dragAbortController.signal }
    );
    this.#draggedEl.addEventListener("mouseup", this.#onMouseUp.bind(this), {
      signal: this.#dragAbortController.signal,
    });
  }

  /**
   * Handle mousemove event on a control point. Only active when there's a control point
   * being dragged.
   *
   * @param {MouseEvent} event
   */
  #onMouseMove = throttle(event => {
    if (!this.#draggedEl) {
      return;
    }

    const { x, y } = this.#getPositionInSvgFromEvent(event);

    // XXX: The spec allows input and output values in the [-Infinity,Infinity] range,
    // but this will be hard to have proper visual representation for all cases, so we
    // clamp x and y between 0 and 1 as it's more likely the range that will be used.
    let cx = clamp(0, 1, x);
    let cy = clamp(0, 1, y);

    if (this.#draggedEl.previousSibling) {
      // We don't allow moving the point before the previous point
      cx = Math.max(
        cx,
        parseFloat(this.#draggedEl.previousSibling.getAttribute("cx"))
      );
    }
    if (this.#draggedEl.nextSibling) {
      // We don't allow moving the point after the next point
      cx = Math.min(
        cx,
        parseFloat(this.#draggedEl.nextSibling.getAttribute("cx"))
      );
    }

    // Enable "Snap to grid" when the user holds the shift key
    if (event.shiftKey) {
      cx = Math.round(cx * 10) / 10;
      cy = Math.round(cy * 10) / 10;
    }

    this.#draggedEl.setAttribute("cx", cx);
    this.#draggedEl.setAttribute("cy", cy);

    this.#updateFunctionPointsFromControlPoints();
    this.#redrawLineFromFunctionPoints();
    this.emit("updated", this.getCssLinearValue());
  }, 20);

  /**
   * Handle mouseup event on a control point. Only active when there's a control point
   * being dragged.
   *
   * @param {MouseEvent} event
   */
  #onMouseUp(event) {
    this.#draggedEl.releasePointerCapture(event.pointerId);
    this.#draggedEl = null;
    this.#dragAbortController.abort();
    this.#dragAbortController = null;
  }

  /**
   * Handle dblclick event on the svg.
   * If the target is a control point, this will remove it, otherwise this will add
   * a new control point at the clicked position.
   *
   * @param {MouseEvent} event
   */
  #onDoubleClick(event) {
    const existingPoints = Array.from(
      this.#controlPointGroupEl.querySelectorAll(
        `.${LinearEasingFunctionWidget.CONTROL_POINTS_CLASSNAME}`
      )
    );

    if (
      event.target.classList.contains(
        LinearEasingFunctionWidget.CONTROL_POINTS_CLASSNAME
      )
    ) {
      // The function is only valid when it has at least 2 points, so don't allow to
      // produce invalid value.
      if (existingPoints.length <= 2) {
        return;
      }

      event.target.remove();
      this.#updateFunctionPointsFromControlPoints();
      this.#redrawFromFunctionPoints();
    } else {
      let { x, y } = this.#getPositionInSvgFromEvent(event);

      // Enable "Snap to grid" when the user holds the shift key
      if (event.shiftKey) {
        x = clamp(0, 1, Math.round(x * 10) / 10);
        y = clamp(0, 1, Math.round(y * 10) / 10);
      }

      // Add a control point at specified x and y in svg coords
      // We need to loop through existing control points to insert it at the correct index.
      const nextSibling = existingPoints.find(
        el => parseFloat(el.getAttribute("cx")) >= x
      );

      this.#controlPointGroupEl.insertBefore(
        this.#createSvgControlPointEl(x, y),
        nextSibling
      );
      this.#updateFunctionPointsFromControlPoints();
      this.#redrawLineFromFunctionPoints();
    }
  }

  /**
   * Update this.#functionPoints based on the control points in the svg
   */
  #updateFunctionPointsFromControlPoints() {
    // We ensure to order the control points based on their x position within the group,
    // so here, we can iterate through them without any need to sort them.
    this.#functionPoints = Array.from(
      this.#controlPointGroupEl.querySelectorAll(
        `.${LinearEasingFunctionWidget.CONTROL_POINTS_CLASSNAME}`
      )
    ).map(el => {
      const input = parseFloat(el.getAttribute("cx"));
      // Since svg coords start from the top-left corner, we need to translate cy
      // to have the actual value we want for the function.
      const output = 1 - parseFloat(el.getAttribute("cy"));

      return {
        input,
        output,
      };
    });
  }

  /**
   * Redraw the control points and the linear() line in the svg,
   * based on the value of this.functionPoints.
   */
  #redrawFromFunctionPoints() {
    // Remove previous control points
    this.#controlPointGroupEl
      .querySelectorAll(
        `.${LinearEasingFunctionWidget.CONTROL_POINTS_CLASSNAME}`
      )
      .forEach(el => el.remove());

    if (this.#functionPoints) {
      // Add controls for each function points
      this.#functionPoints.forEach(({ input, output }) => {
        this.#controlPointGroupEl.append(
          // Since svg coords start from the top-left corner, we need to translate output
          // to properly place it on the graph.
          this.#createSvgControlPointEl(input, 1 - output)
        );
      });
    }

    this.#redrawLineFromFunctionPoints();
  }

  /**
   * Redraw linear() line in the svg based on the value of this.functionPoints.
   */
  #redrawLineFromFunctionPoints() {
    // Set the line points
    this.#linearLineEl.setAttribute(
      "points",
      (this.#functionPoints || [])
        .map(
          ({ input, output }) =>
            // Since svg coords start from the top-left corner, we need to translate output
            // to properly place it on the graph.
            `${input},${1 - output}`
        )
        .join(" ")
    );

    const cssLinearValue = this.getCssLinearValue();
    if (this.#timingPreview) {
      this.#timingPreview.preview(cssLinearValue);
    }

    this.emit("updated", cssLinearValue);
  }

  /**
   * Create a control points for the svg line.
   *
   * @param {Number} cx
   * @param {Number} cy
   * @returns {SVGCircleElement}
   */
  #createSvgControlPointEl(cx, cy) {
    const controlEl = this.parent.ownerDocument.createElementNS(
      SVG_NS,
      "circle"
    );
    controlEl.classList.add("control-point");
    controlEl.setAttribute("cx", cx);
    controlEl.setAttribute("cy", cy);
    controlEl.setAttribute("r", 0.025);
    controlEl.setAttribute("fill", "context-fill");
    controlEl.setAttribute("stroke-width", 0);
    return controlEl;
  }

  /**
   * Return the position in the SVG viewbox from mouse event.
   *
   * @param {MouseEvent} event
   * @returns {Object} An object with x and y properties
   */
  #getPositionInSvgFromEvent(event) {
    const position = this.#svgEl.createSVGPoint();
    position.x = event.clientX;
    position.y = event.clientY;

    const matrix = this.#svgEl.getScreenCTM();
    const inverseSvgMatrix = matrix.inverse();
    const transformedPosition = position.matrixTransform(inverseSvgMatrix);

    return { x: transformedPosition.x, y: transformedPosition.y };
  }

  /**
   * Provide the value of the linear() function we want to visualize here.
   * Called from the tooltip with the value of the function in the rule view.
   *
   * @param {String} linearFunctionValue: e.g. `linear(0, 0.5, 1)`.
   */
  setCssLinearValue(linearFunctionValue) {
    if (!linearFunctionValue) {
      return;
    }

    // Parse the string to extract all the points
    const points = parseTimingFunction(linearFunctionValue);
    this.#functionPoints = points;

    // And draw the line and points
    this.#redrawFromFunctionPoints();
  }

  /**
   * Return the value of the linear() function based on the state of the graph.
   * The resulting value is what we emit in the "updated" event.
   *
   * @return {String|null} e.g. `linear(0 0%, 0.5 50%, 1 100%)`.
   */
  getCssLinearValue() {
    if (!this.#functionPoints) {
      return null;
    }

    return `linear(${this.#functionPoints
      .map(
        ({ input, output }) =>
          `${numberFormatter.format(output)} ${percentFormatter.format(input)}`
      )
      .join(", ")})`;
  }

  destroy() {
    this.#abortController.abort();
    this.#dragAbortController?.abort();
    this.#removeMarkup();
    this.#reducedMotion = null;

    if (this.#timingPreview) {
      this.#timingPreview.destroy();
      this.#timingPreview = null;
    }
  }
}

exports.LinearEasingFunctionWidget = LinearEasingFunctionWidget;

/**
 * The TimingFunctionPreviewWidget animates a dot on a scale with a given
 * timing-function
 */
class TimingFunctionPreviewWidget {
  /**
   * @param {DOMNode} parent The container where this widget should go
   */
  constructor(parent) {
    this.#initMarkup(parent);
  }

  #PREVIEW_DURATION = 1000;
  #dotEl;
  #previousValue;

  #initMarkup(parent) {
    const doc = parent.ownerDocument;

    const container = doc.createElementNS(XHTML_NS, "div");
    container.className = "timing-function-preview";

    this.#dotEl = doc.createElementNS(XHTML_NS, "div");
    this.#dotEl.className = "dot";
    container.appendChild(this.#dotEl);
    parent.appendChild(container);
  }

  destroy() {
    this.#dotEl.getAnimations().forEach(anim => anim.cancel());
    this.#dotEl.parentElement.remove();
  }

  /**
   * Preview a new timing function. The current preview will only be stopped if
   * the supplied function value is different from the previous one. If the
   * supplied function is invalid, the preview will stop.
   * @param {Array} value
   */
  preview(timingFunction) {
    if (this.#previousValue == timingFunction) {
      return;
    }
    this.#restartAnimation(timingFunction);
    this.#previousValue = timingFunction;
  }

  /**
   * Re-start the preview animation from the beginning.
   * @param {Array} points
   */
  #restartAnimation = throttle(timingFunction => {
    // Cancel the previous animation if there was any.
    this.#dotEl.getAnimations().forEach(anim => anim.cancel());

    // And start the new one.
    // The animation consists of a few keyframes that move the dot to the right of the
    // container, and then move it back to the left.
    // It also contains some pause where the dot is semi transparent, before it moves to
    // the right, and once again, before it comes back to the left.
    // The timing function passed to this function is applied to the keyframes that
    // actually move the dot. This way it can be previewed in both direction, instead of
    // being spread over the whole animation.
    this.#dotEl.animate(
      [
        { translate: "0%", opacity: 0.5, offset: 0 },
        { translate: "0%", opacity: 0.5, offset: 0.19 },
        { translate: "0%", opacity: 1, offset: 0.2, easing: timingFunction },
        { translate: "100%", opacity: 1, offset: 0.5 },
        { translate: "100%", opacity: 0.5, offset: 0.51 },
        { translate: "100%", opacity: 0.5, offset: 0.7 },
        { translate: "100%", opacity: 1, offset: 0.71, easing: timingFunction },
        { translate: "0%", opacity: 1, offset: 1 },
      ],
      {
        duration: this.#PREVIEW_DURATION * 2,
        iterations: Infinity,
      }
    );
  }, 250);
}

/**
 * Parse a linear() string to collect the different values.
 * https://drafts.csswg.org/css-easing-2/#the-linear-easing-function
 *
 * @param {String} value
 * @return {Array<Object>|undefined} returns undefined if value isn't a valid linear() value.
 *                                   the items of the array are objects with {Number} `input`
 *                                   and {Number} `output` properties.
 */
function parseTimingFunction(value) {
  value = value.trim();
  const tokenStream = getCSSLexer(value);
  const getNextToken = () => {
    while (true) {
      const token = tokenStream.nextToken();
      if (
        !token ||
        (token.tokenType !== "whitespace" && token.tokenType !== "comment")
      ) {
        return token;
      }
    }
  };

  let token = getNextToken();
  if (!token || token.tokenType !== "function" || token.text !== "linear") {
    return undefined;
  }

  // Let's follow the spec parsing algorithm https://drafts.csswg.org/css-easing-2/#linear-easing-function-parsing
  const points = [];
  let largestInput = -Infinity;

  while ((token = getNextToken())) {
    if (token.text === ")") {
      break;
    }

    if (token.tokenType === "number") {
      // [parsing step 4.1]
      const point = { input: null, output: token.number };
      // [parsing step 4.2]
      points.push(point);

      // get nextToken to see if there's a linear stop length
      token = getNextToken();
      // [parsing step 4.3]
      if (token && token.tokenType === "percentage") {
        // [parsing step 4.3.1]
        point.input = Math.max(token.number, largestInput);
        // [parsing step 4.3.2]
        largestInput = point.input;

        // get nextToken to see if there's a second linear stop length
        token = getNextToken();

        // [parsing step 4.3.3]
        if (token && token.tokenType === "percentage") {
          // [parsing step 4.3.3.1]
          const extraPoint = { input: null, output: point.output };
          // [parsing step 4.3.3.2]
          points.push(extraPoint);

          // [parsing step 4.3.3.3]
          extraPoint.input = Math.max(token.number, largestInput);
          // [parsing step 4.3.3.4]
          largestInput = extraPoint.input;
        }
      } else if (points.length == 1) {
        // [parsing step 4.4]
        // [parsing step 4.4.1]
        point.input = 0;
        // [parsing step 4.4.2]
        largestInput = 0;
      }
    }
  }

  if (points.length < 2) {
    return undefined;
  }

  // [parsing step 4.5]
  if (points.at(-1).input === null) {
    points.at(-1).input = Math.max(largestInput, 1);
  }

  // [parsing step 5]

  // We want to retrieve ranges ("runs" in the spec) of items with null inputs so we
  // can compute their input using linear interpolation.
  const nullInputPoints = [];
  points.forEach((point, index, array) => {
    if (point.input == null) {
      // since the first point is guaranteed to have an non-null input, and given that
      // we iterate through the points in regular order, we are guaranteed to find a previous
      // non null point.
      const previousNonNull = array.findLast(
        (item, i) => i < index && item.input !== null
      ).input;
      // since the last point is guaranteed to have an non-null input, and given that
      // we iterate through the points in regular order, we are guaranteed to find a next
      // non null point.
      const nextNonNull = array.find(
        (item, i) => i > index && item.input !== null
      ).input;

      if (nullInputPoints.at(-1)?.indexes?.at(-1) == index - 1) {
        nullInputPoints.at(-1).indexes.push(index);
      } else {
        nullInputPoints.push({
          indexes: [index],
          previousNonNull,
          nextNonNull,
        });
      }
    }
  });

  // For each range of consecutive null-input indexes
  nullInputPoints.forEach(({ indexes, previousNonNull, nextNonNull }) => {
    // For each null-input points, compute their input by linearly interpolating between
    // the closest previous and next points that have a non-null input.
    indexes.forEach((index, i) => {
      points[index].input = lerp(
        previousNonNull,
        nextNonNull,
        (i + 1) / (indexes.length + 1)
      );
    });
  });

  return points;
}

/**
 * Linearly interpolate between 2 numbers.
 *
 * @param {Number} x
 * @param {Number} y
 * @param {Number} a
 *        A value of 0 returns x, and 1 returns y
 * @return {Number}
 */
function lerp(x, y, a) {
  return x * (1 - a) + y * a;
}

/**
 * Clamp value in a range, meaning the result won't be smaller than min
 * and no bigger than max.
 *
 * @param {Number} min
 * @param {Number} max
 * @param {Number} value
 * @returns {Number}
 */
function clamp(min, max, value) {
  return Math.max(min, Math.min(value, max));
}

exports.parseTimingFunction = parseTimingFunction;
