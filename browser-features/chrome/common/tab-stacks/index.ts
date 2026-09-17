/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { onCleanup } from "solid-js";
import { render } from "@nora/solid-xul";
import {
  noraComponent,
  NoraComponentBase,
} from "#features-chrome/utils/base.ts";
import {
  activateGroup,
  bumpStacksVersion,
  findTabByDragId,
  getGBrowser,
  getGroupDisplayTitle,
  PROXY_DRAG_TYPE,
  rememberSelection,
  StackBar,
  STACK_ATTR,
  type StackGroup,
  type StackTab,
  StackStyleElement,
  syncActiveGroup,
  TAB_DRAG_ID_ATTR,
  type TabBrowser,
} from "./stack-bar.tsx";

export const ENABLED_PREF = "floorp.tabstacks.enabled";
export const GROUP_KINDS_PREF = "floorp.tabstacks.groupKinds";

/** Stacked-layers glyph: a chip's "favicon" at rest, so chips swap
 * icon -> X on hover exactly like tabs do (they have no favicon of their
 * own — the stack is many tabs, not one). */
const STACK_ICON = `data:image/svg+xml;utf8,${
  encodeURIComponent(
    '<svg xmlns="http://www.w3.org/2000/svg" width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="context-stroke" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M12.83 2.18a2 2 0 0 0-1.66 0L2.6 6.08a1 1 0 0 0 0 1.83l8.58 3.91a2 2 0 0 0 1.66 0l8.58-3.9a1 1 0 0 0 0-1.83Z"/><path d="m22 17.65-9.17 4.16a2 2 0 0 1-1.66 0L2 17.65"/><path d="m22 12.65-9.17 4.16a2 2 0 0 1-1.66 0L2 12.65"/></svg>',
  )
}`;

type PrefsLike = {
  getStringPref(name: string, fallback?: string): string;
  setStringPref(name: string, value: string): void;
};

/**
 * Every native tab group is presented as one of two kinds:
 * - "stack": Vivaldi-style two-level presentation (chip + stack bar) — the
 *   default for every group the user creates.
 * - "group": untouched native Firefox presentation (inline tabs, native
 *   label and collapse behavior).
 * Only explicit "group" choices are stored, keyed by the native group id
 * (session restore preserves those ids, so the choice survives restarts).
 */
export type GroupKind = "stack" | "group";

export function readGroupKinds(prefs?: PrefsLike): Record<string, GroupKind> {
  try {
    const raw = (prefs ?? Services.prefs).getStringPref(GROUP_KINDS_PREF, "{}");
    const parsed = JSON.parse(raw) as Record<string, unknown>;
    const kinds: Record<string, GroupKind> = {};
    for (const [id, kind] of Object.entries(parsed)) {
      if (kind === "group") kinds[id] = "group";
    }
    return kinds;
  } catch (e) {
    console.error("[tab-stacks] Failed to read group kinds pref:", e);
    return {};
  }
}

export function getGroupKind(groupId: string, prefs?: PrefsLike): GroupKind {
  return readGroupKinds(prefs)[groupId] ?? "stack";
}

export function setGroupKind(
  groupId: string,
  kind: GroupKind,
  prefs?: PrefsLike,
): void {
  try {
    const target = prefs ?? Services.prefs;
    const kinds = readGroupKinds(prefs);
    if (kind === "group") {
      kinds[groupId] = "group";
    } else {
      delete kinds[groupId];
    }
    target.setStringPref(GROUP_KINDS_PREF, JSON.stringify(kinds));
  } catch (e) {
    console.error("[tab-stacks] Failed to save group kind:", e);
  }
}

/** Smallest unused auto-name: New Stack, New Stack 1, New Stack 2, … */
function nextAutoStackName(gb: { tabGroups: StackGroup[] }): string {
  const used = new Set<number>();
  const re = /^New Stack(?: (\d+))?$/;
  for (const g of gb.tabGroups) {
    const m = re.exec(g.label ?? "");
    if (m) used.add(m[1] ? Number(m[1]) : 0);
  }
  let n = 0;
  while (used.has(n)) n++;
  return n === 0 ? "New Stack" : `New Stack ${n}`;
}

/** Every mutation that can change stack contents or chip labels. */
export const TAB_EVENTS = [
  "TabSelect",
  "TabClose",
  "TabMove",
  "TabAttrModified",
  "TabGrouped",
  "TabUngrouped",
  "TabGroupCreate",
  "TabGroupRemoved",
  "TabGroupCollapse",
  "TabGroupExpand",
] as const;

/**
 * Split-view binds its panes with tab groups too — those must be left
 * completely alone or the two features fight.
 */
export function isSplitViewGroup(group: StackGroup): boolean {
  try {
    return group.tabs.some(
      (t) =>
        t.hasAttribute("floorpSplitViewGroupId") ||
        t.hasAttribute("split-view-layout"),
    );
  } catch {
    // When unsure, hands off.
    return true;
  }
}

/**
 * Decorate one group for its current kind. "stack" hides the native inline
 * presentation behind a chip (all CSS behavior is scoped to STACK_ATTR);
 * "group" restores native presentation end to end.
 */
/** Gecko's Document exposes createXULElement, but its ambient return type is
 * too narrow for our casts; route through a helper instead. */
const createXULElement = (tag: string): XULElement =>
  (document as unknown as {
    createXULElement: (tag: string) => XULElement;
  }).createXULElement(tag);

export function decorateGroup(
  group: StackGroup,
  kind: GroupKind,
  gb: { tabGroups: StackGroup[] },
): void {
  const centerLabels = (): void => {
    for (const sel of [
      ".tab-group-label-container",
      ".tab-group-label-hover-highlight",
    ]) {
      group.querySelector(sel)?.setAttribute("pack", "center");
    }
  };
  const removeStackChrome = (): void => {
    group.removeAttribute(STACK_ATTR);
    group.querySelector(".floorp-stack-close")?.remove();
    group.querySelector(".floorp-stack-icon")?.remove();
  };

  if (kind === "group") {
    removeStackChrome();
    // An unnamed group's label holds a zero-width space rather than
    // nothing at all, so :empty never matches it and the flex gap still
    // reserves room for that invisible character — which is exactly what
    // made the icon chip wider than it is tall. Mark it so the stylesheet
    // can square it off.
    const plainLabel = group.querySelector(".tab-group-label");
    const visibleName = plainLabel?.textContent
      ?.replace(/[\s\u200B-\u200D\uFEFF]/g, "") ?? "";
    group.toggleAttribute("data-floorp-unnamed", !visibleName);

    // Stock's "+N" badge counts every tab in the group, which assumes they
    // are all on the strip. Workspaces breaks that assumption: count only
    // what this workspace can actually reach. Show how many tabs are in
    // there, and drop the plus that promised more on top.
    const reachable = Array.from(group.tabs ?? []).filter((t) => !t.hidden)
      .length;
    group.toggleAttribute("hasmultipletabs", reachable > 1);
    const countLabel = group.querySelector(".tab-group-overflow-count");
    if (countLabel && countLabel.textContent !== String(reachable)) {
      countLabel.textContent = String(reachable);
    }
    // Long typed names: mark only when the text actually overflows.
    const lp = plainLabel as
      | (Element & { scrollWidth?: number; clientWidth?: number })
      | null;
    if (lp && typeof lp.scrollWidth === "number") {
      lp.toggleAttribute(
        "data-floorp-cropped",
        lp.scrollWidth > (lp.clientWidth ?? 0) + 1,
      );
    }
    centerLabels();
    return;
  }

  group.setAttribute(STACK_ATTR, "true");
  // Every stack carries a name. Unnamed stacks used to derive the chip
  // title from a member tab — but real pages retitle themselves on focus
  // (SPAs, notification counters), so the chip appeared to rename on every
  // click inside the stack. A stable auto-name ends that; the native label
  // persists via session restore and prefills the rename editor.
  if (!group.label) {
    group.label = nextAutoStackName(gb);
  }
  // pack="center" is a XUL layout attribute mapped to justify-content as a
  // presentation hint — author CSS cannot beat it, not even inline
  // !important. pack="start" vertically seats the chip like a tab.
  for (const sel of [
    ".tab-group-label-container",
    ".tab-group-label-hover-highlight",
  ]) {
    group.querySelector(sel)?.setAttribute("pack", "start");
  }
  const labelEl = group.querySelector(".tab-group-label");
  if (labelEl && !labelEl.querySelector(".floorp-stack-icon")) {
    const icon = createXULElement("image");
    icon.classList.add("floorp-stack-icon");
    icon.setAttribute("src", STACK_ICON);
    labelEl.appendChild(icon);
  }
  if (labelEl && !labelEl.querySelector(".floorp-stack-close")) {
    const close = createXULElement("image");
    close.classList.add("floorp-stack-close");
    close.setAttribute("src", "chrome://global/skin/icons/close.svg");
    close.setAttribute("tooltiptext", "Close stack");
    labelEl.appendChild(close);
  }
  if (group.collapsed) {
    group.collapsed = false;
  }
  const label = group.querySelector(".tab-group-label");
  label?.setAttribute("data-floorp-count", String(group.tabs.length));
  const chipTitle = group.label || getGroupDisplayTitle(group);
  label?.setAttribute(
    "data-floorp-title",
    chipTitle.length > 60 ? `${chipTitle.slice(0, 60)}…` : chipTitle,
  );
}

/** Reclassify every group and refresh its chip. Cheap; called on demand. */
export function updateGroupChips(gb: { tabGroups: StackGroup[] }): void {
  for (const group of gb.tabGroups) {
    try {
      if (isSplitViewGroup(group)) {
        group.removeAttribute(STACK_ATTR);
        group.querySelector(".floorp-stack-close")?.remove();
        group.querySelector(".floorp-stack-icon")?.remove();
        for (const sel of [
          ".tab-group-label-container",
          ".tab-group-label-hover-highlight",
        ]) {
          group.querySelector(sel)?.setAttribute("pack", "center");
        }
        continue;
      }
      decorateGroup(group, getGroupKind(group.id), gb);
    } catch {
      // Group may be mid-removal; the next event refreshes it.
    }
  }
}

/**
 * Vivaldi-style two-level tab stacks over Firefox's native tab groups.
 * The native group stays the data model (creation, persistence, session
 * restore); this feature only changes presentation: member tabs are hidden
 * from row 1 (the chip represents the stack) and the active group's tabs
 * render as proxies in a second bar under the tab strip.
 */
@noraComponent(import.meta.hot)
export default class TabStacks extends NoraComponentBase {
  init(): void {
    if (typeof document === "undefined" || typeof window === "undefined") {
      return;
    }
    if (!Services.prefs.getBoolPref(ENABLED_PREF, false)) {
      this.logger.info(
        "Tab stacks disabled (floorp.tabstacks.enabled=false).",
      );
      return;
    }

    const gb = getGBrowser();
    // Mount inside #navigator-toolbox, directly under the tab strip
    // (Vivaldi row order: tabs → stack bar → address bar).
    const toolbox = document.getElementById("navigator-toolbox");
    const navBar = document.getElementById("nav-bar");
    if (!gb || !toolbox || !navBar) {
      this.logger.warn("Browser chrome not ready; skipping tab stacks.");
      return;
    }

    render(StackStyleElement, document.head, {
      hotCtx: import.meta.hot,
    });
    render(StackBar, toolbox, {
      marker: navBar,
      hotCtx: import.meta.hot,
    });

    const tabsContainer = gb.tabContainer as unknown as Element;

    this.wrapEnsureElementIsVisible(gb, tabsContainer);
    this.wireCloseSuccessor(gb, tabsContainer);

    // ==== shared drag state ====
    let tabDragActive = false;
    let lastDropPoint: { x: number; y: number } | null = null;
    let lastTabDropTime = 0;
    let lastProxyStripScroll = 0;

    // ==== live rebuild triggers ====
    const onTabEvent = (event: Event) => {
      if (event.type === "TabSelect") {
        rememberSelection();
      }
      // Classify/mark groups first so syncActiveGroup sees fresh marks.
      updateGroupChips(gb);
      // While a drag is in flight the bar must not unmount: pressing a
      // top-row tab selects it on mousedown, and if that tore the bar
      // down there would be nothing left to drop into. Reconciled at
      // drag end.
      if (!tabDragActive) syncActiveGroup();
      bumpStacksVersion();
    };
    for (const type of TAB_EVENTS) {
      addEventListener(type, onTabEvent);
    }
    onCleanup(() => {
      for (const type of TAB_EVENTS) {
        removeEventListener(type, onTabEvent);
      }
    });

    // ==== chip interactions ====
    const onChipPress = (event: MouseEvent) => {
      if (event.button !== 0) return;
      const target = event.target as Element | null;
      // Chip close button: closes the whole stack (all member tabs).
      const closeEl = target?.closest?.(".floorp-stack-close");
      if (closeEl) {
        const group = closeEl.closest(
          `tab-group[${STACK_ATTR}]`,
        ) as StackGroup | null;
        event.preventDefault();
        event.stopImmediatePropagation();
        if (event.type !== "click" || !group) return;
        try {
          // Close the stack as a GROUP, not tab-by-tab. removeTabGroup
          // records the whole group with SessionStore, so Ctrl+Shift+T
          // restores the entire stack in one go. Fall back to removeTabs
          // on runtimes without group-close capture.
          const groupBrowser = gb as unknown as {
            removeTabGroup?: (
              g: StackGroup,
              opts?: { isUserTriggered?: boolean },
            ) => Promise<void>;
            removeTabs?: (tabs: StackTab[]) => void;
          };
          if (groupBrowser.removeTabGroup) {
            Promise.resolve(
              groupBrowser.removeTabGroup(group, { isUserTriggered: true }),
            ).catch((e) =>
              console.error("[tab-stacks] Failed to close stack:", e)
            );
          } else if (groupBrowser.removeTabs) {
            groupBrowser.removeTabs([...group.tabs]);
          } else {
            for (const t of [...group.tabs]) {
              gb.removeTab(t, { animate: false });
            }
          }
        } catch (e) {
          console.error("[tab-stacks] Failed to close stack:", e);
        }
        return;
      }
      const labelContainer = target?.closest?.(".tab-group-label-container");
      if (!labelContainer) return;
      // Only groups we own — split-view groups keep native label behavior.
      const group = labelContainer.closest(
        `tab-group[${STACK_ATTR}]`,
      ) as StackGroup | null;
      if (!group) return;
      // mousedown passes through untouched: the native group-label drag
      // (reordering the whole stack within the strip) starts there. Only
      // click is intercepted — that is where the native collapse toggle
      // lives and where our activate/edit behavior goes.
      if (event.type !== "click") return;
      event.preventDefault();
      event.stopImmediatePropagation();

      if (event.detail >= 2) {
        // Second click of a double: the first already activated the
        // stack (harmless), this one opens the editor.
        try {
          gb.tabGroupMenu?.openEditModal(group);
        } catch (e) {
          console.error("[tab-stacks] Failed to open group editor:", e);
        }
        return;
      }

      activateGroup(group);
      updateGroupChips(gb);
      syncActiveGroup();
      bumpStacksVersion();
    };
    addEventListener("mousedown", onChipPress, true);
    addEventListener("click", onChipPress, true);
    onCleanup(() => {
      removeEventListener("mousedown", onChipPress, true);
      removeEventListener("click", onChipPress, true);
    });

    // ==== native tab context menu for row-2 proxies ====
    // Row-2 proxies use the *native* tab context menu rather than a
    // parallel one. Firefox resolves which tab the menu acts on as
    //   triggerNode.tab || triggerNode.closest("tab") || selectedTab
    // — so stamping `.tab` on the trigger node is the supported hook. It
    // must land before TabContextMenu.updateContextMenu reads it, hence
    // the capture-phase listener on document.
    const onTabContextShowing = (event: Event) => {
      const popup = event.target as Element & { triggerNode?: Node | null };
      if (!(popup instanceof Element) || popup.id !== "tabContextMenu") return;
      const trigger = popup.triggerNode as
        | (Element & { tab?: StackTab | null })
        | null;
      const proxy = trigger?.closest?.(".floorp-stack-tab");
      if (!trigger || !proxy) return;
      const tab = findTabByDragId(
        proxy.getAttribute("data-floorp-drag-id") ?? "",
      );
      if (tab) trigger.tab = tab;
    };
    const onTabContextHidden = (event: Event) => {
      const popup = event.target as Element & { triggerNode?: Node | null };
      if (!(popup instanceof Element) || popup.id !== "tabContextMenu") return;
      const trigger = popup.triggerNode as
        | (Element & { tab?: StackTab | null })
        | null;
      // Don't leave a tab reference on the node once the menu is gone.
      if (trigger && trigger.closest?.(".floorp-stack-tab")) trigger.tab = null;
    };
    document.addEventListener("popupshowing", onTabContextShowing, true);
    document.addEventListener("popuphidden", onTabContextHidden, true);
    onCleanup(() => {
      document?.removeEventListener("popupshowing", onTabContextShowing, true);
      document?.removeEventListener("popuphidden", onTabContextHidden, true);
    });

    // From a row-2 proxy the native menu must not offer "Add Tab to Group":
    // "New Group" would mint another stack from inside this one. Bubble
    // phase, so it runs AFTER the popup's own visibility calls.
    const MOVE_TO_GROUP_ID = "context_moveTabToGroup";
    let hidMoveToGroup = false;
    const onTabContextShowingLate = (event: Event) => {
      const popup = event.target as Element & { triggerNode?: Node | null };
      if (!(popup instanceof Element) || popup.id !== "tabContextMenu") return;
      const trigger = popup.triggerNode as Element | null;
      if (!trigger?.closest?.(".floorp-stack-tab")) return;
      const item = document?.getElementById(MOVE_TO_GROUP_ID) as
        | (XULElement & { hidden: boolean })
        | null;
      if (item && !item.hidden) {
        item.hidden = true;
        hidMoveToGroup = true;
      }
    };
    const onTabContextHiddenLate = (event: Event) => {
      const popup = event.target as Element;
      if (!(popup instanceof Element) || popup.id !== "tabContextMenu") return;
      if (!hidMoveToGroup) return;
      const item = document?.getElementById(MOVE_TO_GROUP_ID) as
        | (XULElement & { hidden: boolean })
        | null;
      if (item) item.hidden = false;
      hidMoveToGroup = false;
    };
    document.addEventListener("popupshowing", onTabContextShowingLate);
    document.addEventListener("popuphidden", onTabContextHiddenLate);
    onCleanup(() => {
      document?.removeEventListener("popupshowing", onTabContextShowingLate);
      document?.removeEventListener("popuphidden", onTabContextHiddenLate);
    });

    // ==== stack ⇄ plain-group conversion menu ====
    const KIND_MENU_ID = "floorp-stack-kind-menu";
    let menuGroup: StackGroup | null = null;
    let kindMenu:
      | (XULElement & {
        openPopupAtScreen: (x: number, y: number, isContext: boolean) => void;
      })
      | null = null;
    let kindItem: XULElement | null = null;
    // Every item carries a Stack and a Group wording; the menu serves both
    // kinds.
    const dualLabelItems: Array<
      { item: XULElement; stack: string; group: string }
    > = [];
    const relabelKindMenu = (kind: GroupKind): void => {
      for (const d of dualLabelItems) {
        d.item.setAttribute("label", kind === "stack" ? d.stack : d.group);
      }
    };
    const popupSet = document.getElementById("mainPopupSet");
    if (popupSet) {
      const menu = createXULElement("menupopup") as XULElement & {
        openPopupAtScreen: (x: number, y: number, isContext: boolean) => void;
      };
      menu.id = KIND_MENU_ID;
      kindMenu = menu;
      const makeItem = (
        stackLabel: string,
        groupLabel: string,
        onCommand: () => void,
      ): XULElement => {
        const item = createXULElement("menuitem");
        item.setAttribute("label", stackLabel);
        item.addEventListener("command", onCommand);
        kindMenu?.appendChild(item);
        dualLabelItems.push({ item, stack: stackLabel, group: groupLabel });
        return item;
      };
      makeItem("Reload Stack", "Reload Group", () => {
        const group = menuGroup;
        if (!group) return;
        try {
          for (const t of [...group.tabs]) gb.reloadTab?.(t);
        } catch (e) {
          console.error("[tab-stacks] reload stack failed:", e);
        }
      });
      makeItem("New Tab in Stack", "New Tab in Group", () => {
        const group = menuGroup;
        if (!group) return;
        try {
          const url = (globalThis as unknown as {
            BROWSER_NEW_TAB_URL?: string;
          }).BROWSER_NEW_TAB_URL ?? "about:newtab";
          const tab = gb.addTab(url, {
            triggeringPrincipal: Services.scriptSecurityManager
              .getSystemPrincipal(),
            skipAnimation: true,
          });
          group.addTabs([tab]);
          gb.selectedTab = tab;
        } catch (e) {
          console.error("[tab-stacks] new tab in stack failed:", e);
        }
      });
      makeItem("Manage Stack…", "Manage Group…", () => {
        // Native group editor: rename, colour and group actions.
        if (menuGroup) gb.tabGroupMenu?.openEditModal(menuGroup);
      });
      kindMenu.appendChild(createXULElement("menuseparator"));
      kindItem = createXULElement("menuitem");
      kindItem.addEventListener("command", () => {
        if (!menuGroup) return;
        const next: GroupKind = getGroupKind(menuGroup.id) === "stack"
          ? "group"
          : "stack";
        setGroupKind(menuGroup.id, next);
        updateGroupChips(gb);
        syncActiveGroup();
        bumpStacksVersion();
      });
      kindMenu.appendChild(kindItem);
      makeItem("Ungroup Stack", "Ungroup Tabs", () => {
        // Dissolve the stack, keep every tab in the strip.
        const group = menuGroup;
        if (!group || !gb.ungroupTab) return;
        try {
          for (const t of [...group.tabs]) gb.ungroupTab(t);
        } catch (e) {
          console.error("[tab-stacks] ungroup stack failed:", e);
        }
      });
      kindMenu.appendChild(createXULElement("menuseparator"));
      makeItem("Close Stack", "Close Group", () => {
        const group = menuGroup;
        if (!group) return;
        try {
          if (gb.removeTabs) {
            gb.removeTabs([...group.tabs]);
          } else {
            for (const t of [...group.tabs]) gb.removeTab(t, { animate: false });
          }
        } catch (e) {
          console.error("[tab-stacks] close stack failed:", e);
        }
      });
      popupSet.appendChild(kindMenu);
    }
    const onLabelContextMenu = (event: MouseEvent) => {
      const target = event.target as Element | null;
      const labelContainer = target?.closest?.(".tab-group-label-container");
      const group = (labelContainer?.closest?.("tab-group") ??
        null) as StackGroup | null;
      if (!group || !kindMenu || !kindItem) return;
      if (isSplitViewGroup(group)) return;
      event.preventDefault();
      event.stopPropagation();
      menuGroup = group;
      const kind = getGroupKind(group.id);
      relabelKindMenu(kind);
      kindItem.setAttribute(
        "label",
        kind === "stack" ? "Change to Tab Group" : "Change to Tab Stack",
      );
      kindMenu.openPopupAtScreen(event.screenX, event.screenY, true);
    };
    addEventListener("contextmenu", onLabelContextMenu, true);
    onCleanup(() => {
      removeEventListener("contextmenu", onLabelContextMenu, true);
      kindMenu?.remove();
    });

    // ==== drag & drop ====
    // A tab dragged PAST a stack chip must reorder, not join: the
    // collapsed member tabs cluster at the chip's edge with zero width,
    // so the native drop math counts the whole neighbourhood as "inside
    // the group". Joining a stack deliberately stays possible by
    // dropping ON the chip itself.
    const TAB_DROP_TYPE = "application/x-moz-tabbrowser-tab";
    const onTabDragStart = (event: DragEvent) => {
      const t = event.target as Element | null;
      if (
        t?.closest?.(".tabbrowser-tab") || t?.closest?.(".floorp-stack-tab")
      ) {
        tabDragActive = true;
      }
    };
    const onAnyDrop = (event: DragEvent) => {
      // Record every TAB drop, not only drags that started in this window:
      // a drop from ANOTHER window adopts the tab here and can land it
      // inside a stack's neighbourhood — it must pass the same on-chip
      // validation (dragstart never fired here).
      const isTabDrop = (event.dataTransfer as
        | (DataTransfer & { mozTypesAt?: (i: number) => ArrayLike<string> })
        | null)?.mozTypesAt?.(0)?.[0] === TAB_DROP_TYPE;
      if (tabDragActive || isTabDrop) {
        lastDropPoint = { x: event.clientX, y: event.clientY };
        lastTabDropTime = Date.now();
      }
    };
    const endTabDrag = () => {
      clearDropIndicators();
      clearChipHighlight();
      // TabGrouped from a drop arrives before dragend; clear on a delay,
      // then reconcile the bar with wherever selection actually landed.
      setTimeout(() => {
        tabDragActive = false;
        lastDropPoint = null;
        updateGroupChips(gb);
        syncActiveGroup();
        bumpStacksVersion();
      }, 150);
    };
    const onTabGroupedDuringDrag = (event: Event) => {
      // In-window drags flag themselves at dragstart; cross-window drops
      // only leave the fresh drop record. Anything else (menu commands,
      // programmatic addTabs) must not be second-guessed here.
      if (!tabDragActive && Date.now() - lastTabDropTime > 500) return;
      const tab = event.target as StackTab;
      const group = tab.group;
      if (!group || group.getAttribute?.(STACK_ATTR) !== "true") return;
      if (!lastDropPoint) return;
      const lc = (group as unknown as Element).querySelector(
        ".tab-group-label-container",
      );
      if (!lc) return;
      const r = lc.getBoundingClientRect();
      const p = lastDropPoint;
      const onChip = p.x >= r.x && p.x <= r.x + r.width &&
        p.y >= r.y && p.y <= r.y + r.height;
      if (onChip) return;
      try {
        // Out of the group, landing right after it; left-side drops then
        // hop before it.
        gb.ungroupTab?.(tab);
        if (p.x < r.x) {
          gb.moveTabBefore?.(tab, group as unknown as StackTab);
        }
      } catch (e) {
        console.error("[tab-stacks] drop-outside eject failed:", e);
      }
    };
    // Deliberate join: dropping a row-1 tab ON the chip adds it to the
    // stack. With the member tabs zero-width, the native drop math almost
    // never resolves to "join" by itself, so the chip performs it.
    const onChipDrop = (event: DragEvent) => {
      if (event.dataTransfer?.types?.includes(PROXY_DRAG_TYPE)) return;
      const target = event.target as Element | null;
      const lc = target?.closest?.(".tab-group-label-container");
      const group = (lc?.closest?.(`tab-group[${STACK_ATTR}]`) ??
        null) as StackGroup | null;
      if (!group) return;
      const dt = event.dataTransfer as
        | (DataTransfer & { mozSourceNode?: Node | null })
        | null;
      const src = ((dt?.mozSourceNode as Element | null)?.closest?.(
        ".tabbrowser-tab",
      ) ?? null) as StackTab | null;
      if (!src || src.group === group) return;
      // A multiselected drag carries every selected tab, exactly like the
      // native drop path would — joining only the grabbed one stranded
      // the rest of the selection outside the stack.
      const srcGlobal = (src.ownerDocument as Document & {
        defaultView?: { gBrowser?: { selectedTabs?: StackTab[] } } | null;
      })?.defaultView;
      const joining = (src as unknown as { multiselected?: boolean })
          .multiselected
        ? (srcGlobal?.gBrowser?.selectedTabs ?? [src]).filter(
          (t) => t.group !== group,
        )
        : [src];
      if (!joining.length) return;
      event.preventDefault();
      event.stopImmediatePropagation();
      clearDropIndicators();
      clearChipHighlight();
      // Adopt after the native drag machinery settles.
      setTimeout(() => {
        try {
          group.addTabs(joining);
          updateGroupChips(gb);
          syncActiveGroup();
          bumpStacksVersion();
          scrollProxyIntoView(src);
        } catch (e) {
          console.error("[tab-stacks] chip drop join failed:", e);
        }
      }, 0);
    };

    // ==== drop indicators ====
    const stripDropIndicator = ():
      | (HTMLElement & { hidden: boolean })
      | null =>
      (gb.tabContainer as unknown as Element)?.querySelector(
        ".tab-drop-indicator",
      ) as (HTMLElement & { hidden: boolean }) | null;
    const showStripDropLine = (boundaryX: number | null): void => {
      const ind = stripDropIndicator();
      if (!ind) return;
      if (boundaryX == null) {
        ind.hidden = true;
        ind.style.transform = "";
        return;
      }
      const rect = (gb.tabContainer as unknown as {
        arrowScrollbox?: Element;
      }).arrowScrollbox?.getBoundingClientRect();
      if (!rect) return;
      // Unhide first — a hidden indicator measures clientWidth 0.
      ind.hidden = false;
      const margin = boundaryX - rect.left + ind.clientWidth / 2;
      ind.style.transform = `translateX(${Math.round(margin)}px)`;
    };
    const clearDropIndicators = () => {
      for (const el of document.querySelectorAll("[data-drop-side]")) {
        el.removeAttribute("data-drop-side");
      }
      const ind = stripDropIndicator();
      if (ind && !ind.hidden) {
        ind.hidden = true;
        ind.style.transform = "";
      }
    };
    const clearChipHighlight = () => {
      for (const el of document.querySelectorAll("[data-floorp-drop-into]")) {
        el.removeAttribute("data-floorp-drop-into");
      }
    };

    // Dragging a row-1 tab over a stack chip lights the WHOLE chip cell
    // (not a thin insertion underline): the gesture means "drop to join
    // this stack", which is a cell-level action, not an insertion point.
    const onChipDragOverHighlight = (event: DragEvent) => {
      const target = event.target as Element | null;
      const chip = target?.closest?.(".tab-group-label-container")
        ?.closest?.(`tab-group[${STACK_ATTR}]`);
      clearChipHighlight();
      if (!chip) return;
      const dt = event.dataTransfer as
        | (DataTransfer & { mozSourceNode?: Node | null })
        | null;
      const src = (dt?.mozSourceNode as Element | null)?.closest?.(
        ".tabbrowser-tab",
      ) as StackTab | null;
      if (!src || (src.group as unknown as Element) === chip) return;
      event.preventDefault();
      clearDropIndicators();
      chip.setAttribute("data-floorp-drop-into", "true");
    };

    const scrollProxyIntoView = (tab: StackTab) => {
      // Let the proxy for the new position render first.
      setTimeout(() => {
        try {
          const bar = document?.getElementById("floorp-stack-bar");
          const proxy = bar?.querySelector(
            `.floorp-stack-tab[data-floorp-drag-id="${
              tab.getAttribute(TAB_DRAG_ID_ATTR)
            }"]`,
          );
          proxy?.scrollIntoView({ block: "nearest", inline: "nearest" });
        } catch (e) {
          console.error("[tab-stacks] scroll-into-view failed:", e);
        }
      }, 30);
    };

    // Row-1 insertion targets for proxy drags: ungrouped tabs AND whole
    // groups. A stack chip (or a plain group) is one atomic cell — a proxy
    // drops beside it, never between its members.
    type StripItem = {
      el: Element;
      group: StackGroup | null;
      x: number;
      width: number;
    };
    const collectStripItems = (excludeTab: StackTab | null): StripItem[] => {
      const items: StripItem[] = [];
      for (const t of gb.tabs) {
        if (t.hidden || t.group || t === excludeTab) continue;
        const r = (t as unknown as Element).getBoundingClientRect();
        if (r.width === 0) continue;
        items.push({
          el: t as unknown as Element,
          group: null,
          x: r.x,
          width: r.width,
        });
      }
      for (const g of gb.tabGroups) {
        const ge = g as unknown as Element;
        const lc = ge.querySelector(".tab-group-label-container");
        const lcRect = lc?.getBoundingClientRect();
        if (!lc || !lcRect || lcRect.width === 0) continue;
        let right = lcRect.right;
        if (ge.getAttribute(STACK_ATTR) !== "true") {
          // Plain group: the cell spans the label plus its visible tabs.
          for (const t of g.tabs) {
            if (t.hidden) continue;
            const r = (t as unknown as Element).getBoundingClientRect();
            if (r.width > 0 && r.right > right) right = r.right;
          }
        }
        items.push({
          el: ge,
          group: g,
          x: lcRect.x,
          width: right - lcRect.x,
        });
      }
      items.sort((a, b) => a.x - b.x);
      return items;
    };
    const resolveStripAnchor = (
      items: StripItem[],
      clientX: number,
    ): StripItem | undefined => {
      let anchor: StripItem | undefined;
      for (const it of items) {
        if (clientX >= it.x + it.width / 2) anchor = it;
      }
      return anchor;
    };

    const onProxyDragOver = (event: DragEvent) => {
      if (!event.dataTransfer?.types?.includes(PROXY_DRAG_TYPE)) return;
      const t = event.target as Element | null;
      const inBar = t?.closest?.("#floorp-stack-bar");
      const inStrip = t?.closest?.("#tabbrowser-tabs");
      if (!inBar && !inStrip) {
        clearDropIndicators();
        return;
      }
      event.preventDefault();
      // The strip's native dragover handler treats our custom type as
      // dropEffect "none" and re-hides the drop indicator this handler
      // just positioned. Cut it out entirely.
      event.stopPropagation();
      if (event.dataTransfer) event.dataTransfer.dropEffect = "move";
      // Insertion indicator on the nearest cell edge, like the top row.
      clearDropIndicators();
      if (inBar) {
        const proxies = [...inBar.querySelectorAll(".floorp-stack-tab")];
        for (const proxy of proxies) {
          const r = proxy.getBoundingClientRect();
          if (event.clientX < r.x + r.width / 2) {
            proxy.setAttribute("data-drop-side", "before");
            return;
          }
        }
        proxies[proxies.length - 1]?.setAttribute("data-drop-side", "after");
      } else if (inStrip) {
        // Releasing on the proxy's own chip is a no-op — show no line there.
        const dt = event.dataTransfer as
          | (DataTransfer & { mozSourceNode?: Node | null })
          | null;
        const srcProxy = (dt?.mozSourceNode as Element | null)?.closest?.(
          ".floorp-stack-tab",
        );
        const srcTab = findTabByDragId(
          srcProxy?.getAttribute("data-floorp-drag-id") ?? "",
        );
        const hoverChip = t?.closest?.(".tab-group-label-container")
          ?.closest?.(`tab-group[${STACK_ATTR}]`);
        if (
          hoverChip && hoverChip === (srcTab?.group as unknown as Element)
        ) {
          return;
        }
        // Autoscroll an overflowing strip while the proxy hovers near its
        // ends — native only autoscrolls its own drag types.
        const asbEl = (gb.tabContainer as unknown as {
          arrowScrollbox?: XULElement & {
            scrollByPixels?: (px: number, instant?: boolean) => void;
            scrollIncrement?: number;
          };
        }).arrowScrollbox;
        if (
          asbEl?.hasAttribute?.("overflowing") &&
          Date.now() - lastProxyStripScroll > 80
        ) {
          const sr = asbEl.getBoundingClientRect();
          const edge = 44;
          const inc = Math.min(asbEl.scrollIncrement ?? 96, 160);
          if (event.clientX < sr.left + edge) {
            asbEl.scrollByPixels?.(-inc, true);
            lastProxyStripScroll = Date.now();
          } else if (event.clientX > sr.right - edge) {
            asbEl.scrollByPixels?.(inc, true);
            lastProxyStripScroll = Date.now();
          }
        }
        const items = collectStripItems(null);
        const anchor = resolveStripAnchor(items, event.clientX);
        if (anchor) {
          showStripDropLine(anchor.x + anchor.width);
        } else if (items[0]) {
          showStripDropLine(items[0].x);
        } else {
          showStripDropLine(null);
        }
      }
    };
    const onProxyDrop = (event: DragEvent) => {
      clearDropIndicators();
      clearChipHighlight();
      const dragId = event.dataTransfer?.getData(PROXY_DRAG_TYPE);
      if (!dragId) return;
      const tab = findTabByDragId(dragId);
      if (!tab) return;
      const target = event.target as Element | null;
      event.preventDefault();
      event.stopImmediatePropagation();
      try {
        const inBar = target?.closest?.("#floorp-stack-bar");
        if (inBar) {
          // Reorder within the stack: place before the first proxy whose
          // midpoint lies right of the drop.
          const bar = inBar as Element;
          const proxies = [...bar.querySelectorAll(".floorp-stack-tab")];
          let placed = false;
          for (const proxy of proxies) {
            const r = proxy.getBoundingClientRect();
            if (event.clientX < r.x + r.width / 2) {
              const other = findTabByDragId(
                proxy.getAttribute("data-floorp-drag-id") ?? "",
              );
              if (other && other !== tab) {
                gb.moveTabBefore?.(tab, other);
              }
              placed = true;
              break;
            }
          }
          if (!placed) {
            const last = proxies[proxies.length - 1];
            const other = findTabByDragId(
              last?.getAttribute("data-floorp-drag-id") ?? "",
            );
            if (other && other !== tab) gb.moveTabAfter?.(tab, other);
          }
        } else if (target?.closest?.("#TabsToolbar")) {
          // The WHOLE strip row is a valid eject zone (not just the tab
          // container) so the two dead spots work: the very start and the
          // gap right next to a stack chip at the end. Only releasing ON
          // the tab's own chip label is a no-op.
          const ownLabel = target?.closest?.(".tab-group-label-container");
          const ownChip = ownLabel?.closest?.(`tab-group[${STACK_ATTR}]`);
          if (
            ownChip && ownChip === (tab.group as unknown as Element)
          ) {
            return;
          }
          // Resolve the drop target from CURRENT geometry, before any
          // mutation: ungroupTab() inserts the tab into the strip and
          // shifts every tab right of the group, so measuring afterwards
          // resolved against moved boxes.
          const items = collectStripItems(tab);
          const anchor = resolveStripAnchor(items, event.clientX);
          const first = items[0];
          gb.ungroupTab?.(tab);
          if (anchor) {
            gb.moveTabAfter?.(
              tab,
              (anchor.group ?? anchor.el) as unknown as StackTab,
            );
          } else if (first) {
            gb.moveTabBefore?.(
              tab,
              (first.group ?? first.el) as unknown as StackTab,
            );
          }
        }
        updateGroupChips(gb);
        syncActiveGroup();
        bumpStacksVersion();
        scrollProxyIntoView(tab);
      } catch (e) {
        console.error("[tab-stacks] proxy drop failed:", e);
      }
    };

    addEventListener("dragstart", onTabDragStart, true);
    addEventListener("drop", onAnyDrop, true);
    addEventListener("drop", onChipDrop, true);
    addEventListener("drop", onProxyDrop, true);
    addEventListener("dragover", onProxyDragOver, true);
    addEventListener("dragover", onChipDragOverHighlight, true);
    addEventListener("dragend", endTabDrag, true);
    addEventListener("TabGrouped", onTabGroupedDuringDrag);
    onCleanup(() => {
      removeEventListener("dragstart", onTabDragStart, true);
      removeEventListener("drop", onAnyDrop, true);
      removeEventListener("drop", onChipDrop, true);
      removeEventListener("drop", onProxyDrop, true);
      removeEventListener("dragover", onProxyDragOver, true);
      removeEventListener("dragover", onChipDragOverHighlight, true);
      removeEventListener("dragend", endTabDrag, true);
      removeEventListener("TabGrouped", onTabGroupedDuringDrag);
    });

    // Some group events don't bubble reliably; a debounced observer on the
    // tab strip catches every structural change (group created, tab joins,
    // titles) regardless of event plumbing.
    let refreshTimer: ReturnType<typeof setTimeout> | null = null;
    const scheduleRefresh = () => {
      if (refreshTimer) return;
      refreshTimer = setTimeout(() => {
        refreshTimer = null;
        rememberSelection();
        syncActiveGroup();
        bumpStacksVersion();
        updateGroupChips(gb);
      }, 80);
    };
    const observer = new MutationObserver(scheduleRefresh);
    observer.observe(tabsContainer, {
      childList: true,
      subtree: true,
      attributes: true,
      attributeFilter: ["label", "selected", "image"],
    });
    onCleanup(() => {
      observer.disconnect();
      if (refreshTimer) clearTimeout(refreshTimer);
    });

    rememberSelection();
    updateGroupChips(gb);
    syncActiveGroup();
    bumpStacksVersion();
    this.logger.info("Tab stacks initialized");
  }

  /**
   * THE one wrap on the strip's scroll-into-view. Firefox re-runs
   * _handleTabSelect -> ensureTabIsVisible(selectedTab) on window resize,
   * fullscreen flips, uidensity changes and every overflow start (tabs.js),
   * which yanks a manually-scrolled strip back to the selected tab. Two
   * skips, one wrapper:
   *  - hidden target: the selected tab is a stack member, hidden (zero
   *    width) at the chip's spot. Scrolling "to" it walked the strip back
   *    to the chip on every strip resize. A hidden element cannot be made
   *    visible by scrolling anyway.
   *  - user-positioned: any wheel over the strip since the current tab was
   *    selected means the user chose this scroll position; recentres onto
   *    that same still-selected tab are refused until the selection changes.
   */
  private wrapEnsureElementIsVisible(
    gb: TabBrowser,
    tabsContainer: Element,
  ): void {
    const asb = (gb.tabContainer as unknown as {
      arrowScrollbox?: XULElement & {
        ensureElementIsVisible: (el: Element, instant?: boolean) => void;
        __floorpEnsureWrapped?: boolean;
      };
    }).arrowScrollbox;
    let userPositionedFor: Element | null = null;
    const armRecentreLatch = () => {
      userPositionedFor = (gb.selectedTab as unknown as Element) ?? null;
    };
    // The arming listener is passive capture: it can only observe — it is
    // physically unable to consume or reorder wheel events.
    tabsContainer.addEventListener("wheel", armRecentreLatch, {
      capture: true,
      passive: true,
    });
    onCleanup(() =>
      tabsContainer.removeEventListener("wheel", armRecentreLatch, true)
    );
    if (asb && !asb.__floorpEnsureWrapped) {
      const orig = asb.ensureElementIsVisible.bind(asb);
      asb.ensureElementIsVisible = (el: Element, instant?: boolean) => {
        try {
          if (
            el &&
            ((el as XULElement).hidden ||
              el.getBoundingClientRect?.().width === 0)
          ) {
            return;
          }
          if (userPositionedFor) {
            const selected = (gb.selectedTab as unknown as Element) ?? null;
            if (selected !== userPositionedFor) {
              userPositionedFor = null;
            } else if (el === selected) {
              return;
            }
          }
        } catch {
          // fall through to native
        }
        return orig(el, instant);
      };
      asb.__floorpEnsureWrapped = true;
      onCleanup(() => {
        asb.ensureElementIsVisible = orig;
        asb.__floorpEnsureWrapped = false;
      });
    }
  }

  /**
   * Closing a stacked tab used to yank selection up to row 1 even with
   * members left: stock blur logic picks the nearest VISIBLE tab, and
   * every other member of a collapsed stack is hidden at the chip. A
   * successor set during TabClose is TOO LATE: _blurTab runs inside
   * _beginRemoveTab, ~90 lines BEFORE the TabClose dispatch. The successor
   * must be in place AHEAD of the close instead: whenever selection sits
   * on a stack member, keep its successor pointed at the neighbouring
   * member (right, else left) — then every close path walks along row 2.
   */
  private wireCloseSuccessor(
    gb: TabBrowser,
    tabsContainer: Element,
  ): void {
    const floorpSuccessors = new WeakSet<Element>();
    const refreshCloseSuccessor = () => {
      try {
        const tab = gb.selectedTab as
          | (Element & {
            closing?: boolean;
            group?: (Element & { tabs: Element[] }) | null;
          })
          | null;
        if (!tab || tab.closing) return;
        const setSucc = (gb as unknown as {
          setSuccessor?: (a: Element, b: Element | null) => void;
        }).setSuccessor;
        if (typeof setSucc !== "function") return;
        const boundSetSucc = setSucc.bind(gb);
        const group = tab.group;
        if (!group || group.getAttribute?.(STACK_ATTR) !== "true") {
          // Only unwind pointers this feature set — extensions can manage
          // their own successors and those must survive us.
          if (floorpSuccessors.has(tab)) {
            floorpSuccessors.delete(tab);
            boundSetSucc(tab, null);
          }
          return;
        }
        const members =
          ([...group.tabs] as (Element & { closing?: boolean })[])
            .filter((t) => !t.closing);
        const idx = members.indexOf(tab);
        const pick = members[idx + 1] ?? members[idx - 1] ?? null;
        if (pick && pick !== tab) {
          boundSetSucc(tab, pick);
          floorpSuccessors.add(tab);
        } else if (floorpSuccessors.has(tab)) {
          // Last member left — stock blur (row 1) is the only place to go.
          floorpSuccessors.delete(tab);
          boundSetSucc(tab, null);
        }
      } catch (e) {
        console.error("[tab-stacks] close successor:", e);
      }
    };
    // TabClose re-picks after a closing neighbour was spliced out of the
    // succession line; the rest are selection/membership/order changes.
    const SUCCESSOR_EVENTS = [
      "TabSelect",
      "TabMove",
      "TabGrouped",
      "TabUngrouped",
      "TabClose",
    ] as const;
    for (const ev of SUCCESSOR_EVENTS) {
      tabsContainer.addEventListener(ev, refreshCloseSuccessor);
    }
    refreshCloseSuccessor();
    onCleanup(() => {
      for (const ev of SUCCESSOR_EVENTS) {
        tabsContainer.removeEventListener(ev, refreshCloseSuccessor);
      }
    });
  }
}
