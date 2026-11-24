// SPDX-License-Identifier: MPL-2.0

import type { ContextMenuAdapter } from "../types.ts";

export const tabContextMenuAdapter: ContextMenuAdapter = {
  key: "browser.tabs",
  label: "Tabs",
  documentURIs: ["chrome://browser/content/browser.xhtml"],
  popupSelectors: ["#tabContextMenu"],
  aliases: [
    { key: "tab.new", selectors: ['[id="context_openANewTab"]'] },
    { key: "tab.reload", selectors: ['[id="context_reloadTab"]'] },
    {
      key: "tab.reload-selected",
      selectors: ['[id="context_reloadSelectedTabs"]'],
    },
    { key: "tab.mute", selectors: ['[id="context_toggleMuteTab"]'] },
    {
      key: "tab.mute-selected",
      selectors: ['[id="context_toggleMuteSelectedTabs"]'],
    },
    { key: "tab.pin", selectors: ['[id="context_pinTab"]'] },
    { key: "tab.unpin", selectors: ['[id="context_unpinTab"]'] },
    { key: "tab.duplicate", selectors: ['[id="context_duplicateTab"]'] },
    { key: "tab.bookmark", selectors: ['[id="context_bookmarkTab"]'] },
    {
      // Firefox's Share row is a <menu> with no id. The class is stable
      // inside #tabContextMenu, which is the same contract used to hide it
      // from userChrome.css.
      key: "tab.share",
      selectors: ["menu.share-tab-url-item"],
    },
    { key: "tab.move", selectors: ['[id="context_moveTabOptions"]'] },
    { key: "tab.select-all", selectors: ['[id="context_selectAllTabs"]'] },
    { key: "tab.close", selectors: ['[id="context_closeTab"]'] },
    {
      key: "tab.close-selected",
      selectors: ['[id="context_closeSelectedTabs"]'],
    },
  ],
  readonlySelectors: ["[tab-group-id]", "[profileid]", "[ext-type]"],
  profiles: [{ key: "default", label: "Tabs" }],
  getProfileKey: () => "default",
};
