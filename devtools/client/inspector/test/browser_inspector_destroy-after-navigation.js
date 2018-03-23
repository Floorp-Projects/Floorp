/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Testing that closing the inspector after navigating to a page doesn't fail.

const URL_1 = "data:text/plain;charset=UTF-8,abcde";
const URL_2 = "data:text/plain;charset=UTF-8,12345";

add_task(function* () {
  let { inspector, toolbox } = yield openInspectorForURL(URL_1);

  yield navigateTo(inspector, URL_2);

  info("Destroying toolbox");
  try {
    yield toolbox.destroy();
    ok(true, "Toolbox destroyed");
  } catch (e) {
    ok(false, "An exception occured while destroying toolbox");
    console.error(e);
  }
});
