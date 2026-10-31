import React, { useEffect, useMemo, useRef, useState } from "react";
import {
  ArrowLeft,
  Check,
  Circle,
  ExternalLink,
  LayoutList,
  Menu,
  MoreHorizontal,
  Plus,
  Rss,
  Search,
  Star,
  X,
} from "lucide-react";
import { articles } from "./data.ts";
import { usePrototype } from "./store.tsx";
import { BrandIcon, Button, EmptyState, IconButton, Input } from "./ui.tsx";
import type { FeedFilter } from "./types.ts";
import "./reader.css";

const filters: { value: FeedFilter; label: string }[] = [
  { value: "all", label: "すべて" },
  { value: "unread", label: "未読" },
  { value: "starred", label: "スター" },
];

function readableDate(value: string): string {
  const date = new Date(value);
  if (Number.isNaN(date.getTime())) return value;
  return new Intl.DateTimeFormat("ja-JP", {
    year: "numeric",
    month: "long",
    day: "numeric",
    timeZone: "Asia/Tokyo",
  }).format(date);
}

export function Reader() {
  const {
    data,
    setData,
    selectedArticleId,
    openArticle,
    feedFilter,
    setFeedFilter,
    selectedFeedId,
    setSelectedFeedId,
    feedQuery,
    setFeedQuery,
    setDialog,
    showToast,
  } = usePrototype();
  const [sourcesOpen, setSourcesOpen] = useState(false);
  const [detailOpen, setDetailOpen] = useState(selectedArticleId !== null);
  const [articleMenuOpen, setArticleMenuOpen] = useState(false);
  const listRef = useRef<HTMLDivElement>(null);
  const listScrollTop = useRef(0);
  const articleScrollRef = useRef<HTMLDivElement>(null);
  const articleHeadingRef = useRef<HTMLHeadingElement>(null);
  const feedIds = useMemo(
    () => new Set(data.feeds.map((feed) => feed.id)),
    [data.feeds],
  );
  const readIds = useMemo(() => new Set(data.readIds), [data.readIds]);
  const starredIds = useMemo(() => new Set(data.starredIds), [data.starredIds]);
  const availableArticles = articles.filter((article) =>
    feedIds.has(article.feedId),
  );
  const selectedFeed = data.feeds.find((feed) => feed.id === selectedFeedId);
  const normalizedQuery = feedQuery.trim().toLocaleLowerCase("ja-JP");
  const matchesSourceAndQuery = (article: (typeof articles)[number]) =>
    (!selectedFeedId || article.feedId === selectedFeedId) &&
    (!normalizedQuery ||
      `${article.title} ${article.excerpt}`
        .toLocaleLowerCase("ja-JP")
        .includes(normalizedQuery));
  const visibleArticles = availableArticles.filter(
    (article) =>
      matchesSourceAndQuery(article) &&
      (feedFilter !== "unread" || !readIds.has(article.id)) &&
      (feedFilter !== "starred" || starredIds.has(article.id)),
  );
  const selectedArticle = availableArticles.find(
    (article) =>
      article.id === selectedArticleId &&
      matchesSourceAndQuery(article) &&
      (feedFilter !== "starred" || starredIds.has(article.id)),
  );
  const currentArticle = selectedArticle ?? visibleArticles[0] ?? null;
  const currentFeed = data.feeds.find(
    (feed) => feed.id === currentArticle?.feedId,
  );
  const totalUnread = availableArticles.filter(
    (article) => !readIds.has(article.id),
  ).length;
  const currentStarred = currentArticle
    ? starredIds.has(currentArticle.id)
    : false;
  const currentRead = currentArticle ? readIds.has(currentArticle.id) : false;
  let sourceUrl = "https://blog.floorp.app/";
  if (currentArticle || currentFeed) {
    try {
      const candidate = new URL(currentArticle?.url ?? currentFeed!.url);
      if (candidate.protocol === "http:" || candidate.protocol === "https:")
        sourceUrl = candidate.href;
    } catch {
      sourceUrl = "https://blog.floorp.app/";
    }
  }

  useEffect(() => {
    if (selectedArticleId) setDetailOpen(true);
    setArticleMenuOpen(false);
    articleScrollRef.current?.scrollTo({ top: 0 });
  }, [selectedArticleId]);

  const chooseFilter = (filter: FeedFilter, clearSource = false) => {
    setFeedFilter(filter);
    if (clearSource) setSelectedFeedId(null);
    setSourcesOpen(false);
    setDetailOpen(false);
    listRef.current?.scrollTo({ top: 0 });
    listScrollTop.current = 0;
  };

  const chooseArticle = (id: string) => {
    listScrollTop.current = listRef.current?.scrollTop ?? 0;
    openArticle(id);
    setDetailOpen(true);
    requestAnimationFrame(() =>
      articleHeadingRef.current?.focus({ preventScroll: true }),
    );
  };

  const backToList = () => {
    setDetailOpen(false);
    requestAnimationFrame(() => {
      if (!listRef.current) return;
      listRef.current.scrollTop = listScrollTop.current;
      listRef.current
        .querySelector<HTMLButtonElement>("[aria-current='true']")
        ?.focus({ preventScroll: true });
    });
  };

  const toggleStar = () => {
    if (!currentArticle) return;
    setData((previous) => ({
      ...previous,
      starredIds: previous.starredIds.includes(currentArticle.id)
        ? previous.starredIds.filter((id) => id !== currentArticle.id)
        : [...previous.starredIds, currentArticle.id],
    }));
  };

  const toggleRead = () => {
    if (!currentArticle) return;
    setData((previous) => ({
      ...previous,
      readIds: previous.readIds.includes(currentArticle.id)
        ? previous.readIds.filter((id) => id !== currentArticle.id)
        : [...previous.readIds, currentArticle.id],
    }));
    setArticleMenuOpen(false);
    showToast(currentRead ? "未読にしました" : "既読にしました");
  };

  const removeFeed = (id: string, title: string) => {
    setData((previous) => ({
      ...previous,
      feeds: previous.feeds.filter((feed) => feed.id !== id),
    }));
    if (selectedFeedId === id) setSelectedFeedId(null);
    showToast(`${title} の購読を解除しました`);
  };

  const emptyTitle =
    data.feeds.length === 0
      ? "フィードを追加して、読みはじめましょう"
      : normalizedQuery
        ? "一致する記事はありません"
        : feedFilter === "unread"
          ? "未読の記事はありません"
          : feedFilter === "starred"
            ? "スターを付けた記事はありません"
            : "この購読先の記事はまだありません";

  return (
    <section
      className={`reader${detailOpen ? " reader-detail-open" : ""}${sourcesOpen ? " reader-sources-open" : ""}`}
      aria-label="RSS リーダー"
      onKeyDown={(event) => {
        if (event.key !== "Escape") return;
        if (articleMenuOpen) setArticleMenuOpen(false);
        else if (sourcesOpen) setSourcesOpen(false);
        else if (detailOpen) backToList();
      }}
    >
      <aside className="reader-sources" aria-label="購読先">
        <div className="reader-sources-heading">
          <h2>フィード</h2>
          <IconButton
            label="購読先を閉じる"
            className="reader-sources-close"
            onClick={() => setSourcesOpen(false)}
          >
            <X size={20} />
          </IconButton>
        </div>
        <Button
          variant="secondary"
          className="reader-add-feed"
          onClick={() => setDialog({ kind: "feed" })}
        >
          <Plus size={18} />
          フィードを追加
        </Button>
        <nav className="reader-source-navigation" aria-label="記事の種類">
          <button
            type="button"
            className={
              !selectedFeedId && feedFilter === "all" ? "is-selected" : ""
            }
            onClick={() => chooseFilter("all", true)}
          >
            <LayoutList size={19} />
            <span>すべての記事</span>
          </button>
          <button
            type="button"
            className={
              !selectedFeedId && feedFilter === "unread" ? "is-selected" : ""
            }
            onClick={() => chooseFilter("unread", true)}
          >
            <Circle size={19} />
            <span>未読</span>
            <span className="reader-source-count">{totalUnread}</span>
          </button>
          <button
            type="button"
            className={
              !selectedFeedId && feedFilter === "starred" ? "is-selected" : ""
            }
            onClick={() => chooseFilter("starred", true)}
          >
            <Star size={19} />
            <span>スター</span>
          </button>
        </nav>
        <div className="reader-subscription-heading">購読中</div>
        <nav className="reader-subscriptions" aria-label="購読中のフィード">
          {data.feeds.map((feed) => {
            const unread = availableArticles.filter(
              (article) =>
                article.feedId === feed.id && !readIds.has(article.id),
            ).length;
            return (
              <div
                className={`reader-subscription${selectedFeedId === feed.id ? " is-selected" : ""}`}
                key={feed.id}
              >
                <button
                  type="button"
                  className="reader-subscription-select"
                  onClick={() => {
                    setSelectedFeedId(feed.id);
                    setSourcesOpen(false);
                    setDetailOpen(false);
                  }}
                  aria-current={selectedFeedId === feed.id ? "true" : undefined}
                >
                  <BrandIcon name={feed.icon} size={23} />
                  <span>{feed.title}</span>
                  {unread > 0 && (
                    <span className="reader-source-count">{unread}</span>
                  )}
                </button>
                <IconButton
                  label={`${feed.title} の購読を解除`}
                  className="reader-unsubscribe"
                  onClick={() => removeFeed(feed.id, feed.title)}
                >
                  <X size={14} />
                </IconButton>
              </div>
            );
          })}
        </nav>
      </aside>
      {sourcesOpen && (
        <button
          className="reader-source-backdrop"
          type="button"
          aria-label="購読先を閉じる"
          onClick={() => setSourcesOpen(false)}
        />
      )}

      <section className="reader-list-pane" aria-label="記事一覧">
        <header className="reader-list-header">
          <div className="reader-list-title">
            <IconButton
              label="購読先を表示"
              className="reader-sources-trigger"
              onClick={() => setSourcesOpen(true)}
            >
              <Menu size={20} />
            </IconButton>
            <h1 className="reader-list-mobile-heading">
              {selectedFeed?.title ?? "すべての記事"}
            </h1>
            <h2 className="reader-list-desktop-heading">
              {selectedFeed?.title ?? "すべての記事"}
            </h2>
          </div>
          <label className="reader-search">
            <Search size={18} aria-hidden="true" />
            <Input
              type="search"
              value={feedQuery}
              onChange={(event) => {
                setFeedQuery(event.currentTarget.value);
                setDetailOpen(false);
              }}
              placeholder="記事を検索"
              aria-label="記事を検索"
            />
          </label>
          <div
            className="reader-filters"
            role="group"
            aria-label="記事の絞り込み"
          >
            {filters.map((filter) => (
              <button
                type="button"
                key={filter.value}
                aria-pressed={feedFilter === filter.value}
                onClick={() => chooseFilter(filter.value)}
              >
                {filter.label}
              </button>
            ))}
          </div>
        </header>
        <div
          className="reader-list-scroll"
          ref={listRef}
          onScroll={(event) => {
            listScrollTop.current = event.currentTarget.scrollTop;
          }}
        >
          {visibleArticles.map((article) => {
            const feed = data.feeds.find((item) => item.id === article.feedId);
            const unread = !readIds.has(article.id);
            return (
              <button
                type="button"
                key={article.id}
                className={`reader-list-article${currentArticle?.id === article.id ? " is-selected" : ""}${unread ? " is-unread" : ""}`}
                onClick={() => chooseArticle(article.id)}
                aria-current={
                  currentArticle?.id === article.id ? "true" : undefined
                }
              >
                <span className="reader-list-source">
                  {feed?.title}
                  {unread && (
                    <span className="reader-unread-dot" aria-label="未読" />
                  )}
                </span>
                <span className="reader-list-article-title">
                  {article.title}
                  {starredIds.has(article.id) && (
                    <Star
                      size={13}
                      fill="currentColor"
                      aria-label="スター付き"
                    />
                  )}
                </span>
                <span className="reader-list-excerpt">{article.excerpt}</span>
                <time className="reader-list-date">
                  {readableDate(article.date)}
                </time>
              </button>
            );
          })}
          {visibleArticles.length === 0 && (
            <div className="reader-list-empty">
              <EmptyState
                title={emptyTitle}
                description={
                  data.feeds.length === 0
                    ? "ブログやニュースサイトのフィードを追加できます。"
                    : normalizedQuery
                      ? "検索する言葉を変えてみてください。"
                      : feedFilter === "starred"
                        ? "あとで読みたい記事にスターを付けて保存できます。"
                        : "ほかの記事も読んでみませんか。"
                }
              >
                {data.feeds.length === 0 ? (
                  <Button onClick={() => setDialog({ kind: "feed" })}>
                    <Plus size={18} />
                    フィードを追加
                  </Button>
                ) : (
                  <Button
                    variant="secondary"
                    onClick={() => {
                      setFeedQuery("");
                      chooseFilter("all", true);
                    }}
                  >
                    すべての記事を見る
                  </Button>
                )}
              </EmptyState>
            </div>
          )}
        </div>
      </section>

      <section className="reader-article-pane" aria-label="記事本文">
        <div className="reader-article-toolbar">
          <Button
            className="reader-back-to-list"
            variant="ghost"
            onClick={backToList}
          >
            <ArrowLeft size={18} />
            記事一覧
          </Button>
          {currentFeed && (
            <span className="reader-current-source">
              <BrandIcon name={currentFeed.icon} size={22} />
              {currentFeed.title}
            </span>
          )}
          {currentArticle && (
            <div className="reader-article-actions">
              <IconButton
                label={currentStarred ? "スターを外す" : "スターを付ける"}
                aria-pressed={currentStarred}
                className={currentStarred ? "reader-star-active" : ""}
                onClick={toggleStar}
              >
                <Star
                  size={21}
                  fill={currentStarred ? "currentColor" : "none"}
                />
              </IconButton>
              <div className="reader-article-menu">
                <IconButton
                  label="記事の操作"
                  aria-expanded={articleMenuOpen}
                  onClick={() => setArticleMenuOpen((open) => !open)}
                >
                  <MoreHorizontal size={21} />
                </IconButton>
                {articleMenuOpen && (
                  <div className="reader-menu" role="menu">
                    <button type="button" role="menuitem" onClick={toggleRead}>
                      {currentRead ? <Circle size={16} /> : <Check size={16} />}
                      {currentRead ? "未読にする" : "既読にする"}
                    </button>
                  </div>
                )}
              </div>
              <a
                className="reader-open-source"
                href={sourceUrl}
                target="_blank"
                rel="noreferrer"
              >
                <ExternalLink size={17} />
                <span>元サイトで開く</span>
              </a>
            </div>
          )}
        </div>
        <div className="reader-article-scroll" ref={articleScrollRef}>
          {currentArticle ? (
            <article className="reader-article-content" key={currentArticle.id}>
              <h1 ref={articleHeadingRef} tabIndex={-1}>
                {currentArticle.title}
              </h1>
              <p className="reader-article-meta">
                <time>{readableDate(currentArticle.date)}</time>
                <span>{currentArticle.author}</span>
              </p>
              <div className="reader-article-body">
                {currentArticle.body.map((paragraph, index) => (
                  <React.Fragment key={`${currentArticle.id}-${index}`}>
                    {index > 0 &&
                      currentArticle.sectionHeadings?.[index - 1] && (
                        <h2>{currentArticle.sectionHeadings[index - 1]}</h2>
                      )}
                    <p>{paragraph}</p>
                  </React.Fragment>
                ))}
              </div>
              {currentArticle.isExcerpt && (
                <div className="reader-excerpt-notice">
                  <p>この記事は抜粋で配信されています。</p>
                  <a href={sourceUrl} target="_blank" rel="noreferrer">
                    元サイトで続きを読む
                    <ExternalLink size={16} />
                  </a>
                </div>
              )}
            </article>
          ) : (
            <div className="reader-no-selection">
              <Rss size={36} strokeWidth={1.4} />
              <h1>フィード</h1>
              <p className="reader-no-selection-title">
                読みたい記事を選んでください
              </p>
              <p>配信された記事を、ここで読みやすく表示します。</p>
            </div>
          )}
        </div>
      </section>
    </section>
  );
}
