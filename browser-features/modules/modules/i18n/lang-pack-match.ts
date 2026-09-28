// SPDX-License-Identifier: MPL-2.0

export function findRequestedLangPack<T extends { target_locale: string }>(
  available: T[] | null,
  requested: string,
): T | null {
  const locale = requested.split(",", 1)[0]?.trim().replaceAll("_", "-");
  if (!locale) return null;

  // Language pack identifiers can be less specific than requested app locales:
  // Firefox publishes Japanese as "ja", while an existing preference may be "ja-JP".
  const exact = available?.find((pack) =>
    pack.target_locale.toLowerCase() === locale.toLowerCase()
  );
  if (exact) return exact;

  const language = locale.split("-")[0];
  if (language === locale) return null;
  return available?.find((pack) =>
    pack.target_locale.toLowerCase() === language.toLowerCase()
  ) ?? null;
}
