/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createSignal } from "solid-js";
import type { JSX } from "solid-js";
import { render } from "@nora/solid-xul";
import type { PwaService } from "./pwaService.ts";
import type { Browser } from "./type.ts";
import { getUserContextIdForBrowser } from "./containerUtils.ts";
import { SsbContainerSelect } from "./SsbContainerSelect.tsx";
import i18next from "i18next";
import { addI18nObserver } from "#i18n/config-browser-chrome.ts";
import { iconUrlParser } from "#features-chrome/utils/iconUrlParser.ts";
import style from "./style.css?inline";
import PWAINSTALL_SVG from "./icons/pwa-install.svg?inline";
import PWALAUNCH_SVG from "./icons/pwa-launch.svg?inline";
import INSTALLING_GIF from "./icons/installing.gif?inline";

export class SsbPageAction {
  private isInstalling = createSignal(false);
  private icon = createSignal("");
  private title = createSignal("");
  private description = createSignal("");
  private canBeInstallAsPwa = createSignal(false);
  private isInstalled = createSignal(false);
  private panelIsInstalled = createSignal(false);
  private selectedContainerId = createSignal(0);
  private shouldShowPageAction = createSignal(false);
  private containerSelectionInitialized = false;
  private translations = createSignal({
    title: i18next.t("ssb.page-action.title"),
    install: i18next.t("ssb.page-action.install"),
    open: i18next.t("ssb.page-action.open"),
    cancel: i18next.t("ssb.page-action.cancel"),
    siteIcon: i18next.t("ssb.page-action.site-icon"),
  });

  constructor(private pwaService: PwaService) {
    const starButtonBox = document?.getElementById("star-button-box");
    const ssbPageAction = document?.getElementById("page-action-buttons");
    if (!starButtonBox || !ssbPageAction) return;

    render(() => this.Render(), ssbPageAction, {
      marker: starButtonBox,
    });

    render(() => this.Style(), document?.head);

    Services.obs.addObserver(
      () => this.onCheckPageHasManifest(),
      "nora-pwa-check-page-has-manifest",
    );
    globalThis.gBrowser.tabContainer.addEventListener(
      "TabSelect",
      () => this.onCheckPageHasManifest(),
    );

    addI18nObserver(() => {
      this.translations[1]({
        title: i18next.t("ssb.page-action.title"),
        install: i18next.t("ssb.page-action.install"),
        open: i18next.t("ssb.page-action.open"),
        cancel: i18next.t("ssb.page-action.cancel"),
        siteIcon: i18next.t("ssb.page-action.site-icon"),
      });
    });

    this.onCheckPageHasManifest();
  }

  private async onCheckPageHasManifest() {
    const browser = globalThis.gBrowser.selectedBrowser as Browser;

    const canBeInstallAsPwa = await this.pwaService
      .checkBrowserCanBeInstallAsPwa(browser);
    this.canBeInstallAsPwa[1](canBeInstallAsPwa);

    const isInstalled = await this.pwaService.checkCurrentPageIsInstalled(
      browser,
    );
    this.isInstalled[1](isInstalled);
    this.shouldShowPageAction[1](canBeInstallAsPwa || isInstalled);
    this.pwaService.updateUIElements(isInstalled);
  }

  private async updatePanelInstallState(browser: Browser, userContextId: number) {
    const installed = await this.pwaService.checkPageIsInstalledForContainer(
      browser,
      userContextId,
    );
    this.panelIsInstalled[1](installed);
  }

  private onContainerSelect = (userContextId: number) => {
    this.selectedContainerId[1](userContextId);
    const browser = globalThis.gBrowser.selectedBrowser as Browser;
    void this.updatePanelInstallState(browser, userContextId);
  };

  private onCommand = () => {
    const selectedContainerId = this.selectedContainerId[0]();
    console.debug("[PWA:install-launch] SsbPageAction onCommand", {
      selectedContainerId,
      pageUrl: globalThis.gBrowser.selectedBrowser?.currentURI?.spec,
    });
    this.pwaService.installOrRunCurrentPageAsSsb(
      globalThis.gBrowser.selectedBrowser as Browser,
      true,
      selectedContainerId,
    );
    this.isInstalling[1](true);
  };

  private onPopupShowing = async () => {
    const selectedBrowser = globalThis.gBrowser.selectedBrowser as Browser;

    if (!this.containerSelectionInitialized) {
      this.containerSelectionInitialized = true;
      const tabContainerId = getUserContextIdForBrowser(selectedBrowser);
      this.selectedContainerId[1](tabContainerId);
      void this.updatePanelInstallState(selectedBrowser, tabContainerId);
    }

    const icon = await this.pwaService.getIcon(selectedBrowser as Browser);
    this.icon[1](icon);

    const manifest = await this.pwaService.getManifest(
      selectedBrowser as Browser,
    );
    this.title[1](
      manifest?.name ?? selectedBrowser.currentURI?.spec ?? "",
    );
    this.description[1](selectedBrowser.currentURI?.host ?? "");
  };

  private onPopupHiding = (event: Event) => {
    const panel = event.currentTarget as XULElement;
    if (panel.id !== "ssb-panel") {
      return;
    }

    globalThis.requestAnimationFrame(() => {
      const ssbPanel = document?.getElementById("ssb-panel") as
        | (XULElement & { state?: string })
        | null;
      const panelState = ssbPanel?.state;
      if (panelState === "open" || panelState === "showing") {
        return;
      }

      this.resetPanelUiState();
    });
  };

  private resetPanelUiState() {
    this.containerSelectionInitialized = false;
    this.isInstalling[1](false);
    this.icon[1]("");
    this.title[1]("");
    this.description[1]("");
  };

  private closePopup = () => {
    const panel = document?.getElementById("ssb-panel") as unknown as XULElement & {
      hidePopup: () => void;
    };
    if (panel) {
      panel.hidePopup();
    }
    this.isInstalling[1](false);
  };

  private Render(): JSX.Element {
    const [isInstalling] = this.isInstalling;
    const [icon] = this.icon;
    const [title] = this.title;
    const [description] = this.description;
    const [isInstalled] = this.isInstalled;
    const [panelIsInstalled] = this.panelIsInstalled;
    const [selectedContainerId] = this.selectedContainerId;
    const [shouldShowPageAction] = this.shouldShowPageAction;
    const [translations] = this.translations;

    return (
      <>
        {shouldShowPageAction() && (
          <xul:hbox
            id="ssbPageAction"
            class="urlbar-page-action"
            popup="ssb-panel"
          >
            <xul:image
              id="ssbPageAction-image"
              class={`urlbar-icon${isInstalled() ? " open-ssb" : ""}`}
            />
            <xul:panel
              id="ssb-panel"
              type="arrow"
              position="bottomright topright"
              noautofocus={true}
              onPopupShowing={this.onPopupShowing}
              onPopupHiding={this.onPopupHiding}
            >
              <xul:vbox id="ssb-box">
                <xul:vbox class="panel-header">
                  <h1>
                    {panelIsInstalled()
                      ? translations().open
                      : translations().install}
                  </h1>
                </xul:vbox>
                <xul:toolbarseparator />
                <xul:hbox id="ssb-content-hbox">
                  <xul:vbox id="ssb-content-icon-vbox">
                    <img
                      id="ssb-content-icon"
                      width="48"
                      height="48"
                      alt={translations().siteIcon}
                      src={icon()}
                    />
                  </xul:vbox>
                  <xul:vbox id="ssb-content-label-vbox">
                    <xul:label id="ssb-content-label">
                      {title()}
                    </xul:label>
                    <xul:description id="ssb-content-description">
                      {description()}
                    </xul:description>
                  </xul:vbox>
                </xul:hbox>
                {!isInstalling() && (
                  <SsbContainerSelect
                    selectedId={selectedContainerId}
                    onSelect={this.onContainerSelect}
                  />
                )}
                <xul:hbox id="ssb-button-hbox">
                  {isInstalling() && (
                    <xul:vbox id="ssb-installing-vbox">
                      <img
                        id="ssb-installing-icon"
                        src={iconUrlParser(INSTALLING_GIF)}
                        width="48"
                        height="48"
                      />
                    </xul:vbox>
                  )}
                  {!isInstalling() && (
                    <>
                      <xul:button
                        id="ssb-app-install-button"
                        class="panel-button ssb-install-buttons footer-button primary"
                        onClick={this.onCommand}
                        label={panelIsInstalled()
                          ? translations().open
                          : translations().install}
                      />
                      <xul:button
                        id="ssb-app-cancel-button"
                        class="panel-button ssb-install-buttons footer-button"
                        onClick={this.closePopup}
                        label={translations().cancel}
                      />
                    </>
                  )}
                </xul:hbox>
              </xul:vbox>
            </xul:panel>
          </xul:hbox>
        )}
      </>
    );
  }

  private Style(): JSX.Element {
    return (
      <>
        <style>{style}</style>
        <style>
          {`
        #ssbPageAction-image {
          list-style-image: url("${iconUrlParser(PWAINSTALL_SVG)}");
        }

        #ssbPageAction-image[open-ssb="true"] {
          list-style-image: url("${iconUrlParser(PWALAUNCH_SVG)}");
        }
      `}
        </style>
      </>
    );
  }
}
