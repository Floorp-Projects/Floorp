/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { z } from "zod";

/** Design configs */
export const zFloorpDesignConfigs = z
  .object({
    globalConfigs: z
      .object({
        faviconColor: z.boolean(),
        userInterface: z.enum([
          "fluerial",
          "lepton",
          "photon",
          "protonfix",
          "proton",
        ]),
        appliedUserJs: z.string(),
      })
      .passthrough(),
    tabbar: z
      .object({
        tabbarStyle: z.enum(["horizontal", "vertical", "multirow"]),
        tabbarPosition: z.enum([
          "hide-horizontal-tabbar",
          "optimise-to-vertical-tabbar",
          "bottom-of-navigation-toolbar",
          "bottom-of-window",
          "default",
        ]),
        multiRowTabBar: z
          .object({
            maxRowEnabled: z.boolean(),
            maxRow: z.number(),
          })
          .passthrough(),
      })
      .passthrough(),
    tab: z
      .object({
        tabScroll: z
          .object({
            enabled: z.boolean(),
            reverse: z.boolean(),
            wrap: z.boolean(),
          })
          .passthrough(),
        tabMinHeight: z.number(),
        tabMinWidth: z.number(),
        tabPinTitle: z.boolean(),
        tabDubleClickToClose: z.boolean(),
        tabOpenPosition: z.number(),
      })
      .passthrough(),
    uiCustomization: z
      .object({
        navbar: z
          .object({
            position: z.enum(["top", "bottom"]).default("top"),
            searchBarTop: z.boolean().default(false),
          })
          .passthrough(),
        display: z
          .object({
            disableFullscreenNotification: z.boolean().default(false),
            deleteBrowserBorder: z.boolean().default(false),
            hideUnifiedExtensionsButton: z.boolean().default(false),
          })
          .passthrough(),
        special: z
          .object({
            optimizeForTreeStyleTab: z.boolean().default(false),
            hideForwardBackwardButton: z.boolean().default(false),
            stgLikeWorkspaces: z.boolean().default(false),
          })
          .passthrough(),
        multirowTab: z
          .object({
            newtabInsideEnabled: z.boolean().default(false),
          })
          .passthrough(),
        bookmarkBar: z
          .object({
            focusExpand: z.boolean().default(false),
          })
          .passthrough(),
        qrCode: z
          .object({
            disableButton: z.boolean().default(false),
          })
          .passthrough(),
      })
      .passthrough(),
  })
  .passthrough();

export type TFloorpDesignConfigs = z.infer<typeof zFloorpDesignConfigs>;
