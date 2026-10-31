// SPDX-License-Identifier: MPL-2.0
import type {
  FeedImageProps,
  FeedContentProps,
  ArticleStatePatch,
} from "./types.ts";
import { createElement, useEffect, useRef, useState } from "react";
import type { ReactNode } from "react";
import {
  ArrowUp,
  Check,
  ChevronLeft,
  ExternalLink,
  Menu,
  MoreHorizontal,
  Plus,
  RefreshCw,
  Rss,
  Search,
  Star,
  Trash2,
} from "lucide-react";
import { useTranslation } from "react-i18next";
import {
  newTabClient,
  NewTabClientError,
} from "../../../../../libs/newtab/client.ts";
import type {
  Article,
  ArticleSummary,
  SafeNode,
} from "../../../../../libs/newtab/types.ts";
import { messageOf, useNewTab } from "./state.tsx";
import { Button, FloatingMenu, IconButton, Modal, safeWebUrl } from "./ui.tsx";

const contentTags = new Set([
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
]);
function FeedImage({ articleId, imageId, alt }: FeedImageProps) {
  const { t } = useTranslation();
  const [source, setSource] = useState("");
  const [error, setError] = useState("");
  const [loading, setLoading] = useState(false);
  const load = async () => {
    setLoading(true);
    setError("");
    try {
      const result = await newTabClient.call("articleImage", {
        articleId,
        imageId,
      });
      if (
        !/^data:image\/(?:png|jpeg|gif|webp|avif);base64,[A-Za-z0-9+/=]+$/i.test(
          result,
        )
      )
        throw new Error(
          t("newtab.imageUnavailable", "This image could not be displayed."),
        );
      setSource(result);
    } catch (cause) {
      setError(messageOf(cause));
    } finally {
      setLoading(false);
    }
  };
  return source ? (
    <img className="nt-feed-image" src={source} alt={alt} />
  ) : (
    <span className="nt-feed-image-placeholder" aria-busy={loading}>
      <span>{alt || t("newtab.externalImage", "External image")}</span>
      <Button
        variant="secondary"
        disabled={loading}
        onClick={() => void load()}
      >
        {loading
          ? t("newtab.loading", "Loading…")
          : t("newtab.loadImage", "Load this image")}
      </Button>
      {error && <span role="alert">{error}</span>}
    </span>
  );
}
export function FeedContent({ nodes, articleId }: FeedContentProps) {
  const render = (node: SafeNode, index: number): ReactNode => {
    if (node.tag === "text") return node.text ?? "";
    if (node.tag === "img")
      return node.imageId ? (
        <FeedImage
          key={`${articleId}:${node.imageId}`}
          articleId={articleId}
          imageId={node.imageId}
          alt={node.alt ?? ""}
        />
      ) : (
        (node.alt ?? "")
      );
    const children = (node.children ?? []).map(render);
    if (node.tag === "a") {
      const href = safeWebUrl(node.href);
      return href ? (
        <a
          key={index}
          href={href}
          target="_blank"
          rel="noopener noreferrer"
          referrerPolicy="no-referrer"
        >
          {children}
        </a>
      ) : (
        <span key={index}>{children}</span>
      );
    }
    if (!contentTags.has(node.tag)) return <span key={index}>{children}</span>;
    // Keep one page h1: article-provided headings are rendered below its title.
    return createElement(
      node.tag === "h1" ? "h2" : node.tag,
      { key: index },
      ["br", "hr"].includes(node.tag) ? undefined : children,
    );
  };
  return <>{nodes.map(render)}</>;
}
function articleFromHash(): string | null {
  try {
    return (
      decodeURIComponent(location.hash.slice(1).split("/")[1] ?? "") || null
    );
  } catch {
    return null;
  }
}
export function Reader() {
  const { t } = useTranslation();
  const { snapshot, epoch, refresh, setDialog, notify } = useNewTab();
  const [online, setOnline] = useState(() => navigator.onLine);
  useEffect(() => {
    const update = () => setOnline(navigator.onLine);
    addEventListener("online", update);
    addEventListener("offline", update);
    return () => {
      removeEventListener("online", update);
      removeEventListener("offline", update);
    };
  }, []);
  const [feedId, setFeedId] = useState<string | undefined>();
  const [filter, setFilter] = useState<"all" | "unread" | "starred">("all");
  const [query, setQuery] = useState("");
  const [search, setSearch] = useState("");
  const [items, setItems] = useState<ArticleSummary[]>([]);
  const [total, setTotal] = useState(0);
  const [offset, setOffset] = useState(0);
  const [selectedId, setSelectedId] = useState<string | null>(articleFromHash);
  const [listSelection, setListSelection] = useState<string | null>(
    articleFromHash,
  );
  const [article, setArticle] = useState<Article | null>(null);
  const [loading, setLoading] = useState(false);
  const [loadingArticle, setLoadingArticle] = useState(false);
  const [refreshing, setRefreshing] = useState(false);
  const [error, setError] = useState("");
  const [articleError, setArticleError] = useState("");
  const [listRetry, setListRetry] = useState(0);
  const [articleRetry, setArticleRetry] = useState(0);
  const [drawer, setDrawer] = useState(false);
  const [menu, setMenu] = useState(false);
  const list = useRef<HTMLDivElement>(null);
  const body = useRef<HTMLDivElement>(null);
  const listHeading = useRef<HTMLHeadingElement>(null);
  const articleRequest = useRef(0);
  const actionTrigger = useRef<HTMLButtonElement>(null);
  const currentFeed = snapshot.feeds.find(
    (feed) => feed.id === (article?.feedId ?? feedId),
  );
  const updating =
    refreshing ||
    snapshot.feeds.some(
      (feed) => feed.updating && (!feedId || feed.id === feedId),
    );
  useEffect(() => {
    if (feedId && !snapshot.feeds.some((feed) => feed.id === feedId))
      setFeedId(undefined);
  }, [feedId, snapshot.feeds]);
  useEffect(() => {
    if (
      !article ||
      snapshot.feeds.some((feed) => feed.id === article.feedId && feed.enabled)
    )
      return;
    if (!article.starred) {
      setSelectedId(null);
      location.hash = "feed";
    } else if (article.feedEnabled !== false)
      setArticle((current) =>
        current ? { ...current, feedEnabled: false } : current,
      );
  }, [snapshot.feeds, article?.feedId, article?.starred]);
  useEffect(() => {
    const timer = setTimeout(() => setSearch(query.trim()), 150);
    return () => clearTimeout(timer);
  }, [query]);
  useEffect(() => {
    setOffset(0);
    list.current?.scrollTo({ top: 0 });
  }, [feedId, filter, search]);
  useEffect(() => {
    let active = true;
    setLoading(true);
    const pages = Array.from(
      { length: Math.floor(offset / 50) + 1 },
      (_, index) =>
        newTabClient.call("articles", {
          feedId,
          filter,
          query: search,
          offset: index * 50,
          limit: 50,
        }),
    );
    void Promise.all(pages)
      .then((results) => {
        if (active) {
          setItems(results.flatMap((result) => result.items));
          setTotal(results[0]?.total ?? 0);
          setError("");
        }
      })
      .catch((cause) => {
        if (active) setError(messageOf(cause));
      })
      .finally(() => {
        if (active) setLoading(false);
      });
    return () => {
      active = false;
    };
  }, [feedId, filter, search, offset, epoch, listRetry]);
  useEffect(() => {
    const sync = () => {
      const id = articleFromHash();
      setSelectedId(id);
      if (id) setListSelection(id);
    };
    addEventListener("hashchange", sync);
    return () => removeEventListener("hashchange", sync);
  }, []);
  useEffect(() => {
    const request = ++articleRequest.current;
    setMenu(false);
    if (!selectedId) {
      setArticle(null);
      setLoadingArticle(false);
      setArticleError("");
      return;
    }
    setArticle(null);
    setLoadingArticle(true);
    setArticleError("");
    void newTabClient
      .call("article", { id: selectedId })
      .then(async (next) => {
        if (request !== articleRequest.current) return;
        setArticle(next);
        body.current?.scrollTo({ top: 0 });
        if (!next.isRead && !snapshot.readOnly) {
          try {
            await newTabClient.call("articleState", {
              ids: [next.id],
              isRead: true,
            });
            if (request === articleRequest.current)
              setArticle((current) =>
                current?.id === next.id
                  ? { ...current, isRead: true }
                  : current,
              );
            await refresh();
          } catch (cause) {
            notify(messageOf(cause));
          }
        }
      })
      .catch((cause) => {
        if (request === articleRequest.current) {
          setArticle(null);
          setArticleError(messageOf(cause));
        }
      })
      .finally(() => {
        if (request === articleRequest.current) setLoadingArticle(false);
      });
    return () => {
      articleRequest.current++;
    };
  }, [selectedId, refresh, notify, snapshot.readOnly, articleRetry]);
  useEffect(() => {
    if (!selectedId || article?.id !== selectedId) return;
    let active = true;
    void newTabClient
      .call("article", { id: selectedId })
      .then((latest) => {
        if (active)
          setArticle((current) =>
            current?.id === latest.id ? latest : current,
          );
      })
      .catch((cause) => {
        if (!active) return;
        if (cause instanceof NewTabClientError && cause.code === "NOT_FOUND") {
          setSelectedId(null);
          location.hash = "feed";
        } else notify(messageOf(cause));
      });
    return () => {
      active = false;
    };
  }, [epoch, selectedId]);
  const open = (id: string) => {
    setListSelection(id);
    setSelectedId(id);
    location.hash = `feed/${encodeURIComponent(id)}`;
    // The list is hidden at narrow widths. Move keyboard navigation into the
    // article pane without resetting the list's scroll position.
    requestAnimationFrame(() => body.current?.focus({ preventScroll: true }));
  };
  const backToList = () => {
    const id = selectedId;
    setSelectedId(null);
    location.hash = "feed";
    requestAnimationFrame(() => {
      const previous = id
        ? list.current?.querySelector<HTMLButtonElement>(
            `[data-article-id="${CSS.escape(id)}"]`,
          )
        : null;
      (previous ?? listHeading.current)?.focus({ preventScroll: true });
    });
  };
  const choose = (id: string | undefined, nextFilter: typeof filter) => {
    setFeedId(id);
    setFilter(nextFilter);
    setDrawer(false);
  };
  const mutate = async (id: string, patch: ArticleStatePatch) => {
    try {
      await newTabClient.call("articleState", { ids: [id], ...patch });
      setItems((previous) =>
        previous.map((item) => (item.id === id ? { ...item, ...patch } : item)),
      );
      setArticle((previous) =>
        previous?.id === id ? { ...previous, ...patch } : previous,
      );
      await refresh();
    } catch (cause) {
      notify(messageOf(cause));
    }
  };
  const update = async () => {
    setRefreshing(true);
    try {
      await newTabClient.call("refresh", feedId ? { ids: [feedId] } : {});
      await refresh();
    } catch (cause) {
      notify(messageOf(cause));
    } finally {
      setRefreshing(false);
    }
  };
  const sources = (
    <>
      <div className="nt-reader-source-heading">
        <h2>{t("newtab.feeds", "Feeds")}</h2>
      </div>
      <Button
        variant="secondary"
        disabled={snapshot.readOnly}
        onClick={() => {
          setDrawer(false);
          setDialog({ type: "subscribe" });
        }}
      >
        <Plus size={17} />
        {t("newtab.addFeed", "Add feed")}
      </Button>
      <nav
        className="nt-reader-source-nav"
        aria-label={t("newtab.feedSources", "Feed sources")}
      >
        <button
          type="button"
          aria-current={!feedId && filter === "all" ? "page" : undefined}
          onClick={() => choose(undefined, "all")}
        >
          <Rss size={18} />
          {t("newtab.allArticles", "All articles")}
        </button>
        <button
          type="button"
          aria-current={!feedId && filter === "unread" ? "page" : undefined}
          onClick={() => choose(undefined, "unread")}
        >
          <span className="nt-ring" />
          {t("newtab.unread", "Unread")}
          <span className="nt-count">
            {snapshot.feeds.reduce((sum, feed) => sum + feed.unreadCount, 0)}
          </span>
        </button>
        <button
          type="button"
          aria-current={!feedId && filter === "starred" ? "page" : undefined}
          onClick={() => choose(undefined, "starred")}
        >
          <Star size={18} />
          {t("newtab.starred", "Starred")}
        </button>
      </nav>
      <p className="nt-reader-label">
        {t("newtab.subscriptions", "Subscriptions")}
      </p>
      <ul className="nt-reader-subscriptions">
        {snapshot.feeds.map((feed) => (
          <li key={feed.id}>
            <button
              type="button"
              aria-current={feedId === feed.id ? "page" : undefined}
              title={feed.title}
              onClick={() => choose(feed.id, "all")}
            >
              <Rss size={17} />
              <span>
                <span className="nt-subscription-title">{feed.title}</span>
                {!feed.enabled && (
                  <small>{t("newtab.unsubscribed", "Unsubscribed")}</small>
                )}
                {feed.updating ? (
                  <small>{t("newtab.updating", "Updating…")}</small>
                ) : (
                  feed.error && (
                    <small>{t("newtab.updateFailed", "Update failed")}</small>
                  )
                )}
              </span>
              <span className="nt-count">{feed.unreadCount}</span>
            </button>
            {feed.enabled && (
              <IconButton
                disabled={snapshot.readOnly}
                label={t("newtab.unsubscribeTitle", {
                  title: feed.title,
                  defaultValue: "Unsubscribe from {{title}}",
                })}
                onClick={() => {
                  setDrawer(false);
                  setDialog({
                    type: "unsubscribe",
                    id: feed.id,
                    title: feed.title,
                  });
                }}
              >
                <Trash2 size={15} />
              </IconButton>
            )}
          </li>
        ))}
      </ul>
    </>
  );
  const title =
    snapshot.feeds.find((feed) => feed.id === feedId)?.title ??
    t("newtab.allArticles", "All articles");
  const origin =
    safeWebUrl(article?.link) ??
    safeWebUrl(currentFeed?.siteUrl ?? article?.siteUrl);
  return (
    <main
      id="main-content"
      className="nt-reader"
      data-article-open={Boolean(selectedId)}
      tabIndex={-1}
    >
      <aside className="nt-reader-sources">{sources}</aside>
      {drawer && (
        <Modal
          title={t("newtab.feeds", "Feeds")}
          closeLabel={t("newtab.close", "Close")}
          close={() => setDrawer(false)}
        >
          <div className="nt-reader-drawer">{sources}</div>
        </Modal>
      )}
      <section
        className="nt-reader-list"
        aria-label={t("newtab.articleList", "Article list")}
      >
        <header>
          <div className="nt-reader-list-heading">
            <IconButton
              className="nt-sources-toggle"
              label={t("newtab.openSources", "Choose feed")}
              onClick={() => setDrawer(true)}
            >
              <Menu size={20} />
            </IconButton>
            {!selectedId ? (
              <h1 ref={listHeading} tabIndex={-1}>
                {title}
              </h1>
            ) : (
              <h2 ref={listHeading} tabIndex={-1}>
                {title}
              </h2>
            )}
            <IconButton
              disabled={updating || snapshot.readOnly}
              label={t("newtab.refreshFeeds", "Refresh feeds")}
              onClick={() => void update()}
            >
              <RefreshCw size={18} className={updating ? "nt-spin" : ""} />
            </IconButton>
          </div>
          <label className="nt-reader-search">
            <Search size={18} />
            <span className="nt-sr-only">
              {t("newtab.searchArticles", "Search articles")}
            </span>
            <input
              type="search"
              value={query}
              onChange={(event) => setQuery(event.currentTarget.value)}
              placeholder={t("newtab.searchArticles", "Search articles")}
            />
          </label>
          <div
            className="nt-reader-filters"
            role="group"
            aria-label={t("newtab.articleFilter", "Article filter")}
          >
            {(["all", "unread", "starred"] as const).map((value) => (
              <button
                type="button"
                key={value}
                aria-pressed={filter === value}
                onClick={() => setFilter(value)}
              >
                {t(
                  `newtab.${value}`,
                  value === "all"
                    ? "All"
                    : value === "unread"
                      ? "Unread"
                      : "Starred",
                )}
              </button>
            ))}
          </div>
          {!online && (
            <p className="nt-reader-updated" role="status">
              {t(
                "newtab.offline",
                "Offline. Saved articles are still available.",
              )}
            </p>
          )}
          {currentFeed?.lastSuccessAt && (
            <p className="nt-reader-updated">
              {t("newtab.lastUpdated", "Last updated")}{" "}
              {new Date(currentFeed.lastSuccessAt).toLocaleString()}
            </p>
          )}
        </header>
        <div className="nt-reader-list-scroll" ref={list} aria-busy={loading}>
          {error && (
            <div className="nt-reader-status">
              <p role="alert" className="nt-form-error">
                {error}
              </p>
              <Button
                variant="secondary"
                disabled={loading}
                onClick={() => setListRetry((current) => current + 1)}
              >
                {t("newtab.retry", "Retry")}
              </Button>
            </div>
          )}
          {!loading && !error && items.length === 0 && (
            <div className="nt-empty">
              <Rss size={30} />
              <p>
                {snapshot.feeds.length
                  ? t("newtab.noArticles", "No matching articles")
                  : t(
                      "newtab.addFeedHint",
                      "Add a feed to see its latest articles here.",
                    )}
              </p>
              {!snapshot.feeds.length && (
                <Button
                  variant="secondary"
                  disabled={snapshot.readOnly}
                  onClick={() => setDialog({ type: "subscribe" })}
                >
                  <Plus size={17} />
                  {t("newtab.addFeed", "Add feed")}
                </Button>
              )}
            </div>
          )}
          {items.map((item) => (
            <article
              className="nt-reader-list-item"
              key={item.id}
              data-selected={item.id === listSelection}
              data-unread={!item.isRead}
            >
              <button
                type="button"
                data-article-id={item.id}
                aria-current={item.id === listSelection ? "true" : undefined}
                onClick={() => open(item.id)}
              >
                <span className="nt-reader-item-source">
                  <span
                    className="nt-reader-item-source-title"
                    title={snapshot.feeds.find((feed) => feed.id === item.feedId)
                      ?.title ?? item.feedTitle}
                  >
                    {snapshot.feeds.find((feed) => feed.id === item.feedId)
                      ?.title ?? item.feedTitle}
                  </span>
                  {!item.isRead && (
                    <span
                      className="nt-unread-dot"
                      aria-label={t("newtab.unread", "Unread")}
                    />
                  )}
                </span>
                <strong>{item.title}</strong>
                {item.excerpt && <p>{item.excerpt}</p>}
                <time
                  dateTime={new Date(
                    item.publishedAt ?? item.receivedAt,
                  ).toISOString()}
                >
                  {new Date(
                    item.publishedAt ?? item.receivedAt,
                  ).toLocaleDateString()}
                </time>
              </button>
              <IconButton
                disabled={snapshot.readOnly}
                aria-pressed={item.starred}
                label={t("newtab.toggleStar", {
                  title: item.title,
                  defaultValue: "Toggle star for {{title}}",
                })}
                onClick={() => void mutate(item.id, { starred: !item.starred })}
              >
                <Star size={17} fill={item.starred ? "currentColor" : "none"} />
              </IconButton>
            </article>
          ))}
          {loading && (
            <p className="nt-reader-loading" role="status">
              {t("newtab.loading", "Loading…")}
            </p>
          )}
          {items.length < total && (
            <Button
              variant="secondary"
              className="nt-load-more"
              disabled={loading}
              onClick={() => setOffset(items.length)}
            >
              {t("newtab.loadMore", "Load more")}
            </Button>
          )}
        </div>
      </section>
      <section
        className="nt-reader-article"
        aria-label={t("newtab.articleBody", "Article body")}
      >
        <header className="nt-reader-article-toolbar">
          <IconButton
            className="nt-back-to-list"
            label={t("newtab.backToList", "Back to article list")}
            onClick={backToList}
          >
            <ChevronLeft size={20} />
          </IconButton>
          <span>
            {currentFeed?.title ?? article?.feedTitle}
            {article?.feedEnabled === false && (
              <small> — {t("newtab.unsubscribed", "Unsubscribed")}</small>
            )}
          </span>
          {article && (
            <>
              <IconButton
                disabled={snapshot.readOnly}
                aria-pressed={article.starred}
                label={t("newtab.toggleStar", {
                  title: article.title,
                  defaultValue: "Toggle star for {{title}}",
                })}
                onClick={() =>
                  void mutate(article.id, { starred: !article.starred })
                }
              >
                <Star
                  size={20}
                  fill={article.starred ? "currentColor" : "none"}
                />
              </IconButton>
              <div className="nt-reader-actions">
                <IconButton
                  ref={actionTrigger}
                  label={t("newtab.articleActions", "Article actions")}
                  aria-expanded={menu}
                  onClick={() => setMenu(!menu)}
                >
                  <MoreHorizontal size={20} />
                </IconButton>
                {menu && (
                  <FloatingMenu
                    anchor={actionTrigger}
                    close={() => setMenu(false)}
                    label={t("newtab.articleActions", "Article actions")}
                  >
                    <button
                      type="button"
                      disabled={snapshot.readOnly}
                      onClick={() => {
                        setMenu(false);
                        actionTrigger.current?.focus({ preventScroll: true });
                        void mutate(article.id, { isRead: !article.isRead });
                      }}
                    >
                      <Check size={17} />
                      {article.isRead
                        ? t("newtab.markUnread", "Mark unread")
                        : t("newtab.markRead", "Mark read")}
                    </button>
                  </FloatingMenu>
                )}
              </div>
              {origin && (
                <a
                  className="nt-external-button"
                  href={origin}
                  target="_blank"
                  rel="noopener noreferrer"
                  referrerPolicy="no-referrer"
                >
                  <ExternalLink size={16} />
                  {safeWebUrl(article.link)
                    ? t("newtab.openOriginal", "Open original site")
                    : t("newtab.openPublisher", "Open publisher site")}
                </a>
              )}
            </>
          )}
        </header>
        <div
          className="nt-reader-article-scroll"
          ref={body}
          tabIndex={-1}
          aria-busy={loadingArticle}
        >
          {loadingArticle ? (
            <div className="nt-empty" role="status">
              {t("newtab.loading", "Loading…")}
            </div>
          ) : article ? (
            <article className="nt-article-content">
              <h1 tabIndex={-1}>{article.title}</h1>
              <p className="nt-article-byline">
                <time
                  dateTime={new Date(
                    article.publishedAt ?? article.receivedAt,
                  ).toISOString()}
                >
                  {new Date(
                    article.publishedAt ?? article.receivedAt,
                  ).toLocaleDateString()}
                </time>
                {article.author && <span>{article.author}</span>}
              </p>
              {article.content.length ? (
                <div className="nt-feed-content">
                  <FeedContent nodes={article.content} articleId={article.id} />
                </div>
              ) : (
                <p>
                  {t(
                    "newtab.noBody",
                    "This feed did not include an article body.",
                  )}
                </p>
              )}
              <footer className="nt-article-footer">
                <p>
                  {t(
                    "newtab.distributedContent",
                    "This is the content supplied by the feed.",
                  )}
                </p>
                {origin && (
                  <a
                    href={origin}
                    target="_blank"
                    rel="noopener noreferrer"
                    referrerPolicy="no-referrer"
                  >
                    {safeWebUrl(article.link)
                      ? t(
                          "newtab.continueOriginal",
                          "Continue on the original site",
                        )
                      : t("newtab.openPublisher", "Open publisher site")}
                    <ExternalLink size={15} />
                  </a>
                )}
              </footer>
              <Button
                variant="ghost"
                onClick={() =>
                  body.current?.scrollTo({
                    top: 0,
                    behavior: matchMedia("(prefers-reduced-motion: reduce)")
                      .matches
                      ? "instant"
                      : "smooth",
                  })
                }
              >
                <ArrowUp size={17} />
                {t("newtab.backToTop", "Back to top")}
              </Button>
            </article>
          ) : (
            <div className="nt-empty">
              {articleError ? (
                <>
                  <p className="nt-form-error" role="alert">
                    {articleError}
                  </p>
                  <Button
                    variant="secondary"
                    onClick={() => setArticleRetry((current) => current + 1)}
                  >
                    {t("newtab.retry", "Retry")}
                  </Button>
                </>
              ) : (
                <>
                  <Rss size={36} />
                  <h2>
                    {t("newtab.chooseArticle", "Choose an article to read")}
                  </h2>
                  <p>
                    {t(
                      "newtab.feedOnlyHint",
                      "Only content supplied by the feed is displayed. Articles are not fetched from the original site.",
                    )}
                  </p>
                </>
              )}
            </div>
          )}
        </div>
      </section>
    </main>
  );
}
