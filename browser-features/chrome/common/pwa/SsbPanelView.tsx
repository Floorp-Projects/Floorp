/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { signal } from "@preact/signals";
import type { Signal } from "@preact/signals";
import { safeRender } from "@nora/preact-xul";
import { useEffect, useState } from "preact/hooks";
import { addDisposer, createRootHMR } from "#features-chrome/utils/base";
import type { Browser, Manifest } from "./type";
import type { PwaService } from "./pwaService";
import {
  getContainerLabel,
  getUserContextIdForBrowser,
  isContainerExperimentEnabled,
} from "./containerUtils.ts";
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
  private static panelIsInstalled: Signal<boolean | null> = signal(null);
  private static installStateRequest = 0;
  private static pwaService: PwaService;
  private isOpen: Signal<boolean> = signal<boolean>(false);
  private isRendered = false;
  private disposePanel = () => {};

  constructor(pwaService: PwaService) {
    SsbPanelView.pwaService = pwaService;
    if (!this.panelUIButton) return;

    createRootHMR(() => {
      addDisposer(() => this.disposePanel());
      const refreshCurrentPage = (resetContainer: boolean) => {
        if (
          document.getElementById("PanelUI-ssb")?.getAttribute("visible") !==
            "true"
        ) {
          return;
        }
        const browser = globalThis.gBrowser.selectedBrowser as Browser;
        const userContextId = resetContainer
          ? getUserContextIdForBrowser(browser)
          : SsbPanelView.selectedContainerId.value;
        if (resetContainer) {
          SsbPanelView.selectedContainerId.value = userContextId;
        }
        void SsbPanelView.updatePanelInstallState(browser, userContextId);
      };
      const onTabSelect = () => refreshCurrentPage(true);
      const progressListener = {
        // addTabsProgressListener passes the browser before nsIWebProgressListener args.
        onLocationChange: (...args: unknown[]) => {
          const browser = args[0] as Browser;
          if (browser === globalThis.gBrowser.selectedBrowser) {
            refreshCurrentPage(false);
          }
        },
      };
      globalThis.gBrowser.tabContainer.addEventListener(
        "TabSelect",
        onTabSelect,
      );
      globalThis.gBrowser.addTabsProgressListener(progressListener);
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
        globalThis.gBrowser.tabContainer.removeEventListener(
          "TabSelect",
          onTabSelect,
        );
        globalThis.gBrowser.removeTabsProgressListener(progressListener);
      });
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

    this.disposePanel = safeRender(
      <SsbPanelView.Render />,
      this.parentElement,
      this.beforeElement,
    );
  }

  private static async showSsbPanelSubView() {
    const browser = globalThis.gBrowser.selectedBrowser as Browser;
    const pageUrl = browser.currentURI.spec;
    const tabContainerId = getUserContextIdForBrowser(browser);
    SsbPanelView.selectedContainerId.value = tabContainerId;
    void SsbPanelView.updatePanelInstallState(browser, tabContainerId);

    await globalThis.PanelUI.showSubView(
      "PanelUI-ssb",
      document?.getElementById("appMenu-ssb-button"),
    );

    if (
      globalThis.gBrowser.selectedBrowser !== browser ||
      browser.currentURI.spec !== pageUrl
    ) {
      const currentBrowser = globalThis.gBrowser.selectedBrowser as Browser;
      const currentContainerId = currentBrowser === browser
        ? SsbPanelView.selectedContainerId.value
        : getUserContextIdForBrowser(currentBrowser);
      if (currentBrowser !== browser) {
        SsbPanelView.selectedContainerId.value = currentContainerId;
      }
      void SsbPanelView.updatePanelInstallState(
        currentBrowser,
        currentContainerId,
      );
    }

    await SsbPanelView.updateInstalledApps();
  }

  private static async updatePanelInstallState(
    browser: Browser,
    userContextId: number,
  ) {
    const request = ++SsbPanelView.installStateRequest;
    const pageUrl = browser.currentURI.spec;
    SsbPanelView.panelIsInstalled.value = null;
    try {
      const installed = await SsbPanelView.pwaService
        .checkPageIsInstalledForContainer(browser, userContextId);
      if (
        request === SsbPanelView.installStateRequest &&
        globalThis.gBrowser.selectedBrowser === browser &&
        browser.currentURI.spec === pageUrl
      ) {
        SsbPanelView.panelIsInstalled.value = installed;
      }
    } catch (error) {
      console.error("[SsbPanelView] Could not check installed app:", error);
    }
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
      return addI18nObserver(() => {
        setTranslations({
          webapps: i18next.t("ssb.menu.webapps"),
          installCurrent: i18next.t("ssb.menu.install-current"),
          openCurrent: i18next.t("ssb.menu.open-current"),
          openInstalled: i18next.t("ssb.menu.open-installed"),
        });
      });
    }, []);

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
        <xul:panelview id="PanelUI-ssb">
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
                {...{
                  disabled: panelIsInstalled === null ? "true" : undefined,
                }}
                label={panelIsInstalled
                  ? translations.openCurrent
                  : translations.installCurrent}
                onCommand={() => {
                  if (panelIsInstalled !== null) {
                    SsbPanelView.handleInstallOrRunCurrentPageAsSsb();
                  }
                }}
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
