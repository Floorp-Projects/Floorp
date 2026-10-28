// SPDX-License-Identifier: MPL-2.0
import type {
  Article,
  ArticlePage,
  ArticleQuery,
  ArticleSummary,
  FeedSummary,
  SafeNode,
  SafeTag,
} from "#libs/newtab/types.ts";
import { normaliseFeedUrl, stableHash } from "./FeedParser.sys.mts";
import type { ParsedFeed, StoredFeed } from "./types.ts";

interface Row {
  getResultByName(name: string): unknown;
}
type BindValue = string | number | null;
interface Connection {
  execute(sql: string, params?: Record<string, BindValue>): Promise<Row[]>;
  executeTransaction<T>(work: () => Promise<T>): Promise<T>;
  close(): Promise<void>;
}
const { Sqlite } = ChromeUtils.importESModule(
  "resource://gre/modules/Sqlite.sys.mjs",
) as {
  Sqlite: { openConnection(options: { path: string }): Promise<Connection> };
};
const SCHEMA_VERSION = 2;
const DATABASE_BYTES = 100 * 1024 * 1024;
const TOMBSTONE_DAYS = 365;
const MAX_TOMBSTONES = 100_000;
const ALLOWED_TAGS = new Set<SafeTag>([
  "text",
  "p",
  "h1",
  "h2",
  "h3",
  "h4",
  "ul",
  "ol",
  "li",
  "strong",
  "em",
  "blockquote",
  "pre",
  "code",
  "br",
  "hr",
  "a",
  "img",
]);
const number = (row: Row, name: string): number =>
  Number(row.getResultByName(name));
const string = (row: Row, name: string): string =>
  String(row.getResultByName(name) ?? "");
const nullableString = (row: Row, name: string): string | null =>
  row.getResultByName(name) === null ? null : string(row, name);
const nullableNumber = (row: Row, name: string): number | null =>
  row.getResultByName(name) === null ? null : number(row, name);

function parseContent(raw: string): SafeNode[] {
  const value: unknown = JSON.parse(raw);
  let count = 0;
  const validate = (nodes: unknown, depth: number): SafeNode[] => {
    if (!Array.isArray(nodes) || depth > 64) {
      throw new Error("The stored feed content is invalid.");
    }
    return nodes.map((node: unknown) => {
      if (!node || typeof node !== "object" || ++count > 20_000) {
        throw new Error("The stored feed content is invalid.");
      }
      const item = node as Record<string, unknown>;
      if (
        typeof item.tag !== "string" || !ALLOWED_TAGS.has(item.tag as SafeTag)
      ) throw new Error("The stored feed content is invalid.");
      const result: SafeNode = { tag: item.tag as SafeTag };
      if (typeof item.text === "string") result.text = item.text;
      if (typeof item.alt === "string") result.alt = item.alt;
      if (typeof item.imageId === "string") result.imageId = item.imageId;
      if (typeof item.href === "string") {
        try {
          result.href = normaliseFeedUrl(item.href);
        } catch { /* Never return a dangerous stored link. */ }
      }
      if (item.children !== undefined) {
        result.children = validate(item.children, depth + 1);
      }
      return result;
    });
  };
  return validate(value, 0);
}

function toFeed(row: Row): StoredFeed {
  return {
    id: string(row, "id"),
    url: string(row, "url"),
    title: string(row, "title"),
    siteUrl: nullableString(row, "site_url"),
    enabled: Boolean(number(row, "enabled")),
    etag: nullableString(row, "etag"),
    lastModified: nullableString(row, "last_modified"),
    lastSuccessAt: nullableNumber(row, "last_success"),
    nextAttemptAt: number(row, "next_attempt"),
    retryAfterAt: nullableNumber(row, "retry_after"),
    failureCount: number(row, "failures"),
    error: nullableString(row, "error"),
  };
}
function toArticle(row: Row): ArticleSummary {
  return {
    id: string(row, "id"),
    feedId: string(row, "feed_id"),
    title: string(row, "title"),
    link: nullableString(row, "link"),
    author: string(row, "author"),
    publishedAt: nullableNumber(row, "published"),
    receivedAt: number(row, "received"),
    excerpt: string(row, "excerpt"),
    isRead: Boolean(number(row, "is_read")),
    starred: Boolean(number(row, "starred")),
    hasFullContent: Boolean(number(row, "full_content")),
    feedTitle: string(row, "feed_title"),
    feedEnabled: Boolean(number(row, "feed_enabled")),
    siteUrl: nullableString(row, "feed_site_url"),
  };
}

/** A single serialized connection owns subscriptions, content, state and quotas.
 * It never deletes or recreates a corrupt/unknown-version database. */
export class FeedStore {
  private connection: Connection | null = null;
  private opening: Promise<Connection> | null = null;
  private queue: Promise<void> = Promise.resolve();
  constructor(
    private readonly path = PathUtils.join(
      PathUtils.profileDir,
      "floorp",
      "feeds.sqlite",
    ),
  ) {}

  async open(): Promise<void> {
    await this.database();
  }
  private database(): Promise<Connection> {
    if (this.connection) return Promise.resolve(this.connection);
    if (this.opening) return this.opening;
    this.opening = (async () => {
      if (
        Services.appinfo.processType !== Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT
      ) throw new Error("Only the browser parent may open feed storage.");
      const directory = PathUtils.parent(this.path);
      if (!directory) throw new Error("Invalid feed storage path.");
      await IOUtils.makeDirectory(directory, {
        createAncestors: true,
        ignoreExisting: true,
        permissions: 0o700,
      });
      const database = await Sqlite.openConnection({ path: this.path });
      try {
        const rows = await database.execute("PRAGMA user_version");
        const version = number(rows[0], "user_version");
        if (version !== 0 && version !== 1 && version !== SCHEMA_VERSION) {
          throw new Error(
            "This feed database was created by a newer version of Floorp.",
          );
        }
        await database.execute("PRAGMA foreign_keys = ON");
        const sizes = await database.execute("PRAGMA page_size");
        const maxPages = Math.floor(
          DATABASE_BYTES / number(sizes[0], "page_size"),
        );
        await database.execute(`PRAGMA max_page_count = ${maxPages}`);
        if (version === 0) {
          await database.executeTransaction(async () => {
            await database.execute(
              "CREATE TABLE feeds (id TEXT PRIMARY KEY, url TEXT NOT NULL UNIQUE, title TEXT NOT NULL, site_url TEXT, enabled INTEGER NOT NULL DEFAULT 1, etag TEXT, last_modified TEXT, last_success INTEGER, next_attempt INTEGER NOT NULL DEFAULT 0, retry_after INTEGER, failures INTEGER NOT NULL DEFAULT 0, error TEXT)",
            );
            await database.execute(
              "CREATE TABLE articles (id TEXT PRIMARY KEY, feed_id TEXT NOT NULL REFERENCES feeds(id), source_key TEXT NOT NULL, title TEXT NOT NULL, link TEXT, author TEXT NOT NULL, published INTEGER, received INTEGER NOT NULL, excerpt TEXT NOT NULL, full_content INTEGER NOT NULL, content TEXT NOT NULL, images TEXT NOT NULL, is_read INTEGER NOT NULL DEFAULT 0, starred INTEGER NOT NULL DEFAULT 0, UNIQUE(feed_id,source_key))",
            );
            await database.execute(
              "CREATE INDEX articles_feed_order ON articles(feed_id,published,received)",
            );
            await database.execute(
              "CREATE INDEX articles_state ON articles(starred,is_read)",
            );
            await database.execute(
              "CREATE TABLE tombstones (feed_id TEXT NOT NULL REFERENCES feeds(id), source_key TEXT NOT NULL, is_read INTEGER NOT NULL, updated INTEGER NOT NULL, PRIMARY KEY(feed_id,source_key))",
            );
            await database.execute(`PRAGMA user_version = ${SCHEMA_VERSION}`);
          });
        }
        if (version === 1) {
          await database.executeTransaction(async () => {
            await database.execute(
              "ALTER TABLE feeds ADD COLUMN retry_after INTEGER",
            );
            await database.execute(`PRAGMA user_version = ${SCHEMA_VERSION}`);
          });
        }
        this.connection = database;
        return database;
      } catch (cause) {
        await database.close();
        throw cause;
      }
    })();
    this.opening.catch(() => {
      this.opening = null;
    });
    return this.opening;
  }
  private serial<T>(work: (database: Connection) => Promise<T>): Promise<T> {
    const next = this.queue.then(async () => work(await this.database()));
    this.queue = next.then(() => {}, () => {});
    return next;
  }

  listFeeds(): Promise<FeedSummary[]> {
    return this.serial(async (db) =>
      (await db.execute(
        "SELECT f.*, (SELECT COUNT(*) FROM articles a WHERE a.feed_id=f.id AND a.is_read=0) AS unread FROM feeds f WHERE enabled=1 ORDER BY title COLLATE NOCASE,id",
      )).map((row) => ({
        id: string(row, "id"),
        url: string(row, "url"),
        title: string(row, "title"),
        siteUrl: nullableString(row, "site_url"),
        enabled: Boolean(number(row, "enabled")),
        unreadCount: number(row, "unread"),
        lastSuccessAt: nullableNumber(row, "last_success"),
        error: nullableString(row, "error"),
      }))
    );
  }
  storedFeeds(enabledOnly = true): Promise<StoredFeed[]> {
    return this.serial(async (db) =>
      (await db.execute(
        `SELECT * FROM feeds ${
          enabledOnly ? "WHERE enabled=1" : ""
        } ORDER BY id`,
      )).map(toFeed)
    );
  }
  getFeed(id: string): Promise<StoredFeed | null> {
    return this.serial(async (db) => {
      const rows = await db.execute("SELECT * FROM feeds WHERE id=:id", { id });
      return rows[0] ? toFeed(rows[0]) : null;
    });
  }
  addFeed(
    url: string,
    title: string,
    siteUrl: string | null,
  ): Promise<StoredFeed> {
    return this.serial(async (db) => {
      const existing = await db.execute("SELECT * FROM feeds WHERE url=:url", {
        url,
      });
      if (existing[0]) {
        await db.execute(
          "UPDATE feeds SET enabled=1, next_attempt=0, retry_after=NULL, error=NULL WHERE url=:url",
          { url },
        );
        return {
          ...toFeed(existing[0]),
          enabled: true,
          nextAttemptAt: 0,
          retryAfterAt: null,
          error: null,
        };
      }
      const count = await db.execute(
        "SELECT COUNT(*) AS count FROM feeds WHERE enabled=1",
      );
      if (number(count[0], "count") >= 1000) {
        throw new Error(
          "The maximum number of feed subscriptions has been reached.",
        );
      }
      const id = Services.uuid.generateUUID().toString().replace(/[{}]/g, "");
      await db.execute(
        "INSERT INTO feeds(id,url,title,site_url) VALUES(:id,:url,:title,:siteUrl)",
        { id, url, title, siteUrl },
      );
      return {
        id,
        url,
        title,
        siteUrl,
        enabled: true,
        etag: null,
        lastModified: null,
        lastSuccessAt: null,
        nextAttemptAt: 0,
        retryAfterAt: null,
        failureCount: 0,
        error: null,
      };
    });
  }

  upsertFeed(
    id: string,
    parsed: ParsedFeed,
    metadata: {
      etag: string | null;
      lastModified: string | null;
      nextAttemptAt: number;
    },
    limits: { retentionDays: number; maxItems: number },
  ): Promise<void> {
    return this.serial(async (db) => {
      const feed = await db.execute("SELECT enabled FROM feeds WHERE id=:id", {
        id,
      });
      if (!feed[0] || !number(feed[0], "enabled")) return;
      const now = Date.now();
      const prepared = await Promise.all(
        parsed.articles.map(async (article) => ({
          article,
          id: await stableHash(`${id}\n${article.sourceKey}`),
        })),
      );
      await db.executeTransaction(async () => {
        for (const { article, id: articleId } of prepared) {
          await db.execute(
            "INSERT INTO articles(id,feed_id,source_key,title,link,author,published,received,excerpt,full_content,content,images,is_read) VALUES(:articleId,:feedId,:sourceKey,:title,:link,:author,:published,:received,:excerpt,:fullContent,:content,:images,COALESCE((SELECT is_read FROM tombstones WHERE feed_id=:feedId AND source_key=:sourceKey),0)) ON CONFLICT(feed_id,source_key) DO UPDATE SET title=excluded.title, link=excluded.link, author=excluded.author, published=excluded.published, excerpt=excluded.excerpt, full_content=excluded.full_content, content=excluded.content, images=excluded.images",
            {
              articleId,
              feedId: id,
              sourceKey: article.sourceKey,
              title: article.title,
              link: article.link,
              author: article.author,
              published: article.publishedAt,
              received: now,
              excerpt: article.excerpt,
              fullContent: article.hasFullContent ? 1 : 0,
              content: JSON.stringify(article.content),
              images: JSON.stringify(article.images),
            },
          );
        }
        await this.prune(db, id, limits.retentionDays, limits.maxItems, now);
        await db.execute(
          "UPDATE feeds SET title=:title,site_url=:siteUrl,etag=:etag,last_modified=:lastModified,last_success=:now,next_attempt=:next,retry_after=NULL,failures=0,error=NULL WHERE id=:id",
          {
            id,
            title: parsed.title,
            siteUrl: parsed.siteUrl,
            etag: metadata.etag,
            lastModified: metadata.lastModified,
            now,
            next: metadata.nextAttemptAt,
          },
        );
      });
    });
  }
  markNotModified(id: string, nextAttemptAt: number): Promise<void> {
    return this.serial(async (db) => {
      await db.execute(
        "UPDATE feeds SET last_success=:now,next_attempt=:next,retry_after=NULL,failures=0,error=NULL WHERE id=:id AND enabled=1",
        { id, now: Date.now(), next: nextAttemptAt },
      );
    });
  }
  markFailure(
    id: string,
    error: string,
    nextAttemptAt: number,
    retryAfterAt: number | null = null,
  ): Promise<void> {
    return this.serial(async (db) => {
      await db.execute(
        "UPDATE feeds SET failures=failures+1,error=:error,next_attempt=:next,retry_after=:retryAfter WHERE id=:id AND enabled=1",
        { id, error, next: nextAttemptAt, retryAfter: retryAfterAt },
      );
    });
  }

  private async prune(
    db: Connection,
    feedId: string,
    days: number,
    maxItems: number,
    now: number,
  ): Promise<void> {
    const cutoff = now - Math.max(1, days) * 86400000;
    const limit = Math.max(1, Math.min(5000, Math.trunc(maxItems)));
    const expired =
      "SELECT id FROM articles WHERE feed_id=:feedId AND starred=0 AND (received<:cutoff OR id IN (SELECT id FROM articles WHERE feed_id=:feedId AND starred=0 ORDER BY COALESCE(published,received) DESC,received DESC,id LIMIT -1 OFFSET :limit))";
    const params = { feedId, cutoff, limit };
    await db.execute(
      `INSERT OR REPLACE INTO tombstones(feed_id,source_key,is_read,updated) SELECT feed_id,source_key,is_read,:now FROM articles WHERE id IN (${expired})`,
      { ...params, now },
    );
    await db.execute(`DELETE FROM articles WHERE id IN (${expired})`, params);
    await db.execute(
      "DELETE FROM tombstones WHERE updated<:cutoff OR rowid IN (SELECT rowid FROM tombstones ORDER BY updated DESC LIMIT -1 OFFSET :limit)",
      { cutoff: now - TOMBSTONE_DAYS * 86400000, limit: MAX_TOMBSTONES },
    );
  }

  unsubscribe(id: string): Promise<void> {
    return this.serial((db) =>
      db.executeTransaction(async () => {
        const now = Date.now();
        await db.execute(
          "UPDATE feeds SET enabled=0,next_attempt=0 WHERE id=:id",
          { id },
        );
        await db.execute(
          "INSERT OR REPLACE INTO tombstones(feed_id,source_key,is_read,updated) SELECT feed_id,source_key,is_read,:now FROM articles WHERE feed_id=:id AND starred=0",
          { id, now },
        );
        await db.execute(
          "DELETE FROM articles WHERE feed_id=:id AND starred=0",
          { id },
        );
        await db.execute(
          "DELETE FROM tombstones WHERE updated<:cutoff OR rowid IN (SELECT rowid FROM tombstones ORDER BY updated DESC LIMIT -1 OFFSET :limit)",
          { cutoff: now - TOMBSTONE_DAYS * 86400000, limit: MAX_TOMBSTONES },
        );
      })
    );
  }

  queryArticles(query: ArticleQuery): Promise<ArticlePage> {
    return this.serial(async (db) => {
      const conditions = ["(f.enabled=1 OR a.starred=1)"];
      const params: Record<string, BindValue> = {};
      if (query.feedId) {
        conditions.push("a.feed_id=:feedId");
        params.feedId = query.feedId;
      }
      if (query.filter === "unread") conditions.push("a.is_read=0");
      if (query.filter === "starred") conditions.push("a.starred=1");
      if (query.query?.trim()) {
        conditions.push(
          "(instr(lower(a.title),lower(:query))>0 OR instr(lower(a.excerpt),lower(:query))>0)",
        );
        params.query = query.query.trim().slice(0, 500);
      }
      const where = conditions.join(" AND ");
      const counts = await db.execute(
        `SELECT COUNT(*) AS total FROM articles a JOIN feeds f ON f.id=a.feed_id WHERE ${where}`,
        params,
      );
      const total = number(counts[0], "total");
      const offset = Math.max(0, Math.trunc(query.offset ?? 0));
      const limit = Math.max(1, Math.min(100, Math.trunc(query.limit ?? 50)));
      const rows = await db.execute(
        `SELECT a.*,f.title AS feed_title,f.enabled AS feed_enabled,f.site_url AS feed_site_url FROM articles a JOIN feeds f ON f.id=a.feed_id WHERE ${where} ORDER BY COALESCE(a.published,a.received) DESC,a.received DESC,a.id LIMIT :limit OFFSET :offset`,
        { ...params, limit, offset },
      );
      return {
        items: rows.map(toArticle),
        total,
        nextOffset: offset + rows.length < total ? offset + rows.length : null,
      };
    });
  }
  getArticle(id: string): Promise<Article> {
    return this.serial(async (db) => {
      const rows = await db.execute(
        "SELECT a.*,f.title AS feed_title,f.enabled AS feed_enabled,f.site_url AS feed_site_url FROM articles a JOIN feeds f ON f.id=a.feed_id WHERE a.id=:id AND (f.enabled=1 OR a.starred=1)",
        { id },
      );
      if (!rows[0]) throw new Error("The article is no longer available.");
      return {
        ...toArticle(rows[0]),
        content: parseContent(string(rows[0], "content")),
      };
    });
  }
  imageSource(articleId: string, imageId: string): Promise<string> {
    return this.serial(async (db) => {
      const rows = await db.execute(
        "SELECT a.images FROM articles a JOIN feeds f ON f.id=a.feed_id WHERE a.id=:articleId AND (f.enabled=1 OR a.starred=1)",
        { articleId },
      );
      if (!rows[0]) throw new Error("The article image is unavailable.");
      const images: unknown = JSON.parse(string(rows[0], "images"));
      if (
        !images || typeof images !== "object" || !Object.hasOwn(images, imageId)
      ) throw new Error("The image is not part of this article.");
      const url = (images as Record<string, unknown>)[imageId];
      if (typeof url !== "string") {
        throw new Error("The image is not part of this article.");
      }
      return normaliseFeedUrl(url);
    });
  }
  patchArticleState(
    ids: string[],
    changes: { isRead?: boolean; starred?: boolean },
  ): Promise<void> {
    return this.serial((db) =>
      db.executeTransaction(async () => {
        const fields: string[] = [];
        const params: Record<string, BindValue> = {};
        if (typeof changes.isRead === "boolean") {
          fields.push("is_read=:isRead");
          params.isRead = changes.isRead ? 1 : 0;
        }
        if (typeof changes.starred === "boolean") {
          fields.push("starred=:starred");
          params.starred = changes.starred ? 1 : 0;
        }
        if (!fields.length) return;
        for (const id of ids) {
          await db.execute(
            `UPDATE articles SET ${fields.join(",")} WHERE id=:id`,
            { ...params, id },
          );
        }
        // Once the final star is explicitly removed from an unsubscribed article,
        // it belongs to the same deletion policy as other non-starred cache.
        await db.execute(
          "DELETE FROM articles WHERE starred=0 AND feed_id IN (SELECT id FROM feeds WHERE enabled=0)",
        );
      })
    );
  }
  async close(): Promise<void> {
    await this.queue;
    const db = this.connection;
    this.connection = null;
    this.opening = null;
    if (db) await db.close();
  }
}
