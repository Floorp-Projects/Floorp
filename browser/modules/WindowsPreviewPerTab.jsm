/* vim: se cin sw=2 ts=2 et filetype=javascript :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * This module implements the front end behavior for AeroPeek. Starting in
 * Windows Vista, the taskbar began showing live thumbnail previews of windows
 * when the user hovered over the window icon in the taskbar. Starting with
 * Windows 7, the taskbar allows an application to expose its tabbed interface
 * in the taskbar by showing thumbnail previews rather than the default window
 * preview. Additionally, when a user hovers over a thumbnail (tab or window),
 * they are shown a live preview of the window (or tab + its containing window).
 *
 * In Windows 7, a title, icon, close button and optional toolbar are shown for
 * each preview. This feature does not make use of the toolbar. For window
 * previews, the title is the window title and the icon the window icon. For
 * tab previews, the title is the page title and the page's favicon. In both
 * cases, the close button "does the right thing."
 *
 * The primary objects behind this feature are nsITaskbarTabPreview and
 * nsITaskbarPreviewController. Each preview has a controller. The controller
 * responds to the user's interactions on the taskbar and provides the required
 * data to the preview for determining the size of the tab and thumbnail. The
 * PreviewController class implements this interface. The preview will request
 * the controller to provide a thumbnail or preview when the user interacts with
 * the taskbar. To reduce the overhead of drawing the tab area, the controller
 * implementation caches the tab's contents in a <canvas> element. If no
 * previews or thumbnails have been requested for some time, the controller will
 * discard its cached tab contents.
 *
 * Screen real estate is limited so when there are too many thumbnails to fit
 * on the screen, the taskbar stops displaying thumbnails and instead displays
 * just the title, icon and close button in a similar fashion to previous
 * versions of the taskbar. If there are still too many previews to fit on the 
 * screen, the taskbar resorts to a scroll up and scroll down button pair to let
 * the user scroll through the list of tabs. Since this is undoubtedly
 * inconvenient for users with many tabs, the AeroPeek objects turns off all of
 * the tab previews. This tells the taskbar to revert to one preview per window.
 * If the number of tabs falls below this magic threshold, the preview-per-tab
 * behavior returns. There is no reliable way to determine when the scroll
 * buttons appear on the taskbar, so a magic pref-controlled number determines
 * when this threshold has been crossed.
 */
this.EXPORTED_SYMBOLS = ["AeroPeek"];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/PrivateBrowsingUtils.jsm");

// Pref to enable/disable preview-per-tab
const TOGGLE_PREF_NAME = "browser.taskbar.previews.enable";
// Pref to determine the magic auto-disable threshold
const DISABLE_THRESHOLD_PREF_NAME = "browser.taskbar.previews.max";
// Pref to control the time in seconds that tab contents live in the cache
const CACHE_EXPIRATION_TIME_PREF_NAME = "browser.taskbar.previews.cachetime";

const WINTASKBAR_CONTRACTID = "@mozilla.org/windows-taskbar;1";

////////////////////////////////////////////////////////////////////////////////
//// Various utility properties
XPCOMUtils.defineLazyServiceGetter(this, "ioSvc",
                                   "@mozilla.org/network/io-service;1",
                                   "nsIIOService");
XPCOMUtils.defineLazyServiceGetter(this, "imgTools",
                                   "@mozilla.org/image/tools;1",
                                   "imgITools");
XPCOMUtils.defineLazyServiceGetter(this, "faviconSvc",
                                   "@mozilla.org/browser/favicon-service;1",
                                   "nsIFaviconService");

// nsIURI -> imgIContainer
function _imageFromURI(doc, uri, privateMode, callback) {
  let channel = ioSvc.newChannelFromURI2(uri,
                                         doc,
                                         null,  // aLoadingPrincipal
                                         null,  // aTriggeringPrincipal
                                         Ci.nsILoadInfo.SEC_NORMAL,
                                         Ci.nsIContentPolicy.TYPE_INTERNAL_IMAGE);
  try {
    channel.QueryInterface(Ci.nsIPrivateBrowsingChannel);
    channel.setPrivate(privateMode);
  } catch (e) {
    // Ignore channels which do not support nsIPrivateBrowsingChannel
  }
  NetUtil.asyncFetch(channel, function(inputStream, resultCode) {
    if (!Components.isSuccessCode(resultCode))
      return;
    try {
      let out_img = { value: null };
      imgTools.decodeImageData(inputStream, channel.contentType, out_img);
      callback(out_img.value);
    } catch (e) {
      // We failed, so use the default favicon (only if this wasn't the default
      // favicon).
      let defaultURI = faviconSvc.defaultFavicon;
      if (!defaultURI.equals(uri))
        _imageFromURI(doc, defaultURI, privateMode, callback);
    }
  });
}

// string? -> imgIContainer
function getFaviconAsImage(doc, iconurl, privateMode, callback) {
  if (iconurl)
    _imageFromURI(doc, NetUtil.newURI(iconurl), privateMode, callback);
  else
    _imageFromURI(doc, faviconSvc.defaultFavicon, privateMode, callback);
}

// Snaps the given rectangle to be pixel-aligned at the given scale
function snapRectAtScale(r, scale) {
  let x = Math.floor(r.x * scale);
  let y = Math.floor(r.y * scale);
  let width = Math.ceil((r.x + r.width) * scale) - x;
  let height = Math.ceil((r.y + r.height) * scale) - y;

  r.x = x / scale;
  r.y = y / scale;
  r.width = width / scale;
  r.height = height / scale;
}

////////////////////////////////////////////////////////////////////////////////
//// PreviewController

/*
 * This class manages the behavior of the preview.
 *
 * To give greater performance when drawing, the dirty areas of the content
 * window are tracked and drawn on demand into a canvas of the same size.
 * This provides a great increase in responsiveness when drawing a preview
 * for unchanged (or even only slightly changed) tabs.
 *
 * @param win
 *        The TabWindow (see below) that owns the preview that this controls
 * @param tab
 *        The <tab> that this preview is associated with
 */
function PreviewController(win, tab) {
  this.win = win;
  this.tab = tab;
  this.linkedBrowser = tab.linkedBrowser;
  this.preview = this.win.createTabPreview(this);

  this.linkedBrowser.addEventListener("MozAfterPaint", this, false);
  this.linkedBrowser.addEventListener("resize", this, false);
  this.tab.addEventListener("TabAttrModified", this, false);

  XPCOMUtils.defineLazyGetter(this, "canvasPreview", function () {
    let canvas = this.win.win.document.createElementNS("http://www.w3.org/1999/xhtml", "canvas");
    canvas.mozOpaque = true;
    return canvas;
  });

  XPCOMUtils.defineLazyGetter(this, "dirtyRegion",
    function () {
      let dirtyRegion = Cc["@mozilla.org/gfx/region;1"]
                       .createInstance(Ci.nsIScriptableRegion);
      dirtyRegion.init();
      return dirtyRegion;
    });

  XPCOMUtils.defineLazyGetter(this, "winutils",
    function () {
      let win = tab.linkedBrowser.contentWindow;
      return win.QueryInterface(Ci.nsIInterfaceRequestor)
                .getInterface(Ci.nsIDOMWindowUtils);
  });
}

PreviewController.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsITaskbarPreviewController,
                                         Ci.nsIDOMEventListener]),
  destroy: function () {
    this.tab.removeEventListener("TabAttrModified", this, false);
    this.linkedBrowser.removeEventListener("resize", this, false);
    this.linkedBrowser.removeEventListener("MozAfterPaint", this, false);

    // Break cycles, otherwise we end up leaking the window with everything
    // attached to it.
    delete this.win;
    delete this.preview;
    delete this.dirtyRegion;
  },
  get wrappedJSObject() {
    return this;
  },

  get dirtyRects() {
    let rectstream = this.dirtyRegion.getRects();
    if (!rectstream)
      return [];
    let rects = [];
    for (let i = 0; i < rectstream.length; i+= 4) {
      let r = {x:      rectstream[i],
               y:      rectstream[i+1],
               width:  rectstream[i+2],
               height: rectstream[i+3]};
      rects.push(r);
    }
    return rects;
  },

  // Resizes the canvasPreview to 0x0, essentially freeing its memory.
  // updateCanvasPreview() will detect the size mismatch as a resize event
  // the next time it is called.
  resetCanvasPreview: function () {
    this.resizeCanvasPreview(0, 0);
  },

  resizeCanvasPreview: function (width, height) {
    this.canvasPreview.width = width;
    this.canvasPreview.height = height;
  },

  get wasResizedSinceLastPreview () {
    let bx = this.linkedBrowser.boxObject;
    return bx.width != this.canvasPreview.width ||
           bx.height != this.canvasPreview.height;
  },

  get zoom() {
    // Note that winutils.fullZoom accounts for "quantization" of the zoom factor
    // from nsIContentViewer due to conversion through appUnits.
    // We do -not- want screenPixelsPerCSSPixel here, because that would -also-
    // incorporate any scaling that is applied due to hi-dpi resolution options.
    return this.winutils.fullZoom;
  },

  // Updates the controller's canvas with the parts of the <browser> that need
  // to be redrawn.
  updateCanvasPreview: function () {
    let win = this.linkedBrowser.contentWindow;
    let bx = this.linkedBrowser.boxObject;
    // If we resized then we need to flush layout so that the previews are up
    // to date. Layout flushing for resizes is deferred for background tabs so
    // we may need to force it. (bug 526620)
    let flushLayout = this.wasResizedSinceLastPreview;
    // Check for resize
    if (flushLayout) {
      // Invalidate the entire area and repaint
      this.onTabPaint({left:0, top:0, right:win.innerWidth, bottom:win.innerHeight});
      this.resizeCanvasPreview(bx.width, bx.height);
    }

    // Draw dirty regions
    let ctx = this.canvasPreview.getContext("2d");
    let scale = this.zoom;

    let flags = this.canvasPreviewFlags;
    if (flushLayout)
      flags &= ~Ci.nsIDOMCanvasRenderingContext2D.DRAWWINDOW_DO_NOT_FLUSH;

    // The dirty region may include parts that are offscreen so we clip to the
    // canvas area.
    this.dirtyRegion.intersectRect(0, 0, win.innerWidth, win.innerHeight);
    this.dirtyRects.forEach(function (r) {
      // We need to snap the rectangle to be pixel aligned in the destination
      // coordinate space. Otherwise natively themed widgets might not draw.
      snapRectAtScale(r, scale);
      let x = r.x;
      let y = r.y;
      let width = r.width;
      let height = r.height;

      ctx.save();
      ctx.scale(scale, scale);
      ctx.translate(x, y);
      ctx.drawWindow(win, x, y, width, height, "white", flags);
      ctx.restore();
    });
    this.dirtyRegion.setToRect(0,0,0,0);

    // If we're updating the canvas, then we're in the middle of a peek so
    // don't discard the cache of previews.
    AeroPeek.resetCacheTimer();
  },

  onTabPaint: function (rect) {
    let x = Math.floor(rect.left),
        y = Math.floor(rect.top),
        width = Math.ceil(rect.right) - x,
        height = Math.ceil(rect.bottom) - y;
    this.dirtyRegion.unionRect(x, y, width, height);
  },

  updateTitleAndTooltip: function () {
    let title = this.win.tabbrowser.getWindowTitleForBrowser(this.linkedBrowser);
    this.preview.title = title;
    this.preview.tooltip = title;
  },

  //////////////////////////////////////////////////////////////////////////////
  //// nsITaskbarPreviewController 

  get width() {
    return this.win.width;
  },

  get height() {
    return this.win.height;
  },

  get thumbnailAspectRatio() {
    let boxObject = this.tab.linkedBrowser.boxObject;
    // Avoid returning 0
    let tabWidth = boxObject.width || 1;
    // Avoid divide by 0
    let tabHeight = boxObject.height || 1;
    return tabWidth / tabHeight;
  },

  drawPreview: function (ctx) {
    this.win.tabbrowser.previewTab(this.tab, () => this.previewTabCallback(ctx));

    // We must avoid having the frame drawn around the window. See bug 520807
    return false;
  },

  previewTabCallback: function (ctx) {
    // This will extract the resolution-scale component of the scaling we need,
    // which should be applied to both chrome and content;
    // the page zoom component is applied (to content only) within updateCanvasPreview.
    let scale = this.winutils.screenPixelsPerCSSPixel / this.winutils.fullZoom;
    ctx.save();
    ctx.scale(scale, scale);
    let width = this.win.width;
    let height = this.win.height;
    // Draw our toplevel window
    ctx.drawWindow(this.win.win, 0, 0, width, height, "transparent");

    // XXX (jfkthame): Pending tabs don't seem to draw with the proper scaling
    // unless we use this block of code; but doing this for "normal" (loaded) tabs
    // results in blurry rendering on hidpi systems, so we avoid it if possible.
    // I don't understand why pending and loaded tabs behave differently here...
    // (see bug 857061).
    if (this.tab.hasAttribute("pending")) {
      // Compositor, where art thou?
      // Draw the tab content on top of the toplevel window
      this.updateCanvasPreview();

      let boxObject = this.linkedBrowser.boxObject;
      ctx.translate(boxObject.x, boxObject.y);
      ctx.drawImage(this.canvasPreview, 0, 0);
    }

    ctx.restore();
  },

  drawThumbnail: function (ctx, width, height) {
    this.updateCanvasPreview();

    let scale = width/this.linkedBrowser.boxObject.width;
    ctx.scale(scale, scale);
    ctx.drawImage(this.canvasPreview, 0, 0);

    // Don't draw a frame around the thumbnail
    return false;
  },

  onClose: function () {
    this.win.tabbrowser.removeTab(this.tab);
  },

  onActivate: function () {
    this.win.tabbrowser.selectedTab = this.tab;

    // Accept activation - this will restore the browser window
    // if it's minimized
    return true;
  },

  //// nsIDOMEventListener
  handleEvent: function (evt) {
    switch (evt.type) {
      case "MozAfterPaint":
        if (evt.originalTarget === this.linkedBrowser.contentWindow) {
          let clientRects = evt.clientRects;
          let length = clientRects.length;
          for (let i = 0; i < length; i++) {
            let r = clientRects.item(i);
            this.onTabPaint(r);
          }
        }
        this.preview.invalidate();
        break;
      case "TabAttrModified":
        this.updateTitleAndTooltip();
        break;
      case "resize":
        // We need to invalidate our window's other tabs' previews since layout
        // due to resizing is delayed for background tabs. Note that this
        // resize may not be the first after the main window has been resized -
        // the user may be switching to our tab which forces the resize.
        this.win.previews.forEach(function (p) {
          let controller = p.controller.wrappedJSObject;
          if (controller.wasResizedSinceLastPreview) {
            controller.resetCanvasPreview();
            p.invalidate();
          }
        });
        break;
    }
  }
};

XPCOMUtils.defineLazyGetter(PreviewController.prototype, "canvasPreviewFlags",
  function () { let canvasInterface = Ci.nsIDOMCanvasRenderingContext2D;
                return canvasInterface.DRAWWINDOW_DRAW_VIEW
                     | canvasInterface.DRAWWINDOW_DRAW_CARET
                     | canvasInterface.DRAWWINDOW_ASYNC_DECODE_IMAGES
                     | canvasInterface.DRAWWINDOW_DO_NOT_FLUSH;
});

////////////////////////////////////////////////////////////////////////////////
//// TabWindow

/*
 * This class monitors a browser window for changes to its tabs
 *
 * @param win
 *        The nsIDOMWindow browser window 
 */
function TabWindow(win) {
  this.win = win;
  this.tabbrowser = win.gBrowser;

  this.previews = new Map();

  for (let i = 0; i < this.tabEvents.length; i++)
    this.tabbrowser.tabContainer.addEventListener(this.tabEvents[i], this, false);
  this.tabbrowser.addTabsProgressListener(this);

  for (let i = 0; i < this.winEvents.length; i++)
    this.win.addEventListener(this.winEvents[i], this, false);

  AeroPeek.windows.push(this);
  let tabs = this.tabbrowser.tabs;
  for (let i = 0; i < tabs.length; i++)
    this.newTab(tabs[i]);

  this.updateTabOrdering();
  AeroPeek.checkPreviewCount();
}

TabWindow.prototype = {
  _enabled: false,
  tabEvents: ["TabOpen", "TabClose", "TabSelect", "TabMove"],
  winEvents: ["tabviewshown", "tabviewhidden"],

  destroy: function () {
    this._destroying = true;

    let tabs = this.tabbrowser.tabs;

    this.tabbrowser.removeTabsProgressListener(this);
    for (let i = 0; i < this.tabEvents.length; i++)
      this.tabbrowser.tabContainer.removeEventListener(this.tabEvents[i], this, false);

    for (let i = 0; i < this.winEvents.length; i++)
      this.win.removeEventListener(this.winEvents[i], this, false);

    for (let i = 0; i < tabs.length; i++)
      this.removeTab(tabs[i]);

    let idx = AeroPeek.windows.indexOf(this.win.gTaskbarTabGroup);
    AeroPeek.windows.splice(idx, 1);
    AeroPeek.checkPreviewCount();
  },

  get width () {
    return this.win.innerWidth;
  },
  get height () {
    return this.win.innerHeight;
  },

  // Invoked when the given tab is added to this window
  newTab: function (tab) {
    let controller = new PreviewController(this, tab);
    // It's OK to add the preview now while the favicon still loads.
    this.previews.set(tab, controller.preview);
    AeroPeek.addPreview(controller.preview);
    // updateTitleAndTooltip relies on having controller.preview which is lazily resolved.
    // Now that we've updated this.previews, it will resolve successfully.
    controller.updateTitleAndTooltip();
  },

  createTabPreview: function (controller) {
    let docShell = this.win
                  .QueryInterface(Ci.nsIInterfaceRequestor)
                  .getInterface(Ci.nsIWebNavigation)
                  .QueryInterface(Ci.nsIDocShell);
    let preview = AeroPeek.taskbar.createTaskbarTabPreview(docShell, controller);
    preview.visible = AeroPeek.enabled;
    preview.active = this.tabbrowser.selectedTab == controller.tab;
    // Grab the default favicon
    getFaviconAsImage(
      controller.linkedBrowser.contentWindow.document,
      null,
      PrivateBrowsingUtils.isWindowPrivate(this.win),
      function (img) {
        // It is possible that we've already gotten the real favicon, so make sure
        // we have not set one before setting this default one.
        if (!preview.icon)
          preview.icon = img;
      });
    return preview;
  },

  // Invoked when the given tab is closed
  removeTab: function (tab) {
    let preview = this.previewFromTab(tab);
    preview.active = false;
    preview.visible = false;
    preview.move(null);
    preview.controller.wrappedJSObject.destroy();

    this.previews.delete(tab);
    AeroPeek.removePreview(preview);
  },

  get enabled () {
    return this._enabled;
  },

  set enabled (enable) {
    this._enabled = enable;
    // Because making a tab visible requires that the tab it is next to be
    // visible, it is far simpler to unset the 'next' tab and recreate them all
    // at once.
    for (let [tab, preview] of this.previews) {
      preview.move(null);
      preview.visible = enable;
    }
    this.updateTabOrdering();
  },

  previewFromTab: function (tab) {
    return this.previews.get(tab);
  },

  updateTabOrdering: function () {
    let previews = this.previews;
    let tabs = this.tabbrowser.tabs;

    // Previews are internally stored using a map, so we need to iterate the
    // tabbrowser's array of tabs to retrieve previews in the same order.
    let inorder = [previews.get(t) for (t of tabs) if (previews.has(t))];

    // Since the internal taskbar array has not yet been updated we must force
    // on it the sorting order of our local array.  To do so we must walk
    // the local array backwards, otherwise we would send move requests in the
    // wrong order.  See bug 522610 for details.
    for (let i = inorder.length - 1; i >= 0; i--) {
      inorder[i].move(inorder[i + 1] || null);
    }
  },

  //// nsIDOMEventListener
  handleEvent: function (evt) {
    let tab = evt.originalTarget;
    switch (evt.type) {
      case "TabOpen":
        this.newTab(tab);
        this.updateTabOrdering();
        break;
      case "TabClose":
        this.removeTab(tab);
        this.updateTabOrdering();
        break;
      case "TabSelect":
        this.previewFromTab(tab).active = true;
        break;
      case "TabMove":
        this.updateTabOrdering();
        break;
      case "tabviewshown":
        this.enabled = false;
        break;
      case "tabviewhidden":
        if (!AeroPeek._prefenabled)
          return;
        this.enabled = true;
        break;
    }
  },

  //// Browser progress listener
  onLinkIconAvailable: function (aBrowser, aIconURL) {
    let self = this;
    getFaviconAsImage(
      aBrowser.contentWindow.document,
      aIconURL,PrivateBrowsingUtils.isWindowPrivate(this.win),
      function (img) {
        let index = self.tabbrowser.browsers.indexOf(aBrowser);
        // Only add it if we've found the index.  The tab could have closed!
        if (index != -1) {
          let tab = self.tabbrowser.tabs[index];
          self.previews.get(tab).icon = img;
        }
      });
  }
}

////////////////////////////////////////////////////////////////////////////////
//// AeroPeek

/*
 * This object acts as global storage and external interface for this feature.
 * It maintains the values of the prefs.
 */
this.AeroPeek = {
  available: false,
  // Does the pref say we're enabled?
  _prefenabled: true,

  _enabled: true,

  // nsITaskbarTabPreview array
  previews: [],

  // TabWindow array
  windows: [],

  // nsIWinTaskbar service
  taskbar: null,

  // Maximum number of previews
  maxpreviews: 20,

  // Length of time in seconds that previews are cached
  cacheLifespan: 20,

  initialize: function () {
    if (!(WINTASKBAR_CONTRACTID in Cc))
      return;
    this.taskbar = Cc[WINTASKBAR_CONTRACTID].getService(Ci.nsIWinTaskbar);
    this.available = this.taskbar.available;
    if (!this.available)
      return;

    this.prefs.addObserver(TOGGLE_PREF_NAME, this, false);
    this.prefs.addObserver(DISABLE_THRESHOLD_PREF_NAME, this, false);
    this.prefs.addObserver(CACHE_EXPIRATION_TIME_PREF_NAME, this, false);

    this.cacheLifespan = this.prefs.getIntPref(CACHE_EXPIRATION_TIME_PREF_NAME);

    this.maxpreviews = this.prefs.getIntPref(DISABLE_THRESHOLD_PREF_NAME);

    this.enabled = this._prefenabled = this.prefs.getBoolPref(TOGGLE_PREF_NAME);
  },

  destroy: function destroy() {
    this._enabled = false;

    this.prefs.removeObserver(TOGGLE_PREF_NAME, this);
    this.prefs.removeObserver(DISABLE_THRESHOLD_PREF_NAME, this);
    this.prefs.removeObserver(CACHE_EXPIRATION_TIME_PREF_NAME, this);

    if (this.cacheTimer)
      this.cacheTimer.cancel();
  },

  get enabled() {
    return this._enabled;
  },

  set enabled(enable) {
    if (this._enabled == enable)
      return;

    this._enabled = enable;

    this.windows.forEach(function (win) {
      win.enabled = enable;
    });
  },

  addPreview: function (preview) {
    this.previews.push(preview);
    this.checkPreviewCount();
  },

  removePreview: function (preview) {
    let idx = this.previews.indexOf(preview);
    this.previews.splice(idx, 1);
    this.checkPreviewCount();
  },

  checkPreviewCount: function () {
    if (this.previews.length > this.maxpreviews)
      this.enabled = false;
    else
      this.enabled = this._prefenabled;
  },

  onOpenWindow: function (win) {
    // This occurs when the taskbar service is not available (xp, vista)
    if (!this.available)
      return;

    win.gTaskbarTabGroup = new TabWindow(win);
  },

  onCloseWindow: function (win) {
    // This occurs when the taskbar service is not available (xp, vista)
    if (!this.available)
      return;

    win.gTaskbarTabGroup.destroy();
    delete win.gTaskbarTabGroup;

    if (this.windows.length == 0)
      this.destroy();
  },

  resetCacheTimer: function () {
    this.cacheTimer.cancel();
    this.cacheTimer.init(this, 1000*this.cacheLifespan, Ci.nsITimer.TYPE_ONE_SHOT);
  },

  //// nsIObserver
  observe: function (aSubject, aTopic, aData) {
    switch (aTopic) {
      case "nsPref:changed":
        if (aData == CACHE_EXPIRATION_TIME_PREF_NAME)
          break;

        if (aData == TOGGLE_PREF_NAME)
          this._prefenabled = this.prefs.getBoolPref(TOGGLE_PREF_NAME);
        else if (aData == DISABLE_THRESHOLD_PREF_NAME)
          this.maxpreviews = this.prefs.getIntPref(DISABLE_THRESHOLD_PREF_NAME);
        // Might need to enable/disable ourselves
        this.checkPreviewCount();
        break;
      case "timer-callback":
        this.previews.forEach(function (preview) {
          let controller = preview.controller.wrappedJSObject;
          controller.resetCanvasPreview();
        });
        break;
    }
  }
};

XPCOMUtils.defineLazyGetter(AeroPeek, "cacheTimer", () =>
  Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer)
);

XPCOMUtils.defineLazyServiceGetter(AeroPeek, "prefs",
                                   "@mozilla.org/preferences-service;1",
                                   "nsIPrefBranch");

AeroPeek.initialize();
