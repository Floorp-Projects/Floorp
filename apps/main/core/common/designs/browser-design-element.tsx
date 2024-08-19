/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import fluerialStyles from "@nora/skin/fluerial/css/fluerial.css?url";
import leptonChromeStyles from "@nora/skin/lepton/css/leptonChrome.css?url";
import leptonTabStyles from "@nora/skin/lepton/css/leptonContent.css?url";
import leptonUserJs from "@nora/skin/lepton/userjs/lepton.js?url";
import photonUserJs from "@nora/skin/lepton/userjs/photon.js?url";
import protonfixUserJs from "@nora/skin/lepton/userjs/protonfix.js?url";
import {
  For,
  createEffect
} from "solid-js";
import type { z } from "zod";
import styleBrowser from "./browser.css?inline";
import type { zFloorpDesignConfigs } from "./configs";
import { config } from "./configs";
import { applyUserJS } from "./userjs-parser";

interface FCSS {
  styles: string[];
  userjs: string | null;
}

function getCSSFromConfig(pref: z.infer<typeof zFloorpDesignConfigs>): FCSS {
  switch (pref.globalConfigs.userInterface) {
    case "fluerial": {
      return { styles: [fluerialStyles], userjs: null };
    }
    case "lepton": {
      return {
        styles: [leptonChromeStyles, leptonTabStyles],
        userjs: `chrome://noraneko${leptonUserJs}`,
      };
    }
    case "photon": {
      return {
        styles: [leptonChromeStyles, leptonTabStyles],
        userjs: `chrome://noraneko${photonUserJs}`,
      };
    }
    case "protonfix": {
      return {
        styles: [leptonChromeStyles, leptonTabStyles],
        userjs: `chrome://noraneko${protonfixUserJs}`,
      };
    }
  }
  return { styles: [null], userjs: null };
}

export function BrowserDesignElement() {
  [100, 500].forEach((time) => {
    setTimeout(() => {
      window.gURLBar._updateLayoutBreakoutDimensions();
    }, time);
  });

  const getCSS = () => getCSSFromConfig(config());

  createEffect(() => {
    const userjs = getCSS().userjs;
    if (userjs) applyUserJS(userjs);
  });

  return (
    <>
      <For each={getCSS().styles}>
        {(style) => (
          <link
            class="nora-designs"
            rel="stylesheet"
            href={`chrome://noraneko${style}`}
          />
        )}
      </For>
    </>
  );
}

export function BrowserStyle() {
  return <style>{styleBrowser}</style>;
}
