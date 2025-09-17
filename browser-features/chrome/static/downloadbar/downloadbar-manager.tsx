// SPDX-License-Identifier: MPL-2.0

import { createEffect, createSignal } from "solid-js";
import type {} from "solid-styled-jsx";

export class DownloadBarManager {
  _showDownloadBar = createSignal(
    Services.prefs.getBoolPref("noraneko.downloadbar.enable", false),
  );
  showDownloadBar = this._showDownloadBar[0];
  setShowDownloadBar = this._showDownloadBar[1];
  constructor() {
    //? this effect will not called when pref is changed to same value.
    Services.prefs.addObserver(
      "noraneko.downloadbar.enable",
      this.observerDownloadbarPref,
    );
    if (!window.gFloorp) {
      window.gFloorp = {};
    }
    window.gFloorp.downloadBar = {
      setShow: this.setShowDownloadBar,
    };
  }

  init() {
    createEffect(() => {
      Services.prefs.setBoolPref(
        "noraneko.downloadbar.enable",
        this.showDownloadBar(),
      );
    });
    //move elem to bottom of window
    document
      .querySelector("#tabbrowser-tabbox")
      ?.appendChild(document.getElementById("downloadsPanel")!);
  }

  //if we use just method, `this` will be broken
  private observerDownloadbarPref = () => {
    this.setShowDownloadBar((_prev) => {
      return Services.prefs.getBoolPref("noraneko.downloadbar.enable");
    });
  };
}
