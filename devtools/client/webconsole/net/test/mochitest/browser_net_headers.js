/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PAGE_URL = URL_ROOT + "page_basic.html";
const JSON_XHR_URL = URL_ROOT + "test.json";

/**
 * This test generates XHR requests in the page, expands
 * networks details in the Console panel and checks that
 * HTTP headers are there.
 */
add_task(function* () {
  info("Test XHR Spy headers started");

  let {hud} = yield addTestTab(TEST_PAGE_URL);

  let netInfoBody = yield executeAndInspectXhr(hud, {
    method: "GET",
    url: JSON_XHR_URL
  });

  // Select "Headers" tab
  let tabBody = yield selectNetInfoTab(hud, netInfoBody, "headers");
  let paramName = tabBody.querySelector(
    ".netInfoParamName > span[title='content-type']");

  // Verify "Content-Type" header (name and value)
  ok(paramName, "Header name must exist");
  is(paramName.textContent, "content-type",
    "The header name must have proper value");

  let paramValue = paramName.parentNode.nextSibling;
  ok(paramValue, "Header value must exist");
  is(paramValue.textContent, "application/json; charset=utf-8",
    "The header value must have proper value");
});
