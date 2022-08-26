/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test require using "raw!".

function run_test() {
  const loader = new DevToolsLoader();
  const require = loader.require;

  const variableFileContents = require("raw!chrome://devtools/skin/variables.css");
  ok(!!variableFileContents.length, "raw browserRequire worked");

  const propertiesFileContents = require("raw!devtools/client/locales/shared.properties");
  ok(
    !!propertiesFileContents.length,
    "unprefixed properties raw require worked"
  );

  const chromePropertiesFileContents = require("raw!chrome://devtools/locale/shared.properties");
  ok(
    !!chromePropertiesFileContents.length,
    "prefixed properties raw require worked"
  );
}
