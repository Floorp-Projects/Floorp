// SPDX-License-Identifier: MPL-2.0

import type { PinnedFixture } from "./FeatureHostTypes.ts";

export type HubPanelMenuVersion =
  | "hub-menu-v1"
  | "hub-menu-v2"
  | "hub-menu-broken";

const compatibility = Object.freeze({
  hostSchema: 2,
  hostBuildId: "m0-schema-2",
  runtimeBuildIds: Object.freeze(["20261112140843"]),
  capabilities: Object.freeze(["app-menu"]),
});

const v1 =
  `(() => { globalThis.__floorpM0 = { activate(context, request) { request(JSON.stringify({ type: "app-menu.register", descriptor: { command: "open-hub" } })); }, attachWindow() {}, health() { return "hub-menu-v1"; }, deactivate() {} }; })();`;
const v2 =
  `(() => { globalThis.__floorpM0 = { activate(context, request) { request(JSON.stringify({ type: "app-menu.register", descriptor: { command: "open-hub" } })); }, attachWindow() {}, health() { return "hub-menu-v2"; }, deactivate() {} }; })();`;
const broken =
  `(() => { globalThis.__floorpM0 = { activate(context, request) { request(JSON.stringify({ type: "app-menu.register", descriptor: { command: "open-hub" } })); throw new Error("HUB menu candidate failure"); }, attachWindow() {}, health() { return "hub-menu-broken"; }, deactivate() {} }; })();`;

export const HUB_PANEL_MENU_FIXTURES: Readonly<
  Record<HubPanelMenuVersion, PinnedFixture<HubPanelMenuVersion>>
> = Object.freeze({
  "hub-menu-v1": Object.freeze({
    version: "hub-menu-v1",
    digest: "c5f95c9be1dba7668e271f1a531a41897b6a2e3abcc2fb871ad3eb4c55184a44",
    source: v1,
    compatibility,
  }),
  "hub-menu-v2": Object.freeze({
    version: "hub-menu-v2",
    digest: "0694214cbd2e6276dc9795274f25e6f13797b2c7f5c30ad42bc616fe1db603a3",
    source: v2,
    compatibility,
  }),
  "hub-menu-broken": Object.freeze({
    version: "hub-menu-broken",
    digest: "54b259b91992dc7cc8c92b67549057dac981c9dc2c634e6ec4d9a9e27f5c4ffa",
    source: broken,
    compatibility,
  }),
});
