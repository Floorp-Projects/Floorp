// SPDX-License-Identifier: MPL-2.0

import { render } from "@nora/solid-xul";
import { DonwloadBar } from "./downloadbar";
import { DownloadBarManager } from "./downloadbar-manager";
import { createRoot } from "solid-js";

export let manager: DownloadBarManager;

// THIS CANNOT BE HOT RELOADED
// TODO: REMOVE ALL CREATE_ROOT_HMR

export function init() {
  createRoot(() => {
    manager = new DownloadBarManager();

    manager.init();
    // console.log(manager.showDownloadBar());
    if (!manager.showDownloadBar()) {
      return;
    }
    document.getElementById("downloadsPanel")?.remove();
    render(DonwloadBar, document.getElementById("appcontent"));
    console.log("init download bar");
    globalThis.DownloadsPanel.hidePanel = () => {
      return;
    };
    delete globalThis.DownloadsView.contextMenu;
    delete globalThis.DownloadsPanel.panel;
    delete globalThis.DownloadsPanel.richListBox;
    globalThis.DownloadsPanel.panel =
      document.getElementById("downloadsPanel") ?? undefined;
    globalThis.DownloadsPanel.richListBox =
      document.getElementById("downloadsListBox") ?? undefined;
    globalThis.DownloadsView.contextMenu =
      document.getElementById("downloadsContextMenu") ?? undefined;
    globalThis.DownloadsPanel._initialized = false;
    globalThis.DownloadsPanel.initialize?.();
    globalThis.DownloadsView.onDownloadAdded = (download) => {
      document.getElementById("downloadsListBox")!.scrollLeft = 0;
      DownloadsView.onDownloadAdded_hook?.(download);
    };
    const scrollElem = document.getElementById("downloadsListBox");
    scrollElem?.addEventListener("wheel", (e: WheelEvent) => {
      if (Math.abs(e.deltaY) < Math.abs(e.deltaX)) {
        return;
      }
      e.preventDefault();
      scrollElem.scrollLeft += e.deltaY * 10;
    });
  });
}
