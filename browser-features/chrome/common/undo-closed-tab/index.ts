/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { noraComponent, NoraComponentBase } from "../../utils/base.ts";
import { createRootHMR } from "@nora/solid-xul";
import { addI18nObserver } from "#i18n/config-browser-chrome.ts";
import { StyleElement } from "./styleElem.tsx";
import { BrowserActionUtils } from "../../utils/browser-action.tsx";
import i18next from "i18next";
import { createSignal } from "solid-js";

const { CustomizableUI } = ChromeUtils.importESModule(
  "moz-src:///browser/components/customizableui/CustomizableUI.sys.mjs",
);

declare global {
  interface Window {
    undoCloseTab: () => void;
  }
}

type UndoClosedTabTexts = {
  buttonLabel: string;
  tooltipText: string;
};

const defaultTexts: UndoClosedTabTexts = {
  buttonLabel: "Undo Closed Tab",
  tooltipText: "Reopen the last closed tab (Ctrl+Shift+T)",
};

@noraComponent(import.meta.hot)
export default class UndoClosedTab extends NoraComponentBase {
  init() {
    BrowserActionUtils.createToolbarClickActionButton(
      "undo-closed-tab",
      null,
      (event: XULCommandEvent) => {
        (
          event.view?.document?.getElementById(
            "toolbar-context-undoCloseTab",
          ) as XULElement
        )?.doCommand();
      },
      StyleElement(),
      CustomizableUI.AREA_NAVBAR,
      3,
      (aNode: XULElement) => {
        const tooltip = document?.createXULElement("tooltip") as XULElement;
        tooltip.id = "undo-closed-tab-tooltip";
        tooltip.setAttribute("hasbeenopened", "false");

        document?.getElementById("mainPopupSet")?.appendChild(tooltip);

        aNode.setAttribute("tooltip", "undo-closed-tab-tooltip");

        createRootHMR(
          () => {
            const [texts, setTexts] =
              createSignal<UndoClosedTabTexts>(defaultTexts);

            aNode.setAttribute("label", texts().buttonLabel);
            tooltip.setAttribute("label", texts().tooltipText);

            addI18nObserver(() => {
              setTexts({
                buttonLabel: i18next.t("undo-closed-tab.label"),
                tooltipText: i18next.t("undo-closed-tab.tooltiptext", {
                  shortcut: "(Ctrl+Shift+T)",
                }),
              });
              aNode.setAttribute("label", texts().buttonLabel);
              tooltip.setAttribute("label", texts().tooltipText);
            });
          },
          import.meta.hot,
        );
      },
    );
  }
}
