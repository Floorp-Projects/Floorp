// SPDX-License-Identifier: MPL-2.0

export type M0FixtureVersion = "v1" | "v2" | "broken";
export type HubFixtureVersion =
  | "hub-v1"
  | "hub-v2"
  | "hub-staged-failure";
export type FixtureVersion = M0FixtureVersion | HubFixtureVersion;

export interface PinnedFixture {
  version: FixtureVersion;
  digest: string;
  source: string;
  compatibility: {
    hostSchema: number;
    hostBuildId: string;
    runtimeBuildIds: readonly string[];
    capabilities: readonly string[];
  };
}

export type M0Fixture = PinnedFixture;
