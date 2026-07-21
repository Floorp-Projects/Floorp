/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { noraComponent, NoraComponentBase } from "#features-chrome/utils/base";
import styles from "./styles.css?inline";

/**
 * Hover-to-reload: rest the pointer on a tab for a beat and a small
 * reload glyph fades in at its right edge; clicking it reloads that tab
 * without selecting anything.
 *
 * The delay is a JS timer stamping [floorp-show-refresh] on the hovered
 * element rather than a CSS transition-delay: pointer-events cannot be
 * delayed in CSS, so a pure-CSS version would have an invisible but
 * clickable button at the tab's right edge during the delay window,
 * turning stray clicks into surprise reloads.
 */

const HOVER_DELAY_MS = 700;
const STAMP = "floorp-show-refresh";
/** Off by default — hover-reload is a taste thing, not for everyone. */
const ENABLED_PREF = "floorp.tabs.hoverReload.enabled";
/** Root attribute the CSS keys off; kept in sync with the pref. */
const ROOT_ATTR = "floorp-hover-reload";
const HOVER_SELECTOR = "#tabbrowser-tabs .tabbrowser-tab:not([pinned])";

const RELOAD_ICON = "chrome://global/skin/icons/reload.svg";

type TabLike = XULElement;

type TabBrowserLike = {
  tabs: TabLike[];
  tabContainer: XULElement;
  reloadTab?: (tab: TabLike) => void;
};

const getGB = (): TabBrowserLike | null =>
  (globalThis as unknown as { gBrowser?: TabBrowserLike }).gBrowser ?? null;

@noraComponent(import.meta.hot)
export default class TabRefresh extends NoraComponentBase {
  init(): void {
    const doc = document as
      | (Document & { createXULElement: (tag: string) => XULElement })
      | undefined;
    const gb = getGB();
    if (!doc || !gb) {
      console.error("[tab-refresh] no document/gBrowser at init");
      return;
    }

    const style = doc.createElement("style");
    style.textContent = styles;
    doc.head?.appendChild(style);

    // ---- glyph injection: a reload glyph in every unpinned tab ----
    const inject = (tab: TabLike) => {
      try {
        const content = tab.querySelector(".tab-content");
        if (!content || content.querySelector(".floorp-tab-refresh")) return;
        const btn = doc.createXULElement("image");
        btn.classList.add("floorp-tab-refresh");
        btn.setAttribute("src", RELOAD_ICON);
        btn.setAttribute("tooltiptext", "Reload tab");
        // Tabs select on mousedown — swallow it so a reload click never
        // switches tabs.
        btn.addEventListener("mousedown", (e: Event) => {
          e.stopPropagation();
          e.preventDefault();
        });
        btn.addEventListener("click", (e: Event) => {
          e.stopPropagation();
          e.preventDefault();
          try {
            getGB()?.reloadTab?.(tab);
          } catch (err) {
            console.error("[tab-refresh] reload failed:", err);
          }
        });
        content.appendChild(btn);
      } catch (e) {
        console.error("[tab-refresh] inject failed:", e);
      }
    };

    for (const tab of gb.tabs) inject(tab);
    const onTabOpen = (event: Event) => inject(event.target as TabLike);
    gb.tabContainer.addEventListener("TabOpen", onTabOpen);

    // Pref gate: glyphs stay injected (cheap, invisible) so the option
    // can flip live without a restart; the root attribute is what the
    // CSS shows them under, and the hover timer is also gated.
    const enabled = () => Services.prefs.getBoolPref(ENABLED_PREF, false);
    const syncRootAttr = () => {
      const root = doc.documentElement;
      if (!root) return;
      if (enabled()) root.setAttribute(ROOT_ATTR, "true");
      else root.removeAttribute(ROOT_ATTR);
    };
    syncRootAttr();
    const prefObserver = () => syncRootAttr();
    Services.prefs.addObserver(ENABLED_PREF, prefObserver);

    // ---- shared hover timer ----
    let hovered: Element | null = null;
    let timer: ReturnType<typeof setTimeout> | null = null;

    const clearHover = () => {
      if (timer !== null) {
        clearTimeout(timer);
        timer = null;
      }
      if (hovered) {
        hovered.removeAttribute(STAMP);
        hovered = null;
      }
    };

    const onMouseOver = (event: Event) => {
      if (!enabled()) return;
      const target = event.target as Element | null;
      const el = target?.closest?.(HOVER_SELECTOR) ?? null;
      // Moving between children of the same element: keep the timer.
      if (el === hovered) return;
      clearHover();
      if (el) {
        hovered = el;
        timer = setTimeout(() => {
          timer = null;
          if (hovered === el && el.isConnected) {
            el.setAttribute(STAMP, "true");
          }
        }, HOVER_DELAY_MS);
      }
    };

    document?.addEventListener("mouseover", onMouseOver);
    // A drag or tab switch mid-hover should retract the glyph.
    document?.addEventListener("dragstart", clearHover, true);
  }
}
