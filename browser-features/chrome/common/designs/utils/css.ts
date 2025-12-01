/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { zFloorpDesignConfigs } from "../../designs/type.ts";
import type * as t from "io-ts";

// UserJS imports
import leptonUserJs from "@nora/skin/lepton/userjs/lepton.js?raw";
import photonUserJs from "@nora/skin/lepton/userjs/photon.js?raw";
import protonfixUserJs from "@nora/skin/lepton/userjs/protonfix.js?raw";

// CSS raw imports for development
import leptonChromeStylesRaw from "@nora/skin/lepton/css/leptonChrome.css?raw";
import leptonContentStylesRaw from "@nora/skin/lepton/css/leptonContent.css?raw";
import fluerialStylesRaw from "@nora/skin/fluerial/css/fluerial.css?raw";

interface FCSS {
  styles?: string[]; // chrome:// URLs for production (AGENT_SHEET - applies to all documents)
  stylesRaw?: string[]; // Raw CSS content for development (AGENT_SHEET - applies to all documents)
  chromeStyles?: string[]; // chrome:// URLs for production (DOM style - Chrome UI only)
  chromeStylesRaw?: string[]; // Raw CSS content for development (DOM style - Chrome UI only)
  iconBasePath?: string; // Base path for icons in development
  userjs: string | null;
  useTabColorAsToolbarColor?: boolean;
}

/**
 * Get the chrome:// URL for a skin CSS file (production only)
 */
const getStylePath = (path: string): string => {
  return `chrome://noraneko-skin/content/${path}`;
};

/**
 * Get CSS configuration based on the selected UI theme
 */
export function getCSSFromConfig(
  pref: t.TypeOf<typeof zFloorpDesignConfigs>,
): FCSS {
  const isDev = import.meta.env.DEV;
  const uiTheme = pref.globalConfigs.userInterface;

  switch (uiTheme) {
    case "fluerial": {
      if (isDev) {
        return {
          chromeStylesRaw: [fluerialStylesRaw],
          iconBasePath: "http://localhost:5174/fluerial/icons",
          userjs: null,
          useTabColorAsToolbarColor: true,
        };
      }
      return {
        chromeStyles: [getStylePath("fluerial/css/fluerial.css")],
        userjs: null,
        useTabColorAsToolbarColor: true,
      };
    }

    case "lepton": {
      if (isDev) {
        return {
          chromeStylesRaw: [leptonChromeStylesRaw],
          stylesRaw: [leptonContentStylesRaw],
          iconBasePath: "http://localhost:5174/lepton/icons",
          userjs: leptonUserJs,
        };
      }
      return {
        chromeStyles: [getStylePath("lepton/css/leptonChrome.css")],
        styles: [getStylePath("lepton/css/leptonContent.css")],
        userjs: leptonUserJs,
      };
    }

    case "photon": {
      if (isDev) {
        return {
          chromeStylesRaw: [leptonChromeStylesRaw],
          stylesRaw: [leptonContentStylesRaw],
          iconBasePath: "http://localhost:5174/lepton/icons",
          userjs: photonUserJs,
        };
      }
      return {
        chromeStyles: [getStylePath("lepton/css/leptonChrome.css")],
        styles: [getStylePath("lepton/css/leptonContent.css")],
        userjs: photonUserJs,
      };
    }

    case "protonfix": {
      if (isDev) {
        return {
          chromeStylesRaw: [leptonChromeStylesRaw],
          stylesRaw: [leptonContentStylesRaw],
          iconBasePath: "http://localhost:5174/lepton/icons",
          userjs: protonfixUserJs,
          useTabColorAsToolbarColor: true,
        };
      }
      return {
        chromeStyles: [getStylePath("lepton/css/leptonChrome.css")],
        styles: [getStylePath("lepton/css/leptonContent.css")],
        userjs: protonfixUserJs,
        useTabColorAsToolbarColor: true,
      };
    }

    case "proton": {
      return { userjs: null };
    }

    default: {
      console.warn(`[getCSSFromConfig] Unknown UI theme: ${uiTheme}`);
      uiTheme satisfies never;
      return { userjs: null };
    }
  }
}
