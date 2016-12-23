/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = [
  "Panel",
];

const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Timer.jsm");

this.Panel = function(panelElt, iframeURL) {
  this.p = panelElt;
  this.iframeURL = iframeURL;
  this._initPanel();
  this.urlbar.addEventListener("keydown", this);
  this.urlbar.addEventListener("input", this);
  this._emitQueue = [];
};

this.Panel.prototype = {

  get document() {
    return this.p.ownerDocument;
  },

  get window() {
    return this.document.defaultView;
  },

  get urlbar() {
    return this.window.gURLBar;
  },

  iframe: null,

  get iframeDocument() {
    return this.iframe.contentDocument;
  },

  get iframeWindow() {
    return this.iframe.contentWindow;
  },

  destroy() {
    this.p.destroyAddonIframe(this);
    this.urlbar.removeEventListener("keydown", this);
    this.urlbar.removeEventListener("input", this);
  },

  _initPanel() {
    this.iframe = this.p.initAddonIframe(this, {
      _invalidate: this._invalidate.bind(this),
    });
    if (!this.iframe) {
      // This will be the case when somebody else already owns the iframe.
      // First consumer wins right now.
      return;
    }
    let onLoad = event => {
      this.iframe.removeEventListener("load", onLoad, true);
      this._initIframeContent(event.target.defaultView);
    };
    this.iframe.addEventListener("load", onLoad, true);
    this.iframe.setAttribute("src", this.iframeURL);
  },

  _initIframeContent(win) {
    // Clone the urlbar API functions into the iframe window.
    win = XPCNativeWrapper.unwrap(win);
    let apiInstance = Cu.cloneInto(iframeAPIPrototype, win, {
      cloneFunctions: true,
    });
    apiInstance._panel = this;
    Object.defineProperty(win, "urlbar", {
      get() {
        return apiInstance;
      },
    });
  },

  // This is called by the popup directly.  It overrides the popup's own
  // _invalidate method.
  _invalidate() {
    this._emit("reset");
    this._currentIndex = 0;
    if (this._appendResultTimeout) {
      this.window.clearTimeout(this._appendResultTimeout);
    }
    this._appendCurrentResult();
  },

  // This emulates the popup's own _appendCurrentResult method, except instead
  // of appending results to the popup, it emits "result" events to the iframe.
  _appendCurrentResult() {
    let controller = this.p.mInput.controller;
    for (let i = 0; i < this.p.maxResults; i++) {
      let idx = this._currentIndex;
      if (idx >= this.p._matchCount) {
        break;
      }
      let url = controller.getValueAt(idx);
      let action = this.urlbar._parseActionUrl(url);
      this._emit("result", {
        url: url,
        action: action,
        image: controller.getImageAt(idx),
        title: controller.getCommentAt(idx),
        type: controller.getStyleAt(idx),
        text: controller.searchString.replace(/^\s+/, "").replace(/\s+$/, ""),
      });
      this._currentIndex++;
    }
    if (this._currentIndex < this.p.matchCount) {
      this._appendResultTimeout = this.window.setTimeout(() => {
        this._appendCurrentResult();
      });
    }
  },

  get height() {
    return this.iframe.getBoundingClientRect().height;
  },

  set height(val) {
    this.p.removeAttribute("height");
    this.iframe.style.height = val + "px";
  },

  handleEvent(event) {
    let methName = "_on" + event.type[0].toUpperCase() + event.type.substr(1);
    this[methName](event);
  },

  _onKeydown(event) {
    let emittedEvent = this._emitUrlbarEvent(event);
    if (emittedEvent && emittedEvent.defaultPrevented) {
      event.preventDefault();
      event.stopPropagation();
    }
  },

  _onInput(event) {
    this._emitUrlbarEvent(event);
  },

  _emitUrlbarEvent(event) {
    let properties = [
      "altKey",
      "code",
      "ctrlKey",
      "key",
      "metaKey",
      "shiftKey",
    ];
    let detail = properties.reduce((memo, prop) => {
      memo[prop] = event[prop];
      return memo;
    }, {});
    return this._emit(event.type, detail);
  },

  _emit(eventName, detailObj = null) {
    this._emitQueue.push({
      name: eventName,
      detail: detailObj,
    });
    return this._processEmitQueue();
  },

  _processEmitQueue() {
    if (!this._emitQueue.length) {
      return null;
    }

    // iframe.contentWindow can be undefined right after the iframe is created,
    // even after a number of seconds have elapsed.  Don't know why.  But that's
    // entirely the reason for having a queue instead of simply dispatching
    // events as they're created, unfortunately.
    if (!this.iframeWindow) {
      if (!this._processEmitQueueTimer) {
        this._processEmitQueueTimer = setInterval(() => {
          this._processEmitQueue();
        }, 100);
      }
      return null;
    }

    if (this._processEmitQueueTimer) {
      clearInterval(this._processEmitQueueTimer);
      delete this._processEmitQueueTimer;
    }

    let { name, detail } = this._emitQueue.shift();
    let win = XPCNativeWrapper.unwrap(this.iframeWindow);
    let event = new this.iframeWindow.CustomEvent(name, {
      detail: Cu.cloneInto(detail, win),
      cancelable: true,
    });
    this.iframeWindow.dispatchEvent(event);

    // More events may be queued up, so recurse.  Do it after a turn of the
    // event loop to avoid growing the stack as big as the queue, and to let the
    // caller handle the returned event first.
    setTimeout(() => {
      this._processEmitQueue();
    }, 100);

    return event;
  },
};


// This is the consumer API that's cloned into the iframe window.  Be careful of
// defining static values on this, or even getters and setters (that aren't real
// functions).  The cloning process means that such values are copied by value,
// at the time of cloning, which is probably not what you want.  That's why some
// of these are functions even though it'd be nicer if they were getters and
// setters.
let iframeAPIPrototype = {

  getPanelHeight() {
    return this._panel.height;
  },

  setPanelHeight(val) {
    this._panel.height = val;
  },

  getValue() {
    return this._panel.urlbar.value;
  },

  setValue(val) {
    this._panel.urlbar.value = val;
  },

  getMaxResults() {
    return this._panel.p.maxResults;
  },

  setMaxResults(val) {
    this._panel.p.maxResults = val;
  },

  enter() {
    this._panel.urlbar.handleCommand();
  },
};
