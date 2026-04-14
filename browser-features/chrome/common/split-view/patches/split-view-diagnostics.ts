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
    diag.debug(`[deep ${phase}] no #tabbrowser-tabpanels`);
    return;
  }
  const sel = gb?.selectedTab as { linkedPanel?: string } | undefined;
  let panelsCsv = "—";
  try {
    panelsCsv = gb?.tabpanels?.splitViewPanels?.join(",") ?? "—";
  } catch {
    panelsCsv = "(read splitViewPanels threw)";
  }
  diag.debug(
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
      const cs = getComputedStyle(bc);
      if (cs) {
        bcSnippet =
          `browserContainer ${Math.round(br.width)}x${Math.round(br.height)} ` +
          `disp=${cs.display} op=${cs.opacity} vis=${cs.visibility} ` +
          `mozSub=${cs.getPropertyValue("-moz-subtree-hidden-only-visually")}`;
      }
    }
    const browsers = child.querySelectorAll("browser");
    const bits: string[] = [];
    browsers.forEach((b, i) => {
      const cs = getComputedStyle(b);
      if (!cs) return;
      const br = (b as HTMLElement).getBoundingClientRect();
      const remote = (b as XULElement).getAttribute?.("remote") ?? "";
      bits.push(
        `#${i} ${Math.round(br.width)}x${Math.round(br.height)} ` +
          `op=${cs.opacity} fil=${cs.filter} cv=${cs.contentVisibility} ` +
          `pe=${cs.pointerEvents} remote=${remote}`,
      );
    });
    diag.debug(
      `[deep ${phase}] panel=${child.id} rect=${Math.round(r.width)}x${Math.round(r.height)} ` +
        `| ${bcSnippet} | browser count=${browsers.length} ${bits.join(" || ")}`,
    );
  }
}

export function logTabpanelsSplitDiagnostics(phase: string): void {
  const tp = document?.getElementById("tabbrowser-tabpanels");
  if (!tp) {
    diag.debug(`[${phase}] no #tabbrowser-tabpanels`);
    return;
  }
  const tpStyle = getComputedStyle(tp);
  diag.debug(
    `[${phase}] tabpanels attrs: splitview=${tp.hasAttribute("splitview")} ` +
      `data-floorp-split=${tp.getAttribute("data-floorp-split") ?? "null"} ` +
      `split-view-layout=${tp.getAttribute("split-view-layout") ?? "null"} ` +
      `display=${tpStyle?.display ?? "unknown"}`,
  );

  for (const child of tp.children) {
    if (!child.classList.contains("split-view-panel")) {
      continue;
    }
    const st = getComputedStyle(child);
    const browser = child.querySelector("browser");
    let browserSnippet = "no <browser>";
    if (browser) {
      const bs = getComputedStyle(browser);
      if (bs) {
        browserSnippet = `<browser> opacity=${bs.opacity} vis=${bs.visibility} mozSub=${bs.getPropertyValue("-moz-subtree-hidden-only-visually")}`;
      }
    }
    diag.debug(
      `[${phase}] panel id=${child.id} column=${child.getAttribute("column") ?? "—"} ` +
        `floorpActive=${child.hasAttribute("data-floorp-active-pane")} ` +
        `classes=[${[...child.classList].filter((c) => c.includes("split") || c === "deck-selected").join(", ") || "—"}] ` +
        `paneActive=${child.classList.contains("split-view-panel-active")} ` +
        `deck-selected=${child.classList.contains("deck-selected")} ` +
        `mozSub=${st?.getPropertyValue("-moz-subtree-hidden-only-visually") ?? "unknown"} ` +
        `vis=${st?.visibility ?? "unknown"} op=${st?.opacity ?? "unknown"} | ${browserSnippet}`,
    );
  }

  if (deepDiagEnabled()) {
    logTabpanelsSplitDeepInvestigation(phase);
  }
}
