// SPDX-License-Identifier: MPL-2.0

/**
 * Authoritative definition table for the dev/test server ports.
 *
 * Every dev/test Vite server, tooling port and dev-origin trust decision in
 * tools/, bridge/ and browser-features/ must reference this table instead of a
 * hardcoded `5173-5192` literal. Production builds never load any code from
 * these ports: their page origins are `chrome://...` documents, and no http
 * origin is trusted in a production build (see
 * `PRODUCTION_TRUSTED_HTTP_ORIGINS` and `isProductionTrustedHttpOrigin`).
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
  /** Default port for dev/test browser launches (tools browser launcher). */
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

/** Return the numeric port of a named dev/test server. */
export function devPort(name: DevPortName): number {
  return DEV_PORTS[name];
}

/** Return the port of a named dev/test server as a string (for URL compares). */
export function devPortString(name: DevPortName): string {
  return String(DEV_PORTS[name]);
}

/**
 * Build the dev/test http origin (plus optional path) of a named server.
 * e.g. `devHttpOrigin("loader", "/loader/index.ts")`.
 */
export function devHttpOrigin(name: DevPortName, pathname = ""): string {
  return `http://localhost:${DEV_PORTS[name]}${pathname}`;
}

/**
 * True when a module/document URL is being served by the named dev server.
 * Mirrors the `includes("localhost:<port>")` dev-mode checks used by pages.
 */
export function moduleUrlServedFromDevPort(
  moduleUrl: string | null | undefined,
  name: DevPortName,
): boolean {
  return (moduleUrl ?? "").includes(`localhost:${DEV_PORTS[name]}`);
}

/**
 * Trust decision for a dev/test page port against an explicit allowlist of
 * table names. `names` must be an explicit set: an empty list trusts nothing.
 * Accepts both DOM `location.port` (always a string, empty on default ports)
 * and numeric `nsIURI.port` values.
 */
export function isTrustedDevPort(
  port: string | number | null | undefined,
  names: readonly DevPortName[],
): boolean {
  if (port === null || port === undefined || port === "") {
    return false;
  }
  const numeric = typeof port === "number" ? port : Number(port);
  if (!Number.isInteger(numeric) || numeric <= 0) {
    return false;
  }
  return names.some((name) => DEV_PORTS[name] === numeric);
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
