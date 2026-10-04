// SPDX-License-Identifier: MPL-2.0

/** Capabilities reported by the verified, connected native App Shim backend. */
export interface AppLifecycleCapabilities {
  readonly protocolVersion: number;
  readonly sharedProfile: boolean;
  readonly nativeWindowOwnership: boolean;
  readonly independentAppQuit: boolean;
  readonly backgroundSession: boolean;
}

export type AppLifecycleWindow =
  | { readonly id: string; readonly kind: "browser" }
  | { readonly id: string; readonly kind: "web-app"; readonly appId: string };

/** A snapshot of a single profile, obtained immediately before planning quit. */
export interface AppLifecycleSnapshot {
  readonly windows: readonly AppLifecycleWindow[];
  readonly pendingLaunches: readonly {
    readonly id: string;
    readonly appId: string;
  }[];
}

export type RuntimeQuitReason = "quit-all" | "restart" | "os-shutdown";

export type AppQuitRequest =
  | { readonly kind: "browser" }
  | { readonly kind: "web-app"; readonly appId: string }
  | { readonly kind: "runtime"; readonly reason: RuntimeQuitReason };

export type AppQuitPlan =
  | { readonly kind: "unhandled" }
  | { readonly kind: "noop" }
  | { readonly kind: "quit-runtime"; readonly reason: RuntimeQuitReason }
  | {
    readonly kind: "close-windows";
    readonly windowIds: readonly string[];
    readonly cancelLaunchIds: readonly string[];
    /**
     * Keep the common runtime alive throughout the close transaction. A false
     * value permits shutdown only after successful close and a fresh idle check.
     */
    readonly retainRuntimeAfterClose: boolean;
  };

export interface AppLifecycleSettings {
  readonly supported: boolean;
  readonly keepRunningAfterBrowserQuit: boolean;
}

/**
 * Registered only after native window ownership and lifecycle integration have
 * been verified. Presence of a native component or a preference is insufficient.
 * Return null when the backend disconnects or cannot honor its capabilities.
 */
export interface AppLifecycleAdapter {
  getCapabilities(): AppLifecycleCapabilities | null;
}
