/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React, { useState, useEffect } from "react";
import Header from "../Header/Header";
import ArticleList from "../ArticleList/ArticleList";
import PopularTopics from "../PopularTopics/PopularTopics";
import Button from "../Button/Button";
import panelMessaging from "../../messages";

function encodeThumbnail(rawSource) {
  return rawSource
    ? `https://img-getpocket.cdn.mozilla.net/80x80/filters:format(jpeg):quality(60):no_upscale():strip_exif()/${encodeURIComponent(
        rawSource
      )}`
    : null;
}

function Home(props) {
  const { locale, topics, pockethost, hideRecentSaves } = props;
  const [{ articles, status }, setArticlesState] = useState({
    articles: [],
    // Can be success, loading, or error.
    status: "",
  });

  useEffect(() => {
    if (!hideRecentSaves) {
      // We don't display the loading message until instructed. This is because cache
      // loads should be fast, so using the loading message for cache just adds loading jank.
      panelMessaging.addMessageListener("PKT_loadingRecentSaves", function(
        resp
      ) {
        setArticlesState({
          articles,
          status: "loading",
        });
      });

      panelMessaging.addMessageListener("PKT_renderRecentSaves", function(
        resp
      ) {
        const { data } = resp;

        if (data.status === "error") {
          setArticlesState({
            articles: [],
            status: "error",
          });
          return;
        }

        setArticlesState({
          articles: data.map(item => ({
            url: item.resolved_url,
            // Using array notation because there is a key titled `1` (`images` is an object)
            thumbnail: encodeThumbnail(
              item?.top_image_url || item?.images?.["1"]?.src
            ),
            alt: "thumbnail image",
            title: item.resolved_title,
            publisher: item.domain_metadata?.name,
          })),
          status: "success",
        });
      });
    }

    // tell back end we're ready
    panelMessaging.sendMessage("PKT_show_home");
  }, []);

  let recentSavesSection = null;

  if (status === "error" || hideRecentSaves) {
    recentSavesSection = (
      <h3
        className="header_medium"
        data-l10n-id="pocket-panel-home-new-user-cta"
      />
    );
  } else if (status === "loading") {
    recentSavesSection = (
      <span data-l10n-id="pocket-panel-home-most-recent-saves-loading" />
    );
  } else if (status === "success") {
    if (articles?.length) {
      recentSavesSection = (
        <>
          <h3
            className="header_medium"
            data-l10n-id="pocket-panel-home-most-recent-saves"
          />
          {articles.length > 3 ? (
            <>
              <ArticleList articles={articles.slice(0, 3)} />
              <span className="stp_button_wide">
                <Button style="secondary">
                  <span data-l10n-id="pocket-panel-button-show-all" />
                </Button>
              </span>
            </>
          ) : (
            <ArticleList articles={articles} />
          )}
        </>
      );
    } else {
      recentSavesSection = (
        <>
          <h3
            className="header_medium"
            data-l10n-id="pocket-panel-home-new-user-cta"
          />
          <h3
            className="header_medium"
            data-l10n-id="pocket-panel-home-new-user-message"
          />
        </>
      );
    }
  }

  return (
    <div className="stp_panel_container">
      <div className="stp_panel stp_panel_home">
        <Header>
          <Button style="primary">
            <span data-l10n-id="pocket-panel-header-my-list" />
          </Button>
        </Header>
        <hr />
        {recentSavesSection}
        <hr />
        {pockethost && locale?.startsWith("en") && topics?.length && (
          <>
            <h3 className="header_medium">Explore popular topics:</h3>
            <PopularTopics topics={topics} pockethost={pockethost} />
          </>
        )}
      </div>
    </div>
  );
}

export default Home;
