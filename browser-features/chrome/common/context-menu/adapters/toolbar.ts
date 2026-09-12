// SPDX-License-Identifier: MPL-2.0

import type { ContextMenuAdapter } from "../types.ts";

export const toolbarContextMenuAdapter: ContextMenuAdapter = {
  key: "browser.toolbar",
  label: "Toolbar",
  documentURIs: ["chrome://browser/content/browser.xhtml"],
  popupSelectors: ["#toolbar-context-menu"],
  aliases: [
    {
      key: "toolbar.pin-overflow",
      selectors: [
        '[id="toolbar-context-pinToOverflowMenu"]',
        '[id="toolbar-context-move-to-panel"]',
      ],
    },
    {
      key: "toolbar.unpin-overflow",
      selectors: [
        '[id="toolbar-context-unpinFromOverflowMenu"]',
        '[id="toolbar-context-pin-to-toolbar"]',
      ],
    },
    {
      key: "toolbar.remove",
      selectors: [
        '[id="toolbar-context-removeFromToolbar"]',
        '[id="toolbar-context-remove-from-toolbar"]',
      ],
    },
    {
      key: "toolbar.customize",
      selectors: ['[id="toolbar-context-customize"]'],
    },
    { key: "toolbar.bookmarks", selectors: ['[id="toggle_PersonalToolbar"]'] },
    { key: "toolbar.menu-bar", selectors: ['[id="toggle_toolbar-menubar"]'] },
  ],
  readonlySelectors: [
    "[toolbarId]",
    "[ext-type]",
    // Firefox asynchronously inserts extension origin controls before this
    // anchor. Keeping it in its native slot prevents the builder from reading
    // a user-reordered DOM and choosing the wrong insertion position.
    ".customize-context-manageExtension",
  ],
  profiles: [{ key: "default", label: "Toolbar" }],
  getProfileKey: () => "default",
};
