/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test require using "raw!".

function run_test() {
  let loader = new DevToolsLoader();
  let require = loader.require;

  let variableFileContents = require("raw!devtools/client/themes/variables.css");
  ok(variableFileContents.length > 0, "raw browserRequire worked");

  let propertiesFileContents = require("raw!devtools/client/locales/shared.properties");
  ok(propertiesFileContents.length > 0, "unprefixed properties raw require worked");

  let chromePropertiesFileContents =
    require("raw!chrome://devtools/locale/shared.properties");
  ok(chromePropertiesFileContents.length > 0, "prefixed properties raw require worked");
}
