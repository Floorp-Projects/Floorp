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
  let urlbarObserver: MutationObserver | null = null;

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

  /** Whether the urlbar is currently open (showing the urlbarView dropdown). */
  const isUrlbarOpen = (): boolean => {
    const urlbar = document!.getElementById("urlbar");
    return urlbar !== null && urlbar.hasAttribute("open");
  };

  /** Attempt to hide the top chrome; reschedule while a menupopup or urlbar is open. */
  const tryHideTop = () => {
    if (!zenModeEnabled()) {
      topHideTimer = null;
      return;
    }
    if (isToolbarPopupOpen() || isUrlbarOpen()) {
      // A menupopup or urlbarView is still open — keep the toolbox visible
      // and re-check after another delay.
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

  // ===== URL bar focus / open handling =====
  // Reveal the toolbox when the URL bar gains focus (e.g. Ctrl+L) and keep
  // it visible while the urlbarView dropdown is open.
  // Use focusin/focusout (bubbling) on the document so we also catch focus on
  // the inner <input> element inside html:moz-urlbar.

  const isOrContainsUrlbar = (target: EventTarget | null): boolean => {
    if (!(target instanceof Element)) return false;
    const urlbar = document!.getElementById("urlbar");
    return urlbar !== null && (urlbar === target || urlbar.contains(target));
  };

  const handleUrlbarFocusIn = (event: FocusEvent) => {
    if (!zenModeEnabled()) return;
    if (!isOrContainsUrlbar(event.target)) return;
    clearTopTimer();
    document!.documentElement!.setAttribute("zenmode-reveal-top", "");
  };

  const handleUrlbarFocusOut = (event: FocusEvent) => {
    if (!zenModeEnabled()) return;
    if (!isOrContainsUrlbar(event.target)) return;
    // Don't hide immediately — the urlbarView may still be open.  Schedule
    // a delayed hide; tryHideTop will keep rescheduling while [open] is set.
    clearTopTimer();
    topHideTimer = setTimeout(tryHideTop, HIDE_DELAY_MS);
  };

  const setupUrlbarListeners = () => {
    const urlbar = document!.getElementById("urlbar");
    if (!urlbar) return;

    // Listen on document so bubbling focusin/focusout from the inner <input>
    // are captured even when focus lands on a descendant of #urlbar.
    document!.addEventListener("focusin", handleUrlbarFocusIn);
    document!.addEventListener("focusout", handleUrlbarFocusOut);

    // Watch for the [open] attribute so we can schedule a hide when the
    // urlbarView dropdown closes while the input itself stays focused.
    urlbarObserver = new MutationObserver((mutations) => {
      for (const mutation of mutations) {
        if (
          mutation.type === "attributes" &&
          mutation.attributeName === "open"
        ) {
          // [open] was removed → urlbarView closed
          if (!urlbar.hasAttribute("open") && zenModeEnabled()) {
            clearTopTimer();
            topHideTimer = setTimeout(tryHideTop, HIDE_DELAY_MS);
          }
        }
      }
    });
    urlbarObserver.observe(urlbar, {
      attributes: true,
      attributeFilter: ["open"],
    });
  };

  setupUrlbarListeners();

  addEventListener("mousemove", handleMouseMove);

  onCleanup(() => {
    removeEventListener("mousemove", handleMouseMove);

    // Clean up urlbar listeners
    document!.removeEventListener("focusin", handleUrlbarFocusIn);
    document!.removeEventListener("focusout", handleUrlbarFocusOut);
    if (urlbarObserver) {
      urlbarObserver.disconnect();
      urlbarObserver = null;
    }

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
