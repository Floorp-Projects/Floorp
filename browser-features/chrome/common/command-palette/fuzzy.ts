// SPDX-License-Identifier: MPL-2.0

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

export function fuzzyScore(query: string, target: FuzzyTarget): number {
  const q = query.toLowerCase();
  const label = target.label.toLowerCase();
  const desc = target.description.toLowerCase();

  // Exact prefix match on label — highest priority
  if (label.startsWith(q)) return 100 + q.length;

  // Word boundary match on label
  const words = label.split(/\s+/);
  if (words.some((w) => w.startsWith(q))) return 80 + q.length;

  // Substring match on label
  if (label.includes(q)) return 60 + q.length;

  // Keyword match
  if (target.keywords.some((kw) => kw.toLowerCase().includes(q))) return 50 + q.length;

  // Substring match on description
  if (desc.includes(q)) return 30 + q.length;

  // Fuzzy character sequence match on label
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
