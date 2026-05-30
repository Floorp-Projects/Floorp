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
import i18next from "i18next";
import { addI18nObserver } from "#i18n/config-browser-chrome.ts";

type PanelTranslations = {
  webapps: string;
  installCurrent: string;
  openInstalled: string;
};

export class SsbPanelView {
  private static installedApps: Signal<Manifest[]> = signal<Manifest[]>([]);
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
    await globalThis.PanelUI.showSubView(
      "PanelUI-ssb",
      document?.getElementById("appMenu-ssb-button"),
    );

    SsbPanelView.updateInstalledApps();
  }

  private static async updateInstalledApps() {
    const apps = await SsbPanelView.pwaService.getInstalledApps();
    SsbPanelView.installedApps.value = Object.values(apps).map(
      (value) => ({ ...(value as Manifest) }),
    );
  }

  private static handleInstallOrRunCurrentPageAsSsb() {
    SsbPanelView.pwaService.installOrRunCurrentPageAsSsb(
      globalThis.gBrowser.selectedBrowser as Browser,
      false,
    );
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
            label={app.name}
            image={app.icon}
            data-ssbId={app.id}
            onCommand={() => {
              SsbPanelView.pwaService.runSsbByUrl(app.start_url);
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
      openInstalled: i18next.t("ssb.menu.open-installed"),
    });

    useEffect(() => {
      addI18nObserver(() => {
        setTranslations({
          webapps: i18next.t("ssb.menu.webapps"),
          installCurrent: i18next.t("ssb.menu.install-current"),
          openInstalled: i18next.t("ssb.menu.open-installed"),
        });
      });
    }, []);

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
            <xul:toolbarbutton
              id="appMenu-install-or-open-ssb-current-page-button"
              class="subviewbutton"
              label={translations.installCurrent}
              onCommand={() =>
                SsbPanelView.handleInstallOrRunCurrentPageAsSsb()}
            />
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
