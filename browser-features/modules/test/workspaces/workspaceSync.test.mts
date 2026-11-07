// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  isSyncableWorkspaceURL,
  liveWorkspaceSyncTabs,
  mergeWorkspaceSyncDocuments,
  parseWorkspaceSyncDocument,
  updateWorkspaceSyncDocument,
} from "../../common/workspace-sync.ts";
import type { WorkspaceSyncSnapshot } from "../../common/workspace-sync-types.ts";
import {
  assert,
  assertEquals,
  assertThrows,
  runTests,
} from "../../../chrome/test/utils/test_harness.ts";

const workspaceId = "11111111-1111-4111-8111-111111111111";
const deviceA = "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa";
const deviceB = "bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb";
const tabA = "22222222-2222-4222-8222-222222222222";
const tabB = "33333333-3333-4333-8333-333333333333";
const snapshot = (ids: string[]): WorkspaceSyncSnapshot => ({
  workspaceId,
  name: "Research",
  icon: "book",
  tabs: ids.map((id) => ({
    id,
    url: `https://example.com/${id}`,
    title: id,
    pinned: false,
  })),
});

export async function runAllTests(): Promise<void> {
  await runTests("workspaceSync.test.mts", [
    {
      name: "empty workspaces retain metadata",
      fn: () => {
        const result = updateWorkspaceSyncDocument(
          null,
          snapshot([]),
          deviceA,
          10,
        );
        assertEquals(result.tabs.length, 0, "empty workspace syncs");
        assertEquals(result.name, "Research", "name syncs");
        assertEquals(result.metadata.modifiedAt, 10, "clock stored");
      },
    },
    {
      name: "unchanged captures are idempotent",
      fn: () => {
        const initial = updateWorkspaceSyncDocument(
          null,
          snapshot([tabA]),
          deviceA,
          10,
        );
        const next = updateWorkspaceSyncDocument(
          initial,
          snapshot([tabA]),
          deviceA,
          20,
        );
        assertEquals(
          JSON.stringify(next),
          JSON.stringify(initial),
          "unchanged snapshots do not re-upload",
        );
      },
    },
    {
      name: "concurrent additions are merged in both directions",
      fn: () => {
        const a = updateWorkspaceSyncDocument(
          null,
          snapshot([tabA]),
          deviceA,
          10,
        );
        const b = updateWorkspaceSyncDocument(
          null,
          snapshot([tabB]),
          deviceB,
          10,
        );
        const merged = mergeWorkspaceSyncDocuments(a, b);
        assertEquals(
          liveWorkspaceSyncTabs(merged).length,
          2,
          "both tabs retained",
        );
        assertEquals(
          JSON.stringify(merged),
          JSON.stringify(mergeWorkspaceSyncDocuments(b, a)),
          "merge commutes",
        );
        assertEquals(
          JSON.stringify(merged),
          JSON.stringify(mergeWorkspaceSyncDocuments(merged, merged)),
          "merge is idempotent",
        );
      },
    },
    {
      name: "offline stale records do not reopen closed tabs",
      fn: () => {
        const initial = updateWorkspaceSyncDocument(
          null,
          snapshot([tabA, tabB]),
          deviceA,
          10,
        );
        const closed = updateWorkspaceSyncDocument(
          initial,
          snapshot([tabB]),
          deviceA,
          20,
        );
        const merged = mergeWorkspaceSyncDocuments(closed, initial);
        assertEquals(
          liveWorkspaceSyncTabs(merged).length,
          1,
          "closed tab stays closed",
        );
        assert(
          merged.tabs.find((tab) => tab.id === tabA)?.deleted,
          "close marker retained",
        );
      },
    },
    {
      name: "offline insertion and reordering cannot revive a closed tab",
      fn: () => {
        const tabC = "44444444-4444-4444-8444-444444444444";
        const initial = updateWorkspaceSyncDocument(
          null,
          snapshot([tabA, tabB]),
          deviceA,
          10,
        );
        const closed = updateWorkspaceSyncDocument(
          initial,
          snapshot([tabB]),
          deviceA,
          20,
        );
        const offline = updateWorkspaceSyncDocument(
          initial,
          snapshot([tabC, tabA, tabB]),
          deviceB,
          30,
        );
        for (
          const merged of [
            mergeWorkspaceSyncDocuments(closed, offline),
            mergeWorkspaceSyncDocuments(offline, closed),
          ]
        ) {
          assert(
            !liveWorkspaceSyncTabs(merged).some((tab) => tab.id === tabA),
            "a later ordering-only stamp cannot override deletion",
          );
          const captured = updateWorkspaceSyncDocument(
            merged,
            snapshot([tabC, tabA, tabB]),
            deviceB,
            40,
          );
          assert(
            captured.tabs.find((tab) => tab.id === tabA)?.deleted,
            "reopening requires a fresh tab UUID",
          );
        }
      },
    },
    {
      name: "closed tab history does not impose a lifetime transport limit",
      fn: () => {
        const initial = updateWorkspaceSyncDocument(
          null,
          snapshot([tabA]),
          deviceA,
          10,
        );
        const tombstones = Array.from({ length: 1001 }, (_, index) => ({
          ...initial.tabs[0],
          id: `44444444-4444-4444-8444-${String(index).padStart(12, "0")}`,
          deleted: true,
        }));
        const parsed = parseWorkspaceSyncDocument({
          ...initial,
          tabs: [...tombstones, initial.tabs[0]],
        });
        assertEquals(parsed.tabs.length, 1002, "pending tombstones retained");
        assertEquals(liveWorkspaceSyncTabs(parsed).length, 1, "one live tab");
        assert(
          new TextEncoder().encode(JSON.stringify(parsed)).length > 256 * 1024,
          "internal aggregates are not constrained to one transport payload",
        );
      },
    },
    {
      name: "pinning order and navigation carry tab clocks",
      fn: () => {
        const initial = updateWorkspaceSyncDocument(
          null,
          snapshot([tabA, tabB]),
          deviceA,
          10,
        );
        const changed = snapshot([tabB, tabA]);
        changed.tabs[0].pinned = true;
        changed.tabs[0].url = "https://example.com/next";
        const next = updateWorkspaceSyncDocument(initial, changed, deviceB, 20);
        const tabs = liveWorkspaceSyncTabs(next);
        assertEquals(tabs[0].id, tabB, "order retained");
        assertEquals(tabs[0].pinned, true, "pin retained");
        assertEquals(
          tabs[0].url,
          "https://example.com/next",
          "navigation retained",
        );
      },
    },
    {
      name: "different window partitions do not restamp incoming tab order",
      fn: () => {
        const tabC = "44444444-4444-4444-8444-444444444444";
        const tabD = "55555555-5555-4555-8555-555555555555";
        const initial = updateWorkspaceSyncDocument(
          null,
          snapshot([tabA, tabB, tabC, tabD]),
          deviceA,
          10,
        );
        const reordered = updateWorkspaceSyncDocument(
          initial,
          { ...snapshot([tabD, tabA, tabB, tabC]), orderChanged: true },
          deviceA,
          20,
        );
        let received = reordered;
        for (let round = 0; round < 3; round++) {
          const deviceBCapture = updateWorkspaceSyncDocument(
            received,
            { ...snapshot([tabD, tabC, tabA, tabB]), orderChanged: false },
            deviceB,
            30 + round * 2,
          );
          assertEquals(
            JSON.stringify(deviceBCapture),
            JSON.stringify(received),
            "B keeps remote positions despite different physical window groups",
          );
          const deviceACapture = updateWorkspaceSyncDocument(
            deviceBCapture,
            { ...snapshot([tabA, tabC, tabD, tabB]), orderChanged: false },
            deviceA,
            31 + round * 2,
          );
          assertEquals(
            JSON.stringify(deviceACapture),
            JSON.stringify(reordered),
            "receiving and recapturing does not produce a new order mutation",
          );
          received = deviceACapture;
        }
      },
    },
    {
      name:
        "content edits retain remote positions when physical order is unchanged",
      fn: () => {
        const initial = updateWorkspaceSyncDocument(
          null,
          snapshot([tabA, tabB]),
          deviceA,
          10,
        );
        const physical = snapshot([tabB, tabA]);
        physical.tabs[0].url = "https://example.com/next";
        const captured = updateWorkspaceSyncDocument(
          initial,
          { ...physical, orderChanged: false },
          deviceB,
          20,
        );
        const ordered = liveWorkspaceSyncTabs(captured);
        assertEquals(ordered[0].id, tabA, "unchanged A keeps its position");
        assertEquals(ordered[0].modifiedAt, 10, "unchanged A keeps its clock");
        assertEquals(ordered[1].id, tabB, "navigated B keeps its position");
        assertEquals(
          ordered[1].url,
          "https://example.com/next",
          "navigation persists",
        );
        assertEquals(ordered[1].modifiedAt, 20, "only B gets an edit clock");
      },
    },
    {
      name:
        "new tabs append after known positions without a physical order mutation",
      fn: () => {
        const tabC = "44444444-4444-4444-8444-444444444444";
        const initial = updateWorkspaceSyncDocument(
          null,
          snapshot([tabA, tabB]),
          deviceA,
          10,
        );
        const captured = updateWorkspaceSyncDocument(
          initial,
          { ...snapshot([tabC, tabB, tabA]), orderChanged: false },
          deviceB,
          20,
        );
        assertEquals(
          liveWorkspaceSyncTabs(captured).map((tab) => tab.id).join(","),
          [tabA, tabB, tabC].join(","),
          "only the previously unknown tab receives a new trailing position",
        );
      },
    },
    {
      name: "regressed local clocks advance past persisted clocks",
      fn: () => {
        const initial = updateWorkspaceSyncDocument(
          null,
          snapshot([tabA]),
          deviceA,
          100,
        );
        const next = updateWorkspaceSyncDocument(
          initial,
          snapshot([]),
          deviceA,
          1,
        );
        assertEquals(next.tabs[0].modifiedAt, 101, "logical clock advances");
      },
    },
    {
      name: "equal clocks have deterministic winners",
      fn: () => {
        const a = updateWorkspaceSyncDocument(
          null,
          snapshot([tabA]),
          deviceA,
          10,
        );
        const b = structuredClone(a);
        b.tabs[0].title = "Different";
        assertEquals(
          JSON.stringify(mergeWorkspaceSyncDocuments(a, b)),
          JSON.stringify(mergeWorkspaceSyncDocuments(b, a)),
          "equal-clock value conflict converges",
        );
      },
    },
    {
      name: "private local and privileged URLs cannot sync",
      fn: () => {
        for (
          const url of [
            "file:///tmp/local",
            "about:config",
            "javascript:alert(1)",
            "data:text/html,test",
            "https://user:pass@example.com",
            "moz-extension://id/file",
          ]
        ) {
          assert(!isSyncableWorkspaceURL(url), `${url} filtered`);
        }
        assert(
          isSyncableWorkspaceURL("https://example.com/#section"),
          "ordinary web URL accepted",
        );
      },
    },
    {
      name: "duplicate tab IDs and future schema are rejected",
      fn: () => {
        const initial = updateWorkspaceSyncDocument(
          null,
          snapshot([tabA]),
          deviceA,
          10,
        );
        assertThrows(() =>
          parseWorkspaceSyncDocument({
            ...initial,
            tabs: [...initial.tabs, initial.tabs[0]],
          }), "duplicate IDs rejected");
        assertThrows(
          () => parseWorkspaceSyncDocument({ ...initial, version: 2 }),
          "unknown schema rejected",
        );
        assertThrows(() =>
          mergeWorkspaceSyncDocuments(initial, {
            ...initial,
            workspaceId: tabA,
          }), "different workspace rejected");
      },
    },
    {
      name: "untrusted fields never enter the wire document",
      fn: () => {
        const initial = updateWorkspaceSyncDocument(
          null,
          snapshot([tabA]),
          deviceA,
          10,
        );
        const parsed = parseWorkspaceSyncDocument({
          ...initial,
          isSelected: true,
          userContextId: 8,
          history: ["secret"],
          tabs: initial.tabs.map((tab) => ({
            ...tab,
            state: { formdata: "secret" },
          })),
        });
        assert(
          !JSON.stringify(parsed).includes("secret"),
          "session state omitted",
        );
        assert(!Object.hasOwn(parsed, "isSelected"), "local selection omitted");
        assert(!Object.hasOwn(parsed, "userContextId"), "container ID omitted");
      },
    },
    {
      name: "field limits and duplicate IDs fail before partial writes",
      fn: () => {
        const initial = updateWorkspaceSyncDocument(
          null,
          snapshot([tabA]),
          deviceA,
          10,
        );
        assertThrows(
          () =>
            parseWorkspaceSyncDocument({ ...initial, name: "x".repeat(257) }),
          "name limit enforced",
        );
        assertThrows(() =>
          parseWorkspaceSyncDocument({
            ...initial,
            tabs: new Array(1001).fill(initial.tabs[0]),
          }), "duplicate tab IDs rejected");
        assertThrows(() =>
          parseWorkspaceSyncDocument({
            ...initial,
            tabs: [{ ...initial.tabs[0], url: "about:config" }],
          }), "incoming unsafe URL rejected");
      },
    },
    {
      name: "independent additions can merge beyond one thousand live tabs",
      fn: () => {
        const ids = Array.from(
          { length: 1002 },
          (_, index) =>
            `44444444-4444-4444-8444-${String(index).padStart(12, "0")}`,
        );
        const a = updateWorkspaceSyncDocument(
          null,
          snapshot(ids.slice(0, 501)),
          deviceA,
          10,
        );
        const b = updateWorkspaceSyncDocument(
          null,
          snapshot(ids.slice(501)),
          deviceB,
          10,
        );
        assertEquals(
          liveWorkspaceSyncTabs(mergeWorkspaceSyncDocuments(a, b)).length,
          1002,
          "per-record transport has no aggregate live tab cap",
        );
      },
    },
  ]);
}
