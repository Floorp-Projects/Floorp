/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file is loaded into the browser window scope.
/* eslint-env mozilla/browser-window */

/**
 * Tab previews utility, produces thumbnails
 */
var tabPreviews = {
  get aspectRatio() {
    let { PageThumbUtils } = ChromeUtils.importESModule(
      "resource://gre/modules/PageThumbUtils.sys.mjs"
    );
    let [width, height] = PageThumbUtils.getThumbnailSize(window);
    delete this.aspectRatio;
    return (this.aspectRatio = height / width);
  },

  /**
   * Get the stored thumbnail URL for a given page URL and wait up to 1s for it
   * to load. If the browser is discarded and there is no stored thumbnail, the
   * image URL will fail to load and this method will return null after 1s.
   * Callers should handle this case by doing nothing or using a fallback image.
   * @param {String} uri The page URL.
   * @returns {Promise<Image|null>}
   */
  loadImage: async function tabPreviews_loadImage(uri) {
    let img = new Image();
    img.src = PageThumbs.getThumbnailURL(uri);
    if (img.complete && img.naturalWidth) {
      return img;
    }
    return new Promise(resolve => {
      const controller = new AbortController();
      img.addEventListener(
        "load",
        () => {
          clearTimeout(timeout);
          controller.abort();
          resolve(img);
        },
        { signal: controller.signal }
      );
      const timeout = setTimeout(() => {
        controller.abort();
        resolve(null);
      }, 1000);
    });
  },

  /**
   * For a given tab, retrieve a preview thumbnail (a canvas or an image) from
   * storage or capture a new one. If the tab's URL has changed since the
   * previous call, the thumbnail will be regenerated.
   * @param {MozTabbrowserTab} aTab The tab to get a preview for.
   * @returns {Promise<HTMLCanvasElement|Image|null>} Resolves to...
   * @resolves {HTMLCanvasElement} If a thumbnail can NOT be captured and stored
   *   for the tab, or if the tab is still loading, a snapshot is taken and
   *   returned as a canvas. It may be cached as a canvas (separately from
   *   thumbnail storage) in aTab.__thumbnail if the tab is finished loading. If
   *   the snapshot CAN be stored as a thumbnail, the snapshot is converted to a
   *   blob image and drawn in the returned canvas, but the image is added to
   *   thumbnail storage and cached in aTab.__thumbnail.
   * @resolves {Image} A cached blob image from a previous thumbnail capture.
   *   e.g. <img src="moz-page-thumb://thumbnails/?url=foo.com&revision=bar">
   * @resolves {null} If a thumbnail cannot be captured for any reason (e.g.
   *   because the tab is discarded) and there is no cached/stored thumbnail.
   */
  get: async function tabPreviews_get(aTab) {
    let browser = aTab.linkedBrowser;
    let uri = browser.currentURI.spec;

    // Invalidate the cached thumbnail since the tab has changed.
    if (aTab.__thumbnail_lastURI && aTab.__thumbnail_lastURI != uri) {
      aTab.__thumbnail = null;
      aTab.__thumbnail_lastURI = null;
    }

    // A cached thumbnail (not from thumbnail storage) is available.
    if (aTab.__thumbnail) {
      return aTab.__thumbnail;
    }

    // This means the browser is discarded. Try to load a stored thumbnail, and
    // use a fallback style otherwise.
    if (!browser.browsingContext) {
      return this.loadImage(uri);
    }

    // Don't cache or store the thumbnail if the tab is still loading.
    return this.capture(aTab, !aTab.hasAttribute("busy"));
  },

  /**
   * For a given tab, capture a preview thumbnail (a canvas), optionally cache
   * it in aTab.__thumbnail, and possibly store it in thumbnail storage.
   * @param {MozTabbrowserTab} aTab The tab to capture a preview for.
   * @param {Boolean} aShouldCache Cache/store the captured thumbnail?
   * @returns {Promise<HTMLCanvasElement|null>} Resolves to...
   * @resolves {HTMLCanvasElement} A snapshot of the tab's content. If the
   *   snapshot is safe for storage and aShouldCache is true, the snapshot is
   *   converted to a blob image, stored and cached, and drawn in the returned
   *   canvas. The thumbnail can then be recovered even if the browser is
   *   discarded. Otherwise, the canvas itself is cached in aTab.__thumbnail.
   * @resolves {null} If a fatal exception occurred during thumbnail capture.
   */
  capture: async function tabPreviews_capture(aTab, aShouldCache) {
    let browser = aTab.linkedBrowser;
    let uri = browser.currentURI.spec;
    let canvas = PageThumbs.createCanvas(window);
    const doStore = await PageThumbs.shouldStoreThumbnail(browser);

    if (doStore && aShouldCache) {
      await PageThumbs.captureAndStore(browser);
      let img = await this.loadImage(uri);
      if (img) {
        // Cache the stored blob image for future use.
        aTab.__thumbnail = img;
        aTab.__thumbnail_lastURI = uri;
        // Draw the stored blob image in the canvas.
        canvas.getContext("2d").drawImage(img, 0, 0);
      } else {
        canvas = null;
      }
    } else {
      try {
        await PageThumbs.captureToCanvas(browser, canvas);
        if (aShouldCache) {
          // Cache the canvas itself for future use.
          aTab.__thumbnail = canvas;
          aTab.__thumbnail_lastURI = uri;
        }
      } catch (error) {
        console.error(error);
        canvas = null;
      }
    }

    return canvas;
  },
};

var tabPreviewPanelHelper = {
  opening(host) {
    host.panel.hidden = false;

    var handler = this._generateHandler(host);
    host.panel.addEventListener("popupshown", handler);
    host.panel.addEventListener("popuphiding", handler);

    host._prevFocus = document.commandDispatcher.focusedElement;
  },
  _generateHandler(host) {
    var self = this;
    return function listener(event) {
      if (event.target == host.panel) {
        host.panel.removeEventListener(event.type, listener);
        self["_" + event.type](host);
      }
    };
  },
  _popupshown(host) {
    if ("setupGUI" in host) {
      host.setupGUI();
    }
  },
  _popuphiding(host) {
    if ("suspendGUI" in host) {
      host.suspendGUI();
    }

    if (host._prevFocus) {
      Services.focus.setFocus(
        host._prevFocus,
        Ci.nsIFocusManager.FLAG_NOSCROLL
      );
      host._prevFocus = null;
    } else {
      gBrowser.selectedBrowser.focus();
    }

    if (host.tabToSelect) {
      gBrowser.selectedTab = host.tabToSelect;
      host.tabToSelect = null;
    }
  },
};

/**
 * Ctrl-Tab panel
 */
var ctrlTab = {
  maxTabPreviews: 7,
  get panel() {
    delete this.panel;
    return (this.panel = document.getElementById("ctrlTab-panel"));
  },
  get showAllButton() {
    delete this.showAllButton;
    this.showAllButton = document.createXULElement("button");
    this.showAllButton.id = "ctrlTab-showAll";
    this.showAllButton.addEventListener("mouseover", this);
    this.showAllButton.addEventListener("command", this);
    this.showAllButton.addEventListener("click", this);
    document
      .getElementById("ctrlTab-showAll-container")
      .appendChild(this.showAllButton);
    return this.showAllButton;
  },
  get previews() {
    delete this.previews;
    this.previews = [];
    let previewsContainer = document.getElementById("ctrlTab-previews");
    for (let i = 0; i < this.maxTabPreviews; i++) {
      let preview = this._makePreview();
      previewsContainer.appendChild(preview);
      this.previews.push(preview);
    }
    this.previews.push(this.showAllButton);
    return this.previews;
  },
  get keys() {
    var keys = {};
    ["close", "find", "selectAll"].forEach(function (key) {
      keys[key] = document
        .getElementById("key_" + key)
        .getAttribute("key")
        .toLocaleLowerCase()
        .charCodeAt(0);
    });
    delete this.keys;
    return (this.keys = keys);
  },
  _selectedIndex: 0,
  get selected() {
    return this._selectedIndex < 0
      ? document.activeElement
      : this.previews[this._selectedIndex];
  },
  get isOpen() {
    return (
      this.panel.state == "open" || this.panel.state == "showing" || this._timer
    );
  },
  get tabCount() {
    return this.tabList.length;
  },
  get tabPreviewCount() {
    return Math.min(this.maxTabPreviews, this.tabCount);
  },

  get tabList() {
    return this._recentlyUsedTabs;
  },

  init: function ctrlTab_init() {
    if (!this._recentlyUsedTabs) {
      this._initRecentlyUsedTabs();
      this._init(true);
    }
  },

  uninit: function ctrlTab_uninit() {
    if (this._recentlyUsedTabs) {
      this._recentlyUsedTabs = null;
      this._init(false);
    }
  },

  prefName: "browser.ctrlTab.sortByRecentlyUsed",
  readPref: function ctrlTab_readPref() {
    var enable =
      Services.prefs.getBoolPref(this.prefName) &&
      !Services.prefs.getBoolPref(
        "browser.ctrlTab.disallowForScreenReaders",
        false
      );

    if (enable) {
      this.init();
    } else {
      this.uninit();
    }
  },
  observe(aSubject, aTopic, aPrefName) {
    this.readPref();
  },

  _makePreview() {
    let preview = document.createXULElement("button");
    preview.className = "ctrlTab-preview";
    preview.setAttribute("pack", "center");
    preview.setAttribute("flex", "1");
    preview.addEventListener("mouseover", this);
    preview.addEventListener("command", this);
    preview.addEventListener("click", this);

    let previewInner = document.createXULElement("vbox");
    previewInner.className = "ctrlTab-preview-inner";
    preview.appendChild(previewInner);

    let canvas = (preview._canvas = document.createXULElement("hbox"));
    canvas.className = "ctrlTab-canvas";
    previewInner.appendChild(canvas);

    let faviconContainer = document.createXULElement("hbox");
    faviconContainer.className = "ctrlTab-favicon-container";
    previewInner.appendChild(faviconContainer);

    let favicon = (preview._favicon = document.createXULElement("image"));
    favicon.className = "ctrlTab-favicon";
    faviconContainer.appendChild(favicon);

    let label = (preview._label = document.createXULElement("label"));
    label.className = "ctrlTab-label plain";
    label.setAttribute("crop", "end");
    previewInner.appendChild(label);

    return preview;
  },

  updatePreviews: function ctrlTab_updatePreviews() {
    for (let i = 0; i < this.previews.length; i++) {
      this.updatePreview(this.previews[i], this.tabList[i]);
    }

    document.l10n.setAttributes(
      this.showAllButton,
      "tabbrowser-ctrl-tab-list-all-tabs",
      { tabCount: this.tabCount }
    );
    this.showAllButton.hidden = !gTabsPanel.canOpen;
  },

  updatePreview: function ctrlTab_updatePreview(aPreview, aTab) {
    if (aPreview == this.showAllButton) {
      return;
    }

    aPreview._tab = aTab;

    if (aTab) {
      let canvas = aPreview._canvas;
      let canvasWidth = this.canvasWidth;
      let canvasHeight = this.canvasHeight;
      canvas.setAttribute("width", canvasWidth);
      canvas.style.minWidth = canvasWidth + "px";
      canvas.style.maxWidth = canvasWidth + "px";
      canvas.style.minHeight = canvasHeight + "px";
      canvas.style.maxHeight = canvasHeight + "px";
      tabPreviews
        .get(aTab)
        .then(img => {
          switch (aPreview._tab) {
            case aTab:
              this._clearCanvas(canvas);
              if (img) {
                canvas.appendChild(img);
              }
              break;
            case null:
              // The preview panel is not open, so don't render anything.
              this._clearCanvas(canvas);
              break;
            // If the tab exists but it has changed since updatePreview was
            // called, the preview will likely be handled by a later
            // updatePreview call, e.g. on TabAttrModified.
          }
        })
        .catch(error => console.error(error));

      aPreview._label.setAttribute("value", aTab.label);
      aPreview.setAttribute("tooltiptext", aTab.label);
      if (aTab.image) {
        aPreview._favicon.setAttribute("src", aTab.image);
      } else {
        aPreview._favicon.removeAttribute("src");
      }
      aPreview.hidden = false;
    } else {
      this._clearCanvas(aPreview._canvas);
      aPreview.hidden = true;
      aPreview._label.removeAttribute("value");
      aPreview.removeAttribute("tooltiptext");
      aPreview._favicon.removeAttribute("src");
    }
  },

  // Remove previous preview images from the canvas box.
  _clearCanvas(canvas) {
    while (canvas.firstElementChild) {
      canvas.firstElementChild.remove();
    }
  },

  advanceFocus: function ctrlTab_advanceFocus(aForward) {
    let selectedIndex = this.previews.indexOf(this.selected);
    do {
      selectedIndex += aForward ? 1 : -1;
      if (selectedIndex < 0) {
        selectedIndex = this.previews.length - 1;
      } else if (selectedIndex >= this.previews.length) {
        selectedIndex = 0;
      }
    } while (this.previews[selectedIndex].hidden);

    if (this._selectedIndex == -1) {
      // Focus is already in the panel.
      this.previews[selectedIndex].focus();
    } else {
      this._selectedIndex = selectedIndex;
    }

    if (this.previews[selectedIndex]._tab) {
      gBrowser.warmupTab(this.previews[selectedIndex]._tab);
    }

    if (this._timer) {
      clearTimeout(this._timer);
      this._timer = null;
      this._openPanel();
    }
  },

  _mouseOverFocus: function ctrlTab_mouseOverFocus(aPreview) {
    if (this._trackMouseOver) {
      aPreview.focus();
    }
  },

  pick: function ctrlTab_pick(aPreview) {
    if (!this.tabCount) {
      return;
    }

    var select = aPreview || this.selected;

    if (select == this.showAllButton) {
      this.showAllTabs("ctrltab-all-tabs-button");
    } else {
      this.close(select._tab);
    }
  },

  showAllTabs: function ctrlTab_showAllTabs(aEntrypoint = "unknown") {
    this.close();
    gTabsPanel.showAllTabsPanel(null, aEntrypoint);
  },

  remove: function ctrlTab_remove(aPreview) {
    if (aPreview._tab) {
      gBrowser.removeTab(aPreview._tab);
    }
  },

  attachTab: function ctrlTab_attachTab(aTab, aPos) {
    // If the tab is hidden, don't add it to the list unless it's selected
    // (Normally hidden tabs would be unhidden when selected, but that doesn't
    // happen for Firefox View).
    if (aTab.closing || (aTab.hidden && !aTab.selected)) {
      return;
    }

    // If the tab is already in the list, remove it before re-inserting it.
    this.detachTab(aTab);

    if (aPos == 0) {
      this._recentlyUsedTabs.unshift(aTab);
    } else if (aPos) {
      this._recentlyUsedTabs.splice(aPos, 0, aTab);
    } else {
      this._recentlyUsedTabs.push(aTab);
    }
  },

  detachTab: function ctrlTab_detachTab(aTab) {
    var i = this._recentlyUsedTabs.indexOf(aTab);
    if (i >= 0) {
      this._recentlyUsedTabs.splice(i, 1);
    }
  },

  open: function ctrlTab_open() {
    if (this.isOpen) {
      return;
    }

    this.canvasWidth = Math.ceil(
      (screen.availWidth * 0.85) / this.maxTabPreviews
    );
    this.canvasHeight = Math.round(this.canvasWidth * tabPreviews.aspectRatio);
    this.updatePreviews();
    this._selectedIndex = 1;
    gBrowser.warmupTab(this.selected._tab);

    // Add a slight delay before showing the UI, so that a quick
    // "ctrl-tab" keypress just flips back to the MRU tab.
    this._timer = setTimeout(() => {
      this._timer = null;
      this._openPanel();
    }, 200);
  },

  _openPanel: function ctrlTab_openPanel() {
    tabPreviewPanelHelper.opening(this);

    let width = Math.min(
      screen.availWidth * 0.99,
      this.canvasWidth * 1.25 * this.tabPreviewCount
    );
    this.panel.style.width = width + "px";
    var estimateHeight = this.canvasHeight * 1.25 + 75;
    this.panel.openPopupAtScreen(
      screen.availLeft + (screen.availWidth - width) / 2,
      screen.availTop + (screen.availHeight - estimateHeight) / 2,
      false
    );
  },

  close: function ctrlTab_close(aTabToSelect) {
    if (!this.isOpen) {
      return;
    }

    if (this._timer) {
      clearTimeout(this._timer);
      this._timer = null;
      this.suspendGUI();
      if (aTabToSelect) {
        gBrowser.selectedTab = aTabToSelect;
      }
      return;
    }

    this.tabToSelect = aTabToSelect;
    this.panel.hidePopup();
  },

  setupGUI: function ctrlTab_setupGUI() {
    this.selected.focus();
    this._selectedIndex = -1;

    // Track mouse movement after a brief delay so that the item that happens
    // to be under the mouse pointer initially won't be selected unintentionally.
    this._trackMouseOver = false;
    setTimeout(
      function (self) {
        if (self.isOpen) {
          self._trackMouseOver = true;
        }
      },
      0,
      this
    );
  },

  suspendGUI: function ctrlTab_suspendGUI() {
    for (let preview of this.previews) {
      this.updatePreview(preview, null);
    }
  },

  onKeyDown(event) {
    let action = ShortcutUtils.getSystemActionForEvent(event);
    if (action != ShortcutUtils.CYCLE_TABS) {
      return;
    }

    event.preventDefault();
    event.stopPropagation();

    if (this.isOpen) {
      this.advanceFocus(!event.shiftKey);
      return;
    }

    if (event.shiftKey) {
      this.showAllTabs("shift-tab");
      return;
    }

    Services.els.addSystemEventListener(document, "keyup", this, false);

    let tabs = gBrowser.visibleTabs;
    if (tabs.length > 2) {
      this.open();
    } else if (tabs.length == 2) {
      let index = tabs[0].selected ? 1 : 0;
      gBrowser.selectedTab = tabs[index];
    }
  },

  onKeyPress(event) {
    if (!this.isOpen || !event.ctrlKey) {
      return;
    }

    event.preventDefault();
    event.stopPropagation();

    if (event.keyCode == event.DOM_VK_DELETE) {
      this.remove(this.selected);
      return;
    }

    switch (event.charCode) {
      case this.keys.close:
        this.remove(this.selected);
        break;
      case this.keys.find:
        this.showAllTabs("ctrltab-key-find");
        break;
      case this.keys.selectAll:
        this.showAllTabs("ctrltab-key-selectAll");
        break;
    }
  },

  removeClosingTabFromUI: function ctrlTab_removeClosingTabFromUI(aTab) {
    if (this.tabCount == 2) {
      this.close();
      return;
    }

    this.updatePreviews();

    if (this.selected.hidden) {
      this.advanceFocus(false);
    }
    if (this.selected == this.showAllButton) {
      this.advanceFocus(false);
    }

    // If the current tab is removed, another tab can steal our focus.
    if (aTab.selected && this.panel.state == "open") {
      setTimeout(
        function (selected) {
          selected.focus();
        },
        0,
        this.selected
      );
    }
  },

  handleEvent: function ctrlTab_handleEvent(event) {
    switch (event.type) {
      case "SSWindowRestored":
        this._initRecentlyUsedTabs();
        break;
      case "TabAttrModified":
        // tab attribute modified (i.e. label, busy, image)
        // update preview only if tab attribute modified in the list
        if (
          event.detail.changed.some((elem, ind, arr) =>
            ["label", "busy", "image"].includes(elem)
          )
        ) {
          for (let i = this.previews.length - 1; i >= 0; i--) {
            if (
              this.previews[i]._tab &&
              this.previews[i]._tab == event.target
            ) {
              this.updatePreview(this.previews[i], event.target);
              break;
            }
          }
        }
        break;
      case "TabSelect":
        this.attachTab(event.target, 0);
        // If the previous tab was hidden (e.g. Firefox View), remove it from
        // the list when it's deselected.
        let previousTab = event.detail.previousTab;
        if (previousTab.hidden) {
          this.detachTab(previousTab);
        }
        break;
      case "TabOpen":
        this.attachTab(event.target, 1);
        break;
      case "TabClose":
        this.detachTab(event.target);
        if (this.isOpen) {
          this.removeClosingTabFromUI(event.target);
        }
        break;
      case "TabHide":
        this.detachTab(event.target);
        break;
      case "TabShow":
        this.attachTab(event.target);
        this._sortRecentlyUsedTabs();
        break;
      case "keydown":
        this.onKeyDown(event);
        break;
      case "keypress":
        this.onKeyPress(event);
        break;
      case "keyup":
        // During cycling tabs, we avoid sending keyup event to content document.
        event.preventDefault();
        event.stopPropagation();

        if (event.keyCode === event.DOM_VK_CONTROL) {
          Services.els.removeSystemEventListener(
            document,
            "keyup",
            this,
            false
          );

          if (this.isOpen) {
            this.pick();
          }
        }
        break;
      case "popupshowing":
        if (event.target.id == "menu_viewPopup") {
          document.getElementById("menu_showAllTabs").hidden =
            !gTabsPanel.canOpen;
        }
        break;
      case "mouseover":
        this._mouseOverFocus(event.currentTarget);
        break;
      case "command":
        this.pick(event.currentTarget);
        break;
      case "click":
        if (event.button == 1) {
          this.remove(event.currentTarget);
        } else if (AppConstants.platform == "macosx" && event.button == 2) {
          // Control+click is a right click on macOS, but in this case we want
          // to handle it like a left click.
          this.pick(event.currentTarget);
        }
        break;
    }
  },

  filterForThumbnailExpiration(aCallback) {
    // Save a few more thumbnails than we actually display, so that when tabs
    // are closed, the previews we add instead still get thumbnails.
    const extraThumbnails = 3;
    const thumbnailCount = Math.min(
      this.tabPreviewCount + extraThumbnails,
      this.tabCount
    );

    let urls = [];
    for (let i = 0; i < thumbnailCount; i++) {
      urls.push(this.tabList[i].linkedBrowser.currentURI.spec);
    }

    aCallback(urls);
  },
  _sortRecentlyUsedTabs() {
    this._recentlyUsedTabs.sort(
      (tab1, tab2) => tab2.lastAccessed - tab1.lastAccessed
    );
  },
  _initRecentlyUsedTabs() {
    this._recentlyUsedTabs = Array.prototype.filter.call(
      gBrowser.tabs,
      tab => !tab.closing && !tab.hidden
    );
    this._sortRecentlyUsedTabs();
  },

  _init: function ctrlTab__init(enable) {
    var toggleEventListener = enable
      ? "addEventListener"
      : "removeEventListener";

    window[toggleEventListener]("SSWindowRestored", this);

    var tabContainer = gBrowser.tabContainer;
    tabContainer[toggleEventListener]("TabOpen", this);
    tabContainer[toggleEventListener]("TabAttrModified", this);
    tabContainer[toggleEventListener]("TabSelect", this);
    tabContainer[toggleEventListener]("TabClose", this);
    tabContainer[toggleEventListener]("TabHide", this);
    tabContainer[toggleEventListener]("TabShow", this);

    if (enable) {
      Services.els.addSystemEventListener(document, "keydown", this, false);
    } else {
      Services.els.removeSystemEventListener(document, "keydown", this, false);
    }
    document[toggleEventListener]("keypress", this);
    gBrowser.tabbox.handleCtrlTab = !enable;

    if (enable) {
      PageThumbs.addExpirationFilter(this);
    } else {
      PageThumbs.removeExpirationFilter(this);
    }

    // If we're not running, hide the "Show All Tabs" menu item,
    // as Shift+Ctrl+Tab will be handled by the tab bar.
    document.getElementById("menu_showAllTabs").hidden = !enable;
    document
      .getElementById("menu_viewPopup")
      [toggleEventListener]("popupshowing", this);
  },
};
