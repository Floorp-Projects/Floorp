/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React, { useState, useEffect, useCallback } from "react";
import Header from "../Header/Header";
import ArticleList from "../ArticleList/ArticleList";
import PopularTopics from "../PopularTopics/PopularTopics";
import Button from "../Button/Button";
import panelMessaging from "../../messages";

function Home(props) {
  const {
    locale,
    topics,
    pockethost,
    hideRecentSaves,
    utmSource,
    utmCampaign,
    utmContent,
  } = props;

  const [{ articles, status }, setArticlesState] = useState({
    articles: [],
    // Can be success, loading, or error.
    status: "",
  });

  const utmParams = `utm_source=${utmSource}${
    utmCampaign && utmContent
      ? `&utm_campaign=${utmCampaign}&utm_content=${utmContent}`
      : ``
  }`;

  const loadingRecentSaves = useCallback(() => {
    setArticlesState(prevState => ({
      ...prevState,
      status: "loading",
    }));
  }, []);

  const renderRecentSaves = useCallback(resp => {
    const { data } = resp;

    if (data.status === "error") {
      setArticlesState(prevState => ({
        ...prevState,
        status: "error",
      }));
      return;
    }

    setArticlesState({
      articles: data,
      status: "success",
    });
  }, []);

  useEffect(() => {
    if (!hideRecentSaves) {
      // We don't display the loading message until instructed. This is because cache
      // loads should be fast, so using the loading message for cache just adds loading jank.
      panelMessaging.addMessageListener(
        "PKT_loadingRecentSaves",
        loadingRecentSaves
      );

      panelMessaging.addMessageListener(
        "PKT_renderRecentSaves",
        renderRecentSaves
      );
    }
  }, [hideRecentSaves, loadingRecentSaves, renderRecentSaves]);

  useEffect(() => {
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
              <ArticleList
                articles={articles.slice(0, 3)}
                source="home_recent_save"
                utmParams={utmParams}
                openInPocketReader={true}
              />
              <span className="stp_button_wide">
                <Button
                  style="secondary"
                  url={`https://${pockethost}/a?${utmParams}`}
                  source="home_view_list"
                >
                  <span data-l10n-id="pocket-panel-button-show-all" />
                </Button>
              </span>
            </>
          ) : (
            <ArticleList
              articles={articles}
              source="home_recent_save"
              utmParams={utmParams}
            />
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
          <Button
            style="primary"
            url={`https://${pockethost}/a?${utmParams}`}
            source="home_view_list"
          >
            <span data-l10n-id="pocket-panel-header-my-saves" />
          </Button>
        </Header>
        <hr />
        {recentSavesSection}
        <hr />
        {pockethost && locale?.startsWith("en") && topics?.length && (
          <>
            <h3 className="header_medium">Explore popular topics:</h3>
            <PopularTopics
              topics={topics}
              pockethost={pockethost}
              utmParams={utmParams}
              source="home_popular_topic"
            />
          </>
        )}
      </div>
    </div>
  );
}

export default Home;
