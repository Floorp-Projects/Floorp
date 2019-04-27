/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const DevToolsUtils = require("devtools/shared/DevToolsUtils");
const { getCurrentZoom, getViewportDimensions } = require("devtools/shared/layout/utils");
const { moveInfobar, createNode } = require("./markup");
const { truncateString } = require("devtools/shared/inspector/utils");

const { accessibility: { SCORES } } = require("devtools/shared/constants");

const STRINGS_URI = "devtools/shared/locales/accessibility.properties";
loader.lazyRequireGetter(this, "LocalizationHelper", "devtools/shared/l10n", true);
DevToolsUtils.defineLazyGetter(this, "L10N", () => new LocalizationHelper(STRINGS_URI));

const { accessibility: { AUDIT_TYPE } } = require("devtools/shared/constants");

// Max string length for truncating accessible name values.
const MAX_STRING_LENGTH = 50;

/**
 * The AccessibleInfobar is a class responsible for creating the markup for the
 * accessible highlighter. It is also reponsible for updating content within the
 * infobar such as role and name values.
 */
class Infobar {
  constructor(highlighter) {
    this.highlighter = highlighter;
    this.audit = new Audit(this);
  }

  get document() {
    return this.highlighter.win.document;
  }

  get bounds() {
    return this.highlighter._bounds;
  }

  get options() {
    return this.highlighter.options;
  }

  get prefix() {
    return this.highlighter.ID_CLASS_PREFIX;
  }

  get win() {
    return this.highlighter.win;
  }

  /**
   * Move the Infobar to the right place in the highlighter.
   *
   * @param  {Element} container
   *         Container of infobar.
   */
  _moveInfobar(container) {
    // Position the infobar using accessible's bounds
    const { left: x, top: y, bottom, width } = this.bounds;
    const infobarBounds = { x, y, bottom, width };

    moveInfobar(container, infobarBounds, this.win);
  }

  /**
   * Build markup for infobar.
   *
   * @param  {Element} root
   *         Root element to build infobar with.
   */
  buildMarkup(root) {
    const container = createNode(this.win, {
      parent: root,
      attributes: {
        "class": "infobar-container",
        "id": "infobar-container",
        "aria-hidden": "true",
        "hidden": "true",
      },
      prefix: this.prefix,
    });

    const infobar = createNode(this.win, {
      parent: container,
      attributes: {
        "class": "infobar",
        "id": "infobar",
      },
      prefix: this.prefix,
    });

    const infobarText = createNode(this.win, {
      parent: infobar,
      attributes: {
        "class": "infobar-text",
        "id": "infobar-text",
      },
      prefix: this.prefix,
    });

    createNode(this.win, {
      nodeType: "span",
      parent: infobarText,
      attributes: {
        "class": "infobar-role",
        "id": "infobar-role",
      },
      prefix: this.prefix,
    });

    createNode(this.win, {
      nodeType: "span",
      parent: infobarText,
      attributes: {
        "class": "infobar-name",
        "id": "infobar-name",
      },
      prefix: this.prefix,
    });

    this.audit.buildMarkup(infobarText);
  }

  /**
   * Destroy the Infobar's highlighter.
   */
  destroy() {
    this.highlighter = null;
    this.audit.destroy();
    this.audit = null;
  }

  /**
   * Gets the element with the specified ID.
   *
   * @param  {String} id
   *         Element ID.
   * @return {Element} The element with specified ID.
   */
  getElement(id) {
    return this.highlighter.getElement(id);
  }

  /**
   * Gets the text content of element.
   *
   * @param  {String} id
   *          Element ID to retrieve text content from.
   * @return {String} The text content of the element.
   */
  getTextContent(id) {
    const anonymousContent = this.highlighter.markup.content;
    return anonymousContent.getTextContentForElement(`${this.prefix}${id}`);
  }

  /**
   * Hide the accessible infobar.
   */
  hide() {
    const container = this.getElement("infobar-container");
    container.setAttribute("hidden", "true");
  }

  /**
   * Show the accessible infobar highlighter.
   */
  show() {
    const container = this.getElement("infobar-container");

    // Remove accessible's infobar "hidden" attribute. We do this first to get the
    // computed styles of the infobar container.
    container.removeAttribute("hidden");

    // Update the infobar's position and content.
    this.update(container);
  }

  /**
   * Update content of the infobar.
   */
  update(container) {
    const { audit, name, role } = this.options;

    this.updateRole(role, this.getElement("infobar-role"));
    this.updateName(name, this.getElement("infobar-name"));
    this.audit.update(audit);

    // Position the infobar.
    this._moveInfobar(container);
  }

  /**
   * Sets the text content of the specified element.
   *
   * @param  {Element} el
   *         Element to set text content on.
   * @param  {String} text
   *         Text for content.
   */
  setTextContent(el, text) {
    el.setTextContent(text);
  }

  /**
   * Show the accessible's name message.
   *
   * @param  {String} name
   *         Accessible's name value.
   * @param  {Element} el
   *         Element to set text content on.
   */
  updateName(name, el) {
    const nameText = name ? `"${truncateString(name, MAX_STRING_LENGTH)}"` : "";
    this.setTextContent(el, nameText);
  }

  /**
   * Show the accessible's role.
   *
   * @param  {String} role
   *         Accessible's role value.
   * @param  {Element} el
   *         Element to set text content on.
   */
  updateRole(role, el) {
    this.setTextContent(el, role);
  }
}

/**
 * The XULAccessibleInfobar handles building the XUL infobar markup where it isn't
 * possible with the regular accessible highlighter.
 */
class XULWindowInfobar extends Infobar {
  /**
   * A helper function that calculates the positioning of a XUL accessible's infobar.
   *
   * @param  {Object} container
   *         The infobar container.
   */
  _moveInfobar(container) {
    const arrow = this.getElement("arrow");

    // Show the container and arrow elements first.
    container.removeAttribute("hidden");
    arrow.removeAttribute("hidden");

    // Set the left value of the infobar container in relation to
    // highlighter's bounds position.
    const {
      left: boundsLeft,
      right: boundsRight,
      top: boundsTop,
      bottom: boundsBottom,
    } = this.bounds;
    const boundsMidPoint = (boundsLeft + boundsRight) / 2;
    container.style.left = `${boundsMidPoint}px`;

    const zoom = getCurrentZoom(this.win);
    let {
      width: viewportWidth,
      height: viewportHeight,
    } = getViewportDimensions(this.win);

    const { width, height, left } = container.getBoundingClientRect();

    const containerHalfWidth = width / 2;
    const containerHeight = height;
    const margin = 100 * zoom;

    viewportHeight *= zoom;
    viewportWidth *= zoom;

    // Determine viewport boundaries for infobar.
    const topBoundary = margin;
    const bottomBoundary = viewportHeight - containerHeight;
    const leftBoundary = containerHalfWidth;
    const rightBoundary = viewportWidth - containerHalfWidth;

    // Determine if an infobar's position is offscreen.
    const isOffScreenOnTop = boundsBottom < topBoundary;
    const isOffScreenOnBottom = boundsBottom > bottomBoundary;
    const isOffScreenOnLeft = left < leftBoundary;
    const isOffScreenOnRight = left > rightBoundary;

    // Check if infobar is offscreen on either left/right of viewport and position.
    if (isOffScreenOnLeft) {
      container.style.left = `${leftBoundary + boundsLeft}px`;
      arrow.setAttribute("hidden", "true");
    } else if (isOffScreenOnRight) {
      const leftOffset = rightBoundary - boundsRight;
      container.style.left = `${rightBoundary - leftOffset - containerHalfWidth}px`;
      arrow.setAttribute("hidden", "true");
    }

    // Check if infobar is offscreen on either top/bottom of viewport and position.
    const bubbleArrowSize = "var(--highlighter-bubble-arrow-size)";

    if (isOffScreenOnTop) {
      if (boundsTop < 0) {
        container.style.top = bubbleArrowSize;
      } else {
        container.style.top = `calc(${boundsBottom}px + ${bubbleArrowSize})`;
      }
      arrow.setAttribute("class", "accessible-arrow top");
    } else if (isOffScreenOnBottom) {
      container.style.top = `calc(${bottomBoundary}px - ${bubbleArrowSize})`;
      arrow.setAttribute("hidden", "true");
    } else {
      container.style.top = `calc(${boundsTop}px -
        (${containerHeight}px + ${bubbleArrowSize}))`;
      arrow.setAttribute("class", "accessible-arrow bottom");
    }
  }

  /**
   * Build markup for XUL window infobar.
   *
   * @param  {Element} root
   *         Root element to build infobar with.
   */
  buildMarkup(root) {
    super.buildMarkup(root, createNode);

    createNode(this.win, {
      parent: this.getElement("infobar"),
      attributes: {
        "class": "arrow",
        "id": "arrow",
      },
      prefix: this.prefix,
    });
  }

  /**
   * Override of Infobar class's getTextContent method.
   *
   * @param  {String} id
   *         Element ID to retrieve text content from.
   * @return {String} Returns the text content of the element.
   */
  getTextContent(id) {
    return this.getElement(id).textContent;
  }

  /**
   * Override of Infobar class's getElement method.
   *
   * @param  {String} id
   *         Element ID.
   * @return {String} Returns the specified element.
   */
  getElement(id) {
    return this.win.document.getElementById(`${this.prefix}${id}`);
  }

  /**
   * Override of Infobar class's setTextContent method.
   *
   * @param  {Element} el
   *         Element to set text content on.
   * @param  {String} text
   *         Text for content.
   */
  setTextContent(el, text) {
    el.textContent = text;
  }
}

/**
 * Audit component used within the accessible highlighter infobar. This component is
 * responsible for rendering and updating its containing AuditReport components that
 * display various audit information such as contrast ratio score.
 */
class Audit {
  constructor(infobar) {
    this.infobar = infobar;

    // A list of audit reports to be shown on the fly when highlighting an accessible
    // object.
    this.reports = [
      new ContrastRatio(this),
    ];
  }

  get prefix() {
    return this.infobar.prefix;
  }

  get win() {
    return this.infobar.win;
  }

  buildMarkup(root) {
    const audit = createNode(this.win, {
      nodeType: "span",
      parent: root,
      attributes: {
        "class": "infobar-audit",
        "id": "infobar-audit",
      },
      prefix: this.prefix,
    });

    this.reports.forEach(report => report.buildMarkup(audit));
  }

  update(audit = {}) {
    const el = this.getElement("infobar-audit");
    el.setAttribute("hidden", true);

    let updated = false;
    this.reports.forEach(report => {
      if (report.update(audit)) {
        updated = true;
      }
    });

    if (updated) {
      el.removeAttribute("hidden");
    }
  }

  getElement(id) {
    return this.infobar.getElement(id);
  }

  setTextContent(el, text) {
    return this.infobar.setTextContent(el, text);
  }

  destroy() {
    this.infobar = null;
    this.reports.forEach(report => report.destroy());
    this.reports = null;
  }
}

/**
 * A common interface between audit report components used to render accessibility audit
 * information for the currently highlighted accessible object.
 */
class AuditReport {
  constructor(audit) {
    this.audit = audit;
  }

  get prefix() {
    return this.audit.prefix;
  }

  get win() {
    return this.audit.win;
  }

  getElement(id) {
    return this.audit.getElement(id);
  }

  setTextContent(el, text) {
    return this.audit.setTextContent(el, text);
  }

  destroy() {
    this.audit = null;
  }
}

/**
 * Contrast ratio audit report that is used to display contrast ratio score as part of the
 * inforbar,
 */
class ContrastRatio extends AuditReport {
  buildMarkup(root) {
    createNode(this.win, {
      nodeType: "span",
      parent: root,
      attributes: {
        "class": "contrast-ratio-label",
        "id": "contrast-ratio-label",
      },
      prefix: this.prefix,
    });

    createNode(this.win, {
      nodeType: "span",
      parent: root,
      attributes: {
        "class": "contrast-ratio-error",
        "id": "contrast-ratio-error",
      },
      prefix: this.prefix,
      text: L10N.getStr("accessibility.contrast.ratio.error"),
    });

    createNode(this.win, {
      nodeType: "span",
      parent: root,
      attributes: {
        "class": "contrast-ratio",
        "id": "contrast-ratio-min",
      },
      prefix: this.prefix,
    });

    createNode(this.win, {
      nodeType: "span",
      parent: root,
      attributes: {
        "class": "contrast-ratio-separator",
        "id": "contrast-ratio-separator",
      },
      prefix: this.prefix,
    });

    createNode(this.win, {
      nodeType: "span",
      parent: root,
      attributes: {
        "class": "contrast-ratio",
        "id": "contrast-ratio-max",
      },
      prefix: this.prefix,
    });
  }

  _fillAndStyleContrastValue(el, { value, className, color, backgroundColor }) {
    value = value.toFixed(2);
    this.setTextContent(el, value);
    el.classList.add(className);
    el.setAttribute("style",
      `--accessibility-highlighter-contrast-ratio-color: rgba(${color});` +
      `--accessibility-highlighter-contrast-ratio-bg: rgba(${backgroundColor});`);
    el.removeAttribute("hidden");
  }

  /**
   * Update contrast ratio score infobar markup.
   * @param  {Number}
   *         Contrast ratio for an accessible object being highlighted.
   * @return {Boolean}
   *         True if the contrast ratio markup was updated correctly and infobar audit
   *         block should be visible.
   */
  update({ [AUDIT_TYPE.CONTRAST]: contrastRatio }) {
    const els = {};
    for (const key of ["label", "min", "max", "error", "separator"]) {
      const el = els[key] = this.getElement(`contrast-ratio-${key}`);
      if (["min", "max"].includes(key)) {
        Object.values(SCORES).forEach(
          className => el.classList.remove(className));
        this.setTextContent(el, "");
      }

      el.setAttribute("hidden", true);
      el.removeAttribute("style");
    }

    if (!contrastRatio) {
      return false;
    }

    const { isLargeText, error } = contrastRatio;
    this.setTextContent(els.label,
      L10N.getStr(`accessibility.contrast.ratio.label${isLargeText ? ".large" : ""}`));
    els.label.removeAttribute("hidden");
    if (error) {
      els.error.removeAttribute("hidden");
      return true;
    }

    if (contrastRatio.value) {
      const { value, color, score, backgroundColor } = contrastRatio;
      this._fillAndStyleContrastValue(els.min,
        { value, className: score, color, backgroundColor });
      return true;
    }

    const {
      min, max, color, backgroundColorMin, backgroundColorMax, scoreMin, scoreMax,
    } = contrastRatio;
    this._fillAndStyleContrastValue(els.min,
      { value: min, className: scoreMin, color, backgroundColor: backgroundColorMin });
    els.separator.removeAttribute("hidden");
    this._fillAndStyleContrastValue(els.max,
      { value: max, className: scoreMax, color, backgroundColor: backgroundColorMax });

    return true;
  }
}

/**
 * A helper function that calculate accessible object bounds and positioning to
 * be used for highlighting.
 *
 * @param  {Object} win
 *         window that contains accessible object.
 * @param  {Object} options
 *         Object used for passing options:
 *         - {Number} x
 *           x coordinate of the top left corner of the accessible object
 *         - {Number} y
 *           y coordinate of the top left corner of the accessible object
 *         - {Number} w
 *           width of the the accessible object
 *         - {Number} h
 *           height of the the accessible object
 *         - {Number} zoom
 *           zoom level of the accessible object's parent window
 * @return {Object|null} Returns, if available, positioning and bounds information for
 *                 the accessible object.
 */
function getBounds(win, { x, y, w, h, zoom }) {
  let { mozInnerScreenX, mozInnerScreenY, scrollX, scrollY } = win;
  let zoomFactor = getCurrentZoom(win);
  let left = x;
  let right = x + w;
  let top = y;
  let bottom = y + h;

  // For a XUL accessible, normalize the top-level window with its current zoom level.
  // We need to do this because top-level browser content does not allow zooming.
  if (zoom) {
    zoomFactor = zoom;
    mozInnerScreenX /= zoomFactor;
    mozInnerScreenY /= zoomFactor;
    scrollX /= zoomFactor;
    scrollY /= zoomFactor;
  }

  left -= mozInnerScreenX - scrollX;
  right -= mozInnerScreenX - scrollX;
  top -= mozInnerScreenY - scrollY;
  bottom -= mozInnerScreenY - scrollY;

  left *= zoomFactor;
  right *= zoomFactor;
  top *= zoomFactor;
  bottom *= zoomFactor;

  const width = right - left;
  const height = bottom - top;

  return { left, right, top, bottom, width, height };
}

exports.MAX_STRING_LENGTH = MAX_STRING_LENGTH;
exports.getBounds = getBounds;
exports.Infobar = Infobar;
exports.XULWindowInfobar = XULWindowInfobar;
