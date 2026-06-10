// SPDX-License-Identifier: MPL-2.0

import { segmentJapaneseText, isCJKLocale } from "./utils/budouxSegmenter.ts";

export interface FuzzyTarget {
  id: string;
  label: string;
  description: string;
  category: string;
  keywords: string[];
}

export interface ScoredResult<T extends FuzzyTarget> {
  item: T;
  score: number;
}

function singleWordScore(query: string, target: FuzzyTarget): number {
  const q = query.toLowerCase();
  const label = target.label.toLowerCase();
  const desc = target.description.toLowerCase();

  if (label.startsWith(q)) return 100 + q.length;

  const words = label.split(/\s+/);
  if (words.some((w) => w.startsWith(q))) return 80 + q.length;

  if (label.includes(q)) return 60 + q.length;

  if (target.keywords.some((kw) => kw.toLowerCase().includes(q))) return 50 + q.length;

  if (desc.includes(q)) return 30 + q.length;

  let score = 0;
  let qi = 0;
  for (let i = 0; i < label.length && qi < q.length; i++) {
    if (label[i] === q[qi]) {
      score += 1;
      qi++;
    }
  }
  if (qi === q.length) return score;

  return 0;
}

/**
 * Splits a query into search terms. For CJK/Thai locales,
 * uses Budoux segmentation to handle text without spaces.
 * Falls back to whitespace splitting for other locales.
 */
function segmentQuery(query: string): string[] {
  const trimmed = query.trim().toLowerCase();
  if (!trimmed) return [];

  if (isCJKLocale()) {
    try {
      const segments = segmentJapaneseText(trimmed);
      if (segments.length > 1) return segments;
    } catch (error) {
      console.error("[CommandPalette]", error);
    }
  }

  return trimmed.split(/\s+/).filter(Boolean);
}

export function fuzzyScore(query: string, target: FuzzyTarget): number {
  const words = segmentQuery(query);

  if (words.length <= 1) {
    return singleWordScore(query.trim(), target);
  }

  let totalScore = 0;
  for (const word of words) {
    const wordScore = singleWordScore(word, target);
    if (wordScore === 0) return 0;
    totalScore += wordScore;
  }
  return totalScore;
}

export function fuzzySearch<T extends FuzzyTarget>(
  query: string,
  items: T[],
  limit = 50,
): T[] {
  const normalizedQuery = query.trim();
  if (!normalizedQuery) return items;

  const results: ScoredResult<T>[] = [];
  for (const item of items) {
    const score = fuzzyScore(normalizedQuery, item);
    if (score > 0) {
      results.push({ item, score });
    }
  }

  results.sort((a, b) => b.score - a.score);
  return results.slice(0, limit).map((r) => r.item);
}
