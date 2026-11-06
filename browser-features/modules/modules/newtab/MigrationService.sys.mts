// SPDX-License-Identifier: MPL-2.0
import type {
  NewTabSettings,
  NewTabSettingsPatch,
  PinnedSite,
} from "#libs/newtab/types.ts";
import { SettingsService } from "./SettingsService.sys.mts";

export interface PlacesBookmark {
  guid: string;
  parentGuid: string;
  title: string | null;
  type: number;
  index: number;
  url?: URL | string;
  childCount?: number;
}
export interface BookmarkBackend {
  readonly TYPE_BOOKMARK: number;
  readonly TYPE_FOLDER: number;
  readonly unfiledGuid: string;
  readonly rootGuid: string;
  readonly userContentRoots: readonly string[];
  fetch(
    query: string | { parentGuid: string },
    callback?: (node: PlacesBookmark) => void,
  ): Promise<PlacesBookmark | null>;
  insert(
    info: {
      guid?: string;
      parentGuid: string;
      type: number;
      title: string;
      url?: string;
      index?: number;
    },
  ): Promise<PlacesBookmark>;
  update(
    info: {
      guid: string;
      parentGuid?: string;
      title?: string;
      url?: string;
      index?: number;
    },
  ): Promise<PlacesBookmark>;
  remove(guid: string): Promise<unknown>;
  reorder(parentGuid: string, guids: string[]): Promise<void>;
}
interface Receipt {
  guid: string;
  parentGuid: string;
  title: string;
  type: "folder" | "bookmark";
  url?: string;
  sourceIndex?: number;
  state: "planned" | "created" | "user-changed";
}
interface Journal {
  version: 1;
  status: "running" | "complete";
  originalRaw: string | null;
  originalPinned: PinnedSite[];
  root: Receipt;
  common: Receipt;
  items: Receipt[];
}
export interface MigrationDependencies {
  bookmarks: BookmarkBackend;
  settings: {
    get(): NewTabSettings;
    updateInternal(changes: NewTabSettingsPatch): Promise<NewTabSettings>;
  };
  storage: {
    read(): Promise<unknown | null>;
    write(value: unknown): Promise<void>;
  };
  readRaw(): string | null;
  makeGuid(): string;
  commonTitle: string;
}
const ROOT_GUID = "flrpSDroot__";
const COMMON_GUID = "flrpSDcomm__";
const GUID = /^[A-Za-z0-9_-]{12}$/;
function failure(code: string, message: string): Error & { code: string } {
  return Object.assign(new Error(message), { code });
}
function record(value: unknown): value is Record<string, unknown> {
  return !!value && typeof value === "object" && !Array.isArray(value);
}
function receipt(value: unknown): value is Receipt {
  return record(value) && typeof value.guid === "string" &&
    GUID.test(value.guid) &&
    typeof value.parentGuid === "string" && GUID.test(value.parentGuid) &&
    typeof value.title === "string" &&
    (value.type === "folder" || value.type === "bookmark") &&
    ["planned", "created", "user-changed"].includes(String(value.state)) &&
    (value.type !== "bookmark" || typeof value.url === "string");
}
function parseJournal(value: unknown): Journal {
  if (
    !record(value) || value.version !== 1 ||
    !["running", "complete"].includes(String(value.status)) ||
    !(value.originalRaw === null || typeof value.originalRaw === "string") ||
    !Array.isArray(value.originalPinned) ||
    !value.originalPinned.every((pin) =>
      record(pin) && typeof pin.url === "string" &&
      typeof pin.title === "string"
    ) ||
    !receipt(value.root) || !receipt(value.common) ||
    !Array.isArray(value.items) || !value.items.every(receipt)
  ) {
    throw failure(
      "UNAVAILABLE",
      "The Speed Dial migration journal is invalid. Existing bookmarks and settings were not reset.",
    );
  }
  return value as unknown as Journal;
}
function canonicalLegacyUrl(value: string): string {
  try {
    return new URL(value).href;
  } catch {
    throw failure(
      "VALIDATION",
      "A saved pinned address is invalid. Existing settings were preserved.",
    );
  }
}
function urlOf(node: PlacesBookmark): string | undefined {
  return node.url === undefined ? undefined : String(node.url);
}
async function stablePinGuid(pin: PinnedSite, index: number): Promise<string> {
  const encoded = new TextEncoder().encode(
    JSON.stringify(["floorp-speed-dial-v1", index, pin.url, pin.title]),
  );
  const bytes = new Uint8Array(encoded.length);
  bytes.set(encoded);
  const digest = new Uint8Array(
    await crypto.subtle.digest("SHA-256", bytes.buffer),
  );
  // A reproducible 72-bit Places identity preserves intentional duplicate entries
  // (source index is included) and avoids duplicating the same import after Sync.
  return ChromeUtils.base64URLEncode(Uint8Array.from(digest.slice(0, 9)), {
    pad: false,
  });
}

/** Independent dependencies allow interrupted imports to be exercised without touching a real profile. */
export function createMigrationRunner(deps: MigrationDependencies) {
  let running: Promise<void> | null = null;
  let completedJournal: Journal | null = null;
  const save = (journal: Journal) => deps.storage.write(journal);
  const validRoot = async (guid: string): Promise<PlacesBookmark | null> => {
    const root = await deps.bookmarks.fetch(guid);
    if (
      !root || root.type !== deps.bookmarks.TYPE_FOLDER ||
      guid === deps.bookmarks.rootGuid ||
      deps.bookmarks.userContentRoots.includes(guid)
    ) return null;
    const visited = new Set([guid]);
    let parent = root.parentGuid;
    while (parent) {
      if (visited.has(parent)) return null;
      visited.add(parent);
      if (deps.bookmarks.userContentRoots.includes(parent)) return root;
      if (parent === deps.bookmarks.rootGuid) return null;
      const node = await deps.bookmarks.fetch(parent);
      if (!node || node.type !== deps.bookmarks.TYPE_FOLDER) return null;
      parent = node.parentGuid;
    }
    return null;
  };
  const run = async (options: { repair?: boolean }) => {
    const settings = deps.settings.get();
    const stored = completedJournal && !options.repair
      ? completedJournal
      : await deps.storage.read();
    let fresh = false;
    let journal: Journal;
    if (stored === null) {
      // Existing configured GUIDs are not arbitrary folders to adopt without a journal.
      if (
        (settings.speedDial.rootGuid &&
          settings.speedDial.rootGuid !== ROOT_GUID) ||
        (settings.speedDial.commonGuid &&
          settings.speedDial.commonGuid !== COMMON_GUID)
      ) {
        throw failure(
          "UNAVAILABLE",
          "Speed Dial has no matching migration journal. Existing bookmarks were not adopted or overwritten.",
        );
      }
      const rootGuid = settings.speedDial.rootGuid ?? ROOT_GUID;
      const commonGuid = settings.speedDial.commonGuid ?? COMMON_GUID;
      const rootExisting = await deps.bookmarks.fetch(rootGuid);
      if (rootExisting && !await validRoot(rootGuid)) {
        throw failure(
          "UNAVAILABLE",
          "The reserved Speed Dial identity is not a valid folder.",
        );
      }
      const commonExisting = await deps.bookmarks.fetch(commonGuid);
      if (
        commonExisting &&
        (commonExisting.type !== deps.bookmarks.TYPE_FOLDER ||
          commonExisting.parentGuid !== rootGuid)
      ) {
        throw failure(
          "UNAVAILABLE",
          "The Common folder was moved. Existing bookmarks were preserved.",
        );
      }
      if (rootExisting && !commonExisting && !options.repair) {
        throw failure(
          "UNAVAILABLE",
          "The Common folder is missing. Use Repair Speed Dial to create an empty Common folder.",
        );
      }
      const pins = settings.topSites.pinned.map((pin) => ({ ...pin }));
      journal = {
        version: 1,
        status: "running",
        originalRaw: deps.readRaw(),
        originalPinned: pins,
        root: {
          guid: rootGuid,
          parentGuid: rootExisting?.parentGuid ?? deps.bookmarks.unfiledGuid,
          title: rootExisting?.title ?? "Speed Dial",
          type: "folder",
          state: rootExisting ? "created" : "planned",
        },
        common: {
          guid: commonGuid,
          parentGuid: rootGuid,
          title: commonExisting?.title ?? deps.commonTitle,
          type: "folder",
          state: commonExisting ? "created" : "planned",
        },
        items: await Promise.all(
          pins.map(async (pin, index): Promise<Receipt> => ({
            guid: await stablePinGuid(pin, index),
            parentGuid: commonGuid,
            title: pin.title,
            url: canonicalLegacyUrl(pin.url),
            type: "bookmark",
            sourceIndex: index,
            state: "planned",
          })),
        ),
      };
      // The raw source and every intended GUID are durable before any Places write.
      await save(journal);
      fresh = true;
    } else journal = parseJournal(stored);

    if (journal.status === "complete") {
      let root = await validRoot(journal.root.guid);
      let common = await deps.bookmarks.fetch(journal.common.guid);
      if (
        !root || !common || common.type !== deps.bookmarks.TYPE_FOLDER ||
        common.parentGuid !== root.guid
      ) {
        if (!options.repair) {
          throw failure(
            "UNAVAILABLE",
            "Speed Dial folders were removed or moved. Your bookmarks were preserved; use Repair Speed Dial to create an empty entry.",
          );
        }
        // Repair creates new empty containers; it never imports the old pinned data again.
        if (!root) {
          journal.root = {
            guid: deps.makeGuid(),
            parentGuid: deps.bookmarks.unfiledGuid,
            title: "Speed Dial",
            type: "folder",
            state: "planned",
          };
          journal.common = {
            guid: deps.makeGuid(),
            parentGuid: journal.root.guid,
            title: deps.commonTitle,
            type: "folder",
            state: "planned",
          };
          await save(journal);
          root = await deps.bookmarks.insert({
            guid: journal.root.guid,
            parentGuid: journal.root.parentGuid,
            title: journal.root.title,
            type: deps.bookmarks.TYPE_FOLDER,
          });
          journal.root.state = "created";
          await save(journal);
          common = null;
        }
        if (
          !common || common.parentGuid !== root.guid ||
          common.type !== deps.bookmarks.TYPE_FOLDER
        ) {
          journal.common = {
            guid: deps.makeGuid(),
            parentGuid: root.guid,
            title: deps.commonTitle,
            type: "folder",
            state: "planned",
          };
          await save(journal);
          common = await deps.bookmarks.insert({
            guid: journal.common.guid,
            parentGuid: root.guid,
            title: journal.common.title,
            type: deps.bookmarks.TYPE_FOLDER,
          });
          journal.common.state = "created";
          await save(journal);
        }
      }
      const current = deps.settings.get().speedDial;
      if (
        current.rootGuid !== journal.root.guid ||
        current.commonGuid !== journal.common.guid
      ) {
        await deps.settings.updateInternal({
          speedDial: {
            rootGuid: journal.root.guid,
            commonGuid: journal.common.guid,
          },
        });
      }
      completedJournal = journal;
      return;
    }

    if (
      JSON.stringify(deps.settings.get().topSites.pinned) !==
        JSON.stringify(journal.originalPinned)
    ) {
      throw failure(
        "CONFLICT",
        "Pinned sites changed during migration. Existing bookmarks and the original backup were preserved.",
      );
    }
    for (const item of [journal.root, journal.common, ...journal.items]) {
      const node = await deps.bookmarks.fetch(item.guid);
      if (item.state !== "planned") {
        // Receipts prove prior commits; user deletion/movement is never undone.
        const missingContainer = item === journal.root
          ? !await validRoot(item.guid)
          : item === journal.common
          ? !node || node.type !== deps.bookmarks.TYPE_FOLDER ||
            node.parentGuid !== journal.root.guid
          : false;
        if (missingContainer) {
          if (!options.repair) {
            throw failure(
              "UNAVAILABLE",
              "A migration folder changed. Existing data was preserved.",
            );
          }
          // An explicit repair recovers an empty entry rather than resurrecting
          // a container (and its children) that the user deliberately removed.
          journal.status = "complete";
          await save(journal);
          return run(options);
        }
        if (
          !node || node.parentGuid !== item.parentGuid ||
          (node.title ?? "") !== item.title ||
          (item.type === "bookmark" && urlOf(node) !== item.url)
        ) {
          item.state = "user-changed";
          await save(journal);
        }
        continue;
      }
      if (node) {
        const matches = node.type === (item.type === "folder"
              ? deps.bookmarks.TYPE_FOLDER
              : deps.bookmarks.TYPE_BOOKMARK) &&
          node.parentGuid === item.parentGuid &&
          (item.type === "folder" ||
            ((node.title ?? "") === item.title && urlOf(node) === item.url));
        if (!matches) {
          throw failure(
            "CONFLICT",
            "A planned bookmark identity is already in use. No existing bookmark was overwritten.",
          );
        }
      } else {
        if (!fresh && !options.repair) {
          throw failure(
            "UNAVAILABLE",
            "The import was interrupted before its commit could be confirmed. Use Repair Speed Dial to resume the unfinished import.",
          );
        }
        await deps.bookmarks.insert({
          guid: item.guid,
          parentGuid: item.parentGuid,
          title: item.title,
          type: item.type === "folder"
            ? deps.bookmarks.TYPE_FOLDER
            : deps.bookmarks.TYPE_BOOKMARK,
          ...(item.url ? { url: item.url } : {}),
        });
      }
      item.state = "created";
      await save(journal);
    }
    await deps.settings.updateInternal({
      speedDial: {
        rootGuid: journal.root.guid,
        commonGuid: journal.common.guid,
      },
    });
    journal.status = "complete";
    await save(journal);
    completedJournal = journal;
  };
  return {
    ensure(options: { repair?: boolean } = {}): Promise<void> {
      if (running) return running;
      const promise = run(options);
      running = promise;
      void promise.finally(() => {
        if (running === promise) running = null;
      }).catch(() => {});
      return promise;
    },
  };
}

const { PlacesUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/PlacesUtils.sys.mjs",
) as {
  PlacesUtils: { bookmarks: BookmarkBackend; history: { makeGuid(): string } };
};
const directory = () => PathUtils.join(PathUtils.profileDir, "floorp");
const filename = () => PathUtils.join(directory(), "newtab-migration-v1.json");
export const MigrationService = createMigrationRunner({
  bookmarks: PlacesUtils.bookmarks,
  settings: SettingsService,
  readRaw: () =>
    Services.prefs.getPrefType("floorp.newtab.configs") ===
        Services.prefs.PREF_STRING
      ? Services.prefs.getStringPref("floorp.newtab.configs")
      : null,
  makeGuid: () => PlacesUtils.history.makeGuid(),
  commonTitle: Services.locale.appLocaleAsBCP47.startsWith("ja")
    ? "共通"
    : "Common",
  storage: {
    async read() {
      return await IOUtils.exists(filename())
        ? await IOUtils.readJSON(filename())
        : null;
    },
    async write(value) {
      await IOUtils.makeDirectory(directory(), {
        createAncestors: true,
        ignoreExisting: true,
        permissions: 0o700,
      });
      await IOUtils.writeJSON(filename(), value, {
        tmpPath: `${filename()}.tmp`,
        flush: true,
      });
    },
  },
});
