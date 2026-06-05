// SPDX-License-Identifier: MPL-2.0

import {
  loadDefaultJapaneseParser,
  loadDefaultSimplifiedChineseParser,
  loadDefaultTraditionalChineseParser,
  loadDefaultThaiParser,
  type Parser,
} from "budoux";
import i18next from "i18next";
import type { LocaleCategory } from "./types.ts";

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
  const lower = locale.toLowerCase();
  const prefix = lower.split("-")[0];

  // Differentiate simplified vs traditional Chinese
  if (prefix === "zh") {
    if (
      lower.includes("-hant") ||
      lower.includes("-tw") ||
      lower.includes("-hk") ||
      lower.includes("-mo")
    ) {
      return "zh-hant";
    }
    return "zh-hans";
  }

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
        parser = loadDefaultJapaneseParser();
        break;
      case "zh-hans":
        parser = loadDefaultSimplifiedChineseParser();
        break;
      case "zh-hant":
        parser = loadDefaultTraditionalChineseParser();
        break;
      case "th":
        parser = loadDefaultThaiParser();
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
 * Also generates hiragana-converted keywords from katakana portions using Budoux
 * segmentation for fine-grained matching.
 */
export function segmentTextForKeywords(text: string): string[] {
  if (!text.trim()) return [];

  const segments = segmentJapaneseText(text);
  const result = new Set<string>();

  // Add the full original text (lowercased)
  const full = text.toLowerCase().trim();
  if (full) result.add(full);

  // Add each segment (lowercased)
  for (const seg of segments) {
    const s = seg.toLowerCase().trim();
    if (s) result.add(s);
  }

  // Generate hiragana-converted keywords from katakana portions
  const hiraganaFull = toHiragana(full);
  if (hiraganaFull !== full) {
    const hiraganaSegments = segmentJapaneseText(hiraganaFull);
    if (hiraganaFull.trim()) result.add(hiraganaFull.trim());
    for (const seg of hiraganaSegments) {
      const s = seg.toLowerCase().trim();
      if (s) result.add(s);
    }
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

/**
 * Converts full-width katakana (U+30A1..U+30F6) to hiragana (U+3041..U+3096)
 * using a fixed Unicode codepoint offset of 0x60.
 * Non-katakana characters pass through unchanged.
 * Prolonged sound mark (ー, U+30FC) is NOT converted (used identically in hiragana).
 */
export function toHiragana(text: string): string {
  let result = "";
  for (let i = 0; i < text.length; i++) {
    const code = text.charCodeAt(i);
    if (code >= 0x30A1 && code <= 0x30F6) {
      result += String.fromCharCode(code - 0x60);
    } else {
      result += text[i];
    }
  }
  return result;
}
