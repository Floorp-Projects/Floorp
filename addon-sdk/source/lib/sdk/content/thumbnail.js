/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

module.metadata = {
  'stability': 'unstable'
};

const { Cc, Ci, Cu } = require('chrome');
const AppShellService = Cc['@mozilla.org/appshell/appShellService;1'].
                        getService(Ci.nsIAppShellService);

const NS = 'http://www.w3.org/1999/xhtml';
const COLOR = 'rgb(255,255,255)';

/**
 * Creates canvas element with a thumbnail of the passed window.
 * @param {Window} window
 * @returns {Element}
 */
function getThumbnailCanvasForWindow(window) {
  let aspectRatio = 0.5625; // 16:9
  let thumbnail = AppShellService.hiddenDOMWindow.document
                    .createElementNS(NS, 'canvas');
  thumbnail.mozOpaque = true;
  thumbnail.width = Math.ceil(window.screen.availWidth / 5.75);
  thumbnail.height = Math.round(thumbnail.width * aspectRatio);
  let ctx = thumbnail.getContext('2d');
  let snippetWidth = window.innerWidth * .6;
  let scale = thumbnail.width / snippetWidth;
  ctx.scale(scale, scale);
  ctx.drawWindow(window, window.scrollX, window.scrollY, snippetWidth,
                snippetWidth * aspectRatio, COLOR);
  return thumbnail;
}
exports.getThumbnailCanvasForWindow = getThumbnailCanvasForWindow;

/**
 * Creates Base64 encoded data URI of the thumbnail for the passed window.
 * @param {Window} window
 * @returns {String}
 */
exports.getThumbnailURIForWindow = function getThumbnailURIForWindow(window) {
  return getThumbnailCanvasForWindow(window).toDataURL()
};
