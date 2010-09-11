/* vim: se cin sw=2 ts=2 et filetype=javascript :
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Rob Arnold <robarnold@cmu.edu> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
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
var EXPORTED_SYMBOLS = ["AeroPeek"];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");

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
function _imageFromURI(uri, callback) {
  let channel = ioSvc.newChannelFromURI(uri);
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
        _imageFromURI(defaultURI, callback);
    }
  });
}

// string? -> imgIContainer
function getFaviconAsImage(iconurl, callback) {
  if (iconurl)
    _imageFromURI(NetUtil.newURI(iconurl), callback);
  else
    _imageFromURI(faviconSvc.defaultFavicon, callback);
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

  this.linkedBrowser.addEventListener("MozAfterPaint", this, false);
  this.tab.addEventListener("TabAttrModified", this, false);

  // Cannot perform the lookup during construction. See TabWindow.newTab 
  XPCOMUtils.defineLazyGetter(this, "preview", function () this.win.previewFromTab(this.tab));

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
}

PreviewController.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsITaskbarPreviewController,
                                         Ci.nsIDOMEventListener]),
  destroy: function () {
    this.tab.removeEventListener("TabAttrModified", this, false);
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
    this.canvasPreview.width = 0;
    this.canvasPreview.height = 0;
  },

  // Updates the controller's canvas with the parts of the <browser> that need
  // to be redrawn.
  updateCanvasPreview: function () {
    let win = this.linkedBrowser.contentWindow;
    let bx = this.linkedBrowser.boxObject;
    // Check for resize
    if (bx.width != this.canvasPreview.width ||
        bx.height != this.canvasPreview.height) {
      // Invalidate the entire area and repaint
      this.onTabPaint({left:0, top:0, width:bx.width, height:bx.height});
      this.canvasPreview.width = bx.width;
      this.canvasPreview.height = bx.height;
    }

    // Draw dirty regions
    let ctx = this.canvasPreview.getContext("2d");
    let flags = this.canvasPreviewFlags;
    // width/height are occasionally bogus and too large for drawWindow
    // so we clip to the canvas region
    this.dirtyRegion.intersectRect(0, 0, bx.width, bx.height);
    this.dirtyRects.forEach(function (r) {
      let x = r.x;
      let y = r.y;
      let width = r.width;
      let height = r.height;
      ctx.save();
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
    // Ignore spurious dirty rects
    if (!rect.width || !rect.height)
      return;

    let r = { x: Math.floor(rect.left),
              y: Math.floor(rect.top),
              width: Math.ceil(rect.width),
              height: Math.ceil(rect.height)
            };
    this.dirtyRegion.unionRect(r.x, r.y, r.width, r.height);
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
    let self = this;
    this.win.tabbrowser.previewTab(this.tab, function () self.previewTabCallback(ctx));

    // We must avoid having the frame drawn around the window. See bug 520807
    return false;
  },

  previewTabCallback: function (ctx) {
    let width = this.win.width;
    let height = this.win.height;
    // Draw our toplevel window
    ctx.drawWindow(this.win.win, 0, 0, width, height, "transparent");

    // Compositor, where art thou?
    // Draw the tab content on top of the toplevel window
    this.updateCanvasPreview();

    let boxObject = this.linkedBrowser.boxObject;
    ctx.translate(boxObject.x, boxObject.y);
    ctx.drawImage(this.canvasPreview, 0, 0);
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
        let preview = this.preview;
        if (preview.visible)
          preview.invalidate();
        break;
      case "TabAttrModified":
        this.updateTitleAndTooltip();
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

  this.previews = [];

  for (let i = 0; i < this.events.length; i++)
    this.tabbrowser.tabContainer.addEventListener(this.events[i], this, false);
  this.tabbrowser.addTabsProgressListener(this);

  AeroPeek.windows.push(this);
  let tabs = this.tabbrowser.tabs;
  for (let i = 0; i < tabs.length; i++)
    this.newTab(tabs[i]);

  this.updateTabOrdering();
  AeroPeek.checkPreviewCount();
}

TabWindow.prototype = {
  _enabled: false,
  events: ["TabOpen", "TabClose", "TabSelect", "TabMove"],

  destroy: function () {
    this._destroying = true;

    let tabs = this.tabbrowser.tabs;

    this.tabbrowser.removeTabsProgressListener(this);

    for (let i = 0; i < this.events.length; i++)
      this.tabbrowser.tabContainer.removeEventListener(this.events[i], this, false);

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
    let docShell = this.win
                  .QueryInterface(Ci.nsIInterfaceRequestor)
                  .getInterface(Ci.nsIWebNavigation)
                  .QueryInterface(Ci.nsIDocShell);
    let preview = AeroPeek.taskbar.createTaskbarTabPreview(docShell, controller);
    preview.visible = AeroPeek.enabled;
    preview.active = this.tabbrowser.selectedTab == tab;
    // Grab the default favicon
    getFaviconAsImage(null, function (img) {
      // It is possible that we've already gotten the real favicon, so make sure
      // we have not set one before setting this default one.
      if (!preview.icon)
        preview.icon = img;
    });

    // It's OK to add the preview now while the favicon still loads.
    this.previews.splice(tab._tPos, 0, preview);
    AeroPeek.addPreview(preview);
    // updateTitleAndTooltip relies on having controller.preview which is lazily resolved.
    // Now that we've updated this.previews, it will resolve successfully.
    controller.updateTitleAndTooltip();
  },

  // Invoked when the given tab is closed
  removeTab: function (tab) {
    let preview = this.previewFromTab(tab);
    preview.active = false;
    preview.visible = false;
    preview.move(null);
    preview.controller.wrappedJSObject.destroy();

    // We don't want to splice from the array if the tabs aren't being removed
    // from the tab bar as well (as is the case when the window closes).
    if (!this._destroying)
      this.previews.splice(tab._tPos, 1);
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
    this.previews.forEach(function (preview) {
      preview.move(null);
      preview.visible = enable;
    });
    this.updateTabOrdering();
  },

  previewFromTab: function (tab) {
    return this.previews[tab._tPos];
  },

  updateTabOrdering: function () {
    // Since the internal taskbar array has not yet been updated we must force
    // on it the sorting order of our local array.  To do so we must walk
    // the local array backwards, otherwise we would send move requests in the
    // wrong order.  See bug 522610 for details.
    for (let i = this.previews.length - 1; i >= 0; i--) {
      let p = this.previews[i];
      let next = i == this.previews.length - 1 ? null : this.previews[i+1];
      p.move(next);
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
        let oldPos = evt.detail;
        let newPos = tab._tPos;
        let preview = this.previews[oldPos];
        this.previews.splice(oldPos, 1);
        this.previews.splice(newPos, 0, preview);
        this.updateTabOrdering();
        break;
    }
  },

  //// Browser progress listener
  onLinkIconAvailable: function (aBrowser, aIconURL) {
    let self = this;
    getFaviconAsImage(aIconURL, function (img) {
      let index = self.tabbrowser.browsers.indexOf(aBrowser);
      // Only add it if we've found the index.  The tab could have closed!
      if (index != -1)
        self.previews[index].icon = img;
    });
  }
}

////////////////////////////////////////////////////////////////////////////////
//// AeroPeek

/*
 * This object acts as global storage and external interface for this feature.
 * It maintains the values of the prefs.
 */
var AeroPeek = {
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

XPCOMUtils.defineLazyGetter(AeroPeek, "cacheTimer", function ()
  Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer)
);

XPCOMUtils.defineLazyServiceGetter(AeroPeek, "prefs",
                                   "@mozilla.org/preferences-service;1",
                                   "nsIPrefBranch2");

AeroPeek.initialize();
