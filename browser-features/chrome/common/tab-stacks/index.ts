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
  rememberSelection,
  StackBar,
  type StackGroup,
  type StackTab,
  StackStyleElement,
  syncActiveGroup,
} from "./stack-bar.tsx";

const ENABLED_PREF = "floorp.tabstacks.enabled";

/** Stacked-layers glyph: a chip's "favicon" at rest, so chips swap
 * icon -> X on hover exactly like tabs do (they have no favicon of their
 * own — the stack is many tabs, not one). */
const STACK_ICON = `data:image/svg+xml;utf8,${
  encodeURIComponent(
    '<svg xmlns="http://www.w3.org/2000/svg" width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="context-stroke" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M12.83 2.18a2 2 0 0 0-1.66 0L2.6 6.08a1 1 0 0 0 0 1.83l8.58 3.91a2 2 0 0 0 1.66 0l8.58-3.9a1 1 0 0 0 0-1.83Z"/><path d="m22 17.65-9.17 4.16a2 2 0 0 1-1.66 0L2 17.65"/><path d="m22 12.65-9.17 4.16a2 2 0 0 1-1.66 0L2 12.65"/></svg>',
  )
}`;
const GROUP_KINDS_PREF = "floorp.tabstacks.groupKinds";

/**
 * Every native tab group is presented as one of two kinds:
 * - "stack": Vivaldi-style two-level presentation (chip + stack bar) — the
 *   default for every group the user creates.
 * - "group": untouched native Firefox presentation (inline tabs, native
 *   label and collapse behavior).
 * Only explicit "group" choices are stored, keyed by the native group id
 * (session restore preserves those ids, so the choice survives restarts).
 * Stale ids of long-gone groups are harmless and tiny; they are dropped
 * whenever their group is toggled back to a stack.
 */
type GroupKind = "stack" | "group";

function readGroupKinds(): Record<string, GroupKind> {
  try {
    const raw = Services.prefs.getStringPref(GROUP_KINDS_PREF, "{}");
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

function getGroupKind(groupId: string): GroupKind {
  return readGroupKinds()[groupId] ?? "stack";
}

function setGroupKind(groupId: string, kind: GroupKind): void {
  try {
    const kinds = readGroupKinds();
    if (kind === "group") {
      kinds[groupId] = "group";
    } else {
      delete kinds[groupId];
    }
    Services.prefs.setStringPref(GROUP_KINDS_PREF, JSON.stringify(kinds));
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
const TAB_EVENTS = [
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
 * Vivaldi-style two-level tab stacks over Firefox's native tab groups.
 * The native group stays the data model (creation, persistence, session
 * restore); this feature only changes presentation: member tabs are hidden
 * from row 1 (the chip represents the stack) and the active group's tabs
 * render as proxies in a second bar under the tab strip.
 *
 * Groups come in two user-selectable kinds ("Change to Tab Group/Stack"
 * in the label context menu): stacks as above, or plain native groups
 * that keep Firefox's inline presentation. Split-view groups are always
 * left native regardless of kind.
 */
@noraComponent(import.meta.hot)
export default class TabStacks extends NoraComponentBase {
  init(): void {
    if (typeof document === "undefined" || typeof window === "undefined") {
      return;
    }
    // Off by default while under review: set floorp.tabstacks.enabled=true
    // to turn the feature on. Groups render natively while disabled.
    if (!Services.prefs.getBoolPref(ENABLED_PREF, false)) {
      this.logger.info("Tab stacks switched off (floorp.tabstacks.enabled=false)");
      return;
    }
    // Built for the horizontal tab strip: the chip lays out as a horizontal
    // tab cell and the second row is a horizontal arrowscrollbox. No-op
    // while vertical tabs are active (startup check only); a vertical
    // presentation is left to a future integration.
    if (Services.prefs.getBoolPref("sidebar.verticalTabs", false)) {
      this.logger.info("Vertical tabs active - tab stacks disabled.");
      return;
    }

    const gb = getGBrowser();
    // Mount inside #navigator-toolbox, directly under the tab strip
    // (Vivaldi row order: tabs → stack bar → address bar). Living inside
    // the toolbox is safe for zen mode: it re-measures the toolbox height
    // right before every retract (zen-mode.tsx tryHideTop), so the bar
    // mounting/unmounting can no longer leave zen with a stale height.
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

    // Stop the strip yanking back to a hidden stacked tab. Firefox re-runs
    // _handleTabSelect -> ensureTabIsVisible(selectedTab) on EVERY tab-strip
    // resize (tabs.js handleResize). When the selected tab is a stack member
    // it is hidden (zero width) at the chip's spot, so this scrolled the row
    // back to the chip during manual scrolling — and each scroll toggles the
    // overflow buttons, resizing the strip, firing it again (a scroll/resize
    // fight). A hidden zero-width element cannot be "made visible" anyway, so
    // skip it; every other tab still scrolls into view normally.
    const asb = (gb.tabContainer as unknown as {
      arrowScrollbox?: XULElement & {
        ensureElementIsVisible: (el: Element, instant?: boolean) => void;
        __floorpEnsureWrapped?: boolean;
      };
    }).arrowScrollbox;
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

    // ===== Native drag corrections around stack chips =====
    // Two fixes, both post-passes over TabDragAndDrop._animateTabMove:
    //
    // (underline) Native paints a "will join this group" cue as a coloured
    // underline on the DRAGGED tab, timed off the dragged tab's box edges —
    // while our chip highlight tracks the cursor. The two disagreed by half
    // a tab width, in opposite directions per drag direction. For stacks the
    // chip highlight is the only signal; the underline is suppressed via a
    // strip attribute (see styles.css). Plain groups keep native behavior.
    //
    // (hold) Native shifts the chip aside ("make room to reorder") once the
    // dragged box overlaps it past moveOverThresholdPercent — but overlap is
    // measured against the NARROWER of the two boxes, so the chip jumped the
    // moment the cursor touched it. While the cursor is over a stack chip
    // the gesture is "drop into": the reorder shift is held (chip stays put,
    // the approach-side gap is kept), and native reordering resumes as soon
    // as the cursor leaves the chip.
    const TAB_DROP_TYPE = "application/x-moz-tabbrowser-tab";
    type DragItem = Element & {
      currentIndex?: number;
      elementIndex?: number;
      pinned?: boolean;
      style?: CSSStyleDeclaration;
    };
    type NativeDragData = {
      fromTabList?: boolean;
      movingTabs?: unknown[];
      movingTabsSet?: Set<unknown>;
      tabWidth?: number;
      animDropElementIndex?: number;
      dropElement?: unknown;
      dropBefore?: boolean;
    };
    const tabsEl = gb.tabContainer as unknown as XULElement & {
      dragAndDropElements?: DragItem[];
      tabDragAndDrop?: {
        _animateTabMove?: (event: DragEvent) => void;
        __floorpAnimateWrapped?: boolean;
      };
    };
    const correctStackDrag = (event: DragEvent) => {
      const dragged = (event.dataTransfer as unknown as {
        mozGetDataAt?: (type: string, index: number) => unknown;
      })?.mozGetDataAt?.(TAB_DROP_TYPE, 0) as
        | (Element & {
          _dragData?: NativeDragData;
          pinned?: boolean;
          elementIndex?: number;
          group?: StackGroup | null;
        })
        | null;
      const dragData = dragged?._dragData;
      if (
        !dragged || !dragData || dragData.fromTabList ||
        !dragged.classList?.contains?.("tabbrowser-tab")
      ) {
        // Group-label and split-view drags keep native behavior wholesale.
        tabsEl.removeAttribute("data-floorp-stack-droptarget");
        return;
      }

      // Suppress the native join-underline while the drop target is a stack.
      const de = dragData.dropElement as Element | null | undefined;
      const deGroup = de?.tagName?.toLowerCase?.() === "tab-group"
        ? de
        : de?.closest?.("tab-group") ?? null;
      tabsEl.toggleAttribute(
        "data-floorp-stack-droptarget",
        deGroup?.getAttribute?.("data-floorp-stack") === "true",
      );

      // Hold the reorder shift while the cursor is over a stack chip.
      // Horizontal LTR strips only — exactly the layout stacks are built
      // for; anything else keeps stock behavior.
      if (
        tabsEl.getAttribute("orient") !== "horizontal" ||
        Services.locale.isAppLocaleRTL || dragged.pinned
      ) {
        return;
      }
      const chipGroup = (event.target as Element | null)
        ?.closest?.(".tab-group-label-container")
        ?.closest?.("tab-group[data-floorp-stack]") as StackGroup | null;
      if (!chipGroup || (dragged.group as unknown) === chipGroup) return;
      const labelEl = ((chipGroup as unknown as { labelElement?: DragItem })
        .labelElement ??
        (chipGroup as unknown as Element).querySelector(
          ".tab-group-label",
        )) as DragItem | null;
      const labelIndex = labelEl?.elementIndex;
      const draggedIndex = dragged.elementIndex;
      if (typeof labelIndex !== "number" || typeof draggedIndex !== "number") {
        return;
      }
      // The no-shift state keeps the gap on the approach side: coming from
      // the left that is the label's own index, from the right one past it
      // (the zero-width members' slots — shifting those moves nothing).
      const clampIndex = draggedIndex < labelIndex
        ? labelIndex
        : labelIndex + 1;
      dragData.animDropElementIndex = clampIndex;
      const firstMember = chipGroup.tabs?.[0];
      if (firstMember) {
        // Releasing here should join the stack even if the native drop
        // handler wins the race (the chip's own drop handler then no-ops).
        dragData.dropElement = firstMember;
        dragData.dropBefore = true;
      }
      const shiftSize = dragData.tabWidth ?? 0;
      const moving = dragData.movingTabsSet ??
        new Set(dragData.movingTabs ?? []);
      for (const item of tabsEl.dragAndDropElements ?? []) {
        if (moving.has(item) || item === dragged || item.pinned) continue;
        const cur = item.currentIndex ?? item.elementIndex;
        if (typeof cur !== "number") continue;
        let shift = 0;
        if (cur < draggedIndex && cur >= clampIndex) shift = shiftSize;
        else if (cur > draggedIndex && cur < clampIndex) shift = -shiftSize;
        const box = item.classList?.contains?.("tab-group-label")
          ? item.closest?.(".tab-group-label-container") as DragItem | null
          : item;
        if (box?.style) {
          box.style.transform = shift ? `translateX(${shift}px)` : "";
        }
      }
    };
    const tdd = tabsEl.tabDragAndDrop;
    if (tdd?._animateTabMove && !tdd.__floorpAnimateWrapped) {
      const origAnimate = tdd._animateTabMove.bind(tdd);
      tdd._animateTabMove = (event: DragEvent) => {
        origAnimate(event);
        try {
          correctStackDrag(event);
        } catch (e) {
          console.error("[tab-stacks] stack drag correction failed:", e);
        }
      };
      tdd.__floorpAnimateWrapped = true;
      onCleanup(() => {
        tdd._animateTabMove = origAnimate;
        tdd.__floorpAnimateWrapped = false;
      });
    }

    // No drag ghosts. Native startTabDrag hands the OS a 160×90 PageThumbs
    // canvas as the drag image for every tab drag — white until the async
    // capture lands, then a live page preview floating beside the tab that
    // is ALREADY moving under the cursor (the "double ghost"; start pages
    // capture instantly, so they showed the full preview every time). The
    // strip's own movingtab animation is the feedback; the floating image
    // adds nothing. Replace it with a transparent 1px node and shadow
    // updateDragImage on this DataTransfer so the async capture cannot
    // bring the preview back mid-drag. setDragImage must happen while the
    // dragstart event is still being dispatched, and native stopPropagates
    // it — hence the wrap, not a listener. All-tabs-list drags keep their
    // image (the list row does not move, so there the image IS the
    // feedback); row-2 proxies never enter startTabDrag and keep their
    // text-pill ghost.
    let ghostBlank: HTMLElement | null = null;
    const killTabDragGhost = (event: DragEvent) => {
      const dt = event.dataTransfer as
        | (DataTransfer & {
          mozTypesAt?: (i: number) => ArrayLike<string>;
          updateDragImage?: (...args: unknown[]) => void;
        })
        | null;
      if (!dt || dt.mozTypesAt?.(0)?.[0] !== TAB_DROP_TYPE) return;
      if (!ghostBlank) {
        ghostBlank = document.createElement("div");
        ghostBlank.style.cssText =
          "width:1px;height:1px;opacity:0;position:fixed;top:-10px;left:-10px;";
        document.documentElement?.appendChild(ghostBlank);
      }
      try {
        dt.setDragImage(ghostBlank, 0, 0);
        dt.updateDragImage = () => {};
      } catch (e) {
        console.error("[tab-stacks] drag ghost suppression failed:", e);
      }
    };
    type StartTabDragFn = (
      event: DragEvent,
      tab: unknown,
      opts?: { fromTabList?: boolean },
    ) => void;
    const tddStart = tdd as
      | (typeof tdd & {
        startTabDrag?: StartTabDragFn;
        __floorpGhostWrapped?: boolean;
      })
      | undefined;
    if (tddStart?.startTabDrag && !tddStart.__floorpGhostWrapped) {
      const origStartTabDrag = tddStart.startTabDrag.bind(tddStart);
      tddStart.startTabDrag = (event, tab, opts) => {
        origStartTabDrag(event, tab, opts);
        if (!opts?.fromTabList) killTabDragGhost(event);
      };
      tddStart.__floorpGhostWrapped = true;
      onCleanup(() => {
        tddStart.startTabDrag = origStartTabDrag;
        tddStart.__floorpGhostWrapped = false;
        ghostBlank?.remove();
        ghostBlank = null;
      });
    }

    // The native hover preview panel lists a hovered group's member tabs.
    // For a stack that's pure noise: it duplicates row 2, it appears only
    // sometimes (the chip owns click/press, so the hover machinery loses
    // races to other controls), and its rows are display-only. Suppress it
    // for stacks; plain groups keep the native panel. previewPanel is a
    // plain property the hover machinery assigns lazily, so an accessor
    // wraps whatever gets stored there, whenever that happens.
    type PreviewPanel = {
      activate?: (target: unknown) => void;
      deactivate?: (target?: unknown, opts?: { force?: boolean }) => void;
      __floorpNoStackPreview?: boolean;
    };
    const wrapPreviewPanel = (
      panel: PreviewPanel | null | undefined,
    ): PreviewPanel | null | undefined => {
      if (
        !panel || panel.__floorpNoStackPreview ||
        typeof panel.activate !== "function"
      ) {
        return panel;
      }
      const origActivate = panel.activate.bind(panel);
      panel.activate = (target: unknown) => {
        const el = target as
          | (Element & { group?: Element | null })
          | null
          | undefined;
        const groupEl = el?.tagName?.toLowerCase?.() === "tab-group"
          ? el
          : el?.closest?.("tab-group") ?? el?.group ?? null;
        if (groupEl?.getAttribute?.("data-floorp-stack") === "true") {
          try {
            panel.deactivate?.(target, { force: true });
          } catch {
            // panel state stays native's problem
          }
          return;
        }
        return origActivate(target);
      };
      panel.__floorpNoStackPreview = true;
      return panel;
    };
    const tabsElPP = tabsEl as XULElement & {
      previewPanel?: PreviewPanel | null;
    };
    let currentPreviewPanel = wrapPreviewPanel(tabsElPP.previewPanel);
    Object.defineProperty(tabsElPP, "previewPanel", {
      configurable: true,
      get: () => currentPreviewPanel,
      set: (v: PreviewPanel | null) => {
        currentPreviewPanel = wrapPreviewPanel(v);
      },
    });
    onCleanup(() => {
      delete (tabsElPP as { previewPanel?: unknown }).previewPanel;
      (tabsElPP as { previewPanel?: unknown }).previewPanel =
        currentPreviewPanel;
    });

    const onTabEvent = (event: Event) => {
      if (event.type === "TabSelect") {
        rememberSelection();
      }
      // Classify/mark groups first so syncActiveGroup sees fresh marks.
      this.updateGroupChips();
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

    // Chip interactions (native behavior would toggle collapse, which
    // fights our always-expanded model): single click activates the stack,
    // double click opens the native group editor (rename + color).
    const onChipPress = (event: MouseEvent) => {
      if (event.button !== 0) return;
      const target = event.target as Element | null;
      // Chip close button: closes the whole stack (all member tabs).
      const closeEl = target?.closest?.(".floorp-stack-close");
      if (closeEl) {
        const group = closeEl.closest(
          "tab-group[data-floorp-stack]",
        ) as StackGroup | null;
        event.preventDefault();
        event.stopImmediatePropagation();
        if (event.type !== "click" || !group) return;
        try {
          const gb = getGBrowser();
          if (gb?.removeTabs) {
            gb.removeTabs([...group.tabs]);
          } else {
            for (const t of [...group.tabs]) {
              getGBrowser()?.removeTab(t, { animate: false });
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
        "tab-group[data-floorp-stack]",
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
          getGBrowser()?.tabGroupMenu?.openEditModal(group);
        } catch (e) {
          console.error("[tab-stacks] Failed to open group editor:", e);
        }
        return;
      }

      // Immediate: the old 230ms double-click disambiguation timer made
      // every stack switch feel laggy.
      activateGroup(group);
      this.updateGroupChips();
      syncActiveGroup();
      bumpStacksVersion();
    };
    addEventListener("mousedown", onChipPress, true);
    addEventListener("click", onChipPress, true);
    onCleanup(() => {
      removeEventListener("mousedown", onChipPress, true);
      removeEventListener("click", onChipPress, true);
    });

    // After a drag lands, bring the moved tab into view — but ONLY then.
    // Nothing here runs during ordinary left/right scrolling, so manual
    // scrolling is never yanked back.
    const scrollProxyIntoView = (tab: StackTab) => {
      // Let the proxy for the new position render first.
      setTimeout(() => {
        try {
          const bar = document?.getElementById("floorp-stack-bar") as
            | (XULElement & {
              ensureElementIsVisible?: (el: Element, instant?: boolean) => void;
            })
            | null;
          const proxy = bar?.querySelector(
            `.floorp-stack-tab[data-floorp-drag-id="${
              tab.getAttribute("data-floorp-tab-id")
            }"]`,
          );
          if (bar && proxy) bar.ensureElementIsVisible?.(proxy, true);
        } catch (e) {
          console.error("[tab-stacks] scroll-into-view failed:", e);
        }
      }, 30);
    };

    // Row-2 proxies use the *native* tab context menu (reload, duplicate,
    // pin, close, move to…) rather than a parallel one. Firefox resolves
    // which tab the menu acts on as
    //   triggerNode.tab || triggerNode.closest("tab") || selectedTab
    // — so stamping `.tab` on the trigger node is the supported hook. It
    // must land before TabContextMenu.updateContextMenu reads it, hence
    // the capture-phase listener on document: capture runs before the
    // popup's own target-phase handler. Without this the menu would
    // silently act on the *selected* tab instead of the one clicked.
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
    // "New Group" would mint another stack from inside this one, and group
    // membership is stack membership here — moving between stacks is done
    // by dragging. Bubble phase, so it runs AFTER the popup's own
    // TabContextMenu.updateContextMenu has made its visibility calls.
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

    // Stack ⇄ plain-group conversion. This build wires no context menu to
    // group labels (no context= attribute on the label chain), so the
    // feature owns one: right-click on any group label/chip opens it.
    // Split-view groups are excluded — they keep native behavior wholesale.
    const KIND_MENU_ID = "floorp-stack-kind-menu";
    const doc = document as Document & {
      createXULElement: (tag: string) => XULElement;
    };
    let menuGroup: StackGroup | null = null;
    let kindMenu:
      | (XULElement & {
        openPopupAtScreen: (x: number, y: number, isContext: boolean) => void;
      })
      | null = null;
    let kindItem: XULElement | null = null;
    // Every item carries a Stack and a Group wording; the menu serves both
    // kinds and used to say "Reload Stack" etc. on plain groups too.
    const dualLabelItems: Array<
      { item: XULElement; stack: string; group: string }
    > = [];
    const relabelKindMenu = (kind: GroupKind): void => {
      for (const d of dualLabelItems) {
        d.item.setAttribute("label", kind === "stack" ? d.stack : d.group);
      }
    };
    const popupSet = doc.getElementById("mainPopupSet");
    if (popupSet) {
      kindMenu = doc.createXULElement("menupopup") as typeof kindMenu &
        XULElement;
      kindMenu.id = KIND_MENU_ID;
      const makeItem = (
        stackLabel: string,
        groupLabel: string,
        onCommand: () => void,
      ): XULElement => {
        const item = doc.createXULElement("menuitem");
        item.setAttribute("label", stackLabel);
        item.addEventListener("command", onCommand);
        kindMenu?.appendChild(item);
        dualLabelItems.push({ item, stack: stackLabel, group: groupLabel });
        return item;
      };
      makeItem("Reload Stack", "Reload Group", () => {
        const gb = getGBrowser();
        const group = menuGroup;
        if (!group || !gb) return;
        try {
          for (const t of [...group.tabs]) gb.reloadTab?.(t);
        } catch (e) {
          console.error("[tab-stacks] reload stack failed:", e);
        }
      });
      makeItem("New Tab in Stack", "New Tab in Group", () => {
        const gb = getGBrowser();
        const group = menuGroup;
        if (!group || !gb) return;
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
        // Native group editor: rename, colour and group actions — it
        // manages, it doesn't just rename, so the label says Manage.
        const gb = getGBrowser();
        if (menuGroup) gb?.tabGroupMenu?.openEditModal(menuGroup);
      });
      kindMenu.appendChild(doc.createXULElement("menuseparator"));
      kindItem = doc.createXULElement("menuitem");
      kindItem.addEventListener("command", () => {
        if (!menuGroup) return;
        const next: GroupKind = getGroupKind(menuGroup.id) === "stack"
          ? "group"
          : "stack";
        setGroupKind(menuGroup.id, next);
        this.updateGroupChips();
        syncActiveGroup();
        bumpStacksVersion();
      });
      kindMenu.appendChild(kindItem);
      makeItem("Ungroup Stack", "Ungroup Tabs", () => {
        // Dissolve the stack, keep every tab in the strip.
        const gb = getGBrowser();
        const group = menuGroup;
        if (!group || !gb?.ungroupTab) return;
        try {
          for (const t of [...group.tabs]) gb.ungroupTab(t);
        } catch (e) {
          console.error("[tab-stacks] ungroup stack failed:", e);
        }
      });
      kindMenu.appendChild(doc.createXULElement("menuseparator"));
      makeItem("Close Stack", "Close Group", () => {
        const gb = getGBrowser();
        const group = menuGroup;
        if (!group || !gb) return;
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
    // ===== Extra group colours (second swatch row) =====
    // Native populates its swatches once from a static list; ours are
    // appended on first editor open and persist (same delegated change
    // listener on the container picks them up — the colour setter accepts
    // any name that has --tab-group-color-<name> vars, see styles.css).
    const EXTRA_GROUP_COLORS: Array<{ name: string; label: string }> = [
      { name: "crimson", label: "Crimson" },
      { name: "amber", label: "Amber" },
      { name: "lime", label: "Lime" },
      { name: "teal", label: "Teal" },
      { name: "indigo", label: "Indigo" },
      { name: "magenta", label: "Magenta" },
      { name: "brown", label: "Brown" },
      { name: "slate", label: "Slate" },
    ];
    const onGroupEditorShowing = (event: Event) => {
      const panel = event.target as Element;
      if (!(panel instanceof Element) || panel.localName !== "panel") return;
      const container = panel.querySelector?.(".tab-group-editor-swatches");
      if (!container) return;
      try {
        if (!container.querySelector("[data-floorp-extra-swatch]")) {
          for (const c of EXTRA_GROUP_COLORS) {
            const input = document.createElement(
              "input",
            ) as HTMLInputElement;
            input.id = `tab-group-editor-swatch-${c.name}`;
            input.type = "radio";
            input.name = "tab-group-color";
            input.value = c.name;
            input.setAttribute("data-floorp-extra-swatch", "true");
            const label = document.createElement("label") as HTMLElement;
            label.classList.add("tab-group-editor-swatch");
            label.setAttribute("data-floorp-extra-swatch", "true");
            (label as HTMLElement & { htmlFor: string }).htmlFor = input.id;
            label.title = c.label;
            label.style.setProperty(
              "--tabgroup-swatch-color",
              `var(--tab-group-color-${c.name})`,
            );
            label.style.setProperty(
              "--tabgroup-swatch-color-invert",
              `var(--tab-group-color-${c.name}-invert)`,
            );
            container.append(input, label);
          }
        }
        // Native's open bookkeeping only checks/unchecks ITS swatches;
        // reflect the edited group's colour on ours each open.
        const menu = (getGBrowser() as unknown as {
          tabGroupMenu?: { activeGroup?: { color?: string } | null };
        } | null)?.tabGroupMenu;
        const color = menu?.activeGroup?.color ?? null;
        for (
          const r of container.querySelectorAll(
            "input[data-floorp-extra-swatch]",
          ) as NodeListOf<HTMLInputElement>
        ) {
          r.checked = !!color && r.value === color;
        }
      } catch (e) {
        console.error("[tab-stacks] extra colour swatches failed:", e);
      }
    };
    document.addEventListener("popupshowing", onGroupEditorShowing);
    onCleanup(() => {
      document?.removeEventListener("popupshowing", onGroupEditorShowing);
    });

    const onLabelContextMenu = (event: MouseEvent) => {
      const target = event.target as Element | null;
      const labelContainer = target?.closest?.(".tab-group-label-container");
      const group = (labelContainer?.closest?.("tab-group") ??
        null) as StackGroup | null;
      if (!group || !kindMenu || !kindItem) return;
      if (this.isSplitViewGroup(group)) return;
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

    // A tab dragged PAST a stack chip must reorder, not join: the
    // collapsed member tabs cluster at the chip's edge with zero width,
    // so the native drop math counts the whole neighbourhood as "inside
    // the group". Joining a stack deliberately stays possible by
    // dropping ON the chip itself. Detection is post-hoc: if a TabGrouped
    // lands on one of our stacks while a tab drag is in flight and the
    // drop point was not on the chip, the tab is moved back out to the
    // side of the stack it was dropped on.
    let tabDragActive = false;
    let lastDropPoint: { x: number; y: number } | null = null;
    let lastTabDropTime = 0;
    let lastProxyStripScroll = 0;
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
      // validation (dragstart never fired here, so tabDragActive alone
      // would miss it).
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
      tabsEl.removeAttribute("data-floorp-stack-droptarget");
      // TabGrouped from a drop arrives before dragend; clear on a delay,
      // then reconcile the bar with wherever selection actually landed.
      setTimeout(() => {
        tabDragActive = false;
        lastDropPoint = null;
        this.updateGroupChips();
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
      if (!group) return;
      if (
        (group as unknown as Element).getAttribute?.("data-floorp-stack") !==
          "true"
      ) {
        // Dropped into a plain group: native scrolls the strip to the
        // group's start (its icon), not to the tab that just landed. Once
        // the native machinery settles, bring the dropped tab itself into
        // view — the whole point of the gesture is seeing where it went.
        setTimeout(() => {
          try {
            if (tab.isConnected && tab.group === group) {
              asb?.ensureElementIsVisible(tab as unknown as Element, false);
            }
          } catch {
            // best-effort scroll
          }
        }, 120);
        return;
      }
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
        const gb = getGBrowser();
        // Out of the group, landing right after it; left-side drops then
        // hop before it.
        gb?.ungroupTab?.(tab);
        if (p.x < r.x) {
          gb?.moveTabBefore?.(tab, group as unknown as StackTab);
        }
      } catch (e) {
        console.error("[tab-stacks] drop-outside eject failed:", e);
      }
    };
    // Deliberate join: dropping a row-1 tab ON the chip adds it to the
    // stack. With the member tabs zero-width, the native drop math almost
    // never resolves to "join" by itself, so the chip performs it.
    const PROXY_TYPE = "application/x-floorp-stack-tab";
    const onChipDrop = (event: DragEvent) => {
      if (event.dataTransfer?.types?.includes(PROXY_TYPE)) return;
      const target = event.target as Element | null;
      const lc = target?.closest?.(".tab-group-label-container");
      const group = (lc?.closest?.("tab-group[data-floorp-stack]") ??
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
          this.updateGroupChips();
          syncActiveGroup();
          bumpStacksVersion();
          // A busy bar scrolls the newcomer into view.
          scrollProxyIntoView(src);
        } catch (e) {
          console.error("[tab-stacks] chip drop join failed:", e);
        }
      }, 0);
    };

    // Proxy drops: reorder inside the bar, or leave the stack by dropping
    // into the tab row.

    // The native strip drop indicator (.tab-drop-indicator) — the same
    // blue line link drops show. Native hides it for our custom drag type,
    // so the proxy path positions it directly. Cell-edge box-shadows were
    // tried first and failed exactly where they matter most: inset on a
    // stack chip they sat flush against the chip's coloured border and
    // were unreadable.
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
      // Mirror the native margin math (drag-and-drop.js handle_dragover):
      // unhide first — a hidden indicator measures clientWidth 0.
      ind.hidden = false;
      const margin = boundaryX - rect.left + ind.clientWidth / 2;
      ind.style.transform = `translateX(${Math.round(margin)}px)`;
    };
    const clearDropIndicators = () => {
      for (
        const el of document.querySelectorAll("[data-drop-side]")
      ) {
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
    // mozSourceNode is readable during dragover (unlike getData); a proxy's
    // source is a .floorp-stack-tab, not a .tabbrowser-tab, so this fires for
    // native tab drags only — exactly the join case.
    const onChipDragOverHighlight = (event: DragEvent) => {
      const target = event.target as Element | null;
      const chip = target?.closest?.(".tab-group-label-container")
        ?.closest?.("tab-group[data-floorp-stack]");
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
    // Row-1 insertion targets for proxy drags: ungrouped tabs AND whole
    // groups. A stack chip (or a plain group) is one atomic cell — a proxy
    // drops beside it, never between its members. Without the groups in
    // this list, hovering directly left/right of a chip showed no
    // indicator and the drop resolved against some tab further away.
    type StripItem = {
      el: Element;
      group: StackGroup | null;
      x: number;
      width: number;
    };
    const collectStripItems = (excludeTab: StackTab | null): StripItem[] => {
      const gb2 = getGBrowser();
      if (!gb2) return [];
      const items: StripItem[] = [];
      for (const t of gb2.tabs) {
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
      for (const g of gb2.tabGroups) {
        const ge = g as unknown as Element;
        const lc = ge.querySelector(".tab-group-label-container");
        const lcRect = lc?.getBoundingClientRect();
        if (!lc || !lcRect || lcRect.width === 0) continue;
        let right = lcRect.right;
        if (ge.getAttribute("data-floorp-stack") !== "true") {
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
      if (!event.dataTransfer?.types?.includes(PROXY_TYPE)) return;
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
      // just positioned (it runs later — target phase vs our capture).
      // It does nothing else for proxy drags (even its autoscroll sits
      // behind the same "none" early-return), so cut it out entirely.
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
        // Releasing on the proxy's own chip is a no-op (see onProxyDrop) —
        // show no line there.
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
          ?.closest?.("tab-group[data-floorp-stack]");
        if (hoverChip && hoverChip === (srcTab?.group as unknown as Element)) {
          return;
        }
        // Autoscroll an overflowing strip while the proxy hovers near its
        // ends — native only autoscrolls its own drag types, so far-away
        // slots were unreachable in one gesture. Throttled; instant, so
        // the geometry below is measured post-scroll.
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
        // The native blue line marks the exact insertion boundary: the
        // trailing edge of the cell the drop will follow (or the leading
        // edge of the first cell for a drop at the very start).
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
      const dragId = event.dataTransfer?.getData(PROXY_TYPE);
      if (!dragId) return;
      const gb = getGBrowser();
      const tab = findTabByDragId(dragId);
      if (!gb || !tab) return;
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
          // container) so the two dead spots work: the very start (over the
          // workspace chooser / pre-tab spacer) and the gap right next to a
          // stack chip at the end. Only releasing ON the tab's own chip
          // label is a no-op — you did not drag anywhere.
          const ownLabel = target?.closest?.(".tab-group-label-container");
          const ownChip = ownLabel?.closest?.("tab-group[data-floorp-stack]");
          if (ownChip && (ownChip === (tab.group as unknown as Element))) {
            return;
          }
          // Resolve the drop target from CURRENT geometry, before any
          // mutation: ungroupTab() inserts the tab into the strip and
          // shifts every tab right of the group, so measuring afterwards
          // resolved against moved boxes and landed a slot early.
          // Whole groups count as cells too, so a drop directly beside a
          // stack chip lands exactly there (moveTabAfter/Before with a
          // tab-group element places the tab outside, adjacent to it).
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
        this.updateGroupChips();
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
        this.updateGroupChips();
      }, 80);
    };
    const observer = new MutationObserver(scheduleRefresh);
    observer.observe(gb.tabContainer as unknown as Node, {
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
    this.updateGroupChips();
    syncActiveGroup();
    bumpStacksVersion();
    this.logger.info("Tab stacks initialized");
  }


  /**
   * Classify groups and keep chips informative. Only groups we MARK with
   * data-floorp-stack get stack behavior (CSS hiding, forced expansion, chip
   * click handling). Split-view binds its panes with tab groups too — those
   * must be left completely alone or the two features fight (runaway
   * revive/expand loop, mangled tabs, eventual crash).
   */
  private isSplitViewGroup(group: StackGroup): boolean {
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

  private updateGroupChips(): void {
    const gb = getGBrowser();
    if (!gb) return;
    for (const group of gb.tabGroups) {
      try {
        if (this.isSplitViewGroup(group)) {
          group.removeAttribute("data-floorp-stack");
          group.querySelector(".floorp-stack-close")?.remove();
          group.querySelector(".floorp-stack-icon")?.remove();
          for (
            const sel of [
              ".tab-group-label-container",
              ".tab-group-label-hover-highlight",
            ]
          ) {
            group.querySelector(sel)?.setAttribute("pack", "center");
          }
          continue;
        }
        if (getGroupKind(group.id) === "group") {
          // Plain tab group by user choice: native presentation end to
          // end. All stack CSS/behavior is scoped to [data-floorp-stack].
          group.removeAttribute("data-floorp-stack");
          group.querySelector(".floorp-stack-close")?.remove();
          group.querySelector(".floorp-stack-icon")?.remove();
          // An unnamed group's label holds a zero-width space rather than
          // nothing at all, so :empty never matches it and the flex gap
          // still reserves room for that invisible character — which is
          // exactly what made the icon chip wider than it is tall. Mark it
          // so the stylesheet can square it off.
          const plainLabel = group.querySelector(".tab-group-label");
          const visibleName = plainLabel?.textContent
            ?.replace(/[\s\u200B-\u200D\uFEFF]/g, "") ?? "";
          group.toggleAttribute("data-floorp-unnamed", !visibleName);

          // Stock's "+N" badge counts every tab in the group, which assumes
          // they are all on the strip. Workspaces breaks that assumption: a
          // group's tabs can be parked in other workspaces and hidden, so the
          // badge promised more tabs than existed here (six-tab group, two in
          // another workspace, four on screen, badge saying five). Count only
          // what this workspace can actually reach. Stock rewrites the label
          // from an async Fluent lookup, so this re-corrects on the same tick
          // that everything else here runs on.
          // Stock shows "+N" meaning "N other tabs besides the active one", so
          // a three-tab group read as 2. Beside a bare icon that just looks
          // like a miscount — show how many tabs are in there, and drop the
          // plus that promised more on top.
          const reachable =
            Array.from(group.tabs ?? []).filter((t) => !t.hidden).length;
          group.toggleAttribute("hasmultipletabs", reachable > 1);
          const countLabel = group.querySelector(".tab-group-overflow-count");
          if (countLabel && countLabel.textContent !== String(reachable)) {
            countLabel.textContent = String(reachable);
          }
          // Long typed names: the label grows to the stack chip's cap and
          // then fades out (styles.css) — but only mark it cropped when
          // the text actually overflows. The box hugs its content, so an
          // unconditional fade would eat the tail of every short name.
          const lp = plainLabel as
            | (Element & { scrollWidth?: number; clientWidth?: number })
            | null;
          if (lp && typeof lp.scrollWidth === "number") {
            lp.toggleAttribute(
              "data-floorp-cropped",
              lp.scrollWidth > (lp.clientWidth ?? 0) + 1,
            );
          }
          for (
            const sel of [
              ".tab-group-label-container",
              ".tab-group-label-hover-highlight",
            ]
          ) {
            group.querySelector(sel)?.setAttribute("pack", "center");
          }
          continue;
        }
        group.setAttribute("data-floorp-stack", "true");
        // Every stack carries a name. Unnamed stacks used to derive the
        // chip title from a member tab — but real pages retitle themselves
        // on focus (SPAs, notification counters), so the chip appeared to
        // rename and resize on every click inside the stack. A stable
        // auto-name (New Stack, New Stack 1, …) ends that for good; the
        // native label persists via session restore and prefills the
        // rename editor.
        if (!group.label) {
          group.label = nextAutoStackName(gb);
        }
        // pack="center" is a XUL layout attribute mapped to
        // justify-content: center as a presentation hint — author CSS
        // cannot beat it at any specificity, not even inline !important.
        // It centred the chip in the 2px-taller row, leaving it 1px below
        // the tab selection boxes. The attribute is the only lever.
        for (
          const sel of [
            ".tab-group-label-container",
            ".tab-group-label-hover-highlight",
          ]
        ) {
          group.querySelector(sel)?.setAttribute("pack", "start");
        }
        // Close button lives inside the label so it rides the chip's flex
        // row (order puts it after the count badge); shown on hover.
        const labelEl = group.querySelector(".tab-group-label");
        const makeImage = (): XULElement =>
          (document as Document & {
            createXULElement: (tag: string) => XULElement;
          }).createXULElement("image");
        if (labelEl && !labelEl.querySelector(".floorp-stack-icon")) {
          const icon = makeImage();
          icon.classList.add("floorp-stack-icon");
          icon.setAttribute("src", STACK_ICON);
          labelEl.appendChild(icon);
        }
        if (labelEl && !labelEl.querySelector(".floorp-stack-close")) {
          const close = makeImage();
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
        // The label always exists now (auto-named above); the member-tab
        // fallback only covers the beat between group creation and the
        // auto-name landing. Bound either way; the chip fades the rest.
        const chipTitle = group.label || getGroupDisplayTitle(group);
        label?.setAttribute(
          "data-floorp-title",
          chipTitle.length > 60 ? `${chipTitle.slice(0, 60)}…` : chipTitle,
        );
      } catch {
        // Group may be mid-removal; the next event refreshes it.
      }
    }
  }
}
