/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import('resource://gre/modules/XPCOMUtils.jsm');

XPCOMUtils.defineLazyModuleGetter(this, "SystemAppProxy", "resource://gre/modules/SystemAppProxy.jsm");

this.EXPORTED_SYMBOLS = ['Screenshot'];

var Screenshot = {
  get: function screenshot_get() {
    let systemAppFrame = SystemAppProxy.getFrame();
    let window = systemAppFrame.ownerDocument.defaultView;
    let document = window.document;

    var canvas = document.createElementNS('http://www.w3.org/1999/xhtml', 'canvas');
    var docRect = document.body.getBoundingClientRect();
    var width = docRect.width;
    var height = docRect.height;

    // Convert width and height from CSS pixels (potentially fractional)
    // to device pixels (integer).
    var scale = window.devicePixelRatio;
    canvas.setAttribute('width', Math.round(width * scale));
    canvas.setAttribute('height', Math.round(height * scale));

    var context = canvas.getContext('2d');
    var flags =
      context.DRAWWINDOW_DRAW_CARET |
      context.DRAWWINDOW_DRAW_VIEW |
      context.DRAWWINDOW_USE_WIDGET_LAYERS;
    context.scale(scale, scale);
    context.drawWindow(window, 0, 0, width, height, 'rgb(255,255,255)', flags);

    return canvas.mozGetAsFile('screenshot', 'image/png');
  }
};
this.Screenshot = Screenshot;
