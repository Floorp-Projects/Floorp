// SPDX-License-Identifier: MPL-2.0

export type TextSegment = { text: string; matched: boolean };

export function getHighlightSegments(
  query: string,
  text: string,
): TextSegment[] {
  const q = query.trim().toLowerCase();
  if (!q) return [{ text, matched: false }];

  const lower = text.toLowerCase();
  const ranges = findMatchRanges(q, lower);
  if (ranges.length === 0) return [{ text, matched: false }];

  const segments: TextSegment[] = [];
  let lastEnd = 0;
  for (const [start, end] of ranges) {
    if (start > lastEnd) {
      segments.push({ text: text.slice(lastEnd, start), matched: false });
    }
    segments.push({ text: text.slice(start, end), matched: true });
    lastEnd = end;
  }
  if (lastEnd < text.length) {
    segments.push({ text: text.slice(lastEnd), matched: false });
  }
  return segments;
}

function findMatchRanges(
  query: string,
  lowerText: string,
): [number, number][] {
  // Prefix match
  if (lowerText.startsWith(query)) {
    return [[0, query.length]];
  }

  // Word boundary match — find the word that starts with the query
  const wordRegex = /\b\w/g;
  let match: RegExpExecArray | null;
  while ((match = wordRegex.exec(lowerText)) !== null) {
    if (lowerText.slice(match.index).startsWith(query)) {
      return [[match.index, match.index + query.length]];
    }
  }

  // Substring match
  const idx = lowerText.indexOf(query);
  if (idx !== -1) {
    return [[idx, idx + query.length]];
  }

  // Fuzzy character sequence — collect individual char positions
  const positions: number[] = [];
  let qi = 0;
  for (let i = 0; i < lowerText.length && qi < query.length; i++) {
    if (lowerText[i] === query[qi]) {
      positions.push(i);
      qi++;
    }
  }
  if (qi === query.length && positions.length > 0) {
    return mergePositions(positions);
  }

  return [];
}

function mergePositions(positions: number[]): [number, number][] {
  const ranges: [number, number][] = [];
  let start = positions[0];
  let prev = positions[0];
  for (let i = 1; i < positions.length; i++) {
    if (positions[i] === prev + 1) {
      prev = positions[i];
    } else {
      ranges.push([start, prev + 1]);
      start = positions[i];
      prev = positions[i];
    }
  }
  ranges.push([start, prev + 1]);
  return ranges;
}
