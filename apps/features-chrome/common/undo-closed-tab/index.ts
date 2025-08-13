/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { noraComponent, NoraComponentBase } from "#features-chrome/utils/base";
import { createRootHMR } from "@nora/solid-xul";
import { addI18nObserver } from "#i18n/chrome-features/config.ts";
import { StyleElement } from "./styleElem";
import { BrowserActionUtils } from "#features-chrome/utils/browser-action";
import i18next from "i18next";

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

        createRootHMR(
          () => {
            addI18nObserver((locale) => {
              aNode.label = i18next.t("undo-closed-tab.label", {
                lng: locale,
                ns: "undo",
              });
              tooltip.label = i18next.t("undo-closed-tab.tooltiptext", {
                lng: locale,
                ns: "undo",
              });
            });
          },
          import.meta.hot,
        );
      },
    );
  }
}
