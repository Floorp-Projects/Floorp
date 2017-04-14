/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Ci } = require("chrome");
const Services = require("Services");
const { Task } = require("devtools/shared/task");
const { BrowserElementWebNavigation } = require("./web-navigation");
const { getStack } = require("devtools/shared/platform/stack");

// A symbol used to hold onto the frame loader from the outer browser while tunneling.
const FRAME_LOADER = Symbol("devtools/responsive/frame-loader");

function debug(msg) {
  // console.log(msg);
}

/**
 * Properties swapped between browsers by browser.xml's `swapDocShells`.  See also the
 * list at /devtools/client/responsive.html/docs/browser-swap.md.
 */
const SWAPPED_BROWSER_STATE = [
  "_remoteFinder",
  "_securityUI",
  "_documentURI",
  "_documentContentType",
  "_contentTitle",
  "_characterSet",
  "_contentPrincipal",
  "_imageDocument",
  "_fullZoom",
  "_textZoom",
  "_isSyntheticDocument",
  "_innerWindowID",
  "_manifestURI",
];

/**
 * This module takes an "outer" <xul:browser> from a browser tab as described by
 * Firefox's tabbrowser.xml and wires it up to an "inner" <iframe mozbrowser>
 * browser element containing arbitrary page content of interest.
 *
 * The inner <iframe mozbrowser> element is _just_ the page content.  It is not
 * enough to to replace <xul:browser> on its own.  <xul:browser> comes along
 * with lots of associated functionality via XBL bindings defined for such
 * elements in browser.xml and remote-browser.xml, and the Firefox UI depends on
 * these various things to make the UI function.
 *
 * By mapping various methods, properties, and messages from the outer browser
 * to the inner browser, we can control the content inside the inner browser
 * using the standard Firefox UI elements for navigation, reloading, and more.
 *
 * The approaches used in this module were chosen to avoid needing changes to
 * the core browser for this specialized use case.  If we start to increase
 * usage of <iframe mozbrowser> in the core browser, we should avoid this module
 * and instead refactor things to work with mozbrowser directly.
 *
 * For the moment though, this serves as a sufficient path to connect the
 * Firefox UI to a mozbrowser.
 *
 * @param outer
 *        A <xul:browser> from a regular browser tab.
 * @param inner
 *        A <iframe mozbrowser> containing page content to be wired up to the
 *        primary browser UI via the outer browser.
 */
function tunnelToInnerBrowser(outer, inner) {
  let browserWindow = outer.ownerDocument.defaultView;
  let gBrowser = browserWindow.gBrowser;
  let mmTunnel;

  return {

    start: Task.async(function* () {
      if (outer.isRemoteBrowser) {
        throw new Error("The outer browser must be non-remote.");
      }
      if (!inner.isRemoteBrowser) {
        throw new Error("The inner browser must be remote.");
      }

      // Various browser methods access the `frameLoader` property, including:
      //   * `saveBrowser` from contentAreaUtils.js
      //   * `docShellIsActive` from remote-browser.xml
      //   * `hasContentOpener` from remote-browser.xml
      //   * `preserveLayers` from remote-browser.xml
      //   * `receiveMessage` from SessionStore.jsm
      // In general, these methods are interested in the `frameLoader` for the content,
      // so we redirect them to the inner browser's `frameLoader`.
      outer[FRAME_LOADER] = outer.frameLoader;
      Object.defineProperty(outer, "frameLoader", {
        get() {
          let stack = getStack();
          // One exception is `receiveMessage` from SessionStore.jsm.  SessionStore
          // expects data updates to come in as messages targeted to a <xul:browser>.
          // In addition, it verifies[1] correctness by checking that the received
          // message's `targetFrameLoader` property matches the `frameLoader` of the
          // <xul:browser>. To keep SessionStore functioning as expected, we give it the
          // outer `frameLoader` as if nothing has changed.
          // [1]: https://dxr.mozilla.org/mozilla-central/rev/b1b18f25c0ea69d9ee57c4198d577dfcd0129ce1/browser/components/sessionstore/SessionStore.jsm#716
          if (stack.caller.filename.endsWith("SessionStore.jsm")) {
            return outer[FRAME_LOADER];
          }
          return inner.frameLoader;
        },
        configurable: true,
        enumerable: true,
      });

      // The `outerWindowID` of the content is used by browser actions like view source
      // and print.  They send the ID down to the client to find the right content frame
      // to act on.
      Object.defineProperty(outer, "outerWindowID", {
        get() {
          return inner.outerWindowID;
        },
        configurable: true,
        enumerable: true,
      });

      // The `permanentKey` property on a <xul:browser> is used to index into various maps
      // held by the session store.  When you swap content around with
      // `_swapBrowserDocShells`, these keys are also swapped so they follow the content.
      // This means the key that matches the content is on the inner browser.  Since we
      // want the browser UI to believe the page content is part of the outer browser, we
      // copy the content's `permanentKey` up to the outer browser.
      debug("Copy inner permanentKey to outer browser");
      outer.permanentKey = inner.permanentKey;

      // Replace the outer browser's native messageManager with a message manager tunnel
      // which we can use to route messages of interest to the inner browser instead.
      // Note: The _actual_ messageManager accessible from
      // `browser.frameLoader.messageManager` is not overridable and is left unchanged.
      // Only the XBL getter `browser.messageManager` is overridden.  Browser UI code
      // always uses this getter instead of `browser.frameLoader.messageManager` directly,
      // so this has the effect of overriding the message manager for browser UI code.
      mmTunnel = new MessageManagerTunnel(outer, inner);

      // We are tunneling to an inner browser with a specific remoteness, so it is simpler
      // for the logic of the browser UI to assume this tab has taken on that remoteness,
      // even though it's not true.  Since the actions the browser UI performs are sent
      // down to the inner browser by this tunnel, the tab's remoteness effectively is the
      // remoteness of the inner browser.
      outer.setAttribute("remote", "true");
      outer.setAttribute("remoteType", inner.remoteType);

      // Clear out any cached state that references the current non-remote XBL binding,
      // such as form fill controllers.  Otherwise they will remain in place and leak the
      // outer docshell.
      outer.destroy();
      // The XBL binding for remote browsers uses the message manager for many actions in
      // the UI and that works well here, since it gives us one main thing we need to
      // route to the inner browser (the messages), instead of having to tweak many
      // different browser properties.  It is safe to alter a XBL binding dynamically.
      // The content within is not reloaded.
      outer.style.MozBinding = "url(chrome://browser/content/tabbrowser.xml" +
                               "#tabbrowser-remote-browser)";

      // The constructor of the new XBL binding is run asynchronously and there is no
      // event to signal its completion.  Spin an event loop to watch for properties that
      // are set by the contructor.
      while (!outer._remoteWebNavigation) {
        Services.tm.currentThread.processNextEvent(true);
      }

      // Replace the `webNavigation` object with our own version which tries to use
      // mozbrowser APIs where possible.  This replaces the webNavigation object that the
      // remote-browser.xml binding creates.  We do not care about it's original value
      // because stop() will remove the remote-browser.xml binding and these will no
      // longer be used.
      let webNavigation = new BrowserElementWebNavigation(inner);
      webNavigation.copyStateFrom(inner._remoteWebNavigationImpl);
      outer._remoteWebNavigation = webNavigation;
      outer._remoteWebNavigationImpl = webNavigation;

      // Now that we've flipped to the remote browser XBL binding, add `progressListener`
      // onto the remote version of `webProgress`.  Normally tabbrowser.xml does this step
      // when it creates a new browser, etc.  Since we manually changed the XBL binding
      // above, it caused a fresh webProgress object to be created which does not have any
      // listeners added.  So, we get the listener that gBrowser is using for the tab and
      // reattach it here.
      let tab = gBrowser.getTabForBrowser(outer);
      let filteredProgressListener = gBrowser._tabFilters.get(tab);
      outer.webProgress.addProgressListener(filteredProgressListener);

      // Add the inner browser to tabbrowser's WeakMap from browser to tab.  This assists
      // with tabbrowser's processing of some events such as MozLayerTreeReady which
      // bubble up from the remote content frame and trigger tabbrowser to lookup the tab
      // associated with the browser that triggered the event.
      gBrowser._tabForBrowser.set(inner, tab);

      // All of the browser state from content was swapped onto the inner browser.  Pull
      // this state up to the outer browser.
      for (let property of SWAPPED_BROWSER_STATE) {
        outer[property] = inner[property];
      }

      // Expose `PopupNotifications` on the content's owner global.
      // This is used by PermissionUI.jsm for permission doorhangers.
      // Note: This pollutes the responsive.html tool UI's global.
      Object.defineProperty(inner.ownerGlobal, "PopupNotifications", {
        get() {
          return outer.ownerGlobal.PopupNotifications;
        },
        configurable: true,
        enumerable: true,
      });

      // Expose `whereToOpenLink` on the content's owner global.
      // This is used by ContentClick.jsm when opening links in ways other than just
      // navigating the viewport.
      // Note: This pollutes the responsive.html tool UI's global.
      Object.defineProperty(inner.ownerGlobal, "whereToOpenLink", {
        get() {
          return outer.ownerGlobal.whereToOpenLink;
        },
        configurable: true,
        enumerable: true,
      });

      // Add mozbrowser event handlers
      inner.addEventListener("mozbrowseropenwindow", this);
    }),

    handleEvent(event) {
      if (event.type != "mozbrowseropenwindow") {
        return;
      }

      // Minimal support for <a target/> and window.open() which just ensures we at
      // least open them somewhere (in a new tab).  The following things are ignored:
      //   * Specific target names (everything treated as _blank)
      //   * Window features
      //   * window.opener
      // These things are deferred for now, since content which does depend on them seems
      // outside the main focus of RDM.
      let { detail } = event;
      event.preventDefault();
      let uri = Services.io.newURI(detail.url);
      // This API is used mainly because it's near the path used for <a target/> with
      // regular browser tabs (which calls `openURIInFrame`).  The more elaborate APIs
      // that support openers, window features, etc. didn't seem callable from JS and / or
      // this event doesn't give enough info to use them.
      browserWindow.browserDOMWindow
        .openURI(uri, null, Ci.nsIBrowserDOMWindow.OPEN_NEWTAB,
                 Ci.nsIBrowserDOMWindow.OPEN_NEW);
    },

    stop() {
      let tab = gBrowser.getTabForBrowser(outer);
      let filteredProgressListener = gBrowser._tabFilters.get(tab);

      // The browser's state has changed over time while the tunnel was active.  Push the
      // the current state down to the inner browser, so that it follows the content in
      // case that browser will be swapped elsewhere.
      for (let property of SWAPPED_BROWSER_STATE) {
        inner[property] = outer[property];
      }

      // Remove the inner browser from the WeakMap from browser to tab.
      gBrowser._tabForBrowser.delete(inner);

      // Remove the progress listener we added manually.
      outer.webProgress.removeProgressListener(filteredProgressListener);

      // Reset the XBL binding back to the default.
      outer.destroy();
      outer.style.MozBinding = "";

      // Reset @remote since this is now back to a regular, non-remote browser
      outer.setAttribute("remote", "false");
      outer.removeAttribute("remoteType");

      // Delete browser window properties exposed on content's owner global
      delete inner.ownerGlobal.PopupNotifications;
      delete inner.ownerGlobal.whereToOpenLink;

      // Remove mozbrowser event handlers
      inner.removeEventListener("mozbrowseropenwindow", this);

      mmTunnel.destroy();
      mmTunnel = null;

      // Reset overridden XBL properties and methods.  Deleting the override
      // means it will fallback to the original XBL binding definitions which
      // are on the prototype.
      delete outer.frameLoader;
      delete outer[FRAME_LOADER];
      delete outer.outerWindowID;

      // Invalidate outer's permanentKey so that SessionStore stops associating
      // things that happen to the outer browser with the content inside in the
      // inner browser.
      outer.permanentKey = { id: "zombie" };

      browserWindow = null;
      gBrowser = null;
    },

  };
}

exports.tunnelToInnerBrowser = tunnelToInnerBrowser;

/**
 * This module allows specific messages of interest to be directed from the
 * outer browser to the inner browser (and vice versa) in a targetted fashion
 * without having to touch the original code paths that use them.
 */
function MessageManagerTunnel(outer, inner) {
  if (outer.isRemoteBrowser) {
    throw new Error("The outer browser must be non-remote.");
  }
  this.outer = outer;
  this.inner = inner;
  this.tunneledMessageNames = new Set();
  this.init();
}

MessageManagerTunnel.prototype = {

  /**
   * Most message manager methods are left alone and are just passed along to
   * the outer browser's real message manager.
   */
  PASS_THROUGH_METHODS: [
    "removeDelayedFrameScript",
    "getDelayedFrameScripts",
    "loadProcessScript",
    "removeDelayedProcessScript",
    "getDelayedProcessScripts",
    "addWeakMessageListener",
    "removeWeakMessageListener",
  ],

  /**
   * The following methods are overridden with special behavior while tunneling.
   */
  OVERRIDDEN_METHODS: [
    "loadFrameScript",
    "addMessageListener",
    "removeMessageListener",
    "sendAsyncMessage",
  ],

  OUTER_TO_INNER_MESSAGES: [
    // Messages sent from remote-browser.xml
    "Browser:PurgeSessionHistory",
    "InPermitUnload",
    "PermitUnload",
    // Messages sent from browser.js
    "Browser:Reload",
    // Messages sent from SelectParentHelper.jsm
    "Forms:DismissedDropDown",
    "Forms:MouseOut",
    "Forms:MouseOver",
    "Forms:SelectDropDownItem",
    // Messages sent from SessionStore.jsm
    "SessionStore:flush",
  ],

  INNER_TO_OUTER_MESSAGES: [
    // Messages sent to RemoteWebProgress.jsm
    "Content:LoadURIResult",
    "Content:LocationChange",
    "Content:ProgressChange",
    "Content:SecurityChange",
    "Content:StateChange",
    "Content:StatusChange",
    // Messages sent to remote-browser.xml
    "DOMTitleChanged",
    "ImageDocumentLoaded",
    "Forms:ShowDropDown",
    "Forms:HideDropDown",
    "InPermitUnload",
    "PermitUnload",
    // Messages sent to tabbrowser.xml
    "contextmenu",
    // Messages sent to SelectParentHelper.jsm
    "Forms:UpdateDropDown",
    // Messages sent to browser.js
    "PageVisibility:Hide",
    "PageVisibility:Show",
    // Messages sent to SessionStore.jsm
    "SessionStore:update",
    // Messages sent to BrowserTestUtils.jsm
    "browser-test-utils:loadEvent",
  ],

  OUTER_TO_INNER_MESSAGE_PREFIXES: [
    // Messages sent from nsContextMenu.js
    "ContextMenu:",
    // Messages sent from DevTools
    "debug:",
    // Messages sent from findbar.xml
    "Findbar:",
    // Messages sent from RemoteFinder.jsm
    "Finder:",
    // Messages sent from InlineSpellChecker.jsm
    "InlineSpellChecker:",
    // Messages sent from pageinfo.js
    "PageInfo:",
    // Messages sent from printUtils.js
    "Printing:",
    // Messages sent from browser-social.js
    "Social:",
    "PageMetadata:",
    // Messages sent from viewSourceUtils.js
    "ViewSource:",
  ],

  INNER_TO_OUTER_MESSAGE_PREFIXES: [
    // Messages sent to nsContextMenu.js
    "ContextMenu:",
    // Messages sent to DevTools
    "debug:",
    // Messages sent to findbar.xml
    "Findbar:",
    // Messages sent to RemoteFinder.jsm
    "Finder:",
    // Messages sent to pageinfo.js
    "PageInfo:",
    // Messages sent to printUtils.js
    "Printing:",
    // Messages sent to browser-social.js
    "Social:",
    "PageMetadata:",
    // Messages sent to viewSourceUtils.js
    "ViewSource:",
  ],

  OUTER_TO_INNER_FRAME_SCRIPTS: [
    // DevTools server for OOP frames
    "resource://devtools/server/child.js"
  ],

  get outerParentMM() {
    if (!this.outer[FRAME_LOADER]) {
      return null;
    }
    return this.outer[FRAME_LOADER].messageManager;
  },

  get outerChildMM() {
    // This is only possible because we require the outer browser to be
    // non-remote, so we're able to reach into its window and use the child
    // side message manager there.
    let docShell = this.outer[FRAME_LOADER].docShell;
    return docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                   .getInterface(Ci.nsIContentFrameMessageManager);
  },

  get innerParentMM() {
    if (!this.inner.frameLoader) {
      return null;
    }
    return this.inner.frameLoader.messageManager;
  },

  init() {
    for (let method of this.PASS_THROUGH_METHODS) {
      this[method] = (...args) => {
        if (!this.outerParentMM) {
          return null;
        }
        return this.outerParentMM[method](...args);
      };
    }

    for (let name of this.INNER_TO_OUTER_MESSAGES) {
      this.innerParentMM.addMessageListener(name, this);
      this.tunneledMessageNames.add(name);
    }

    Services.obs.addObserver(this, "message-manager-close");

    // Replace the outer browser's messageManager with this tunnel
    Object.defineProperty(this.outer, "messageManager", {
      value: this,
      writable: false,
      configurable: true,
      enumerable: true,
    });
  },

  destroy() {
    if (this.destroyed) {
      return;
    }
    this.destroyed = true;
    debug("Destroy tunnel");

    // Watch for the messageManager to close.  In most cases, the caller will stop the
    // tunnel gracefully before this, but when the browser window closes or application
    // exits, we may not see the high-level close events.
    Services.obs.removeObserver(this, "message-manager-close");

    // Reset the messageManager.  Deleting the override means it will fallback to the
    // original XBL binding definitions which are on the prototype.
    delete this.outer.messageManager;

    for (let name of this.tunneledMessageNames) {
      this.innerParentMM.removeMessageListener(name, this);
    }

    // Some objects may have cached this tunnel as the messageManager for a frame.  To
    // ensure it keeps working after tunnel close, rewrite the overidden methods as pass
    // through methods.
    for (let method of this.OVERRIDDEN_METHODS) {
      this[method] = (...args) => {
        if (!this.outerParentMM) {
          return null;
        }
        return this.outerParentMM[method](...args);
      };
    }
  },

  observe(subject, topic, data) {
    if (topic != "message-manager-close") {
      return;
    }
    if (subject == this.innerParentMM) {
      debug("Inner messageManager has closed");
      this.destroy();
    }
    if (subject == this.outerParentMM) {
      debug("Outer messageManager has closed");
      this.destroy();
    }
  },

  loadFrameScript(url, ...args) {
    debug(`Calling loadFrameScript for ${url}`);

    if (!this.OUTER_TO_INNER_FRAME_SCRIPTS.includes(url)) {
      debug(`Should load ${url} into inner?`);
      this.outerParentMM.loadFrameScript(url, ...args);
      return;
    }

    debug(`Load ${url} into inner`);
    this.innerParentMM.loadFrameScript(url, ...args);
  },

  addMessageListener(name, ...args) {
    debug(`Calling addMessageListener for ${name}`);

    debug(`Add outer listener for ${name}`);
    // Add an outer listener, just like a simple pass through
    this.outerParentMM.addMessageListener(name, ...args);

    // If the message name is part of a prefix we're tunneling, we also need to add the
    // tunnel as an inner listener.
    if (this.INNER_TO_OUTER_MESSAGE_PREFIXES.some(prefix => name.startsWith(prefix))) {
      debug(`Add inner listener for ${name}`);
      this.innerParentMM.addMessageListener(name, this);
      this.tunneledMessageNames.add(name);
    }
  },

  removeMessageListener(name, ...args) {
    debug(`Calling removeMessageListener for ${name}`);

    debug(`Remove outer listener for ${name}`);
    // Remove an outer listener, just like a simple pass through
    this.outerParentMM.removeMessageListener(name, ...args);

    // Leave the tunnel as an inner listener for the case of prefix messages to avoid
    // tracking counts of add calls.  The inner listener will get removed on destroy.
  },

  sendAsyncMessage(name, ...args) {
    debug(`Calling sendAsyncMessage for ${name}`);

    if (!this._shouldTunnelOuterToInner(name)) {
      debug(`Should ${name} go to inner?`);
      this.outerParentMM.sendAsyncMessage(name, ...args);
      return;
    }

    debug(`${name} outer -> inner`);
    this.innerParentMM.sendAsyncMessage(name, ...args);
  },

  receiveMessage({ name, data, objects, principal, sync }) {
    if (!this._shouldTunnelInnerToOuter(name)) {
      debug(`Received unexpected message ${name}`);
      return undefined;
    }

    debug(`${name} inner -> outer, sync: ${sync}`);
    if (sync) {
      return this.outerChildMM.sendSyncMessage(name, data, objects, principal);
    }
    this.outerChildMM.sendAsyncMessage(name, data, objects, principal);
    return undefined;
  },

  _shouldTunnelOuterToInner(name) {
    return this.OUTER_TO_INNER_MESSAGES.includes(name) ||
           this.OUTER_TO_INNER_MESSAGE_PREFIXES.some(prefix => name.startsWith(prefix));
  },

  _shouldTunnelInnerToOuter(name) {
    return this.INNER_TO_OUTER_MESSAGES.includes(name) ||
           this.INNER_TO_OUTER_MESSAGE_PREFIXES.some(prefix => name.startsWith(prefix));
  },

};
