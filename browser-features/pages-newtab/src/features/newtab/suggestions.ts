// SPDX-License-Identifier: MPL-2.0
import type {
  BookmarkNode,
  PinnedSite,
  Sponsor,
} from "../../../../../libs/newtab/types.ts";

function key(value: string): string | null {
  try {
    const url = new URL(value);
    if (
      !["http:", "https:"].includes(url.protocol) || url.username ||
      url.password
    ) return null;
    url.hash = "";
    return url.href;
  } catch {
    return null;
  }
}

export function selectHistorySuggestions(
  sites: PinnedSite[],
  bookmarks: BookmarkNode[],
  sponsors: Sponsor[],
  blocked: string[],
): PinnedSite[] {
  const excluded = new Set(
    [
      ...bookmarks.flatMap((node) => node.url ? [node.url] : []),
      ...sponsors.map((site) => site.url),
      ...blocked,
    ]
      .map(key).filter((url): url is string => url !== null),
  );
  return sites.filter((site) => {
    const url = key(site.url);
    if (!url || excluded.has(url)) return false;
    excluded.add(url);
    return true;
  });
}
