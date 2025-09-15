/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import * as t from "io-ts";
import { PathReporter } from "io-ts/PathReporter";
import { isLeft } from "fp-ts/Either";

/* io-ts codecs (kept original export names for compatibility) */
export const zPanel = t.intersection([
  t.type({
    id: t.string,
    type: t.union([t.literal("web"), t.literal("static"), t.literal("extension")]),
    width: t.number,
  }),
  t.partial({
    url: t.union([t.string, t.null]),
    icon: t.union([t.string, t.null]),
    userContextId: t.union([t.number, t.null]),
    zoomLevel: t.union([t.number, t.null]),
    userAgent: t.union([t.boolean, t.null]),
    extensionId: t.union([t.string, t.null]),
  }),
]);

export const zPanels = t.array(zPanel);

export const zWindowPanelSidebarState = t.type({
  panels: zPanels,
  currentPanelId: t.union([t.string, t.null]),
});

export const zPanelSidebarConfig = t.intersection([
  t.type({
    globalWidth: t.number,
    autoUnload: t.boolean,
    position_start: t.boolean,
    displayed: t.boolean,
    webExtensionRunningEnabled: t.boolean,
  }),
  t.partial({
    floatingWidth: t.number,
    floatingHeight: t.number,
    floatingPositionLeft: t.number,
    floatingPositionTop: t.number,
  }),
]);

export const zPanelSidebarData = t.type({
  data: zPanels,
});

/* small helper to decode or throw (keeps previous zod .parse semantics) */
export function parseOrThrow<T>(codec: t.Type<T>, value: unknown): T {
  const res = codec.decode(value);
  if (isLeft(res)) {
    throw new Error(PathReporter.report(res).join("; "));
  }
  return res.right as T;
}

/* Export as types */
export type Panel = t.TypeOf<typeof zPanel>;
export type Panels = t.TypeOf<typeof zPanels>;
export type WindowPanelSidebarState = t.TypeOf<typeof zWindowPanelSidebarState>;
export type PanelSidebarConfig = t.TypeOf<typeof zPanelSidebarConfig>;
export type PanelSidebarData = t.TypeOf<typeof zPanelSidebarData>;

export type Sidebar = {
  title: string;
  extensionId: string;
  url: string;
  menuId: string;
  keyId: string;
  menuL10nId: string;
  revampL10nId: string;
  iconUrl: string;
  disabled: boolean;
};

export type MapSidebars = [string, Sidebar][];
