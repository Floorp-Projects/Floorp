/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EventEmitter = require("devtools/shared/event-emitter");
const Services = require("Services");

loader.lazyRequireGetter(
  this,
  "gDevToolsBrowser",
  "devtools/client/framework/devtools-browser",
  true
);

loader.lazyRequireGetter(
  this,
  "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm",
  true
);

/* A host should always allow this much space for the page to be displayed.
 * There is also a min-height on the browser, but we still don't want to set
 * frame.height to be larger than that, since it can cause problems with
 * resizing the toolbox and panel layout. */
const MIN_PAGE_SIZE = 25;

const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

/**
 * A toolbox host represents an object that contains a toolbox (e.g. the
 * sidebar or a separate window). Any host object should implement the
 * following functions:
 *
 * create() - create the UI
 * destroy() - destroy the host's UI
 */

/**
 * Host object for the dock on the bottom of the browser
 */
function BottomHost(hostTab) {
  this.hostTab = hostTab;

  EventEmitter.decorate(this);
}

BottomHost.prototype = {
  type: "bottom",

  heightPref: "devtools.toolbox.footer.height",

  /**
   * Create a box at the bottom of the host tab.
   */
  create: async function() {
    await gDevToolsBrowser.loadBrowserStyleSheet(this.hostTab.ownerGlobal);

    const gBrowser = this.hostTab.ownerDocument.defaultView.gBrowser;
    const ownerDocument = gBrowser.ownerDocument;
    this._browserContainer = gBrowser.getBrowserContainer(
      this.hostTab.linkedBrowser
    );

    this._splitter = ownerDocument.createXULElement("splitter");
    this._splitter.setAttribute("class", "devtools-horizontal-splitter");
    // Avoid resizing notification containers
    this._splitter.setAttribute("resizebefore", "flex");

    this.frame = createDevToolsFrame(
      ownerDocument,
      "devtools-toolbox-bottom-iframe"
    );
    this.frame.height = Math.min(
      Services.prefs.getIntPref(this.heightPref),
      this._browserContainer.clientHeight - MIN_PAGE_SIZE
    );

    this._browserContainer.appendChild(this._splitter);
    this._browserContainer.appendChild(this.frame);

    focusTab(this.hostTab);
    return this.frame;
  },

  /**
   * Raise the host.
   */
  raise: function() {
    focusTab(this.hostTab);
  },

  /**
   * Set the toolbox title.
   * Nothing to do for this host type.
   */
  setTitle: function() {},

  /**
   * Destroy the bottom dock.
   */
  destroy: function() {
    if (!this._destroyed) {
      this._destroyed = true;

      Services.prefs.setIntPref(this.heightPref, this.frame.height);
      this._browserContainer.removeChild(this._splitter);
      this._browserContainer.removeChild(this.frame);
      this.frame = null;
      this._browserContainer = null;
      this._splitter = null;
    }

    return Promise.resolve(null);
  },
};

/**
 * Base Host object for the in-browser sidebar
 */
class SidebarHost {
  constructor(hostTab, type) {
    this.hostTab = hostTab;
    this.type = type;
    this.widthPref = "devtools.toolbox.sidebar.width";

    EventEmitter.decorate(this);
  }

  /**
   * Create a box in the sidebar of the host tab.
   */
  async create() {
    await gDevToolsBrowser.loadBrowserStyleSheet(this.hostTab.ownerGlobal);
    const gBrowser = this.hostTab.ownerDocument.defaultView.gBrowser;
    const ownerDocument = gBrowser.ownerDocument;
    this._browserContainer = gBrowser.getBrowserContainer(
      this.hostTab.linkedBrowser
    );
    this._browserPanel = gBrowser.getPanel(this.hostTab.linkedBrowser);

    this._splitter = ownerDocument.createXULElement("splitter");
    this._splitter.setAttribute("class", "devtools-side-splitter");

    this.frame = createDevToolsFrame(
      ownerDocument,
      "devtools-toolbox-side-iframe"
    );
    this.frame.width = Math.min(
      Services.prefs.getIntPref(this.widthPref),
      this._browserPanel.clientWidth - MIN_PAGE_SIZE
    );

    // We should consider the direction when changing the dock position.
    const topWindow = this.hostTab.ownerDocument.defaultView.top;
    const topDoc = topWindow.document.documentElement;
    const isLTR = topWindow.getComputedStyle(topDoc).direction === "ltr";

    if ((isLTR && this.type == "right") || (!isLTR && this.type == "left")) {
      this._browserPanel.appendChild(this._splitter);
      this._browserPanel.appendChild(this.frame);
    } else {
      this._browserPanel.insertBefore(this.frame, this._browserContainer);
      this._browserPanel.insertBefore(this._splitter, this._browserContainer);
    }

    focusTab(this.hostTab);
    return this.frame;
  }

  /**
   * Raise the host.
   */
  raise() {
    focusTab(this.hostTab);
  }

  /**
   * Set the toolbox title.
   * Nothing to do for this host type.
   */
  setTitle() {}

  /**
   * Destroy the sidebar.
   */
  destroy() {
    if (!this._destroyed) {
      this._destroyed = true;

      Services.prefs.setIntPref(this.widthPref, this.frame.width);
      this._browserPanel.removeChild(this._splitter);
      this._browserPanel.removeChild(this.frame);
    }

    return Promise.resolve(null);
  }
}

/**
 * Host object for the in-browser left sidebar
 */
class LeftHost extends SidebarHost {
  constructor(hostTab) {
    super(hostTab, "left");
  }
}

/**
 * Host object for the in-browser right sidebar
 */
class RightHost extends SidebarHost {
  constructor(hostTab) {
    super(hostTab, "right");
  }
}

/**
 * Host object for the toolbox in a separate window
 */
function WindowHost(hostTab) {
  this._boundUnload = this._boundUnload.bind(this);
  this.hostTab = hostTab;
  EventEmitter.decorate(this);
}

WindowHost.prototype = {
  type: "window",

  WINDOW_URL: "chrome://devtools/content/framework/toolbox-window.xhtml",

  /**
   * Create a new xul window to contain the toolbox.
   */
  create: function() {
    return new Promise(resolve => {
      let flags = "chrome,centerscreen,resizable,dialog=no";

      // If we are debugging a tab which is in a Private window, we must also
      // set the private flag on the DevTools host window. Otherwise switching
      // hosts between docked and window modes can fail due to incompatible
      // docshell origin attributes. See 1581093.
      if (
        this.hostTab &&
        PrivateBrowsingUtils.isWindowPrivate(this.hostTab.ownerGlobal)
      ) {
        flags += ",private";
      }

      // If the current window is a non-fission window, force the non-fission
      // flag. Otherwise switching to window host from a non-fission window in
      // a fission Firefox (!) will attempt to swapFrameLoaders between fission
      // and non-fission frames. See Bug 1650963.
      if (this.hostTab && !this.hostTab.ownerGlobal.gFissionBrowser) {
        flags += ",non-fission";
      }

      const win = Services.ww.openWindow(
        null,
        this.WINDOW_URL,
        "_blank",
        flags,
        null
      );

      const frameLoad = () => {
        win.removeEventListener("load", frameLoad, true);
        win.focus();

        this.frame = createDevToolsFrame(
          win.document,
          "devtools-toolbox-window-iframe"
        );
        win.document
          .getElementById("devtools-toolbox-window")
          .appendChild(this.frame);

        // The forceOwnRefreshDriver attribute is set to avoid Windows only issues with
        // CSS transitions when switching from docked to window hosts.
        // Added in Bug 832920, should be reviewed in Bug 1542468.
        this.frame.setAttribute("forceOwnRefreshDriver", "");
        resolve(this.frame);
      };

      win.addEventListener("load", frameLoad, true);
      win.addEventListener("unload", this._boundUnload);

      this._window = win;
    });
  },

  /**
   * Catch the user closing the window.
   */
  _boundUnload: function(event) {
    if (event.target.location != this.WINDOW_URL) {
      return;
    }
    this._window.removeEventListener("unload", this._boundUnload);

    this.emit("window-closed");
  },

  /**
   * Raise the host.
   */
  raise: function() {
    this._window.focus();
  },

  /**
   * Set the toolbox title.
   */
  setTitle: function(title) {
    this._window.document.title = title;
  },

  /**
   * Destroy the window.
   */
  destroy: function() {
    if (!this._destroyed) {
      this._destroyed = true;

      this._window.removeEventListener("unload", this._boundUnload);
      this._window.close();
    }

    return Promise.resolve(null);
  },
};

/**
 * Host object for the Browser Toolbox
 */
function BrowserToolboxHost(hostTab, options) {
  this.doc = options.doc;
  EventEmitter.decorate(this);
}

BrowserToolboxHost.prototype = {
  type: "browsertoolbox",

  create: async function() {
    this.frame = createDevToolsFrame(
      this.doc,
      "devtools-toolbox-browsertoolbox-iframe"
    );

    this.doc.body.appendChild(this.frame);

    return this.frame;
  },

  /**
   * Raise the host.
   */
  raise: function() {
    this.doc.defaultView.focus();
  },

  /**
   * Set the toolbox title.
   */
  setTitle: function(title) {
    this.doc.title = title;
  },

  // Do nothing. The BrowserToolbox is destroyed by quitting the application.
  destroy: function() {
    return Promise.resolve(null);
  },
};

/**
 * Host object for the toolbox as a page.
 * This is typically used by `about:debugging`, when opening toolbox in a new tab,
 * via `about:devtools-toolbox` URLs.
 * The `iframe` ends up being the tab's browser element.
 */
function PageHost(hostTab, options) {
  this.frame = options.customIframe;
}

PageHost.prototype = {
  type: "page",

  create: function() {
    return Promise.resolve(this.frame);
  },

  // Do nothing.
  raise: function() {},

  // Do nothing.
  setTitle: function(title) {},

  // Do nothing.
  destroy: function() {
    return Promise.resolve(null);
  },
};

/**
 *  Switch to the given tab in a browser and focus the browser window
 */
function focusTab(tab) {
  const browserWindow = tab.ownerDocument.defaultView;
  browserWindow.focus();
  browserWindow.gBrowser.selectedTab = tab;
}

/**
 * Create an iframe that can be used to load DevTools via about:devtools-toolbox.
 */
function createDevToolsFrame(doc, className) {
  const frame = doc.createXULElement("browser");
  frame.setAttribute("type", "content");
  frame.flex = 1; // Required to be able to shrink when the window shrinks
  frame.className = className;

  const inXULDocument = doc.documentElement.namespaceURI === XUL_NS;
  if (inXULDocument) {
    // When the toolbox frame is loaded in a XUL document, tooltips rely on a
    // special XUL <tooltip id="aHTMLTooltip"> element.
    // This attribute should not be set when the frame is loaded in a HTML
    // document (for instance: Browser Toolbox).
    frame.tooltip = "aHTMLTooltip";
  }
  return frame;
}

exports.Hosts = {
  bottom: BottomHost,
  left: LeftHost,
  right: RightHost,
  window: WindowHost,
  browsertoolbox: BrowserToolboxHost,
  page: PageHost,
};
