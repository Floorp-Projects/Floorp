/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";

function ArticleList(props) {
  return (
    <ul className="stp_article_list">
      {props.articles.map(article => (
        <li className="stp_article_list_item">
          <a className="stp_article_list_link" href={article.url}>
            <img
              className="stp_article_list_thumb"
              src={article.thumbnail}
              alt={article.alt}
            />
            <div className="stp_article_list_meta">
              <header className="stp_article_list_header">
                {article.title}
              </header>
              <p className="stp_article_list_publisher">{article.publisher}</p>
            </div>
          </a>
        </li>
      ))}
    </ul>
  );
}

export default ArticleList;
