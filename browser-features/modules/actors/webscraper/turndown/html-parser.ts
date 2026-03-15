/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * HTML Parser for Turndown
 * Uses native DOMParser in Firefox actor environment
 * No external dependencies like domino required
 */

class HTMLParser {
  private domParser: DOMParser;

  constructor() {
    // In Firefox actor environment, DOMParser is available globally
    this.domParser = new DOMParser();
  }

  parseFromString(html: string): Document {
    // DOM parsers arrange elements in the <head> and <body>.
    // Wrapping in a custom element ensures elements are reliably arranged in
    // a single element.
    const wrappedHtml =
      '<x-turndown id="turndown-root">' + html + "</x-turndown>";
    return this.domParser.parseFromString(wrappedHtml, "text/html");
  }
}

export default HTMLParser;
