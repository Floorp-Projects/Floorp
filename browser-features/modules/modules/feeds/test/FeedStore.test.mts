// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import {
  assert,
  assertEquals,
  runTests,
} from "../../../../chrome/test/utils/test_harness.ts";
import { FeedStore } from "../FeedStore.sys.mts";
import type { ParsedFeed } from "../types.ts";

const limits = { retentionDays: 90, maxItems: 500 };
function parsed(count = 2, title = "Feed"): ParsedFeed {
  return {
    title,
    siteUrl: "https://example.com/",
    articles: Array.from({ length: count }, (_, index) => ({
      sourceKey: `id:${index}`,
      title: `Article ${index}`,
      link: `https://example.com/${index}`,
      author: "Author",
      publishedAt: 1_700_000_000_000 + index * 1000,
      excerpt: `Excerpt ${index}`,
      hasFullContent: true,
      content: [{
        tag: "p",
        children: [{ tag: "text", text: `Body ${index}` }],
      }],
      images: { "image-0": "https://example.com/image.png" },
    })),
  };
}
const metadata = () => ({
  etag: '"version-1"',
  lastModified: null,
  nextAttemptAt: Date.now() + 900000,
});
async function fixture(
  work: (store: FeedStore, path: string) => Promise<void>,
): Promise<void> {
  const directory = PathUtils.join(
    PathUtils.tempDir,
    `floorp-feed-${Services.uuid.generateUUID()}`,
  );
  const path = PathUtils.join(directory, "feeds.sqlite");
  const store = new FeedStore(path);
  try {
    await store.open();
    await work(store, path);
  } finally {
    await store.close();
    await IOUtils.remove(directory, { recursive: true, ignoreAbsent: true });
  }
}
async function testStateAndFeedNamespace(): Promise<void> {
  await fixture(async (store) => {
    const feed = await store.addFeed("https://example.com/rss", "Feed", null);
    await store.upsertFeed(feed.id, parsed(), metadata(), limits);
    let page = await store.queryArticles({});
    assertEquals(page.total, 2, "Two items are stored");
    const article = page.items.find((item) => item.title === "Article 0")!;
    await Promise.all([
      store.patchArticleState([article.id], { isRead: true }),
      store.patchArticleState([article.id], { starred: true }),
    ]);
    const next = parsed();
    next.articles[0].title = "Updated title";
    next.articles[0].content = [{ tag: "text", text: "Updated body" }];
    await store.upsertFeed(feed.id, next, metadata(), limits);
    const updated = await store.getArticle(article.id);
    assert(
      updated.isRead && updated.starred,
      "Update preserves concurrent read/star state",
    );
    assertEquals(
      updated.receivedAt,
      article.receivedAt,
      "Update does not reset original receipt time",
    );
    assertEquals(updated.title, "Updated title", "Publisher content updates");
    const other = await store.addFeed(
      "https://other.example/rss",
      "Other",
      null,
    );
    await store.upsertFeed(other.id, parsed(1), metadata(), limits);
    page = await store.queryArticles({});
    assertEquals(
      page.total,
      3,
      "Same publisher source key in different feeds stays separate",
    );
    assertEquals(
      (await store.queryArticles({ limit: 1 })).nextOffset,
      1,
      "Lists are paged",
    );
    assertEquals(
      (await store.queryArticles({ query: "' OR 1=1 --" })).total,
      0,
      "Search is bound rather than SQL interpolation",
    );
    assertEquals(
      await store.imageSource(article.id, "image-0"),
      "https://example.com/image.png",
      "Known article image is resolved in the store",
    );
    let rejected = false;
    try {
      await store.imageSource(article.id, "https://attacker.invalid/");
    } catch {
      rejected = true;
    }
    assert(rejected, "Arbitrary URLs cannot be requested as image IDs");
  });
}
async function testUnsubscribeRetainsStars(): Promise<void> {
  await fixture(async (store) => {
    const feed = await store.addFeed("https://example.com/rss", "Feed", null);
    await store.upsertFeed(feed.id, parsed(), metadata(), limits);
    const page = await store.queryArticles({});
    const starred = page.items[0];
    await store.patchArticleState([starred.id], {
      starred: true,
      isRead: true,
    });
    await store.unsubscribe(feed.id);
    assertEquals(
      (await store.listFeeds()).length,
      0,
      "Unsubscribed feed is not active",
    );
    assertEquals(
      (await store.queryArticles({})).total,
      1,
      "Only saved stars remain visible after unsubscribe",
    );
    assertEquals(
      (await store.getArticle(starred.id)).starred,
      true,
      "Saved star keeps its body",
    );
    const archived = await store.getArticle(starred.id);
    assertEquals(
      archived.feedTitle,
      "Feed",
      "Unsubscribed star retains its source title",
    );
    assertEquals(
      archived.feedEnabled,
      false,
      "Reader can identify unsubscribed source",
    );
    assertEquals(
      archived.siteUrl,
      "https://example.com/",
      "Reader has a validated source fallback URL",
    );
    const revived = await store.addFeed(feed.url, "Feed", null);
    assertEquals(
      revived.id,
      feed.id,
      "Resubscription reuses original feed identity",
    );
    await store.upsertFeed(feed.id, parsed(), metadata(), limits);
    assertEquals(
      (await store.queryArticles({})).total,
      2,
      "Refetch does not duplicate saved star",
    );
    assert(
      (await store.getArticle(starred.id)).isRead,
      "Saved read state remains",
    );
  });
}
async function testPruningAndReopen(): Promise<void> {
  await fixture(async (store, path) => {
    const feed = await store.addFeed("https://example.com/rss", "Feed", null);
    await store.upsertFeed(feed.id, parsed(), metadata(), limits);
    const first = (await store.queryArticles({})).items.find((item) =>
      item.title === "Article 0"
    )!;
    await store.patchArticleState([first.id], { isRead: true });
    await store.upsertFeed(feed.id, parsed(3), metadata(), {
      retentionDays: 90,
      maxItems: 2,
    });
    assertEquals(
      (await store.queryArticles({})).total,
      2,
      "Unstarred per-feed count is bounded",
    );
    await store.upsertFeed(feed.id, parsed(3), metadata(), limits);
    assert(
      (await store.getArticle(first.id)).isRead,
      "Pruned-and-republished content retains read tombstone",
    );
    await store.close();
    const reopened = new FeedStore(path);
    try {
      await reopened.open();
      assertEquals(
        (await reopened.queryArticles({})).total,
        3,
        "Content survives reopening the profile database",
      );
    } finally {
      await reopened.close();
    }
  });
}
async function testRetryDeadlineMigration(): Promise<void> {
  await fixture(async (store, path) => {
    const feed = await store.addFeed("https://example.com/rss", "Feed", null);
    await store.upsertFeed(feed.id, parsed(1), metadata(), limits);
    const article = (await store.queryArticles({})).items[0];
    await store.patchArticleState([article.id], { starred: true });
    await store.close();
    const module = ChromeUtils.importESModule(
      "resource://gre/modules/Sqlite.sys.mjs",
    ) as {
      Sqlite: {
        openConnection(
          options: { path: string },
        ): Promise<
          { execute(sql: string): Promise<unknown>; close(): Promise<void> }
        >;
      };
    };
    const legacy = await module.Sqlite.openConnection({ path });
    await legacy.execute("ALTER TABLE feeds DROP COLUMN retry_after");
    await legacy.execute("PRAGMA user_version=1");
    await legacy.close();
    const migrated = new FeedStore(path);
    try {
      await migrated.open();
      assert(
        (await migrated.getArticle(article.id)).starred,
        "Version upgrade preserves saved articles and state",
      );
      assertEquals(
        (await migrated.getFeed(feed.id))!.retryAfterAt,
        null,
        "New server cooldown starts unset",
      );
      const until = Date.now() + 3600000;
      await migrated.markFailure(feed.id, "HTTP 429", until, until);
      assertEquals(
        (await migrated.getFeed(feed.id))!.retryAfterAt,
        until,
        "Server Retry-After is persisted independently",
      );
      await migrated.markNotModified(feed.id, Date.now() + 900000);
      assertEquals(
        (await migrated.getFeed(feed.id))!.retryAfterAt,
        null,
        "Success clears server cooldown",
      );
    } finally {
      await migrated.close();
    }
  });
}

async function testUnknownDatabaseVersion(): Promise<void> {
  await fixture(async (store, path) => {
    await store.close();
    const module = ChromeUtils.importESModule(
      "resource://gre/modules/Sqlite.sys.mjs",
    ) as {
      Sqlite: {
        openConnection(
          options: { path: string },
        ): Promise<
          { execute(sql: string): Promise<unknown>; close(): Promise<void> }
        >;
      };
    };
    const connection = await module.Sqlite.openConnection({ path });
    await connection.execute("PRAGMA user_version=99");
    await connection.close();
    const future = new FeedStore(path);
    let rejected = false;
    try {
      await future.open();
    } catch {
      rejected = true;
    }
    assert(rejected, "An unknown database version is rejected without reset");
    await future.close();
    assert(
      await IOUtils.exists(path),
      "Unknown-version database is left intact",
    );
  });
}
export async function runAllTests(): Promise<void> {
  await runTests("FeedStore.test.mts", [
    {
      name: "content upsert preserves state and feed identity namespaces",
      fn: testStateAndFeedNamespace,
    },
    {
      name: "unsubscribe retains stars and resubscription identity",
      fn: testUnsubscribeRetainsStars,
    },
    {
      name: "bounded cache, tombstone state and profile reopen",
      fn: testPruningAndReopen,
    },
    {
      name: "schema upgrade preserves content and independent Retry-After",
      fn: testRetryDeadlineMigration,
    },
    {
      name: "future database versions fail without destructive reset",
      fn: testUnknownDatabaseVersion,
    },
  ]);
}
