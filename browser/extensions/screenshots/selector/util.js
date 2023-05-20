/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.util = (function () {
  // eslint-disable-line no-unused-vars
  const exports = {};

  /** Removes a node from its document, if it's a node and the node is attached to a parent */
  exports.removeNode = function (el) {
    if (el && el.parentNode) {
      el.remove();
    }
  };

  /** Truncates the X coordinate to the document size */
  exports.truncateX = function (x) {
    const max = Math.max(
      document.documentElement.clientWidth,
      document.body.clientWidth,
      document.documentElement.scrollWidth,
      document.body.scrollWidth
    );
    if (x < 0) {
      return 0;
    } else if (x > max) {
      return max;
    }
    return x;
  };

  /** Truncates the Y coordinate to the document size */
  exports.truncateY = function (y) {
    const max = Math.max(
      document.documentElement.clientHeight,
      document.body.clientHeight,
      document.documentElement.scrollHeight,
      document.body.scrollHeight
    );
    if (y < 0) {
      return 0;
    } else if (y > max) {
      return max;
    }
    return y;
  };

  // Pixels of wiggle the captured region gets in captureSelectedText:
  const CAPTURE_WIGGLE = 10;
  const ELEMENT_NODE = document.ELEMENT_NODE;

  exports.captureEnclosedText = function (box) {
    const scrollX = window.scrollX;
    const scrollY = window.scrollY;
    const text = [];
    function traverse(el) {
      let elBox = el.getBoundingClientRect();
      elBox = {
        top: elBox.top + scrollY,
        bottom: elBox.bottom + scrollY,
        left: elBox.left + scrollX,
        right: elBox.right + scrollX,
      };
      if (
        elBox.bottom < box.top ||
        elBox.top > box.bottom ||
        elBox.right < box.left ||
        elBox.left > box.right
      ) {
        // Totally outside of the box
        return;
      }
      if (
        elBox.bottom > box.bottom + CAPTURE_WIGGLE ||
        elBox.top < box.top - CAPTURE_WIGGLE ||
        elBox.right > box.right + CAPTURE_WIGGLE ||
        elBox.left < box.left - CAPTURE_WIGGLE
      ) {
        // Partially outside the box
        for (let i = 0; i < el.childNodes.length; i++) {
          const child = el.childNodes[i];
          if (child.nodeType === ELEMENT_NODE) {
            traverse(child);
          }
        }
        return;
      }
      addText(el);
    }
    function addText(el) {
      let t;
      if (el.tagName === "IMG") {
        t = el.getAttribute("alt") || el.getAttribute("title");
      } else if (el.tagName === "A") {
        t = el.innerText;
        if (
          el.getAttribute("href") &&
          !el.getAttribute("href").startsWith("#")
        ) {
          t += " (" + el.href + ")";
        }
      } else {
        t = el.innerText;
      }
      if (t) {
        text.push(t);
      }
    }
    traverse(document.body);
    if (text.length) {
      let result = text.join("\n");
      result = result.replace(/^\s+/, "");
      result = result.replace(/\s+$/, "");
      result = result.replace(/[ \t]+\n/g, "\n");
      return result;
    }
    return null;
  };

  return exports;
})();
null;
