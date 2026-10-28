// SPDX-License-Identifier: MPL-2.0
const { setTimeout, clearTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
) as {
  setTimeout: (callback: () => void, milliseconds: number) => number;
  clearTimeout: (timer: number) => void;
};
import type { NewTabSettings } from "#libs/newtab/types.ts";
import {
  decodeFeedResponse,
  MAX_FEED_BYTES,
  requestFeed,
} from "./FeedNetwork.sys.mts";
import { parseFeed } from "./FeedParser.sys.mts";
import type { FeedStore } from "./FeedStore.sys.mts";
import type { FeedResponse, StoredFeed } from "./types.ts";

type Transport = typeof requestFeed;
interface QueuedRequest {
  origin: string;
  signal?: AbortSignal;
  work: () => Promise<unknown>;
  resolve: (value: unknown) => void;
  reject: (reason: unknown) => void;
}

/** Shared by feed inspection, updates and explicitly requested article images. */
export class FeedRequestPool {
  private active = 0;
  private origins = new Map<string, number>();
  private queued: QueuedRequest[] = [];
  run<T>(
    url: string,
    signal: AbortSignal | undefined,
    work: () => Promise<T>,
  ): Promise<T> {
    return new Promise((resolve, reject) => {
      const aborted = () => this.drain();
      const cleanup = () => signal?.removeEventListener("abort", aborted);
      this.queued.push({
        origin: new URL(url).origin,
        signal,
        work,
        resolve: (value) => {
          cleanup();
          resolve(value as T);
        },
        reject: (reason) => {
          cleanup();
          reject(reason);
        },
      });
      signal?.addEventListener("abort", aborted, { once: true });
      this.drain();
    });
  }
  private drain(): void {
    for (let index = 0; index < this.queued.length;) {
      const task = this.queued[index];
      if (task.signal?.aborted) {
        this.queued.splice(index, 1);
        task.reject(new Error("The feed request was cancelled."));
        continue;
      }
      if (this.active >= 4) return;
      if ((this.origins.get(task.origin) ?? 0) >= 2) {
        index++;
        continue;
      }
      this.queued.splice(index, 1);
      this.active++;
      this.origins.set(task.origin, (this.origins.get(task.origin) ?? 0) + 1);
      void Promise.resolve().then(task.work).then(task.resolve, task.reject)
        .finally(() => {
          this.active--;
          this.origins.set(
            task.origin,
            (this.origins.get(task.origin) ?? 1) - 1,
          );
          this.drain();
        });
    }
  }
}

export function parseRetryAfter(
  value: string | undefined,
  now = Date.now(),
): number | null {
  if (!value) return null;
  if (/^\d+$/.test(value.trim())) {
    const seconds = Number(value);
    return Number.isSafeInteger(seconds) && seconds >= 0
      ? Math.min(Number.MAX_SAFE_INTEGER, now + seconds * 1000)
      : null;
  }
  const date = Date.parse(value);
  return Number.isFinite(date) && date > now ? date : null;
}
export function nextFailureAt(
  failures: number,
  intervalMinutes: number,
  now = Date.now(),
  retryAfter: number | null = null,
  jitter = Math.random(),
): number {
  const base = Math.max(15, intervalMinutes) * 60_000;
  const delay = Math.min(
    6 * 3600_000,
    base * 2 ** Math.min(10, Math.max(0, failures)) * (0.9 + jitter * 0.2),
  );
  return Math.max(now + delay, retryAfter ?? 0);
}

export class FeedScheduler {
  private timer: ReturnType<typeof setTimeout> | null = null;
  private jobs = new Map<
    string,
    { controller: AbortController; promise: Promise<void> }
  >();
  private ticking: Promise<void> | null = null;
  private running = false;
  private readonly pool = new FeedRequestPool();
  constructor(
    private readonly store: FeedStore,
    private readonly settings: () => NewTabSettings["rss"],
    private readonly changed: () => void,
    private readonly transport: Transport = requestFeed,
  ) {}

  fetch(
    url: string,
    headers: Record<string, string> = {},
    signal?: AbortSignal,
    limit = MAX_FEED_BYTES,
  ): Promise<FeedResponse> {
    return this.pool.run(
      url,
      signal,
      () => this.transport(url, headers, signal, limit),
    );
  }
  start(): void {
    if (this.running) return;
    this.running = true;
    void this.tick();
  }
  reschedule(): void {
    if (this.timer) clearTimeout(this.timer);
    this.timer = null;
    if (this.running) void this.tick();
  }
  private tick(): Promise<void> {
    if (this.ticking) return this.ticking;
    this.ticking = Promise.resolve().then(async () => {
      if (
        this.running && this.settings().refreshMinutes > 0 &&
        !Services.io.offline
      ) {
        const now = Date.now();
        const feeds = await this.store.storedFeeds();
        if (
          !this.running || Services.io.offline ||
          this.settings().refreshMinutes === 0
        ) return;
        const due = feeds.filter((feed) => feed.nextAttemptAt <= now);
        await Promise.all(due.map((feed) => this.update(feed)));
      }
    }).catch(() => {
      /* Storage/config errors are surfaced by the page's snapshot. */
    }).finally(() => {
      this.ticking = null;
      if (this.running) {
        this.timer = setTimeout(() => {
          this.timer = null;
          void this.tick();
        }, 30_000);
      }
    });
    return this.ticking;
  }
  async refresh(ids?: string[]): Promise<void> {
    if (Services.io.offline) {
      throw new Error(
        "The browser is offline. Saved articles remain available.",
      );
    }
    const feeds = await this.store.storedFeeds();
    if (!this.running) return;
    const selected = ids ? new Set(ids) : null;
    await Promise.all(
      feeds.filter((feed) => !selected || selected.has(feed.id)).map((feed) => {
        // Manual actions can bypass local backoff, but not server Retry-After.
        if ((feed.retryAfterAt ?? 0) > Date.now()) {
          return Promise.resolve();
        }
        return this.update(feed);
      }),
    );
  }
  private update(feed: StoredFeed): Promise<void> {
    const existing = this.jobs.get(feed.id);
    if (existing) return existing.promise;
    const controller = new AbortController();
    const promise = (async () => {
      let retryAfter: number | null = null;
      try {
        const settings = this.settings();
        const headers: Record<string, string> = {};
        if (feed.etag) headers["If-None-Match"] = feed.etag;
        if (feed.lastModified) headers["If-Modified-Since"] = feed.lastModified;
        const response = await this.fetch(feed.url, headers, controller.signal);
        const next = Date.now() +
          Math.max(15, settings.refreshMinutes) * 60_000;
        if (response.status === 304) {
          await this.store.markNotModified(feed.id, next);
          return;
        }
        retryAfter = parseRetryAfter(response.headers["retry-after"]);
        if (response.status < 200 || response.status >= 300) {
          throw new Error(`The feed server returned HTTP ${response.status}.`);
        }
        const parsed = await parseFeed(
          decodeFeedResponse(response),
          response.url,
        );
        if (controller.signal.aborted) return;
        await this.store.upsertFeed(feed.id, parsed, {
          etag: response.headers.etag ?? null,
          lastModified: response.headers["last-modified"] ?? null,
          nextAttemptAt: next,
        }, settings);
      } catch (cause) {
        if (controller.signal.aborted) return;
        const error =
          cause instanceof Error && !/https?:\/\//i.test(cause.message)
            ? cause.message.slice(0, 200)
            : "The feed could not be updated.";
        await this.store.markFailure(
          feed.id,
          error,
          nextFailureAt(
            feed.failureCount,
            this.settings().refreshMinutes,
            Date.now(),
            retryAfter,
          ),
          retryAfter,
        );
      } finally {
        this.jobs.delete(feed.id);
        this.changed();
      }
    })();
    this.jobs.set(feed.id, { controller, promise });
    this.changed();
    return promise;
  }
  isUpdating(id: string): boolean {
    return this.jobs.has(id);
  }
  abort(id: string): void {
    this.jobs.get(id)?.controller.abort();
  }
  async stop(): Promise<void> {
    this.running = false;
    if (this.timer) clearTimeout(this.timer);
    this.timer = null;
    const jobs = [...this.jobs.values()];
    jobs.forEach((job) => job.controller.abort());
    await Promise.allSettled(jobs.map((job) => job.promise));
    await this.ticking;
  }
}
