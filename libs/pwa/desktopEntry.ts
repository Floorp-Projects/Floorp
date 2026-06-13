/**
 * Sanitizes a single Desktop Entry value so manifest-controlled text cannot
 * inject additional keys by including line breaks or control characters.
 */
export function sanitizeDesktopEntryValue(value: string): string {
  return value
    .replace(/[\r\n]+/g, " ")
    .replace(/[\u0000-\u0008\u000B\u000C\u000E-\u001F\u007F]/g, "")
    .replace(/\\/g, "\\\\")
    .trim();
}
