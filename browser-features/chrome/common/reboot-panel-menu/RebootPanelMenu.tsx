/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createSignal, onCleanup } from "solid-js";
import type { JSX } from "solid-js";
import { createRootHMR, render } from "@nora/solid-xul";
import i18next from "i18next";
import { addI18nObserver } from "#i18n/config-browser-chrome.ts";

export class RebootPanelMenu {
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
    return document?.getElementById(
      "appMenu-quit-button2",
    ) as HTMLElement | null;
  }

  private get panelUIButton(): HTMLElement | null {
    return document?.getElementById(
      "PanelUI-menu-button",
    ) as HTMLElement | null;
  }

  private renderPanel(): void {
    if (!this.parentElement || !this.beforeElement) return;

    this.isRendered = true;
    render(() => <RebootPanelMenu.Render />, this.parentElement, {
      marker: this.beforeElement,
    });
  }

  private static async showRebootPanelSubView() {
    await window.PanelUI.showSubView(
      "PanelUI-reboot",
      document?.getElementById("appMenu-restart-button"),
    );
  }

  private static handleRestart() {
    Services.startup.quit(
      Services.startup.eForceQuit | Services.startup.eRestart,
    );
  }

  private static handleRestartWithCacheClear() {
    Services.appinfo.invalidateCachesOnRestart();
    Services.startup.quit(
      Ci.nsIAppStartup.eRestart | Ci.nsIAppStartup.eAttemptQuit,
    );
  }

  private static handleRestartInSafeMode() {
    Services.obs.notifyObservers(
      window as nsISupports,
      "restart-in-safe-mode",
    );
  }

  public static Render(): JSX.Element {
    const [translations, setTranslations] = createSignal({
      reboot: i18next.t("reboot.menu.title"),
      normalRestart: i18next.t("reboot.menu.normal-restart"),
      restartWithCacheClear: i18next.t("reboot.menu.restart-cache-clear"),
      restartInSafeMode: i18next.t("reboot.menu.restart-safe-mode"),
    });

    addI18nObserver(() => {
      setTranslations({
        reboot: i18next.t("reboot.menu.title"),
        normalRestart: i18next.t("reboot.menu.normal-restart"),
        restartWithCacheClear: i18next.t("reboot.menu.restart-cache-clear"),
        restartInSafeMode: i18next.t("reboot.menu.restart-safe-mode"),
      });
    });

    return (
      <>
        <xul:toolbarbutton
          id="appMenu-restart-button"
          class="subviewbutton subviewbutton-nav"
          label={translations().reboot}
          closemenu="none"
          onCommand={() => RebootPanelMenu.showRebootPanelSubView()}
        />
        <xul:panelview id="PanelUI-reboot">
          <xul:vbox id="reboot-subview-body" class="panel-subview-body">
            <xul:toolbarbutton
              id="appMenu-restart-normal-button"
              class="subviewbutton"
              label={translations().normalRestart}
              onCommand={() => RebootPanelMenu.handleRestart()}
            />
            <xul:toolbarseparator />
            <xul:toolbarbutton
              id="appMenu-restart-cache-clear-button"
              class="subviewbutton"
              label={translations().restartWithCacheClear}
              onCommand={() => RebootPanelMenu.handleRestartWithCacheClear()}
            />
            <xul:toolbarbutton
              id="appMenu-restart-safe-mode-button"
              class="subviewbutton"
              label={translations().restartInSafeMode}
              onCommand={() => RebootPanelMenu.handleRestartInSafeMode()}
            />
          </xul:vbox>
        </xul:panelview>
      </>
    );
  }
}
