/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */


add_task(async function() {
  Assert.throws(
    () => ChromeUtils.import("resource://test/error_import.sys.mjs"),
    /use ChromeUtils.importESModule instead/,
    "Error should be caught and suggest ChromeUtils.importESModule"
  );

  Assert.throws(
    () => ChromeUtils.import("resource://test/error_export.sys.mjs"),
    /use ChromeUtils.importESModule instead/,
    "Error should be caught and suggest ChromeUtils.importESModule"
  );

  Assert.throws(
    () => ChromeUtils.import("resource://test/error_other.sys.mjs"),
    /expected expression, got end of script/,
    "Error should be caught but should not suggest ChromeUtils.importESModule"
  );
});
