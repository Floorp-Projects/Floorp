// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";
import {
  BookmarkImportReviewChangedError,
  reviewBookmarkImport,
} from "../import-review.ts";
import type {
  BookmarkImportPreview,
  BookmarkImportReviewOptions,
} from "../types.ts";

function preview(generation = 1): BookmarkImportPreview {
  return {
    generation,
    knownWorkspaceIds: ["destination-workspace"],
    settings: {},
    applicableRules: [],
    unresolvedFolders: [],
    unresolvedWorkspaces: [],
    changes: [],
    sourceMatches: true,
  };
}

async function testMappedChangesAreReviewedBeforeCommit(): Promise<void> {
  const initial = {
    ...preview(),
    unresolvedFolders: ["old-folder"],
    unresolvedWorkspaces: ["old-workspace"],
  };
  const mapped = {
    ...preview(2),
    changes: [{
      guid: "new-folder",
      title: "Mapped folder",
      before: null,
      after: ["destination-workspace"],
    }],
  };
  const order: string[] = [];
  let calls = 0;
  const result = await reviewBookmarkImport({
    raw: {},
    workspaceIds: () => ["destination-workspace"],
    isOpen: () => true,
    service: {
      getStatus: () => ({ generation: 2 }),
      previewImportSettings: (_raw, _ids, mapping) => {
        calls++;
        order.push(`preview-${calls}`);
        if (calls === 1) return Promise.resolve(initial);
        assertEquals(
          mapping?.folders?.["old-folder"],
          "new-folder",
          "mapped folder is included in final preview",
        );
        assertEquals(
          mapping?.workspaces?.["old-workspace"],
          "destination-workspace",
          "mapped workspace is included in final preview",
        );
        return Promise.resolve(mapped);
      },
      applyImportSettings: (reviewed) => {
        assertEquals(
          reviewed,
          mapped,
          "commit uses the final reviewed preview",
        );
        order.push("apply");
        return Promise.resolve(reviewed);
      },
    },
    map: (unresolved) => {
      assertEquals(unresolved, initial, "mapping uses the unresolved preview");
      order.push("map");
      return Promise.resolve({
        folders: { "old-folder": "new-folder" },
        workspaces: { "old-workspace": "destination-workspace" },
      });
    },
    confirm: (reviewed) => {
      assertEquals(reviewed, mapped, "final review shows mapped destinations");
      order.push("confirm");
      return Promise.resolve(true);
    },
  });
  assertEquals(result, mapped, "successful review returns the commit result");
  assertEquals(
    order.join(","),
    "preview-1,map,preview-2,confirm,apply",
    "no write occurs before the mapped changes are confirmed",
  );
}

async function testStaleFinalReviewsCannotCommit(): Promise<void> {
  for (const change of ["generation", "workspaces"] as const) {
    let generation = 1;
    let known = ["destination-workspace"];
    let writes = 0;
    let rejected = false;
    try {
      await reviewBookmarkImport({
        raw: {},
        workspaceIds: () => known,
        isOpen: () => true,
        service: {
          getStatus: () => ({ generation }),
          previewImportSettings: () => Promise.resolve(preview()),
          applyImportSettings: (reviewed) => {
            writes++;
            return Promise.resolve(reviewed);
          },
        },
        map: () => Promise.resolve({}),
        confirm: () => {
          if (change === "generation") generation++;
          else known = ["replacement-workspace"];
          return Promise.resolve(true);
        },
      });
    } catch (error) {
      rejected = error instanceof BookmarkImportReviewChangedError;
    }
    assert(rejected, `${change} change invalidates the reviewed settings`);
    assertEquals(writes, 0, `${change} change prevents all writes`);
  }
}

async function testCancellationAndClosedWindowsCannotCommit(): Promise<void> {
  for (const cancelAt of ["mapping", "review", "closed"] as const) {
    let writes = 0;
    let open = true;
    const options: BookmarkImportReviewOptions = {
      raw: {},
      workspaceIds: () => ["destination-workspace"],
      isOpen: () => open,
      service: {
        getStatus: () => ({ generation: 1 }),
        previewImportSettings: () =>
          Promise.resolve({
            ...preview(),
            unresolvedFolders: ["old-folder"],
          }),
        applyImportSettings: (reviewed) => {
          writes++;
          return Promise.resolve(reviewed);
        },
      },
      map: () => Promise.resolve(cancelAt === "mapping" ? null : {}),
      confirm: () => {
        if (cancelAt === "closed") open = false;
        return Promise.resolve(cancelAt !== "review");
      },
    };
    assertEquals(
      await reviewBookmarkImport(options),
      null,
      `${cancelAt} cancels the operation`,
    );
    assertEquals(writes, 0, `${cancelAt} does not write any settings`);
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("importReview.test.ts", [
    {
      name: "mapped destinations are reviewed before any import write",
      fn: testMappedChangesAreReviewedBeforeCommit,
    },
    {
      name: "concurrent settings and workspace changes reject the final review",
      fn: testStaleFinalReviewsCannotCommit,
    },
    {
      name: "cancellation and closed windows leave import settings unchanged",
      fn: testCancellationAndClosedWindowsCannotCommit,
    },
  ]);
}
