// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import type { FolderView } from "../../../../../libs/newtab/types.ts";
import { recoverFolder } from "../../../src/features/newtab/folderRecovery.ts";
import {
  assertEquals,
  runTests,
  type TestCase,
} from "../../../../chrome/test/utils/test_harness.ts";

function view(guid: string, path: string[], groupGuid = "group"): FolderView {
  return {
    guid,
    title: guid,
    groupGuid,
    ancestors: path.map((id, index) => ({
      guid: id,
      title: id,
      parentGuid: path[index - 1] ?? "root",
      type: "folder",
      index: 0,
    })),
    children: [],
    revision: 1,
  };
}
const tests: TestCase[] = [
  {
    name: "Deleting an open folder returns to its nearest surviving parent",
    fn: async () => {
      const requested: string[] = [];
      const recovered = await recoverFolder(
        view("open", ["group", "parent", "open"]),
        "open",
        "group",
        (guid) => {
          requested.push(guid);
          return Promise.resolve(view(guid, ["group", guid]));
        },
        () => true,
      );
      assertEquals(
        recovered?.guid,
        "parent",
        "Keep the closest valid location",
      );
      assertEquals(requested.join(","), "parent", "Do not jump to the group");
    },
  },
  {
    name: "Removed ancestors are skipped and ancestors moved to another group are rejected",
    fn: async () => {
      const requested: string[] = [];
      const recovered = await recoverFolder(
        view("open", ["group", "moved", "removed", "open"]),
        "open",
        "group",
        (guid) => {
          requested.push(guid);
          if (guid === "removed") return Promise.reject(new Error("Removed"));
          return Promise.resolve(
            view(guid, [guid], guid === "moved" ? "other" : "group"),
          );
        },
        () => true,
      );
      assertEquals(recovered?.guid, "group", "Remain within the active group");
      assertEquals(
        requested.join(","),
        "removed,moved,group",
        "Try nearest ancestors first",
      );
    },
  },
  {
    name: "Switching groups does not reuse the previous group's ancestor path",
    fn: async () => {
      const requested: string[] = [];
      const recovered = await recoverFolder(
        view("open", ["group", "parent", "open"]),
        "open",
        "new-group",
        (guid) => {
          requested.push(guid);
          return Promise.resolve(view(guid, [guid], "new-group"));
        },
        () => true,
      );
      assertEquals(
        recovered?.guid,
        "new-group",
        "Use the newly selected group",
      );
      assertEquals(
        requested.join(","),
        "new-group",
        "Never query the old path",
      );
    },
  },
  {
    name: "Navigation during recovery discards stale results and stops further requests",
    fn: async () => {
      let current = true;
      const requested: string[] = [];
      const recovered = await recoverFolder(
        view("open", ["group", "parent", "open"]),
        "open",
        "group",
        (guid) => {
          requested.push(guid);
          current = false;
          return Promise.resolve(view(guid, ["group", guid]));
        },
        () => current,
      );
      assertEquals(
        recovered,
        null,
        "A stale request must not restore a location",
      );
      assertEquals(
        requested.join(","),
        "parent",
        "Do not continue an obsolete recovery",
      );
    },
  },
];
export async function runAllTests(): Promise<void> {
  await runTests("folderRecovery.test.ts", tests);
}
