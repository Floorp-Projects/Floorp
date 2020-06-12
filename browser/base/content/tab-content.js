/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This content script contains code that requires a tab browser. */

/* eslint-env mozilla/frame-script */

var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(
  this,
  "E10SUtils",
  "resource://gre/modules/E10SUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "BrowserUtils",
  "resource://gre/modules/BrowserUtils.jsm"
);

var { ActorManagerChild } = ChromeUtils.import(
  "resource://gre/modules/ActorManagerChild.jsm"
);

ActorManagerChild.attach(this, "browsers");

// BrowserChildGlobal
var global = this;

var WebBrowserChrome = {
  onBeforeLinkTraversal(originalTarget, linkURI, linkNode, isAppTab) {
    return BrowserUtils.onBeforeLinkTraversal(
      originalTarget,
      linkURI,
      linkNode,
      isAppTab
    );
  },

  // Check whether this URI should load in the current process
  shouldLoadURI(
    aDocShell,
    aURI,
    aReferrerInfo,
    aHasPostData,
    aTriggeringPrincipal,
    aCsp
  ) {
    if (!E10SUtils.shouldLoadURI(aDocShell, aURI, aHasPostData)) {
      E10SUtils.redirectLoad(
        aDocShell,
        aURI,
        aReferrerInfo,
        aTriggeringPrincipal,
        false,
        null,
        aCsp
      );
      return false;
    }

    return true;
  },

  shouldLoadURIInThisProcess(aURI) {
    let remoteSubframes = docShell.QueryInterface(Ci.nsILoadContext)
      .useRemoteSubframes;
    return E10SUtils.shouldLoadURIInThisProcess(aURI, remoteSubframes);
  },

  // Try to reload the currently active or currently loading page in a new process.
  reloadInFreshProcess(
    aDocShell,
    aURI,
    aReferrerInfo,
    aTriggeringPrincipal,
    aLoadFlags,
    aCsp
  ) {
    E10SUtils.redirectLoad(
      aDocShell,
      aURI,
      aReferrerInfo,
      aTriggeringPrincipal,
      true,
      aLoadFlags,
      aCsp
    );
    return true;
  },
};

if (Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_CONTENT) {
  let tabchild = docShell
    .QueryInterface(Ci.nsIInterfaceRequestor)
    .getInterface(Ci.nsIBrowserChild);
  tabchild.webBrowserChrome = WebBrowserChrome;
}

Services.obs.notifyObservers(this, "tab-content-frameloader-created");

// This is a temporary hack to prevent regressions (bug 1471327).
void content;
