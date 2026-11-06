// SPDX-License-Identifier: MPL-2.0
import type {
  Article,
  ArticlePage,
  ArticleQuery,
  FeedSummary,
  SubscribeResult,
} from "#libs/newtab/types.ts";
import { SettingsService } from "../newtab/SettingsService.sys.mts";
import { decodeFeedResponse, MAX_IMAGE_BYTES } from "./FeedNetwork.sys.mts";
import { inspectFeed, normaliseFeedUrl } from "./FeedParser.sys.mts";
import { FeedScheduler } from "./FeedScheduler.sys.mts";
import { FeedStore } from "./FeedStore.sys.mts";

const { AsyncShutdown } = ChromeUtils.importESModule(
  "resource://gre/modules/AsyncShutdown.sys.mjs",
) as {
  AsyncShutdown: {
    profileBeforeChange: {
      addBlocker(name: string, condition: () => Promise<void>): void;
      removeBlocker(condition: () => Promise<void>): void;
    };
  };
};

class ProfileFeedService {
  private store: FeedStore | null = null;
  private scheduler: FeedScheduler | null = null;
  private starting: Promise<void> | null = null;
  private listeners = new Set<() => void>();
  private settingsStop: (() => void) | null = null;
  private inspection = new Map<string, Promise<SubscribeResult>>();
  private inspections = new Set<AbortController>();
  private imageRequests = new Set<AbortController>();
  private shutdownBlocker = () => this.stop();
  private closing = false;
  private stopping: Promise<void> | null = null;
  private observing = false;
  private observer = {
    observe: (_subject: unknown, topic: string) => {
      if (topic === "profile-before-change") void this.stop();
      else this.scheduler?.reschedule();
    },
  };

  start(): Promise<void> {
    if (this.closing) {
      return Promise.reject(new Error("Feed storage is shutting down."));
    }
    if (this.starting) return this.starting;
    this.starting = (async () => {
      const store = new FeedStore();
      await store.open();
      if (this.closing) {
        await store.close();
        throw new Error("Feed storage is shutting down.");
      }
      this.store = store;
      this.scheduler = new FeedScheduler(
        store,
        () => SettingsService.get().rss,
        () => this.notify(),
      );
      this.settingsStop = SettingsService.subscribe(() =>
        this.scheduler?.reschedule()
      );
      AsyncShutdown.profileBeforeChange.addBlocker(
        "Floorp feed storage",
        this.shutdownBlocker,
      );
      Services.obs.addObserver(this.observer, "network:offline-status-changed");
      Services.obs.addObserver(this.observer, "wake_notification");
      this.observing = true;
      this.scheduler.start();
    })();
    void this.starting.catch(async () => {
      this.settingsStop?.();
      this.settingsStop = null;
      await this.store?.close();
      this.store = null;
      this.scheduler = null;
      this.starting = null;
    }).catch(() => {});
    return this.starting;
  }
  private assertRunning(): void {
    if (this.closing || !this.store || !this.scheduler) {
      throw new Error("Feed storage is shutting down.");
    }
  }
  private async ready(): Promise<void> {
    await this.start();
    this.assertRunning();
  }
  subscribeChanges(listener: () => void): () => void {
    this.listeners.add(listener);
    return () => this.listeners.delete(listener);
  }
  private notify(): void {
    for (const listener of this.listeners) {
      try {
        listener();
      } catch { /* A closed page cannot block another observer. */ }
    }
  }
  async listFeeds(): Promise<FeedSummary[]> {
    await this.ready();
    const feeds = await this.store!.listFeeds();
    return feeds.map((feed) => ({
      ...feed,
      updating: this.scheduler?.isUpdating(feed.id) ?? false,
    }));
  }
  async queryArticles(query: ArticleQuery): Promise<ArticlePage> {
    await this.ready();
    return this.store!.queryArticles(query);
  }
  async getArticle(id: string): Promise<Article> {
    await this.ready();
    return this.store!.getArticle(id);
  }
  async subscribe(input: string): Promise<SubscribeResult> {
    const url = normaliseFeedUrl(input.trim());
    const existing = this.inspection.get(url);
    if (existing) return existing;
    const promise = this.subscribeOnce(url);
    this.inspection.set(url, promise);
    void promise.finally(() => this.inspection.delete(url)).catch(() => {});
    return await promise;
  }
  private async subscribeOnce(url: string): Promise<SubscribeResult> {
    await this.ready();
    const active = (await this.store!.listFeeds()).find((feed) =>
      feed.url === url
    );
    if (active) return { type: "subscribed", feed: active };
    this.assertRunning();
    if (Services.io.offline) throw new Error("The browser is offline.");
    const controller = new AbortController();
    this.inspections.add(controller);
    try {
      const response = await this.scheduler!.fetch(url, {}, controller.signal);
      if (response.status < 200 || response.status >= 300) {
        throw new Error(`The feed server returned HTTP ${response.status}.`);
      }
      this.assertRunning();
      const inspected = await inspectFeed(
        decodeFeedResponse(response),
        response.url,
        response.contentType,
      );
      if (inspected.type === "discovery") return inspected;
      const parsed = inspected.feed;
      this.assertRunning();
      const feed = await this.store!.addFeed(url, parsed.title, parsed.siteUrl);
      this.assertRunning();
      await this.store!.upsertFeed(feed.id, parsed, {
        etag: response.headers.etag ?? null,
        lastModified: response.headers["last-modified"] ?? null,
        nextAttemptAt: Date.now() +
          Math.max(15, SettingsService.get().rss.refreshMinutes) * 60_000,
      }, SettingsService.get().rss);
      this.notify();
      this.scheduler!.reschedule();
      const summary = (await this.store!.listFeeds()).find((item) =>
        item.id === feed.id
      );
      if (!summary) throw new Error("The subscription could not be saved.");
      return { type: "subscribed", feed: summary };
    } finally {
      this.inspections.delete(controller);
    }
  }
  async unsubscribe(id: string): Promise<void> {
    await this.ready();
    this.scheduler!.abort(id);
    await this.store!.unsubscribe(id);
    this.notify();
  }
  async refresh(ids?: string[]): Promise<void> {
    await this.ready();
    await this.scheduler!.refresh(ids);
  }
  async patchArticleState(
    ids: string[],
    changes: { isRead?: boolean; starred?: boolean },
  ): Promise<void> {
    await this.ready();
    if (
      !ids.length || ids.length > 1000 ||
      ids.some((id) => typeof id !== "string" || id.length > 256)
    ) throw new Error("Invalid article selection.");
    await this.store!.patchArticleState(ids, changes);
    this.notify();
  }
  async loadArticleImage(articleId: string, imageId: string): Promise<string> {
    await this.ready();
    const url = await this.store!.imageSource(articleId, imageId);
    this.assertRunning();
    const controller = new AbortController();
    this.imageRequests.add(controller);
    try {
      const response = await this.scheduler!.fetch(
        url,
        {},
        controller.signal,
        MAX_IMAGE_BYTES,
      );
      if (response.status < 200 || response.status >= 300) {
        throw new Error("The article image could not be downloaded.");
      }
      this.assertRunning();
      const type = response.contentType.toLowerCase().split(";", 1)[0];
      if (
        !["image/png", "image/jpeg", "image/gif", "image/webp", "image/avif"]
          .includes(type)
      ) throw new Error("This image format is not supported.");
      // Decode before returning a data URL; a small compressed file must not
      // allocate an unbounded surface in the privileged reader.
      const tools = Cc["@mozilla.org/image/tools;1"].getService(Ci.imgITools);
      const image = tools.decodeImageFromArrayBuffer(
        response.bytes.buffer,
        type,
      );
      if (
        image.width <= 0 || image.height <= 0 || image.width > 16384 ||
        image.height > 16384 || image.width * image.height > 32_000_000
      ) throw new Error("The article image is too large.");
      const encoded = ChromeUtils.base64URLEncode(
        response.bytes.buffer as ArrayBuffer,
        { pad: true },
      ).replaceAll("-", "+").replaceAll("_", "/");
      return `data:${type};base64,${encoded}`;
    } finally {
      this.imageRequests.delete(controller);
    }
  }
  stop(): Promise<void> {
    if (this.stopping) return this.stopping;
    this.closing = true;
    this.stopping = (async () => {
      await this.starting?.catch(() => {});
      this.inspections.forEach((controller) => controller.abort());
      this.imageRequests.forEach((controller) => controller.abort());
      await Promise.allSettled([...this.inspection.values()]);
      await this.scheduler?.stop();
      this.settingsStop?.();
      this.settingsStop = null;
      if (this.observing) {
        AsyncShutdown.profileBeforeChange.removeBlocker(this.shutdownBlocker);
        Services.obs.removeObserver(
          this.observer,
          "network:offline-status-changed",
        );
        Services.obs.removeObserver(this.observer, "wake_notification");
        this.observing = false;
      }
      await this.store?.close();
      this.store = null;
      this.scheduler = null;
      this.starting = null;
    })().finally(() => {
      this.closing = false;
      this.stopping = null;
    });
    return this.stopping;
  }
}

export const FeedService = new ProfileFeedService();
