<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

The `places/favicon` module provides simple helper functions for working with favicons.

<api name="getFavicon">
@function
Takes an `object` that represents a page's URL and returns a promise
that resolves with the favicon URL for that page. The `object` can
be a URL `String` or a [`Tab`](modules/sdk/tabs.html#Tab). The platform service
([mozIAsyncFavicons](https://developer.mozilla.org/en-US/docs/XPCOM_Interface_Reference/mozIAsyncFavicons)) retrieves favicon data stored from previously visited sites, and as
such, will only return favicon URLs for visited sites.

    let { getFavicon } = require("sdk/places/favicon");

    // String example
    getFavicon("http://mozilla.org").then(function (url) {
      console.log(url); // http://mozorg.cdn.mozilla.net/media/img/favicon.ico
    });

    // Tab example
    require("sdk/tabs").open({
      url: "http://mozilla.org",
      onReady: function (tab) {
        getFavicon(tab).then(function (url) {
          console.log(url); // http://mozorg.cdn.mozilla.net/media/img/favicon.ico
        });
      }
    });

    // An optional callback can be provided to handle
    // the promise's resolve and reject states
    getFavicon("http://mozilla.org", function (url) {
      console.log(url); // http://mozorg.cdn.mozilla.net/media/img/favicon.ico
    });

@param object {string|tab}
  A value that represents the URL of the page to get the favicon URL from.
  Can be a URL `String` or a [`Tab`](modules/sdk/tabs.html#Tab).

@param callback {function}
  An optional callback function that will be used in both resolve and
  reject cases.

@returns {promise}
  A promise that resolves with the favicon URL.
</api>
