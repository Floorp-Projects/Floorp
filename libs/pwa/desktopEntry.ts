function removeUnsafeControlCharacters(value: string): string {
  let sanitized = "";
  for (const char of value) {
    const code = char.charCodeAt(0);
    const isUnsafeControl = (code >= 0 && code <= 8) ||
      code === 11 ||
      code === 12 ||
      (code >= 14 && code <= 31) ||
      code === 127;
    if (!isUnsafeControl) {
      sanitized += char;
    }
  }
  return sanitized;
}

/**
 * Sanitizes a single Desktop Entry value so manifest-controlled text cannot
 * inject additional keys by including line breaks or control characters.
 */
export function sanitizeDesktopEntryValue(value: string): string {
  return removeUnsafeControlCharacters(value.replace(/[\r\n]+/g, " "))
    .replace(/\\/g, "\\\\")
    .trim();
}
