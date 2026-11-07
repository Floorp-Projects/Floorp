// SPDX-License-Identifier: MPL-2.0

/**
 * Authoritative definition table for the dev/test server ports.
 *
 * Every dev/test Vite server, tooling port and dev-origin trust decision in
 * tools/, bridge/ and browser-features/ must reference this table instead of a
 * hardcoded `5173-5192` literal. Production builds trust no http origin: their
 * page origins are `chrome://...` documents (see
 * `PRODUCTION_TRUSTED_HTTP_ORIGINS` and `isProductionTrustedHttpOrigin`).
 * A few dev-only call sites still construct localhost URLs unconditionally,
 * but those URLs are not trusted in production.
 *
 * The numeric values are frozen: changing them changes runtime port behavior.
 */

/**
 * Named dev/test server ports. The names mirror the dev-server names used by
 * `tools/src/dev_server.ts` (`getPortFor`) and the feature directories.
 */
export const DEV_PORTS = {
  /** Dev-only `update.xml` fixture server used by `NoranekoUpdateChecker`. */
  updateDevServer: 5173,
  /** Designs/dev server (fluerial/lepton icon base paths). */
  designs: 5174,
  /** Profile manager page dev server. */
  profileManager: 5179,
  /**
   * Marionette automation port used for dev/test browser launches (tools
   * browser launcher); not a page origin.
   */
  browserRemote: 5180,
  /** Loader dev server: bridge startup imports features from here in dev/test. */
  loader: 5181,
  /** Settings/Hub page dev server. */
  settings: 5183,
  /** Modal child page dev server. */
  modalChild: 5185,
  /** New tab page dev server. */
  newtab: 5186,
  /** Welcome page dev server. */
  welcome: 5187,
  /** Notes page dev server. */
  notes: 5188,
  /** Workflow progress page dev server. */
  workflowProgress: 5192,
} as const;

export type DevPortName = keyof typeof DEV_PORTS;

/** Lowest dev/test server port in this topology. */
export const DEV_PORT_MIN = Math.min(
  ...Object.values(DEV_PORTS),
) as (typeof DEV_PORTS)[DevPortName];

/** Highest dev/test server port in this topology. */
export const DEV_PORT_MAX = Math.max(
  ...Object.values(DEV_PORTS),
) as (typeof DEV_PORTS)[DevPortName];

/**
 * Launch-profile pref carrying the port offset of the dev/test launch that
 * created the profile. Terminology (FDH Gate 2 task 2-B): one offset value
 * shifts every port of `DEV_PORTS` by the same amount, so concurrent dev/test
 * worktrees on one machine can run against disjoint port sets.
 *
 * The launcher (tools/src/dev_env_manager.ts) writes this pref into the launch
 * profile's `user.js` **only when the offset is non-zero**; a profile without
 * the pref resolves to offset 0, which keeps the pre-offset port behavior
 * byte-identical.
 */
export const DEV_PORT_OFFSET_PREF = "nora.dev.port_offset";

/**
 * The numeric span of the dev/test port table. Any two launches whose offsets
 * differ by at least this value use completely disjoint port sets, including
 * the Marionette port (`browserRemote`), because the whole shifted range
 * `[DEV_PORT_MIN + offset, DEV_PORT_MAX + offset]` cannot overlap.
 */
export const DEV_PORT_NON_OVERLAP_SPAN = DEV_PORT_MAX - DEV_PORT_MIN + 1;

/**
 * Dev page ports that window actors historically trust together with the
 * settings/hub page (the `{settings, newtab, welcome, notes}` set).
 */
export const HUB_DEV_PAGES = [
  "settings",
  "newtab",
  "welcome",
  "notes",
] as const satisfies readonly DevPortName[];

/**
 * Dev page ports trusted together including the profile-manager page
 * (the `{profileManager, settings, newtab, welcome, notes}` set).
 */
export const HUB_DEV_PAGES_PLUS_PROFILE = [
  ...HUB_DEV_PAGES,
  "profileManager",
] as const satisfies readonly DevPortName[];

/**
 * Return the numeric port of a named dev/test server.
 * `offset` shifts the port to the offset-adjusted launch port (2-B); the
 * default `0` keeps the frozen base topology identical to main.
 */
export function devPort(name: DevPortName, offset = 0): number {
  return DEV_PORTS[name] + offset;
}

/**
 * Return the port of a named dev/test server as a string (for URL compares).
 */
export function devPortString(name: DevPortName, offset = 0): string {
  return String(DEV_PORTS[name] + offset);
}

/**
 * Build the dev/test http origin (plus optional path) of a named server.
 * e.g. `devHttpOrigin("loader", "/loader/index.ts")`.
 */
export function devHttpOrigin(
  name: DevPortName,
  pathname = "",
  offset = 0,
): string {
  return `http://localhost:${DEV_PORTS[name] + offset}${pathname}`;
}

/**
 * True when a module/document URL is being served by the named dev server.
 * Mirrors the `includes("localhost:<port>")` dev-mode checks used by pages.
 * This is a substring dev-mode detector, not a security predicate: it also
 * matches lookalike hosts such as `localhost:5181.evil.example`. Use
 * `isTrustedDevPort` for trust decisions.
 */
export function moduleUrlServedFromDevPort(
  moduleUrl: string | null | undefined,
  name: DevPortName,
  offset = 0,
): boolean {
  return (moduleUrl ?? "").includes(
    `localhost:${DEV_PORTS[name] + offset}`,
  );
}

/**
 * Offset-agnostic dev-mode detector: true when a module URL is served by
 * **any** localhost http dev/test server, regardless of which table port (or
 * offset) that server actually runs on. In production, page module URLs are
 * `chrome://...` documents, so this matches exactly the load paths that the
 * per-port detector matched without needing to know the launch offset.
 *
 * This is a dev-mode detector, not a security predicate: privileged trust
 * decisions must go through `isTrustedDevPort` (or the launch-port resolver)
 * with an explicit allowlist. Like `moduleUrlServedFromDevPort`, it also
 * accepts the `?dev` form used by loader test pages.
 */
export function moduleUrlServedByDevHttpServer(
  moduleUrl: string | null | undefined,
): boolean {
  if (!moduleUrl) {
    return false;
  }
  const match = (moduleUrl as string).match(
    /^https?:\/\/(?:localhost|127\.0\.0\.1|\[::1\])(?::\d+)?\//,
  );
  return match !== null;
}

/**
 * Trust decision for a dev/test page port against an explicit allowlist of
 * table names. `names` must be an explicit set: an empty list trusts nothing.
 * Accepts both DOM `location.port` (always a string, empty on default ports)
 * and numeric `nsIURI.port` values.
 *
 * `offset` shifts the allowlist to the offset-adjusted launch ports (2-B):
 * with a non-zero offset the **base** table port is no longer trusted and the
 * offset-adjusted port is, so the trust checks match the ports a launch
 * actually uses.
 */
export function isTrustedDevPort(
  port: string | number | null | undefined,
  names: readonly DevPortName[],
  offset = 0,
): boolean {
  if (port === null || port === undefined || port === "") {
    return false;
  }
  const numeric = typeof port === "number" ? port : Number(port);
  if (!Number.isInteger(numeric) || numeric <= 0) {
    return false;
  }
  return names.some((name) => DEV_PORTS[name] + offset === numeric);
}

/**
 * http origins trusted by **production** builds. Intentionally empty:
 * production page origins are `chrome://...` documents and no http origin is
 * trusted. This exists so the production trust surface has a single
 * nameable, testable seam instead of being spread over code paths.
 */
export const PRODUCTION_TRUSTED_HTTP_ORIGINS: readonly string[] = [];

/** Whether the given origin is trusted by a production build (denies http). */
export function isProductionTrustedHttpOrigin(origin: string): boolean {
  return PRODUCTION_TRUSTED_HTTP_ORIGINS.includes(origin);
}

// ─────────────────────────────────────────────────────────────
// Port offset (FDH Gate 2 task 2-B)
// ─────────────────────────────────────────────────────────────

/**
 * Highest HTTP port. The whole shifted table must stay inside the valid port
 * range.
 */
const MAX_HTTP_PORT = 65_535;

/** Largest admissible offset: the last port of the table must stay in range. */
export const DEV_PORT_OFFSET_MAX = MAX_HTTP_PORT - DEV_PORT_MAX;

/**
 * Parse and validate a dev port offset.
 *
 * An offset must be:
 * - a non-negative integer (or a decimal string of one), and
 * - either `0` (the frozen base topology) or at least
 *   `DEV_PORT_NON_OVERLAP_SPAN`, so the shifted table cannot share ports with
 *   another concurrent launch of this topology (`Marionette` included), and
 * - small enough that the whole shifted table stays inside the port range.
 *
 * `auto` is handled by the tooling layer (`tools/src/port_offset.ts`), which
 * probes free offsets; this function validates numeric offsets only.
 *
 * Returns the offset number, or `null` when the value is not an admissible
 * offset. A user-facing reason is available from `devPortOffsetError`.
 */
export function normalizeDevPortOffset(
  value: number | string | null | undefined,
): number | null {
  return parseDevPortOffset(value).offset;
}

/**
 * Error text explaining why `value` is not an admissible dev port offset, or
 * `null` when it is admissible.
 */
export function devPortOffsetError(
  value: number | string | null | undefined,
): string | null {
  return parseDevPortOffset(value).error;
}

function parseDevPortOffset(
  value: number | string | null | undefined,
): { offset: number | null; error: string | null } {
  if (value === null || value === undefined) {
    return { offset: null, error: "no port offset given" };
  }
  const text = String(value).trim();
  if (!/^\d+$/.test(text)) {
    return {
      offset: null,
      error: `port offset must be a non-negative integer or "auto", got ${
        JSON.stringify(value)
      }`,
    };
  }
  const offset = Number(text);
  if (!Number.isSafeInteger(offset) || offset < 0) {
    return {
      offset: null,
      error: `port offset must be >= 0, got ${JSON.stringify(value)}`,
    };
  }
  if (offset > DEV_PORT_OFFSET_MAX) {
    return {
      offset: null,
      error:
        `port offset too large: ${DEV_PORT_MAX} + ${offset} exceeds port ${MAX_HTTP_PORT}`,
    };
  }
  if (offset > 0 && offset < DEV_PORT_NON_OVERLAP_SPAN) {
    return {
      offset: null,
      error:
        `port offset ${offset} is below the non-overlap span ${DEV_PORT_NON_OVERLAP_SPAN}; concurrent dev/test launch port sets would overlap (use 0 or >= ${DEV_PORT_NON_OVERLAP_SPAN})`,
    };
  }
  return { offset, error: null };
}

/**
 * Validate a dev port offset, throwing a user-facing error when it is not
 * admissible. Use for CLI/tool boundaries where a bad offset must abort.
 */
export function requireDevPortOffset(
  value: number | string | null | undefined,
): number {
  const { offset, error } = parseDevPortOffset(value);
  if (offset === null) {
    throw new Error(error ?? "Invalid port offset");
  }
  return offset;
}

/** Coerce a launch-profile pref value into a valid offset (0 when invalid). */
export function devPortOffsetFromPref(
  value: number | null | undefined,
): number {
  if (!Number.isInteger(value) || (value as number) < 0) {
    return 0;
  }
  return parseDevPortOffset(value).offset ?? 0;
}

/**
 * Marionette port of a dev/test launch with the given offset: the
 * `browserRemote` table default shifted by the launch offset. Distinct
 * admissible offsets therefore give concurrent worktrees distinct Marionette
 * ports, including the `browserRemote` 5180 base default.
 */
export function marionettePortForOffset(offset = 0): number {
  return DEV_PORTS.browserRemote + offset;
}

/**
 * True when two offsets produce completely disjoint port sets for this
 * topology (every table port, Marionette included). Guaranteed whenever both
 * offsets are admissible and their difference is at least
 * `DEV_PORT_NON_OVERLAP_SPAN`.
 */
export function devPortOffsetsAreDisjoint(a: number, b: number): boolean {
  if (
    a === null || b === null || !Number.isSafeInteger(a) ||
    !Number.isSafeInteger(b)
  ) {
    return false;
  }
  return Math.abs(a - b) >= DEV_PORT_NON_OVERLAP_SPAN;
}
