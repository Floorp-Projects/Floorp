/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { require } = ChromeUtils.importESModule(
  "resource://devtools/shared/loader/Loader.sys.mjs"
);
const {
  withCommonPathPrefixRemoved,
} = require("resource://devtools/client/performance-new/shared/utils.js");

add_task(function test() {
  info(
    "withCommonPathPrefixRemoved() removes the common prefix from an array " +
      "of paths. This test ensures that the paths are correctly removed."
  );

  if (Services.appinfo.OS === "WINNT") {
    info("Check Windows paths");

    deepEqual(withCommonPathPrefixRemoved([]), [], "Windows empty paths");

    deepEqual(
      withCommonPathPrefixRemoved(["source\\file1.js", "source\\file2.js"]),
      ["source\\file1.js", "source\\file2.js"],
      "Windows relative paths"
    );

    deepEqual(
      withCommonPathPrefixRemoved([
        "C:\\Users\\SomeUser\\Desktop\\source\\file1.js",
        "D:\\Users\\SomeUser\\Desktop\\source\\file2.js",
      ]),
      [
        "C:\\Users\\SomeUser\\Desktop\\source\\file1.js",
        "D:\\Users\\SomeUser\\Desktop\\source\\file2.js",
      ],
      "Windows multiple disks"
    );

    deepEqual(
      withCommonPathPrefixRemoved([
        "C:\\Users\\SomeUser\\Desktop\\source\\file1.js",
        "C:\\Users\\SomeUser\\Desktop\\source\\file2.js",
      ]),
      ["file1.js", "file2.js"],
      "Windows full path match"
    );

    deepEqual(
      withCommonPathPrefixRemoved([
        "C:\\Users\\SomeUser\\Desktop\\source\\file1.js",
        "C:\\Users\\SomeUser\\file2.js",
      ]),
      ["Desktop\\source\\file1.js", "file2.js"],
      "Windows path match at level 3"
    );

    deepEqual(
      withCommonPathPrefixRemoved([
        "C:\\Users\\SomeUser\\Desktop\\source\\file1.js",
        "C:\\Users\\SomeUser\\file2.js",
        "C:\\Users\\file3.js",
      ]),
      ["SomeUser\\Desktop\\source\\file1.js", "SomeUser\\file2.js", "file3.js"],
      "Windows path match at level 2"
    );

    deepEqual(
      withCommonPathPrefixRemoved(["C:\\dev"]),
      ["C:\\dev"],
      "Windows path match at level 1"
    );
  } else {
    info("Check UNIX paths");

    deepEqual(withCommonPathPrefixRemoved([]), [], "UNIX empty paths");

    deepEqual(
      withCommonPathPrefixRemoved(["source/file1.js", "source/file2.js"]),
      ["source/file1.js", "source/file2.js"],
      "UNIX relative paths"
    );

    deepEqual(
      withCommonPathPrefixRemoved([
        "/home/someuser/Desktop/source/file1.js",
        "/home/someuser/Desktop/source/file2.js",
      ]),
      ["file1.js", "file2.js"],
      "UNIX full path match"
    );

    deepEqual(
      withCommonPathPrefixRemoved([
        "/home/someuser/Desktop/source/file1.js",
        "/home/someuser/file2.js",
      ]),
      ["Desktop/source/file1.js", "file2.js"],
      "UNIX path match at level 3"
    );

    deepEqual(
      withCommonPathPrefixRemoved([
        "/home/someuser/Desktop/source/file1.js",
        "/home/someuser/file2.js",
        "/home/file3.js",
      ]),
      ["someuser/Desktop/source/file1.js", "someuser/file2.js", "file3.js"],
      "UNIX path match at level 2"
    );

    deepEqual(
      withCommonPathPrefixRemoved(["/bin"]),
      ["/bin"],
      "UNIX path match at level 1"
    );
  }
});
