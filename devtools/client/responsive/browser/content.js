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
  const gFloatingScrollbarsStylesheet = Services.io.newURI(
    "chrome://devtools/skin/floating-scrollbars-responsive-design.css"
  );

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
    const webProgress = docShell
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIWebProgress);
    webProgress.addProgressListener(
      WebProgressListener,
      Ci.nsIWebProgress.NOTIFY_ALL
    );
    docShell.deviceSizeIsPageSize = true;
    requiresFloatingScrollbars = data.requiresFloatingScrollbars;
    if (data.notifyOnResize) {
      startOnResize();
    }

    // At this point, a content viewer might not be loaded for this
    // docshell. setDocumentInRDMPane and makeScrollbarsFloating will be
    // triggered by onLocationChange.
    if (docShell.contentViewer) {
      setDocumentInRDMPane(true);
      makeScrollbarsFloating();
    }
    active = true;
    sendAsyncMessage("ResponsiveMode:Start:Done");
  }

  function onResize() {
    // Send both a content-resize event and a viewport-resize event, since both
    // may have changed.
    let { width, height } = content.screen;
    debug(`EMIT CONTENTRESIZE: ${width} x ${height}`);
    sendAsyncMessage("ResponsiveMode:OnContentResize", {
      width,
      height,
    });

    const zoom = content.windowUtils.getResolution();
    width = content.innerWidth * zoom;
    height = content.innerHeight * zoom;
    debug(`EMIT RESIZEVIEWPORT: ${width} x ${height}`);
    sendAsyncMessage("ResponsiveMode:OnResizeViewport", {
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
    const webProgress = docShell
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIWebProgress);
    webProgress.removeProgressListener(WebProgressListener);
    docShell.deviceSizeIsPageSize = gDeviceSizeWasPageSize;
    // Restore the original physical screen orientation values before RDM is stopped.
    // This is necessary since the window document's `setCurrentRDMPaneOrientation`
    // WebIDL operation can only modify the window's screen orientation values while the
    // window content is in RDM.
    restoreScreenOrientation();
    restoreScrollbars();
    setDocumentInRDMPane(false);
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
      const winUtils = win.windowUtils;
      try {
        winUtils.loadSheet(gFloatingScrollbarsStylesheet, win.AGENT_SHEET);
      } catch (e) {}
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
      const winUtils = win.windowUtils;
      try {
        winUtils.removeSheet(gFloatingScrollbarsStylesheet, win.AGENT_SHEET);
      } catch (e) {}
    }
    flushStyle();
  }

  function restoreScreenOrientation() {
    docShell.contentViewer.DOMDocument.setRDMPaneOrientation(
      "landscape-primary",
      0
    );
  }

  function setDocumentInRDMPane(inRDMPane) {
    // We don't propegate this property to descendent documents.
    docShell.contentViewer.DOMDocument.inRDMPane = inRDMPane;
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
    const canvas = content.document.createElementNS(
      "http://www.w3.org/1999/xhtml",
      "canvas"
    );
    const ratio = content.devicePixelRatio;
    const width = content.innerWidth * ratio;
    const height = content.innerHeight * ratio;
    canvas.mozOpaque = true;
    canvas.width = width;
    canvas.height = height;
    const ctx = canvas.getContext("2d");
    ctx.scale(ratio, ratio);
    ctx.drawWindow(
      content,
      content.scrollX,
      content.scrollY,
      width,
      height,
      "#fff"
    );
    sendAsyncMessage(
      "ResponsiveMode:RequestScreenshot:Done",
      canvas.toDataURL()
    );
  }

  const WebProgressListener = {
    onLocationChange(webProgress, request, URI, flags) {
      if (flags & Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT) {
        return;
      }
      setDocumentInRDMPane(true);
      // Notify the Responsive UI manager to set orientation state on a location change.
      // This is necessary since we want to ensure that the RDM Document's orientation
      // state persists throughout while RDM is opened.
      sendAsyncMessage("ResponsiveMode:OnLocationChange", {
        width: content.innerWidth,
        height: content.innerHeight,
      });
      makeScrollbarsFloating();
    },
    QueryInterface: ChromeUtils.generateQI([
      "nsIWebProgressListener",
      "nsISupportsWeakReference",
    ]),
  };
})();

global.responsiveFrameScriptLoaded = true;
sendAsyncMessage("ResponsiveMode:ChildScriptReady");
