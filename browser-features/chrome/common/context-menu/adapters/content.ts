// SPDX-License-Identifier: MPL-2.0

import type { ContextMenuAdapter } from "../types.ts";

interface ContentContextState {
  onLink?: boolean;
  onImage?: boolean;
  onVideo?: boolean;
  onAudio?: boolean;
  onEditable?: boolean;
  onTextInput?: boolean;
  isContentSelected?: boolean;
  inFrame?: boolean;
  inPDFViewer?: boolean;
}

function getContentProfileKey(window: Window): string {
  const context = (window as Window & {
    gContextMenu?: ContentContextState | null;
  }).gContextMenu;
  if (!context) return "page";
  if (context.inPDFViewer) return "pdf";
  if (context.onEditable || context.onTextInput) return "editable";
  if (context.isContentSelected) return "selection";
  if (context.onLink && context.onImage) return "link-image";
  if (context.onLink) return "link";
  if (context.onImage) return "image";
  if (context.onVideo || context.onAudio) return "media";
  if (context.inFrame) return "frame";
  return "page";
}

export const contentContextMenuAdapter: ContextMenuAdapter = {
  key: "browser.content",
  label: "Web page",
  documentURIs: [
    "chrome://browser/content/browser.xhtml",
    "chrome://browser/content/webext-panels.xhtml",
  ],
  popupSelectors: ["#contentAreaContextMenu"],
  aliases: [
    { key: "content.navigation", selectors: ['[id="context-navigation"]'] },
    { key: "content.navigation.back", selectors: ['[id="context-back"]'] },
    {
      key: "content.navigation.forward",
      selectors: ['[id="context-forward"]'],
    },
    { key: "content.navigation.reload", selectors: ['[id="context-reload"]'] },
    { key: "content.navigation.stop", selectors: ['[id="context-stop"]'] },
    {
      key: "content.link.open-new-tab",
      selectors: ['[id="context-openlinkintab"]'],
    },
    {
      key: "content.link.open-split-view",
      selectors: ['[id="context-openlinkinsplitview"]'],
    },
    { key: "content.link.open-window", selectors: ['[id="context-openlink"]'] },
    {
      key: "content.link.open-private-window",
      selectors: ['[id="context-openlinkprivate"]'],
    },
    {
      key: "content.link.bookmark",
      selectors: ['[id="context-bookmarklink"]'],
    },
    { key: "content.link.save", selectors: ['[id="context-savelink"]'] },
    { key: "content.link.copy", selectors: ['[id="context-copylink"]'] },
    { key: "content.image.view", selectors: ['[id="context-viewimage"]'] },
    { key: "content.image.save", selectors: ['[id="context-saveimage"]'] },
    {
      key: "content.image.copy",
      selectors: ['[id="context-copyimage-contents"]'],
    },
    { key: "content.selection.copy", selectors: ['[id="context-copy"]'] },
    {
      key: "content.selection.select-all",
      selectors: ['[id="context-selectall"]'],
    },
    {
      key: "content.page.screenshot",
      selectors: ['[id="context-take-screenshot"]'],
    },
    { key: "content.page.inspect", selectors: ['[id="context-inspect"]'] },
  ],
  readonlySelectors: [
    "[ext-type]",
    "[data-usercontextid]",
  ],
  profiles: [
    { key: "page", label: "Page" },
    { key: "link", label: "Link" },
    { key: "image", label: "Image" },
    { key: "link-image", label: "Linked image" },
    { key: "media", label: "Audio or video" },
    { key: "selection", label: "Selection" },
    { key: "editable", label: "Editable text" },
    { key: "frame", label: "Frame" },
    { key: "pdf", label: "PDF" },
  ],
  getProfileKey: getContentProfileKey,
};
