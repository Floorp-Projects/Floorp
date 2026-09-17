import type { LocaleInfo } from "./type.ts";

export function resolveSystemLocale(
  systemLocale: LocaleInfo["systemLocale"],
  available: string[],
  negotiatedLocale?: string,
): string | undefined {
  // Prefer the OS region when supported, then the browser's negotiated pack.
  // Some packs use only a language code (ja), others require a region (en-US).
  for (
    const candidate of [
      systemLocale.baseName,
      negotiatedLocale,
      systemLocale.language,
    ]
  ) {
    if (!candidate) continue;
    const match = available.find((locale) =>
      locale.toLowerCase() === candidate.toLowerCase()
    );
    if (match) return match;
  }
  return undefined;
}
