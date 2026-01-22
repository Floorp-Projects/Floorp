/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createSignal, onCleanup } from "solid-js";
import type { JSX } from "solid-js";
import { createRootHMR, render } from "@nora/solid-xul";
import i18next from "i18next";
import { addI18nObserver } from "#i18n/config-browser-chrome.ts";

export class HubPanelMenu {
  private isOpen = createSignal<boolean>(false);
  private isRendered = false;

  constructor() {
    if (!this.panelUIButton) return;

    createRootHMR(() => {
      const [, setIsOpen] = this.isOpen;
      const observer = new MutationObserver((mutations) => {
        mutations.forEach((mutation) => {
          if (
            mutation.type === "attributes" &&
            mutation.attributeName === "open"
          ) {
            const isOpened =
              this.panelUIButton?.getAttribute("open") === "true";
            setIsOpen(isOpened);

            if (isOpened && !this.isRendered) {
              this.renderPanel();
            }
          }
        });
      });

      observer.observe(this.panelUIButton!, {
        attributes: true,
      });

      onCleanup(() => observer.disconnect());
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
    render(() => <HubPanelMenu.Render />, this.parentElement, {
      marker: this.beforeElement,
    });
  }

  private static handleOpenHub() {
    const win = window;
    win.gBrowser.selectedTab = win.gBrowser.addTab("about:hub", {
      relatedToCurrent: true,
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });
    win.PanelUI.hide();
  }

  public static Render(): JSX.Element {
    const [translations, setTranslations] = createSignal({
      title: i18next.t("hub.menu.title", { defaultValue: "Floorp Hub" }),
    });

    addI18nObserver(() => {
      setTranslations({
        title: i18next.t("hub.menu.title", { defaultValue: "Floorp Hub" }),
      });
    });

    return (
      <xul:toolbarbutton
        id="appMenu-floorp-hub-button"
        class="subviewbutton"
        label={translations().title}
        onCommand={() => HubPanelMenu.handleOpenHub()}
      />
    );
  }
}
