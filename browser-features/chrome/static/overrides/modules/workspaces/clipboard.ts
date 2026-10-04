// SPDX-License-Identifier: MPL-2.0

export function readNewTabClipboard(
  readSelection: () => string,
  readGlobal: () => string,
  sanitize: ((text: string) => string) | undefined,
): string {
  let text = "";
  try {
    text = readSelection().trim();
  } catch {
    // Try the global clipboard when the selection is unavailable.
  }
  if (!text) {
    try {
      text = readGlobal().trim();
    } catch {
      return "";
    }
  }
  // Never pass unsanitized clipboard text to a trusted navigation API.
  try {
    return sanitize?.(text).trim() ?? "";
  } catch {
    return "";
  }
}
