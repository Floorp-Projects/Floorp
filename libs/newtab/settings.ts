// SPDX-License-Identifier: MPL-2.0
import type { NewTabSettings, NewTabSettingsPatch } from "./types.ts";

export const NEWTAB_SETTINGS_PREF = "floorp.newtab.configs";
export const NEWTAB_SETTINGS_VERSION = 1;

export class NewTabSettingsError extends Error {
  constructor(public readonly code: string, message: string) {
    super(message);
    this.name = "NewTabSettingsError";
  }
}

export function createDefaultNewTabSettings(): NewTabSettings {
  return {
    schemaVersion: NEWTAB_SETTINGS_VERSION,
    revision: 0,
    components: {
      topSites: true,
      clock: true,
      searchBar: true,
      firefoxLayout: true,
    },
    background: {
      type: "random",
      customImage: null,
      fileName: null,
      folderPath: null,
      selectedFloorp: null,
      slideshowEnabled: false,
      slideshowInterval: 30,
    },
    searchBar: { searchEngine: "default" },
    topSites: {
      pinned: [{
        url: "https://www.cube-soft.jp/",
        title: "Cubesoft (Sponsor)",
      }, {
        url: "https://docs.floorp.app/docs/features/",
        title: "Floorp Support",
      }],
      blocked: [],
    },
    speedDial: {
      rootGuid: null,
      commonGuid: null,
      workspaceGroups: {},
      show: true,
      columns: 6,
      tileSize: "standard",
    },
    home: {
      showHistory: true,
      showFeed: true,
      layout: "balanced",
      translucentPanels: true,
    },
    rss: { refreshMinutes: 15, retentionDays: 90, maxItems: 500 },
    sponsors: { enabled: true, dismissedIds: [] },
  };
}

function invalid(): never {
  throw new NewTabSettingsError(
    "VALIDATION",
    "The new tab settings are invalid. The saved settings have not been changed.",
  );
}
function record(value: unknown): Record<string, unknown> {
  if (!value || typeof value !== "object" || Array.isArray(value)) {
    return invalid();
  }
  return value as Record<string, unknown>;
}
function block(value: unknown): Record<string, unknown> {
  return value === undefined ? {} : record(value);
}
function boolean(value: unknown, fallback: boolean): boolean {
  return value === undefined
    ? fallback
    : typeof value === "boolean"
    ? value
    : invalid();
}
function string(value: unknown, fallback: string): string {
  return value === undefined
    ? fallback
    : typeof value === "string"
    ? value
    : invalid();
}
function nullableString(
  value: unknown,
  fallback: string | null = null,
): string | null {
  return value === undefined
    ? fallback
    : value === null || typeof value === "string"
    ? value
    : invalid();
}
function integer(
  value: unknown,
  fallback: number,
  min: number,
  max: number,
): number {
  return value === undefined
    ? fallback
    : typeof value === "number" && Number.isSafeInteger(value) &&
        value >= min && value <= max
    ? value
    : invalid();
}
function legacyInterval(value: unknown): number {
  // The old number input had no upper bound and could save zero/fractions.
  // Reading those values must not disable the whole new-tab profile.
  return value === undefined
    ? 30
    : typeof value === "number" && Number.isFinite(value) && value >= 0 &&
        value <= Number.MAX_SAFE_INTEGER
    ? value
    : invalid();
}
function choice<T extends string | number>(
  value: unknown,
  choices: readonly T[],
  fallback: T,
): T {
  return value === undefined
    ? fallback
    : choices.includes(value as T)
    ? value as T
    : invalid();
}
function strings(value: unknown, fallback: string[]): string[] {
  return value === undefined
    ? fallback
    : Array.isArray(value) && value.every((item) => typeof item === "string")
    ? [...value]
    : invalid();
}

/** Merge defaults without erasing unknown keys or legacy settings. Never writes. */
export function normalizeNewTabSettings(value: unknown): NewTabSettings {
  const source = record(value);
  const version = integer(
    source.schemaVersion,
    NEWTAB_SETTINGS_VERSION,
    0,
    Number.MAX_SAFE_INTEGER,
  );
  if (version > NEWTAB_SETTINGS_VERSION) {
    throw new NewTabSettingsError(
      "UNSUPPORTED_VERSION",
      "These new tab settings were saved by a newer version of Floorp.",
    );
  }
  const defaults = createDefaultNewTabSettings();
  const components = block(source.components),
    background = block(source.background),
    searchBar = block(source.searchBar),
    topSites = block(source.topSites),
    speedDial = block(source.speedDial),
    home = block(source.home),
    rss = block(source.rss),
    sponsors = block(source.sponsors);
  const oldTopSites = boolean(components.topSites, true);
  let pinned = defaults.topSites.pinned;
  if (topSites.pinned !== undefined) {
    if (!Array.isArray(topSites.pinned)) return invalid();
    pinned = topSites.pinned.map((item) => {
      const site = record(item);
      if (typeof site.url !== "string" || typeof site.title !== "string") {
        return invalid();
      }
      return { ...site, url: site.url, title: site.title };
    });
  }
  const workspaceGroups = block(speedDial.workspaceGroups);
  if (
    !Object.values(workspaceGroups).every((guid) =>
      typeof guid === "string" && /^[a-zA-Z0-9_-]{12}$/.test(guid)
    )
  ) return invalid();
  const result: NewTabSettings = {
    ...source,
    schemaVersion: NEWTAB_SETTINGS_VERSION,
    revision: integer(source.revision, 0, 0, Number.MAX_SAFE_INTEGER - 1),
    components: {
      ...components,
      topSites: oldTopSites,
      clock: boolean(components.clock, true),
      searchBar: boolean(components.searchBar, true),
      firefoxLayout: boolean(components.firefoxLayout, true),
    },
    background: {
      ...background,
      type: choice(background.type, [
        "none",
        "random",
        "custom",
        "folderPath",
        "floorp",
      ], "random"),
      customImage: nullableString(background.customImage),
      fileName: nullableString(background.fileName),
      folderPath: nullableString(background.folderPath),
      selectedFloorp: nullableString(background.selectedFloorp),
      slideshowEnabled: boolean(background.slideshowEnabled, false),
      slideshowInterval: legacyInterval(background.slideshowInterval),
    },
    searchBar: {
      ...searchBar,
      searchEngine: string(searchBar.searchEngine, "default"),
    },
    topSites: { ...topSites, pinned, blocked: strings(topSites.blocked, []) },
    speedDial: {
      ...speedDial,
      rootGuid: nullableString(speedDial.rootGuid),
      commonGuid: nullableString(speedDial.commonGuid),
      workspaceGroups: { ...workspaceGroups } as Record<string, string>,
      show: boolean(speedDial.show, oldTopSites),
      columns: choice(speedDial.columns, [4, 5, 6], 6),
      tileSize: choice(speedDial.tileSize, ["standard", "compact"], "standard"),
    },
    home: {
      ...home,
      showHistory: boolean(home.showHistory, oldTopSites),
      showFeed: boolean(home.showFeed, true),
      layout: choice(home.layout, ["balanced", "links", "reading"], "balanced"),
      translucentPanels: boolean(home.translucentPanels, true),
    },
    rss: {
      ...rss,
      refreshMinutes: choice(rss.refreshMinutes, [0, 15, 30, 60], 15),
      retentionDays: integer(rss.retentionDays, 90, 1, 3650),
      maxItems: integer(rss.maxItems, 500, 10, 10000),
    },
    sponsors: {
      ...sponsors,
      enabled: boolean(sponsors.enabled, true),
      dismissedIds: strings(sponsors.dismissedIds, []),
    },
  };
  for (const guid of [result.speedDial.rootGuid, result.speedDial.commonGuid]) {
    if (guid !== null && !/^[a-zA-Z0-9_-]{12}$/.test(guid)) return invalid();
  }
  return result;
}

export function parseNewTabSettings(raw: string | null): NewTabSettings {
  if (raw === null) return createDefaultNewTabSettings();
  let value: unknown;
  try {
    value = JSON.parse(raw);
  } catch {
    throw new NewTabSettingsError(
      "VALIDATION",
      "The saved new tab settings could not be read. They have not been changed.",
    );
  }
  return normalizeNewTabSettings(value);
}

const PATCH_KEYS: Record<string, readonly string[]> = {
  components: ["topSites", "clock", "searchBar", "firefoxLayout"],
  background: [
    "type",
    "customImage",
    "fileName",
    "folderPath",
    "selectedFloorp",
    "slideshowEnabled",
    "slideshowInterval",
  ],
  searchBar: ["searchEngine"],
  topSites: ["blocked"],
  speedDial: ["workspaceGroups", "show", "columns", "tileSize"],
  home: ["showHistory", "showFeed", "layout", "translucentPanels"],
  rss: ["refreshMinutes", "retentionDays", "maxItems"],
  sponsors: ["enabled", "dismissedIds"],
};

/** Only known leaf settings may be changed by a page. Unknown saved keys survive. */
export function mergeNewTabSettings(
  current: NewTabSettings,
  changes: NewTabSettingsPatch,
  internal = false,
): NewTabSettings {
  const patch = record(changes);
  const merged: Record<string, unknown> = { ...current };
  for (const [key, value] of Object.entries(patch)) {
    const allowed = Object.hasOwn(PATCH_KEYS, key)
      ? PATCH_KEYS[key]
      : undefined;
    if (!allowed) return invalid();
    const fields = record(value);
    for (const field of Object.keys(fields)) {
      if (
        !allowed.includes(field) &&
        !(internal && key === "speedDial" &&
          ["rootGuid", "commonGuid"].includes(field)) &&
        !(internal && key === "topSites" && field === "pinned")
      ) return invalid();
    }
    if (key === "background" && fields.slideshowInterval !== undefined) {
      const interval = fields.slideshowInterval;
      if (
        typeof interval !== "number" || !Number.isFinite(interval) ||
        interval < 1 || interval > Number.MAX_SAFE_INTEGER
      ) return invalid();
    }
    merged[key] = { ...record(merged[key]), ...fields };
  }
  merged.revision = current.revision + 1;
  return normalizeNewTabSettings(merged);
}
