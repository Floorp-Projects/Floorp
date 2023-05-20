/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This module exports a provider that provides a heuristic result. The result
 * will be provided if the query requests the URL and the URL is in Places with
 * the page title.
 */

import {
  UrlbarProvider,
  UrlbarUtils,
} from "resource:///modules/UrlbarUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
  UrlbarResult: "resource:///modules/UrlbarResult.sys.mjs",
});

/**
 * Class used to create the provider.
 */
class ProviderHistoryUrlHeuristic extends UrlbarProvider {
  /**
   * Returns the name of this provider.
   *
   * @returns {string} the name of this provider.
   */
  get name() {
    return "HistoryUrlHeuristic";
  }

  /**
   * Returns the type of this provider.
   *
   * @returns {integer} one of the types from UrlbarUtils.PROVIDER_TYPE.*
   */
  get type() {
    return UrlbarUtils.PROVIDER_TYPE.HEURISTIC;
  }

  /**
   * Whether this provider should be invoked for the given context.
   * If this method returns false, the providers manager won't start a query
   * with this provider, to save on resources.
   *
   * @param {UrlbarQueryContext} queryContext The query context object
   * @returns {boolean} Whether this provider should be invoked for the search.
   */
  isActive(queryContext) {
    // For better performance, this provider tries to return a result only when
    // the input value can become a URL of the http(s) protocol and its length
    // is less than `MAX_TEXT_LENGTH`. That way its SQL query avoids calling
    // `hash()` on atypical or very long URLs.
    return (
      queryContext.fixupInfo?.href &&
      !queryContext.fixupInfo.isSearch &&
      queryContext.fixupInfo.scheme.startsWith("http") &&
      queryContext.fixupInfo.href.length <= UrlbarUtils.MAX_TEXT_LENGTH
    );
  }

  /**
   * Starts querying.
   *
   * @param {object} queryContext The query context object
   * @param {Function} addCallback Callback invoked by the provider to add a new
   *        result.
   * @returns {Promise} resolved when the query stops.
   */
  async startQuery(queryContext, addCallback) {
    const instance = this.queryInstance;
    const result = await this.#getResult(queryContext);
    if (result && instance === this.queryInstance) {
      addCallback(this, result);
    }
  }

  async #getResult(queryContext) {
    const inputedURL = queryContext.fixupInfo.href;
    const [strippedURL] = UrlbarUtils.stripPrefixAndTrim(inputedURL, {
      stripHttp: true,
      stripHttps: true,
      stripWww: true,
      trimEmptyQuery: true,
    });
    const connection = await lazy.PlacesUtils.promiseLargeCacheDBConnection();
    const resultSet = await connection.executeCached(
      `
      SELECT url, title, frecency
      FROM moz_places
      WHERE
        url_hash IN (
          hash('https://' || :strippedURL),
          hash('https://www.' || :strippedURL),
          hash('http://' || :strippedURL),
          hash('http://www.' || :strippedURL)
        )
        AND frecency <> 0
      ORDER BY
        title IS NOT NULL DESC,
        title || '/' <> :strippedURL DESC,
        url = :inputedURL DESC,
        frecency DESC,
        id DESC
      LIMIT 1
      `,
      { inputedURL, strippedURL }
    );

    if (!resultSet.length) {
      return null;
    }

    const title = resultSet[0].getResultByName("title");
    if (!title) {
      return null;
    }

    return Object.assign(
      new lazy.UrlbarResult(
        UrlbarUtils.RESULT_TYPE.URL,
        UrlbarUtils.RESULT_SOURCE.HISTORY,
        ...lazy.UrlbarResult.payloadAndSimpleHighlights(queryContext.tokens, {
          url: [inputedURL, UrlbarUtils.HIGHLIGHT.TYPED],
          title: [title, UrlbarUtils.HIGHLIGHT.NONE],
          icon: UrlbarUtils.getIconForUrl(resultSet[0].getResultByName("url")),
        })
      ),
      { heuristic: true }
    );
  }
}

export var UrlbarProviderHistoryUrlHeuristic =
  new ProviderHistoryUrlHeuristic();
