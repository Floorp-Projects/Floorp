// SPDX-License-Identifier: MPL-2.0

import {
  createLaunchDevPorts,
  type LaunchDevPorts,
} from "../../../libs/shared/dev-ports/launch_offset.ts";

/**
 * Dev/test launch port resolver (FDH Gate 2 task 2-B).
 *
 * Ties the shared port table to the launch profile: when the dev/test launch
 * runs with `--port-offset`, the launcher writes the integer pref
 * `nora.dev.port_offset` into the profile, and everything browser-privileged
 * in that launch resolves dev/test URLs and trust decisions through this
 * module so they match the ports actually in use.
 */

declare const Services: {
  prefs?: {
    getIntPref(prefName: string, fallback: number): number;
  };
};

/**
 * The launch resolver. Created on every call (reads are one cheap pref lookup):
 * - privileged browser context → offset read from `Services.prefs`;
 * - contexts without the pref surface (content-principal dev pages must not
 *   use trust helpers) → the frozen base table, byte-identical to main.
 */
export function launchDevPorts(): LaunchDevPorts {
  const prefs = typeof Services === "undefined"
    ? null
    : (Services.prefs as unknown as Parameters<typeof createLaunchDevPorts>[0]);
  return createLaunchDevPorts(prefs);
}

/**
 * The launch's port offset (0 when the profile has no offset pref). Reading
 * tests/diagnostics can call this without mapping through the table.
 */
export function launchDevPortOffset(): number {
  return launchDevPorts().offset();
}
