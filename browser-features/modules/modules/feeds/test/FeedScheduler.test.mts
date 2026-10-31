// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import {
  assert,
  assertEquals,
  runTests,
} from "../../../../chrome/test/utils/test_harness.ts";
import {
  FeedRequestPool,
  FeedScheduler,
  nextFailureAt,
  parseRetryAfter,
} from "../FeedScheduler.sys.mts";

function testRetryPolicy(): void {
  const now = 1_700_000_000_000;
  assertEquals(
    parseRetryAfter("120", now),
    now + 120000,
    "Retry-After seconds resolve against current time",
  );
  assertEquals(
    parseRetryAfter("not a date", now),
    null,
    "Invalid Retry-After is ignored",
  );
  assertEquals(
    nextFailureAt(0, 15, now, null, 0.5),
    now + 900000,
    "First retry starts at selected base interval",
  );
  assertEquals(
    nextFailureAt(20, 15, now, null, 0.5),
    now + 6 * 3600000,
    "Local exponential retries stop at six hours",
  );
  assertEquals(
    nextFailureAt(20, 15, now, now + 86400000, 0.5),
    now + 86400000,
    "Server cooldown can exceed local retry cap",
  );
}
async function testRequestPool(): Promise<void> {
  const pool = new FeedRequestPool();
  let active = 0;
  let maximum = 0;
  let sameOrigin = 0;
  let maximumOrigin = 0;
  const resolvers: (() => void)[] = [];
  const jobs = Array.from({ length: 9 }, (_, index) => {
    const primary = index < 5;
    return pool.run(
      primary
        ? "https://one.example/rss"
        : `https://other-${index}.example/rss`,
      undefined,
      async () => {
        active++;
        maximum = Math.max(maximum, active);
        if (primary) {
          sameOrigin++;
          maximumOrigin = Math.max(maximumOrigin, sameOrigin);
        }
        await new Promise<void>((resolve) => resolvers.push(resolve));
        active--;
        if (primary) sameOrigin--;
      },
    );
  });
  for (let iteration = 0; iteration < 20; iteration++) {
    await new Promise<void>((resolve) => setTimeout(resolve, 0));
    for (const resolve of resolvers.splice(0)) resolve();
  }
  await Promise.all(jobs);
  assertEquals(maximum, 4, "Only four total requests run at once");
  assertEquals(maximumOrigin, 2, "Only two requests run for one origin");
  const cancelled = new AbortController();
  cancelled.abort();
  let called = false;
  let rejected = false;
  try {
    await pool.run("https://cancelled.example/", cancelled.signal, () => {
      called = true;
      return Promise.resolve();
    });
  } catch {
    rejected = true;
  }
  assert(rejected && !called, "Cancelled queued request never starts");
}
async function testManualRetryPolicy(): Promise<void> {
  const { FeedStore } = await import("../FeedStore.sys.mts");
  const feed = {
    id: "feed",
    url: "https://example.com/rss",
    title: "Feed",
    siteUrl: null,
    enabled: true,
    etag: null,
    lastModified: null,
    lastSuccessAt: null,
    nextAttemptAt: Date.now() + 3600000,
    retryAfterAt: null as number | null,
    failureCount: 1,
    error: "Network failure",
  };
  class MemoryStore extends FeedStore {
    override storedFeeds() {
      return Promise.resolve([{ ...feed }]);
    }
    override markNotModified(_id: string, next: number) {
      feed.nextAttemptAt = next;
      return Promise.resolve();
    }
  }
  let requests = 0;
  const scheduler = new FeedScheduler(
    new MemoryStore(),
    () => ({ refreshMinutes: 0, retentionDays: 90, maxItems: 500 }),
    () => {},
    () => {
      requests++;
      return Promise.resolve({
        status: 304,
        url: feed.url,
        bytes: new Uint8Array(),
        headers: {},
        contentType: "application/xml",
        charset: "",
      });
    },
  );
  scheduler.start();
  try {
    await scheduler.refresh();
    assertEquals(
      requests,
      1,
      "Manual retry can recover before a local backoff ends",
    );
    feed.retryAfterAt = Date.now() + 3600000;
    await scheduler.refresh();
    assertEquals(
      requests,
      1,
      "Manual retry does not bypass the server cooldown",
    );
  } finally {
    await scheduler.stop();
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("FeedScheduler.test.mts", [
    {
      name: "conditional failure cooldown and server Retry-After",
      fn: testRetryPolicy,
    },
    {
      name: "manual retry overrides local backoff but respects server cooldown",
      fn: testManualRetryPolicy,
    },
    {
      name: "shared global and per-origin request concurrency",
      fn: testRequestPool,
    },
  ]);
}
