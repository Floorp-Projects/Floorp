/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const PRODUCTION_ORIGINS = new Set([
  "https://plugins.floorp.app",
  "https://store.floorp.app",
]);
const DEVELOPMENT_HOSTS = new Set(["localhost", "127.0.0.1"]);

export function isTrustedPluginStoreSource(
  uri: string | null | undefined,
  allowDevelopment: boolean,
): boolean {
  if (!uri) {
    return false;
  }

  try {
    const url = new URL(uri);
    if (PRODUCTION_ORIGINS.has(url.origin)) {
      return true;
    }
    return allowDevelopment &&
      (url.protocol === "http:" || url.protocol === "https:") &&
      DEVELOPMENT_HOSTS.has(url.hostname);
  } catch {
    return false;
  }
}

export function isValidPluginId(value: unknown): value is string {
  if (
    typeof value !== "string" ||
    value.length === 0 ||
    value.length > 512
  ) {
    return false;
  }
  for (const character of value) {
    const codePoint = character.codePointAt(0) ?? 0;
    if (codePoint < 0x20 || codePoint === 0x7f) {
      return false;
    }
  }
  return true;
}

function isBoundedOptionalString(
  value: unknown,
  maximumLength: number,
): boolean {
  return value === undefined ||
    (typeof value === "string" && value.length <= maximumLength);
}

export function isValidPluginMetadata(value: unknown): boolean {
  if (value === undefined) {
    return true;
  }
  if (!value || typeof value !== "object" || Array.isArray(value)) {
    return false;
  }

  const metadata = value as Record<string, unknown>;
  if (
    !isBoundedOptionalString(metadata.id, 512) ||
    !isBoundedOptionalString(metadata.name, 512) ||
    !isBoundedOptionalString(metadata.description, 8192) ||
    !isBoundedOptionalString(metadata.version, 128) ||
    !isBoundedOptionalString(metadata.author, 512) ||
    !isBoundedOptionalString(metadata.category, 256) ||
    !isBoundedOptionalString(metadata.icon, 8192) ||
    !isBoundedOptionalString(metadata.uri, 8192) ||
    (metadata.isOfficial !== undefined &&
      typeof metadata.isOfficial !== "boolean") ||
    (metadata.functions !== undefined && !Array.isArray(metadata.functions))
  ) {
    return false;
  }

  try {
    return JSON.stringify(metadata.functions ?? []).length <= 32768;
  } catch {
    return false;
  }
}
