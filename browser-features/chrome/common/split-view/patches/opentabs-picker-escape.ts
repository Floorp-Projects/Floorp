/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { onCleanup } from "solid-js";
import {
  getGBrowser,
  type SplitViewTab,
  type SplitViewWrapper,
} from "../data/types.js";

const OPEN_TABS_URL = "about:opentabs";

function targetDocument(target: EventTarget | null): Document | null {
  const node = target as Node | null;
  if (!node) return null;
  return node.nodeType === Node.DOCUMENT_NODE
    ? node as Document
    : node.ownerDocument;
}

/**
 * Cancels the about:opentabs tab picker shown in a split view pane when
 * Escape is pressed inside it, by closing the picker pane.
 */
export function initOpenTabsPickerEscape(logger: ConsoleInstance): void {
  const onKeyDown = (event: KeyboardEvent): void => {
    if (
      event.key !== "Escape" || event.defaultPrevented || event.repeat ||
      event.shiftKey || event.ctrlKey || event.altKey || event.metaKey
    ) return;
    const doc = targetDocument(event.target);
    if (!doc || doc === document || doc.documentURI !== OPEN_TABS_URL) {
      return;
    }
    const gBrowser = getGBrowser();
    const pickerTab = gBrowser?.tabs.find((tab) =>
      (tab.linkedBrowser as { contentDocument?: Document | null } | null)
        ?.contentDocument === doc
    );
    const wrapper = pickerTab?.splitview as SplitViewWrapper | null | undefined;
    if (!gBrowser || !pickerTab || !wrapper) return;

    event.preventDefault();
    event.stopPropagation();

    const tabs = wrapper.tabs;
    const index = tabs.indexOf(pickerTab);
    const fallback: SplitViewTab | undefined = tabs[index - 1] ??
      tabs[index + 1];
    if (pickerTab.selected && fallback) {
      gBrowser.selectedTab = fallback;
    }
    gBrowser.removeTab(pickerTab);
    const remainingTabs = tabs.filter((tab) => tab !== pickerTab);
    if (remainingTabs.length >= 2) {
      gBrowser.showSplitViewPanels(remainingTabs);
    }
    logger.debug("[opentabs-escape] cancelled split view tab picker");
  };

  globalThis.addEventListener("keydown", onKeyDown);
  onCleanup(() => globalThis.removeEventListener("keydown", onKeyDown));
}
