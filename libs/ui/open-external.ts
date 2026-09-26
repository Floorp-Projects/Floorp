// SPDX-License-Identifier: MPL-2.0
//
// Opening an external link from an internal page (about:welcome, about:hub)
// cannot rely on the browser's default <a target="_blank"> handling: these
// pages are registered as system-principal custom about pages
// (CustomAboutPage in NoranekoStartup.sys.mts), and a plain anchor click opens
// an about:blank tab instead of the URL. Opening the tab through the browser
// window with an explicit system triggeringPrincipal is what Firefox's own
// chrome pages do (openTrustedLinkIn / moz-support-link).
//
// The page runs in a content process, so it cannot reach the browser window
// itself. The child actors (NRSettingsChild / NRWelcomePageChild) export
// `NROpenExternalLink` into the page, which asks the parent process to open the
// tab. See https://github.com/Floorp-Projects/Floorp/issues/2787.

declare const Services: {
  wm: {
    getMostRecentWindow(aWindowType: string): unknown;
  };
  scriptSecurityManager: {
    getSystemPrincipal(): unknown;
  };
};

interface TrustedLinkBrowserWindow {
  openTrustedLinkIn(
    url: string,
    where: string,
    params: {
      triggeringPrincipal: unknown;
      relatedToCurrent: boolean;
      allowInheritPrincipal: boolean;
    },
  ): void;
}

/**
 * Open `url` in a new tab.
 *
 * @returns true when the link was handed off, false when no opener is reachable
 *   so the caller should let the default anchor behaviour run.
 */
export function openExternalLink(url: string): boolean {
  if (!url) {
    return false;
  }

  // Preferred: the child actor exported by NRSettingsChild / NRWelcomePageChild,
  // which opens the tab in the parent process.
  const g = globalThis as unknown as Record<string, unknown>;
  const bridge = g.NROpenExternalLink;
  if (typeof bridge === "function") {
    try {
      (bridge as (u: string) => void)(url);
      return true;
    } catch {
      // fall through
    }
  }

  // Fallback: when the page happens to have direct access to the browser window
  // (e.g. a privileged page running in the parent process).
  try {
    const win = Services.wm.getMostRecentWindow(
      "navigator:browser",
    ) as TrustedLinkBrowserWindow | null;
    if (!win || typeof win.openTrustedLinkIn !== "function") {
      return false;
    }
    win.openTrustedLinkIn(url, "tab", {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
      relatedToCurrent: true,
      allowInheritPrincipal: false,
    });
    return true;
  } catch {
    return false;
  }
}

/**
 * Route external `target="_blank"` link clicks on an internal page through the
 * parent process. Installs one capturing listener; safe to call multiple times.
 */
export function installExternalLinkHandler(): void {
  const g = globalThis as unknown as Record<string, unknown>;
  if (g.__floorpExternalLinkHandlerInstalled) {
    return;
  }
  g.__floorpExternalLinkHandlerInstalled = true;

  document.addEventListener(
    "click",
    (event) => {
      if (event.defaultPrevented || event.button !== 0) {
        return;
      }
      if (event.metaKey || event.ctrlKey || event.shiftKey || event.altKey) {
        return;
      }
      const path = (event.composedPath?.() ?? []) as EventTarget[];
      const anchor = path.find(
        (node): node is HTMLAnchorElement =>
          node instanceof HTMLAnchorElement,
      );
      if (!anchor || anchor.target !== "_blank") {
        return;
      }
      // Only web links need the chrome-side open; about:/chrome: targets work
      // with the default handling.
      if (!/^https?:$/i.test(anchor.protocol)) {
        return;
      }
      if (openExternalLink(anchor.href)) {
        event.preventDefault();
      }
    },
    true,
  );
}
