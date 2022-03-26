/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";
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

  const { article } = props;
  const url = article.url || article.resolved_url;
  // Using array notation because there is a key titled `1` (`images` is an object)
  const thumbnail =
    article.thumbnail ||
    encodeThumbnail(article?.top_image_url || article?.images?.["1"]?.src);
  const alt = article.alt || "thumbnail image";
  const title = article.title || article.resolved_title;
  // Sometimes domain_metadata is not there, depending on the source.
  const publisher =
    article.publisher ||
    article.domain_metadata?.name ||
    article.resolved_domain;
  return (
    <li className="stp_article_list_item">
      <ArticleUrl
        url={url}
        savedArticle={props.savedArticle}
        position={props.position}
        source={props.source}
      >
        <>
          {thumbnail ? (
            <img className="stp_article_list_thumb" src={thumbnail} alt={alt} />
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
        />
      ))}
    </ul>
  );
}

export default ArticleList;
