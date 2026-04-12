/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createEffect, createSignal, onCleanup } from "solid-js";
import { Show } from "solid-js";
import { createRootHMR } from "@nora/solid-xul";
import { addI18nObserver } from "#i18n/config-browser-chrome.ts";
import i18next from "i18next";
import zenModeStyle from "./zen-mode.css?inline";

const ZENMODE_PREF = "floorp.zenmode.enabled";

const EDGE_THRESHOLD = 10;
const HIDE_DELAY_MS = 500;

export const [zenModeEnabled, setZenModeEnabled] = createRootHMR(
  () =>
    createSignal(
      typeof Services !== "undefined"
        ? Services.prefs.getBoolPref(ZENMODE_PREF, false)
        : false,
    ),
  import.meta.hot,
);

function measureAndSetCSSVariables() {
  const root = document!.documentElement as HTMLElement;

  const toolbox = document!.getElementById("navigator-toolbox");
  if (toolbox) {
    root.style.setProperty(
      "--zenmode-toolbox-height",
      `${toolbox.getBoundingClientRect().height}px`,
    );
  }

  const sidebar = document!.getElementById("panel-sidebar-box");
  if (sidebar) {
    root.style.setProperty(
      "--zenmode-sidebar-width",
      `${sidebar.getBoundingClientRect().width}px`,
    );
  }

  const selectBox = document!.getElementById("panel-sidebar-select-box");
  if (selectBox) {
    root.style.setProperty(
      "--zenmode-selectbox-width",
      `${selectBox.getBoundingClientRect().width}px`,
    );
  }

  const statusbar = document!.getElementById("nora-statusbar");
  if (statusbar) {
    root.style.setProperty(
      "--zenmode-statusbar-height",
      `${statusbar.getBoundingClientRect().height}px`,
    );
  }
}

function setupPrefSync() {
  createEffect(() => {
    const enabled = zenModeEnabled();
    Services.prefs.setBoolPref(ZENMODE_PREF, enabled);

    if (enabled) {
      // Measure element sizes before hiding so animations have correct distances
      measureAndSetCSSVariables();
      document!.documentElement!.setAttribute("zenmode", "");
    } else {
      document!.documentElement!.removeAttribute("zenmode");
      document!.documentElement!.removeAttribute("zenmode-reveal-top");
      document!.documentElement!.removeAttribute("zenmode-reveal-bottom");
      document!.documentElement!.removeAttribute("zenmode-reveal-side");
    }
  });

  const observer = () => {
    const prefValue = Services.prefs.getBoolPref(ZENMODE_PREF, false);
    if (prefValue !== zenModeEnabled()) {
      setZenModeEnabled(prefValue);
    }
  };
  Services.prefs.addObserver(ZENMODE_PREF, observer);
  onCleanup(() => {
    Services.prefs.removeObserver(ZENMODE_PREF, observer);
  });
}

/**
 * Check whether a menupopup inside the navigator-toolbox (e.g. a menu-bar
 * dropdown) is currently open.  These popups are rendered as native OS-level
 * overlays, so they stay visible even after the toolbox is hidden with
 * opacity/pointer-events — producing the "ghost dropdown" artifact described
 * in issue #2374.
 */
function isToolbarPopupOpen(): boolean {
  const toolbox = document!.getElementById("navigator-toolbox");
  if (!toolbox) return false;
  // menupopup: menu-bar dropdowns (File, Edit, View…)
  // panel: toolbar button popups (appMenu, hamburger menu, etc.)
  return (
    toolbox.querySelector("menupopup[open]") !== null ||
    toolbox.querySelector("panel[open]") !== null
  );
}

function setupHoverReveal() {
  let topHideTimer: ReturnType<typeof setTimeout> | null = null;
  let bottomHideTimer: ReturnType<typeof setTimeout> | null = null;
  let sideHideTimer: ReturnType<typeof setTimeout> | null = null;

  const clearTopTimer = () => {
    if (topHideTimer !== null) {
      clearTimeout(topHideTimer);
      topHideTimer = null;
    }
  };

  const clearBottomTimer = () => {
    if (bottomHideTimer !== null) {
      clearTimeout(bottomHideTimer);
      bottomHideTimer = null;
    }
  };

  const clearSideTimer = () => {
    if (sideHideTimer !== null) {
      clearTimeout(sideHideTimer);
      sideHideTimer = null;
    }
  };

  /** Attempt to hide the top chrome; reschedule while a menupopup is open. */
  const tryHideTop = () => {
    if (!zenModeEnabled()) {
      topHideTimer = null;
      return;
    }
    if (isToolbarPopupOpen()) {
      // A menu-bar dropdown is still open — keep the toolbox visible and
      // re-check after another delay so we don't produce ghost graphics.
      topHideTimer = setTimeout(tryHideTop, HIDE_DELAY_MS);
      return;
    }
    document!.documentElement!.removeAttribute("zenmode-reveal-top");
    topHideTimer = null;
  };

  const handleMouseMove = (event: MouseEvent) => {
    if (!zenModeEnabled()) return;

    const { clientX, clientY } = event;
    const windowWidth = innerWidth;
    const windowHeight = innerHeight;

    // Top edge detection
    if (clientY <= EDGE_THRESHOLD) {
      clearTopTimer();
      document!.documentElement!.setAttribute("zenmode-reveal-top", "");
    } else if (
      document!.documentElement!.hasAttribute("zenmode-reveal-top")
    ) {
      const navigatorToolbox = document!.getElementById("navigator-toolbox");
      if (navigatorToolbox) {
        const rect = navigatorToolbox.getBoundingClientRect();
        if (clientY > rect.bottom) {
          clearTopTimer();
          topHideTimer = setTimeout(tryHideTop, HIDE_DELAY_MS);
        } else {
          clearTopTimer();
        }
      }
    }

    // Bottom edge detection
    if (clientY >= windowHeight - EDGE_THRESHOLD) {
      clearBottomTimer();
      document!.documentElement!.setAttribute("zenmode-reveal-bottom", "");
    } else if (
      document!.documentElement!.hasAttribute("zenmode-reveal-bottom")
    ) {
      const statusbar = document!.getElementById("nora-statusbar");
      if (statusbar) {
        const rect = statusbar.getBoundingClientRect();
        if (clientY < rect.top) {
          clearBottomTimer();
          bottomHideTimer = setTimeout(() => {
            document!.documentElement!.removeAttribute("zenmode-reveal-bottom");
            bottomHideTimer = null;
          }, HIDE_DELAY_MS);
        } else {
          clearBottomTimer();
        }
      }
    }

    // Left/right edge detection for panel sidebar
    if (clientX <= EDGE_THRESHOLD || clientX >= windowWidth - EDGE_THRESHOLD) {
      clearSideTimer();
      document!.documentElement!.setAttribute("zenmode-reveal-side", "");
    } else if (
      document!.documentElement!.hasAttribute("zenmode-reveal-side")
    ) {
      const panelSidebar = document!.getElementById("panel-sidebar-box");
      const panelSelectBox = document!.getElementById(
        "panel-sidebar-select-box",
      );
      const insideSidebar = (panelSidebar &&
        clientX >= panelSidebar.getBoundingClientRect().left &&
        clientX <= panelSidebar.getBoundingClientRect().right) ||
        (panelSelectBox &&
          clientX >= panelSelectBox.getBoundingClientRect().left &&
          clientX <= panelSelectBox.getBoundingClientRect().right);

      if (!insideSidebar) {
        clearSideTimer();
        sideHideTimer = setTimeout(() => {
          document!.documentElement!.removeAttribute("zenmode-reveal-side");
          sideHideTimer = null;
        }, HIDE_DELAY_MS);
      } else {
        clearSideTimer();
      }
    }
  };

  addEventListener("mousemove", handleMouseMove);

  onCleanup(() => {
    removeEventListener("mousemove", handleMouseMove);
    clearTopTimer();
    clearBottomTimer();
    clearSideTimer();
    document!.documentElement!.removeAttribute("zenmode");
    document!.documentElement!.removeAttribute("zenmode-reveal-top");
    document!.documentElement!.removeAttribute("zenmode-reveal-bottom");
    document!.documentElement!.removeAttribute("zenmode-reveal-side");
  });
}

export function initZenModeState() {
  setupPrefSync();
  setupHoverReveal();
}

export function ZenModeMenuElement() {
  const [label, setLabel] = createSignal(
    i18next.t("zen-mode.menu-label", { defaultValue: "Toggle Zen Mode" }),
  );

  addI18nObserver(() => {
    setLabel(
      i18next.t("zen-mode.menu-label", { defaultValue: "Toggle Zen Mode" }),
    );
  });

  return (
    <>
      <xul:menuitem
        label={label()}
        type="checkbox"
        id="toggle_zenmode"
        checked={zenModeEnabled()}
        onCommand={() => setZenModeEnabled((prev) => !prev)}
        accesskey="Z"
      />

      <Show when={zenModeEnabled()}>
        <style>{zenModeStyle}</style>
      </Show>
    </>
  );
}
