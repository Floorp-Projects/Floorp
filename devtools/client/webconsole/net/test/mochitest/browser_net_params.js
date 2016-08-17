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
 * HTTP parameters (query string) are there.
 */
add_task(function* () {
  info("Test XHR Spy params started");

  let {hud} = yield addTestTab(TEST_PAGE_URL);

  let netInfoBody = yield executeAndInspectXhr(hud, {
    method: "GET",
    url: JSON_XHR_URL,
    queryString: "?foo=bar"
  });

  // Check headers
  let tabBody = yield selectNetInfoTab(hud, netInfoBody, "params");

  let paramName = tabBody.querySelector(
    ".netInfoParamName > span[title='foo']");

  // Verify "Content-Type" header (name and value)
  ok(paramName, "Header name must exist");
  is(paramName.textContent, "foo",
    "The param name must have proper value");

  let paramValue = paramName.parentNode.nextSibling;
  ok(paramValue, "param value must exist");
  is(paramValue.textContent, "bar",
    "The param value must have proper value");
});
