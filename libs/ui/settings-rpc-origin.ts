// SPDX-License-Identifier: MPL-2.0
export function usesSettingsActor(
  pageUrl: string,
  developmentPort: string,
): boolean {
  const url = new URL(pageUrl);
  return url.protocol === "http:" && url.port === developmentPort &&
    (url.hostname === "localhost" || url.hostname === "127.0.0.1");
}

/**
 * True when the document itself is served by a localhost http dev server,
 * regardless of which port the launch bound. Pages entered by a direct
 * `http://localhost:<shifted port>/` URL run with a content principal where
 * `Services.prefs` is unavailable, so the launch-port resolver cannot infer
 * the offset there; this detector keeps the birpc transport in dev at any
 * `--port-offset`.
 */
export function isDevHttpPage(pageUrl: string): boolean {
  try {
    const url = new URL(pageUrl);
    return url.protocol === "http:" &&
      (url.hostname === "localhost" || url.hostname === "127.0.0.1");
  } catch {
    return false;
  }
}
