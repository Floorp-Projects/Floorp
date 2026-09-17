/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createMemo, createSignal, For, onCleanup, onMount, Show } from "solid-js";
import styles from "./styles.css?inline";

/** Drag data type used by row-2 proxy drags (reorder / eject). */
export const PROXY_DRAG_TYPE = "application/x-floorp-stack-tab";
/** Attribute stamped on every `tab-group` this feature presents as a stack. */
export const STACK_ATTR = "data-floorp-stack";
/** Per-tab drag identity attribute (stable for the tab's whole life). */
export const TAB_DRAG_ID_ATTR = "data-floorp-tab-id";

export function StackStyleElement() {
  return <style>{styles}</style>;
}

export type StackTab = XULElement & {
  label: string;
  selected: boolean;
  linkedPanel: string;
  group: StackGroup | null;
  hidden?: boolean;
  closing?: boolean;
  multiselected?: boolean;
  isConnected?: boolean;
  linkedBrowser?: {
    currentURI?: { spec?: string };
  };
};

export type StackGroup = XULElement & {
  id: string;
  label: string;
  collapsed: boolean;
  tabs: StackTab[];
  addTabs: (tabs: StackTab[]) => void;
  style: CSSStyleDeclaration;
};

export type TabBrowser = {
  tabs: StackTab[];
  selectedTab: StackTab;
  tabGroups: StackGroup[];
  tabContainer: XULElement;
  tabGroupMenu?: { openEditModal: (group: StackGroup) => void };
  removeTab: (tab: StackTab, opts?: Record<string, unknown>) => void;
  removeTabs?: (tabs: StackTab[], opts?: Record<string, unknown>) => void;
  addTab: (url: string, opts: Record<string, unknown>) => StackTab;
  ungroupTab?: (tab: StackTab) => void;
  reloadTab?: (tab: StackTab) => void;
  moveTabBefore?: (
    tab: StackTab | StackGroup,
    target: StackTab | StackGroup,
  ) => void;
  moveTabAfter?: (
    tab: StackTab | StackGroup,
    target: StackTab | StackGroup,
  ) => void;
};

export const getGBrowser = (): TabBrowser | null =>
  (globalThis as unknown as { gBrowser?: TabBrowser }).gBrowser ?? null;

/** Bumped on any tab/group mutation so proxies re-read live tab state. */
const [version, setVersion] = createSignal(0);
export const bumpStacksVersion = () => setVersion((v) => v + 1);

const [activeGroup, setActiveGroup] = createSignal<StackGroup | null>(null);

export const getActiveGroup = () => activeGroup();

/**
 * The stack bar shows the group of the currently selected tab — but only
 * groups we own (marked data-floorp-stack). Split-view groups are not ours,
 * and groups hidden by a workspace (`style.display === "none"`) must never
 * surface their bar either.
 */
export const syncActiveGroup = () => {
  const gb = getGBrowser();
  const group = gb?.selectedTab?.group ?? null;
  const isOurs = group &&
    (group as unknown as Element).getAttribute?.(STACK_ATTR) === "true" &&
    group.style?.display !== "none";
  setActiveGroup(isOurs ? group : null);
};

/** Remember the last selected tab per group so chip clicks feel right. */
const lastSelectedInGroup = new WeakMap<StackGroup, StackTab>();

export const rememberSelection = () => {
  const gb = getGBrowser();
  const tab = gb?.selectedTab;
  const group = tab?.group;
  if (tab && group) {
    lastSelectedInGroup.set(group, tab);
  }
};

/**
 * Title shown on an unnamed stack chip: its first REACHABLE tab, as a
 * stable anchor. Workspaces can park members hidden — never title the chip
 * after a tab this workspace cannot even see.
 */
export const getGroupDisplayTitle = (group: StackGroup): string => {
  const anchor = group.tabs.find((t) => !t.hidden) ?? group.tabs[0];
  const label = anchor?.label ?? "";
  return label.length > 60 ? `${label.slice(0, 60)}…` : label;
};

export const activateGroup = (group: StackGroup) => {
  const gb = getGBrowser();
  if (!gb) return;
  const remembered = lastSelectedInGroup.get(group);
  const usable = (t: StackTab | undefined): t is StackTab =>
    !!t && t.isConnected !== false && t.group === group && !t.hidden;
  // Never select a workspace-hidden member — that yanks the session into
  // another workspace's state. First reachable tab is the fallback.
  const target = usable(remembered)
    ? remembered
    : group.tabs.find((t) => usable(t)) ?? null;
  if (target) {
    gb.selectedTab = target;
  }
};

const DEFAULT_FAVICON = "chrome://global/skin/icons/defaultFavicon.svg";

/**
 * Stable per-tab drag identity. NOT linkedPanel: that is null until a tab
 * has a browser attached (lazy/session-restored tabs), so a first drag of
 * an unloaded tab would carry an empty id. Stamped on demand and good for
 * the tab's whole life.
 */
let dragIdCounter = 0;
export const getTabDragId = (tab: StackTab): string => {
  let id = tab.getAttribute(TAB_DRAG_ID_ATTR);
  if (!id) {
    id = `dt${++dragIdCounter}`;
    tab.setAttribute(TAB_DRAG_ID_ATTR, id);
  }
  return id;
};

export const findTabByDragId = (id: string): StackTab | undefined =>
  getGBrowser()?.tabs.find((t) => t.getAttribute(TAB_DRAG_ID_ATTR) === id);

/** Pixels per wheel/arrow notch for the overflow scroller. */
const SCROLL_STEP_PX = 48;

function StackTabProxy(props: { tab: StackTab }) {
  const label = () => {
    version();
    return props.tab.label;
  };
  const icon = () => {
    version();
    return props.tab.getAttribute("image") || DEFAULT_FAVICON;
  };
  const selected = () => {
    version();
    return props.tab.selected ? "true" : "false";
  };

  return (
    <xul:hbox
      class="floorp-stack-tab"
      align="center"
      data-selected={selected()}
      data-floorp-drag-id={getTabDragId(props.tab)}
      context="tabContextMenu"
      draggable="true"
      onDragStart={(event: DragEvent) => {
        // Proxies drag with a private type: within the bar to reorder
        // the stack, up into the tab row to leave it (the real member
        // tabs are hidden in row 1, so this is the only drag handle).
        event.dataTransfer?.setData(PROXY_DRAG_TYPE, getTabDragId(props.tab));
        if (event.dataTransfer) {
          event.dataTransfer.effectAllowed = "move";
          try {
            const ghost = document!.createElement("div");
            ghost.className = "floorpStackDragGhost";
            ghost.textContent = props.tab.label ?? "";
            document!.documentElement?.appendChild(ghost);
            event.dataTransfer.setDragImage(ghost, 12, 14);
            setTimeout(() => ghost.remove(), 0);
          } catch {
            // default drag image
          }
        }
      }}
      onClick={() => {
        const gb = getGBrowser();
        if (gb) {
          gb.selectedTab = props.tab;
        }
      }}
    >
      <xul:hbox class="floorp-stack-tab-iconbox" align="center">
        <xul:image class="floorp-stack-tab-icon" src={icon()} />
        <xul:toolbarbutton
          class="floorp-stack-tab-close"
          tooltiptext="Close tab"
          onClick={(event: MouseEvent) => {
            event.stopPropagation();
            getGBrowser()?.removeTab(props.tab, { animate: false });
          }}
        />
      </xul:hbox>
      <xul:label class="floorp-stack-tab-label" crop="end">
        {label()}
      </xul:label>
      <xul:image
        class="floorp-stack-tab-refresh"
        src="chrome://global/skin/icons/reload.svg"
        tooltiptext="Reload tab"
        onMouseDown={(event: MouseEvent) => {
          event.stopPropagation();
          event.preventDefault();
        }}
        onClick={(event: MouseEvent) => {
          event.stopPropagation();
          getGBrowser()?.reloadTab?.(props.tab);
        }}
      />
    </xul:hbox>
  );
}

function StackRow() {
  const tabs = createMemo(() => {
    version();
    const group = activeGroup();
    return group ? [...group.tabs] : [];
  });

  const addTabToActiveGroup = () => {
    const gb = getGBrowser();
    const group = activeGroup();
    if (!gb || !group) return;
    try {
      const newTabUrl =
        (globalThis as unknown as { BROWSER_NEW_TAB_URL?: string })
          .BROWSER_NEW_TAB_URL ?? "about:newtab";
      const tab = gb.addTab(newTabUrl, {
        triggeringPrincipal: Services.scriptSecurityManager
          .getSystemPrincipal(),
        skipAnimation: true,
      });
      group.addTabs([tab]);
      gb.selectedTab = tab;
    } catch (e) {
      console.error("[tab-stacks] Failed to add tab to group:", e);
    }
  };

  const scrollBy = (dir: number) => () => {
    const scroller = document?.getElementById("floorp-stack-scroller");
    scroller?.scrollBy({ left: dir * SCROLL_STEP_PX, behavior: "smooth" });
  };

  // The scroller is overflow-x:auto with a hidden scrollbar; this single
  // wheel handler drives horizontal scrolling so a vertical wheel over the
  // bar does not fight the tab strip's own vertical listeners.
  onMount(() => {
    const scroller = document?.getElementById("floorp-stack-scroller");
    if (!scroller) return;
    const onWheel = (event: WheelEvent) => {
      if (scroller.scrollWidth - scroller.clientWidth <= 1) return;
      const delta = Math.abs(event.deltaX) > Math.abs(event.deltaY)
        ? event.deltaX
        : event.deltaY;
      if (delta === 0) return;
      event.preventDefault();
      event.stopPropagation();
      scroller.scrollLeft += delta;
    };
    scroller.addEventListener("wheel", onWheel, { passive: false });
    onCleanup(() => scroller.removeEventListener("wheel", onWheel));
  });

  return (
    <xul:hbox id="floorp-stack-bar" align="center">
      <xul:toolbarbutton
        id="floorp-stack-scroll-up"
        class="floorp-stack-scrollbutton"
        tooltiptext="Scroll tabs left"
        onClick={scrollBy(-1)}
      />
      <xul:hbox id="floorp-stack-scroller">
        <xul:hbox id="floorp-stack-items" align="center">
          <For each={tabs()}>{(tab) => <StackTabProxy tab={tab} />}</For>
        </xul:hbox>
      </xul:hbox>
      <xul:toolbarbutton
        id="floorp-stack-scroll-down"
        class="floorp-stack-scrollbutton"
        tooltiptext="Scroll tabs right"
        onClick={scrollBy(1)}
      />
      {/* Outside the scroller and last in the row, which is where row 1
          keeps the "+" it actually shows: #new-tab-button is a sibling
          AFTER #tabbrowser-tabs, so it sits past the right arrow and never
          scrolls away. */}
      <xul:toolbarbutton
        id="floorp-stack-newtab"
        tooltiptext="New tab in stack"
        onClick={addTabToActiveGroup}
      />
    </xul:hbox>
  );
}

export function StackBar() {
  return (
    <Show when={activeGroup()}>
      <StackRow />
    </Show>
  );
}
