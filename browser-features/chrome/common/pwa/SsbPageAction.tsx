/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { signal } from "@preact/signals";
import type { Signal } from "@preact/signals";
import { render } from "preact";
import type { PwaService } from "./pwaService.ts";
import type { Browser } from "./type.ts";
import i18next from "i18next";
import { addI18nObserver } from "#i18n/config-browser-chrome.ts";
import { iconUrlParser } from "#features-chrome/utils/iconUrlParser.ts";
import style from "./style.css?inline";
import PWAINSTALL_SVG from "./icons/pwa-install.svg?inline";
import PWALAUNCH_SVG from "./icons/pwa-launch.svg?inline";
import INSTALLING_GIF from "./icons/installing.gif?inline";

type Translations = {
  title: string;
  install: string;
  open: string;
  cancel: string;
  siteIcon: string;
};

export class SsbPageAction {
  private isInstalling: Signal<boolean> = signal(false);
  private icon: Signal<string> = signal("");
  private title: Signal<string> = signal("");
  private description: Signal<string> = signal("");
  private canBeInstallAsPwa: Signal<boolean> = signal(false);
  private isInstalled: Signal<boolean> = signal(false);
  private shouldShowPageAction: Signal<boolean> = signal(false);
  private translations: Signal<Translations> = signal({
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

    // Render before the marker (starButtonBox) — insert a container before it
    const renderContainer = document?.createElement("span") as HTMLElement;
    ssbPageAction.insertBefore(renderContainer, starButtonBox);
    const RenderWrapper = () => this.Render();
    render(<RenderWrapper />, renderContainer);

    // Render style into head
    const styleContainer = document?.createElement("span") as HTMLElement;
    document?.head?.appendChild(styleContainer);
    const StyleWrapper = () => this.Style();
    render(<StyleWrapper />, styleContainer);

    Services.obs.addObserver(
      () => this.onCheckPageHasManifest(),
      "nora-pwa-check-page-has-manifest",
    );
    globalThis.gBrowser.tabContainer.addEventListener(
      "TabSelect",
      () => this.onCheckPageHasManifest(),
    );

    addI18nObserver(() => {
      this.translations.value = {
        title: i18next.t("ssb.page-action.title"),
        install: i18next.t("ssb.page-action.install"),
        open: i18next.t("ssb.page-action.open"),
        cancel: i18next.t("ssb.page-action.cancel"),
        siteIcon: i18next.t("ssb.page-action.site-icon"),
      };
    });

    this.onCheckPageHasManifest();
  }

  private async onCheckPageHasManifest() {
    const browser = globalThis.gBrowser.selectedBrowser as Browser;

    const canBeInstallAsPwa = await this.pwaService
      .checkBrowserCanBeInstallAsPwa(browser);
    this.canBeInstallAsPwa.value = canBeInstallAsPwa;

    const isInstalled = await this.pwaService.checkCurrentPageIsInstalled(
      browser,
    );
    this.isInstalled.value = isInstalled;
    this.shouldShowPageAction.value = canBeInstallAsPwa || isInstalled;
    this.pwaService.updateUIElements(isInstalled);
  }

  private onCommand = () => {
    this.pwaService.installOrRunCurrentPageAsSsb(
      globalThis.gBrowser.selectedBrowser as Browser,
      true,
    );
    this.isInstalling.value = true;
  };

  private onPopupShowing = async () => {
    const selectedBrowser = globalThis.gBrowser.selectedBrowser;
    const icon = await this.pwaService.getIcon(selectedBrowser as Browser);
    this.icon.value = icon;

    const manifest = await this.pwaService.getManifest(
      selectedBrowser as Browser,
    );
    this.title.value =
      manifest?.name ?? selectedBrowser.currentURI?.spec ?? "";
    this.description.value = selectedBrowser.currentURI?.host ?? "";
  };

  private onPopupHiding = () => {
    this.isInstalling.value = false;
    this.icon.value = "";
    this.title.value = "";
    this.description.value = "";
  };

  private closePopup = () => {
    const panel = document?.getElementById("ssb-panel") as unknown as XULElement & {
      hidePopup: () => void;
    };
    if (panel) {
      panel.hidePopup();
    }
    this.isInstalling.value = false;
  };

  private Render() {
    const isInstalling = this.isInstalling.value;
    const icon = this.icon.value;
    const title = this.title.value;
    const description = this.description.value;
    const isInstalled = this.isInstalled.value;
    const shouldShowPageAction = this.shouldShowPageAction.value;
    const translations = this.translations.value;

    return (
      <>
        {shouldShowPageAction && (
          <xul:hbox
            id="ssbPageAction"
            class="urlbar-page-action"
            popup="ssb-panel"
          >
            <xul:image
              id="ssbPageAction-image"
              class={`urlbar-icon${isInstalled ? " open-ssb" : ""}`}
            />
            <xul:panel
              id="ssb-panel"
              type="arrow"
              position="bottomright topright"
              onPopupShowing={this.onPopupShowing}
              onPopupHiding={this.onPopupHiding}
            >
              <xul:vbox id="ssb-box">
                <xul:vbox class="panel-header">
                  <h1>
                    {isInstalled
                      ? translations.open
                      : translations.install}
                  </h1>
                </xul:vbox>
                <xul:toolbarseparator />
                <xul:hbox id="ssb-content-hbox">
                  <xul:vbox id="ssb-content-icon-vbox">
                    <img
                      id="ssb-content-icon"
                      width="48"
                      height="48"
                      alt={translations.siteIcon}
                      src={icon}
                    />
                  </xul:vbox>
                  <xul:vbox id="ssb-content-label-vbox">
                    <xul:label id="ssb-content-label">
                      {title}
                    </xul:label>
                    <xul:description id="ssb-content-description">
                      {description}
                    </xul:description>
                  </xul:vbox>
                </xul:hbox>
                <xul:hbox id="ssb-button-hbox">
                  {isInstalling && (
                    <xul:vbox id="ssb-installing-vbox">
                      <img
                        id="ssb-installing-icon"
                        src={iconUrlParser(INSTALLING_GIF)}
                        width="48"
                        height="48"
                      />
                    </xul:vbox>
                  )}
                  {!isInstalling && (
                    <>
                      <xul:button
                        id="ssb-app-install-button"
                        class="panel-button ssb-install-buttons footer-button primary"
                        onClick={this.onCommand}
                        label={isInstalled
                          ? translations.open
                          : translations.install}
                      />
                      <xul:button
                        id="ssb-app-cancel-button"
                        class="panel-button ssb-install-buttons footer-button"
                        onClick={this.closePopup}
                        label={translations.cancel}
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

  private Style() {
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
