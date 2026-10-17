// SPDX-License-Identifier: MPL-2.0

import type { TestSourceProof } from "./source-types.ts";

export type TestOrigin =
  | "floorp"
  | "mozilla-converted"
  | "mozilla-adapted"
  | "tooling"
  | "unknown";

export type TestExecutionKind =
  | "development-browser"
  | "development-mozilla-wrapper"
  | "development-mozilla-adaptation"
  | "development-harness-compat"
  | "host-tooling"
  | "unsupported";

export interface TestInventoryMetadata {
  inventoryVersion: 1;
  origin: TestOrigin;
  executionKind: TestExecutionKind;
  packagedStatus: "not-onboarded" | "unsupported";
}

export interface BrowserInventoryCatalogGroup {
  origin: TestOrigin;
  executionKind: TestExecutionKind;
  files: string[];
}

export interface BrowserInventoryCatalog {
  schemaVersion: 1;
  groups: BrowserInventoryCatalogGroup[];
}

export interface TestInventoryEntry extends TestInventoryMetadata {
  id: string;
  sourceSha256: string;
  browserDiscoverable: boolean;
  selected: boolean;
  requiredInSelection: boolean;
}

export interface BrowserInventorySelection {
  scope: "full-development-suite" | "explicit-development-selection";
  requiredCatalogIds: string[];
}

export interface ResolvedDevelopmentTargets {
  testFiles: string[];
  targetRels: string[];
  selection: BrowserInventorySelection;
}

export interface BrowserInventoryPlan {
  schemaVersion: 1;
  scope: "colocated-source-files";
  selection: BrowserInventorySelection;
  catalogSha256: string;
  entries: TestInventoryEntry[];
  issues: string[];
  unselectedMissingCatalogSources: string[];
  unselectedUnknownBrowserSources: string[];
  upstreamQuarantine: Array<{
    path: string;
    classification: string;
    reason: string;
    requiredApis: string[];
    sourceRef: string;
    lastObserved: string;
  }>;
  counts: {
    discovered: number;
    browserDiscoverable: number;
    catalogBrowserFiles: number;
    missingCatalogSources: number;
    selected: number;
    requiredCatalogFiles: number;
    packagedEligible: 0;
    notOnboarded: number;
    unsupported: number;
    unknown: number;
    quarantinedColocated: 0;
    quarantinedUpstreamCandidates: number;
    executed: 0;
  };
  productVerdict: "UNVERIFIED";
  publicationAuthorized: false;
}

export interface InventoryBrowserResult {
  file: string;
  sourceProof?: TestSourceProof;
  inventoryVersion?: number;
  origin?: TestOrigin;
  executionKind?: TestExecutionKind;
  source?: string;
}
