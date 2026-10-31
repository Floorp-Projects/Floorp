import { groups } from "./data.ts";
import type {
  DialEntry,
  DialFolder,
  FolderDestination,
  GroupId,
  PrototypeState,
} from "./types.ts";

export function getFolderPath(
  folders: DialFolder[],
  folderId: string | null,
): DialFolder[] {
  const path: DialFolder[] = [];
  const visited = new Set<string>();
  let id = folderId;
  while (id) {
    if (visited.has(id)) return [];
    visited.add(id);
    const folder = folders.find((item) => item.id === id);
    if (!folder) return [];
    path.unshift(folder);
    id = folder.parentId;
  }
  return path;
}

export function getFolderDescendantIds(
  folders: DialFolder[],
  folderId: string,
): Set<string> {
  const result = new Set([folderId]);
  const queue = [folderId];
  for (let index = 0; index < queue.length; index++) {
    for (const folder of folders) {
      if (folder.parentId === queue[index] && !result.has(folder.id)) {
        result.add(folder.id);
        queue.push(folder.id);
      }
    }
  }
  return result;
}

export function getDialEntries(
  data: PrototypeState,
  groupId: GroupId,
  folderId: string | null,
): DialEntry[] {
  return [
    ...data.folders
      .filter(
        (folder) => folder.groupId === groupId && folder.parentId === folderId,
      )
      .map(
        (folder): DialEntry => ({
          type: "folder",
          id: folder.id,
          folder,
          order: folder.order,
        }),
      ),
    ...data.sites
      .filter(
        (site) =>
          site.groupId === groupId && (site.folderId ?? null) === folderId,
      )
      .map(
        (site, index): DialEntry => ({
          type: "site",
          id: site.id,
          site,
          order: site.order ?? index,
        }),
      ),
  ].sort((a, b) => a.order - b.order);
}

export function nextDialOrder(
  data: PrototypeState,
  groupId: GroupId,
  folderId: string | null,
): number {
  return (
    Math.max(
      -1,
      ...getDialEntries(data, groupId, folderId).map((entry) => entry.order),
    ) + 1
  );
}

export function destinationValue(
  groupId: GroupId,
  folderId: string | null,
): string {
  return folderId ? `folder:${folderId}` : `group:${groupId}`;
}

export function folderDestinationOptions(
  data: PrototypeState,
  excludeFolderId?: string,
): FolderDestination[] {
  const excluded = excludeFolderId
    ? getFolderDescendantIds(data.folders, excludeFolderId)
    : new Set<string>();
  return groups.flatMap((group) => {
    const root: FolderDestination = {
      value: destinationValue(group.id, null),
      label: group.title,
      groupId: group.id,
      folderId: null,
    };
    const children = data.folders
      .filter(
        (folder) => folder.groupId === group.id && !excluded.has(folder.id),
      )
      .map(
        (folder): FolderDestination => ({
          value: destinationValue(group.id, folder.id),
          label: `${group.title} / ${getFolderPath(data.folders, folder.id)
            .map((item) => item.title)
            .join(" / ")}`,
          groupId: group.id,
          folderId: folder.id,
        }),
      )
      .sort((a, b) => a.label.localeCompare(b.label, "ja"));
    return [root, ...children];
  });
}

export function resolveDestination(
  data: PrototypeState,
  value: string,
): FolderDestination {
  const destination = folderDestinationOptions(data).find(
    (item) => item.value === value,
  );
  if (!destination) throw new Error("保存先のフォルダーが見つかりません。");
  return destination;
}

export function moveDialEntry(
  data: PrototypeState,
  type: DialEntry["type"],
  id: string,
  groupId: GroupId,
  folderId: string | null,
): PrototypeState {
  resolveDestination(data, destinationValue(groupId, folderId));
  const order = nextDialOrder(data, groupId, folderId);
  if (type === "site") {
    const site = data.sites.find((item) => item.id === id);
    if (!site) throw new Error("移動するサイトが見つかりません。");
    if (site.groupId === groupId && (site.folderId ?? null) === folderId)
      return data;
    return {
      ...data,
      sites: data.sites.map((item) =>
        item.id === id ? { ...item, groupId, folderId, order } : item,
      ),
    };
  }
  const folder = data.folders.find((item) => item.id === id);
  if (!folder) throw new Error("移動するフォルダーが見つかりません。");
  const descendants = getFolderDescendantIds(data.folders, id);
  if (folderId && descendants.has(folderId))
    throw new Error("フォルダーを自分自身やその中には移動できません。");
  if (folder.groupId === groupId && folder.parentId === folderId) return data;
  return {
    ...data,
    folders: data.folders.map((item) =>
      item.id === id
        ? { ...item, groupId, parentId: folderId, order }
        : descendants.has(item.id)
          ? { ...item, groupId }
          : item,
    ),
    sites: data.sites.map((site) =>
      site.folderId && descendants.has(site.folderId)
        ? { ...site, groupId }
        : site,
    ),
  };
}

export function reorderDialEntry(
  data: PrototypeState,
  groupId: GroupId,
  folderId: string | null,
  id: string,
  targetId: string,
): PrototypeState {
  const entries = getDialEntries(data, groupId, folderId);
  const from = entries.findIndex((entry) => entry.id === id);
  const to = entries.findIndex((entry) => entry.id === targetId);
  if (from < 0 || to < 0 || from === to) return data;
  const [selected] = entries.splice(from, 1);
  entries.splice(to, 0, selected);
  const positions = new Map(entries.map((entry, index) => [entry.id, index]));
  return {
    ...data,
    folders: data.folders.map((folder) =>
      positions.has(folder.id)
        ? { ...folder, order: positions.get(folder.id)! }
        : folder,
    ),
    sites: data.sites.map((site) =>
      positions.has(site.id)
        ? { ...site, order: positions.get(site.id)! }
        : site,
    ),
  };
}

export function removeDialFolder(
  data: PrototypeState,
  id: string,
): PrototypeState {
  const descendants = getFolderDescendantIds(data.folders, id);
  return {
    ...data,
    folders: data.folders.filter((folder) => !descendants.has(folder.id)),
    sites: data.sites.filter(
      (site) => !site.folderId || !descendants.has(site.folderId),
    ),
  };
}
