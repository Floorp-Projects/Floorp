// SPDX-License-Identifier: MPL-2.0

import type { M0Fixture, M0FixtureVersion } from "./FeatureHostTypes.ts";

export type { FixtureVersion, M0FixtureVersion } from "./FeatureHostTypes.ts";
export type { M0Fixture } from "./FeatureHostTypes.ts";

const compatibility = Object.freeze({
  hostSchema: 2,
  hostBuildId: "m0-schema-2",
  runtimeBuildIds: Object.freeze(["20261112140843"]),
  capabilities: Object.freeze(["window-marker"]),
});

const v1 =
  `(() => { let activations = 0; globalThis.__floorpM0 = { activate() { activations += 1; }, attachWindow(context, request) { const value = JSON.parse(context); request(JSON.stringify({ type: "marker", windowId: value.windowId })); }, health() { return "v1:" + activations; }, deactivate() {} }; })();`;
const v2 =
  `(() => { let activations = 0; globalThis.__floorpM0 = { activate() { activations += 1; }, attachWindow(context, request) { const value = JSON.parse(context); request(JSON.stringify({ type: "marker", windowId: value.windowId })); }, health() { return "v2:" + activations; }, deactivate() {} }; })();`;
const broken = `throw new Error("M0 broken fixture");`;

export const M0_FIXTURES: Readonly<Record<M0FixtureVersion, M0Fixture>> = Object
  .freeze({
    v1: Object.freeze({
      version: "v1",
      digest:
        "614d7220b1730a52e7864a4d30263f8fb9b96e7b42143394cb21fd04b5d9d07f",
      source: v1,
      compatibility,
    }),
    v2: Object.freeze({
      version: "v2",
      digest:
        "ff357233eba473377f6cff5640bccaa38a4ded392a571c161d0b2231f661ee51",
      source: v2,
      compatibility,
    }),
    broken: Object.freeze({
      version: "broken",
      digest:
        "ffe0a4943c9adfd1796f0942bd98b4fe7f8aecc391f324e9a1261b7ce612cff8",
      source: broken,
      compatibility,
    }),
  });
