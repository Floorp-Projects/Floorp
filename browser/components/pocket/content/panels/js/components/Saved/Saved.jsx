/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React, { useState, useEffect } from "react";
import Header from "../Header/Header";
import Button from "../Button/Button";
import ArticleList from "../ArticleList/ArticleList";
import TagPicker from "../TagPicker/TagPicker";
import panelMessaging from "../../messages";

function Saved(props) {
  const { locale, pockethost, utmSource, utmCampaign, utmContent } = props;
  // savedStatus can be success, loading, or error.
  const [
    { savedStatus, savedErrorId, itemId, itemUrl },
    setSavedStatusState,
  ] = useState({ savedStatus: "loading" });
  // removedStatus can be removed, removing, or error.
  const [
    { removedStatus, removedErrorMessage },
    setRemovedStatusState,
  ] = useState({});
  const [savedStory, setSavedStoryState] = useState();
  const [articleInfoAttempted, setArticleInfoAttempted] = useState();
  const [{ similarRecs, similarRecsModel }, setSimilarRecsState] = useState({});
  const utmParams = `utm_source=${utmSource}${
    utmCampaign && utmContent
      ? `&utm_campaign=${utmCampaign}&utm_content=${utmContent}`
      : ``
  }`;

  function removeItem(event) {
    event.preventDefault();
    setRemovedStatusState({ removedStatus: "removing" });
    panelMessaging.sendMessage(
      "PKT_deleteItem",
      {
        itemId,
      },
      function(resp) {
        const { data } = resp;
        if (data.status == "success") {
          setRemovedStatusState({ removedStatus: "removed" });
        } else if (data.status == "error") {
          let errorMessage = "";
          // The server returns English error messages, so in the case of
          // non English, we do our best with a generic translated error.
          if (data.error.message && locale?.startsWith("en")) {
            errorMessage = data.error.message;
          }
          setRemovedStatusState({
            removedStatus: "error",
            removedErrorMessage: errorMessage,
          });
        }
      }
    );
  }

  useEffect(() => {
    // Wait confirmation of save before flipping to final saved state
    panelMessaging.addMessageListener("PKT_saveLink", function(resp) {
      const { data } = resp;
      if (data.status == "error") {
        // Use localizedKey or fallback to a generic catch all error.
        setSavedStatusState({
          savedStatus: "error",
          savedErrorId:
            data?.error?.localizedKey || "pocket-panel-saved-error-generic",
        });
        return;
      }

      // Success, so no localized error id needed.
      setSavedStatusState({
        savedStatus: "success",
        itemId: data.item?.item_id,
        itemUrl: data.item?.given_url,
        savedErrorId: "",
      });
    });

    panelMessaging.addMessageListener("PKT_articleInfoFetched", function(resp) {
      setSavedStoryState(resp?.data?.item_preview);
    });

    panelMessaging.addMessageListener("PKT_getArticleInfoAttempted", function(
      resp
    ) {
      setArticleInfoAttempted(true);
    });

    panelMessaging.addMessageListener("PKT_renderItemRecs", function(resp) {
      const { data } = resp;

      // This is the ML model used to recommend the story.
      // Right now this value is the same for all three items returned together,
      // so we can just use the first item's value for all.
      const model = data?.recommendations?.[0]?.experiment || "";
      setSimilarRecsState({
        similarRecs: data?.recommendations?.map(rec => rec.item),
        similarRecsModel: model,
      });
    });

    // tell back end we're ready
    panelMessaging.sendMessage("PKT_show_saved");
  }, []);

  if (savedStatus === "error") {
    return (
      <div className="stp_panel_container">
        <div className="stp_panel stp_panel_error">
          <div className="stp_panel_error_icon" />
          <h3
            className="header_large"
            data-l10n-id="pocket-panel-saved-error-not-saved"
          />
          <p data-l10n-id={savedErrorId} />
        </div>
      </div>
    );
  }

  return (
    <div className="stp_panel_container">
      <div className="stp_panel stp_panel_saved">
        <Header>
          <Button
            style="primary"
            url={`https://${pockethost}/a?${utmParams}`}
            source="view_list"
          >
            <span data-l10n-id="pocket-panel-header-my-list"></span>
          </Button>
        </Header>
        <hr />
        {!removedStatus && savedStatus === "success" && (
          <>
            <h3 className="header_large header_flex">
              <span data-l10n-id="pocket-panel-saved-page-saved-b" />
              <Button style="text" onClick={removeItem}>
                <span data-l10n-id="pocket-panel-button-remove"></span>
              </Button>
            </h3>
            {savedStory && (
              <ArticleList
                articles={[savedStory]}
                openInPocketReader={true}
                utmParams={utmParams}
              />
            )}
            {articleInfoAttempted && <TagPicker tags={[]} itemUrl={itemUrl} />}
            {articleInfoAttempted &&
              similarRecs?.length &&
              locale?.startsWith("en") && (
                <>
                  <hr />
                  <h3 className="header_medium">Similar Stories</h3>
                  <ArticleList
                    articles={similarRecs}
                    source="on_save_recs"
                    model={similarRecsModel}
                    utmParams={utmParams}
                  />
                </>
              )}
          </>
        )}
        {savedStatus === "loading" && (
          <h3
            className="header_large"
            data-l10n-id="pocket-panel-saved-saving-tags"
          />
        )}
        {removedStatus === "removing" && (
          <h3
            className="header_large header_center"
            data-l10n-id="pocket-panel-saved-processing-remove"
          />
        )}
        {removedStatus === "removed" && (
          <h3
            className="header_large header_center"
            data-l10n-id="pocket-panel-saved-removed"
          />
        )}
        {removedStatus === "error" && (
          <>
            <h3
              className="header_large"
              data-l10n-id="pocket-panel-saved-error-remove"
            />
            {removedErrorMessage && <p>{removedErrorMessage}</p>}
          </>
        )}
      </div>
    </div>
  );
}

export default Saved;
