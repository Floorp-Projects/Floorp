/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { render } from "@nora/solid-xul";
import { CSplitView } from "./splitView";
import i18next from "i18next";
import { createSignal } from "solid-js";
import { addI18nObserver } from "#i18n/config-browser-chrome.ts";

const translationKeys = {
  splitTabs: "splitview.tab-context.split-tabs",
  splitFixedTab: "splitview.tab-context.split-fixed-tab",
};

export class SplitViewContextMenu {
  private ctx: CSplitView;

  constructor(ctx: CSplitView) {
    this.ctx = ctx;
    const parentElem = document?.getElementById("tabContextMenu");
    if (!parentElem) return;

    const marker = document?.getElementById("context_closeDuplicateTabs");
    render(() => this.contextMenu(), parentElem, {
      marker: marker as XULElement,
    });
  }

  private contextMenu() {
    const gSplitView = this.ctx;

    const [texts, setTexts] = createSignal({
      splitTabs: i18next.t(translationKeys.splitTabs),
      splitFixedTab: i18next.t(translationKeys.splitFixedTab),
    });

    addI18nObserver(() => {
      setTexts({
        splitTabs: i18next.t(translationKeys.splitTabs),
        splitFixedTab: i18next.t(translationKeys.splitFixedTab),
      });
    });

    return (
      <>
        <xul:menuseparator />
        <xul:menuitem
          id="context_splittabs"
          label={texts().splitTabs}
          onCommand={() => gSplitView.contextSplitTabs()}
        />
        <xul:menuitem
          id="context_split_fixedtab"
          label={texts().splitFixedTab}
          onCommand={() => gSplitView.splitContextFixedTab()}
        />
        <xul:menuseparator />
      </>
    );
  }
}
