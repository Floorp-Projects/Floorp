/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

class Viewport {
  constructor() {
    this._canvasContainer = document.getElementById('canvasContainer');
    this._viewportController = document.getElementById('viewportController');
    this._sizer = document.getElementById('sizer');

    this._scrollbarWidth = this._getScrollbarWidth();
    this._hasVisibleScrollbars = {
      horizontal: false,
      vertical: false
    };

    this._page = 0;
    this._zoom = 1;
    this._fitting = 'auto';

    // Caches the next position set during a series of actions and will be set
    // to scrollbar's position until calling "_refresh".
    this._nextPosition = null;

    // Indicates the last position that runtime knows. Will update runtime only
    // if "scroll" event gives us a different value.
    this._runtimePosition = this.getScrollOffset();

    // Similar to above. Will notify runtime only if "_notifyRuntimeOfResized()"
    // gets a different value.
    this._runtimeSize = this.getBoundingClientRect();
    this._runtimeOnResizedListener = [];

    this.onProgressChanged = null;
    this.onZoomChanged = null;
    this.onDimensionChanged = null;
    this.onPageChanged = null;

    this._viewportController.addEventListener('scroll', this);
    window.addEventListener('resize', this);
  }

  get zoom() {
    return this._zoom;
  }

  set zoom(newZoom) {
    newZoom = parseFloat(newZoom);
    if (!isNaN(newZoom) &&
        (newZoom != this._zoom || this._fitting != 'none')) {
      this._fitting = 'none';
      this._setZoom(newZoom);
      this._refresh();
    }
  }

  get fitting() {
    return this._fitting;
  }

  set fitting(newFitting) {
    let VALID_VALUE = ['none', 'auto', 'page-actual', 'page-width', 'page-fit'];
    if (!VALID_VALUE.includes(newFitting)) {
      return;
    }

    if (newFitting != this._fitting) {
      this._fitting = newFitting;
      this._setZoom(this._computeFittingZoom());
      this._refresh();
    }
  }

  get pageCount() {
    if (!this._pageDimensions) {
      return 0;
    }
    return this._pageDimensions.length;
  }

  get page() {
    return this._page;
  }

  set page(newPage) {
    newPage = parseFloat(newPage);

    let pageCount = this.pageCount;
    if (!pageCount || !Number.isInteger(newPage)) {
      return;
    }

    newPage = Math.max(0, Math.min(pageCount - 1, newPage));
    this._setPage(newPage);
    this._refresh();
  }

  _getScrollbarWidth() {
    var div = document.createElement('div');
    div.style.visibility = 'hidden';
    div.style.overflow = 'scroll';
    div.style.width = '50px';
    div.style.height = '50px';
    div.style.position = 'absolute';
    document.body.appendChild(div);
    var result = div.offsetWidth - div.clientWidth;
    div.remove();
    return result;
  }

  _documentHasVisibleScrollbars(zoom) {
    let zoomedDimensions = this._getZoomedDocumentDimensions(zoom);
    if (!zoomedDimensions || !this._scrollbarWidth) {
      return {
        horizontal: false,
        vertical: false
      };
    }

    let outerRect = this._viewportController.getBoundingClientRect();
    if (zoomedDimensions.width > outerRect.width) {
      zoomedDimensions.height += this._scrollbarWidth;
    } else if (zoomedDimensions.height > outerRect.height) {
      zoomedDimensions.width += this._scrollbarWidth;
    }

    return {
      horizontal: zoomedDimensions.width > outerRect.width,
      vertical: zoomedDimensions.height > outerRect.height
    };
  }

  _updateProgress(progress) {
    if (typeof this.onProgressChanged === 'function') {
      this.onProgressChanged(progress);
    }
  }

  _setDocumentDimensions(documentDimensions) {
    this._documentDimensions = documentDimensions;
    this._pageDimensions = documentDimensions.pageDimensions;
    this._setZoom(this._computeFittingZoom());
    this._setPage(this._page);
    if (typeof this.onDimensionChanged === 'function') {
      this.onDimensionChanged();
    }
    this._refresh();
  }

  _computeFittingZoom() {
    let newZoom = this._zoom;
    let fitting = this._fitting;

    if (fitting == 'none') {
      return newZoom;
    }

    let FITTING_PADDING = 40;
    let MAX_AUTO_ZOOM = 1.25;

    let page = this._pageDimensions[this._page];
    let viewportRect = this.getBoundingClientRect();

    let pageWidthZoom = (viewportRect.width - FITTING_PADDING) / page.width;
    let pageHeightZoom = viewportRect.height / page.height;

    switch (fitting) {
      case 'auto':
        let isLandscape = (page.width > page.height);
        // For pages in landscape mode, fit the page height to the viewer
        // *unless* the page would thus become too wide to fit horizontally.
        let horizontalZoom = isLandscape ?
          Math.min(pageWidthZoom, pageHeightZoom) : pageWidthZoom;
        newZoom = Math.min(MAX_AUTO_ZOOM, horizontalZoom);
        break;
      case 'page-actual':
        newZoom = 1;
        break;
      case 'page-width':
        newZoom = pageWidthZoom;
        break;
      case 'page-fit':
        newZoom = Math.min(pageWidthZoom, pageHeightZoom);
        break;
    }

    return newZoom;
  }

  _getMostVisiblePage() {
    let firstVisiblePage = 0;

    let pageCount = this.pageCount;
    if (!pageCount) {
      return firstVisiblePage;
    }

    let position = this.getScrollOffset();

    let min = 0;
    let max = pageCount - 1;
    let y = position.y / this._zoom;

    while (max >= min) {
      let page = Math.floor(min + ((max - min) / 2));
      // There might be a gap between the pages, in which case use the bottom
      // of the previous page as the top for finding the page.
      let top = 0;
      if (page > 0) {
        top = this._pageDimensions[page - 1].y +
              this._pageDimensions[page - 1].height;
      }
      let bottom = this._pageDimensions[page].y +
                   this._pageDimensions[page].height;

      if (top <= y && bottom > y) {
        firstVisiblePage = page;
        break;
      } else if (top > y) {
        max = page - 1;
      } else {
        min = page + 1;
      }
    }

    if (firstVisiblePage == pageCount - 1) {
      return firstVisiblePage;
    }

    let rect = this.getBoundingClientRect();
    let viewportRect = {
      x: position.x / this._zoom,
      y: position.y / this._zoom,
      width: rect.width / this._zoom,
      height: rect.height / this._zoom
    };

    let getVisibleHeightRatio = rect => {
      let intersectionHeight =
        Math.min(rect.y + rect.height, viewportRect.y + viewportRect.height) -
        Math.max(rect.y, viewportRect.y);
      return Math.max(0, intersectionHeight) / rect.height;
    };

    let firstVisiblePageVisibility =
      getVisibleHeightRatio(this._pageDimensions[firstVisiblePage]);
    let nextPageVisibility =
      getVisibleHeightRatio(this._pageDimensions[firstVisiblePage + 1]);

    if (nextPageVisibility > firstVisiblePageVisibility) {
      return firstVisiblePage + 1;
    }

    return firstVisiblePage;
  }

  _getZoomedDocumentDimensions(zoom) {
    if (!this._documentDimensions) {
      return null;
    }
    return {
      width: Math.round(this._documentDimensions.width * zoom),
      height: Math.round(this._documentDimensions.height * zoom)
    };
  }

  _setPosition(x, y) {
    this._nextPosition = {x, y};
  }

  _setZoom(newZoom) {
    let currentPos = this._nextPosition || this.getScrollOffset();
    currentPos.x /= this._zoom;
    currentPos.y /= this._zoom;
    this._zoom = newZoom;
    this._contentSizeChanged();
    this._setPosition(
      currentPos.x * newZoom,
      currentPos.y * newZoom
    );
    if (typeof this.onZoomChanged === 'function') {
      this.onZoomChanged(this._zoom);
    }
  }

  _setPage(newPage) {
    if (newPage < 0 || newPage >= this.pageCount) {
      return;
    }
    this._setPosition(
      this._pageDimensions[newPage].x * this._zoom,
      this._pageDimensions[newPage].y * this._zoom
    );
  }

  _updateCanvasSize() {
    let hasScrollbars = this._documentHasVisibleScrollbars(this._zoom);
    if (hasScrollbars.horizontal == this._hasVisibleScrollbars.horizontal &&
        hasScrollbars.vertical == this._hasVisibleScrollbars.vertical) {
      return;
    }
    this._hasVisibleScrollbars = hasScrollbars;
    this._canvasContainer.style.bottom =
      hasScrollbars.horizontal ? this._scrollbarWidth + 'px' : 0;
    this._canvasContainer.style.right =
      hasScrollbars.vertical ? this._scrollbarWidth + 'px' : 0;
    this._notifyRuntimeOfResized();
  }

  _contentSizeChanged() {
    let zoomedDimensions = this._getZoomedDocumentDimensions(this._zoom);
    if (zoomedDimensions) {
      this._sizer.style.width = zoomedDimensions.width + 'px';
      this._sizer.style.height = zoomedDimensions.height + 'px';
      this._updateCanvasSize();
    }
  }

  _resize() {
    let newZoom = this._computeFittingZoom();
    if (newZoom != this._zoom) {
      this._setZoom(newZoom);
    } else {
      this._updateCanvasSize();
    }
  }

  _notifyRuntimeOfResized() {
    let rect = this.getBoundingClientRect();

    if (rect.width == this._runtimeSize.width &&
        rect.height == this._runtimeSize.height) {
      return;
    }

    this._runtimeSize = rect;
    this._runtimeOnResizedListener.forEach(listener => {
      let evt = {
        type: 'resize',
        target: this._viewportController
      };
      if (typeof listener === 'function') {
        listener(evt);
      } else if (typeof listener.handleEvent === 'function') {
        listener.handleEvent(evt);
      }
    });
  }

  _getEventTarget(type) {
    switch(type) {
      case 'keydown':
      case 'keyup':
      case 'keypress':
        return window;
    }
    return this._viewportController;
  }

  _doAction(message) {
    if (this._actionHandler) {
      this._actionHandler(message);
    }
  }

  _refresh() {
    if (this._nextPosition) {
      this._viewportController.scrollTo(
        this._nextPosition.x, this._nextPosition.y);
      this._nextPosition = null;
    }

    this._runtimePosition = this.getScrollOffset();
    this._doAction({
      type: 'viewport',
      xOffset: this._runtimePosition.x,
      yOffset: this._runtimePosition.y,
      zoom: this._zoom
    });

    let newPage = this._getMostVisiblePage();
    if (newPage != this._page) {
      this._page = newPage;
      if (typeof this.onPageChanged === 'function') {
        this.onPageChanged(newPage);
      }
    }
  }

  handleEvent(evt) {
    switch(evt.type) {
      case 'resize':
        this._resize();
        this._notifyRuntimeOfResized();
        this._refresh();
        break;
      case 'scroll':
        this._nextPosition = null;
        let position = this.getScrollOffset();
        if (this._runtimePosition.x != position.x ||
            this._runtimePosition.y != position.y) {
          this._refresh();
        }
        break;
    }
  }

  rotateClockwise() {
    this._doAction({
      type: 'rotateClockwise'
    });
  }

  rotateCounterClockwise() {
    this._doAction({
      type: 'rotateCounterclockwise'
    });
  }

  // A handler for delivering messages to runtime.
  registerActionHandler(handler) {
    if (typeof handler === 'function') {
      this._actionHandler = handler;
    }
  }

  /***************************/
  /* PPAPIViewport Interface */
  /***************************/

  addView(canvas) {
    this._canvasContainer.appendChild(canvas);
  }

  clearView() {
    this._canvasContainer.innerHTML = '';
  }

  getBoundingClientRect() {
    return this._canvasContainer.getBoundingClientRect();
  }

  is(element) {
    return element == this._viewportController;
  }

  bindUIEvent(type, listener) {
    if (type == 'fullscreenchange' || type == 'MozScrolledAreaChanged') {
      // These two events won't be bound on a target because they should be
      // fully controlled by UI layer, and we'll manually trigger the resize
      // event once needed.
      return;
    }
    switch(type) {
      case 'resize':
        this._runtimeOnResizedListener.push(listener);
        break;
      default:
        this._getEventTarget(type).addEventListener(type, listener);
    }
  }

  unbindUIEvent(type, listener) {
    if (type == 'fullscreenchange' || type == 'MozScrolledAreaChanged') {
      return;
    }
    switch(type) {
      case 'resize':
        this._runtimeOnResizedListener =
          this._runtimeOnResizedListener.filter(item => item != listener);
        break;
      default:
        this._getEventTarget(type).removeEventListener(type, listener);
    }
  }

  setCursor(cursor) {
    this._viewportController.style.cursor = cursor;
  }

  getScrollOffset() {
    return {
      x: this._viewportController.scrollLeft,
      y: this._viewportController.scrollTop
    };
  }

  // Notified by runtime.
  notify(message) {
    switch (message.type) {
      case 'loadProgress':
        this._updateProgress(message.progress);
        break;
      case 'documentDimensions':
        this._setDocumentDimensions(message);
        break;
    }
  }
}
