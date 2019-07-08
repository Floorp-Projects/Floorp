/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Tests for DevToolsUtils.saveFileStream file:

const { FileUtils } = ChromeUtils.import(
  "resource://gre/modules/FileUtils.jsm",
  {}
);

/**
 * Tests that a file is properly saved using the saveFileStream function
 */
add_task(async function test_save_file() {
  const testText = "This is a text";
  const fileName = "test_console_save-file-" + Math.random();
  const file = FileUtils.getFile("TmpD", [fileName]);
  info("Test creating temporary file: " + file.path);
  await DevToolsUtils.saveFileStream(file, convertToInputStream(testText));
  Assert.ok(file.exists(), "Checking if test file exists");
  const { content } = await DevToolsUtils.fetch(file.path);
  deepEqual(content, testText, "The content was correct.");
  cleanup(fileName);
});

/**
 * Converts a string to an input stream.
 * @param String content
 * @return nsIInputStream
 */
function convertToInputStream(content) {
  const converter = Cc[
    "@mozilla.org/intl/scriptableunicodeconverter"
  ].createInstance(Ci.nsIScriptableUnicodeConverter);
  converter.charset = "UTF-8";
  return converter.convertToInputStream(content);
}

/**
 * Removes the temporary file after the test completes.
 */
function cleanup(fileName) {
  const file = FileUtils.getFile("TmpD", [fileName]);
  registerCleanupFunction(() => {
    file.remove(false);
  });
}
