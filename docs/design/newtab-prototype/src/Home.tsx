import React, { useState } from "react";
import { ArrowRight, Clock3, Plus, Search, Star } from "lucide-react";
import { articles } from "./data.ts";
import { usePrototype } from "./store.tsx";
import { GroupSelect, SiteGrid } from "./SiteGrid.tsx";
import { Button, EmptyState, IconButton, Input } from "./ui.tsx";
import { Clock } from "./Clock.tsx";

export function Home() {
  const { data, setData, navigate, openArticle, setDialog } = usePrototype();
  const [query, setQuery] = useState("");
  const shownArticles = articles.filter((article) =>
    data.feeds.some((feed) => feed.id === article.feedId),
  );
  const unread = shownArticles.filter(
    (article) => !data.readIds.includes(article.id),
  ).length;
  const toggleStar = (id: string) =>
    setData((previous) => ({
      ...previous,
      starredIds: previous.starredIds.includes(id)
        ? previous.starredIds.filter((item) => item !== id)
        : [...previous.starredIds, id],
    }));
  const search = (event: React.FormEvent) => {
    event.preventDefault();
    const text = query.trim();
    if (!text) return;
    const url = /^https?:\/\//i.test(text)
      ? text
      : /^[\w.-]+\.[a-z]{2,}(\/.*)?$/i.test(text)
        ? `https://${text}`
        : `https://duckduckgo.com/?q=${encodeURIComponent(text)}`;
    window.open(url, "_blank", "noopener,noreferrer");
  };
  return (
    <main id="main-content" className="home-page">
      <h1 className="sr-only">新しいタブ</h1>
      {data.settings.showClock && (
        <div className="home-intro page-container">
          <Clock />
        </div>
      )}
      <section
        className={`search-region page-container ${data.settings.showSearch ? "" : "without-search"}`}
        aria-label="ウェブ検索"
      >
        {data.settings.showSearch && (
          <form className="web-search" onSubmit={search}>
            <Search size={22} aria-hidden="true" />
            <Input
              className="web-search-input"
              value={query}
              onChange={(event) => setQuery(event.target.value)}
              aria-label="検索または URL を入力"
              placeholder="検索または URL を入力"
            />
            <IconButton label="検索する" type="submit">
              <ArrowRight size={22} />
            </IconButton>
          </form>
        )}
      </section>
      <div
        className="home-content page-container"
        data-layout={data.settings.layout}
        data-only={
          data.settings.showDials && !data.settings.showFeed
            ? "dials"
            : !data.settings.showDials && data.settings.showFeed
              ? "feed"
              : undefined
        }
      >
        {data.settings.showDials && (
          <section className="home-dials" aria-labelledby="home-dial-heading">
            <div className="section-heading home-dial-header">
              <h2 id="home-dial-heading">スピードダイアル</h2>
              <GroupSelect />
              <div className="section-actions">
                <button
                  className="text-action"
                  onClick={() => navigate("dial")}
                >
                  すべて見る
                  <ArrowRight size={15} />
                </button>
                <button
                  className="text-action"
                  onClick={() =>
                    setDialog({ kind: "site", initialFolderId: null })
                  }
                >
                  <Plus size={19} />
                  サイトを追加
                </button>
              </div>
            </div>
            <SiteGrid compact />
            <button
              className="resume-session"
              onClick={() => setDialog({ kind: "session" })}
            >
              <Clock3 size={18} />
              <span>前回のつづき</span>
              <span className="resume-title">ブラウザー調査・3 タブ</span>
              <ArrowRight size={16} />
            </button>
          </section>
        )}
        {data.settings.showFeed && (
          <section className="home-feed" aria-labelledby="home-feed-heading">
            <div className="section-heading">
              <h2 id="home-feed-heading">フィード</h2>
              <span className="unread-count">未読 {unread}</span>
            </div>
            {shownArticles.length ? (
              <div className="home-article-list">
                {shownArticles.slice(0, 5).map((article) => {
                  const feed = data.feeds.find(
                    (item) => item.id === article.feedId,
                  );
                  const isUnread = !data.readIds.includes(article.id);
                  return (
                    <article
                      key={article.id}
                      className="home-article"
                      data-unread={isUnread}
                    >
                      <span
                        className={`unread-dot ${isUnread ? "" : "is-read"}`}
                        aria-label={isUnread ? "未読" : "既読"}
                      />
                      <button
                        className="article-open"
                        onClick={() => openArticle(article.id)}
                      >
                        <span className="article-meta">
                          <span>{feed?.title}</span>
                          <time>{article.relativeTime}</time>
                        </span>
                        <strong>{article.title}</strong>
                      </button>
                      <IconButton
                        className="star-button"
                        label={`${article.title}のスター${data.starredIds.includes(article.id) ? "を外す" : "を付ける"}`}
                        aria-pressed={data.starredIds.includes(article.id)}
                        onClick={() => toggleStar(article.id)}
                      >
                        <Star
                          size={19}
                          fill={
                            data.starredIds.includes(article.id)
                              ? "currentColor"
                              : "none"
                          }
                        />
                      </IconButton>
                    </article>
                  );
                })}
              </div>
            ) : (
              <EmptyState
                title="読みたい情報を、ひとつに"
                description="フィードを追加すると、新しい記事がここに届きます。"
              >
                <Button
                  variant="secondary"
                  onClick={() => setDialog({ kind: "feed" })}
                >
                  <Plus size={17} />
                  フィードを追加
                </Button>
              </EmptyState>
            )}
            <button
              className="text-action reader-entry"
              onClick={() => navigate("feed")}
            >
              リーダーで開く
              <ArrowRight size={17} />
            </button>
          </section>
        )}
        {!data.settings.showDials && !data.settings.showFeed && (
          <EmptyState
            title="自分のホームをつくりましょう"
            description="Floorp Hub で、表示する項目を選べます。"
          >
            <Button variant="secondary" onClick={() => navigate("hub")}>
              表示する項目を選ぶ
            </Button>
          </EmptyState>
        )}
      </div>
    </main>
  );
}
