/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

// When handling Bookmark View, we use Cartesian coordinate system which is
// different from PDFium engine. Our origin is at bottom-left of every page,
// while PDFium counts y position continuously from the top of page 1.
// Moreover, the coordinate used in PDF.js is scaled by 0.75 for some reason,
// so we keep it here for backward compability.
const PAGE_COORDINATE_RATIO = 0.75;

class Viewport {
  constructor() {
    this._viewerContainer = document.getElementById('viewerContainer');
    this._fullscreenWrapper = document.getElementById('fullscreenWrapper');
    this._canvasContainer = document.getElementById('canvasContainer');
    this._viewportController = document.getElementById('viewportController');
    this._sizer = document.getElementById('sizer');

    this._fullscreenStatus = 'none';
    this._scrollbarWidth = this._getScrollbarWidth();

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

    // If the document is opened with a bookmarkView hash, we save it until
    // the document dimension is revealed to move the view.
    this._initPosition = null;

    this.onProgressChanged = null;
    this.onZoomChanged = null;
    this.onDimensionChanged = null;
    this.onPageChanged = null;
    this.onPasswordRequest = null;

    this._viewportController.addEventListener('scroll', this);
    this._viewportController.addEventListener('copy', this);
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
    if (!this._isValidFitting(newFitting)) {
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

  get fullscreen() {
    return this._fullscreenStatus != 'none';
  }

  set fullscreen(enable) {
    if (this._fullscreenStatus == 'changing' ||
        this._fullscreenStatus == (enable ? 'fullscreen' : 'none')) {
      return;
    }

    // The next step after sending "setFullscreen" will happen in the function
    // "_handleFullscreenChange" triggered by "fullscreenChange" event. The
    // "_fullscreenStatus" will also be reset there. Note that the viewport
    // stops refreshing while in the "changing" status to avoid flickers.
    //
    // XXX: Since we rely on "fullscreenChange" event to reset the status, the
    //      viewport might freeze if, for some reason, the event isn't sent back
    //      and we get stuck in the "changing" status. Not sure if it's the case
    //      we need to worry about though.
    this._fullscreenStatus = 'changing';
    this._doAction({
      type: 'setFullscreen',
      fullscreen: enable
    });
  }

  _isValidFitting(fitting) {
    let VALID_VALUE = ['none', 'auto', 'page-actual', 'page-width', 'page-fit'];
    return VALID_VALUE.includes(fitting);
  }

  _getScrollbarWidth() {
    let div = document.createElement('div');
    div.style.visibility = 'hidden';
    div.style.overflow = 'scroll';
    div.style.width = '50px';
    div.style.height = '50px';
    div.style.position = 'absolute';
    document.body.appendChild(div);
    let result = div.offsetWidth - div.clientWidth;
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

    if (this._initPosition) {
      this._jumpToBookmark(this._initPosition);
      this._initPosition = null;
    } else {
      this._refresh();
    }
  }

  _computeFittingZoom(pageIndex) {
    let newZoom = this._zoom;
    let fitting = this._fitting;

    if (pageIndex === undefined) {
      pageIndex = this._page;
    }

    if (fitting == 'none' || pageIndex < 0 || pageIndex >= this.pageCount) {
      return newZoom;
    }

    let FITTING_PADDING = 40;
    let MAX_AUTO_ZOOM = 1.25;

    let page = this._pageDimensions[pageIndex];
    let viewportRect = this.fullscreen ?
      this._viewerContainer.getBoundingClientRect() :
      this.getBoundingClientRect();

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

    if (this.fullscreen) {
      let pageDimension = this._pageDimensions[newPage];
      let newZoom = this._computeFittingZoom(newPage);

      this._fullscreenWrapper.style.width =
        (pageDimension.width * newZoom) + 'px';
      this._fullscreenWrapper.style.height =
        (pageDimension.height * newZoom) + 'px';

      if (newZoom != this._zoom) {
        this._setZoom(newZoom);
      }
      this._notifyRuntimeOfResized();
    }

    this._setPosition(
      this._pageDimensions[newPage].x * this._zoom,
      this._pageDimensions[newPage].y * this._zoom
    );
  }

  _updateCanvasSize() {
    let hasScrollbars = this._documentHasVisibleScrollbars(this._zoom);
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

  _handleFullscreenChange(fullscreen) {
    // Set status to "changing" again in case it isn't triggered by setter.
    this._fullscreenStatus = 'changing';
    this._viewerContainer.classList.toggle('pdfPresentationMode', fullscreen);

    // XXX: DOM elements' size changing hasn't taken place when fullscreenChange
    //      event is triggered in our setup, so "setTimeout" is necessary to get
    //      the exact size. The 100ms delay is set based on try-and-error. We
    //      might need to find a proper way to know when exactly the resizing is
    //      done.
    setTimeout(() => {
      let currentPage = this._page;

      if (fullscreen) {
        this._previousZoom = this._zoom;
        this._previousFitting = this._fitting;
        this._fitting = 'page-fit';
        // No need to call "_setZoom" here because we will deal with zooming
        // case in the "_setPage" below.
      } else {
        this._zoom = this._previousZoom;
        this._fitting = this._previousFitting;
        this._setZoom(this._computeFittingZoom());
      }

      this._fullscreenStatus = fullscreen ? 'fullscreen' : 'none';

      // Reset position to the beginning of the current page.
      this._setPage(currentPage);
      this._refresh();
    }, 100);
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
    if (this._fullscreenStatus == 'changing') {
      return;
    }

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
      zoom: this._zoom,
      // FIXME Since Chromium improves pinch-zoom for PDF. PostMessage of type
      //       viewport takes an addition parameter pinchPhase. We workaround
      //       here by adding a pinchPhase of value 0 to make sure that viewing
      //       pdf works normally. More details about pinch-zoom please refer
      //       to chromium revision: 6e1abbfb2450eedddb1ab128be1b31cc93104e41
      pinchPhase: 0
    });

    let newPage = this._getMostVisiblePage();
    if (newPage != this._page) {
      this._page = newPage;
      if (typeof this.onPageChanged === 'function') {
        this.onPageChanged(newPage);
      }
    }
  }

  _copyToClipboard(text) {
    if (!text) {
      return;
    }
    const gClipboardHelper = Cc["@mozilla.org/widget/clipboardhelper;1"]
                                .getService(Ci.nsIClipboardHelper);
    gClipboardHelper.copyString(text);
  }

  _getPageCoordinate() {
    let currentPos = this._nextPosition || this.getScrollOffset();
    let pageDimension = this._pageDimensions[this._page];

    let pageOrigin = {
      x: pageDimension.x,
      y: pageDimension.y + pageDimension.height
    };
    currentPos.x /= this._zoom;
    currentPos.y /= this._zoom;

    let pageCoordinate = {
      x: Math.round((currentPos.x - pageOrigin.x) * PAGE_COORDINATE_RATIO),
      y: Math.round((pageOrigin.y - currentPos.y) * PAGE_COORDINATE_RATIO)
    };
    return pageCoordinate;
  }

  _getScreenCooridnate(pageNo, pageX, pageY) {
    let pageDimension = this._pageDimensions[pageNo];
    // Both pageX and pageY are omittable, and in this case, we assume the most
    // top and left corner of the page as their default values.
    pageX = Number.isInteger(pageX) ? pageX : 0;
    pageY = Number.isInteger(pageY) ? pageY : pageDimension.height *
                                              PAGE_COORDINATE_RATIO;
    pageX /= PAGE_COORDINATE_RATIO;
    pageY /= PAGE_COORDINATE_RATIO;

    let pageOrigin = {
      x: pageDimension.x,
      y: pageDimension.y + pageDimension.height
    };

    return {
      x: Math.round((pageX - pageOrigin.x) * this._zoom),
      y: Math.round((pageOrigin.y - pageY) * this._zoom)
    };
  }

  /**
   * @param hash
   *        contains page and zoom parameters which should be in the following
   *        format: page={page}&zoom={scale},{x},{y}
   *        for example the following hashes are valid:
   *        page=1&zoom=auto,100,100
   *        page=3&zoom=300,10,-50
   */
  _jumpToBookmark(hash) {
    let params = {};
    hash.split('&').forEach(param => {
      let [name, value] = param.split('=');
      params[name.toLowerCase()] = value.toLowerCase();
    });

    let pageNo = parseInt(params.page, 10);
    pageNo = Number.isNaN(pageNo) ? this._page : pageNo;
    pageNo = Math.max(0, Math.min(this.pageCount - 1, pageNo - 1));

    params.zoom = (typeof(params.zoom) == 'string') ? params.zoom : "";
    let [scale, pageX, pageY] = params.zoom.split(',');
    pageX = parseInt(pageX, 10);
    pageY = parseInt(pageY, 10);

    if (this._isValidFitting(scale)) {
      this._fitting = scale;
    } else {
      this._fitting = 'none';
      let zoom = parseFloat(scale);
      zoom = (Number.isNaN(zoom) || zoom <= 0) ? 100 : zoom;
      this._zoom = zoom / 100;
    }

    let screenPos = this._getScreenCooridnate(pageNo, pageX, pageY);
    this._setPosition(screenPos.x, screenPos.y);
    this._setZoom(this._computeFittingZoom());
    this._refresh();
  }

  _handleHashChange(hash) {
    if (!this._documentDimensions) {
      this._initPosition = hash;
    } else {
      this._jumpToBookmark(hash);
    }
  }

  _handleCommand(name) {
    switch(name) {
      case 'cmd_selectAll':
        this._doAction({
          type: 'selectAll'
        });
        break;
      case 'cmd_copy':
        this._doAction({
          type: 'getSelectedText'
        })
        break;
    }
  }

  verifyPassword(password) {
    this._doAction({
      type: 'getPasswordComplete',
      password: password
    });
  }

  handleEvent(evt) {
    switch(evt.type) {
      case 'resize':
        this.invokeResize();
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

  invokeResize() {
    if (this._fullscreenStatus == 'changing') {
      return;
    }
    this._resize();
    this._notifyRuntimeOfResized();
    this._refresh();
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

  save() {
    this._doAction({
      type: 'save'
    });
  }

  // A handler for delivering messages to runtime.
  registerActionHandler(handler) {
    if (typeof handler === 'function') {
      this._actionHandler = handler;
    }
  }

  createBookmarkHash() {
    let pagePosition = this._getPageCoordinate();
    let scale = this._fitting == 'none' ?
                  Math.round(this._zoom * 100) :
                  this._fitting;
    let hash = "page=" + (this._page + 1) +
               "&zoom=" + scale +
               "," + pagePosition.x +
               "," + pagePosition.y;
    this._doAction({
      type: 'setHash',
      hash: hash
    })
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
      case 'fullscreenChange':
        this._handleFullscreenChange(message.fullscreen);
        break;
      case 'getPassword':
        this.onPasswordRequest && this.onPasswordRequest();
        break;
      case 'getSelectedTextReply':
        // For now this message is used only by text copy so we handle just
        // that case.
        this._copyToClipboard(message.selectedText);
        break;
      case 'hashChange':
        this._handleHashChange(message.hash);
        break;
      case 'command':
        this._handleCommand(message.name);
        break;
    }
  }
}
