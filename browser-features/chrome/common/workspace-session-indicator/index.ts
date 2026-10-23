// SPDX-License-Identifier: MPL-2.0

import {
  noraComponent,
  NoraComponentBase,
} from "#features-chrome/utils/base.ts";
import { createEffect, onCleanup } from "solid-js";
import Workspaces from "../workspaces/index.ts";
import { enabled } from "../workspaces/data/config.ts";
import {
  selectedWorkspaceID,
  workspacesDataStore,
} from "../workspaces/data/data.ts";
import { tabSessionState, workspaceSessionLabel } from "./session-info.ts";
import type { SessionIndicatorTab } from "./types.ts";
import styles from "./styles.css?inline";

@noraComponent(import.meta.hot)
export default class WorkspaceSessionIndicator extends NoraComponentBase {
  init(): void {
    const win = window;
    const style = document.createElementNS(
      "http://www.w3.org/1999/xhtml",
      "style",
    );
    style.textContent = styles;
    document.documentElement.appendChild(style);
    let scheduled = false;
    let disposed = false;
    const badges = new Map<Element, Element>();
    const clear = () => {
      for (const badge of badges.values()) badge.remove();
      badges.clear();
      for (const tab of globalThis.gBrowser.tabs) {
        tab.removeAttribute("floorp-session-exception");
        tab.removeAttribute("floorp-session-pending");
      }
    };
    const render = () => {
      scheduled = false;
      if (disposed) return;
      const ctx = Workspaces.getCtx(win);
      if (!ctx || !enabled()) {
        clear();
        return;
      }
      for (const id of workspacesDataStore.order) {
        const workspace = ctx.getRawWorkspace(id);
        if (!workspace) continue;
        const label = workspaceSessionLabel(
          workspace.name,
          workspace.userContextId ?? 0,
        );
        for (
          const button of document.querySelectorAll(`[id="workspace-${id}"]`)
        ) {
          button.setAttribute("aria-label", label);
          button.setAttribute("title", label);
          if (button.localName === "toolbarbutton") {
            button.setAttribute("label", label);
            button.setAttribute("tooltiptext", label);
          }
        }
      }
      const alive = new Set<Element>();
      for (const browserTab of globalThis.gBrowser.tabs) {
        const tab = browserTab as unknown as SessionIndicatorTab;
        const workspaceId =
          ctx.tabManagerCtx.getWorkspaceIdFromAttribute(browserTab) ??
            ctx.getSelectedWorkspaceID();
        const workspace = ctx.getRawWorkspace(workspaceId);
        if (!workspace || !tab.linkedBrowser) continue;
        const state = tabSessionState(tab, workspace.userContextId ?? 0);
        const visible = state.exceptional || state.unavailable;
        tab.toggleAttribute("floorp-session-exception", state.exceptional);
        tab.toggleAttribute("floorp-session-pending", state.pending);
        const panel = tab.linkedPanel
          ? document.getElementById(tab.linkedPanel)
          : null;
        const targets: Element[] = [tab];
        if (
          panel &&
          (panel.classList.contains("split-view-panel") || visible)
        ) targets.push(panel);
        for (const target of targets) {
          alive.add(target);
          let badge = badges.get(target);
          if (!visible && target === tab) {
            badge?.remove();
            badges.delete(target);
            continue;
          }
          const elementName = state.pending ? "button" : "span";
          if (badge && badge.localName !== elementName) {
            badge.remove();
            badges.delete(target);
            badge = undefined;
          }
          if (!badge || !badge.isConnected) {
            badge = document.createElementNS(
              "http://www.w3.org/1999/xhtml",
              elementName,
            );
            badge.className = target === tab
              ? "floorp-session-badge"
              : "floorp-pane-session-badge";
            if (state.pending) badge.setAttribute("type", "button");
            else badge.setAttribute("role", "note");
            // Resolve again on activation: tabs can be moved or recovered meanwhile.
            if (state.pending) {
              badge.addEventListener("click", (event) => {
                event.preventDefault();
                event.stopPropagation();
                const current = Workspaces.getCtx(win);
                const id = current?.tabManagerCtx.getWorkspaceIdFromAttribute(
                  browserTab,
                );
                if (!current || !id) return;
                const info = tabSessionState(
                  tab,
                  current.getRawWorkspace(id)?.userContextId ?? 0,
                );
                if (info.pending) {
                  void current.recoverMissingSessionFromDialog(
                    info.actualId,
                    id,
                  );
                }
              });
            }
            const container = target === tab
              ? tab.querySelector(".tab-content") ?? tab
              : target.querySelector(".browserStack") ?? target;
            container.appendChild(badge);
            badges.set(target, badge);
          }
          const text = state.unavailable ? `⚠ ${state.name}` : state.name;
          if (badge.textContent !== text) badge.textContent = text;
          if (badge.getAttribute("aria-label") !== state.label) {
            badge.setAttribute("aria-label", state.label);
          }
          if (badge.getAttribute("title") !== state.label) {
            badge.setAttribute("title", state.label);
          }
          badge.toggleAttribute("data-pending", state.pending);
          badge.toggleAttribute("data-unavailable", state.unavailable);
          if (state.pending) badge.setAttribute("tabindex", "0");
        }
      }
      for (const [target, badge] of badges) {
        if (!alive.has(target)) {
          badge.remove();
          badges.delete(target);
        }
      }
    };
    const schedule = () => {
      if (scheduled || disposed) return;
      scheduled = true;
      win.queueMicrotask(render);
    };
    createEffect(() => {
      enabled();
      selectedWorkspaceID();
      workspacesDataStore.data;
      schedule();
    });
    const events = [
      "TabOpen",
      "TabClose",
      "TabSelect",
      "TabAttrModified",
      "SSTabRestored",
    ];
    for (const type of events) {
      globalThis.gBrowser.tabContainer.addEventListener(type, schedule);
    }
    const observer = new MutationObserver((records) => {
      if (
        records.some((record) =>
          record.type === "attributes" ||
          Array.from(record.addedNodes).some((node) =>
            node instanceof Element &&
            !node.matches(".floorp-session-badge,.floorp-pane-session-badge")
          )
        )
      ) schedule();
    });
    observer.observe(
      document.getElementById("tabbrowser-tabpanels") ??
        document.documentElement,
      {
        subtree: true,
        childList: true,
        attributes: true,
        attributeFilter: [
          "usercontextid",
          "class",
          "floorpPendingSessionId",
          "floorpPendingSessionState",
        ],
      },
    );
    observer.observe(globalThis.gBrowser.tabContainer as unknown as Node, {
      subtree: true,
      attributes: true,
      attributeFilter: [
        "usercontextid",
        "floorpWorkspaceId",
        "floorpPendingSessionId",
        "floorpPendingSessionState",
      ],
    });
    const identityObserver = { observe: schedule };
    const topics = [
      "contextual-identity-created",
      "contextual-identity-updated",
      "contextual-identity-deleted",
    ];
    for (const topic of topics) {
      Services.obs.addObserver(identityObserver, topic);
    }
    schedule();
    onCleanup(() => {
      disposed = true;
      observer.disconnect();
      for (const type of events) {
        globalThis.gBrowser.tabContainer.removeEventListener(type, schedule);
      }
      for (const topic of topics) {
        Services.obs.removeObserver(identityObserver, topic);
      }
      clear();
      style.remove();
    });
  }
}
