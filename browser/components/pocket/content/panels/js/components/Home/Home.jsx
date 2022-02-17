/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";
import Header from "../Header/Header";
import ArticleList from "../ArticleList/ArticleList";
import PopularTopics from "../PopularTopics/PopularTopics";

function Home(props) {
  const { articles, locale, topics, pockethost } = props;
  return (
    <div className="stp_panel_container">
      <div className="stp_panel stp_panel_home">
        <Header>
          <a>
            <span data-l10n-id="pocket-panel-header-my-list"></span>
          </a>
        </Header>
        <hr />
        {articles?.length ? (
          <>
            <p data-l10n-id="pocket-panel-home-most-recent-saves"></p>
            <ArticleList articles={articles} />
            <span data-l10n-id="pocket-panel-button-show-all"></span>
          </>
        ) : (
          <>
            <p data-l10n-id="pocket-panel-home-new-user-cta"></p>
            <p data-l10n-id="pocket-panel-home-new-user-message"></p>
          </>
        )}
        <hr />
        {pockethost && locale?.startsWith("en") && topics?.length && (
          <>
            <div>Explore popular topics:</div>
            <PopularTopics topics={topics} pockethost={pockethost} />
          </>
        )}
      </div>
    </div>
  );
}

export default Home;
