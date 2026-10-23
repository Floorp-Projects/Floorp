// SPDX-License-Identifier: MPL-2.0

import type {
  BookmarkImportMapping,
  BookmarkImportPreview,
  BookmarkImportReviewOptions,
} from "./types.ts";

export class BookmarkImportReviewChangedError extends Error {
  constructor() {
    super(
      "Bookmark display settings changed while the import review was open.",
    );
    this.name = "BookmarkImportReviewChangedError";
  }
}

function workspaceSet(ids: readonly string[]): string {
  return JSON.stringify([...new Set(ids)].sort());
}

export async function reviewBookmarkImport(
  options: BookmarkImportReviewOptions,
): Promise<BookmarkImportPreview | null> {
  const initial = await options.service.previewImportSettings(
    options.raw,
    options.workspaceIds(),
  );
  if (!options.isOpen()) return null;
  let mapping: BookmarkImportMapping = {};
  if (initial.unresolvedFolders.length || initial.unresolvedWorkspaces.length) {
    const result = await options.map(initial);
    if (!result || !options.isOpen()) return null;
    mapping = result;
  }
  const knownIds = options.workspaceIds();
  // Mapping may target folders whose current rules were absent from the first
  // preview. Review their actual changes before asking for final approval.
  const reviewed = await options.service.previewImportSettings(
    initial.settings,
    knownIds,
    mapping,
  );
  if (!options.isOpen() || !await options.confirm(reviewed)) return null;
  if (!options.isOpen()) return null;
  if (
    options.service.getStatus().generation !== reviewed.generation ||
    workspaceSet(options.workspaceIds()) !== workspaceSet(knownIds)
  ) {
    throw new BookmarkImportReviewChangedError();
  }
  // The service also checks the generation inside its serialized mutation so a
  // write queued after this synchronous check cannot overwrite newer settings.
  return await options.service.applyImportSettings(reviewed, mapping);
}
