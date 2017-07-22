/* globals log, util, catcher, inlineSelectionCss, callBackground, assertIsTrusted, assertIsBlankDocument */

"use strict";

this.ui = (function() { // eslint-disable-line no-unused-vars
  let exports = {};
  const SAVE_BUTTON_HEIGHT = 50;

  const { watchFunction } = catcher;

  // The <body> tag itself can have margins and offsets, which need to be used when
  // setting the position of the boxEl.
  function getBodyRect() {
    if (getBodyRect.cached) {
      return getBodyRect.cached;
    }
    let rect = document.body.getBoundingClientRect();
    let cached = {
      top: rect.top + window.scrollY,
      bottom: rect.bottom + window.scrollY,
      left: rect.left + window.scrollX,
      right: rect.right + window.scrollX
    };
    // FIXME: I can't decide when this is necessary
    // *not* necessary on http://patriciogonzalezvivo.com/2015/thebookofshaders/
    // (actually causes mis-selection there)
    // *is* necessary on http://atirip.com/2015/03/17/sorry-sad-state-of-matrix-transforms-in-browsers/
    cached = {top: 0, bottom: 0, left: 0, right: 0};
    getBodyRect.cached = cached;
    return cached;
  }

  exports.isHeader = function(el) {
    while (el) {
      if (el.classList &&
          (el.classList.contains("myshots-button") ||
           el.classList.contains("visible") ||
           el.classList.contains("full-page"))) {
        return true;
      }
      el = el.parentNode;
    }
    return false;
  }

  let substitutedCss = inlineSelectionCss.replace(/MOZ_EXTENSION([^\"]+)/g, (match, filename) => {
    return browser.extension.getURL(filename);
  });

  function makeEl(tagName, className) {
    if (!iframe.document()) {
      throw new Error("Attempted makeEl before iframe was initialized");
    }
    let el = iframe.document().createElement(tagName);
    if (className) {
      el.className = className;
    }
    return el;
  }

  function onResize() {
    if (this.sizeTracking.windowDelayer) {
      clearTimeout(this.sizeTracking.windowDelayer);
    }
    this.sizeTracking.windowDelayer = setTimeout(watchFunction(() => {
      this.updateElementSize(true);
    }), 50);
  }

  let iframeSelection = exports.iframeSelection = {
    element: null,
    addClassName: "",
    sizeTracking: {
      timer: null,
      windowDelayer: null,
      lastHeight: null,
      lastWidth: null
    },
    document: null,
    display(installHandlerOnDocument) {
      return new Promise((resolve, reject) => {
        if (!this.element) {
          this.element = document.createElement("iframe");
          this.element.src = browser.extension.getURL("blank.html");
          this.element.id = "firefox-screenshots-selection-iframe";
          this.element.style.display = "none";
          this.element.style.zIndex = "99999999999";
          this.element.style.border = "none";
          this.element.style.position = "absolute";
          this.element.style.top = "0";
          this.element.style.left = "0";
          this.element.style.margin = "0";
          this.element.scrolling = "no";
          this.updateElementSize();
          this.element.addEventListener("load", watchFunction(() => {
            this.document = this.element.contentDocument;
            assertIsBlankDocument(this.document);
            this.document.documentElement.innerHTML = `
               <head>
                <style>${substitutedCss}</style>
                <title></title>
               </head>
               <body></body>`;
            installHandlerOnDocument(this.document);
            if (this.addClassName) {
              this.document.body.className = this.addClassName;
            }
            this.document.documentElement.dir = browser.i18n.getMessage("@@bidi_dir");
            this.document.documentElement.lang = browser.i18n.getMessage("@@ui_locale");
            resolve();
          }), {once: true});
          document.body.appendChild(this.element);
        } else {
          resolve();
        }
      });
    },

    hide() {
      this.element.style.display = "none";
      this.stopSizeWatch();
    },

    unhide() {
      this.updateElementSize();
      this.element.style.display = "";
      this.initSizeWatch();
      this.element.focus();
    },

    updateElementSize(force) {
      // Note: if someone sizes down the page, then the iframe will keep the
      // document from naturally shrinking.  We use force to temporarily hide
      // the element so that we can tell if the document shrinks
      const visible = this.element.style.display !== "none";
      if (force && visible) {
        this.element.style.display = "none";
      }
      let height = Math.max(
        document.documentElement.clientHeight,
        document.body.clientHeight,
        document.documentElement.scrollHeight,
        document.body.scrollHeight,
        window.innerHeight);
      if (height !== this.sizeTracking.lastHeight) {
        this.sizeTracking.lastHeight = height;
        this.element.style.height = height + "px";
      }
      let width = Math.max(
        document.documentElement.clientWidth,
        document.body.clientWidth,
        document.documentElement.scrollWidth,
        document.body.scrollWidth,
        window.innerWidth);
      if (width !== this.sizeTracking.lastWidth) {
        this.sizeTracking.lastWidth = width;
        this.element.style.width = width + "px";
      }
      if (force && visible) {
        this.element.style.display = "";
      }
    },

    initSizeWatch() {
      this.stopSizeWatch();
      this.sizeTracking.timer = setInterval(watchFunction(this.updateElementSize.bind(this)), 2000);
      window.addEventListener("resize", this.onResize, true);
    },

    stopSizeWatch() {
      if (this.sizeTracking.timer) {
        clearTimeout(this.sizeTracking.timer);
        this.sizeTracking.timer = null;
      }
      if (this.sizeTracking.windowDelayer) {
        clearTimeout(this.sizeTracking.windowDelayer);
        this.sizeTracking.windowDelayer = null;
      }
      this.sizeTracking.lastHeight = this.sizeTracking.lastWidth = null;
      window.removeEventListener("resize", this.onResize, true);
    },

    getElementFromPoint(x, y) {
      this.element.style.pointerEvents = "none";
      let el;
      try {
        el = document.elementFromPoint(x, y);
      } finally {
        this.element.style.pointerEvents = "";
      }
      return el;
    },

    remove() {
      this.stopSizeWatch();
      util.removeNode(this.element);
      this.element = this.document = null;
    }
  };

  iframeSelection.onResize = watchFunction(assertIsTrusted(onResize.bind(iframeSelection)), true);

  let iframePreSelection = exports.iframePreSelection = {
    element: null,
    document: null,
    sizeTracking: {
      windowDelayer: null
    },
    display(installHandlerOnDocument, standardOverlayCallbacks) {
      return new Promise((resolve, reject) => {
        if (!this.element) {
          this.element = document.createElement("iframe");
          this.element.src = browser.extension.getURL("blank.html");
          this.element.id = "firefox-screenshots-preselection-iframe";
          this.element.style.zIndex = "99999999999";
          this.element.style.border = "none";
          this.element.style.position = "fixed";
          this.element.style.top = "0";
          this.element.style.left = "0";
          this.element.style.margin = "0";
          this.element.scrolling = "no";
          this.updateElementSize();
          this.element.addEventListener("load", watchFunction(() => {
            this.document = this.element.contentDocument;
            assertIsBlankDocument(this.document)
            this.document.documentElement.innerHTML = `
               <head>
                <style>${substitutedCss}</style>
                <title></title>
               </head>
               <body>
                 <div class="preview-overlay">
                   <div class="fixed-container">
                     <div class="face-container">
                       <div class="eye left"><div class="eyeball"></div></div>
                       <div class="eye right"><div class="eyeball"></div></div>
                       <div class="face"></div>
                     </div>
                     <div class="preview-instructions"></div>
                     <div class="myshots-all-buttons-container">
                       <button class="myshots-button myshots-link" tabindex="1"></button>
                       <div class="spacer"></div>
                       <button class="myshots-button visible" tabindex="2"></button>
                       <button class="myshots-button full-page" tabindex="3"></button>
                     </div>
                   </div>
                 </div>
               </body>`;
            installHandlerOnDocument(this.document);
            if (this.addClassName) {
              this.document.body.className = this.addClassName;
            }
            this.document.documentElement.dir = browser.i18n.getMessage("@@bidi_dir");
            this.document.documentElement.lang = browser.i18n.getMessage("@@ui_locale");
            const overlay = this.document.querySelector(".preview-overlay");
            overlay.querySelector(".preview-instructions").textContent = browser.i18n.getMessage("screenshotInstructions");
            overlay.querySelector(".myshots-link").textContent = browser.i18n.getMessage("myShotsLink");
            overlay.querySelector(".visible").textContent = browser.i18n.getMessage("saveScreenshotVisibleArea");
            overlay.querySelector(".full-page").textContent = browser.i18n.getMessage("saveScreenshotFullPage");
            overlay.querySelector(".myshots-button").addEventListener(
              "click", watchFunction(assertIsTrusted(standardOverlayCallbacks.onOpenMyShots)));
            overlay.querySelector(".visible").addEventListener(
              "click", watchFunction(assertIsTrusted(standardOverlayCallbacks.onClickVisible)));
            overlay.querySelector(".full-page").addEventListener(
              "click", watchFunction(assertIsTrusted(standardOverlayCallbacks.onClickFullPage)));
            resolve();
          }), {once: true});
          document.body.appendChild(this.element);
          this.unhide();
        } else {
          resolve();
        }
      });
    },

    updateElementSize() {
      if (!this.element) {
        // This can happen if the selector is unloaded during the resize adjustment
        // time-delay
        return;
      }
      this.element.style.height = window.innerHeight + "px";
      this.element.style.width = window.innerWidth + "px";
    },

    hide() {
      window.removeEventListener("scroll", watchFunction(assertIsTrusted(this.onScroll)));
      window.removeEventListener("resize", this.onResize, true);
      if (this.element) {
        this.element.style.display = "none";
      }
    },

    unhide() {
      this.updateElementSize();
      window.addEventListener("scroll", watchFunction(assertIsTrusted(this.onScroll)));
      window.addEventListener("resize", this.onResize, true);
      this.element.style.display = "";
      this.element.focus();
    },

    onScroll() {
      exports.HoverBox.hide();
    },

    getElementFromPoint(x, y) {
      this.element.style.pointerEvents = "none";
      let el;
      try {
        el = document.elementFromPoint(x, y);
      } finally {
        this.element.style.pointerEvents = "";
      }
      return el;
    },

    remove() {
      this.hide();
      util.removeNode(this.element);
      this.element = null;
      this.document = null;
    }
  };

  iframePreSelection.onResize = watchFunction(onResize.bind(iframePreSelection), true);

  let iframe = exports.iframe = {
    currentIframe: iframePreSelection,
    display(installHandlerOnDocument, standardOverlayCallbacks) {
      return iframeSelection.display(installHandlerOnDocument)
        .then(() => iframePreSelection.display(installHandlerOnDocument, standardOverlayCallbacks));
    },

    hide() {
      this.currentIframe.hide();
    },

    unhide() {
      this.currentIframe.unhide();
    },

    getElementFromPoint(x, y) {
      return this.currentIframe.getElementFromPoint(x, y);
    },

    remove() {
      iframeSelection.remove();
      iframePreSelection.remove();
    },

    document() {
      return this.currentIframe.document;
    },

    useSelection() {
      if (this.currentIframe === iframePreSelection) {
        this.hide();
      }
      this.currentIframe = iframeSelection;
      this.unhide();
    },

    usePreSelection() {
      if (this.currentIframe === iframeSelection) {
        this.hide();
      }
      this.currentIframe = iframePreSelection;
      this.unhide();
    }
  };

  let movements = ["topLeft", "top", "topRight", "left", "right", "bottomLeft", "bottom", "bottomRight"];

  /** Creates the selection box */
  exports.Box = {

    display(pos, callbacks) {
      this._createEl();
      if (callbacks !== undefined && callbacks.cancel) {
        // We use onclick here because we don't want addEventListener
        // to add multiple event handlers to the same button
        this.cancel.onclick = watchFunction(assertIsTrusted(callbacks.cancel));
        this.cancel.style.display = "";
      } else {
        this.cancel.style.display = "none";
      }
      if (callbacks !== undefined && callbacks.save) {
        // We use onclick here because we don't want addEventListener
        // to add multiple event handlers to the same button
        this.save.removeAttribute("disabled");
        this.save.onclick = watchFunction(assertIsTrusted((e) => {
          this.save.setAttribute("disabled", "true");
          callbacks.save(e);
        }));
        this.save.style.display = "";
      } else {
        this.save.style.display = "none";
      }
      if (callbacks !== undefined && callbacks.download) {
        this.download.removeAttribute("disabled");
        this.download.onclick = watchFunction(assertIsTrusted((e) => {
          this.download.setAttribute("disabled", true);
          callbacks.download(e);
          e.preventDefault();
          e.stopPropagation();
          return false;
        }));
        this.download.style.display = "";
      } else {
        this.download.style.display = "none";
      }
      let bodyRect = getBodyRect();
      // Note, document.documentElement.scrollHeight is zero on some strange pages (such as the page created when you load an image):
      let docHeight = Math.max(document.documentElement.scrollHeight || 0, document.body.scrollHeight);
      let docWidth = Math.max(document.documentElement.scrollWidth, document.body.scrollWidth);

      let winBottom = window.innerHeight;
      let pageYOffset = window.pageYOffset;

      if ((pos.right - pos.left) < 78 || (pos.bottom - pos.top) < 78) {
        this.el.classList.add("small-selection");
      } else {
        this.el.classList.remove("small-selection");
      }

      // if the selection bounding box is w/in SAVE_BUTTON_HEIGHT px of the bottom of
      // the window, flip controls into the box
      if (pos.bottom > ((winBottom + pageYOffset) - SAVE_BUTTON_HEIGHT)) {
        this.el.classList.add("bottom-selection");
      } else {
        this.el.classList.remove("bottom-selection");
      }

      if (pos.right < 200) {
        this.el.classList.add("left-selection");
      } else {
        this.el.classList.remove("left-selection");
      }
      this.el.style.top = (pos.top - bodyRect.top) + "px";
      this.el.style.left = (pos.left - bodyRect.left) + "px";
      this.el.style.height = (pos.bottom - pos.top - bodyRect.top) + "px";
      this.el.style.width = (pos.right - pos.left - bodyRect.left) + "px";
      this.bgTop.style.top = "0px";
      this.bgTop.style.height = (pos.top - bodyRect.top) + "px";
      this.bgTop.style.left = "0px";
      this.bgTop.style.width = docWidth + "px";
      this.bgBottom.style.top = (pos.bottom - bodyRect.top) + "px";
      this.bgBottom.style.height = docHeight - (pos.bottom - bodyRect.top) + "px";
      this.bgBottom.style.left = "0px";
      this.bgBottom.style.width = docWidth + "px";
      this.bgLeft.style.top = (pos.top - bodyRect.top) + "px";
      this.bgLeft.style.height = pos.bottom - pos.top + "px";
      this.bgLeft.style.left = "0px";
      this.bgLeft.style.width = (pos.left - bodyRect.left) + "px";
      this.bgRight.style.top = (pos.top - bodyRect.top) + "px";
      this.bgRight.style.height = pos.bottom - pos.top + "px";
      this.bgRight.style.left = (pos.right - bodyRect.left) + "px";
      this.bgRight.style.width = docWidth - (pos.right - bodyRect.left) + "px";

      if (!(this.isElementInViewport(this.buttons))) {
        this.cancel.style.position = this.download.style.position = this.save.style.position = "fixed";
        this.cancel.style.left = (pos.left - bodyRect.left - 50) + "px";
        this.download.style.left = ((pos.left - bodyRect.left - 100)) + "px";
        this.save.style.left = ((pos.left - bodyRect.left) - 190) + "px";
        this.cancel.style.top = this.download.style.top = this.save.style.top = (pos.top - bodyRect.top) + "px";
      } else {
        this.cancel.style.position = this.download.style.position = this.save.style.position = "initial";
        this.cancel.style.top = this.download.style.top = this.save.style.top = 0;
        this.cancel.style.left = this.download.style.left = this.save.style.left = 0;
      }
    },

    remove() {
      for (let name of ["el", "bgTop", "bgLeft", "bgRight", "bgBottom"]) {
        if (name in this) {
          util.removeNode(this[name]);
          this[name] = null;
        }
      }
    },

    _createEl() {
      let boxEl = this.el;
      if (boxEl) {
        return;
      }
      boxEl = makeEl("div", "highlight");
      let buttons = makeEl("div", "highlight-buttons");
      let cancel = makeEl("button", "highlight-button-cancel");
      cancel.title = browser.i18n.getMessage("cancelScreenshot");
      buttons.appendChild(cancel);
      let download = makeEl("button", "highlight-button-download");
      download.title = browser.i18n.getMessage("downloadScreenshot");
      buttons.appendChild(download);
      let save = makeEl("button", "highlight-button-save");
      save.textContent = browser.i18n.getMessage("saveScreenshotSelectedArea");
      save.title = browser.i18n.getMessage("saveScreenshotSelectedArea");
      buttons.appendChild(save);
      this.buttons = buttons;
      this.cancel = cancel;
      this.download = download;
      this.save = save;
      boxEl.appendChild(buttons);
      for (let name of movements) {
        let elTarget = makeEl("div", "mover-target direction-" + name);
        let elMover = makeEl("div", "mover");
        elTarget.appendChild(elMover);
        boxEl.appendChild(elTarget);
      }
      this.bgTop = makeEl("div", "bghighlight");
      iframe.document().body.appendChild(this.bgTop);
      this.bgLeft = makeEl("div", "bghighlight");
      iframe.document().body.appendChild(this.bgLeft);
      this.bgRight = makeEl("div", "bghighlight");
      iframe.document().body.appendChild(this.bgRight);
      this.bgBottom = makeEl("div", "bghighlight");
      iframe.document().body.appendChild(this.bgBottom);
      iframe.document().body.appendChild(boxEl);
      this.el = boxEl;
    },

    draggerDirection(target) {
      while (target) {
        if (target.nodeType == document.ELEMENT_NODE) {
          if (target.classList.contains("mover-target")) {
            for (let name of movements) {
              if (target.classList.contains("direction-" + name)) {
                return name;
              }
            }
            catcher.unhandled(new Error("Surprising mover element"), {element: target.outerHTML});
            log.warn("Got mover-target that wasn't a specific direction");
          }
        }
        target = target.parentNode;
      }
      return null;
    },

    isSelection(target) {
      while (target) {
        if (target.tagName === "BUTTON") {
          return false;
        }
        if (target.nodeType == document.ELEMENT_NODE && target.classList.contains("highlight")) {
          return true;
        }
        target = target.parentNode;
      }
      return false;
    },

    isControl(target) {
      while (target) {
        if (target.nodeType === document.ELEMENT_NODE && target.classList.contains("highlight-buttons")) {
          return true;
        }
        target = target.parentNode;
      }
      return false;
    },

    isElementInViewport(el) {
      let rect = el.getBoundingClientRect();
      return (rect.right <= window.innerWidth);
    },

    clearSaveDisabled() {
      this.save.removeAttribute("disabled");
    },

    el: null,
    boxTopEl: null,
    boxLeftEl: null,
    boxRightEl: null,
    boxBottomEl: null
  };

  exports.HoverBox = {

    el: null,

    display(rect) {
      if (!this.el) {
        this.el = makeEl("div", "hover-highlight");
        iframe.document().body.appendChild(this.el);
      }
      this.el.style.display = "";
      this.el.style.top = (rect.top - 1) + "px";
      this.el.style.left = (rect.left - 1) + "px";
      this.el.style.width = (rect.right - rect.left + 2) + "px";
      this.el.style.height = (rect.bottom - rect.top + 2) + "px";
    },

    hide() {
      if (this.el) {
        this.el.style.display = "none";
      }
    },

    remove() {
      util.removeNode(this.el);
      this.el = null;
    }
  };

  exports.PixelDimensions = {
    el: null,
    xEl: null,
    yEl: null,
    display(xPos, yPos, x, y) {
      if (!this.el) {
        this.el = makeEl("div", "pixel-dimensions");
        this.xEl = makeEl("div");
        this.el.appendChild(this.xEl);
        this.yEl = makeEl("div");
        this.el.appendChild(this.yEl);
        iframe.document().body.appendChild(this.el);
      }
      this.xEl.textContent = x;
      this.yEl.textContent = y;
      this.el.style.top = (yPos + 12) + "px";
      this.el.style.left = (xPos + 12) + "px";
    },
    remove() {
      util.removeNode(this.el);
      this.el = this.xEl = this.yEl = null;
    }
  };

  /** Removes every UI this module creates */
  exports.remove = function() {
    for (let name in exports) {
      if (name.startsWith("iframe")) {
        continue;
      }
      if (typeof exports[name] == "object" && exports[name].remove) {
        exports[name].remove();
      }
    }
    exports.iframe.remove();
  };

  exports.triggerDownload = function(url, filename) {
    return catcher.watchPromise(callBackground("downloadShot", {url, filename}));
  };

  exports.unload = exports.remove;

  return exports;
})();
null;
