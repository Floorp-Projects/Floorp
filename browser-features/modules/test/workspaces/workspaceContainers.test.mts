// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  mergeWorkspaceContainerDefinitions,
  parseWorkspaceContainerDefinition,
  parseWorkspaceContainerRef,
  sameWorkspaceContainerRef,
} from "../../common/workspace-container.ts";
import type {
  WorkspaceContainerDefinition,
  WorkspaceContainerRef,
} from "../../common/workspace-container-types.ts";
import {
  mergeWorkspaceSyncDocuments,
  migrateWorkspaceSyncDocumentToV2,
  parseWorkspaceSyncDocument,
  updateWorkspaceSyncDocument,
} from "../../common/workspace-sync.ts";
import {
  parseWorkspaceSyncRecord,
  WORKSPACE_SYNC_RECORD_MAX_BYTES,
  workspaceContainerRecord,
  workspaceMetadataRecord,
  workspaceTabRecord,
} from "../../common/workspace-sync-record.ts";
import type { WorkspaceSyncSnapshot } from "../../common/workspace-sync-types.ts";
import {
  assert,
  assertEquals,
  assertThrows,
  runTests,
} from "../../../chrome/test/utils/test_harness.ts";

const workspaceId = "11111111-1111-4111-8111-111111111111";
const tabA = "22222222-2222-4222-8222-222222222222";
const tabB = "33333333-3333-4333-8333-333333333333";
const tabC = "44444444-4444-4444-8444-444444444444";
const containerId = "55555555-5555-4555-8555-555555555555";
const deviceA = "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa";
const deviceB = "bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb";
const work: WorkspaceContainerRef = { kind: "container", containerId };
const none: WorkspaceContainerRef = { kind: "none" };

function definition(): WorkspaceContainerDefinition {
  return {
    version: 2,
    kind: "container",
    containerId,
    name: "仕事 📚",
    color: "blue",
    icon: "briefcase",
    modifiedAt: 10,
    modifiedBy: deviceA,
  };
}

function snapshot(ids = [tabA, tabB]): WorkspaceSyncSnapshot {
  return {
    version: 2,
    workspaceId,
    name: "Development",
    icon: "code",
    defaultContainer: work,
    tabs: ids.map((id) => ({
      id,
      url: `https://example.com/${id}`,
      title: id,
      pinned: false,
      container: id === tabB ? none : work,
    })),
  };
}

const document = () =>
  updateWorkspaceSyncDocument(null, snapshot(), deviceA, 10);

export async function runAllTests(): Promise<void> {
  await runTests("workspaceContainers.test.mts", [
    {
      name: "v2 records retain workspace and individual tab containers",
      fn: () => {
        const source = document();
        const metadata = workspaceMetadataRecord(source);
        assertEquals(metadata.version, 2, "v2 metadata");
        assert(
          sameWorkspaceContainerRef(metadata.defaultContainer, work),
          "workspace default retained",
        );
        const workTab = workspaceTabRecord(workspaceId, source.tabs[0], 2);
        const plainTab = workspaceTabRecord(workspaceId, source.tabs[1], 2);
        assertEquals(workTab.tab.container?.kind, "container", "work tab");
        assertEquals(plainTab.tab.container?.kind, "none", "explicit none");
        for (
          const record of [
            metadata,
            workTab,
            plainTab,
            workspaceContainerRecord(definition()),
          ]
        ) {
          assertEquals(
            JSON.stringify(parseWorkspaceSyncRecord(record, 2)),
            JSON.stringify(record),
            "canonical round trip",
          );
          assertThrows(
            () => parseWorkspaceSyncRecord(record, 1),
            "v2 cannot enter legacy transport",
          );
        }
      },
    },
    {
      name: "v2 requires explicit references and rejects ambiguous local IDs",
      fn: () => {
        for (
          const ref of [
            undefined,
            null,
            3,
            {},
            { kind: "container", containerId: 3 },
            { kind: "container", containerId: "invalid" },
            { kind: "none", containerId },
            { kind: "container", containerId, userContextId: 7 },
            { kind: "default" },
          ]
        ) {
          assertThrows(
            () => parseWorkspaceContainerRef(ref),
            "not an explicit portable reference",
          );
        }
        const source = document();
        assertThrows(
          () =>
            parseWorkspaceSyncDocument({
              ...source,
              defaultContainer: undefined,
            }),
          "missing default is not none",
        );
        assertThrows(
          () =>
            parseWorkspaceSyncDocument({
              ...source,
              tabs: [{ ...source.tabs[0], container: undefined }],
            }),
          "missing tab reference is not workspace default",
        );
        assertThrows(
          () => workspaceTabRecord(workspaceId, source.tabs[0], 1),
          "no downcast that silently loses a reference",
        );
        for (const collision of [workspaceId, tabA, tabB]) {
          assertThrows(
            () =>
              parseWorkspaceSyncDocument({
                ...source,
                defaultContainer: { kind: "container", containerId: collision },
              }),
            "container records cannot alias workspace or tab record IDs",
          );
        }
      },
    },
    {
      name: "legacy import preserves identity and never guesses container zero",
      fn: () => {
        const old = updateWorkspaceSyncDocument(
          null,
          {
            workspaceId,
            name: "Old",
            icon: null,
            tabs: [{
              id: tabA,
              url: "https://example.com/",
              title: "Old",
              pinned: true,
            }],
          },
          deviceA,
          9,
        );
        const migrated = migrateWorkspaceSyncDocumentToV2(old);
        assertEquals(migrated.version, 2, "upgraded payload");
        assertEquals(migrated.workspaceId, workspaceId, "same workspace");
        assertEquals(migrated.tabs[0].id, tabA, "same tab");
        assertEquals(
          migrated.tabs[0].modifiedAt,
          9,
          "migration is not a new edit",
        );
        assertEquals(
          migrated.defaultContainer?.kind,
          "legacy-unresolved",
          "unknown default",
        );
        assertEquals(
          migrated.tabs[0].container?.kind,
          "legacy-unresolved",
          "unknown tab",
        );
        const observed = migrateWorkspaceSyncDocumentToV2(old, {
          defaultContainer: work,
          tabs: { [tabA]: none },
        });
        assertEquals(
          observed.tabs[0].container?.kind,
          "none",
          "actual observed none",
        );
        assertEquals(
          JSON.stringify(migrateWorkspaceSyncDocumentToV2(observed)),
          JSON.stringify(observed),
          "migration is idempotent",
        );
        assertThrows(
          () => mergeWorkspaceSyncDocuments(old, migrated),
          "cross-version merge requires explicit import",
        );
        assertThrows(
          () => parseWorkspaceSyncRecord(workspaceMetadataRecord(old), 2),
          "legacy wire cannot enter v2 engine",
        );
      },
    },
    {
      name:
        "workspace default changes do not rewrite individual tab references",
      fn: () => {
        const initial = document();
        const changed = updateWorkspaceSyncDocument(
          initial,
          {
            ...snapshot(),
            defaultContainer: none,
          },
          deviceB,
          20,
        );
        assertEquals(changed.metadata.modifiedAt, 20, "default changed clock");
        assertEquals(changed.defaultContainer?.kind, "none", "new default");
        assertEquals(changed.tabs[0].modifiedAt, 10, "work tab unchanged");
        assertEquals(
          changed.tabs[0].container?.kind,
          "container",
          "work tab retained",
        );
        assertEquals(
          changed.tabs[1].container?.kind,
          "none",
          "explicit none retained",
        );
      },
    },
    {
      name:
        "container-only tab edits are merged without losing concurrent tabs",
      fn: () => {
        const initial = document();
        const local = snapshot();
        local.tabs[0].container = none;
        const changed = updateWorkspaceSyncDocument(
          initial,
          local,
          deviceA,
          20,
        );
        const added = updateWorkspaceSyncDocument(
          initial,
          snapshot([tabA, tabB, tabC]),
          deviceB,
          20,
        );
        const merged = mergeWorkspaceSyncDocuments(changed, added);
        assertEquals(merged.tabs.length, 3, "concurrent addition retained");
        assertEquals(
          merged.tabs.find((tab) => tab.id === tabA)?.container?.kind,
          "none",
          "new reference retained",
        );
        assertEquals(
          JSON.stringify(merged),
          JSON.stringify(mergeWorkspaceSyncDocuments(added, changed)),
          "reference merge commutes",
        );
      },
    },
    {
      name:
        "protected missing tabs retain desired state and do not shift observed positions",
      fn: () => {
        const initial = updateWorkspaceSyncDocument(
          null,
          snapshot([tabA, tabB, tabC]),
          deviceA,
          10,
        );
        const next = updateWorkspaceSyncDocument(
          initial,
          {
            ...snapshot([tabC]),
            protectedTabIds: [tabA],
          },
          deviceA,
          20,
        );
        const protectedTab = next.tabs.find((tab) => tab.id === tabA);
        assertEquals(
          JSON.stringify(protectedTab),
          JSON.stringify(initial.tabs[0]),
          "protected payload unchanged",
        );
        assertEquals(
          next.tabs.find((tab) => tab.id === tabB)?.deleted,
          true,
          "ordinary missing tab closes",
        );
        assertEquals(
          next.tabs.find((tab) => tab.id === tabC)?.position,
          2,
          "missing slot is not a reorder",
        );
        assertEquals(
          next.tabs.find((tab) => tab.id === tabC)?.modifiedAt,
          10,
          "unchanged visible tab not restamped",
        );
        const wire = JSON.stringify(workspaceMetadataRecord(next));
        assert(
          !wire.includes("protectedTabIds"),
          "local protection never enters wire",
        );
      },
    },
    {
      name:
        "remote deletion wins over local close protection and later materialization",
      fn: () => {
        const initial = document();
        const protectedLocal = updateWorkspaceSyncDocument(
          initial,
          {
            ...snapshot([tabB]),
            protectedTabIds: [tabA],
          },
          deviceA,
          20,
        );
        const remote = updateWorkspaceSyncDocument(
          initial,
          snapshot([tabB]),
          deviceB,
          30,
        );
        const merged = mergeWorkspaceSyncDocuments(protectedLocal, remote);
        assertEquals(
          merged.tabs.find((tab) => tab.id === tabA)?.deleted,
          true,
          "remote tombstone wins",
        );
        const restored = updateWorkspaceSyncDocument(
          merged,
          {
            ...snapshot(),
            protectedTabIds: [tabA],
          },
          deviceA,
          40,
        );
        assertEquals(
          restored.tabs.find((tab) => tab.id === tabA)?.deleted,
          true,
          "stale materialization cannot revive retired ID",
        );
      },
    },
    {
      name: "unchanged v2 captures do not republish metadata or tab references",
      fn: () => {
        const initial = document();
        const next = updateWorkspaceSyncDocument(
          initial,
          snapshot(),
          deviceA,
          20,
        );
        assertEquals(
          JSON.stringify(next),
          JSON.stringify(initial),
          "capture idempotent",
        );
        assertThrows(
          () =>
            updateWorkspaceSyncDocument(
              initial,
              { ...snapshot(), protectedTabIds: ["invalid"] },
              deviceA,
              20,
            ),
          "malformed protection cannot mask closures",
        );
      },
    },
    {
      name: "container definitions exclude local identity and session fields",
      fn: () => {
        const parsed = parseWorkspaceContainerDefinition({
          ...definition(),
          userContextId: 7,
          cookies: "private-session",
          localStorage: "private-session",
        });
        assertEquals(
          JSON.stringify(parsed),
          JSON.stringify(definition()),
          "only portable creation fields",
        );
        assert(
          !JSON.stringify(parsed).includes("private-session"),
          "no session data",
        );
        assertThrows(
          () => parseWorkspaceSyncRecord({ ...definition(), version: 1 }),
          "legacy does not support container record kind",
        );
        assertThrows(
          () => parseWorkspaceSyncRecord({ ...definition(), version: 3 }),
          "future versions rejected",
        );
      },
    },
    {
      name:
        "container definition validation rejects unsafe or malformed creation metadata",
      fn: () => {
        for (
          const invalid of [
            { ...definition(), containerId: "invalid" },
            { ...definition(), name: "x".repeat(257) },
            { ...definition(), name: "\ud800" },
            { ...definition(), color: "url(https://example.com/)" },
            { ...definition(), icon: "https://example.com/icon.svg" },
            { ...definition(), icon: "../icon" },
            { ...definition(), modifiedAt: -1 },
            { ...definition(), modifiedAt: Number.MAX_SAFE_INTEGER + 1 },
            { ...definition(), modifiedBy: "invalid" },
          ]
        ) {
          assertThrows(
            () => parseWorkspaceContainerDefinition(invalid),
            "invalid definition rejected",
          );
        }
        parseWorkspaceContainerDefinition({
          ...definition(),
          name: "x".repeat(256),
        });
        assertEquals(
          parseWorkspaceContainerDefinition({
            ...definition(),
            icon: "future-icon",
          }).icon,
          "future-icon",
          "safe unknown identifiers remain distinguishable for local support checks",
        );
      },
    },
    {
      name:
        "definition conflict resolution is canonical and does not alias IDs by name",
      fn: () => {
        const a = definition();
        const b = { ...a, name: "Other" };
        assertEquals(
          JSON.stringify(mergeWorkspaceContainerDefinitions(a, b)),
          JSON.stringify(mergeWorkspaceContainerDefinitions(b, a)),
          "equal-clock conflict commutes",
        );
        const newer = { ...a, name: "Newer", modifiedAt: 11 };
        assertEquals(
          mergeWorkspaceContainerDefinitions(a, newer).name,
          "Newer",
          "newer clock",
        );
        assertThrows(
          () =>
            mergeWorkspaceContainerDefinitions(a, { ...a, containerId: tabC }),
          "same name is not same wire identity",
        );
      },
    },
    {
      name:
        "v2 records keep the 16 KiB wire ceiling including container references",
      fn: () => {
        const source = document();
        const record = workspaceTabRecord(workspaceId, source.tabs[0], 2);
        record.tab.title = "x".repeat(1024);
        record.tab.url = `https://example.com/${"あ".repeat(4500)}`;
        const bytes = new TextEncoder().encode(JSON.stringify(record)).length;
        record.tab.url += "x".repeat(WORKSPACE_SYNC_RECORD_MAX_BYTES - bytes);
        assertEquals(
          new TextEncoder().encode(JSON.stringify(record)).length,
          WORKSPACE_SYNC_RECORD_MAX_BYTES,
          "exact UTF-8 boundary",
        );
        parseWorkspaceSyncRecord(record, 2);
        assertThrows(
          () =>
            parseWorkspaceSyncRecord({
              ...record,
              tab: { ...record.tab, url: `${record.tab.url}x` },
            }, 2),
          "one byte beyond ceiling rejected",
        );
      },
    },
  ]);
}
