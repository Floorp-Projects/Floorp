/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { signal, useSignal } from "@preact/signals";
import { useEffect } from "preact/hooks";
import type { ComponentChild } from "preact";
import { h } from "preact";
import { render } from "@nora/preact-xul";
import { addDisposer, createRootHMR } from "@nora/preact-xul/lifetime";
import i18next from "i18next";
import { addI18nObserver } from "#i18n/config-browser-chrome.ts";

export class HubPanelMenu {
  private isOpen = signal<boolean>(false);
  private isRendered = false;
  private disposeRender: (() => void) | undefined;

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

      addDisposer(() => {
        observer.disconnect();
        this.disposeRender?.();
      });
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
    this.disposeRender = render(
      h(HubPanelMenu.Render, null),
      this.parentElement!,
      { marker: this.beforeElement },
    );
  }

  private static handleOpenHub() {
    const win = window;
    win.gBrowser.selectedTab = win.gBrowser.addTab(
      "about:hub",
      {
        relatedToCurrent: true, // type def gap: @types/gecko missing this option
        triggeringPrincipal: Services.scriptSecurityManager
          .getSystemPrincipal(),
      } as Parameters<typeof win.gBrowser.addTab>[1],
    );
    (win.PanelUI as unknown as { hide: () => void })?.hide();
  }

  public static Render(): ComponentChild {
    const translations = useSignal({
      title: i18next.t("hub.menu.title", { defaultValue: "Floorp Hub" }),
    });

    useEffect(() => {
      return addI18nObserver(() => {
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
