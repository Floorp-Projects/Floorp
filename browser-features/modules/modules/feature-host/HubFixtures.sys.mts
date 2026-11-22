// SPDX-License-Identifier: MPL-2.0

import type { HubFixtureVersion, M0Fixture } from "./FeatureHostTypes.ts";

const compatibility = Object.freeze({
  hostSchema: 2,
  hostBuildId: "m0-schema-2",
  runtimeBuildIds: Object.freeze(["20261112140843"]),
  capabilities: Object.freeze(["hub-pages"]),
});

const hubV1 =
  `(() => { let activations = 0; globalThis.__floorpM0 = { activate(context, request) { activations += 1; request(JSON.stringify({ type: "hub.register", descriptor: { localId: "sample", route: "/extensions/experimental-local-sample/sample", titleKey: "hotfixHub.sample.title", navigation: { labelKey: "hotfixHub.sample.navigation", order: 10 }, view: { kind: "message", titleKey: "hotfixHub.sample.title", bodyKey: "hotfixHub.sample.body", params: { version: "hub-v1" } } } })); }, attachWindow() {}, health() { return "hub-v1:" + activations; }, deactivate() {} }; })();`;
const hubV2 =
  `(() => { let activations = 0; globalThis.__floorpM0 = { activate(context, request) { activations += 1; request(JSON.stringify({ type: "hub.register", descriptor: { localId: "sample", route: "/extensions/experimental-local-sample/sample", titleKey: "hotfixHub.sample.title", navigation: { labelKey: "hotfixHub.sample.navigation", order: 10 }, view: { kind: "message", titleKey: "hotfixHub.sample.title", bodyKey: "hotfixHub.sample.body", params: { version: "hub-v2" } } } })); }, attachWindow() {}, health() { return "hub-v2:" + activations; }, deactivate() {} }; })();`;
const stagedFailure =
  `(() => { globalThis.__floorpM0 = { activate(context, request) { request(JSON.stringify({ type: "hub.register", descriptor: { localId: "staged", route: "/extensions/experimental-local-sample/staged", titleKey: "hotfixHub.staged.title", view: { kind: "message", titleKey: "hotfixHub.staged.title", bodyKey: "hotfixHub.staged.body" } } })); throw new Error("HUB staged fixture failure"); }, attachWindow() {}, health() { return "failed"; }, deactivate() {} }; })();`;

export const HUB_FIXTURES: Readonly<Record<HubFixtureVersion, M0Fixture>> =
  Object.freeze({
    "hub-v1": Object.freeze({
      version: "hub-v1",
      digest:
        "6adb04e1ff5c6eb851adb70d5592ca01802e546cd8739ed3d7792a598402880f",
      source: hubV1,
      compatibility,
    }),
    "hub-v2": Object.freeze({
      version: "hub-v2",
      digest:
        "96d4e9597fc61ec51470fca2c340457477b7e23c638f18b0eed8757bb1ce2d4b",
      source: hubV2,
      compatibility,
    }),
    "hub-staged-failure": Object.freeze({
      version: "hub-staged-failure",
      digest:
        "3ceb18a5433ec0e206a1ab91156f9705dcf40c3f3991d4d63f53035aaf657883",
      source: stagedFailure,
      compatibility,
    }),
  });
