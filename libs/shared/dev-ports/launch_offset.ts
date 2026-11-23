// SPDX-License-Identifier: MPL-2.0

/**
 * Launch-port resolver for the dev port table (FDH Gate 2 task 2-B).
 *
 * A dev/test launch that runs with a non-zero port offset writes the integer
 * pref `DEV_PORT_OFFSET_PREF` (`nora.dev.port_offset`) into the launch
 * profile. Browser-privileged code (bridge startup, actors, chrome scripts,
 * privileged dev pages) then builds the URLs and trust decisions that match
 * the ports the launch actually uses through this resolver instead of the
 * frozen base table.
 *
 * The framework is injected: `prefs` is provided by the embedding context
 * (`Services.prefs` inside the browser, a double in tests), so the resolver
 * itself has no side effects and no global knowledge. Without the pref the
 * resolver collapses to the base table and every value is byte-identical to
 * the pre-offset behavior.
 */

import {
  DEV_PORT_OFFSET_PREF,
  devHttpOrigin,
  devPort,
  type DevPortName,
  devPortOffsetFromPref,
  devPortString,
  isTrustedDevPort,
} from "./defines.ts";

/** The minimal pref surface the resolver needs from the host context. */
export interface DevPortPrefReader {
  getIntPref(prefName: string, fallback: number): number;
}

/**
 * Read the launch port offset from a prefs surface. Returns `0` when the
 * surface is unavailable (production builds, non-privileged contexts, tests),
 * the pref is missing, or the pref holds a value that is not an admissible
 * offset — i.e. the frozen base topology never changes semantics.
 */
export function readDevPortOffset(
  prefs: DevPortPrefReader | null | undefined,
): number {
  try {
    if (!prefs || typeof prefs.getIntPref !== "function") {
      return 0;
    }
    return devPortOffsetFromPref(prefs.getIntPref(DEV_PORT_OFFSET_PREF, 0));
  } catch {
    return 0;
  }
}

/** Offset-adjusted accessors for one launched dev/test environment. */
export interface LaunchDevPorts {
  /** The launch offset this resolver resolves table names against. */
  offset(): number;
  /** Actual port of a named dev/test server in this launch. */
  port(name: DevPortName): number;
  /** Actual port of a named dev/test server as a string (URL compares). */
  portString(name: DevPortName): string;
  /** Actual http origin of a named dev/test server in this launch. */
  httpOrigin(name: DevPortName, pathname?: string): string;
  /**
   * Trust decision for a page port against an explicit table-name allowlist,
   * evaluated against this launch's actual ports.
   */
  isTrustedPort(
    port: string | number | null | undefined,
    names: readonly DevPortName[],
  ): boolean;
}

/**
 * Build the launch-port resolver for one dev/test environment. Pass the host
 * prefs surface (`Services.prefs`); pass `null`/`undefined` to always resolve
 * the frozen base table (tests without prefs, non-privileged contexts).
 */
export function createLaunchDevPorts(
  prefs: DevPortPrefReader | null | undefined,
): LaunchDevPorts {
  const offset = readDevPortOffset(prefs);
  return {
    offset: () => offset,
    port: (name) => devPort(name, offset),
    portString: (name) => devPortString(name, offset),
    httpOrigin: (name, pathname = "") => devHttpOrigin(name, pathname, offset),
    isTrustedPort: (port, names) => isTrustedDevPort(port, names, offset),
  };
}

/** Exposed for host-side diagnostics; the resolver only reads this pref. */
export const LAUNCH_DEV_PORT_OFFSET_PREF = DEV_PORT_OFFSET_PREF;
