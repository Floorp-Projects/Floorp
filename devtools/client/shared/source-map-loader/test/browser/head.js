/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from ../../../../shared/test/shared-head.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/shared-head.js",
  this
);

const {
  startSourceMapWorker,
  getOriginalURLs,
  getOriginalLocation,
  getGeneratedLocation,
  getGeneratedRangesForOriginal,
  clearSourceMaps,
} = require("resource://devtools/client/shared/source-map-loader/index.js");

startSourceMapWorker();

function fetchFixtureSourceMap(name) {
  clearSourceMaps();

  const source = {
    id: `${name}.js`,
    sourceMapURL: `${name}.js.map`,
    sourceMapBaseURL: `${URL_ROOT_SSL}fixtures/${name}.js`,
  };

  return getOriginalURLs(source);
}
