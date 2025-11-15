/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { render } from "@nora/solid-xul";
import i18next from "i18next";
import { For, Show } from "solid-js";
import type { TWorkspaceID } from "./utils/type.ts";
import type { WorkspacesService } from "./workspacesService.ts";
import { workspacesDataStore } from "./data/data.ts";
import { addI18nObserver } from "#i18n/config-browser-chrome.ts";
import { ContextMenuUtils } from "#features-chrome/utils/context-menu.tsx";

const WORKSPACE_MENU_ID = "context_openInWorkspace";
const WORKSPACE_MENU_POPUP_ID = "context_openInWorkspace_popup";
const LINK_OPEN_MENU_ID = "context-openlink";

export class WorkspacesLinkContextMenu {
  private readonly ctx: WorkspacesService;
  private observer: MutationObserver | null = null;
  private readonly boundUpdateVisibility = () => this.updateVisibility();
  private readonly contentContextMenu: XULElement | null;

  constructor(ctx: WorkspacesService) {
    this.ctx = ctx;
    if (typeof document === "undefined") {
      return;
    }

    this.contentContextMenu = ContextMenuUtils.contentAreaContextMenu();
    if (!this.contentContextMenu) {
      console.error(
        "[WorkspacesLinkContextMenu] #contentAreaContextMenu not found; skip link menu injection.",
      );
      return;
    }

    const marker = document?.getElementById(LINK_OPEN_MENU_ID) ?? undefined;

    try {
      render(() => this.menu(), this.contentContextMenu, {
        marker,
      });
    } catch (error) {
      const reason = error instanceof Error ? error : new Error(String(error));
      console.error(
        "[WorkspacesLinkContextMenu] Failed to render workspace link menu.",
        reason,
      );
      return;
    }

    this.updateLabels();
    this.updateVisibility();

    const openLinkElement = document?.getElementById(LINK_OPEN_MENU_ID);
    if (openLinkElement) {
      this.observer = new MutationObserver(this.boundUpdateVisibility);
      this.observer.observe(openLinkElement, {
        attributes: true,
        attributeFilter: ["hidden", "disabled"],
      });
    }

    this.contentContextMenu.addEventListener(
      "popupshowing",
      this.boundUpdateVisibility,
    );

    window.addEventListener(
      "unload",
      () => {
        this.cleanup();
      },
      { once: true },
    );

    addI18nObserver(() => {
      this.updateLabels();
    });
  }

  private cleanup() {
    this.observer?.disconnect();
    this.observer = null;
    this.contentContextMenu?.removeEventListener(
      "popupshowing",
      this.boundUpdateVisibility,
    );
  }

  private menu() {
    return (
      <xul:menu
        id={WORKSPACE_MENU_ID}
        label={i18next.t("workspaces.menu.openLinkInWorkspace")}
      >
        <xul:menupopup
          id={WORKSPACE_MENU_POPUP_ID}
          onPopupShowing={() => {
            this.populateMenu();
          }}
        />
      </xul:menu>
    );
  }

  private updateLabels() {
    const menu = document?.getElementById(WORKSPACE_MENU_ID);
    if (menu) {
      menu.setAttribute(
        "label",
        i18next.t("workspaces.menu.openLinkInWorkspace"),
      );
    }
  }

  private updateVisibility() {
    const menu = document?.getElementById(
      WORKSPACE_MENU_ID,
    ) as XULElement | null;
    const openLink = document?.getElementById(
      LINK_OPEN_MENU_ID,
    ) as XULElement | null;

    if (!menu || !openLink) {
      if (menu) {
        menu.hidden = true;
      }
      return;
    }

    const currentWorkspaceId = this.ctx.getSelectedWorkspaceID();
    const availableTargets = workspacesDataStore.order.filter(
      (id) => id !== currentWorkspaceId,
    );
    const hasAlternativeWorkspace = availableTargets.length > 0;
    const hasLink = Boolean(window.gContextMenu?.linkURL);

    const shouldHide = openLink.hidden || !hasLink || !hasAlternativeWorkspace;

    menu.hidden = shouldHide;
    menu.disabled = openLink.disabled || !hasAlternativeWorkspace;
  }

  private populateMenu() {
    const popup = document?.getElementById(
      WORKSPACE_MENU_POPUP_ID,
    ) as XULElement | null;
    if (!popup) {
      return;
    }

    while (popup.firstChild) {
      popup.firstChild.remove();
    }

    const currentTab = window.gBrowser?.selectedTab as XULElement | null;
    const currentWorkspaceId = (currentTab &&
      this.ctx.tabManagerCtx.getWorkspaceIdFromAttribute(currentTab)) ||
      this.ctx.getSelectedWorkspaceID();

    const order = workspacesDataStore.order.filter(
      (id) => id !== currentWorkspaceId,
    );

    try {
      render(() => this.renderMenuItems(order), popup);
    } catch (error) {
      const reason = error instanceof Error ? error : new Error(String(error));
      console.error(
        "[WorkspacesLinkContextMenu] Failed to render workspace link items.",
        reason,
      );
    }

    this.updateVisibility();
  }

  private renderMenuItems(order: TWorkspaceID[]) {
    return (
      <Show
        when={order.length > 0}
        fallback={
          <xul:menuitem
            disabled="true"
            label={i18next.t("workspaces.menu.noOtherWorkspace")}
          />
        }
      >
        <For each={order}>
          {(id) => {
            const workspace = this.ctx.getRawWorkspace(id);
            if (!workspace) {
              return null;
            }
            const iconUrl = this.ctx.iconCtx.getWorkspaceIconUrl(
              workspace.icon,
            );
            return (
              <xul:menuitem
                class="menuitem-iconic"
                style={iconUrl ? `list-style-image: url(${iconUrl})` : ""}
                label={workspace.name}
                onCommand={() => this.openLinkInWorkspace(id)}
              />
            );
          }}
        </For>
      </Show>
    );
  }

  private openLinkInWorkspace(workspaceId: TWorkspaceID) {
    const url = window.gContextMenu?.linkURL;
    if (!url) {
      return;
    }

    const workspace = this.ctx.getRawWorkspace(workspaceId);
    const userContextId = workspace?.userContextId ?? 0;
    const triggeringPrincipal = Services.scriptSecurityManager
      .getSystemPrincipal();

    const newTab = window.gBrowser.addTrustedTab("about:blank", {
      relatedToCurrent: false,
      inBackground: true,
      userContextId,
      triggeringPrincipal,
    });

    this.ctx.tabManagerCtx.setWorkspaceIdToAttribute(newTab, workspaceId);
    if (userContextId > 0) {
      newTab.setAttribute("usercontextid", String(userContextId));
    }

    try {
      const uri = Services.io.newURI(url);
      window.gBrowser.getBrowserForTab(newTab).loadURI(uri, {
        triggeringPrincipal,
        loadFlags: Ci.nsIWebNavigation.LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP,
      });
    } catch (error) {
      console.error(
        "[WorkspacesLinkContextMenu] Failed to open link in workspace",
        error,
      );
      window.gBrowser.removeTab(newTab);
      return;
    }

    this.ctx.tabManagerCtx.updateTabsVisibility();
  }
}
