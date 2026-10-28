// SPDX-License-Identifier: MPL-2.0
import type { FolderView } from "../../../../../libs/newtab/types.ts";

export async function recoverFolder(
  previous: FolderView | null,
  missingGuid: string,
  groupGuid: string,
  load: (guid: string) => Promise<FolderView>,
  isCurrent: () => boolean,
): Promise<FolderView | null> {
  if (!groupGuid || missingGuid === groupGuid) return null;
  const path =
    previous?.guid === missingGuid && previous.groupGuid === groupGuid
      ? previous.ancestors
      : [];
  const boundary = path.findIndex((node) => node.guid === groupGuid);
  const ancestors = boundary < 0 ? [] : path.slice(boundary).reverse();
  const candidates = new Set([
    ...ancestors
      .filter((node) => node.guid !== missingGuid)
      .map((node) => node.guid),
    groupGuid,
  ]);
  for (const guid of candidates) {
    if (!isCurrent()) return null;
    try {
      const folder = await load(guid);
      if (!isCurrent()) return null;
      if (folder.groupGuid === groupGuid) return folder;
    } catch {
      // An ancestor may have been removed or moved while this view was open.
    }
  }
  return null;
}
