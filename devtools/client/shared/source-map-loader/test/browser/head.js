/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/shared-head.js",
  this
);

const {
  SourceMapLoader,
} = require("resource://devtools/client/shared/source-map-loader/index.js");

const gSourceMapLoader = new SourceMapLoader();

function fetchFixtureSourceMap(name) {
  gSourceMapLoader.clearSourceMaps();

  const source = {
    id: `${name}.js`,
    sourceMapURL: `${name}.js.map`,
    sourceMapBaseURL: `${URL_ROOT_SSL}fixtures/${name}.js`,
  };

  return gSourceMapLoader.getOriginalURLs(source);
}
