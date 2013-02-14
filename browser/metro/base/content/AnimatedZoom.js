// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Responsible for zooming in to a given view rectangle
 */
const AnimatedZoom = {
  startScale: null,

  /** Starts an animated zoom to zoomRect. */
  animateTo: function(aZoomRect) {
    if (!aZoomRect)
      return;

    this.zoomTo = aZoomRect.clone();

    if (this.animationDuration === undefined)
      this.animationDuration = Services.prefs.getIntPref("browser.ui.zoom.animationDuration");

    Browser.forceChromeReflow();

    this.start();

    // Check if zooming animations were occuring before.
    if (!this.zoomRect) {
      this.updateTo(this.zoomFrom);

      mozRequestAnimationFrame(this);

      let event = document.createEvent("Events");
      event.initEvent("AnimatedZoomBegin", true, true);
      window.dispatchEvent(event);
    }
  },

  start: function start() {
    this.tab = Browser.selectedTab;
    this.browser = this.tab.browser;
    this.bcr = this.browser.getBoundingClientRect();
    this.zoomFrom = this.zoomRect || this.getStartRect();
    this.startScale = this.browser.scale;
    this.beginTime = mozAnimationStartTime;
  },

  /** Get the visible rect, in device pixels relative to the content origin. */
  getStartRect: function getStartRect() {
    let browser = this.browser;
    let scroll = browser.getRootView().getPosition();
    return new Rect(scroll.x, scroll.y, this.bcr.width, this.bcr.height);
  },

  /** Update the visible rect, in device pixels relative to the content origin. */
  updateTo: function(nextRect) {
    // Stop animating if the browser has been destroyed
    if (typeof this.browser.fuzzyZoom !== "function") {
      this.reset();
      return false;
    }

    let zoomRatio = this.bcr.width / nextRect.width;
    let scale = this.startScale * zoomRatio;
    let scrollX = nextRect.left * zoomRatio;
    let scrollY = nextRect.top * zoomRatio;

    this.browser.fuzzyZoom(scale, scrollX, scrollY);

    this.zoomRect = nextRect;
    return true;
  },

  /** Stop animation, zoom to point, and clean up. */
  finish: function() {
    if (!this.updateTo(this.zoomTo || this.zoomRect))
      return;

    // Check whether the zoom limits have changed since the animation started.
    let browser = this.browser;
    let finalScale = this.tab.clampZoomLevel(browser.scale);
    if (browser.scale != finalScale)
      browser.scale = finalScale; // scale= calls finishFuzzyZoom.
    else
      browser.finishFuzzyZoom();

    this.reset();
    browser._updateCSSViewport();
  },

  reset: function reset() {
    this.beginTime = null;
    this.zoomTo = null;
    this.zoomFrom = null;
    this.zoomRect = null;
    this.startScale = null;

    let event = document.createEvent("Events");
    event.initEvent("AnimatedZoomEnd", true, true);
    window.dispatchEvent(event);
  },

  isZooming: function isZooming() {
    return this.beginTime != null;
  },

  sample: function(aTimeStamp) {
    try {
      let tdiff = aTimeStamp - this.beginTime;
      let counter = tdiff / this.animationDuration;
      if (counter < 1) {
        // update browser to interpolated rectangle
        let rect = this.zoomFrom.blend(this.zoomTo, counter);
        if (this.updateTo(rect))
          mozRequestAnimationFrame(this);
      } else {
        // last cycle already rendered final scaled image, now clean up
        this.finish();
      }
    } catch(e) {
      this.finish();
      throw e;
    }
  }
};
