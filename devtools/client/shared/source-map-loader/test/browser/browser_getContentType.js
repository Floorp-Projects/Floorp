/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Cover the automatic mapping of content type based on file extension

const {
  getContentType,
  contentMapForTesting,
} = require("resource://devtools/client/shared/source-map-loader/utils/index.js");

add_task(async () => {
  for (const ext in contentMapForTesting) {
    is(
      getContentType(`whatever.${ext}`),
      contentMapForTesting[ext],
      `${ext} file extension is correctly mapping the expected content type`
    );
  }
  is(
    getContentType(`whateverjs`),
    "text/plain",
    `A valid extension in file name doesn't cause a special content type mapping`
  );

  is(
    getContentType("whatever.platypus"),
    "text/plain",
    "Test unknown extension defaults to text plain"
  );
});
