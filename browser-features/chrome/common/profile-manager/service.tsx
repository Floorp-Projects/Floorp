/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { BrowserActionUtils } from "#features-chrome/utils/browser-action.tsx";
import { MenuPopup } from "./components/popup.tsx";
import toolbarStyles from "./styles.css?inline";
import type { JSX } from "solid-js";
import i18next from "i18next";
import { createRootHMR } from "@nora/solid-xul";
import { createSignal } from "solid-js";
import { addI18nObserver } from "#i18n/config-browser-chrome.ts";

const { CustomizableUI } = ChromeUtils.importESModule(
  "moz-src:///browser/components/customizableui/CustomizableUI.sys.mjs",
);

type ProfileManagerTexts = {
  label: string;
  tooltipText: string;
};

const defaultTexts: ProfileManagerTexts = {
  label: "Profile",
  tooltipText: "Manage profiles",
};

const getTranslatedTexts = (): ProfileManagerTexts => {
  const t = (i18next as unknown as { t: (k: string) => string }).t;
  return {
    label: t("profile-manager-button.label"),
    tooltipText: t("profile-manager-button.tooltiptext"),
  } as ProfileManagerTexts;
};

export class ProfileManagerService {
  private static _instance: ProfileManagerService | null = null;

  static getInstance(): ProfileManagerService {
    if (!this._instance) this._instance = new ProfileManagerService();
    return this._instance;
  }

  private StyleElement = () => {
    return <style>{toolbarStyles}</style>;
  };

  private getButtonNode(): XULElement | null {
    return document?.getElementById("profile-manager-button") as
      | XULElement
      | null;
  }

  init(): void {
    try {
      BrowserActionUtils.createMenuToolbarButton(
        "profile-manager-button",
        "profile-manager-button",
        "profile-manager-popup",
        <MenuPopup />,
        null,
        () => {
          this.updateButtonIfNeeded();
        },
        CustomizableUI.AREA_NAVBAR,
        this.StyleElement() as JSX.Element,
        0,
      );
      // Ensure we update the button immediately as well in case the widget
      // already exists and onCreated wasn't invoked.
      this.updateButtonIfNeeded();
    } catch (e) {
      console.error("Failed to initialize ProfileManagerService:", e);
    }
  }

  private updateButtonIfNeeded(): void {
    const aNode = this.getButtonNode();
    if (!aNode) return;

    // Ensure a XUL tooltip element exists so we can update it like other features
    let tooltip = document?.getElementById("profile-manager-button-tooltip") as
      | XULElement
      | null;
    if (!tooltip) {
      tooltip = document?.createXULElement("tooltip") as XULElement;
      tooltip.id = "profile-manager-button-tooltip";
      tooltip.setAttribute("hasbeenopened", "false");
      document?.getElementById("mainPopupSet")?.appendChild(tooltip);
      aNode.setAttribute("tooltip", "profile-manager-button-tooltip");
    }

    createRootHMR(() => {
      const [texts, setTexts] = createSignal<ProfileManagerTexts>(
        // use translated texts if available, otherwise fallback to defaults
        getTranslatedTexts() ?? defaultTexts,
      );

      addI18nObserver(() => {
        setTexts(getTranslatedTexts());
        // update attributes when language changes
        aNode.setAttribute("label", texts().label);
        aNode.setAttribute("tooltiptext", texts().tooltipText);
        tooltip?.setAttribute("label", texts().tooltipText);
      });

      console.debug("Updating Profile Manager button texts");

      // initial set
      aNode.setAttribute("label", texts().label);
      aNode.setAttribute("tooltiptext", texts().tooltipText);
      tooltip?.setAttribute("label", texts().tooltipText);
    }, import.meta.hot);
  }
}

export default ProfileManagerService;
