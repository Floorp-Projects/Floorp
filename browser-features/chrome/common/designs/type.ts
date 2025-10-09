/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import * as t from "io-ts";

/** Design configs */
export const zFloorpDesignConfigs = t.intersection([
  t.type({
    globalConfigs: t.intersection([
      t.type({
        faviconColor: t.boolean,
        userInterface: t.union([
          t.literal("fluerial"),
          t.literal("lepton"),
          t.literal("photon"),
          t.literal("protonfix"),
          t.literal("proton"),
        ]),
        appliedUserJs: t.string,
      }),
      t.UnknownRecord,
    ]),
    tabbar: t.intersection([
      t.type({
        tabbarStyle: t.union([
          t.literal("horizontal"),
          t.literal("vertical"),
          t.literal("multirow"),
        ]),
        tabbarPosition: t.union([
          t.literal("hide-horizontal-tabbar"),
          t.literal("optimise-to-vertical-tabbar"),
          t.literal("bottom-of-navigation-toolbar"),
          t.literal("bottom-of-window"),
          t.literal("default"),
        ]),
        multiRowTabBar: t.intersection([
          t.type({
            maxRowEnabled: t.boolean,
            maxRow: t.number,
          }),
          t.UnknownRecord,
        ]),
      }),
      t.UnknownRecord,
    ]),
    tab: t.intersection([
      t.type({
        tabScroll: t.intersection([
          t.type({
            enabled: t.boolean,
            reverse: t.boolean,
            wrap: t.boolean,
          }),
          t.UnknownRecord,
        ]),
        tabMinHeight: t.number,
        tabMinWidth: t.number,
        tabPinTitle: t.boolean,
        tabDubleClickToClose: t.boolean,
        tabOpenPosition: t.number,
      }),
      t.UnknownRecord,
    ]),
    uiCustomization: t.intersection([
      t.type({
        navbar: t.intersection([
          t.type({
            position: t.union([t.literal("top"), t.literal("bottom")]),
            searchBarTop: t.boolean,
          }),
          t.UnknownRecord,
        ]),
        display: t.intersection([
          t.type({
            disableFullscreenNotification: t.boolean,
            deleteBrowserBorder: t.boolean,
            hideUnifiedExtensionsButton: t.boolean,
          }),
          t.UnknownRecord,
        ]),
        special: t.intersection([
          t.type({
            optimizeForTreeStyleTab: t.boolean,
            hideForwardBackwardButton: t.boolean,
            stgLikeWorkspaces: t.boolean,
          }),
          t.UnknownRecord,
        ]),
        multirowTab: t.intersection([
          t.type({
            newtabInsideEnabled: t.boolean,
          }),
          t.UnknownRecord,
        ]),
        bookmarkBar: t.intersection([
          t.type({
            focusExpand: t.boolean,
          }),
          t.UnknownRecord,
        ]),
        qrCode: t.intersection([
          t.type({
            disableButton: t.boolean,
          }),
          t.UnknownRecord,
        ]),
      }),
      t.UnknownRecord,
    ]),
  }),
  t.UnknownRecord,
]);

export type TFloorpDesignConfigs = t.TypeOf<typeof zFloorpDesignConfigs>;
