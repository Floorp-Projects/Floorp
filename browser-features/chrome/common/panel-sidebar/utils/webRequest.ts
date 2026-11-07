/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { isWebPanelChildUrl } from "./web-panel-context.ts";

const { BrowserWindowTracker } = ChromeUtils.importESModule(
  "resource:///modules/BrowserWindowTracker.sys.mjs",
);

// Mobile User-Agent string for Android
const MOBILE_UA =
  "Mozilla/5.0 (Linux; Android 6.0; Nexus 5 Build/MRA58N) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/114.0.0.0 Mobile Safari/537.36 Edg/114.0.1823.79";

/** Own the HTTP override in the parent window, never in embedded panel realms. */
export function observeWebPanelRequests(ownerWindow: Window): () => void {
  if (
    ownerWindow.closed || ownerWindow.floorpWebPanelWindow ||
    isWebPanelChildUrl(ownerWindow.location.href)
  ) return () => {};

  const topic = "http-on-modify-request";
  const httpRequestObserver = {
    QueryInterface: ChromeUtils.generateQI(["nsIObserver"]),
    observe(subject: nsISupports, observedTopic: string): void {
      if (
        observedTopic !== topic || !(subject instanceof Ci.nsIHttpChannel)
      ) return;

      const channel = subject as nsIHttpChannel;
      // Each parent owns only its own panels, including panels hidden for reuse.
      if (!getWebPanelWindowByBrowserId(channel.browserId, [ownerWindow])) {
        return;
      }
      try {
        channel.setRequestHeader("User-Agent", MOBILE_UA, false);
      } catch (error) {
        console.error("[PanelSidebar] Failed to set User-Agent:", error);
      }
    },
  };

  let observing = true;
  const dispose = () => {
    if (!observing) return;
    observing = false;
    ownerWindow.removeEventListener("unload", dispose);
    Services.obs.removeObserver(httpRequestObserver, topic);
  };
  Services.obs.addObserver(httpRequestObserver, topic, false);
  // Native window close and Solid/HMR cleanup may both call this disposer.
  ownerWindow.addEventListener("unload", dispose, { once: true });
  return dispose;
}

export function getWebPanelWindowByBrowserId(
  browserId: number,
  windows: readonly Window[] = BrowserWindowTracker.orderedWindows,
): Window | null {
  // A missing ID must never match an uninitialized browser.
  if (!Number.isSafeInteger(browserId) || browserId <= 0) return null;
  for (const win of windows) {
    if (win.closed) continue;
    if (
      win.floorpWebPanelWindow && win.floorpBmsUserAgent &&
      win.floorpWebPanelContentBrowser?.isConnected !== false &&
      win.floorpWebPanelContentBrowser?.browserId === browserId
    ) return win;

    // Web panels embed browser.xhtml inside a sidebar <browser>; those child
    // windows are not necessarily in BrowserWindowTracker. Match the inner
    // content browser, never the sidebar host or a normal browser tab.
    for (
      const host of win.document.querySelectorAll(".sidebar-panel-browser")
    ) {
      const child = (host as XULBrowserElement).browsingContext
        ?.associatedWindow as Window | undefined;
      if (
        child?.floorpWebPanelWindow && !child.closed &&
        child.floorpBmsUserAgent &&
        child.floorpWebPanelContentBrowser?.isConnected !== false &&
        child.floorpWebPanelContentBrowser?.browserId === browserId
      ) return child;
    }
  }
  return null;
}
