'use strict';

class PresentationController {
  constructor(viewport) {
    this._viewport = viewport;

    viewport.onFullscreenChange = this._onFullscreenChange.bind(this);

    this._wheelTimestamp = 0;
    this._wheelDelta = 0;
  }

  _onFullscreenChange(isFullscreen) {
    if (isFullscreen) {
      this._viewport.bindUIEvent('click', this);
      this._viewport.bindUIEvent('wheel', this);
      this._viewport.bindUIEvent('mousedown', this, true);
      this._viewport.bindUIEvent('mousemove', this, true);
    } else {
      this._viewport.unbindUIEvent('click', this);
      this._viewport.unbindUIEvent('wheel', this);
      this._viewport.unbindUIEvent('mousedown', this, true);
      this._viewport.unbindUIEvent('mousemove', this, true);
    }
  }

  _normalizeWheelEventDelta(evt) {
    let delta = Math.sqrt(evt.deltaX * evt.deltaX + evt.deltaY * evt.deltaY);
    let angle = Math.atan2(evt.deltaY, evt.deltaX);
    if (-0.25 * Math.PI < angle && angle < 0.75 * Math.PI) {
      // All that is left-up oriented has to change the sign.
      delta = -delta;
    }

    let MOUSE_PIXELS_PER_LINE = 30;
    let MOUSE_LINES_PER_PAGE = 30;

    // Converts delta to per-page units
    if (evt.deltaMode === WheelEvent.DOM_DELTA_PIXEL) {
      delta /= MOUSE_PIXELS_PER_LINE * MOUSE_LINES_PER_PAGE;
    } else if (evt.deltaMode === WheelEvent.DOM_DELTA_LINE) {
      delta /= MOUSE_LINES_PER_PAGE;
    }
    return delta;
  }

  _handleWheel(evt) {
    let delta = this._normalizeWheelEventDelta(evt);

    let WHEEL_COOLDOWN_TIME = 50;
    let PAGE_SWITCH_THRESHOLD = 0.1;

    let currentTime = new Date().getTime();
    let storedTime = this._wheelTimestamp;

    // If we've already switched page, avoid accidentally switching again.
    if (currentTime - storedTime < WHEEL_COOLDOWN_TIME) {
      return;
    }
    // If the scroll direction changed, reset the accumulated scroll delta.
    if (this._wheelDelta * delta < 0) {
      this._wheelTimestamp = this._wheelDelta = 0;
    }

    this._wheelDelta += delta;

    if (Math.abs(this._wheelDelta) >= PAGE_SWITCH_THRESHOLD) {
      this._wheelDelta > 0 ? this._viewport.page--
                           : this._viewport.page++;
      this._wheelDelta = 0;
      this._wheelTimestamp = currentTime;
    }
  }

  handleEvent(evt) {
    switch(evt.type) {
      case 'mousedown':
        // We catch mousedown earlier than runtime to detect if user clicked
        // on an internal link, by watching changes of page number between
        // mousedown and mouseup. This is also the main reason we need to invoke
        // page change on click rather than mousedown event.
        this._storedPageNum = this._viewport.page;
        break;
      case 'mousemove':
        if (evt.buttons != 0) {
          // We blocks mousemove when there are buttons clicked to prevent
          // text selection. Blocking all mousemove rubs the cursor out so
          // we just block events when there are buttons being pushed.
          evt.stopImmediatePropagation();
        }
        break;
      case 'click':
        if (this._storedPageNum != this._viewport.page) {
          // User may clicked on an internal link already, so we don't do
          // further page change.
          return;
        }
        this._viewport.page++;
        break;
      case 'wheel':
        this._handleWheel(evt);
        break;
    }
  }
}
