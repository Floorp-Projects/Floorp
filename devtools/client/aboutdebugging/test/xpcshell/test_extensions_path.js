/* global equal */

"use strict";

const {
  parseFileUri,
} = require("resource://devtools/client/aboutdebugging/src/modules/extensions-helper.js");

add_task(async function testParseFileUri() {
  equal(
    parseFileUri("file:///home/me/my-extension/"),
    "/home/me/my-extension/",
    "UNIX paths are supported"
  );

  equal(
    parseFileUri("file:///C:/Documents/my-extension/"),
    "C:/Documents/my-extension/",
    "Windows paths are supported"
  );

  equal(
    parseFileUri("file://home/Documents/my-extension/"),
    "home/Documents/my-extension/",
    "Windows network paths are supported"
  );
});
