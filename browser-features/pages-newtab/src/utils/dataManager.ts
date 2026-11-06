// SPDX-License-Identifier: MPL-2.0
// Compatibility adapter for existing new-tab components. The parent service is
// the only owner of settings; no page writes the raw preference anymore.
import { newTabClient } from "../../../../libs/newtab/client.ts";
import type {
  NewTabSettings,
  NewTabSettingsPatch,
  PinnedSite,
} from "../../../../libs/newtab/types.ts";
export type { NewTabSettings, PinnedSite };
export interface FolderPathResult {
  path: string | null;
  success: boolean;
}
export interface RandomImageResult {
  image: string | null;
  fileName: string | null;
  success: boolean;
}
export const DEFAULT_SUGGESTED_SITES: PinnedSite[] = [
  { url: "https://youtube.com", title: "YouTube" },
  { url: "https://x.com", title: "X" },
  { url: "https://www.reddit.com/", title: "Reddit" },
  { url: "https://wikipedia.org/", title: "Wikipedia" },
  { url: "https://instagram.com", title: "Instagram" },
];
export async function getNewTabSettings(): Promise<NewTabSettings> {
  return (await newTabClient.call("snapshot", {})).settings;
}
function defined<T extends object, K extends keyof T>(
  source: T,
  keys: readonly K[],
): Partial<Pick<T, K>> {
  return Object.fromEntries(
    keys
      .filter((key) => source[key] !== undefined)
      .map((key) => [key, source[key]]),
  ) as Partial<Pick<T, K>>;
}
let saveQueue = Promise.resolve();
export function saveNewTabSettings(
  settings: Partial<NewTabSettings>,
): Promise<void> {
  const changes: NewTabSettingsPatch = {};
  if (settings.components)
    changes.components = defined(settings.components, [
      "clock",
      "searchBar",
      "firefoxLayout",
      "topSites",
    ]);
  if (settings.background)
    changes.background = defined(settings.background, [
      "type",
      "customImage",
      "fileName",
      "folderPath",
      "selectedFloorp",
      "slideshowEnabled",
      "slideshowInterval",
    ]);
  if (settings.searchBar)
    changes.searchBar = defined(settings.searchBar, ["searchEngine"]);
  if (settings.topSites)
    changes.topSites = defined(settings.topSites, ["blocked"]);
  if (settings.home)
    changes.home = defined(settings.home, [
      "layout",
      "showHistory",
      "showFeed",
      "translucentPanels",
    ]);
  if (settings.speedDial)
    changes.speedDial = defined(settings.speedDial, [
      "show",
      "columns",
      "tileSize",
      "workspaceGroups",
    ]);
  if (settings.rss) changes.rss = defined(settings.rss, ["refreshMinutes"]);
  if (settings.sponsors)
    changes.sponsors = defined(settings.sponsors, ["enabled"]);
  const operation = saveQueue
    .catch(() => {})
    .then(async () => {
      const current = await getNewTabSettings();
      await newTabClient.call("patchSettings", {
        expectedRevision: current.revision,
        changes,
      });
    });
  saveQueue = operation;
  return operation;
}
export async function getFolderPathFromDialog(): Promise<FolderPathResult> {
  const result = await newTabClient.call("pickFolder", {});
  return { path: result.path, success: Boolean(result.path) };
}
export async function getRandomImageFromFolder(
  _folderPath: string,
): Promise<RandomImageResult> {
  // The parent uses its saved, user-selected path. Pages cannot select an
  // arbitrary filesystem path by passing it to this legacy signature.
  const result = await newTabClient.call("folderImage", {});
  return { ...result, success: Boolean(result.image) };
}
