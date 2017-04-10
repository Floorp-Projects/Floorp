/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

this.INITIAL_STATE = {
  TopSites: {
    rows: [
      {
        "title": "Facebook",
        "url": "https://www.facebook.com/"
      },
      {
        "title": "YouTube",
        "url": "https://www.youtube.com/"
      },
      {
        "title": "Amazon",
        "url": "http://www.amazon.com/"
      },
      {
        "title": "Yahoo",
        "url": "https://www.yahoo.com/"
      },
      {
        "title": "eBay",
        "url": "http://www.ebay.com"
      },
      {
        "title": "Twitter",
        "url": "https://twitter.com/"
      }
    ]
  }
};

// TODO: Handle some real actions here, once we have a TopSites feed working
function TopSites(prevState = INITIAL_STATE.TopSites, action) {
  return prevState;
}

this.reducers = {TopSites};

this.EXPORTED_SYMBOLS = ["reducers", "INITIAL_STATE"];
