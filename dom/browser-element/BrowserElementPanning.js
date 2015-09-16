/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 sw=2 sts=2 et: */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
dump("############################### browserElementPanning.js loaded\n");

var { classes: Cc, interfaces: Ci, results: Cr, utils: Cu }  = Components;
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Geometry.jsm");

const kObservedEvents = [
  "BEC:ShownModalPrompt",
  "Activity:Success",
  "Activity:Error"
];

const ContentPanning = {
  init: function cp_init() {
    addEventListener("unload",
		     this._unloadHandler.bind(this),
		     /* useCapture = */ false,
		     /* wantsUntrusted = */ false);

    addMessageListener("Viewport:Change", this._recvViewportChange.bind(this));
    addMessageListener("Gesture:DoubleTap", this._recvDoubleTap.bind(this));
    addEventListener("visibilitychange", this._handleVisibilityChange.bind(this));
    kObservedEvents.forEach((topic) => {
      Services.obs.addObserver(this, topic, false);
    });
  },

  observe: function cp_observe(subject, topic, data) {
    this._resetHover();
  },

  get _domUtils() {
    delete this._domUtils;
    return this._domUtils = Cc['@mozilla.org/inspector/dom-utils;1']
                              .getService(Ci.inIDOMUtils);
  },

  _resetHover: function cp_resetHover() {
    const kStateHover = 0x00000004;
    try {
      let element = content.document.createElement('foo');
      this._domUtils.setContentState(element, kStateHover);
    } catch(e) {}
  },

  _recvViewportChange: function(data) {
    let metrics = data.json;
    this._viewport = new Rect(metrics.x, metrics.y,
                              metrics.viewport.width,
                              metrics.viewport.height);
    this._cssCompositedRect = new Rect(metrics.x, metrics.y,
                                       metrics.cssCompositedRect.width,
                                       metrics.cssCompositedRect.height);
    this._cssPageRect = new Rect(metrics.cssPageRect.x,
                                 metrics.cssPageRect.y,
                                 metrics.cssPageRect.width,
                                 metrics.cssPageRect.height);
  },

  _recvDoubleTap: function(data) {
    data = data.json;

    // We haven't received a metrics update yet; don't do anything.
    if (this._viewport == null) {
      return;
    }

    let win = content;

    let element = ElementTouchHelper.anyElementFromPoint(win, data.x, data.y);
    if (!element) {
      this._zoomOut();
      return;
    }

    while (element && !this._shouldZoomToElement(element))
      element = element.parentNode;

    if (!element) {
      this._zoomOut();
    } else {
      const margin = 15;
      let rect = ElementTouchHelper.getBoundingContentRect(element);

      let cssPageRect = this._cssPageRect;
      let viewport = this._viewport;
      let bRect = new Rect(Math.max(cssPageRect.x, rect.x - margin),
                           rect.y,
                           rect.w + 2 * margin,
                           rect.h);
      // constrict the rect to the screen's right edge
      bRect.width = Math.min(bRect.width, cssPageRect.right - bRect.x);

      // if the rect is already taking up most of the visible area and is stretching the
      // width of the page, then we want to zoom out instead.
      if (this._isRectZoomedIn(bRect, this._cssCompositedRect)) {
        this._zoomOut();
        return;
      }

      rect.x = Math.round(bRect.x);
      rect.y = Math.round(bRect.y);
      rect.w = Math.round(bRect.width);
      rect.h = Math.round(bRect.height);

      // if the block we're zooming to is really tall, and the user double-tapped
      // more than a screenful of height from the top of it, then adjust the y-coordinate
      // so that we center the actual point the user double-tapped upon. this prevents
      // flying to the top of a page when double-tapping to zoom in (bug 761721).
      // the 1.2 multiplier is just a little fuzz to compensate for bRect including horizontal
      // margins but not vertical ones.
      let cssTapY = viewport.y + data.y;
      if ((bRect.height > rect.h) && (cssTapY > rect.y + (rect.h * 1.2))) {
        rect.y = cssTapY - (rect.h / 2);
      }

      Services.obs.notifyObservers(docShell, 'browser-zoom-to-rect', JSON.stringify(rect));
    }
  },

  _handleVisibilityChange: function(evt) {
    if (!evt.target.hidden)
      return;

    this._resetHover();
  },

  _shouldZoomToElement: function(aElement) {
    let win = aElement.ownerDocument.defaultView;
    if (win.getComputedStyle(aElement, null).display == "inline")
      return false;
    if (aElement instanceof Ci.nsIDOMHTMLLIElement)
      return false;
    if (aElement instanceof Ci.nsIDOMHTMLQuoteElement)
      return false;
    return true;
  },

  _zoomOut: function() {
    let rect = new Rect(0, 0, 0, 0);
    Services.obs.notifyObservers(docShell, 'browser-zoom-to-rect', JSON.stringify(rect));
  },

  _isRectZoomedIn: function(aRect, aViewport) {
    // This function checks to see if the area of the rect visible in the
    // viewport (i.e. the "overlapArea" variable below) is approximately 
    // the max area of the rect we can show.
    let vRect = new Rect(aViewport.x, aViewport.y, aViewport.width, aViewport.height);
    let overlap = vRect.intersect(aRect);
    let overlapArea = overlap.width * overlap.height;
    let availHeight = Math.min(aRect.width * vRect.height / vRect.width, aRect.height);
    let showing = overlapArea / (aRect.width * availHeight);
    let ratioW = (aRect.width / vRect.width);
    let ratioH = (aRect.height / vRect.height);

    return (showing > 0.9 && (ratioW > 0.9 || ratioH > 0.9)); 
  },

  _unloadHandler: function() {
    kObservedEvents.forEach((topic) => {
      Services.obs.removeObserver(this, topic);
    });
  }
};

const ElementTouchHelper = {
  anyElementFromPoint: function(aWindow, aX, aY) {
    let cwu = aWindow.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
    let elem = cwu.elementFromPoint(aX, aY, true, true);

    let HTMLIFrameElement = Ci.nsIDOMHTMLIFrameElement;
    let HTMLFrameElement = Ci.nsIDOMHTMLFrameElement;
    while (elem && (elem instanceof HTMLIFrameElement || elem instanceof HTMLFrameElement)) {
      let rect = elem.getBoundingClientRect();
      aX -= rect.left;
      aY -= rect.top;
      cwu = elem.contentDocument.defaultView.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
      elem = cwu.elementFromPoint(aX, aY, true, true);
    }

    return elem;
  },

  getBoundingContentRect: function(aElement) {
    if (!aElement)
      return {x: 0, y: 0, w: 0, h: 0};

    let document = aElement.ownerDocument;
    while (document.defaultView.frameElement)
      document = document.defaultView.frameElement.ownerDocument;

    let cwu = document.defaultView.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
    let scrollX = {}, scrollY = {};
    cwu.getScrollXY(false, scrollX, scrollY);

    let r = aElement.getBoundingClientRect();

    // step out of iframes and frames, offsetting scroll values
    for (let frame = aElement.ownerDocument.defaultView; frame.frameElement && frame != content; frame = frame.parent) {
      // adjust client coordinates' origin to be top left of iframe viewport
      let rect = frame.frameElement.getBoundingClientRect();
      let left = frame.getComputedStyle(frame.frameElement, "").borderLeftWidth;
      let top = frame.getComputedStyle(frame.frameElement, "").borderTopWidth;
      scrollX.value += rect.left + parseInt(left);
      scrollY.value += rect.top + parseInt(top);
    }

    return {x: r.left + scrollX.value,
            y: r.top + scrollY.value,
            w: r.width,
            h: r.height };
  }
};
