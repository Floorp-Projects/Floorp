/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PAGE_URL = URL_ROOT + "page_basic.html";
const JSON_XHR_URL = URL_ROOT + "test-cookies.json";

/**
 * This test generates XHR requests in the page, expands
 * networks details in the Console panel and checks that
 * Cookies are properly displayed.
 */
add_task(function* () {
  info("Test XHR Spy cookies started");

  let {hud} = yield addTestTab(TEST_PAGE_URL);

  let netInfoBody = yield executeAndInspectXhr(hud, {
    method: "GET",
    url: JSON_XHR_URL
  });

  // Select "Cookies" tab
  let tabBody = yield selectNetInfoTab(hud, netInfoBody, "cookies");

  let requestCookieName = tabBody.querySelector(
    ".netInfoGroup.requestCookies .netInfoParamName > span[title='bar']");

  // Verify request cookies (name and value)
  ok(requestCookieName, "Request Cookie name must exist");
  is(requestCookieName.textContent, "bar",
    "The cookie name must have proper value");

  let requestCookieValue = requestCookieName.parentNode.nextSibling;
  ok(requestCookieValue, "Request Cookie value must exist");
  is(requestCookieValue.textContent, "foo",
    "The cookie value must have proper value");

  let responseCookieName = tabBody.querySelector(
    ".netInfoGroup.responseCookies .netInfoParamName > span[title='test']");

  // Verify response cookies (name and value)
  ok(responseCookieName, "Response Cookie name must exist");
  is(responseCookieName.textContent, "test",
    "The cookie name must have proper value");

  let responseCookieValue = responseCookieName.parentNode.nextSibling;
  ok(responseCookieValue, "Response Cookie value must exist");
  is(responseCookieValue.textContent, "abc",
    "The cookie value must have proper value");
});
