/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";
import Header from "../Header/Header";
import ArticleList from "../ArticleList/ArticleList";

function Saved(props) {
  const { similarRecs, savedStory } = props;
  return (
    <div className="stp_panel_container">
      <div className="stp_panel stp_panel_home">
        <Header>
          <a>
            <span data-l10n-id="pocket-panel-header-my-list"></span>
          </a>
        </Header>
        <hr />
        {savedStory && (
          <>
            <p data-l10n-id="pocket-panel-saved-page-saved"></p>
            <ArticleList articles={[savedStory]} />
            <span data-l10n-id="pocket-panel-button-add-tags"></span>
            <span data-l10n-id="pocket-panel-saved-remove-page"></span>
          </>
        )}
        <hr />
        {similarRecs?.length && <ArticleList articles={similarRecs} />}
      </div>
    </div>
  );
}

export default Saved;
