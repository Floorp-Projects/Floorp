/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/* globals document */

/**
 * React components grab the namespace of the element they are mounting to. This function
 * takes a XUL element, and makes sure to create a properly namespaced HTML element to
 * avoid React creating XUL elements.
 *
 * {XULElement} xulElement
 * return {HTMLElement} div
 */

exports.createHtmlMount = function (xulElement) {
  let htmlElement = document.createElementNS("http://www.w3.org/1999/xhtml", "div");
  xulElement.appendChild(htmlElement);
  return htmlElement;
};
