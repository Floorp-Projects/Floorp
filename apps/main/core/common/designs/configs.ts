/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createEffect, createSignal } from "solid-js";
import { z } from "zod";
import {
  getOldInterfaceConfig,
  getOldTabbarPositionConfig,
  getOldTabbarStyleConfig,
} from "./old-config-migrator";

export type zFloorpDesignConfigsType = z.infer<typeof zFloorpDesignConfigs>;

export const zFloorpDesignConfigs = z.object({
  globalConfigs: z.object({
    userInterface: z.enum([
      "fluerial",
      "lepton",
      "photon",
      "protonfix",
      "proton",
    ]),
    appliedUserJs: z.string(),
  }),
  tabbar: z.object({
    tabbarStyle: z.enum(["horizontal", "vertical", "multirow"]),
    tabbarPosition: z.enum([
      "hide-horizontal-tabbar",
      "optimise-to-vertical-tabbar",
      "bottom-of-navigation-toolbar",
      "bottom-of-window",
      "default",
    ]),
    multiRowTabBar: z.object({
      maxRowEnabled: z.boolean(),
      maxRow: z.number(),
    }),
    verticalTabBar: z.object({
      hoverEnabled: z.boolean(),
      paddingEnabled: z.boolean(),
      width: z.number(),
    }),
    tabScroll: z.object({
      reverse: z.boolean(),
      wrap: z.number(),
    }),
  }),
  fluerial: z.object({
    roundVerticalTabs: z.boolean(),
  }),
});

const oldObjectConfigs: zFloorpDesignConfigsType = {
  globalConfigs: {
    userInterface: getOldInterfaceConfig(),
    appliedUserJs: "",
  },
  tabbar: {
    tabbarStyle: getOldTabbarStyleConfig(),
    tabbarPosition: getOldTabbarPositionConfig(),
    multiRowTabBar: {
      maxRowEnabled: Services.prefs.getBoolPref(
        "floorp.browser.tabbar.multirow.max.enabled",
        false,
      ),
      maxRow: Services.prefs.getIntPref(
        "floorp.browser.tabbar.multirow.max.row",
        3,
      ),
    },
    verticalTabBar: {
      hoverEnabled: false,
      paddingEnabled: Services.prefs.getBoolPref(
        "floorp.verticaltab.paddingtop.enabled",
        false,
      ),
      width: Services.prefs.getIntPref(
        "floorp.browser.tabs.verticaltab.width",
        200,
      ),
    },
    tabScroll: {
      reverse: Services.prefs.getBoolPref("floorp.tabscroll.reverse", false),
      wrap: Services.prefs.getIntPref("floorp.tabscroll.wrap", 1),
    },
  },
  fluerial: {
    roundVerticalTabs: Services.prefs.getBoolPref(
      "floorp.fluerial.roundVerticalTabs",
      false,
    ),
  },
};

const getOldConfigs = JSON.stringify(oldObjectConfigs);

export const [config, setConfig] = createSignal(
  zFloorpDesignConfigs.parse(
    JSON.parse(
      Services.prefs.getStringPref("floorp.design.configs", getOldConfigs),
    ),
  ),
);

export function setGlobalDesignConfig<
  C extends zFloorpDesignConfigsType["globalConfigs"],
  K extends keyof C,
>(key: K, value: C[K]) {
  setConfig((prev) => ({
    ...prev,
    globalConfigs: {
      ...prev.globalConfigs,
      [key]: value,
    },
  }));
}

export function setBrowserInterface(
  value: zFloorpDesignConfigsType["globalConfigs"]["userInterface"],
) {
  setGlobalDesignConfig("userInterface", value);
}

createEffect(() => {
  Services.prefs.setStringPref(
    "floorp.design.configs",
    JSON.stringify(config()),
  );
});

Services.prefs.addObserver("floorp.design.configs", () =>
  setConfig(
    zFloorpDesignConfigs.parse(
      JSON.parse(Services.prefs.getStringPref("floorp.design.configs")),
    ),
  ),
);
