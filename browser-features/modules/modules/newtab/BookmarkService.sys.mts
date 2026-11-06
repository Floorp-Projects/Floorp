// SPDX-License-Identifier: MPL-2.0
import type {
  BookmarkCommand,
  BookmarkNode,
  DeleteSummary,
  FolderView,
} from "#libs/newtab/types.ts";
import { SettingsService } from "./SettingsService.sys.mts";
import {
  type BookmarkBackend,
  MigrationService,
  type PlacesBookmark,
} from "./MigrationService.sys.mts";

const EVENTS = [
  "bookmark-added",
  "bookmark-removed",
  "bookmark-moved",
  "bookmark-guid-changed",
  "bookmark-title-changed",
  "bookmark-url-changed",
  "purge-caches",
];
interface BookmarkDependencies {
  bookmarks: BookmarkBackend;
  managedGuids(): { rootGuid: string | null; commonGuid: string | null };
  ensure(options?: { repair?: boolean }): Promise<void>;
  observe(listener: () => void): () => void;
}
function failure(code: string, message: string): Error & { code: string } {
  return Object.assign(new Error(message), { code });
}
function guid(value: unknown): asserts value is string {
  if (typeof value !== "string" || !/^[A-Za-z0-9_-]{12}$/.test(value)) {
    throw failure("VALIDATION", "A valid bookmark identifier is required.");
  }
}
function title(value: unknown): asserts value is string {
  if (typeof value !== "string" || value.length > 4096 || !value.trim()) {
    throw failure("VALIDATION", "Enter a bookmark or folder name.");
  }
}
function safeUrl(value: unknown): string {
  if (typeof value !== "string" || value.length > 65536) {
    throw failure("VALIDATION", "Enter a valid web address.");
  }
  let url: URL;
  try {
    url = new URL(value);
  } catch {
    throw failure("VALIDATION", "Enter a valid web address.");
  }
  if (url.protocol !== "https:" && url.protocol !== "http:") {
    throw failure("VALIDATION", "New sites must use HTTP or HTTPS.");
  }
  if (url.username || url.password) {
    throw failure(
      "VALIDATION",
      "Addresses containing credentials are not supported.",
    );
  }
  return url.href;
}
function dto(
  node: PlacesBookmark,
  backend: BookmarkBackend,
): BookmarkNode | null {
  if (
    node.type !== backend.TYPE_BOOKMARK && node.type !== backend.TYPE_FOLDER
  ) return null;
  if (node.type === backend.TYPE_BOOKMARK) {
    try {
      const protocol = new URL(String(node.url)).protocol;
      if (protocol !== "http:" && protocol !== "https:") return null;
    } catch {
      return null;
    }
  }
  return {
    guid: node.guid,
    parentGuid: node.parentGuid,
    title: node.title ?? "",
    index: node.index,
    type: node.type === backend.TYPE_FOLDER ? "folder" : "bookmark",
    ...(node.type === backend.TYPE_BOOKMARK
      ? { url: String(node.url ?? "") }
      : { childCount: node.childCount ?? 0 }),
  };
}

/** Places is the only bookmark store. This view owns no copied tree. */
export function createBookmarkService(deps: BookmarkDependencies) {
  let revision = 0;
  let queue: Promise<void> = Promise.resolve();
  let unobserve: (() => void) | null = null;
  const listeners = new Set<(revision: number) => void>();
  const changed = () => {
    ++revision;
    for (const listener of listeners) {
      try {
        listener(revision);
      } catch (error) {
        console.error("[NewTabBookmarks] Listener failed", error);
      }
    }
  };
  const observe = () => {
    unobserve ??= deps.observe(changed);
  };
  function enqueue<T>(work: () => Promise<T>): Promise<T> {
    const result = queue.then(work);
    queue = result.then(() => {}, () => {});
    return result;
  }
  const get = async (id: string): Promise<PlacesBookmark> => {
    guid(id);
    const result = await deps.bookmarks.fetch(id);
    if (!result) {
      throw failure("NOT_FOUND", "This bookmark or folder no longer exists.");
    }
    return result;
  };
  const rootGuid = (): string => {
    const id = deps.managedGuids().rootGuid;
    if (!id) throw failure("UNAVAILABLE", "Speed Dial is not initialized.");
    return id;
  };
  const children = async (parentGuid: string): Promise<PlacesBookmark[]> => {
    const result: PlacesBookmark[] = [];
    await deps.bookmarks.fetch({ parentGuid }, (node) => result.push(node));
    return result.sort((a, b) => a.index - b.index);
  };
  const path = async (id: string): Promise<PlacesBookmark[]> => {
    const result: PlacesBookmark[] = [];
    const visited = new Set<string>();
    const root = rootGuid();
    let current = await get(id);
    while (current.guid !== root) {
      if (visited.has(current.guid)) {
        throw failure(
          "VALIDATION",
          "A cyclic bookmark hierarchy cannot be opened.",
        );
      }
      visited.add(current.guid);
      result.unshift(current);
      if (!current.parentGuid) {
        throw failure("VALIDATION", "This location is outside Speed Dial.");
      }
      current = await get(current.parentGuid);
    }
    if (current.type !== deps.bookmarks.TYPE_FOLDER) {
      throw failure("UNAVAILABLE", "The Speed Dial root is not a folder.");
    }
    return result;
  };
  const folderInScope = async (
    id: string,
    allowRoot = true,
  ): Promise<PlacesBookmark[]> => {
    const ancestry = await path(id);
    const current = ancestry.at(-1) ?? await get(rootGuid());
    if (current.type !== deps.bookmarks.TYPE_FOLDER) {
      throw failure("VALIDATION", "The destination is not a folder.");
    }
    if (!allowRoot && ancestry.length === 0) {
      throw failure("VALIDATION", "Choose a group or a folder inside it.");
    }
    return ancestry;
  };
  const summary = async (id: string): Promise<DeleteSummary> => {
    const ancestry = await path(id);
    if (!ancestry.length) {
      throw failure(
        "VALIDATION",
        "The Speed Dial root cannot be deleted here.",
      );
    }
    const result: DeleteSummary = {
      guid: id,
      folders: 0,
      bookmarks: 0,
      revision,
    };
    const pending = [await get(id)];
    const visited = new Set<string>();
    for (let index = 0; index < pending.length; index++) {
      const node = pending[index];
      if (visited.has(node.guid)) {
        throw failure(
          "VALIDATION",
          "A cyclic bookmark hierarchy cannot be deleted.",
        );
      }
      visited.add(node.guid);
      if (node.type === deps.bookmarks.TYPE_FOLDER) {
        if (node.guid !== id) result.folders++;
        pending.push(...await children(node.guid));
      } else if (node.type === deps.bookmarks.TYPE_BOOKMARK) result.bookmarks++;
    }
    if (result.revision !== revision) {
      throw failure(
        "CONFLICT",
        "Bookmarks changed while counting this folder. Review it again.",
      );
    }
    return result;
  };
  return {
    async ensure(options: { repair?: boolean } = {}): Promise<void> {
      observe();
      await deps.ensure(options);
    },
    async groups(): Promise<BookmarkNode[]> {
      observe();
      const root = await get(rootGuid());
      if (root.type !== deps.bookmarks.TYPE_FOLDER) {
        throw failure("UNAVAILABLE", "The Speed Dial root is not a folder.");
      }
      return (await children(root.guid)).filter((node) =>
        node.type === deps.bookmarks.TYPE_FOLDER
      ).map((node) => dto(node, deps.bookmarks)!);
    },
    async folder(id: string): Promise<FolderView> {
      observe();
      const ancestry = await folderInScope(id, false);
      const node = ancestry.at(-1)!;
      const items = (await children(id)).map((item) =>
        dto(item, deps.bookmarks)
      ).filter((item): item is BookmarkNode => item !== null);
      return {
        guid: node.guid,
        title: node.title ?? "",
        groupGuid: ancestry[0].guid,
        ancestors: ancestry.map((item) => dto(item, deps.bookmarks)!),
        children: items,
        revision,
      };
    },
    deleteSummary(id: string): Promise<DeleteSummary> {
      observe();
      return summary(id);
    },
    mutate(command: BookmarkCommand): Promise<{ guid?: string }> {
      return enqueue(async () => {
        observe();
        if (!command || typeof command !== "object") {
          throw failure("VALIDATION", "A bookmark operation is required.");
        }
        switch (command.action) {
          case "create": {
            title(command.title);
            await folderInScope(command.parentGuid);
            if (command.type !== "folder" && command.type !== "bookmark") {
              throw failure("VALIDATION", "Unknown bookmark type.");
            }
            if (
              command.parentGuid === rootGuid() && command.type !== "folder"
            ) {
              throw failure(
                "VALIDATION",
                "The Speed Dial root contains groups, not sites.",
              );
            }
            const inserted = await deps.bookmarks.insert({
              parentGuid: command.parentGuid,
              title: command.title.trim(),
              type: command.type === "folder"
                ? deps.bookmarks.TYPE_FOLDER
                : deps.bookmarks.TYPE_BOOKMARK,
              ...(command.type === "bookmark"
                ? { url: safeUrl(command.url) }
                : {}),
            });
            return { guid: inserted.guid };
          }
          case "update": {
            title(command.title);
            const ancestry = await path(command.guid);
            if (!ancestry.length) {
              throw failure(
                "VALIDATION",
                "The Speed Dial root cannot be edited here.",
              );
            }
            const node = ancestry.at(-1)!;
            if (
              node.type !== deps.bookmarks.TYPE_BOOKMARK &&
              node.type !== deps.bookmarks.TYPE_FOLDER
            ) {
              throw failure(
                "VALIDATION",
                "This bookmark type is not editable here.",
              );
            }
            if (
              node.type === deps.bookmarks.TYPE_FOLDER &&
              command.url !== undefined
            ) {
              throw failure(
                "VALIDATION",
                "A folder cannot have a site address.",
              );
            }
            const url =
              command.url === undefined || command.url === String(node.url)
                ? undefined
                : safeUrl(command.url);
            await deps.bookmarks.update({
              guid: command.guid,
              title: command.title.trim(),
              ...(url ? { url } : {}),
            });
            return { guid: command.guid };
          }
          case "move": {
            const ancestry = await path(command.guid);
            if (!ancestry.length) {
              throw failure(
                "VALIDATION",
                "The Speed Dial root cannot be moved here.",
              );
            }
            const source = ancestry.at(-1)!;
            const target = await folderInScope(command.parentGuid);
            if (
              command.parentGuid === command.guid ||
              target.some((node) => node.guid === command.guid)
            ) {
              throw failure(
                "VALIDATION",
                "A folder cannot be moved into itself or a descendant.",
              );
            }
            if (
              command.parentGuid === rootGuid() &&
              source.type !== deps.bookmarks.TYPE_FOLDER
            ) {
              throw failure(
                "VALIDATION",
                "The Speed Dial root contains groups, not sites.",
              );
            }
            if (
              command.index !== undefined &&
              (!Number.isSafeInteger(command.index) || command.index < -1)
            ) throw failure("VALIDATION", "Invalid bookmark position.");
            await deps.bookmarks.update({
              guid: command.guid,
              parentGuid: command.parentGuid,
              index: command.index ?? -1,
            });
            return { guid: command.guid };
          }
          case "reorder": {
            await folderInScope(command.parentGuid);
            if (
              !Array.isArray(command.guids) ||
              new Set(command.guids).size !== command.guids.length
            ) {
              throw failure(
                "VALIDATION",
                "The order contains duplicate identifiers.",
              );
            }
            const all = await children(command.parentGuid);
            const editable = all.filter((node) =>
              dto(node, deps.bookmarks) !== null
            );
            const expected = new Set(editable.map((node) => node.guid));
            if (
              expected.size !== command.guids.length ||
              command.guids.some((id) => !expected.has(id))
            ) {
              throw failure(
                "CONFLICT",
                "The folder contents changed. Reload before changing their order.",
              );
            }
            // Preserve separators/unsupported items at their original positions.
            let offset = 0;
            const ordered = all.map((node) =>
              expected.has(node.guid) ? command.guids[offset++] : node.guid
            );
            if (ordered.length) {
              await deps.bookmarks.reorder(command.parentGuid, ordered);
            }
            return {};
          }
          case "delete": {
            if (
              !Number.isSafeInteger(command.expectedRevision) ||
              command.expectedRevision !== revision
            ) {
              throw failure(
                "CONFLICT",
                "Bookmarks changed. Review the deletion again.",
              );
            }
            const counts = await summary(command.guid);
            if (counts.revision !== command.expectedRevision) {
              throw failure(
                "CONFLICT",
                "Bookmarks changed. Review the deletion again.",
              );
            }
            await deps.bookmarks.remove(command.guid);
            return {};
          }
          default:
            throw failure("VALIDATION", "Unknown bookmark operation.");
        }
      });
    },
    subscribe(listener: (revision: number) => void): () => void {
      observe();
      listeners.add(listener);
      return () => listeners.delete(listener);
    },
    shutdown(): void {
      unobserve?.();
      unobserve = null;
      listeners.clear();
    },
  };
}

const { PlacesUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/PlacesUtils.sys.mjs",
) as {
  PlacesUtils: {
    bookmarks: BookmarkBackend;
    observers: {
      addListener(events: string[], listener: () => void): void;
      removeListener(events: string[], listener: () => void): void;
    };
  };
};
export const BookmarkService = createBookmarkService({
  bookmarks: PlacesUtils.bookmarks,
  managedGuids: () => SettingsService.get().speedDial,
  ensure: (options) => MigrationService.ensure(options),
  observe(listener) {
    PlacesUtils.observers.addListener(EVENTS, listener);
    return () => PlacesUtils.observers.removeListener(EVENTS, listener);
  },
});
