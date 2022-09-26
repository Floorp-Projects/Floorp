/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Collects Open Graph (https://opengraphprotocol.org/) related data from a page.
 */
export const OpenGraphPageData = {
  /**
   * Collects the opengraph data from the page.
   *
   * @param {Document} document
   *   The document to collect from
   *
   * @returns {PageData}
   */
  collect(document) {
    let pageData = {};

    // Sites can technically define an Open Graph prefix other than `og:`.
    // However, `og:` is one of the default RDFa prefixes and it's likely
    // uncommon that sites use a custom prefix. If we find that metadata is
    // missing for common sites due to this issue, we could consider adding a
    // basic RDFa parser.
    let openGraphTags = document.querySelectorAll("meta[property^='og:'");

    for (let tag of openGraphTags) {
      // Strip "og:" from the property name.
      let propertyName = tag.getAttribute("property").substring(3);

      switch (propertyName) {
        case "description":
          pageData.description = tag.getAttribute("content");
          break;
        case "site_name":
          pageData.siteName = tag.getAttribute("content");
          break;
        case "image":
          pageData.image = tag.getAttribute("content");
          break;
      }
    }

    return pageData;
  },
};
