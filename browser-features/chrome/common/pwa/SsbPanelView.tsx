/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { signal } from "@preact/signals";
import type { Signal } from "@preact/signals";
import { render } from "preact";
import { useState, useEffect } from "preact/hooks";
import { createRootHMR } from "#features-chrome/utils/base";
import type { Browser, Manifest } from "./type";
import type { PwaService } from "./pwaService";
import { getContainerLabel, getUserContextIdForBrowser, isContainerExperimentEnabled } from "./containerUtils.ts";
import { SsbContainerSelect } from "./SsbContainerSelect.tsx";
import i18next from "i18next";
import { addI18nObserver } from "#i18n/config-browser-chrome.ts";

type PanelTranslations = {
  webapps: string;
  installCurrent: string;
  openCurrent: string;
  openInstalled: string;
};

export class SsbPanelView {
  private static installedApps: Signal<Manifest[]> = signal<Manifest[]>([]);
  private static selectedContainerId: Signal<number> = signal(0);
  private static panelIsInstalled: Signal<boolean> = signal(false);
  private static subviewSessionActive = false;
  private static pwaService: PwaService;
  private isOpen: Signal<boolean> = signal<boolean>(false);
  private isRendered = false;

  constructor(pwaService: PwaService) {
    SsbPanelView.pwaService = pwaService;
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

      import.meta.hot?.dispose(() => observer.disconnect());
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

    // Insert a container before the marker element
    const container = document?.createElement("span") as HTMLElement;
    this.parentElement.insertBefore(container, this.beforeElement);
    render(<SsbPanelView.Render />, container);
  }

  private static async showSsbPanelSubView() {
    const browser = globalThis.gBrowser.selectedBrowser as Browser;

    if (!SsbPanelView.subviewSessionActive) {
      SsbPanelView.subviewSessionActive = true;
      const tabContainerId = getUserContextIdForBrowser(browser);
      SsbPanelView.selectedContainerId.value = tabContainerId;
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
    SsbPanelView.panelIsInstalled.value = installed;
  }

  private static async updateInstalledApps() {
    const apps = await SsbPanelView.pwaService.getInstalledApps();
    SsbPanelView.installedApps.value = Object.values(apps).map(
      (value) => ({ ...(value as Manifest) }),
    );
  }

  private static onContainerSelect = (userContextId: number) => {
    SsbPanelView.selectedContainerId.value = userContextId;
    const browser = globalThis.gBrowser.selectedBrowser as Browser;
    void SsbPanelView.updatePanelInstallState(browser, userContextId);
  };

  private static handleInstallOrRunCurrentPageAsSsb() {
    const selectedContainerId = SsbPanelView.selectedContainerId.value;
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
    if (!isContainerExperimentEnabled()) {
      return app.name;
    }
    const containerLabel = getContainerLabel(app.userContextId ?? 0);
    if (!containerLabel) {
      return app.name;
    }
    return `${app.name} (${containerLabel})`;
  }

  private static InstalledAppsList() {
    const apps = SsbPanelView.installedApps.value;
    return (
      <>
        {apps.map((app) => (
          <xul:toolbarbutton
            key={app.id}
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
        ))}
      </>
    );
  }

  public static Render() {
    const [translations, setTranslations] = useState<PanelTranslations>({
      webapps: i18next.t("ssb.menu.webapps"),
      installCurrent: i18next.t("ssb.menu.install-current"),
      openCurrent: i18next.t("ssb.menu.open-current"),
      openInstalled: i18next.t("ssb.menu.open-installed"),
    });

    useEffect(() => {
      addI18nObserver(() => {
        setTranslations({
          webapps: i18next.t("ssb.menu.webapps"),
          installCurrent: i18next.t("ssb.menu.install-current"),
          openCurrent: i18next.t("ssb.menu.open-current"),
          openInstalled: i18next.t("ssb.menu.open-installed"),
        });
      });
    }, []);

    const selectedContainerId = SsbPanelView.selectedContainerId.value;
    const panelIsInstalled = SsbPanelView.panelIsInstalled.value;

    return (
      <>
        <xul:toolbarbutton
          id="appMenu-ssb-button"
          class="subviewbutton subviewbutton-nav"
          label={translations.webapps}
          closemenu="none"
          onCommand={() => SsbPanelView.showSsbPanelSubView()}
        />
        <xul:panelview
          id="PanelUI-ssb"
          onViewHiding={() => SsbPanelView.resetSubviewSession()}
        >
          <xul:vbox id="ssb-subview-body" class="panel-subview-body">
            <xul:vbox id="ssb-install-section" class="ssb-menu-install-section">
              {isContainerExperimentEnabled() && (
                <SsbContainerSelect
                  selectedId={() => SsbPanelView.selectedContainerId.value}
                  onSelect={SsbPanelView.onContainerSelect}
                  labelKey="ssb.menu.container"
                  menuPopupLevel="top"
                />
              )}
              <xul:toolbarbutton
                id="appMenu-install-or-open-ssb-current-page-button"
                class="subviewbutton"
                label={panelIsInstalled
                  ? translations.openCurrent
                  : translations.installCurrent}
                onCommand={() =>
                  SsbPanelView.handleInstallOrRunCurrentPageAsSsb()}
              />
            </xul:vbox>
            <xul:toolbarseparator />
            <h2
              id="panelMenu_openInstalledApps"
              class="subview-subheader"
              aria-label={translations.openInstalled}
            >
              {translations.openInstalled}
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
