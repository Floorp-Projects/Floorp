/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

add_task(async function init() {
  const testCases = await require_module(
    "dom/fs/test/common/test_fileSystemDirectoryHandle.js"
  );
  Object.values(testCases).forEach(async testItem => {
    add_task(testItem);
  });
});
