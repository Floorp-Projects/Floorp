// SPDX-License-Identifier: MPL-2.0
import { useEffect, useState } from "react";
import { ArrowRight, Plus, Search, Star, X } from "lucide-react";
import { useTranslation } from "react-i18next";
import { newTabClient } from "../../../../../libs/newtab/client.ts";
import type {
  ArticleSummary,
  PinnedSite,
} from "../../../../../libs/newtab/types.ts";
import { DEFAULT_SUGGESTED_SITES } from "../../utils/dataManager.ts";
import { BookmarkGrid, GroupSelect } from "./Bookmarks.tsx";
import { messageOf, useNewTab } from "./state.tsx";
import { Button, IconButton, SiteIcon, safeWebUrl } from "./ui.tsx";

function Clock() {
  const [time, setTime] = useState(() => new Date());
  useEffect(() => {
    const timer = setInterval(() => setTime(new Date()), 1000);
    return () => clearInterval(timer);
  }, []);
  return (
    <time
      className="nt-clock"
      dateTime={time.toISOString()}
      aria-label={time.toLocaleString()}
    >
      <strong>
        {time.toLocaleTimeString(undefined, {
          hour: "2-digit",
          minute: "2-digit",
          hour12: false,
        })}
      </strong>
      <span>
        {time.toLocaleDateString(undefined, {
          month: "long",
          day: "numeric",
          weekday: "short",
        })}
      </span>
    </time>
  );
}
function HistorySites() {
  const { t } = useTranslation();
  const { snapshot, groupGuid, epoch, setDialog, refresh, notify } =
    useNewTab();
  const [sites, setSites] = useState<PinnedSite[]>([]);
  useEffect(() => {
    let active = true;
    void newTabClient
      .call("history", {})
      .then((history) => {
        const allowed = history.filter(
          (site) => !snapshot.settings.topSites.blocked.includes(site.url),
        );
        const items =
          allowed.length <= 3
            ? DEFAULT_SUGGESTED_SITES.filter(
                (site) =>
                  !snapshot.settings.topSites.blocked.includes(site.url),
              )
            : allowed;
        if (active) setSites(items.slice(0, 8));
      })
      .catch(() => {
        if (active) setSites([]);
      });
    return () => {
      active = false;
    };
  }, [epoch, snapshot.settings.topSites.blocked]);
  const save = async (site: PinnedSite) => {
    try {
      await newTabClient.call("bookmark", {
        action: "create",
        type: "bookmark",
        parentGuid: groupGuid,
        title: site.title,
        url: site.url,
      });
      await refresh();
      notify(t("newtab.savedSite", "Saved to Speed Dial"));
    } catch (cause) {
      notify(messageOf(cause));
    }
  };
  if (!sites.length) return null;
  return (
    <section
      className="nt-history nt-surface nt-container"
      aria-labelledby="nt-history-heading"
    >
      <div className="nt-section-heading">
        <h2 id="nt-history-heading">
          {t("newtab.frequentSites", "Frequently visited")}
        </h2>
      </div>
      <div className="nt-history-grid">
        {sites.map((site) => (
          <article className="nt-history-site" key={site.url}>
            <a href={safeWebUrl(site.url) ?? undefined}>
              <SiteIcon url={site.url} />
              <strong>{site.title}</strong>
            </a>
            <div>
              <IconButton
                disabled={snapshot.readOnly || !groupGuid}
                label={t("newtab.saveSite", {
                  title: site.title,
                  defaultValue: "Save {{title}} to Speed Dial",
                })}
                onClick={() => void save(site)}
              >
                <Plus size={17} />
              </IconButton>
              <IconButton
                disabled={snapshot.readOnly}
                label={t("newtab.hideSite", {
                  title: site.title,
                  defaultValue: "Hide {{title}}",
                })}
                onClick={() =>
                  setDialog({ type: "block", url: site.url, title: site.title })
                }
              >
                <X size={17} />
              </IconButton>
            </div>
          </article>
        ))}
      </div>
    </section>
  );
}
export function Home() {
  const { t } = useTranslation();
  const { snapshot, groupGuid, epoch, navigate, setDialog, refresh, notify } =
    useNewTab();
  const settings = snapshot.settings;
  const [articles, setArticles] = useState<ArticleSummary[]>([]);
  useEffect(() => {
    if (!settings.home.showFeed) return;
    let active = true;
    void newTabClient
      .call("articles", { limit: 5, filter: "unread" })
      .then((result) => {
        if (active) setArticles(result.items);
      })
      .catch(() => {
        if (active) setArticles([]);
      });
    return () => {
      active = false;
    };
  }, [settings.home.showFeed, epoch]);
  const star = async (article: ArticleSummary) => {
    try {
      await newTabClient.call("articleState", {
        ids: [article.id],
        starred: !article.starred,
      });
      await refresh();
    } catch (cause) {
      notify(messageOf(cause));
    }
  };
  const focusSearch = async () => {
    try {
      await newTabClient.call("focusUrlBar", {});
    } catch (cause) {
      notify(messageOf(cause));
    }
  };
  return (
    <main
      id="main-content"
      className={`nt-home${settings.components.firefoxLayout ? "" : " is-sites-first"}`}
      tabIndex={-1}
    >
      <h1 className="nt-sr-only">{t("newtab.title", "New tab")}</h1>
      {settings.components.clock && (
        <div className="nt-clock-row nt-container">
          <Clock />
        </div>
      )}
      {settings.components.searchBar && (
        <section
          className="nt-search-region nt-container"
          aria-label={t("newtab.webSearch", "Web search")}
        >
          <button
            type="button"
            className="nt-web-search"
            onClick={() => void focusSearch()}
          >
            <Search size={21} />
            <span>
              {t("searchBar.searchOrEnterUrl", "Search or enter a URL")}
            </span>
            <ArrowRight size={21} />
          </button>
        </section>
      )}
      <div
        className="nt-home-content nt-container"
        data-layout={settings.home.layout}
        data-only={
          !settings.speedDial.show || !settings.home.showFeed
            ? "true"
            : undefined
        }
      >
        {settings.speedDial.show && (
          <section
            className="nt-home-dials nt-surface"
            aria-labelledby="nt-home-dial-heading"
          >
            <div className="nt-section-heading">
              <h2 id="nt-home-dial-heading">
                {t("newtab.dial", "Speed Dial")}
              </h2>
              {snapshot.groups.length > 0 && <GroupSelect />}
              <div className="nt-section-actions">
                <button
                  type="button"
                  className="nt-text-action"
                  onClick={() => navigate("dial")}
                >
                  {t("newtab.viewAll", "View all")}
                  <ArrowRight size={16} />
                </button>
                <button
                  type="button"
                  className="nt-text-action"
                  disabled={snapshot.readOnly || !groupGuid}
                  onClick={() =>
                    setDialog({
                      type: "bookmark",
                      kind: "bookmark",
                      parentGuid: groupGuid,
                    })
                  }
                >
                  <Plus size={18} />
                  {t("newtab.addSite", "Add site")}
                </button>
              </div>
            </div>
            <BookmarkGrid compact />
          </section>
        )}
        {settings.home.showFeed && (
          <section
            className="nt-home-feeds nt-surface"
            aria-labelledby="nt-home-feed-heading"
          >
            <div className="nt-section-heading">
              <h2 id="nt-home-feed-heading">{t("newtab.feeds", "Feeds")}</h2>
              <span className="nt-count">
                {t("newtab.unreadCount", {
                  count: snapshot.feeds.reduce(
                    (sum, feed) => sum + feed.unreadCount,
                    0,
                  ),
                  defaultValue: "{{count}} unread",
                })}
              </span>
            </div>
            {articles.length ? (
              <div className="nt-home-article-list">
                {articles.map((article) => (
                  <article className="nt-home-article" key={article.id}>
                    <span
                      className="nt-unread-dot"
                      aria-label={t("newtab.unread", "Unread")}
                    />
                    <button
                      type="button"
                      onClick={() => {
                        location.hash = `feed/${encodeURIComponent(article.id)}`;
                      }}
                    >
                      <small>
                        {snapshot.feeds.find(
                          (feed) => feed.id === article.feedId,
                        )?.title ?? article.feedTitle}
                      </small>
                      <strong>{article.title}</strong>
                      <time>
                        {new Date(
                          article.publishedAt ?? article.receivedAt,
                        ).toLocaleDateString()}
                      </time>
                    </button>
                    <IconButton
                      disabled={snapshot.readOnly}
                      aria-pressed={article.starred}
                      label={t("newtab.toggleStar", {
                        title: article.title,
                        defaultValue: "Toggle star for {{title}}",
                      })}
                      onClick={() => void star(article)}
                    >
                      <Star
                        size={18}
                        fill={article.starred ? "currentColor" : "none"}
                      />
                    </IconButton>
                  </article>
                ))}
              </div>
            ) : (
              <div className="nt-empty">
                <h3>
                  {snapshot.feeds.some((feed) => feed.enabled)
                    ? t("newtab.caughtUp", "You are all caught up")
                    : t("newtab.addFeedHeading", "Bring your reading together")}
                </h3>
                <p>
                  {t(
                    "newtab.addFeedHint",
                    "Add a feed to see its latest articles here.",
                  )}
                </p>
                <Button
                  variant="secondary"
                  disabled={snapshot.readOnly}
                  onClick={() => setDialog({ type: "subscribe" })}
                >
                  <Plus size={17} />
                  {t("newtab.addFeed", "Add feed")}
                </Button>
              </div>
            )}
            <button
              type="button"
              className="nt-text-action nt-reader-entry"
              onClick={() => navigate("feed")}
            >
              {t("newtab.openReader", "Open reader")}
              <ArrowRight size={17} />
            </button>
          </section>
        )}
        {!settings.speedDial.show && !settings.home.showFeed && (
          <div className="nt-empty nt-surface">
            <h2>{t("newtab.makeHome", "Make this page your own")}</h2>
            <Button
              variant="secondary"
              onClick={() => void newTabClient.call("openHub", {})}
            >
              {t("newtab.openSettings", "Choose what to show")}
            </Button>
          </div>
        )}
      </div>
      {settings.home.showHistory && <HistorySites />}
    </main>
  );
}
