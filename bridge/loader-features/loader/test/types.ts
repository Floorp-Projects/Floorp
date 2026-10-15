// SPDX-License-Identifier: MPL-2.0

export interface BrowserBootstrapIsolationLease {
  purpose: "restart-bootstrap-isolation";
  expiresAtMs: number;
}

export interface BrowserTestDiscoveryDependencies {
  chrome: Record<string, () => Promise<unknown>>;
  esm: Record<string, () => Promise<unknown>>;
  pages?: () => Promise<Record<string, () => Promise<unknown>>>;
  retainForCollection: () => void;
}
