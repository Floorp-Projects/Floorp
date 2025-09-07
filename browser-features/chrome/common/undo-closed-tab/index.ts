// SPDX-License-Identifier: MPL-2.0


import { noraComponent, NoraComponentBase } from "#features-chrome/utils/base";
import { createRootHMR } from "@nora/solid-xul";
import { addI18nObserver, setLanguage } from "#i18n/config-browser-chrome.ts";
import { StyleElement } from "./styleElem";
import { BrowserActionUtils } from "#features-chrome/utils/browser-action";
import i18next from "i18next";
import "#features-chrome/@types/i18next.d.ts";

const { CustomizableUI } = ChromeUtils.importESModule(
  "resource:///modules/CustomizableUI.sys.mjs",
);

@noraComponent(import.meta.hot)
export default class UndoClosedTab extends NoraComponentBase {
  init() {
    BrowserActionUtils.createToolbarClickActionButton(
      "undo-closed-tab",
      null,
      () => {
        window.undoCloseTab();
      },
      StyleElement(),
      CustomizableUI.AREA_NAVBAR,
      2,
      (aNode: XULElement) => {
        const tooltip = document?.createXULElement("tooltip") as XULElement;
        tooltip.id = "undo-closed-tab-tooltip";
        tooltip.hasbeenopened = "false";

        document?.getElementById("mainPopupSet")?.appendChild(tooltip);

        aNode.tooltip = "undo-closed-tab-tooltip";

        window.setLanguage = setLanguage;

        createRootHMR(
          () => {
            addI18nObserver((locale) => {
              aNode.label = i18next.t("undo-closed-tab.label", {
                lng: locale,
              });
              tooltip.label = i18next.t("undo-closed-tab.tooltiptext", {
                lng: locale,
              });
            });
          },
          import.meta.hot,
        );
      },
    );
  }
}
