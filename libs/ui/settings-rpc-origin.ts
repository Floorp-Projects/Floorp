import {
  DEV_PORT_NON_OVERLAP_SPAN,
  DEV_PORT_OFFSET_MAX,
} from "../shared/dev-ports/defines.ts";

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
 * True when the document is a localhost http page on a port that belongs to
 * the dev/test launch topology for `basePort`: the base port itself, or a
 * `--port-offset` shifted port (offset 0 or >= DEV_PORT_NON_OVERLAP_SPAN).
 * Content-principal pages entered by a direct `http://localhost:<shifted
 * port>/` URL cannot read `Services.prefs`, so the launch-port resolver
 * cannot infer the offset there; this detector keeps the birpc transport in
 * dev at any admissible offset while leaving the colocated integration test
 * servers (5196/5197, which are not launch ports) on their fixture path.
 */
export function isDevLaunchPage(
  pageUrl: string,
  basePort: number,
): boolean {
  try {
    const url = new URL(pageUrl);
    if (
      url.protocol !== "http:" ||
      (url.hostname !== "localhost" && url.hostname !== "127.0.0.1")
    ) {
      return false;
    }
    const port = Number(url.port);
    if (!Number.isInteger(port) || port <= 0) return false;
    const offset = port - basePort;
    return offset === 0 ||
      (offset >= DEV_PORT_NON_OVERLAP_SPAN &&
        offset <= DEV_PORT_OFFSET_MAX);
  } catch {
    return false;
  }
}
