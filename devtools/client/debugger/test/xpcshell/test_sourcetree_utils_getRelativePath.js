/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { require } = ChromeUtils.import("resource://devtools/shared/Loader.jsm");
const {
  getRelativePath,
} = require("devtools/client/debugger/src/utils/sources-tree/utils");

function run_test() {
  info("Test a url without any path");
  equal(getRelativePath("http://example.com/"), "(index)", "Returns a string representing the index file");

  info("Test a url with directory path");
  equal(getRelativePath("http://example.com/path/"), "path/", "Returns the directory path");

  info("Test a http url a simple file path with no extension");
  equal(getRelativePath("http://example.com/file"), "file", "Returns the simple path");

  info("Test a file url with a multi directory file path with no extension");
  equal(getRelativePath("file:///path/to/file"), "path/to/file", "Returns the full file path");

  info("Test a http url which is multi directory with html file path");
  equal(getRelativePath("http://example.com/path/to/file.html"), "path/to/file.html", "Returns the full html file path");

   info("Test a http url which is multi directory with js file path");
  equal(getRelativePath("http://example.com/path/to/file.js"), "path/to/file.js", "Returns the full js file path");

  info("Test a file url with path and query parameters");
  equal(getRelativePath("file:///path/to/file.js?bla=bla"), "path/to/file.js", "Returns the full path without the query params");

  info("Test a webpack url with path and fragment");
  equal(getRelativePath("webpack:///path/to/file.js#bla"), "path/to/file.js", "Returns the full path without the query params");
}
