/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createEffect, createSignal, onCleanup } from "solid-js";
import type {} from "solid-styled-jsx";

export class StatusBarManager {
  _showStatusBar = createSignal(
    Services.prefs.getBoolPref("noraneko.statusbar.enable", false),
  );
  showStatusBar = this._showStatusBar[0];
  setShowStatusBar = this._showStatusBar[1];
  constructor() {
    //? this effect will not called when pref is changed to same value.
    Services.prefs.addObserver(
      "noraneko.statusbar.enable",
      this.observerStatusbarPref,
    );
    createEffect(() => {
      console.log("solid to pref");
      Services.prefs.setBoolPref(
        "noraneko.statusbar.enable",
        this.showStatusBar(),
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
      window.CustomizableUI.unregisterArea("nora-statusbar", true);
    });
  }

  init() {
    window.CustomizableUI.registerArea("nora-statusbar", {
      type: window.CustomizableUI.TYPE_TOOLBAR,
      defaultPlacements: ["screenshot-button", "fullscreen-button"],
    });

    window.CustomizableUI.registerToolbarNode(
      document.getElementById("nora-statusbar"),
    );
    window.CustomizableUI.addWidgetToArea("zoom-controls", "nora-statusbar", 1);

    //move elem to bottom of window
    document
      .querySelector("#appcontent")
      ?.appendChild(document.getElementById("nora-statusbar")!);

    createEffect(() => {
      const statuspanel_label = document?.querySelector("#statuspanel-label");
      const statuspanel = document?.querySelector<XULElement>("#statuspanel");
      const statusText = document.querySelector<XULElement>("#status-text");
      const observer = new MutationObserver(() => {
        if (statuspanel.getAttribute("inactive") === "true" && statusText) {
          statusText.setAttribute("hidden", "true");
        } else {
          statusText?.removeAttribute("hidden");
        }
      });
      if (this.showStatusBar()) {
        statusText?.appendChild(statuspanel_label!);
        observer.observe(statuspanel, { attributes: true });
      } else {
        statuspanel?.appendChild(statuspanel_label!);
        observer?.disconnect();
      }
    });
  }

  //if we use just method, `this` will be broken
  private observerStatusbarPref = () => {
    console.log("pref to solid");
    this.setShowStatusBar((_prev) => {
      return Services.prefs.getBoolPref("noraneko.statusbar.enable");
    });
  };
}
