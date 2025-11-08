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

    const statusbarNode = document?.getElementById("nora-statusbar");
    if (!statusbarNode) {
      console.error(
        "[StatusBarManager] #nora-statusbar element not found; status bar initialization aborted.",
      );
      return;
    }

    window.CustomizableUI.registerToolbarNode(statusbarNode);

    //move elem to bottom of window
    const appContent = document?.querySelector("#appcontent");
    if (!appContent) {
      console.warn(
        "[StatusBarManager] #appcontent not found; status bar will not be moved to bottom.",
      );
    } else {
      appContent.appendChild(statusbarNode);
    }

    createEffect(() => {
      const statuspanelLabel = document?.querySelector("#statuspanel-label");
      const statuspanel = document?.querySelector<XULElement>("#statuspanel") ??
        null;
      const statusText = document?.querySelector<XULElement>("#status-text") ??
        null;

      if (!statuspanelLabel) {
        console.warn(
          "[StatusBarManager] #statuspanel-label not found; skip status text injection.",
        );
        return;
      }

      if (!statuspanel && !statusText) {
        console.warn(
          "[StatusBarManager] Neither #statuspanel nor #status-text found; skip status text handling.",
        );
        return;
      }

      const observer = new MutationObserver(() => {
        if (!statuspanel) {
          observer.disconnect();
          return;
        }
        const isInactive = statuspanel.getAttribute("inactive") === "true";
        if (isInactive) {
          statusText?.setAttribute("hidden", "true");
        } else {
          statusText?.removeAttribute("hidden");
        }
      });
      if (this.showStatusBar()) {
        if (statusText) {
          statusText.appendChild(statuspanelLabel);
        } else {
          statuspanel?.appendChild(statuspanelLabel);
        }
        if (statuspanel) {
          observer.observe(statuspanel, { attributes: true });
        }
      } else {
        statuspanel?.appendChild(statuspanelLabel);
        observer.disconnect();
      }
    });
  }

  //if we use just method, `this` will be broken
  private observerStatusbarPref = () => {
    this.setShowStatusBar((_prev) => {
      return Services.prefs.getBoolPref("noraneko.statusbar.enable");
    });
  };
}
