// SPDX-License-Identifier: MPL-2.0

/**
 * Shared PWA URL eligibility policy.
 *
 * A PWA may only be installed or launched for `https:` URLs, or for `http:`
 * URLs targeting loopback hosts (`localhost`, `127.0.0.1`). Non-loopback
 * plain-HTTP pages are not secure contexts, so installing or launching them
 * as apps would present untrusted content in a trusted-looking window.
 */
export function isPwaUrlEligible(url: string): boolean {
  let parsed: URL;
  try {
    parsed = new URL(url);
  } catch {
    return false;
  }
  if (parsed.protocol === "https:") {
    return true;
  }
  if (parsed.protocol === "http:") {
    return parsed.hostname === "localhost" || parsed.hostname === "127.0.0.1";
  }
  return false;
}
