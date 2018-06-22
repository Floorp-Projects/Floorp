/* globals log, util, catcher, inlineSelectionCss, callBackground, assertIsTrusted, assertIsBlankDocument, buildSettings blobConverters */

"use strict";

this.ui = (function() { // eslint-disable-line no-unused-vars
  const exports = {};
  const SAVE_BUTTON_HEIGHT = 50;

  const { watchFunction } = catcher;

  exports.isHeader = function(el) {
    while (el) {
      if (el.classList &&
          (el.classList.contains("myshots-button") ||
           el.classList.contains("visible") ||
           el.classList.contains("full-page") ||
           el.classList.contains("cancel-shot"))) {
        return true;
      }
      el = el.parentNode;
    }
    return false;
  };

  const substitutedCss = inlineSelectionCss.replace(/MOZ_EXTENSION([^"]+)/g, (match, filename) => {
    return browser.extension.getURL(filename);
  });

  function makeEl(tagName, className) {
    if (!iframe.document()) {
      throw new Error("Attempted makeEl before iframe was initialized");
    }
    const el = iframe.document().createElement(tagName);
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

  function localizeText(doc) {
    const els = doc.querySelectorAll("[data-l10n-id], [data-l10n-title]");
    for (const el of els) {
      const id = el.getAttribute("data-l10n-id");
      if (id) {
        const text = browser.i18n.getMessage(id);
        el.textContent = text;
      }
      const title = el.getAttribute("data-l10n-title");
      if (title) {
        const titleText = browser.i18n.getMessage(title);
        const sanitized = titleText && titleText.replace("&", "&amp;")
                                              .replace('"', "&quot;")
                                              .replace("<", "&lt;")
                                              .replace(">", "&gt;");
        el.setAttribute("title", sanitized);
      }
    }
  }

  function highContrastCheck(win) {
    const doc = win.document;
    const el = doc.createElement("div");
    el.style.backgroundImage = "url('#')";
    el.style.display = "none";
    doc.body.appendChild(el);
    const computed = win.getComputedStyle(el);
    doc.body.removeChild(el);
    // When Windows is in High Contrast mode, Firefox replaces background
    // image URLs with the string "none".
    return (computed && computed.backgroundImage === "none");
  }

  const isDownloadOnly = exports.isDownloadOnly = function() {
    return window.downloadOnly;
  };

  // the download notice is rendered in iframes that match the document height
  // or the window height. If parent iframe matches window height, pass in true
  function renderDownloadNotice(initAtBottom = false) {
    const notice = makeEl("table", "notice");
    notice.innerHTML = `
      <div class="notice-tooltip">
        <p data-l10n-id="downloadOnlyDetails"></p>
        <ul>
          <li data-l10n-id="downloadOnlyDetailsPrivate"></li>
          <li data-l10n-id="downloadOnlyDetailsNeverRemember"></li>
          <li data-l10n-id="downloadOnlyDetailsESR"></li>
          <li data-l10n-id="downloadOnlyDetailsNoUploadPref"></li>
        </ul>
      </div>
      <tbody>
        <tr class="notice-wrapper">
          <td class="notice-content" data-l10n-id="downloadOnlyNotice"></td>
          <td class="notice-help"></td>
        </tr>
      <tbody>`;
    localizeText(notice);
    if (initAtBottom) {
      notice.style.bottom = "10px";
    }
    return notice;
  }

  function initializeIframe() {
    const el = document.createElement("iframe");
    el.src = browser.extension.getURL("blank.html");
    el.style.zIndex = "99999999999";
    el.style.border = "none";
    el.style.top = "0";
    el.style.left = "0";
    el.style.margin = "0";
    el.scrolling = "no";
    el.style.clip = "auto";
    return el;
  }

  const iframeSelection = exports.iframeSelection = {
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
          this.element = initializeIframe();
          this.element.id = "firefox-screenshots-selection-iframe";
          this.element.style.display = "none";
          this.element.style.setProperty("position", "absolute", "important");
          this.updateElementSize();
          this.element.addEventListener("load", watchFunction(() => {
            this.document = this.element.contentDocument;
            assertIsBlankDocument(this.document);
            // eslint-disable-next-line no-unsanitized/property
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
      this.element.style.display = "block";
      catcher.watchPromise(callBackground("sendEvent", "internal", "unhide-selection-frame"));
      if (highContrastCheck(this.element.contentWindow)) {
        this.element.contentDocument.body.classList.add("hcm");
      }
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
      const height = Math.max(
        document.documentElement.clientHeight,
        document.body.clientHeight,
        document.documentElement.scrollHeight,
        document.body.scrollHeight);
      if (height !== this.sizeTracking.lastHeight) {
        this.sizeTracking.lastHeight = height;
        this.element.style.height = height + "px";
      }
      // Do not use window.innerWidth since that includes the width of the
      // scroll bar.
      const width = Math.max(
        document.documentElement.clientWidth,
        document.body.clientWidth,
        document.documentElement.scrollWidth,
        document.body.scrollWidth);
      if (width !== this.sizeTracking.lastWidth) {
        this.sizeTracking.lastWidth = width;
        this.element.style.width = width + "px";
        // Since this frame has an absolute position relative to the parent
        // document, if the parent document's body has a relative position and
        // left and/or top not at 0, then the left and/or top of the parent
        // document's body is not at (0, 0) of the viewport. That makes the
        // frame shifted relative to the viewport. These margins negates that.
        if (window.getComputedStyle(document.body).position === "relative") {
          const docBoundingRect = document.documentElement.getBoundingClientRect();
          const bodyBoundingRect = document.body.getBoundingClientRect();
          this.element.style.marginLeft = `-${bodyBoundingRect.left - docBoundingRect.left}px`;
          this.element.style.marginTop = `-${bodyBoundingRect.top - docBoundingRect.top}px`;
        }
      }
      if (force && visible) {
        this.element.style.display = "block";
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

  const iframePreSelection = exports.iframePreSelection = {
    element: null,
    document: null,
    display(installHandlerOnDocument, standardOverlayCallbacks) {
      return new Promise((resolve, reject) => {
        if (!this.element) {
          this.element = initializeIframe();
          this.element.id = "firefox-screenshots-preselection-iframe";
          this.element.style.setProperty("position", "fixed", "important");
          this.element.style.width = "100%";
          this.element.style.height = "100%";
          this.element.addEventListener("load", watchFunction(() => {
            this.document = this.element.contentDocument;
            assertIsBlankDocument(this.document);
            // eslint-disable-next-line no-unsanitized/property
            this.document.documentElement.innerHTML = `
               <head>
                <style>${substitutedCss}</style>
                <title></title>
               </head>
               <body>
                 <div class="preview-overlay precision-cursor">
                   <div class="fixed-container">
                     <div class="face-container">
                       <div class="eye left"><div class="eyeball"></div></div>
                       <div class="eye right"><div class="eyeball"></div></div>
                       <div class="face"></div>
                     </div>
                     <div class="preview-instructions" data-l10n-id="screenshotInstructions"></div>
                     <button class="cancel-shot">${browser.i18n.getMessage("cancelScreenshot")}</button>
                     <div class="myshots-all-buttons-container">
                       ${isDownloadOnly() ? "" : `
                         <button class="myshots-button" tabindex="1" data-l10n-id="myShotsLink"></button>
                         <div class="spacer"></div>
                       `}
                       <button class="visible" tabindex="2" data-l10n-id="saveScreenshotVisibleArea"></button>
                       <button class="full-page" tabindex="3" data-l10n-id="saveScreenshotFullPage"></button>
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
            localizeText(this.document);
            if (!(isDownloadOnly())) {
              overlay.querySelector(".myshots-button").addEventListener(
                "click", watchFunction(assertIsTrusted(standardOverlayCallbacks.onOpenMyShots)));
            }
            overlay.querySelector(".visible").addEventListener(
              "click", watchFunction(assertIsTrusted(standardOverlayCallbacks.onClickVisible)));
            overlay.querySelector(".full-page").addEventListener(
              "click", watchFunction(assertIsTrusted(standardOverlayCallbacks.onClickFullPage)));
            overlay.querySelector(".cancel-shot").addEventListener(
              "click", watchFunction(assertIsTrusted(standardOverlayCallbacks.onClickCancel)));
            resolve();
          }), {once: true});
          document.body.appendChild(this.element);
        } else {
          resolve();
        }
      });
    },

    hide() {
      window.removeEventListener("scroll", watchFunction(assertIsTrusted(this.onScroll)));
      window.removeEventListener("resize", this.onResize, true);
      if (this.element) {
        this.element.style.display = "none";
      }
    },

    unhide() {
      window.addEventListener("scroll", watchFunction(assertIsTrusted(this.onScroll)));
      window.addEventListener("resize", this.onResize, true);
      this.element.style.display = "block";
      catcher.watchPromise(callBackground("sendEvent", "internal", "unhide-preselection-frame"));
      if (highContrastCheck(this.element.contentWindow)) {
        this.element.contentDocument.body.classList.add("hcm");
      }
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

  const iframePreview = exports.iframePreview = {
    element: null,
    document: null,
    display(installHandlerOnDocument, standardOverlayCallbacks) {
      return new Promise((resolve, reject) => {
        if (!this.element) {
          this.element = initializeIframe();
          this.element.id = "firefox-screenshots-preview-iframe";
          this.element.style.display = "none";
          this.element.style.setProperty("position", "fixed", "important");
          this.element.style.height = "100%";
          this.element.style.width = "100%";
          this.element.onload = watchFunction(() => {
            this.document = this.element.contentDocument;
            // eslint-disable-next-line no-unsanitized/property
            this.document.documentElement.innerHTML = `
              <head>
                <style>${substitutedCss}</style>
                <title></title>
              </head>
              <body>
                <div class="preview-overlay">
                  <div class="preview-image">
                    <div class="preview-buttons">
                      <button class="highlight-button-cancel"
                        data-l10n-title="cancelScreenshot"><img
                        src="${browser.extension.getURL("icons/cancel.svg")}" /></button>
                      <button class="highlight-button-copy"
                        data-l10n-title="copyScreenshot"><img
                        src="${browser.extension.getURL("icons/copy.svg")}" /></button>
                      ${isDownloadOnly() ?
                        `<button class="highlight-button-download download-only-button"
                          data-l10n-title="downloadScreenshot"><img
                          src="${browser.extension.getURL("icons/download.svg")}"
                          />${browser.i18n.getMessage("downloadScreenshot")}</button>` :
                        `<button class="highlight-button-download"
                          data-l10n-title="downloadScreenshot"><img
                          src="${browser.extension.getURL("icons/download.svg")}" /></button>
                         <button class="preview-button-save"
                          data-l10n-title="saveScreenshotSelectedArea"><img
                          src="${browser.extension.getURL("icons/cloud.svg")}"
                          />${browser.i18n.getMessage("saveScreenshotSelectedArea")}</button>`
                      }
                    </div>
                  </div>
                  <div class="loader" style="display:none">
                    <div class="loader-inner"></div>
                  </div>
                </div>
              </body>`;
            installHandlerOnDocument(this.document);
            this.document.documentElement.dir = browser.i18n.getMessage("@@bidi_dir");
            this.document.documentElement.lang = browser.i18n.getMessage("@@ui_locale");
            localizeText(this.document);
            const overlay = this.document.querySelector(".preview-overlay");
            if (isDownloadOnly()) {
              overlay.appendChild(renderDownloadNotice(true));
            } else {
              overlay.querySelector(".preview-button-save").addEventListener(
                "click", watchFunction(assertIsTrusted(standardOverlayCallbacks.onSavePreview)));
            }
            overlay.querySelector(".highlight-button-copy").addEventListener(
              "click", watchFunction(assertIsTrusted(standardOverlayCallbacks.onCopyPreview)));
            overlay.querySelector(".highlight-button-download").addEventListener(
              "click", watchFunction(assertIsTrusted(standardOverlayCallbacks.onDownloadPreview)));
            overlay.querySelector(".highlight-button-cancel").addEventListener(
              "click", watchFunction(assertIsTrusted(standardOverlayCallbacks.cancel)));
            resolve();
          });
          document.body.appendChild(this.element);
        } else {
          resolve();
        }
      });
    },

    hide() {
      if (this.element) {
        this.element.style.display = "none";
      }
    },

    unhide() {
      this.element.style.display = "block";
      catcher.watchPromise(callBackground("sendEvent", "internal", "unhide-preview-frame"));
      this.element.focus();
    },

    showLoader() {
      this.document.body.querySelector(".preview-image").style.display = "none";
      this.document.body.querySelector(".notice").style.display = "none";
      this.document.body.querySelector(".loader").style.display = "";
    },

    remove() {
      this.hide();
      util.removeNode(this.element);
      this.element = null;
      this.document = null;
    }
  };

  iframePreSelection.onResize = watchFunction(onResize.bind(iframePreSelection), true);

  const iframe = exports.iframe = {
    currentIframe: iframePreSelection,
    display(installHandlerOnDocument, standardOverlayCallbacks) {
      return iframeSelection.display(installHandlerOnDocument)
        .then(() => iframePreSelection.display(installHandlerOnDocument, standardOverlayCallbacks))
        .then(() => iframePreview.display(installHandlerOnDocument, standardOverlayCallbacks));
    },

    hide() {
      this.currentIframe.hide();
    },

    unhide() {
      this.currentIframe.unhide();
    },

    showLoader() {
      if (this.currentIframe.showLoader) {
        this.currentIframe.showLoader();
        this.currentIframe.unhide();
      }
    },

    getElementFromPoint(x, y) {
      return this.currentIframe.getElementFromPoint(x, y);
    },

    remove() {
      iframeSelection.remove();
      iframePreSelection.remove();
      iframePreview.remove();
    },

    document() {
      return this.currentIframe.document;
    },

    useSelection() {
      if (this.currentIframe === iframePreSelection || this.currentIframe === iframePreview) {
        this.hide();
      }
      this.currentIframe = iframeSelection;
      this.unhide();
    },

    usePreSelection() {
      if (this.currentIframe === iframeSelection || this.currentIframe === iframePreview) {
        this.hide();
      }
      this.currentIframe = iframePreSelection;
      this.unhide();
    },

    usePreview() {
      if (this.currentIframe === iframeSelection || this.currentIframe === iframePreSelection) {
        this.hide();
      }
      this.currentIframe = iframePreview;
      this.unhide();
    }
  };

  const movements = ["topLeft", "top", "topRight", "left", "right", "bottomLeft", "bottom", "bottomRight"];

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
      if (callbacks !== undefined && callbacks.save && this.save) {
        // We use onclick here because we don't want addEventListener
        // to add multiple event handlers to the same button
        this.save.removeAttribute("disabled");
        this.save.onclick = watchFunction(assertIsTrusted((e) => {
          this.save.setAttribute("disabled", "true");
          callbacks.save(e);
        }));
        this.save.style.display = "";
      } else if (this.save) {
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
      if (callbacks !== undefined && callbacks.copy) {
        this.copy.removeAttribute("disabled");
        this.copy.onclick = watchFunction(assertIsTrusted((e) => {
          this.copy.setAttribute("disabled", true);
          callbacks.copy(e);
          e.preventDefault();
          e.stopPropagation();
        }));
        this.copy.style.display = "";
      } else {
        this.copy.style.display = "none";
      }

      const winBottom = window.innerHeight;
      const pageYOffset = window.pageYOffset;

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
      this.el.style.top = `${pos.top}px`;
      this.el.style.left = `${pos.left}px`;
      this.el.style.height = `${pos.bottom - pos.top}px`;
      this.el.style.width = `${pos.right - pos.left}px`;
      this.bgTop.style.top = "0px";
      this.bgTop.style.height = `${pos.top}px`;
      this.bgTop.style.left = "0px";
      this.bgTop.style.width = "100%";
      this.bgBottom.style.top = `${pos.bottom}px`;
      this.bgBottom.style.height = "100vh";
      this.bgBottom.style.left = "0px";
      this.bgBottom.style.width = "100%";
      this.bgLeft.style.top = `${pos.top}px`;
      this.bgLeft.style.height = `${pos.bottom - pos.top}px`;
      this.bgLeft.style.left = "0px";
      this.bgLeft.style.width = `${pos.left}px`;
      this.bgRight.style.top = `${pos.top}px`;
      this.bgRight.style.height = `${pos.bottom - pos.top}px`;
      this.bgRight.style.left = `${pos.right}px`;
      this.bgRight.style.width = `${document.body.scrollWidth - pos.right}px`;
      // the download notice is injected into an iframe that matches the document size
      // in order to reposition it on scroll we need to bind an updated positioning
      // function to some window events.
      this.repositionDownloadNotice = () => {
        if (this.downloadNotice) {
          const currentYOffset = window.pageYOffset;
          const currentWinBottom = window.innerHeight;
          this.downloadNotice.style.top = (currentYOffset + currentWinBottom - 60) + "px";
        }
      };

      if (this.downloadNotice) {
        this.downloadNotice.style.top = (pageYOffset + winBottom - 60) + "px";
        // event callbacks are delayed 100ms each to keep from overloading things
        this.windowChangeStop = this.delayExecution(100, this.repositionDownloadNotice);
        window.addEventListener("scroll", watchFunction(assertIsTrusted(this.windowChangeStop)));
        window.addEventListener("resize", watchFunction(assertIsTrusted(this.windowChangeStop)));
      }
    },

    // used to eventually move the download-only warning
    // when a user ends scrolling or ends resizing a window
    delayExecution(delay, cb) {
      let timer;
      return function() {
        if (typeof timer !== "undefined") {
          clearTimeout(timer);
        }
        timer = setTimeout(cb, delay);
      };
    },

    remove() {
      if (this.downloadNotice) {
        window.removeEventListener("scroll", this.windowChangeStop, true);
        window.removeEventListener("resize", this.windowChangeStop, true);
      }
      for (const name of ["el", "bgTop", "bgLeft", "bgRight", "bgBottom", "downloadNotice"]) {
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
      const buttons = makeEl("div", "highlight-buttons");
      const cancel = makeEl("button", "highlight-button-cancel");
      const cancelImg = makeEl("img");
      cancelImg.src = browser.extension.getURL("icons/cancel.svg");
      cancel.title = browser.i18n.getMessage("cancelScreenshot");
      cancel.appendChild(cancelImg);
      buttons.appendChild(cancel);

      const copy = makeEl("button", "highlight-button-copy");
      copy.title = browser.i18n.getMessage("copyScreenshot");
      const copyImg = makeEl("img");
      copyImg.src = browser.extension.getURL("icons/copy.svg");
      copy.appendChild(copyImg);
      buttons.appendChild(copy);

      let download, save;

      if (isDownloadOnly()) {
        download = makeEl("button", "highlight-button-download download-only-button");
        const downloadImg = makeEl("img");
        downloadImg.src = browser.extension.getURL("icons/download.svg");
        download.appendChild(downloadImg);
        download.append(browser.i18n.getMessage("downloadScreenshot"));
        download.title = browser.i18n.getMessage("downloadScreenshot");
      } else {
        download = makeEl("button", "highlight-button-download");
        download.title = browser.i18n.getMessage("downloadScreenshot");
        const downloadImg = makeEl("img");
        downloadImg.src = browser.extension.getURL("icons/download.svg");
        download.appendChild(downloadImg);
        save = makeEl("button", "highlight-button-save");
        const saveImg = makeEl("img");
        saveImg.src = browser.extension.getURL("icons/cloud.svg");
        save.appendChild(saveImg);
        save.append(browser.i18n.getMessage("saveScreenshotSelectedArea"));
        save.title = browser.i18n.getMessage("saveScreenshotSelectedArea");
      }
      buttons.appendChild(download);
      if (save) {
        buttons.appendChild(save);
      }
      this.cancel = cancel;
      this.download = download;
      this.copy = copy;
      this.save = save;
      boxEl.appendChild(buttons);
      for (const name of movements) {
        const elTarget = makeEl("div", "mover-target direction-" + name);
        const elMover = makeEl("div", "mover");
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
      if (isDownloadOnly()) {
        this.downloadNotice = renderDownloadNotice();
        iframe.document().body.appendChild(this.downloadNotice);
      }
      this.el = boxEl;
    },

    draggerDirection(target) {
      while (target) {
        if (target.nodeType === document.ELEMENT_NODE) {
          if (target.classList.contains("mover-target")) {
            for (const name of movements) {
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
        if (target.nodeType === document.ELEMENT_NODE && target.classList.contains("highlight")) {
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

    clearSaveDisabled() {
      if (!this.save) {
        // Happens if we try to remove the disabled status after the worker
        // has been shut down
        return;
      }
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
      if (this.downloadNotice) {
        this.downloadNotice.display = "none";
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

  exports.Preview = {
    display(dataUrl, showCropWarning) {
      const img = makeEl("IMG");
      const imgBlob = blobConverters.dataUrlToBlob(dataUrl);
      img.src = URL.createObjectURL(imgBlob);
      iframe.document().querySelector(".preview-image").appendChild(img);
      if (showCropWarning && !(isDownloadOnly())) {
        const imageCroppedEl = makeEl("table", "notice");
        imageCroppedEl.style.bottom = "10px";
        imageCroppedEl.innerHTML = `<tbody>
          <tr class="notice-wrapper">
            <td class="notice-content"></td>
          </tr>
        </tbody>`;
        const contentCell = imageCroppedEl.getElementsByTagName("td");
        contentCell[0].textContent = browser.i18n.getMessage("imageCroppedWarning", buildSettings.maxImageHeight);
        iframe.document().querySelector(".preview-overlay").appendChild(imageCroppedEl);
      }
    }
  };

  /** Removes every UI this module creates */
  exports.remove = function() {
    for (const name in exports) {
      if (name.startsWith("iframe")) {
        continue;
      }
      if (typeof exports[name] === "object" && exports[name].remove) {
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
