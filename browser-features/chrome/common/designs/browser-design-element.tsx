/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createEffect, onCleanup } from "solid-js";
import { applyUserJS } from "./utils/userjs-parser.ts";
import styleBrowser from "./browser.css?inline";
import { config } from "./configs.ts";
import { getCSSFromConfig } from "./utils/css.ts";

const AGENT_SHEET = Ci.nsIStyleSheetService.AGENT_SHEET as number;
const sss = Cc["@mozilla.org/content/style-sheet-service;1"].getService(
  Ci.nsIStyleSheetService,
);

export function BrowserDesignElement() {
  const getCSS = () => getCSSFromConfig(config());

  // Apply UserJS preferences
  createEffect(() => {
    const { userjs } = getCSS();
    if (userjs) {
      applyUserJS(userjs);
    }
  });

  // Register CSS using StyleSheetService
  createEffect(() => {
    const { styles, stylesRaw, iconBasePath } = getCSS();
    const registeredURIs: nsIURI[] = [];

    // Development mode: Use raw CSS with icon path replacement
    if (stylesRaw?.length) {
      for (let i = 0; i < stylesRaw.length; i++) {
        let cssContent = stylesRaw[i];

        try {
          // Replace relative icon paths with absolute URLs
          if (iconBasePath) {
            cssContent = cssContent.replaceAll(/\.\.\/icons/g, iconBasePath);
          }

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
    } // Production mode: Use chrome:// URLs
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

  return (
    <>
      <style>{styleBrowser}</style>
    </>
  );
}
