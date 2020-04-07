/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  BrowserUtils: "resource://gre/modules/BrowserUtils.jsm",
  Services: "resource://gre/modules/Services.jsm",
  SiteSpecificBrowser: "resource:///modules/SiteSpecificBrowserService.jsm",
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.jsm",
  WindowsSupport: "resource:///modules/ssb/WindowsSupport.jsm",
});

let gSSBBrowser = null;
var gSSB = null;

function init() {
  gSSB = SiteSpecificBrowser.get(window.arguments[0]);

  let uri = gSSB.startURI;
  if (window.arguments.length > 1) {
    uri = Services.io.newURI(window.arguments[1]);
  }

  window.browserDOMWindow = new BrowserDOMWindow();

  gSSBBrowser = document.createXULElement("browser");
  gSSBBrowser.setAttribute("id", "browser");
  gSSBBrowser.setAttribute("type", "content");
  gSSBBrowser.setAttribute("remote", "true");
  gSSBBrowser.setAttribute("nodefaultsrc", "true");
  document.getElementById("browser-container").appendChild(gSSBBrowser);

  // Give our actor the SSB's ID.
  let actor = gSSBBrowser.browsingContext.currentWindowGlobal.getActor(
    "SiteSpecificBrowser"
  );
  actor.sendAsyncMessage("SetSSB", gSSB.id);

  gSSBBrowser.addProgressListener(
    new ProgressListener(),
    Ci.nsIWebProgress.NOTIFY_STATE_ALL
  );
  gSSBBrowser.src = uri.spec;

  document.getElementById("title").textContent = gSSB.name;
}

class ProgressListener {
  constructor() {
    this.isInitial = true;
  }

  /**
   * Called when the load state changes
   *
   * @param {nsIWebProgress} webProgress
   * @param {nsIRequest} request
   * @param {Number} state
   * @param {Number} status
   */
  async onStateChange(webProgress, request, state, status) {
    if (!webProgress.isTopLevel) {
      return;
    }

    let final =
      Ci.nsIWebProgressListener.STATE_IS_WINDOW +
      Ci.nsIWebProgressListener.STATE_STOP;
    if ((state & final) != final) {
      return;
    }

    // Load complete. Does the SSB need an update?
    let { isInitial } = this;
    this.isInitial = false;
    if (isInitial && gSSB.needsUpdate) {
      await gSSB.updateFromBrowser(gSSBBrowser);
      if (Services.appinfo.OS == "WINNT") {
        WindowsSupport.applyOSIntegration(gSSB, window);
      }
    }

    // So the testing harness knows when the ssb is properly initialized.
    let event = new CustomEvent("SSBLoad");
    gSSBBrowser.dispatchEvent(event);
  }
}

ProgressListener.prototype.QueryInterface = ChromeUtils.generateQI([
  Ci.nsIWebProgressListener,
  Ci.nsISupportsWeakReference,
]);

class BrowserDOMWindow {
  /**
   * Called when a page in the main process needs a new window to display a new
   * page in.
   *
   * @param {nsIURI?} uri
   * @param {nsIOpenWindowInfo} openWindowInfo
   * @param {Number} where
   * @param {Number} flags
   * @param {nsIPrincipal} triggeringPrincipal
   * @param {nsIContentSecurityPolicy?} csp
   * @return {BrowsingContext} the BrowsingContext the URI should be loaded in.
   */
  createContentWindow(
    uri,
    openWindowInfo,
    where,
    flags,
    triggeringPrincipal,
    csp
  ) {
    console.error(
      "createContentWindow should never be called from a remote browser"
    );
    throw Cr.NS_ERROR_FAILURE;
  }

  /**
   * Called from a page in the main process to open a new URI.
   *
   * @param {nsIURI} uri
   * @param {nsIOpenWindowInfo} openWindowInfo
   * @param {Number} where
   * @param {Number} flags
   * @param {nsIPrincipal} triggeringPrincipal
   * @param {nsIContentSecurityPolicy?} csp
   * @return {BrowsingContext} the BrowsingContext the URI should be loaded in.
   */
  openURI(uri, openWindowInfo, where, flags, triggeringPrincipal, csp) {
    console.error("openURI should never be called from a remote browser");
    throw Cr.NS_ERROR_FAILURE;
  }

  /**
   * Finds a new frame to load some content in.
   *
   * @param {nsIURI?} uri
   * @param {nsIOpenURIInFrameParams} params
   * @param {Number} where
   * @param {Number} flags
   * @param {string} name
   * @param {boolean} shouldOpen should the load start or not.
   * @return {Element} the frame element the URI should be loaded in.
   */
  getContentWindowOrOpenURIInFrame(
    uri,
    params,
    where,
    flags,
    name,
    shouldOpen
  ) {
    // It's been determined that this load needs to happen in a new frame.
    // Either onBeforeLinkTraversal set this correctly or this is the result
    // of a window.open call.

    // If this ssb can load the url then just load it internally.
    if (gSSB.canLoad(uri)) {
      return gSSBBrowser;
    }

    // Try and find a browser window to open in.
    let win = BrowserWindowTracker.getTopWindow({
      private: params.isPrivate,
      allowPopups: false,
    });

    if (win) {
      // Just hand off to the window's handler
      win.focus();
      return win.browserDOMWindow.openURIInFrame(
        shouldOpen ? uri : null,
        params,
        where,
        flags,
        name
      );
    }

    // We need to open a new browser window and a tab in it. That's an
    // asychronous operation but luckily if we return null here the platform
    // handles doing that for us.
    return null;
  }

  /**
   * Gets an nsFrameLoaderOwner to load some new content in.
   *
   * @param {nsIURI?} uri
   * @param {nsIOpenURIInFrameParams} params
   * @param {Number} where
   * @param {Number} flags
   * @param {string} name
   * @return {Element} the frame element the URI should be loaded in.
   */
  createContentWindowInFrame(uri, params, where, flags, name) {
    return this.getContentWindowOrOpenURIInFrame(
      uri,
      params,
      where,
      flags,
      name,
      false
    );
  }

  /**
   * Create a new nsFrameLoaderOwner and load some content into it.
   *
   * @param {nsIURI} uri
   * @param {nsIOpenURIInFrameParams} params
   * @param {Number} where
   * @param {Number} flags
   * @param {string} name
   * @return {Element} the frame element the URI is loading in.
   */
  openURIInFrame(uri, params, where, flags, name) {
    return this.getContentWindowOrOpenURIInFrame(
      uri,
      params,
      where,
      flags,
      name,
      true
    );
  }

  isTabContentWindow(window) {
    // This method is probably not needed anymore: bug 1602915
    return gSSBBrowser.contentWindow == window;
  }

  canClose() {
    return BrowserUtils.canCloseWindow(window);
  }

  get tabCount() {
    return 1;
  }
}

BrowserDOMWindow.prototype.QueryInterface = ChromeUtils.generateQI([
  Ci.nsIBrowserDOMWindow,
]);

window.addEventListener("DOMContentLoaded", init, true);
