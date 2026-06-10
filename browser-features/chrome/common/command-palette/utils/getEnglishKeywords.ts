// SPDX-License-Identifier: MPL-2.0

import i18next from "i18next";

const ENGLISH_LOCALE = "en-US";

const STOP_WORDS = new Set([
  "a",
  "an",
  "the",
  "in",
  "on",
  "at",
  "to",
  "of",
  "for",
  "with",
  "and",
  "or",
  "is",
  "it",
  "by",
  "from",
]);

/**
 * Splits text into search tokens: the full lowercase string plus individual
 * significant words (excluding common stop words).
 */
function tokenizeEnglish(text: string): string[] {
  const lower = text.toLowerCase().trim();
  if (!lower) return [];

  const tokens = new Set<string>();
  tokens.add(lower);

  for (const word of lower.split(/\s+/)) {
    if (word && !STOP_WORDS.has(word)) {
      tokens.add(word);
    }
  }
  return [...tokens];
}

/**
 * Returns English search keywords for a gesture action.
 * Fetches the English label and description via i18next with lng: "en-US".
 * Only returns keywords when the English text differs from the current locale.
 */
export function getEnglishGestureKeywords(actionId: string): string[] {
  try {
    const enLabel = i18next.t(`mouseGesture.actions.${actionId}`, {
      lng: ENGLISH_LOCALE,
      defaultValue: "",
    });
    const enDescription = i18next.t(`mouseGesture.descriptions.${actionId}`, {
      lng: ENGLISH_LOCALE,
      defaultValue: "",
    });

    const keywords: string[] = [];

    const currentLabel = i18next.t(`mouseGesture.actions.${actionId}`, {
      defaultValue: "",
    });
    if (enLabel && enLabel !== currentLabel) {
      keywords.push(...tokenizeEnglish(enLabel));
    }

    const currentDesc = i18next.t(`mouseGesture.descriptions.${actionId}`, {
      defaultValue: "",
    });
    if (enDescription && enDescription !== currentDesc) {
      keywords.push(...tokenizeEnglish(enDescription));
    }

    return keywords;
  } catch (err) {
    console.error("[CommandPalette] getEnglishGestureKeywords error:", err);
    return [];
  }
}

/**
 * Returns English search keywords for a step command.
 * Takes the i18n keys for label and description and fetches their English values.
 * Only returns keywords when the English text differs from the current locale.
 */
export function getEnglishStepCommandKeywords(
  labelKey: string,
  descriptionKey: string,
): string[] {
  try {
    const enLabel = i18next.t(labelKey, {
      lng: ENGLISH_LOCALE,
      defaultValue: "",
    });
    const enDescription = i18next.t(descriptionKey, {
      lng: ENGLISH_LOCALE,
      defaultValue: "",
    });

    const keywords: string[] = [];

    const currentLabel = i18next.t(labelKey, { defaultValue: "" });
    if (enLabel && enLabel !== currentLabel) {
      keywords.push(...tokenizeEnglish(enLabel));
    }

    const currentDesc = i18next.t(descriptionKey, { defaultValue: "" });
    if (enDescription && enDescription !== currentDesc) {
      keywords.push(...tokenizeEnglish(enDescription));
    }

    return keywords;
  } catch (err) {
    console.error("[CommandPalette] getEnglishStepCommandKeywords error:", err);
    return [];
  }
}
