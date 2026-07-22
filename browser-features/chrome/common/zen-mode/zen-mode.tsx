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

// Windows always open out of zen. Seeding from the pref meant a new window
// inherited whichever window happened to toggle last, which is not a state
// anyone asked for — and a window that opens with its chrome retracted is
// disorienting when you did not choose it for that window.
export const [zenModeEnabled, setZenModeEnabled] = createRootHMR(
  () => createSignal(false),
  import.meta.hot,
);

function measureAndSetCSSVariables() {
  const root = document!.documentElement as HTMLElement;

  // Only the panel sidebar still uses measured retract distances (its
  // two boxes share one window edge, so the self-sizing transform trick
  // the toolbox/statusbar use doesn't compose for them). Only record
  // elements that are actually rendered: a toggled-off sidebar
  // (display:none) measures 0, and recording that poisons the distances
  // — the side reveal then slides out an empty zero-based husk. A hidden
  // element's vars simply keep their last (or default) value, which is
  // correct: its retract math is moot while it has no box.
  const setIfRendered = (id: string, prop: string, axis: "w" | "h") => {
    const el = document!.getElementById(id);
    if (!el) return;
    const rect = el.getBoundingClientRect();
    const size = axis === "w" ? rect.width : rect.height;
    if (size > 0) {
      root.style.setProperty(prop, `${size}px`);
    }
  };

  setIfRendered("panel-sidebar-box", "--zenmode-sidebar-width", "w");
  setIfRendered("panel-sidebar-select-box", "--zenmode-selectbox-width", "w");
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

  // Deliberately no pref observer. Zen mode is per-window: every window runs
  // its own copy of this feature, so an observer here made each of them mirror
  // the pref and one window entering zen dragged all the others in with it.
  //
  // The pref is still written above, but only as persistence — it seeds the
  // signal when a window is created, so a new window (and the next launch)
  // opens in the state last chosen, without linking live windows together.
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
    // Refresh the sidebar's measured distances opportunistically; the
    // toolbox needs none (it retracts by its own height via transform).
    measureAndSetCSSVariables();
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
            measureAndSetCSSVariables();
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
          measureAndSetCSSVariables();
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

  // Keep the retract distance honest while zen is active: the toolbox's
  // natural height changes under zen (address-bar hide/unhide, stack bar
  // mounting), and a stale --zenmode-toolbox-height either over-pulls
  // the toolbox (page top cut off) or under-pulls it (blank strip above
  // the content). Margins animate, heights don't, so the observer stays
  // quiet during zen's own retract/reveal transitions.
  if (typeof ResizeObserver !== "undefined") {
    const observer = new ResizeObserver(() => {
      if (!zenModeEnabled()) return;
      // Snap to the new distance without playing the slide transition —
      // the chrome is off-screen and the toggle should be invisible.
      const root = document!.documentElement!;
      root.setAttribute("zenmode-rebase", "");
      measureAndSetCSSVariables();
      requestAnimationFrame(() => {
        requestAnimationFrame(() => {
          root.removeAttribute("zenmode-rebase");
        });
      });
    });
    // Toolbox and statusbar retract by their own size now (translate
    // -100%/100%) and need no observation. The sidebar boxes remain
    // measured: watch them so their first real layout after boot (and
    // splitter drags) true the variables up — the CSS defaults are a few
    // px off the real sizes, visible as a small hop on early toggles.
    for (
      const id of [
        "panel-sidebar-select-box",
        "panel-sidebar-box",
      ]
    ) {
      const el = document!.getElementById(id);
      if (el) observer.observe(el);
    }
    onCleanup(() => observer.disconnect());
  }

  // Disable zen mode when entering toolbar customization
  const customizeObserver = new MutationObserver(() => {
    if (document!.documentElement!.hasAttribute("customizing")) {
      setZenModeEnabled(false);
    }
  });
  customizeObserver.observe(document!.documentElement!, {
    attributes: true,
    attributeFilter: ["customizing"],
  });
  onCleanup(() => customizeObserver.disconnect());
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
        checked={zenModeEnabled() || undefined}
        onCommand={() => setZenModeEnabled((prev) => !prev)}
        accesskey="Z"
      />

      <Show when={zenModeEnabled()}>
        <style>{zenModeStyle}</style>
      </Show>
    </>
  );
}
