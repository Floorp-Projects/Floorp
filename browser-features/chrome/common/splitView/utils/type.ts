/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import * as t from "io-ts";

export const zSplitViewDatum = t.intersection([
  t.type({
    tabIds: t.array(t.string),
    reverse: t.boolean,
    method: t.union([t.literal("row"), t.literal("column")]),
  }),
  t.partial({
    fixedMode: t.boolean,
  }),
]);

export const zSplitViewData = t.array(zSplitViewDatum);

export const zFixedSplitViewDataGroup = t.type({
  fixedTabId: t.union([t.string, t.null]),
  options: t.type({
    reverse: t.boolean,
    method: t.union([t.literal("row"), t.literal("column")]),
  }),
});

export const zSplitViewConfigData = t.type({
  currentViewIndex: t.number,
  fixedSplitViewData: zFixedSplitViewDataGroup,
  splitViewData: zSplitViewData,
});

// Types for SplitView
export type TabEvent = {
  target: Tab;
  forUnsplit: boolean;
} & Event;

export type Browser = XULElement & {
  docShellIsActive: boolean;
  renderLayers: boolean;
  spliting: boolean;
};

export type Tab = XULElement & {
  linkedPanel: string;
  linkedBrowser: Browser;
  splitView: boolean;
};

export type TSplitViewDatum = t.TypeOf<typeof zSplitViewDatum>;
export type TFixedSplitViewDataGroup = t.TypeOf<
  typeof zFixedSplitViewDataGroup
>;
export type TSplitViewData = t.TypeOf<typeof zSplitViewData>;
