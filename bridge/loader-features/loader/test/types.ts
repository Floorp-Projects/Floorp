// SPDX-License-Identifier: MPL-2.0

import type { TestSourceContext } from "../../../../libs/test-inventory/source-types.ts";

export interface BrowserBootstrapIsolationLease {
  purpose: "restart-bootstrap-isolation";
  expiresAtMs: number;
}

export interface BrowserTestDiscoveryDependencies {
  sourceBinding?: TestSourceContext;
  chrome: Record<string, () => Promise<unknown>>;
  esm: Record<string, () => Promise<unknown>>;
  pages?: () => Promise<Record<string, () => Promise<unknown>>>;
  retainForCollection: () => void;
}
