/**
 * Composite key helpers for SSB (PWA) storage.
 * Format: "start_url:userContextId"
 *
 * Shared between chrome dataStore and parent-process DataStore.sys.mts.
 */

export function buildSsbKey(
  startUrl: string,
  userContextId: number = 0,
): string {
  return `${startUrl}:${userContextId}`;
}

export function parseSsbKey(
  key: string,
): { startUrl: string; userContextId: number } {
  const lastColon = key.lastIndexOf(":");
  if (lastColon === -1) {
    return { startUrl: key, userContextId: 0 };
  }
  const startUrl = key.substring(0, lastColon);
  const userContextId = parseInt(key.substring(lastColon + 1), 10) || 0;
  return { startUrl, userContextId };
}
