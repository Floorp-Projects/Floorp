/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React, { useState } from "react";
import TelemetryLink from "../TelemetryLink/TelemetryLink";

function ArticleUrl(props) {
  // We turn off the link if we're either a saved article, or if the url doesn't exist.
  if (props.savedArticle || !props.url) {
    return (
      <div className="stp_article_list_saved_article">{props.children}</div>
    );
  }
  return (
    <TelemetryLink
      className="stp_article_list_link"
      href={props.url}
      source={props.source}
      position={props.position}
      model={props.model}
    >
      {props.children}
    </TelemetryLink>
  );
}

function Article(props) {
  function encodeThumbnail(rawSource) {
    return rawSource
      ? `https://img-getpocket.cdn.mozilla.net/80x80/filters:format(jpeg):quality(60):no_upscale():strip_exif()/${encodeURIComponent(
          rawSource
        )}`
      : null;
  }

  const [thumbnailLoaded, setThumbnailLoaded] = useState(false);
  const [thumbnailLoadFailed, setThumbnailLoadFailed] = useState(false);

  const {
    article,
    savedArticle,
    position,
    source,
    model,
    utmParams,
    openInPocketReader,
  } = props;

  if (!article.url && !article.resolved_url && !article.given_url) {
    return null;
  }
  const url = new URL(article.url || article.resolved_url || article.given_url);
  const urlSearchParams = new URLSearchParams(utmParams);

  if (
    openInPocketReader &&
    article.item_id &&
    !url.href.match(/getpocket\.com\/read/)
  ) {
    url.href = `https://getpocket.com/read/${article.item_id}`;
  }

  for (let [key, val] of urlSearchParams.entries()) {
    url.searchParams.set(key, val);
  }

  // Using array notation because there is a key titled `1` (`images` is an object)
  const thumbnail =
    article.thumbnail ||
    encodeThumbnail(article?.top_image_url || article?.images?.["1"]?.src);
  const alt = article.alt || "thumbnail image";
  const title = article.title || article.resolved_title || article.given_title;
  // Sometimes domain_metadata is not there, depending on the source.
  const publisher =
    article.publisher ||
    article.domain_metadata?.name ||
    article.resolved_domain;

  return (
    <li className="stp_article_list_item">
      <ArticleUrl
        url={url.href}
        savedArticle={savedArticle}
        position={position}
        source={source}
        model={model}
        utmParams={utmParams}
      >
        <>
          {thumbnail && !thumbnailLoadFailed ? (
            <img
              className="stp_article_list_thumb"
              src={thumbnail}
              alt={alt}
              width="40"
              height="40"
              onLoad={() => {
                setThumbnailLoaded(true);
              }}
              onError={() => {
                setThumbnailLoadFailed(true);
              }}
              style={{
                visibility: thumbnailLoaded ? `visible` : `hidden`,
              }}
            />
          ) : (
            <div className="stp_article_list_thumb_placeholder" />
          )}
          <div className="stp_article_list_meta">
            <header className="stp_article_list_header">{title}</header>
            <p className="stp_article_list_publisher">{publisher}</p>
          </div>
        </>
      </ArticleUrl>
    </li>
  );
}

function ArticleList(props) {
  return (
    <ul className="stp_article_list">
      {props.articles?.map((article, position) => (
        <Article
          article={article}
          savedArticle={props.savedArticle}
          position={position}
          source={props.source}
          model={props.model}
          utmParams={props.utmParams}
          openInPocketReader={props.openInPocketReader}
        />
      ))}
    </ul>
  );
}

export default ArticleList;
