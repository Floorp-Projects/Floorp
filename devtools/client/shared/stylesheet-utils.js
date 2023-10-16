/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */
"use strict";

function stylesheetLoadPromise(styleSheet, url) {
  return new Promise((resolve, reject) => {
    styleSheet.addEventListener("load", resolve, { once: true });
    styleSheet.addEventListener("error", reject, { once: true });
  });
}

/*
 * Put the DevTools theme stylesheet into the provided chrome document.
 *
 * @param  {Document} doc
 *         The chrome document where the stylesheet should be appended.
 * @param  {String} url
 *         The url of the stylesheet to load.
 * @return {Object}
 *         - styleSheet {XMLStylesheetProcessingInstruction} the instruction node created.
 *         - loadPromise {Promise} that will resolve/reject when the stylesheet finishes
 *           or fails to load.
 */
function appendStyleSheet(doc, url) {
  const styleSheet = doc.createElementNS(
    "http://www.w3.org/1999/xhtml",
    "link"
  );
  styleSheet.setAttribute("rel", "stylesheet");
  styleSheet.setAttribute("href", url);
  const loadPromise = stylesheetLoadPromise(styleSheet);

  // In order to make overriding styles sane, we want the order of loaded
  // sheets to be something like this:
  //   global.css
  //   *-theme.css (the file loaded here)
  //   document-specific-sheet.css
  // See Bug 1582786 / https://phabricator.services.mozilla.com/D46530#inline-284756.

  // If there is a global sheet then insert after it.
  const globalSheet = doc.querySelector(
    "link[href='chrome://global/skin/global.css']"
  );
  if (globalSheet) {
    globalSheet.after(styleSheet);
    return { styleSheet, loadPromise };
  }

  // Otherwise insert before any existing link.
  const links = doc.querySelectorAll("link");
  if (links.length) {
    links[0].before(styleSheet);
    return { styleSheet, loadPromise };
  }

  // Fall back to putting at the start of the head or in a non-HTML doc the
  // start of the document.
  if (doc.head) {
    doc.head.prepend(styleSheet);
  } else {
    doc.documentElement.prepend(styleSheet);
  }

  return { styleSheet, loadPromise };
}

exports.appendStyleSheet = appendStyleSheet;
