// SPDX-License-Identifier: MPL-2.0

import { Parser } from "budoux";
import i18next from "i18next";

type LocaleCategory = "ja" | "zh-hans" | "zh-hant" | "th" | "other";

const CJK_LOCALE_PREFIXES: ReadonlySet<string> = new Set([
  "ja",
  "zh",
  "th",
]);

const LOCALE_TO_CATEGORY: ReadonlyMap<string, LocaleCategory> = new Map([
  ["ja", "ja"],
  ["zh", "zh-hans"],
  ["th", "th"],
]);

/**
 * Determines the locale category for Budoux segmentation.
 * Returns "other" for non-CJK/Thai locales.
 */
function getLocaleCategory(locale: string): LocaleCategory {
  const prefix = locale.split("-")[0].toLowerCase();
  return LOCALE_TO_CATEGORY.get(prefix) ?? "other";
}

// Lazy singleton parsers, keyed by locale category.
const parserCache = new Map<LocaleCategory, Parser | null>();

function getParser(category: LocaleCategory): Parser | null {
  if (category === "other") return null;
  const cached = parserCache.get(category);
  if (cached !== undefined) return cached;

  let parser: Parser | null = null;
  try {
    switch (category) {
      case "ja":
        parser = Parser.loadDefaultJapaneseParser();
        break;
      case "zh-hans":
        parser = Parser.loadDefaultSimplifiedChineseParser();
        break;
      case "zh-hant":
        parser = Parser.loadDefaultTraditionalChineseParser();
        break;
      case "th":
        parser = Parser.loadDefaultThaiParser();
        break;
    }
  } catch (err) {
    console.error("[CommandPalette] Failed to initialize Budoux parser:", err);
    parser = null;
  }
  parserCache.set(category, parser);
  return parser;
}

/**
 * Segments Japanese/CJK text into morpheme-like phrases using Budoux.
 * For non-CJK/Thai locales, returns [text] unchanged.
 */
export function segmentJapaneseText(text: string): string[] {
  if (!text.trim()) return [];

  const locale = i18next.language ?? "";
  const category = getLocaleCategory(locale);
  const parser = getParser(category);

  if (!parser) return [text];

  try {
    return parser.parse(text);
  } catch (err) {
    console.error("[CommandPalette] Budoux segmentation error:", err);
    return [text];
  }
}

/**
 * Segments text and returns all segments plus the full original text (lowercased).
 * Useful for keyword generation where both whole and parts are valuable.
 */
export function segmentTextForKeywords(text: string): string[] {
  if (!text.trim()) return [];

  const segments = segmentJapaneseText(text);
  const result = new Set<string>();

  // Add the full original text
  const full = text.toLowerCase().trim();
  if (full) result.add(full);

  // Add each segment (lowercased)
  for (const seg of segments) {
    const s = seg.toLowerCase().trim();
    if (s) result.add(s);
  }

  return [...result];
}

/**
 * Fetches label and description from i18next using the given keys,
 * then segments them into Budoux keywords.
 */
export function getSegmentedKeywordsFromI18nKeys(
  labelKey: string,
  descriptionKey: string,
): string[] {
  const label = i18next.t(labelKey, { defaultValue: "" });
  const description = i18next.t(descriptionKey, { defaultValue: "" });
  return [
    ...segmentTextForKeywords(label),
    ...segmentTextForKeywords(description),
  ];
}

/**
 * Checks if the current locale benefits from Budoux segmentation.
 */
export function isCJKLocale(): boolean {
  const locale = i18next.language ?? "";
  const prefix = locale.split("-")[0].toLowerCase();
  return CJK_LOCALE_PREFIXES.has(prefix);
}
