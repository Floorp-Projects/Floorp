/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

interface BrowserWindowLike {
  gBrowser?: {
    selectedTab?: { label?: string };
    selectedBrowser?: { currentURI?: { spec?: string } };
  };
  openWebLinkIn?: (url: string, where: string) => void;
}

/** A local file, reached from a path the page is holding. */
function localFile(filePath: string): nsIFile | null {
  try {
    const file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
    file.initWithPath(filePath);
    return file;
  } catch (e) {
    console.error("[NRClips] Not a usable path:", filePath, e);
    return null;
  }
}

export class NRClipsParent extends JSWindowActorParent {
  // deno-lint-ignore require-await
  async receiveMessage(message: { name: string; data?: unknown }) {
    const data = message.data as Record<string, unknown> | undefined;
    const pathOf = () =>
      data && typeof data.path === "string" ? data.path : null;

    switch (message.name) {
      case "getActiveTabInfo": {
        const win = this.browserWindow();
        const url = win?.gBrowser?.selectedBrowser?.currentURI?.spec;
        if (!url) return null;
        return { title: win?.gBrowser?.selectedTab?.label ?? "", url };
      }

      case "openLinkInTab": {
        const url = data && typeof data.url === "string" ? data.url : null;
        if (!url) return null;
        // openWebLinkIn only accepts web schemes, so a clip holding
        // javascript: or chrome: cannot get itself opened through here.
        this.browserWindow()?.openWebLinkIn?.(url, "tab");
        return null;
      }

      case "readClipboardText":
        return this.readClipboardText();

      case "fileExists": {
        const filePath = pathOf();
        if (!filePath) return false;
        try {
          return localFile(filePath)?.exists() ?? false;
        } catch {
          return false;
        }
      }

      case "revealFile": {
        const filePath = pathOf();
        if (!filePath) return false;
        try {
          localFile(filePath)?.reveal();
          return true;
        } catch (e) {
          console.error("[NRClips] Failed to reveal:", filePath, e);
          return false;
        }
      }

      case "launchFile": {
        const filePath = pathOf();
        if (!filePath) return false;
        try {
          localFile(filePath)?.launch();
          return true;
        } catch (e) {
          console.error("[NRClips] Failed to launch:", filePath, e);
          return false;
        }
      }

      case "getSessionStartTime":
        return sessionStartTime();
    }
    return null;
  }

  /** The plain text currently on the system clipboard, if there is any. */
  private readClipboardText(): string | null {
    try {
      const trans = Cc["@mozilla.org/widget/transferable;1"].createInstance(
        Ci.nsITransferable,
      );
      // A null load context means "not tied to a browsing context", which is
      // what reading the global clipboard from here is.
      trans.init(null as unknown as nsILoadContext);
      trans.addDataFlavor("text/plain");
      Services.clipboard.getData(
        trans,
        Ci.nsIClipboard.kGlobalClipboard,
        this.browsingContext?.currentWindowContext as WindowContext | undefined,
      );
      const result = { value: null as unknown as nsISupports };
      trans.getTransferData("text/plain", result);
      // The text/plain flavor always comes back as an nsISupportsString.
      const text = (result.value as unknown as nsISupportsString).data;
      return typeof text === "string" && text.length > 0 ? text : null;
    } catch (e) {
      // An empty clipboard, or one holding something that is not text, throws.
      console.debug("[NRClips] Nothing readable on the clipboard:", e);
      return null;
    }
  }

  /** The browser window this page lives in, or the most recent one. */
  private browserWindow(): BrowserWindowLike | null {
    const ctx = this.browsingContext as unknown as {
      topChromeWindow?: BrowserWindowLike | null;
    } | null;
    return (
      ctx?.topChromeWindow ??
      (Services.wm.getMostRecentWindow(
        "navigator:browser",
      ) as BrowserWindowLike | null)
    );
  }
}

/**
 * When this browser session started.
 *
 * Clips is asked to forget unpinned clips when Floorp closes, but the clips
 * live in the page's own storage and the page is not open at shutdown. So the
 * page compares this against the session it last ran in: a different value
 * means Floorp has been restarted since, and the forgetting happens then.
 */
function sessionStartTime(): number {
  try {
    const info = Services.startup.getStartupInfo();
    const started = info.process ?? info.main;
    if (started) return started.getTime();
  } catch (e) {
    console.error("[NRClips] Failed to read the startup time:", e);
  }
  return 0;
}
