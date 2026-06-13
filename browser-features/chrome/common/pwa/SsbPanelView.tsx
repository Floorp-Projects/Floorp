/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createSignal, For, onCleanup } from "solid-js";
import type { JSX } from "solid-js";
import { createRootHMR, render } from "@nora/solid-xul";
import type { Browser, Manifest } from "./type";
import type { PwaService } from "./pwaService";
import { getContainerLabel, getUserContextIdForBrowser } from "./containerUtils.ts";
import { SsbContainerSelect } from "./SsbContainerSelect.tsx";
import i18next from "i18next";
import { addI18nObserver } from "#i18n/config-browser-chrome.ts";

export class SsbPanelView {
  private static installedApps = createSignal<Manifest[]>([]);
  private static selectedContainerId = createSignal(0);
  private static panelIsInstalled = createSignal(false);
  private static subviewSessionActive = false;
  private static pwaService: PwaService;
  private isOpen = createSignal<boolean>(false);
  private isRendered = false;

  constructor(pwaService: PwaService) {
    SsbPanelView.pwaService = pwaService;
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
      "appMenu-bookmarks-button",
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
    render(() => <SsbPanelView.Render />, this.parentElement, {
      marker: this.beforeElement,
    });
  }

  private static async showSsbPanelSubView() {
    const browser = globalThis.gBrowser.selectedBrowser as Browser;

    if (!SsbPanelView.subviewSessionActive) {
      SsbPanelView.subviewSessionActive = true;
      const tabContainerId = getUserContextIdForBrowser(browser);
      SsbPanelView.selectedContainerId[1](tabContainerId);
      void SsbPanelView.updatePanelInstallState(browser, tabContainerId);
    }

    await globalThis.PanelUI.showSubView(
      "PanelUI-ssb",
      document?.getElementById("appMenu-ssb-button"),
    );

    await SsbPanelView.updateInstalledApps();
  }

  private static resetSubviewSession() {
    SsbPanelView.subviewSessionActive = false;
  }

  private static async updatePanelInstallState(
    browser: Browser,
    userContextId: number,
  ) {
    const installed = await SsbPanelView.pwaService
      .checkPageIsInstalledForContainer(browser, userContextId);
    SsbPanelView.panelIsInstalled[1](installed);
  }

  private static async updateInstalledApps() {
    const [, setInstalledApps] = SsbPanelView.installedApps;
    const apps = await SsbPanelView.pwaService.getInstalledApps();
    setInstalledApps(
      Object.values(apps).map((value) => ({ ...(value as Manifest) })),
    );
  }

  private static onContainerSelect = (userContextId: number) => {
    SsbPanelView.selectedContainerId[1](userContextId);
    const browser = globalThis.gBrowser.selectedBrowser as Browser;
    void SsbPanelView.updatePanelInstallState(browser, userContextId);
  };

  private static handleInstallOrRunCurrentPageAsSsb() {
    const selectedContainerId = SsbPanelView.selectedContainerId[0]();
    console.debug("[PWA:install-launch] SsbPanelView install/open", {
      selectedContainerId,
      pageUrl: globalThis.gBrowser.selectedBrowser?.currentURI?.spec,
    });
    SsbPanelView.pwaService.installOrRunCurrentPageAsSsb(
      globalThis.gBrowser.selectedBrowser as Browser,
      false,
      selectedContainerId,
    );
  }

  private static formatAppLabel(app: Manifest): string {
    const containerLabel = getContainerLabel(app.userContextId ?? 0);
    if (!containerLabel) {
      return app.name;
    }
    return `${app.name} (${containerLabel})`;
  }

  private static InstalledAppsList(): JSX.Element {
    const [apps] = SsbPanelView.installedApps;
    return (
      <For each={apps()}>
        {(app) => (
          <xul:toolbarbutton
            id={`ssb-${app.id}`}
            class="subviewbutton ssb-app-info-button"
            label={SsbPanelView.formatAppLabel(app)}
            image={app.icon}
            data-ssbId={app.id}
            onCommand={() => {
              SsbPanelView.pwaService.runSsbByUrl(
                app.start_url,
                app.userContextId,
              );
            }}
          />
        )}
      </For>
    );
  }

  public static Render(): JSX.Element {
    const [translations, setTranslations] = createSignal({
      webapps: i18next.t("ssb.menu.webapps"),
      installCurrent: i18next.t("ssb.menu.install-current"),
      openCurrent: i18next.t("ssb.menu.open-current"),
      openInstalled: i18next.t("ssb.menu.open-installed"),
    });

    addI18nObserver(() => {
      setTranslations({
        webapps: i18next.t("ssb.menu.webapps"),
        installCurrent: i18next.t("ssb.menu.install-current"),
        openCurrent: i18next.t("ssb.menu.open-current"),
        openInstalled: i18next.t("ssb.menu.open-installed"),
      });
    });

    const [selectedContainerId] = SsbPanelView.selectedContainerId;
    const [panelIsInstalled] = SsbPanelView.panelIsInstalled;

    return (
      <>
        <xul:toolbarbutton
          id="appMenu-ssb-button"
          class="subviewbutton subviewbutton-nav"
          label={translations().webapps}
          closemenu="none"
          onCommand={() => SsbPanelView.showSsbPanelSubView()}
        />
        <xul:panelview
          id="PanelUI-ssb"
          onViewHiding={() => SsbPanelView.resetSubviewSession()}
        >
          <xul:vbox id="ssb-subview-body" class="panel-subview-body">
            <xul:vbox id="ssb-install-section" class="ssb-menu-install-section">
              <SsbContainerSelect
                selectedId={selectedContainerId}
                onSelect={SsbPanelView.onContainerSelect}
                labelKey="ssb.menu.container"
                menuPopupLevel="top"
              />
              <xul:toolbarbutton
                id="appMenu-install-or-open-ssb-current-page-button"
                class="subviewbutton"
                label={panelIsInstalled()
                  ? translations().openCurrent
                  : translations().installCurrent}
                onCommand={() =>
                  SsbPanelView.handleInstallOrRunCurrentPageAsSsb()}
              />
            </xul:vbox>
            <xul:toolbarseparator />
            <h2
              id="panelMenu_openInstalledApps"
              class="subview-subheader"
              aria-label={translations().openInstalled}
            >
              {translations().openInstalled}
            </h2>
            <xul:toolbaritem
              id="panelMenu_installedSsbMenu"
              orient="vertical"
              smoothscroll={false}
              flatList
              tooltip="bhTooltip"
              context="ssbInstalledAppMenu-context"
              aria-labelledby="panelMenu_openInstalledApps"
            >
              <SsbPanelView.InstalledAppsList />
            </xul:toolbaritem>
          </xul:vbox>
          <xul:toolbarseparator hidden />
          <xul:toolbarbutton
            id="PanelUI-openManageSsbPage"
            class="subviewbutton panel-subview-footer-button"
            hidden
          />
        </xul:panelview>
      </>
    );
  }
}
