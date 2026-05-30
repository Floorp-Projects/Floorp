/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { getGBrowser } from "../data/types.js";

/**
 * Verbose split-view visibility diagnostics (session restore / tab switch).
 * Filter console by: nora@split-view:diag
 *
 * Deep layout / compositor hints: in Browser Console run
 *   __floorpSplitViewDeepDiag = true
 * then reproduce; each diag pass also logs bounding rects and every <browser>.
 */
const diag = console.createInstance({
  prefix: "nora@split-view:diag",
});

function emitDiag(message: string): void {
  diag.debug(message);
  try {
    Services.console.logStringMessage(`nora@split-view:diag ${message}`);
  } catch {
    // ignore in environments without Services.console
  }
}

function safeComputedStyle(
  el: Element | null | undefined,
): CSSStyleDeclaration | null {
  if (!el) {
    return null;
  }
  try {
    return getComputedStyle(el);
  } catch {
    return null;
  }
}

function cssValue(style: CSSStyleDeclaration | null, prop: string): string {
  if (!style) {
    return "n/a";
  }
  const v = style.getPropertyValue(prop).trim();
  return v || "n/a";
}

function deepDiagEnabled(): boolean {
  return Boolean(
    (globalThis as { __floorpSplitViewDeepDiag?: boolean })
      .__floorpSplitViewDeepDiag,
  );
}

/**
 * Extra signals when CSS computed style looks fine but a pane still "looks gray":
 * zero-size rects, multiple browsers, filter/content-visibility, remote type.
 */
export function logTabpanelsSplitDeepInvestigation(phase: string): void {
  const gb = getGBrowser();
  const tp = document?.getElementById("tabbrowser-tabpanels");
  if (!tp) {
    emitDiag(`[deep ${phase}] no #tabbrowser-tabpanels`);
    return;
  }
  const sel = gb?.selectedTab as { linkedPanel?: string } | undefined;
  let panelsCsv = "—";
  try {
    panelsCsv = gb?.tabpanels?.splitViewPanels?.join(",") ?? "—";
  } catch {
    panelsCsv = "(read splitViewPanels threw)";
  }
  emitDiag(
    `[deep ${phase}] selected.linkedPanel=${sel?.linkedPanel ?? "null"} ` +
      `splitViewPanels=[${panelsCsv}]`,
  );

  for (const child of tp.children) {
    if (!child.classList.contains("split-view-panel")) {
      continue;
    }
    const el = child as HTMLElement;
    const r = el.getBoundingClientRect();
    const bc = child.querySelector(".browserContainer");
    let bcSnippet = "no .browserContainer";
    if (bc) {
      const bcEl = bc as HTMLElement;
      const br = bcEl.getBoundingClientRect();
      const cs = safeComputedStyle(bc);
      bcSnippet =
        `browserContainer ${Math.round(br.width)}x${Math.round(br.height)} ` +
        `disp=${cssValue(cs, "display")} op=${cssValue(cs, "opacity")} vis=${cssValue(cs, "visibility")} ` +
        `mozSub=${cssValue(cs, "-moz-subtree-hidden-only-visually")}`;
    }
    const browsers = child.querySelectorAll("browser");
    const bits: string[] = [];
    const browserElements = Array.from(
      browsers as NodeListOf<Element>,
    ) as Element[];
    browserElements.forEach((b, i) => {
      const cs = safeComputedStyle(b);
      const br = (b as HTMLElement).getBoundingClientRect();
      const remote = (b as unknown as XULElement).getAttribute?.("remote") ?? "";
      bits.push(
        `#${i} ${Math.round(br.width)}x${Math.round(br.height)} ` +
          `op=${cssValue(cs, "opacity")} fil=${cssValue(cs, "filter")} cv=${cssValue(cs, "content-visibility")} ` +
          `pe=${cssValue(cs, "pointer-events")} remote=${remote}`,
      );
    });
    emitDiag(
      `[deep ${phase}] panel=${child.id} rect=${Math.round(r.width)}x${Math.round(r.height)} ` +
        `| ${bcSnippet} | browser count=${browsers.length} ${bits.join(" || ")}`,
    );
  }
}

export function logTabpanelsSplitDiagnostics(phase: string): void {
  const tp = document?.getElementById("tabbrowser-tabpanels");
  if (!tp) {
    emitDiag(`[${phase}] no #tabbrowser-tabpanels`);
    return;
  }
  const tpStyle = safeComputedStyle(tp);
  emitDiag(
    `[${phase}] tabpanels attrs: splitview=${tp.hasAttribute("splitview")} ` +
      `data-floorp-split=${tp.getAttribute("data-floorp-split") ?? "null"} ` +
      `split-view-layout=${tp.getAttribute("split-view-layout") ?? "null"} ` +
      `display=${cssValue(tpStyle, "display")}`,
  );

  for (const child of tp.children) {
    if (!child.classList.contains("split-view-panel")) {
      continue;
    }
    const st = safeComputedStyle(child);
    const browser = child.querySelector("browser");
    let browserSnippet = "no <browser>";
    if (browser) {
      const bs = safeComputedStyle(browser);
      browserSnippet = `<browser> opacity=${cssValue(bs, "opacity")} vis=${cssValue(bs, "visibility")} mozSub=${cssValue(bs, "-moz-subtree-hidden-only-visually")}`;
    }
    emitDiag(
      `[${phase}] panel id=${child.id} column=${child.getAttribute("column") ?? "—"} ` +
        `floorpActive=${child.hasAttribute("data-floorp-active-pane")} ` +
        `classes=[${[...child.classList].filter((c) => !!c && (c.includes("split") || c === "deck-selected")).join(", ") || "—"}] ` +
        `paneActive=${child.classList.contains("split-view-panel-active")} ` +
        `deck-selected=${child.classList.contains("deck-selected")} ` +
        `mozSub=${cssValue(st, "-moz-subtree-hidden-only-visually")} ` +
        `vis=${cssValue(st, "visibility")} op=${cssValue(st, "opacity")} | ${browserSnippet}`,
    );
  }

  // Tab strip diagnostics for Lepton collision cases.
  const tabsToolbar = document?.getElementById("TabsToolbar");
  const arrowscrollbox = document?.getElementById("tabbrowser-arrowscrollbox");
  const pinnedContainer = document?.getElementById("pinned-tabs-container");
  const tabbrowserTabs = document?.getElementById("tabbrowser-tabs");

  const splitGroupTabs: HTMLElement[] = document
    ? Array.from(
        document.querySelectorAll<HTMLElement>(
          "#tabbrowser-tabs .tabbrowser-tab[floorpSplitViewGroupId], #tabbrowser-tabs .tabbrowser-tab[floorpsplitviewgroupid]",
        ),
      )
    : [];
  const splitMarkerTabs: HTMLElement[] = document
    ? Array.from(
        document.querySelectorAll<HTMLElement>(
          "#tabbrowser-tabs .tabbrowser-tab[data-floorp-split-tab]",
        ),
      )
    : [];
  const allTabs: HTMLElement[] = document
    ? Array.from(
        document.querySelectorAll<HTMLElement>(
          "#tabbrowser-tabs .tabbrowser-tab",
        ),
      )
    : [];

  let splitviewPropTabs = 0;
  for (const tab of allTabs) {
    const maybeSplit = (tab as unknown as { splitview?: unknown }).splitview;
    if (maybeSplit) {
      splitviewPropTabs++;
    }
  }

  const selectedTab = getGBrowser()?.selectedTab as
    | { linkedPanel?: string; label?: string; splitview?: unknown }
    | undefined;

  emitDiag(
    `[${phase}] strip attrs: toolbar.multibar=${tabsToolbar?.getAttribute("multibar") ?? "null"} ` +
      `toolbar.splitview-multibar=${tabsToolbar?.getAttribute("splitview-multibar") ?? "null"} ` +
      `tabpanels.splitview=${tp.getAttribute("splitview") ?? "null"} ` +
      `tabpanels.data-floorp-split=${tp.getAttribute("data-floorp-split") ?? "null"} ` +
      `groupTabs=${splitGroupTabs.length} markerTabs=${splitMarkerTabs.length} splitviewPropTabs=${splitviewPropTabs} ` +
      `selected.linkedPanel=${selectedTab?.linkedPanel ?? "null"} selected.hasSplit=${!!selectedTab?.splitview}`,
  );

  if (arrowscrollbox) {
    const s = safeComputedStyle(arrowscrollbox);
    emitDiag(
      `[${phase}] arrowscrollbox: overflowing=${arrowscrollbox.getAttribute("overflowing") ?? "null"} ` +
        `maxHeight=${cssValue(s, "max-height")} height=${cssValue(s, "height")} display=${cssValue(s, "display")} vis=${cssValue(s, "visibility")}`,
    );
  }
  if (pinnedContainer) {
    const s = safeComputedStyle(pinnedContainer);
    emitDiag(
      `[${phase}] pinned-tabs-container: overflowing=${pinnedContainer.getAttribute("overflowing") ?? "null"} ` +
        `maxHeight=${cssValue(s, "max-height")} height=${cssValue(s, "height")} display=${cssValue(s, "display")} vis=${cssValue(s, "visibility")}`,
    );
  }
  if (tabbrowserTabs) {
    const s = safeComputedStyle(tabbrowserTabs);
    emitDiag(
      `[${phase}] tabbrowser-tabs: orient=${tabbrowserTabs.getAttribute("orient") ?? "null"} ` +
        `overflow=${tabbrowserTabs.getAttribute("overflow") ?? "null"} ` +
        `maxHeight=${cssValue(s, "max-height")} height=${cssValue(s, "height")}`,
    );
  }

  // Per-tab strip style snapshots (focus on split tabs, incl. non-selected).
  const splitTabsDetailed: HTMLElement[] = allTabs.filter(
    (tab: HTMLElement) => {
      const hasGroupAttr =
        tab.hasAttribute("floorpSplitViewGroupId") ||
        tab.hasAttribute("floorpsplitviewgroupid");
      const hasMarkerAttr = tab.hasAttribute("data-floorp-split-tab");
      const hasSplitProp = Boolean(
        (tab as unknown as { splitview?: unknown }).splitview,
      );
      return hasGroupAttr || hasMarkerAttr || hasSplitProp;
    },
  );

  for (const [idx, tabEl] of splitTabsDetailed.entries()) {
    if (idx >= 6) {
      break;
    }

    const tabStyle = safeComputedStyle(tabEl);
    const stack = tabEl.querySelector(".tab-stack") as HTMLElement | null;
    const content = tabEl.querySelector(".tab-content") as HTMLElement | null;
    const stackStyle = safeComputedStyle(stack);
    const contentStyle = safeComputedStyle(content);

    emitDiag(
      `[${phase}] splitTab#${idx} id=${tabEl.id || "(no-id)"} ` +
        `label=${tabEl.getAttribute("label") ?? ""} ` +
        `selected=${tabEl.hasAttribute("selected")} ` +
        `visuallyselected=${tabEl.hasAttribute("visuallyselected")} ` +
        `multiselected=${tabEl.hasAttribute("multiselected")} ` +
        `pinned=${tabEl.hasAttribute("pinned")} ` +
        `group=${tabEl.getAttribute("floorpSplitViewGroupId") ?? tabEl.getAttribute("floorpsplitviewgroupid") ?? "null"} ` +
        `marker=${tabEl.getAttribute("data-floorp-split-tab") ?? "null"} ` +
        `tab[maxH=${cssValue(tabStyle, "max-height")} h=${cssValue(tabStyle, "height")}] ` +
        `stack[maxH=${cssValue(stackStyle, "max-height")} h=${cssValue(stackStyle, "height")}] ` +
        `content[maxH=${cssValue(contentStyle, "max-height")} h=${cssValue(contentStyle, "height")}]`,
    );
  }

  if (deepDiagEnabled()) {
    logTabpanelsSplitDeepInvestigation(phase);
  }
}
