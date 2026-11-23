// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { updateWorkspaceSyncDocument } from "../../common/workspace-sync.ts";
import {
  parseWorkspaceSyncRecord,
  WORKSPACE_SYNC_RECORD_MAX_BYTES,
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
const device = "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa";
const tabId = "22222222-2222-4222-8222-222222222222";
const document = () =>
  updateWorkspaceSyncDocument(
    null,
    {
      workspaceId,
      name: "Research",
      icon: "book",
      tabs: [{
        id: tabId,
        url: "https://example.com/",
        title: "Example",
        pinned: false,
      }],
    },
    device,
    10,
  );

export async function runAllTests(): Promise<void> {
  await runTests("workspaceSyncRecord.test.mts", [
    {
      name: "metadata and each live tab are independent wire records",
      fn: () => {
        const source = document();
        const metadata = workspaceMetadataRecord(source);
        const tab = workspaceTabRecord(workspaceId, source.tabs[0]);
        assertEquals(metadata.kind, "workspace", "metadata discriminant");
        assertEquals(metadata.workspaceId, workspaceId, "workspace identity");
        assertEquals(metadata.name, "Research", "name retained");
        assertEquals(
          metadata.metadata.modifiedAt,
          10,
          "Unix millisecond clock",
        );
        assert(
          !Object.hasOwn(metadata, "tabs"),
          "no tab aggregate on the wire",
        );
        assertEquals(tab.kind, "tab", "tab discriminant");
        assertEquals(tab.tab.id, tabId, "tab identity");
        assertEquals(tab.tab.deleted, false, "live tab only");
        assert(!Object.hasOwn(tab, "name"), "metadata is stored separately");
        for (const record of [metadata, tab]) {
          assertEquals(
            JSON.stringify(
              parseWorkspaceSyncRecord(JSON.parse(JSON.stringify(record))),
            ),
            JSON.stringify(record),
            "wire round trip is canonical",
          );
        }
      },
    },
    {
      name: "unknown fields and session state never enter a wire record",
      fn: () => {
        const source = document();
        const parsed = parseWorkspaceSyncRecord({
          ...workspaceTabRecord(workspaceId, source.tabs[0]),
          history: ["secret"],
          tab: { ...source.tabs[0], formdata: "secret" },
        });
        assert(
          !JSON.stringify(parsed).includes("secret"),
          "session state omitted",
        );
        const metadata = parseWorkspaceSyncRecord({
          ...workspaceMetadataRecord(source),
          tabs: source.tabs,
        });
        assert(!Object.hasOwn(metadata, "tabs"), "aggregate fields dropped");
        assertThrows(
          () => parseWorkspaceSyncRecord(source),
          "an aggregate document cannot be sent as a record",
        );
      },
    },
    {
      name: "deleted tabs require standard Sync tombstones",
      fn: () => {
        const source = document();
        assertThrows(
          () =>
            workspaceTabRecord(workspaceId, {
              ...source.tabs[0],
              deleted: true,
            }),
          "deleted tab content is never uploaded as a live record",
        );
      },
    },
    {
      name:
        "each wire record has a UTF-8 payload budget with encryption headroom",
      fn: () => {
        const source = document();
        const record = workspaceTabRecord(workspaceId, source.tabs[0]);
        record.tab.title = "x".repeat(1024);
        record.tab.url = `https://example.com/${"あ".repeat(5000)}`;
        const bytes = new TextEncoder().encode(JSON.stringify(record)).length;
        record.tab.url += "x".repeat(WORKSPACE_SYNC_RECORD_MAX_BYTES - bytes);
        assertEquals(
          new TextEncoder().encode(JSON.stringify(record)).length,
          WORKSPACE_SYNC_RECORD_MAX_BYTES,
          "fixture reaches the exact UTF-8 boundary within field limits",
        );
        parseWorkspaceSyncRecord(record);
        assertThrows(
          () =>
            parseWorkspaceSyncRecord({
              ...record,
              tab: { ...record.tab, url: `${record.tab.url}x` },
            }),
          "one extra byte exceeds the payload budget",
        );
      },
    },
    {
      name: "wire strings retain valid Unicode and reject isolated surrogates",
      fn: () => {
        const source = document();
        source.name = "調査 📚";
        source.icon = "📚";
        source.tabs[0].title = "閲覧 🧭";
        const metadata = workspaceMetadataRecord(source);
        const record = workspaceTabRecord(workspaceId, source.tabs[0]);
        assertEquals(metadata.name, source.name, "Unicode workspace name");
        assertEquals(metadata.icon, source.icon, "emoji workspace icon");
        assertEquals(
          record.tab.title,
          source.tabs[0].title,
          "Unicode tab title",
        );
        for (const invalid of ["\ud83d", "\udc00"]) {
          for (
            const value of [
              { ...metadata, name: invalid },
              { ...metadata, icon: invalid },
              { ...record, tab: { ...record.tab, title: invalid } },
              {
                ...record,
                tab: { ...record.tab, url: `https://example.com/${invalid}` },
              },
            ]
          ) {
            assertThrows(
              () => parseWorkspaceSyncRecord(value),
              "invalid Unicode rejected",
            );
          }
        }
        const truncated = `${"x".repeat(1023)}📚`.slice(0, 1024);
        assertThrows(
          () =>
            workspaceTabRecord(workspaceId, {
              ...record.tab,
              title: truncated,
            }),
          "a title cut through an emoji is rejected",
        );
        workspaceTabRecord(workspaceId, {
          ...record.tab,
          title: truncated.toWellFormed(),
        });
      },
    },
    {
      name: "untrusted schemas IDs and privileged URLs are rejected",
      fn: () => {
        const source = document();
        const record = workspaceTabRecord(workspaceId, source.tabs[0]);
        for (
          const invalid of [
            { ...record, version: 2 },
            { ...record, kind: "session" },
            { ...record, workspaceId: "invalid" },
            { ...record, tab: { ...record.tab, id: "invalid" } },
            { ...record, tab: { ...record.tab, id: workspaceId } },
            { ...record, tab: { ...record.tab, modifiedAt: -1 } },
            { ...record, tab: { ...record.tab, modifiedBy: "invalid" } },
            { ...record, tab: { ...record.tab, url: "about:config" } },
            { ...record, tab: { ...record.tab, url: "javascript:alert(1)" } },
            {
              ...record,
              tab: { ...record.tab, url: "https://user:pass@example.com" },
            },
          ]
        ) {
          assertThrows(
            () => parseWorkspaceSyncRecord(invalid),
            "invalid record rejected",
          );
        }
      },
    },
  ]);
}
