// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  WorkspaceBookmarksService,
  type WorkspaceBookmarkState,
} from "../WorkspaceBookmarks.sys.mts";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../chrome/test/utils/test_harness.ts";

const A = "11111111-1111-4111-8111-111111111111";
const B = "22222222-2222-4222-8222-222222222222";
const SOURCE = "55555555-5555-4555-8555-555555555555";

interface InsertTree {
  guid: string;
  type?: number;
  title?: string;
  url?: string;
  children?: InsertTree[];
}

interface BookmarkDatabase {
  executeCached(
    sql: string,
    parameters: Record<string, string | null>,
  ): Promise<Array<{ getResultByName(name: string): unknown }>>;
}

interface Places {
  history: { makeGuid(): string };
  bookmarks: {
    unfiledGuid: string;
    TYPE_FOLDER: number;
    TYPE_BOOKMARK: number;
    insertTree(tree: InsertTree): Promise<unknown>;
    remove(guid: string): Promise<unknown>;
  };
  promiseDBConnection(): Promise<BookmarkDatabase>;
}

function p95(samples: number[]): number {
  return [...samples].sort((left, right) => left - right)[
    Math.floor(samples.length * 0.95)
  ];
}

/**
 * Uses real Places insertion, the production tree reader, and the same
 * parameterized JSON GUID predicate used by the URL bar. All added bookmarks
 * are placed beneath one generated folder and removed in finally.
 */
async function testRealPlacesBulkScopeQueries(): Promise<void> {
  const { PlacesUtils: places } = ChromeUtils.importESModule(
    "resource://gre/modules/PlacesUtils.sys.mjs",
  ) as { PlacesUtils: Places };
  const folderGuid = places.history.makeGuid();
  const aGuid = places.history.makeGuid();
  const bGuid = places.history.makeGuid();
  const prefix = `https://floorp-scope-benchmark.invalid/${folderGuid}/`;
  let saved: WorkspaceBookmarkState = {
    schemaVersion: 1,
    sourceId: SOURCE,
    rules: { [aGuid]: [A], [bGuid]: [B] },
    workspaceFolders: {},
  };
  let service: WorkspaceBookmarksService | undefined;
  let created = false;
  const buildBookmarks = (
    workspace: "A" | "B",
    start: number,
    count: number,
  ): InsertTree[] =>
    Array.from({ length: count }, (_, offset) => ({
      guid: places.history.makeGuid(),
      type: places.bookmarks.TYPE_BOOKMARK,
      title: `${workspace} benchmark ${start + offset}`,
      url: `${prefix}${workspace}/${start + offset}`,
    }));
  try {
    await places.bookmarks.insertTree({
      guid: places.bookmarks.unfiledGuid,
      children: [{
        guid: folderGuid,
        type: places.bookmarks.TYPE_FOLDER,
        title: "Floorp workspace bookmark benchmark",
        children: [
          {
            guid: aGuid,
            type: places.bookmarks.TYPE_FOLDER,
            title: "A",
            children: buildBookmarks("A", 0, 50),
          },
          {
            guid: bGuid,
            type: places.bookmarks.TYPE_FOLDER,
            title: "B",
            children: buildBookmarks("B", 0, 50),
          },
        ],
      }],
    });
    created = true;
    service = new WorkspaceBookmarksService({
      observe: false,
      sourceId: SOURCE,
      readEnabled: () => true,
      knownWorkspaces: () => [A, B],
      storage: {
        read: () => Promise.resolve(structuredClone(saved)),
        readRecovery: () => Promise.resolve(null),
        write: (next) => {
          saved = structuredClone(next);
          return Promise.resolve();
        },
      },
    });
    await service.ready;
    const win = { closed: false } as unknown as Window;
    service.setWindowContext(win, A, true);
    const db = await places.promiseDBConnection();
    const select = `SELECT b.guid, b.title, p.url
      FROM moz_bookmarks b JOIN moz_places p ON p.id = b.fk
      WHERE b.type = 1 AND p.url LIKE :prefix`;
    const visible = ` AND b.guid IN (
      SELECT value FROM json_each(:floorpVisibleGuids)
    )`;
    const suffix = " ORDER BY b.dateAdded DESC, b.id DESC LIMIT 30";
    const reports: Array<Record<string, number | string>> = [];
    const benchmark = async (size: number): Promise<void> => {
      assert(service, "the scope service is initialized");
      const coldStart = performance.now();
      const context = service.getContext(win);
      const json = service.getVisibleBookmarkGuidsJSON(context);
      const coldScopeMs = performance.now() - coldStart;
      assert(json, "scoped SQL receives an explicit visible GUID set");
      const counts = await db.executeCached(
        `SELECT COUNT(*) AS count FROM moz_bookmarks b
         JOIN moz_places p ON p.id=b.fk
         WHERE b.type=1 AND p.url LIKE :prefix${visible}`,
        { prefix: `${prefix}%`, floorpVisibleGuids: json },
      );
      assertEquals(
        Number(counts[0].getResultByName("count")),
        size / 2,
        "real Places SQL excludes the other workspace before limiting results",
      );
      const baseline: number[] = [];
      const scoped: number[] = [];
      const switches: number[] = [];
      for (let iteration = 0; iteration < 20; iteration++) {
        let started = performance.now();
        const normal = await db.executeCached(select + suffix, {
          prefix: `${prefix}%`,
        });
        baseline.push(performance.now() - started);
        assertEquals(
          normal.length,
          30,
          "ordinary Places search has enough benchmark candidates",
        );
        started = performance.now();
        const filtered = await db.executeCached(select + visible + suffix, {
          prefix: `${prefix}%`,
          floorpVisibleGuids: json,
        });
        scoped.push(performance.now() - started);
        assertEquals(
          filtered.length,
          30,
          "invisible matches cannot consume the query's result limit",
        );
        assert(
          filtered.every((row) =>
            String(row.getResultByName("title")).startsWith("A benchmark ")
          ),
          "the query returns only visible titles",
        );
        started = performance.now();
        service.setWindowContext(win, iteration % 2 ? A : B, true);
        const switching = service.getContext(win);
        service.getVisibleBookmarkGuidsJSON(switching);
        switches.push(performance.now() - started);
      }
      service.setWindowContext(win, A, true);
      reports.push({
        bookmarks: size,
        coldScopeMs,
        baselineQueryP95Ms: p95(baseline),
        scopedQueryP95Ms: p95(scoped),
        switchP95Ms: p95(switches),
      });
    };
    await benchmark(100);
    await places.bookmarks.insertTree({
      guid: aGuid,
      children: buildBookmarks("A", 50, 4_950),
    });
    await places.bookmarks.insertTree({
      guid: bGuid,
      children: buildBookmarks("B", 50, 4_950),
    });
    await service.refresh();
    await benchmark(10_000);
    console.info(
      `[WorkspaceBookmarks Places benchmark] ${JSON.stringify(reports)}`,
    );
    const reportPath = PathUtils.join(
      PathUtils.profileDir,
      "workspace-bookmarks-benchmark.json",
    );
    await IOUtils.writeJSON(reportPath, {
      measuredAt: new Date().toISOString(),
      runtime: {
        os: Services.appinfo.OS,
        version: Services.appinfo.version,
        buildId: Services.appinfo.appBuildID,
      },
      samplesPerSize: 20,
      units: "milliseconds",
      reports,
    }, { tmpPath: `${reportPath}.tmp`, flush: true });
  } finally {
    service?.destroy();
    if (created) await places.bookmarks.remove(folderGuid);
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("WorkspaceBookmarksPlaces.test.mts", [
    {
      name: "real Places 100/10,000-bookmark SQL and switch benchmark",
      fn: testRealPlacesBulkScopeQueries,
    },
  ]);
}
