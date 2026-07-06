// SPDX-License-Identifier: MPL-2.0

import { signal, effect } from "@preact/signals";

export class DownloadBarManager {
  showDownloadBar = signal(
    Services.prefs.getBoolPref("noraneko.downloadbar.enable", false),
  );
  constructor() {
    //? this effect will not called when pref is changed to same value.
    Services.prefs.addObserver(
      "noraneko.downloadbar.enable",
      this.observerDownloadbarPref,
    );
    if (!globalThis.gFloorp) {
      globalThis.gFloorp = {};
    }
    globalThis.gFloorp.downloadBar = {
      setShow: (v: boolean) => {
        this.showDownloadBar.value = v;
      },
    };
  }

  init() {
    effect(() => {
      Services.prefs.setBoolPref(
        "noraneko.downloadbar.enable",
        this.showDownloadBar.value,
      );
    });
    //move elem to bottom of window
    document
      .querySelector("#tabbrowser-tabbox")
      ?.appendChild(document.getElementById("downloadsPanel")!);
  }

  //if we use just method, `this` will be broken
  private observerDownloadbarPref = () => {
    this.showDownloadBar.value = Services.prefs.getBoolPref(
      "noraneko.downloadbar.enable",
    );
  };
}
