/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createEffect, createMemo, For, onCleanup, Show } from "solid-js";
import { applyUserJS } from "./utils/userjs-parser.ts";
import styleBrowser from "./browser.css?inline";
import { config } from "./configs.ts";
import { getCSSFromConfig } from "./utils/css.ts";
import { TAB_COLOR_LIKE_TOOLBAR_CSS } from "./utils/tab-color-like-toolbar.css.ts";
// Gecko 152 renamed many CSS variables; Floorp's own components (statusbar,
// panel-sidebar, workspaces, ...) still reference the pre-152 names. These
// aliases are injected for every design so the legacy names keep resolving.
import { GECKO_152_VAR_ALIASES_CSS } from "./utils/gecko-152-var-aliases.css.ts";

const AGENT_SHEET = Ci.nsIStyleSheetService.AGENT_SHEET as number;
const sss = Cc["@mozilla.org/content/style-sheet-service;1"].getService(
  Ci.nsIStyleSheetService,
);

/**
 * Replace relative icon paths with absolute URLs in CSS content
 */
export function replaceIconPaths(
  css: string,
  iconBasePath: string | undefined,
): string {
  if (!iconBasePath) return css;
  return css.replaceAll(/\.\.\/icons/g, iconBasePath);
}

export function BrowserDesignElement() {
  const getCSS = () => getCSSFromConfig(config());

  // Apply UserJS preferences
  createEffect(() => {
    const { userjs } = getCSS();
    if (userjs) {
      applyUserJS(userjs);
    }
  });

  let tabColorSheetURI: nsIURI | null = null;

  // Register content CSS using StyleSheetService (AGENT_SHEET)
  // These styles apply to all documents including web content
  createEffect(() => {
    const { styles, stylesRaw, iconBasePath, useTabColorAsToolbarColor } =
      getCSS();
    const registeredURIs: nsIURI[] = [];

    if (useTabColorAsToolbarColor === true) {
      if (!tabColorSheetURI) {
        try {
          const dataUri = `data:text/css;charset=utf-8,${
            encodeURIComponent(TAB_COLOR_LIKE_TOOLBAR_CSS)
          }`;
          const uri = Services.io.newURI(dataUri);
          sss.loadAndRegisterSheet(uri, AGENT_SHEET);
          tabColorSheetURI = uri;
        } catch (error) {
          console.error(
            `[BrowserDesignElement] Failed to register tab color CSS:`,
            error,
          );
        }
      }
    } else if (tabColorSheetURI) {
      if (sss.sheetRegistered(tabColorSheetURI, AGENT_SHEET)) {
        sss.unregisterSheet(tabColorSheetURI, AGENT_SHEET);
      }
      tabColorSheetURI = null;
    }

    // Development mode: Use raw CSS with icon path replacement (content styles only)
    if (stylesRaw?.length) {
      for (let i = 0; i < stylesRaw.length; i++) {
        let cssContent = stylesRaw[i];

        try {
          // Replace relative icon paths with absolute URLs
          cssContent = replaceIconPaths(cssContent, iconBasePath);

          // Create data URI and register
          const dataUri = `data:text/css;charset=utf-8,${
            encodeURIComponent(cssContent)
          }`;
          const uri = Services.io.newURI(dataUri);

          if (!sss.sheetRegistered(uri, AGENT_SHEET)) {
            sss.loadAndRegisterSheet(uri, AGENT_SHEET);
            registeredURIs.push(uri);
          }
        } catch (error) {
          console.error(
            `[BrowserDesignElement] Failed to register raw CSS ${i + 1}:`,
            error,
          );
        }
      }
    } // Production mode: Use chrome:// URLs (content styles only)
    else if (styles?.length) {
      for (const styleUrl of styles) {
        try {
          const uri = Services.io.newURI(styleUrl);

          if (!sss.sheetRegistered(uri, AGENT_SHEET)) {
            sss.loadAndRegisterSheet(uri, AGENT_SHEET);
            registeredURIs.push(uri);
          }
        } catch (error) {
          console.error(
            `[BrowserDesignElement] Failed to register CSS: ${styleUrl}`,
            error,
          );
        }
      }
    }

    // Cleanup: Unregister sheets when component unmounts or styles change
    onCleanup(() => {
      for (const uri of registeredURIs) {
        try {
          if (sss.sheetRegistered(uri, AGENT_SHEET)) {
            sss.unregisterSheet(uri, AGENT_SHEET);
          }
        } catch (error) {
          console.error(
            "[BrowserDesignElement] Failed to unregister CSS:",
            error,
          );
        }
      }
    });
  });

  const chromeStyleUrls = createMemo(() => getCSS().chromeStyles ?? []);

  // Inline Chrome-only CSS (dev bundles + production supplementary rules)
  const chromeInlineStyleContent = createMemo(() => {
    const { chromeStylesRaw, iconBasePath } = getCSS();
    if (!chromeStylesRaw?.length) {
      return "";
    }
    return chromeStylesRaw
      .map((css) => replaceIconPaths(css, iconBasePath))
      .join("\n");
  });

  return (
    <>
      <style>{styleBrowser}</style>
      {/* Gecko 152 variable aliases — Floorp-wide, applied to every design.
          Keep this BEFORE theme-specific chrome styles so per-theme rules can
          still override. */}
      <style>{GECKO_152_VAR_ALIASES_CSS}</style>
      <For each={chromeStyleUrls()}>
        {(url) => <link rel="stylesheet" href={url} />}
      </For>
      <Show when={chromeInlineStyleContent()}>
        <style>{chromeInlineStyleContent()}</style>
      </Show>
    </>
  );
}
