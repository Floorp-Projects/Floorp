/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  type Accessor,
  createEffect,
  createSignal,
  onCleanup,
  type Setter,
} from "solid-js";
import type {} from "solid-styled-jsx";

export class StatusBarManager {
  _showStatusBar = createSignal(
    Services.prefs.getBoolPref("noraneko.statusbar.enable", false),
  );
  showStatusBar = this._showStatusBar[0];
  setShowStatusBar = this._showStatusBar[1];
  constructor() {
    //? this effect will not called when pref is changed to same value.
    createEffect(() => {
      console.log("solid to pref");
      Services.prefs.setBoolPref(
        "noraneko.statusbar.enable",
        this.showStatusBar(),
      );
      Services.prefs.addObserver(
        "noraneko.statusbar.enable",
        this.observerStatusbarPref,
      );
      onCleanup(() => {
        Services.prefs.removeObserver(
          "noraneko.statusbar.enable",
          this.observerStatusbarPref,
        );
      });
    });

    if (!window.gFloorp) {
      window.gFloorp = {};
    }
    window.gFloorp.statusBar = {
      setShow: this.setShowStatusBar,
    };
    onCleanup(() => {
      window.CustomizableUI.unregisterArea("statusBar", true);
    });
  }

  init() {
    window.CustomizableUI.registerArea("statusBar", {
      type: window.CustomizableUI.TYPE_TOOLBAR,
      defaultPlacements: ["screenshot-button", "fullscreen-button"],
    });
    window.CustomizableUI.registerToolbarNode(
      document.getElementById("statusBar"),
    );

    //move elem to bottom of window
    document
      .querySelector("#appcontent")
      ?.appendChild(document.getElementById("statusBar")!);
  }

  //if we use just method, `this` will be broken
  private observerStatusbarPref = () => {
    console.log("pref to solid");
    this.setShowStatusBar((_prev) => {
      return Services.prefs.getBoolPref("noraneko.statusbar.enable");
    });
  };
}
