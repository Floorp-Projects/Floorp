/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  createEffect,
  createResource,
  createSignal,
  onCleanup,
  Show,
  Suspense,
  untrack,
} from "solid-js";
import i18next from "i18next";
import { addI18nObserver } from "#i18n/config-browser-chrome.ts";
import { savedUnreadBadgeForPanel } from "../utils/saved-unread-badge.ts";
import { getWebPanelChromeWindow } from "../utils/web-panel-browser.ts";
import {
  createUnreadBadgeState,
  getUnreadBadge,
  getUnreadFavicon,
  supportsUnreadBadge,
} from "../utils/unread-badge.ts";
import { getFaviconURLForPanel } from "../utils/favicon-getter";
import type { CPanelSidebar } from "./panel-sidebar";
import {
  isPanelSidebarVisible,
  panelSidebarData,
  selectedPanelId,
  setPanelSidebarData,
} from "../data/data";
import type { Panel } from "../utils/type";
import { isExtensionExist } from "../extension-panels";
import { getUserContextColor } from "../utils/userContextColor-getter";
import { setContextPanel } from "./sidebar-contextMenu";

// Keep dismissal with the loaded browser across button remounts (for example
// when resizing replaces the panel object). A recreated browser starts fresh.
const browserBadgeStates = new WeakMap<XULBrowserElement, {
  identity: string;
  state: ReturnType<typeof createUnreadBadgeState>;
}>();

export function PanelSidebarButton(props: {
  panel: Panel;
  ctx: CPanelSidebar;
}) {
  const gPanelSidebar = props.ctx;
  const [faviconURL] = createResource(
    () => props.panel,
    async () => await getFaviconURLForPanel(props.panel),
  );

  const [locale, setLocale] = createSignal(i18next.language);
  addI18nObserver(setLocale);
  const [unreadBadge, setUnreadBadge] = createSignal("");
  const [restored, setRestored] = createSignal(false);
  const [liveFavicon, setLiveFavicon] = createSignal<string>();
  const panelLabel = () =>
    props.panel.url || props.panel.extensionId || props.panel.id;

  createEffect(() => {
    const { id, type, url, userContextId } = props.panel;
    setUnreadBadge("");
    setRestored(false);
    setLiveFavicon(undefined);
    if (type !== "web" || !url || !supportsUnreadBadge(url)) return;

    let badgeState = createUnreadBadgeState();
    let pendingDismissal: string | undefined;
    const badgeIdentity = JSON.stringify([id, url, userContextId ?? 0]);
    const saved = savedUnreadBadgeForPanel(props.panel);
    let savedCount = saved?.count ?? "";
    let awaitingLiveCount = !!savedCount;
    let sawEligiblePage = false;
    const saveCount = (count: string) => {
      if (!saved || savedCount === count) return;
      saved.write(count);
      savedCount = count;
    };
    gPanelSidebar.clearUnreadBadges.set(id, () => {
      const dismissed = awaitingLiveCount ? savedCount : undefined;
      if (!observedBrowser && dismissed) pendingDismissal = dismissed;
      badgeState.clear(dismissed);
      awaitingLiveCount = false;
      saveCount("");
      setRestored(false);
      setUnreadBadge("");
    });

    let observedBrowser: XULBrowserElement | undefined;
    const onPageTitleChanged = (event: Event) => {
      if (event.isTrusted && event.target === observedBrowser) updateBadge();
    };

    const updateBadge = () => {
      // Read the URL and title from the same committed top-level document.
      // The inner browser belongs to this panel and its configured container.
      const browser = getWebPanelChromeWindow(id)?.floorpWebPanelContentBrowser;
      if (browser !== observedBrowser) {
        observedBrowser?.removeEventListener(
          "pagetitlechanged",
          onPageTitleChanged,
        );
        observedBrowser = browser;
        const existing = browser && browserBadgeStates.get(browser);
        badgeState = existing?.identity === badgeIdentity
          ? existing.state
          : createUnreadBadgeState();
        if (browser && existing?.identity !== badgeIdentity) {
          browserBadgeStates.set(browser, {
            identity: badgeIdentity,
            state: badgeState,
          });
        }
        browser?.addEventListener("pagetitlechanged", onPageTitleChanged);
      }
      const page = browser?.browsingContext?.currentWindowGlobal;
      const uri = page?.documentURI;
      const principal = page?.documentPrincipal;
      const eligible = page && uri && !page.isClosed &&
        principal?.isContentPrincipal &&
        principal.userContextId === (userContextId ?? 0) &&
        principal.isSameOrigin(uri);
      const signal = eligible
        ? getUnreadBadge(url, uri.spec, page.documentTitle)
        : "";
      if (eligible && signal !== undefined && pendingDismissal) {
        badgeState.clear(pendingDismissal);
        pendingDismissal = undefined;
      }
      const liveCount = badgeState.update(signal);
      const viewing = selectedPanelId() === id && isPanelSidebarVisible();
      if (eligible) {
        sawEligiblePage = true;
        if (signal !== undefined || viewing) {
          awaitingLiveCount = false;
          saveCount(signal === undefined ? "" : liveCount);
        }
      } else if (sawEligiblePage && uri && uri.spec !== "about:blank") {
        awaitingLiveCount = false;
        saveCount("");
      }
      const showSaved = !!eligible && signal === undefined && awaitingLiveCount;
      setRestored(showSaved);
      setUnreadBadge(showSaved ? savedCount : liveCount);
      setLiveFavicon(
        eligible
          ? getUnreadFavicon(url, uri.spec, browser?.mIconURL)
          : undefined,
      );
    };

    untrack(updateBadge);
    // Also handles lazy creation, hidden panels and unload/recreation without
    // subscribing to notifications or installing a content-side listener.
    const timer = window.setInterval(updateBadge, 1000);
    onCleanup(() => {
      window.clearInterval(timer);
      observedBrowser?.removeEventListener(
        "pagetitlechanged",
        onPageTitleChanged,
      );
      gPanelSidebar.clearUnreadBadges.delete(id);
    });
  });

  const handleDragStart = (e: DragEvent) => {
    e.dataTransfer?.setData("text/floorp-panel-id", props.panel.id);
    (e.target as HTMLElement).classList.add("dragging");
  };

  const handleDragEnd = (e: DragEvent) => {
    (e.target as HTMLElement).classList.remove("dragging");
  };

  const handleDragOver = (e: DragEvent) => {
    e.preventDefault();
    e.stopPropagation();
    (e.target as HTMLElement).classList.add("drag-over");
  };

  const handleDragLeave = (e: DragEvent) => {
    e.preventDefault();
    e.stopPropagation();
    (e.target as HTMLElement).classList.remove("drag-over");
  };

  const handleDrop = (e: DragEvent) => {
    e.preventDefault();
    e.stopPropagation();
    (e.target as HTMLElement).classList.remove("drag-over");

    const sourceId = e.dataTransfer?.getData("text/floorp-panel-id");
    const targetId = props.panel.id;

    if (sourceId === targetId) return;

    const panels = [...panelSidebarData()];
    const sourceIndex = panels.findIndex((p) => p.id === sourceId);
    const targetIndex = panels.findIndex((p) => p.id === targetId);

    if (sourceIndex === -1 || targetIndex === -1) return;

    const [movedPanel] = panels.splice(sourceIndex, 1);
    panels.splice(targetIndex, 0, movedPanel);

    setPanelSidebarData(panels);
  };

  const handleContextMenu = (e: MouseEvent) => {
    e.preventDefault();
    setContextPanel(props.panel);

    const contextMenu = document?.getElementById(
      "webpanel-context",
    ) as unknown as XULPopupElement;
    if (contextMenu) {
      contextMenu.openPopupAtScreen(e.screenX, e.screenY, true);
    }
  };

  if (
    props.panel.type === "extension" &&
    !isExtensionExist(props.panel.extensionId as string)
  ) {
    return null;
  }

  return (
    <div
      draggable
      onDragStart={handleDragStart}
      onDragEnd={handleDragEnd}
      onDragOver={handleDragOver}
      onDragLeave={handleDragLeave}
      onDrop={handleDrop}
      onContextMenu={handleContextMenu}
      class="panel-sidebar-button-wrapper"
    >
      <div
        id={props.panel.id}
        class={`${props.panel.type} panel-sidebar-panel`}
        data-checked={selectedPanelId() === props.panel.id}
        data-panel-id={props.panel.id}
        role="button"
        tabindex={0}
        aria-label={unreadBadge()
          ? i18next.t(
            restored()
              ? "panelSidebar.savedUnreadBadge"
              : "panelSidebar.unreadBadge",
            {
              panel: panelLabel(),
              count: unreadBadge(),
              lng: locale(),
            },
          )
          : panelLabel()}
        aria-expanded={selectedPanelId() === props.panel.id &&
          isPanelSidebarVisible()}
        onKeyDown={(event: KeyboardEvent) => {
          if (event.key === "Enter" || event.key === " ") {
            event.preventDefault();
            gPanelSidebar.changePanel(props.panel.id);
          }
        }}
        onClick={() => {
          gPanelSidebar.changePanel(props.panel.id);
        }}
        style={{
          "border-right": props.panel.userContextId !== 0 &&
              props.panel.userContextId !== null &&
              props.panel.type === "web"
            ? `2px solid ${getUserContextColor(props.panel.userContextId ?? 0)}`
            : "none",
        }}
      >
        <Suspense fallback={<div>I</div>}>
          <img
            src={liveFavicon() ?? faviconURL()}
            width={liveFavicon() ? 20 : 16}
            height={liveFavicon() ? 20 : 16}
          />
        </Suspense>
        <Show when={unreadBadge()}>
          <span
            class="panel-sidebar-unread-badge"
            data-restored={restored()}
            aria-hidden="true"
          >
            {unreadBadge()}
          </span>
        </Show>
      </div>
    </div>
  );
}
