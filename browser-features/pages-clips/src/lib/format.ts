/**
 * The short date and time shown under a clip. i18n.language can carry
 * Mozilla-only tags like "ja-JP-mac"; Intl wants a plain BCP47 tag.
 */
export function formatClipTime(createdAt: number, language: string): string {
  const locale = language.replace(/-mac$/, "");
  try {
    return new Intl.DateTimeFormat(locale, {
      month: "2-digit",
      day: "2-digit",
      hour: "2-digit",
      minute: "2-digit",
    }).format(new Date(createdAt));
  } catch {
    return new Date(createdAt).toLocaleString();
  }
}
