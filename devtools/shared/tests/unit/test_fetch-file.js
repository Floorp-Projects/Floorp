/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Tests for DevToolsUtils.fetch on file:// URI's.

const { FileUtils } = ChromeUtils.import("resource://gre/modules/FileUtils.jsm", {});
const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm", {});

const TEST_CONTENT = "aéd";

// The TEST_CONTENT encoded as UTF-8.
const UTF8_TEST_BUFFER = new Uint8Array([0x61, 0xc3, 0xa9, 0x64]);

// The TEST_CONTENT encoded as ISO 8859-1.
const ISO_8859_1_BUFFER = new Uint8Array([0x61, 0xe9, 0x64]);

/**
 * Tests that URLs with arrows pointing to an actual source are handled properly
 * (bug 808960). For example 'resource://gre/modules/XPIProvider.jsm ->
 * file://l10n.js' should load 'file://l10n.js'.
 */
add_task(async function test_arrow_urls() {
  const { path } = createTemporaryFile(".js");
  const url = "resource://gre/modules/XPIProvider.jsm -> file://" + path;

  await OS.File.writeAtomic(path, TEST_CONTENT, { encoding: "utf-8" });
  const { content } = await DevToolsUtils.fetch(url);

  deepEqual(content, TEST_CONTENT, "The file contents were correctly read.");
});

/**
 * Tests that empty files are read correctly.
 */
add_task(async function test_empty() {
  const { path } = createTemporaryFile();
  const { content } = await DevToolsUtils.fetch("file://" + path);
  deepEqual(content, "", "The empty file was read correctly.");
});

/**
 * Tests that UTF-8 encoded files are correctly read.
 */
add_task(async function test_encoding_utf8() {
  const { path } = createTemporaryFile();
  await OS.File.writeAtomic(path, UTF8_TEST_BUFFER);

  const { content } = await DevToolsUtils.fetch(path);
  deepEqual(content, TEST_CONTENT,
    "The UTF-8 encoded file was correctly read.");
});

/**
 * Tests that ISO 8859-1 (Latin-1) encoded files are correctly read.
 */
add_task(async function test_encoding_iso_8859_1() {
  const { path } = createTemporaryFile();
  await OS.File.writeAtomic(path, ISO_8859_1_BUFFER);

  const { content } = await DevToolsUtils.fetch(path);
  deepEqual(content, TEST_CONTENT,
    "The ISO 8859-1 encoded file was correctly read.");
});

/**
 * Test that non-existent files are handled correctly.
 */
add_task(async function test_missing() {
  await DevToolsUtils.fetch("file:///file/not/found.right").then(result => {
    info(result);
    ok(false, "Fetch resolved unexpectedly when the file was not found.");
  }, () => {
    ok(true, "Fetch rejected as expected because the file was not found.");
  });
});

/**
 * Test that URLs without file:// scheme work.
 */
add_task(async function test_schemeless_files() {
  const { path } = createTemporaryFile();

  await OS.File.writeAtomic(path, TEST_CONTENT, { encoding: "utf-8" });

  const { content } = await DevToolsUtils.fetch(path);
  deepEqual(content, TEST_CONTENT, "The content was correct.");
});

/**
 * Creates a temporary file that is removed after the test completes.
 */
function createTemporaryFile(extension) {
  const name = "test_fetch-file-" + Math.random() + (extension || "");
  const file = FileUtils.getFile("TmpD", [name]);
  file.create(Ci.nsIFile.NORMAL_FILE_TYPE, parseInt("0755", 8));

  registerCleanupFunction(() => {
    file.remove(false);
  });

  return file;
}
