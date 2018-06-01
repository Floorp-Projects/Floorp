/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global content, docShell, addEventListener, addMessageListener,
   removeEventListener, removeMessageListener, sendAsyncMessage, Services */

var global = this;

// Guard against loading this frame script mutiple times
(function() {
  if (global.responsiveFrameScriptLoaded) {
    return;
  }

  const gDeviceSizeWasPageSize = docShell.deviceSizeIsPageSize;
  const gFloatingScrollbarsStylesheet = Services.io.newURI("chrome://devtools/skin/floating-scrollbars-responsive-design.css");

  let requiresFloatingScrollbars;
  let active = false;
  let resizeNotifications = false;

  addMessageListener("ResponsiveMode:Start", startResponsiveMode);
  addMessageListener("ResponsiveMode:Stop", stopResponsiveMode);
  addMessageListener("ResponsiveMode:IsActive", isActive);

  function debug(msg) {
    // dump(`RDM CHILD: ${msg}\n`);
  }

  /**
   * Used by tests to verify the state of responsive mode.
   */
  function isActive() {
    sendAsyncMessage("ResponsiveMode:IsActive:Done", { active });
  }

  function startResponsiveMode({ data }) {
    debug("START");
    if (active) {
      debug("ALREADY STARTED");
      sendAsyncMessage("ResponsiveMode:Start:Done");
      return;
    }
    addMessageListener("ResponsiveMode:RequestScreenshot", screenshot);
    const webProgress = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                              .getInterface(Ci.nsIWebProgress);
    webProgress.addProgressListener(WebProgressListener, Ci.nsIWebProgress.NOTIFY_ALL);
    docShell.deviceSizeIsPageSize = true;
    requiresFloatingScrollbars = data.requiresFloatingScrollbars;
    if (data.notifyOnResize) {
      startOnResize();
    }

    // At this point, a content viewer might not be loaded for this
    // docshell. makeScrollbarsFloating will be triggered by onLocationChange.
    if (docShell.contentViewer) {
      makeScrollbarsFloating();
    }
    active = true;
    sendAsyncMessage("ResponsiveMode:Start:Done");
  }

  function onResize() {
    const { width, height } = content.screen;
    debug(`EMIT RESIZE: ${width} x ${height}`);
    sendAsyncMessage("ResponsiveMode:OnContentResize", {
      width,
      height,
    });
  }

  function bindOnResize() {
    content.addEventListener("resize", onResize);
  }

  function startOnResize() {
    debug("START ON RESIZE");
    if (resizeNotifications) {
      return;
    }
    resizeNotifications = true;
    bindOnResize();
    addEventListener("DOMWindowCreated", bindOnResize, false);
  }

  function stopOnResize() {
    debug("STOP ON RESIZE");
    if (!resizeNotifications) {
      return;
    }
    resizeNotifications = false;
    content.removeEventListener("resize", onResize);
    removeEventListener("DOMWindowCreated", bindOnResize, false);
  }

  function stopResponsiveMode() {
    debug("STOP");
    if (!active) {
      debug("ALREADY STOPPED, ABORT");
      return;
    }
    active = false;
    removeMessageListener("ResponsiveMode:RequestScreenshot", screenshot);
    const webProgress = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                              .getInterface(Ci.nsIWebProgress);
    webProgress.removeProgressListener(WebProgressListener);
    docShell.deviceSizeIsPageSize = gDeviceSizeWasPageSize;
    restoreScrollbars();
    stopOnResize();
    sendAsyncMessage("ResponsiveMode:Stop:Done");
  }

  function makeScrollbarsFloating() {
    if (!requiresFloatingScrollbars) {
      return;
    }

    const allDocShells = [docShell];

    for (let i = 0; i < docShell.childCount; i++) {
      const child = docShell.getChildAt(i).QueryInterface(Ci.nsIDocShell);
      allDocShells.push(child);
    }

    for (const d of allDocShells) {
      const win = d.contentViewer.DOMDocument.defaultView;
      const winUtils = win.QueryInterface(Ci.nsIInterfaceRequestor)
                        .getInterface(Ci.nsIDOMWindowUtils);
      try {
        winUtils.loadSheet(gFloatingScrollbarsStylesheet, win.AGENT_SHEET);
      } catch (e) { }
    }

    flushStyle();
  }

  function restoreScrollbars() {
    const allDocShells = [docShell];
    for (let i = 0; i < docShell.childCount; i++) {
      allDocShells.push(docShell.getChildAt(i).QueryInterface(Ci.nsIDocShell));
    }
    for (const d of allDocShells) {
      const win = d.contentViewer.DOMDocument.defaultView;
      const winUtils = win.QueryInterface(Ci.nsIInterfaceRequestor)
                        .getInterface(Ci.nsIDOMWindowUtils);
      try {
        winUtils.removeSheet(gFloatingScrollbarsStylesheet, win.AGENT_SHEET);
      } catch (e) { }
    }
    flushStyle();
  }

  function flushStyle() {
    // Force presContext destruction
    const isSticky = docShell.contentViewer.sticky;
    docShell.contentViewer.sticky = false;
    docShell.contentViewer.hide();
    docShell.contentViewer.show();
    docShell.contentViewer.sticky = isSticky;
  }

  function screenshot() {
    const canvas = content.document.createElementNS("http://www.w3.org/1999/xhtml", "canvas");
    const ratio = content.devicePixelRatio;
    const width = content.innerWidth * ratio;
    const height = content.innerHeight * ratio;
    canvas.mozOpaque = true;
    canvas.width = width;
    canvas.height = height;
    const ctx = canvas.getContext("2d");
    ctx.scale(ratio, ratio);
    ctx.drawWindow(content, content.scrollX, content.scrollY, width, height, "#fff");
    sendAsyncMessage("ResponsiveMode:RequestScreenshot:Done", canvas.toDataURL());
  }

  const WebProgressListener = {
    onLocationChange(webProgress, request, URI, flags) {
      if (flags & Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT) {
        return;
      }
      makeScrollbarsFloating();
    },
    QueryInterface: ChromeUtils.generateQI(["nsIWebProgressListener",
                                            "nsISupportsWeakReference"]),
  };
})();

global.responsiveFrameScriptLoaded = true;
sendAsyncMessage("ResponsiveMode:ChildScriptReady");
