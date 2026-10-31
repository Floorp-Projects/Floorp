// SPDX-License-Identifier: MPL-2.0
import type { Sponsor } from "#libs/newtab/types.ts";
import { isRecord } from "#libs/newtab/origin.ts";
import { SettingsService } from "./SettingsService.sys.mts";
import { BookmarkService } from "./BookmarkService.sys.mts";

const CATALOG_PREF = "floorp.newtab.sponsorCatalog";
// The existing partner is retained. A second real placement can be supplied by
// Floorp's catalog; fictitious design-preview advertisers never ship.
const BUILTIN_CATALOG = [{
  id: "cubesoft",
  title: "Cubesoft",
  url: "https://www.cube-soft.jp/",
}];
export function parseSponsorCatalog(
  value: unknown,
  now = Date.now(),
): Sponsor[] {
  if (!Array.isArray(value)) return [];
  const result: Sponsor[] = [], ids = new Set<string>();
  for (const row of value.slice(0, 100)) {
    if (
      !isRecord(row) || typeof row.id !== "string" ||
      !/^[a-zA-Z0-9._:-]{1,128}$/.test(row.id) || ids.has(row.id) ||
      typeof row.title !== "string" || !row.title.trim() ||
      row.title.length > 200 || typeof row.url !== "string"
    ) continue;
    if (
      row.activeFrom !== undefined &&
      (typeof row.activeFrom !== "number" || !Number.isFinite(row.activeFrom) ||
        now < row.activeFrom)
    ) continue;
    if (
      row.activeUntil !== undefined &&
      (typeof row.activeUntil !== "number" ||
        !Number.isFinite(row.activeUntil) || now >= row.activeUntil)
    ) continue;
    try {
      const url = new URL(row.url);
      if (
        !["http:", "https:"].includes(url.protocol) || url.username ||
        url.password
      ) continue;
      ids.add(row.id);
      result.push({ id: row.id, title: row.title.trim(), url: url.href });
    } catch { /* Invalid placements do not interrupt bookmarks. */ }
  }
  return result;
}
let saving: Promise<unknown> = Promise.resolve();
function enqueue<T>(work: () => Promise<T>): Promise<T> {
  const result = saving.then(work);
  saving = result.catch(() => {});
  return result;
}
export const SponsorService = {
  catalog(): Sponsor[] {
    const raw = Services.prefs.getStringPref(CATALOG_PREF, "");
    if (!raw) return parseSponsorCatalog(BUILTIN_CATALOG);
    try {
      return parseSponsorCatalog(JSON.parse(raw));
    } catch {
      return [];
    }
  },
  list(): Sponsor[] {
    const settings = SettingsService.get();
    return settings.sponsors.enabled
      ? this.catalog().filter((item) =>
        !settings.sponsors.dismissedIds.includes(item.id)
      ).slice(0, 2)
      : [];
  },
  dismiss(id: string) {
    return enqueue(() => {
      if (!this.catalog().some((item) => item.id === id)) {
        throw new Error("This sponsor is no longer available.");
      }
      const current = SettingsService.get();
      if (current.sponsors.dismissedIds.includes(id)) return Promise.resolve(current);
      return SettingsService.updateInternal({
        sponsors: { dismissedIds: [...current.sponsors.dismissedIds, id] },
      });
    });
  },
  restore() {
    return enqueue(() =>
      SettingsService.updateInternal({ sponsors: { dismissedIds: [] } })
    );
  },
  save(
    id: string,
    parentGuid: string,
    expectedTitle: string,
    expectedUrl: string,
  ): Promise<{ guid?: string }> {
    const work = async () => {
      const sponsor = this.catalog().find((item) => item.id === id);
      if (!sponsor) throw new Error("This sponsor is no longer available.");
      if (sponsor.title !== expectedTitle || sponsor.url !== expectedUrl) {
        throw Object.assign(
          new Error("This sponsor changed. Reload the page before saving it."),
          { code: "CONFLICT" },
        );
      }
      const folder = await BookmarkService.folder(parentGuid);
      const existing = folder.children.find((item) =>
        item.type === "bookmark" && item.url === sponsor.url
      );
      if (existing) return { guid: existing.guid };
      return BookmarkService.mutate({
        action: "create",
        type: "bookmark",
        parentGuid,
        title: sponsor.title,
        url: sponsor.url,
      });
    };
    return enqueue(work);
  },
};
