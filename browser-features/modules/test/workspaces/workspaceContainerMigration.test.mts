// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  acknowledgeWorkspaceMigrationRecords,
  buildWorkspaceMigrationSnapshot,
  canonicalWorkspaceMigrationRecord,
  type WorkspaceMigrationRecord,
} from "../../common/workspace-container-migration.ts";
import { updateWorkspaceSyncDocument } from "../../common/workspace-sync.ts";
import {
  workspaceMetadataRecord,
  workspaceTabRecord,
} from "../../common/workspace-sync-record.ts";
import {
  assert,
  assertEquals,
  assertThrows,
  runTests,
} from "../../../chrome/test/utils/test_harness.ts";

const workspaceId = "11111111-1111-4111-8111-111111111111";
const tabId = "22222222-2222-4222-8222-222222222222";
const deviceId = "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa";
const extraId = "33333333-3333-4333-8333-333333333333";
const containerId = "44444444-4444-4444-8444-444444444444";
const legacy = () =>
  updateWorkspaceSyncDocument(
    null,
    {
      workspaceId,
      name: "Work",
      icon: "briefcase",
      tabs: [{
        id: tabId,
        url: "https://example.test/",
        title: "Work",
        pinned: false,
      }],
    },
    deviceId,
    10,
  );
const records = () => {
  const doc = legacy();
  return [
    { id: workspaceId, entity: workspaceMetadataRecord(doc) },
    { id: tabId, entity: workspaceTabRecord(workspaceId, doc.tabs[0]) },
  ];
};

export async function runAllTests(): Promise<void> {
  await runTests("workspaceContainerMigration.test.mts", [
    {
      name:
        "legacy migration retains stable IDs without guessing container none",
      fn: () => {
        const migrated = buildWorkspaceMigrationSnapshot(records(), [], [], []);
        assertEquals(
          migrated.documents[0].workspaceId,
          workspaceId,
          "workspace identity",
        );
        assertEquals(migrated.documents[0].tabs[0].id, tabId, "tab identity");
        assertEquals(
          migrated.documents[0].defaultContainer?.kind,
          "legacy-unresolved",
          "unknown default",
        );
        assertEquals(
          migrated.documents[0].tabs[0].container?.kind,
          "legacy-unresolved",
          "unknown tab",
        );
      },
    },
    {
      name:
        "local observations give known container refs without losing newer legacy URLs",
      fn: () => {
        const local = updateWorkspaceSyncDocument(
          null,
          {
            version: 2,
            workspaceId,
            name: "Work",
            icon: "briefcase",
            defaultContainer: { kind: "container", containerId },
            tabs: [{
              id: tabId,
              url: "https://old.test/",
              title: "Old",
              pinned: false,
              container: { kind: "none" },
            }],
          },
          deviceId,
          5,
        );
        const migrated = buildWorkspaceMigrationSnapshot(
          records(),
          [local],
          [],
          [],
        );
        const doc = migrated.documents[0];
        assertEquals(
          doc.defaultContainer?.kind,
          "container",
          "known local default",
        );
        assertEquals(
          doc.tabs[0].container?.kind,
          "none",
          "explicit known none",
        );
        assertEquals(
          doc.tabs[0].url,
          "https://example.test/",
          "newer server URL preserved",
        );
      },
    },
    {
      name:
        "local unsent additions and remote tombstones both survive migration",
      fn: () => {
        const local = updateWorkspaceSyncDocument(
          legacy(),
          {
            workspaceId,
            name: "Work",
            icon: "briefcase",
            tabs: [
              {
                id: tabId,
                url: "https://example.test/new",
                title: "New",
                pinned: false,
              },
              {
                id: extraId,
                url: "https://extra.test/",
                title: "Extra",
                pinned: true,
              },
            ],
          },
          deviceId,
          50,
        );
        const source = [records()[0], { id: tabId, deleted: true as const }];
        const migrated = buildWorkspaceMigrationSnapshot(
          source,
          [local],
          [],
          [],
        );
        assertEquals(
          migrated.documents[0].tabs.length,
          1,
          "closed tab removed",
        );
        assertEquals(
          migrated.documents[0].tabs[0].id,
          extraId,
          "unsent addition retained",
        );
        assert(
          migrated.records.some((r) => r.id === tabId && r.deleted),
          "tombstone retained",
        );
      },
    },
    {
      name: "parent deletion suppresses local workspace and orphan children",
      fn: () => {
        const migrated = buildWorkspaceMigrationSnapshot(
          [{ id: workspaceId, deleted: true }, records()[1]],
          [legacy()],
          [],
          [],
        );
        assertEquals(migrated.documents.length, 0, "parent not revived");
        assertEquals(
          migrated.records.length,
          1,
          "only parent tombstone migrated",
        );
      },
    },
    {
      name: "orphan tabs stay staged without synthetic workspace metadata",
      fn: () => {
        const migrated = buildWorkspaceMigrationSnapshot(
          [records()[1]],
          [],
          [],
          [],
        );
        assertEquals(migrated.documents.length, 1, "orphan payload preserved");
        assertEquals(migrated.metadataKnown.length, 0, "no metadata invented");
        assertEquals(
          migrated.records.length,
          1,
          "no placeholder metadata upload",
        );
        assertEquals(migrated.records[0].id, tabId, "orphan record retained");
      },
    },
    {
      name:
        "metadata-known remains local and is not manufactured for local staged tabs",
      fn: () => {
        const migrated = buildWorkspaceMigrationSnapshot(
          [],
          [legacy()],
          [],
          [],
          [],
        );
        assertEquals(migrated.records.length, 1, "only tab emitted");
        assert(!Object.hasOwn(migrated, "selected"), "no selection transfer");
        assert(!Object.hasOwn(migrated, "excluded"), "no exclusion transfer");
      },
    },
    {
      name: "acknowledgement must match the exact staged entity",
      fn: () => {
        const stage = buildWorkspaceMigrationSnapshot(records(), [], [], []);
        const sent: WorkspaceMigrationRecord[] = structuredClone(stage.records);
        const tab = sent.find((r) => r.id === tabId);
        if (tab?.entity?.kind !== "tab") throw new Error("missing fixture tab");
        tab.entity.tab.title = "Different in-flight edit";
        const remaining = acknowledgeWorkspaceMigrationRecords(
          stage.records.map((r) => r.id),
          stage.records,
          sent,
        );
        assertEquals(
          JSON.stringify(remaining),
          JSON.stringify([tabId]),
          "newer payload is not initial manifest acknowledgement",
        );
        assertEquals(
          acknowledgeWorkspaceMigrationRecords(
            remaining,
            stage.records,
            stage.records,
          ).length,
          0,
          "exact retry acknowledged",
        );
      },
    },
    {
      name:
        "invalid identities, duplicate records and deleted-entity mixtures fail closed",
      fn: () => {
        assertThrows(
          () =>
            buildWorkspaceMigrationSnapshot(
              [...records(), records()[1]],
              [],
              [],
              [],
            ),
          "duplicate ID rejected",
        );
        assertThrows(
          () =>
            buildWorkspaceMigrationSnapshot(
              [{ ...records()[1], id: extraId }],
              [],
              [],
              [],
            ),
          "identity mismatch rejected",
        );
        assertThrows(
          () =>
            canonicalWorkspaceMigrationRecord({
              ...records()[0],
              deleted: true,
            }),
          "tombstone with live content rejected",
        );
      },
    },
  ]);
}
