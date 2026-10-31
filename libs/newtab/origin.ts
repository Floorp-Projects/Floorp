// SPDX-License-Identifier: MPL-2.0
/** Exact privileged document and explicitly enabled development-origin allowlist. */
export function isNewTabDocument(uri: string, development: boolean): boolean {
  const withoutFragment = uri.split("#", 1)[0].split("?", 1)[0];
  if (["about:newtab", "about:home", "about:hub"].includes(withoutFragment)) {
    return true;
  }
  try {
    const url = new URL(uri);
    if (
      url.protocol === "chrome:" &&
      ["noraneko-newtab", "noraneko-settings"].includes(url.hostname)
    ) {
      return url.pathname === "/content/index.html" ||
        url.pathname === "/content/index.xhtml";
    }
    return development && url.protocol === "http:" &&
      ["localhost", "127.0.0.1"].includes(url.hostname) &&
      ["5183", "5186"].includes(url.port) && url.pathname === "/";
  } catch {
    return false;
  }
}
export function isRecord(value: unknown): value is Record<string, unknown> {
  return value !== null && typeof value === "object" && !Array.isArray(value);
}
