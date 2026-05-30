/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { signal, useSignal } from "@preact/signals";
import { useEffect } from "preact/hooks";
import type { ComponentChild } from "preact";
import { h, render } from "preact";
import { addDisposer, createRootHMR } from "@nora/preact-xul/lifetime";
import i18next from "i18next";
import { addI18nObserver } from "#i18n/config-browser-chrome.ts";

export class HubPanelMenu {
  private isOpen = signal<boolean>(false);
  private isRendered = false;

  constructor() {
    if (!this.panelUIButton) return;

    createRootHMR(() => {
      const observer = new MutationObserver((mutations) => {
        mutations.forEach((mutation) => {
          if (
            mutation.type === "attributes" &&
            mutation.attributeName === "open"
          ) {
            const isOpened =
              this.panelUIButton?.getAttribute("open") === "true";
            this.isOpen.value = isOpened;

            if (isOpened && !this.isRendered) {
              this.renderPanel();
            }
          }
        });
      });

      observer.observe(this.panelUIButton!, {
        attributes: true,
      });

      addDisposer(() => observer.disconnect());
    }, import.meta.hot);
  }

  private get parentElement(): HTMLElement | null {
    return document?.querySelector(
      "#appMenu-mainView > .panel-subview-body",
    ) as HTMLElement | null;
  }

  private get beforeElement(): HTMLElement | null {
    // Insert after Settings button.
    // The settings button is #appMenu-settings-button.
    // We want to insert after it, so we look for the next sibling or a known button after it.
    const settingsButton = document?.getElementById("appMenu-settings-button");
    return (settingsButton?.nextElementSibling as HTMLElement | null) || null;
  }

  private get panelUIButton(): HTMLElement | null {
    return document?.getElementById(
      "PanelUI-menu-button",
    ) as HTMLElement | null;
  }

  private renderPanel(): void {
    if (!this.parentElement) return;

    this.isRendered = true;
    render(h(HubPanelMenu.Render, null), this.parentElement!);
  }

  private static handleOpenHub() {
    const win = window;
    // deno-lint-ignore no-explicit-any
    win.gBrowser.selectedTab = win.gBrowser.addTab("about:hub", {
      relatedToCurrent: true, // type def gap: @types/gecko missing this option
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    } as any);
    // deno-lint-ignore no-explicit-any
    (win as any).PanelUI?.hide(); // type def gap: PanelUI.hide() missing from @types/gecko
  }

  public static Render(): ComponentChild {
    const translations = useSignal({
      title: i18next.t("hub.menu.title", { defaultValue: "Floorp Hub" }),
    });

    useEffect(() => {
      addI18nObserver(() => {
        translations.value = {
          title: i18next.t("hub.menu.title", { defaultValue: "Floorp Hub" }),
        };
      });
    }, []);

    return (
      <xul:toolbarbutton
        id="appMenu-floorp-hub-button"
        class="subviewbutton"
        label={translations.value.title}
        onCommand={() => HubPanelMenu.handleOpenHub()}
      />
    );
  }
}
