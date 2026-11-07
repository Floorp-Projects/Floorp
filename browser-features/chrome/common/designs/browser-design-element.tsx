/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  createEffect,
  createMemo,
  createSignal,
  For,
  onCleanup,
  Show,
} from "solid-js";
import { applyUserJS } from "./utils/userjs-parser.ts";
import styleBrowser from "./browser.css?inline";
import { config, getChromeExtrasSettings } from "./configs.ts";
import { getCSSFromConfig } from "./utils/css.ts";
import { syncLegacyChromeExtrasPrefs } from "./utils/old-config-migrator.ts";
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

/** Balance one native sheet registration per owner, including shared URIs. */
export function registerContentStyleSheets({
  styles,
  stylesRaw,
  iconBasePath,
  useTabColorAsToolbarColor,
}: ReturnType<typeof getCSSFromConfig>): void {
  const registeredURIs: nsIURI[] = [];
  // Gecko retains duplicate registrations and removes one entry per unregister.
  // Every owner must acquire its own entry so another window cannot remove it.
  const register = (uri: nsIURI) => {
    sss.loadAndRegisterSheet(uri, AGENT_SHEET);
    registeredURIs.push(uri);
  };
  onCleanup(() => {
    for (const uri of registeredURIs) {
      try {
        sss.unregisterSheet(uri, AGENT_SHEET);
      } catch (error) {
        console.error(
          "[BrowserDesignElement] Failed to unregister CSS:",
          error,
        );
      }
    }
  });

  if (useTabColorAsToolbarColor === true) {
    try {
      register(Services.io.newURI(
        `data:text/css;charset=utf-8,${
          encodeURIComponent(TAB_COLOR_LIKE_TOOLBAR_CSS)
        }`,
      ));
    } catch (error) {
      console.error(
        "[BrowserDesignElement] Failed to register tab color CSS:",
        error,
      );
    }
  }

  // Development content styles use raw CSS; production uses chrome:// URLs.
  if (stylesRaw?.length) {
    for (let i = 0; i < stylesRaw.length; i++) {
      try {
        const cssContent = replaceIconPaths(stylesRaw[i], iconBasePath);
        register(Services.io.newURI(
          `data:text/css;charset=utf-8,${encodeURIComponent(cssContent)}`,
        ));
      } catch (error) {
        console.error(
          `[BrowserDesignElement] Failed to register raw CSS ${i + 1}:`,
          error,
        );
      }
    }
  } else if (styles?.length) {
    for (const styleUrl of styles) {
      try {
        register(Services.io.newURI(styleUrl));
      } catch (error) {
        console.error(
          `[BrowserDesignElement] Failed to register CSS: ${styleUrl}`,
          error,
        );
      }
    }
  }
}

export function BrowserDesignElement() {
  const getCSS = () => getCSSFromConfig(config());
  const verticalPref = "sidebar.verticalTabs";
  const [verticalTabs, setVerticalTabs] = createSignal(
    Services.prefs.getBoolPref(verticalPref, false),
  );
  const updateVerticalTabs = () =>
    setVerticalTabs(Services.prefs.getBoolPref(verticalPref, false));
  Services.prefs.addObserver(verticalPref, updateVerticalTabs);
  onCleanup(() =>
    Services.prefs.removeObserver(verticalPref, updateVerticalTabs)
  );

  // Apply UserJS preferences
  createEffect(() => {
    const { userjs } = getCSS();
    if (userjs) {
      applyUserJS(userjs);
    }
  });

  // The Lepton-family designs still load pref-gated vendor CSS. Mirror the
  // migrated settings after applying user.js so turning a toggle off also
  // turns off the corresponding legacy rule.
  createEffect(() => {
    const design = config().globalConfigs.userInterface;
    const settings = getChromeExtrasSettings();
    if (design === "lepton" || design === "photon" || design === "protonfix") {
      syncLegacyChromeExtrasPrefs(settings, verticalTabs());
    }
  });

  // Register content CSS using StyleSheetService (AGENT_SHEET)
  // These styles apply to all documents including web content
  createEffect(() => registerContentStyleSheets(getCSS()));

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
      {
        /* Gecko 152 variable aliases — Floorp-wide, applied to every design.
          Keep this BEFORE theme-specific chrome styles so per-theme rules can
          still override. */
      }
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
