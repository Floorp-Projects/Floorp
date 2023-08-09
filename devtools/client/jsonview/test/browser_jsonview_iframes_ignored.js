/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_JSON_URL = URL_ROOT + "doc_with_json_in_iframe.html";

add_task(async function () {
  info("Test JSON view is disabled for iframes");

  await addTab(TEST_JSON_URL);

  // Wait for JSONView to have a chance to load.
  // As we expect it to not load, we can't listen to any particular event.
  await wait(1000);

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    const frameWin = content.frames[0];
    ok(
      !frameWin.wrappedJSObject.JSONView,
      "JSONView object isn't instantiated in the iframe"
    );
    is(
      frameWin.document.contentType,
      "application/json",
      "The mime type is json and not jsonview one"
    );
    // When JSON file is displayed as-is, it is within a made up HTML document
    // where the JSON text is put as child text element of a <pre> element.
    ok(
      frameWin.document.querySelector("pre"),
      "There is a <pre> element in plain text html document"
    );
    // That made up HTML doesn't include any <div> element.
    ok(
      !frameWin.document.querySelector("div"),
      "But there is no <div> coming from JSONView"
    );
  });
});
