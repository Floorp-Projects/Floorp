/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* global __dirname */

module.exports = {
  verbose: true,
  moduleNameMapper: {
    "^devtools/client/shared/link": `${__dirname}/fixtures/stub`,
    "^devtools\\/(.*)": `${__dirname}/../../../../../$1`,
    "^promise": `${__dirname}/fixtures/promise`,
    "^Services": `${__dirname}/fixtures/Services`,
  },
  setupFiles: ["<rootDir>setup.js"],
  snapshotSerializers: ["enzyme-to-json/serializer"],
};
