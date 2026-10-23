// SPDX-License-Identifier: MPL-2.0

export interface TestBootstrapLifecycle {
  initializeFeatures(): Promise<void>;
  waitForOwnerStartup(): Promise<void>;
  executeTests(): Promise<void>;
  recordStartupError(error: string): void;
  reportRejectedBootstrap(error: unknown): void;
}

export interface TestOwnerStartupWindow {
  readonly closed: boolean;
  readonly document: { readonly readyState: string };
  readonly gBrowserInit?: { readonly delayedStartupFinished?: boolean };
  addEventListener(type: string, listener: () => void): void;
  removeEventListener(type: string, listener: () => void): void;
}

export type RegisterTestStartupObserver = (
  observer: (subject: unknown) => void,
) => () => void;
