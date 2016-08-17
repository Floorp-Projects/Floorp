/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PAGE_URL = URL_ROOT + "page_basic.html";
const JSON_XHR_URL = URL_ROOT + "test.json";

/**
 * Basic test that generates XHR in the content and
 * checks the related log in the Console panel can
 * be expanded.
 */
add_task(function* () {
  info("Test XHR Spy basic started");

  let {hud} = yield addTestTab(TEST_PAGE_URL);

  let netInfoBody = yield executeAndInspectXhr(hud, {
    method: "GET",
    url: JSON_XHR_URL
  });

  ok(netInfoBody, "The network details must be available");

  // There should be at least two tabs: Headers and Response
  ok(netInfoBody.querySelector(".tabs .tabs-menu-item.headers"),
    "Headers tab must be available");
  ok(netInfoBody.querySelector(".tabs .tabs-menu-item.response"),
    "Response tab must be available");
});
