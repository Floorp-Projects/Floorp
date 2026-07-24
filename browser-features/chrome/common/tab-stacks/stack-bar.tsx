/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createMemo, createSignal, For, Show } from "solid-js";
import styles from "./styles.css?inline";

export function StackStyleElement() {
  return <style>{styles}</style>;
}

export type StackTab = XULElement & {
  label: string;
  selected: boolean;
  linkedPanel: string;
  group: StackGroup | null;
};

export type StackGroup = XULElement & {
  id: string;
  label: string;
  collapsed: boolean;
  tabs: StackTab[];
  addTabs: (tabs: StackTab[]) => void;
};

type TabBrowser = {
  tabs: StackTab[];
  selectedBrowser?: { currentURI?: { spec?: string } };
  tabGroups: StackGroup[];
  selectedTab: StackTab;
  tabContainer: XULElement;
  tabGroupMenu?: { openEditModal: (group: StackGroup) => void };
  removeTab: (tab: StackTab, opts?: Record<string, unknown>) => void;
  removeTabs?: (tabs: StackTab[], opts?: Record<string, unknown>) => void;
  addTab: (url: string, opts: Record<string, unknown>) => StackTab;
  ungroupTab?: (tab: StackTab) => void;
  reloadTab?: (tab: StackTab) => void;
  moveTabBefore?: (tab: StackTab, target: StackTab) => void;
  moveTabAfter?: (tab: StackTab, target: StackTab) => void;
};

export const getGBrowser = (): TabBrowser | null =>
  (globalThis as unknown as { gBrowser?: TabBrowser }).gBrowser ?? null;

/** Bumped on any tab/group mutation so proxies re-read live tab state. */
const [version, setVersion] = createSignal(0);
export const bumpStacksVersion = () => setVersion((v) => v + 1);

const [activeGroup, setActiveGroup] = createSignal<StackGroup | null>(null);

/** The stack bar shows the group of the currently selected tab — but only
 * groups we own (marked data-floorp-stack); split-view groups are not ours. */
export const syncActiveGroup = () => {
  const gb = getGBrowser();
  const group = gb?.selectedTab?.group ?? null;
  const isOurs = group &&
    (group as unknown as Element).getAttribute?.("data-floorp-stack") ===
      "true";
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

/** Title shown on an unnamed stack chip: its first REACHABLE tab, as a
 * stable anchor. It used to track the last tab clicked inside the stack,
 * which read as the chip constantly renaming itself while you worked in
 * it. Workspaces can park members hidden — never title the chip after a
 * tab this workspace cannot even see.
 * (A user-given name always wins over this — see updateGroupChips.) */
export const getGroupDisplayTitle = (group: StackGroup): string => {
  const anchor = group.tabs.find((t) => !(t as { hidden?: boolean }).hidden) ??
    group.tabs[0];
  const label = anchor?.label ?? "";
  // Bound the attribute size only — visually the chip's box crops and
  // fades the text like a tab, so the cut must stay past the widest chip.
  return label.length > 60 ? `${label.slice(0, 60)}…` : label;
};

export const activateGroup = (group: StackGroup) => {
  const gb = getGBrowser();
  if (!gb) return;
  const remembered = lastSelectedInGroup.get(group);
  const usable = (t: StackTab | undefined): t is StackTab =>
    !!t && t.isConnected && t.group === group &&
    !(t as { hidden?: boolean }).hidden;
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
 * an unloaded tab carried an empty id and the drop silently no-opped —
 * "only works the second try". Stamped on demand and good for the tab's
 * whole life.
 */
let dragIdCounter = 0;
export const getTabDragId = (tab: StackTab): string => {
  let id = tab.getAttribute("data-floorp-tab-id");
  if (!id) {
    id = `dt${++dragIdCounter}`;
    tab.setAttribute("data-floorp-tab-id", id);
  }
  return id;
};

export const findTabByDragId = (id: string): StackTab | undefined =>
  getGBrowser()?.tabs.find((t) => t.getAttribute("data-floorp-tab-id") === id);

function StackTabProxy(props: { tab: StackTab }) {
  // Inline address edit: double-click widens the cell into a text input,
  // Enter navigates that tab, Escape/blur closes. Editing a tab in place
  // beats selecting it, reaching for the address bar and coming back.
  const [editing, setEditing] = createSignal(false);

  const currentUrl = (): string => {
    try {
      const b = props.tab.linkedBrowser as
        | { currentURI?: { spec?: string } }
        | undefined;
      const spec = b?.currentURI?.spec ?? "";
      return spec === "about:newtab" || spec === "about:blank" ? "" : spec;
    } catch {
      return "";
    }
  };

  const startEditing = () => {
    setEditing(true);
    // Focus once solid-xul has swapped the row for the input (the
    // universal renderer gives us no `ref` helper, hence the id lookup).
    setTimeout(() => {
      const input = document?.getElementById(
        `floorp-stack-url-${getTabDragId(props.tab)}`,
      ) as HTMLInputElement | null;
      input?.focus();
      input?.select();
    }, 0);
  };

  const commit = (value: string) => {
    setEditing(false);
    const url = value.trim();
    if (!url) return;
    try {
      const uri = Services.uriFixup.getFixupURIInfo(
        url,
        Ci.nsIURIFixup.FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP |
          Ci.nsIURIFixup.FIXUP_FLAG_FIX_SCHEME_TYPOS,
      ).preferredURI;
      if (!uri) return;
      // Typed input must never run script or spoof a document.
      if (uri.schemeIs("javascript") || uri.schemeIs("data")) return;
      const b = props.tab.linkedBrowser as
        | { fixupAndLoadURIString?: (u: string, o: unknown) => void }
        | undefined;
      b?.fixupAndLoadURIString?.(uri.spec, {
        triggeringPrincipal: Services.scriptSecurityManager
          .getSystemPrincipal(),
      });
    } catch (e) {
      console.error("[tab-stacks] inline address load failed:", e);
    }
  };

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
        event.dataTransfer?.setData(
          "application/x-floorp-stack-tab",
          getTabDragId(props.tab),
        );
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
      data-editing={editing() ? "true" : "false"}
      onClick={() => {
        if (editing()) return;
        const gb = getGBrowser();
        if (gb) {
          gb.selectedTab = props.tab;
        }
      }}
      onDblClick={(event: MouseEvent) => {
        event.stopPropagation();
        startEditing();
      }}
    >
      <Show
        when={!editing()}
        fallback={
          <input
            id={`floorp-stack-url-${getTabDragId(props.tab)}`}
            class="floorp-stack-url-input"
            value={currentUrl()}
            placeholder="Enter address"
            onKeyDown={(e: KeyboardEvent) => {
              if (e.key === "Enter") {
                commit((e.currentTarget as HTMLInputElement).value);
              } else if (e.key === "Escape") {
                setEditing(false);
              }
              // The strip's own key handling must not see this typing.
              e.stopPropagation();
            }}
            onBlur={() => setEditing(false)}
          />
        }
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
      </Show>
    </xul:hbox>
  );
}

export function StackBar() {
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

  return (
    <Show when={activeGroup()}>
      {/* arrowscrollbox: one row forever — overflow scrolls behind arrow
          buttons exactly like the tab strip, instead of wrapping into a
          second row and growing the bar. */}
      <xul:arrowscrollbox
        id="floorp-stack-bar"
        orient="horizontal"
        clicktoscroll="true"
        align="center"
      >
        <For each={tabs()}>{(tab) => <StackTabProxy tab={tab} />}</For>
        <xul:toolbarbutton
          id="floorp-stack-newtab"
          label="+"
          tooltiptext="New tab in stack"
          onClick={addTabToActiveGroup}
        />
      </xul:arrowscrollbox>
    </Show>
  );
}
